# 👶 Multi-Node Infant Cry Detection System: Master Technical Architecture

This repository contains the authoritative, distributed embedded firmware for the final-year Computer Engineering project:

> **"Design and Construction of a Multi-Node Infant Cry Detection System with Wearable Caregiver Alert"**  
> **Student:** Amazing Ayomikun Adelowo (22/05/04/0029)  
> **Institution:** University of Maiduguri  

---

## 🏛️ System Topology (5-Controller Distributed Edge-AI)

```
[ COT 01 ]                [ COT 02 ]                [ COT 03 ]
ESP32-C3 Node 1           ESP32-C3 Node 2           ESP32-C3 Node 3
INMP441 Microphone        INMP441 Microphone        INMP441 Microphone
Resistor Divider ADC      Resistor Divider ADC      Resistor Divider ADC
64KB Ring Buffer          64KB Ring Buffer          64KB Ring Buffer
      │                         │                         │
      │ 100ms Telemetry         │ 100ms Telemetry         │ 100ms Telemetry
      └─────────────────────────┼─────────────────────────┘
                                │
                                ▼ ESP-NOW (2.4 GHz PHY, No Router)
                    ┌─────────────────────────┐
                    │    CENTRAL S3 HUB       │
                    │    ESP32-S3 (N16R8)     │
                    │                         │
                    │ 1. Cot Candidate Select │
                    │ 2. Pull 64KB PCM Audio  │
                    │ 3. ESP-DSP Log-Mel Extr │
                    │ 4. TinyCryNet Inference │
                    │ 5. Decision & Cooldown  │
                    │ 6. ILI9341 Color TFT    │
                    └───────────┬─────────────┘
                                │
                                │ BLE GATT (< 30ms latency)
                                ▼
                    ┌─────────────────────────┐
                    │   CAREGIVER WEARABLE    │
                    │   ESP32-C3-MINI         │
                    │   SSD1306 0.96" OLED    │
                    │   AO3400A + 3V Motor    │
                    │   Vibrates: "COT 02"    │
                    └─────────────────────────┘
```

---

## 📁 Firmware Repository Structure

```
firmware/
├── README.md                      <-- This master architecture document
├── HARDWARE_SCHEMATICS.md         <-- Circuit schematics and electrical specifications
├── common/
│   └── protocol.h                 <-- Shared protocol: Telemetry (14B), Audio Req (7B), Audio Frag (208B), Wearable Alert (6B)
│
├── node_firmware/                 <-- Flashed onto Cot 1, Cot 2, and Cot 3 (ESP32-C3-DevKitC-02)
│   ├── pins.h                     <-- Centralized GPIO map & LED drive logic for Cot Nodes
│   └── node_firmware.ino          <-- I2S DMA 16kHz sampling, 64KB rolling buffer, 100ms RMS, 320-packet audio burst
│
├── central_hub_s3/                <-- Flashed onto Central Intelligence Hub (ESP32-S3-DevKitC-1 N16R8)
│   ├── pins.h                     <-- Centralized GPIO map & PSRAM safety definitions for Hub
│   ├── central_hub_s3.ino         <-- Cot selection, 320-fragment reassembly into PSRAM, TinyML, BLE Server
│   ├── dsp_mel_extractor.h/.cpp   <-- Hardware-accelerated ESP-DSP Log-Mel spectrogram engine
│   └── model_data.h               <-- Quantized TinyCryNet neural network weights
│
├── wearable_firmware/             <-- Flashed onto Caregiver Wristband (ESP32-C3 SuperMini)
│   ├── pins.h                     <-- Centralized GPIO map & MOSFET driver schematic for Wearable
│   └── wearable_firmware.ino      <-- BLE GATT Client, pulsed haptic motor driver, Cot ID OLED display
│
└── diagnostics/                   <-- Standalone hardware verification tools (Bench testing)
    ├── 01_inmp441_audio_plotter/  <-- Live serial audio waveform plotter (I2S verification)
    ├── 02_esp_now_range_probe/    <-- Dual-node wireless packet loss & RSSI range tester
    ├── 03_wearable_haptic_oled_test/ <-- I2C bus scanner, OLED graphic cycle & haptic motor test
    ├── 04_node_led_battery_test/  <-- Cot LED logic (Active-HIGH / Active-LOW) & ADC divider test
    └── 05_central_hub_ili9341_test/ <-- SPI2 TFT color bar rendering & benchmark
```

See the root [SYSTEM_WIRING_GUIDE.md](../SYSTEM_WIRING_GUIDE.md) for the complete, step-by-step physical wiring manual for both the component testing phase and final project execution.

---

## ⚡ The Two-Tier ESP-NOW Protocol (Solving the 250-Byte Limit)

