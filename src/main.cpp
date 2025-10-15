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

// ============================================================================
// CONFIGURATION
// ============================================================================

// Hardware Configuration
#define BUZZER_PIN 3  // GPIO3 (D2) - PWM capable pin on Xiao ESP32 S3

// Audio Configuration
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
#define BLE_SCAN_DURATION 1    // Seconds
#define BLE_SCAN_INTERVAL 5000 // Milliseconds between scans
static unsigned long last_ble_scan = 0;

// Debug
#define DEBUG_BLE 0
static unsigned long last_fake_ble = 0;
#define DEBUG_WIFI 0
static unsigned long last_fake_wifi = 0;

// Detection Pattern Limits
#define MAX_SSID_PATTERNS 10
#define MAX_MAC_PATTERNS 50
#define MAX_DEVICE_NAMES 20

// ============================================================================
// DETECTION PATTERNS (Extracted from Real Flock Safety Device Databases)
// ============================================================================

// WiFi SSID patterns to detect (case-insensitive)
static const char* wifi_ssid_patterns[] = {
    "flock",        // Standard Flock Safety naming
    "Flock",        // Capitalized variant
    "FLOCK",        // All caps variant
    "FS Ext Battery", // Flock Safety Extended Battery devices
    "Penguin",      // Penguin surveillance devices
    "Pigvision"     // Pigvision surveillance systems
};

// Known Flock Safety MAC address prefixes (from real device databases)
static const char* mac_prefixes[] = {
    // FS Ext Battery devices
    "58:8e:81", "cc:cc:cc", "ec:1b:bd", "90:35:ea", "04:0d:84", 
    "f0:82:c0", "1c:34:f1", "38:5b:44", "94:34:69", "b4:e3:f9",
    
    // Flock WiFi devices
    "70:c9:4e", "3c:91:80", "d8:f3:bc", "80:30:49", "14:5a:fc",
    "74:4c:a1", "08:3a:88", "9c:2f:9d", "94:08:53", "e4:aa:ea"
    
    // Penguin devices - these are NOT OUI based, so use local ouis
    // from the wigle.net db relative to your location 
    // "cc:09:24", "ed:c7:63", "e8:ce:56", "ea:0c:ea", "d8:8f:14",
    // "f9:d9:c0", "f1:32:f9", "f6:a0:76", "e4:1c:9e", "e7:f2:43",
    // "e2:71:33", "da:91:a9", "e1:0e:15", "c8:ae:87", "f4:ed:b2",
    // "d8:bf:b5", "ee:8f:3c", "d7:2b:21", "ea:5a:98"
};

