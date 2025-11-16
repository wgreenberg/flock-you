#include "beep.h"
#include <Arduino.h>
#include <vector>

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

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