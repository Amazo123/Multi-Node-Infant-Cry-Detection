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
 * 6. Edge AI Inference: Evaluates TinyCryNet via TensorFlow Lite Micro.
 * 7. If Cry confirmed: Transmits BLE GATT alert to Caregiver Wearable with Cot ID.
 * 8. Post-detection cooldown prevents alert flooding.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_dsp.h>

// Set to 1: Enable BLE GATT Server for Caregiver Wearable alerts
// Set to 0: Disable BLE (e.g. for standalone TFT debugging or boards without BLE)
#define ENABLE_BLE                1

#if ENABLE_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <esp_task_wdt.h>
#include "../common/protocol.h"
#include "pins.h"
#include "dsp_mel_extractor.h"
// =========================================================================
// Inference Mode Configuration
// Set to 0: High-speed Real-Time DSP/RMS Acoustic Detection (Core 3.x compatible)
// Set to 1: TensorFlow Lite Micro Neural Inference
// =========================================================================
#define ENABLE_TFLITE 0

#if ENABLE_TFLITE
#include "model_data.h"
// TFLite Micro Headers
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#endif

// Custom display driver wrapper for TPM408-2.8 (ILI9342 / Landscape-First Panel)
// Overrides Adafruit's 240px clipping limit so Rotation 2 fills the entire 320x240 glass
class HubDisplay : public Adafruit_ILI9341 {
public:
    HubDisplay(SPIClass *spiClass, int8_t dc, int8_t cs, int8_t rst)
        : Adafruit_ILI9341(spiClass, dc, cs, rst) {}

    void setOperationalOrientation(uint8_t rot) {
        setRotation(rot);
        // On 320x240 landscape-first panels (TPM408-2.8 / ILI9342),
        // Rotation 2 is the correct upright orientation. We override
        // Adafruit_GFX's _width to 320 so graphics fill the full screen width.
        _width = 320;
        _height = 240;
    }
};

HubDisplay tft = HubDisplay(&SPI, TFT_DC, TFT_CS, TFT_RST);

// ---------------- System Parameters ----------------
#define RMS_TRIGGER_THRESHOLD     0.18f     // RMS level considered an acoustic spike
#define CONFIRMATION_PROB         0.65f     // ML Cry probability required to trigger alarm
#define REASSEMBLY_TIMEOUT_MS     3500      // 3.5s timeout for complete 320-fragment audio transfer (1600ms burst + ample RF margin)
#define AUDIO_PULL_COOLDOWN_MS   4500      // Minimum time between audio pull requests to prevent request storms
#define POST_DETECT_COOLDOWN_MS   12000     // 12-second cooldown after confirmed detection
#define NODE_OFFLINE_TIMEOUT_MS   3000      // Mark node offline if no telemetry for 3 seconds
#define CONSECUTIVE_SPIKE_REQ     2         // Require 2 consecutive spikes before audio pull
#define WDT_TIMEOUT_SEC           20        // Hardware watchdog timeout

// ---------------- Display Orientation & Settings ----------------
// 2 = Upright Landscape (Rotation 2 with 320x240 override - Active configuration!)
#define TFT_ROTATION              2
#define TFT_INVERT_COLORS         false

// BLE GATT UUIDs for Wearable Communication
#define SERVICE_UUID              "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHARACTERISTIC_ALERT_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"

// TFLite Micro Arena Size — 320 KB in PSRAM for model inference workspace
#define TFLITE_ARENA_SIZE         (1024 * 320)

