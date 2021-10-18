#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <driver/i2s.h>

#include <sys/time.h>
#include <memory>
#include <vector>

// Use for the final result 16 bits per channel
typedef int16_t mono_sample_t;

typedef struct audio_sample_buffer
{
    struct timeval timestamp;
    std::vector<mono_sample_t> samples;

    audio_sample_buffer(size_t size);
} audio_sample_buffer_t;

class audio_capture
{
private:
    static void callback(void *self);
    virtual void record_task();

    QueueHandle_t sample_queue_;
    TaskHandle_t task_handle_;

protected:
    typedef ushort esp32_i2s_sample_t;

    i2s_port_t i2s_port_;
    float seconds_per_buffer_;
    uint sample_rate_;
    ushort samples_per_buffer_;
    i2s_channel_t channels_;
    ushort bits_per_sample_;

    virtual void convert_from_raw_samples(esp32_i2s_sample_t *raw_samples, mono_sample_t *samples, size_t size) = 0;
    void push_samples(audio_sample_buffer_t *sample_buffer);

public:
    audio_capture(i2s_port_t i2s_port, float seconds_per_buffer = 0.016f, uint sample_rate = 16000, i2s_channel_t channels = I2S_CHANNEL_MONO, ushort bits_per_sample = 16);
    virtual ~audio_capture();
    virtual const char *task_name() const = 0;

    i2s_port_t get_i2s_port() const { return i2s_port_; };
    uint get_sample_rate() const { return sample_rate_; };
    float get_seconds_per_buffer() const { return seconds_per_buffer_; };
    ushort get_channels() const { return channels_; };
    ushort get_bits_per_sample() const { return bits_per_sample_; };
    ushort get_samples_per_buffer() const { return samples_per_buffer_; };

    void start(int stack_size = 2048, UBaseType_t priority = 5);

    std::unique_ptr<audio_sample_buffer_t> pop_samples(uint ticks_to_wait = portMAX_DELAY);

    std::vector<unsigned char> wav_header(size_t number_of_samples) const;
};