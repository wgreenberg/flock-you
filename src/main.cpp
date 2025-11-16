#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <MsgPack.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "scanManager.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

// Hardware Configuration
#define BUZZER_PIN 3  // GPIO3 (D2) - PWM capable pin on Xiao ESP32 S3

// Audio Configuration
#define MUTE 0
#define LOW_FREQ 200      // Boot sequence - low pitch
#define HIGH_FREQ 800     // Boot sequence - high pitch & detection alert
#define DETECT_FREQ 1000  // Detection alert - high pitch (faster beeps)
#define HEARTBEAT_FREQ 600 // Heartbeat pulse frequency
#define BOOT_BEEP_DURATION 300   // Boot beep duration
#define DETECT_BEEP_DURATION 150 // Detection beep duration (faster)
#define HEARTBEAT_DURATION 100   // Short heartbeat pulse

// WiFi Promiscuous Mode Configuration
#define MAX_CHANNEL 13
#define CHANNEL_HOP_INTERVAL 500  // milliseconds

// BLE SCANNING CONFIGURATION
#define BLE_SCAN_DURATION_SECS 1
#define BLE_SCAN_INTERVAL_MS 200
#define BLE_SCAN_WINDOW_MS 100
#define BLE_SCAN_DELAY_MS 1500
static unsigned long last_ble_scan = 0;

// Detection Pattern Limits
#define MAX_SSID_PATTERNS 10
#define MAX_MAC_PATTERNS 50
#define MAX_DEVICE_NAMES 20

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// BLE state
static NimBLEScan* pBLEScan;
static NimBLEServer* pServer;
static const char* BLE_DEVICE_NAME = "FlockYou";
static const NimBLEUUID BLE_SERVICE_UUID = NimBLEUUID(0xACAB0001);
static const char* SCAN_CHARACTERISTIC_UUID = "0001";
static const char* FOXHUNT_CHARACTERISTIC_UUID = "0002";

// WiFi state
static uint8_t current_channel = 1;
static unsigned long last_channel_hop = 0;

static ScanManager scanManager;

// ============================================================================
// AUDIO SYSTEM
// ============================================================================

const int B3hz = 247;
const int C4hz = 262;
const int D4hz = 294;
const int E4hz = 330;
const int F4hz = 349;
const int G4hz = 392;
const int A4hz = 440;
const int B4hz = 494;

void beep(int frequency, int duration_ms) {
    if (MUTE) return;
    tone(BUZZER_PIN, frequency, duration_ms);
    delay(duration_ms + 50);
}

void bootBeepSequence() {
    printf("Initializing audio system...\n");
    printf("Playing boot sequence: Low -> High pitch\n");
    beep(C4hz, BOOT_BEEP_DURATION);
    beep(B4hz, BOOT_BEEP_DURATION);
    beep(G4hz, BOOT_BEEP_DURATION);
    printf("Audio system ready\n\n");
}

void detectionBeepSequence() {
    printf("FLOCK SAFETY DEVICE DETECTED!\n");
    printf("Playing alert sequence: 3 fast high-pitch beeps\n");
    for (int i = 0; i < 3; i++) {
        beep(DETECT_FREQ, DETECT_BEEP_DURATION);
        if (i < 2) delay(50); // Short gap between beeps
    }
    printf("Detection complete - device identified!\n\n");
}

void notify(MsgPack::Packer packer) {
    NimBLEService* pSvc = pServer->getServiceByUUID(BLE_SERVICE_UUID);
    if (!pSvc) {
        printf("no service found\n");
        return;
    }
    NimBLECharacteristic* pChr = pSvc->getCharacteristic(SCAN_CHARACTERISTIC_UUID);
    if (!pChr) {
        printf("no characteristic found!\n");
        return;
    }
    if (packer.size() > 256) {
        printf("failed to notify, data too large!");
        MsgPack::Packer tooLargePacker;
        tooLargePacker.to_array("data_too_large");
        pChr->notify(tooLargePacker.data());
        return;
    }

    pChr->notify(packer.data(), packer.size());
}

void sendDetectionEvent(const uint8_t mac[6], std::string detectionType, std::string categoryName) {
    std::array<unsigned int, 6> packed_mac { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] }; 
    MsgPack::Packer packer;
    packer.to_array(
        "detection",
        detectionType.c_str(),
        categoryName.c_str(),
        packed_mac
    );
    notify(packer);
}

bool checkRange(int val, int low, int high) {
    return val >= low && val <= high;
}

void multibeep(std::vector<int> freqs, int total_duration) {
    int duration_per_beep = total_duration / freqs.size();
    for (int freq: freqs) {
        beep(freq, duration_per_beep);
    }
}

void proximityBeep(int rssi) {
    int duration = 500;
    if (rssi < -150) {
        // play 2 sad descending beeps
        multibeep({ C4hz, B3hz }, duration);
    } else if (checkRange(rssi, -150, -100)) {
        multibeep({ C4hz }, duration);
    } else if (checkRange(rssi, -100, -80)) {
        multibeep({ C4hz, E4hz }, duration);
    } else if (checkRange(rssi, -80, -50)) {
        multibeep({ C4hz, E4hz, G4hz }, duration);
    } else if (checkRange(rssi, -50, -20)) {
        multibeep({ C4hz, E4hz, G4hz, B4hz }, duration);
    } else {
        multibeep({ C4hz, 2 * C4hz, C4hz, 2 * C4hz }, duration);
    }
}

