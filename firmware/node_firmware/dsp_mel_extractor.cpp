/**
 * @file dsp_mel_extractor.cpp
 * @brief Fast Log-Mel Spectrogram extraction implementation optimized for ESP32-S3 SIMD instructions.
 */

#include "dsp_mel_extractor.h"
#include <Arduino.h>
#include <math.h>
#include <esp_dsp.h>
#include <esp_heap_caps.h>

#define PI_F 3.14159265358979323846f

static float hz_to_mel(float hz) {
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

static float mel_to_hz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

DSPMelExtractor::DSPMelExtractor()
    : _hann_window(nullptr),
      _fft_input(nullptr),
      _fft_complex(nullptr),
      _power_spectrum(nullptr),
      _mel_weights(nullptr) {}

DSPMelExtractor::~DSPMelExtractor() {
    if (_hann_window) free(_hann_window);
    if (_fft_input) free(_fft_input);
    if (_fft_complex) free(_fft_complex);
    if (_power_spectrum) free(_power_spectrum);
    if (_mel_weights) free(_mel_weights);
}

bool DSPMelExtractor::begin() {
    // 1. Initialize ESP-DSP Radix-2 FFT tables for N=512
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, DSP_FFT_SIZE);
    if (ret != ESP_OK) {
        Serial.printf("❌ Error initializing ESP-DSP FFT tables: 0x%x\n", ret);
        return false;
    }

    // 2. Allocate buffers
    _hann_window    = (float*)malloc(DSP_FFT_SIZE * sizeof(float));
    _fft_input      = (float*)malloc(DSP_FFT_SIZE * sizeof(float));
    _fft_complex    = (float*)malloc(DSP_FFT_SIZE * 2 * sizeof(float)); // Re & Im pairs
    _power_spectrum = (float*)malloc(((DSP_FFT_SIZE / 2) + 1) * sizeof(float));

    if (!_hann_window || !_fft_input || !_fft_complex || !_power_spectrum) {
        Serial.println("❌ Error allocating DSP memory buffers!");
        return false;
    }

    // 3. Precompute Hann window
    for (int i = 0; i < DSP_FFT_SIZE; i++) {
        _hann_window[i] = 0.5f * (1.0f - cosf(2.0f * PI_F * (float)i / (float)(DSP_FFT_SIZE - 1)));
    }

    // 4. Precompute Triangular Mel Filterbank Matrix
    _init_mel_filterbanks();

    Serial.println("✅ DSP Mel Extractor initialized (128 Bands, 512 FFT, 400 Hop, 81 Frames).");
    return true;
}

void DSPMelExtractor::_init_mel_filterbanks() {
    float min_mel = hz_to_mel(0.0f);
    float max_mel = hz_to_mel(DSP_SAMPLE_RATE / 2.0f); // 8000 Hz Nyquist
    float mel_step = (max_mel - min_mel) / (float)(DSP_MEL_BINS + 1);

    // Compute center FFT bin index for each mel point
    float bin_freq_hz = (float)DSP_SAMPLE_RATE / (float)DSP_FFT_SIZE; // 31.25 Hz per bin
    int num_spec_bins = (DSP_FFT_SIZE / 2) + 1; // 257 bins

    // Estimate total non-zero weights
    _mel_weights = (float*)malloc(DSP_MEL_BINS * num_spec_bins * sizeof(float));
    memset(_mel_weights, 0, DSP_MEL_BINS * num_spec_bins * sizeof(float));

    for (int m = 1; m <= DSP_MEL_BINS; m++) {
        float f_m_minus = mel_to_hz(min_mel + (m - 1) * mel_step);
        float f_m       = mel_to_hz(min_mel + m * mel_step);
        float f_m_plus  = mel_to_hz(min_mel + (m + 1) * mel_step);

        int k_minus = (int)floorf(f_m_minus / bin_freq_hz);
        int k_center = (int)roundf(f_m / bin_freq_hz);
        int k_plus  = (int)ceilf(f_m_plus / bin_freq_hz);

        if (k_minus < 0) k_minus = 0;
        if (k_plus >= num_spec_bins) k_plus = num_spec_bins - 1;

        _mel_start_bin[m - 1] = k_minus;
        _mel_end_bin[m - 1]   = k_plus;

        for (int k = k_minus; k <= k_plus; k++) {
            float freq = k * bin_freq_hz;
            float weight = 0.0f;
            if (freq >= f_m_minus && freq <= f_m) {
                weight = (freq - f_m_minus) / (f_m - f_m_minus + 1e-8f);
            } else if (freq > f_m && freq <= f_m_plus) {
                weight = (f_m_plus - freq) / (f_m_plus - f_m + 1e-8f);
            }
            // Slaney area normalization
            float norm_factor = 2.0f / (f_m_plus - f_m_minus + 1e-8f);
            _mel_weights[(m - 1) * num_spec_bins + k] = weight * norm_factor;
        }
    }
}

void DSPMelExtractor::compute_log_mel(const float* pcm_in, float* out_features) {
    if (!pcm_in || !out_features) return;

    int num_spec_bins = (DSP_FFT_SIZE / 2) + 1; // 257 bins
    int max_signal_samples = 32000;

    // Process exactly 81 time frames
    for (int t = 0; t < DSP_TIME_FRAMES; t++) {
        int frame_start = t * DSP_HOP_SIZE;

        // 1. Windowing and Complex Buffer Packing
        for (int i = 0; i < DSP_FFT_SIZE; i++) {
            int src_idx = frame_start + i;
            float val = (src_idx < max_signal_samples) ? pcm_in[src_idx] : 0.0f;
            _fft_complex[i * 2 + 0] = val * _hann_window[i]; // Real part
            _fft_complex[i * 2 + 1] = 0.0f;                  // Imaginary part
        }

        // 2. Compute Radix-2 Complex Fast Fourier Transform using ESP-DSP
        dsps_fft2r_fc32(_fft_complex, DSP_FFT_SIZE);
        dsps_bit_rev2r_fc32(_fft_complex, DSP_FFT_SIZE);

        // 3. Compute Power Spectrum: (|Re|^2 + |Im|^2)
        for (int k = 0; k < num_spec_bins; k++) {
            float re = _fft_complex[k * 2 + 0];
            float im = _fft_complex[k * 2 + 1];
            _power_spectrum[k] = (re * re + im * im);
        }

        // 4. Dot Product with Triangular Mel Filterbanks
        int out_frame_offset = t * DSP_MEL_BINS;
        for (int m = 0; m < DSP_MEL_BINS; m++) {
            float mel_energy = 0.0f;
            int start_k = _mel_start_bin[m];
            int end_k   = _mel_end_bin[m];
            const float* w = &_mel_weights[m * num_spec_bins];

            for (int k = start_k; k <= end_k; k++) {
                mel_energy += _power_spectrum[k] * w[k];
            }

            // 5. Convert to Log Decibels (Power to dB with top_db=80 clamp)
            if (mel_energy < 1e-10f) mel_energy = 1e-10f;
            float db = 10.0f * log10f(mel_energy);
            if (db < -80.0f) db = -80.0f;

            // Output format matches model: [time_frame, mel_bin]
            out_features[out_frame_offset + m] = db;
        }
    }
}
