/**
 * System-wide constants and configuration values
 * Eliminates magic numbers throughout the codebase
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>

// Timing Constants
constexpr uint32_t STARTUP_DELAY_MS = 5000;
constexpr uint32_t STATUS_PRINT_INTERVAL_MS = 30000;
constexpr uint32_t SSDP_ANNOUNCE_INTERVAL_MS = 30000;
constexpr uint32_t HEOS_HEARTBEAT_INTERVAL_MS = 5000;
constexpr uint32_t WIFI_RECONNECT_DELAY_MS = 5000;
constexpr uint32_t STREAM_STOP_DELAY_MS = 100;
constexpr uint32_t BODY_READ_TIMEOUT_MS = 2000;

// SSDP Response Timing (UPnP spec compliance)
constexpr uint32_t SSDP_MIN_DELAY_MS = 10;
constexpr uint32_t SSDP_MAX_DELAY_MS = 100;
constexpr uint32_t SSDP_RESPONSE_SPACING_MS = 250;

// Buffer Sizes
constexpr size_t AUDIO_TASK_STACK_SIZE = 8192;
constexpr size_t STREAM_BUFFER_SIZE = 65536;  // 64KB for DLNA stability
constexpr size_t SSDP_BUFFER_SIZE = 1024;
constexpr size_t XML_RESERVE_SIZE = 2048;
constexpr size_t IP_STRING_BUFFER_SIZE = 16;
constexpr size_t MAC_STRING_BUFFER_SIZE = 18;

// FreeRTOS Task Configuration
constexpr uint8_t AUDIO_TASK_PRIORITY = 5;
constexpr uint8_t AUDIO_TASK_CORE = 1;  // Application core

// Audio Sample Conversion
constexpr float SAMPLES_TO_MS_DIVISOR = 44.1f;  // For 44100 Hz sample rate

// Input Validation Limits (Security)
constexpr size_t MAX_SSID_LENGTH = 32;
constexpr size_t MAX_PASSWORD_LENGTH = 64;
constexpr size_t MAX_URL_LENGTH = 512;
constexpr size_t MAX_DEVICE_NAME_LENGTH = 64;
constexpr size_t MAX_HEADER_VALUE_LENGTH = 256;

// DLNA/UPnP Error Recovery
constexpr uint8_t MAX_URI_EXTRACTION_ATTEMPTS = 3;
constexpr uint32_t URI_EXTRACTION_TIMEOUT_MS = 5000;

// Debug Output Control
#ifdef CORE_DEBUG_LEVEL
    #define DEBUG_ENABLED (CORE_DEBUG_LEVEL >= 3)
#else
    #define DEBUG_ENABLED 0
#endif

#if DEBUG_ENABLED
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(fmt, ...)
#endif

#endif // CONSTANTS_H
