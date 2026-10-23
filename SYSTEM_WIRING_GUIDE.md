# 🔌 Master System Wiring & Hardware Interconnect Guide
### Project: Multi-Node Infant Cry Detection System with Wearable Caregiver Alert
**Author:** Amazing Ayomikun Adelowo (22/05/04/0029) | **Institution:** University of Maiduguri  
**Target Hardware:** 3× ESP32-C3 Cot Nodes, 1× ESP32-S3 Central Hub (N16R8), 1× ESP32-C3 SuperMini Wearable

---

## 📑 Table of Contents
1. [Master System Architecture & Hardware Bill of Materials](#1-master-system-architecture--hardware-bill-of-materials)
2. [Master Pin Assignment Matrix (All Subsystems)](#2-master-pin-assignment-matrix-all-subsystems)
3. [Phase 1: Component Testing Phase (Bench Diagnostics Wiring)](#3-phase-1-component-testing-phase-bench-diagnostics-wiring)
   - [Test 01: INMP441 I2S Microphone Audio Plotter Test](#test-01-inmp441-i2s-microphone-audio-plotter-test)
   - [Test 02: ESP-NOW Multi-Node Wireless Range Probe](#test-02-esp-now-multi-node-wireless-range-probe)
   - [Test 03: Caregiver Wearable Haptic Motor & SSD1306 OLED Test](#test-03-caregiver-wearable-haptic-motor--ssd1306-oled-test)
   - [Test 04: Cot Node Status LEDs & Battery ADC Divider Test](#test-04-cot-node-status-leds--battery-adc-divider-test)
   - [Test 05: Central Hub ILI9341 2.4" SPI TFT Color Display Test](#test-05-central-hub-ili9341-24-spi-tft-color-display-test)
4. [Phase 2: Real Project Execution Wiring (Final Physical Assembly)](#4-phase-2-real-project-execution-wiring-final-physical-assembly)
   - [Subsystem A: Edge Sensing Nodes (Cot 1, Cot 2, Cot 3)](#subsystem-a-edge-sensing-nodes-cot-1-cot-2-cot-3)
   - [Subsystem B: Central Intelligence Hub (ESP32-S3 N16R8)](#subsystem-b-central-intelligence-hub-esp32-s3-n16r8)
   - [Subsystem C: Caregiver Wearable Wristband (ESP32-C3 SuperMini)](#subsystem-c-caregiver-wearable-wristband-esp32-c3-supermini)
   - [Subsystem D: Complete Power Distribution & Decoupling Blueprint](#subsystem-d-complete-power-distribution--decoupling-blueprint)
5. [Critical Hardware Protection & Safety Rules](#5-critical-hardware-protection--safety-rules)
6. [Pre-Flight Commissioning & Verification Checklist](#6-pre-flight-commissioning--verification-checklist)

---

## 1. Master System Architecture & Hardware Bill of Materials

The system operates across **5 independent microcontrollers** organized into 3 distinct functional tiers:

```
[ COT NODE 1 ]              [ COT NODE 2 ]              [ COT NODE 3 ]
ESP32-C3-DevKitC-02          ESP32-C3-DevKitC-02          ESP32-C3-DevKitC-02
INMP441 MEMS Mic             INMP441 MEMS Mic             INMP441 MEMS Mic
Resistor Divider ADC         Resistor Divider ADC         Resistor Divider ADC
Red & Blue Status LEDs       Red & Blue Status LEDs       Red & Blue Status LEDs
       │                            │                            │
       │ 100ms RMS Telemetry        │ 100ms RMS Telemetry        │ 100ms RMS Telemetry
       └────────────────────────────┼────────────────────────────┘
                                    │
                                    ▼ ESP-NOW (2.4 GHz Dedicated PHY)
                       ┌─────────────────────────┐
                       │  CENTRAL INTELLIGENCE   │
                       │           HUB           │
                       │ ESP32-S3-DevKitC-1 N16R8│
                       │ 2.4" ILI9341 SPI TFT    │
                       │ 16MB Flash, 8MB PSRAM   │
                       │ Edge AI Neural Engine   │
                       └────────────┬────────────┘
                                    │
                                    │ BLE GATT Server (<30ms Latency)
                                    ▼
                       ┌─────────────────────────┐
                       │   CAREGIVER WEARABLE    │
                       │   ESP32-C3 SuperMini    │
                       │   SSD1306 0.96" OLED    │
                       │   Haptic Vibration Unit │
                       └─────────────────────────┘
```

### Complete System Bill of Materials (BOM)

| Subsystem | Qty | Component / Module | Interface / Model | Power Requirements |
| :--- | :---: | :--- | :--- | :--- |
| **Cot Nodes (1, 2, 3)** | 3 | **ESP32-C3-DevKitC-02** | Single-Core RISC-V @ 160MHz | 5V via USB/Boost $\rightarrow$ 3.3V LDO |
| | 3 | **INMP441 MEMS Microphone** | Digital I2S (SCK, WS, SD) | 3.3V DC, ~1.4 mA |
| | 6 | **100 kΩ Resistors (1% Tol.)** | 2 per node (Battery Divider) | Passive |
| | 3 | **100 nF Ceramic Capacitors** | 1 per node (ADC Filter) | Passive |
| | 3 | **100 nF Ceramic Capacitors** | 1 per node (Mic Decoupling) | Passive |
| | 3 | **Red LEDs (5mm / 3mm)** | Battery Status (Active-HIGH) | 3.3V via 330 Ω resistor |
| | 3 | **Blue LEDs (5mm / 3mm)** | ESP-NOW Link (Active-LOW) | 3.3V via 330 Ω resistor |
| | 6 | **330 Ω Current-Limit Resistors** | 2 per node (LED Damping) | Passive |
| | 3 | **1S 3.7V ~2000 mAh Li-Po Batteries** | JST-PH 2.0mm Connector | 3.0V – 4.2V discharge |
| | 3 | **TP4056 USB-C Charger Modules** | With DW01A battery protection | 5V USB-C input |
| | 3 | **5V Micro Step-Up Boost Converters** | MT3608 or Mini 5V boost | Input 3.0-4.2V $\rightarrow$ Output 5.0V |
| **Central Hub** | 1 | **ESP32-S3-DevKitC-1 N16R8** | Dual-Core LX7, 16MB Flash, 8MB Octal PSRAM | 5V via USB/Boost $\rightarrow$ 3.3V LDO |
| | 1 | **ILI9341 2.4" SPI TFT Display** | 240×320 Color TFT (SPI2 Bus) | 5V/3.3V VCC, 3.3V LED via 100 Ω |
| | 1 | **100 Ω Resistor** | Backlight current limit | Passive |
| | 1 | **1S 3.7V ~3000 mAh Li-Ion / Li-Po** | High capacity pack | 3.0V – 4.2V discharge |
| | 1 | **TP4056 USB-C Charger Module** | With DW01A protection | 5V USB-C input |
| | 1 | **5V Micro Step-Up Boost Converter** | Powers S3 5V/VIN & TFT VCC | Input 3.0-4.2V $\rightarrow$ Output 5.0V |
| **Caregiver Wearable** | 1 | **ESP32-C3 SuperMini** | Ultra-compact RISC-V board | 3.7V Li-Po $\rightarrow$ onboard 3.3V LDO |
| | 1 | **SSD1306 0.96" I2C OLED Display** | 128×64 Monochrome (I2C 0x3C) | 3.3V DC, ~20 mA |
| | 1 | **3V Coin ERM Vibration Motor** | 10mm flat coin, ~60-80 mA | 3.3V DC switched via MOSFET |
| | 1 | **IRLML2502TR or AO3400A N-MOSFET** | SOT-23 logic-level N-Channel | $V_{\text{GS(th)}} \le 1.2\text{V}$, $I_{\text{D}} > 2\text{A}$ |
| | 1 | **1N5819 Schottky Diode** | Flyback inductive snubber | 40V, 1A fast Schottky |
| | 1 | **100 Ω Resistor** | Gate ringing damping | Passive |
| | 1 | **10 kΩ Resistor** | Gate pulldown resistor | Passive |
| | 1 | **1S 3.7V ~500 mAh Li-Po Battery** | Ultra-lightweight pouch | 3.0V – 4.2V discharge |
| | 1 | **TP4056 Micro/Type-C Charger** | Battery recharging circuit | 5V USB-C input |
| | 1 | **Mini SPDT Slide Switch** | Power ON/OFF toggle | 50V 0.5A |

---

## 2. Master Pin Assignment Matrix (All Subsystems)

Every GPIO assignment in the firmware source code (`pins.h`) is locked and mapped in this single authoritative table:

| Subsystem | GPIO | Pin Function | Electrical Dir | Drive Logic | Connected Peripheral & Circuit Details |
| :--- | :---: | :--- | :---: | :---: | :--- |
| **Cot Nodes (1, 2, 3)** | **GPIO 1** | Battery ADC | IN | Analog 12b | Center tap of $100\text{k}\Omega / 100\text{k}\Omega$ divider + $100\text{nF}$ cap to GND |
| | **GPIO 4** | Red Status LED | OUT | Active-HIGH | GPIO 4 $\rightarrow$ $330\Omega$ $\rightarrow$ Anode (+) $\rightarrow$ Cathode (−) $\rightarrow$ GND |
| | **GPIO 5** | I2S Bit Clock (SCK) | OUT | Digital Clock | Connected to INMP441 `SCK` pin (Serial Clock) |
| | **GPIO 6** | I2S Word Select (WS) | OUT | Digital Frame | Connected to INMP441 `WS` pin (Left/Right Frame Sync) |
| | **GPIO 7** | I2S Serial Data (SD) | IN | Digital Audio | Connected to INMP441 `SD` pin (Master In, Slave Out) |
| | **GPIO 9** | Blue Status LED | OUT | **Active-LOW** | 3.3V $\rightarrow$ $330\Omega$ $\rightarrow$ Anode (+) $\rightarrow$ Cathode (−) $\rightarrow$ GPIO 9 |
| | *GND* | Audio Channel Select | — | Ground | INMP441 `L/R` pin wired directly to GND (Left Channel) |
| **Central Hub (S3)** | **GPIO 10** | TFT Chip Select (CS) | OUT | Active-LOW | Connected to ILI9341 `CS` pin |
| | **GPIO 11** | SPI2 MOSI (SDI) | OUT | SPI2 HW MOSI | Connected to ILI9341 `MOSI` / `SDI` pin (Data Out) |
| | **GPIO 12** | SPI2 Serial Clock (SCK) | OUT | SPI2 HW SCK | Connected to ILI9341 `SCK` / `CLK` pin (Clock) |
| | **GPIO 13** | TFT Data / Command (DC) | OUT | Digital Mode | Connected to ILI9341 `DC` / `RS` pin (Command=0, Data=1) |
| | **GPIO 14** | TFT Reset (RST) | OUT | Active-LOW | Connected to ILI9341 `RESET` / `RST` pin |
| | *3.3V* | Backlight Supply | PWR | Analog Current | Connected to ILI9341 `LED` / `BLK` pin via $100\Omega$ resistor |
| | *NC* | SPI MISO / SDO | — | Unconnected | Leave ILI9341 `MISO` / `SDO` **unconnected** (Write-only) |
| | **GPIO 33-37** | **OCTAL PSRAM BUS** | **INTERNAL** | **LOCKED** | **DO NOT WIRE ANYTHING TO THESE PINS! RESERVED FOR 8MB PSRAM** |
| **Wearable Wristband** | **GPIO 3** | Haptic Motor Gate | OUT | Active-HIGH | Connected to Gate of N-MOSFET via $100\Omega$ series resistor |
| | **GPIO 4** | I2C Data (SDA) | I/O | Open-Drain | Connected to SSD1306 OLED `SDA` pin (requires pull-up) |
| | **GPIO 5** | I2C Clock (SCL) | OUT | Open-Drain | Connected to SSD1306 OLED `SCL` pin (requires pull-up) |

---

## 3. Phase 1: Component Testing Phase (Bench Diagnostics Wiring)

Before assembling permanent housings or soldering custom perfboards, you **must validate each component individually on a breadboard** using the diagnostic firmware in `firmware/diagnostics/`. Follow the wiring below for each test.

---

### Test 01: INMP441 I2S Microphone Audio Plotter Test
* **Firmware Location:** `firmware/diagnostics/01_inmp441_audio_plotter/01_inmp441_audio_plotter.ino`
* **Target Board:** ESP32-C3-DevKitC-02 (or ESP32-S3)
* **Goal:** Verify that the MEMS microphone captures clean, noise-free 16kHz audio and live waveforms respond to acoustic stimulus.

#### Wiring Diagram:
```
   ESP32-C3-DevKitC-02                     INMP441 Microphone Breakout
┌─────────────────────────┐               ┌─────────────────────────────┐
│                     3V3 │──────────────►│ VDD                         │
│                     GND │──────────────►│ GND                         │
│                  GPIO 5 │──────────────►│ SCK (Bit Clock)             │
│                  GPIO 6 │──────────────►│ WS  (Word Select / LRCK)    │
│                  GPIO 7 │◄──────────────│ SD  (Serial Data Out)       │
│                     GND │──────────────►│ L/R (Channel: GND = Left)   │
└─────────────────────────┘               └─────────────────────────────┘
                                                 │ 100nF Ceramic Cap
                                                 └───||─── GND (Decoupling)
```

#### Point-to-Point Wiring Table:
| INMP441 Pin | ESP32-C3 Pin | Wire Color Recommendation | Notes |
| :--- | :--- | :--- | :--- |
| **VDD** | **3V3** | Red | Clean 3.3V power (NEVER connect to 5V, it will destroy the MEMS chip!) |
| **GND** | **GND** | Black | Common Ground |
| **SCK** | **GPIO 5** | Yellow | I2S Serial Clock |
| **WS** | **GPIO 6** | Blue | I2S Word Select (Frame Sync) |
| **SD** | **GPIO 7** | Green | I2S Serial Data input into MCU |
| **L/R** | **GND** | Black | **MANDATORY:** Ground this pin to select Left channel. Do not leave floating! |

#### Verification Steps:
1. Connect ESP32-C3 via USB.
2. Flash `01_inmp441_audio_plotter.ino`.
3. In Arduino IDE, click **Tools $\rightarrow$ Serial Plotter** (Set baud to `115200`).
4. **Pass Criteria:**
   - In a quiet room: Audio waveform oscillates steadily around `0` with low background RMS noise ($\le 0.02$).
   - Speak, clap, or snap fingers: Waveform amplitude spikes immediately and tracks vocal harmonics smoothly without clipping or square-wave distortion.
5. **Common Faults & Fixes:**
   - *Flat line at 0:* Check `VDD` and `GND`. Ensure `SD` is connected to GPIO 7, not GPIO 6.
   - *Constant rail-to-rail oscillation / white noise:* `L/R` pin is floating! Bridge `L/R` directly to `GND`.
   - *Reboot loop:* Add a 100nF ceramic capacitor across `VDD` and `GND` right on the microphone breakout pins.

---

### Test 02: ESP-NOW Multi-Node Wireless Range Probe
* **Firmware Location:** `firmware/diagnostics/02_esp_now_range_probe/`
  - Unit 1: `esp_now_sender_probe.ino` (Flashed to Cot Node ESP32-C3)
  - Unit 2: `esp_now_responder_probe.ino` (Flashed to Central Hub ESP32-S3)
* **Goal:** Measure wireless Packet Delivery Rate (PDR), latency, and RSSI across physical obstacles (drywall, nursery doors, distance up to 20 meters).

#### Wiring Diagram:
```
  [ESP32-C3 Sender Node]                           [ESP32-S3 Responder Hub]
┌─────────────────────────┐                     ┌─────────────────────────┐
│ Power: USB-C 5V         │      2.4 GHz        │ Power: USB-C 5V         │
│ No external sensors     │   ESP-NOW PHY       │ No external sensors     │
│ needed for this test    │ ~ ~ ~ ~ ~ ~ ~ ~ ~ ► │ needed for this test    │
│ Built-in PCB Antenna    │ ◄ ~ ~ ~ ~ ~ ~ ~ ~ ~ │ Built-in PCB Antenna    │
└─────────────────────────┘      ACK Echo       └─────────────────────────┘
```

#### Point-to-Point Wiring Table:
* For both boards: Simply connect via USB-C cable to your PC or power banks. No peripheral wiring is required for this diagnostic!

#### Verification Steps:
1. Flash `esp_now_responder_probe.ino` to the ESP32-S3. Open Serial Monitor (`115200 baud`) to note the MAC address.
2. Flash `esp_now_sender_probe.ino` to the ESP32-C3.
3. Open Serial Monitor on the Sender.
4. Walk the ESP32-C3 away from the S3 across the room and down the hallway.
5. **Pass Criteria:**
   - Round-trip ping time $\le 8\text{ ms}$.
   - Packet Delivery Rate (PDR) $\ge 98\%$ at 10 meters line-of-sight.
   - RSSI stronger than $-80\text{ dBm}$ within a standard two-bedroom / nursery distance.

---

### Test 03: Caregiver Wearable Haptic Motor & SSD1306 OLED Test
* **Firmware Location:** `firmware/diagnostics/03_wearable_haptic_oled_test/03_wearable_haptic_oled_test.ino`
* **Target Board:** ESP32-C3 SuperMini (or ESP32-C3-DevKitM-1)
* **Goal:** Verify I2C bus communication with the 0.96" OLED display and test the N-Channel MOSFET haptic driver circuit without brownout resets.

#### Wiring Diagram:
```
                              +3.3V Rail
                                  │
               ┌──────────────────┴────────────────────┐
               │                                       │
               │                                      [+]
        ┌──────────────┐                            ┌─────┐
        │ SSD1306 OLED │                            │MOTOR│ 3V Coin ERM Motor
        │              │                            │     │ 
        │ VCC      3V3 │                            └─────┘  ▲ 1N5819 Schottky Diode
        │ GND      GND │                               │     │ (Cathode stripe to 3.3V,
        │ SDA   GPIO 4 │                              [-]    │  Anode to Drain)
        │ SCL   GPIO 5 │                               │     │
        └──────────────┘                               ├─────┘
                                                       │ Drain (D)
    ESP32-C3 SuperMini                          ┌──────────────┐
  ┌────────────────────┐                        │ IRLML2502TR  │
  │             GPIO 3 │───[ 100 Ω ]───────────►│ Gate (G)     │ N-MOSFET (SOT-23)
  │                    │         │              │              │
  │                GND │────┐    └──[ 10 kΩ ]──►│ Source (S)   │
  └────────────────────┘    │          │        └──────────────┘
                            │          │               │
                           GND        GND             GND
```

#### Point-to-Point Wiring Table:
| Component & Pin | ESP32-C3 SuperMini Pin | Wire Color | Notes |
| :--- | :--- | :--- | :--- |
| **OLED VCC** | **3V3** | Red | Regulated 3.3V power |
| **OLED GND** | **GND** | Black | Common Ground |
| **OLED SDA** | **GPIO 4** | Blue | I2C Data line (needs 4.7k pullup if not on breakout) |
| **OLED SCL** | **GPIO 5** | Yellow | I2C Clock line (needs 4.7k pullup if not on breakout) |
| **100 Ω Resistor Lead 1** | **GPIO 3** | Orange | Connects directly to MCU GPIO 3 |
| **100 Ω Resistor Lead 2** | **MOSFET Gate (G)** | — | Damping resistor prevents high-frequency ringing |
| **10 kΩ Resistor** | **MOSFET Gate $\rightarrow$ GND** | — | Pulls gate firmly to GND when MCU boots |
| **MOSFET Source (S)** | **GND** | Black | Ground reference for MOSFET switch |
| **MOSFET Drain (D)** | **Motor Negative (−)** | Blue/Black | Switched node |
| **Motor Positive (+)** | **3V3** | Red | Motor supply rail |
| **1N5819 Diode Cathode** | **3V3** | Red | **STRIPE SIDE** must face 3.3V rail! |
| **1N5819 Diode Anode** | **MOSFET Drain (D)** | — | Connected across motor terminals |

#### Verification Steps:
1. Flash `03_wearable_haptic_oled_test.ino`.
2. Open Serial Monitor (`115200 baud`).
3. **Pass Criteria:**
   - I2C Scanner prints: `✅ Found I2C device at 0x3C`.
   - OLED screen lights up, displaying the test splash screen and diagnostic counter.
   - The ERM coin motor cycles through:
     - Profile 1: Gentle short tap (100 ms).
     - Profile 2: Double alert tap (150 ms on, 100 ms off, 150 ms on).
     - Profile 3: Triple urgent alert vibration (simulating cry detection).
   - ESP32-C3 **does NOT reset or crash** during motor spin.
4. **Common Faults & Fixes:**
   - *MCU reboots every time the motor activates:* The motor's inrush current is dropping the 3.3V rail below the ESP32 brownout threshold ($2.8\text{V}$). Add a $10\mu\text{F} - 47\mu\text{F}$ capacitor across 3.3V and GND!
   - *Motor never turns off (always spinning):* Check the 10kΩ pulldown resistor; ensure the Gate is not shorted to 3.3V.
   - *OLED display stays black:* Verify I2C address in code (`0x3C` vs `0x3D`). Check that SDA and SCL are not reversed.

---

### Test 04: Cot Node Status LEDs & Battery ADC Divider Test
* **Firmware Location:** `firmware/diagnostics/04_node_led_battery_test/04_node_led_battery_test.ino`
* **Target Board:** ESP32-C3-DevKitC-02
* **Goal:** Verify that the resistor voltage divider correctly measures battery voltage on GPIO 1 ADC, and test the Two-LED status logic (Red Active-HIGH, Blue Active-LOW).

#### Wiring Diagram:
```
  Li-Po Battery (+)                               ESP32-C3-DevKitC-02
  (3.0V - 4.2V VBAT)                             ┌────────────────────┐
          │                                      │                    │
          ├──[ 100 kΩ ]───┬─────────────────────►│ GPIO 1 (ADC)       │
                          │                      │                    │
                          ├──[ 100 kΩ ]──┐       │                    │
                          │              │       │                    │
                          └───||─────────┤       │                    │
                              100nF Cap  │       │                    │
                                         ▼       │                    │
                                        GND      │                    │
                                                 │                    │
             GND ◄── Cathode [−] ── Anode [+] ───│ GPIO 4 (Red LED)   │
                                    (via 330Ω)   │ (Active-HIGH)      │
                                                 │                    │
           3.3V ─── Anode [+] ── Cathode [−] ───►│ GPIO 9 (Blue LED)  │
                    (via 330Ω)                   │ (Active-LOW)       │
                                                 └────────────────────┘
```

#### Point-to-Point Wiring Table:
| Component / Circuit | ESP32-C3 Pin | Wiring Details |
| :--- | :--- | :--- |
| **Battery Divider High Side** | **GPIO 1** | $100\text{k}\Omega$ resistor connected between Battery (+) and GPIO 1 |
| **Battery Divider Low Side** | **GND** | $100\text{k}\Omega$ resistor connected between GPIO 1 and GND |
| **Filter Capacitor** | **GPIO 1 $\rightarrow$ GND** | $100\text{nF}$ ceramic capacitor in parallel with the low-side resistor |
| **Red LED Anode (+)** | **GPIO 4** | Connected via $330\Omega$ series resistor to GPIO 4 (Active-HIGH) |
| **Red LED Cathode (−)** | **GND** | Connected directly to Ground |
| **Blue LED Anode (+)** | **3.3V** | Connected via $330\Omega$ series resistor to 3.3V rail |
| **Blue LED Cathode (−)** | **GPIO 9** | Connected directly to GPIO 9 (Active-LOW: Pulls LOW to illuminate) |

#### ⚠️ Critical Boot Strapping Warning (GPIO 9):
> **GPIO 9 is a boot strapping pin on the ESP32-C3.**
> - If GPIO 9 is LOW (shorted to GND) during power-up or reset, the chip enters **ROM Serial Download Mode** instead of executing your code!
> - The Blue LED circuit is wired as **Active-LOW** specifically to protect this pin: `3.3V → 330Ω → LED → GPIO 9`.
> - This configuration naturally acts as a **pull-up resistor**, keeping GPIO 9 safely **HIGH** at boot time so the ESP32-C3 boots normally into SPI Flash mode!
> - **Never** wire an LED from GPIO 9 to GND directly.

#### Verification Steps:
1. Flash `04_node_led_battery_test.ino`.
2. Connect a variable power supply or 1S Li-Po battery to the divider input.
3. Open Serial Monitor (`115200 baud`).
4. **Pass Criteria:**
   - Serial Monitor prints: `VADC: 1850 mV | Estimated VBAT: 3700 mV`.
   - Measure actual battery voltage with a multimeter. Estimated VBAT should match multimeter reading within $\pm 50\text{ mV}$.
   - Red LED demonstrates all 3 battery modes:
     - Simulated High ($> 3.8\text{V}$): Solid ON.
     - Simulated Medium ($3.4\text{V} - 3.8\text{V}$): Slow steady blink ($1\text{ Hz}$).
     - Simulated Low ($< 3.4\text{V}$): Rapid urgency blink ($4\text{ Hz}$).
   - Blue LED toggles cleanly: Illuminates brightly when GPIO 9 is driven `LOW`, extinguishes when driven `HIGH`.

---

### Test 05: Central Hub ILI9341 2.4" SPI TFT Color Display Test
* **Firmware Location:** `firmware/diagnostics/05_central_hub_ili9341_test/05_central_hub_ili9341_test.ino`
* **Target Board:** ESP32-S3-DevKitC-1 N16R8
* **Goal:** Verify high-speed SPI2 communication with the ILI9341 color display and confirm zero interference with the onboard 8MB Octal PSRAM.

#### Wiring Diagram:
```
   ESP32-S3-DevKitC-1 N16R8                  ILI9341 2.4" SPI TFT (240x320)
┌───────────────────────────┐               ┌──────────────────────────────┐
│                    5V/VIN │──────────────►│ VCC  (Onboard 3.3V LDO)      │
│                       GND │──────────────►│ GND                          │
│                   GPIO 10 │──────────────►│ CS   (Chip Select)           │
│                   GPIO 14 │──────────────►│ RESET / RST                  │
│                   GPIO 13 │──────────────►│ DC / RS (Data / Command)     │
│                   GPIO 11 │──────────────►│ MOSI / SDI (Data In)         │
│                   GPIO 12 │──────────────►│ SCK / CLK (Clock)            │
│          3V3 ──[ 100 Ω ]──┼──────────────►│ LED / BLK (Backlight)        │
│              (Do NOT use) │               │ MISO / SDO ──► LEAVE OPEN    │
│        GPIO 33,34,35,36,37│               └──────────────────────────────┘
│       [OCTAL PSRAM LOCKED]│
└───────────────────────────┘
```

#### Point-to-Point Wiring Table:
| ILI9341 Display Pin | ESP32-S3 Pin | Wire Color | Electrical Notes |
| :--- | :--- | :--- | :--- |
| **VCC** | **5V (or VIN)** | Red | Connect to 5V (most ILI9341 red breakout boards have an onboard 3.3V LDO) |
| **GND** | **GND** | Black | Common Ground |
| **CS** | **GPIO 10** | White | TFT Chip Select (Active-LOW) |
| **RESET / RST** | **GPIO 14** | Brown | Hardware Reset (Active-LOW) |
| **DC / RS** | **GPIO 13** | Orange | Data/Command selector (LOW=Command, HIGH=Data) |
| **MOSI / SDI** | **GPIO 11** | Green | SPI2 Hardware Master-Out-Slave-In |
| **SCK / CLK** | **GPIO 12** | Yellow | SPI2 Hardware Clock |
| **LED / BLK** | **3V3 via 100 Ω** | Red/Orange | Backlight LED. A 100 Ω resistor limits current to ~20 mA |
| **MISO / SDO** | **NC (No Connect)**| — | **DO NOT CONNECT.** Display is write-only. Leaving floating avoids bus contention |

#### ⚠️ Critical Octal PSRAM Isolation Rule:
> **The ESP32-S3-WROOM-1 N16R8 has 8MB of Octal PSRAM.**
> - Pins **GPIO 33, 34, 35, 36, and 37** are hardwired internally to the high-speed Octal SPI RAM chip.
> - **NEVER** connect any display wire, sensor wire, or jumper to GPIO 33, 34, 35, 36, or 37!
> - Doing so will immediately corrupt the PSRAM cache, causing `Guru Meditation Error: Core 0 panic'ed (Cache error)` and permanent boot loops.
> - All TFT SPI signals are intentionally assigned to **GPIO 10, 11, 12, 13, and 14** to guarantee 100% PSRAM isolation!

#### Verification Steps:
1. Flash `05_central_hub_ili9341_test.ino`.
2. Open Serial Monitor (`115200 baud`).
3. **Pass Criteria:**
   - Backlight turns on brightly.
   - Screen cycles through full-screen color test bars: Red, Green, Blue, Yellow, White, Black.
   - Live nursery telemetry layout renders with high refresh rate: Cot status boxes, waveform graphics, and confidence gauges render crisply with zero screen flickering.
4. **Common Faults & Fixes:**
   - *White screen with backlight ON:* Pin mismatch! Re-verify that `CS=10`, `MOSI=11`, `SCK=12`, `DC=13`, `RESET=14`. Confirm that `VCC` is supplied with 5V.
   - *Colors inverted or mirrored:* Check `tft.setRotation(1)` in the sketch.

---

## 4. Phase 2: Real Project Execution Wiring (Final Physical Assembly)

Once all components have passed their individual bench diagnostics, proceed to final physical assembly, soldering, and enclosure integration.

---

### Subsystem A: Edge Sensing Nodes (Cot 1, Cot 2, Cot 3)
* **Target Hardware:** 3 identical units. Each unit is permanently assigned a `CURRENT_NODE_ID` (1, 2, or 3 in `node_firmware.ino`).

#### Complete Node Schematic Diagram:
```
                                        +5V Rail (from Boost Converter)
                                            │
                                            ▼
                                   ┌─────────────────┐
                                   │  5V / VIN       │
                                   │                 │
                                   │ ESP32-C3        │
                                   │ DevKitC-02      │
                                   │                 │
                                   │  3V3        GND │
                                   └────┬─────────┬──┘
                                        │         │
                 ┌──────────────────────┼─────────┴─────────────────────┐
                 │                      │                               │
                 │                      ▼                               ▼
                 │             ┌─────────────────┐             ┌─────────────────┐
                 │             │ INMP441 MEMS    │             │ Battery Divider │
                 │             │                 │             │                 │
                 │             │ VDD ◄── 3V3     │             │ VBAT (Li-Po +)  │
                 │             │ GND ◄── GND     │             │      │          │
                 │             │ SCK ◄── GPIO 5  │             │   [100 kΩ]      │
                 │             │ WS  ◄── GPIO 6  │             │      ├── GPIO 1 │
                 │             │ SD  ──► GPIO 7  │             │   [100 kΩ]      │
                 │             │ L/R ◄── GND     │             │      │          │
                 │             └─────────────────┘             │     GND         │
                 │                                             └─────────────────┘
                 │
                 ├──────────────────────────────┐
                 │                              │
                 ▼                              ▼
        ┌─────────────────┐            ┌─────────────────┐
        │ Red Battery LED │            │ Blue Link LED   │
        │                 │            │                 │
        │ GPIO 4 (HIGH)   │            │ 3.3V Rail       │
        │   │             │            │   │             │
        │ [330 Ω]         │            │ [330 Ω]         │
        │   ▼             │            │   ▼             │
        │ Anode (+)       │            │ Anode (+)       │
        │ Cathode (−)     │            │ Cathode (−)     │
        │   │             │            │   │             │
        │  GND            │            │ GPIO 9 (LOW=ON) │
        └─────────────────┘            └─────────────────┘
```

#### Final Assembly Step-by-Step Instructions:
1. **Physical ID Marking:** Label each node housing clearly with a permanent label: `COT 01`, `COT 02`, and `COT 03`.
2. **Flash Node Firmware:** In `firmware/node_firmware/node_firmware.ino`, edit line 34 for each board before flashing:
   - Board 1: `#define CURRENT_NODE_ID 1`
   - Board 2: `#define CURRENT_NODE_ID 2`
   - Board 3: `#define CURRENT_NODE_ID 3`
3. **Acoustic Porting:** When mounting the INMP441 in the cot enclosure, drill a **2.0 mm acoustic port hole** in the front bezel directly over the sound port hole of the MEMS microphone. Use an acoustic mesh or dust filter over the hole.
4. **Decoupling Placement:** Solder the 100nF ceramic capacitor across the `VDD` and `GND` pins directly on the backside of the INMP441 breakout to ensure maximum RF noise immunity during ESP-NOW bursts.

---

### Subsystem B: Central Intelligence Hub (ESP32-S3 N16R8)
* **Target Hardware:** 1 unit. Performs continuous audio collection, ML inference, and caregiver alert dispatch.

#### Complete Central Hub Schematic Diagram:
```
                      +5V Rail (from Boost Converter / USB)
                          │
         ┌────────────────┴──────────────────┐
         │                                   │
         ▼                                   ▼
┌──────────────────┐               ┌───────────────────┐
│ ESP32-S3-DevKit  │               │ ILI9341 2.4" TFT  │
│ N16R8            │               │                   │
│                  │               │ VCC   ◄── 5V      │
│ 5V/VIN ◄── 5V    │               │ GND   ◄── GND     │
│ GND    ◄── GND   │               │ CS    ◄── GPIO 10 │
│                  │               │ RESET ◄── GPIO 14 │
│ GPIO 10 ─────────┼──────────────►│ DC    ◄── GPIO 13 │
│ GPIO 11 ─────────┼──────────────►│ MOSI  ◄── GPIO 11 │
│ GPIO 12 ─────────┼──────────────►│ SCK   ◄── GPIO 12 │
│ GPIO 13 ─────────┼──────────────►│                   │
│ GPIO 14 ─────────┼──────────────►│ LED ◄── 3.3V (100Ω)
│                  │               │ MISO ──► OPEN(NC) │
│ 3V3 ──[100Ω]─────┼──────────────►│                   │
│                  │               └───────────────────┘
│ GPIO 33-37       │
│ [OCTAL PSRAM]    │
│ DO NOT WIRE!     │
└──────────────────┘
```

#### Final Assembly Step-by-Step Instructions:
1. **Display Mounting:** Mount the ILI9341 display rigidly to the front panel of the hub console enclosure.
2. **Short SPI Wiring:** Keep SPI wire leads (GPIO 10, 11, 12, 13, 14) **shorter than 12 cm** to prevent high-frequency SPI clock reflection at 40 MHz.
3. **No Buzzer Policy:** To maintain silent operation in a hospital nursery or home nursery, **no piezo buzzer is installed on the hub**. All acoustic alarms are routed directly to the Caregiver's Wearable Wristband via BLE GATT notifications and displayed visually on the hub's TFT dashboard.

---

### Subsystem C: Caregiver Wearable Wristband (ESP32-C3 SuperMini)
* **Target Hardware:** 1 unit. Compact, battery-powered wristband with OLED display and silent haptic vibration.

#### Complete Wearable Schematic Diagram:
```
     1S 3.7V Li-Po Battery (500 mAh)
                │
                ▼
        [ SPDT Power Switch ]
                │
                ├───► TP4056 BAT+ / Charger Input
                │
                ▼
      ESP32-C3 SuperMini 3V3 / BAT
                │
       +3.3V System Rail
                │
   ┌────────────┴──────────────────────┬──────────────────────────────┐
   │                                   │                              │
   ▼                                   ▼                              ▼
┌──────────────┐              ┌────────────────┐             ┌─────────────────┐
│ SuperMini    │              │ SSD1306 OLED   │             │ Haptic Motor    │
│              │              │                │             │                 │
│ 3V3    ◄─3.3V│              │ VCC  ◄── 3.3V  │             │ Motor (+) ◄─3.3V│
│ GND    ◄─GND │              │ GND  ◄── GND   │             │ Motor (−) ──┐   │
│              │              │ SDA  ◄── GPIO 4│             │ 1N5819 Diode│   │
│ GPIO 4 ──────┼─────────────►│ SCL  ◄── GPIO 5│             │ (Flyback)   │   │
│ GPIO 5 ──────┼─────────────►│                │             └─────────────┼───┘
│ GPIO 3 ──┐   │              └────────────────┘                           │
└──────────┼───┘                                                           │
           │                                                               │
           ▼                                                               │
     [ 100 Ω ]                                                             │
           │                                                               │
           ├────────────────────────► Gate (G) ──┐                         ▼
           │                                     │ Drain (D) ◄─────────────┘
       [ 10 kΩ ]                               ┌────────────┐
           │                                   │ AO3400A /  │
           ▼                                   │ IRLML2502  │
          GND ───────────────────────► Source(S)│ N-MOSFET   │
                                               └────────────┘
```

#### Final Assembly Step-by-Step Instructions:
1. **Form Factor Construction:** Mount the SuperMini board, SSD1306 OLED, MOSFET circuit, and 500 mAh Li-Po in a compact 3D-printed wristband enclosure ($42\text{ mm} \times 36\text{ mm} \times 14\text{ mm}$).
2. **Motor Isolation:** Mount the ERM vibration motor with double-sided foam tape against the bottom shell of the wristband so vibrations transfer directly to the wearer's wrist rather than stressing soldered PCB joints.
3. **Power Switch:** Wire a miniature SPDT slide switch in series with the positive battery lead so the user can easily turn the wristband off when not in use.

---

### Subsystem D: Complete Power Distribution & Decoupling Blueprint

A distributed wireless system with multiple RF transmitters (ESP-NOW bursts at 20 dBm and BLE advertisements) creates substantial transient current spikes (up to $350\text{ mA}$). A stable power tree and proper decoupling capacitors are essential.

#### Power Tree Architecture:

```
[ COT NODES 1, 2, 3 (x3) ]
1S 3.7V 2000mAh Li-Po ──► TP4056 Charger ──► 5V Boost Converter ──► ESP32-C3 5V/VIN
                                                    │               (Onboard LDO steps
                                                    └──[ 100µF Cap ] down to 3.3V for
                                                                    MCU & INMP441)

[ CENTRAL INTELLIGENCE HUB (x1) ]
1S 3.7V 3000mAh Li-Ion ──► TP4056 Charger ──► 5V Boost Converter ──┬──► ESP32-S3 5V/VIN
                                                    │              └──► ILI9341 TFT VCC
                                                    └──[ 100µF Cap ]

[ CAREGIVER WEARABLE (x1) ]
1S 3.7V 500mAh Li-Po ────► SPDT Switch ────► TP4056 Charger ──────► SuperMini 3.3V LDO
                                                    │               (Powers MCU, OLED,
                                                    └──[ 47µF Cap ]  and Haptic Motor)
```

#### Mandatory Decoupling & Buffer Capacitors:

| Location | Capacitor Value | Type | Purpose |
| :--- | :--- | :--- | :--- |
| **INMP441 VDD $\rightarrow$ GND** | **100 nF** | Ceramic (0805 or DIP) | Suppresses high-frequency RF ripple from ESP-NOW radio transmissions |
| **Cot Node GPIO 1 $\rightarrow$ GND** | **100 nF** | Ceramic (0805 or DIP) | Low-pass filter for battery ADC; eliminates false low-battery alarms |
| **5V Boost Converter Outputs** | **47 µF – 100 µF** | Electrolytic (16V) | Absorbs instant $350\text{ mA}$ RF power amplifier transmit surges |
| **Wearable 3.3V Rail** | **10 µF – 47 µF** | Ceramic or Tantalum | Buffers motor startup inrush current; prevents MCU brownout resets |

---

## 5. Critical Hardware Protection & Safety Rules

### 1. Reverse-Polarity Battery Hazard (JST-PH 2.0mm)
* **DANGER:** JST 2.0mm Li-Po connectors from different battery suppliers often have Red (+) and Black (−) swapped!
* **Rule:** **ALWAYS verify battery polarity with a digital multimeter before plugging into any TP4056 or DevKit board.** Plugging in reverse polarity will instantly destroy the TP4056 protection IC and the microcontroller.

### 2. Octal PSRAM Isolation on ESP32-S3
* On the ESP32-S3-WROOM-1 N16R8, **GPIO 33, 34, 35, 36, and 37 are internally bonded to the 8MB Octal PSRAM**.
* **Rule:** Leave these 5 pins completely open and unconnected under all circumstances.

### 3. Strapping Pin Safety on ESP32-C3
* **GPIO 9** controls the ROM download bootloader.
* **Rule:** Never connect an LED or sensor that pulls GPIO 9 to GND at startup. Always wire the Blue LED as **Active-LOW** (`3.3V → Resistor → LED → GPIO 9`).

### 4. Flyback Diode Polarity on Haptic Motor
* Inductive coils in ERM vibration motors generate negative voltage spikes up to $-50\text{V}$ when switching off.
* **Rule:** The **1N5819 Schottky diode** must have its **cathode (the white/silver striped end) connected to 3.3V**, and its anode connected to the MOSFET drain. If reversed, the diode will short the 3.3V rail directly to GND!

### 5. INMP441 Voltage Level
* The INMP441 MEMS sensor is strictly rated for **$1.8\text{V} - 3.3\text{V}$**.
* **Rule:** Never connect the INMP441 `VDD` to a 5V rail. Always power it from the regulated `3V3` pin.

---

## 6. Pre-Flight Commissioning & Verification Checklist

Before applying battery power or enclosing your boards, perform this 10-step checkout:

- [ ] **1. Continuity & Short-Circuit Test:** With power disconnected, use a multimeter in continuity mode to measure resistance between `3V3` and `GND`, and between `5V` and `GND` on all 5 boards. Verify **no direct shorts (resistance must be $> 10\text{ k}\Omega$)**.
- [ ] **2. Li-Po Polarity Verification:** Measure battery lead voltage with a multimeter. Confirm Red wire is Positive ($+3.7\text{V}$ to $+4.2\text{V}$) and Black wire is Negative ($0\text{V}$).
- [ ] **3. INMP441 L/R Grounding:** Verify zero ohms resistance between INMP441 `L/R` pin and `GND` on all 3 cot nodes.
- [ ] **4. Cot Node Flash Verification:** Confirm that Cot 1 is flashed with `CURRENT_NODE_ID 1`, Cot 2 with `CURRENT_NODE_ID 2`, and Cot 3 with `CURRENT_NODE_ID 3`.
- [ ] **5. Blue LED Boot State:** Power on each cot node. Confirm that the node boots into normal runtime (Blue LED starts blinking if Hub is off; does not stay stuck in bootloader).
- [ ] **6. S3 Octal PSRAM Verification:** In the Hub serial output on boot, confirm that the console prints:
  ```
  Total PSRAM: 8388608 bytes
  Free PSRAM:  8323072 bytes
  ```
- [ ] **7. ILI9341 Color Test:** Verify that the Hub TFT display boots without a white screen, showing the 3 cot status monitoring boxes.
- [ ] **8. Wearable I2C Address:** Confirm that the Wearable serial console prints `✅ Found I2C device at 0x3C` and renders the idle monitoring screen.
- [ ] **9. Wearable Brownout Check:** Trigger a test vibration on the wearable. Confirm that the motor pulses cleanly and the ESP32-C3 does NOT reboot or flicker the OLED.
- [ ] **10. End-to-End System Integration Test:**
  - Clap or play a baby cry audio file near Cot 2.
  - Cot 2 RMS spikes $> 0.18$ and sends telemetry to Hub.
  - Hub identifies Cot 2 as candidate, fetches 64KB audio burst over ESP-NOW in $\sim 95\text{ ms}$.
  - Hub runs ESP-DSP Log-Mel extraction and ML inference in $\sim 75\text{ ms}$.
  - Hub ILI9341 TFT updates: `COT 02: CRY DETECTED (Confidence: 94%)`.
  - Caregiver Wearable buzzes with urgency pulse and displays `🚨 COT 02 CRY DETECTED! 🚨`.
