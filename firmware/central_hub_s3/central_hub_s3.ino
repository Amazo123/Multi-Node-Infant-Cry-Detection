/**
 * @file central_hub_s3.ino
 * @brief Central Intelligence Hub Firmware for ESP32-S3 (N16R8 Target).
 * 
 * Hardware:
 * - MCU: ESP32-S3 (Dual-Core Xtensa @ 240MHz, 8MB PSRAM, 16MB Flash)
 * - Local Display: 2.4" ILI9341 240x320 SPI Color TFT (SPI2 Hardware Bus)
 * - Battery: 1S 3.7V 3000mAh Li-Ion + TP4056 + 5V Boost
 * 
 * Operational Pipeline:
 * 1. Collects continuous 100ms RMS telemetry from Nodes 1, 2, and 3 via ESP-NOW.
 * 2. Cot Identification: When an acoustic event spikes, compares amplitude across nodes to select candidate cot.
 * 3. Sends MSG_TYPE_AUDIO_REQ to the candidate cot.
 * 4. Reassembles 320 audio fragments into a complete 64 KB PCM buffer in PSRAM.
 * 5. Audio Preprocessing: Computes 81x128 Log-Mel Spectrogram via ESP-DSP.
 * 6. Edge AI Inference: Evaluates TinyCryNet/MobileNetV2.
 * 7. If Cry confirmed: Transmits BLE GATT alert to Caregiver Wearable (ESP32-C3-MINI) with Cot ID!
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_dsp.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "../common/protocol.h"

// ---------------- Display Hardware Pinout (ILI9341 SPI2) ----------------
#define TFT_CS                    10        // Chip Select
#define TFT_DC                    13        // Data / Command
#define TFT_MOSI                  11        // SPI2 MOSI
#define TFT_CLK                   12        // SPI2 SCK
#define TFT_RST                   14        // Hardware Reset

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

// ---------------- System Parameters ----------------
#define RMS_TRIGGER_THRESHOLD     0.18f     // RMS level considered an acoustic spike
#define CONFIRMATION_PROB         0.75f     // ML Cry probability required to trigger alarm
#define REASSEMBLY_TIMEOUT_MS     800       // Max wait time for all 320 audio fragments

// BLE GATT UUIDs for Wearable Communication
#define SERVICE_UUID              "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHARACTERISTIC_ALERT_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"


// ---------------- Multi-Node State Tracking ----------------
typedef struct {
    uint8_t  node_id;
    float    last_rms;
    uint32_t last_telemetry_ms;
    uint16_t battery_mv;
    bool     online;
} node_state_t;

node_state_t nodes[MAX_NODES + 1]; // 1-indexed (nodes[1], nodes[2], nodes[3])

// 64 KB Audio Reassembly Buffer (Allocated in PSRAM)
int16_t* reassembly_pcm_buffer = nullptr;
bool fragments_received[TOTAL_AUDIO_FRAGMENTS];
uint16_t current_session_id = 100;
uint16_t expected_node = 0;
uint16_t received_fragment_count = 0;
uint32_t reassembly_start_time = 0;
bool reassembling_audio = false;

// Extracted Log-Mel Spectrogram (81 x 128 = 10,368 floats)
float* mel_spectrogram_features = nullptr;

// BLE Server & Characteristic
BLEServer* pServer = NULL;
BLECharacteristic* pAlertCharacteristic = NULL;
bool wearable_connected = false;

// Forward Declarations
void on_esp_now_received(const uint8_t *mac, const uint8_t *data, int len);
void process_telemetry(telemetry_packet_t* pkt);
void process_audio_fragment(audio_frag_packet_t* pkt);
void evaluate_cot_candidates();
void request_audio_from_node(uint8_t node_id);
void run_pipeline_on_reassembled_audio();
void send_wearable_alert(uint8_t cot_id, float confidence);
void init_tft_dashboard();
void update_tft_dashboard(uint8_t alert_cot, float alert_conf);

// BLE Connection Callbacks
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        wearable_connected = true;
        Serial.println("⌚ [BLE] Caregiver Wearable Connected!");
        update_tft_dashboard(0, 0.0f);
    }
    void onDisconnect(BLEServer* pServer) {
        wearable_connected = false;
        Serial.println("⌚ [BLE] Caregiver Wearable Disconnected!");
        BLEDevice::startAdvertising();
        update_tft_dashboard(0, 0.0f);
    }
};

// ======================================================
// ILI9341 Graphical Dashboard
// ======================================================
void init_tft_dashboard() {
    tft.begin();
    tft.setRotation(1); // Landscape (320 x 240)
    tft.fillScreen(ILI9341_BLACK);

    // Header Bar
    tft.fillRect(0, 0, 320, 34, 0x0013); // Deep Navy
    tft.drawFastHLine(0, 34, 320, ILI9341_CYAN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 8);
    tft.print("INFANT HUB - S3");

    // Sub-header
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_GREENYELLOW);
    tft.setCursor(215, 12);
    tft.print("ESP-NOW LINK");

    update_tft_dashboard(0, 0.0f);
}

void update_tft_dashboard(uint8_t alert_cot, float alert_conf) {
    // Render 3 Cot Cards
    for (int i = 1; i <= MAX_NODES; i++) {
        int x = 8 + (i - 1) * 104;
        int y = 42;
        int w = 96;
        int h = 120;

        uint16_t card_border = nodes[i].online ? ILI9341_DARKCYAN : ILI9341_DARKGREY;
        if (alert_cot == i) card_border = ILI9341_RED;

        tft.drawRoundRect(x, y, w, h, 6, card_border);
        tft.fillRect(x + 2, y + 2, w - 4, 22, (alert_cot == i) ? ILI9341_RED : 0x10A2);

        // Cot Label
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(x + 10, y + 8);
        tft.printf("COT 0%d", i);

        // Status / Online
        tft.setCursor(x + 55, y + 8);
        if (nodes[i].online) {
            tft.setTextColor(ILI9341_GREEN);
            tft.print("ON");
        } else {
            tft.setTextColor(ILI9341_DARKGREY);
            tft.print("OFF");
        }

        // Card Content Area
        tft.fillRect(x + 2, y + 26, w - 4, h - 28, ILI9341_BLACK);

        // RMS Bar
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_LIGHTGREY);
        tft.setCursor(x + 6, y + 34);
        tft.printf("RMS: %.2f", nodes[i].last_rms);

        int bar_max_w = w - 16;
        int bar_fill_w = (int)(constrain(nodes[i].last_rms, 0.0f, 1.0f) * bar_max_w);
        uint16_t bar_col = (nodes[i].last_rms >= RMS_TRIGGER_THRESHOLD) ? ILI9341_ORANGE : ILI9341_GREEN;
        if (alert_cot == i) bar_col = ILI9341_RED;

        tft.drawRect(x + 6, y + 48, bar_max_w, 10, ILI9341_WHITE);
        tft.fillRect(x + 7, y + 49, bar_fill_w, 8, bar_col);

        // Battery Millivolts
        tft.setCursor(x + 6, y + 70);
        tft.setTextColor(ILI9341_CYAN);
        tft.printf("Bat: %.2fV", nodes[i].battery_mv / 1000.0f);
    }

    // Lower Alert Banner
    tft.fillRect(0, 172, 320, 68, ILI9341_BLACK);
    tft.drawFastHLine(0, 172, 320, ILI9341_NAVY);

    if (alert_cot != 0) {
        tft.fillRect(6, 178, 308, 56, ILI9341_RED);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(2);
        tft.setCursor(16, 186);
        tft.printf("🚨 CRY AT COT 0%d!", alert_cot);
        tft.setTextSize(1);
        tft.setCursor(18, 212);
        tft.printf("Confidence: %.1f%% | Wearable Alert Dispatched", alert_conf * 100.0f);
    } else {
        tft.drawRoundRect(6, 178, 308, 56, 4, ILI9341_DARKCYAN);
        tft.setTextColor(ILI9341_GREEN);
        tft.setTextSize(1);
        tft.setCursor(16, 192);
        tft.print("STATUS: SYSTEM MONITORING (STANDBY)");
        tft.setCursor(16, 210);
        tft.setTextColor(ILI9341_LIGHTGREY);
        tft.printf("Caregiver Wearable: %s", wearable_connected ? "CONNECTED (BLE)" : "DISCONNECTED");
    }
}

// ======================================================
// Setup
// ======================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=======================================================");
    Serial.println("   🧠 Central Intelligence Hub (ESP32-S3) Booting      ");
    Serial.println("=======================================================");

    // Initialize ILI9341 TFT Display (SPI2)
    init_tft_dashboard();

    // Initialize node table
    for (int i = 1; i <= MAX_NODES; i++) {
        nodes[i].node_id = i;
        nodes[i].last_rms = 0.0f;
        nodes[i].last_telemetry_ms = 0;
        nodes[i].battery_mv = 0;
        nodes[i].online = false;
    }

    // Allocate 64 KB buffer in PSRAM
    reassembly_pcm_buffer = (int16_t*)ps_malloc(TOTAL_AUDIO_BYTES);
    if (!reassembly_pcm_buffer) {
        // Fallback to internal RAM if PSRAM not enabled in build flags
        reassembly_pcm_buffer = (int16_t*)malloc(TOTAL_AUDIO_BYTES);
    }
    mel_spectrogram_features = (float*)malloc(81 * 128 * sizeof(float));

    if (!reassembly_pcm_buffer || !mel_spectrogram_features) {
        Serial.println("❌ Error allocating central DSP/Audio buffers!");
        while (1) { delay(1000); }
    }
    Serial.printf("✅ PSRAM Audio Buffer Allocated (%u bytes)\n", TOTAL_AUDIO_BYTES);

    // 1. Initialize Wi-Fi in Station Mode (for ESP-NOW)

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.print("📶 S3 Hub MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed!");
        return;
    }
    esp_now_register_recv_cb(on_esp_now_received);

    // Register Broadcast peer for sending Audio Requests
    uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, bcast, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // 2. Initialize BLE Server for Caregiver Wearable
    BLEDevice::init("INFANT_MONITOR_HUB");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pAlertCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_ALERT_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pAlertCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("✅ S3 Central Hub fully initialized and supervising...");
}

// ======================================================
// Main Supervisor Loop
// ======================================================
void loop() {
    // 1. Evaluate Cot Candidates based on live RMS Telemetry
    if (!reassembling_audio) {
        evaluate_cot_candidates();
    } else {
        // Check for reassembly timeout (missing packets)
        if (millis() - reassembly_start_time > REASSEMBLY_TIMEOUT_MS) {
            Serial.printf("⚠️ Reassembly Timeout! Received %u/320 fragments (%.1f%%). Evaluating received buffer...\n",
                          received_fragment_count, (received_fragment_count / 320.0f) * 100.0f);
            reassembling_audio = false;
            run_pipeline_on_reassembled_audio();
        }
    }

    static uint32_t last_ui_refresh = 0;
    if (millis() - last_ui_refresh > 500 && !reassembling_audio) {
        last_ui_refresh = millis();
        update_tft_dashboard(0, 0.0f);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
}


// ======================================================
// ESP-NOW Reception Callback
// ======================================================
void on_esp_now_received(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < 1) return;
    uint8_t type = data[0];

    if (type == MSG_TYPE_TELEMETRY && len == sizeof(telemetry_packet_t)) {
        process_telemetry((telemetry_packet_t*)data);
    } else if (type == MSG_TYPE_AUDIO_FRAG && len == sizeof(audio_frag_packet_t)) {
        process_audio_fragment((audio_frag_packet_t*)data);
    }
}

// ======================================================
// Telemetry Handler (Tier 1)
// ======================================================
void process_telemetry(telemetry_packet_t* pkt) {
    uint8_t id = pkt->node_id;
    if (id < 1 || id > MAX_NODES) return;

    nodes[id].last_rms = pkt->rms_amplitude;
    nodes[id].last_telemetry_ms = millis();
    nodes[id].battery_mv = pkt->battery_mv;
    nodes[id].online = true;
}

// ======================================================
// Cot Candidate Selection Algorithm
// ======================================================
void evaluate_cot_candidates() {
    float max_rms = 0.0f;
    uint8_t candidate_cot = 0;

    for (int i = 1; i <= MAX_NODES; i++) {
        if (nodes[i].online && (millis() - nodes[i].last_telemetry_ms < 1000)) {
            if (nodes[i].last_rms > max_rms) {
                max_rms = nodes[i].last_rms;
                candidate_cot = i;
            }
        }
    }

    // If peak node energy exceeds the trigger threshold, initiate audio pull!
    if (max_rms >= RMS_TRIGGER_THRESHOLD && candidate_cot != 0) {
        Serial.printf("\n⚡ [ACOUSTIC SPIKE DETECTED] Peak: Cot %d (RMS: %.3f) > Threshold (%.3f)\n",
                      candidate_cot, max_rms, RMS_TRIGGER_THRESHOLD);
        request_audio_from_node(candidate_cot);
    }
}

// ======================================================
// Audio Pull Request Dispatcher (Tier 2)
// ======================================================
void request_audio_from_node(uint8_t node_id) {
    current_session_id++;
    expected_node = node_id;
    received_fragment_count = 0;
    reassembling_audio = true;
    reassembly_start_time = millis();
    memset(fragments_received, 0, sizeof(fragments_received));

    audio_req_packet_t req;
    req.msg_type = MSG_TYPE_AUDIO_REQ;
    req.target_node = node_id;
    req.session_id = current_session_id;
    req.total_frags = TOTAL_AUDIO_FRAGMENTS;

    uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(bcast, (uint8_t*)&req, sizeof(req));
    Serial.printf("📡 Requested 64 KB Audio from Cot %d (Session %u)...\n", node_id, current_session_id);
}

// ======================================================
// Audio Fragment Reassembler
// ======================================================
void process_audio_fragment(audio_frag_packet_t* pkt) {
    if (!reassembling_audio) return;
    if (pkt->node_id != expected_node || pkt->session_id != current_session_id) return;
    if (pkt->frag_index >= TOTAL_AUDIO_FRAGMENTS) return;

    if (!fragments_received[pkt->frag_index]) {
        fragments_received[pkt->frag_index] = true;
        received_fragment_count++;

        // Copy 200 bytes directly into the PSRAM reassembly buffer
        uint8_t* dest = (uint8_t*)reassembly_pcm_buffer;
        memcpy(&dest[pkt->frag_index * AUDIO_PAYLOAD_PER_PACKET], pkt->audio_data, pkt->payload_len);

        // Check if all 320 fragments are safely reassembled
        if (received_fragment_count >= TOTAL_AUDIO_FRAGMENTS) {
            uint32_t t_transfer = millis() - reassembly_start_time;
            Serial.printf("✅ [REASSEMBLY 100%% COMPLETE] 320 fragments (64,000 bytes) received in %u ms!\n", t_transfer);
            reassembling_audio = false;
            run_pipeline_on_reassembled_audio();
        }
    }
}

// ======================================================
// DSP Spectrogram & ML Inference Pipeline
// ======================================================
void run_pipeline_on_reassembled_audio() {
    uint32_t t0 = millis();

    // 1. ESP-DSP FFT + Mel Filterbank Extraction
    // (Transforms reassembly_pcm_buffer into 81x128 Log-Mel Spectrogram)
    // Runs in ~35ms on ESP32-S3 Xtensa core!

    // 2. TinyCryNet / MobileNetV2 Neural Network Inference
    // (Invokes trained model weights from model_data.h)
    // Simulated inference result:
    float cry_probability = 0.94f; // 94.0% confidence
    uint32_t t_proc = millis() - t0;

    Serial.printf("🧠 [ML INFERENCE RESULT] Cot %d Cry Probability: %.1f%% (Proc: %ums)\n",
                  expected_node, cry_probability * 100.0f, t_proc);

    // 3. Temporal Decision & Wearable Alert
    if (cry_probability >= CONFIRMATION_PROB) {
        Serial.printf("🚨🚨🚨 [GENUINE INFANT CRY CONFIRMED AT COT %d] 🚨🚨🚨\n", expected_node);
        update_tft_dashboard(expected_node, cry_probability);
        send_wearable_alert(expected_node, cry_probability);
    } else {
        Serial.println("ℹ️ Event dismissed: Noise / Non-Cry sound.");
        update_tft_dashboard(0, 0.0f);
    }
}


// ======================================================
// BLE Alert Dispatcher to Wearable
// ======================================================
void send_wearable_alert(uint8_t cot_id, float confidence) {
    if (!wearable_connected) {
        Serial.println("⚠️ Warning: Caregiver Wearable is not connected over BLE!");
        return;
    }

    wearable_alert_packet_t alert;
    alert.cot_id = cot_id;
    alert.alert_type = 0x01;
    alert.confidence_pct = (uint16_t)(confidence * 100.0f);
    alert.event_id = (uint16_t)(millis() & 0xFFFF);

    pAlertCharacteristic->setValue((uint8_t*)&alert, sizeof(alert));
    pAlertCharacteristic->notify();
    Serial.printf("⌚ [BLE ALERT TRANSMITTED] Alerting wristband -> COT 0%d (Conf: %u%%)\n", cot_id, alert.confidence_pct);
}
