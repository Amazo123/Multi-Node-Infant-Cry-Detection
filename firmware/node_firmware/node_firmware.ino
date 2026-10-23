/**
 * @file node_firmware.ino
 * @brief Edge Sensing Node Firmware (Designed specifically for ESP32-C3-DevKitC-02).
 * 
 * LOCKED HARDWARE CONFIGURATION:
 * - MCU: ESP32-C3 (Single-Core RISC-V @ 160MHz)
 * - Microphone (INMP441):
 *     GPIO5 -> SCK (BCLK)
 *     GPIO6 -> WS  (LRCK)
 *     GPIO7 -> SD  (DIN)
 * - Battery Monitoring:
 *     GPIO1 -> ADC (Voltage divider: 100k + 100k, VADC = VBAT / 2, 100nF filter)
 * - Two-LED Status System:
 *     GPIO4 -> RED LED (Active-HIGH: Battery Status)
 *              High: Solid ON | Medium: Slow Blink | Low: Fast Blink
 *     GPIO9 -> BLUE LED (Active-LOW: 3.3V -> 330R -> LED -> GPIO9)
 *              Connected: Solid ON (LOW) | Disconnected: Blinking
 * 
 * Functional Pipeline:
 * 1. Streams DMA PCM audio at 16,000 Hz, 16-bit mono into a 64 KB rolling ring buffer.
 * 2. Computes 100ms window RMS energy.
 * 3. Reads battery ADC on GPIO1 (calibrated threshold-based estimation).
 * 4. Broadcasts 14-byte telemetry packets over ESP-NOW to ESP32-S3 hub.
 * 5. On MSG_TYPE_AUDIO_REQ from S3 hub, freezes buffer and bursts 320 packets (200 bytes each).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include "../common/protocol.h"
#include "pins.h"

// ---------------- Node Configuration ----------------
#define CURRENT_NODE_ID       1       // 1, 2, or 3 for each physical cot
#define TELEMETRY_INTERVAL_MS 100     // Milliseconds between RMS telemetry updates
#define S3_HEARTBEAT_TIMEOUT  5000    // If no hub contact for >5s, mark disconnected
#define BURST_PACING_US       2500    // Microseconds between ESP-NOW fragment transmissions (2.5ms allows clean RF queue drain)
#define BURST_MAX_RETRIES     3       // Max retries per fragment on send failure
#define BURST_COOLDOWN_MS     3000    // Minimum time between audio bursts
#define WDT_TIMEOUT_SEC       15      // Hardware watchdog timeout

// ---------------- Global Buffers ----------------
// 2.0-second circular audio buffer: 32,000 samples = 64,000 bytes
int16_t ring_buffer[AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC];
volatile size_t ring_write_idx = 0;
bool buffer_ready = false;

// Transmit buffer to freeze 64 KB without interrupting DMA acquisition
int16_t tx_buffer[AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC];

// Tracking Variables
uint16_t telemetry_seq = 0;
uint32_t last_telemetry_time = 0;
uint32_t last_s3_contact_time = 0;
uint32_t last_burst_time = 0;
volatile bool burst_in_progress = false;
bool is_connected_to_hub = false;
volatile bool pending_audio_burst = false;
volatile uint16_t pending_session_id = 0;
volatile bool espnow_send_done = true;   // Flow-control flag for burst TX callback

// Target Address for S3 Hub (Hardware MAC: 30:30:F9:17:2F:80; updated dynamically on contact)
uint8_t hub_mac_address[] = {0x30, 0x30, 0xF9, 0x17, 0x2F, 0x80};

// ---------------- Forward Declarations ----------------
bool init_i2s();
void capture_audio();
float compute_100ms_rms();
uint16_t read_battery_millivolts();
void send_telemetry();
void send_audio_burst(uint16_t session_id);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len);
void on_send_complete(const wifi_tx_info_t *info, esp_now_send_status_t status);
#else
void on_data_received(const uint8_t *mac, const uint8_t *incomingData, int len);
void on_send_complete(const uint8_t *mac_addr, esp_now_send_status_t status);
#endif
void update_status_leds(uint16_t battery_mv);

// ======================================================
// Setup
// ======================================================
void setup() {
    Serial.begin(115200);
    // On ESP32-C3 SuperMini USB-CDC, wait briefly for serial monitor connection
    uint32_t t_start = millis();
    while (!Serial && (millis() - t_start < 2500));
    delay(200);

    Serial.println("\n=======================================================");
    Serial.printf ("   🎙️ ESP32-C3 Cot Node %d (Final 2-LED Implementation)\n", CURRENT_NODE_ID);
    Serial.println("=======================================================");

    // 1. Initialize 2 Status LEDs
    pinMode(PIN_LED_RED, OUTPUT);
    digitalWrite(PIN_LED_RED, LOW); // Active-HIGH: Start OFF

    pinMode(PIN_LED_BLUE, OUTPUT);
    digitalWrite(PIN_LED_BLUE, HIGH); // Active-LOW: Start OFF (HIGH = OFF)

    // 2. Configure ADC Pin for Battery Reading
    analogReadResolution(12); // 12-bit ADC (0 - 4095)
    analogSetAttenuation(ADC_11db); // Full range ~0 to 3.1V on GPIO1

    // 3. Initialize I2S Microphone Driver (GPIO5=SCK, GPIO6=WS, GPIO7=SD)
    if (!init_i2s()) {
        Serial.println("❌ I2S initialization failed!");
        while (1) { delay(1000); }
    }

    // 4. Initialize Wi-Fi in Station Mode (Required for ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Keep radio awake 100% of the time (prevents modem-sleep packet drops)
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable Wi-Fi power save at IDF layer

    uint8_t node_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, node_mac);
    Serial.printf("📶 Node MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  node_mac[0], node_mac[1], node_mac[2], node_mac[3], node_mac[4], node_mac[5]);

    // Lock Wi-Fi to Channel 1 using promiscuous toggle (required in STA mode without AP)
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    // 5. Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed!");
        return;
    }

    // Register S3 Hub Unicast Peer on Channel 1
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    memcpy(peerInfo.peer_addr, hub_mac_address, 6);
    esp_now_add_peer(&peerInfo);

    // Register Broadcast Peer fallback on Channel 1
    uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(peerInfo.peer_addr, bcast, 6);
    esp_now_add_peer(&peerInfo);

    // Register Reception Callback for S3 Hub Audio Requests
    esp_now_register_recv_cb(on_data_received);
    esp_now_register_send_cb(on_send_complete);

    // 6. Enable Hardware Watchdog Timer
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&twdt_config);
#else
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
#endif
    esp_task_wdt_add(NULL);

    Serial.println("✅ ESP32-C3 Node online. Continuous audio acquisition running...\n");
}

// ======================================================
// I2S Configuration (GPIO5, 6, 7)
// ======================================================
bool init_i2s() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // INMP441 L/R tied to GND
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_INMP441_SCK,
        .ws_io_num = PIN_INMP441_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_INMP441_SD
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) return false;

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) return false;

    i2s_zero_dma_buffer(I2S_PORT);
    return true;
}

// ======================================================
// Main Loop (Single-Core ESP32-C3)
// ======================================================
void loop() {
    // Feed hardware watchdog
    esp_task_wdt_reset();

    // Check for pending audio burst request from S3 Hub
    if (pending_audio_burst) {
        pending_audio_burst = false;
        send_audio_burst(pending_session_id);
    }

    // 1. Ingest Audio DMA Chunks into the 64 KB Ring Buffer
    capture_audio();

    // 2. Periodic 100ms Telemetry Broadcast & LED State Update (skip during burst)
    if (!burst_in_progress) {
        uint32_t now = millis();
        if (now - last_telemetry_time >= TELEMETRY_INTERVAL_MS) {
            last_telemetry_time = now;

            uint16_t bat_mv = read_battery_millivolts();
            send_telemetry();
            update_status_leds(bat_mv);
        }
    }
}

// ======================================================
// Audio DMA Ingestion Routine
// ======================================================
void capture_audio() {
    int16_t temp_chunk[128];
    size_t bytes_read = 0;

    esp_err_t res = i2s_read(I2S_PORT, temp_chunk, sizeof(temp_chunk), &bytes_read, 0);
    if (res == ESP_OK && bytes_read > 0) {
        size_t samples = bytes_read / sizeof(int16_t);
        for (size_t i = 0; i < samples; i++) {
            ring_buffer[ring_write_idx] = temp_chunk[i];
            ring_write_idx = (ring_write_idx + 1) % (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC);
            if (ring_write_idx == 0) buffer_ready = true;
        }
    }
}

// ======================================================
// 100ms RMS Calculator
// ======================================================
float compute_100ms_rms() {
    size_t samples_100ms = 1600; // 16,000 Hz * 0.10s
    int64_t sum = 0;
    int64_t sum_squares = 0;

    size_t current = ring_write_idx;
    for (size_t i = 0; i < samples_100ms; i++) {
        size_t pos = (current + (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC) - i - 1) % (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC);
        int32_t val = ring_buffer[pos];
        sum += val;
        sum_squares += (int64_t)val * val;
    }

    // DC-coupled RMS: Var(X) = E[X²] - E[X]²
    // The INMP441 has a large DC bias (~18000 raw counts) that must be
    // subtracted before computing actual acoustic signal energy
    float mean = (float)sum / (float)samples_100ms;
    float mean_square = (float)sum_squares / (float)samples_100ms;
    float variance = mean_square - (mean * mean);
    if (variance < 0.0f) variance = 0.0f; // Guard against floating-point rounding
    return sqrtf(variance) / 32768.0f; // Normalized [0.0, 1.0]
}

// ======================================================
// Battery ADC Voltage Reader (GPIO1)
// Circuit: VBAT -> 100k -> GPIO1 -> 100k -> GND (VADC = VBAT / 2)
// ======================================================
uint16_t read_battery_millivolts() {
    // Oversample 8 times for smooth reading
    uint32_t raw_sum = 0;
    for (int i = 0; i < 8; i++) {
        raw_sum += analogReadMilliVolts(PIN_BATTERY_ADC);
    }
    uint32_t vadc_mv = raw_sum / 8;
    
    // Voltage divider ratio: VBAT = VADC * 2
    uint16_t vbat_mv = (uint16_t)(vadc_mv * 2);
    return vbat_mv;
}

// ======================================================
// Telemetry Dispatcher (Tier 1: 14 Bytes)
// ======================================================
void send_telemetry() {
    telemetry_packet_t pkt;
    pkt.msg_type = MSG_TYPE_TELEMETRY;
    pkt.node_id = CURRENT_NODE_ID;
    pkt.seq_num = telemetry_seq++;
    pkt.timestamp_ms = millis();
    pkt.rms_amplitude = compute_100ms_rms();
    pkt.battery_mv = read_battery_millivolts();

    esp_now_send(hub_mac_address, (uint8_t*)&pkt, sizeof(pkt));
}

// ======================================================
// ESP-NOW Receive Callback (Listens for Hub Audio Requests)
// ======================================================
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    const uint8_t *mac = recv_info->src_addr;
#else
void on_data_received(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
    if (len < 1) return;
    last_s3_contact_time = millis();
    is_connected_to_hub = true;

    // Dynamically capture and register S3 Hub MAC address on Channel 1
    if (mac[0] != 0xFF && !(mac[0] == 0 && mac[1] == 0)) {
        if (memcmp(hub_mac_address, mac, 6) != 0) {
            memcpy(hub_mac_address, mac, 6);
            if (!esp_now_is_peer_exist(hub_mac_address)) {
                esp_now_peer_info_t peer = {};
                memcpy(peer.peer_addr, hub_mac_address, 6);
                peer.channel = 1;
                peer.encrypt = false;
                esp_now_add_peer(&peer);
            }
        }
    }

    if (len == sizeof(audio_req_packet_t)) {
        audio_req_packet_t req;
        memcpy(&req, incomingData, sizeof(req));

        if (req.msg_type == MSG_TYPE_AUDIO_REQ && req.target_node == CURRENT_NODE_ID) {
            // Enforce burst cooldown to prevent rapid re-triggering
            if (burst_in_progress || pending_audio_burst || (millis() - last_burst_time < BURST_COOLDOWN_MS)) {
                Serial.println("⏳ Burst cooldown active, ignoring request.");
                return;
            }
            Serial.printf("⚡ [AUDIO REQ RECEIVED] S3 Hub requested audio buffer! Session: %u\n", req.session_id);
            pending_session_id = req.session_id;
            pending_audio_burst = true;
        }
    }
}

// ======================================================
// ESP-NOW Send Completion Callback (Flow Control for Burst)
// ======================================================
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void on_send_complete(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    espnow_send_done = true;
}
#else
void on_send_complete(const uint8_t *mac_addr, esp_now_send_status_t status) {
    espnow_send_done = true;
}
#endif

// ======================================================
// 64 KB Audio Burst (Tier 2: 320 Fragments x 200 Bytes)
// ======================================================
void send_audio_burst(uint16_t session_id) {
    burst_in_progress = true;

    // Snapshot the ring buffer into tx_buffer (freeze DMA-safe copy)
    size_t start = buffer_ready ? ring_write_idx : 0;
    for (size_t i = 0; i < (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC); i++) {
        size_t pos = (start + i) % (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC);
        tx_buffer[i] = ring_buffer[pos];
    }

    uint8_t* raw_pcm_bytes = (uint8_t*)tx_buffer;
    uint32_t t0 = millis();
    uint16_t failed_count = 0;

    audio_frag_packet_t frag;
    frag.msg_type = MSG_TYPE_AUDIO_FRAG;
    frag.node_id = CURRENT_NODE_ID;
    frag.session_id = session_id;
    frag.payload_len = AUDIO_PAYLOAD_PER_PACKET;

    for (uint16_t f = 0; f < TOTAL_AUDIO_FRAGMENTS; f++) {
        frag.frag_index = f;
        memcpy(frag.audio_data, &raw_pcm_bytes[f * AUDIO_PAYLOAD_PER_PACKET], AUDIO_PAYLOAD_PER_PACKET);

        // Flow-controlled send: wait for each packet's actual RF transmission completion
        bool sent = false;
        for (int retry = 0; retry < 5; retry++) {
            espnow_send_done = false;
            esp_err_t result = esp_now_send(hub_mac_address, (uint8_t*)&frag, sizeof(frag));
            if (result == ESP_OK) {
                // Wait for send callback to confirm actual RF transmission (max 50ms)
                uint32_t wait_start = millis();
                while (!espnow_send_done && (millis() - wait_start < 50)) {
                    delay(1); // Yield to FreeRTOS Wi-Fi task on single-core C3
                }
                sent = true;
                break;
            }
            // TX queue full (ESP_ERR_ESPNOW_NO_MEM): yield to let Wi-Fi hardware drain
            delay(3);
        }
        if (!sent) {
            failed_count++;
        }

        // Feed watchdog during burst in loopTask (every 20 fragments ~ 80ms)
        if (f % 20 == 0) {
            esp_task_wdt_reset();
        }
    }

    uint32_t elapsed = millis() - t0;
    burst_in_progress = false;
    last_burst_time = millis();
    i2s_zero_dma_buffer(I2S_PORT); // Flush stale DMA audio accumulated during burst
    Serial.printf("✅ Burst Complete: %u/%u fragments sent in %u ms (failed: %u)\n",
                  TOTAL_AUDIO_FRAGMENTS - failed_count, TOTAL_AUDIO_FRAGMENTS, elapsed, failed_count);
}

// ======================================================
// Two-LED Status Logic Implementation
// ------------------------------------------------------
// RED LED (GPIO4, Active-HIGH): Battery Condition
//   - High Battery   (> 3.8V) : Solid ON
//   - Medium Battery (3.4V - 3.8V): Slow Blink (1000ms cycle)
//   - Low Battery    (< 3.4V) : Fast Blink (250ms cycle)
//
// BLUE LED (GPIO9, Active-LOW): ESP-NOW Communication State
//   - Connected    (Heartbeat < 5s): Solid ON (GPIO9 = LOW)
//   - Disconnected (Timeout > 5s)  : Blinking (GPIO9 toggles)
// ======================================================
void update_status_leds(uint16_t battery_mv) {
    uint32_t now = millis();

    // 1. Control RED LED (GPIO4, Active-HIGH)
    if (battery_mv >= 3800) {
        // High Battery: Solid ON
        digitalWrite(PIN_LED_RED, HIGH);
    } else if (battery_mv >= 3400) {
        // Medium Battery: Slow Blink (500ms ON, 500ms OFF)
        bool slow_state = (now / 500) % 2;
        digitalWrite(PIN_LED_RED, slow_state ? HIGH : LOW);
    } else {
        // Low Battery: Fast Blink (125ms ON, 125ms OFF)
        bool fast_state = (now / 125) % 2;
        digitalWrite(PIN_LED_RED, fast_state ? HIGH : LOW);
    }

    // 2. Control BLUE LED (GPIO9, Active-LOW: LOW=ON, HIGH=OFF)
    if (now - last_s3_contact_time > S3_HEARTBEAT_TIMEOUT) {
        is_connected_to_hub = false;
    }

    if (is_connected_to_hub) {
        // Connected: Solid ON (Drive GPIO9 LOW)
        digitalWrite(PIN_LED_BLUE, LOW);
    } else {
        // Disconnected: Blinking (Drive GPIO9 alternating LOW/HIGH)
        bool blink_state = (now / 500) % 2;
        digitalWrite(PIN_LED_BLUE, blink_state ? LOW : HIGH);
    }
}
