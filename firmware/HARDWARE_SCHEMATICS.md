# 🔌 Complete Hardware Schematic, Pinout & Wiring Blueprint
### Project: Multi-Node Infant Cry Detection System with Wearable Caregiver Alert
**Author:** Amazing Ayomikun Adelowo (22/05/04/0029) | **University of Maiduguri**

This document serves as your **step-by-step physical assembly guide**. When your hardware arrives, follow these exact pin mappings to connect each subsystem without guessing.

---

## 1. Subsystem A: Edge Sensing Nodes (Cot 1, Cot 2, Cot 3)
Each of the 3 cots has an identical dedicated audio node.

### Component Bill of Materials per Node:
* 1× **ESP32-C3-DevKitC-02** (RISC-V single-core @ 160 MHz)
* 1× **INMP441 Digital MEMS Microphone** (I2S interface)
* 1× **Battery Monitoring Voltage Divider** (100kΩ + 100kΩ resistors + 100nF filter capacitor to GPIO1 ADC)
* 1× **RED LED** (GPIO4, Battery Status, Active-HIGH) + 220Ω/330Ω current-limiting resistor
* 1× **BLUE LED** (GPIO9, ESP-NOW Link, Active-LOW) + 330Ω resistor (connected between 3.3V and GPIO9)
* 1× **1S 3.7V ~2000 mAh Li-Po Battery** + TP4056 USB-C Charging Board

### Pin-to-Pin Wiring Table:

```
+-------------------+----------------------+-----------------------------------------------+
| Peripheral Pin    | ESP32-C3 GPIO        | Electrical / Wiring Notes                     |
+-------------------+----------------------+-----------------------------------------------+
| INMP441 VDD       | 3V3                  | Clean 3.3V power                              |
| INMP441 GND       | GND                  | Common Ground                                 |
| INMP441 SCK       | GPIO 5               | I2S Serial Bit Clock (BCLK)                   |
| INMP441 WS        | GPIO 6               | I2S Word Select / Frame Sync (LRCK)           |
| INMP441 SD        | GPIO 7               | I2S Serial Data Out (DIN on ESP32-C3)         |
| INMP441 L/R       | GND                  | Ties microphone to LEFT Audio Channel         |
+-------------------+----------------------+-----------------------------------------------+
| Battery + (VBAT)  | 100kΩ -> GPIO 1      | Voltage divider high side                     |
| GPIO 1 Node       | 100kΩ -> GND         | Voltage divider low side (VADC = VBAT / 2)    |
| GPIO 1 Node       | 100nF -> GND         | Noise filtering capacitor                     |
+-------------------+----------------------+-----------------------------------------------+
| Red LED Anode     | GPIO 4  (via 330Ω)   | Active-HIGH (HIGH=ON). Battery status:        |
|                   |                      | Solid = High, Slow Blink = Med, Fast = Low    |
| Red LED Cathode   | GND                  | Ground                                        |
+-------------------+----------------------+-----------------------------------------------+
| Blue LED Anode    | 3.3V (via 330Ω)      | Active-LOW. 3.3V -> 330Ω -> Anode -> Cathode  |
| Blue LED Cathode  | GPIO 9               | GPIO 9 LOW = ON (Connected), HIGH = OFF       |
|                   |                      | Solid = Linked, Blinking = Disconnected       |
+-------------------+----------------------+-----------------------------------------------+
```

---

## 2. Subsystem B: Central Intelligence Hub (ESP32-S3)
The hub performs cot identification, audio reassembly in PSRAM, Log-Mel extraction, ML inference, and BLE alert dispatch.

### Component Bill of Materials:
* 1× **ESP32-S3-DevKitC-1 N16R8** (16MB Flash, 8MB Octal PSRAM)
* 1× **ILI9341 2.4-inch 240×320 SPI Color TFT Display**
* 1× **1S 3.7V ~3000 mAh Li-Ion / Li-Po Battery** (with TP4056 + Boost)
*(Note: Buzzer removed from design to maintain silent hospital/nursery operation; all alerting routed to wearable and TFT).*

### Pin-to-Pin Wiring Table (SPI2 Hardware Bus):

```
+--------------------+----------------------+-----------------------------------------------+
| ILI9341 TFT Pin    | ESP32-S3 GPIO        | Electrical / Wiring Notes                     |
+--------------------+----------------------+-----------------------------------------------+
| VCC                | 5V (or 3V3)          | Display Power (Use 5V if board has 3.3V LDO)  |
| GND                | GND                  | Common Ground                                 |
| CS                 | GPIO 10              | TFT Chip Select (Active-LOW)                  |
| RESET              | GPIO 14              | Hardware Reset                                |
| DC / RS            | GPIO 13              | Data / Command Selection                      |
| MOSI / SDI         | GPIO 11              | SPI2 Hardware MOSI (High speed)               |
| SCK / CLK          | GPIO 12              | SPI2 Hardware Serial Clock                    |
| LED / BLK          | 3V3 (via 100Ω)       | Backlight Power                               |
| MISO / SDO         | NC (Unconnected)     | Display is write-only; leave unconnected      |
+--------------------+----------------------+-----------------------------------------------+
```

