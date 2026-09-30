/**
 * @file dsp_mel_extractor.h
 * @brief Embedded Log Mel Spectrogram Extractor using ESP-DSP on ESP32-S3.
 * 
 * Matches reference_pipeline.py and the trained model input shape:
 * - Sampling Rate : 16,000 Hz
 * - FFT Length    : 512
 * - Window Length : 512 (Hann window)
 * - Hop Length    : 400 (25 ms)
 * - Mel Filterbanks: 128 triangular filters (HTK / Slaney scale)
 * - Output shape  : 81 time frames x 128 mel frequency bins
 */

#ifndef DSP_MEL_EXTRACTOR_H
#define DSP_MEL_EXTRACTOR_H

#include <stdint.h>
#include <stddef.h>

#define DSP_SAMPLE_RATE     16000
#define DSP_FFT_SIZE        512
#define DSP_HOP_SIZE        400
#define DSP_MEL_BINS        128
#define DSP_TIME_FRAMES     81
#define DSP_TOTAL_FEATURES  (DSP_TIME_FRAMES * DSP_MEL_BINS) // 10,368 floats

class DSPMelExtractor {
public:
    DSPMelExtractor();
    ~DSPMelExtractor();

    /**
     * @brief Pre-computes Hann window and triangular Mel filterbank weights.
     * Call once during setup().
     * @return true if initialized successfully.
     */
    bool begin();

    /**
     * @brief Transforms 32,000 samples (2.0s @ 16kHz) into a Log-Mel spectrogram.
     * @param pcm_in Pointer to 32,000 normalized float samples [-1.0f, 1.0f].
     * @param out_features Output array of size (81 * 128 = 10,368 floats).
     * Output order is [time_frame, mel_bin], exactly matching TinyCryNet input!
     */
    void compute_log_mel(const float* pcm_in, float* out_features);

private:
    float* _hann_window;
    float* _fft_input;
    float* _fft_complex;
    float* _power_spectrum; // size: FFT_SIZE/2 + 1 = 257 bins
    
    // Sparse Mel filterbank lookup tables to minimize RAM
    uint16_t _mel_start_bin[DSP_MEL_BINS];
    uint16_t _mel_end_bin[DSP_MEL_BINS];
    float*   _mel_weights; // Packed non-zero triangular weights

    void _init_mel_filterbanks();
};

#endif // DSP_MEL_EXTRACTOR_H
