/**
 * Bluetooth A2DP Sink Speaker (Simplified - disabled for now)
 */

#ifndef BLUETOOTH_SPEAKER_H
#define BLUETOOTH_SPEAKER_H

#include <Arduino.h>
#include "AudioOutput.h"

class BluetoothSpeaker {
public:
    BluetoothSpeaker(I2SAudioOutput& audioOut);
    ~BluetoothSpeaker();

    bool begin(const char* deviceName);
    void end();

    bool isConnected();
    String getConnectedDeviceName();

private:
    I2SAudioOutput& audioOutput;
    bool initialized;
};

#endif // BLUETOOTH_SPEAKER_H
