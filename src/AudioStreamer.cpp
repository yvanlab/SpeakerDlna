/**
 * Audio Streamer Implementation (simplified)
 */

#include "AudioStreamer.h"
#include "config.h"

// Bridge to route library samples through our custom I2SAudioOutput (DAC)
class AudioOutputBridge : public AudioOutput {
    I2SAudioOutput& _out;
    uint32_t _samplesPlayed;
public:
    AudioOutputBridge(I2SAudioOutput& out) : _out(out), _samplesPlayed(0) {}
    virtual bool begin() override { return true; }
    virtual bool ConsumeSample(int16_t sample[2]) override {
        _samplesPlayed++;
        _out.consumeSample(sample);
        return true;
    }
    virtual bool stop() override { return true; }
    uint32_t getSamplesPlayed() const { return _samplesPlayed; }
    void resetSamples() { _samplesPlayed = 0; }
};

AudioStreamer::AudioStreamer(I2SAudioOutput& audioOut)
    : audioOutput(audioOut), audioSource(nullptr), audioDecoder(nullptr), audioOutputDestination(nullptr),
      state(STREAM_STOPPED), initialized(false), lastUpdate(0) {
}

AudioStreamer::~AudioStreamer() {
    end();
}

bool AudioStreamer::begin() {
    if (initialized) {
        return true;
    }

    Serial.println("Initializing Audio Streamer (MP3 support)...");

    // Use bridge as a generic AudioOutput. The decoder only needs the base AudioOutput interface.
    // This avoids unsafe casting issues and ensures correct polymorphism.
    audioOutputDestination = new AudioOutputBridge(audioOutput);

    initialized = true;
    state = STREAM_STOPPED;

    Serial.println("Audio Streamer initialized (MP3 support)");
    return true;
}

void AudioStreamer::end() {
    stopStream();

    if (audioOutputDestination) {
        delete audioOutputDestination;
        audioOutputDestination = nullptr;
    }

    initialized = false;
    Serial.println("Audio Streamer stopped");
}

void AudioStreamer::loop() {
    if (!initialized) {
        return;
    }

    // Process audio stream
    if (state == STREAM_PLAYING && audioDecoder) {
        if (audioDecoder->isRunning()) {
            if (!audioDecoder->loop()) {
                // Decode error or end of stream
                Serial.println("Stream decode error or ended");
                stopStream();
            }
        } else {
            // Stream ended
            Serial.println("Stream ended");
            setState(STREAM_STOPPED);
        }
    }
}

bool AudioStreamer::startStream(const String& url) {
    if (!initialized) {
        Serial.println("Audio Streamer not initialized");
        return false;
    }

    Serial.printf("Starting stream: %s\n", url.c_str());

    // Stop current stream if playing
    if (state != STREAM_STOPPED) {
        stopStream();
        delay(100);
    }

    currentURL = url;
    setState(STREAM_CONNECTING);

    // Reset sample counter for accurate time tracking
    if (audioOutputDestination) {
        static_cast<AudioOutputBridge*>(audioOutputDestination)->resetSamples();
    }

    Serial.printf("Connecting to stream URL: %s\n", url.c_str());

    // Create HTTP source and add a buffer to handle network jitter
    AudioFileSourceHTTPStream* httpSrc = new AudioFileSourceHTTPStream();
    if (!httpSrc->open(url.c_str())) {
        Serial.println("Failed to open HTTP stream");
        delete httpSrc;
        setState(STREAM_ERROR);
        return false;
    }

    // Allocate a small buffer for the ESP32-C3 (modest size to save RAM)
    // This allows the decoder to stay fed during minor WiFi fluctuations
    audioSource = new AudioFileSourceBuffer(httpSrc, 32768); // Increased to 32KB for stability

    // Detect format from URL and create appropriate decoder
    String urlLower = url;
    urlLower.toLowerCase();

    if (urlLower.endsWith(".aac") || urlLower.indexOf("type=aac") >= 0) {
        Serial.println("Using AAC decoder");
        audioDecoder = new AudioGeneratorAAC();
    } else if (urlLower.endsWith(".wav")) {
        Serial.println("Using WAV decoder");
        audioDecoder = new AudioGeneratorWAV();
    } else if (urlLower.endsWith(".flac")) {
        Serial.println("Using FLAC decoder");
        audioDecoder = new AudioGeneratorFLAC();
    } else {
        // Default to MP3 for most streams
        Serial.println("Using MP3 decoder (default)");
        audioDecoder = new AudioGeneratorMP3();
    }
    
    // Note: audioI2SOutput is used as the destination
    if (!audioDecoder->begin(audioSource, audioOutputDestination)) {
        Serial.println("Failed to start decoder");
        stopStream();
        setState(STREAM_ERROR);
        return false;
    }

    Serial.println("Stream started");
    setState(STREAM_PLAYING);
    return true;
}

void AudioStreamer::stopStream() {
    if (audioDecoder) {
        audioDecoder->stop();
        delete audioDecoder;
        audioDecoder = nullptr;
    }
    
    // Explicitly clear the hardware buffer to stop sound immediately
    audioOutput.flush();

    if (audioSource) {
        audioSource->close();
        delete audioSource;
        audioSource = nullptr;
    }

    currentURL = "";
    trackTitle = "";
    trackArtist = "";
    trackAlbum = "";
    setState(STREAM_STOPPED);

    Serial.println("Stream stopped");
}

uint32_t AudioStreamer::getAudioPos() {
    if (state == STREAM_PLAYING && audioOutputDestination) {
        // Convert samples to milliseconds: (samples / 44100) * 1000
        return static_cast<AudioOutputBridge*>(audioOutputDestination)->getSamplesPlayed() / 44.1f;
    }
    return 0;
}

uint32_t AudioStreamer::getAudioDuration() {
    // For live streams, duration is often unknown (0)
    return audioSource ? audioSource->getSize() : 0; 
}

void AudioStreamer::pauseStream() {
    // Since we don't support resume, make Pause behave like Stop 
    // to ensure hardware and network resources are released.
    Serial.println("Pause requested - stopping stream (resume not supported)");
    stopStream();
    setState(STREAM_PAUSED);
}

void AudioStreamer::resumeStream() {
    // Would need to restart stream from beginning
    if (state == STREAM_PAUSED && currentURL.length() > 0) {
        Serial.println("Resume not supported, restart stream manually");
    }
}

void AudioStreamer::setMetadata(const String& title, const String& artist, const String& album) {
    trackTitle = title;
    trackArtist = artist;
    trackAlbum = album;

    Serial.printf("Metadata: %s - %s (%s)\n",
                  artist.c_str(), title.c_str(), album.c_str());
}

void AudioStreamer::setState(StreamState newState) {
    if (state != newState) {
        state = newState;

        const char* stateNames[] = {
            "STOPPED", "CONNECTING", "BUFFERING", "PLAYING", "PAUSED", "ERROR"
        };

        Serial.printf("Stream state: %s\n", stateNames[state]);
    }
}
