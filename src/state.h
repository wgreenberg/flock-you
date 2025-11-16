#include <Arduino.h>
#include <string.h>
#include <ctype.h>
#include <vector>

// Forward decls
class ScanManager;
class ScanResult;

struct FoxhunterState {
    uint8_t target_mac[6];
    int rssi;
};

class State {
    public:
        bool muted = false;
        enum ModeType { Detector, Foxhunter } mode;
        ScanManager *scanManager;
        State(ScanManager *m) : scanManager(m) {
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
        void foxhunt(const uint8_t mac[6], bool doNotify);
        void detect();
    private:
        bool deviceInRange;
        unsigned long lastDetectionTime;
        unsigned long lastHeartbeat;
        bool foxhuntAutoOff = false;
        FoxhunterState foxhunterState;
        void handleFoxhuntQuery(const uint8_t mac[6], int rssi);
        void handleScanDetection(const uint8_t mac[6], ScanResult *result);
        void resetDetection();
};