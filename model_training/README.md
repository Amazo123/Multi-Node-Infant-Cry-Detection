# 🧠 Model Training & Deep Learning Architecture

This directory houses the neural network architecture definition, audio signal processing reference routines, and end-to-end cloud training pipelines for the **TinyCryNet** edge AI model.

---

## 📁 Directory File Breakdown

| File Name | Format | Role & Purpose |
| :--- | :--- | :--- |
| **`tiny_crynet_esp32.py`** | Python (PyTorch) | Authoritative neural network architecture definition for `TinyCryNet_ESP32`. Implements Blueprint Separable Convolutions (`BSConvU`) and Contrast-Aware Channel Attention (`CCALayer`) to deliver high acoustic precision with minimal computational overhead. |
| **`reference_pipeline.py`** | Python (Librosa / NumPy) | Standalone reference implementation of the 16 kHz Log-Mel spectrogram pipeline (512-point FFT, 400-sample hop, 128 triangular Mel filters, log dB compression) matching the embedded ESP-DSP routines on the ESP32-S3. |
| **`kaggle_fast_train_drive_backup.ipynb`** | Jupyter Notebook | Turnkey 35-epoch training notebook executed on Kaggle GPUs (T4/P100). Implements SpeechBrain training, data augmentation, ONNX export, and automated backup to Google Drive. |
| **`kaggle_fast_train_drive_backup.py`** | Python Script | Exported command-line script equivalent of the Kaggle training notebook for headless execution. |
| **`colab_fast_train_drive_backup.ipynb`** | Jupyter Notebook | Google Colab equivalent of the fast training pipeline with Google Drive auto-mounting and synchronization. |
| **`colab_fast_train_drive_backup.py`** | Python Script | Headless Python script version of the Google Colab training suite. |

---

## 🔬 Architectural Highlights: Why TinyCryNet?

Standard 2D Convolutional Neural Networks (CNNs) are too computationally heavy to execute within the strict $<1.5\text{ s}$ latency ceiling of an embedded microcontroller like the ESP32-S3. TinyCryNet achieves edge efficiency through two key mechanisms:

1. **Blueprint Separable Convolutions (`BSConvU`)**:
   * Replaces standard $3 \times 3$ convolutions with a $1 \times 1$ pointwise projection followed by a $3 \times 3$ depthwise convolution.
   * Reduces parameter count and Multiply-Accumulate (MAC) operations by over **$75\%$** compared to standard CNN layers while retaining high representational fidelity.

2. **Contrast-Aware Channel Attention (`CCALayer`)**:
   * Combines standard spatial average pooling with spatial standard deviation (contrast).
   * Infant distress cries are characterized by distinct fundamental frequencies ($F_0 \approx 400 - 500\text{ Hz}$) and sharp harmonic peaks. Contrast attention amplifies these spectral peaks while suppressing diffuse ambient noise (fans, air conditioners, distant doors).

---

## ⚡ Input / Output Tensor Specification

* **Input Tensor Shape:** `[1, 1, 81, 128]`
  * `Batch Size`: 1
  * `Channels`: 1 (Monophonic spectrogram)
  * `Time Frames`: 81 frames ($\approx 2.0\text{ seconds}$ at 16 kHz with 400-sample hop length)
  * `Frequency Bins`: 128 Mel bands ($0 - 8,000\text{ Hz}$)
* **Output Tensor Shape:** `[1, 2]`
  * `Class 0`: Non-Cry (Ambient nursery sounds, speech, babble, silence)
  * `Class 1`: Infant Cry (Distress, hunger, pain vocalizations)