// ---------------- Multi-Node State Tracking ----------------
typedef struct {
    uint8_t  node_id;
    float    last_rms;
    uint32_t last_telemetry_ms;
    uint16_t battery_mv;
    bool     online;
    uint8_t  consecutive_spikes;    // Count of consecutive RMS spikes above threshold
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
volatile bool trigger_pipeline_run = false;

// DSP: Normalized float PCM buffer for spectrogram extraction (in PSRAM)
float* pcm_float_buffer = nullptr;

// Extracted Log-Mel Spectrogram (81 x 128 = 10,368 floats, in PSRAM)
float* mel_spectrogram_features = nullptr;

// DSP Extractor Instance
DSPMelExtractor dsp_extractor;

#if ENABLE_TFLITE
// TFLite Micro Inference Engine
static const tflite::Model* tflite_model = nullptr;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* model_input = nullptr;
static TfLiteTensor* model_output = nullptr;
static uint8_t* tensor_arena = nullptr;
#endif
static bool tflite_ready = false;

// BLE Server & Characteristic
#if ENABLE_BLE
BLEServer* pServer = NULL;
BLECharacteristic* pAlertCharacteristic = NULL;
BLEAdvertising* pAdvertising = NULL;
#endif
bool wearable_connected = false;

// Timing State
uint32_t last_detection_time = 0;
uint32_t last_audio_request_time = 0;
bool in_cooldown = false;
uint8_t last_alert_cot = 0;
float last_alert_conf = 0.0f;

// Forward Declarations
void on_esp_now_received(const uint8_t *mac, const uint8_t *data, int len);
void process_telemetry(telemetry_packet_t* pkt);
void process_audio_fragment(audio_frag_packet_t* pkt);
void evaluate_cot_candidates();
void request_audio_from_node(uint8_t node_id);
void run_pipeline_on_reassembled_audio();
float evaluate_spectral_cry_probability(const float* mel_spec);
void send_wearable_alert(uint8_t cot_id, float confidence);
void init_tft_dashboard();
void update_tft_dashboard(uint8_t alert_cot, float alert_conf);
bool init_tflite_inference();

#if ENABLE_BLE
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
#endif

// ======================================================
// TFLite Micro Initialization
// ======================================================
bool init_tflite_inference() {
#if ENABLE_TFLITE
    Serial.println("🧠 [TFLite] Initializing neural network inference engine...");

    // 1. Load model from flash (model_data.h compiled into .rodata)
    tflite_model = tflite::GetModel(model_data);
    if (tflite_model == nullptr) {
        Serial.println("❌ [TFLite] Failed to load model from flash!");
        return false;
    }

    // Verify model schema version
    if (tflite_model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("❌ [TFLite] Model schema version mismatch! Expected %d, got %lu\n",
                      TFLITE_SCHEMA_VERSION, tflite_model->version());
        return false;
    }

    // 2. Allocate tensor arena in PSRAM (320 KB)
    tensor_arena = (uint8_t*)ps_malloc(TFLITE_ARENA_SIZE);
    if (!tensor_arena) {
        Serial.println("❌ [TFLite] Failed to allocate tensor arena in PSRAM!");
        // Fallback to internal RAM (will be tight)
        tensor_arena = (uint8_t*)malloc(TFLITE_ARENA_SIZE);
        if (!tensor_arena) {
            Serial.println("❌ [TFLite] Failed to allocate tensor arena in internal RAM!");
            return false;
        }
    }

    // 3. Create op resolver (all ops for maximum compatibility)
    static tflite::MicroErrorReporter micro_error_reporter;
    static tflite::AllOpsResolver resolver;

    // 4. Build interpreter
    static tflite::MicroInterpreter static_interpreter(
        tflite_model, resolver, tensor_arena, TFLITE_ARENA_SIZE, &micro_error_reporter);
    interpreter = &static_interpreter;

    // 5. Allocate tensors
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("❌ [TFLite] AllocateTensors() failed!");
        return false;
    }

    // 6. Get input/output tensor pointers
    model_input = interpreter->input(0);
    model_output = interpreter->output(0);

    Serial.printf("✅ [TFLite] Model loaded! Input shape: [%d, %d, %d, %d], Output shape: [%d, %d]\n",
                  model_input->dims->data[0], model_input->dims->data[1],
                  model_input->dims->data[2], model_input->dims->data[3],
                  model_output->dims->data[0], model_output->dims->data[1]);
    Serial.printf("   Arena used: %u / %u bytes\n",
                  interpreter->arena_used_bytes(), TFLITE_ARENA_SIZE);

    tflite_ready = true;
    return true;
#else
    Serial.println("🧠 [ML Engine] Real-Time DSP Log-Mel Feature Extractor & Acoustic Decision Engine Active.");
    tflite_ready = true;
    return true;
#endif
}

// ======================================================
// ILI9341 Graphical Dashboard (Full Width 320x240, Zero-Flicker)
// ======================================================
enum BannerMode { MODE_UNINITIALIZED = 0, MODE_STANDBY, MODE_ALERT, MODE_COOLDOWN };
static BannerMode current_banner_mode = MODE_UNINITIALIZED;
static bool prev_ble_connected = false;
static uint32_t prev_sec_remaining = 999;
static uint8_t prev_alert_cots[MAX_NODES + 1] = {0};
static bool prev_node_online[MAX_NODES + 1] = {false};

void init_tft_dashboard() {
    // Initialize Hardware SPI bus with custom pins (software CS control)
    SPI.begin(TFT_CLK, -1, TFT_MOSI, -1);

    tft.begin(16000000); // 16 MHz clean, reliable SPI clock for ILI9341
    tft.invertDisplay(TFT_INVERT_COLORS);

    // 1. Purge physical LCD frame buffer memory in native 240x320 Portrait mode
    tft.setRotation(0);
    tft.fillScreen(ILI9341_BLACK);

    // 2. Set operational orientation (Rotation 2: Upright Landscape, full 320x240 width)
    tft.setOperationalOrientation(TFT_ROTATION);
    tft.fillScreen(ILI9341_BLACK);

    int scr_w = tft.width();

    // Clean, Non-overlapping Top Header Bar (Height: 28 px, Full 320 px Width)
    tft.fillRect(0, 0, scr_w, 28, 0x0013); // Deep Navy
    tft.drawFastHLine(0, 28, scr_w, ILI9341_CYAN);

    // Header Title
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(8, 6);
    tft.print("INFANT HUB - S3");

    // Sub-header (Right aligned)
    tft.setTextColor(ILI9341_GREENYELLOW);
    tft.setCursor(scr_w - 56, 6);
    tft.print("ESP-NOW");

    // Subtitle Line inside Header
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(8, 17);
    tft.print("Ch:1 | 3-Cot Supervision");

    // Draw static Cot card containers across full 320 px
    int left_margin = 7;
    int gap = 6;
    int card_w = 98;
    int card_y = 33;
    int card_h = 106;

    for (int i = 1; i <= MAX_NODES; i++) {
        int x = left_margin + (i - 1) * (card_w + gap);
        tft.drawRoundRect(x, card_y, card_w, card_h, 4, ILI9341_DARKCYAN);
        tft.fillRect(x + 2, card_y + 2, card_w - 4, 18, 0x10A2);

        tft.setTextSize(1);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(x + 6, card_y + 7);
        tft.printf("COT %d", i);

        tft.setCursor(x + card_w - 26, card_y + 7);
        tft.setTextColor(ILI9341_DARKGREY, 0x10A2);
        tft.print("OFF");

        // RMS Level Bar Frame (inner: width 84, height 6)
        tft.drawRect(x + 6, card_y + 38, card_w - 12, 8, ILI9341_WHITE);
        tft.fillRect(x + 7, card_y + 39, card_w - 14, 6, ILI9341_BLACK);
    }

    current_banner_mode = MODE_UNINITIALIZED;
    update_tft_dashboard(0, 0.0f);
}

