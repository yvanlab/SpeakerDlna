/**
 * ESP32 Multi-Protocol Wireless Speaker - Main Application
 *
 * Supports HEOS (Denon), DLNA (UPnP), and Bluetooth A2DP
 * Configuration loaded from JSON file
 */

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "ConfigManager.h"
#include "AudioOutput.h"
#include "AudioStreamer.h"
#include "BluetoothSpeaker.h"
#include "DLNARenderer.h"
#include "HEOSPlayer.h"
#include "config.h"
#include "constants.h"

// Global objects
ConfigManager configManager;
I2SAudioOutput audioOutput;
AudioStreamer* audioStreamer = nullptr;
BluetoothSpeaker* btSpeaker = nullptr;
DLNARenderer* dlnaRenderer = nullptr;
HEOSPlayer* heosPlayer = nullptr;

bool wifiConnected = false;
unsigned long lastStatusPrint = 0;

// Task handle for background audio processing
TaskHandle_t audioTaskHandle = NULL;

void setupWiFi(const WiFiConfig& wifiConfig) {
    Serial.println("\n=== WiFi Setup ===");
    Serial.printf("Connecting to %s", wifiConfig.ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(wifiConfig.hostname.c_str());
    WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < wifiConfig.timeout_ms) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi connected!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Hostname: %s\n", wifiConfig.hostname.c_str());
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
        wifiConnected = true;
    } else {
        Serial.println("\n✗ WiFi connection failed!");
        Serial.println("Network features will not be available.");
        Serial.println("Bluetooth will still work.");
        wifiConnected = false;
    }
}

void printStatus() {
    Serial.println("\n=== Status ===");

    // WiFi status - use stack-allocated buffer to avoid heap fragmentation
    if (wifiConnected && WiFi.status() == WL_CONNECTED) {
        char ipBuf[IP_STRING_BUFFER_SIZE];
        WiFi.localIP().toString().toCharArray(ipBuf, sizeof(ipBuf));
        Serial.printf("WiFi: Connected (%d dBm) - %s\n", WiFi.RSSI(), ipBuf);
    } else {
        Serial.println("WiFi: Disconnected");
    }

    // Bluetooth status
    if (btSpeaker && btSpeaker->isConnected()) {
        Serial.printf("Bluetooth: Connected to '%s'\n",
            btSpeaker->getConnectedDeviceName().c_str());
    } else {
        DeviceConfig deviceCfg = configManager.getDeviceConfig();
        Serial.printf("Bluetooth: Waiting for connection (%s)\n", deviceCfg.name.c_str());
    }

    // DLNA status
    if (dlnaRenderer && dlnaRenderer->isRunning()) {
        Serial.println("DLNA: Active and discoverable");
    } else {
        Serial.println("DLNA: Inactive");
    }

    // HEOS status
    if (heosPlayer && heosPlayer->isRunning()) {
        Serial.printf("HEOS: Active - %s (Volume: %d%%)\n",
            heosPlayer->isPlaying() ? "Playing" : "Stopped",
            heosPlayer->getVolume());
    } else {
        Serial.println("HEOS: Inactive");
    }

    // Audio Streamer status
    if (audioStreamer && audioStreamer->isStreaming()) {
        Serial.printf("Streaming: %s\n", audioStreamer->getCurrentURL().c_str());
        String title = audioStreamer->getTitle();
        String artist = audioStreamer->getArtist();
        if (title.length() > 0) {
            Serial.printf("Now Playing: %s - %s\n", artist.c_str(), title.c_str());
        }
    }

    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("==============\n");
}

