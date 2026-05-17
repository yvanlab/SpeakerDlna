# Audio Streaming Guide

Complete guide for using HEOS audio streaming with your ESP32 speaker.

## Overview

The ESP32 speaker now supports **full audio streaming** via HEOS protocol, including:

- ✅ HTTP/HTTPS audio streams
- ✅ MP3 format
- ✅ AAC format
- ✅ M4A format
- ✅ FLAC format (lossless)
- ✅ Stream metadata (title, artist, album)
- ✅ Play/Pause/Stop controls
- ✅ Volume control
- ✅ Buffering management

## Supported Audio Formats

### Lossy Formats
- **MP3**: MPEG-1/2 Layer 3 (most common)
- **AAC**: Advanced Audio Coding (better quality than MP3 at same bitrate)
- **M4A**: MPEG-4 Audio (iTunes format)

### Lossless Format
- **FLAC**: Free Lossless Audio Codec (CD quality)

### Streaming Protocols
- **HTTP**: Standard web streams
- **HTTPS**: Secure streams (SSL/TLS)
- **Progressive Download**: Fetch-as-you-play

## How It Works

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│  HEOS App       │ ─────>│  ESP32 Speaker  │ ─────>│  Audio Stream   │
│  (Controller)   │       │  (Player)       │       │  (Internet)     │
└─────────────────┘       └─────────────────┘       └─────────────────┘
        │                          │                          │
        │  1. Send stream URL      │                          │
        │─────────────────────────>│                          │
        │                          │                          │
        │                          │  2. Fetch audio data     │
        │                          │─────────────────────────>│
        │                          │                          │
        │                          │  3. Decode & Play        │
        │                          │◄─────────────────────────│
        │                          │                          │
        │  4. Metadata & Status    │                          │
        │◄─────────────────────────│                          │
```

## Using HEOS Streaming

### From HEOS App

1. **Open HEOS app** on your phone/tablet
2. **Add your ESP32 speaker** (see HEOS_PROTOCOL.md)
3. **Browse music sources:**
   - TIDAL
   - Spotify (via HEOS)
   - TuneIn Radio
   - Amazon Music
   - Deezer
   - Local music servers
4. **Select a track/station**
5. **Choose ESP32 speaker** as output
6. **Audio streams automatically!**

### From Denon/Marantz Receiver

1. **Select HEOS input** on receiver
2. **Browse available players**
3. **Find your ESP32 speaker**
4. **Send audio stream** to ESP32
5. **Control from receiver or app**

## Streaming Commands

### Via HEOS Protocol

You can control streaming programmatically:

**Start streaming:**
```
heos://player/set_play_state?pid=<player_id>&state=play&url=http://example.com/song.mp3
```

**Stop streaming:**
```
heos://player/set_play_state?pid=<player_id>&state=stop
```

**Pause streaming:**
```
heos://player/set_play_state?pid=<player_id>&state=pause
```

### HTTP Endpoint

Direct HTTP streaming via the stream endpoint:

```bash
# Start stream
curl "http://<ESP32_IP>:8081/stream?url=http://example.com/song.mp3"

# Stop stream
curl "http://<ESP32_IP>:8081/stop"
```

## Streaming Quality

### Bitrate Recommendations

| Format | Bitrate | Quality | Buffering |
|--------|---------|---------|-----------|
| MP3 128kbps | 128kbps | Good | Fast |
| MP3 192kbps | 192kbps | Better | Medium |
| MP3 320kbps | 320kbps | Best | Slower |
| AAC 128kbps | 128kbps | Better | Fast |
| AAC 256kbps | 256kbps | Excellent | Medium |
| FLAC | 800-1400kbps | Lossless | Slow |

**Recommendation:** Use 128-192kbps AAC or MP3 for best balance of quality and streaming performance.

### Network Requirements

| Quality | Min Speed | Recommended | WiFi |
|---------|-----------|-------------|------|
| Low (128k) | 256 kbps | 512 kbps | 2.4GHz OK |
| Medium (192k) | 384 kbps | 768 kbps | 2.4GHz OK |
| High (320k) | 640 kbps | 1 Mbps | 2.4GHz OK |
| Lossless | 1.5 Mbps | 3 Mbps | 5GHz Preferred |

## Buffering Behavior

The audio streamer includes smart buffering:

1. **Initial buffer:** 512KB before playback starts
2. **Continuous buffering:** Downloads while playing
3. **Re-buffering:** Automatic if connection drops
4. **Buffer overflow protection:** Prevents memory issues

### Buffer States

You'll see these states in serial monitor:

```
Stream state: CONNECTING   # Establishing connection
Stream state: BUFFERING    # Initial buffer filling
Stream state: PLAYING      # Audio playing
Stream state: PAUSED       # Playback paused
Stream state: STOPPED      # Playback stopped
Stream state: ERROR        # Connection/decode error
```

## Metadata Support

### What's Supported

- ✅ Song title
- ✅ Artist name
- ✅ Album name
- ✅ Stream URL
- ✅ Format detection

### Metadata Sources

1. **ID3 tags** (MP3 files)
2. **M4A metadata** (iTunes files)
3. **Icecast headers** (Internet radio)
4. **HTTP headers** (streaming services)

### Viewing Metadata

**Via HEOS command:**
```
heos://player/get_now_playing_media?pid=<player_id>
```

**Response:**
```json
{
  "song": "Bohemian Rhapsody",
  "artist": "Queen",
  "album": "A Night at the Opera",
  "type": "station"
}
```

## Streaming from Different Sources

### Internet Radio

```bash
# Example: BBC Radio 1
http://<ESP32_IP>:8081/stream?url=http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one

