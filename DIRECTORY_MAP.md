# 📁 System Directory Map & Technical Architecture Inventory

> **Project Title**: Design and Construction of a Multi-Node Infant Cry Detection System with Wearable Caregiver Alert  
> **Candidate**: Amazing Ayomikun Adelowo (Matriculation: 22/05/04/0029)  
> **Affiliation**: Department of Computer Engineering, Faculty of Engineering, University of Maiduguri, Borno State, Nigeria  
> **Hardware Configuration**: 3× Cot Sensing Nodes (ESP32-C3) + 1× Central Intelligence Hub (ESP32-S3 N16R8) + 1× Caregiver Wearable (ESP32-C3 SuperMini)  
> **Design Philosophy**: Completely offline, edge-native acoustic intelligence operating under a strict 1.5-second total latency ceiling without reliance on local Wi-Fi routers or remote cloud servers.

---

## 🗺️ Architectural Structure of the Repository

```
Multi-node/
├── 📁 firmware/                      # Production C/C++ embedded firmware across all microcontrollers
│   ├── central_hub_s3/               # Central S3 Hub: candidate selection, PSRAM reassembly, ESP-DSP, TinyCryNet, TFT dashboard
│   ├── node_firmware/                # Cot Nodes 1, 2, 3: I2S DMA circular buffer, 100ms RMS telemetry, dual LED signaling
│   ├── wearable_firmware/            # Caregiver Wristband: BLE GATT client, SSD1306 OLED, IRLML2502TR haptic driver
│   ├── common/                       # Central wire protocol definitions (protocol.h) shared by all firmware targets
│   ├── diagnostics/                  # Standalone hardware bring-up and verification sketches
│   ├── HARDWARE_SCHEMATICS.md        # Definitive pinout mappings, electrical safety limits, and circuit schematics
│   ├── README.md                     # Deep architectural breakdown of two-tier ESP-NOW & latency budgets
│   ├── platformio.ini                # Multi-target PlatformIO toolchain and environment configuration
│   ├── partitions_16MB.csv           # Custom 16MB flash layout dedicating 6MB for the ESP32-S3 application binary
│   └── partitions_4MB.csv            # Custom 4MB partition table maximizing app headroom on the ESP32-C3 nodes
│
├── 📁 hardware_images/               # Trace-accurate breadboard wiring diagrams and vector schematics
│   ├── central_hub_breadboard_wiring.png # 2200x1700 high-res breadboard diagram for Central Hub
│   ├── central_hub_exact_schematic.svg   # Vector schematic with pin labels for Central Hub
│   ├── cot_node_breadboard_wiring.png    # 2200x1700 high-res breadboard diagram for Cot Nodes
│   ├── cot_node_exact_schematic.svg      # Vector schematic with pin labels for Cot Nodes
│   ├── wearable_breadboard_wiring.png    # 2200x1700 high-res breadboard diagram for Caregiver Wearable
│   ├── wearable_exact_schematic.svg      # Vector schematic with pin labels for Caregiver Wearable
│   ├── VIEW_ALL_SCHEMATICS.html          # Interactive dark-mode web viewer to inspect all schematics side-by-side
│   ├── generate_accurate_wiring_images.py# Algorithmic Python (Pillow) engine that generates the PNG wiring graphics
│   ├── convert_to_png.ps1                # PowerShell script for SVG-to-PNG rendering workflows
│   └── README.md                         # Detailed guide on wiring schematics and image assets
│
├── 📁 simulation_and_benchmarks/     # Software-in-the-Loop simulation, mathematical proofs, and stress testing
│   ├── test_multi_node_simulation.py # Comprehensive 5-node Software-in-the-Loop simulator with room acoustic propagation
│   ├── benchmark_cot_localization.py # 180-trial Monte-Carlo stress test evaluating cot localization across SNR levels
│   ├── verify_dsp_parity.py          # Mathematical validation suite proving >99.2% cosine parity between Python and C++ DSP
│   └── README.md                     # Benchmarking methodology, execution instructions & expected outputs
│
├── 📁 model_artifacts/               # Neural network runtime graphs, weights, and training archives
│   ├── tiny_crynet_2s.onnx           # 985 KB exported ONNX graph (input: [1, 1, 81, 128])
│   ├── model_data.h                  # 6.15 MB C-byte array compiling weights into ESP32-S3 read-only flash (.rodata)
│   ├── kaggle_run_artifacts.tar.gz   # 105 MB trained SpeechBrain checkpoint archive (tracked via Git LFS)
│   └── README.md                     # Model specifications, memory footprint & input/output tensor contracts
│
├── 📁 model_training/                # Deep learning model definition, reference DSP, and cloud pipelines
│   ├── tiny_crynet_esp32.py          # PyTorch definition of TinyCryNet (BSConvU + CCALayer attention)
│   ├── reference_pipeline.py         # Pure Python Log-Mel spectrogram reference generator matching embedded DSP
│   ├── kaggle_fast_train_drive_backup.ipynb / .py # 35-epoch Kaggle training pipeline with Drive checkpointing
│   ├── colab_fast_train_drive_backup.ipynb / .py  # Equivalent Google Colab training notebook & script
│   └── README.md                     # Neural network design rationale, hyperparameter setup & training notes
│
├── 📁 docs/                          # Master operational guides, deployment checklists, and testing roadmaps
│   ├── SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md # Step-by-step firmware flashing, hardware assembly, and diagnostic manual
│   └── README.md                     # System documentation index and reading guide
│
└── 📁 ICD_MMSP/                      # Academic baseline reference repository
    ├── paper.pdf                     # Published IEEE MMSP 2025 conference baseline paper
    ├── train_test.py                 # Baseline training script from original authors
    ├── hparams.yaml                  # Baseline SpeechBrain hyperparameter file
    └── README.md                     # Summary of baseline study and our architectural enhancements
```

