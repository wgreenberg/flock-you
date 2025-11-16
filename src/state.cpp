#include "state.h"
#include "beep.h"
#include "ble.h"
#include "wifiScan.h"
#include "scanManager.h"

void State::resetDetection() {
    this->deviceInRange = false;
    this->lastDetectionTime = 0;
    this->lastHeartbeat = 0;
}

void State::foxhunt(const uint8_t mac[6], bool doNotify) {
    this->mode = State::ModeType::Foxhunter;
    for (int i=0; i<6; i++) {
        this->foxhunterState.target_mac[i] = mac[i];
        if (doNotify) {
            // FIXME
        }
    }
    this->foxhunterState.rssi = 0;
    this->resetDetection();
}

void State::detect() {
    if (this->mode == ModeType::Detector) {
        return;
    }
    this->mode = State::ModeType::Detector;
    this->resetDetection();
}

void State::handleScanDetection(const uint8_t mac[6], ScanResult *result) {
    sendDetectionEvent(mac, result->detectionType, result->categoryName);
    if (!this->deviceInRange) {
        detectionBeepSequence();
        this->deviceInRange = true;
        this->lastHeartbeat = millis();

        // if we're not already hunting, find this new device
        if (this->mode != ModeType::Foxhunter) {
            this->foxhunt(mac, true);
            this->foxhuntAutoOff = true; // if we go out of range, go back to detect mode
        }
    }
    this->lastDetectionTime = millis();
}

void State::update() {
    // Handle heartbeat pulse if device is in range
    if (this->deviceInRange) {
        // Check if device has gone out of range (no detection for 30 seconds)
        if (millis() - this->lastDetectionTime >= 30000) {
            printf("Device out of range - stopping heartbeat\n");
            this->deviceInRange = false;
            outOfRangeBeep();
            if (this->foxhuntAutoOff && this->mode == State::ModeType::Foxhunter) {
                this->detect();
            }
            return;
        }

        unsigned long interval = this->mode == State::ModeType::Detector ? 10000 : 3000;
        if (millis() - this->lastHeartbeat >= interval) {
            if (this->mode == State::ModeType::Detector) {
                heartbeatPulse();
            } else {
                proximityBeep(this->foxhunterState.rssi);
            }
            this->lastHeartbeat = millis();
        }
    }
}

void State::handleWifiPacket(
    std::string ssid,
    const uint8_t mac[6],
    int rssi,
    int frameType
) {
    sendWiFiDeviceInfo(ssid, current_channel, mac, rssi, frameType);

    bool flockDetected = ssid.length() > 0 && scanManager->checkSSIDPattern(mac, ssid) ||
        scanManager->checkMACPrefix(mac);
    if (flockDetected) {
        this->handleScanDetection(mac, &scanManager->scanResult);
    }
    if (this->mode == State::ModeType::Foxhunter) {
        this->handleFoxhuntQuery(mac, rssi);
    }
}

void State::handleBLEPacket(
    const uint8_t mac[6],
    std::string name,
    int rssi,
    std::vector<u16_t> manufacturerIDs,
    std::vector<std::string> manufacturerData
) {
    sendBLEDeviceInfo(mac, name.c_str(), rssi, manufacturerIDs, manufacturerData);

    bool flockDetected = scanManager->checkBLEManufacturerIDs(mac, manufacturerIDs)
        || scanManager->checkMACPrefix(mac)
        || scanManager->checkBLEDeviceName(mac, name);
    if (flockDetected) {
        this->handleScanDetection(mac, &scanManager->scanResult);
    }

    if (this->mode == State::ModeType::Foxhunter) {
        this->handleFoxhuntQuery(mac, rssi);
    }
}

void State::handleFoxhuntQuery(const uint8_t mac[6], int rssi) {
    for (int i=0; i<6; i++) {
        if (mac[i] != this->foxhunterState.target_mac[i]) {
            return;
        }
    }
    this->lastDetectionTime = millis();
    this->foxhunterState.rssi = rssi;
    if (!this->deviceInRange) {
        this->deviceInRange = true;
    }
}