// Device name patterns for BLE advertisement detection
static const char* device_name_patterns[] = {
    "FS Ext Battery",  // Flock Safety Extended Battery
    "Penguin",         // Penguin surveillance devices
    "Flock",           // Standard Flock Safety devices
    "Pigvision"        // Pigvision surveillance systems
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

static uint8_t current_channel = 1;
static unsigned long last_channel_hop = 0;
static bool triggered = false;
static bool device_in_range = false;
static unsigned long last_detection_time = 0;
static unsigned long last_heartbeat = 0;
static NimBLEScan* pBLEScan;
static NimBLEServer* pServer;
static const char* BLE_DEVICE_NAME = "FlockYou";
static const NimBLEUUID BLE_SERVICE_UUID = NimBLEUUID(0xACAB0001);
static const char* BLE_CHARACTERISTIC_UUID = "0001";


// ============================================================================
// AUDIO SYSTEM
// ============================================================================

void beep(int frequency, int duration_ms)
{
    tone(BUZZER_PIN, frequency, duration_ms);
    delay(duration_ms + 50);
}

void boot_beep_sequence()
{
    printf("Initializing audio system...\n");
    printf("Playing boot sequence: Low -> High pitch\n");
    beep(LOW_FREQ, BOOT_BEEP_DURATION);
    beep(HIGH_FREQ, BOOT_BEEP_DURATION);
    printf("Audio system ready\n\n");
}

void flock_detected_beep_sequence()
{
    printf("FLOCK SAFETY DEVICE DETECTED!\n");
    printf("Playing alert sequence: 3 fast high-pitch beeps\n");
    for (int i = 0; i < 3; i++) {
        beep(DETECT_FREQ, DETECT_BEEP_DURATION);
        if (i < 2) delay(50); // Short gap between beeps
    }
    printf("Detection complete - device identified!\n\n");
    
    // Mark device as in range and start heartbeat tracking
    device_in_range = true;
    last_detection_time = millis();
    last_heartbeat = millis();
}

void notify(MsgPack::Packer packer) {
    NimBLEService* pSvc = pServer->getServiceByUUID(BLE_SERVICE_UUID);
    if (!pSvc) {
        printf("fuck\n");
        return;
    }
    NimBLECharacteristic* pChr = pSvc->getCharacteristic(BLE_CHARACTERISTIC_UUID);
    if (!pChr) {
        printf("fuck!\n");
        return;
    }
    if (packer.size() > 256) {
        printf("failed to notify, data too large!");
        MsgPack::Packer tooLargePacker;
        tooLargePacker.to_array("data_too_large");
        pChr->notify(tooLargePacker.data());
        return;
    }

    // MsgPack::Unpacker unpacker;
    // StaticJsonDocument<256> doc;
    // String jsonOutput;
    // unpacker.feed(packer.data(), packer.size());
    // unpacker.deserialize(doc);
    // serializeJson(doc, jsonOutput);
    printf("notifying (%d bytes)...\n", packer.size());
    // Serial.println(jsonOutput);

    pChr->setValue(packer.data(), packer.size());
    pChr->notify();
}

void heartbeat_pulse()
{
    printf("Heartbeat: Device still in range\n");
    beep(HEARTBEAT_FREQ, HEARTBEAT_DURATION);
    delay(100);
    beep(HEARTBEAT_FREQ, HEARTBEAT_DURATION);
}

// ============================================================================
// JSON OUTPUT FUNCTIONS
// ============================================================================

void output_wifi_detection_json(const char* ssid, const uint8_t mac[6], int rssi, int frame_type)
{
    char mac_prefix[9];
    snprintf(mac_prefix, sizeof(mac_prefix), "%02x:%02x:%02x", mac[0], mac[1], mac[2]);
    
    // Detection pattern matching
    String matched_ssid;
    for (int i = 0; i < sizeof(wifi_ssid_patterns)/sizeof(wifi_ssid_patterns[0]); i++) {
        if (strcasestr(ssid, wifi_ssid_patterns[i])) {
            matched_ssid += wifi_ssid_patterns[i];
            break;
        }
    }
    
    String matched_mac;
    for (int i = 0; i < sizeof(mac_prefixes)/sizeof(mac_prefixes[0]); i++) {
        if (strncasecmp(mac_prefix, mac_prefixes[i], 8) == 0) {
            matched_mac += mac_prefixes[i];
            break;
        }
    }

    std::array<unsigned int, 6> packed_mac { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] };

    MsgPack::Packer packer;
    packer.to_array(
        "wifi",
        rssi,
        ssid,
        matched_ssid,
        current_channel,
        packed_mac,
        matched_mac,
        frame_type
    );

    notify(packer);
}

void output_out_of_range_json()
{
    MsgPack::Packer packer;
    packer.packString("out_of_range");
    notify(packer);

}

void output_ble_detection_json(const uint8_t mac[6], const char* name, int rssi, const char* detection_method)
{
    // MAC address analysis
    char mac_prefix[9];
    snprintf(mac_prefix, sizeof(mac_prefix), "%02x:%02x:%02x", mac[0], mac[1], mac[2]);
    
    // Check MAC prefix patterns
    String matched_mac;
    for (int i = 0; i < sizeof(mac_prefixes)/sizeof(mac_prefixes[0]); i++) {
        if (strncasecmp(mac_prefix, mac_prefixes[i], 8) == 0) {
            matched_mac += mac_prefixes[i];
            break;
        }
    }

    // Check device name patterns
    String matched_name;
    if (name && strlen(name) > 0) {
        for (int i = 0; i < sizeof(device_name_patterns)/sizeof(device_name_patterns[0]); i++) {
            if (strcasestr(name, device_name_patterns[i])) {
                matched_name += device_name_patterns[i];
                break;
            }
        }
    }

    std::array<unsigned int, 6> packed_mac { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] };

    MsgPack::Packer packer;
    packer.to_array(
        "bluetooth_le",
        name,
        matched_name,
        rssi,
        packed_mac,
        matched_mac
    );
    
    notify(packer);
}

// ============================================================================
// DETECTION HELPER FUNCTIONS
// ============================================================================

bool check_mac_prefix(const uint8_t* mac)
{
    char mac_str[9];  // Only need first 3 octets for prefix check
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x", mac[0], mac[1], mac[2]);
    
    for (int i = 0; i < sizeof(mac_prefixes)/sizeof(mac_prefixes[0]); i++) {
        if (strncasecmp(mac_str, mac_prefixes[i], 8) == 0) {
            return true;
        }
    }
    return false;
}

bool check_ssid_pattern(const char* ssid)
{
    if (!ssid) return false;
    
    for (int i = 0; i < sizeof(wifi_ssid_patterns)/sizeof(wifi_ssid_patterns[0]); i++) {
        if (strcasestr(ssid, wifi_ssid_patterns[i])) {
            return true;
        }
    }
    return false;
}