---

## 🔍 In-Depth Subsystem and Component Descriptions

### 1. `firmware/` — Distributed Microcontroller Firmware Subsystem

The `firmware/` directory contains all production embedded code running on the five physical microcontrollers in the system. The firmware is structured to decouple continuous environmental monitoring from computationally intensive acoustic inference, ensuring that battery-constrained edge nodes remain lightweight while the central hub performs deep neural analysis.

#### A. Central Intelligence Hub (`firmware/central_hub_s3/`)
* **`central_hub_s3.ino`**: This is the core supervisory application flashed onto the **ESP32-S3-DevKitC-1 N16R8**. It runs on dual Xtensa LX7 cores clocked at 240 MHz. Its primary responsibilities include:
  1. *ESP-NOW Network Orchestration*: Listens continuously for 12-byte telemetry packets broadcast by the three cot nodes every 100 ms.
  2. *Cot Candidate Selection & Spatial Localization*: Maintains a sliding energy comparison window across all active cots. When an acoustic surge occurs, it identifies the cot reporting the highest Root-Mean-Square (RMS) amplitude above the trigger threshold ($0.18$), pinpointing the true origin of the distress signal even in noisy nursery conditions.
  3. *High-Speed Audio Burst Pulling*: Dispatches a unicast `AUDIO_REQ` command to the candidate node, requesting its frozen 2-second audio history.
  4. *Octal PSRAM Reassembly*: Captures 320 consecutive audio fragment packets (200 bytes each, totaling 64,000 bytes) arriving over ESP-NOW, reconstructing the uncompressed 16 kHz 16-bit PCM waveform directly in high-speed external PSRAM in under 100 ms.
  5. *Edge Feature Extraction (ESP-DSP)*: Computes a 128-band Log-Mel spectrogram using hardware-accelerated discrete Hann windowing, Radix-2 Fast Fourier Transforms (FFT), and triangular Mel filterbanks.
  6. *Neural Network Inference*: Executes the TinyCryNet convolutional neural network against the extracted spectrogram to classify whether the vocalization is an infant cry versus ambient nursery disturbance.
  7. *Graphical Telemetry Dashboard*: Drives a 2.4-inch ILI9341 SPI color TFT display (240×320 resolution), rendering dynamic live audio energy bars for Cot 1, Cot 2, and Cot 3, link health status, battery levels, and flashing visual alert banners.
  8. *Caregiver BLE GATT Server*: Upon positive cry classification, dispatches an immediate high-priority Bluetooth Low Energy notification containing the alarmed cot ID and confidence score to the caregiver wearable wristband.
* **`model_data.h`**: A standalone copy of the compiled neural network weights header placed directly within the hub sketch folder. This allows the Arduino IDE or PlatformIO to build the project without complex external include paths.

