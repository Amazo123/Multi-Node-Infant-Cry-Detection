# 🧠 Model Training & Deep Learning Architecture

This directory houses the neural network architecture definition, audio signal processing reference routines, and automated cloud training pipelines for the **TinyCryNet** infant cry detection model.

Executing deep learning models on resource-constrained microcontrollers presents severe memory and computational challenges. While desktop audio classification models frequently utilize multi-million parameter architectures (such as ResNet-50 or Audio Spectrogram Transformers), the ESP32-S3 microcontroller operates with limited internal SRAM (512 KB) and a strict latency budget ($\le 1.1\text{ s}$). The codebase in this directory defines, trains, and exports an ultra-compact, attention-augmented convolutional neural network designed specifically for edge deployment.

---

## 📁 Detailed Directory Inventory & Module Roles

### 1. `tiny_crynet_esp32.py` — Edge-Optimized Neural Network Architecture
* **Purpose**: Provides the complete PyTorch definition of the `TinyCryNet_ESP32` convolutional neural network.
* **Core Architectural Innovations**:
  1. **Blueprint Separable Convolutions (`BSConvU`)**:
     * Standard 2D convolutions simultaneously perform spatial filtering and channel cross-correlation, which is computationally expensive.
     * Blueprint Separable Convolutions invert standard depthwise-separable convolutions by performing a $1 \times 1$ pointwise convolution first to project cross-channel representations, followed by a $3 \times 3$ depthwise convolution for spatial filtering.
     * This mathematical factorisation reduces parameter count and Multiply-Accumulate (MAC) operations by over **75%** compared to standard convolutional layers while preserving high representational capacity.
  2. **Contrast-Aware Channel Attention (`CCALayer`)**:
     * Infant cries exhibit sharp, highly distinct harmonic structures centered around a fundamental frequency ($F_0 \approx 400 - 500\text{ Hz}$) and its integer multiples.
     * Standard channel attention mechanisms (like Squeeze-and-Excitation) rely exclusively on Global Average Pooling (GAP), which smooths out high-frequency spectral peaks.
     * The `CCALayer` computes both the spatial mean and the **spatial standard deviation (contrast)** across feature maps:
       $$\text{Contrast}(X) = \sqrt{\frac{1}{H \cdot W} \sum (X - \mu)^2 + \epsilon}$$
     * Combining contrast with global average pooling allows the network to emphasize the sharp harmonic signatures of an infant cry while dynamically suppressing diffuse, broadband background sounds like fans, air conditioners, or distant chatter.
  3. **Multi-Scale Feature Hierarchy**:
     * Structured as a progressive 4-stage hierarchy that downsamples the 128 Mel frequency bins into compact, semantically rich acoustic embeddings before feeding a final classification head.

---

### 2. `reference_pipeline.py` — Audio Signal Processing Reference Routine
* **Purpose**: Serves as the authoritative floating-point DSP reference implementation for converting raw 16 kHz monophonic PCM audio into normalized Log-Mel spectrograms.
* **Mathematical Transformation Parameters**:
  * **Sampling Rate**: $16,000\text{ Hz}$ (16-bit mono PCM).
  * **Analysis Window Length**: $32,000\text{ samples}$ ($2.0\text{ seconds}$).
  * **Short-Time Fourier Transform (STFT)**: $512\text{-point}$ FFT with a symmetric Hann window.
  * **Hop Length**: $400\text{ samples}$ ($25\text{ ms}$ frame step).
  * **Mel Filterbank**: $128\text{ triangular filters}$ distributed across the $0 - 8,000\text{ Hz}$ frequency range.
  * **Dynamic Range Compression**: Power spectrum converted to logarithmic decibels ($10 \cdot \log_{10}(\text{Power} + 10^{-6})$) normalized across an $80\text{ dB}$ dynamic ceiling.
* **Role in System Parity**: This Python script generates the reference feature tensors against which the embedded ESP-DSP fixed-point C++ code is compared to ensure numerical parity.

---

### 3. `kaggle_fast_train_drive_backup.ipynb` & `.py` — Cloud Training Pipeline
* **Purpose**: A self-contained, automated deep learning training suite designed to execute on Kaggle GPU instances (NVIDIA Tesla T4 or P100).
* **Key Automated Functions**:
  1. *Environment Setup & Dependency Resolution*: Automatically installs SpeechBrain, HyperPyYAML, Librosa, ONNX, and PyDrive2 without user intervention.
  2. *Dataset Ingestion*: Clones and structures the official infant cry dataset (including augmented background noise from the ESC-50 environmental sound database).
  3. *SpeechBrain Training Loop*: Executes a 35-epoch training schedule utilizing AdamW optimization, cosine annealing learning rate decay, and cross-entropy loss with label smoothing.
  4. *Automated Checkpointing to Google Drive*: Connects via PyDrive2 to continuously upload model checkpoints (`.ckpt`), optimizer state dicts, and training logs to a dedicated folder on Google Drive (`MyDrive/TinyCryNet_Kaggle_Project`), safeguarding progress against Kaggle session timeouts.
  5. *ONNX Graph Exportation*: Traces the trained PyTorch model and exports the optimized computational graph to `tiny_crynet_2s.onnx`.

---

### 4. `colab_fast_train_drive_backup.ipynb` & `.py` — Google Colab Alternative Pipeline
* **Purpose**: Provides an identical turnkey training pipeline tailored for Google Colab environments.
* **Features**: Incorporates native Google Colab Drive mounting (`google.colab.drive`), GPU runtime checks, and local file caching for seamless reproduction on Google's cloud platform.

---

## ⚡ Input / Output Tensor Specification

* **Input Tensor Shape:** `[1, 1, 81, 128]`
  * `Batch Size`: 1 (single-sample real-time edge inference)
  * `Channels`: 1 (monophonic acoustic spectrogram)
  * `Time Frames`: 81 frames ($\approx 2.0\text{ seconds}$ at 16 kHz with 400-sample hop length)
  * `Frequency Bins`: 128 Mel bands ($0 - 8,000\text{ Hz}$)
* **Output Tensor Shape:** `[1, 2]`
  * `Index 0`: Non-Cry (ambient nursery sounds, adult speech, babble, appliance hum, silence)
  * `Index 1`: Infant Cry (distress, pain, hunger vocalizations)
* **Softmax Decision Threshold**: A confidence score $> 0.70$ on Class 1 triggers the high-priority caregiver alert.
