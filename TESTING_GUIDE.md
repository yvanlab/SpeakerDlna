# DLNA Testing Guide

## Quick Start Testing

### Step 1: Upload Firmware

```bash
cd c:\dev\SpeakerDlna
pio run --target upload
pio device monitor -b 115200
```

### Step 2: Verify Initialization

Look for these messages in serial monitor:

```
✓ DLNA Renderer initialized: ESP32-Speaker (UUID: uuid:12345678-1234-1234-1234-XXXXXXXXXXXX)
[SSDP] Initial announcement 1/3 sent
[SSDP] Initial announcement 2/3 sent
[SSDP] Initial announcement 3/3 sent
```

---

## Test 1: VLC Media Player

### Windows VLC Testing

1. **Open VLC** (version 3.0.x or newer)

2. **Enable Renderer Discovery**:
   - View menu > Click "Renderer"
   - Wait 5-10 seconds

3. **Look for ESP32-Speaker**:
   - Should appear in the Renderer submenu
   - If not visible, try:
     - Media > Refresh Renderer List
     - Wait 30 seconds (for next SSDP announcement)

4. **Test Playback**:
   ```
   a) Select a local MP3/FLAC file
   b) Click "ESP32-Speaker" in Renderer menu
   c) Press Play
   ```

5. **Expected Serial Output**:
   ```
   [SSDP] M-SEARCH from X.X.X.X:XXXXX ST: urn:schemas-upnp-org:device:MediaRenderer:1
   [HTTP] Sent device description
   [DLNA] >>> Incoming AVTransport Control Request
   [DLNA] Processing Action: SetAVTransportURI
   [DLNA] SUCCESS - URI EXTRACTED: http://...
   [DLNA] Processing Action: Play
   Starting stream: http://...
   Stream started
   ```

6. **Test Controls**:
   - **Volume**: Adjust VLC volume slider (0-100%)
   - **Mute**: Click mute button
   - **Stop**: Press stop button
   - **Pause**: Press pause (may not work for all streams)

### VLC Not Discovering?

**Troubleshoot**:

1. **Check Network**:
   ```
   ping ESP32_IP_ADDRESS
   ```

2. **Test Description Manually**:
   ```
   curl http://ESP32_IP:8080/description.xml
   ```
   Should return XML with DMR-1.50

3. **Check Windows Firewall**:
   - Allow VLC through Windows Firewall
   - Allow SSDP (UDP 1900)

4. **Reset VLC Cache**:
   - Tools > Preferences > Reset Preferences
   - Restart VLC

---

## Test 2: Hi-Fi Cast

### Android Hi-Fi Cast Testing

1. **Install Hi-Fi Cast**:
   - Google Play Store
   - Latest version

2. **Open App**:
   - Grant local network permissions
   - Wait for device scan

3. **Look for ESP32-Speaker**:
   - Should appear in "Renderers" or "Cast To" menu
   - Icon: speaker/audio device

4. **Test Playback**:
   ```
   a) Select music from your library
   b) Tap cast icon
   c) Select "ESP32-Speaker"
   d) Press play
   ```

5. **Test Controls**:
   - **Volume**: Use in-app volume slider
   - **Mute**: Tap mute icon
   - **Next**: If playlist (may show "not implemented")
   - **Info**: Check track information displays

### Hi-Fi Cast Issues?

1. **Permissions**:
   - Settings > Apps > Hi-Fi Cast
   - Allow "Nearby devices" / "Local network"

2. **Network**:
   - Both phone and ESP32 on same WiFi
   - Not on VPN
   - Router AP isolation disabled

3. **Restart**:
   - Force stop Hi-Fi Cast
   - Restart app
   - Wait 30 seconds for discovery

---

## Test 3: BubbleUPnP (Should Still Work)

1. **Open BubbleUPnP**

2. **Select Renderer**:
   - Tap renderer icon (top right)
   - Select "ESP32-Speaker"

3. **Play Content**:
   - Browse library
   - Select track
   - Should start immediately

