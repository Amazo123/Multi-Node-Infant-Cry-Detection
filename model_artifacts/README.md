# 📦 Neural Network Weights & Model Artifacts

This directory stores the trained model checkpoints, converted runtime computational graphs, and compiled embedded C-array headers for the **TinyCryNet** infant cry detection model.

---

## 📁 Directory File Breakdown

| File Name | Size | Format | Role & Purpose |
| :--- | :--- | :--- | :--- |
| **`tiny_crynet_2s.onnx`** | 985 KB | ONNX Computational Graph | Exported trained neural network graph taking a 2.0-second Log-Mel spectrogram input `[1, 1, 81, 128]` and outputting two-class classification probabilities. Used for desktop simulation (`test_multi_node_simulation.py`) and fast verification. |
| **`model_data.h`** | 6.15 MB | C Header File (`const uint8_t[]`) | C-byte array representing the trained model weights. When compiled into the ESP32-S3 firmware, this header resides in read-only flash (`.rodata`), avoiding allocation from the 512 KB internal SRAM. |
| **`kaggle_run_artifacts.tar.gz`** | 105 MB | Compressed Tarball (Git LFS) | Complete checkpoint archive from the 35-epoch SpeechBrain training run on Kaggle GPUs. Contains PyTorch `.ckpt` model state dicts, optimizer states, training logs, loss curves, and hyperparameter snapshots. Tracked via Git Large File Storage (LFS). |

---

## ⚙️ Model Specification & Footprint

* **Framework:** PyTorch $\rightarrow$ ONNX $\rightarrow$ Embedded C-Array
* **Target Hardware:** ESP32-S3-DevKitC-1 N16R8 (Xtensa dual-core @ 240 MHz)
* **Flash Partition Requirement:** `partitions_16MB.csv` allocates a **6.0 MB** `app` partition to accommodate `model_data.h` alongside the core firmware and ILI9341 graphical display drivers.
* **Inference Execution Latency:** $\approx 40\text{ ms}$ on ESP32-S3.
