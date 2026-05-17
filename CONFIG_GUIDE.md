# Configuration Guide

This guide explains how to configure your ESP32 speaker using the JSON configuration file.

## Configuration File Location

The configuration is stored in: `data/config.json`

This file is uploaded to the ESP32's LittleFS filesystem and can be edited at any time.

## Uploading Configuration

### Method 1: Using PlatformIO (VS Code)

1. Edit `data/config.json` with your settings
2. In VS Code, click **PlatformIO: Upload Filesystem Image** from the PlatformIO toolbar
3. Or run from terminal:
   ```bash
   pio run --target uploadfs
   ```

### Method 2: Using PlatformIO CLI

```bash
cd c:/dev/SpeakerDlna
pio run --target uploadfs
```

### Method 3: Manual Upload

If you have the ESP32 filesystem uploader tool, you can upload the `data/` folder directly.

## Configuration Structure

### Complete Example

```json
{
  "wifi": {
    "ssid": "YOUR_WIFI_SSID",
    "password": "YOUR_WIFI_PASSWORD",
    "hostname": "esp32-speaker",
    "timeout_ms": 20000
  },
  "device": {
    "name": "ESP32-Speaker",
    "model": "ESP32-HEOS-DLNA-BT",
    "location": "Living Room"
  },
  "audio": {
    "sample_rate": 44100,
    "bits_per_sample": 16,
    "i2s": {
      "bclk_pin": 26,
      "lrc_pin": 25,
      "dout_pin": 22
    },
    "default_volume": 50
  },
  "protocols": {
    "bluetooth": {
      "enabled": true
    },
    "dlna": {
      "enabled": true,
      "port": 8080
    },
    "heos": {
      "enabled": true,
      "command_port": 1255,
      "stream_port": 8081
    }
  }
}
```

## Configuration Sections

### WiFi Configuration

```json
"wifi": {
  "ssid": "YOUR_WIFI_SSID",        // Your WiFi network name
  "password": "YOUR_WIFI_PASSWORD", // Your WiFi password
  "hostname": "esp32-speaker",      // Network hostname for the device
  "timeout_ms": 20000               // Connection timeout in milliseconds
}
```

**Important:** 
- SSID and password are case-sensitive
- Special characters in password must be properly escaped
- Hostname should be lowercase, no spaces

### Device Configuration

```json
"device": {
  "name": "ESP32-Speaker",           // Display name (shown in apps)
  "model": "ESP32-HEOS-DLNA-BT",    // Model identifier
  "location": "Living Room"          // Physical location (optional)
}
```

**Usage:**
- `name` appears in Bluetooth, HEOS, and DLNA discovery
- `model` is reported in device information
- `location` helps identify the speaker in multi-room setups

### Audio Configuration

```json
"audio": {
  "sample_rate": 44100,              // Sample rate in Hz (44100, 48000)
  "bits_per_sample": 16,             // Bit depth (16, 24, 32)
  "i2s": {
    "bclk_pin": 26,                 // I2S Bit Clock pin
    "lrc_pin": 25,                  // I2S Left/Right Clock pin
    "dout_pin": 22                  // I2S Data Out pin
  },
  "default_volume": 50               // Startup volume (0-100)
}
```

**Sample Rates:**
- `44100` - CD quality (recommended)
- `48000` - Professional audio

**Bit Depth:**
- `16` - Standard quality (recommended)
- `24` - High quality (more CPU usage)
- `32` - Studio quality (highest CPU usage)

**I2S Pins:**
- Must match your DAC module wiring
- Common configurations:
  - **MAX98357A**: BCLK=26, LRC=25, DIN=22
  - **PCM5102A**: BCK=26, LCK=25, DIN=22

### Protocol Configuration

```json
"protocols": {
  "bluetooth": {
    "enabled": true                  // Enable/disable Bluetooth
  },
  "dlna": {
    "enabled": true,                 // Enable/disable DLNA
    "port": 8080                     // DLNA HTTP server port
  },
  "heos": {
    "enabled": true,                 // Enable/disable HEOS
    "command_port": 1255,            // HEOS command port (standard: 1255)
    "stream_port": 8081              // HEOS streaming port
  }
}
```

**Protocol Settings:**
- Set `enabled: false` to disable any protocol
- Disabling protocols saves memory
- DLNA and HEOS require WiFi (Bluetooth works without WiFi)

