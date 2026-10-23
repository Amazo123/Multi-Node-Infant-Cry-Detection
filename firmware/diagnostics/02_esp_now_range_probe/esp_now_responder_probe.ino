/**
 * @file esp_now_responder_probe.ino
 * @brief ESP-NOW Range & Packet Loss Diagnostic Tool (Responder / Hub Echo).
 * 
 * Target: ESP32-S3 (or any ESP32)
 * Function:
 * - Listens for incoming probe_ping_t packets from sender nodes.
 * - Instantly echoes back a probe_pong_t packet with sender timestamp.
 * - Displays incoming packet counter and sender MAC address.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

typedef struct __attribute__((packed)) {
    uint32_t seq_id;
    uint32_t send_timestamp_ms;
    char     payload[64];
} probe_ping_t;

typedef struct __attribute__((packed)) {
    uint32_t seq_id;
    uint32_t echo_timestamp_ms;
} probe_pong_t;

uint32_t ping_count = 0;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming_data, int len) {
    const uint8_t *mac = recv_info->src_addr;
#else
void on_data_recv(const uint8_t *mac, const uint8_t *incoming_data, int len) {
#endif
    Serial.printf("📡 [PACKET ARRIVED] %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\n",
                  len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (len == sizeof(probe_ping_t)) {
        probe_ping_t ping;
        memcpy(&ping, incoming_data, sizeof(ping));
        ping_count++;

        // Echo back to the sender
        probe_pong_t pong;
        pong.seq_id = ping.seq_id;
        pong.echo_timestamp_ms = ping.send_timestamp_ms;

        // Ensure sender is registered as peer for direct unicast response
        if (!esp_now_is_peer_exist(mac)) {
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, mac, 6);
            peerInfo.channel = 0; // Send on current interface channel
            peerInfo.encrypt = false;
            esp_err_t add_err = esp_now_add_peer(&peerInfo);
            if (add_err != ESP_OK) {
                Serial.printf("⚠️ Failed to add peer for reply: %d\n", add_err);
            }
        }

        esp_err_t send_err = esp_now_send(mac, (uint8_t*)&pong, sizeof(pong));
        if (send_err == ESP_OK) {
            Serial.printf("   ✅ Echoed Pong #%u back to sender!\n", ping.seq_id);
        } else {
            Serial.printf("   ❌ Echo failed: %d\n", send_err);
        }
    } else {
        Serial.printf("⚠️ Length mismatch: got %d, expected %d\n", len, (int)sizeof(probe_ping_t));
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    delay(500);

    Serial.println("\n==============================================");
    Serial.println("  ESP-NOW RANGE & LINK PROBE (RESPONDER/HUB)");
    Serial.println("==============================================");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Lock Wi-Fi to Channel 1 using promiscuous toggle (required in STA mode without AP)
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    uint8_t primaryChan = 0;
    wifi_second_chan_t secondChan;
    esp_wifi_get_channel(&primaryChan, &secondChan);
    Serial.printf("📡 Hub Responder MAC: %s | Wi-Fi Channel: %d\n", 
                  WiFi.macAddress().c_str(), primaryChan);

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed!");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);
    Serial.printf("✅ Listening for probe pings on Channel %d...\n", primaryChan);
}

void loop() {
    static uint32_t last_heartbeat = 0;
    if (millis() - last_heartbeat > 3000) {
        last_heartbeat = millis();
        uint8_t chan = 0;
        wifi_second_chan_t sc;
        esp_wifi_get_channel(&chan, &sc);
        Serial.printf("⏳ [HUB IDLE] Listening on Ch:%d | Total Pings Echoed: %u\n", chan, ping_count);
    }
    delay(100);
}
