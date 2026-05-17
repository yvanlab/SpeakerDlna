/**
 * HEOS Protocol Implementation
 */

#include "HEOSPlayer.h"
#include "config.h"

HEOSPlayer::HEOSPlayer(I2SAudioOutput& audioOut, AudioStreamer& streamer)
    : audioOutput(audioOut), audioStreamer(streamer), commandServer(nullptr), streamServer(nullptr),
      state(HEOS_STATE_STOP), volume(50), muted(false),
      initialized(false), lastHeartbeat(0) {

    // Generate unique player ID based on MAC address
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    playerID = "pid_" + mac;
    playerSerial = ESP.getEfuseMac();
}

HEOSPlayer::~HEOSPlayer() {
    end();
}

bool HEOSPlayer::begin(const char* name) {
    if (initialized) {
        return true;
    }

    playerName = String(name);

    Serial.println("Initializing HEOS Player...");

    // Create command server (TCP port 1255)
    commandServer = new WiFiServer(HEOS_PORT);
    commandServer->begin();
    Serial.printf("HEOS command server started on port %d\n", HEOS_PORT);

    // Create HTTP streaming server
    streamServer = new WebServer(HEOS_HTTP_PORT);

    streamServer->on("/stream", HTTP_GET, [this]() { handleStreamRequest(); });
    streamServer->on("/heartbeat", HTTP_GET, [this]() { handleHeartbeat(); });

    streamServer->begin();
    Serial.printf("HEOS stream server started on port %d\n", HEOS_HTTP_PORT);

    initialized = true;
    state = HEOS_STATE_STOP;

    Serial.printf("HEOS Player initialized: %s (ID: %s)\n",
                  playerName.c_str(), playerID.c_str());

    return true;
}

void HEOSPlayer::end() {
    if (initialized) {
        if (commandServer) {
            commandServer->stop();
            delete commandServer;
            commandServer = nullptr;
        }
        if (streamServer) {
            streamServer->stop();
            delete streamServer;
            streamServer = nullptr;
        }
        if (commandClient) {
            commandClient.stop();
        }
        initialized = false;
        Serial.println("HEOS Player stopped");
    }
}

void HEOSPlayer::handle() {
    if (!initialized) return;

    // Handle command connections
    handleCommandConnection();

    // Handle HTTP requests
    if (streamServer) {
        streamServer->handleClient();
    }

    // Send periodic heartbeat
    if (millis() - lastHeartbeat > 5000) {
        sendHeartbeat();
        lastHeartbeat = millis();
    }
}

void HEOSPlayer::handleCommandConnection() {
    // Check for new client connections
    if (commandServer->hasClient()) {
        if (commandClient && commandClient.connected()) {
            // Already have a client, reject new one
            WiFiClient newClient = commandServer->available();
            newClient.stop();
            Serial.println("HEOS: Rejected new client (already connected)");
        } else {
            commandClient = commandServer->available();
            Serial.printf("HEOS: Client connected from %s\n",
                         commandClient.remoteIP().toString().c_str());

            // Send initial event
            JsonDocument eventDoc;
            eventDoc["pid"] = playerID;
            sendEvent("event/player_state_changed", eventDoc);
        }
    }

    // Handle existing client
    if (commandClient && commandClient.connected()) {
        while (commandClient.available()) {
            String command = commandClient.readStringUntil('\n');
            command.trim();
            if (command.length() > 0) {
                Serial.printf("HEOS Command: %s\n", command.length() > 100 ?
                             (command.substring(0, 100) + "...").c_str() : command.c_str());
                handleCommand(command);
            }
        }
    } else if (commandClient) {
        // Client disconnected
        commandClient.stop();
        Serial.println("HEOS: Client disconnected");
    }
}