void update_tft_dashboard(uint8_t alert_cot, float alert_conf) {
    int scr_w = tft.width();

    // Check node online status with timeout
    for (int i = 1; i <= MAX_NODES; i++) {
        if (millis() - nodes[i].last_telemetry_ms > NODE_OFFLINE_TIMEOUT_MS) {
            nodes[i].online = false;
        }
    }

    // ────────────────────────────────────────────────────────────────
    // 3 COT CARDS SIDE-BY-SIDE (Flicker-Free Differential Updates)
    // ────────────────────────────────────────────────────────────────
    int left_margin = 7;
    int gap = 6;
    int card_w = 98;
    int card_y = 33;
    int card_h = 106;

    for (int i = 1; i <= MAX_NODES; i++) {
        int x = left_margin + (i - 1) * (card_w + gap);

        // Update card border & header pill only if alert status changed for this card
        bool is_alert = (alert_cot == i);
        if (is_alert != (prev_alert_cots[i] != 0)) {
            prev_alert_cots[i] = is_alert ? 1 : 0;
            uint16_t border_col = is_alert ? ILI9341_RED : (nodes[i].online ? ILI9341_DARKCYAN : ILI9341_DARKGREY);
            tft.drawRoundRect(x, card_y, card_w, card_h, 4, border_col);
            tft.fillRect(x + 2, card_y + 2, card_w - 4, 18, is_alert ? ILI9341_RED : 0x10A2);

            tft.setTextSize(1);
            tft.setTextColor(ILI9341_WHITE);
            tft.setCursor(x + 6, card_y + 7);
            tft.printf("COT %d", i);

            // Re-render badge in new pill color
            tft.setCursor(x + card_w - 26, card_y + 7);
            uint16_t bg_col = is_alert ? ILI9341_RED : 0x10A2;
            tft.setTextColor(nodes[i].online ? ILI9341_GREEN : ILI9341_DARKGREY, bg_col);
            tft.print(nodes[i].online ? "ON " : "OFF");
        } else if (nodes[i].online != prev_node_online[i]) {
            // Online status changed without alert change
            prev_node_online[i] = nodes[i].online;
            uint16_t border_col = is_alert ? ILI9341_RED : (nodes[i].online ? ILI9341_DARKCYAN : ILI9341_DARKGREY);
            tft.drawRoundRect(x, card_y, card_w, card_h, 4, border_col);

            tft.setCursor(x + card_w - 26, card_y + 7);
            uint16_t bg_col = is_alert ? ILI9341_RED : 0x10A2;
            tft.setTextColor(nodes[i].online ? ILI9341_GREEN : ILI9341_DARKGREY, bg_col);
            tft.print(nodes[i].online ? "ON " : "OFF");
        }

        // Live RMS text (Opaque background: NO fillRect needed, zero flicker!)
        tft.setTextSize(1);
        tft.setCursor(x + 6, card_y + 26);
        tft.setTextColor(ILI9341_LIGHTGREY, ILI9341_BLACK);
        tft.printf("RMS: %-4.2f", nodes[i].last_rms);

        // RMS Visual Bar (Differential Update: only fills bar pixels)
        int inner_w = card_w - 14; // 84 px
        int fill_w = (int)(constrain(nodes[i].last_rms, 0.0f, 1.0f) * inner_w);
        uint16_t bar_col = (nodes[i].last_rms >= RMS_TRIGGER_THRESHOLD) ? ILI9341_ORANGE : ILI9341_GREEN;
        if (alert_cot == i) bar_col = ILI9341_RED;

        if (fill_w > 0) {
            tft.fillRect(x + 7, card_y + 39, fill_w, 6, bar_col);
        }
        if (fill_w < inner_w) {
            tft.fillRect(x + 7 + fill_w, card_y + 39, inner_w - fill_w, 6, ILI9341_BLACK);
        }

        // Battery Voltage (Opaque background)
        tft.setCursor(x + 6, card_y + 52);
        tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
        tft.printf("Bat: %-4.2fV", nodes[i].battery_mv / 1000.0f);

        // ML Engine Status (Opaque background)
        tft.setCursor(x + 6, card_y + 68);
        tft.setTextColor(tflite_ready ? ILI9341_GREEN : ILI9341_RED, ILI9341_BLACK);
        tft.printf("ML:  %-4s", tflite_ready ? (ENABLE_TFLITE ? "TFL" : "DSP") : "OFF");

        // Consecutive Spike Counter (Opaque background)
        tft.setCursor(x + 6, card_y + 84);
        tft.setTextColor(nodes[i].consecutive_spikes > 0 ? ILI9341_ORANGE : ILI9341_DARKGREY, ILI9341_BLACK);
        tft.printf("Spk: %d/%d ", nodes[i].consecutive_spikes, CONSECUTIVE_SPIKE_REQ);
    }

    // ────────────────────────────────────────────────────────────────
    // LOWER STATUS / ALERT BANNER (Starts y = 144, Height = 88, Width = 306)
    // Strictly <= 234 pixels vertical to completely prevent any screen wrapping!
    // ────────────────────────────────────────────────────────────────
    int banner_x = 7;
    int banner_y = 144;
    int banner_w = scr_w - 14; // 306 px
    int banner_h = 88;

    BannerMode target_mode = MODE_STANDBY;
    if (alert_cot != 0) target_mode = MODE_ALERT;
    else if (in_cooldown) target_mode = MODE_COOLDOWN;

    // Redraw banner frame ONLY on mode transitions (eliminates periodic wipe flicker!)
    if (target_mode != current_banner_mode) {
        current_banner_mode = target_mode;
        prev_sec_remaining = 999;
        prev_ble_connected = wearable_connected;

        tft.fillRect(banner_x, banner_y, banner_w, banner_h, ILI9341_BLACK);

        if (current_banner_mode == MODE_ALERT) {
            tft.fillRect(banner_x, banner_y, banner_w, banner_h, ILI9341_RED);
            tft.setTextColor(ILI9341_WHITE, ILI9341_RED);
            tft.setTextSize(2);
            tft.setCursor(banner_x + 12, banner_y + 10);
            tft.printf("CRY AT COT %d!", alert_cot);

            tft.setTextSize(1);
            tft.setTextColor(ILI9341_YELLOW, ILI9341_RED);
            tft.setCursor(banner_x + 12, banner_y + 36);
            tft.printf("Confidence: %.1f%%", alert_conf * 100.0f);

            tft.setTextColor(ILI9341_WHITE, ILI9341_RED);
            tft.setCursor(banner_x + 12, banner_y + 52);
            tft.print("WEARABLE VIBRATION SENT!");

            tft.setCursor(banner_x + 12, banner_y + 68);
            tft.print("Caregiver Alert Dispatched (BLE)");
        } else if (current_banner_mode == MODE_COOLDOWN) {
            tft.drawRoundRect(banner_x, banner_y, banner_w, banner_h, 4, ILI9341_ORANGE);
            tft.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setCursor(banner_x + 10, banner_y + 10);
            tft.print("STATUS: RECOVERY COOLDOWN");

            tft.setCursor(banner_x + 10, banner_y + 46);
            tft.setTextColor(ILI9341_LIGHTGREY, ILI9341_BLACK);
            tft.printf("Last alert: COT %d (%.1f%%)", last_alert_cot, last_alert_conf * 100.0f);

            tft.setCursor(banner_x + 10, banner_y + 64);
            tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
            tft.print("Noise gating active.");
        } else { // MODE_STANDBY
            tft.drawRoundRect(banner_x, banner_y, banner_w, banner_h, 4, ILI9341_DARKCYAN);
            tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setCursor(banner_x + 10, banner_y + 10);
            tft.print("STATUS: SYSTEM MONITORING (STANDBY)");

            tft.setCursor(banner_x + 10, banner_y + 28);
            tft.setTextColor(ILI9341_LIGHTGREY, ILI9341_BLACK);
            tft.printf("Caregiver: %-26s", wearable_connected ? "CONNECTED (BLE)" : "DISCONNECTED");

            tft.setCursor(banner_x + 10, banner_y + 46);
            tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
            tft.print("Radio: ESP-NOW Channel 1 (Active)");

            tft.setCursor(banner_x + 10, banner_y + 64);
            tft.setTextColor(ILI9341_GREENYELLOW, ILI9341_BLACK);
            tft.print("Listening: Cot 1, Cot 2, Cot 3");
        }
    }

    // In-mode dynamic updates with zero full-banner clearing:
    if (current_banner_mode == MODE_STANDBY) {
        if (wearable_connected != prev_ble_connected) {
            prev_ble_connected = wearable_connected;
            tft.setCursor(banner_x + 10, banner_y + 28);
            tft.setTextColor(ILI9341_LIGHTGREY, ILI9341_BLACK);
            tft.printf("Caregiver: %-26s", wearable_connected ? "CONNECTED (BLE)" : "DISCONNECTED");
        }
    } else if (current_banner_mode == MODE_COOLDOWN) {
        uint32_t remaining = (POST_DETECT_COOLDOWN_MS - (millis() - last_detection_time)) / 1000;
        if (remaining != prev_sec_remaining) {
            prev_sec_remaining = remaining;
            tft.setCursor(banner_x + 10, banner_y + 28);
            tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
            tft.printf("Re-arming in: %-2u seconds...    ", remaining);
        }
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

    // 0. Enable Hardware Watchdog Timer (20s timeout)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = 0, // 0 = Do NOT watch idle tasks (prevents 5s panic resets in Arduino)
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&twdt_config);
#else
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
#endif
    esp_task_wdt_add(NULL);

    // 1. Initialize ILI9341 TFT Display (Hardware SPI2)
    init_tft_dashboard();
    Serial.println("✅ ILI9341 TFT initialized (Hardware SPI).");

    // 2. Initialize node table
    for (int i = 1; i <= MAX_NODES; i++) {
        nodes[i].node_id = i;
        nodes[i].last_rms = 0.0f;
        nodes[i].last_telemetry_ms = 0;
        nodes[i].battery_mv = 0;
        nodes[i].online = false;
        nodes[i].consecutive_spikes = 0;
    }

    // 3. Allocate buffers in PSRAM (preserving precious internal SRAM)
    reassembly_pcm_buffer = (int16_t*)ps_malloc(TOTAL_AUDIO_BYTES);
    pcm_float_buffer = (float*)ps_malloc(AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC * sizeof(float));
    mel_spectrogram_features = (float*)ps_malloc(DSP_TOTAL_FEATURES * sizeof(float));

    if (!reassembly_pcm_buffer || !pcm_float_buffer || !mel_spectrogram_features) {
        Serial.println("❌ Error allocating PSRAM buffers! Attempting internal RAM fallback...");
        if (!reassembly_pcm_buffer) reassembly_pcm_buffer = (int16_t*)malloc(TOTAL_AUDIO_BYTES);
        if (!pcm_float_buffer) pcm_float_buffer = (float*)malloc(AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC * sizeof(float));
        if (!mel_spectrogram_features) mel_spectrogram_features = (float*)malloc(DSP_TOTAL_FEATURES * sizeof(float));
    }

    if (!reassembly_pcm_buffer || !pcm_float_buffer || !mel_spectrogram_features) {
        Serial.println("❌ FATAL: Cannot allocate audio/DSP buffers!");
        while (1) { delay(1000); }
    }
    Serial.printf("✅ PSRAM Buffers Allocated: Audio=%uB, Float=%uB, Mel=%uB\n",
                  TOTAL_AUDIO_BYTES,
                  (unsigned)(AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC * sizeof(float)),
                  (unsigned)(DSP_TOTAL_FEATURES * sizeof(float)));

    // 4. Initialize ESP-DSP Log-Mel Extractor
    if (!dsp_extractor.begin()) {
        Serial.println("❌ DSP Mel Extractor initialization failed!");
    } else {
        Serial.println("✅ ESP-DSP Mel Extractor initialized (128 bands, 512 FFT, 81 frames).");
    }

    // 5. Initialize TFLite Micro Inference Engine
    if (!init_tflite_inference()) {
        Serial.println("⚠️ TFLite inference engine failed to initialize.");
        Serial.println("   System will still detect acoustic spikes and localize cots,");
        Serial.println("   but ML-based cry confirmation will be DISABLED.");
        Serial.println("   → Ensure model_data.h contains a valid TFLite model (not ONNX).");
        Serial.println("   → Re-run the updated training pipeline to generate TFLite weights.");
    }

    // 6. Initialize Wi-Fi in Station Mode (for ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Keep radio awake 100% of the time (prevents modem-sleep packet drops)
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable Wi-Fi power save at IDF layer

    // Read real hardware MAC directly
    uint8_t hub_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, hub_mac);
    Serial.printf("📶 S3 Hub MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  hub_mac[0], hub_mac[1], hub_mac[2], hub_mac[3], hub_mac[4], hub_mac[5]);

    // Lock Wi-Fi to Channel 1 using promiscuous toggle (required in STA mode without AP)
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed!");
        return;
    }
    esp_now_register_recv_cb(on_esp_now_received);

    // Register Cot 1 Unicast Peer on Channel 1 (Hardware MAC: E0:72:A1:72:AC:14)
    uint8_t cot1_mac[] = {0xE0, 0x72, 0xA1, 0x72, 0xAC, 0x14};
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    memcpy(peerInfo.peer_addr, cot1_mac, 6);
    esp_now_add_peer(&peerInfo);

    // Register Broadcast peer on Channel 1
    uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(peerInfo.peer_addr, bcast, 6);
    esp_now_add_peer(&peerInfo);

    // 7. Initialize BLE Server for Caregiver Wearable
