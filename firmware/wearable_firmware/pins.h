/**
 * @file pins.h
 * @brief Centralised Pin Definitions for the Caregiver Wearable Wristband (ESP32-C3 SuperMini).
 *
 * HARDWARE TARGET: ESP32-C3-MINI-1 / ESP32-C3 SuperMini
 *                  (Single-Core RISC-V @ 160 MHz, 4 MB Flash)
 * QUANTITY:        1 unit
 *
 * This file is the single source of truth for every GPIO assignment on the
 * wearable wristband.  All wearable firmware files should #include this
 * header instead of hard-coding magic pin numbers.
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ GPIO │ Function             │ Dir  │ Drive Logic  │ Peripheral         │
 * ├──────┼──────────────────────┼──────┼──────────────┼────────────────────┤
 * │   3  │ MOSFET Gate (Motor)  │ OUT  │ Active-HIGH  │ Vibration Motor    │
 * │   4  │ I2C SDA              │ I/O  │ Open-Drain   │ SSD1306 OLED       │
 * │   5  │ I2C SCL              │ OUT  │ Open-Drain   │ SSD1306 OLED       │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * WIRELESS INTERFACES (No external GPIO wiring required):
 *   - BLE GATT Client:  Internal radio, no external connections.
 *     Device Name:      "CAREGIVER_BAND"
 *     Connects to hub:  "INFANT_MONITOR_HUB"
 *     Service UUID:     "19b10000-e8f2-537e-4f6c-d104768a1214"
 *     Alert Char UUID:  "19b10001-e8f2-537e-4f6c-d104768a1214"
 */

#ifndef WEARABLE_PINS_H
#define WEARABLE_PINS_H

// ═══════════════════════════════════════════════════════════════════════
//  Haptic Vibration Motor (Low-Side N-MOSFET Switch)
// ═══════════════════════════════════════════════════════════════════════
//  MOSFET: IRLML2502TR or AO3400A (SOT-23, logic-level N-Channel)
//
//  Schematic:
//              3.3 V
//                │
//                ├────┐
//                │    │
//              [+]  [K]  1N5819 Schottky Flyback Diode
//            MOTOR  │    (Cathode stripe → 3.3 V, Anode → Drain)
//              [-]  [A]
//                │    │
//                ├────┘
//                │
//                ▼ Drain (D)
//         ┌─────────────┐
//  GPIO 3 ──[100 Ω]──► Gate (G)│  N-MOSFET (SOT-23)
//         │             │
//       [10 kΩ]   Source (S)
//         │             │
//        GND           GND
//
//  HIGH → Motor ON, LOW → Motor OFF
#define MOTOR_PIN             3       // GPIO 3 → MOSFET Gate (via 100 Ω series R)

// ═══════════════════════════════════════════════════════════════════════
//  SSD1306 0.96" 128×64 Monochrome I2C OLED Display
// ═══════════════════════════════════════════════════════════════════════
//  I2C Address: 0x3C (default, some modules use 0x3D)
//  Pull-ups:    4.7 kΩ from SDA to 3.3 V, 4.7 kΩ from SCL to 3.3 V
//               (Many breakout boards include these on-board; check before
//                adding external pull-ups to avoid I2C bus overloading.)
#define I2C_SDA_PIN           4       // GPIO 4 → I2C Data (SDA)
#define I2C_SCL_PIN           5       // GPIO 5 → I2C Clock (SCL)

#define OLED_I2C_ADDRESS      0x3C    // SSD1306 default I2C address
#define SCREEN_WIDTH          128     // OLED pixel width
#define SCREEN_HEIGHT         64      // OLED pixel height
#define OLED_RESET            -1      // No dedicated reset pin (uses software reset)

#endif // WEARABLE_PINS_H
