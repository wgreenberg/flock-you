#include "scanManager.h"
#include "state.h"
#include "beep.h"
#include "ble.h"
#include "wifiScan.h"

static ScanManager scanManager;
static State state(&scanManager);

void setup() {
    Serial.begin(115200);
    delay(1000);
    initBuzzer();
    bootBeepSequence();
    
    initWifi(&state);
    printf("WiFi promiscuous mode enabled");
    printf("Monitoring probe requests and beacons...\n");
    
    initBLE(&state);
    printf("BLE scanner initialized\n");
    printf("System ready - the hunt begins\n\n");
}

void loop() {
    state.update();
    hopChannel();
    scanBLE();
    delay(100);
}
