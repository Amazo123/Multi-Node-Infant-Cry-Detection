# 📊 Simulation, Verification & Benchmark Suite

This directory contains the Python-based Software-in-the-Loop (SIL) simulation framework, empirical spatial localization benchmarks, and mathematical DSP parity suites for the Multi-Node Infant Cry Detection System.

These tools allow complete mathematical validation of the system's distributed logic before and during physical hardware testing, providing concrete numerical evidence for **Chapter 4 (Results and Discussion)** of the project thesis.

---

## 📁 Directory File Breakdown

### 1. `test_multi_node_simulation.py` — Software-in-the-Loop (SIL) 5-Node Simulator
* **Purpose:** Simulates the entire 5-controller distributed ecosystem in pure software without physical hardware.
* **Physics & Network Modeling:**
  * Models three cots positioned at $0\text{ m}$ (Cot 1), $2\text{ m}$ (Cot 2), and $4\text{ m}$ (Cot 3) along a nursery room axis with realistic $1/d$ acoustic pressure decay.
  * Simulates 3× ESP32-C3 cot nodes capturing 16 kHz audio via a circular DMA ring buffer and broadcasting 100ms RMS acoustic energy telemetry.
  * Models the ESP32-S3 Central Hub receiving telemetry, selecting the loudest candidate cot above threshold ($0.18$), issuing an `AUDIO_REQ` packet, receiving 320 packets (64 KB) of audio, extracting a 128-band Log-Mel spectrogram, and executing ONNX neural network inference against `tiny_crynet_2s.onnx`.
  * Simulates BLE GATT transmission to the Caregiver Wristband and validates cot alert matching.
* **Execution:**
  ```bash
  python test_multi_node_simulation.py
  ```

---

### 2. `benchmark_cot_localization.py` — Cot Localization Stress Test
* **Purpose:** Performs a 180-trial Monte-Carlo stress test evaluating spatial cot localization accuracy across varying acoustic signal-to-noise ratios (SNR).
* **Experimental Matrix:**
  * Sweeps 6 SNR levels: $+20\text{ dB}$ (quiet studio), $+15\text{ dB}$, $+10\text{ dB}$, $+5\text{ dB}$, $0\text{ dB}$ (equal cry and room noise), and $-5\text{ dB}$ (severe background chatter/crying).
  * Executes 30 trials per SNR level with randomly positioned cry events across Cots 1, 2, and 3.
  * Computes a full Cot Confusion Matrix and empirical accuracy metrics for Chapter 4 of the project thesis.
* **Execution:**
  ```bash
  python benchmark_cot_localization.py
  ```

---

### 3. `verify_dsp_parity.py` — Embedded vs Reference DSP Mathematical Parity Suite
* **Purpose:** Implements the formal verification protocol required by Section 36 of the project handover: proves that the embedded C++ fixed-point DSP algorithm implemented in `firmware/` produces mathematical outputs equivalent to the Python floating-point Librosa reference pipeline.
* **Metrics Evaluated:**
  * Peak Absolute Error ($\max |y_{\text{ref}} - y_{\text{emb}}|$)
  * Mean Squared Error (MSE)
  * Cosine Parity / Similarity ($\frac{\vec{u} \cdot \vec{v}}{\|\vec{u}\| \|\vec{v}\|}$)
* **Target Milestone:** Consistently demonstrates $>99.2\%$ cosine similarity and sub-0.001 MSE across the full 128-band Log-Mel spectrogram space.
* **Execution:**
  ```bash
  python verify_dsp_parity.py
  ```
