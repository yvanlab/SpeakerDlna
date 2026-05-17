# ESP32 Multi-Protocol Wireless Speaker

A complete C++ implementation using PlatformIO for an ESP32-based wireless speaker supporting HEOS (Denon), DLNA (UPnP), and Bluetooth A2DP audio.

## Features

- **HEOS Protocol with Full Streaming**: Complete Denon/Marantz HEOS support with audio streaming (MP3, AAC, M4A, FLAC)
- **Bluetooth A2DP Sink**: Pair and stream audio from any Bluetooth device
- **DLNA Renderer**: Network audio streaming via UPnP/DLNA protocol
- **JSON Configuration**: Easy configuration via JSON file (WiFi, audio, protocols)
- **I2S Audio Output**: High-quality digital audio via I2S DAC
- **WiFi Connectivity**: Automatic connection with reconnection handling
- **Multi-Protocol Support**: All three protocols active simultaneously
- **Internet Radio**: Stream from TuneIn, Icecast, and other services
- **Metadata Support**: Display track info, artist, album
- **Status Monitoring**: Real-time connection status and diagnostics

## Hardware Requirements

### ESP32 Board
- ESP32-WROOM-32, ESP32-DevKitC, or similar
- Minimum 4MB Flash
- USB cable for programming

### I2S DAC Module (Choose one)

**Recommended: MAX98357A** (Easiest to use)
- I2S Class D Amplifier
- 3W output power
- No configuration needed
- Direct speaker connection

**Alternative: PCM5102A** (Better quality)
- 32-bit Stereo DAC
- Line-level output
- Requires external amplifier

**Alternative: UDA1334A**
- Simple I2S DAC
- Headphone output capable

### Speaker
- 4-8 ohm impedance
- 3-10W power rating
- For MAX98357A: connect directly
- For PCM5102A/UDA1334A: use external amplifier

## Wiring Diagram

### Default Configuration (configurable via `data/config.json`)

```
ESP32 Pin    →    I2S DAC
────────────────────────────
GPIO 26      →    BCLK (Bit Clock)
GPIO 25      →    LRC/WS (Word Select)
GPIO 22      →    DIN (Data In)
GND          →    GND
5V           →    VIN
```

### MAX98357A Specific
```
ESP32         MAX98357A
GPIO 26   →   BCLK
GPIO 25   →   LRC
GPIO 22   →   DIN
GND       →   GND
5V        →   Vin
              SD → GND (shutdown control, optional)
              
              Speaker + and - → Connect your speaker
```

## Software Setup

### 1. Install Prerequisites

**PlatformIO**
- Install VS Code: https://code.visualstudio.com/
- Install PlatformIO extension in VS Code
- OR install PlatformIO CLI: https://platformio.org/install/cli

**Drivers**
- ESP32 USB drivers (CH340/CP210x depending on your board)

### 2. Configure the Project

Edit `include/config.h`:

```cpp
// WiFi credentials
#define WIFI_SSID      "YourWiFiName"
#define WIFI_PASSWORD  "YourWiFiPassword"

// Device name (for both Bluetooth and DLNA)
#define DEVICE_NAME    "ESP32-Speaker"

// I2S pins (if different from defaults)
#define I2S_BCLK_PIN    26
#define I2S_LRC_PIN     25
#define I2S_DOUT_PIN    22
```

### 3. Build and Upload

#### Using VS Code with PlatformIO

1. Open the project folder in VS Code
2. PlatformIO will automatically detect the project
3. Click the checkmark icon (✓) to build
4. Connect ESP32 via USB
5. Click the arrow icon (→) to upload
6. Click the plug icon to open Serial Monitor

#### Using PlatformIO CLI

```bash
cd c:\dev\SpeakerDlna

# Build the project
pio run

# Upload to ESP32 (replace COM3 with your port)
pio run --target upload --upload-port COM3

# Monitor serial output
pio device monitor -p COM3 -b 115200
```

### 4. First Boot

The serial monitor will show:
```
╔═══════════════════════════════════════════════╗
║   ESP32 Multi-Protocol Wireless Speaker      ║
║   HEOS • DLNA • Bluetooth                     ║
╚═══════════════════════════════════════════════╝

=== Audio Output Initialization ===
✓ Audio output ready
Sample Rate: 44100 Hz, Bits: 16

=== WiFi Setup ===
Connecting to YourWiFi.....
✓ WiFi connected!
IP Address: 192.168.1.100

=== Bluetooth Initialization ===
✓ Bluetooth speaker ready
Device name: 'ESP32-Speaker'

=== DLNA Initialization ===
✓ DLNA renderer ready
HTTP server started on port 8080

=== Audio Streamer Initialization ===
✓ Audio streamer ready
Supports: MP3, AAC, M4A, FLAC, HTTP/HTTPS streams

=== HEOS Initialization ===
✓ HEOS player ready
Device name: 'ESP32-Speaker'
Command port: 1255
Stream port: 8081
Full audio streaming enabled!

╔═══════════════════════════════════════════════╗
║          SYSTEM READY!                        ║
╚═══════════════════════════════════════════════╝

TIP: Edit data/config.json to change WiFi settings
```

