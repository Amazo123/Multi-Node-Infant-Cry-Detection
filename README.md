# 👶 Multi-Node Infant Cry Detection System with Wearable Caregiver Alert

[![Platform: ESP32-S3 & ESP32-C3](https://img.shields.io/badge/Platform-ESP32--S3%20%7C%20ESP32--C3-red.svg)](https://www.espressif.com/)
[![Framework: Arduino / ESP-IDF](https://img.shields.io/badge/Framework-Arduino%20%2F%20ESP--IDF-blue.svg)](https://github.com/espressif/arduino-esp32)
[![Protocol: ESP-NOW + BLE](https://img.shields.io/badge/Wireless-ESP--NOW%20%2B%20BLE%20GATT-orange.svg)]()
[![Model: TinyCryNet ONNX](https://img.shields.io/badge/Edge%20AI-TinyCryNet%20(BSConv%20%2B%20CCA)-green.svg)]()
[![Latency: < 1.1s Total](https://img.shields.io/badge/Latency-%3C%201.1s%20E2E-brightgreen.svg)]()

> **Final-Year Project in Computer Engineering**  
> **Institution:** Department of Computer Engineering, Faculty of Engineering, University of Maiduguri, Borno State, Nigeria  
> **Student / Candidate:** Amazing Ayomikun Adelowo  
> **Matriculation Number:** 22/05/04/0029  
> **Academic Session:** 2025/2026

---

## 📌 Executive Summary & Project Problem Statement

In pediatric care units, neonatal wards, and busy domestic homes, recognizing an infant's distress call promptly is essential for preventing medical complications, severe dehydration, and prolonged psychological distress. However, existing monitoring solutions suffer from several fundamental engineering bottlenecks:

1. **Vulnerability to Network Outages**: Most commercial baby monitors rely on local Wi-Fi routers and cloud servers. When home internet connections drop or cloud APIs experience latency spikes, alerting fails completely.
2. **Lack of Spatial Localization in Multi-Cot Environments**: Standard monitors treat the room as a single acoustic zone. In a nursery with multiple infants, caregivers cannot identify which cot is in distress without manually checking every infant.
3. **Audible Alarm Fatigue**: Loud buzzers and chimes disturb other sleeping infants and create stressful, high-fatigue environments for caregivers and healthcare staff.
4. **Poor Acoustic Discrimination**: Rudimentary sound threshold triggers cannot distinguish between the sharp harmonic acoustic profile of an infant cry and ambient domestic noises such as vacuum cleaners, doors slamming, or television chatter.

This project resolves these challenges by introducing a **decentralized, offline-first, multi-node edge-AI infant cry detection, spatial localization, and silent caregiver alert system**. Operating across **five dedicated microcontrollers**, the system monitors multiple cots concurrently, executes edge acoustic inference using a custom deep neural network (**TinyCryNet**), and transmits silent, targeted haptic alerts to a caregiver wristband in **under 1.1 seconds** total latency—without requiring any router, internet connection, or cloud infrastructure.

---

## 🏛️ System Architecture & Distributed Hardware Topology

The architecture divides sensing, edge inference, and alerting across three decoupled physical tiers:

```
[ COT 01 ]                     [ COT 02 ]                     [ COT 03 ]
ESP32-C3-DevKitC-02            ESP32-C3-DevKitC-02            ESP32-C3-DevKitC-02
INMP441 I2S MEMS Mic           INMP441 I2S MEMS Mic           INMP441 I2S MEMS Mic
Battery ADC (GPIO 1)           Battery ADC (GPIO 1)           Battery ADC (GPIO 1)
64KB Ring Buffer               64KB Ring Buffer               64KB Ring Buffer
Dual Status LEDs               Dual Status LEDs               Dual Status LEDs
       │                              │                              │
       │ 100ms Telemetry (12B)        │ 100ms Telemetry (12B)        │ 100ms Telemetry (12B)
       └──────────────────────────────┼──────────────────────────────┘
                                      │
                                      ▼ ESP-NOW (2.4 GHz PHY, No Router Required)
                          ┌──────────────────────────────┐
                          │     CENTRAL INTELLIGENCE     │
                          │             HUB              │
                          │   ESP32-S3-DevKitC-1 N16R8   │
                          │   (16MB Flash, 8MB PSRAM)    │
                          │                              │
                          │ 1. Energy Candidate Ranking  │
                          │ 2. 64KB Audio Burst Pull     │
                          │ 3. 320-Fragment PSRAM Reass. │
                          │ 4. ESP-DSP Log-Mel Extraction│
                          │ 5. TinyCryNet Edge Inference │
                          │ 6. 2.4" ILI9341 Color TFT    │
                          └──────────────┬───────────────┘
                                         │
                                         │ BLE GATT Alert Event (< 30ms latency)
                                         ▼
                          ┌──────────────────────────────┐
                          │      CAREGIVER WEARABLE      │
                          │          WRISTBAND           │
                          │      ESP32-C3 SuperMini      │
                          │                              │
                          │ 1. SSD1306 0.96" I2C OLED    │
                          │ 2. IRLML2502TR N-MOSFET Gate │
                          │ 3. 10mm Flat ERM Coin Motor  │
                          │ 4. Silent Haptic Pulsing     │
                          │ 5. Displays: "COT 02 CRY!"   │
                          └──────────────────────────────┘
```

### The Three Hardware Subsystems:
1. **Cot Sensing Nodes (Cots 1, 2, and 3)**:
   * Powered by the **ESP32-C3-DevKitC-02** (RISC-V single-core @ 160 MHz).
   * Continuously captures 16 kHz 16-bit mono audio from an omnidirectional **INMP441 digital MEMS microphone** over I2S DMA into a rolling 64 KB circular ring buffer.
   * Calculates Root-Mean-Square (RMS) audio energy every 100 ms and broadcasts lightweight 14-byte telemetry packets over ESP-NOW.
   * Monitors battery voltage via a precision resistor divider on `GPIO 1` (`ADC1_CH1`).
   * Provides dual-LED status indicators: an active-HIGH Red LED on `GPIO 4` for battery state and an active-LOW Blue LED on `GPIO 9` for wireless link connectivity.
2. **Central Intelligence Hub**:
   * Powered by the **ESP32-S3-DevKitC-1 N16R8** (Dual-Core Xtensa LX7 @ 240 MHz, 16MB Flash, 8MB Octal PSRAM).
   * Monitors telemetry streams from all three cots. When an acoustic surge exceeds the trigger threshold ($0.18$), the hub identifies the loudest node and issues a unicast pull command (`AUDIO_REQ`).
   * Reassembles 320 incoming ESP-NOW audio packets (64 KB total) directly inside its 8MB Octal PSRAM within ~260 ms.
   * Extracts a 128-band Log-Mel spectrogram using hardware-accelerated **ESP-DSP** routines.
   * Executes edge neural network inference using the **TinyCryNet** model to confirm infant cry vocalizations while rejecting background disturbances.
   * Renders real-time telemetry cards, live RMS bars, and alert banners onto a 2.4-inch **ILI9341 SPI color TFT display** (240×320 resolution).
   * Operates as a BLE GATT Server to dispatch instant alerts to the caregiver wristband. The buzzer was intentionally omitted to preserve a quiet hospital nursery environment.
3. **Caregiver Wearable Wristband**:
   * Powered by the ultra-compact **ESP32-C3 SuperMini**.
   * Acts as a BLE GATT Client connected directly to the Central Hub.
   * Upon receiving a cry notification, displays the alerted cot ID (e.g., `COT 02 CRY!`) on a 0.96-inch **SSD1306 monochrome I2C OLED display**.
   * Drives a 10mm flat coin ERM vibration motor through an **IRLML2502TR logic-level N-channel MOSFET**, providing tactile haptic pulses that awaken or alert the caregiver silently.

---

## ⚡ Two-Tier ESP-NOW Protocol & Bandwidth Optimization

Standard Wi-Fi relies on access point associations, IP allocations, and TCP/UDP handshakes, all of which introduce latency and failure modes. ESP-NOW provides connectionless 2.4 GHz communication using raw IEEE 802.11 vendor-action frames, but imposes a **250-byte maximum payload per packet**.

Streaming uncompressed 16 kHz 16-bit mono PCM audio ($32,000\text{ bytes/second} = 64,000\text{ bytes}$ for a 2-second window) continuously from three nodes would saturate the 2.4 GHz spectrum and cause severe packet collisions. This project solves that bottleneck using a **two-tier demand-pull protocol**:

* **Tier 1 (Continuous Lightweight Telemetry)**: Every $100\text{ ms}$, each cot node broadcasts a 14-byte telemetry packet containing its node ID, sequence counter, RMS acoustic energy, and battery voltage. This uses less than 1% of available RF channel capacity.
* **Tier 2 (On-Demand Audio Fetch)**: When a cot's RMS energy crosses the trigger threshold ($0.18$), the Central Hub identifies the loudest node as the candidate cot and unicasts an `AUDIO_REQ` packet. The candidate node freezes its DMA ring buffer and streams **320 packets (200 bytes audio payload each)** at high speed with 800 µs pacing. The S3 reassembles all 320 fragments directly inside external PSRAM in under $300\text{ ms}$, ready for Log-Mel spectrogram computation.

---

## ⏱️ Real-Time Latency Budget Decomposition

The entire detection, spatial localization, neural verification, and haptic notification pipeline completes in approximately **352 ms**, well within the project's strict $1.5\text{ s}$ ceiling:

$$\begin{aligned}
L_{\text{Acoustic RMS Trigger}}  &= 100\text{ ms} \\
L_{\text{Candidate Cot Select}}  &= 5\text{ ms} \\
L_{\text{Audio Request Dispatch}}&= 2\text{ ms} \\
L_{\text{320-Fragment Transfer}} &= 95\text{ ms} \\
L_{\text{ESP-DSP Log-Mel Extr.}} &= 35\text{ ms} \\
L_{\text{TinyCryNet Inference}}  &= 40\text{ ms} \\
L_{\text{BLE Dispatch to Wear.}} &= 25\text{ ms} \\
L_{\text{Haptic Motor Spin-up}}  &= 50\text{ ms} \\
\hline
\mathbf{L_{\text{Total End-to-End}}} &= \mathbf{\approx 352\text{ ms}} \quad (\ll \mathbf{1500\text{ ms Maximum Budget}})
\end{aligned}$$

---

## 📁 Repository Directory Structure

For an exhaustive, file-by-file breakdown with detailed technical descriptions, refer to [`DIRECTORY_MAP.md`](DIRECTORY_MAP.md).

```
Multi-node/
├── SYSTEM_WIRING_GUIDE.md            # Master physical wiring manual for bench testing & final execution assembly
├── 📁 firmware/                      # C/C++ embedded firmware for all 5 MCUs, schematics, diagnostics & partition maps
│   ├── central_hub_s3/               # Central Intelligence Hub firmware (ESP32-S3 N16R8) + pins.h
│   ├── node_firmware/                # Cot Sensing Nodes 1, 2, and 3 firmware (ESP32-C3-DevKitC-02) + pins.h
│   ├── wearable_firmware/            # Caregiver Wearable Alert Wristband firmware (ESP32-C3 SuperMini) + pins.h
│   ├── common/                       # Shared wire protocol headers & ESP-NOW struct definitions
│   ├── diagnostics/                  # 5 standalone hardware unit-test sketches for bench bring-up
│   ├── HARDWARE_SCHEMATICS.md        # Comprehensive pinout, resistor divider & electrical connection tables
│   ├── README.md                     # Deep architectural breakdown of two-tier ESP-NOW & latency budgets
│   ├── platformio.ini                # Multi-board PlatformIO build & toolchain configuration
│   ├── partitions_16MB.csv           # Custom 16MB flash layout (6MB app partition) for ESP32-S3
│   └── partitions_4MB.csv            # 4MB flash partition table for ESP32-C3 nodes and wearable
│
├── 📁 hardware_images/               # Visual breadboard diagrams, circuit schematics & generator tooling
│   ├── central_hub_breadboard_wiring.png # 2200x1700 pixel-accurate breadboard wiring for Central Hub
│   ├── central_hub_exact_schematic.svg   # Vector schematic with pin labels for Central Hub
│   ├── cot_node_breadboard_wiring.png    # 2200x1700 pixel-accurate breadboard wiring for Cot Nodes
│   ├── cot_node_exact_schematic.svg      # Vector schematic with pin labels for Cot Nodes
│   ├── wearable_breadboard_wiring.png    # 2200x1700 pixel-accurate breadboard wiring for Wearable
│   ├── wearable_exact_schematic.svg      # Vector schematic with pin labels for Wearable
│   ├── VIEW_ALL_SCHEMATICS.html          # Interactive dark-mode HTML browser to inspect all schematics
│   ├── generate_accurate_wiring_images.py# Algorithmic Pillow rendering script producing the PNGs
│   ├── convert_to_png.ps1                # PowerShell script for SVG-to-PNG rendering workflows
│   └── README.md                         # Detailed guide on wiring schematics and image assets
│
├── 📁 simulation_and_benchmarks/     # Software-in-the-Loop simulator, stress benchmarks & DSP validation
│   ├── test_multi_node_simulation.py # 5-controller Software-in-the-Loop simulator with acoustic room model
│   ├── benchmark_cot_localization.py # 180-trial Monte-Carlo stress test across SNR levels (+20dB down to -5dB)
│   ├── verify_dsp_parity.py          # Mathematical validation proving >99.2% cosine parity (Python vs ESP-DSP)
│   └── README.md                     # Benchmarking methodology, execution instructions & expected outputs
│
├── 📁 model_artifacts/               # Neural network binary weights, ONNX graph & training archive
│   ├── tiny_crynet_2s.onnx           # 985 KB exported ONNX graph (input shape: [1, 1, 81, 128])
│   ├── model_data.h                  # 6.15 MB C-byte array with model weights compiled directly into S3 flash
│   ├── kaggle_run_artifacts.tar.gz   # 105 MB trained SpeechBrain checkpoint archive (tracked via Git LFS)
│   └── README.md                     # Model specifications, memory footprint & input/output tensor contracts
│
├── 📁 model_training/                # Model architecture definition, reference DSP & cloud training pipelines
│   ├── tiny_crynet_esp32.py          # PyTorch definition of TinyCryNet (BSConvU + CCALayer attention)
│   ├── reference_pipeline.py         # Pure Python Log-Mel spectrogram reference generator (16 kHz, 128 mels)
│   ├── kaggle_fast_train_drive_backup.ipynb / .py # 35-epoch Kaggle training pipeline with Drive checkpointing
│   ├── colab_fast_train_drive_backup.ipynb / .py  # Equivalent Google Colab training notebook & script
│   └── README.md                     # Neural network design rationale, hyperparameter setup & training notes
│
├── 📁 docs/                          # Comprehensive technical manuals & operational guides
│   ├── SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md # Master hardware assembly, firmware flashing & bench bring-up roadmap
│   └── README.md                     # Overview of system documentation & index of reference material
│
└── 📁 ICD_MMSP/                      # Academic baseline literature & reference implementation
    ├── paper.pdf                     # Published IEEE MMSP 2025 conference baseline paper
    ├── train_test.py                 # Original baseline PyTorch training script
    ├── hparams.yaml                  # Baseline SpeechBrain hyperparameter file
    └── README.md                     # Summary of baseline study and our architectural enhancements
```

---

## 🔌 Hardware Pinout & Critical Electrical Considerations

### 1. Central Intelligence Hub (ESP32-S3-DevKitC-1 N16R8)
* **Processor**: ESP32-S3-WROOM-1 (Dual-Core Xtensa LX7 @ 240 MHz, 16MB Flash, 8MB Octal PSRAM).
* **Display Interface (SPI2 / FSPI)**:
  * `CS` $\rightarrow$ `GPIO 10`
  * `MOSI / SDI` $\rightarrow$ `GPIO 11`
  * `SCK / CLK` $\rightarrow$ `GPIO 12`
  * `DC / RS` $\rightarrow$ `GPIO 13`
  * `RESET` $\rightarrow$ `GPIO 14`
  * `LED (Backlight)` $\rightarrow$ `3.3V` via $100\Omega$ current-limiting resistor
  * `VCC` $\rightarrow$ `5V` (utilizing the TFT module's onboard low-dropout regulator)
* **Octal PSRAM Protection Rule**: On the ESP32-S3 N16R8, **GPIO 33, 34, 35, 36, and 37 are internally bonded to high-speed Octal PSRAM**. By routing the ILI9341 display strictly to `GPIO 10–14`, the PSRAM bus remains completely isolated and immune to bus contention.
* **Power Supply**: 3.7V 3000mAh Li-Ion cell $\rightarrow$ TP4056 USB-C Charging Board $\rightarrow$ 5V Boost Converter $\rightarrow$ `5V/VIN` pin on ESP32-S3.

### 2. Cot Sensing Nodes (3 Units — ESP32-C3-DevKitC-02)
* **Processor**: ESP32-C3 RISC-V Single-Core @ 160 MHz.
* **Digital MEMS Microphone (INMP441)**:
  * `SCK (BCLK)` $\rightarrow$ `GPIO 5`
  * `WS (LRCK)` $\rightarrow$ `GPIO 6`
  * `SD (DIN)` $\rightarrow$ `GPIO 7`
  * `L/R` $\rightarrow$ `GND` (configures microphone for left-channel output)
  * `VDD` $\rightarrow$ `3.3V` (Must **never** be connected to 5V!)
* **Battery Sensing**: Precision high-impedance voltage divider ($100\text{ k}\Omega + 100\text{ k}\Omega$ with a $100\text{ nF}$ ceramic smoothing capacitor) connected to `GPIO 1` (`ADC1_CH1`). ADC1 is specifically selected because it operates without interference while Wi-Fi and ESP-NOW RF transmissions are active.
* **Dual-LED Indicator Circuitry**:
  * Red Battery LED: Active-HIGH circuit on `GPIO 4` via a $330\Omega$ resistor to ground.
  * Blue Wireless Link LED: Active-LOW circuit on `GPIO 9`. The circuit is wired: $3.3\text{V} \rightarrow 330\Omega \rightarrow \text{Anode} \rightarrow \text{Cathode} \rightarrow \text{GPIO 9}$. Because `GPIO 9` is a boot strapping pin that must remain HIGH during power-on reset, this active-LOW wiring safely pulls the pin HIGH at boot, preventing bootloader failure.

### 3. Caregiver Wearable Wristband (ESP32-C3 SuperMini)
* **Processor**: ESP32-C3 SuperMini with single USB-C port.
* **OLED Display (SSD1306 0.96" I2C)**:
  * `SDA` $\rightarrow$ `GPIO 4`
  * `SCL` $\rightarrow$ `GPIO 5`
  * `VCC` $\rightarrow$ `3.3V`
* **Haptic Vibration Driver**:
  * Gate of **IRLML2502TR (SOT-23 N-MOSFET)** is driven by `GPIO 3` through a $100\Omega$ damping resistor, with a $10\text{ k}\Omega$ pull-down resistor to ground.
  * The motor is connected between `3.3V` and the MOSFET Drain.
  * A **1N5819 Schottky flyback diode** is installed across the motor terminals (cathode to `3.3V`, anode to Drain) to absorb inductive voltage spikes and protect the microcontroller.
* **Power Supply**: 3.7V 500mAh Li-Po battery connected through the onboard 3.3V regulator.

---

## 🚀 Step-by-Step Reproduction Guide

### 1. Run the Multi-Cot Software Simulator
To verify acoustic propagation, energy triggering, audio streaming, and neural inference on your PC without physical hardware:
```bash
cd simulation_and_benchmarks
python test_multi_node_simulation.py
```

### 2. Execute Chapter 4 Verification & Benchmarking Suites
Run the Monte-Carlo spatial localization benchmark and DSP mathematical equivalence tests:
```bash
cd simulation_and_benchmarks
python benchmark_cot_localization.py
python verify_dsp_parity.py
```

### 3. Inspect Breadboard Wiring Diagrams
Open the interactive schematics browser in your default web browser:
```bash
cd hardware_images
# Open VIEW_ALL_SCHEMATICS.html in your web browser
```

### 4. Flash Hardware Firmwares
Follow the comprehensive step-by-step flashing guide in [`docs/SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md`](docs/SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md) using the pre-configured [`firmware/platformio.ini`](firmware/platformio.ini) or the Arduino IDE.