> **CRITICAL OCTAL PSRAM PROTECTION NOTE**:
> On the ESP32-S3-WROOM-1 N16R8, **GPIO 33, 34, 35, 36, and 37 are internally dedicated to the 8MB Octal PSRAM**.
> The ILI9341 TFT wiring above strictly uses **GPIO 10, 11, 12, 13, and 14**, completely protecting the PSRAM bus from conflicts!


---

## 3. Subsystem C: Caregiver Wearable Wristband (ESP32-C3 SuperMini)
A compact battery-powered wristband with OLED display and haptic feedback.

### Component Bill of Materials:
* 1× **ESP32-C3 SuperMini Board** (Compact castellated form factor with single USB-C)
* 1× **SSD1306 0.96-inch 128×64 Monochrome I2C OLED Display**
* 1× **3V Coin ERM Vibration Motor** (~10mm, ~60mA)
* 1× **IRLML2502TR (or AO3400A) N-Channel MOSFET** (SOT-23 logic-level)
* 1× **1N5819 Schottky Diode** (Flyback snubbing diode across motor leads)
* 1× **100Ω Resistor** (Gate damping) + 1× **10kΩ Resistor** (Gate pulldown)
* 1× **1S 3.7V ~500 mAh Li-Po Battery** + 3.3V Low-Dropout Regulator

### Motor Driver Schematic Diagram:

```
                 3.3V (VCC)
                    │
                    ├───┐
                    │   │
                   [+] [K]  1N5819 Schottky Diode
                 MOTOR  │   (Cathode stripe to 3.3V, Anode to Drain)
                   [-] [A]
                    │   │
                    ├───┘
                    │
                    ▼ Drain (D)
             ┌─────────────┐
ESP32-C3     │ IRLML2502TR │
GPIO 3 ──[100Ω]──► Gate (G)│  N-MOSFET (SOT-23)
             │             │
           [10kΩ]   Source (S)
             │      │
            GND    GND
```

### Pin-to-Pin Wiring Table:

```
+--------------------+----------------------+-----------------------------------------------+
| Component Pin      | ESP32-C3 SuperMini   | Function / Wiring Details                     |
+--------------------+----------------------+-----------------------------------------------+
| OLED VCC           | 3V3                  | 3.3V Regulated Power                          |
| OLED GND           | GND                  | Common Ground                                 |
| OLED SDA           | GPIO 4               | Display I2C Data                              |
| OLED SCL           | GPIO 5               | Display I2C Clock                             |
+--------------------+----------------------+-----------------------------------------------+
| MOSFET Gate (via R)| GPIO 3               | 100Ω series resistor to Gate; 10kΩ to GND     |
| Motor Positive (+) | 3V3                  | Connected to 3.3V rail                        |
| Motor Negative (-) | MOSFET Drain         | Switched to GND when GPIO 3 goes HIGH         |
+--------------------+----------------------+-----------------------------------------------+
```

---

## 4. Power Architecture & Decoupling Capacitor Placements

### Power Stages:
1. **Cot Nodes (3 Units)**:
   * 1S 3.7V ~2000 mAh Li-Ion battery $\rightarrow$ **TP4056 USB-C Charger** $\rightarrow$ **5V Boost Converter** $\rightarrow$ `5V/VIN` pin on ESP32-C3.
   * Onboard LDO on ESP32-C3 steps down to stable 3.3V for MCU and INMP441.
2. **Central Hub (1 Unit)**:
   * 1S 3.7V ~3000 mAh Li-Ion battery $\rightarrow$ **TP4056 USB-C Charger** $\rightarrow$ **5V Boost Converter** $\rightarrow$ `5V/VIN` pin on ESP32-S3 + `VCC` on ILI9341 TFT.
3. **Caregiver Wearable (1 Unit)**:
   * 1S 3.7V ~500 mAh Li-Po battery $\rightarrow$ **3.3V Low-Dropout Regulator** (or direct to SuperMini 3.3V LDO).

### Capacitor Placement Rules:
* **INMP441 Microphone Supply Decoupling**: Solder one **100nF ceramic capacitor** directly across `VDD` and `GND` pins of the INMP441 breakout to suppress Wi-Fi/ESP-NOW transmission RF ripple.
* **Battery ADC Filtering**: Connect one **100nF ceramic capacitor** between `GPIO 1` and `GND` on each cot node to eliminate ADC sampling noise.
* **Power Rail Transient Buffering**: Place one **47µF – 100µF electrolytic capacitor** across `5V` and `GND` on the output of the 5V boost converters to absorb sudden RF transmit power surges.

---

## 5. Hardware Verification Checklist Before Powering On
1. **Multimeter Continuity Check**: Verify that `3V3` and `GND` lines are NOT shorted on any board.
2. **Li-Po Polarity**: Standard JST 2.0mm battery connectors often have Red (+) and Black (-) reversed depending on the manufacturer. Always measure with a multimeter before plugging into charging boards!
3. **INMP441 L/R Pin**: Ensure the `L/R` pin is securely grounded to `GND` so the microphone reliably outputs on the left channel as configured in `i2s_config`.
4. **Flyback Diode**: Confirm that the cathode (striped side) of the 1N5819 diode is facing `3.3V`, and the anode is connected to the MOSFET drain.
5. **Octal PSRAM Integrity**: Confirm that pins `GPIO 33, 34, 35, 36, and 37` on the ESP32-S3 are completely open and unconnected.