bool check_device_name_pattern(const char* name)
{
    if (!name) return false;
    
    for (int i = 0; i < sizeof(device_name_patterns)/sizeof(device_name_patterns[0]); i++) {
        if (strcasestr(name, device_name_patterns[i])) {
            return true;
        }
    }
    return false;
}

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
    uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{
    
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    
    // Check for probe requests (subtype 0x04) and beacons (subtype 0x08)
    uint8_t frame_type = (hdr->frame_ctrl & 0xFF) >> 2;
    if (frame_type != 0x20 && frame_type != 0x80) { // Probe request or beacon
        return;
    }
    
    // Extract SSID from probe request or beacon
    char ssid[33] = {0};
    uint8_t *payload = (uint8_t *)ipkt + 24; // Skip MAC header
    
    if (frame_type == 0x20) { // Probe request
        payload += 0; // Probe requests start with SSID immediately
    } else { // Beacon frame
        payload += 12; // Skip fixed parameters in beacon
    }
    
    // Parse SSID element (tag 0, length, data)
    if (payload[0] == 0 && payload[1] <= 32) {
        memcpy(ssid, &payload[2], payload[1]);
        ssid[payload[1]] = '\0';
    }

    if (DEBUG_WIFI && millis() - last_fake_wifi >= 1000) {
        output_wifi_detection_json(ssid, hdr->addr2, ppkt->rx_ctrl.rssi, frame_type);
        last_fake_wifi = millis();
    }
    
    // Check if SSID matches our patterns
    if (strlen(ssid) > 0 && check_ssid_pattern(ssid)) {
        output_wifi_detection_json(ssid, hdr->addr2, ppkt->rx_ctrl.rssi, frame_type);
        
        if (!triggered) {
            triggered = true;
            flock_detected_beep_sequence();
        }
        // Always update detection time for heartbeat tracking
        last_detection_time = millis();
        return;
    }
    
    // Check MAC address
    if (check_mac_prefix(hdr->addr2)) {
        output_wifi_detection_json(ssid, hdr->addr2, ppkt->rx_ctrl.rssi, frame_type);
        
        if (!triggered) {
            triggered = true;
            flock_detected_beep_sequence();
        }
        // Always update detection time for heartbeat tracking
        last_detection_time = millis();
        return;
    }
}

// ============================================================================
// BLE SCANNING
// ============================================================================

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        NimBLEAddress addr = advertisedDevice->getAddress();
        std::string addrStr = addr.toString();
        uint8_t mac[6];
        sscanf(addrStr.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", 
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
        
        int rssi = advertisedDevice->getRSSI();
        std::string name = "";
        if (advertisedDevice->haveName()) {
            name = advertisedDevice->getName();
        }
        
        // Check MAC prefix
        if (check_mac_prefix(mac)) {
            output_ble_detection_json(mac, name.c_str(), rssi, "mac_prefix");
            if (!triggered) {
                triggered = true;
                flock_detected_beep_sequence();
            }
            // Always update detection time for heartbeat tracking
            last_detection_time = millis();
            return;
        }

        if (DEBUG_BLE && millis() - last_fake_ble >= 1000) {
            output_ble_detection_json(mac, name.c_str(), rssi, "device_name");
            last_fake_ble = millis();
        }
        
        // Check device name
        if (!name.empty() && check_device_name_pattern(name.c_str())) {
            output_ble_detection_json(mac, name.c_str(), rssi, "device_name");
            if (!triggered) {
                triggered = true;
                flock_detected_beep_sequence();
            }
            // Always update detection time for heartbeat tracking
            last_detection_time = millis();
            return;
        }
    }
};

// ============================================================================
// CHANNEL HOPPING
// ============================================================================

void hop_channel()
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

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    // Initialize buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    // boot_beep_sequence();
    
    printf("Starting Flock Squawk Enhanced Detection System...\n\n");
    
    // Initialize WiFi in promiscuous mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
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
        BLE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE
    );

    pService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->start();

    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(10);
    
    printf("BLE scanner initialized\n");
    printf("System ready - hunting for Flock Safety devices...\n\n");
    
    last_channel_hop = millis();
}

void loop()
{
    // Handle channel hopping for WiFi promiscuous mode
    hop_channel();
    
    // Handle heartbeat pulse if device is in range
    if (device_in_range) {
        unsigned long now = millis();
        
        // Check if 10 seconds have passed since last heartbeat
        if (now - last_heartbeat >= 10000) {
            heartbeat_pulse();
            last_heartbeat = now;
        }
        
        // Check if device has gone out of range (no detection for 30 seconds)
        if (now - last_detection_time >= 30000) {
            printf("Device out of range - stopping heartbeat\n");
            device_in_range = false;
            triggered = false; // Allow new detections
        }
    }
    
    if (millis() - last_ble_scan >= BLE_SCAN_INTERVAL && !pBLEScan->isScanning()) {
        printf("[BLE] scan...\n");
        pBLEScan->start(BLE_SCAN_DURATION, false);
        last_ble_scan = millis();
    }
    
    if (pBLEScan->isScanning() == false && millis() - last_ble_scan > BLE_SCAN_DURATION * 1000) {
        pBLEScan->clearResults();
    }
    
    delay(100);
}
