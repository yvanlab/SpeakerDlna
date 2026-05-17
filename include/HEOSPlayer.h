/**
 * HEOS Protocol Implementation
 * Denon's wireless multi-room audio protocol
 */

#ifndef HEOS_PLAYER_H
#define HEOS_PLAYER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "AudioOutput.h"
#include "AudioStreamer.h"

// HEOS Protocol Constants
#define HEOS_PORT           1255
#define HEOS_DISCOVERY_PORT 1900
#define HEOS_HTTP_PORT      8081

// Player States
enum HEOSPlayerState {
    HEOS_STATE_STOP = 0,
    HEOS_STATE_PLAY = 1,
    HEOS_STATE_PAUSE = 2
};

class HEOSPlayer {
public:
    HEOSPlayer(I2SAudioOutput& audioOut, AudioStreamer& streamer);
    ~HEOSPlayer();

    bool begin(const char* playerName);
    void end();
    void handle();

    bool isRunning();
    bool isPlaying();

    // Player control
    void play();
    void pause();
    void stop();
    void setVolume(uint8_t volume);
    uint8_t getVolume();

private:
    I2SAudioOutput& audioOutput;
    AudioStreamer& audioStreamer;
    WiFiServer* commandServer;
    WiFiClient commandClient;
    WebServer* streamServer;

    String playerName;
    String playerID;
    uint32_t playerSerial;

    HEOSPlayerState state;
    uint8_t volume;
    bool muted;

    String currentStreamURL;
    String currentTrackArtist;
    String currentTrackAlbum;
    String currentTrackTitle;

    bool initialized;
    unsigned long lastHeartbeat;

    // Command handling
    void handleCommandConnection();
    void handleCommand(const String& command);
    void sendResponse(const String& response);
    void sendEvent(const String& event, JsonDocument& message);

    // Command processors
    void handlePlayerGetPlayers(JsonDocument& params);
    void handlePlayerGetPlayerInfo(JsonDocument& params);
    void handlePlayerGetPlayState(JsonDocument& params);
    void handlePlayerSetPlayState(JsonDocument& params);
    void handlePlayerGetVolume(JsonDocument& params);
    void handlePlayerSetVolume(JsonDocument& params);
    void handlePlayerGetMute(JsonDocument& params);
    void handlePlayerSetMute(JsonDocument& params);
    void handlePlayerGetNowPlayingMedia(JsonDocument& params);
    void handlePlayerSetPlayMode(JsonDocument& params);
    void handleBrowseGetSources(JsonDocument& params);

    // HTTP streaming handlers
    void handleStreamRequest();
    void handleHeartbeat();

    // Utility
    String generateResponse(const String& command, const String& result, JsonDocument& message);
    String generateEvent(const String& command, JsonDocument& message);
    void sendHeartbeat();
    String urlDecode(const String& str);
};

#endif // HEOS_PLAYER_H
