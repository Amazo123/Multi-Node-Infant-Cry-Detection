/**
 * @file i2s_sampler.cpp
 * @brief Implementation of I2S DMA microphone sampler with circular buffer.
 */

#include "i2s_sampler.h"
#include <esp_heap_caps.h>
#include <math.h>

#define DMA_BUF_COUNT 8
#define DMA_BUF_LEN   512

I2SSampler::I2SSampler() 
    : _port(I2S_NUM_0), 
      _ring_buffer(nullptr), 
      _write_idx(0), 
      _buffer_full(false), 
      _mux(portMUX_INITIALIZER_UNLOCKED) {}

I2SSampler::~I2SSampler() {
    i2s_driver_uninstall(_port);
    if (_ring_buffer) {
        free(_ring_buffer);
        _ring_buffer = nullptr;
    }
}

bool I2SSampler::begin(i2s_port_t port, int bclk, int ws, int din) {
    _port = port;

    // Allocate 32,000 samples (64 KB) in Internal SRAM or PSRAM
    _ring_buffer = (int16_t*)heap_caps_malloc(AUDIO_BUFFER_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_ring_buffer) {
        // Fallback to internal RAM if PSRAM not enabled
        _ring_buffer = (int16_t*)malloc(AUDIO_BUFFER_SAMPLES * sizeof(int16_t));
    }

    if (!_ring_buffer) {
        Serial.println("❌ Error: Failed to allocate I2S audio circular buffer!");
        return false;
    }
    memset(_ring_buffer, 0, AUDIO_BUFFER_SAMPLES * sizeof(int16_t));

    // Configure I2S Driver Parameters
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // INMP441 L/R pin tied to GND = Left
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = bclk,
        .ws_io_num = ws,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = din
    };

    esp_err_t err = i2s_driver_install(_port, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("❌ Error installing I2S driver: 0x%x\n", err);
        return false;
    }

    err = i2s_set_pin(_port, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("❌ Error setting I2S pins: 0x%x\n", err);
        return false;
    }

    // Zero out I2S DMA buffers
    i2s_zero_dma_buffer(_port);
    Serial.printf("✅ I2S Sampler active: 16 kHz, 16-bit Mono, 2.0s Circular Buffer (%u bytes)\n", 
                  AUDIO_BUFFER_SAMPLES * sizeof(int16_t));
    return true;
}

void I2SSampler::update() {
    int16_t temp_buf[DMA_BUF_LEN];
    size_t bytes_read = 0;

    esp_err_t result = i2s_read(_port, temp_buf, sizeof(temp_buf), &bytes_read, portMAX_DELAY);
    if (result == ESP_OK && bytes_read > 0) {
        size_t samples = bytes_read / sizeof(int16_t);

        portENTER_CRITICAL(&_mux);
        for (size_t i = 0; i < samples; i++) {
            _ring_buffer[_write_idx] = temp_buf[i];
            _write_idx = (_write_idx + 1) % AUDIO_BUFFER_SAMPLES;
            if (_write_idx == 0) {
                _buffer_full = true;
            }
        }
        portEXIT_CRITICAL(&_mux);
    }
}

void I2SSampler::get_audio_window(float* dest_buffer) {
    if (!dest_buffer || !_ring_buffer) return;

    portENTER_CRITICAL(&_mux);
    size_t start = _buffer_full ? _write_idx : 0;
    for (size_t i = 0; i < AUDIO_BUFFER_SAMPLES; i++) {
        size_t ring_pos = (start + i) % AUDIO_BUFFER_SAMPLES;
        // Normalize int16_t [-32768, 32767] to float [-1.0f, 1.0f]
        dest_buffer[i] = (float)_ring_buffer[ring_pos] / 32768.0f;
    }
    portEXIT_CRITICAL(&_mux);
}

float I2SSampler::get_peak_db() {
    if (!_ring_buffer) return -100.0f;

    int32_t peak = 0;
    portENTER_CRITICAL(&_mux);
    size_t samples_to_check = min((size_t)1600, (size_t)AUDIO_BUFFER_SAMPLES); // check last 100ms
    size_t curr = _write_idx;
    for (size_t i = 0; i < samples_to_check; i++) {
        size_t pos = (curr + AUDIO_BUFFER_SAMPLES - i - 1) % AUDIO_BUFFER_SAMPLES;
        int32_t val = abs(_ring_buffer[pos]);
        if (val > peak) peak = val;
    }
    portEXIT_CRITICAL(&_mux);

    if (peak == 0) return -100.0f;
    float db = 20.0f * log10f((float)peak / 32768.0f);
    return db;
}
