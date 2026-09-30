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

## 📌 Project Executive Summary

In multi-infant pediatric care units, neonatal wards, and domestic nurseries, infant distress must be detected and localized rapidly to prevent complications. Traditional commercial baby monitors suffer from severe limitations: they rely on cloud infrastructure and home Wi-Fi (introducing latency, jitter, privacy vulnerabilities, and service downtime), fail to differentiate between multiple adjacent cots, produce ear-fatiguing audible alarms that disturb resting infants, and cannot distinguish infant cries from ambient background chatter, domestic noise, or door slams.

This project delivers a **decentralized, offline-first, multi-node edge-AI infant cry detection, spatial localization, and silent caregiver alert system**. Operating across **5 specialized microcontrollers**, the system captures acoustic signals at each cot, continuously broadcasts lightweight energy telemetry, pulls a 2-second high-resolution audio buffer from the loudest candidate cot via a two-tier connectionless ESP-NOW protocol, executes an embedded deep convolutional neural network (**TinyCryNet** with Blueprint Separable Convolutions and Contrast-Aware Attention) on an ESP32-S3 microcontroller, and dispatches a silent haptic and visual alert to a wearable wristband over Bluetooth Low Energy (BLE) in **under 1.1 seconds** total latency—completely eliminating internet, router, and cloud dependencies.

---

## 🏛️ System Architecture & Distributed Hardware Topology

The architecture divides sensing, edge inference, and alerting into three decoupled tiers:

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

---

## ⚡ The Two-Tier ESP-NOW Audio Pull Protocol

Standard Wi-Fi introduces connection handshakes, DHCP delays, and router failure points. ESP-NOW provides raw 2.4 GHz IEEE 802.11 vendor-action frames without an AP, but enforces a **250-byte maximum payload per packet**. 

Streaming uncompressed 16 kHz 16-bit mono PCM audio ($16,000 \times 2 = 32,000\text{ bytes/second} = 64,000\text{ bytes}$ for a 2-second inference window) continuously from 3 nodes would choke the 2.4 GHz spectrum with packet collisions. This project solves that bandwidth bottleneck using a **two-tier demand-pull protocol**:

1. **Tier 1 — Continuous Lightweight Telemetry**: Every $100\text{ ms}$, each cot node broadcasts a tiny **12-byte telemetry packet** containing its node ID, sequence counter, RMS acoustic energy, and battery voltage. The Central Hub continuously tracks ambient room sound and compares relative amplitude across all three cots.
2. **Tier 2 — On-Demand 64 KB Audio Fetch**: When a cot's RMS energy crosses the calibrated trigger threshold ($0.18$), the Central Hub identifies the loudest node as the candidate cot and unicasts an `AUDIO_REQ` packet. The candidate node freezes its DMA ring buffer and streams **320 packets (200 bytes audio payload each)** at high speed. The S3 reassembles all 320 fragments directly inside its 8MB Octal PSRAM in under $100\text{ ms}$, ready for Log-Mel spectrogram computation.

---

## ⏱️ Real-Time Latency Budget Decomposition

The design guarantees a worst-case latency well below the $1.5\text{ s}$ specification limit:

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

## 📁 Repository Directory Structure & File Map

For a granular file-by-file breakdown with line references and data types, refer to [`DIRECTORY_MAP.md`](DIRECTORY_MAP.md).

```
Multi-node/
├── 📁 firmware/                      # C/C++ embedded firmware for all 5 MCUs, schematics, diagnostics & partition maps
│   ├── central_hub_s3/               # Central Intelligence Hub firmware (ESP32-S3 N16R8)
│   ├── node_firmware/                # Cot Sensing Nodes 1, 2, and 3 firmware (ESP32-C3-DevKitC-02)
│   ├── wearable_firmware/            # Caregiver Wearable Alert Wristband firmware (ESP32-C3 SuperMini)
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
* **Microcontroller:** ESP32-S3-WROOM-1 (Dual-Core Xtensa LX7 @ 240 MHz, 16MB Flash, 8MB Octal PSRAM).
* **Display:** 2.4-inch ILI9341 SPI Color TFT Display (240×320 resolution).
* **Power:** 3.7V 3000mAh Li-Ion battery $\rightarrow$ TP4056 USB-C Charger $\rightarrow$ 5V Boost Converter $\rightarrow$ ESP32-S3 `5V/VIN`.
* **Octal PSRAM Safety Protection:** On the N16R8 module, `GPIO 33, 34, 35, 36, and 37` are dedicated exclusively to high-speed Octal PSRAM. The ILI9341 display uses `GPIO 10 (CS), 11 (MOSI), 12 (SCK), 13 (DC), and 14 (RESET)` on SPI2 (FSPI), preventing memory corruption.
* **Acoustic Design:** Buzzer removed from design to ensure zero nursery disturbance; all alerting is routed to the caregiver wristband and TFT display.

### 2. Cot Sensing Nodes (3 Units — ESP32-C3-DevKitC-02)
* **Microcontroller:** ESP32-C3 RISC-V Single-Core @ 160 MHz, 4MB Flash, 400KB SRAM.
* **Microphone:** INMP441 Omnidirectional Digital MEMS (`SCK -> GPIO 5`, `WS -> GPIO 6`, `SD -> GPIO 7`, `L/R -> GND`).
* **Battery Sensing:** High-impedance voltage divider ($100\text{ k}\Omega + 100\text{ k}\Omega$ with $100\text{ nF}$ filter cap) on `GPIO 1` (`ADC1_CH1`), remaining functional alongside active Wi-Fi.
* **Dual Status LEDs:** 
  * Red LED on `GPIO 4` (Active-HIGH, battery status indication).
  * Blue LED on `GPIO 9` (Active-LOW, pull-up circuit safely maintaining strapping pin HIGH at boot).

### 3. Caregiver Wearable Wristband (ESP32-C3 SuperMini)
* **Microcontroller:** Ultra-compact ESP32-C3 SuperMini.
* **Display:** 0.96-inch SSD1306 Monochrome I2C OLED (`SDA -> GPIO 4`, `SCL -> GPIO 5`).
* **Haptic Driver:** Logic-level IRLML2502TR SOT-23 N-MOSFET driven via `GPIO 3` ($100\Omega$ gate resistor, $10\text{ k}\Omega$ pulldown), switching a 10mm ERM flat coin motor snubbed with a 1N5819 Schottky flyback diode.
* **Power:** 3.7V 500mAh Li-Po cell with onboard 3.3V low-dropout regulation.

---

## 🚀 Quick-Start Reproduction Guide

### 1. Run the Multi-Cot Software Simulator
To test multi-cot acoustic propagation, energy triggering, audio streaming, and neural inference on your PC without hardware:
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
# Double-click VIEW_ALL_SCHEMATICS.html or open via browser
```

### 4. Flash Hardware Firmwares
Follow the comprehensive step-by-step flashing guide in [`docs/SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md`](docs/SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md) using the pre-configured [`firmware/platformio.ini`](firmware/platformio.ini) or the Arduino IDE.