#### B. Cot Sensing Nodes (`firmware/node_firmware/`)
* **`node_firmware.ino`**: The edge firmware flashed onto the three **ESP32-C3-DevKitC-02** units positioned at Cot 1, Cot 2, and Cot 3. It operates on a single 32-bit RISC-V core clocked at 160 MHz. Key functions include:
  1. *Continuous DMA Audio Acquisition*: Operates an INMP441 omnidirectional MEMS microphone over an I2S digital audio bus, continuously pulling 16 kHz 16-bit mono audio into a 64 KB circular ring buffer using direct memory access (DMA) without CPU intervention.
  2. *Low-Latency Acoustic Energy Calculation*: Every 100 ms, computes the Root-Mean-Square (RMS) amplitude of the most recent 1,600 audio samples.
  3. *Lightweight Telemetry Broadcasting*: Encapsulates node ID, sequence counter, RMS value, and battery millivolts into a 12-byte telemetry packet broadcast over ESP-NOW.
  4. *Audio Freeze and Burst Transmission*: Upon receiving an `AUDIO_REQ` command from the hub, freezes the circular buffer to prevent overwriting, slices the 64,000 bytes of audio into 320 structured packets (208 bytes each, including sequence headers), and streams them over ESP-NOW in a rapid burst.
  5. *Battery Voltage Sensing*: Measures the single-cell Li-Po battery voltage through a precision 100kΩ/100kΩ resistor divider connected to GPIO 1 (`ADC1_CH1`), which remains fully operational alongside active Wi-Fi.
  6. *Dual-LED Indicator Signaling*: Manages an active-HIGH Red LED on GPIO 4 to indicate battery status (solid for healthy, blinking for low battery) and an active-LOW Blue LED on GPIO 9 to show wireless link connectivity. GPIO 9 is wired with an external pull-up circuit to guarantee that the ESP32-C3 bootloader strapping requirements are strictly respected.
* **`i2s_sampler.cpp` / `i2s_sampler.h`**: The low-level hardware abstraction layer configuring the ESP32-C3 I2S peripheral, DMA buffer descriptors, clock dividers, and sample conversion routines.
* **`dsp_mel_extractor.cpp` / `dsp_mel_extractor.h`**: Auxiliary signal processing modules utilized during standalone edge benchmarking.

#### C. Caregiver Wearable Wristband (`firmware/wearable_firmware/`)
* **`wearable_firmware.ino`**: Flashed onto the ultra-compact **ESP32-C3 SuperMini** inside the caregiver's wearable wristband. Its responsibilities are:
  1. *BLE GATT Client Operations*: Continuously scans for and maintains an authenticated low-energy Bluetooth connection with the Central Hub.
  2. *Instant Alert Processing*: Receives high-priority cry notification packets in under 30 ms.
  3. *OLED Visual Alert Renderer*: Drives a 0.96-inch SSD1306 monochrome I2C display (128×64 resolution), rendering clear graphical alert banners indicating which cot is in distress (e.g., `COT 02 CRY!`), along with battery indicators and timestamp information.
  4. *Pulsed Haptic Motor Driver*: Drives a 10mm flat coin ERM vibration motor through an IRLML2502TR logic-level N-channel MOSFET on GPIO 3, executing tactile vibration pulses that awaken or alert the caregiver silently without producing any sound in the nursery.

#### D. Central Wire Protocol (`firmware/common/protocol.h`)
This header acts as the single source of truth for wireless data communication across all five microcontrollers. It defines packed C structures:
* `telemetry_packet_t` (12 bytes): Transmits periodic telemetry (`msg_type`, `node_id`, `seq_num`, `timestamp_ms`, `rms_energy`, `battery_mv`).
* `audio_req_packet_t` (7 bytes): Sent by the hub to command a specific cot node to transmit its audio buffer.
* `audio_frag_packet_t` (208 bytes): Packs 200 bytes of raw PCM audio data alongside fragment indices (0 to 319) and CRC checksums.
* `wearable_alert_packet_t` (6 bytes): Sent over BLE to dispatch alarms (`cot_id`, `confidence_pct`, `alert_flags`).

#### E. Hardware Diagnostic Suite (`firmware/diagnostics/`)
Contains five dedicated bench-test sketches to validate physical components independently before assembling the multi-node network:
* **`01_inmp441_audio_plotter`**: Streams raw I2S microphone samples directly to the Arduino Serial Plotter to verify clock lines, gain, and signal fidelity.
* **`02_esp_now_range_probe`**: Executes a ping-pong packet exchange between two nodes to measure round-trip time (RTT) and packet delivery ratio (PDR) across nursery walls.
* **`03_wearable_haptic_oled_test`**: Cycles test graphics on the SSD1306 OLED and pulses the ERM vibration motor through the MOSFET to verify gate switching and flyback diode snubbing.
* **`04_node_led_battery_test`**: Verifies ADC battery voltage measurements against a digital multimeter and proves that the strapping pin pull-up on GPIO 9 allows reliable booting.
* **`05_central_hub_ili9341_test`**: Renders test patterns and simulated cot telemetry cards on the 2.4-inch ILI9341 display while proving that the SPI2 bus operates cleanly without interfering with the internal Octal PSRAM lines.

