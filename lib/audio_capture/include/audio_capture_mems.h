#pragma once

#include <audio_capture.h>

class audio_capture_mems : public audio_capture
{
protected:
    virtual void convert_from_raw_samples(esp32_i2s_sample_t *raw_samples, mono_sample_t *samples, size_t size);

public:
    audio_capture_mems(i2s_port_t i2s_port, i2s_pin_config_t pin_config, float seconds_per_buffer = 0.016, size_t sample_rate = 16000, i2s_channel_t channels = I2S_CHANNEL_MONO, ushort bits_per_sample = 16);
    virtual const char *task_name() const { return "audio_capture_mems"; };
};