void outOfRangeBeep() {
    multibeep({ B3hz, B3hz, B3hz }, 300);
}

void heartbeatPulse() {
    printf("Heartbeat: Device still in range\n");
    beep(HEARTBEAT_FREQ, HEARTBEAT_DURATION);
    delay(100);
    beep(HEARTBEAT_FREQ, HEARTBEAT_DURATION);
}

// ============================================================================
// JSON OUTPUT FUNCTIONS
// ============================================================================

void sendWiFiDeviceInfo(std::string ssid, const uint8_t mac[6], int rssi, int frameType)
{
    std::array<unsigned int, 6> packed_mac { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] };

    MsgPack::Packer packer;
    packer.to_array(
        "wifi",
        rssi,
        ssid.c_str(),
        current_channel,
        packed_mac,
        frameType
    );

    notify(packer);
}

void sendBLEDeviceInfo(
    const uint8_t mac[6],
    const char* name,
    int rssi,
    std::vector<u16_t> manufacturerCodes,
    std::vector<std::string> manufacturerData
)
{
    std::array<unsigned int, 6> packed_mac { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] };
    std::vector<const char*> manufacturerDataCstr;

    for (int i=0; i<manufacturerData.size(); i++) {
        manufacturerDataCstr.push_back(manufacturerData[i].c_str());
    }

    MsgPack::Packer packer;
    packer.to_array(
        "bluetooth_le",
        name,
        rssi,
        packed_mac,
        manufacturerCodes,
        manufacturerDataCstr
    );
    
    notify(packer);
}

struct FoxhunterState {
    uint8_t target_mac[6];
    int rssi;
};

class State {
    public:
        bool muted = false;
        enum ModeType { Detector, Foxhunter } mode;
        State() {
            this->mode = State::ModeType::Detector;
            this->resetDetection();
            this->foxhunterState.rssi = 0;
        };
        void handleWifiPacket(std::string ssid, const uint8_t mac[6], int rssi, int frameType);
        void handleBLEPacket(
            const uint8_t mac[6],
            std::string name,
            int rssi,
            std::vector<u16_t> manufacturerCodes,
            std::vector<std::string> manufacturerData
        );
        void update();
        void foxhunt(const uint8_t mac[6]);
        void detect();
    private:
        bool deviceInRange;
        unsigned long lastDetectionTime;
        unsigned long lastHeartbeat;
        bool foxhuntAutoOff = false;
        FoxhunterState foxhunterState;
        void handleFoxhuntQuery(const uint8_t mac[6], int rssi);
        void handleScanDetection(const uint8_t mac[6], ScanResult result);
        void resetDetection();
};

void State::resetDetection() {
    this->deviceInRange = false;
    this->lastDetectionTime = 0;
    this->lastHeartbeat = 0;
}

void State::foxhunt(const uint8_t mac[6]) {
    this->mode = State::ModeType::Foxhunter;
    for (int i=0; i<6; i++) {
        this->foxhunterState.target_mac[i] = mac[i];
    }
    this->foxhunterState.rssi = 0;
    this->resetDetection();
}

void State::detect() {
    this->mode = State::ModeType::Detector;
    this->resetDetection();
}

