#include <string>
#include <vector>
#include <MsgPack.h>

class ScanCategory {
    public:
        std::string name;
        std::set<std::string> wifi_ssids;
        std::set<std::array<uint8_t, 3>> ouis;
        std::set<std::string> ble_names;
        std::set<u16_t> ble_manufacturer_ids;
        ScanCategory(std::string n) : name(n) {}
        static ScanCategory defaultFlockScanCategory();
        static ScanCategory debugCategory();
        bool checkBLEManufacturerIDs(std::vector<u16_t> needles);
        bool checkMACPrefix(const uint8_t mac[6]);
        bool checkSSIDPattern(std::string needle);
        bool checkBLEDeviceName(std::string needle);
};

struct ScanResult {
    std::string categoryName;
    std::string detectionType;
};

class CategoryEdit {
    public:
        enum Tag { BLEManufacturerID, SSID, BLEDeviceName, OUI } tag;
        u16_t ble_manufacturer_id;
        std::string ssid;
        std::string bleDeviceName;
        std::array<uint8_t, 3> oui;

        bool unpack(MsgPack::Unpacker unpacker);
};

class ScanManager {
    public:
        std::vector<ScanCategory> categories;
        ScanResult scanResult;
        ScanManager() {
            // TODO: read from ROM, then fallback to defaults
            categories.push_back(ScanCategory::defaultFlockScanCategory());
            // categories.push_back(ScanCategory::debugCategory());
            scanResult = { std::string(), std::string() };
        }

        void drop();
        void commit();
        void handleCategoryEdit(std::string categoryName, CategoryEdit edit);
        bool checkBLEManufacturerIDs(const uint8_t mac[6], std::vector<u16_t> ids);
        bool checkMACPrefix(const uint8_t mac[6]);
        bool checkSSIDPattern(const uint8_t mac[6], std::string ssid);
        bool checkBLEDeviceName(const uint8_t mac[6], std::string name);
};