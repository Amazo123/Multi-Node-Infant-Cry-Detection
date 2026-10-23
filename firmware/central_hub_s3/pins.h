/**
 * @file pins.h
 * @brief Centralised Pin Definitions for the Central Intelligence Hub (ESP32-S3-DevKitC-1 N16R8).
 *
 * HARDWARE TARGET: ESP32-S3-DevKitC-1 N16R8
 *                  (Dual-Core Xtensa LX7 @ 240 MHz, 16 MB Flash, 8 MB Octal PSRAM)
 * QUANTITY:        1 unit
 *
 * This file is the single source of truth for every GPIO assignment on the
 * central hub board.  All hub firmware files should #include this header
 * instead of hard-coding magic pin numbers.
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ GPIO │ Function             │ Dir  │ Drive Logic  │ Peripheral         │
 * ├──────┼──────────────────────┼──────┼──────────────┼────────────────────┤
 * │  10  │ TFT Chip Select (CS) │ OUT  │ Active-LOW   │ ILI9341 SPI TFT    │
 * │  11  │ TFT SPI MOSI (SDI)   │ OUT  │ SPI2 HW Bus  │ ILI9341 SPI TFT    │
 * │  12  │ TFT SPI Clock (SCK)  │ OUT  │ SPI2 HW Bus  │ ILI9341 SPI TFT    │
 * │  13  │ TFT Data/Command(DC) │ OUT  │ Digital      │ ILI9341 SPI TFT    │
 * │  14  │ TFT Hardware Reset   │ OUT  │ Digital      │ ILI9341 SPI TFT    │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║ CRITICAL:  OCTAL PSRAM RESERVED GPIO — DO NOT USE!                  ║
 * ║                                                                     ║
 * ║ On the ESP32-S3-WROOM-1 N16R8, GPIO 33, 34, 35, 36, and 37 are     ║
 * ║ internally dedicated to the 8 MB Octal PSRAM data bus.  Connecting  ║
 * ║ anything external to these pins will cause PSRAM cache collisions,  ║
 * ║ memory corruption, and hard faults.                                 ║
 * ║                                                                     ║
 * ║ All ILI9341 TFT wiring is deliberately routed to GPIO 10-14 to     ║
 * ║ stay well clear of the PSRAM bus.                                   ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 *
 * WIRELESS INTERFACES (No external GPIO wiring required):
 *   - ESP-NOW (Wi-Fi STA mode):  Internal radio, no external connections.
 *   - BLE GATT Server:           Internal radio, no external connections.
 *     BLE Device Name: "INFANT_MONITOR_HUB"
 *     Service UUID:    "19b10000-e8f2-537e-4f6c-d104768a1214"
 *     Alert Char UUID: "19b10001-e8f2-537e-4f6c-d104768a1214"
 */

#ifndef HUB_PINS_H
#define HUB_PINS_H

// ═══════════════════════════════════════════════════════════════════════
//  ILI9341 2.4" 240×320 SPI Color TFT Display (SPI2 Hardware Bus)
// ═══════════════════════════════════════════════════════════════════════
#define TFT_CS                10      // GPIO 10 → Chip Select (Active-LOW)
#define TFT_MOSI              11      // GPIO 11 → SPI2 Master-Out-Slave-In (SDI)
#define TFT_CLK               12      // GPIO 12 → SPI2 Serial Clock (SCK)
#define TFT_DC                13      // GPIO 13 → Data / Command selection (RS)
#define TFT_RST               14      // GPIO 14 → Hardware Reset

// TFT Backlight:
//   Wiring: TFT LED/BLK pin → 100 Ω resistor → 3.3 V
//   (Always on; no GPIO-controlled PWM dimming in this design.)

// TFT MISO / SDO:
//   Leave unconnected (NC).  The display is write-only in this design;
//   no pixel read-back is performed.

// ═══════════════════════════════════════════════════════════════════════
//  PSRAM Bus — RESERVED PINS (DO NOT CONNECT ANYTHING)
// ═══════════════════════════════════════════════════════════════════════
//  GPIO 33 — PSRAM Data 0  (INTERNAL)
//  GPIO 34 — PSRAM Data 1  (INTERNAL)
//  GPIO 35 — PSRAM Data 2  (INTERNAL)
//  GPIO 36 — PSRAM Data 3  (INTERNAL)
//  GPIO 37 — PSRAM Data 4  (INTERNAL)

#endif // HUB_PINS_H