#### F. Schematics & Build Configurations
* **`HARDWARE_SCHEMATICS.md`**: Authoritative electrical blueprint detailing exact pin mappings, resistor divider values, MOSFET connections, power rail decoupling capacitor placements, and strapping pin safety proofs.
* **`platformio.ini`**: Multi-environment build configuration supporting one-click building and flashing of the hub, nodes, and wearable from VS Code.
* **`partitions_16MB.csv` & `partitions_4MB.csv`**: Partition maps establishing large application memory partitions (6.0 MB on the S3) to host neural network weights.

---

### 2. `hardware_images/` — Visual Schematics & Breadboard Layouts

This directory provides visual representations of the physical circuits, ensuring that students, technicians, and evaluators can replicate the exact hardware setup without wiring errors.

* **`central_hub_breadboard_wiring.png`**: A high-resolution ($2200 \times 1700$) graphical layout illustrating the Central Hub assembled on a full-size solderless breadboard. Clearly shows the ESP32-S3-DevKitC-1 N16R8 wired to the 2.4" ILI9341 SPI color TFT display, TP4056 USB-C charging module, 5V boost converter, 3000mAh Li-Ion battery, and power rail filtering capacitors. The buzzer has been intentionally omitted to ensure silent nursery operation.
* **`cot_node_breadboard_wiring.png`**: High-resolution breadboard wiring for the Cot Sensing Nodes. Illustrates the ESP32-C3-DevKitC-02 connected to the INMP441 I2S microphone breakout, the 100kΩ/100kΩ battery voltage divider on GPIO 1, the active-LOW Blue status LED on GPIO 9, and the active-HIGH Red battery LED on GPIO 4.
* **`wearable_breadboard_wiring.png`**: High-resolution breadboard wiring for the Caregiver Wearable Wristband. Shows the compact ESP32-C3 SuperMini board wired to the 0.96" SSD1306 I2C OLED display, the IRLML2502TR SOT-23 logic-level N-channel MOSFET, the 1N5819 Schottky flyback diode, the gate pulldown resistor, and the 10mm flat coin ERM vibration motor.
* **`central_hub_exact_schematic.svg`**, **`cot_node_exact_schematic.svg`**, **`wearable_exact_schematic.svg`**: Infinitely scalable vector schematics annotated with pin identifiers, signal directions, and component values.
* **`VIEW_ALL_SCHEMATICS.html`**: A standalone dark-mode interactive HTML viewer that renders all system schematics side-by-side in any standard web browser for quick workbench reference.
* **`generate_accurate_wiring_images.py`**: The custom Python Pillow rendering script that calculates precise coordinates and generates the $2200 \times 1700$ PNG breadboard diagrams directly from electrical pinout definitions.
* **`convert_to_png.ps1`**: A PowerShell script automating the conversion of vector SVGs into high-resolution bitmap images.

---

### 3. `simulation_and_benchmarks/` — Simulation, Validation & Academic Benchmarks

This directory provides the mathematical and software-in-the-loop verification tools used to validate system performance and generate empirical data for the project thesis.

* **`test_multi_node_simulation.py`**:
  * An end-to-end Software-in-the-Loop (SIL) testbed that simulates the complete distributed multi-node ecosystem on a standard PC.
  * Models three cots positioned at $0\text{ m}$, $2\text{ m}$, and $4\text{ m}$ along a nursery room axis, simulating acoustic inverse-square sound pressure decay and ambient room noise.
  * Emulates three ESP32-C3 nodes continuously capturing audio and broadcasting 100ms RMS telemetry over simulated ESP-NOW.
  * Emulates the Central Hub processing telemetry, selecting the candidate cot exceeding the $0.18$ RMS threshold, pulling 320 packets of audio, computing a 128-band Log-Mel spectrogram, and executing inference using the exported `tiny_crynet_2s.onnx` neural network.
  * Emulates the Caregiver Wearable receiving the BLE alert and validates cot identity matching.
* **`benchmark_cot_localization.py`**:
  * A 180-trial Monte-Carlo stress test designed to evaluate the spatial cot localization algorithm across challenging acoustic environments.
  * Sweeps six Signal-to-Noise Ratio (SNR) levels: $+20\text{ dB}$, $+15\text{ dB}$, $+10\text{ dB}$, $+5\text{ dB}$, $0\text{ dB}$, and $-5\text{ dB}$ (where background room noise is stronger than the infant cry).
  * Executes 30 independent trials per SNR level, generating a full Cot Confusion Matrix and calculating localization accuracy, false alarm rates, and noise robustness metrics for Chapter 4 of the project thesis.
