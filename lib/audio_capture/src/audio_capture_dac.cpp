#include <esp32-hal-log.h>
#include <audio_capture_dac.h>

#define DMA_BUFFERS 4

audio_capture_dac::audio_capture_dac(i2s_port_t i2s_port, adc1_channel_t adc1_channel, float seconds_per_buffer /*= 0.016*/, size_t sample_rate /*= 16000*/, i2s_channel_t channels /*= I2S_CHANNEL_MONO*/, ushort bits_per_sample /*= 16*/)
    : audio_capture(i2s_port, seconds_per_buffer, sample_rate, channels, bits_per_sample)
{
  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
      .sample_rate = (int)sample_rate_,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = DMA_BUFFERS,
      .dma_buf_len = (int)samples_per_buffer_, // This is the number of SAMPLES!
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  // Install I2S driver
  ESP_ERROR_CHECK(i2s_driver_install(i2s_port_, &i2s_config, 0, nullptr));
  ESP_ERROR_CHECK(i2s_set_adc_mode(ADC_UNIT_1, adc1_channel));
  // Enable the adc
  ESP_ERROR_CHECK(i2s_adc_enable(i2s_port_));
}

void audio_capture_dac::convert_from_raw_samples(esp32_i2s_sample_t *raw_samples, mono_sample_t *samples, size_t size)
{
  // Convert from 12 bits (0x1000) raw samples to 16 bits signed
  // 00001111 11111111

  while (size--)
    *samples++ = (mono_sample_t)((0xfff - (*raw_samples++ & 0xfff)) - 0x800);
}