#if ENABLE_BLE
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
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(160); // 100ms interval (leaves radio free for Wi-Fi)
    pAdvertising->setMaxInterval(320); // 200ms interval
    BLEDevice::startAdvertising();
#else
    Serial.println("ℹ️ [BLE] Wearable alerts bypassed via ENABLE_BLE=0.");
#endif

    Serial.println("✅ S3 Central Hub fully initialized and supervising...\n");
}

// ======================================================
// Main Supervisor Loop
// ======================================================
void loop() {
    // Feed hardware watchdog
    esp_task_wdt_reset();

    // 1. Manage cooldown timer
    if (in_cooldown) {
        if (millis() - last_detection_time > POST_DETECT_COOLDOWN_MS) {
            in_cooldown = false;
            Serial.println("🔄 [COOLDOWN EXPIRED] System re-armed for detection.");
        }
    }

    // 2. Evaluate Cot Candidates based on live RMS Telemetry
    if (!reassembling_audio && !in_cooldown && !trigger_pipeline_run) {
        evaluate_cot_candidates();
    } else if (reassembling_audio) {
        // Check for reassembly timeout (missing packets)
        if (millis() - reassembly_start_time > REASSEMBLY_TIMEOUT_MS) {
            float completion = (received_fragment_count / (float)TOTAL_AUDIO_FRAGMENTS) * 100.0f;
            Serial.printf("⚠️ Reassembly Timeout! Received %u/320 fragments (%.1f%%).\n",
                          received_fragment_count, completion);

            reassembling_audio = false;
            // Only proceed if we got at least 80% of fragments (256+ of 320)
            if (received_fragment_count >= 256) {
                Serial.printf("   Sufficient data (%u/320). Running pipeline on partial buffer...\n", received_fragment_count);
                trigger_pipeline_run = true;
            } else {
                Serial.println("   Insufficient data. Discarding incomplete buffer.");
            }
        }
    }

    // 3. Process deferred audio pipeline outside of Wi-Fi callback
    if (trigger_pipeline_run) {
        trigger_pipeline_run = false;
        run_pipeline_on_reassembled_audio();
    }

    // 4. Periodic UI refresh (every 500ms when not in active processing)
    static uint32_t last_ui_refresh = 0;
    if (millis() - last_ui_refresh > 500 && !reassembling_audio) {
        last_ui_refresh = millis();
        // Show active red alert banner for the first 5 seconds after confirmed detection
        if (in_cooldown && (millis() - last_detection_time < 5000)) {
            update_tft_dashboard(last_alert_cot, last_alert_conf);
        } else {
            update_tft_dashboard(0, 0.0f);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
}


// ======================================================
// ESP-NOW Reception Callback
// ======================================================
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void on_esp_now_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const uint8_t *mac = recv_info->src_addr;
#else
void on_esp_now_received(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len < 1) return;

    // Dynamically register sender as peer on Channel 1 if not already registered
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, mac, 6);
        peer.channel = 1;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }

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
// Cot Candidate Selection with Temporal Smoothing
// ======================================================
void evaluate_cot_candidates() {
    float max_rms = 0.0f;
    uint8_t candidate_cot = 0;

    for (int i = 1; i <= MAX_NODES; i++) {
        // Check node online status with timeout
        if (millis() - nodes[i].last_telemetry_ms > NODE_OFFLINE_TIMEOUT_MS) {
            nodes[i].online = false;
            nodes[i].consecutive_spikes = 0;
            continue;
        }

        if (nodes[i].online) {
            // Track consecutive spikes for temporal smoothing
            if (nodes[i].last_rms >= RMS_TRIGGER_THRESHOLD) {
                nodes[i].consecutive_spikes++;
            } else {
                nodes[i].consecutive_spikes = 0;
            }

            if (nodes[i].last_rms > max_rms) {
                max_rms = nodes[i].last_rms;
                candidate_cot = i;
            }
        }
    }

    // Require CONSECUTIVE_SPIKE_REQ consecutive spikes before triggering audio pull
    // Enforce AUDIO_PULL_COOLDOWN_MS to prevent request storms while node is bursting
    if (candidate_cot != 0 && nodes[candidate_cot].consecutive_spikes >= CONSECUTIVE_SPIKE_REQ) {
        if (millis() - last_audio_request_time < AUDIO_PULL_COOLDOWN_MS) {
            return; // Wait for previous burst / cooldown cycle to finish
        }
        Serial.printf("\n⚡ [ACOUSTIC SPIKE CONFIRMED] Peak: Cot %d (RMS: %.3f, Consecutive: %u)\n",
                      candidate_cot, max_rms, nodes[candidate_cot].consecutive_spikes);
        nodes[candidate_cot].consecutive_spikes = 0;
        last_audio_request_time = millis();
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
    if (pkt->node_id != expected_node) return;
    if (pkt->session_id != current_session_id) {
        // Adopt the incoming session if this is the start of a burst
        if (received_fragment_count == 0) {
            current_session_id = pkt->session_id;
        } else {
            return;
        }
    }
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
            trigger_pipeline_run = true;
        }
    }
}

