# Quick Start Guide

Get your ESP32 HEOS Speaker up and running in minutes!

## What You Need

### Hardware
- ✅ ESP32 development board (ESP32-WROOM-32 or similar)
- ✅ I2S DAC module (MAX98357A recommended)
- ✅ Speaker (4-8 ohm, 3-10W)
- ✅ USB cable for programming
- ✅ Jumper wires

### Software
- ✅ VS Code with PlatformIO extension
- ✅ ESP32 USB drivers

## Step-by-Step Setup

### 1. Hardware Connections

```
ESP32 Pin    →    MAX98357A
────────────────────────────
GPIO 26      →    BCLK
GPIO 25      →    LRC
GPIO 22      →    DIN
GND          →    GND
5V           →    Vin

Connect your speaker to MAX98357A + and - terminals
```

**Power:** Use USB power (5V, 1A minimum) or external 5V supply

### 2. Software Installation

**Install VS Code:**
1. Download from https://code.visualstudio.com/
2. Install VS Code

**Install PlatformIO:**
1. Open VS Code
2. Click Extensions (Ctrl+Shift+X)
3. Search for "PlatformIO"
4. Click Install
5. Wait for installation to complete
6. Restart VS Code

### 3. Configure Your Speaker

**Edit config file:**
1. Open `c:/dev/SpeakerDlna/data/config.json`
2. Update WiFi credentials:

```json
{
  "wifi": {
    "ssid": "YourWiFiName",
    "password": "YourWiFiPassword"
  },
  "device": {
    "name": "My ESP32 Speaker"
  }
}
```

3. Save the file

### 4. Upload to ESP32

**In VS Code:**

1. **Open folder:** File → Open Folder → Select `c:/dev/SpeakerDlna`
2. **Wait:** Let PlatformIO load the project (watch bottom status bar)
3. **Connect ESP32:** Plug in via USB
4. **Upload config:**
   - Click PlatformIO icon (alien head) on left sidebar
   - Expand "Platform"
   - Click "Upload Filesystem Image"
   - Wait for completion
5. **Upload firmware:**
   - Click the arrow (→) icon in bottom status bar
   - Or click "Upload" under "General"
   - Wait for upload to complete

**Command line alternative:**
```bash
cd c:/dev/SpeakerDlna
pio run --target uploadfs   # Upload config
pio run --target upload      # Upload firmware
```

### 5. First Boot

**Open Serial Monitor:**
- Click the plug icon in VS Code bottom bar
- Or: `pio device monitor -b 115200`

**You should see:**
```
╔═══════════════════════════════════════════════╗
║   ESP32 Multi-Protocol Wireless Speaker      ║
║   HEOS • DLNA • Bluetooth                     ║
╚═══════════════════════════════════════════════╝

=== Configuration Loading ===
✓ Configuration loaded

=== Audio Output Initialization ===
✓ Audio output ready

=== WiFi Setup ===
Connecting to YourWiFi.....
✓ WiFi connected!
IP Address: 192.168.1.100

=== Bluetooth Initialization ===
✓ Bluetooth speaker ready

=== DLNA Initialization ===
✓ DLNA renderer ready

=== HEOS Initialization ===
✓ HEOS player ready
Full audio streaming enabled!

╔═══════════════════════════════════════════════╗
║          SYSTEM READY!                        ║
╚═══════════════════════════════════════════════╝
```

**Note your IP address** - you'll need it for HEOS!

## Using Your Speaker

### Option 1: Bluetooth (Simplest)

1. **On your phone/computer:**
   - Open Bluetooth settings
   - Look for "My ESP32 Speaker" (or your device name)
   - Connect to it

2. **Play music:**
   - Open any music app
   - Audio plays through ESP32 speaker!

### Option 2: HEOS App (Best Quality)

1. **Install HEOS app:**
   - iOS: App Store → Search "HEOS"
   - Android: Play Store → Search "HEOS"
   - Download and install

2. **Add speaker:**
   - Open HEOS app
   - Tap Settings (⚙️) → Add Device
   - Look for "My ESP32 Speaker"
   - Tap to add

