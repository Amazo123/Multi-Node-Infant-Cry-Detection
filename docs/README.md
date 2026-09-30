# 📖 System Documentation & Deployment Manuals

This directory contains the operational deployment guides, hardware assembly instructions, firmware flashing roadmaps, and bench verification manuals for the Multi-Node Infant Cry Detection System.

Bridging the gap between software simulation and physical hardware deployment requires rigorous, step-by-step procedures to ensure that hardware targets receive their designated codebases, correct partition mappings, and appropriate electrical checks. The documents in this directory serve as the standard operating procedure for the project.

---

## 📁 Detailed Directory Inventory & Document Roles

### 1. `SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md` — Master Deployment & Verification Roadmap
* **Role & Scope**:
  * Acts as the primary field and laboratory manual for assembling, flashing, testing, and troubleshooting the multi-node hardware setup.
* **Key Sections Covered**:
  1. *Workspace Inventory*: Exhaustive audit of all production files, explaining their purpose, hardware targets, and dependencies.
  2. *Hardware Deployment Matrix*: Explicit mapping table defining which source code folder must be flashed to which microcontroller, the associated PlatformIO environment name, and the required partition table.
  3. *Four-Phase Testing Protocol*:
     * **Phase 1: Pre-Power-On Electrical Verification**: Multimeter continuity checks to prevent VCC-to-GND dead shorts, Li-Po connector polarity validation, and verifying that the high-speed Octal PSRAM pins on the ESP32-S3 (`GPIO 33–37`) remain completely isolated.
     * **Phase 2: Standalone Component Bring-Up**: Utilizing the five specialized diagnostic tools in `firmware/diagnostics/` to independently verify the INMP441 digital microphone, ESP-NOW RF range, wearable haptic motor and OLED display, battery divider voltage measurement, and ILI9341 SPI color TFT display.
     * **Phase 3: Multi-Cot Staged Network Integration**: Incrementally bringing up Cot Node 1, observing Central Hub candidate selection, adding Cot Nodes 2 and 3, and verifying acoustic localization under spatial sound decay.
     * **Phase 4: Full System E2E Validation**: Testing the end-to-end pipeline from an infant cry acoustic stimulus to wearable haptic vibration and verifying that total latency remains well below the $1.5\text{ s}$ specification limit.
  4. *Comprehensive Troubleshooting Matrix*: Detailed solutions for common embedded pitfalls, including I2C bus hangs, unhandled bootloader strapping errors on GPIO 9, PSRAM memory corruption, and I2S audio clipping.

---

## 🗺️ Recommended Academic & Engineering Reading Progression

For researchers, evaluators, and engineers exploring this repository, the following reading progression provides the most logical technical flow:

1. **Executive Project Overview**: Begin with the root [`README.md`](../README.md) to understand the project motivation, distributed 5-controller topology, two-tier ESP-NOW mechanics, and real-time latency budgets.
2. **Directory & File Manifest**: Inspect [`DIRECTORY_MAP.md`](../DIRECTORY_MAP.md) for a comprehensive breakdown of every file, line reference, and component role across the workspace.
3. **Hardware Schematics & Wiring**: Review [`firmware/HARDWARE_SCHEMATICS.md`](../firmware/HARDWARE_SCHEMATICS.md) and open [`hardware_images/VIEW_ALL_SCHEMATICS.html`](../hardware_images/VIEW_ALL_SCHEMATICS.html) in a browser to examine the photorealistic breadboard layouts and vector schematics.
4. **Physical Assembly & Flashing**: Follow [`SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md`](SYSTEM_DEPLOYMENT_AND_TESTING_GUIDE.md) when preparing to flash firmwares using PlatformIO or Arduino IDE.
5. **Standalone Module Diagnostics**: Consult [`firmware/diagnostics/README.md`](../firmware/diagnostics/README.md) to test each hardware sensor and display individually prior to network integration.
6. **Mathematical Verification & Simulation**: Review [`simulation_and_benchmarks/README.md`](../simulation_and_benchmarks/README.md) and execute the Software-in-the-Loop simulator and Monte-Carlo benchmarks to inspect empirical results.
7. **Neural Network Architecture & Training**: Study [`model_training/README.md`](../model_training/README.md) to understand the Blueprint Separable Convolution and Contrast-Aware Attention design.