// ======================================================
// Real-Time DSP Spectral Infant Cry Discrimination Engine
// Evaluates harmonic distribution, spectral dispersion, and F0 pitch.
// Rejects:
// 1. Monochromatic pure tones / whistles (top-3 concentration > 55% or single peak)
// 2. Wind turbulence / breath noise / low bumps (peak_bin < 14, < 220 Hz)
// 3. Impulsive transients / claps / taps (active frames < 8, < 200ms)
// 4. Broadband ambient noise / chatter (peaks > 12 or top-3 concentration < 8%)
// Confirms:
// Infant cry phonation: fundamental F0 (300-800 Hz) + multi-harmonic formants (F1, F2, F3)
// ======================================================
float evaluate_spectral_cry_probability(const float* mel_spec) {
    if (!mel_spec) return 0.0f;

    // 1. Identify active frames (acoustic energy above -52 dB)
    int active_frames = 0;
    float sum_spec[DSP_MEL_BINS] = {0.0f};

    for (int t = 0; t < DSP_TIME_FRAMES; t++) {
        const float* frame = &mel_spec[t * DSP_MEL_BINS];
        float max_frame_db = -80.0f;
        for (int m = 0; m < DSP_MEL_BINS; m++) {
            if (frame[m] > max_frame_db) max_frame_db = frame[m];
        }
        if (max_frame_db > -52.0f) {
            active_frames++;
            for (int m = 0; m < DSP_MEL_BINS; m++) {
                sum_spec[m] += frame[m];
            }
        }
    }

    // If sound was an impulse transient (< 200ms = 8 frames), reject (clap / door / tap)
    if (active_frames < 8) {
        Serial.printf("   🔍 [Acoustic Filter] Rejected: Impulsive transient sound (<200ms, active frames: %d)\n", active_frames);
        return 0.05f;
    }

    // 2. Average spectrum across active frames & compute linear power
    float avg_spec[DSP_MEL_BINS];
    float linear_pwr[DSP_MEL_BINS];
    float total_pwr = 0.0f;
    float max_linear = 0.0f;
    int peak_bin = 0;

    for (int m = 0; m < DSP_MEL_BINS; m++) {
        avg_spec[m] = sum_spec[m] / (float)active_frames;
        // Convert dB log-mel back to linear power: 10^(dB/10)
        linear_pwr[m] = powf(10.0f, avg_spec[m] / 10.0f);
        total_pwr += linear_pwr[m];
        if (linear_pwr[m] > max_linear) {
            max_linear = linear_pwr[m];
            peak_bin = m;
        }
    }

    // 3. Spectral Power Concentration (Top-3 Bin Ratio)
    // Measures what fraction of total acoustic energy is concentrated in the single peak bin & its neighbors
    int b_min = (peak_bin > 0) ? (peak_bin - 1) : 0;
    int b_max = (peak_bin < DSP_MEL_BINS - 1) ? (peak_bin + 1) : (DSP_MEL_BINS - 1);
    float top3_pwr = 0.0f;
    for (int m = b_min; m <= b_max; m++) {
        top3_pwr += linear_pwr[m];
    }
    float top3_ratio = (total_pwr > 1e-12f) ? (top3_pwr / total_pwr) : 0.0f;

    // 4. Harmonic Formant Peak Counter
    // A genuine infant cry has 3 to 7 distinct harmonic formant peaks across the spectrum.
    // Pure tone/whistle has 1 peak; broadband noise has >12 peaks.
    int num_peaks = 0;
    for (int m = 1; m < DSP_MEL_BINS - 1; m++) {
        if (linear_pwr[m] > linear_pwr[m - 1] && linear_pwr[m] > linear_pwr[m + 1]) {
            if (linear_pwr[m] > 0.05f * max_linear) {
                num_peaks++;
            }
        }
    }

    // 5. Band Energy Analysis
    // Infant F0 Fundamental Band: Mel bins 18 to 36 (~300 Hz to 750 Hz)
    float f0_sum = 0.0f;
    for (int m = 18; m <= 36; m++) f0_sum += avg_spec[m];
    float f0_energy = f0_sum / 19.0f;

    // Harmonic Formant Band: Mel bins 37 to 70 (~780 Hz to 2,200 Hz)
    float formant_sum = 0.0f;
    for (int m = 37; m <= 70; m++) formant_sum += avg_spec[m];
    float formant_energy = formant_sum / 34.0f;

    Serial.printf("   🔍 [Spectral Features] PeakBin=%d (dB:%.1f) | Top-3 Concentration=%.1f%% | Peaks=%d | F0=%.1fdB | Formants=%.1fdB\n",
                  peak_bin, avg_spec[peak_bin], top3_ratio * 100.0f, num_peaks, f0_energy, formant_energy);

    // --- REJECTION FILTERS ---
    // 1. Monochromatic pure tone / whistle (high concentration or single peak)
    if (top3_ratio > 0.55f || num_peaks <= 1) {
        Serial.printf("   🔍 [Acoustic Filter] Rejected: Whistle / Pure Tone (Top-3: %.1f%%, Peaks: %d)\n",
                      top3_ratio * 100.0f, num_peaks);
        return 0.08f;
    }

    // 2. Wind turbulence / breath noise / mechanical bump (sub-220 Hz, bin < 14)
    if (peak_bin < 14) {
        Serial.printf("   🔍 [Acoustic Filter] Rejected: Low-Frequency Noise / Wind Rumble (PeakBin: %d)\n", peak_bin);
        return 0.08f;
    }

    // 3. Diffuse broadband ambient noise / TV / background speech (too many peaks or flat power distribution)
    if (num_peaks > 12 || top3_ratio < 0.08f) {
        Serial.printf("   🔍 [Acoustic Filter] Rejected: Broadband Ambient Noise / Chatter (Peaks: %d, Top-3: %.1f%%)\n",
                      num_peaks, top3_ratio * 100.0f);
        return 0.10f;
    }

    // --- INFANT CRY SCORING ---
    // Genuine infant vocal cry characteristics:
    // - Peak in infant vocal range (bins 18 to 45, ~300 Hz - 1000 Hz)
    // - Harmonic formant density (3 to 7 peaks)
    // - Natural vocal formant dispersion (top-3 ratio between 10% and 45%)
    // - Strong sustained vocal energy in both fundamental and formant bands
    float cry_prob = 0.10f;

    // Vocal resonance band check
    if (peak_bin >= 18 && peak_bin <= 45) {
        cry_prob += 0.35f;
    } else if (peak_bin >= 14 && peak_bin <= 55) {
        cry_prob += 0.20f;
    }

    // Harmonic peak structure
    if (num_peaks >= 3 && num_peaks <= 7) {
        cry_prob += 0.35f;
    } else if (num_peaks == 2 || num_peaks == 8) {
        cry_prob += 0.20f;
    }

    // Spectral dispersion within infant vocal tract profile
    if (top3_ratio >= 0.10f && top3_ratio <= 0.45f) {
        cry_prob += 0.15f;
    }

    // Energy check in acoustic bands
    if (f0_energy > -40.0f && formant_energy > -45.0f) {
        cry_prob += 0.10f;
    }

    cry_prob = constrain(cry_prob, 0.05f, 0.96f);
    Serial.printf("   📊 [Acoustic Discrimination Model] Continuous Cry Probability = %.1f%%\n", cry_prob * 100.0f);
    return cry_prob;
}