3. **Stream music:**
   - Browse music sources (TuneIn, etc.)
   - Select a station or track
   - Choose "My ESP32 Speaker" as player
   - **Music streams with full quality!**

### Option 3: DLNA

1. **Install DLNA app:**
   - Android: BubbleUPnP, Hi-Fi Cast
   - iOS: ArkMC, nPlayer
   - Windows: Windows Media Player

2. **Find speaker:**
   - Scan for DLNA devices
   - Find "My ESP32 Speaker"

3. **Cast audio:**
   - Select speaker as renderer
   - Play music from your library

## Troubleshooting

### No WiFi Connection

**Check:**
- SSID and password are correct (case-sensitive!)
- Router is 2.4GHz (ESP32 doesn't support 5GHz)
- ESP32 is in range of router

**Fix:**
- Re-edit `data/config.json`
- Upload filesystem: `pio run --target uploadfs`
- Restart ESP32

### No Audio Output

**Check:**
- I2S wiring is correct
- Speaker is connected
- Volume is not zero
- DAC has power (LED on)

**Fix:**
- Verify wiring matches pin numbers in config
- Check serial monitor for errors
- Try increasing volume in HEOS app

### Bluetooth Not Visible

**Fix:**
- Restart ESP32
- Remove old pairings on your phone
- Make sure no other device is connected

### HEOS App Can't Find Speaker

**Check:**
- ESP32 and phone on same WiFi network
- WiFi connected (check serial monitor)
- IP address assigned
- Router allows device discovery

**Fix:**
- Try adding by IP address: `192.168.1.xxx:1255`
- Restart ESP32
- Restart HEOS app

## Testing Your Setup

### Test 1: Check Serial Output

Should show:
```
WiFi: Connected (-45 dBm) - 192.168.1.100
Bluetooth: Waiting for connection
DLNA: Active and discoverable
HEOS: Active - Stopped (Volume: 50%)
```

### Test 2: Test Bluetooth

1. Pair phone to speaker
2. Play music
3. Should hear audio immediately

### Test 3: Test HEOS Streaming

1. Open HEOS app
2. Go to TuneIn → Local Radio
3. Select any station
4. Choose ESP32 speaker
5. Should hear streaming audio

### Test 4: Check Metadata

In HEOS app, you should see:
- Station name
- Song title (if available)
- Artist name (if available)

## What's Next?

✅ **You're done!** Your speaker is fully functional.

**Optional enhancements:**
- Add more ESP32 speakers for multi-room audio
- Experiment with different I2S DACs
- Try different audio formats (FLAC for highest quality)
- Customize device name and settings in config.json

## Common Adjustments

### Change Device Name

Edit `data/config.json`:
```json
"device": {
  "name": "Kitchen Speaker"
}
```
Upload: `pio run --target uploadfs`

### Adjust Default Volume

Edit `data/config.json`:
```json
"audio": {
  "default_volume": 75
}
```
Upload: `pio run --target uploadfs`

### Disable Bluetooth (WiFi only)

Edit `data/config.json`:
```json
"protocols": {
  "bluetooth": {"enabled": false}
}
```
Upload: `pio run --target uploadfs`

## Help & Documentation

- **Configuration:** See `CONFIG_GUIDE.md`
- **HEOS Protocol:** See `HEOS_PROTOCOL.md`
- **Audio Streaming:** See `STREAMING_GUIDE.md`
- **Full README:** See `README.md`

## Success Checklist

- [ ] Hardware connected correctly
- [ ] PlatformIO installed in VS Code
- [ ] Config file updated with WiFi
- [ ] Filesystem uploaded (`uploadfs`)
- [ ] Firmware uploaded (`upload`)
- [ ] Serial monitor shows "SYSTEM READY"
- [ ] Can see device in Bluetooth
- [ ] Can add device in HEOS app
- [ ] Audio plays successfully

**Enjoy your ESP32 multi-protocol wireless speaker!** 🎵

---

**Questions?** Check the troubleshooting guides or serial monitor output for error messages.
