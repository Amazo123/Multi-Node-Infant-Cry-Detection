# Multi-Node Infant Cry Detection System with Wearable Caregiver Alert
### Final Year Project — Computer Engineering Department, University of Maiduguri
**Author:** Amazing Ayomikun Adelowo (22/05/04/0029)  
**System Architecture:** 3× Cot Sensing Nodes (ESP32-C3) + 1× Central Hub (ESP32-S3 N16R8) + 1× Caregiver Wearable (ESP32-C3 SuperMini)

---

## 📁 Repository Directory Structure

The repository is modularly organized into distinct, specialized subsystems:

```
Multi-node/
│
├── 📁 firmware/                      <-- Production C/C++ Embedded Code & Hardware Schematics
│   ├── central_hub_s3/               <-- ESP32-S3 Hub Firmware (2-Tier Pull, ESP-DSP, TinyCryNet, ILI9341 TFT)
│   ├── node_firmware/                <-- ESP32-C3 Cot Nodes 1, 2, 3 Firmware (DMA Ring Buffer & ESP-NOW Streamer)
│   ├── wearable_firmware/            <-- ESP32-C3 SuperMini Caregiver Wristband Firmware (BLE & Haptic Alert)
│   ├── common/                       <-- Shared protocol.h packet headers across all microcontrollers
│   ├── diagnostics/                  <-- 5 Standalone hardware checkout sketches (mic, range, OLED, TFT, LEDs)
│   ├── platformio.ini                <-- Multi-target PlatformIO build configuration
│   ├── partitions_16MB.csv           <-- 16MB flash partition table for ESP32-S3 (6MB app partition)
│   ├── partitions_4MB.csv            <-- 4MB flash partition table for ESP32-C3
│   └── HARDWARE_SCHEMATICS.md        <-- Complete electrical pinout, strapping safety & wiring schematics
│
├── 📁 simulation_and_benchmarks/     <-- Python Simulation & Mathematical Parity Test Suite
│   ├── test_multi_node_simulation.py <-- Software-in-the-Loop (SIL) 5-controller multi-cot simulator
│   ├── benchmark_cot_localization.py <-- 180-trial Monte-Carlo stress test (100% spatial accuracy down to -5dB SNR)
│   └── verify_dsp_parity.py          <-- DSP mathematical parity suite (>99.2% cosine similarity Python vs C++)
│
├── 📁 model_training/                <-- Deep Learning Pipeline & Retraining Scripts
│   ├── tiny_crynet_esp32.py          <-- PyTorch BSConvU + CCALayer neural network architecture definition
│   ├── reference_pipeline.py         <-- 16 kHz Log-Mel spectrogram reference generator
│   ├── kaggle_fast_train_drive_backup.ipynb / .py <-- Kaggle 35-epoch SpeechBrain training suite with Drive backup
│   └── colab_fast_train_drive_backup.ipynb / .py  <-- Google Colab equivalent training suite
│
├── 📁 model_artifacts/               <-- Trained Neural Network Weights & Checkpoints
│   ├── tiny_crynet_2s.onnx           <-- Exported ONNX graph (input shape: [1, 1, 81, 128])
│   ├── model_data.h                  <-- 6.15 MB C-array model weights compiled into ESP32-S3 flash
│   └── kaggle_run_artifacts.tar.gz   <-- Complete trained SpeechBrain checkpoint archive
│
├── 📁 hardware_images/               <-- Visual Breadboard Wiring Diagrams & Board Photos
│   ├── central_hub_full_wiring.jpg   <-- Complete Central Hub (S3 + ILI9341 TFT + TP4056 + 5V Boost + 3000mAh)
│   ├── cot_node_wiring.jpg           <-- Cot Node (C3 + INMP441 + 2 LEDs + Battery Divider)
│   ├── wearable_wiring.jpg           <-- Wearable (C3 SuperMini + SSD1306 OLED + IRLML2502TR + Coin Motor)
│   └── ...                           <-- Component workbench layout photos
│
├── 📁 docs/                          <-- Master Technical Guides & Deployment Manuals
│   └── SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md <-- Master hardware flashing, bring-up, and test roadmap
│
└── 📁 ICD_MMSP/                      <-- Academic Baseline Literature & Research Paper
    └── paper.pdf                     <-- Published IEEE MMSP 2025 baseline paper
```

---

## ⚡ Quick Start Navigation

1. **Wiring Hardware on the Breadboard**:
   * Open [`firmware/HARDWARE_SCHEMATICS.md`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/HARDWARE_SCHEMATICS.md) or inspect visual wiring diagrams in [`hardware_images/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/hardware_images/).
2. **Testing Hardware Modules Before Assembly**:
   * Refer to [`firmware/diagnostics/README.md`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/README.md) and run individual checkout sketches in [`firmware/diagnostics/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/).
3. **Flashing Firmware to Microcontrollers**:
   * Read the step-by-step flashing guide in [`docs/SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/docs/SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md).
4. **Running Software-in-the-Loop Simulation**:
   ```bash
   cd simulation_and_benchmarks
   python test_multi_node_simulation.py
   python benchmark_cot_localization.py
   python verify_dsp_parity.py
   ```
