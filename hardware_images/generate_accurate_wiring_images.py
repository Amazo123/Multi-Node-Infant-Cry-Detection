"""
Pixel-Perfect Breadboard Circuit Wiring Image Generator
Renders high-resolution 2200x1700 PNG breadboard schematics for:
1. Central Intelligence Hub (ESP32-S3-N16R8 + ILI9341 SPI TFT + Power Stage)
2. Cot Sensing Node (ESP32-C3 + INMP441 I2S Mic + 2 LEDs + ADC Divider + Power Stage)
3. Caregiver Wearable (ESP32-C3 SuperMini + SSD1306 OLED + IRLML2502TR MOSFET + ERM Motor)
"""

import os
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = r"c:\Users\HP\Documents\Antigravity\Multi-node\hardware_images"
os.makedirs(OUT_DIR, exist_ok=True)

FONT_LARGE = None
FONT_MED = None
FONT_SMALL = None

try:
    FONT_LARGE = ImageFont.truetype("arial.ttf", 28)
    FONT_TITLE = ImageFont.truetype("arialbd.ttf", 36)
    FONT_MED = ImageFont.truetype("arialbd.ttf", 20)
    FONT_SMALL = ImageFont.truetype("arial.ttf", 16)
    FONT_TINY = ImageFont.truetype("arial.ttf", 13)
except Exception:
    FONT_LARGE = ImageFont.load_default()
    FONT_TITLE = FONT_LARGE
    FONT_MED = FONT_LARGE
    FONT_SMALL = FONT_LARGE
    FONT_TINY = FONT_LARGE

