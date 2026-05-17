# HEOS Protocol Implementation Guide

This document explains how to use the HEOS protocol support in your ESP32 speaker.

## What is HEOS?

HEOS (Home Entertainment Operating System) is Denon's proprietary wireless multi-room audio protocol. It allows your ESP32 speaker to:

- Be discovered by HEOS apps and Denon receivers
- Receive streaming audio commands
- Be controlled remotely (play, pause, volume, etc.)
- Appear as a HEOS-compatible speaker in multi-room setups

## Supported HEOS Apps

Your ESP32 speaker will work with:
- **HEOS App** (iOS/Android) - Official Denon HEOS app
- **Denon AVR Receivers** with HEOS support (2016+)
- **Marantz Receivers** with HEOS support

## Network Ports Used

The HEOS implementation uses the following ports:
- **Port 1255**: HEOS command/control protocol (TCP)
- **Port 8081**: HEOS audio streaming (HTTP)

Make sure these ports are not blocked by your firewall.

## How to Use

### 1. Connect to WiFi

First, configure your WiFi credentials in `include/config.h`:

```cpp
#define WIFI_SSID      "YourWiFiNetwork"
#define WIFI_PASSWORD  "YourPassword"
```

The ESP32 must be on the same network as your HEOS controller.

### 2. Power On and Initialize

When you power on your ESP32, you'll see in the serial monitor:

```
=== HEOS Initialization ===
✓ HEOS player ready
Device name: 'ESP32-Speaker'
Command port: 1255
Stream port: 8081
Search for device in HEOS app!
```

### 3. Discover in HEOS App

**On HEOS App (iOS/Android):**
1. Open the HEOS app
2. Go to Settings → Add Device
3. Look for "ESP32-Speaker" (or your custom device name)
4. Select it to add to your system

**On Denon AVR:**
1. Go to HEOS menu
2. Select "Browse HEOS Music"
3. Your ESP32 speaker should appear in available players

### 4. Control the Speaker

Once connected, you can:
- **Play/Pause/Stop**: Control playback state
- **Volume Control**: Adjust volume (0-100%)
- **Mute/Unmute**: Mute the speaker
- **Stream Music**: Play music from HEOS sources

## Supported HEOS Commands

The ESP32 implements the following HEOS protocol commands:

### Player Commands
- `player/get_players` - List available players
- `player/get_player_info` - Get player information
- `player/get_play_state` - Get current play state (play/pause/stop)
- `player/set_play_state` - Control playback
- `player/get_volume` - Get current volume
- `player/set_volume` - Set volume (0-100)
- `player/get_mute` - Get mute state
- `player/set_mute` - Mute/unmute
- `player/get_now_playing_media` - Get current track info
- `player/set_play_mode` - Set repeat/shuffle mode

### Browse Commands
- `browse/get_music_sources` - List available music sources

## HEOS Protocol Format

HEOS uses a text-based protocol over TCP. Commands follow this format:

```
heos://command/subcommand?param1=value1&param2=value2
```

Example commands:
```
heos://player/get_players
heos://player/set_volume?pid=12345&level=50
heos://player/set_play_state?pid=12345&state=play
```

Responses follow this format:
```
heos://command/subcommand?result=success&message={json_data}
```

## Audio Streaming

HEOS audio streaming works via HTTP:

1. Controller sends stream URL to ESP32
2. ESP32 fetches audio from URL
3. Audio is decoded and played via I2S

Current implementation handles the control protocol. Full audio streaming requires additional HTTP client implementation for fetching remote streams.

## Testing HEOS Commands Manually

You can test HEOS commands using telnet:

```bash
telnet <ESP32_IP> 1255
```

Then send commands:
```
heos://player/get_players
heos://player/get_play_state?pid=<your_player_id>
heos://player/set_volume?pid=<your_player_id>&level=75
```

## Debugging

Enable HEOS debug output by checking the serial monitor. You'll see:
```
HEOS: Client connected from 192.168.1.100
HEOS Command: heos://player/get_players
HEOS Response: heos://player/get_players?result=success&message={...}
```

## Troubleshooting

### Device Not Discovered

**Check network:**
```
- ESP32 and HEOS controller on same network
- No AP isolation enabled on router
- Ports 1255 and 8081 not blocked
```

**Check ESP32:**
```
- WiFi connected (check serial monitor)
- HEOS initialization successful
- IP address assigned
```

**Try manual connection:**
```
- Note ESP32 IP address from serial monitor
- In HEOS app, try adding by IP address
```

### Connection Drops

```
- Check WiFi signal strength
- Reduce distance to router
- Disable WiFi power saving on ESP32
```

### No Audio Playback

The current implementation handles HEOS control protocol but requires additional work for full audio streaming:

1. HTTP client to fetch remote streams
2. Audio decoder (MP3/AAC/FLAC)
3. Buffer management

For now, use Bluetooth or DLNA for actual audio playback while HEOS provides the control interface.

## Technical Details

### Player ID

Each HEOS player has a unique ID (PID) based on the ESP32 MAC address:
```cpp
playerID = "pid_" + WiFi.macAddress();
```

### Events

The ESP32 sends HEOS events to notify controllers of state changes:
- `event/player_state_changed` - Play/pause/stop
- `event/player_volume_changed` - Volume changes
- `event/player_mute_changed` - Mute state changes
- `event/player_heartbeat` - Keepalive (every 5 seconds)

### JSON Message Format

HEOS uses JSON for structured data in responses:
```json
{
  "name": "ESP32-Speaker",
  "pid": "pid_AABBCCDDEEFF",
  "model": "ESP32 HEOS Speaker",
  "version": "1.0.0",
  "ip": "192.168.1.100",
  "network": "wired"
}
```

## Integration with Bluetooth and DLNA

The ESP32 speaker supports three protocols simultaneously:

- **Bluetooth**: Direct pairing for phone/tablet audio
- **DLNA**: Network streaming via UPnP
- **HEOS**: Denon multi-room control and streaming

All three protocols share the same audio output (I2S), but only one can play at a time.

## Limitations

Current implementation limitations:
- **Basic control only**: Play/pause/volume/mute commands work
- **No audio streaming**: HTTP streaming not yet implemented
- **No grouping**: Multi-room sync not supported
- **No playlists**: Queue management not implemented
- **No favorites**: Favorites/presets not supported

## Future Enhancements

Planned features:
- [ ] Full HTTP audio streaming
- [ ] Multi-format decoder (MP3, AAC, FLAC)
- [ ] Multi-room synchronization
- [ ] Playlist and queue management
- [ ] HEOS favorites support
- [ ] Album art display
- [ ] Search functionality

## Resources

- [HEOS CLI Protocol Specification](https://denon.com/heos)
- [HEOS API Documentation](https://rn.dmglobal.com/euheos/HEOS_CLI_ProtocolSpecification.pdf)
- [Denon HEOS Products](https://www.denon.com/en-us/heos)

## Support

For HEOS-specific issues:
1. Check serial monitor for connection logs
2. Verify network connectivity
3. Try manual telnet connection to port 1255
4. Check HEOS app logs/diagnostics

---

**HEOS Implementation for ESP32** - Compatible with Denon & Marantz HEOS systems 🎵
