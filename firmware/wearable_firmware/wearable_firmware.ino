/**
 * @file wearable_firmware.ino
 * @brief Caregiver Wearable Alert Wristband Firmware for ESP32-C3 SuperMini.
 * 
 * Hardware:
 * - MCU: ESP32-C3 SuperMini (Single-Core RISC-V @ 160 MHz)
 * - Display: SSD1306 0.96" 128x64 I2C OLED
 * - Haptic Alert: 3V Coin ERM Vibration Motor driven via IRLML2502TR N-MOSFET (Gate GPIO 3)
 * - Battery: 1S 3.7V ~500 mAh Li-Po
 * 
 * Function:
 * 1. Connects as a BLE GATT Client to the central ESP32-S3 Hub ("INFANT_MONITOR_HUB").
 * 2. Listens for the 6-byte wearable_alert_packet_t notification.
 * 3. On reception:
 *    - Vibrates the motor with PWM-graduated intensity based on cry confidence.
 *    - Immediately displays the Cot ID (e.g. "COT 02 - CRY DETECTED!") on the OLED.
 *    - Clears alert and returns to monitoring after 8 seconds.
 * 4. Auto-reconnects BLE if the hub connection drops (range exceeded, hub reset).
 * 5. Includes hardware watchdog timer for crash recovery.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_task_wdt.h>
#include "../common/protocol.h"
#include "pins.h"

// BLE GATT UUIDs matching the S3 Hub
#define SERVICE_UUID              "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHARACTERISTIC_ALERT_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"

// Timing Constants
#define ALERT_DURATION_MS         8000    // How long the alert stays active (vibrating + display)
#define BLE_RESCAN_INTERVAL_MS    10000   // Retry BLE scan every 10 seconds if disconnected
#define WDT_TIMEOUT_SEC           30      // Hardware watchdog timeout

// PWM Configuration for Graduated Haptic Feedback
#define MOTOR_PWM_CHANNEL         0
#define MOTOR_PWM_FREQ            1000    // 1 kHz PWM frequency
#define MOTOR_PWM_RESOLUTION      8       // 8-bit resolution (0-255)

// ======================================================
// SSD1306 OLED Display Object
// ======================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// State Variables
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static BLEAdvertisedDevice* myDevice = nullptr;
static BLEClient* pClient = nullptr;

uint32_t alert_start_time = 0;
bool alert_active = false;
uint8_t alert_cot = 0;
uint16_t alert_conf = 0;
uint32_t last_scan_time = 0;

// Forward Declarations
void set_motor_intensity(uint8_t duty_cycle);
void update_oled_display();
bool connectToServer();

// ======================================================
// BLE Client Callbacks (Detect Disconnection)
// ======================================================
class MyClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pClient) {
        connected = true;
        Serial.println("✅ [BLE] Connected to Central Hub!");
    }
    void onDisconnect(BLEClient* pClient) {
        connected = false;
        pRemoteCharacteristic = nullptr;
        Serial.println("⚠️ [BLE] Disconnected from Central Hub. Will auto-reconnect...");
        update_oled_display();
    }
};

// ======================================================
// BLE Notification Callback (Triggered when S3 sends Alert)
// ======================================================
static void notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify
) {
    if (length == sizeof(wearable_alert_packet_t)) {
        wearable_alert_packet_t* alert = (wearable_alert_packet_t*)pData;
        alert_cot = alert->cot_id;
        alert_conf = alert->confidence_pct;
        alert_active = true;
        alert_start_time = millis();

        Serial.printf("\n🚨 [WRISTBAND ALERT TRIGGERED] COT %d (Confidence: %u%%)\n", alert_cot, alert_conf);
        update_oled_display();
    }
}

// ======================================================
// Advertised Device Callback (Finds "INFANT_MONITOR_HUB")
// ======================================================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = false;
        }
    }
};

// ======================================================
// Setup
// ======================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=======================================================");
    Serial.println("   ⌚ Caregiver Wearable Wristband (ESP32-C3 SuperMini)");
    Serial.println("=======================================================");

    // 1. Initialize PWM for Vibration Motor (Graduated Haptic Feedback)
    ledcSetup(MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(MOTOR_PIN, MOTOR_PWM_CHANNEL);
    ledcWrite(MOTOR_PWM_CHANNEL, 0); // Motor OFF

    // 2. Initialize I2C Bus for SSD1306 OLED Display
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        Serial.println("❌ SSD1306 OLED initialization failed!");
        // Continue without display — alerts still work via haptics
    } else {
        Serial.println("✅ SSD1306 OLED initialized (128x64).");
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();

    // 3. Initialize BLE Client
    BLEDevice::init("CAREGIVER_BAND");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false); // Initial 5-second scan
    last_scan_time = millis();

    // 4. Enable Hardware Watchdog Timer (30s timeout)
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

    // 5. Show initial monitoring screen
    update_oled_display();
    Serial.println("✅ Wearable initialized. Searching for Central Hub...");
}

// ======================================================
// Connect to S3 Hub (with proper callbacks)
// ======================================================
bool connectToServer() {
    Serial.print("📶 Connecting to S3 Central Hub: ");
    Serial.println(myDevice->getAddress().toString().c_str());

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks());
    pClient->connect(myDevice);

    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("❌ Failed to find BLE service!");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_ALERT_UUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("❌ Failed to find alert characteristic!");
        pClient->disconnect();
        return false;
    }

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    connected = true;
    update_oled_display();
    return true;
}

// ======================================================
// Main Event & Haptic Loop
// ======================================================
void loop() {
    // Feed hardware watchdog
    esp_task_wdt_reset();

    // 1. Handle pending BLE connection
    if (doConnect) {
        if (connectToServer()) {
            Serial.println("✅ Connected to Central S3 Hub successfully!");
        } else {
            Serial.println("⚠️ Connection failed. Will retry...");
            doScan = true;
        }
        doConnect = false;
    }

    // 2. Auto-reconnect: Periodically re-scan if disconnected
    if (!connected && !doConnect) {
        uint32_t now = millis();
        if (now - last_scan_time > BLE_RESCAN_INTERVAL_MS) {
            last_scan_time = now;
            Serial.println("🔄 [BLE] Re-scanning for Central Hub...");
            BLEDevice::getScan()->start(5, false);
            update_oled_display(); // Refresh "SEARCHING" screen
        }
    }

    // 3. Handle Active Alert with PWM-Graduated Vibration Pattern
    if (alert_active) {
        uint32_t elapsed = millis() - alert_start_time;
        if (elapsed < ALERT_DURATION_MS) {
            // Calculate PWM duty cycle based on cry confidence
            // Higher confidence = stronger vibration (150-255 range)
            uint8_t base_intensity = map(constrain(alert_conf, 50, 100), 50, 100, 150, 255);

            // Pulse pattern: 350ms ON, 150ms OFF
            bool pulse_on = (elapsed % 500) < 350;
            set_motor_intensity(pulse_on ? base_intensity : 0);

            // Update OLED with flashing effect (toggle every 500ms)
            static uint32_t last_oled_toggle = 0;
            if (millis() - last_oled_toggle > 500) {
                last_oled_toggle = millis();
                update_oled_display();
            }
        } else {
            // Alert timeout: stop motor and return to monitoring
            set_motor_intensity(0);
            alert_active = false;
            update_oled_display();
        }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}

// ======================================================
// PWM Motor Driver (Graduated Haptic Intensity)
// ======================================================
void set_motor_intensity(uint8_t duty_cycle) {
    ledcWrite(MOTOR_PWM_CHANNEL, duty_cycle);
}

// ======================================================
// SSD1306 OLED Display Renderer
// ======================================================
void update_oled_display() {
    display.clearDisplay();

    if (alert_active) {
        // ─── ALERT MODE: Full-Screen Cry Warning ───
        // Inverted header bar
        display.fillRect(0, 0, SCREEN_WIDTH, 18, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setTextSize(2);
        display.setCursor(6, 1);
        display.print("!! ALERT !!");

        // Cot ID in large text
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(2);
        display.setCursor(14, 22);
        display.printf("COT 0%d", alert_cot);

        // "CRY DETECTED" label
        display.setTextSize(1);
        display.setCursor(12, 42);
        display.print("CRY DETECTED!");

        // Confidence bar
        display.setCursor(0, 55);
        display.printf("Confidence: %u%%", alert_conf);
        int bar_w = map(constrain(alert_conf, 0, 100), 0, 100, 0, SCREEN_WIDTH - 4);
        display.drawRect(0, 54, SCREEN_WIDTH, 10, SSD1306_WHITE);
        display.fillRect(2, 56, bar_w, 6, SSD1306_WHITE);

    } else {
        // ─── MONITORING MODE: Status Dashboard ───
        // Header
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("INFANT CRY MONITOR");
        display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

        // Connection status with icon
        display.setCursor(0, 16);
        display.print("Hub: ");
        if (connected) {
            display.print("CONNECTED");
            // Draw signal strength icon
            display.fillRect(110, 14, 3, 8, SSD1306_WHITE);
            display.fillRect(115, 17, 3, 5, SSD1306_WHITE);
            display.fillRect(120, 20, 3, 2, SSD1306_WHITE);
        } else {
            display.print("SEARCHING...");
        }

        // Status message
        display.setCursor(0, 32);
        display.print("Status: MONITORING");
        display.setCursor(0, 44);
        display.print("All cots supervised.");

        // Bottom status bar
        display.drawFastHLine(0, 54, SCREEN_WIDTH, SSD1306_WHITE);
        display.setCursor(0, 56);
        uint32_t uptime_s = millis() / 1000;
        uint32_t mins = uptime_s / 60;
        uint32_t secs = uptime_s % 60;
        display.printf("Uptime: %um %us", mins, secs);
    }

    display.display();

    // Mirror to Serial for debugging
    if (alert_active) {
        Serial.printf("[OLED] ALERT: COT 0%d | Conf: %u%%\n", alert_cot, alert_conf);
    } else {
        Serial.printf("[OLED] Monitoring | Hub: %s\n", connected ? "CONNECTED" : "SEARCHING");
    }
}
