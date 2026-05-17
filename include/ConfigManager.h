/**
 * Configuration Manager - Load settings from JSON file
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

struct WiFiConfig {
    String ssid;
    String password;
    String hostname;
    uint32_t timeout_ms;
};

struct DeviceConfig {
    String name;
    String model;
    String location;
};

struct AudioConfig {
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t i2s_bclk_pin;
    uint8_t i2s_lrc_pin;
    uint8_t i2s_dout_pin;
    uint8_t default_volume;
};

struct ProtocolConfig {
    bool bluetooth_enabled;
    bool dlna_enabled;
    uint16_t dlna_port;
    bool heos_enabled;
    uint16_t heos_command_port;
    uint16_t heos_stream_port;
};

class ConfigManager {
public:
    ConfigManager();

    bool begin();
    bool loadConfig();
    bool saveConfig();

    // Getters
    WiFiConfig getWiFiConfig() const { return wifiConfig; }
    DeviceConfig getDeviceConfig() const { return deviceConfig; }
    AudioConfig getAudioConfig() const { return audioConfig; }
    ProtocolConfig getProtocolConfig() const { return protocolConfig; }

    // Setters
    void setWiFiSSID(const String& ssid) { wifiConfig.ssid = ssid; }
    void setWiFiPassword(const String& password) { wifiConfig.password = password; }
    void setDeviceName(const String& name) { deviceConfig.name = name; }
    void setVolume(uint8_t volume) { audioConfig.default_volume = volume; }

private:
    WiFiConfig wifiConfig;
    DeviceConfig deviceConfig;
    AudioConfig audioConfig;
    ProtocolConfig protocolConfig;

    static constexpr const char* CONFIG_FILE = "/config.json";
    bool loadDefaults();
};

#endif // CONFIG_MANAGER_H
