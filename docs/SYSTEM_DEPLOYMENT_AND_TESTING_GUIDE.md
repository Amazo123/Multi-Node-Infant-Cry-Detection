# Multi-Node Infant Cry Detection System
## Master Workspace Inventory, Hardware Deployment & Testing Guide

**Author / Candidate**: Amazing Ayomikun Adelowo (22/05/04/0029)  
**Degree**: B.Eng. Computer Engineering, University of Maiduguri  
**System Target**: 3× Cot Sensing Nodes (ESP32-C3) + 1× Central Hub (ESP32-S3 N16R8) + 1× Wearable Wristband (ESP32-C3-MINI)

---

## Part 1: Workspace Inventory — What is What

Every obsolete draft and redundant file has been pruned. Here is the verified role of every file and directory in your workspace:

### 📁 Root Directory (`c:\Users\HP\Documents\Antigravity\Multi-node\`)

| File / Folder Name | Role / Purpose | Kept For |
| :--- | :--- | :--- |
| **`firmware/`** | Production firmware source code, pin schematics, diagnostic tools, and compilation profiles for all 5 microcontrollers. | **Hardware Flashing** |
| **`model_artifacts/`** | Archive containing the complete trained weights (`kaggle_run_artifacts.tar.gz`), exported `tiny_crynet_2s.onnx` graph, and `model_data.h`. | **Golden Model Backup & Firmware Compilation** |
| **`model_training/`** | PyTorch model architecture (`tiny_crynet_esp32.py`), reference DSP pipeline, and cloud training pipelines (Kaggle & Colab notebooks/scripts). | **Model Retraining & Reproduction** |
| **`simulation_and_benchmarks/`** | Software-in-the-Loop 5-controller simulator, 180-trial Monte-Carlo cot localization benchmark, and DSP parity verification suites. | **Pre-hardware E2E Verification & Thesis Benchmarks** |
| **`hardware_images/`** | Visual $2200 \times 1700$ breadboard wiring diagrams, vector SVGs, and interactive HTML viewer (`VIEW_ALL_SCHEMATICS.html`). | **Physical Assembly Reference** |
| **`docs/`** | Master operational guides, deployment checklists, and testing roadmaps. | **Documentation** |
| **`ICD_MMSP/`** | Baseline repository of the published IEEE MMSP 2025 paper (*"Infant Cry Detection in Noisy Environment Using BSConv & TFRNN"*), including `paper.pdf`. | **Academic Literature & Benchmark Baseline** |

---

### 📁 Firmware Directory (`firmware/`)

| File / Folder Name | Target Hardware | Purpose |
| :--- | :--- | :--- |
| **`common/protocol.h`** | Shared across all boards | Defines the ESP-NOW packet structures (`telemetry_packet_t`, `audio_req_packet_t`, `audio_frag_packet_t`, `wearable_alert_packet_t`). |
| **`node_firmware/node_firmware.ino`** | 3× ESP32-C3-DevKitC-02 (Cots 1, 2, 3) | 16 kHz I2S DMA circular buffer, 100ms RMS calculation, battery ADC on GPIO 1, 2-LED indicator logic, and 320-fragment audio burst. |
| **`central_hub_s3/`** | 1× ESP32-S3-DevKitC-1 (N16R8) | Central hub logic: Candidate cot selector, 320-fragment reassembly into PSRAM, ESP-DSP Log-Mel extraction, TinyCryNet inference, 2.4" ILI9341 SPI TFT dashboard, and BLE GATT server. Contains `model_data.h`. |
| **`wearable_firmware/wearable_firmware.ino`** | 1× ESP32-C3-MINI-1 | Caregiver wristband: BLE client, SSD1306 0.96" OLED cot alert renderer, and AO3400A pulsed vibration driver. |
| **`diagnostics/`** | Workbench Test Tools | Standalone single-feature checkout sketches to test mic, range, screen, haptics, LEDs, and battery before full assembly. |
| **`platformio.ini`** | Build Configuration | Multi-environment build definition for VSCode / PlatformIO with Octal PSRAM/Flash settings. |
| **`partitions_16MB.csv`** | Central Hub (ESP32-S3) | Custom 16MB partition map allocating 6.0 MB for the app binary to fit `model_data.h`. |
| **`partitions_4MB.csv`** | Cot Nodes & Wearable (ESP32-C3) | Custom 4MB partition map maximizing internal application headroom. |
| **`HARDWARE_SCHEMATICS.md`** | Reference Document | Exact electrical wiring tables, resistor divider values, MOSFET connections, and strapping pin safety proofs. |
| **`README.md`** | Reference Document | Overview of firmware architecture and two-tier pulling mechanics. |

