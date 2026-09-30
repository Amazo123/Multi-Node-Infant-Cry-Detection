# 📁 Multi-Node Infant Cry Detection System — Repository Directory & Architecture Map

> **Academic Project Title**: Design and Construction of a Multi-Node Infant Cry Detection System with Wearable Caregiver Alert  
> **Author & Candidate**: Amazing Ayomikun Adelowo (Matric: 22/05/04/0029)  
> **Department & Institution**: Department of Computer Engineering, Faculty of Engineering, University of Maiduguri, Nigeria  
> **Hardware Profile**: 3× Cot Sensing Nodes (ESP32-C3) + 1× Central Intelligence Hub (ESP32-S3 N16R8) + 1× Caregiver Wearable (ESP32-C3 SuperMini)  
> **Key Performance Metric**: Total end-to-end detection, localization, and alert latency $\le 1.1\text{ s}$ (Target ceiling: $< 1.5\text{ s}$), zero internet dependency.

---

## 🗺️ High-Level Directory Overview

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

## 🔍 In-Depth Folder & File Descriptions

### 1. `firmware/` — Microcontroller Embedded Firmware & Hardware Specifications
Houses all embedded source code for the 5 distributed microcontrollers, written in C++ using the ESP-IDF / Arduino framework.

* **`central_hub_s3/`**:
  * `central_hub_s3.ino`: Flashed to the **ESP32-S3-DevKitC-1 N16R8**. Coordinates the two-tier ESP-NOW pull protocol, reassembles 320-packet (64 KB) incoming audio streams inside internal Octal PSRAM, computes a 128-band Log-Mel spectrogram using hardware-accelerated ESP-DSP routines, runs TinyCryNet inference, renders a real-time 3-cot dashboard onto the 2.4" ILI9341 SPI color TFT display, and alerts the caregiver over BLE GATT.
  * `model_data.h`: Golden binary neural network weights header placed in the hub's directory for local standalone compilation.
* **`node_firmware/`**:
  * `node_firmware.ino`: Flashed to the 3× **ESP32-C3-DevKitC-02** Cot Sensing Nodes (Cot 1, 2, and 3). Drives the INMP441 MEMS microphone over I2S DMA, maintains a rolling 64 KB PCM circular ring buffer, broadcasts continuous 100ms RMS acoustic telemetry over ESP-NOW, reads battery voltage via ADC1 (GPIO 1), and operates the dual LED status indicators.
  * `i2s_sampler.cpp` / `i2s_sampler.h`: High-speed 16 kHz 16-bit mono DMA audio sampling driver.
  * `dsp_mel_extractor.cpp` / `dsp_mel_extractor.h`: Edge feature extraction helper for standalone audio testing.
* **`wearable_firmware/`**:
  * `wearable_firmware.ino`: Flashed to the **ESP32-C3 SuperMini** inside the caregiver wristband. Acts as a BLE GATT client that connects to the Central Hub, decodes cry alert events, displays the alarmed Cot ID on a 0.96" SSD1306 OLED screen, and fires tactile vibration bursts via an IRLML2502TR N-MOSFET.
* **`common/protocol.h`**:
  * Central shared protocol header included across all microcontrollers. Defines byte-packed C structs for:
    * `telemetry_packet_t` (12 bytes): 100ms node health, RMS acoustic energy, and battery millivolts.
    * `audio_req_packet_t` (7 bytes): Hub command instructing candidate cot node to freeze and transmit audio.
    * `audio_frag_packet_t` (208 bytes): 200-byte audio chunks streamed with index headers (320 fragments total = 64 KB).
    * `wearable_alert_packet_t` (6 bytes): High-priority BLE dispatch carrying cot ID, confidence score, and battery alert.
* **`diagnostics/`**:
  * Standalone checkout sketches designed to test single hardware modules in isolation before full system integration:
    * `01_inmp441_audio_plotter`: Visualizes raw I2S microphone samples on Arduino Serial Plotter.
    * `02_esp_now_range_probe`: Sender/responder test measuring round-trip time (RTT) and packet delivery ratio (PDR) across rooms.
    * `03_wearable_haptic_oled_test`: Tests SSD1306 I2C display and vibrates the ERM coin motor safely through the MOSFET.
    * `04_node_led_battery_test`: Validates ADC battery divider math and checks strapping pin stability on GPIO 9.
    * `05_central_hub_ili9341_test`: Cycles test patterns on the ILI9341 TFT SPI bus without triggering PSRAM pin conflicts.
* **`HARDWARE_SCHEMATICS.md`**:
  * Authoritative electrical connection guide detailing pin-to-pin hookup tables, current-limiting resistor calculations, MOSFET flyback diode wiring, and PSRAM safety guidelines.
* **`platformio.ini`**:
  * Pre-configured multi-target PlatformIO project file for automated building and flashing of all nodes from VS Code.
* **`partitions_16MB.csv` & `partitions_4MB.csv`**:
  * Custom ESP-IDF partition tables allocating flash memory partitions to accommodate large TinyML neural weights.

---

### 2. `hardware_images/` — Breadboard Wiring & Schematics
Contains visual aids, trace-accurate schematics, and generation scripts that depict the physical wiring of all system components.

