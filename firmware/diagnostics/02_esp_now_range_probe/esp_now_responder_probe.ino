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

void on_data_recv(const uint8_t *mac, const uint8_t *incoming_data, int len) {
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
            peerInfo.channel = 1;
            peerInfo.encrypt = false;
            esp_now_add_peer(&peerInfo);
        }

        esp_now_send(mac, (uint8_t*)&pong, sizeof(pong));

        Serial.printf("📡 [PING RECEIVED #%u] Seq: %u from %02X:%02X:%02X:%02X:%02X:%02X -> Echoed Pong!\n",
                      ping_count, ping.seq_id,
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    delay(500);

    Serial.println("\n==============================================");
    Serial.println("  ESP-NOW RANGE & LINK PROBE (RESPONDER/HUB)");
    Serial.println("==============================================");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    Serial.printf("📡 Hub Responder MAC: %s\n", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed!");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);
    Serial.println("✅ Listening for probe pings on Channel 1...");
}

void loop() {
    delay(1000);
}