* **`verify_dsp_parity.py`**:
  * A mathematical verification suite that establishes formal equivalence between the Python reference signal processing pipeline (using Librosa) and the embedded C++ DSP implementation (using ESP-DSP).
  * Evaluates Peak Absolute Error, Mean Squared Error (MSE), and Cosine Parity across the full 128-band Log-Mel spectrogram.
  * Demonstrates $>99.2\%$ cosine similarity and sub-0.001 MSE, proving that feature extraction on the microcontroller produces tensors identical to the cloud training environment.

---

### 4. `model_artifacts/` — Trained Weights & Runtime Checkpoints

This directory houses the trained neural network models, exported runtime graphs, and golden weight arrays ready for embedded compilation.

* **`tiny_crynet_2s.onnx`**:
  * The exported Open Neural Network Exchange (ONNX) computational graph (985 KB) for the 2.0-second TinyCryNet model.
  * Takes an input tensor of shape `[1, 1, 81, 128]` (representing 81 time frames and 128 Mel frequency bins) and outputs two-class softmax probabilities for infant cry detection.
  * Serves as the primary execution engine for desktop simulations and runtime verification.
* **`model_data.h`**:
  * A 6.15 MB C header file containing the trained neural network weight tensors represented as a constant unsigned byte array (`const uint8_t g_model[]`).
  * Placed into the read-only flash memory segment (`.rodata`) of the ESP32-S3, allowing the model to be compiled directly into firmware without consuming internal SRAM.
* **`kaggle_run_artifacts.tar.gz`**:
  * A 105 MB compressed archive containing the complete checkpoints, optimizer state dictionaries, training logs, loss curves, and evaluation metrics from the 35-epoch SpeechBrain training run on Kaggle GPUs. Tracked using Git Large File Storage (LFS).

---

### 5. `model_training/` — Deep Learning Architecture & Training Pipelines

This directory contains the deep learning source code, neural network architecture definitions, and automated cloud training scripts.

* **`tiny_crynet_esp32.py`**:
  * The authoritative PyTorch implementation of the `TinyCryNet_ESP32` convolutional neural network architecture.
  * Employs **Blueprint Separable Convolutions (`BSConvU`)**, which factorize standard 2D convolutions into a $1 \times 1$ pointwise projection followed by a $3 \times 3$ depthwise convolution. This reduces parameter count and multiply-accumulate operations by over 75% compared to standard CNNs.
  * Integrates **Contrast-Aware Channel Attention (`CCALayer`)**, which computes both spatial mean and spatial standard deviation (contrast) across feature maps. This mechanism emphasizes sharp harmonic infant cry vocalizations ($F_0 \approx 450\text{ Hz}$) while suppressing broadband background noise.
* **`reference_pipeline.py`**:
  * A Python module defining the reference 16 kHz Log-Mel spectrogram transformation (512-point FFT, 400-sample hop length, 128 triangular Mel filters, logarithmic dB compression). Serves as the numerical baseline for the embedded ESP-DSP implementation.
* **`kaggle_fast_train_drive_backup.ipynb` / `.py`**:
  * A complete training pipeline designed to run on Kaggle GPUs (T4 or P100). Automates dataset loading, data augmentation, SpeechBrain training loop execution for 35 epochs, ONNX export, and automatic backup of checkpoints to Google Drive.
* **`colab_fast_train_drive_backup.ipynb` / `.py`**:
  * The Google Colab equivalent of the cloud training notebook, providing identical training and export functionality.

---

### 6. `docs/` — Deployment Guides & Operational Manuals

This directory contains technical documentation and hardware setup procedures.

* **`SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md`**:
  * A master operational manual providing a detailed workspace inventory, step-by-step firmware flashing tables for all five boards, custom partition map configurations, a 4-phase bench testing roadmap, and pre-power-on electrical sanity checklists.

---

### 7. `ICD_MMSP/` — Academic Literature & Baseline Reference

This directory preserves the foundational research paper and reference codebase used as the academic baseline for this project.

* **`paper.pdf`**:
  * The published IEEE MMSP 2025 conference paper (*"Infant Cry Detection in Noisy Environment Using BSConv & TFRNN"*), establishing the baseline acoustic classification methodology.
* **`train_test.py` & `hparams.yaml`**:
  * The original authors' SpeechBrain implementation files, serving as the benchmark against which our edge-optimized TinyCryNet architecture is compared.
