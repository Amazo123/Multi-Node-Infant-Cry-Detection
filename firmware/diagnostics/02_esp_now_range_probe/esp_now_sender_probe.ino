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

#define ONBOARD_LED 8

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

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming_data, int len) {
    const uint8_t *mac = recv_info->src_addr;
#else
void on_data_recv(const uint8_t *mac, const uint8_t *incoming_data, int len) {
#endif
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
    } else {
        Serial.printf("⚠️ [RECV UNEXPECTED] %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\n",
                      len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

void setup() {
    pinMode(ONBOARD_LED, OUTPUT);
    digitalWrite(ONBOARD_LED, HIGH); // Off

    Serial.begin(115200);
    
    // Wait up to 4 seconds for Serial Monitor while blinking LED
    uint32_t start = millis();
    while (!Serial && (millis() - start < 4000)) {
        digitalWrite(ONBOARD_LED, !digitalRead(ONBOARD_LED));
        delay(100);
    }
    digitalWrite(ONBOARD_LED, HIGH);

    delay(300);

    Serial.println("\n==============================================");
    Serial.println("  ESP-NOW RANGE & LINK PROBE (SENDER - C3)");
    Serial.println("==============================================");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Force Wi-Fi channel to Channel 1 using promiscuous toggle (required in STA mode without AP)
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    uint8_t primaryChan = 0;
    wifi_second_chan_t secondChan;
    esp_wifi_get_channel(&primaryChan, &secondChan);
    Serial.printf("📡 Node MAC Address: %s | Wi-Fi Channel: %d\n", 
                  WiFi.macAddress().c_str(), primaryChan);

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed!");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);

    // Register broadcast peer (channel 0 = use current interface channel)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcast_mac, 6);
    peerInfo.channel = 0;
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

    // Pulse onboard blue LED on transmit
    digitalWrite(ONBOARD_LED, LOW);
    esp_err_t send_err = esp_now_send(broadcast_mac, (uint8_t*)&ping, sizeof(ping));
    delay(20);
    digitalWrite(ONBOARD_LED, HIGH);

    if (send_err != ESP_OK) {
        Serial.printf("❌ esp_now_send failed: error code %d\n", send_err);
    }

    // Print active status to Serial every 1 second (every 5 pings)
    if (sequence_num % 5 == 0) {
        Serial.printf("📤 [PING SENT #%u] Total Sent: %u | Awaiting Hub... (Recv: %u)\n",
                      sequence_num, packets_sent, packets_received);
    }

    delay(180); // 5 Hz test rate (~200ms total)
}