void HEOSPlayer::handleCommand(const String& command) {
    // Parse HEOS command format: heos://command/subcommand?param1=value1&param2=value2

    if (!command.startsWith("heos://")) {
        Serial.println("HEOS: Invalid command format");
        return;
    }

    // Extract command and parameters
    int queryStart = command.indexOf('?');
    String cmdPath = (queryStart > 0) ? command.substring(7, queryStart) : command.substring(7);
    String queryString = (queryStart > 0) ? command.substring(queryStart + 1) : "";

    // Parse parameters
    JsonDocument params;
    if (queryString.length() > 0) {
        int pos = 0;
        while (pos < queryString.length()) {
            int eqPos = queryString.indexOf('=', pos);
            int ampPos = queryString.indexOf('&', pos);

            if (eqPos > 0) {
                String key = queryString.substring(pos, eqPos);
                String value;

                if (ampPos > eqPos) {
                    value = queryString.substring(eqPos + 1, ampPos);
                    pos = ampPos + 1;
                } else {
                    value = queryString.substring(eqPos + 1);
                    pos = queryString.length();
                }

                params[key] = urlDecode(value);
            } else {
                break;
            }
        }
    }

    // Route to appropriate handler
    if (cmdPath == "player/get_players") {
        handlePlayerGetPlayers(params);
    } else if (cmdPath == "player/get_player_info") {
        handlePlayerGetPlayerInfo(params);
    } else if (cmdPath == "player/get_play_state") {
        handlePlayerGetPlayState(params);
    } else if (cmdPath == "player/set_play_state") {
        handlePlayerSetPlayState(params);
    } else if (cmdPath == "player/get_volume") {
        handlePlayerGetVolume(params);
    } else if (cmdPath == "player/set_volume") {
        handlePlayerSetVolume(params);
    } else if (cmdPath == "player/get_mute") {
        handlePlayerGetMute(params);
    } else if (cmdPath == "player/set_mute") {
        handlePlayerSetMute(params);
    } else if (cmdPath == "player/get_now_playing_media") {
        handlePlayerGetNowPlayingMedia(params);
    } else if (cmdPath == "player/set_play_mode") {
        handlePlayerSetPlayMode(params);
    } else if (cmdPath == "browse/get_music_sources") {
        handleBrowseGetSources(params);
    } else {
        Serial.printf("HEOS: Unknown command: %s\n", cmdPath.c_str());

        // Send generic success response
        JsonDocument msg;
        sendResponse(generateResponse(cmdPath, "success", msg));
    }
}

void HEOSPlayer::handlePlayerGetPlayers(JsonDocument& params) {
    JsonDocument msg;
    JsonArray players = msg["payload"].to<JsonArray>();

    JsonObject player = players.add<JsonObject>();
    player["name"] = playerName;
    player["pid"] = playerID;
    player["model"] = "ESP32 HEOS Speaker";
    player["version"] = "1.0.0";
    player["ip"] = WiFi.localIP().toString();
    player["network"] = "wired";
    player["lineout"] = 0;
    player["serial"] = String(playerSerial);

    sendResponse(generateResponse("player/get_players", "success", msg));
}

void HEOSPlayer::handlePlayerGetPlayerInfo(JsonDocument& params) {
    JsonDocument msg;
    msg["name"] = playerName;
    msg["pid"] = playerID;
    msg["model"] = "ESP32 HEOS Speaker";
    msg["version"] = "1.0.0";
    msg["ip"] = WiFi.localIP().toString();
    msg["network"] = "wired";
    msg["lineout"] = 0;

    sendResponse(generateResponse("player/get_player_info", "success", msg));
}

void HEOSPlayer::handlePlayerGetPlayState(JsonDocument& params) {
    JsonDocument msg;
    msg["pid"] = playerID;

    String stateStr;
    switch (state) {
        case HEOS_STATE_PLAY:  stateStr = "play"; break;
        case HEOS_STATE_PAUSE: stateStr = "pause"; break;
        case HEOS_STATE_STOP:
        default:               stateStr = "stop"; break;
    }
    msg["state"] = stateStr;

    sendResponse(generateResponse("player/get_play_state", "success", msg));
}

