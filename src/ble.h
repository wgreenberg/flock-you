#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <MsgPack.h>

// forward decl
class State;

// BLE SCANNING CONFIGURATION
#define BLE_SCAN_DURATION_SECS 1
#define BLE_SCAN_INTERVAL_MS 200
#define BLE_SCAN_WINDOW_MS 100
#define BLE_SCAN_DELAY_MS 1500

// BLE state
static NimBLEScan* pBLEScan;
static NimBLEServer* pServer;
static const char* BLE_DEVICE_NAME = "FlockYou";
static const NimBLEUUID BLE_SERVICE_UUID = NimBLEUUID(0xACAB0001);
static const char* SCAN_CHARACTERISTIC_UUID = "0001";
static const char* FOXHUNT_CHARACTERISTIC_UUID = "0002";
static const char* SCAN_CONFIG_CHARACTERISTIC_UUID = "0003";

static unsigned long last_ble_scan = 0;

void notify(MsgPack::Packer packer, const char* characteristicUUID);
void sendDetectionEvent(const uint8_t mac[6], std::string detectionType, std::string categoryName);
void sendWiFiDeviceInfo(
    std::string ssid,
    uint8_t current_channel,
    const uint8_t mac[6],
    int rssi,
    int frameType
);
void sendBLEDeviceInfo(
    const uint8_t mac[6],
    const char* name,
    int rssi,
    std::vector<u16_t> manufacturerCodes,
    std::vector<std::string> manufacturerData
);

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    public:
        AdvertisedDeviceCallbacks(State *state) : mState(state) {
        }
    private:
        State *mState;

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
};

class FoxhuntCharactersticCallbacks: public NimBLECharacteristicCallbacks {
    public:
        FoxhuntCharactersticCallbacks(State *state) : mState(state) {
        }
    private:
        State *mState;

    void onWrite(NimBLECharacteristic* pCharacteristic) override;
};

class ScanConfigCharactersticCallbacks: public NimBLECharacteristicCallbacks {
    public:
        ScanConfigCharactersticCallbacks(State *state) : mState(state) {
        }
    private:
        State *mState;

    void onWrite(NimBLECharacteristic* pCharacteristic) override;
};

void initBLE(State *state);
void scanBLE();