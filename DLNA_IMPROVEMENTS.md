# DLNA Improvements for VLC and Hi-Fi Cast Compatibility

## Issues Identified:

### 1. VLC Discovery Issues
- Missing required SCPD actions
- Incomplete AVTransport service
- Need Seek, Next, Previous actions

### 2. Hi-Fi Cast Issues
- Need proper MediaInfo responses
- Missing GetMediaInfo action
- Need proper metadata in responses

### 3. Control Issues (Both)
- Volume control works but needs Channel parameter handling
- Mute needs proper implementation
- Seek functionality missing
- GetMediaInfo missing

## Fixes Applied:

1. Added GetMediaInfo action
2. Added Seek action
3. Added Next/Previous actions (stub)
4. Improved RenderingControl responses
5. Better SSDP response timing
6. Enhanced device description with proper icons
7. Added proper mute state tracking
