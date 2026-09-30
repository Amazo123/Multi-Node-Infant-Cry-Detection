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
 * 4. Broadcasts 12-byte telemetry packets over ESP-NOW to ESP32-S3 hub.
 * 5. On MSG_TYPE_AUDIO_REQ from S3 hub, freezes buffer and bursts 320 packets (200 bytes each).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <driver/i2s.h>
#include "../common/protocol.h"

// ---------------- Node Configuration ----------------
#define CURRENT_NODE_ID       1       // 1, 2, or 3 for each physical cot
#define TELEMETRY_INTERVAL_MS 100     // Milliseconds between RMS telemetry updates
#define S3_HEARTBEAT_TIMEOUT  5000    // If no hub contact for >5s, mark disconnected

// ---------------- Final Locked Pinout ----------------
#define PIN_BATTERY_ADC       1       // GPIO1: Battery Voltage Divider (100k / 100k)
#define PIN_LED_RED           4       // GPIO4: Red LED (Battery Status, Active-HIGH)
#define PIN_INMP441_SCK       5       // GPIO5: I2S Serial Bit Clock (BCLK)
#define PIN_INMP441_WS        6       // GPIO6: I2S Word Select / Frame Sync (LRCK)
#define PIN_INMP441_SD        7       // GPIO7: I2S Serial Data (DIN on MCU)
#define PIN_LED_BLUE          9       // GPIO9: Blue LED (ESP-NOW Link, Active-LOW)

#define I2S_PORT              I2S_NUM_0

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
bool is_connected_to_hub = false;

// Broadcast Address for S3 Hub
uint8_t hub_mac_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ---------------- Forward Declarations ----------------
bool init_i2s();
void capture_audio();
float compute_100ms_rms();
uint16_t read_battery_millivolts();
void send_telemetry();
void send_audio_burst(uint16_t session_id);
void on_data_received(const uint8_t *mac, const uint8_t *incomingData, int len);
void update_status_leds(uint16_t battery_mv);

// ======================================================
// Setup
// ======================================================
void setup() {
    Serial.begin(115200);
    delay(500);
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
    WiFi.disconnect();
    Serial.print("📶 Node MAC: ");
    Serial.println(WiFi.macAddress());

    // 5. Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed!");
        return;
    }

    // Register S3 Hub Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, hub_mac_address, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // Register Reception Callback for S3 Hub Audio Requests
    esp_now_register_recv_cb(on_data_received);

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
    // 1. Ingest Audio DMA Chunks into the 64 KB Ring Buffer
    capture_audio();

    // 2. Periodic 100ms Telemetry Broadcast & LED State Update
    uint32_t now = millis();
    if (now - last_telemetry_time >= TELEMETRY_INTERVAL_MS) {
        last_telemetry_time = now;

        uint16_t bat_mv = read_battery_millivolts();
        send_telemetry();
        update_status_leds(bat_mv);
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
    int64_t sum_squares = 0;

    size_t current = ring_write_idx;
    for (size_t i = 0; i < samples_100ms; i++) {
        size_t pos = (current + (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC) - i - 1) % (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC);
        int32_t val = ring_buffer[pos];
        sum_squares += (int64_t)val * val;
    }

    float mean_square = (float)sum_squares / (float)samples_100ms;
    return sqrtf(mean_square) / 32768.0f; // Normalized [0.0, 1.0]
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
// Telemetry Dispatcher (Tier 1: 12 Bytes)
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
void on_data_received(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len < 1) return;
    last_s3_contact_time = millis();
    is_connected_to_hub = true;

    if (len == sizeof(audio_req_packet_t)) {
        audio_req_packet_t req;
        memcpy(&req, incomingData, sizeof(req));

        if (req.msg_type == MSG_TYPE_AUDIO_REQ && req.target_node == CURRENT_NODE_ID) {
            Serial.printf("⚡ [AUDIO REQ RECEIVED] S3 Hub requested audio buffer! Session: %u\n", req.session_id);
            send_audio_burst(req.session_id);
        }
    }
}

// ======================================================
// 64 KB Audio Burst (Tier 2: 320 Fragments x 200 Bytes)
// ======================================================
void send_audio_burst(uint16_t session_id) {
    size_t start = buffer_ready ? ring_write_idx : 0;
    for (size_t i = 0; i < (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC); i++) {
        size_t pos = (start + i) % (AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC);
        tx_buffer[i] = ring_buffer[pos];
    }

    uint8_t* raw_pcm_bytes = (uint8_t*)tx_buffer;
    uint32_t t0 = millis();

    audio_frag_packet_t frag;
    frag.msg_type = MSG_TYPE_AUDIO_FRAG;
    frag.node_id = CURRENT_NODE_ID;
    frag.session_id = session_id;
    frag.payload_len = AUDIO_PAYLOAD_PER_PACKET;

    for (uint16_t f = 0; f < TOTAL_AUDIO_FRAGMENTS; f++) {
        frag.frag_index = f;
        memcpy(frag.audio_data, &raw_pcm_bytes[f * AUDIO_PAYLOAD_PER_PACKET], AUDIO_PAYLOAD_PER_PACKET);

        esp_now_send(hub_mac_address, (uint8_t*)&frag, sizeof(frag));
        delayMicroseconds(250); // Pacing delay
    }

    uint32_t elapsed = millis() - t0;
    Serial.printf("✅ Burst Complete: Transmitted 320 fragments (64,000 bytes) in %u ms!\n", elapsed);
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