4. **Test All Controls** (all should work):
   - ✅ Play
   - ✅ Pause
   - ✅ Stop
   - ✅ Volume (0-100%)
   - ✅ Mute
   - ⚠️ Seek (returns OK but doesn't seek)
   - ⚠️ Next/Previous (returns OK but no action)

---

## Serial Monitor Output Reference

### Normal Playback Session

```
[SSDP] M-SEARCH from 192.168.1.100:55123 ST: ssdp:all
[SSDP] Sent response to 192.168.1.100:55123 for upnp:rootdevice
[SSDP] Sent response to 192.168.1.100:55123 for uuid:12345678...
[SSDP] Sent response to 192.168.1.100:55123 for urn:schemas-upnp-org:device:MediaRenderer:1

[HTTP] Sent device description

[DLNA] >>> Incoming AVTransport Control Request
[DLNA] Action Header: "urn:schemas-upnp-org:service:AVTransport:1#SetAVTransportURI"
[DLNA] Processing Action: SetAVTransportURI
[DLNA] SUCCESS - URI EXTRACTED: http://192.168.1.100:8000/music.mp3
[SOAP] AVTransport Action: SetAVTransportURI

[DLNA] >>> Incoming AVTransport Control Request
[DLNA] Processing Action: Play
Starting stream: http://192.168.1.100:8000/music.mp3
Connecting to stream URL: http://192.168.1.100:8000/music.mp3
Using MP3 decoder (default)
Stream started
Stream state: PLAYING

[DLNA] Incoming RenderingControl request
[DLNA] SOAPACTION: "urn:schemas-upnp-org:service:RenderingControl:1#GetVolume"
[SOAP] RenderingControl Action: GetVolume

[DLNA] Incoming RenderingControl request
[DLNA] SOAPACTION: "urn:schemas-upnp-org:service:RenderingControl:1#SetVolume"
[DLNA] Volume changed to 75%

[DLNA] Incoming RenderingControl request
[DLNA] SOAPACTION: "urn:schemas-upnp-org:service:RenderingControl:1#SetMute"
[DLNA] Muted

[DLNA] Incoming RenderingControl request
[DLNA] SOAPACTION: "urn:schemas-upnp-org:service:RenderingControl:1#SetMute"
[DLNA] Unmuted

[DLNA] >>> Incoming AVTransport Control Request
[DLNA] Processing Action: GetPositionInfo
[SOAP] AVTransport Action: GetPositionInfo

[DLNA] >>> Incoming AVTransport Control Request
[DLNA] Processing Action: GetMediaInfo
[SOAP] AVTransport Action: GetMediaInfo

[DLNA] >>> Incoming AVTransport Control Request
[DLNA] Processing Action: Stop
Stream stopped
```

### Error Messages (What to Watch For)

**Bad** - Should NOT see:
```
✗ Failed to open HTTP stream
Stream decode error or ended (repeatedly)
[DLNA] Could not parse volume value
```

**Good** - Informational:
```
[DLNA] Seek requested (not implemented for streams)
[DLNA] Next requested (not implemented)
```

---

## Network Diagnostic Commands

### Check Device Responds

```bash
# Windows
curl http://ESP32_IP:8080/description.xml

# PowerShell
Invoke-WebRequest -Uri "http://ESP32_IP:8080/description.xml"
```

Expected: XML document with `<friendlyName>ESP32-Speaker</friendlyName>`

### Check SSDP Multicast

```bash
# Windows (PowerShell as Admin)
Test-NetConnection -ComputerName 239.255.255.250 -Port 1900

# Linux
tcpdump -i wlan0 -n -vv udp port 1900
```

Expected: NOTIFY and M-SEARCH packets

### Check HTTP Port

```bash
# Windows
netstat -an | findstr :8080

# Linux
netstat -tuln | grep 8080
```

Expected: `0.0.0.0:8080` or `*:8080` listening

---

## Common Issues & Solutions

### Issue 1: "Device Not Found"

**Symptoms**: No ESP32-Speaker in any app

**Checks**:
1. Serial monitor shows "✓ DLNA Renderer initialized"?
2. Can ping ESP32 IP?
3. Can curl description.xml?
4. Same WiFi network?

**Solutions**:
- Restart ESP32
- Wait 30 seconds (for next SSDP cycle)
- Disable VPN
- Check router AP isolation

---

### Issue 2: "Device Found But Won't Play"

**Symptoms**: ESP32-Speaker appears, but play fails

**Checks**:
1. Serial shows "SetAVTransportURI" message?
2. Serial shows "Play" action?
3. Serial shows "Stream started"?

**Solutions**:
- Check audio format (MP3/AAC/FLAC supported)
- Try different audio file
- Check network bandwidth
- Monitor free heap (should stay stable)

---

### Issue 3: "Volume Control Doesn't Work"

**Symptoms**: Volume slider moves but no volume change

**Checks**:
1. Serial shows "[DLNA] Volume changed to X%"?
2. Audio actually playing?

**Solutions**:
- Check I2S DAC connections
- Test with 'v' command in serial (emergency volume reset)
- Verify AudioOutput.getVolume() returns correct value

---

### Issue 4: "Mute Doesn't Work"

**Symptoms**: Mute button doesn't mute audio

**Checks**:
1. Serial shows "[DLNA] Muted" or "[DLNA] Unmuted"?
2. Is `isMuted` state tracked correctly?

**Solutions**:
- Check `lastVolume` is saved before muting
- Verify SOAP body parsing for DesiredMute
- Test SetVolume after mute (should unmute)

---

## Performance Monitoring

### Monitor in Serial

Watch these values periodically:

```
Free Heap: XXXXX bytes
```

**Good**: Stable or slowly decreasing then stable
**Bad**: Continuously decreasing (memory leak)

### Expected Values:
- **Idle**: ~46,000 bytes free
- **Streaming**: ~40,000 bytes free
- **Multiple DLNA commands**: ~38,000 bytes free

If heap drops below 30,000 bytes, investigate memory leak.

---

## Advanced Testing

### Test All AVTransport Actions

Use a tool like `curl` or Postman to send SOAP requests:

```bash
# GetMediaInfo
curl -X POST http://ESP32_IP:8080/avt/control \
  -H "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#GetMediaInfo\"" \
  -H "Content-Type: text/xml" \
  -d '<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
  <s:Body>
    <u:GetMediaInfo xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">
      <InstanceID>0</InstanceID>
    </u:GetMediaInfo>
  </s:Body>
</s:Envelope>'
```

Expected: XML response with NrTracks, MediaDuration, etc.

### Test GetProtocolInfo

```bash
curl -X POST http://ESP32_IP:8080/cm/control \
  -H "SOAPACTION: \"urn:schemas-upnp-org:service:ConnectionManager:1#GetProtocolInfo\"" \
  -H "Content-Type: text/xml" \
  -d '<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
  <s:Body>
    <u:GetProtocolInfo xmlns:u="urn:schemas-upnp-org:service:ConnectionManager:1">
    </u:GetProtocolInfo>
  </s:Body>
</s:Envelope>'
```

Expected: Long list of audio/* MIME types

---

## Success Criteria

### ✅ VLC Discovery
- [ ] ESP32-Speaker appears in View > Renderer within 30 seconds
- [ ] Selecting device shows checkmark
- [ ] Device stays in list (doesn't disappear)

### ✅ VLC Playback
- [ ] Audio plays from local file
- [ ] Volume slider affects output
- [ ] Mute button works
- [ ] Stop button works
- [ ] Position updates (for files)

### ✅ Hi-Fi Cast Discovery
- [ ] Device appears in app renderer list
- [ ] Device name is "ESP32-Speaker"
- [ ] Icon shows audio/speaker device

### ✅ Hi-Fi Cast Playback
- [ ] Audio starts within 2 seconds
- [ ] Volume control responsive
- [ ] Mute toggle works
- [ ] Track info displays

### ✅ BubbleUPnP (Regression Test)
- [ ] Still works as before
- [ ] All controls responsive
- [ ] No new issues introduced

---

## Report Template

When reporting issues, include:

```
**Device**: ESP32-XXXX (board type)
**App**: VLC 3.0.18 / Hi-Fi Cast 5.2 / etc
**Platform**: Windows 11 / Android 13 / etc
**Network**: WiFi 5GHz / 2.4GHz
**Router**: Brand/Model

**Issue Description**:
(What happened)

**Expected**:
(What should happen)

**Serial Output**:
```
(paste relevant serial output)
```

**Steps to Reproduce**:
1. 
2. 
3. 

**Additional Info**:
- Can ping ESP32: Yes/No
- Description.xml accessible: Yes/No
- Other DLNA devices work: Yes/No
```

---

## Next Steps After Testing

1. **If all tests pass**: Ready for production use! 🎉

2. **If VLC doesn't discover**:
   - Check DLNA_IMPROVEMENTS_COMPLETE.md debugging section
   - Verify SSDP packets with Wireshark
   - Try different VLC version

3. **If controls don't work**:
   - Monitor serial for SOAP action names
   - Check if actions are recognized
   - Verify volume/mute state changes

4. **Report Success**:
   - Share working configurations
   - Note any quirks or workarounds
   - Suggest improvements

Happy testing! 🎵
