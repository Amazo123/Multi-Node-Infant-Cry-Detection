/**
 * @file 04_node_led_battery_test.ino
 * @brief Standalone Hardware Diagnostic: Cot Node LEDs & Battery Divider Checkout.
 * 
 * Target Board: ESP32-C3-DevKitC-02
 * Hardware Connections:
 *   - GPIO 1: Battery ADC (100k + 100k voltage divider to Li-Po +, with 100nF capacitor to GND)
 *   - GPIO 4: RED LED (Battery Status, Active-HIGH: GPIO4 -> 330R -> Anode -> Cathode -> GND)
 *   - GPIO 9: BLUE LED (ESP-NOW Link, Active-LOW: 3.3V -> 330R -> Anode -> Cathode -> GPIO9)
 * 
 * Test Routine:
 * 1. Reads raw ADC and calibrated millivolts on GPIO 1 every 500ms.
 * 2. Multiplies by 2.0 to calculate estimated VBAT.
 * 3. Cycles Red LED through all 3 battery indicator modes:
 *      Phase 1 (5s): High Battery Sim -> Red LED Solid ON
 *      Phase 2 (5s): Medium Battery Sim -> Red LED Slow Blink (500ms)
 *      Phase 3 (5s): Low Battery Sim -> Red LED Rapid Alert Blink (150ms)
 * 4. Toggles Blue LED (Active-LOW) to confirm wireless link visual feedback.
 */

#include <Arduino.h>

#define PIN_BATTERY_ADC   1
#define PIN_LED_RED       4
#define PIN_LED_BLUE      9

// Active-LOW helper macros for GPIO 9
#define BLUE_LED_ON()     digitalWrite(PIN_LED_BLUE, LOW)
#define BLUE_LED_OFF()    digitalWrite(PIN_LED_BLUE, HIGH)

// Active-HIGH helper macros for GPIO 4
#define RED_LED_ON()      digitalWrite(PIN_LED_RED, HIGH)
#define RED_LED_OFF()     digitalWrite(PIN_LED_RED, LOW)

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    delay(500);

    Serial.println("\n==============================================");
    Serial.println("  COT NODE LED & BATTERY ADC CHECKOUT TOOL   ");
    Serial.println("==============================================");

    // Initialize LED Pins
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);

    // Set initial safe states:
    RED_LED_OFF();
    BLUE_LED_OFF(); // HIGH because active-LOW

    // Configure ADC on GPIO 1
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db); // Full-scale range up to ~2.6V (safe for up to 5V battery with /2 divider)

    Serial.println("✅ GPIO configured:");
    Serial.println("   - GPIO 1: ADC 12-bit, 11dB attenuation");
    Serial.println("   - GPIO 4: RED LED (Active-HIGH)");
    Serial.println("   - GPIO 9: BLUE LED (Active-LOW)");
    Serial.println("----------------------------------------------");
}

void loop() {
    // 1. Read Actual Battery ADC
    uint32_t adc_mv = analogReadMilliVolts(PIN_BATTERY_ADC);
    float vbat_measured = (adc_mv * 2.0f) / 1000.0f; // 2x divider multiplier

    Serial.println("\n--- [BATTERY VOLTAGE MEASUREMENT] ---");
    Serial.printf("Raw Pin ADC: %u mV | Estimated VBAT: %.3f V\n", adc_mv, vbat_measured);

    if (vbat_measured >= 3.80f) {
        Serial.println("🔋 Battery Assessment: HIGH (>= 3.8V)");
    } else if (vbat_measured >= 3.40f) {
        Serial.println("🔋 Battery Assessment: MEDIUM (3.4V - 3.8V)");
    } else {
        Serial.println("⚠️ Battery Assessment: LOW (< 3.4V) - Recharging Needed");
    }

    // 2. Exercise Visual Indicator States
    Serial.println("\n[LED DEMO PHASE 1] Testing HIGH Battery Mode: Red Solid ON + Blue Solid ON (Active-LOW)...");
    RED_LED_ON();
    BLUE_LED_ON();
    delay(3000);

    Serial.println("[LED DEMO PHASE 2] Testing MEDIUM Battery Mode: Red Slow Blink (500ms)...");
    BLUE_LED_OFF();
    for (int i = 0; i < 6; i++) {
        RED_LED_ON();
        delay(250);
        RED_LED_OFF();
        delay(250);
    }

    Serial.println("[LED DEMO PHASE 3] Testing LOW Battery Mode: Red Rapid Blink (150ms) + Blue Blinking...");
    for (int i = 0; i < 10; i++) {
        RED_LED_ON();
        BLUE_LED_ON();
        delay(150);
        RED_LED_OFF();
        BLUE_LED_OFF();
        delay(150);
    }

    Serial.println("✅ One full diagnostic cycle complete. Repeating in 2 seconds...");
    delay(2000);
}
