"""
Generates an elegant, professionally formatted Microsoft Word (.docx) document
containing the comprehensive Bill of Materials (BOM) and component lists for the
Multi-Node Infant Cry Detection System. Includes blank price columns for manual entry.
"""

import os
from docx import Document
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_ALIGN_VERTICAL
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

OUTPUT_PATH = r"c:\Users\HP\Documents\Antigravity\Multi-node\BILL_OF_MATERIALS_AND_COMPONENTS.docx"

def set_cell_margins(cell, top=100, bottom=100, left=150, right=150):
    """Set padding for a table cell in dxa (1 pt = 20 dxa)"""
    tcPr = cell._element.get_or_add_tcPr()
    tcMar = OxmlElement('w:tcMar')
    for m, val in [('w:top', top), ('w:bottom', bottom), ('w:left', left), ('w:right', right)]:
        node = OxmlElement(m)
        node.set(qn('w:w'), str(val))
        node.set(qn('w:type'), 'dxa')
        tcMar.append(node)
    tcPr.append(tcMar)

def set_cell_background(cell, color_hex):
    """Set background color for a table cell"""
    tcPr = cell._element.get_or_add_tcPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:color'), 'auto')
    shd.set(qn('w:fill'), color_hex)
    tcPr.append(shd)

def create_bom_document():
    doc = Document()

    # Page Margins: 1 inch everywhere
    for section in doc.sections:
        section.top_margin = Inches(1.0)
        section.bottom_margin = Inches(1.0)
        section.left_margin = Inches(1.0)
        section.right_margin = Inches(1.0)

    # Document Title
    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run_title = title.add_run("MULTI-NODE INFANT CRY DETECTION SYSTEM\nWITH WEARABLE CAREGIVER ALERT")
    run_title.font.name = "Arial"
    run_title.font.size = Pt(16)
    run_title.font.bold = True
    run_title.font.color.rgb = RGBColor(16, 44, 87) # Deep Navy

    # Subtitle
    sub = doc.add_paragraph()
    sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run_sub = sub.add_run("Official Hardware Bill of Materials (BOM) & Component Costing Schedule\n")
    run_sub.font.name = "Arial"
    run_sub.font.size = Pt(12)
    run_sub.font.bold = True
    run_sub.font.color.rgb = RGBColor(53, 95, 142)

    # Academic & Student Metadata Block
    meta = doc.add_paragraph()
    meta.paragraph_format.space_after = Pt(14)
    run_meta = meta.add_run(
        "Candidate / Author: Amazing Ayomikun Adelowo (Matric: 22/05/04/0029)\n"
        "Department: Department of Computer Engineering, Faculty of Engineering\n"
        "Institution: University of Maiduguri, Borno State, Nigeria\n"
        "Project Architecture: 3× Cot Sensing Nodes + 1× Central Hub + 1× Caregiver Wearable\n"
        "Currency / Price Column: Blank (Reserved for Student / Market Quotation Input)"
    )
    run_meta.font.name = "Calibri"
    run_meta.font.size = Pt(10.5)
    run_meta.font.color.rgb = RGBColor(60, 60, 60)

    # Horizontal Divider Line
    divider = doc.add_paragraph()
    divider.paragraph_format.space_after = Pt(14)
    run_div = divider.add_run("—" * 68)
    run_div.font.color.rgb = RGBColor(200, 200, 200)

    # Table Definitions
    # Columns: Item # | Component Name | Description & Key Specs | Ref Designator / Pinout | Qty | Unit Price (₦) | Total Price (₦)
    col_widths = [Inches(0.45), Inches(1.35), Inches(2.1), Inches(1.2), Inches(0.45), Inches(0.95), Inches(0.95)]

    def style_header_row(row):
        for idx, cell in enumerate(row.cells):
            cell.width = col_widths[idx]
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
            set_cell_background(cell, "1F4E79") # Deep Professional Steel Navy
            set_cell_margins(cell, top=140, bottom=140, left=100, right=100)
            p = cell.paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            for r in p.runs:
                r.font.name = "Arial"
                r.font.size = Pt(9.5)
                r.font.bold = True
                r.font.color.rgb = RGBColor(255, 255, 255)

    def add_section_header(title_text, subtitle_text):
        h = doc.add_paragraph()
        h.paragraph_format.space_before = Pt(16)
        h.paragraph_format.space_after = Pt(2)
        r_h = h.add_run(title_text)
        r_h.font.name = "Arial"
        r_h.font.size = Pt(12)
        r_h.font.bold = True
        r_h.font.color.rgb = RGBColor(16, 44, 87)

        if subtitle_text:
            s = doc.add_paragraph()
            s.paragraph_format.space_after = Pt(6)
            r_s = s.add_run(subtitle_text)
            r_s.font.name = "Calibri"
            r_s.font.size = Pt(9.5)
            r_s.font.italic = True
            r_s.font.color.rgb = RGBColor(100, 100, 100)

    def populate_table(table, items):
        # Header
        hdr_row = table.rows[0]
        headers = ["Item", "Component Name", "Description & Specifications", "Hardware Pinout / Notes", "Qty", "Unit Price", "Total Price"]
        for idx, text in enumerate(headers):
            p = hdr_row.cells[idx].paragraphs[0]
            r = p.add_run(text)
        style_header_row(hdr_row)

        # Rows
        for item_idx, item in enumerate(items, 1):
            row = table.add_row()
            bg_color = "F9FBFD" if item_idx % 2 == 0 else "FFFFFF"
            for c_idx, cell in enumerate(row.cells):
                cell.width = col_widths[c_idx]
                cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
                set_cell_background(cell, bg_color)
                set_cell_margins(cell, top=100, bottom=100, left=90, right=90)
                p = cell.paragraphs[0]
                p.paragraph_format.space_after = Pt(0)
                p.paragraph_format.space_before = Pt(0)

                # Alignment
                if c_idx in [0, 4]: # Item # and Qty
                    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                elif c_idx in [5, 6]: # Prices
                    p.alignment = WD_ALIGN_PARAGRAPH.RIGHT
                else:
                    p.alignment = WD_ALIGN_PARAGRAPH.LEFT

                run = p.add_run(str(item[c_idx]))
                run.font.name = "Calibri"
                run.font.size = Pt(9.5)
                run.font.color.rgb = RGBColor(30, 30, 30)

    # -------------------------------------------------------------
    # 1. SUBSYSTEM A: COT SENSING NODES (3 UNITS)
    # -------------------------------------------------------------
    add_section_header("1. Subsystem A: Cot Sensing Nodes (3 Dedicated Cot Units)",
                       "Quantities below reflect the combined total required to construct all three (3) active cot sensing nodes.")
    
    cot_items = [
        ["A1", "ESP32-C3-DevKitC-02", "RISC-V single-core @ 160MHz, 4MB Flash, 400KB SRAM, native 2.4GHz Wi-Fi/ESP-NOW", "Node Core MCU (Cots 1, 2, 3)", "3", "", ""],
        ["A2", "INMP441 Digital MEMS Mic", "Omnidirectional 24-bit I2S digital microphone module, bottom-port acoustic inlet", "SCK: GPIO5, WS: GPIO6, SD: GPIO7, L/R: GND", "3", "", ""],
        ["A3", "100kΩ Resistors (0.25W, 1%)", "Metal film precision resistors for 2:1 battery monitoring voltage divider", "GPIO 1 (ADC1_CH1) divider network", "6", "", ""],
        ["A4", "100nF (0.1µF) Ceramic Caps", "Multilayer ceramic capacitors (MLCC) for ADC noise filtering & microphone decoupling", "1x across INMP441 VDD/GND, 1x on GPIO1", "6", "", ""],
        ["A5", "3mm / 5mm Red LED", "High-efficiency Red LED for battery status signaling (solid, slow blink, fast blink)", "GPIO 4 via 330Ω resistor (Active-HIGH)", "3", "", ""],
        ["A6", "3mm / 5mm Blue LED", "High-efficiency Blue LED for ESP-NOW link status; pulls GPIO 9 HIGH at boot", "3.3V -> 330Ω -> LED -> GPIO 9 (Active-LOW)", "3", "", ""],
        ["A7", "330Ω Resistors (0.25W)", "Current-limiting resistors for Red and Blue indicator LEDs", "Red LED (GPIO 4) & Blue LED (GPIO 9)", "6", "", ""],
        ["A8", "TP4056 1S Li-Ion Charger", "1A constant-current/constant-voltage USB-C Li-Ion charging module with protection", "Battery management (Cots 1, 2, 3)", "3", "", ""],
        ["A9", "5V Step-Up Boost Converter", "MT3608 micro DC-DC boost module (converts 3.7V Li-Po to 5.0V system rail)", "Powers ESP32-C3 5V/VIN pin", "3", "", ""],
        ["A10", "1S 3.7V 2000mAh Li-Po Cell", "Rechargeable single-cell lithium-polymer battery with integrated PCM protection", "Primary power source for cot nodes", "3", "", ""],
        ["A11", "47µF - 100µF Electrolytic Caps", "25V radial electrolytic buffer capacitors for RF transmit transient suppression", "Placed across 5V and GND boost rails", "3", "", ""],
        ["A12", "Solderless Breadboards (Half)", "400-tie point solderless half-size breadboards for clean workbench layout", "Subsystem prototype mounting", "3", "", ""],
    ]
    populate_table(doc.add_table(rows=1, cols=7), cot_items)

    # -------------------------------------------------------------
    # 2. SUBSYSTEM B: CENTRAL INTELLIGENCE HUB (1 UNIT)
    # -------------------------------------------------------------
    add_section_header("2. Subsystem B: Central Intelligence Hub (1 Unit)",
                       "The central coordination hub that performs audio reassembly, ESP-DSP feature extraction, and TinyCryNet edge inference. (Buzzer removed for quiet nursery operation).")
    
    hub_items = [
        ["B1", "ESP32-S3-DevKitC-1 N16R8", "Dual-Core Xtensa LX7 @ 240MHz, 16MB Quad-SPI Flash, 8MB Octal PSRAM", "Central Edge-AI Hub (Strictly avoids GPIO 33-37)", "1", "", ""],
        ["B2", "2.4\" ILI9341 Color TFT Display", "240x320 SPI Color TFT with onboard 3.3V LDO; real-time cot GUI dashboard", "CS: 10, MOSI: 11, SCK: 12, DC: 13, RESET: 14", "1", "", ""],
        ["B3", "100Ω Resistor (0.25W)", "Current-limiting backlight ballast resistor for ILI9341 LED backlight", "Placed between 3.3V rail and TFT LED pin", "1", "", ""],
        ["B4", "TP4056 1S Li-Ion Charger", "1A USB-C battery charging module with over-discharge and short protection", "Central hub battery management", "1", "", ""],
        ["B5", "5V Step-Up Boost Converter", "MT3608 DC-DC step-up boost converter stepping 3.7V Li-Po up to 5.0V", "Powers S3 5V/VIN and TFT VCC", "1", "", ""],
        ["B6", "1S 3.7V 3000mAh Li-Ion / Li-Po", "High-capacity rechargeable lithium cell for extended continuous hub runtime", "Primary hub energy reservoir", "1", "", ""],
        ["B7", "100µF 25V Electrolytic Cap", "Radial decoupling capacitor across 5V boost converter output rail", "Suppresses display and Wi-Fi current spikes", "1", "", ""],
        ["B8", "100nF Ceramic Capacitor", "High-frequency ceramic decoupling capacitor placed across ESP32-S3 3.3V rail", "Power supply ripple rejection", "1", "", ""],
        ["B9", "Solderless Breadboard (Full)", "830-tie point solderless breadboard to mount ESP32-S3, TFT, and power modules", "Central hub workbench mounting", "1", "", ""],
    ]
    populate_table(doc.add_table(rows=1, cols=7), hub_items)

    # -------------------------------------------------------------
    # 3. SUBSYSTEM C: CAREGIVER WEARABLE WRISTBAND (1 UNIT)
    # -------------------------------------------------------------
    add_section_header("3. Subsystem C: Caregiver Wearable Alert Wristband (1 Unit)",
                       "Compact wrist-worn unit receiving BLE GATT cry alerts and delivering silent haptic vibration pulses alongside OLED cot text.")
    
    wearable_items = [
        ["C1", "ESP32-C3 SuperMini", "Ultra-compact single-core RISC-V board with castellated pins and USB-C", "Wearable MCU core (BLE Client)", "1", "", ""],
        ["C2", "0.96\" SSD1306 I2C OLED", "128x64 monochrome graphic OLED display; displays cot ID & alarm banner", "SDA: GPIO 4, SCL: GPIO 5, VCC: 3.3V", "1", "", ""],
        ["C3", "IRLML2502TR (or AO3400A)", "Logic-level N-Channel MOSFET in SOT-23 (Vth < 1.2V, Id > 4A, Rds(on) < 45mΩ)", "Gate driven by GPIO 3 (switches motor GND)", "1", "", ""],
        ["C4", "10mm Flat Coin ERM Motor", "3V ~60mA eccentric rotating mass coin vibration motor for silent tactile alerts", "Connected between 3.3V and MOSFET Drain", "1", "", ""],
        ["C5", "1N5819 Schottky Diode", "Fast 40V 1A Schottky flyback diode; protects MOSFET from inductive kickback", "Cathode to 3.3V, Anode to MOSFET Drain", "1", "", ""],
        ["C6", "100Ω Resistor (0.25W)", "Gate series damping resistor to suppress high-frequency switching oscillations", "Between GPIO 3 and MOSFET Gate", "1", "", ""],
        ["C7", "10kΩ Resistor (0.25W)", "Gate pull-down resistor to guarantee motor remains completely OFF at boot", "Between MOSFET Gate and GND", "1", "", ""],
        ["C8", "1S 3.7V 500mAh Li-Po Cell", "Compact ultra-lightweight Li-Po pouch battery for wristband form factor", "Wearable wristband power source", "1", "", ""],
        ["C9", "TP4056 Micro/USB-C Board", "Compact 1S Li-Po charging board with onboard battery protection", "Wristband charging interface", "1", "", ""],
        ["C10", "Mini Breadboard / Perfboard", "170-tie point mini breadboard or compact prototyping perfboard", "Wearable circuit prototyping", "1", "", ""],
    ]
    populate_table(doc.add_table(rows=1, cols=7), wearable_items)

    # -------------------------------------------------------------
    # 4. HARDWARE ACCESSORIES, PROTOTYPING & WORKBENCH TOOLS
    # -------------------------------------------------------------
    add_section_header("4. Subsystem D: Hardware Prototyping Accessories & Workbench Consumables",
                       "Essential wiring interconnects, test leads, and workshop consumables required for complete multi-node bench deployment.")
    
    acc_items = [
        ["D1", "Premium Breadboard Jumpers (M-M)", "65-piece assorted male-to-male flexible solderless jumper wires (various lengths)", "Inter-rail & IC breadboard connections", "2 packs", "", ""],
        ["D2", "Breadboard Jumpers (M-F)", "40-pin female-to-male Dupont jumper ribbon cables for displays & breakouts", "ILI9341 TFT & INMP441 mic connections", "2 packs", "", ""],
        ["D3", "USB-C to USB-A Data Cables", "High-speed 1.0m shielded USB-C data cables (must support data transfer, not charge-only)", "Flashing & serial debugging across 5 MCUs", "3", "", ""],
        ["D4", "2-Pin JST-PH 2.0mm Connectors", "Pre-crimped male/female JST-PH 2.0mm battery pigtail leads with polarity checks", "Battery disconnects for TP4056 chargers", "5 pairs", "", ""],
        ["D5", "Lead-Free Solder Wire (63/37)", "0.8mm rosin-core solder wire spool for header pins and flyback diode assembly", "Breakout header pin soldering", "1 spool", "", ""],
        ["D6", "Heat-Shrink Tubing Assortment", "Assorted diameter polyolefin heat-shrink tubing (1mm - 5mm)", "Insulating diode leads & battery pigtails", "1 kit", "", ""],
    ]
    populate_table(doc.add_table(rows=1, cols=7), acc_items)

    # -------------------------------------------------------------
    # 5. COST SUMMARY TABLE (BLANK TOTALS FOR STUDENT)
    # -------------------------------------------------------------
    add_section_header("5. Grand Cost Summary Table",
                       "Consolidated financial expenditure across all subsystems. Complete the total column upon obtaining local quotations.")

    summary_widths = [Inches(0.6), Inches(4.2), Inches(1.8)]
    summary_table = doc.add_table(rows=1, cols=3)
    sum_hdr = summary_table.rows[0]
    for idx, text in enumerate(["No.", "Subsystem / Expenditure Category", "Subtotal Cost (₦)"]):
        p = sum_hdr.cells[idx].paragraphs[0]
        p.add_run(text)
    
    # Style summary header
    for idx, cell in enumerate(sum_hdr.cells):
        cell.width = summary_widths[idx]
        cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
        set_cell_background(cell, "1F4E79")
        set_cell_margins(cell, top=140, bottom=140, left=100, right=100)
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER if idx == 0 else (WD_ALIGN_PARAGRAPH.LEFT if idx == 1 else WD_ALIGN_PARAGRAPH.RIGHT)
        for r in p.runs:
            r.font.name = "Arial"
            r.font.size = Pt(10)
            r.font.bold = True
            r.font.color.rgb = RGBColor(255, 255, 255)

    summary_rows = [
        ["1", "Subsystem A: 3× Cot Sensing Nodes (MCUs, INMP441 Mics, LEDs, Li-Po Batteries, Boosters)", ""],
        ["2", "Subsystem B: 1× Central Intelligence Hub (ESP32-S3 N16R8, ILI9341 Color TFT, 3000mAh Battery)", ""],
        ["3", "Subsystem C: 1× Caregiver Wearable (ESP32-C3 SuperMini, SSD1306 OLED, MOSFET, ERM Motor)", ""],
        ["4", "Subsystem D: Prototyping Accessories, Cables, Connectors & Solder Consumables", ""],
        ["5", "Contingency / Miscellaneous Workshop Buffer (~10%)", ""],
        ["TOTAL", "GRAND TOTAL ESTIMATED HARDWARE EXPENDITURE", ""]
    ]

    for row_idx, data in enumerate(summary_rows, 1):
        row = summary_table.add_row()
        is_grand_total = (row_idx == len(summary_rows))
        bg_col = "D9E1F2" if is_grand_total else ("F2F2F2" if row_idx % 2 == 0 else "FFFFFF")

        for c_idx, cell in enumerate(row.cells):
            cell.width = summary_widths[c_idx]
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
            set_cell_background(cell, bg_col)
            set_cell_margins(cell, top=110, bottom=110, left=100, right=100)
            p = cell.paragraphs[0]
            p.paragraph_format.space_after = Pt(0)
            p.paragraph_format.space_before = Pt(0)
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if c_idx == 0 else (WD_ALIGN_PARAGRAPH.LEFT if c_idx == 1 else WD_ALIGN_PARAGRAPH.RIGHT)
            run = p.add_run(str(data[c_idx]))
            run.font.name = "Calibri"
            run.font.size = Pt(10)
            if is_grand_total:
                run.font.bold = True
                run.font.color.rgb = RGBColor(16, 44, 87)
            else:
                run.font.color.rgb = RGBColor(30, 30, 30)

    # Verification / Sign-Off Footer
    doc.add_paragraph().paragraph_format.space_before = Pt(20)
    sign = doc.add_paragraph()
    r_s = sign.add_run("Verified By (Student / Candidate): _________________________      Date: ______________\n\nApproved By (Project Supervisor): _________________________      Date: ______________")
    r_s.font.name = "Calibri"
    r_s.font.size = Pt(10)
    r_s.font.color.rgb = RGBColor(80, 80, 80)

    doc.save(OUTPUT_PATH)
    print(f"SUCCESS: Generated professional BOM document at: {OUTPUT_PATH}")

if __name__ == "__main__":
    create_bom_document()
