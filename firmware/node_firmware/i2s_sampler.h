/**
 * @file i2s_sampler.h
 * @brief DMA-driven I2S Microphone Driver with Circular Buffer for ESP32-S3.
 * 
 * Configured for 16,000 Hz, 16-bit mono PCM input (e.g., INMP441, SPH0645, MSM261S).
 * Maintains a continuous 2.0-second ring buffer (32,000 samples) ready for DSP processing.
 */

#ifndef I2S_SAMPLER_H
#define I2S_SAMPLER_H

#include <Arduino.h>
#include <driver/i2s.h>

// Audio Acquisition Parameters
#define AUDIO_SAMPLE_RATE    16000
#define AUDIO_WINDOW_SEC     2.0f
#define AUDIO_BUFFER_SAMPLES (int)(AUDIO_SAMPLE_RATE * AUDIO_WINDOW_SEC) // 32,000 samples

// Default Pin Configuration for ESP32-S3 and INMP441 Microphone
// (Adjustable according to your board routing)
#define I2S_PORT             I2S_NUM_0
#define I2S_SCK_PIN          4   // Serial Clock (BCLK)
#define I2S_WS_PIN           5   // Word Select (LRCK)
#define I2S_SD_PIN           6   // Serial Data (DIN/DOUT)

class I2SSampler {
public:
    I2SSampler();
    ~I2SSampler();

    /**
     * @brief Initializes I2S peripheral with DMA descriptors and allocates ring buffer.
     * @return true if initialized successfully, false otherwise.
     */
    bool begin(i2s_port_t port = I2S_PORT, int bclk = I2S_SCK_PIN, int ws = I2S_WS_PIN, int din = I2S_SD_PIN);

    /**
     * @brief Reads incoming DMA audio chunks and pushes them into the circular buffer.
     * Call this inside a dedicated FreeRTOS task on Core 0.
     * @param bytes_read Optional pointer to count of bytes fetched.
     */
    void update();

    /**
     * @brief Copies the most recent 2.0 seconds of audio into destination buffer.
     * Destination must have room for AUDIO_BUFFER_SAMPLES float elements.
     * Audio is converted from int16_t to normalized float in range [-1.0f, 1.0f].
     */
    void get_audio_window(float* dest_buffer);

    /**
     * @brief Computes peak decibel level of current audio window for activity checking.
     */
    float get_peak_db();

private:
    i2s_port_t _port;
    int16_t*   _ring_buffer;
    size_t     _write_idx;
    bool       _buffer_full;
    portMUX_TYPE _mux;
};

#endif // I2S_SAMPLER_H
