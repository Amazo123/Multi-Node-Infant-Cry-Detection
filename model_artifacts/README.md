# 📦 Neural Network Weights & Model Artifacts

This directory stores the trained deep learning weights, exported computational graphs, compiled embedded C-array headers, and training run checkpoint archives for the **TinyCryNet** infant cry detection model.

In embedded machine learning projects, model deployment requires managing multiple representations of the same neural network: an ONNX graph for desktop simulation and cross-platform validation, a raw C byte array for direct compilation into microcontroller flash memory, and a comprehensive archive of PyTorch training checkpoints for reproducibility. This directory unifies these assets under a single, version-controlled location.

---

## 📁 Detailed Directory Inventory & Asset Roles

### 1. `tiny_crynet_2s.onnx` (985 KB — ONNX Runtime Computational Graph)
* **Role & Function**:
  * An optimized computational graph of the trained `TinyCryNet_ESP32` neural network exported using the Open Neural Network Exchange (ONNX) standard.
  * Ingests a 2.0-second Log-Mel spectrogram input tensor of shape `[1, 1, 81, 128]` (representing 81 time frames and 128 Mel frequency bins) and outputs a two-dimensional classification probability vector `[1, 2]` corresponding to `[Non-Cry, Infant-Cry]`.
* **Primary Use Cases**:
  * Powers the Software-in-the-Loop simulator ([`simulation_and_benchmarks/test_multi_node_simulation.py`](../simulation_and_benchmarks/test_multi_node_simulation.py)) by providing high-speed inference on desktop CPUs without requiring a full PyTorch runtime.
  * Serves as the golden reference model for validating quantization, pruning, and embedded kernel conversion against desktop floating-point baselines.

---

### 2. `model_data.h` (6.15 MB — Embedded C Byte Array Header)
* **Role & Function**:
  * A compiled C header file that translates the trained weight tensors, bias vectors, and layer topology of TinyCryNet into a static, constant byte array:
    ```c
    const unsigned char g_model[] = {
        0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, ...
    };
    const unsigned int g_model_len = 6157199;
    ```
* **Microcontroller Memory Strategy**:
  * On the ESP32-S3-DevKitC-1 N16R8, internal SRAM is limited to 512 KB, which is insufficient to load a 6.15 MB model into volatile RAM.
  * By defining `g_model` as `const`, the compiler places the entire weight tensor structure into the **read-only flash segment (`.rodata`)**. The microcontroller's internal memory management unit (MMU) pages the weights into cache on demand during inference, consuming zero internal SRAM for static model storage.
  * The custom partition table ([`firmware/partitions_16MB.csv`](../firmware/partitions_16MB.csv)) dedicates a **6.0 MB application partition (`app0`)** to ensure that `model_data.h` compiles smoothly alongside the FreeRTOS kernel, ESP-DSP routines, and the ILI9341 display driver library.

---

### 3. `kaggle_run_artifacts.tar.gz` (105 MB — Complete Checkpoint Archive)
* **Role & Function**:
  * A compressed tarball containing the complete set of artifacts generated during the 35-epoch SpeechBrain training run on Kaggle GPUs (NVIDIA Tesla T4).
  * Managed and version-controlled via **Git Large File Storage (Git LFS)** to avoid bloating the primary git tree while preserving complete academic reproducibility.
* **Archive Contents**:
  * PyTorch model checkpoint files (`model.ckpt` and `best_model.ckpt`) containing complete parameter state dictionaries.
  * Optimizer state dictionaries (`optimizer.ckpt`) capturing AdamW momentum and variance buffers.
  * Learning rate scheduler states (`lr_scheduler.ckpt`).
  * Training logs, loss curves, validation accuracy tables, and hyperparameter snapshots (`hparams.yaml`).

---

## ⚙️ Summary Technical Specifications

| Parameter | Specification |
| :--- | :--- |
| **Model Architecture** | TinyCryNet (BSConvU + CCALayer Attention) |
| **Input Shape** | `[1, 1, 81, 128]` (Batch, Channel, Time Frames, Mel Bins) |
| **Input Feature Domain** | 16 kHz Log-Mel Spectrogram (dB scaled, $0 - 8,000\text{ Hz}$) |
| **Output Shape** | `[1, 2]` (`0: Non-Cry`, `1: Infant Cry`) |
| **Target Microcontroller** | ESP32-S3-WROOM-1 N16R8 (240 MHz Dual-Core Xtensa LX7) |
| **Flash Allocation** | 6.0 MB dedicated `app0` partition in `partitions_16MB.csv` |
| **Inference Latency** | $\approx 40\text{ ms}$ on ESP32-S3 hardware |
| **Quantization Format** | FP32 Golden Reference & INT8 Optimized Candidates |
