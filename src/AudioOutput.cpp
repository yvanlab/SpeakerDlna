/**
 * Audio Output Implementation
 */

#include "AudioOutput.h"
#include "config.h"
#include "ConfigManager.h"

I2SAudioOutput::I2SAudioOutput() : initialized(false), currentVolume(50) {
}

I2SAudioOutput::~I2SAudioOutput() {
    end();
}

bool I2SAudioOutput::begin(const AudioConfig& config) {
    if (initialized) {
        return true;
    }

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = (uint32_t)config.sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB, // Required for Internal DAC
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    // Enable internal DAC mode if pins are set to 25/26 (Standard ESP32 only)
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    // Force Internal DAC mode if specific pins are used
    if (config.i2s_bclk_pin == 26 || config.i2s_lrc_pin == 25) {
        i2s_config.mode = (i2s_mode_t)(i2s_config.mode | I2S_MODE_DAC_BUILT_IN);
    }
#endif

    esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %d\n", err);
        return false;
    }

    i2s_pin_config_t pin_config = {
        .bck_io_num = config.i2s_bclk_pin,
        .ws_io_num = config.i2s_lrc_pin,
        .data_out_num = config.i2s_dout_pin,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    // If internal DAC is enabled, use the specific dac_mode setter
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    if (i2s_config.mode & I2S_MODE_DAC_BUILT_IN) {
        i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    } else 
#endif
    {
    err = i2s_set_pin(I2S_NUM, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed to set I2S pins: %d\n", err);
        i2s_driver_uninstall(I2S_NUM);
        return false;
    }
    }

    i2s_zero_dma_buffer(I2S_NUM);

    initialized = true;
    Serial.println("Audio Output initialized");
    Serial.printf("Sample Rate: %d Hz\n", config.sample_rate);

    return true;
}

void I2SAudioOutput::end() {
    if (initialized) {
        i2s_driver_uninstall(I2S_NUM);
        initialized = false;
        Serial.println("Audio Output stopped");
    }
}

void I2SAudioOutput::flush() {
    if (initialized) {
        i2s_zero_dma_buffer(I2S_NUM);
    }
}

size_t I2SAudioOutput::write(const uint8_t* data, size_t len) {
    if (!initialized || data == NULL || len == 0) {
        return 0;
    }

    size_t bytesWritten = 0;
    esp_err_t err = i2s_write(I2S_NUM, data, len, &bytesWritten, portMAX_DELAY);

    if (err != ESP_OK) {
        Serial.printf("I2S write error: %d\n", err);
        return 0;
    }

    return bytesWritten;
}

void I2SAudioOutput::consumeSample(int16_t sample[2]) {
    if (!initialized) {
        return;
    }

    // Volume scaling
    int32_t left = sample[0];
    int32_t right = sample[1];
    
    left = (left * (int32_t)currentVolume) / 100;
    right = (right * (int32_t)currentVolume) / 100;

    // Write 16-bit stereo samples directly to the I2S peripheral
    int16_t stereoBuffer[2] = {(int16_t)left, (int16_t)right};

    write((const uint8_t*)stereoBuffer, sizeof(stereoBuffer));
}

void I2SAudioOutput::setVolume(uint8_t volume) {
    currentVolume = min(volume, (uint8_t)100);
    Serial.printf("Volume set to: %d%%\n", currentVolume);
}
