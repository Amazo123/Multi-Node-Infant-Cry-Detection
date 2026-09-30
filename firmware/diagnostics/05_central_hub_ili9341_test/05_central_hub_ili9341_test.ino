/**
 * @file 05_central_hub_ili9341_test.ino
 * @brief Standalone Hardware Diagnostic: Central Hub ILI9341 2.4" SPI TFT Verification.
 * 
 * Target Board: ESP32-S3-DevKitC-1 N16R8
 * Wiring Check (SPI2 Hardware Bus):
 *   - TFT CS:    GPIO 10
 *   - TFT RESET: GPIO 14
 *   - TFT DC/RS: GPIO 13
 *   - TFT MOSI:  GPIO 11
 *   - TFT SCK:   GPIO 12
 *   - TFT VCC:   5V (or 3.3V)
 *   - TFT GND:   GND
 *   - TFT LED:   3.3V (via 100 ohm resistor)
 * 
 * Safety:
 *   - Completely avoids GPIO 33-37 (internally used by the 8MB Octal PSRAM).
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS    10
#define TFT_DC    13
#define TFT_MOSI  11
#define TFT_CLK   12
#define TFT_RST   14

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    delay(500);

    Serial.println("\n==============================================");
    Serial.println("  ESP32-S3 ILI9341 2.4\" SPI TFT TEST TOOL     ");
    Serial.println("==============================================");
    Serial.println("Testing SPI2 bus pins (10, 11, 12, 13, 14)...");

    tft.begin();
    tft.setRotation(1); // Landscape mode 320x240

    Serial.println("✅ ILI9341 initialized. Drawing color test screens...");
}

int test_cycle = 0;

void loop() {
    test_cycle++;
    Serial.printf("🖥️ Running Display Test Cycle #%d...\n", test_cycle);

    // 1. Color Bars
    tft.fillScreen(ILI9341_BLACK);
    tft.fillRect(0, 0, 80, 240, ILI9341_RED);
    tft.fillRect(80, 0, 80, 240, ILI9341_GREEN);
    tft.fillRect(160, 0, 80, 240, ILI9341_BLUE);
    tft.fillRect(240, 0, 80, 240, ILI9341_YELLOW);
    delay(1500);

    // 2. Hub Dashboard UI Simulation
    tft.fillScreen(ILI9341_BLACK);

    // Header
    tft.fillRect(0, 0, 320, 32, 0x0013); // Deep Navy
    tft.drawFastHLine(0, 32, 320, ILI9341_CYAN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 8);
    tft.print("INFANT HUB DISPLAY TEST");

    // Cot Cards
    for (int i = 1; i <= 3; i++) {
        int x = 8 + (i - 1) * 104;
        int y = 42;
        int w = 96;
        int h = 110;

        tft.drawRoundRect(x, y, w, h, 6, ILI9341_CYAN);
        tft.fillRect(x + 2, y + 2, w - 4, 20, 0x10A2);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(1);
        tft.setCursor(x + 22, y + 6);
        tft.printf("COT 0%d", i);

        // Simulation RMS bar
        tft.setCursor(x + 8, y + 34);
        tft.setTextColor(ILI9341_LIGHTGREY);
        tft.print("RMS Energy");
        tft.drawRect(x + 8, y + 48, 80, 10, ILI9341_WHITE);
        tft.fillRect(x + 9, y + 49, (i == 2) ? 65 : 20, 8, (i == 2) ? ILI9341_RED : ILI9341_GREEN);

        tft.setCursor(x + 8, y + 72);
        tft.setTextColor(ILI9341_CYAN);
        tft.printf("Bat: %.2fV", 3.90f - (i * 0.15f));
    }

    // Lower Banner
    tft.fillRect(8, 164, 304, 66, ILI9341_RED);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 176);
    tft.print("🚨 ALERT: COT 02 CRY!");
    tft.setTextSize(1);
    tft.setCursor(22, 204);
    tft.print("Confidence: 96.4% | Wearable Alert Sent");

    Serial.println("✅ Frame drawn successfully! Display working.");
    delay(4000);
}