void HEOSPlayer::handlePlayerSetPlayState(JsonDocument& params) {
    String stateStr = params["state"].as<String>();

    if (stateStr == "play") {
        play();
    } else if (stateStr == "pause") {
        pause();
    } else if (stateStr == "stop") {
        stop();
    }

    JsonDocument msg;
    msg["pid"] = playerID;
    msg["state"] = stateStr;

    sendResponse(generateResponse("player/set_play_state", "success", msg));

    // Send state changed event
    sendEvent("event/player_state_changed", msg);
}

void HEOSPlayer::handlePlayerGetVolume(JsonDocument& params) {
    JsonDocument msg;
    msg["pid"] = playerID;
    msg["level"] = volume;

    sendResponse(generateResponse("player/get_volume", "success", msg));
}

void HEOSPlayer::handlePlayerSetVolume(JsonDocument& params) {
    if (params["level"].is<int>()) {
        int newVolume = params["level"].as<int>();
        setVolume(constrain(newVolume, 0, 100));
    }

    JsonDocument msg;
    msg["pid"] = playerID;
    msg["level"] = volume;

    sendResponse(generateResponse("player/set_volume", "success", msg));

    // Send volume changed event
    sendEvent("event/player_volume_changed", msg);
}

void HEOSPlayer::handlePlayerGetMute(JsonDocument& params) {
    JsonDocument msg;
    msg["pid"] = playerID;
    msg["state"] = muted ? "on" : "off";

    sendResponse(generateResponse("player/get_mute", "success", msg));
}

void HEOSPlayer::handlePlayerSetMute(JsonDocument& params) {
    String muteState = params["state"].as<String>();
    muted = (muteState == "on");

    JsonDocument msg;
    msg["pid"] = playerID;
    msg["state"] = muted ? "on" : "off";

    sendResponse(generateResponse("player/set_mute", "success", msg));

    // Send mute changed event
    sendEvent("event/player_mute_changed", msg);
}

void HEOSPlayer::handlePlayerGetNowPlayingMedia(JsonDocument& params) {
    JsonDocument msg;
    msg["type"] = "station";

    // Get metadata from audio streamer
    String title = audioStreamer.getTitle();
    String artist = audioStreamer.getArtist();
    String album = audioStreamer.getAlbum();

    msg["song"] = title.length() > 0 ? title : (currentTrackTitle.length() > 0 ? currentTrackTitle : "No Media");
    msg["album"] = album.length() > 0 ? album : currentTrackAlbum;
    msg["artist"] = artist.length() > 0 ? artist : currentTrackArtist;
    msg["image_url"] = "";
    msg["album_id"] = "";
    msg["mid"] = "1";
    msg["qid"] = "1";
    msg["sid"] = "1";

    sendResponse(generateResponse("player/get_now_playing_media", "success", msg));
}

void HEOSPlayer::handlePlayerSetPlayMode(JsonDocument& params) {
    JsonDocument msg;
    msg["pid"] = playerID;
    msg["repeat"] = params["repeat"];
    msg["shuffle"] = params["shuffle"];

    sendResponse(generateResponse("player/set_play_mode", "success", msg));
}

void HEOSPlayer::handleBrowseGetSources(JsonDocument& params) {
    JsonDocument msg;
    JsonArray sources = msg["payload"].to<JsonArray>();

    JsonObject source1 = sources.add<JsonObject>();
    source1["name"] = "Bluetooth";
    source1["image_url"] = "";
    source1["type"] = "music_service";
    source1["sid"] = "1";
    source1["available"] = "true";
    source1["service_username"] = "";

    JsonObject source2 = sources.add<JsonObject>();
    source2["name"] = "DLNA";
    source2["image_url"] = "";
    source2["type"] = "music_service";
    source2["sid"] = "2";
    source2["available"] = "true";
    source2["service_username"] = "";

    sendResponse(generateResponse("browse/get_music_sources", "success", msg));
}