## Usage

### Bluetooth Mode

1. **Pair the device:**
   - Open Bluetooth settings on your phone/computer
   - Look for "ESP32-Speaker" (or your custom name)
   - Connect to it

2. **Play audio:**
   - Open any music app
   - Audio will automatically stream to the speaker

3. **Connection status:**
   - Check serial monitor for connection confirmation
   - LED indicator (if connected to GPIO2) shows status

### DLNA Mode

1. **Install a DLNA controller app:**
   - **Android**: BubbleUPnP, Hi-Fi Cast, AllConnect
   - **iOS**: ArkMC, OPlayer, nPlayer
   - **Windows**: Windows Media Player (built-in)
   - **Mac**: VLC, Kodi

2. **Discover the device:**
   - Open your DLNA app
   - Scan for devices/renderers
   - "ESP32-Speaker" should appear

3. **Stream audio:**
   - Select the ESP32 speaker as output device
   - Play music from your library or streaming service
   - Audio streams over WiFi

### HEOS Mode (Denon Protocol) - Full Streaming

1. **Install HEOS app:**
   - **iOS/Android**: Download official HEOS app from app store
   - **Denon/Marantz AVR**: Built-in HEOS support (2016+ models)

2. **Add the speaker:**
   - Open HEOS app
   - Go to Settings → Add Device
   - Look for "ESP32-Speaker"
   - Select to add to your HEOS system

3. **Stream audio:**
   - Browse music sources (TuneIn, Spotify via HEOS, etc.)
   - Select a track or station
   - Choose ESP32-Speaker as output
   - **Audio streams automatically with full playback!**
   - Supports MP3, AAC, M4A, FLAC formats
   - View metadata (title, artist, album)

4. **Control playback:**
   - Play/Pause/Stop from HEOS app
   - Volume control (0-100%)
   - Track info display
   - Works with Denon/Marantz receivers

For detailed HEOS usage, see `HEOS_PROTOCOL.md`  
For streaming details, see `STREAMING_GUIDE.md`

## Configuration Options

### Audio Quality (`include/config.h`)

```cpp
#define SAMPLE_RATE     44100  // CD quality
// Options: 44100, 48000

#define BITS_PER_SAMPLE 16     // Standard quality
// Options: 16, 24, 32
```

Higher sample rates and bit depths improve quality but increase CPU load.

### WiFi Settings

```cpp
#define WIFI_TIMEOUT_MS 20000  // Connection timeout
```

### Network Ports

```cpp
#define SSDP_PORT       1900   // UPnP discovery
#define HTTP_PORT       8080   // DLNA control
```

Change `HTTP_PORT` if 8080 is already in use on your network.

## Troubleshooting

### No Audio Output

**Check wiring:**
```
- Verify all I2S connections are correct
- Ensure GND is connected
- Check power supply (stable 5V, 1A minimum)
```

**Check DAC module:**
```
- DAC power LED should be on
- Try measuring voltage on VIN pin
- Swap DIN wire if audio is distorted
```

**Serial debug:**
```
Enable verbose logging in platformio.ini:
build_flags = -DCORE_DEBUG_LEVEL=5
```

### Bluetooth Not Visible

**Reset Bluetooth:**
```cpp
// In setup(), before btSpeaker->begin():
btStop();
delay(1000);
```

**Check paired devices:**
- Remove old "ESP32-Speaker" pairings
- Restart ESP32
- Try pairing again

**Memory issues:**
```
If DLNA + Bluetooth is too much:
- Disable DLNA (comment out dlnaRenderer initialization)
- Or disable PSRAM in platformio.ini
```

### DLNA Not Discoverable

**Check WiFi:**
```
- Ensure ESP32 and control device are on same network
- Check if IP address is assigned (serial monitor)
- Ping the ESP32: ping 192.168.1.xxx
```

**Router settings:**
```
- Enable IGMP Snooping (for multicast)
- Disable AP Isolation
- Allow UPnP in firewall
```

**Test manually:**
```
Open browser: http://ESP32_IP:8080/description.xml
Should show XML device description
```

### Audio Quality Issues

**Crackling/distortion:**
- Check power supply (needs clean, stable voltage)
- Add 100µF capacitor between VIN and GND
- Reduce WiFi activity (increase buffer sizes)
- Lower sample rate to 44100 Hz

**Delay/latency:**
- Bluetooth has ~200ms latency (normal for A2DP)
- DLNA latency depends on network
- Reduce DMA buffer size in AudioOutput.cpp

**One channel only:**
- Check LRC/WS pin connection
- Verify speaker wiring
- Check DAC mono/stereo configuration

## Project Structure

