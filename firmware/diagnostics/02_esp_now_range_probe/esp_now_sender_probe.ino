/**
 * @file esp_now_sender_probe.ino
 * @brief ESP-NOW Range & Packet Loss Diagnostic Tool (Sender / Cot Node).
 * 
 * Target: ESP32-C3
 * Function:
 * - Sends 5 packets per second to the Central Hub (broadcast or unicast).
 * - Measures Round-Trip Time (RTT) and Packet Delivery Rate (PDR).
 * - Prints live statistics to the Serial Monitor (115200 baud).
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

uint32_t sequence_num = 0;
uint32_t packets_sent = 0;
uint32_t packets_received = 0;
uint32_t total_rtt_ms = 0;

uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void on_data_recv(const uint8_t *mac, const uint8_t *incoming_data, int len) {
    if (len == sizeof(probe_pong_t)) {
        probe_pong_t pong;
        memcpy(&pong, incoming_data, sizeof(pong));
        
        uint32_t rtt = millis() - pong.echo_timestamp_ms;
        packets_received++;
        total_rtt_ms += rtt;

        float pdr = ((float)packets_received / (float)packets_sent) * 100.0f;
        float avg_rtt = (float)total_rtt_ms / (float)packets_received;

        Serial.printf("📥 [PONG RECV] Seq: %u | RTT: %u ms | PDR: %.1f%% (%u/%u) | Avg RTT: %.1f ms\n",
                      pong.seq_id, rtt, pdr, packets_received, packets_sent, avg_rtt);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    delay(500);

    Serial.println("\n==============================================");
    Serial.println("  ESP-NOW RANGE & LINK PROBE (SENDER)");
    Serial.println("==============================================");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    Serial.printf("📡 Node MAC Address: %s\n", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed!");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcast_mac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to add broadcast peer!");
    } else {
        Serial.println("✅ Broadcast Peer added. Starting range probe...");
    }
}

void loop() {
    sequence_num++;
    packets_sent++;

    probe_ping_t ping;
    ping.seq_id = sequence_num;
    ping.send_timestamp_ms = millis();
    snprintf(ping.payload, sizeof(ping.payload), "PROBE_PING_PACKET_%u", sequence_num);

    esp_now_send(broadcast_mac, (uint8_t*)&ping, sizeof(ping));

    delay(200); // 5 Hz test rate
}
