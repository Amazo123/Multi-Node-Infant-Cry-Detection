# 🖼️ Hardware Schematics, Wiring Diagrams & Visual Assets

This directory contains the authoritative visual wiring diagrams, trace-accurate breadboard schematics, vector diagrams, and algorithmic rendering tools for the Multi-Node Infant Cry Detection System.

When assembling embedded hardware, relying on generic connection tables often leads to layout errors, loose jumper wires, or shorted power rails. The assets in this directory eliminate guesswork by providing complete, photorealistic breadboard renderings and vector schematics reflecting the **true physical wiring** of all five microcontrollers and their associated passive components.

---

## 📁 Detailed Directory Inventory & Asset Roles

### 1. `central_hub_breadboard_wiring.png` (High-Resolution $2200 \times 1700$ Bitmap)
* **Subsystem**: Central Intelligence Hub.
* **Core Components Shown**:
  * **ESP32-S3-DevKitC-1 N16R8**: Dual-core 240 MHz microcontroller positioned across the breadboard center trough.
  * **2.4-inch ILI9341 SPI Color TFT Display**: Wired over the high-speed SPI2 (FSPI) bus using pins `GPIO 10` (CS), `GPIO 11` (MOSI), `GPIO 12` (SCK), `GPIO 13` (DC/RS), and `GPIO 14` (RESET). The backlight LED pin is wired through a $100\Omega$ current-limiting resistor to the 3.3V rail.
  * **Power Conditioning Stage**: Shows a 3.7V 3000mAh single-cell Li-Ion battery feeding a TP4056 USB-C charging module, whose output is stepped up to 5V via an MT3608 boost converter to power the ESP32-S3 `5V/VIN` pin and display regulator.
  * **Decoupling Capacitors**: Illustrates a $100\mu\text{F}$ electrolytic capacitor placed across the 5V power rails and a $100\text{ nF}$ ceramic capacitor across 3.3V to absorb RF transmit current spikes.
  * **Important Design Note**: The audible buzzer has been intentionally omitted from this schematic. In pediatric hospital and domestic nursery environments, high-decibel buzzers disturb resting infants. All alerts are routed silently to the caregiver's wearable wristband and rendered graphically on the color TFT screen.

### 2. `cot_node_breadboard_wiring.png` (High-Resolution $2200 \times 1700$ Bitmap)
* **Subsystem**: Cot Sensing Nodes (Cot 1, Cot 2, and Cot 3).
* **Core Components Shown**:
  * **ESP32-C3-DevKitC-02**: RISC-V 160 MHz microcontroller mounted on the breadboard.
  * **INMP441 Digital MEMS Microphone Breakout**: Connected via I2S digital audio lines: `SCK` to `GPIO 5`, `WS` to `GPIO 6`, and `SD` to `GPIO 7`. The `L/R` channel pin is tied to GND, configuring the microphone for mono left-channel operation. A $100\text{ nF}$ ceramic decoupling capacitor is placed directly between `VDD` (3.3V) and `GND` adjacent to the microphone to eliminate RF noise.
  * **Battery Voltage Divider**: A precision resistor network consisting of two $100\text{ k}\Omega$ resistors dividing the battery voltage by two ($V_{\text{ADC}} = V_{\text{BAT}} / 2$), with a $100\text{ nF}$ smoothing capacitor filtering ADC noise on `GPIO 1` (`ADC1_CH1`).
  * **Dual Status LEDs**:
    * **Red Battery Indicator LED**: Connected to `GPIO 4` via a $330\Omega$ resistor to GND (active-HIGH).
    * **Blue Link Status LED**: Connected between the 3.3V rail and `GPIO 9` via a $330\Omega$ resistor (active-LOW). This specific wiring safely maintains `GPIO 9` at a logic HIGH level during boot, fulfilling the ESP32-C3 strapping pin requirement.
  * **Power Stage**: Shows a 3.7V 2000mAh Li-Po cell powering a TP4056 charger and 5V boost converter.

### 3. `wearable_breadboard_wiring.png` (High-Resolution $2200 \times 1700$ Bitmap)
* **Subsystem**: Caregiver Wearable Alert Wristband.
* **Core Components Shown**:
  * **ESP32-C3 SuperMini**: Ultra-compact development board wired to minimal external components for wristband integration.
  * **0.96-inch SSD1306 Monochrome I2C OLED Display**: Connected via `GPIO 4` (SDA) and `GPIO 5` (SCL) to display alphanumeric cot alert banners (e.g., `COT 02 CRY!`).
  * **Haptic Vibration Subsystem**:
    * **10mm Flat Coin ERM Vibration Motor**: Connected between 3.3V and the Drain of an **IRLML2502TR** logic-level N-channel MOSFET.
    * **1N5819 Schottky Flyback Diode**: Connected across the motor leads with its cathode (striped side) to 3.3V and anode to the Drain, absorbing inductive kickback and protecting the MOSFET.
    * **Gate Damping & Pulldown Resistors**: A $100\Omega$ series resistor connects `GPIO 3` to the MOSFET Gate to suppress ringing, and a $10\text{ k}\Omega$ pulldown resistor connects Gate to GND to prevent accidental vibration during microcontroller boot-up.
  * **Power Stage**: Shows a 3.7V 500mAh Li-Po pouch cell connected directly to the SuperMini's onboard low-dropout voltage regulator.

### 4. Vector Schematics (`.svg`)
* **`central_hub_exact_schematic.svg`**, **`cot_node_exact_schematic.svg`**, **`wearable_exact_schematic.svg`**:
  * Clean, infinitely scalable vector schematics representing the exact electrical connections.
  * Unlike static raster images, these vector files can be embedded in LaTeX documents, academic posters, and thesis defense presentations without resolution loss.
  * Every wire is color-coded by electrical function: Red for 5V/3.3V, Black for GND, Blue/Cyan for clock lines, Green/Orange for data lines, and Yellow for status signals.

### 5. `VIEW_ALL_SCHEMATICS.html` (Interactive Web Dashboard)
* An interactive, standalone HTML5/CSS3 application that loads all three breadboard images and vector schematics side-by-side in any standard web browser.
* Features a sleek dark-mode visual interface with zoomable cards, high-contrast pin callouts, and component specification badges, making it ideal for real-time reference on a lab monitor while soldering or breadboarding.

### 6. Algorithmic Image Generator & Conversion Scripts
* **`generate_accurate_wiring_images.py`**:
  * A custom Python script written using the Pillow (PIL) imaging library.
  * Instead of relying on generative AI diffusion models (which frequently hallucinate floating wires, incorrect pin counts, and impossible connections), this script algorithmically constructs a virtual breadboard grid, places microcontrollers and breakout boards at exact coordinate offsets, routes color-coded jumper traces with rounded corners, and renders crisp pin labels.
  * Generates pixel-perfect $2200 \times 1700$ PNG diagrams deterministically in seconds.
* **`convert_to_png.ps1`**:
  * A PowerShell utility script automating the conversion of SVG schematics into high-resolution PNGs using available system renderers.

---

## 🛠️ How to View and Re-Generate Assets

### To inspect all schematics in your browser:
Open the interactive viewer directly from the command line:
```powershell
Start-Process c:\Users\HP\Documents\Antigravity\Multi-node\hardware_images\VIEW_ALL_SCHEMATICS.html
```

### To re-generate all breadboard wiring PNGs:
If pin mappings or resistor values are modified in the future, re-render the images using the Pillow script:
```powershell
python c:\Users\HP\Documents\Antigravity\Multi-node\hardware_images\generate_accurate_wiring_images.py
```