ESP-NOW frames have a hardware limit of 250 bytes. To transfer a 2-second audio buffer ($16,000 \times 2 \times 2 = 64,000\text{ bytes}$), the system employs a two-tier protocol:

### Tier 1: Continuous Lightweight Telemetry
- Every $100\text{ ms}$, each node sends a **14-byte telemetry packet** containing `[msg_type, node_id, seq_num, timestamp, rms_amplitude, battery_mv]`.
- The central S3 tracks the ambient room sound and compares relative amplitude across all three cots.

### Tier 2: On-Demand 64 KB Audio Fetch
- When a node's RMS exceeds `RMS_TRIGGER_THRESHOLD` ($0.18$), the S3 selects that node as the candidate cot and broadcasts an `AUDIO_REQ` packet.
- The candidate node immediately freezes its recent 64 KB buffer and streams **320 packets (200 bytes audio payload each)**.
- Total transfer time is **$\sim 260\text{ ms}$** (800 µs queue pacing), and the S3 reassembles the full buffer directly in **PSRAM**.

---

## ⏱️ Latency Budget (< 1.5s Target)

$$\begin{aligned}
L_{\text{Acoustic Trigger}}   &= 100\text{ ms} \\
L_{\text{Candidate Select}}   &= 5\text{ ms} \\
L_{\text{Audio Request}}      &= 2\text{ ms} \\
L_{\text{320-Frag Transfer}}  &= 95\text{ ms} \\
L_{\text{ESP-DSP Log-Mel}}    &= 35\text{ ms} \\
L_{\text{TinyCryNet ML}}      &= 40\text{ ms} \\
L_{\text{BLE Alert to Band}}  &= 25\text{ ms} \\
L_{\text{Haptic Motor Spin}}  &= 50\text{ ms} \\
\hline
\mathbf{L_{\text{Total}}}     &= \mathbf{\approx 352\text{ ms}} \quad (\ll \mathbf{1500\text{ ms target!}})
\end{aligned}$$

---

## 🔌 Hardware Pinout Reference

### 1. Edge Sensing Nodes (ESP32-C3-DevKitC-02 + INMP441)
| Peripheral Pin | ESP32-C3 GPIO | Function / Wiring Details |
| :--- | :--- | :--- |
| **INMP441 SCK** | **GPIO 5** | I2S Serial Clock (BCLK) |
| **INMP441 WS** | **GPIO 6** | I2S Word Select (LRCK) |
| **INMP441 SD** | **GPIO 7** | I2S Serial Data Out (DIN) |
| **INMP441 L/R** | **GND** | Left channel selection |
| **Battery Divider** | **GPIO 1** | ADC Input ($V_{\text{ADC}} = V_{\text{BAT}} / 2$, 100k + 100k + 100nF) |
| **Red LED** | **GPIO 4** | Active-HIGH: Battery Status (Solid=High, Slow Blink=Med, Fast Blink=Low) |
| **Blue LED** | **GPIO 9** | Active-LOW: 3.3V $\rightarrow$ 330Ω $\rightarrow$ LED $\rightarrow$ GPIO9 (Solid=Linked, Blink=Lost) |

### 2. Central Hub (ESP32-S3-DevKitC-1 N16R8)
| Peripheral Pin | ESP32-S3 GPIO | Function / Notes |
| :--- | :--- | :--- |
| **ILI9341 CS** | **GPIO 10** | TFT Chip Select (Active-LOW) |
| **ILI9341 MOSI** | **GPIO 11** | SPI2 Hardware Serial Data (High speed) |
| **ILI9341 SCK** | **GPIO 12** | SPI2 Hardware Serial Clock |
| **ILI9341 DC** | **GPIO 13** | Data / Command Selection |
| **ILI9341 RESET** | **GPIO 14** | Hardware Reset |
| **ILI9341 LED** | **3.3V (via 100Ω)**| Backlight Power |
| **ILI9341 VCC** | **5V (or 3.3V)** | Power to TFT onboard regulator |
| *PSRAM Bus* | *GPIO 33-37* | **Strictly Reserved** for internal 8MB Octal PSRAM |

### 3. Caregiver Wearable (ESP32-C3-MINI-1)
| Component | ESP32-C3-MINI Pin | Circuit Details |
| :--- | :--- | :--- |
| **Motor Gate** | **GPIO 3** | Drives AO3400A N-MOSFET (100Ω gate resistor, 10kΩ pulldown, 1N5819 flyback diode) |
| **OLED SDA** | **GPIO 4** | SSD1306 128x64 Display I2C |
| **OLED SCL** | **GPIO 5** | SSD1306 128x64 Display I2C |
