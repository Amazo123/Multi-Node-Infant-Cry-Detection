/**
 * @file 05_central_hub_ili9341_test.ino
 * @brief Standalone Hardware Diagnostic: Central Hub 2.8" SPI TFT Verification
 *        Includes Automatic Backlight Polarity Detector (Active-LOW vs Active-HIGH).
 * 
 * Target Board: ESP32-S3-DevKitC-1 N16R8
 * Wiring Check (SPI2 Hardware Bus):
 *   - TFT CS:    GPIO 10
 *   - TFT RESET: GPIO 14
 *   - TFT DC/RS: GPIO 13
 *   - TFT MOSI:  GPIO 11
 *   - TFT SCK:   GPIO 12
 *   - TFT VCC:   5V (or 3.3V if J1 is closed)
 *   - TFT GND:   GND
 *   - TFT LED:   GPIO 4 (Backlight control)
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
#define TFT_LED   4   // GPIO 4 drives the display backlight

// Hardware SPI constructor
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2500);
    delay(400);

    Serial.println("\n==============================================");
    Serial.println("  ESP32-S3 2.8\" SPI TFT BACKLIGHT DIAGNOSTIC  ");
    Serial.println("==============================================");

    // Initialize Backlight pin
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, LOW); // Start LOW

    // Initialize Hardware SPI bus with custom pins on ESP32-S3
    SPI.begin(TFT_CLK, -1, TFT_MOSI, TFT_CS);
    tft.begin();
    tft.setRotation(1); // Landscape mode 320x240

    // Fill screen with bright YELLOW initially so it is unmistakable when lit
    tft.fillScreen(ILI9341_YELLOW);
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(3);
    tft.setCursor(20, 90);
    tft.print("DISPLAY WORKING!");

    Serial.println("✅ SPI graphics drawn in RAM. Starting Backlight Toggle Test...\n");
}

int cycle = 0;

void loop() {
    cycle++;
    Serial.printf("----------------- TEST CYCLE #%d -----------------\n", cycle);

    // =========================================================================
    // TEST PHASE A: Drive GPIO 4 LOW (0V / GND) - Active-LOW Check
    // =========================================================================
    digitalWrite(TFT_LED, LOW);
    tft.fillScreen(ILI9341_RED);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(15, 80);
    tft.print("PHASE A: GPIO 4 is LOW");
    tft.setCursor(15, 110);
    tft.print("If ON now -> Active-LOW!");

    Serial.println("💡 [PHASE A] GPIO 4 set to LOW (0V / GND) for 3 seconds...");
    Serial.println("   --> Look at screen: Is the RED screen glowing right now?");
    delay(3000);

    // =========================================================================
    // TEST PHASE B: Drive GPIO 4 HIGH (3.3V) - Active-HIGH Check
    // =========================================================================
    digitalWrite(TFT_LED, HIGH);
    tft.fillScreen(ILI9341_BLUE);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(15, 80);
    tft.print("PHASE B: GPIO 4 is HIGH");
    tft.setCursor(15, 110);
    tft.print("If ON now -> Active-HIGH!");

    Serial.println("💡 [PHASE B] GPIO 4 set to HIGH (3.3V) for 3 seconds...");
    Serial.println("   --> Look at screen: Is the BLUE screen glowing right now?");
    delay(3000);
}
