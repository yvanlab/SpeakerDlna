/**
 * Bluetooth Speaker Implementation (Simplified - disabled for compatibility)
 */

#include "BluetoothSpeaker.h"

BluetoothSpeaker::BluetoothSpeaker(I2SAudioOutput& audioOut)
    : audioOutput(audioOut), initialized(false) {
}

BluetoothSpeaker::~BluetoothSpeaker() {
    end();
}

bool BluetoothSpeaker::begin(const char* deviceName) {
    Serial.println("Bluetooth A2DP temporarily disabled due to library compatibility");
    Serial.println("Use HEOS or DLNA for audio streaming");

    initialized = false;
    return false;
}

void BluetoothSpeaker::end() {
    initialized = false;
}

bool BluetoothSpeaker::isConnected() {
    return false;
}

String BluetoothSpeaker::getConnectedDeviceName() {
    return "";
}