void State::handleScanDetection(const uint8_t mac[6], ScanResult result) {
    sendDetectionEvent(mac, result.detectionType, result.categoryName);
    if (!this->deviceInRange) {
        detectionBeepSequence();
        this->deviceInRange = true;
        this->lastHeartbeat = millis();

        // if we're not already hunting, find this new device
        if (this->mode != ModeType::Foxhunter) {
            this->foxhunt(mac);
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
    sendWiFiDeviceInfo(ssid, mac, rssi, frameType);

    bool flockDetected = ssid.length() > 0 && scanManager.checkSSIDPattern(mac, ssid) ||
        scanManager.checkMACPrefix(mac);
    if (flockDetected) {
        this->handleScanDetection(mac, scanManager.scanResult);
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

    bool flockDetected = scanManager.checkBLEManufacturerIDs(mac, manufacturerIDs)
        || scanManager.checkMACPrefix(mac)
        || scanManager.checkBLEDeviceName(mac, name);
    if (flockDetected) {
        this->handleScanDetection(mac, scanManager.scanResult);
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

static State state;

// ============================================================================
// WIFI PROMISCUOUS MODE HANDLER
// ============================================================================

typedef struct {
    unsigned frame_ctrl:16;
    unsigned duration_id:16;
    uint8_t addr1[6]; /* receiver address */
    uint8_t addr2[6]; /* sender address */
    uint8_t addr3[6]; /* filtering address */
    unsigned sequence_ctrl:16;
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
} wifi_ieee80211_packet_t;

void wifiSnifferPacketHandler(void* buff, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) {
        return;
    }

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    uint8_t *payload = (uint8_t *)ipkt + 24;
    
    // Check for probe requests (subtype 0x04) and beacons (subtype 0x08)
    uint8_t frameType = ppkt->payload[0];
    if (frameType != 0x80 && frameType != 0x40) {
        return;
    }
    
    // Extract SSID from probe request or beacon
    std::string ssid = "";
    int ssid_start = frameType == 0x80 ? 13 : 1;
    uint8_t ssid_len = payload[ssid_start];
    if (ssid_len > 33) {
        return;
    }
    for (int i=0; i < ssid_len; i++) {
        ssid += (char)payload[ssid_start + i + 1];
    }

    state.handleWifiPacket(ssid, hdr->addr2, ppkt->rx_ctrl.rssi, frameType);
}

// ============================================================================
// BLE SCANNING
// ============================================================================

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        NimBLEAddress addr = advertisedDevice->getAddress();
        const uint8_t* mac = addr.getNative();
        
        int rssi = advertisedDevice->getRSSI();
        std::string name = "";
        if (advertisedDevice->haveName()) {
            name = advertisedDevice->getName();
        }

        std::vector<std::string> manufacturerData;
        std::vector<u16_t> manufacturerIDs;
        for (int i=0; i<advertisedDevice->getManufacturerDataCount(); i++) {
            std::string data = advertisedDevice->getManufacturerData(i);
            if (data.size() < 2) {
                printf("!! manufacturer data size too small (%d)\n", data.size());
                continue;
            }
            u16_t code = ((uint16_t)data[1] << 8) + (uint16_t)data[0];
            manufacturerIDs.push_back(code);
            manufacturerData.push_back(data);
        }

        state.handleBLEPacket(mac, name, rssi, manufacturerIDs, manufacturerData);
    }
};

// ============================================================================
// CHANNEL HOPPING
// ============================================================================

void hopChannel()
{
    unsigned long now = millis();
    if (now - last_channel_hop > CHANNEL_HOP_INTERVAL) {
        current_channel++;
        if (current_channel > MAX_CHANNEL) {
            current_channel = 1;
        }
        esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
        last_channel_hop = now;
        printf("[WiFi] Hopped to channel %d\n", current_channel);
    }
}

// ============================================================================
// MAIN FUNCTIONS
// ============================================================================

class FoxhuntCharactersticCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        printf("received foxhunt request, parsing...\n");
        NimBLEAttValue value = pCharacteristic->getValue();
        MsgPack::Unpacker unpacker;
        unpacker.feed(value.data(), value.length());
        const uint8_t mac[6] = {
            unpacker.unpackUInt8(),
            unpacker.unpackUInt8(),
            unpacker.unpackUInt8(),
            unpacker.unpackUInt8(),
            unpacker.unpackUInt8(),
            unpacker.unpackUInt8()
        };
        if (!unpacker.decoded()) {
            printf("failed! invalid MAC\n");
            return;
        }
        printf("now foxhunting...\n");
        state.foxhunt(mac);
    }
} foxhuntCallbacks;

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    // Initialize buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    bootBeepSequence();

    // Initialize WiFi in promiscuous mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&wifiSnifferPacketHandler);
    esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
    
    printf("WiFi promiscuous mode enabled on channel %d\n", current_channel);
    printf("Monitoring probe requests and beacons...\n");
    
    // Initialize BLE
    printf("Initializing BLE scanner...\n");
    NimBLEDevice::init(BLE_DEVICE_NAME);

    NimBLEDevice::setSecurityAuth(true, true, true);
    // NimBLEDevice::setSecurityPasskey(1312);
    pServer = NimBLEDevice::createServer();
    NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);
    NimBLECharacteristic *pScanResultCharacteristic = pService->createCharacteristic(
        SCAN_CHARACTERISTIC_UUID,
        READ | READ_ENC | NOTIFY
    );
    NimBLECharacteristic *pFoxhuntCharacteristic = pService->createCharacteristic(
        FOXHUNT_CHARACTERISTIC_UUID,
        WRITE | WRITE_ENC | NOTIFY
    );
    // WRITE_AUTHEN breaks read/write calbacks?
    pFoxhuntCharacteristic->setCallbacks(&foxhuntCallbacks);

    pService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->start();

    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(BLE_SCAN_INTERVAL_MS);
    pBLEScan->setWindow(BLE_SCAN_WINDOW_MS);
    
    printf("BLE scanner initialized\n");
    printf("System ready - hunting for Flock Safety devices...\n\n");
    
    last_channel_hop = millis();
}

void loop()
{
    // Handle channel hopping for WiFi promiscuous mode
    hopChannel();
    
    state.update();
    
    if (millis() - last_ble_scan >= BLE_SCAN_DELAY_MS && !pBLEScan->isScanning()) {
        printf("[BLE] scan...\n");
        pBLEScan->start(BLE_SCAN_DURATION_SECS, false);
        last_ble_scan = millis();
    }
    
    if (pBLEScan->isScanning() == false && millis() - last_ble_scan > BLE_SCAN_DURATION_SECS * 1000) {
        pBLEScan->clearResults();
    }
    
    delay(100);
}
