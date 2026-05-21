/**
 * DLNA Renderer Implementation
 */

#include "DLNARenderer.h"
#include "config.h"
#include "constants.h"

DLNARenderer::DLNARenderer(I2SAudioOutput& audioOut, AudioStreamer& streamer)
    : audioOutput(audioOut), audioStreamer(streamer), httpServer(nullptr), initialized(false), lastSSDPAnnounce(0) {
    // Generate a UUID for this device
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    // Ensure the "uuid:" prefix remains intact with its colon
    deviceUUID = "uuid:12345678-1234-1234-1234-" + mac;
    currentURI = "";
    currentURIMetadata = "";
    awaitingURI = false;
    isMuted = false;
    lastVolume = 50;
    bootId = millis() / 1000;
}

DLNARenderer::~DLNARenderer() {
    end();
}

bool DLNARenderer::begin(const char* name, const char* model, uint16_t port) {
    if (initialized) {
        return true;
    }

    deviceName = String(name);
    deviceModel = String(model);
    serverPort = port;

    Serial.println("Initializing DLNA Renderer...");

    // Create HTTP server
    httpServer = new WebServer(serverPort);

    // Enable header collection for SOAP processing
    const char* headerkeys[] = {"SOAPACTION", "Callback", "SID", "Timeout", "Content-Type", "Content-Length", 
                                "Expect", "User-Agent", "Host", "X-CurrentURI", "X-AV-URI", "X-URI", "X-Uri",
                                "URI", "URL", "X-AV-URL", "X-Target-URI", "Accept-Encoding"};
    httpServer->collectHeaders(headerkeys, sizeof(headerkeys) / sizeof(char*));

    // Register HTTP handlers
    httpServer->on("/description.xml", [this]() { handleDescription(); });
    httpServer->on("/avt/control", HTTP_POST, [this]() { handleAVTransportControl(); });
    httpServer->on("/rc/control", HTTP_POST, [this]() { handleRenderingControl(); });
    httpServer->on("/cm/control", HTTP_POST, [this]() { handleConnectionManagerControl(); });
    httpServer->on("/avt/scpd.xml", [this]() { handleAVTransportSCPD(); });
    httpServer->on("/rc/scpd.xml", [this]() { handleRenderingControlSCPD(); });
    httpServer->on("/cm/scpd.xml", [this]() { handleConnectionManagerSCPD(); });

    // Serve a simple icon for devices that request it
    httpServer->on("/icon48.png", [this]() {
        httpServer->send(404, "text/plain", "Icon not available");
    });
    
    // Standard routes usually only match GET/POST. Custom UPnP methods (SUBSCRIBE/UNSUBSCRIBE)
    // are often Method 6 and 7, which are handled in onNotFound as a fallback.
    httpServer->on("/avt/event", HTTP_ANY, [this]() { handleEventSubscription(); });
    httpServer->on("/rc/event", HTTP_ANY, [this]() { handleEventSubscription(); });
    
    // Catch-all for unhandled requests
    httpServer->onNotFound([this]() { 
        int method = (int)httpServer->method();
        if ((method == 6 || method == 7) && (httpServer->uri().endsWith("/event"))) {
            handleEventSubscription();
            return;
        }
        Serial.printf("[HTTP] 404 - %s (Method: %d)\n", httpServer->uri().c_str(), (int)httpServer->method());
        httpServer->send(404, "text/plain", "Not Found");
    });

    httpServer->begin();
    Serial.printf("✓ HTTP server started on port %d\n", serverPort);
    Serial.printf("WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("WiFi Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("WiFi Subnet: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("WiFi DNS: %s\n", WiFi.dnsIP().toString().c_str());

    // Start SSDP socket - proper multicast binding for ESP32
    if (!ssdpSocket.begin(SSDP_PORT)) {
        Serial.println("✗ Failed to bind SSDP socket to port");
        delete httpServer;
        return false;
    }
    Serial.printf("✓ SSDP socket bound to port %d\n", SSDP_PORT);
    
    // Join multicast group
    if (!ssdpSocket.beginMulticast(IPAddress(239, 255, 255, 250), SSDP_PORT)) {
        Serial.println("✗ Failed to join multicast group");
        Serial.println("⚠️ SSDP discovery will not work if multicast is blocked on the network.");
        ssdpSocket.stop();
        delete httpServer;
        return false;
    }
    Serial.println("✓ Joined SSDP multicast group");
    Serial.printf("SSDP multicast address: %s:%d\n", IPAddress(239, 255, 255, 250).toString().c_str(), SSDP_PORT);

    initialized = true;
    Serial.printf("✓ DLNA Renderer initialized: %s (UUID: %s)\n", deviceName.c_str(), deviceUUID.c_str());

    // Send initial SSDP announcements with proper timing
    // Multiple announcements help with discovery reliability
    unsigned long startTime = millis();
    while (millis() - startTime < 100) yield();  // Small delay before first announce

    sendSSDPNotify();
    Serial.println("[SSDP] Initial announcement 1/3 sent");

    startTime = millis();
    while (millis() - startTime < 500) yield();  // Wait between announcements

    sendSSDPNotify();
    Serial.println("[SSDP] Initial announcement 2/3 sent");

    startTime = millis();
    while (millis() - startTime < 500) yield();

    sendSSDPNotify();
    Serial.println("[SSDP] Initial announcement 3/3 sent");

    return true;
}

void DLNARenderer::end() {
    if (initialized) {
        ssdpSocket.stop();
        if (httpServer) {
            httpServer->stop();
            delete httpServer;
            httpServer = nullptr;
        }
        initialized = false;
        Serial.println("DLNA Renderer stopped");
    }
}

void DLNARenderer::handle() {
    if (!initialized) return;

    // Handle HTTP requests
    httpServer->handleClient();

    // Handle SSDP
    handleSSDP();

    // Send periodic SSDP announcements
    if (millis() - lastSSDPAnnounce > SSDP_ANNOUNCE_INTERVAL_MS) {
        sendSSDPNotify();
        lastSSDPAnnounce = millis();
    }
}

void DLNARenderer::handleSSDP() {
    int packetSize = ssdpSocket.parsePacket();
    if (packetSize > 0) {
        DEBUG_PRINTF("[SSDP] Packet received: %d bytes\n", packetSize);
        char buffer[SSDP_BUFFER_SIZE];
        int len = ssdpSocket.read(buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';

            // Check if it's an M-SEARCH request
            if (strstr(buffer, "M-SEARCH") != NULL) {
                IPAddress remoteIP = ssdpSocket.remoteIP();
                uint16_t remotePort = ssdpSocket.remotePort();

                // Extract Search Target (ST) header robustly (handle 'ST:' or 'st:')
                char* stStart = strcasestr(buffer, "ST:");
                String target = "";
                if (stStart) {
                    char* stEnd = strchr(stStart, '\r');
                    if (stEnd) {
                        target = String(stStart).substring(3, stEnd - stStart);
                        target.trim();
                        DEBUG_PRINTF("[SSDP] M-SEARCH from %s:%d ST: %s\n",
                                      remoteIP.toString().c_str(), remotePort, target.c_str());
                    }
                }

                // Determine what the client is looking for
                bool searchAll = target.equalsIgnoreCase("ssdp:all") || target.equals("239.255.255.250:1900");
                bool searchRoot = searchAll || target.equalsIgnoreCase("upnp:rootdevice");
                bool searchRenderer = searchAll || target.equalsIgnoreCase("urn:schemas-upnp-org:device:MediaRenderer:1");
                bool searchAVT = searchAll || target.equalsIgnoreCase("urn:schemas-upnp-org:service:AVTransport:1");
                bool searchRC = searchAll || target.equalsIgnoreCase("urn:schemas-upnp-org:service:RenderingControl:1");
                bool searchCM = searchAll || target.equalsIgnoreCase("urn:schemas-upnp-org:service:ConnectionManager:1");
                bool searchUUID = searchAll || target.equalsIgnoreCase(deviceUUID);

                // If it's an M-SEARCH but we didn't match anything specific yet, fallback to root device
                if (!searchRoot && !searchRenderer && !searchAVT && !searchRC && !searchCM && !searchUUID) {
                    searchRoot = true;
                }

                // Small random delay to prevent UDP collisions (UPnP spec compliance)
                // Use yield() instead of delay() to allow background tasks to run
                unsigned long delayStart = millis();
                unsigned long randomDelay = random(SSDP_MIN_DELAY_MS, SSDP_MAX_DELAY_MS);
                while (millis() - delayStart < randomDelay) {
                    yield();  // Let WiFi/BT tasks run
                }

                // Send all relevant responses without blocking delays
                // The random initial delay above is sufficient for collision avoidance
                if (searchRoot || target.equals("239.255.255.250:1900")) {
                    const char* st = target.equals("239.255.255.250:1900") ? target.c_str() : "upnp:rootdevice";
                    sendSSDPResponse(remoteIP, remotePort, st);
                }
                if (searchUUID) {
                    sendSSDPResponse(remoteIP, remotePort, deviceUUID.c_str());
                }
                if (searchRenderer) {
                    sendSSDPResponse(remoteIP, remotePort, "urn:schemas-upnp-org:device:MediaRenderer:1");
                }
                if (searchAVT) {
                    sendSSDPResponse(remoteIP, remotePort, "urn:schemas-upnp-org:service:AVTransport:1");
                }
                if (searchRC) {
                    sendSSDPResponse(remoteIP, remotePort, "urn:schemas-upnp-org:service:RenderingControl:1");
                }
                if (searchCM) {
                    sendSSDPResponse(remoteIP, remotePort, "urn:schemas-upnp-org:service:ConnectionManager:1");
                }
            }
            // Silently ignore NOTIFY packets from other devices
        }
    }
}

void DLNARenderer::sendSSDPResponse(IPAddress remoteIP, uint16_t remotePort, const char* searchTarget) {
    String localIP = WiFi.localIP().toString();

    String response = "HTTP/1.1 200 OK\r\n";
    response += "CACHE-CONTROL: max-age=120\r\n";
    response += "EXT:\r\n";
    response += "LOCATION: http://" + localIP + ":" + String(serverPort) + "/description.xml\r\n";
    response += "SERVER: FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0\r\n"; 
    response += "DATE: Tue, 19 May 2026 20:05:00 GMT\r\n"; // Required by some strict libupnp versions
    response += "ST: " + String(searchTarget) + "\r\n";
    
    // USN must be just the UUID if the ST is the UUID, otherwise UUID::ST
    if (deviceUUID == String(searchTarget) || String(searchTarget).indexOf("239.255.255.250") != -1) {
        response += "USN: " + deviceUUID + "\r\n";
    } else {
        response += "USN: " + deviceUUID + "::" + String(searchTarget) + "\r\n";
    }
    
    response += "BOOTID.UPNP.ORG: " + String(bootId) + "\r\n";
    response += "CONFIGID.UPNP.ORG: 1\r\n";
    response += "\r\n";

    ssdpSocket.beginPacket(remoteIP, remotePort);
    ssdpSocket.write((const uint8_t*)response.c_str(), response.length());
    ssdpSocket.endPacket();

    DEBUG_PRINTF("[SSDP] Sent response to %s:%d for %s\n",
        remoteIP.toString().c_str(), remotePort, searchTarget);
}

void DLNARenderer::sendSSDPNotify() {
    // Pre-allocate IP string buffer to avoid heap fragmentation
    char ipBuf[IP_STRING_BUFFER_SIZE];
    WiFi.localIP().toString().toCharArray(ipBuf, sizeof(ipBuf));
    IPAddress multicastIP(239, 255, 255, 250);

    auto sendPacket = [this, &ipBuf, &multicastIP](const char* nt, const char* usn) {
        String notify;
        notify.reserve(512);  // Pre-allocate to avoid reallocations

        notify = "NOTIFY * HTTP/1.1\r\n";
        notify += "HOST: 239.255.255.250:1900\r\n";
        notify += "CACHE-CONTROL: max-age=1800\r\n";
        notify += "LOCATION: http://";
        notify += ipBuf;
        notify += ":";
        notify += String(serverPort);
        notify += "/description.xml\r\n";
        notify += "NT: ";
        notify += nt;
        notify += "\r\n";
        notify += "NTS: ssdp:alive\r\n";
        notify += "SERVER: FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0\r\n";
        notify += "USN: ";
        notify += usn;
        notify += "\r\n";
        notify += "BOOTID.UPNP.ORG: ";
        notify += String(bootId);
        notify += "\r\n";
        notify += "CONFIGID.UPNP.ORG: 1\r\n";
        notify += "\r\n";

        yield();  // Allow background WiFi tasks
        ssdpSocket.beginPacket(multicastIP, SSDP_PORT);
        ssdpSocket.write((const uint8_t*)notify.c_str(), notify.length());
        if (!ssdpSocket.endPacket()) {
            DEBUG_PRINTF("[SSDP] Failed to send NOTIFY for %s (Queue Full)\n", nt);
        }
    };

    // Send all notifications without blocking delays
    // UPnP spec allows rapid succession of NOTIFY messages
    sendPacket("upnp:rootdevice", (deviceUUID + "::upnp:rootdevice").c_str());
    yield();  // Brief yield instead of delay

    sendPacket(deviceUUID.c_str(), deviceUUID.c_str());
    yield();

    sendPacket("urn:schemas-upnp-org:device:MediaRenderer:1",
               (deviceUUID + "::urn:schemas-upnp-org:device:MediaRenderer:1").c_str());
    yield();

    sendPacket("urn:schemas-upnp-org:service:AVTransport:1",
               (deviceUUID + "::urn:schemas-upnp-org:service:AVTransport:1").c_str());
    yield();

    sendPacket("urn:schemas-upnp-org:service:RenderingControl:1",
               (deviceUUID + "::urn:schemas-upnp-org:service:RenderingControl:1").c_str());
    yield();

    sendPacket("urn:schemas-upnp-org:service:ConnectionManager:1",
               (deviceUUID + "::urn:schemas-upnp-org:service:ConnectionManager:1").c_str());

    DEBUG_PRINTLN("[SSDP] Sent full tiered NOTIFY suite");
}
void DLNARenderer::handleDescription() {
    String xml = generateDeviceDescription();
    httpServer->sendHeader("Server", "FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->sendHeader("Content-Type", "text/xml; charset=\"utf-8\"");
    httpServer->sendHeader("Cache-Control", "no-cache");
    httpServer->sendHeader("Connection", "close");
    httpServer->send(200, "text/xml; charset=\"utf-8\"", xml);
    DEBUG_PRINTLN("[HTTP] Sent device description");
}

void DLNARenderer::handleEventSubscription() {
    // UPnP requirement: Provide a SID (Subscription ID)
    // We use the device UUID to ensure uniqueness
    httpServer->sendHeader("SID", deviceUUID);
    httpServer->sendHeader("TIMEOUT", "Second-1800");
    httpServer->sendHeader("Server", "Arduino/1.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->send(200, "text/plain", "");
    Serial.printf("[HTTP] Event subscription from %s\n", httpServer->client().remoteIP().toString().c_str());
}

void DLNARenderer::handleAVTransportControl() {
    Serial.println("\n[DLNA] >>> Incoming AVTransport Control Request");
    String soapAction = httpServer->header("SOAPACTION");
    Serial.printf("[DLNA] Action Header: %s\n", soapAction.c_str());

    // Retrieve the SOAP body. Check "plain" first, then fallback to raw index.
    String body = "";
    if (httpServer->hasArg("plain")) {
        body = httpServer->arg("plain");
    } else if (httpServer->args() > 0) {
        body = httpServer->arg(0);
    }

    // If Content-Length indicates more bytes than we've obtained, attempt to read remaining raw bytes
    String contentLenStr = httpServer->header("Content-Length");
    if (contentLenStr.length() > 0) {
        int expected = contentLenStr.toInt();
        if (expected > body.length()) {
            WiFiClient client = httpServer->client();
            unsigned long start = millis();
            while ((int)body.length() < expected && millis() - start < 2000) {
                while (client.available() > 0 && (int)body.length() < expected) {
                    char c = client.read();
                    body += c;
                }
                delay(1);
            }
        }
    }

    Serial.printf("[DLNA] Full Body Received (%d bytes):\n%s\n", body.length(), body.c_str());

    // Robust Unescaping: Some controllers double-encode or use mixed entities
    if (body.indexOf("&lt;") != -1 || body.indexOf("&amp;") != -1) {
        body.replace("&lt;", "<");
        body.replace("&gt;", ">");
        body.replace("&quot;", "\"");
        body.replace("&apos;", "'");
        body.replace("&amp;", "&"); // Do ampersand last to avoid partial entity corruption
    }
    
    // Extract the action name from the header: "urn:schemas-upnp-org:service:AVTransport:1#ActionName"
    int hashPos = soapAction.lastIndexOf('#');
    String actionName = (hashPos != -1) ? soapAction.substring(hashPos + 1) : "Unknown";
    actionName.replace("\"", ""); // Remove surrounding quotes
    actionName.trim();

    Serial.printf("[DLNA] Processing Action: %s\n", actionName.c_str());
    String actionResult = "";

    if (actionName.equalsIgnoreCase("SetAVTransportURI")) {
        String receivedURI = "";
        // 1. Check ALL HTTP arguments. Some controllers send parameters as POST fields.
        for (int i = 0; i < httpServer->args(); i++) {
            if (httpServer->argName(i).equalsIgnoreCase("CurrentURI")) {
                currentURI = httpServer->arg(i);
                Serial.printf("[DLNA] Found URI in Arguments: %s\n", currentURI.c_str());
                break;
            }
        }
        
        // 2. Fallback to aggressive body parsing (handles namespaces and CDATA)
        if (currentURI.length() == 0) {
            // Look for CurrentURI tag regardless of namespace (e.g., <u:CurrentURI> or <CurrentURI>)
            int tagPos = body.indexOf("CurrentURI");
            if (tagPos != -1) {
                int startBracket = body.indexOf('>', tagPos);
                int endBracket = body.indexOf('<', startBracket);
                
                if (startBracket != -1 && endBracket > startBracket) {
                    String extracted = body.substring(startBracket + 1, endBracket);
                    // Handle CDATA wrappers if present
                    if (extracted.indexOf("![CDATA[") != -1) {
                        int cstart = extracted.indexOf("[") + 1;
                        int cend = extracted.lastIndexOf("]");
                        if (cend > cstart) extracted = extracted.substring(cstart, cend);
                    }
                    currentURI = extracted;
                    currentURI.trim();
                }
            }
        }

        if (currentURI.length() > 0) {
            Serial.println("**************************************************");
            Serial.printf("[DLNA] SUCCESS - URI EXTRACTED: %s\n", currentURI.c_str());
            Serial.println("**************************************************");
            Serial.printf("[DLNA] Queued URI: %s\n", currentURI.c_str());
        } else {
            // If we couldn't get a URI, set awaiting flag so a following call can supply it
            awaitingURI = true;
            Serial.println("[DLNA] SetAVTransportURI received with no URI; awaiting next URI-carrying call");

            // Extra debugging: log collected headers to help controllers analysis
            int hdrs = httpServer->headers();
            for (int h = 0; h < hdrs; h++) {
                Serial.printf("[DLNA] Header[%d] %s: %s\n", h, httpServer->headerName(h).c_str(), httpServer->header(h).c_str());
            }
            Serial.printf("[DLNA] Body: %s\n", body.c_str());

            // Dump raw body bytes in hex to catch any hidden data/encoding issues
            const int dumpLen = (body.length() < 128) ? body.length() : 128;
            Serial.print("[DLNA] Raw body hex: ");
            for (int k = 0; k < dumpLen; k++) {
                char c = body[k];
                Serial.printf("%02X", (uint8_t)c);
                if (k < dumpLen - 1) Serial.print(" ");
            }
            Serial.println();

            // Attempt additional aggressive parsing: handle URL-encoded forms and double-escaped XML
            String tmp = body;

            // If the body looks URL-encoded (e.g., CurrentURI=http%3A...), try to locate and decode
            int paramPos = -1;
            if ((paramPos = tmp.indexOf("CurrentURI%3D")) != -1 || (paramPos = tmp.indexOf("CurrentURI=")) != -1) {
                int valStart = tmp.indexOf('=', paramPos);
                if (valStart != -1) {
                    String enc = tmp.substring(valStart + 1);
                    int amp = enc.indexOf('&');
                    if (amp != -1) enc = enc.substring(0, amp);

                    // URL-decode
                    String dec = "";
                    for (int j = 0; j < enc.length(); j++) {
                        char c = enc[j];
                        if (c == '+') {
                            dec += ' ';
                        } else if (c == '%' && j + 2 < enc.length()) {
                            char h1 = enc[j+1];
                            char h2 = enc[j+2];
                            auto hexval = [](char h)->int {
                                if (h >= '0' && h <= '9') return h - '0';
                                if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                                if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                                return 0;
                            };
                            int hi = hexval(h1);
                            int lo = hexval(h2);
                            dec += (char)((hi << 4) + lo);
                            j += 2;
                        } else {
                            dec += c;
                        }
                    }
                    dec.trim();
                    if (dec.length() > 0) {
                        currentURI = dec;
                        Serial.printf("[DLNA] Queued URI (url-decoded): %s\n", currentURI.c_str());
                    }
                }
            }

            // If still empty, try repeated unescaping of XML entities and extract tag content
            if (currentURI.length() == 0) {
                for (int pass = 0; pass < 3 && currentURI.length() == 0; pass++) {
                    if (tmp.indexOf("&lt;") != -1 || tmp.indexOf("&amp;") != -1) {
                        tmp.replace("&lt;", "<");
                        tmp.replace("&gt;", ">");
                        tmp.replace("&quot;", "\"");
                        tmp.replace("&apos;", "'");
                        tmp.replace("&amp;", "&");
                    }

                    int pos = tmp.indexOf("CurrentURI");
                    if (pos != -1) {
                        int openEnd = tmp.indexOf('>', pos);
                        int closeStart = -1;
                        if (openEnd != -1) {
                            closeStart = tmp.indexOf("</", openEnd);
                        }
                        if (openEnd != -1 && closeStart != -1 && closeStart > openEnd) {
                            String extracted = tmp.substring(openEnd + 1, closeStart);
                            extracted.trim();
                            if (extracted.indexOf("![CDATA[") != -1) {
                                int cstart = extracted.indexOf("[") + 1;
                                int cend = extracted.lastIndexOf("]");
                                if (cend > cstart) extracted = extracted.substring(cstart, cend);
                            }
                            currentURI = extracted;
                            if (currentURI.length() > 0) {
                                Serial.printf("[DLNA] Queued URI (extracted): %s\n", currentURI.c_str());
                                break;
                            }
                        }
                    }
                }
            }

            // Log all arguments to see if the controller hid the URI in a named parameter
            for (int i = 0; i < httpServer->args(); i++) {
                Serial.printf("[DLNA] Arg[%d] %s = %s\n", i, httpServer->argName(i).c_str(), httpServer->arg(i).c_str());
            }
        }
    } else if (actionName == "Play") {
        if (currentURI.length() > 0) {
            audioStreamer.startStream(currentURI);
            awaitingURI = false;
        } else {
            Serial.println("[DLNA] Play command ignored: No URI queued");
            awaitingURI = true; // Still waiting
        }
    } else if (actionName == "SetNextAVTransportURI" || actionName == "SetNextAVTransportURIx" ) {
        // Some controllers send the next URI in a separate action. Try to extract it.
        String next = extractURIFromRequest(body);
        if (next.length() > 0) {
            currentURI = next;
            awaitingURI = false;
            Serial.printf("[DLNA] Queued URI (SetNext): %s\n", currentURI.c_str());
        } else {
            Serial.println("[DLNA] SetNextAVTransportURI received but no URI found");
        }
    } else if (actionName == "Stop") {
        audioStreamer.stopStream();
    } else if (actionName == "Pause") {
        audioStreamer.pauseStream();
    } else if (actionName == "GetTransportInfo") {
        String state;
        switch (audioStreamer.getState()) {
            case STREAM_PLAYING:    state = "PLAYING"; break;
            case STREAM_CONNECTING:
            case STREAM_BUFFERING:  state = "TRANSITIONING"; break;
            case STREAM_PAUSED:     state = "PAUSED_PLAYBACK"; break;
            default:                state = "STOPPED"; break;
        }
        actionResult = "<CurrentTransportState>" + state + "</CurrentTransportState>";
        actionResult += "<CurrentTransportStatus>OK</CurrentTransportStatus>";
        actionResult += "<CurrentSpeed>1</CurrentSpeed>";
    } else if (actionName == "GetPositionInfo") {
        auto formatTime = [](uint32_t ms) {
            uint32_t s = ms / 1000;
            int h = s / 3600;
            int m = (s % 3600) / 60;
            int secs = s % 60;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, secs);
            return String(buf);
        };

        String duration = formatTime(audioStreamer.getAudioDuration());
        String relTime = formatTime(audioStreamer.getAudioPos());

        actionResult = "<Track>1</Track>";
        actionResult += "<TrackDuration>" + duration + "</TrackDuration>";
        actionResult += "<TrackMetaData></TrackMetaData>";
        actionResult += "<TrackURI>" + currentURI + "</TrackURI>";
        actionResult += "<RelTime>" + relTime + "</RelTime>";
        actionResult += "<AbsTime>" + relTime + "</AbsTime>";
        actionResult += "<RelCount>2147483647</RelCount>";
        actionResult += "<AbsCount>2147483647</AbsCount>";
    } else if (actionName == "GetMediaInfo") {
        // VLC and Hi-Fi Cast request this to get current media information
        auto formatTime = [](uint32_t ms) {
            uint32_t s = ms / 1000;
            int h = s / 3600;
            int m = (s % 3600) / 60;
            int secs = s % 60;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, secs);
            return String(buf);
        };

        String duration = formatTime(audioStreamer.getAudioDuration());

        actionResult = "<NrTracks>1</NrTracks>";
        actionResult += "<MediaDuration>" + duration + "</MediaDuration>";
        actionResult += "<CurrentURI>" + currentURI + "</CurrentURI>";
        actionResult += "<CurrentURIMetaData>" + currentURIMetadata + "</CurrentURIMetaData>";
        actionResult += "<NextURI></NextURI>";
        actionResult += "<NextURIMetaData></NextURIMetaData>";
        actionResult += "<PlayMedium>NETWORK</PlayMedium>";
        actionResult += "<RecordMedium>NOT_IMPLEMENTED</RecordMedium>";
        actionResult += "<WriteStatus>NOT_IMPLEMENTED</WriteStatus>";
    } else if (actionName == "Seek") {
        // VLC uses this for seeking within media
        // For now, we don't support seeking in streams, but we must respond
        Serial.println("[DLNA] Seek requested (not implemented for streams)");
        actionResult = "";  // Empty response = success
    } else if (actionName == "Next" || actionName == "Previous") {
        // VLC may send these for playlist navigation
        Serial.printf("[DLNA] %s requested (not implemented)\n", actionName.c_str());
        actionResult = "";  // Empty response = success
    } else if (actionName == "GetTransportSettings") {
        actionResult = "<PlayMode>NORMAL</PlayMode>";
        actionResult += "<RecQualityMode>NOT_IMPLEMENTED</RecQualityMode>";
    } else if (actionName == "GetCurrentTransportActions") {
        // Tell controller what actions are available in current state
        String actions = "Play,Stop,Pause";
        if (audioStreamer.getState() == STREAM_PLAYING) {
            actions += ",Seek";
        }
        actionResult = "<Actions>" + actions + "</Actions>";
    }

    Serial.printf("[SOAP] AVTransport Action: %s\n", actionName.c_str());

    // SOAP response with proper headers
    String response = "<?xml version=\"1.0\"?>";
    response += "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
    response += "<s:Body>";
    response += "<u:" + actionName + "Response xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">";
    response += actionResult;
    response += "</u:" + actionName + "Response>";
    response += "</s:Body>";
    response += "</s:Envelope>";

    httpServer->sendHeader("Content-Type", "text/xml; charset=\"utf-8\"");
    httpServer->sendHeader("Server", "FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->sendHeader("EXT", "");
    httpServer->send(200, "text/xml; charset=\"utf-8\"", response);
}

void DLNARenderer::handleConnectionManagerControl() {
    String soapAction = httpServer->header("SOAPACTION");
    int hashPos = soapAction.lastIndexOf('#');
    String actionName = (hashPos != -1) ? soapAction.substring(hashPos + 1) : "Unknown";
    actionName.replace("\"", "");

    String actionResult = "";

    if (actionName == "GetProtocolInfo") {
        // Comprehensive protocol info for VLC, Hi-Fi Cast compatibility
        // Format: protocol:network:contentFormat:additionalInfo
        String protocolInfo =
            "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;DLNA.ORG_OP=01;DLNA.ORG_FLAGS=01700000000000000000000000000000,"
            "http-get:*:audio/mpeg:*,"
            "http-get:*:audio/mp3:*,"
            "http-get:*:audio/x-mpeg:*,"
            "http-get:*:audio/aac:*,"
            "http-get:*:audio/x-aac:*,"
            "http-get:*:audio/mp4:*,"
            "http-get:*:audio/x-m4a:*,"
            "http-get:*:audio/flac:*,"
            "http-get:*:audio/x-flac:*,"
            "http-get:*:audio/wav:*,"
            "http-get:*:audio/x-wav:*,"
            "http-get:*:audio/L16:*,"
            "http-get:*:audio/vnd.dlna.adts:*,"
            "http-get:*:application/ogg:*";
        actionResult = "<Source></Source>";
        actionResult += "<Sink>" + protocolInfo + "</Sink>";
    } else if (actionName == "GetCurrentConnectionIDs") {
        actionResult = "<ConnectionIDs>0</ConnectionIDs>";
    } else if (actionName == "GetCurrentConnectionInfo") {
        actionResult = "<RcsID>-1</RcsID><AVTransportID>-1</AVTransportID><ProtocolInfo></ProtocolInfo><PeerConnectionManager></PeerConnectionManager><PeerConnectionID>-1</PeerConnectionID><Direction>Input</Direction><Status>OK</Status>";
    }

    String response = "<?xml version=\"1.0\"?>";
    response += "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
    response += "<s:Body>";
    response += "<u:" + actionName + "Response xmlns:u=\"urn:schemas-upnp-org:service:ConnectionManager:1\">";
    response += actionResult;
    response += "</u:" + actionName + "Response>";
    response += "</s:Body>";
    response += "</s:Envelope>";

    httpServer->sendHeader("Content-Type", "text/xml; charset=\"utf-8\"");
    httpServer->sendHeader("Server", "FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->sendHeader("EXT", "");
    httpServer->send(200, "text/xml; charset=\"utf-8\"", response);
}

void DLNARenderer::handleRenderingControl() {
    // High-priority entry log to verify the endpoint is hit
    Serial.println("[DLNA] Incoming RenderingControl request");
    String soapAction = httpServer->header("SOAPACTION");
    Serial.printf("[DLNA] SOAPACTION: %s\n", soapAction.c_str());

    // Retrieve the SOAP body robustly to handle complex XML structures from apps like BubbleUPnP
    String body = "";
    if (httpServer->hasArg("plain")) {
        body = httpServer->arg("plain");
    } else if (httpServer->args() > 0) {
        body = httpServer->arg(0);
    }

    // Handle potential fragmented body reads if Content-Length is larger than currently buffered data
    String contentLenStr = httpServer->header("Content-Length");
    if (contentLenStr.length() > 0) {
        int expected = contentLenStr.toInt();
        if (expected > body.length()) {
            WiFiClient client = httpServer->client();
            unsigned long start = millis();
            while ((int)body.length() < expected && millis() - start < 2000) {
                while (client.available() > 0 && (int)body.length() < expected) {
                    char c = client.read();
                    body += c;
                }
                delay(1);
            }
        }
    }

    int hashPos = soapAction.lastIndexOf('#');
    String actionName = (hashPos != -1) ? soapAction.substring(hashPos + 1) : "Unknown";
    actionName.replace("\"", "");
    actionName.trim();

    String actionResult = "";

    if (actionName == "GetVolume") {
        uint8_t currentVol = audioOutput.getVolume();
        actionResult = "<CurrentVolume>" + String(currentVol) + "</CurrentVolume>";
    } else if (actionName == "GetMute") {
        actionResult = "<CurrentMute>" + String(isMuted ? "1" : "0") + "</CurrentMute>";
    } else if (actionName.equalsIgnoreCase("SetMute")) {
        // Parse mute value from body
        int muteVal = -1;

        // Check for DesiredMute in arguments or body
        if (httpServer->hasArg("DesiredMute")) {
            muteVal = httpServer->arg("DesiredMute").toInt();
        } else {
            int tagPos = body.indexOf("DesiredMute");
            if (tagPos != -1) {
                int startBracket = body.indexOf('>', tagPos);
                int endBracket = body.indexOf('<', startBracket != -1 ? startBracket : tagPos);

                if (startBracket != -1 && endBracket != -1 && endBracket > startBracket) {
                    String val = body.substring(startBracket + 1, endBracket);
                    val.trim();
                    muteVal = val.toInt();
                }
            }
        }

        if (muteVal == 1 && !isMuted) {
            // Mute: save current volume and set to 0
            lastVolume = audioOutput.getVolume();
            audioOutput.setVolume(0);
            isMuted = true;
            Serial.println("[DLNA] Muted");
        } else if (muteVal == 0 && isMuted) {
            // Unmute: restore previous volume
            audioOutput.setVolume(lastVolume);
            isMuted = false;
            Serial.println("[DLNA] Unmuted");
        }
        actionResult = "";
    } else if (actionName.equalsIgnoreCase("SetVolume")) {
        Serial.println("[DLNA] Identified SetVolume action");
        
        int vol = -1;
        // 1. Check named argument first
        if (httpServer->hasArg("DesiredVolume")) {
            vol = httpServer->arg("DesiredVolume").toInt();
        } else if (httpServer->hasArg("u:DesiredVolume")) {
            vol = httpServer->arg("u:DesiredVolume").toInt();
        }
        
        // 2. Aggressive body parsing for DesiredVolume (handles namespaces like <u:DesiredVolume>)
        // Look for the value between the first '>' and the following '<' after "DesiredVolume"
        if (vol == -1) {
            int tagPos = body.indexOf("DesiredVolume");
            if (tagPos != -1) {
                int startBracket = body.indexOf('>', tagPos);
                int endBracket = body.indexOf('<', startBracket != -1 ? startBracket : tagPos);
                
                if (startBracket != -1 && endBracket != -1 && endBracket > startBracket) {
                    String val = body.substring(startBracket + 1, endBracket);
                    val.trim();
                    vol = val.toInt();
                }
            }
        }

        if (vol >= 0 && vol <= 100) {
            audioOutput.setVolume((uint8_t)vol);
            // If volume is set while muted, unmute
            if (vol > 0 && isMuted) {
                isMuted = false;
                Serial.printf("[DLNA] Volume changed to %d%% (unmuted)\n", vol);
            } else {
                Serial.printf("[DLNA] Volume changed to %d%%\n", vol);
            }
            lastVolume = vol;
        } else {
            Serial.printf("[DLNA] Could not parse volume value from SOAP body. Action: %s\n", actionName.c_str());
        }
        actionResult = "";
    }

    Serial.printf("[SOAP] RenderingControl Action: %s\n", actionName.c_str());

    String response = "<?xml version=\"1.0\"?>";
    response += "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
    response += "<s:Body>";
    response += "<u:" + actionName + "Response xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">";
    response += actionResult;
    response += "</u:" + actionName + "Response>";
    response += "</s:Body>";
    response += "</s:Envelope>";

    httpServer->sendHeader("Content-Type", "text/xml; charset=\"utf-8\"");
    httpServer->sendHeader("Server", "FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->sendHeader("EXT", "");
    httpServer->send(200, "text/xml; charset=\"utf-8\"", response);
}

void DLNARenderer::handleAVTransportSCPD() {
    String xml = generateAVTransportSCPD();
    httpServer->sendHeader("Server", "FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->send(200, "text/xml; charset=\"utf-8\"", xml);
    Serial.println("[HTTP] Sent AVTransport SCPD");
}

void DLNARenderer::handleRenderingControlSCPD() {
    String xml = generateRenderingControlSCPD();
    httpServer->sendHeader("Server", "FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->send(200, "text/xml; charset=\"utf-8\"", xml);
    Serial.println("[HTTP] Sent RenderingControl SCPD");
}

void DLNARenderer::handleConnectionManagerSCPD() {
    String xml = generateConnectionManagerSCPD();
    httpServer->sendHeader("Server", "FreeRTOS/10.0 UPnP/1.1 ESP32-Speaker/1.0");
    httpServer->send(200, "text/xml; charset=\"utf-8\"", xml);
    Serial.println("[HTTP] Sent ConnectionManager SCPD");
}

String DLNARenderer::generateDeviceDescription() {
    char ipBuf[IP_STRING_BUFFER_SIZE];
    WiFi.localIP().toString().toCharArray(ipBuf, sizeof(ipBuf));

    String xml;
    xml.reserve(XML_RESERVE_SIZE);  // Pre-allocate to avoid reallocations

    xml = "<?xml version=\"1.0\"?>\n";
    xml += "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\">\n";
    xml += "  <specVersion>\n";
    xml += "    <major>1</major>\n";
    xml += "    <minor>0</minor>\n";
    xml += "  </specVersion>\n";
    xml += "  <device>\n";
    xml += "    <deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>\n";
    xml += "    <dlna:X_DLNADOC xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\">DMR-1.50</dlna:X_DLNADOC>\n";
    xml += "    <friendlyName>" + deviceName + "</friendlyName>\n";
    xml += "    <manufacturer>Espressif Systems</manufacturer>\n";
    xml += "    <manufacturerURL>https://www.espressif.com</manufacturerURL>\n";
    xml += "    <modelDescription>ESP32 DLNA Digital Media Renderer</modelDescription>\n";
    xml += "    <modelName>" + deviceModel + "</modelName>\n";
    xml += "    <modelNumber>1.0</modelNumber>\n";
    xml += "    <modelURL>https://github.com/espressif/esp32</modelURL>\n";
    xml += "    <serialNumber>" + WiFi.macAddress() + "</serialNumber>\n";
    xml += "    <UDN>" + deviceUUID + "</UDN>\n";
    xml += "    <presentationURL>http://";
    xml += ipBuf;
    xml += "/</presentationURL>\n";
    xml += "    <iconList>\n";
    xml += "      <icon>\n";
    xml += "        <mimetype>image/png</mimetype>\n";
    xml += "        <width>48</width>\n";
    xml += "        <height>48</height>\n";
    xml += "        <depth>24</depth>\n";
    xml += "        <url>/icon48.png</url>\n";
    xml += "      </icon>\n";
    xml += "    </iconList>\n";
    xml += "    <serviceList>\n";
    xml += "      <service>\n";
    xml += "        <serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>\n";
    xml += "        <serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>\n";
    xml += "        <controlURL>/avt/control</controlURL>\n";
    xml += "        <eventSubURL>/avt/event</eventSubURL>\n";
    xml += "        <SCPDURL>/avt/scpd.xml</SCPDURL>\n";
    xml += "      </service>\n";
    xml += "      <service>\n";
    xml += "        <serviceType>urn:schemas-upnp-org:service:RenderingControl:1</serviceType>\n";
    xml += "        <serviceId>urn:upnp-org:serviceId:RenderingControl</serviceId>\n";
    xml += "        <controlURL>/rc/control</controlURL>\n";
    xml += "        <eventSubURL>/rc/event</eventSubURL>\n";
    xml += "        <SCPDURL>/rc/scpd.xml</SCPDURL>\n";
    xml += "      </service>\n";
      xml += "      <service>\n";
      xml += "        <serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>\n";
      xml += "        <serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>\n";
      xml += "        <controlURL>/cm/control</controlURL>\n";
      xml += "        <eventSubURL>/cm/event</eventSubURL>\n";
      xml += "        <SCPDURL>/cm/scpd.xml</SCPDURL>\n";
      xml += "      </service>\n";
    xml += "    </serviceList>\n";
    xml += "  </device>\n";
    xml += "</root>";

    return xml;
}

String DLNARenderer::generateAVTransportSCPD() {
    String xml;
    xml.reserve(XML_RESERVE_SIZE);  // Pre-allocate

    xml = "<?xml version=\"1.0\"?>\n";
    xml += "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">\n";
    xml += "  <specVersion><major>1</major><minor>0</minor></specVersion>\n";
    xml += "  <actionList>\n";
    xml += "    <action>\n";
    xml += "      <name>SetAVTransportURI</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentURI</name><direction>in</direction><relatedStateVariable>AVTransportURI</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentURIMetaData</name><direction>in</direction><relatedStateVariable>AVTransportURIMetaData</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>Play</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Speed</name><direction>in</direction><relatedStateVariable>TransportPlaySpeed</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>Stop</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>Pause</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>GetTransportInfo</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentTransportState</name><direction>out</direction><relatedStateVariable>TransportState</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentTransportStatus</name><direction>out</direction><relatedStateVariable>TransportStatus</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentSpeed</name><direction>out</direction><relatedStateVariable>TransportPlaySpeed</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>GetPositionInfo</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Track</name><direction>out</direction><relatedStateVariable>CurrentTrack</relatedStateVariable></argument>\n";
    xml += "        <argument><name>TrackDuration</name><direction>out</direction><relatedStateVariable>CurrentTrackDuration</relatedStateVariable></argument>\n";
    xml += "        <argument><name>TrackMetaData</name><direction>out</direction><relatedStateVariable>CurrentTrackMetaData</relatedStateVariable></argument>\n";
    xml += "        <argument><name>TrackURI</name><direction>out</direction><relatedStateVariable>CurrentTrackURI</relatedStateVariable></argument>\n";
    xml += "        <argument><name>RelTime</name><direction>out</direction><relatedStateVariable>RelativeTimePosition</relatedStateVariable></argument>\n";
    xml += "        <argument><name>AbsTime</name><direction>out</direction><relatedStateVariable>AbsoluteTimePosition</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>GetMediaInfo</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>NrTracks</name><direction>out</direction><relatedStateVariable>NumberOfTracks</relatedStateVariable></argument>\n";
    xml += "        <argument><name>MediaDuration</name><direction>out</direction><relatedStateVariable>CurrentMediaDuration</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentURI</name><direction>out</direction><relatedStateVariable>AVTransportURI</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentURIMetaData</name><direction>out</direction><relatedStateVariable>AVTransportURIMetaData</relatedStateVariable></argument>\n";
    xml += "        <argument><name>NextURI</name><direction>out</direction><relatedStateVariable>NextAVTransportURI</relatedStateVariable></argument>\n";
    xml += "        <argument><name>NextURIMetaData</name><direction>out</direction><relatedStateVariable>NextAVTransportURIMetaData</relatedStateVariable></argument>\n";
    xml += "        <argument><name>PlayMedium</name><direction>out</direction><relatedStateVariable>PlaybackStorageMedium</relatedStateVariable></argument>\n";
    xml += "        <argument><name>RecordMedium</name><direction>out</direction><relatedStateVariable>RecordStorageMedium</relatedStateVariable></argument>\n";
    xml += "        <argument><name>WriteStatus</name><direction>out</direction><relatedStateVariable>RecordMediumWriteStatus</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>Seek</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Unit</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_SeekMode</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Target</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_SeekTarget</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>Next</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>Previous</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>GetTransportSettings</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>PlayMode</name><direction>out</direction><relatedStateVariable>CurrentPlayMode</relatedStateVariable></argument>\n";
    xml += "        <argument><name>RecQualityMode</name><direction>out</direction><relatedStateVariable>CurrentRecordQualityMode</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>GetCurrentTransportActions</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Actions</name><direction>out</direction><relatedStateVariable>CurrentTransportActions</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "  </actionList>\n";
    xml += "  <serviceStateTable>\n";
    xml += "    <stateVariable sendEvents=\"yes\">\n";
    xml += "      <name>TransportState</name>\n";
    xml += "      <dataType>string</dataType>\n";
    xml += "      <allowedValueList>\n";
    xml += "        <allowedValue>STOPPED</allowedValue>\n";
    xml += "        <allowedValue>PLAYING</allowedValue>\n";
    xml += "        <allowedValue>PAUSED_PLAYBACK</allowedValue>\n";
    xml += "        <allowedValue>TRANSITIONING</allowedValue>\n";
    xml += "        <allowedValue>NO_MEDIA_PRESENT</allowedValue>\n";
    xml += "      </allowedValueList>\n";
    xml += "    </stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>TransportStatus</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>TransportPlaySpeed</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>AVTransportURI</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>AVTransportURIMetaData</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\">\n";
    xml += "      <name>CurrentTrack</name>\n";
    xml += "      <dataType>ui4</dataType>\n";
    xml += "      <allowedValueRange><minimum>0</minimum><maximum>1</maximum><step>1</step></allowedValueRange>\n";
    xml += "    </stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>CurrentTrackDuration</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>CurrentTrackMetaData</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>CurrentTrackURI</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>RelativeTimePosition</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>AbsoluteTimePosition</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>NumberOfTracks</name><dataType>ui4</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>CurrentMediaDuration</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>NextAVTransportURI</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>NextAVTransportURIMetaData</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>PlaybackStorageMedium</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>RecordStorageMedium</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>RecordMediumWriteStatus</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>CurrentPlayMode</name><dataType>string</dataType><defaultValue>NORMAL</defaultValue></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>CurrentRecordQualityMode</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>CurrentTransportActions</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"no\"><name>A_ARG_TYPE_SeekMode</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"no\"><name>A_ARG_TYPE_SeekTarget</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"no\"><name>A_ARG_TYPE_InstanceID</name><dataType>ui4</dataType></stateVariable>\n";
    xml += "  </serviceStateTable>\n";
    xml += "</scpd>";

    return xml;
}

String DLNARenderer::generateRenderingControlSCPD() {
    String xml;
    xml.reserve(XML_RESERVE_SIZE);  // Pre-allocate

    xml = "<?xml version=\"1.0\"?>\n";
    xml += "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">\n";
    xml += "  <specVersion>\n";
    xml += "    <major>1</major>\n";
    xml += "    <minor>0</minor>\n";
    xml += "  </specVersion>\n";
    xml += "  <actionList>\n";
    xml += "    <action>\n";
    xml += "      <name>GetVolume</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Channel</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentVolume</name><direction>out</direction><relatedStateVariable>Volume</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>SetVolume</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Channel</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable></argument>\n";
    xml += "        <argument><name>DesiredVolume</name><direction>in</direction><relatedStateVariable>Volume</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>GetMute</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Channel</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable></argument>\n";
    xml += "        <argument><name>CurrentMute</name><direction>out</direction><relatedStateVariable>Mute</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "    <action>\n";
    xml += "      <name>SetMute</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>InstanceID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Channel</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable></argument>\n";
    xml += "        <argument><name>DesiredMute</name><direction>in</direction><relatedStateVariable>Mute</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "  </actionList>\n";
    xml += "  <serviceStateTable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>LastChange</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>Volume</name><dataType>ui2</dataType><allowedValueRange><minimum>0</minimum><maximum>100</maximum><step>1</step></allowedValueRange></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>Mute</name><dataType>boolean</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"no\"><name>A_ARG_TYPE_Channel</name><dataType>string</dataType><allowedValueList><allowedValue>Master</allowedValue></allowedValueList></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"no\"><name>A_ARG_TYPE_InstanceID</name><dataType>ui4</dataType></stateVariable>\n";
    xml += "  </serviceStateTable>\n";
    xml += "</scpd>\n";

    return xml;
}

String DLNARenderer::generateConnectionManagerSCPD() {
    String xml;
    xml.reserve(1024);  // Smaller reserve for this simpler SCPD

    xml = "<?xml version=\"1.0\"?>\n";
    xml += "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">\n";
    xml += "  <specVersion><major>1</major><minor>0</minor></specVersion>\n";
    xml += "  <actionList>\n";
    xml += "    <action>\n";
    xml += "      <name>GetProtocolInfo</name>\n";
    xml += "      <argumentList>\n";
    xml += "        <argument><name>Source</name><direction>out</direction><relatedStateVariable>SourceProtocolInfo</relatedStateVariable></argument>\n";
    xml += "        <argument><name>Sink</name><direction>out</direction><relatedStateVariable>SinkProtocolInfo</relatedStateVariable></argument>\n";
    xml += "      </argumentList>\n";
    xml += "    </action>\n";
    xml += "  </actionList>\n";
    xml += "  <serviceStateTable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>SourceProtocolInfo</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"yes\"><name>SinkProtocolInfo</name><dataType>string</dataType></stateVariable>\n";
    xml += "    <stateVariable sendEvents=\"no\"><name>CurrentConnectionIDs</name><dataType>string</dataType></stateVariable>\n";
    xml += "  </serviceStateTable>\n";
    xml += "</scpd>";

    return xml;
}

bool DLNARenderer::isRunning() {
    return initialized;
}

String DLNARenderer::extractURIFromRequest(const String& body) {
    // 1) Check POST args
    for (int i = 0; i < httpServer->args(); i++) {
        String name = httpServer->argName(i);
        if (name.equalsIgnoreCase("CurrentURI") || name.equalsIgnoreCase("Uri") || name.equalsIgnoreCase("URI")) {
            String v = httpServer->arg(i);
            v.trim();
            if (v.length() > 0) return v;
        }
    }

    // 2) Check common nonstandard headers
    const char* altHeaders[] = {"X-CurrentURI", "X-AV-URI", "X-URI", "X-Uri", "URI", "URL", "X-AV-URL", "X-Target-URI"};
    for (auto h : altHeaders) {
        String hv = httpServer->header(h);
        if (hv.length() > 0) {
            hv.trim();
            if (hv.length() > 0) return hv;
        }
    }

    // 3) Look in body for CurrentURI or NextURI tags
    String tmp = body;
    // Try multiple rounds of unescaping entities to penetrate nested XML (like Metadata)
    for (int pass = 0; pass < 3; pass++) {
        // Expanded tag list: 'res' is common for URIs inside Metadata blocks
        const char* tags[] = {"CurrentURI", "NextURI", "res", "URI", "URL", "val"};
        for (auto t : tags) {
            int pos = tmp.indexOf(t);
            // Use case-insensitive check and ensure we don't accidentally match parts of other tags
            if (pos == -1) {
                String lowerT = String(t);
                lowerT.toLowerCase();
                pos = tmp.indexOf(lowerT);
            }
            if (pos == -1) {
                String upperT = String(t);
                upperT.toUpperCase();
                pos = tmp.indexOf(upperT);
            }
            
            if (pos == -1) continue;

            int openEnd = tmp.indexOf('>', pos); 
            int closeStart = -1;
            if (openEnd != -1) closeStart = tmp.indexOf("</", openEnd);
            
            if (openEnd != -1 && closeStart != -1 && closeStart > openEnd) {
                String extracted = tmp.substring(openEnd + 1, closeStart);
                extracted.trim();
                if (extracted.indexOf("![CDATA[") != -1) {
                    int cstart = extracted.indexOf("CDATA[") + 6;
                    int cend = extracted.lastIndexOf("]]");
                    if (cend > cstart && cstart >= 6) extracted = extracted.substring(cstart, cend);
                }
                // Return only if it looks like a valid URL
                if (extracted.startsWith("http") || extracted.indexOf("://") != -1) {
                    return extracted;
                }
            }
        }

        if (tmp.indexOf("&lt;") != -1 || tmp.indexOf("&amp;") != -1) {
            tmp.replace("&lt;", "<");
            tmp.replace("&gt;", ">");
            tmp.replace("&quot;", "\"");
            tmp.replace("&apos;", "'");
            tmp.replace("&amp;", "&");
        } else break;
    }

    // 4) Desperate search for any HTTP/HTTPS URL string in the raw body
    int httpPos = body.indexOf("http://");
    if (httpPos == -1) httpPos = body.indexOf("https://");
    if (httpPos != -1) {
        // Find end of URL (space, double quote, single quote, <, or &)
        int end = body.length();
        const char* terminators = " \"'<&";
        for (int i = 0; i < 5; i++) {
            int t = body.indexOf(terminators[i], httpPos);
            if (t != -1 && t < end) end = t;
        }
        String url = body.substring(httpPos, end);
        url.trim();
        if (url.length() > 15) {
            // Exclude common SOAP/UPnP/XML namespaces that are part of the protocol metadata
            if (url.indexOf("schemas.xmlsoap.org") == -1 && 
                url.indexOf("schemas-upnp-org") == -1 &&
                url.indexOf("www.w3.org") == -1 &&
                url.indexOf("<") == -1 && url.indexOf(">") == -1 && url.indexOf("&lt;") == -1) {
                return url;
            }
        }
    }

    // 5) URL-encoded forms: look for CurrentURI=...
    int paramPos = -1;
    if ((paramPos = body.indexOf("CurrentURI%3D")) != -1 || (paramPos = body.indexOf("CurrentURI=")) != -1) {
        int valStart = body.indexOf('=', paramPos);
        if (valStart != -1) {
            String enc = body.substring(valStart + 1);
            int amp = enc.indexOf('&');
            if (amp != -1) enc = enc.substring(0, amp);
            // simple URL-decode
            String dec = "";
            for (int i = 0; i < enc.length(); i++) {
                char c = enc[i];
                if (c == '+') dec += ' ';
                else if (c == '%' && i + 2 < enc.length()) {
                    char h1 = enc[i+1]; char h2 = enc[i+2];
                    auto hexval = [](char h)->int { if (h>='0'&&h<='9') return h-'0'; if (h>='A'&&h<='F') return h-'A'+10; if (h>='a'&&h<='f') return h-'a'+10; return 0; };
                    int hi = hexval(h1), lo = hexval(h2);
                    dec += (char)((hi<<4)+lo); i+=2;
                } else dec += c;
            }
            dec.trim();
            if (dec.length() > 0) return dec;
        }
    }

    return "";
}