# Example: SomaFM
http://<ESP32_IP>:8081/stream?url=http://ice1.somafm.com/groovesalad-128-mp3
```

### Direct MP3 Files

```bash
http://<ESP32_IP>:8081/stream?url=http://example.com/music/song.mp3
```

### Streaming Services (via HEOS)

Streaming services are accessed through the HEOS app:
- Services handle authentication
- App sends authenticated stream URLs to ESP32
- ESP32 plays the stream

## Performance Considerations

### CPU Usage

| Format | CPU Usage | Notes |
|--------|-----------|-------|
| MP3 128kbps | ~30% | Very efficient |
| MP3 320kbps | ~40% | Efficient |
| AAC 256kbps | ~50% | Moderate |
| FLAC | ~70% | High CPU |

### Memory Usage

- **Audio buffer:** ~64KB
- **Stream buffer:** ~512KB
- **Decoder overhead:** ~128KB
- **Total overhead:** ~700KB

**Free heap required:** Minimum 1MB recommended

### WiFi Performance

**Tips for best streaming:**
- Place ESP32 near router for strong signal
- Use 2.4GHz for better range
- Avoid WiFi congestion (choose less crowded channel)
- Use Ethernet adapter if available

## Troubleshooting

### Stream Won't Start

**Check:**
1. WiFi is connected
2. URL is accessible (test in browser)
3. Format is supported (MP3, AAC, M4A, FLAC)
4. Enough free heap memory

**Serial output:**
```
Stream state: CONNECTING
Failed to connect to stream
Stream state: ERROR
```

**Solution:** Verify URL and network connectivity

### Audio Stuttering

**Causes:**
- Weak WiFi signal
- Slow internet connection
- High bitrate stream
- CPU overload

**Solutions:**
- Move closer to router
- Lower stream quality
- Reduce other tasks
- Check available heap

### No Metadata

**Some streams don't include metadata:**
- Check if stream supports ICY/Icecast metadata
- Internet radio usually has metadata
- Plain MP3 files may not

### Stream Drops/Disconnects

**Check:**
1. Network stability
2. Router buffer settings
3. Stream server availability
4. ESP32 heap memory

**Monitor:**
```
Free Heap: 150000 bytes  # Should stay above 100KB
Stream state: BUFFERING  # Should be brief
```

## Advanced Streaming

### Custom Stream Headers

Modify `AudioStreamer.cpp` to add custom HTTP headers:

```cpp
audio->setConnectionParameter("Authorization", "Bearer YOUR_TOKEN");
audio->setConnectionParameter("User-Agent", "ESP32-Speaker/1.0");
```

### Stream Proxies

Use a proxy server for geoblocked content:

```bash
# Use local proxy
http://<ESP32_IP>:8081/stream?url=http://proxy-server/stream.mp3
```

### Playlist Support

M3U/PLS playlist parsing can be added:

```cpp
// Future feature
audio->connecttoplaylist("http://example.com/playlist.m3u");
```

## Codec Details

### MP3 Decoder
- **Decoder:** Helix MP3 (built-in)
- **Sample rates:** 8-48 kHz
- **Channels:** Mono/Stereo
- **Bitrates:** 32-320 kbps

### AAC Decoder
- **Decoder:** FDK-AAC compatible
- **Sample rates:** 8-96 kHz
- **Channels:** Up to 5.1 (downmixed to stereo)
- **Profiles:** LC, HE-AAC, HE-AACv2

### FLAC Decoder
- **Decoder:** libFLAC compatible
- **Sample rates:** 8-192 kHz
- **Bit depth:** 8-32 bits
- **Channels:** Mono/Stereo

## Best Practices

### For Best Quality
1. Use AAC 256kbps or MP3 320kbps
2. Strong WiFi signal (-60 dBm or better)
3. Wired Ethernet if possible
4. Close browser/apps when streaming

### For Reliability
1. Use lower bitrates (128-192kbps)
2. Check available heap regularly
3. Monitor WiFi signal strength
4. Avoid simultaneous Bluetooth/DLNA

### For Battery Life (if portable)
1. Use lower bitrates
2. Disable unused protocols
3. Reduce volume
4. Use WiFi power save mode

## Examples

### Streaming Internet Radio

```cpp
// In your code
audioStreamer->startStream("http://ice1.somafm.com/groovesalad-128-mp3");
```

### Streaming from Local Server

```cpp
// Plex, Jellyfin, or simple HTTP server
audioStreamer->startStream("http://192.168.1.100:8096/audio.mp3");
```

### Control from HEOS App

```
1. Open HEOS app
2. Select "TuneIn Radio"
3. Choose station
4. Select ESP32-Speaker as player
5. Audio streams automatically
```

## Performance Monitoring

Watch serial output for streaming health:

```
Stream state: PLAYING
Free Heap: 180000 bytes
Now Playing: Artist Name - Song Title
WiFi: Connected (-45 dBm) - 192.168.1.100
```

## Resources

- [ESP32-audioI2S Library](https://github.com/schreibfaul1/ESP32-audioI2S)
- [Audio Format Reference](https://en.wikipedia.org/wiki/Audio_coding_format)
- [Helix MP3 Decoder](https://github.com/ultraembedded/libhelix-mp3)
- [Internet Radio Stations](http://dir.xiph.org/)

---

**Full Audio Streaming Powered by ESP32-audioI2S** 🎵
