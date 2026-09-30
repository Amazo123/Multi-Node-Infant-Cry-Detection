# 🖼️ Hardware Schematics, Wiring Diagrams & Visual Assets

This directory contains the visual schematics, breadboard layout drawings, and algorithmic image generation tools for the Multi-Node Infant Cry Detection System.

All diagrams are designed to reflect the **unapproximated, exact physical pinouts** of the production hardware, eliminating breadboard assembly guesswork.

---

## 📁 Directory File Breakdown

| File Name | Format | Role & Contents |
| :--- | :--- | :--- |
| **`central_hub_breadboard_wiring.png`** | PNG ($2200 \times 1700$) | High-resolution visual breadboard wiring for the Central Intelligence Hub. Illustrates connections between the ESP32-S3-DevKitC-1 N16R8, 2.4" ILI9341 SPI Color TFT, TP4056 USB-C charger, 5V step-up boost converter, 3000mAh Li-Ion battery, and decoupling capacitors (buzzer removed). |
| **`cot_node_breadboard_wiring.png`** | PNG ($2200 \times 1700$) | High-resolution visual breadboard wiring for the Cot Sensing Nodes (Cots 1, 2, and 3). Shows the ESP32-C3-DevKitC-02 wired to the INMP441 I2S digital microphone, $100\text{ k}\Omega + 100\text{ k}\Omega$ battery divider on GPIO 1, active-LOW Blue LED on GPIO 9, and active-HIGH Red LED on GPIO 4. |
| **`wearable_breadboard_wiring.png`** | PNG ($2200 \times 1700$) | High-resolution visual breadboard wiring for the Caregiver Wearable Wristband. Shows the ESP32-C3 SuperMini wired to the 0.96" SSD1306 I2C OLED display, IRLML2502TR SOT-23 N-MOSFET, 1N5819 Schottky snubber diode, and 10mm flat coin ERM vibration motor. |
| **`central_hub_exact_schematic.svg`** | Scalable Vector (SVG) | Infinitely-scalable vector schematic of the Central Hub with numbered GPIO headers and power rail annotations. |
| **`cot_node_exact_schematic.svg`** | Scalable Vector (SVG) | Vector schematic of the Cot Sensing Nodes showing I2S signal lines (BCLK, LRCK, DIN) and strapping-safe LED wiring. |
| **`wearable_exact_schematic.svg`** | Scalable Vector (SVG) | Vector schematic of the Caregiver Wristband with motor driver schematic diagram and I2C lines. |
| **`VIEW_ALL_SCHEMATICS.html`** | HTML5 / CSS | Dark-mode interactive web dashboard to review all three subsystem schematics side-by-side in any web browser. |
| **`generate_accurate_wiring_images.py`** | Python 3 (Pillow) | The underlying algorithmic rendering engine that draws vector breadboards, color-coded jumper wires, IC pinouts, capacitors, and labels to produce the $2200 \times 1700$ PNGs. |
| **`convert_to_png.ps1`** | PowerShell | Utility script to convert SVG schematics to raster images using available system tools. |

---

## ⚡ Quick Inspection

To view all schematics immediately in your default browser:
```powershell
Start-Process VIEW_ALL_SCHEMATICS.html
```

To re-render all breadboard PNG images from scratch:
```powershell
python generate_accurate_wiring_images.py
```