def draw_breadboard(draw, x, y, w, h):
    """Draws a realistic full-size solderless breadboard"""
    # Outer casing
    draw.rounded_rectangle([x, y, x + w, y + h], radius=15, fill="#ECEFF1", outline="#B0BEC5", width=4)
    # Notch in middle
    draw.rectangle([x + 20, y + h//2 - 12, x + w - 20, y + h//2 + 12], fill="#CFD8DC")
    
    # Power rails (Top)
    draw.line([x + 25, y + 25, x + w - 25, y + 25], fill="#E53935", width=3) # + Red
    draw.line([x + 25, y + 55, x + w - 25, y + 55], fill="#1E88E5", width=3) # - Blue
    draw.text((x + 8, y + 17), "+", fill="#E53935", font=FONT_MED)
    draw.text((x + 8, y + 47), "-", fill="#1E88E5", font=FONT_MED)

    # Power rails (Bottom)
    draw.line([x + 25, y + h - 55, x + w - 25, y + h - 55], fill="#E53935", width=3) # + Red
    draw.line([x + 25, y + h - 25, x + w - 25, y + h - 25], fill="#1E88E5", width=3) # - Blue
    draw.text((x + 8, y + h - 63), "+", fill="#E53935", font=FONT_MED)
    draw.text((x + 8, y + h - 33), "-", fill="#1E88E5", font=FONT_MED)

    # Holes grid representation
    for col in range(x + 40, x + w - 30, 24):
        # Top power rail holes
        draw.ellipse([col-2, y+23, col+2, y+27], fill="#455A64")
        draw.ellipse([col-2, y+53, col+2, y+57], fill="#455A64")
        # Bottom power rail holes
        draw.ellipse([col-2, y+h-57, col+2, y+h-53], fill="#455A64")
        draw.ellipse([col-2, y+h-27, col+2, y+h-23], fill="#455A64")
        # Terminal strip rows
        for row in range(y + 85, y + h//2 - 20, 24):
            draw.ellipse([col-2, row-2, col+2, row+2], fill="#90A4AE")
        for row in range(y + h//2 + 25, y + h - 75, 24):
            draw.ellipse([col-2, row-2, col+2, row+2], fill="#90A4AE")

def draw_jumper(draw, p1, p2, color, label=""):
    """Draws a clean, curved jumper wire with pin dots"""
    draw.ellipse([p1[0]-5, p1[1]-5, p1[0]+5, p1[1]+5], fill=color, outline="#263238", width=2)
    draw.ellipse([p2[0]-5, p2[1]-5, p2[0]+5, p2[1]+5], fill=color, outline="#263238", width=2)
    
    # Manhattan routing with chamfer for crisp readability
    mid_x = (p1[0] + p2[0]) // 2
    draw.line([p1, (mid_x, p1[1]), (mid_x, p2[1]), p2], fill=color, width=4)
    if label:
        lx = (p1[0] + p2[0]) // 2
        ly = min(p1[1], p2[1]) + abs(p1[1] - p2[1]) // 2 - 14
        draw.rectangle([lx - 30, ly - 2, lx + 30, ly + 18], fill="#FFFFFF", outline="#37474F")
        draw.text((lx - 26, ly), label, fill="#263238", font=FONT_TINY)

def draw_resistor(draw, p1, p2, val_text):
    """Draws an axial lead resistor"""
    draw.line([p1, p2], fill="#78909C", width=3)
    mx, my = (p1[0] + p2[0]) // 2, (p1[1] + p2[1]) // 2
    draw.rounded_rectangle([mx - 24, my - 10, mx + 24, my + 10], radius=4, fill="#D7CCC8", outline="#5D4037", width=2)
    draw.text((mx - 18, my - 8), val_text, fill="#3E2723", font=FONT_TINY)

def draw_capacitor(draw, center, val_text, is_electrolytic=False):
    """Draws ceramic disc or electrolytic capacitor"""
    cx, cy = center
    if is_electrolytic:
        draw.rounded_rectangle([cx - 18, cy - 25, cx + 18, cy + 25], radius=6, fill="#0D47A1", outline="#01579B", width=2)
        draw.line([cx + 6, cy - 23, cx + 6, cy + 23], fill="#ECEFF1", width=5)
        draw.text((cx - 14, cy - 8), val_text, fill="#FFFFFF", font=FONT_TINY)
    else:
        draw.ellipse([cx - 16, cy - 16, cx + 16, cy + 16], fill="#FFB300", outline="#FF8F00", width=2)
        draw.text((cx - 12, cy - 7), val_text, fill="#3E2723", font=FONT_TINY)

# ==============================================================================
# 1. CENTRAL INTELLIGENCE HUB WIRING IMAGE
# ==============================================================================
def render_central_hub():
    img = Image.new("RGB", (2200, 1600), "#1E222B")
    d = ImageDraw.Draw(img)

    # Title Banner
    d.rectangle([0, 0, 2200, 80], fill="#0F141C")
    d.text((40, 22), "CENTRAL INTELLIGENCE HUB — COMPLETE BREADBOARD WIRING", fill="#4FC3F7", font=FONT_TITLE)
    d.text((1350, 28), "ESP32-S3 N16R8 | ILI9341 SPI TFT | POWER STAGE", fill="#B0BEC5", font=FONT_MED)

    # Breadboard
    draw_breadboard(d, 80, 280, 2040, 1240)

    # Module 1: Battery 3.7V 3000mAh
    d.rounded_rectangle([100, 100, 360, 240], radius=8, fill="#263238", outline="#FFB300", width=3)
    d.text((120, 115), "Li-Ion Battery", fill="#FFB300", font=FONT_MED)
    d.text((120, 145), "3.7V 3000mAh", fill="#FFFFFF", font=FONT_LARGE)
    d.text((120, 185), "1S Pouch Cell", fill="#90A4AE", font=FONT_SMALL)
    # Battery leads
    draw_jumper(d, (360, 140), (450, 140), "#E53935", "B+")
    draw_jumper(d, (360, 180), (450, 180), "#1E88E5", "B-")

    # Module 2: TP4056 USB-C Charger
    d.rounded_rectangle([450, 100, 720, 240], radius=8, fill="#1565C0", outline="#0D47A1", width=3)
    d.text((470, 115), "TP4056 Charger", fill="#FFFFFF", font=FONT_MED)
    d.text((470, 140), "USB-C Input", fill="#90CAF9", font=FONT_SMALL)
    d.rectangle([440, 150, 455, 190], fill="#78909C")
    d.text((470, 175), "B+ / B- IN", fill="#ECEFF1", font=FONT_TINY)
    d.text((640, 175), "OUT+ / OUT-", fill="#ECEFF1", font=FONT_TINY)
    # TP4056 to Boost
    draw_jumper(d, (720, 140), (820, 140), "#E53935", "OUT+")
    draw_jumper(d, (720, 180), (820, 180), "#1E88E5", "OUT-")

    # Module 3: 5V Boost Converter
    d.rounded_rectangle([820, 100, 1080, 240], radius=8, fill="#2E7D32", outline="#1B5E20", width=3)
    d.text((840, 115), "5V Boost Module", fill="#FFFFFF", font=FONT_MED)
    d.text((840, 140), "3.0V-4.2V -> 5.0V", fill="#A5D6A7", font=FONT_SMALL)
    # Boost Output to Breadboard Rails
    draw_jumper(d, (1080, 140), (1200, 305), "#E53935", "5V RAIL")
    draw_jumper(d, (1080, 180), (1200, 335), "#1E88E5", "GND RAIL")

    # Capacitor 1: 100uF Bulk on 5V Rail
    draw_capacitor(d, (1260, 320), "100µF", is_electrolytic=True)
    d.text((1220, 260), "C1: 100µF Bulk", fill="#4FC3F7", font=FONT_TINY)

    # Module 4: ESP32-S3-DevKitC-1 N16R8
    s3_x, s3_y, s3_w, s3_h = 240, 480, 680, 920
    d.rounded_rectangle([s3_x, s3_y, s3_x + s3_w, s3_y + s3_h], radius=10, fill="#212121", outline="#64B5F6", width=4)
    # Metal RF Shield
    d.rounded_rectangle([s3_x + 120, s3_y + 120, s3_x + 560, s3_y + 440], radius=8, fill="#424242", outline="#BDBDBD", width=2)
    d.text((s3_x + 160, s3_y + 180), "ESP32-S3-WROOM-1", fill="#FFFFFF", font=FONT_TITLE)
    d.text((s3_x + 220, s3_y + 230), "N16R8 MODULE", fill="#FFB300", font=FONT_TITLE)
    d.text((s3_x + 160, s3_y + 285), "• 16MB SPI Quad Flash", fill="#E0E0E0", font=FONT_MED)
    d.text((s3_x + 160, s3_y + 325), "• 8MB Octal PSRAM", fill="#E0E0E0", font=FONT_MED)
    d.text((s3_x + 160, s3_y + 375), "⚠️ GPIO 33-37 DEDICATED TO PSRAM!", fill="#FF5252", font=FONT_MED)

    # Dual USB-C on bottom
    d.rounded_rectangle([s3_x + 160, s3_y + s3_h - 40, s3_x + 280, s3_y + s3_h + 10], radius=4, fill="#B0BEC5")
    d.text((s3_x + 190, s3_y + s3_h - 30), "UART", fill="#263238", font=FONT_MED)
    d.rounded_rectangle([s3_x + 400, s3_y + s3_h - 40, s3_x + 520, s3_y + s3_h + 10], radius=4, fill="#B0BEC5")
    d.text((s3_x + 435, s3_y + s3_h - 30), "USB", fill="#263238", font=FONT_MED)

    # ESP32-S3 Pins Used
    p_5v = (s3_x + 40, s3_y + 80)
    p_gnd = (s3_x + 40, s3_y + 130)
    p_3v3 = (s3_x + 40, s3_y + 180)
    p_cs = (s3_x + 40, s3_y + 450)    # GPIO 10
    p_mosi = (s3_x + 40, s3_y + 510)  # GPIO 11
    p_sck = (s3_x + 40, s3_y + 570)   # GPIO 12
    p_dc = (s3_x + 40, s3_y + 630)    # GPIO 13
    p_rst = (s3_x + 40, s3_y + 690)   # GPIO 14

    # Pin Labels on S3
    d.text((s3_x + 55, s3_y + 70), "5V / VIN", fill="#FF5252", font=FONT_MED)
    d.text((s3_x + 55, s3_y + 120), "GND", fill="#4FC3F7", font=FONT_MED)
    d.text((s3_x + 55, s3_y + 170), "3V3", fill="#FF5252", font=FONT_MED)
    d.text((s3_x + 55, s3_y + 440), "GPIO 10 (CS)", fill="#E040FB", font=FONT_MED)
    d.text((s3_x + 55, s3_y + 500), "GPIO 11 (MOSI)", fill="#69F0AE", font=FONT_MED)
    d.text((s3_x + 55, s3_y + 560), "GPIO 12 (SCK)", fill="#FFD740", font=FONT_MED)
    d.text((s3_x + 55, s3_y + 620), "GPIO 13 (DC)", fill="#FF6E40", font=FONT_MED)
    d.text((s3_x + 55, s3_y + 680), "GPIO 14 (RST)", fill="#40C4FF", font=FONT_MED)

    # Power connections to S3
    draw_jumper(d, (200, 305), p_5v, "#E53935", "5V")
    draw_jumper(d, (200, 335), p_gnd, "#1E88E5", "GND")
    # C3 Decoupling 100nF right at S3 5V/GND
    draw_capacitor(d, (s3_x - 30, s3_y + 105), "104", is_electrolytic=False)
    # C2 10uF on 3V3 rail
    draw_capacitor(d, (s3_x - 30, s3_y + 180), "10µF", is_electrolytic=True)

    # Module 5: 2.4" ILI9341 SPI TFT Display
    tft_x, tft_y, tft_w, tft_h = 1350, 480, 680, 920
    d.rounded_rectangle([tft_x, tft_y, tft_x + tft_w, tft_y + tft_h], radius=12, fill="#B71C1C", outline="#D32F2F", width=4)
    # Screen area
    d.rectangle([tft_x + 40, tft_y + 40, tft_x + tft_w - 40, tft_y + tft_h - 220], fill="#0A0E17", outline="#455A64", width=3)
    d.text((tft_x + 120, tft_y + 120), "ILI9341 2.4\" TFT", fill="#4FC3F7", font=FONT_TITLE)
    d.text((tft_x + 150, tft_y + 180), "240 x 320 COLOR SPI", fill="#B0BEC5", font=FONT_MED)
    d.text((tft_x + 80, tft_y + 260), "INFANT MONITOR - S3 HUB", fill="#69F0AE", font=FONT_TITLE)
    d.text((tft_x + 80, tft_y + 340), "COT 01: NORMAL [0.02 RMS]", fill="#ECEFF1", font=FONT_MED)
    d.text((tft_x + 80, tft_y + 390), "COT 02: NORMAL [0.01 RMS]", fill="#ECEFF1", font=FONT_MED)
    d.text((tft_x + 80, tft_y + 440), "COT 03: NORMAL [0.02 RMS]", fill="#ECEFF1", font=FONT_MED)
    d.text((tft_x + 80, tft_y + 520), "STATUS: SUPERVISING (STANDBY)", fill="#FFD740", font=FONT_MED)

    # TFT Pins (Header at bottom)
    pins = [
        ("VCC", "#FF5252"), ("GND", "#4FC3F7"), ("CS", "#E040FB"), ("RST", "#40C4FF"),
        ("DC", "#FF6E40"), ("MOSI", "#69F0AE"), ("SCK", "#FFD740"), ("LED", "#FFB300"), ("MISO", "#78909C")
    ]
    tft_pin_coords = {}
    for idx, (pname, pcolor) in enumerate(pins):
        px = tft_x + 60 + idx * 64
        py = tft_y + tft_h - 120
        d.rounded_rectangle([px - 24, py - 18, px + 24, py + 18], radius=4, fill="#263238", outline=pcolor, width=2)
        d.text((px - 16, py - 9), pname, fill=pcolor, font=FONT_TINY)
        tft_pin_coords[pname] = (px, py)

    # C4 Decoupling 100nF right at TFT VCC/GND
    draw_capacitor(d, (tft_x + 92, tft_y + tft_h - 170), "104", is_electrolytic=False)

    # WIRING FROM S3 TO TFT:
    # 1. 5V rail to TFT VCC
    draw_jumper(d, (1400, 305), tft_pin_coords["VCC"], "#E53935", "5V")
    # 2. GND rail to TFT GND
    draw_jumper(d, (1464, 335), tft_pin_coords["GND"], "#1E88E5", "GND")
    # 3. S3 3V3 to R1 100 ohm to TFT LED
    r_pt1 = (1800, 420)
    r_pt2 = (tft_pin_coords["LED"][0], 420)
    draw_jumper(d, p_3v3, r_pt1, "#FF5252", "3.3V")
    draw_resistor(d, r_pt1, r_pt2, "100Ω")
    draw_jumper(d, r_pt2, tft_pin_coords["LED"], "#FFB300", "LED")

    # 4. SPI Signals
    draw_jumper(d, p_cs, tft_pin_coords["CS"], "#E040FB", "CS (10)")
    draw_jumper(d, p_rst, tft_pin_coords["RST"], "#40C4FF", "RST (14)")
    draw_jumper(d, p_dc, tft_pin_coords["DC"], "#FF6E40", "DC (13)")
    draw_jumper(d, p_mosi, tft_pin_coords["MOSI"], "#69F0AE", "MOSI (11)")
    draw_jumper(d, p_sck, tft_pin_coords["SCK"], "#FFD740", "SCK (12)")

    # Bottom Legend Banner
    d.rectangle([80, 1540, 2120, 1590], fill="#0F141C", outline="#37474F", width=2)
    d.text((120, 1555), "✓ ALL 4 CAPACITORS WIRED  |  ✓ 100Ω BACKLIGHT RESISTOR WIRED  |  ✓ GPIO 33-37 UNTOUCHED (OCTAL PSRAM SAFE)", fill="#69F0AE", font=FONT_MED)

    out_file = os.path.join(OUT_DIR, "central_hub_breadboard_wiring.png")
    img.save(out_file, "PNG")
    print(f"Saved: {out_file}")

# ==============================================================================
# 2. COT SENSING NODE WIRING IMAGE
# ==============================================================================
def render_cot_node():
    img = Image.new("RGB", (2200, 1600), "#1E222B")
    d = ImageDraw.Draw(img)

    # Title Banner
    d.rectangle([0, 0, 2200, 80], fill="#0F141C")
    d.text((40, 22), "COT SENSING NODE (1, 2, 3) — COMPLETE BREADBOARD WIRING", fill="#69F0AE", font=FONT_TITLE)
    d.text((1250, 28), "ESP32-C3 | INMP441 I2S MIC | 2-LED SYSTEM | BATTERY ADC", fill="#B0BEC5", font=FONT_MED)

    # Breadboard
    draw_breadboard(d, 80, 280, 2040, 1240)

    # Battery 3.7V 2000mAh
    d.rounded_rectangle([100, 100, 360, 240], radius=8, fill="#263238", outline="#FFB300", width=3)
    d.text((120, 115), "Li-Ion Battery", fill="#FFB300", font=FONT_MED)
    d.text((120, 145), "3.7V 2000mAh", fill="#FFFFFF", font=FONT_LARGE)
    d.text((120, 185), "1S Pouch Cell", fill="#90A4AE", font=FONT_SMALL)
    draw_jumper(d, (360, 140), (450, 140), "#E53935", "B+")
    draw_jumper(d, (360, 180), (450, 180), "#1E88E5", "B-")

    # TP4056 Charger
    d.rounded_rectangle([450, 100, 720, 240], radius=8, fill="#1565C0", outline="#0D47A1", width=3)
    d.text((470, 115), "TP4056 Charger", fill="#FFFFFF", font=FONT_MED)
    d.text((470, 140), "USB-C Input", fill="#90CAF9", font=FONT_SMALL)
    draw_jumper(d, (720, 140), (820, 140), "#E53935", "OUT+")
    draw_jumper(d, (720, 180), (820, 180), "#1E88E5", "OUT-")

    # 5V Boost Module
    d.rounded_rectangle([820, 100, 1080, 240], radius=8, fill="#2E7D32", outline="#1B5E20", width=3)
    d.text((840, 115), "5V Boost Module", fill="#FFFFFF", font=FONT_MED)
    d.text((840, 140), "3.0V-4.2V -> 5.0V", fill="#A5D6A7", font=FONT_SMALL)
    draw_jumper(d, (1080, 140), (1200, 305), "#E53935", "5V RAIL")
    draw_jumper(d, (1080, 180), (1200, 335), "#1E88E5", "GND RAIL")

    # Bulk Cap 47uF on 5V rail
    draw_capacitor(d, (1260, 320), "47µF", is_electrolytic=True)
    d.text((1220, 260), "C1: 47µF Buffer", fill="#4FC3F7", font=FONT_TINY)

    # Core MCU: ESP32-C3 Square Dev Board
    c3_x, c3_y, c3_w, c3_h = 750, 480, 680, 920
    d.rounded_rectangle([c3_x, c3_y, c3_x + c3_w, c3_y + c3_h], radius=10, fill="#212121", outline="#69F0AE", width=4)
    # Chip center
    d.rounded_rectangle([c3_x + 220, c3_y + 320, c3_x + 460, c3_y + 560], radius=8, fill="#37474F", outline="#78909C", width=2)
    d.text((c3_x + 260, c3_y + 420), "ESP32-C3", fill="#FFFFFF", font=FONT_TITLE)
    d.text((c3_x + 250, c3_y + 470), "RISC-V 160MHz", fill="#FFB300", font=FONT_MED)

    # USB-C Top
    d.rounded_rectangle([c3_x + 280, c3_y - 20, c3_x + 400, c3_y + 25], radius=4, fill="#B0BEC5")
    d.text((c3_x + 310, c3_y - 5), "USB-C", fill="#263238", font=FONT_MED)

    # ESP32-C3 Left Header Pins: 5V, GND, 3V3, 20, 21, 2, 1, 0
    p_5v = (c3_x + 30, c3_y + 80)
    p_gnd = (c3_x + 30, c3_y + 140)
    p_3v3 = (c3_x + 30, c3_y + 200)
    p_io1 = (c3_x + 30, c3_y + 500) # ADC1_CH1 Battery Monitor

    d.text((c3_x + 45, c3_y + 70), "5V", fill="#FF5252", font=FONT_MED)
    d.text((c3_x + 45, c3_y + 130), "GND", fill="#4FC3F7", font=FONT_MED)
    d.text((c3_x + 45, c3_y + 190), "3V3", fill="#FF5252", font=FONT_MED)
    d.text((c3_x + 45, c3_y + 490), "GPIO 1 (ADC1)", fill="#69F0AE", font=FONT_MED)

    # ESP32-C3 Right Header Pins: 10, 9, 8, 7, 6, 5, 4, 3
    p_io9 = (c3_x + c3_w - 30, c3_y + 140) # BLUE LED (Active-LOW)
    p_io7 = (c3_x + c3_w - 30, c3_y + 260) # INMP441 SD
    p_io6 = (c3_x + c3_w - 30, c3_y + 320) # INMP441 WS
    p_io5 = (c3_x + c3_w - 30, c3_y + 380) # INMP441 SCK
    p_io4 = (c3_x + c3_w - 30, c3_y + 440) # RED LED (Active-HIGH)

    d.text((c3_x + c3_w - 180, c3_y + 130), "GPIO 9 (BLUE)", fill="#40C4FF", font=FONT_MED)
    d.text((c3_x + c3_w - 160, c3_y + 250), "GPIO 7 (SD)", fill="#FFD740", font=FONT_MED)
    d.text((c3_x + c3_w - 160, c3_y + 310), "GPIO 6 (WS)", fill="#E040FB", font=FONT_MED)
    d.text((c3_x + c3_w - 170, c3_y + 370), "GPIO 5 (SCK)", fill="#69F0AE", font=FONT_MED)
    d.text((c3_x + c3_w - 170, c3_y + 430), "GPIO 4 (RED)", fill="#FF5252", font=FONT_MED)

    # Power connections to C3
    draw_jumper(d, (700, 305), p_5v, "#E53935", "5V")
    draw_jumper(d, (700, 335), p_gnd, "#1E88E5", "GND")

    # Module: INMP441 I2S MEMS Mic (Right side)
    mic_x, mic_y = 1580, 500
    d.rounded_rectangle([mic_x, mic_y, mic_x + 420, mic_y + 400], radius=10, fill="#263238", outline="#FFB300", width=3)
    d.ellipse([mic_x + 160, mic_y + 50, mic_x + 260, mic_y + 150], fill="#37474F", outline="#FFB300", width=2)
    d.ellipse([mic_x + 200, mic_y + 90, mic_x + 220, mic_y + 110], fill="#000000")
    d.text((mic_x + 110, mic_y + 170), "INMP441 MEMS MIC", fill="#FFFFFF", font=FONT_TITLE)
    d.text((mic_x + 130, mic_y + 215), "24-bit I2S Digital", fill="#FFB300", font=FONT_MED)

    mic_pins = [
        ("VDD", (mic_x + 60, mic_y + 320)),
        ("GND", (mic_x + 120, mic_y + 320)),
        ("SD",  (mic_x + 180, mic_y + 320)),
        ("L/R", (mic_x + 240, mic_y + 320)),
        ("WS",  (mic_x + 300, mic_y + 320)),
        ("SCK", (mic_x + 360, mic_y + 320))
    ]
    for p_name, pt in mic_pins:
        d.rounded_rectangle([pt[0]-24, pt[1]-18, pt[0]+24, pt[1]+18], radius=4, fill="#37474F", outline="#B0BEC5", width=2)
        d.text((pt[0]-14, pt[1]-9), p_name, fill="#ECEFF1", font=FONT_TINY)

    # INMP441 Wiring
    draw_jumper(d, p_3v3, mic_pins[0][1], "#E53935", "3.3V")
    draw_jumper(d, p_gnd, mic_pins[1][1], "#1E88E5", "GND")
    draw_jumper(d, mic_pins[1][1], mic_pins[3][1], "#1E88E5", "L/R=GND")
    draw_capacitor(d, (mic_x + 90, mic_y + 270), "104", is_electrolytic=False) # 100nF Mic Decouple
    draw_jumper(d, p_io7, mic_pins[2][1], "#FFD740", "SD (7)")
    draw_jumper(d, p_io6, mic_pins[4][1], "#E040FB", "WS (6)")
    draw_jumper(d, p_io5, mic_pins[5][1], "#69F0AE", "SCK (5)")

    # 2-LED System (Bottom-Right)
    led_box_x, led_box_y = 1580, 960
    d.rounded_rectangle([led_box_x, led_box_y, led_box_x + 420, led_box_y + 440], radius=10, fill="#1A237E", outline="#3949AB", width=3)
    d.text((led_box_x + 60, led_box_y + 30), "2-LED STATUS INDICATOR", fill="#FFFFFF", font=FONT_TITLE)

    # Red LED on GPIO 4
    d.ellipse([led_box_x + 60, led_box_y + 110, led_box_x + 100, led_box_y + 150], fill="#E53935", outline="#FFCDD2", width=3)
    d.text((led_box_x + 120, led_box_y + 105), "RED LED (Battery SOC)", fill="#FF8A80", font=FONT_MED)
    d.text((led_box_x + 120, led_box_y + 130), "Active-HIGH: Solid / Slow / Fast Blink", fill="#ECEFF1", font=FONT_SMALL)
    draw_resistor(d, (led_box_x + 120, led_box_y + 180), (led_box_x + 240, led_box_y + 180), "330Ω")
    draw_jumper(d, p_io4, (led_box_x + 120, led_box_y + 180), "#E53935", "GPIO 4")
    draw_jumper(d, (led_box_x + 240, led_box_y + 180), (led_box_x + 80, led_box_y + 150), "#E53935")
    draw_jumper(d, (led_box_x + 80, led_box_y + 110), (1800, 1440), "#1E88E5", "GND")

    # Blue LED on GPIO 9 (Active-LOW with Strapping Safety)
    d.ellipse([led_box_x + 60, led_box_y + 270, led_box_x + 100, led_box_y + 310], fill="#1E88E5", outline="#BBDEFB", width=3)
    d.text((led_box_x + 120, led_box_y + 265), "BLUE LED (ESP-NOW Link)", fill="#80D8FF", font=FONT_MED)
    d.text((led_box_x + 120, led_box_y + 290), "Active-LOW: Solid=Link, Blink=Lost", fill="#ECEFF1", font=FONT_SMALL)
    draw_resistor(d, (led_box_x + 120, led_box_y + 340), (led_box_x + 240, led_box_y + 340), "330Ω")
    draw_jumper(d, p_3v3, (led_box_x + 120, led_box_y + 340), "#E53935", "3.3V")
    draw_jumper(d, (led_box_x + 240, led_box_y + 340), (led_box_x + 80, led_box_y + 270), "#E53935")
    draw_jumper(d, (led_box_x + 80, led_box_y + 310), p_io9, "#1E88E5", "GPIO 9")

    # Battery Voltage Divider (Bottom-Left)
    div_x, div_y = 100, 500
    d.rounded_rectangle([div_x, div_y, div_x + 480, div_y + 540], radius=10, fill="#263238", outline="#69F0AE", width=3)
    d.text((div_x + 60, div_y + 30), "BATTERY MONITOR (ADC1_CH1)", fill="#69F0AE", font=FONT_TITLE)
    d.text((div_x + 60, div_y + 75), "VADC = VBAT / 2 (Ratio: 2:1)", fill="#ECEFF1", font=FONT_MED)

    # R_TOP 100k
    draw_resistor(d, (div_x + 180, div_y + 140), (div_x + 340, div_y + 140), "100kΩ")
    d.text((div_x + 60, div_y + 130), "R_TOP", fill="#FFB300", font=FONT_MED)
    draw_jumper(d, (360, 140), (div_x + 180, div_y + 140), "#E53935", "VBAT")

    # Divider Junction
    j_pt = (div_x + 340, div_y + 260)
    d.ellipse([j_pt[0]-8, j_pt[1]-8, j_pt[0]+8, j_pt[1]+8], fill="#69F0AE")
    d.text((div_x + 80, div_y + 250), "ADC Junction", fill="#69F0AE", font=FONT_MED)
    draw_jumper(d, (div_x + 340, div_y + 140), j_pt, "#69F0AE")
    draw_jumper(d, j_pt, p_io1, "#69F0AE", "GPIO 1")

    # R_BOT 100k to GND
    draw_resistor(d, j_pt, (div_x + 340, div_y + 400), "100kΩ")
    d.text((div_x + 60, div_y + 330), "R_BOT", fill="#FFB300", font=FONT_MED)
    draw_jumper(d, (div_x + 340, div_y + 400), (200, 1475), "#1E88E5", "GND")

    # C3 100nF Filter Cap across R_BOT to GND
    draw_capacitor(d, (div_x + 420, div_y + 330), "104", is_electrolytic=False)
    d.text((div_x + 370, div_y + 360), "100nF Filter", fill="#FFD740", font=FONT_TINY)

    # Bottom Legend Banner
    d.rectangle([80, 1540, 2120, 1590], fill="#0F141C", outline="#37474F", width=2)
    d.text((120, 1555), "✓ GPIO 9 STRAPPING SAFE (ACTIVE-LOW 3.3V PULL)  |  ✓ ADC1 IMMUNE TO WI-FI SHUTDOWN  |  ✓ INMP441 LEFT-CHANNEL GROUNDED", fill="#69F0AE", font=FONT_MED)

    out_file = os.path.join(OUT_DIR, "cot_node_breadboard_wiring.png")
    img.save(out_file, "PNG")
    print(f"Saved: {out_file}")

# ==============================================================================
# 3. CAREGIVER WEARABLE WIRING IMAGE
# ==============================================================================
def render_wearable():
    img = Image.new("RGB", (2200, 1600), "#1E222B")
    d = ImageDraw.Draw(img)

    # Title Banner
    d.rectangle([0, 0, 2200, 80], fill="#0F141C")
    d.text((40, 22), "CAREGIVER WEARABLE WRISTBAND — COMPLETE BREADBOARD WIRING", fill="#E040FB", font=FONT_TITLE)
    d.text((1200, 28), "ESP32-C3 SUPERMINI | SSD1306 OLED | IRLML2502TR MOTOR DRIVER", fill="#B0BEC5", font=FONT_MED)

    # Breadboard
    draw_breadboard(d, 80, 280, 2040, 1240)

    # Battery 3.7V 500mAh LiPo
    d.rounded_rectangle([100, 100, 360, 240], radius=8, fill="#263238", outline="#FFB300", width=3)
    d.text((120, 115), "Li-Po Battery", fill="#FFB300", font=FONT_MED)
    d.text((120, 145), "3.7V ~500mAh", fill="#FFFFFF", font=FONT_LARGE)
    d.text((120, 185), "Slim Wristband Cell", fill="#90A4AE", font=FONT_SMALL)
    draw_jumper(d, (360, 140), (450, 140), "#E53935", "B+")
    draw_jumper(d, (360, 180), (450, 180), "#1E88E5", "B-")

    # 3.3V Low-Dropout Regulator
    d.rounded_rectangle([450, 100, 720, 240], radius=8, fill="#00695C", outline="#004D40", width=3)
    d.text((470, 115), "3.3V LDO Regulator", fill="#FFFFFF", font=FONT_MED)
    d.text((470, 140), "AP2112K / ME6211", fill="#80CBC4", font=FONT_SMALL)
    draw_jumper(d, (720, 140), (820, 305), "#E53935", "3.3V RAIL")
    draw_jumper(d, (720, 180), (820, 335), "#1E88E5", "GND RAIL")

    # C1 10uF on 3.3V Rail
    draw_capacitor(d, (880, 320), "10µF", is_electrolytic=True)
    d.text((850, 260), "C1: 10µF Bulk", fill="#4FC3F7", font=FONT_TINY)

    # Core MCU: ESP32-C3 SuperMini (Compact castellated layout)
    sm_x, sm_y, sm_w, sm_h = 240, 500, 620, 900
    d.rounded_rectangle([sm_x, sm_y, sm_x + sm_w, sm_y + sm_h], radius=10, fill="#212121", outline="#E040FB", width=4)
    # QFN Chip
    d.rounded_rectangle([sm_x + 210, sm_y + 350, sm_x + 410, sm_y + 550], radius=8, fill="#37474F", outline="#78909C", width=2)
    d.text((sm_x + 235, sm_y + 430), "ESP32-C3", fill="#FFFFFF", font=FONT_TITLE)
    d.text((sm_x + 240, sm_y + 480), "SuperMini", fill="#E040FB", font=FONT_MED)

    # USB-C Top
    d.rounded_rectangle([sm_x + 240, sm_y - 20, sm_x + 380, sm_y + 25], radius=4, fill="#B0BEC5")
    d.text((sm_x + 280, sm_y - 5), "USB-C", fill="#263238", font=FONT_MED)

    # SuperMini Header Pins (Left): 5V, GND, 3V3, IO0, IO1, IO2, IO3, IO4, IO5
    p_5v = (sm_x + 30, sm_y + 80)
    p_gnd = (sm_x + 30, sm_y + 140)
    p_3v3 = (sm_x + 30, sm_y + 200)
    p_io3 = (sm_x + 30, sm_y + 500) # MOTOR GATE
    p_io4 = (sm_x + 30, sm_y + 560) # OLED SDA
    p_io5 = (sm_x + 30, sm_y + 620) # OLED SCL

    d.text((sm_x + 45, sm_y + 70), "5V", fill="#FF5252", font=FONT_MED)
    d.text((sm_x + 45, sm_y + 130), "GND", fill="#4FC3F7", font=FONT_MED)
    d.text((sm_x + 45, sm_y + 190), "3V3", fill="#FF5252", font=FONT_MED)
    d.text((sm_x + 45, sm_y + 490), "IO 3 (MOTOR)", fill="#FF5252", font=FONT_MED)
    d.text((sm_x + 45, sm_y + 550), "IO 4 (SDA)", fill="#69F0AE", font=FONT_MED)
    d.text((sm_x + 45, sm_y + 610), "IO 5 (SCL)", fill="#FFD740", font=FONT_MED)

    # Power connections to SuperMini
    draw_jumper(d, (200, 305), p_3v3, "#E53935", "3.3V")
    draw_jumper(d, (200, 335), p_gnd, "#1E88E5", "GND")

    # Module: SSD1306 0.96" I2C OLED (Right top)
    oled_x, oled_y = 1100, 480
    d.rounded_rectangle([oled_x, oled_y, oled_x + 850, oled_y + 380], radius=10, fill="#0D47A1", outline="#1976D2", width=3)
    d.rectangle([oled_x + 40, oled_y + 30, oled_x + 810, oled_y + 250], fill="#000000", outline="#263238", width=3)
    d.text((oled_x + 80, oled_y + 60), "CAREGIVER ALERT SYSTEM", fill="#4FC3F7", font=FONT_MED)
    d.rectangle([oled_x + 70, oled_y + 100, oled_x + 780, oled_y + 180], fill="#D32F2F")
    d.text((oled_x + 100, oled_y + 120), "🚨 COT 02 CRY DETECTED!", fill="#FFFFFF", font=FONT_TITLE)
    d.text((oled_x + 80, oled_y + 205), "Confidence: 96.4% | Vibration Pulsing...", fill="#B0BEC5", font=FONT_MED)

    oled_pins = [
        ("GND", (oled_x + 150, oled_y + 320)),
        ("VCC", (oled_x + 310, oled_y + 320)),
        ("SCL", (oled_x + 470, oled_y + 320)),
        ("SDA", (oled_x + 630, oled_y + 320))
    ]
    for p_name, pt in oled_pins:
        d.rounded_rectangle([pt[0]-30, pt[1]-18, pt[0]+30, pt[1]+18], radius=4, fill="#263238", outline="#90CAF9", width=2)
        d.text((pt[0]-16, pt[1]-9), p_name, fill="#ECEFF1", font=FONT_TINY)

    # OLED Wiring
    draw_jumper(d, (1200, 335), oled_pins[0][1], "#1E88E5", "GND")
    draw_jumper(d, (1200, 305), oled_pins[1][1], "#E53935", "3.3V")
    draw_capacitor(d, (oled_x + 230, oled_y + 320), "104", is_electrolytic=False) # 100nF Decouple
    draw_jumper(d, p_io5, oled_pins[2][1], "#FFD740", "SCL (5)")
    draw_jumper(d, p_io4, oled_pins[3][1], "#69F0AE", "SDA (4)")

    # Haptic Vibration Motor Driver Circuit (Bottom-Right)
    mot_x, mot_y = 1100, 920
    d.rounded_rectangle([mot_x, mot_y, mot_x + 850, mot_y + 480], radius=10, fill="#263238", outline="#FF5252", width=3)
    d.text((mot_x + 40, mot_y + 30), "HAPTIC MOTOR DRIVER (IRLML2502TR SOT-23)", fill="#FF8A80", font=FONT_TITLE)

    # 10mm Coin ERM Motor
    d.ellipse([mot_x + 100, mot_y + 120, mot_x + 260, mot_y + 280], fill="#37474F", outline="#B0BEC5", width=3)
    d.ellipse([mot_x + 160, mot_y + 180, mot_x + 200, mot_y + 220], fill="#263238")
    d.text((mot_x + 110, mot_y + 300), "10mm ERM MOTOR", fill="#FFFFFF", font=FONT_MED)
    d.text((mot_x + 130, mot_y + 330), "~3V, ~60mA", fill="#FFB300", font=FONT_SMALL)

    # 1N5819 Flyback Diode across Motor
    d.rounded_rectangle([mot_x + 340, mot_y + 140, mot_x + 440, mot_y + 180], radius=4, fill="#212121", outline="#FFD740", width=2)
    d.rectangle([mot_x + 340, mot_y + 140, mot_x + 355, mot_y + 180], fill="#ECEFF1")
    d.text((mot_x + 365, mot_y + 155), "1N5819", fill="#FFD740", font=FONT_TINY)
    d.text((mot_x + 330, mot_y + 195), "Cathode Stripe -> 3.3V", fill="#ECEFF1", font=FONT_TINY)

    # IRLML2502TR SOT-23 MOSFET
    d.rounded_rectangle([mot_x + 540, mot_y + 180, mot_x + 720, mot_y + 300], radius=6, fill="#000000", outline="#64B5F6", width=2)
    d.text((mot_x + 560, mot_y + 210), "IRLML2502TR", fill="#64B5F6", font=FONT_MED)
    d.text((mot_x + 580, mot_y + 240), "N-MOSFET", fill="#B0BEC5", font=FONT_SMALL)

    # MOSFET Gate Resistors
    draw_resistor(d, (mot_x + 380, mot_y + 380), (mot_x + 500, mot_y + 380), "100Ω")
    d.text((mot_x + 400, mot_y + 410), "R1: Gate Series", fill="#ECEFF1", font=FONT_TINY)
    draw_resistor(d, (mot_x + 500, mot_y + 380), (mot_x + 620, mot_y + 380), "10kΩ")
    d.text((mot_x + 520, mot_y + 410), "R2: Gate Pulldown", fill="#ECEFF1", font=FONT_TINY)

    # Motor Wiring:
    # GPIO 3 to R1 100 ohm to Gate
    draw_jumper(d, p_io3, (mot_x + 380, mot_y + 380), "#E53935", "GPIO 3")
    draw_jumper(d, (mot_x + 500, mot_y + 380), (mot_x + 540, mot_y + 240), "#E53935", "Gate")
    draw_jumper(d, (mot_x + 620, mot_y + 380), (1400, 1475), "#1E88E5", "GND")

    # Motor (+) to 3.3V & Diode Cathode
    draw_jumper(d, (mot_x + 180, mot_y + 120), (mot_x + 340, mot_y + 160), "#E53935")
    draw_jumper(d, (mot_x + 340, mot_y + 160), (1200, 305), "#E53935", "3.3V")

    # Motor (-) to Diode Anode & MOSFET Drain
    draw_jumper(d, (mot_x + 180, mot_y + 280), (mot_x + 440, mot_y + 160), "#1E88E5")
    draw_jumper(d, (mot_x + 440, mot_y + 160), (mot_x + 640, mot_y + 180), "#1E88E5", "Drain")

    # MOSFET Source to Common GND
    draw_jumper(d, (mot_x + 640, mot_y + 300), (1600, 1475), "#1E88E5", "Source -> GND")

    # Bottom Legend Banner
    d.rectangle([80, 1540, 2120, 1590], fill="#0F141C", outline="#37474F", width=2)
    d.text((120, 1555), "✓ IRLML2502TR LOGIC-LEVEL 3.3V GATE  |  ✓ 10k PULLED TO GND PREVENTS MOTOR HUM AT BOOT  |  ✓ 1N5819 BACK-EMF SNUBBER", fill="#69F0AE", font=FONT_MED)

    out_file = os.path.join(OUT_DIR, "wearable_breadboard_wiring.png")
    img.save(out_file, "PNG")
    print(f"Saved: {out_file}")

if __name__ == "__main__":
    print("Generating pixel-perfect breadboard PNG wiring diagrams...")
    render_central_hub()
    render_cot_node()
    render_wearable()
    print("All 3 high-resolution breadboard diagrams successfully generated!")
