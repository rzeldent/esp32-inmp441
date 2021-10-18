#include <esp32-hal-log.h>
#include <audio_capture.h>

// Number of queues
#define SAMPLE_BUFFER_NUM_QUEUES 2

audio_sample_buffer::audio_sample_buffer(size_t size)
{
    samples = std::vector<mono_sample_t>(size);
    gettimeofday(&timestamp, nullptr);
}

audio_capture::audio_capture(i2s_port_t i2s_port, float seconds_per_buffer /*= 0.016f*/, uint sample_rate /*= 16000*/, i2s_channel_t channels /*= I2S_CHANNEL_MONO*/, ushort bits_per_sample /*= 16*/)
    : i2s_port_(i2s_port), seconds_per_buffer_(seconds_per_buffer), sample_rate_(sample_rate), channels_(channels), bits_per_sample_(bits_per_sample)
{
    samples_per_buffer_ = sample_rate * seconds_per_buffer_;
    if (samples_per_buffer_ > 1024)
        log_e("DMA buffer must not exceed 1024 samples. Currently: %d", samples_per_buffer_);

    log_i("Sample rate: %ud Hz. Seconds per buffer: %f. Channels: %d. Bits per sample: %d.", sample_rate_, seconds_per_buffer_, channels_, bits_per_sample_);
    log_i("Calculated samples per buffer: %d", samples_per_buffer_);

    // Create the sample queue of pointers to buffers
    sample_queue_ = xQueueCreate(SAMPLE_BUFFER_NUM_QUEUES, sizeof(audio_sample_buffer_t *));
    if (!sample_queue_)
        log_e("Unable to create queue");
}

audio_capture::~audio_capture()
{
    vQueueDelete(sample_queue_);
}

void audio_capture::callback(void *self)
{
    ((audio_capture *)self)->record_task();
}

void audio_capture::record_task()
{
    log_i("Recording task started");

    auto esp32_raw_samples = new esp32_i2s_sample_t[samples_per_buffer_];
    if (!esp32_raw_samples)
        log_e("Unable to allocate %d bytes for raw esp samples", samples_per_buffer_ * sizeof(esp32_i2s_sample_t));

    log_i("Starting loop for task %s", task_name());
    // Run until signal terminate
    while (true)
    {
        // Read samples from ESP32
        log_d("Reading samples from I2S");
        size_t i2s_bytes_read;
        ESP_ERROR_CHECK(i2s_read(i2s_port_, esp32_raw_samples, samples_per_buffer_ * sizeof(esp32_i2s_sample_t), &i2s_bytes_read, portMAX_DELAY));
        size_t samples_read = i2s_bytes_read / sizeof(esp32_i2s_sample_t);
        auto samples = new audio_sample_buffer_t(samples_read);
        // Get left channel in buffer
        log_d("Normalizing raw samples");
        convert_from_raw_samples(esp32_raw_samples, samples->samples.data(), samples_read);
        // Save to queue
        push_samples(samples);
    }

    // Should never come here
    delete[] esp32_raw_samples;
    log_i("Recording task stopped");
    vTaskDelete(nullptr);
}

void audio_capture::start(int stack_size /*= 2048*/, UBaseType_t priority /*= 5*/)
{
    log_i("Starting recording");
    // Create a task with 2kb stack, priority 5, on Application CPU
    xTaskCreatePinnedToCore(audio_capture::callback, task_name(), stack_size, (void *)this, priority, &task_handle_, 1);
}

void audio_capture::push_samples(audio_sample_buffer_t *sample_buffer)
{
    // Wait until queued. No timeout
    xQueueSend(sample_queue_, &sample_buffer, portMAX_DELAY);
    log_d("Send %d samples to queue. ts=%d.%d", sample_buffer->samples.size(), sample_buffer->timestamp.tv_sec, sample_buffer->timestamp.tv_usec);
}

std::unique_ptr<audio_sample_buffer_t> audio_capture::pop_samples(uint ticks_to_wait /*= portMAX_DELAY*/)
{
    audio_sample_buffer_t *sample_buffer = nullptr;
    if (xQueueReceive(sample_queue_, &sample_buffer, ticks_to_wait) == pdTRUE)
        log_d("Retrieved %d samples from queue. ts=%d.d", sample_buffer->samples.size(), sample_buffer->timestamp.tv_sec, sample_buffer->timestamp.tv_usec);
    else
        log_w("Unable to retrieve samples");

    return std::unique_ptr<audio_sample_buffer_t>(sample_buffer);
}

std::vector<unsigned char> audio_capture::wav_header(size_t number_of_samples) const
{
    // See: https://docs.fileformat.com/audio/wav/
    const auto header_size = 36;
    const auto bytes_per_sample = channels_ * (bits_per_sample_ + 0x7) >> 3;
    const auto bytes_per_second = sample_rate_ * bytes_per_sample;
    const auto samples_data_size = number_of_samples * bytes_per_sample;
    const auto file_size_minus_8 = samples_data_size + header_size; // Total files size - 8 'data' + length not uncluded
    unsigned char wav_header[44] = {
        'R',
        'I',
        'F',
        'F', // 0
        (unsigned char)file_size_minus_8,
        (unsigned char)(file_size_minus_8 >> 8),
        (unsigned char)(file_size_minus_8 >> 16),
        (unsigned char)(file_size_minus_8 >> 24),
        'W',
        'A',
        'V',
        'E',
        'f',
        'm',
        't',
        ' ',
        0x10, // Length of above format data
        0x0,  // Length of above format data
        0x0,  // Length of above format data
        0x0,  // Length of above format data
        0x1,  // Format type (1 - PCM)
        0x0,  // Format type (1 - PCM)
        (unsigned char)channels_,
        (unsigned char)(channels_ >> 8),
        (unsigned char)sample_rate_,
        (unsigned char)(sample_rate_ >> 8),
        (unsigned char)(sample_rate_ >> 16),
        (unsigned char)(sample_rate_ >> 24),
        (unsigned char)(bytes_per_second),       // (sampleRate * channels * bitsPerSample // 8)
        (unsigned char)(bytes_per_second >> 8),  // (sampleRate * channels * bitsPerSample // 8)
        (unsigned char)(bytes_per_second >> 16), // (sampleRate * channels * bitsPerSample // 8)
        (unsigned char)(bytes_per_second >> 24), // (sampleRate * channels * bitsPerSample // 8)
        (unsigned char)(bytes_per_sample),       // (channels * bitsPerSample // 8)
        (unsigned char)(bytes_per_sample >> 8),  // (channels * bitsPerSample // 8)
        (unsigned char)(bits_per_sample_),       // bitsPerSample
        (unsigned char)(bits_per_sample_ >> 8),  // bitsPerSample
        'd',
        'a',
        't',
        'a',
        (unsigned char)samples_data_size,
        (unsigned char)(samples_data_size >> 8),
        (unsigned char)(samples_data_size >> 16),
        (unsigned char)(samples_data_size >> 24),
    };
    return std::vector<unsigned char>(wav_header, wav_header + sizeof(wav_header));
}
