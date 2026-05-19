/**
 * Audio Output via I2S
 */

#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <Arduino.h>
#include <driver/i2s.h>

struct AudioConfig;

class I2SAudioOutput {
public:
    I2SAudioOutput();
    ~I2SAudioOutput();

    bool begin(const AudioConfig& config);
    void end();
    void consumeSample(int16_t sample[2]);
    void flush();

    size_t write(const uint8_t* data, size_t len);
    void setVolume(uint8_t volume);
    uint8_t getVolume() const { return currentVolume; }

private:
    bool initialized;
    volatile uint8_t currentVolume;

    static constexpr i2s_port_t I2S_NUM = I2S_NUM_0;
};

#endif // AUDIO_OUTPUT_H