// Background task to handle audio decoding without blocking the main loop
void audioTask(void* pvParameters) {
    for (;;) {
        if (audioStreamer) {
            audioStreamer->loop();
        }
        // Yield slightly to let other tasks/WiFi run
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup() {
    // Initialize Serial
    Serial.begin(115200);
    randomSeed(micros());
    
    delay(1000);

    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════════════╗");
    Serial.println("║   ESP32 Multi-Protocol Wireless Speaker      ║");
    Serial.println("║   HEOS • DLNA • Bluetooth                     ║");
    Serial.println("╚═══════════════════════════════════════════════╝");
    Serial.println();

    // Initialize Configuration Manager
    Serial.println("=== Configuration Loading ===");
    if (!configManager.begin()) {
        Serial.println("✗ Failed to load configuration!");
        Serial.println("Using default values. Check data/config.json");
    } else {
        Serial.println("✓ Configuration loaded");
    }

    // Get configuration
    WiFiConfig wifiConfig = configManager.getWiFiConfig();
    DeviceConfig deviceConfig = configManager.getDeviceConfig();
    AudioConfig audioConfig = configManager.getAudioConfig();
    ProtocolConfig protocolConfig = configManager.getProtocolConfig();

    Serial.println("\n--- Loaded WiFi Config ---");
    Serial.printf("SSID: %s\n", wifiConfig.ssid.c_str());
    Serial.printf("Password: %s\n", wifiConfig.password.c_str());
    Serial.printf("Hostname: %s\n", wifiConfig.hostname.c_str());
    Serial.printf("Timeout: %u ms\n", wifiConfig.timeout_ms);

    Serial.println("\n--- Loaded Device Config ---");
    Serial.printf("Name: %s\n", deviceConfig.name.c_str());
    Serial.printf("Model: %s\n", deviceConfig.model.c_str());
    Serial.printf("Location: %s\n", deviceConfig.location.c_str());

    Serial.println("\n--- Loaded Audio Config ---");
    Serial.printf("Sample Rate: %u Hz\n", audioConfig.sample_rate);
    Serial.printf("Bits Per Sample: %d\n", audioConfig.bits_per_sample);
    Serial.printf("I2S Pins - BCLK: %d, LRC: %d, DOUT: %d\n", 
                  audioConfig.i2s_bclk_pin, audioConfig.i2s_lrc_pin, audioConfig.i2s_dout_pin);
    Serial.printf("Default Volume: %d%%\n", audioConfig.default_volume);

    Serial.println("\n--- Loaded Protocol Config ---");
    Serial.printf("Bluetooth: %s\n", protocolConfig.bluetooth_enabled ? "Enabled" : "Disabled");
    Serial.printf("DLNA: %s (Port: %u)\n", protocolConfig.dlna_enabled ? "Enabled" : "Disabled", protocolConfig.dlna_port);
    Serial.printf("HEOS: %s (Cmd Port: %u, Stream Port: %u)\n", 
                  protocolConfig.heos_enabled ? "Enabled" : "Disabled", protocolConfig.heos_command_port, protocolConfig.heos_stream_port);
    
    // Initialize WiFi
    setupWiFi(wifiConfig);

    delay(STARTUP_DELAY_MS);

    // Initialize Audio Output (I2S)
    Serial.println("\n=== Audio Output Initialization ===");
    if (audioOutput.begin(audioConfig)) {
        Serial.println("✓ Audio output ready");
        Serial.printf("Sample Rate: %d Hz, Bits: %d\n",
                     audioConfig.sample_rate, audioConfig.bits_per_sample);
        Serial.printf("I2S Pins - BCLK:%d LRC:%d DOUT:%d\n",
                     audioConfig.i2s_bclk_pin, audioConfig.i2s_lrc_pin, audioConfig.i2s_dout_pin);
        audioOutput.setVolume(audioConfig.default_volume);
    } else {
        Serial.println("✗ Audio output failed!");
        Serial.println("Check I2S wiring and restart");
        while(1) { delay(1000); }
    }

    // Initialize Audio Streamer
    Serial.println("\n=== Audio Streamer Initialization ===");
    audioStreamer = new AudioStreamer(audioOutput);
    if (audioStreamer->begin()) {
        Serial.println("✓ Audio streamer ready");
        Serial.println("Supports: MP3, AAC, M4A, FLAC, HTTP/HTTPS streams");
    } else {
        Serial.println("✗ Audio streamer initialization failed!");
        delete audioStreamer;
        audioStreamer = nullptr;
    }

    // Start the background audio task on Core 1 (Application core)
    // Higher priority than main loop but allows system tasks to run
    // Core 1 is used for application tasks, leaving Core 0 for WiFi/Bluetooth
    xTaskCreatePinnedToCore(
        audioTask,              // Function to run
        "AudioTask",            // Name
        AUDIO_TASK_STACK_SIZE,  // Stack size (bytes)
        NULL,                   // Parameter to pass to function
        AUDIO_TASK_PRIORITY,    // Priority (0 to configMAX_PRIORITIES - 1)
        &audioTaskHandle,       // Task handle
        AUDIO_TASK_CORE         // Core 1
    );


    // Initialize Bluetooth Speaker
    if (protocolConfig.bluetooth_enabled) {
        Serial.println("\n=== Bluetooth Initialization ===");
        btSpeaker = new BluetoothSpeaker(audioOutput);
        if (btSpeaker->begin(deviceConfig.name.c_str())) {
            Serial.println("✓ Bluetooth speaker ready");
            Serial.printf("Device name: '%s'\n", deviceConfig.name.c_str());
            Serial.println("Pair from your phone/computer now!");
        } else {
            Serial.println("✗ Bluetooth initialization failed!");
            delete btSpeaker;
            btSpeaker = nullptr;
        }
    } else {
        Serial.println("\n=== Bluetooth Disabled ===");
    }

    // Initialize DLNA Renderer (only if WiFi is connected)
    if (wifiConnected && protocolConfig.dlna_enabled) {
        Serial.println("\n=== DLNA Initialization ===");
        dlnaRenderer = new DLNARenderer(audioOutput, *audioStreamer);
        if (dlnaRenderer->begin(deviceConfig.name.c_str(), deviceConfig.model.c_str(), protocolConfig.dlna_port)) {
            Serial.println("✓ DLNA renderer ready");
            Serial.printf("Device name: '%s'\n", deviceConfig.name.c_str());
            Serial.printf("HTTP port: %d\n", protocolConfig.dlna_port);
            Serial.printf("DLNA Discovery URL: http://%s:%d/description.xml\n", WiFi.localIP().toString().c_str(), protocolConfig.dlna_port);
            Serial.println("Search for the device in your DLNA app!");
        } else {
            Serial.println("✗ DLNA initialization failed!");
            delete dlnaRenderer;
            dlnaRenderer = nullptr;
        }
    } else if (!wifiConnected) {
        Serial.println("\n=== DLNA Skipped (No WiFi) ===");
    } else {
        Serial.println("\n=== DLNA Disabled ===");
    }

    // Initialize HEOS Player (only if WiFi is connected and streamer available)
    if (wifiConnected && protocolConfig.heos_enabled && audioStreamer) {
        Serial.println("\n=== HEOS Initialization ===");
        heosPlayer = new HEOSPlayer(audioOutput, *audioStreamer);
        if (heosPlayer->begin(deviceConfig.name.c_str())) {
            Serial.println("✓ HEOS player ready");
            Serial.printf("Device name: '%s'\n", deviceConfig.name.c_str());
            Serial.printf("Command port: %d\n", protocolConfig.heos_command_port);
            Serial.printf("Stream port: %d\n", protocolConfig.heos_stream_port);
            Serial.println("Search for device in HEOS app!");
            Serial.println("Full audio streaming enabled!");
        } else {
            Serial.println("✗ HEOS initialization failed!");
            delete heosPlayer;
            heosPlayer = nullptr;
        }
        
        if (audioTaskHandle) vTaskResume(audioTaskHandle);
    } else if (!wifiConnected) {
        Serial.println("\n=== HEOS Skipped (No WiFi) ===");
    } else if (!audioStreamer) {
        Serial.println("\n=== HEOS Skipped (No Audio Streamer) ===");
    } else {
        Serial.println("\n=== HEOS Disabled ===");
    }

    Serial.println("\n╔═══════════════════════════════════════════════╗");
    Serial.println("║          SYSTEM READY!                        ║");
    Serial.println("╚═══════════════════════════════════════════════╝");
    Serial.println();

    printStatus();

    Serial.println("Monitoring connections...\n");
    Serial.println("TIP: Edit data/config.json to change WiFi settings\n");
}

void loop() {
    // Handle DLNA requests
    if (dlnaRenderer) {
        dlnaRenderer->handle();
    }

    // Handle HEOS protocol
    if (heosPlayer) {
        heosPlayer->handle();
    }

    // Check WiFi connection and reconnect if needed
    if (wifiConnected && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost! Reconnecting...");
        WiFi.reconnect();
        delay(WIFI_RECONNECT_DELAY_MS);
    }

    // Print status periodically
    if (millis() - lastStatusPrint > STATUS_PRINT_INTERVAL_MS) {
        printStatus();
        lastStatusPrint = millis();
    }

    // Serial Test Mode: Frequency Modulation (FM) via DAC
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'f' || cmd == 'F') {
            Serial.println("\n[TEST] Starting FM Tone (Vibrato) via DAC...");
            
            // Stop background streaming to prevent I2S conflict
            if (audioTaskHandle) vTaskSuspend(audioTaskHandle);
            if (audioStreamer) audioStreamer->stopStream();

            Serial.printf("Base: %.1fHz, Mod: %.1fHz, Dev: %.1fHz\n", 
                          TEST_SINE_FREQ, TEST_FM_MOD_FREQ, TEST_FM_DEVIATION);
            
            float carrierPhase = 0;
            float modPhase = 0;
            
            // Pre-calculate fixed increments
            float modPhaseInc = (2.0f * PI * TEST_FM_MOD_FREQ) / SAMPLE_RATE;
            
            unsigned long start = millis();
            while (millis() - start < TEST_DURATION_MS) {
                // FM Synthesis: Freq = Base + (Deviation * sin(ModPhase))
                float instantFreq = TEST_SINE_FREQ + (TEST_FM_DEVIATION * sin(modPhase));
                float carrierPhaseInc = (2.0f * PI * instantFreq) / SAMPLE_RATE;

                // Generate 16-bit PCM sample
                int16_t val = (int16_t)(sin(carrierPhase) * TEST_SINE_VOLUME * 32767);
                int16_t sample[2] = {val, val}; // Stereo (Left/Right)

                // Send to DAC via audio output object
                audioOutput.consumeSample(sample);

                // Update phases
                carrierPhase += carrierPhaseInc;
                if (carrierPhase >= 2.0f * PI) carrierPhase -= 2.0f * PI;
                
                modPhase += modPhaseInc;
                if (modPhase >= 2.0f * PI) modPhase -= 2.0f * PI;

                yield(); // Prevent watchdog reset and handle background tasks
            }
            Serial.println("[TEST] FM Tone complete. Resuming services.");
            if (audioTaskHandle) vTaskResume(audioTaskHandle);
        } 
        else if (cmd == '+') {
            uint8_t current = audioOutput.getVolume();
            uint8_t next = (current >= 95) ? 100 : current + 5;
            audioOutput.setVolume(next);
            Serial.printf("[AUDIO] Volume UP: %d%%\n", next);
        } 
        else if (cmd == '-') {
            uint8_t current = audioOutput.getVolume();
            uint8_t next = (current <= 5) ? 0 : current - 5;
            audioOutput.setVolume(next);
            Serial.printf("[AUDIO] Volume DOWN: %d%%\n", next);
        }
        else if (cmd == 'v' || cmd == 'V') {
            // Emergency volume reset to 70%
            audioOutput.setVolume(70);
            Serial.println("[AUDIO] Emergency volume reset to 70%");
        }
    }

    // Small delay to prevent watchdog issues
    delay(10);
}