**Port Settings:**
- Default ports work with standard apps
- Change if ports conflict with other devices
- Standard HEOS port is 1255 (don't change unless necessary)

## Common Configuration Scenarios

### Scenario 1: Home Setup (All Protocols)

```json
{
  "wifi": {
    "ssid": "HomeNetwork",
    "password": "MyPassword123",
    "hostname": "living-room-speaker"
  },
  "device": {
    "name": "Living Room Speaker",
    "location": "Living Room"
  },
  "protocols": {
    "bluetooth": {"enabled": true},
    "dlna": {"enabled": true},
    "heos": {"enabled": true}
  }
}
```

### Scenario 2: Bluetooth-Only (Portable)

```json
{
  "wifi": {
    "ssid": "",
    "password": ""
  },
  "device": {
    "name": "Portable Speaker"
  },
  "protocols": {
    "bluetooth": {"enabled": true},
    "dlna": {"enabled": false},
    "heos": {"enabled": false}
  }
}
```

### Scenario 3: HEOS Multi-Room Setup

```json
{
  "wifi": {
    "ssid": "HomeNetwork",
    "password": "MyPassword123"
  },
  "device": {
    "name": "Bedroom Speaker",
    "location": "Master Bedroom"
  },
  "protocols": {
    "bluetooth": {"enabled": false},
    "dlna": {"enabled": false},
    "heos": {"enabled": true}
  }
}
```

### Scenario 4: Custom I2S Pins

```json
{
  "audio": {
    "sample_rate": 44100,
    "bits_per_sample": 16,
    "i2s": {
      "bclk_pin": 14,
      "lrc_pin": 15,
      "dout_pin": 13
    }
  }
}
```

## Troubleshooting

### Configuration Not Loading

**Problem:** ESP32 keeps using defaults

**Solutions:**
1. Check if filesystem was uploaded:
   ```bash
   pio run --target uploadfs
   ```
2. Verify `data/config.json` exists
3. Check serial monitor for parsing errors
4. Validate JSON syntax: https://jsonlint.com/

### WiFi Not Connecting

**Problem:** WiFi connection fails

**Check:**
- SSID and password are correct (case-sensitive)
- No special characters causing issues
- Router is on and in range
- 2.4GHz WiFi (ESP32 doesn't support 5GHz)

### Invalid JSON Error

**Problem:** "Failed to parse config"

**Solutions:**
1. Validate JSON at https://jsonlint.com/
2. Common issues:
   - Missing commas between fields
   - Extra commas at end of lists
   - Unescaped quotes in strings
   - Missing closing braces

### Audio Quality Issues

**Problem:** Crackling or distortion

**Try:**
- Lower sample rate to 44100
- Reduce bits_per_sample to 16
- Check I2S pin configuration
- Verify stable power supply

## Editing Configuration

### On Your Computer (Before Upload)

1. Open `c:/dev/SpeakerDlna/data/config.json`
2. Edit with any text editor
3. Save file
4. Upload filesystem:
   ```bash
   pio run --target uploadfs
   ```
5. Restart ESP32

### While ESP32 is Running

Currently, configuration requires re-uploading the filesystem. Future versions may include:
- Web interface for configuration
- WiFi AP mode for initial setup
- Configuration via serial commands

## Advanced Configuration

### Multiple WiFi Networks

For multiple location support, you can manually handle multiple configs:

1. Create `config-home.json` and `config-office.json`
2. Rename the appropriate file to `config.json` before upload
3. Or implement WiFi fallback in code

### Environment-Specific Settings

Use different configs for development vs. production:

```bash
# Development
cp data/config-dev.json data/config.json
pio run --target uploadfs

# Production
cp data/config-prod.json data/config.json
pio run --target uploadfs
```

### Factory Reset

To reset to defaults:

1. Delete `config.json` from filesystem
2. ESP32 will create new file with defaults on next boot

Or erase and re-upload:
```bash
pio run --target erase
pio run --target upload
pio run --target uploadfs
```

## Configuration Backup

**Backup your config:**
```bash
# Copy config file
cp data/config.json data/config-backup.json
```

**Restore config:**
```bash
# Restore from backup
cp data/config-backup.json data/config.json
pio run --target uploadfs
```

## Security Notes

⚠️ **Important:**
- Config file contains WiFi password in plain text
- Don't commit `config.json` with real passwords to version control
- Use `.gitignore` to exclude sensitive configs
- Consider using environment variables for sensitive data

## Getting Help

If configuration issues persist:
1. Check serial monitor output at 115200 baud
2. Look for configuration error messages
3. Verify JSON syntax is valid
4. Try with minimal/default configuration first
5. Check filesystem was uploaded successfully

---

**Configuration System Powered by ArduinoJson and LittleFS** 📝