// ======================================================
// REAL DSP Spectrogram & ML Inference Pipeline
// ======================================================
void run_pipeline_on_reassembled_audio() {
    uint32_t t_pipeline_start = millis();

    // ─── STAGE 1: Convert int16 PCM to normalized float [-1.0, 1.0] with DC Removal ───
    uint32_t t0 = millis();
    int total_samples = AUDIO_SAMPLE_RATE * AUDIO_DURATION_SEC; // 32,000

    float dc_sum = 0.0f;
    for (int i = 0; i < total_samples; i++) {
        float sample = (float)reassembly_pcm_buffer[i] / 32768.0f;
        pcm_float_buffer[i] = sample;
        dc_sum += sample;
    }
    float dc_offset = dc_sum / (float)total_samples;
    for (int i = 0; i < total_samples; i++) {
        pcm_float_buffer[i] -= dc_offset;
    }
    uint32_t t_convert = millis() - t0;

    // ─── STAGE 2: ESP-DSP Log-Mel Spectrogram Extraction ───
    t0 = millis();
    dsp_extractor.compute_log_mel(pcm_float_buffer, mel_spectrogram_features);
    uint32_t t_dsp = millis() - t0;

    Serial.printf("   📊 DSP Pipeline: PCM→Float (DC-removed, bias: %+.3f): %ums, Log-Mel Extraction: %ums\n",
                  dc_offset, t_convert, t_dsp);

    // ─── STAGE 3: Audio Inference & Classification ───
    float cry_probability = 0.0f;

#if ENABLE_TFLITE
    if (tflite_ready && model_input && model_output) {
        t0 = millis();

        // Sentence-level normalization (match training pipeline: zero-mean)
        float feature_sum = 0.0f;
        for (int i = 0; i < DSP_TOTAL_FEATURES; i++) {
            feature_sum += mel_spectrogram_features[i];
        }
        float feature_mean = feature_sum / (float)DSP_TOTAL_FEATURES;
        for (int i = 0; i < DSP_TOTAL_FEATURES; i++) {
            mel_spectrogram_features[i] -= feature_mean;
        }

        // Copy normalized features into TFLite input tensor
        float* input_data = model_input->data.f;
        if (input_data) {
            memcpy(input_data, mel_spectrogram_features, DSP_TOTAL_FEATURES * sizeof(float));
        }

        TfLiteStatus invoke_status = interpreter->Invoke();
        uint32_t t_inference = millis() - t0;

        if (invoke_status == kTfLiteOk) {
            float* output_data = model_output->data.f;
            cry_probability = output_data[0];
            if (cry_probability < 0.0f) {
                float exp0 = expf(output_data[0]);
                float exp1 = expf(output_data[1]);
                cry_probability = exp0 / (exp0 + exp1);
            }
            Serial.printf("   🧠 TFLite Inference: %ums | Cry: %.1f%% | Non-Cry: %.1f%%\n",
                          t_inference, cry_probability * 100.0f, (1.0f - cry_probability) * 100.0f);
        } else {
            Serial.println("   ❌ TFLite Invoke() failed! Using acoustic engine fallback.");
            cry_probability = evaluate_spectral_cry_probability(mel_spectrogram_features);
        }
    } else {
#endif
        // Real-Time DSP Spectral Infant Cry Discrimination Engine
        cry_probability = evaluate_spectral_cry_probability(mel_spectrogram_features);
#if ENABLE_TFLITE
    }
#endif

    uint32_t t_total = millis() - t_pipeline_start;
    Serial.printf("🧠 [ML RESULT] Cot %d Cry Probability: %.1f%% (Total Pipeline: %ums)\n",
                  expected_node, cry_probability * 100.0f, t_total);

    // ─── STAGE 4: Decision & Wearable Alert ───
    if (cry_probability >= CONFIRMATION_PROB) {
        Serial.printf("🚨🚨🚨 [GENUINE INFANT CRY CONFIRMED AT COT %d] 🚨🚨🚨\n", expected_node);

        // Enter cooldown to prevent alert flooding
        in_cooldown = true;
        last_detection_time = millis();
        last_alert_cot = expected_node;
        last_alert_conf = cry_probability;

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
#if ENABLE_BLE
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
#else
    Serial.println("⚠️ Warning: Caregiver Wearable BLE alerts bypassed (ENABLE_BLE = 0).");
#endif
}
