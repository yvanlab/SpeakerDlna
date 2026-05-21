/**
 * Audio Streamer - HTTP audio streaming (simplified)
 */

#ifndef AUDIO_STREAMER_H
#define AUDIO_STREAMER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include "AudioOutput.h"
#include "AudioFileSource.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorAAC.h"
#include "AudioGeneratorWAV.h"
#include "AudioGeneratorFLAC.h"
#include "AudioOutputI2S.h"

enum StreamState {
    STREAM_STOPPED,
    STREAM_CONNECTING,
    STREAM_BUFFERING,
    STREAM_PLAYING,
    STREAM_PAUSED,
    STREAM_ERROR
};

// Forward declaration of bridge class
class AudioOutputBridge;

class AudioStreamer {
public:
    AudioStreamer(I2SAudioOutput& audioOut);
    ~AudioStreamer();

    bool begin();
    void end();
    void loop();

    // Stream control
    bool startStream(const String& url);
    void stopStream();
    void pauseStream();
    void resumeStream();

    // Status
    StreamState getState() const { return state; }
    bool isStreaming() const { return state == STREAM_PLAYING; }
    String getCurrentURL() const { return currentURL; }

    // Metadata
    void setMetadata(const String& title, const String& artist, const String& album);
    String getTitle() const { return trackTitle; }
    String getArtist() const { return trackArtist; }
    String getAlbum() const { return trackAlbum; }

    /**
     * Playback progress (in milliseconds).
     */
    uint32_t getAudioPos();
    uint32_t getAudioDuration();

private:
    I2SAudioOutput& audioOutput;
    AudioFileSource* audioSource;
    AudioGenerator* audioDecoder;
    AudioOutputBridge* audioOutputBridge;  // Concrete type to avoid unsafe casting

    StreamState state;
    String currentURL;
    String trackTitle;
    String trackArtist;
    String trackAlbum;

    bool initialized;
    unsigned long lastUpdate;

    void setState(StreamState newState);
    void cleanupResources();  // Centralized cleanup for error paths
};

#endif // AUDIO_STREAMER_H
