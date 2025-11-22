#include "ble.h"
#include "wifiScan.h"
#include "state.h"
#include "scanManager.h"

void notify(MsgPack::Packer packer, const char* characteristicUUID) {
    NimBLEService* pSvc = pServer->getServiceByUUID(BLE_SERVICE_UUID);
    if (!pSvc) {
        printf("no service found\n");
        return;
    }
    NimBLECharacteristic* pChr = pSvc->getCharacteristic(characteristicUUID);
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
    notify(packer, SCAN_CHARACTERISTIC_UUID);
}

void sendWiFiDeviceInfo(
    std::string ssid,
    uint8_t current_channel,
    const uint8_t mac[6],
    int rssi,
    int frameType
) {
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

    notify(packer, SCAN_CHARACTERISTIC_UUID);
}

void sendBLEDeviceInfo(
    const uint8_t mac[6],
    const char* name,
    int rssi,
    std::vector<u16_t> manufacturerCodes,
    std::vector<std::string> manufacturerData
) {
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
    
    notify(packer, SCAN_CHARACTERISTIC_UUID);
}

void initBLE(State *state) {
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
        // WRITE_AUTHEN breaks read/write calbacks?
        READ | WRITE | WRITE_ENC | NOTIFY
    );
    pFoxhuntCharacteristic->setCallbacks(new FoxhuntCharactersticCallbacks(state));
    NimBLECharacteristic *pScanConfigCharacteristic = pService->createCharacteristic(
        SCAN_CONFIG_CHARACTERISTIC_UUID,
        READ | WRITE | WRITE_ENC | NOTIFY
    );
    pScanConfigCharacteristic->setCallbacks(new ScanConfigCharactersticCallbacks(state));

    pService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->start();

    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(state));
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(BLE_SCAN_INTERVAL_MS);
    pBLEScan->setWindow(BLE_SCAN_WINDOW_MS);
}

void scanBLE() {
    if (millis() - last_ble_scan >= BLE_SCAN_DELAY_MS && !pBLEScan->isScanning()) {
        printf("[BLE] scan...\n");
        pBLEScan->start(BLE_SCAN_DURATION_SECS, false);
        last_ble_scan = millis();
    }
    
    if (pBLEScan->isScanning() == false && millis() - last_ble_scan > BLE_SCAN_DURATION_SECS * 1000) {
        pBLEScan->clearResults();
    }
}

void AdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
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

    mState->handleBLEPacket(mac, name, rssi, manufacturerIDs, manufacturerData);
}

void FoxhuntCharactersticCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    NimBLEAttValue value = pCharacteristic->getValue();
    if (value.length() == 0) {
        printf("received empty foxhunt request, going to detection mode...\n");
        mState->detect();
        return;
    }
    printf("received foxhunt request, parsing...\n");
    MsgPack::Unpacker unpacker;
    unpacker.feed(value.data(), value.length());
    if (unpacker.isArray()) {
        unpacker.unpackArraySize();
        const uint8_t mac[6] = {
            (uint8_t)unpacker.unpackUInt(),
            (uint8_t)unpacker.unpackUInt(),
            (uint8_t)unpacker.unpackUInt(),
            (uint8_t)unpacker.unpackUInt(),
            (uint8_t)unpacker.unpackUInt(),
            (uint8_t)unpacker.unpackUInt(),
        };
        printf(
            "now foxhunting %02X:%02X:%02X:%02X:%02X:%02X...\n",
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]
        );
        mState->foxhunt(mac, false);
        return;
    }
    printf("failed! invalid MAC\n");
}

void ScanConfigCharactersticCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    NimBLEAttValue value = pCharacteristic->getValue();
    printf("received scan config request, parsing %d bytes...\n", value.length());
    MsgPack::Unpacker unpacker;
    unpacker.feed(value.data(), value.length());

    if (!unpacker.isArray()) {
        MsgPack::str_t type = unpacker.unpackString();
        if (type.equals("drop")) {
            printf("dropping\n");
            mState->scanManager->drop();
        } else if (type.equals("commit")) {
            printf("committing\n");
            mState->scanManager->commit();
        } else {
            printf("invalid config type \"%s\"\n", type.c_str());
            return;
        }
    } else {
        unpacker.unpackArraySize();
        MsgPack::str_t type = unpacker.unpackString();
        if (type.equals("edit")) {
            MsgPack::str_t categoryName = unpacker.unpackString();
            if (!unpacker.decoded()) {
                printf("failed to decode category name\n");
                return;
            }
            CategoryEdit edit;
            if (!edit.unpack(unpacker)) {
                printf("failed to decode edit\n");
                return;
            }
            mState->scanManager->handleCategoryEdit(categoryName.c_str(), edit);
        } else {
            printf("invalid config type \"%s\"\n", type.c_str());
            return;
        }
    }
}