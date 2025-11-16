#include <vector>

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

const int B3hz = 247;
const int C4hz = 262;
const int D4hz = 294;
const int E4hz = 330;
const int F4hz = 349;
const int G4hz = 392;
const int A4hz = 440;
const int B4hz = 494;

void initBuzzer();
void beep(int frequency, int duration_ms);
void bootBeepSequence();
void detectionBeepSequence();
bool checkRange(int val, int low, int high);
void multibeep(std::vector<int> freqs, int total_duration);
void proximityBeep(int rssi);
void outOfRangeBeep();
void heartbeatPulse();