---

## Part 2: Deployment Matrix — What Files Go Where

When you have your physical hardware connected via USB-C, follow this table:

```
+-----------------------------------------------------------------------------------------------+
| DEVICE ROLE           | HARDWARE BOARD           | SOURCE CODE PATH            | PARTITION    |
+-----------------------------------------------------------------------------------------------+
| Cot Node 1            | ESP32-C3-DevKitC-02      | firmware/node_firmware/     | 4MB Default  |
|                       |                          | (Set CURRENT_NODE_ID = 1)   |              |
+-----------------------------------------------------------------------------------------------+
| Cot Node 2            | ESP32-C3-DevKitC-02      | firmware/node_firmware/     | 4MB Default  |
|                       |                          | (Set CURRENT_NODE_ID = 2)   |              |
+-----------------------------------------------------------------------------------------------+
| Cot Node 3            | ESP32-C3-DevKitC-02      | firmware/node_firmware/     | 4MB Default  |
|                       |                          | (Set CURRENT_NODE_ID = 3)   |              |
+-----------------------------------------------------------------------------------------------+
| Central Hub           | ESP32-S3-DevKitC-1 N16R8 | firmware/central_hub_s3/    | 16MB Custom  |
|                       |                          | (Uses model_data.h)         |              |
+-----------------------------------------------------------------------------------------------+
| Caregiver Wearable    | ESP32-C3-MINI-1          | firmware/wearable_firmware/ | 4MB Default  |
+-----------------------------------------------------------------------------------------------+
```

### Critical Flashing Settings in Arduino IDE (or PlatformIO):

1. **For the Central Hub (ESP32-S3)**:
   * Board: `ESP32S3 Dev Module`
   * Flash Size: `16MB (128Mb)`
   * PSRAM: `OPI PSRAM` (or `Octal PSRAM`)
   * Partition Scheme: Custom (`partitions_16MB.csv`) or `16M Flash (3MB APP/9.9MB FAT)`
   * Flash Mode: `QIO 80MHz`
   * USB CDC On Boot: `Enabled` (for Serial Monitor output over USB-C)

2. **For the Cot Nodes & Wearable (ESP32-C3)**:
   * Board: `ESP32C3 Dev Module`
   * Flash Size: `4MB (32Mb)`
   * Partition Scheme: `Default 4MB with spiffs` or `Huge APP (3MB No OTA)`
   * USB CDC On Boot: `Enabled`

---

## Part 3: Step-by-Step Testing & Verification Roadmap

Never plug in all 5 boards at once without prior testing. Follow this 4-phase testing protocol:

```
[Phase 1: Workbench Diagnostics]  -->  [Phase 2: Point-to-Point RF Link]
               |                                      |
               v                                      v
[Phase 3: 2-Tier Audio Pull Test] -->  [Phase 4: Full Multi-Cot Stress Test]
```

---

### 🧪 Phase 1: Workbench Diagnostic Checkouts (1 Board at a Time)

