#include <esp32-hal-log.h>
#include <audio_capture_mems.h>

#define DMA_BUFFERS 4
#define IS_SPH0645 false

audio_capture_mems::audio_capture_mems(i2s_port_t i2s_port, i2s_pin_config_t pin_config, float seconds_per_buffer /*= 0.064*/, size_t sample_rate /*= 16000*/, i2s_channel_t channels /*= I2S_CHANNEL_MONO*/, ushort bits_per_sample /*= 16*/)
    : audio_capture(i2s_port, seconds_per_buffer, sample_rate, channels, bits_per_sample)
{
  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = (int)sample_rate_,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = DMA_BUFFERS,
      .dma_buf_len = (int)samples_per_buffer_, // This is the number of SAMPLES!
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  // Install I2S driver
  ESP_ERROR_CHECK(i2s_driver_install(i2s_port_, &i2s_config, 0, nullptr));
  if (IS_SPH0645)
  {
    // Fixes for SPH0645
    REG_SET_BIT(I2S_TIMING_REG(i2s_port_), BIT(9));
    REG_SET_BIT(I2S_CONF_REG(i2s_port_), I2S_RX_MSB_SHIFT);
  }

  // Set I2S hardware pins
  ESP_ERROR_CHECK(i2s_set_pin(i2s_port_, &pin_config));
  // Clear I2S DMA buffer
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(i2s_port_));
}

void audio_capture_mems::convert_from_raw_samples(esp32_i2s_sample_t *raw_samples, mono_sample_t *samples, size_t size)
{
  // Convert from 24 bits (0x1000000) raw samples to 16 bits signed
  // 00000000 11111111 11111111 11111111

  while (size--)
  *samples++ = (mono_sample_t)(~(*raw_samples++) + 1);
    //*samples++ = (mono_sample_t)(~((*raw_samples++) >> 14) + 1);
}
