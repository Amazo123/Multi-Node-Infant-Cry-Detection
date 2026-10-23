/**
 * @file pins.h
 * @brief Centralised Pin Definitions for Cot Sensing Nodes (ESP32-C3-DevKitC-02).
 *
 * HARDWARE TARGET: ESP32-C3-DevKitC-02 (Single-Core RISC-V @ 160 MHz, 4 MB Flash)
 * QUANTITY:        3 identical units (Cot 1, Cot 2, Cot 3)
 *
 * This file is the single source of truth for every GPIO assignment on the
 * cot node boards.  All firmware source files should #include this header
 * instead of hard-coding magic pin numbers.
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ GPIO │ Function             │ Dir  │ Drive Logic  │ Peripheral         │
 * ├──────┼──────────────────────┼──────┼──────────────┼────────────────────┤
 * │   1  │ Battery ADC          │ IN   │ Analog 12b   │ Resistor Divider   │
 * │   4  │ Red LED              │ OUT  │ Active-HIGH  │ Status LED         │
 * │   5  │ I2S BCLK (SCK)       │ OUT  │ Digital      │ INMP441 Microphone │
 * │   6  │ I2S LRCK (WS)        │ OUT  │ Digital      │ INMP441 Microphone │
 * │   7  │ I2S DIN  (SD)        │ IN   │ Digital      │ INMP441 Microphone │
 * │   9  │ Blue LED             │ OUT  │ Active-LOW   │ Status LED         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * STRAPPING PIN WARNING:
 *   GPIO 9 is a boot-strapping pin on the ESP32-C3.  The Blue LED wiring
 *   (3.3V → 330Ω → Anode → Cathode → GPIO 9) naturally holds the pin
 *   HIGH through the external pull-up, which is the correct state for
 *   normal SPI boot.  Do NOT short GPIO 9 to GND during power-on or the
 *   chip will enter ROM download mode instead of booting the application.
 */

#ifndef NODE_PINS_H
#define NODE_PINS_H

// ═══════════════════════════════════════════════════════════════════════
//  INMP441 I2S MEMS Microphone
// ═══════════════════════════════════════════════════════════════════════
#define PIN_INMP441_SCK       5       // GPIO 5 → I2S Serial Bit Clock (BCLK)
#define PIN_INMP441_WS        6       // GPIO 6 → I2S Word Select / Frame Sync (LRCK)
#define PIN_INMP441_SD        7       // GPIO 7 → I2S Serial Data In (DIN on MCU side)
#define I2S_PORT              I2S_NUM_0

// ═══════════════════════════════════════════════════════════════════════
//  Battery Voltage Monitoring (Resistor Divider ADC)
// ═══════════════════════════════════════════════════════════════════════
//  Circuit: VBAT → 100 kΩ → GPIO 1 → 100 kΩ → GND
//           + 100 nF ceramic cap between GPIO 1 and GND for noise filtering.
//  Formula: VADC = VBAT / 2 → VBAT = VADC × 2
#define PIN_BATTERY_ADC       1       // GPIO 1 → ADC input (12-bit, 11 dB attenuation)

// ═══════════════════════════════════════════════════════════════════════
//  Two-LED Visual Status Indicators
// ═══════════════════════════════════════════════════════════════════════

// RED LED — Battery Health Indicator (Active-HIGH)
//  Wiring: GPIO 4 → 330 Ω → Anode (+) → Cathode (−) → GND
//  States:
//    Solid ON    → High Battery  (≥ 3.8 V)
//    Slow Blink  → Medium Battery (3.4 V – 3.8 V)
//    Fast Blink  → Low Battery   (< 3.4 V)
#define PIN_LED_RED           4       // GPIO 4 → Red LED (Active-HIGH)

// BLUE LED — ESP-NOW Communication Link Indicator (Active-LOW)
//  Wiring: 3.3 V → 330 Ω → Anode (+) → Cathode (−) → GPIO 9
//  States:
//    Solid ON  (GPIO LOW)  → Connected to S3 Hub (heartbeat < 5 s)
//    Blinking  (toggling)  → Disconnected / Searching
#define PIN_LED_BLUE          9       // GPIO 9 → Blue LED (Active-LOW, boot strap!)

// ═══════════════════════════════════════════════════════════════════════
//  Convenience Macros
// ═══════════════════════════════════════════════════════════════════════
#define RED_LED_ON()          digitalWrite(PIN_LED_RED,  HIGH)
#define RED_LED_OFF()         digitalWrite(PIN_LED_RED,  LOW)
#define BLUE_LED_ON()         digitalWrite(PIN_LED_BLUE, LOW)   // Active-LOW
#define BLUE_LED_OFF()        digitalWrite(PIN_LED_BLUE, HIGH)  // Active-LOW

#endif // NODE_PINS_H