* **`central_hub_breadboard_wiring.png`**: High-resolution ($2200 \times 1700$) breadboard rendering of the ESP32-S3 Central Hub, showing exact wiring to the 2.4" ILI9341 SPI TFT display, TP4056 charging board, 5V boost converter, 3000mAh Li-Ion battery, and power rail decoupling capacitors (buzzer removed).
* **`cot_node_breadboard_wiring.png`**: High-resolution breadboard wiring for the ESP32-C3 Cot Sensing Nodes, detailing connections to the INMP441 I2S microphone, battery voltage divider on GPIO 1, strapping-safe Blue LED on GPIO 9, and Red LED on GPIO 4.
* **`wearable_breadboard_wiring.png`**: High-resolution breadboard wiring for the ESP32-C3 SuperMini Caregiver Wristband, illustrating the SSD1306 0.96" I2C OLED display, IRLML2502TR SOT-23 N-MOSFET motor driver, 1N5819 Schottky snubber diode, and 10mm flat coin ERM vibration motor.
* **`central_hub_exact_schematic.svg`**, **`cot_node_exact_schematic.svg`**, **`wearable_exact_schematic.svg`**: Clean, infinitely-scalable vector schematics with exact pin labels and electrical annotations.
* **`VIEW_ALL_SCHEMATICS.html`**: A standalone dark-mode interactive web page allowing instant visual review of all system schematics side-by-side in any web browser.
* **`generate_accurate_wiring_images.py`**: The underlying Python Pillow rendering engine that computes pin coordinates and draws trace-accurate breadboard wiring without graphical artifacts.
* **`convert_to_png.ps1`**: PowerShell automation script for SVG to raster image conversions.

---

### 3. `simulation_and_benchmarks/` — Simulation, Validation & Academic Benchmarks
Provides mathematical verification and software-in-the-loop simulation tools to validate the system before physical deployment.

* **`test_multi_node_simulation.py`**:
  * Complete Software-in-the-Loop (SIL) testbed that simulates the entire 5-controller distributed ecosystem. Models 3 cots positioned along a nursery wall, generates synthetic infant cries and room noise, simulates 100ms RMS telemetry, executes candidate selection logic, streams 320 audio fragments, extracts 128-band Log-Mel spectrograms, and runs inference against `tiny_crynet_2s.onnx`.
* **`benchmark_cot_localization.py`**:
  * 180-trial Monte-Carlo stress test that measures cot localization accuracy across varying acoustic signal-to-noise ratios (from clean $+20\text{ dB}$ down to severe $-5\text{ dB}$ noise). Generates confusion matrices and empirical accuracy tables for Chapter 4 of the project thesis.
* **`verify_dsp_parity.py`**:
  * Formal numerical parity suite comparing the Python Librosa reference audio pipeline against the embedded C++ ESP-DSP pipeline (Hann windowing, Radix-2 FFT, triangular Mel filterbanks, and logarithmic dB compression). Demonstrates $>99.2\%$ cosine similarity and sub-0.001 Mean Squared Error.

---

### 4. `model_artifacts/` — Exported Weights & Training Archives
Houses trained model weights, runtime computational graphs, and training checkpoint archives.

* **`tiny_crynet_2s.onnx`**:
  * Exported ONNX graph (985 KB) taking a 2.0-second Log-Mel spectrogram input tensor `[1, 1, 81, 128]` (81 time frames $\times$ 128 Mel frequency bins) and outputting infant cry probability. Optimized for low-latency desktop testing and inference validation.
* **`model_data.h`**:
  * C header file (6.15 MB) containing the trained weight tensors compiled as constant byte arrays in the read-only flash memory segment of the ESP32-S3 microcontroller.
* **`kaggle_run_artifacts.tar.gz`**:
  * 105 MB compressed archive containing complete training run artifacts, PyTorch model checkpoints, optimizer state dictionaries, training logs, and loss curves from the 35-epoch training run on Kaggle (tracked via Git LFS).

---

### 5. `model_training/` — Neural Network Architecture & Cloud Pipelines
Contains the PyTorch deep learning codebase, reference signal processing pipelines, and cloud training notebooks.

* **`tiny_crynet_esp32.py`**:
  * PyTorch implementation of the `TinyCryNet_ESP32` convolutional neural network. Utilizes Blueprint Separable Convolutions (`BSConvU`) to drastically reduce parameter count and multiply-accumulate operations, combined with Contrast-Aware Channel Attention (`CCALayer`) to amplify infant cry harmonics while suppressing ambient nursery noise.
* **`reference_pipeline.py`**:
  * Standalone DSP module computing 16 kHz Log-Mel spectrograms from raw PCM audio buffers using Librosa, matching the mathematical transformation implemented on the ESP32-S3.
* **`kaggle_fast_train_drive_backup.ipynb` / `.py`**:
  * Production training notebook executed on Kaggle GPUs (T4/P100). Automates dataset downloading, SpeechBrain 35-epoch training loop, checkpoint saving, ONNX model exportation, and automated backup to Google Drive.
* **`colab_fast_train_drive_backup.ipynb` / `.py`**:
  * Google Colab equivalent of the training pipeline with integrated Google Drive mounting and authentication.

---

### 6. `docs/` — System Manuals & Deployment Documentation
Houses long-form documentation, deployment guides, and hardware testing procedures.

* **`SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md`**:
  * Master guide detailing workspace inventory, step-by-step flashing instructions for all 5 boards, 4-phase bench testing methodology, troubleshooting checklists, and electrical sanity checks before power-on.

---

### 7. `ICD_MMSP/` — Academic Literature & Baseline Reference
Preserves the foundation paper and source code used as the research baseline for this project.

* **`paper.pdf`**:
  * Full published IEEE MMSP 2025 conference paper (*"Infant Cry Detection in Noisy Environment Using BSConv & TFRNN"*), establishing the baseline acoustic methodology.
* **`train_test.py` & `hparams.yaml`**:
  * Baseline SpeechBrain model scripts from the original authors, serving as the benchmark against which our edge-optimized TinyCryNet architecture is evaluated.
