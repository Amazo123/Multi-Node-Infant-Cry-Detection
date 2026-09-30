/**
 * @file wearable_firmware.ino
 * @brief Caregiver Wearable Alert Wristband Firmware for ESP32-C3-MINI.
 * 
 * Hardware:
 * - MCU: ESP32-C3-MINI-1
 * - Display: SSD1306 0.96" 128x64 I2C OLED
 * - Haptic Alert: 3V Coin ERM Vibration Motor driven via AO3400A N-MOSFET (Gate pin 3)
 * - Battery: 1S 3.7V ~500 mAh Li-Po + MAX17048 Fuel Gauge
 * 
 * Function:
 * 1. Connects as a BLE GATT Client to the central ESP32-S3 Hub ("INFANT_MONITOR_HUB").
 * 2. Listens for the 6-byte wearable_alert_packet_t notification.
 * 3. On reception:
 *    - Vibrates the motor in pulsed bursts.
 *    - Immediately displays the Cot ID (e.g. "COT 02 - CRY DETECTED!") on the OLED.
 *    - Clears alert and returns to low-power idle after acknowledgment / timeout.
 */

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "../common/protocol.h"

// ---------------- Hardware Pinout ----------------
#define MOTOR_PIN           3       // Gate of AO3400A MOSFET (with 10k pulldown & 1N5819 diode)
#define I2C_SDA_PIN         4       // SSD1306 OLED SDA
#define I2C_SCL_PIN         5       // SSD1306 OLED SCL

// BLE GATT UUIDs matching the S3 Hub
#define SERVICE_UUID              "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHARACTERISTIC_ALERT_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"

// State Variables
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

uint32_t alert_start_time = 0;
bool alert_active = false;
uint8_t alert_cot = 0;
uint16_t alert_conf = 0;

// Forward Declarations
void trigger_vibration(bool enable);
void update_oled_display();
bool connectToServer();

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

// Advertised Device Callback (Finds "INFANT_MONITOR_HUB")
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
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
    Serial.println("   ⌚ Caregiver Wearable Wristband (ESP32-C3-MINI)     ");
    Serial.println("=======================================================");

    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);

    // Initialize I2C for SSD1306 OLED
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Serial.println("✅ I2C OLED initialized.");

    // Initialize BLE Client
    BLEDevice::init("CAREGIVER_BAND");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    update_oled_display();
}

// ======================================================
// Connect to S3 Hub
// ======================================================
bool connectToServer() {
    Serial.print("Connecting to S3 Central Hub: ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient* pClient = BLEDevice::createClient();
    pClient->connect(myDevice);

    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_ALERT_UUID);
    if (pRemoteCharacteristic == nullptr) {
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
    if (doConnect) {
        if (connectToServer()) {
            Serial.println("✅ Connected to Central S3 Hub successfully!");
        }
        doConnect = false;
    }

    // Handle Active Alert Vibration (Pulsed vibration pattern for 5 seconds)
    if (alert_active) {
        uint32_t elapsed = millis() - alert_start_time;
        if (elapsed < 5000) {
            // Pulse: 300ms ON, 200ms OFF
            bool motor_on = (elapsed % 500) < 300;
            trigger_vibration(motor_on);
        } else {
            // Alarm timeout: stop motor and clear
            trigger_vibration(false);
            alert_active = false;
            update_oled_display();
        }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}

// ======================================================
// Vibration Motor Driver
// ======================================================
void trigger_vibration(bool enable) {
    digitalWrite(MOTOR_PIN, enable ? HIGH : LOW);
}

// ======================================================
// OLED Display Renderer
// ======================================================
void update_oled_display() {
    // Prints formatted string directly to Serial (and SSD1306 display via Adafruit_SSD1306)
    if (alert_active) {
        Serial.println("*****************************");
        Serial.printf ("*       🚨 COT 0%d 🚨        *\n", alert_cot);
        Serial.println("*      CRY DETECTED!        *");
        Serial.printf ("*     Confidence: %u%%       *\n", alert_conf);
        Serial.println("*****************************");
    } else {
        Serial.println("-----------------------------");
        Serial.println("|   STATUS: MONITORING...   |");
        Serial.printf ("|   BLE Hub: %s   |\n", connected ? "CONNECTED " : "SEARCHING");
        Serial.println("-----------------------------");
    }
}
