/**
 * @file 01_inmp441_audio_plotter.ino
 * @brief Standalone Hardware Diagnostic: INMP441 I2S Microphone Verification
 * 
 * Target Board: ESP32-C3-DevKitC-02 (or ESP32-S3)
 * Wiring:
 *   - SCK (BCLK): GPIO 5
 *   - WS  (LRCK): GPIO 6
 *   - SD  (DIN):  GPIO 7
 *   - L/R:        GND (Left Channel)
 *   - VDD:        3.3V
 *   - GND:        GND
 * 
 * Instructions:
 * 1. Flash to ESP32-C3.
 * 2. Open Arduino IDE -> Tools -> Serial Plotter (baud rate 115200).
 * 3. Make sounds (clap, talk, whistle) near the microphone.
 * 4. You should see a live waveform oscillating smoothly around 0, and RMS tracking amplitude.
 */

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

#define PIN_INMP441_SCK   5
#define PIN_INMP441_WS    6
#define PIN_INMP441_SD    7
#define I2S_PORT          I2S_NUM_0

#define SAMPLE_RATE       16000
#define BLOCK_SAMPLES     256
#define ONBOARD_LED 8

int32_t raw_i2s_buffer[BLOCK_SAMPLES];
int16_t pcm_samples[BLOCK_SAMPLES];

void setup_i2s() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 outputs 24-bit in 32-bit slot
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = BLOCK_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_INMP441_SCK,
        .ws_io_num = PIN_INMP441_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_INMP441_SD
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("❌ Failed to install I2S driver: %d\n", err);
        while(1) delay(100);
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("❌ Failed to set I2S pins: %d\n", err);
        while(1) delay(100);
    }

    i2s_zero_dma_buffer(I2S_PORT);
    Serial.println("✅ INMP441 I2S DMA Initialized successfully at 16,000 Hz!");
}

void setup() {
    pinMode(ONBOARD_LED, OUTPUT);
    digitalWrite(ONBOARD_LED, LOW); // Turn LED ON (Active-LOW)
    Serial.begin(115200);
    // Wait up to 5 seconds for Serial Monitor to connect, blinking the LED
    uint32_t start = millis();
    while (!Serial && (millis() - start < 5000)) {
        digitalWrite(ONBOARD_LED, !digitalRead(ONBOARD_LED));
        delay(100);
    }
    digitalWrite(ONBOARD_LED, LOW); // Solid ON when ready
    delay(500);
    Serial.println("\n==============================================");
    Serial.println("  INMP441 I2S HARDWARE CHECKOUT TOOL (SuperMini)");
    Serial.println("  Switch to Serial Plotter for live graph!");
    Serial.println("==============================================");
    setup_i2s();
}


void loop() {
    size_t bytes_read = 0;
    esp_err_t result = i2s_read(I2S_PORT, raw_i2s_buffer, sizeof(raw_i2s_buffer), &bytes_read, portMAX_DELAY);

    if (result != ESP_OK || bytes_read == 0) {
        Serial.println("⚠️ I2S Read Timeout / Error");
        return;
    }

    size_t samples_read = bytes_read / sizeof(int32_t);
    double sum_sq = 0.0;
    int16_t peak_val = 0;

    for (size_t i = 0; i < samples_read; i++) {
        // INMP441 is 24-bit MSB aligned in 32-bit word -> shift down 14 bits for 16-bit PCM
        int16_t sample = (int16_t)(raw_i2s_buffer[i] >> 14);
        pcm_samples[i] = sample;

        sum_sq += (double)sample * (double)sample;
        int16_t abs_s = abs(sample);
        if (abs_s > peak_val) {
            peak_val = abs_s;
        }
    }

    float rms = sqrt(sum_sq / samples_read);

    // Sanity checks:
    // If RMS is exactly 0.0 for > 10 blocks -> DIN or Clock wire disconnected
    // If peak_val == 32767 -> VDD floating or DC rail saturation
    static int zero_counter = 0;
    if (peak_val == 0) {
        zero_counter++;
        if (zero_counter > 50) {
            Serial.println("⚠️ WARNING: All audio samples are 0! Check SCK (GPIO5), WS (GPIO6), and SD (GPIO7) connections.");
            zero_counter = 0;
        }
    } else {
        zero_counter = 0;
    }

    // Output formatted for Arduino Serial Plotter
    // Sample a few points from the block so plotter doesn't lag
    for (size_t i = 0; i < samples_read; i += 16) {
        Serial.printf("Audio:%d, RMS:%.1f, Peak:%d\n", pcm_samples[i], rms, peak_val);
    }
}
