#include <WiFi.h>
#include "wifiScan.h"
#include "state.h"

void hopChannel() {
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

State *mState;

void wifiSnifferCallback(void* buff, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) {
        return;
    }

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    uint8_t *payload = (uint8_t *)ipkt + 24;
    
    // Check for probe requests (subtype 0x04) and beacons (subtype 0x08)
    uint8_t frameType = ppkt->payload[0];
    if (frameType != 0x80 && frameType != 0x40) {
        return;
    }
    
    // Extract SSID from probe request or beacon
    std::string ssid = "";
    int ssid_start = frameType == 0x80 ? 13 : 1;
    uint8_t ssid_len = payload[ssid_start];
    if (ssid_len > 33) {
        return;
    }
    for (int i=0; i < ssid_len; i++) {
        ssid += (char)payload[ssid_start + i + 1];
    }

    mState->handleWifiPacket(ssid, hdr->addr2, ppkt->rx_ctrl.rssi, frameType);
}

void initWifi(State *state) {
    // Initialize WiFi in promiscuous mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    mState = state;
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&wifiSnifferCallback);
    esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
    last_channel_hop = millis();
}