class ScanCategory {
    public:
        std::string name;
        std::set<std::string> wifi_ssids;
        std::set<std::array<uint8_t, 3>> ouis;
        std::set<std::string> ble_names;
        std::set<u16_t> ble_manufacturer_ids;
        bool checkBLEManufacturerIDs(std::vector<u16_t> needles) {
            for (uint16_t haystack : ble_manufacturer_ids) {
                for (uint16_t needle : needles) {
                    if (haystack == needle) {
                        return true;
                    }
                }
            }
            return false;
        }

        bool checkMACPrefix(const uint8_t mac[6]) {
            std::array<uint8_t, 3> needle = { mac[0], mac[1], mac[2] };
            for (auto haystack : ouis) {
                if (haystack == needle) {
                    return true;
                }
            }
            return false;
        }

        bool checkSSIDPattern(std::string needle) {
            for (auto haystack : wifi_ssids) {
                if (haystack == needle) {
                    return true;
                }
            }
            return false;
        }

        bool checkBLEDeviceName(std::string needle) {
            for (auto haystack : ble_names) {
                if (haystack == needle) {
                    return true;
                }
            }
            return false;
        }
};

struct ScanResult {
    std::string categoryName;
    std::string detectionType;
};

struct CategoryEdit {
    enum Tag { BLEManufacturerID, SSID, BLEDeviceName, OUI } tag;
    union data {
        u16_t ble_manufacturer_id;
        std::string ssid;
        std::string bleDeviceName;
        std::array<uint8_t, 3> oui;
    } data;
};

ScanCategory defaultFlockScanCategory() {
    ScanCategory result;
    result.name = "Flock";

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

ScanCategory debugCategory() {
    ScanCategory result;
    result.name = "debug";
    result.wifi_ssids = {
        "cat girl cult",
    };
    return result;
}

class ScanManager {
    public:
        std::vector<ScanCategory> categories;
        ScanResult scanResult;
        ScanManager() {
            categories.push_back(defaultFlockScanCategory());
            // categories.push_back(debugCategory());
            scanResult = { std::string(), std::string() };
        }

        void handleCategoryEdit(std::string categoryName, CategoryEdit edit) {
            ScanCategory *cat = NULL;
            for (auto haystack : categories) {
                if (haystack.name == categoryName) {
                    printf("editing ScanCategory %s\n", categoryName.c_str());
                    cat = &haystack;
                }
            }
            if (cat == NULL) {
                printf("creating new ScanCategory %s\n", categoryName.c_str());
                ScanCategory newCat;
                newCat.name = categoryName;
                categories.push_back(newCat);
                cat = &newCat;
            }
            if (edit.tag == CategoryEdit::Tag::BLEDeviceName) {
                cat->ble_names.insert(edit.data.bleDeviceName);
            } else if (edit.tag == CategoryEdit::Tag::BLEManufacturerID) {
                cat->ble_manufacturer_ids.insert(edit.data.ble_manufacturer_id);
            } else if (edit.tag == CategoryEdit::Tag::OUI) {
                cat->ouis.insert(edit.data.oui);
            } else {
                cat->wifi_ssids.insert(edit.data.ssid);
            }
        }

        bool checkBLEManufacturerIDs(const uint8_t mac[6], std::vector<u16_t> ids) {
            for (auto &cat : categories) {
                if (cat.checkBLEManufacturerIDs(ids)) {
                    scanResult.categoryName = cat.name;
                    scanResult.detectionType = "ble_id";
                    return true;
                }
            }
            return false;
        }

        bool checkMACPrefix(const uint8_t mac[6]) {
            for (auto &cat : categories) {
                if (cat.checkMACPrefix(mac)) {
                    scanResult.categoryName = cat.name;
                    scanResult.detectionType = "mac_prefix";
                    return true;
                }
            }
            return false;
        }

        bool checkSSIDPattern(const uint8_t mac[6], std::string ssid) {
            for (auto cat : categories) {
                if (cat.checkSSIDPattern(ssid)) {
                    scanResult.categoryName = cat.name;
                    scanResult.detectionType = "ssid";
                    return true;
                }
            }
            return false;
        }

        bool checkBLEDeviceName(const uint8_t mac[6], std::string name) {
            for (auto cat : categories) {
                if (cat.checkBLEDeviceName(name)) {
                    scanResult.categoryName = cat.name;
                    scanResult.detectionType = "ble_name";
                    return true;
                }
            }
            return false;
        }
};