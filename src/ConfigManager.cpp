/**
 * Configuration Manager Implementation
 */

#include "ConfigManager.h"

ConfigManager::ConfigManager() {
    loadDefaults();
}

bool ConfigManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return false;
    }

    Serial.println("LittleFS mounted successfully");
    return loadConfig();
}

bool ConfigManager::loadDefaults() {
    // WiFi defaults
 
    wifiConfig.ssid = "YOUR_WIFI_SSID";
    wifiConfig.password = "YOUR_WIFI_PASSWORD";
    wifiConfig.hostname = "esp32-speaker";
    wifiConfig.timeout_ms = 20000;

    // Device defaults
    deviceConfig.name = "ESP32-Speaker";
    deviceConfig.model = "ESP32-HEOS-DLNA-BT";
    deviceConfig.location = "Living Room";

    // Audio defaults
    audioConfig.sample_rate = 44100;
    audioConfig.bits_per_sample = 16;
    audioConfig.i2s_bclk_pin = 26;
    audioConfig.i2s_lrc_pin = 25;
    audioConfig.i2s_dout_pin = 22;
    audioConfig.default_volume = 50;

    // Protocol defaults
    protocolConfig.bluetooth_enabled = true;
    protocolConfig.dlna_enabled = true;
    protocolConfig.dlna_port = 8080;
    protocolConfig.heos_enabled = true;
    protocolConfig.heos_command_port = 1255;
    protocolConfig.heos_stream_port = 8081;

    return true;
}

bool ConfigManager::loadConfig() {
    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        Serial.println("Config file not found, using defaults");
        Serial.println("Creating default config file...");
        saveConfig();
        return true;
    }

    size_t size = file.size();
    if (size == 0) {
        Serial.println("Config file is empty");
        file.close();
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("Failed to parse config: %s\n", error.c_str());
        return false;
    }

    // Parse WiFi config
    if (doc["wifi"].is<JsonObject>()) {
        JsonObject wifi = doc["wifi"];
        wifiConfig.ssid = wifi["ssid"] | wifiConfig.ssid;
        wifiConfig.password = wifi["password"] | wifiConfig.password;
        wifiConfig.hostname = wifi["hostname"] | wifiConfig.hostname;
        wifiConfig.timeout_ms = wifi["timeout_ms"] | wifiConfig.timeout_ms;
    }

    // Parse device config
    if (doc["device"].is<JsonObject>()) {
        JsonObject device = doc["device"];
        deviceConfig.name = device["name"] | deviceConfig.name;
        deviceConfig.model = device["model"] | deviceConfig.model;
        deviceConfig.location = device["location"] | deviceConfig.location;
    }

    // Parse audio config
    if (doc["audio"].is<JsonObject>()) {
        JsonObject audio = doc["audio"];
        audioConfig.sample_rate = audio["sample_rate"] | audioConfig.sample_rate;
        audioConfig.bits_per_sample = audio["bits_per_sample"] | audioConfig.bits_per_sample;
        audioConfig.default_volume = audio["default_volume"] | audioConfig.default_volume;

        if (audio["i2s"].is<JsonObject>()) {
            JsonObject i2s = audio["i2s"];
            audioConfig.i2s_bclk_pin = i2s["bclk_pin"] | audioConfig.i2s_bclk_pin;
            audioConfig.i2s_lrc_pin = i2s["lrc_pin"] | audioConfig.i2s_lrc_pin;
            audioConfig.i2s_dout_pin = i2s["dout_pin"] | audioConfig.i2s_dout_pin;
        }
    }

    // Parse protocol config
    if (doc["protocols"].is<JsonObject>()) {
        JsonObject protocols = doc["protocols"];

        if (protocols["bluetooth"].is<JsonObject>()) {
            protocolConfig.bluetooth_enabled = protocols["bluetooth"]["enabled"] | true;
        }

        if (protocols["dlna"].is<JsonObject>()) {
            JsonObject dlna = protocols["dlna"];
            protocolConfig.dlna_enabled = dlna["enabled"] | true;
            protocolConfig.dlna_port = dlna["port"] | 8080;
        }

        if (protocols["heos"].is<JsonObject>()) {
            JsonObject heos = protocols["heos"];
            protocolConfig.heos_enabled = heos["enabled"] | true;
            protocolConfig.heos_command_port = heos["command_port"] | 1255;
            protocolConfig.heos_stream_port = heos["stream_port"] | 8081;
        }
    }

    Serial.println("Configuration loaded successfully");
    return true;
}

bool ConfigManager::saveConfig() {
    JsonDocument doc;

    // WiFi config
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = wifiConfig.ssid;
    wifi["password"] = wifiConfig.password;
    wifi["hostname"] = wifiConfig.hostname;
    wifi["timeout_ms"] = wifiConfig.timeout_ms;

    // Device config
    JsonObject device = doc["device"].to<JsonObject>();
    device["name"] = deviceConfig.name;
    device["model"] = deviceConfig.model;
    device["location"] = deviceConfig.location;

    // Audio config
    JsonObject audio = doc["audio"].to<JsonObject>();
    audio["sample_rate"] = audioConfig.sample_rate;
    audio["bits_per_sample"] = audioConfig.bits_per_sample;
    audio["default_volume"] = audioConfig.default_volume;

    JsonObject i2s = audio["i2s"].to<JsonObject>();
    i2s["bclk_pin"] = audioConfig.i2s_bclk_pin;
    i2s["lrc_pin"] = audioConfig.i2s_lrc_pin;
    i2s["dout_pin"] = audioConfig.i2s_dout_pin;

    // Protocol config
    JsonObject protocols = doc["protocols"].to<JsonObject>();

    JsonObject bluetooth = protocols["bluetooth"].to<JsonObject>();
    bluetooth["enabled"] = protocolConfig.bluetooth_enabled;

    JsonObject dlna = protocols["dlna"].to<JsonObject>();
    dlna["enabled"] = protocolConfig.dlna_enabled;
    dlna["port"] = protocolConfig.dlna_port;

    JsonObject heos = protocols["heos"].to<JsonObject>();
    heos["enabled"] = protocolConfig.heos_enabled;
    heos["command_port"] = protocolConfig.heos_command_port;
    heos["stream_port"] = protocolConfig.heos_stream_port;

    // Write to file
    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    if (serializeJsonPretty(doc, file) == 0) {
        Serial.println("Failed to write config");
        file.close();
        return false;
    }

    file.close();
    Serial.println("Configuration saved successfully");
    return true;
}
