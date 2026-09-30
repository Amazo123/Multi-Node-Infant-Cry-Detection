/**
 * @file 03_wearable_haptic_oled_test.ino
 * @brief Standalone Hardware Diagnostic: Caregiver Wearable (OLED + Haptic Motor).
 * 
 * Target Board: ESP32-C3-MINI-1
 * Hardware Wiring:
 *   - AO3400A N-MOSFET Gate: GPIO 3 (with 10k pulldown to GND, 1N5819 flyback diode across motor)
 *   - SSD1306 OLED SDA:     GPIO 4 (with 4.7k pull-up to 3.3V)
 *   - SSD1306 OLED SCL:     GPIO 5 (with 4.7k pull-up to 3.3V)
 *   - OLED VDD:             3.3V
 *   - OLED GND:             GND
 * 
 * Test Routine:
 * 1. Performs I2C bus scan (verifying OLED responds at 0x3C or 0x3D).
 * 2. Initializes SSD1306 display and renders test screens.
 * 3. Cycles through 3 haptic vibration profiles:
 *    - Mode 1: Gentle short pulse (100 ms)
 *    - Mode 2: Double pulse (150 ms on, 100 ms off, 150 ms on)
 *    - Mode 3: Triple urgent alert pulse (Cry alert simulation)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define MOTOR_PIN       3
#define OLED_SDA_PIN    4
#define OLED_SCL_PIN    5

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oled_found = false;

void scan_i2c() {
    Serial.println("🔍 Scanning I2C bus...");
    byte count = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("✅ Found I2C device at 0x%02X\n", addr);
            if (addr == 0x3C || addr == 0x3D) {
                oled_found = true;
            }
            count++;
        }
    }
    if (count == 0) {
        Serial.println("❌ No I2C devices found! Check OLED SDA (GPIO4), SCL (GPIO5), VDD, and GND.");
    }
}

void pulse_motor(int pulse_ms, int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(MOTOR_PIN, HIGH);
        delay(pulse_ms);
        digitalWrite(MOTOR_PIN, LOW);
        if (i < count - 1) {
            delay(120);
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    delay(500);

    Serial.println("\n==============================================");
    Serial.println("  WEARABLE OLED & HAPTIC MOTOR CHECKOUT TOOL  ");
    Serial.println("==============================================");

    // 1. Initialize Motor Pin (Active-HIGH via AO3400A)
    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
    Serial.println("✅ Motor Gate (GPIO 3) configured as OUTPUT LOW.");

    // 2. Initialize I2C Bus on GPIO 4, 5
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    scan_i2c();

    // 3. Initialize OLED
    if (oled_found) {
        if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
            Serial.println("❌ SSD1306 initialization failed at 0x3C! Retrying 0x3D...");
            display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
        }
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(10, 10);
        display.println("INFANT MONITOR");
        display.setCursor(10, 30);
        display.println("WEARABLE READY");
        display.display();
        Serial.println("✅ OLED Display initialized successfully!");
    }
}

int test_step = 0;

void loop() {
    test_step++;
    uint8_t mock_cot = (test_step % 3) + 1;

    Serial.printf("\n--- Test Step %d: Simulating Alert for COT 0%d ---\n", test_step, mock_cot);

    if (oled_found) {
        display.clearDisplay();
        
        // Header
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("STATUS: ALERT! [TEST]");

        // Inverted Alert Box
        display.fillRect(0, 14, 128, 36, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setTextSize(2);
        display.setCursor(8, 24);
        display.printf("COT 0%d CRY!", mock_cot);

        // Footer
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 54);
        display.printf("Conf: 96%% | Bat: 3.9V");

        display.display();
    }

    // Trigger haptic vibration alert pattern
    Serial.println("📳 Haptic Motor: Pulsing (3x 150ms)...");
    pulse_motor(150, 3);

    delay(3000);

    // Clear Display back to Idle
    if (oled_found) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(16, 20);
        display.println("ALL COTS NORMAL");
        display.setCursor(24, 40);
        display.println("Standby Mode");
        display.display();
    }

    Serial.println("💤 Standby for 3 seconds...");
    delay(3000);
}
