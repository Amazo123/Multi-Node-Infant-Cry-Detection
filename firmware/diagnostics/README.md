# Hardware Diagnostic & Bring-Up Guide

This directory contains standalone diagnostic firmware tools to verify individual hardware components **before** deploying the full multi-node network.

Testing each module independently eliminates guesswork and ensures that faulty wiring, dry solder joints, or dead ICs are caught immediately.

> 📖 **COMPLETE WIRING MANUAL:** For detailed schematics, breadboard hookup tables, and troubleshooting steps for every test, consult the project-wide [SYSTEM_WIRING_GUIDE.md](../../SYSTEM_WIRING_GUIDE.md).

---

## Tool 1: INMP441 I2S Microphone Checkout (`01_inmp441_audio_plotter`)

* **Target MCU**: ESP32-C3-DevKitC-02 (or ESP32-S3)
* **Folder**: [`01_inmp441_audio_plotter/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/01_inmp441_audio_plotter/01_inmp441_audio_plotter.ino)
* **Wiring Check**:
  * `SCK` (BCLK) $\rightarrow$ `GPIO 5`
  * `WS` (LRCK) $\rightarrow$ `GPIO 6`
  * `SD` (DIN) $\rightarrow$ `GPIO 7`
  * `L/R` $\rightarrow$ `GND` (sets left-channel mode)
  * `VDD` $\rightarrow$ `3.3V` (Do **NOT** connect to 5V!)
  * `GND` $\rightarrow$ `GND`

### How to Run:
1. Open the `.ino` in Arduino IDE or compile via PlatformIO.
2. Flash to the ESP32-C3.
3. Open **Tools $\rightarrow$ Serial Plotter** (Set baud to `115200`).
4. **Pass Criteria**:
   * In quiet silence: Waveform hovers around `0` with small noise amplitude ($< 200$).
   * Clapping or speaking: Instant high-amplitude sinusoidal spikes and RMS jumps above $1,000 - 5,000$.
   * **Fail Indicators**:
     * Flat line at exactly `0`: Missing clock (`SCK`/`WS`) or loose `SD` wire.
     * Clipped rail at `+32767` / `-32768`: Floating pin or missing GND reference.

---

## Tool 2: ESP-NOW RF Range & Packet Loss Probe (`02_esp_now_range_probe`)

* **Target MCUs**: 1× ESP32-S3 (Responder/Hub) + 1× ESP32-C3 (Sender/Node)
* **Folder**: [`02_esp_now_range_probe/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/02_esp_now_range_probe/)

### How to Run:
1. Flash [`esp_now_responder_probe.ino`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/02_esp_now_range_probe/esp_now_responder_probe.ino) to the central ESP32-S3 board placed in your central hub location.
2. Flash [`esp_now_sender_probe.ino`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/02_esp_now_range_probe/esp_now_sender_probe.ino) to the ESP32-C3 cot node board.
3. Open Serial Monitor on the sender board (115200 baud).
4. Walk across rooms, through walls, and between floors.
5. **Pass Criteria**:
   * Average Round-Trip Time (RTT) should be **$< 5\text{ ms}$**.
   * Packet Delivery Ratio (PDR) should remain **$\ge 98\%$** across your testing perimeter.

---

## Tool 3: Wearable Haptic Motor & OLED Checkout (`03_wearable_haptic_oled_test`)

* **Target MCU**: ESP32-C3-MINI-1
* **Folder**: [`03_wearable_haptic_oled_test/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/03_wearable_haptic_oled_test/03_wearable_haptic_oled_test.ino)
* **Wiring Check**:
  * `MOTOR_GATE` $\rightarrow$ `GPIO 3` (Gate of AO3400A N-MOSFET with $10\text{ k}\Omega$ pull-down to GND and 1N5819 flyback diode across motor)
  * `OLED SDA` $\rightarrow$ `GPIO 4`
  * `OLED SCL` $\rightarrow$ `GPIO 5`

### How to Run:
1. Flash to the wearable ESP32-C3.
2. Open Serial Monitor (115200 baud).
3. **Pass Criteria**:
   * Serial confirms `Found I2C device at 0x3C`.
   * OLED lights up and cycles between an inverted alert banner (`COT 01 CRY!`) and standby text.
   * Motor pulses crisp, distinct vibration bursts without rebooting or browning out the microcontroller.

---

## Tool 4: Cot Node LED & Battery ADC Checkout (`04_node_led_battery_test`)

* **Target MCU**: ESP32-C3-DevKitC-02
* **Folder**: [`04_node_led_battery_test/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/04_node_led_battery_test/04_node_led_battery_test.ino)
* **Wiring Check**:
  * `Battery Voltage Divider` $\rightarrow$ `GPIO 1` ($100\text{ k}\Omega + 100\text{ k}\Omega$ with $100\text{ nF}$ filter cap)
  * `Red LED` $\rightarrow$ `GPIO 4` (Active-HIGH: `GPIO 4` $\rightarrow 330\Omega \rightarrow$ Anode $\rightarrow$ Cathode $\rightarrow$ GND)
  * `Blue LED` $\rightarrow$ `GPIO 9` (Active-LOW: $3.3\text{V} \rightarrow 330\Omega \rightarrow$ Anode $\rightarrow$ Cathode $\rightarrow$ `GPIO 9`)

### How to Run:
1. Flash to the ESP32-C3 cot node.
2. Power cycle or press the Reset button.
3. **Pass Criteria**:
   * Board boots immediately and reliably (verifying that the pull-up wiring on `GPIO 9` strapping pin does not hold it LOW).
   * Red LED executes: Solid ON $\rightarrow$ Slow Blink $\rightarrow$ Rapid Blink.
   * Blue LED lights up when driven LOW and turns off when driven HIGH.
   * Serial Monitor displays estimated battery voltage matching a digital multimeter reading $\pm 0.05\text{V}$.

---

## Tool 5: Central Hub ILI9341 2.4" SPI TFT Checkout (`05_central_hub_ili9341_test`)

* **Target MCU**: ESP32-S3-DevKitC-1 N16R8
* **Folder**: [`05_central_hub_ili9341_test/`](file:///c:/Users/HP/Documents/Antigravity/Multi-node/firmware/diagnostics/05_central_hub_ili9341_test/05_central_hub_ili9341_test.ino)
* **Wiring Check (SPI2 Hardware Bus)**:
  * `CS` $\rightarrow$ `GPIO 10`
  * `RESET` $\rightarrow$ `GPIO 14`
  * `DC / RS` $\rightarrow$ `GPIO 13`
  * `MOSI / SDI` $\rightarrow$ `GPIO 11`
  * `SCK / CLK` $\rightarrow$ `GPIO 12`
  * `VCC` $\rightarrow$ `5V` (or `3.3V` depending on module LDO)
  * `GND` $\rightarrow$ `GND`
  * `LED / BLK` $\rightarrow$ `3.3V` via $100\Omega$ resistor

### How to Run:
1. Flash to the ESP32-S3 Central Hub board.
2. Open Serial Monitor (115200 baud).
3. **Pass Criteria**:
   * Screen powers on and renders primary color test bars (Red, Green, Blue, Yellow).
   * Automatically transitions to the graphical multi-cot telemetry cards and an inverted alert banner.
   * Board boots cleanly without any PSRAM cache collision (verifying safe avoidance of GPIO 33–37).

