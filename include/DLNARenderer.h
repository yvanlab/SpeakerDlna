/**
 * DLNA/UPnP Media Renderer
 */

#ifndef DLNA_RENDERER_H
#define DLNA_RENDERER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include "AudioOutput.h"
#include "AudioStreamer.h"

class DLNARenderer {
public:
    DLNARenderer(I2SAudioOutput& audioOut, AudioStreamer& streamer);
    ~DLNARenderer();

    bool begin(const char* name, const char* model, uint16_t port);
    void end();
    void handle();

    bool isRunning();

private:
    I2SAudioOutput& audioOutput;
    AudioStreamer& audioStreamer;
    WebServer* httpServer;
    WiFiUDP ssdpSocket;

    String deviceName;
    String deviceModel;
    String deviceUUID;
    String currentURI;
    uint16_t serverPort;
    bool awaitingURI;

    bool initialized;
    unsigned long lastSSDPAnnounce;
    uint32_t bootId;

    void handleSSDP();
    void sendSSDPResponse(IPAddress remoteIP, uint16_t remotePort, const char* searchTarget);
    void sendSSDPNotify();

    // HTTP handlers
    void handleDescription();
    void handleEventSubscription();
    void handleAVTransportControl();
    void handleConnectionManagerControl();
    void handleRenderingControl();
    void handleAVTransportSCPD();
    void handleRenderingControlSCPD();
    void handleConnectionManagerSCPD();

    // Attempt to extract a URI from the current HTTP request (headers/args/body)
    String extractURIFromRequest(const String& body);

    String generateDeviceDescription();
    String generateAVTransportSCPD();
    String generateRenderingControlSCPD();
    String generateConnectionManagerSCPD();
};

#endif // DLNA_RENDERER_H
