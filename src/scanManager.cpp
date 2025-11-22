#include "scanManager.h"

ScanCategory ScanCategory::defaultFlockScanCategory() {
    ScanCategory result("Flock");

    result.wifi_ssids = {
        "flock", "Flock", "FLOCK",
        "FS Ext Battery", // Flock Safety Extended Battery devices
        "Penguin", // Penguin surveillance devices
        "Pigvision", // Pigvision surveillance systems
    };

    result.ouis = {
        // FS Ext Battery devices
        {0x58, 0x8e, 0x81}, {0xcc, 0xcc, 0xcc}, {0xec, 0x1b, 0xbd}, {0x90, 0x35, 0xea},
        {0x04, 0x0d, 0x84}, {0xf0, 0x82, 0xc0}, {0x1c, 0x34, 0xf1}, {0x38, 0x5b, 0x44},
        {0x94, 0x34, 0x69}, {0xb4, 0xe3, 0xf9},

        // Flock WiFi devices
        {0x70, 0xc9, 0x4e}, {0x3c, 0x91, 0x80}, {0xd8, 0xf3, 0xbc}, {0x80, 0x30, 0x49},
        {0x14, 0x5a, 0xfc}, {0x74, 0x4c, 0xa1}, {0x08, 0x3a, 0x88}, {0x9c, 0x2f, 0x9d},
        {0x94, 0x08, 0x53}, {0xe4, 0xaa, 0xea},
    };

    result.ble_names = {
        "flock", "Flock", "FLOCK",
        "FS Ext Battery", // Flock Safety Extended Battery devices
        "Penguin", // Penguin surveillance devices
        "Pigvision", // Pigvision surveillance systems
    };

    result.ble_manufacturer_ids = {
        0x09C8, // XUNTONG
    };

    return result;
}

ScanCategory ScanCategory::debugCategory() {
    ScanCategory result("debug");
    result.wifi_ssids = {
        "cat girl cult",
    };
    return result;
}

bool ScanCategory::checkBLEManufacturerIDs(std::vector<u16_t> needles) {
    for (uint16_t haystack : ble_manufacturer_ids) {
        for (uint16_t needle : needles) {
            if (haystack == needle) {
                return true;
            }
        }
    }
    return false;
}

bool ScanCategory::checkMACPrefix(const uint8_t mac[6]) {
    std::array<uint8_t, 3> needle = { mac[0], mac[1], mac[2] };
    for (auto haystack : ouis) {
        if (haystack == needle) {
            return true;
        }
    }
    return false;
}

bool ScanCategory::checkSSIDPattern(std::string needle) {
    for (auto haystack : wifi_ssids) {
        if (haystack == needle) {
            return true;
        }
    }
    return false;
}

bool ScanCategory::checkBLEDeviceName(std::string needle) {
    for (auto haystack : ble_names) {
        if (haystack == needle) {
            return true;
        }
    }
    return false;
}

bool CategoryEdit::unpack(MsgPack::Unpacker unpacker) {
    MsgPack::str_t type = unpacker.unpackString();
    if (!unpacker.decoded()) {
        printf("failed to decode type\n");
        return false;
    }
    if (type.equals("ble_id")) {
        u16_t id = unpacker.unpackInt();
        if (!unpacker.decoded()) {
            printf("failed to decode id\n");
            return false;
        }
        printf("adding id %d\n", id);
        tag = Tag::BLEManufacturerID;
        ble_manufacturer_id = id;
    } else if (type.equals("ble_name")) {
        tag = Tag::BLEDeviceName;
        bleDeviceName = std::string(unpacker.unpackString().c_str());
        if (!unpacker.decoded()) {
            printf("failed to decode ble_name\n");
            return false;
        }
        printf("adding ble_name %s\n", bleDeviceName.c_str());
    } else if (type.equals("ssid")) {
        tag = Tag::SSID;
        ssid = std::string(unpacker.unpackString().c_str());
        if (!unpacker.decoded()) {
            printf("failed to decode ssid\n");
            return false;
        }
        printf("adding ssid %s\n", ssid.c_str());
    } else if (type.equals("oui")) {
        unpacker.unpackArraySize();
        oui = {
            (uint8_t)unpacker.unpackUInt(),
            (uint8_t)unpacker.unpackUInt(),
            (uint8_t)unpacker.unpackUInt(),
        };
        printf("adding oui: %d:%d:%d\n", oui[0], oui[1], oui[2]);
        tag = Tag::OUI;
    } else {
        printf("invalid type %s\n", type.c_str());
        return false;
    }
    return true;
}

void ScanManager::drop() {
    categories.clear();
}

void ScanManager::commit() {
    // TODO: serialize to ROM
}

void ScanManager::handleCategoryEdit(std::string categoryName, CategoryEdit edit) {
    ScanCategory *cat = NULL;
    for (auto &haystack : categories) {
        if (haystack.name == categoryName) {
            printf("editing ScanCategory %s\n", categoryName.c_str());
            cat = &haystack;
        }
    }
    if (cat == NULL) {
        printf("creating new ScanCategory %s\n", categoryName.c_str());
        categories.push_back(ScanCategory(categoryName));
        cat = &categories[categories.size() - 1];
    }
    if (edit.tag == CategoryEdit::Tag::BLEDeviceName) {
        cat->ble_names.insert(edit.bleDeviceName);
    } else if (edit.tag == CategoryEdit::Tag::BLEManufacturerID) {
        cat->ble_manufacturer_ids.insert(edit.ble_manufacturer_id);
    } else if (edit.tag == CategoryEdit::Tag::OUI) {
        cat->ouis.insert(edit.oui);
    } else if (edit.tag == CategoryEdit::Tag::SSID) {
        cat->wifi_ssids.insert(edit.ssid);
    }
}

bool ScanManager::checkBLEManufacturerIDs(const uint8_t mac[6], std::vector<u16_t> ids) {
    for (auto &cat : categories) {
        if (cat.checkBLEManufacturerIDs(ids)) {
            scanResult.categoryName = cat.name;
            scanResult.detectionType = "ble_id";
            return true;
        }
    }
    return false;
}

bool ScanManager::checkMACPrefix(const uint8_t mac[6]) {
    for (auto &cat : categories) {
        if (cat.checkMACPrefix(mac)) {
            scanResult.categoryName = cat.name;
            scanResult.detectionType = "mac_prefix";
            return true;
        }
    }
    return false;
}

bool ScanManager::checkSSIDPattern(const uint8_t mac[6], std::string ssid) {
    for (auto &cat : categories) {
        if (cat.checkSSIDPattern(ssid)) {
            scanResult.categoryName = cat.name;
            scanResult.detectionType = "ssid";
            return true;
        }
    }
    return false;
}

bool ScanManager::checkBLEDeviceName(const uint8_t mac[6], std::string name) {
    for (auto &cat : categories) {
        if (cat.checkBLEDeviceName(name)) {
            scanResult.categoryName = cat.name;
            scanResult.detectionType = "ble_name";
            return true;
        }
    }
    return false;
}