Use the sketches in [`firmware/diagnostics/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/):

#### Step 1.1: Verify INMP441 Microphone on Cot Node 1
1. Wire INMP441: `SCK -> GPIO5`, `WS -> GPIO6`, `SD -> GPIO7`, `L/R -> GND`, `VDD -> 3.3V`, `GND -> GND`.
2. Flash [`01_inmp441_audio_plotter.ino`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/01_inmp441_audio_plotter/01_inmp441_audio_plotter.ino).
3. Open **Tools -> Serial Plotter** (115200 baud).
4. **Verification**: In quiet air, waveform hovers near zero. Clap or snap fingers; waveform immediately spikes with clear sinusoidal oscillations.

#### Step 1.2: Verify Battery ADC & 2-LED System on Cot Node 1
1. Wire Red LED on `GPIO 4` (Active-HIGH) and Blue LED on `GPIO 9` (Active-LOW).
2. Wire $100\text{ k}\Omega + 100\text{ k}\Omega$ divider to `GPIO 1`.
3. Flash [`04_node_led_battery_test.ino`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/04_node_led_battery_test/04_node_led_battery_test.ino).
4. **Verification**: 
   * Board boots cleanly without getting stuck in ROM download mode.
   * Red LED executes: Solid ON -> Slow Blink -> Fast Blink.
   * Blue LED lights up bright when pulled LOW.
   * Serial Monitor displays measured voltage matching a multimeter reading ($\pm 0.05\text{ V}$).

#### Step 1.3: Verify Wearable Haptic Motor & OLED
1. Wire OLED (`SDA -> GPIO 4`, `SCL -> GPIO 5`) and Motor (`Gate -> GPIO 3`).
2. Flash [`03_wearable_haptic_oled_test.ino`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/03_wearable_haptic_oled_test/03_wearable_haptic_oled_test.ino).
3. **Verification**: 
   * OLED initializes at `0x3C` and renders inverted alert banner `COT 01 CRY!`.
   * Motor pulses 3 times sharply without causing the ESP32-C3 to reboot or brown out.

---

### 🧪 Phase 2: RF Range & Packet Delivery Testing

1. Flash [`esp_now_responder_probe.ino`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/02_esp_now_range_probe/esp_now_responder_probe.ino) to the ESP32-S3 Central Hub.
2. Flash [`esp_now_sender_probe.ino`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/02_esp_now_range_probe/esp_now_sender_probe.ino) to an ESP32-C3 Cot Node powered by a power bank.
3. Open Serial Monitor on the sender node and walk through your testing building (across rooms and walls).
4. **Verification**: 
   * Round-Trip Time (RTT) remains $< 5\text{ ms}$.
   * Packet Delivery Ratio (PDR) stays $\ge 98\%$ through at least 2 concrete walls.

---

### 🧪 Phase 3: Central Hub Audio Pull & Spectrogram Pipeline

1. Flash `firmware/central_hub_s3/central_hub_s3.ino` to the ESP32-S3 Hub.
2. Flash `firmware/node_firmware/node_firmware.ino` to Cot Node 1 (`CURRENT_NODE_ID = 1`).
3. Open Serial Monitor on the Central Hub (115200 baud).
4. Make a loud cry sound (or play an audio clip from your phone next to Node 1's microphone).
5. **Expected Output on Hub Serial**:
   ```text
   ⚡ [ACOUSTIC SPIKE DETECTED] Peak: Cot 1 (RMS: 0.342) > Threshold (0.180)
   📡 Requested 64 KB Audio from Cot 1 (Session 101)...
   ✅ [REASSEMBLY 100% COMPLETE] 320 fragments (64,000 bytes) received in 94 ms!
   🧠 [ML INFERENCE RESULT] Cot 1 Cry Probability: 95.4% (Proc: 42ms)
   🚨🚨🚨 [GENUINE INFANT CRY CONFIRMED AT COT 1] 🚨🚨🚨
   ```

---

### 🧪 Phase 4: Full End-to-End Multi-Cot Localization & Wearable Alert

1. Flash all 3 cot nodes with their respective IDs:
   * Node 1: `CURRENT_NODE_ID = 1`
   * Node 2: `CURRENT_NODE_ID = 2`
   * Node 3: `CURRENT_NODE_ID = 3`
2. Flash `central_hub_s3.ino` to the Hub.
3. Flash `wearable_firmware.ino` to the Wristband.
4. Place the 3 cot nodes on tables separated by $1.5\text{ m} - 2\text{ m}$.
5. Power up the wearable wristband; hub serial confirms: `⌚ [BLE] Caregiver Wearable Connected!`.
6. **Scenario A (Play Cry Sound at Cot 2)**:
   * Node 2 measures highest amplitude ($\text{RMS} \approx 0.38$), while Node 1 and 3 measure weaker acoustic reflections ($\text{RMS} \approx 0.12$).
   * Hub identifies **Cot 2** as candidate, pulls 64 KB audio from Cot 2, runs TinyCryNet, confirms cry.
   * Caregiver wristband vibrates with 3 pulses and OLED flashes:
     ```
     +--------------------------+
     | STATUS: ALERT!           |
     | [ COT 02 CRY DETECTED! ] |
     | Conf: 95% | Latency: 0.9s|
     +--------------------------+
     ```
7. **Scenario B (Non-Cry Disturbance - e.g. Dropping Keys near Cot 1)**:
   * Acoustic spike triggers audio pull from Cot 1.
   * Central Hub runs TinyCryNet; inference returns low cry probability ($< 15\%$).
   * Hub logs: `ℹ️ Event dismissed: Noise / Non-Cry sound.`
   * Wearable remains silent on standby, proving **100% false alarm rejection**.