String HEOSPlayer::generateResponse(const String& command, const String& result, JsonDocument& message) {
    String response = "heos://" + command + "?";

    // Add result
    response += "result=" + result;

    // Add message if exists
    if (!message.isNull() && message.size() > 0) {
        response += "&message=";
        String msgStr;
        serializeJson(message, msgStr);
        response += msgStr;
    }

    response += "\r\n";
    return response;
}

String HEOSPlayer::generateEvent(const String& command, JsonDocument& message) {
    return generateResponse(command, "", message);
}

void HEOSPlayer::sendResponse(const String& response) {
    if (commandClient && commandClient.connected()) {
        commandClient.print(response);
        Serial.printf("HEOS Response: %s", response.length() > 100 ?
                     (response.substring(0, 100) + "...\n").c_str() : response.c_str());
    }
}

void HEOSPlayer::sendEvent(const String& event, JsonDocument& message) {
    String eventMsg = generateEvent(event, message);
    sendResponse(eventMsg);
}

void HEOSPlayer::sendHeartbeat() {
    if (commandClient && commandClient.connected()) {
        JsonDocument msg;
        msg["pid"] = playerID;
        sendEvent("event/player_heartbeat", msg);
    }
}

void HEOSPlayer::handleStreamRequest() {
    Serial.println("HEOS: Stream request received");

    // Get URL parameter
    if (streamServer->hasArg("url")) {
        currentStreamURL = streamServer->arg("url");
        Serial.printf("HEOS: Stream URL: %s\n", currentStreamURL.c_str());

        // Start audio streaming
        if (audioStreamer.startStream(currentStreamURL)) {
            streamServer->send(200, "text/plain", "Stream started");
            state = HEOS_STATE_PLAY;

            // Send play state event
            JsonDocument msg;
            msg["pid"] = playerID;
            msg["state"] = "play";
            sendEvent("event/player_state_changed", msg);
        } else {
            streamServer->send(500, "text/plain", "Failed to start stream");
            state = HEOS_STATE_STOP;
        }
    } else {
        streamServer->send(400, "text/plain", "Missing URL parameter");
    }
}

void HEOSPlayer::handleHeartbeat() {
    streamServer->send(200, "text/plain", "OK");
}

void HEOSPlayer::play() {
    if (state == HEOS_STATE_PAUSE && currentStreamURL.length() > 0) {
        audioStreamer.resumeStream();
    } else if (currentStreamURL.length() > 0) {
        audioStreamer.startStream(currentStreamURL);
    }
    state = HEOS_STATE_PLAY;
    Serial.println("HEOS: Play");
}

void HEOSPlayer::pause() {
    if (state == HEOS_STATE_PLAY) {
        audioStreamer.pauseStream();
    }
    state = HEOS_STATE_PAUSE;
    Serial.println("HEOS: Pause");
}

void HEOSPlayer::stop() {
    audioStreamer.stopStream();
    state = HEOS_STATE_STOP;
    currentStreamURL = "";
    Serial.println("HEOS: Stop");
}

void HEOSPlayer::setVolume(uint8_t vol) {
    volume = constrain(vol, 0, 100);
    audioOutput.setVolume(volume);
    Serial.printf("HEOS: Volume set to %d%%\n", volume);
}

uint8_t HEOSPlayer::getVolume() {
    return volume;
}

bool HEOSPlayer::isRunning() {
    return initialized;
}

bool HEOSPlayer::isPlaying() {
    return state == HEOS_STATE_PLAY;
}

String HEOSPlayer::urlDecode(const String& str) {
    String decoded = "";
    char temp[] = "0x00";
    unsigned int len = str.length();

    for (unsigned int i = 0; i < len; i++) {
        char c = str.charAt(i);
        if (c == '+') {
            decoded += ' ';
        } else if (c == '%') {
            if (i + 2 < len) {
                temp[2] = str.charAt(i + 1);
                temp[3] = str.charAt(i + 2);
                decoded += (char)strtol(temp, NULL, 16);
                i += 2;
            }
        } else {
            decoded += c;
        }
    }

    return decoded;
}