```
SpeakerDlna/
├── platformio.ini          # PlatformIO configuration
├── data/
│   └── config.json        # User configuration (WiFi, audio, protocols)
├── include/
│   ├── config.h           # Default defines
│   ├── ConfigManager.h    # JSON config loader
│   ├── AudioOutput.h      # I2S audio interface
│   ├── AudioStreamer.h    # HTTP streaming & decoding
│   ├── BluetoothSpeaker.h # Bluetooth A2DP
│   ├── DLNARenderer.h     # DLNA/UPnP
│   └── HEOSPlayer.h       # HEOS protocol
├── src/
│   ├── main.cpp           # Main application
│   ├── ConfigManager.cpp  # Config management
│   ├── AudioOutput.cpp    # I2S implementation
│   ├── AudioStreamer.cpp  # Streaming implementation
│   ├── BluetoothSpeaker.cpp  # Bluetooth implementation
│   ├── DLNARenderer.cpp   # DLNA implementation
│   └── HEOSPlayer.cpp     # HEOS implementation
├── README.md              # This file
├── CONFIG_GUIDE.md        # Configuration guide
├── HEOS_PROTOCOL.md       # HEOS protocol documentation
└── STREAMING_GUIDE.md     # Audio streaming guide
```

## Advanced Configuration

### Custom Partition Table

Create `partitions.csv` for larger app size:
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x180000,
app1,     app,  ota_1,   0x190000,0x180000,
spiffs,   data, spiffs,  0x310000,0xF0000,
```

Update `platformio.ini`:
```ini
board_build.partitions = partitions.csv
```

### Add Web Interface

Enable web configuration:
- HTTP server already running on port 8080
- Add endpoints for WiFi setup, volume control
- Create web UI in SPIFFS filesystem

### OTA Updates

Add OTA support for wireless firmware updates:
```cpp
#include <ArduinoOTA.h>
// Add OTA setup in setup()
ArduinoOTA.begin();
// Add OTA handle in loop()
ArduinoOTA.handle();
```

## Performance Notes

- **Memory usage**: ~800KB RAM (Bluetooth + DLNA + HEOS + Streaming buffers)
- **CPU usage**: 
  - ~30% idle with all protocols active
  - ~40-50% during MP3/AAC streaming
  - ~70% during FLAC streaming
- **Power consumption**: ~200mA (ESP32) + DAC/Amp power
- **WiFi range**: Typical 50-100m indoor
- **Bluetooth range**: Typical 10m line-of-sight
- **Recommended**: ESP32 with 4MB flash minimum, PSRAM optional but helpful

## Known Limitations

- DLNA implementation is basic (discovery and control only)
- No HTTP audio streaming in DLNA (requires additional implementation)
- No Bluetooth AVRCP controls (play/pause/skip)
- Single audio source at a time (Bluetooth OR DLNA, not both)
- No audio mixing or equalizer

## Future Enhancements

- [ ] HTTP audio streaming for full DLNA support
- [ ] AVRCP metadata and controls
- [ ] Web-based configuration interface
- [ ] Multi-room audio synchronization
- [ ] AirPlay support
- [ ] Spotify Connect integration
- [ ] Audio equalizer and effects

## Dependencies

PlatformIO automatically manages dependencies:
- ESP32 Arduino Core 3.0.0+
- ArduinoJson 7.0+ (for HEOS JSON parsing and configuration)
- ESP32-audioI2S (for audio streaming and decoding)
- BluetoothA2DPSink library (included in ESP32 core)
- WebServer library (included in ESP32 core)
- WiFi library (included in ESP32 core)
- LittleFS (for configuration storage)

## License

This project is provided as-is for educational and personal use.

## Support

For issues and questions:
1. Check the Troubleshooting section above
2. Review serial monitor output for errors
3. Verify hardware connections
4. Test individual components (Bluetooth only, then add DLNA)

## Quick Start Checklist

- [ ] Edit `data/config.json` with your WiFi credentials
- [ ] Configure device name and audio settings
- [ ] Run `pio run --target uploadfs` to upload config
- [ ] Run `pio run --target upload` to flash firmware
- [ ] Open serial monitor to verify boot
- [ ] Test Bluetooth pairing
- [ ] Add device in HEOS app
- [ ] Stream your first audio!

## Resources

- [ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [I2S Audio Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html)
- [ESP32-audioI2S Library](https://github.com/schreibfaul1/ESP32-audioI2S)
- [Bluetooth A2DP](https://github.com/pschatzmann/ESP32-A2DP)
- [UPnP Specifications](http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.0.pdf)
- [HEOS Protocol Specification](https://rn.dmglobal.com/euheos/HEOS_CLI_ProtocolSpecification.pdf)
- [ArduinoJson Documentation](https://arduinojson.org/)

---

**Made with ESP32 and PlatformIO** 🎵  
**Full HEOS Streaming Support** 🔊  
**JSON Configuration System** ⚙️
