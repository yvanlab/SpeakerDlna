/**
 * Configuration file for ESP32 DLNA and Bluetooth Speaker
 */

#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration - UPDATE THESE!
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define WIFI_TIMEOUT_MS 20000

// Device Configuration
#define DEVICE_NAME    "ESP32-Speaker"
#define DEVICE_MODEL   "ESP32-DLNA-BT-Speaker"

// I2S Audio Configuration
#ifndef I2S_BCLK_PIN
#define I2S_BCLK_PIN    26  // Bit Clock
#endif
#ifndef I2S_LRC_PIN
#define I2S_LRC_PIN     25  // Left/Right Clock (Word Select)
#endif
#ifndef I2S_DOUT_PIN
#define I2S_DOUT_PIN    22  // Data Out
#endif

#define SAMPLE_RATE     44100
#define BITS_PER_SAMPLE 16

// DLNA/UPnP Configuration
#define SSDP_PORT       1900
#define HTTP_PORT       8080
#define SSDP_MULTICAST_ADDR "239.255.255.250"

// HEOS Configuration
#define HEOS_PORT       1255
#define HEOS_HTTP_PORT  8081

// Bluetooth Configuration
#define BT_DEVICE_NAME  DEVICE_NAME

// Test Mode Configuration
#define TEST_SINE_FREQ    440.0f  // Frequency in Hz (440Hz is A4 note)
#define TEST_SINE_VOLUME  0.10f   // Volume level (0.0 to 1.0)
#define TEST_FM_MOD_FREQ  5.0f    // Speed of frequency change (Hz)
#define TEST_FM_DEVIATION 50.0f   // Range of frequency swing (Hz)
#define TEST_DURATION_MS  5000    // Duration of test in milliseconds

#endif // CONFIG_H
