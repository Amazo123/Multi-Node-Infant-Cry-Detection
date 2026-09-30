"""
Audio Preprocessing Reference Pipeline for Infant Cry Detection.
Compatible with IEEE MMSP 2025 paper and ESP32-S3 ESP-DSP implementation.
Features:
- Sample rate: 16,000 Hz (16-bit mono PCM)
- FFT length: 512
- Hop length: 400 (25 ms)
- Mel filterbanks: 128 triangular filters
- Log compression (Power to dB)
"""

import numpy as np
import librosa

def extract_log_mel_spectrogram(
    audio_pcm: np.ndarray,
    sr: int = 16000,
    n_fft: int = 512,
    hop_length: int = 400,
    n_mels: int = 128,
    top_db: float = 80.0
) -> np.ndarray:
    """
    Computes log Mel-spectrogram from raw PCM float array [-1.0, 1.0].
    Returns:
        np.ndarray of shape (n_mels, time_frames)
    """
    # 1. Short-Time Fourier Transform with Hanning window
    stft = librosa.stft(
        audio_pcm,
        n_fft=n_fft,
        hop_length=hop_length,
        win_length=n_fft,
        window="hann",
        center=True,
        pad_mode="reflect"
    )
    
    # 2. Power Spectrum
    power_spectrogram = np.abs(stft) ** 2
    
    # 3. Mel Filterbank Matrix (128 bands)
    mel_basis = librosa.filters.mel(
        sr=sr,
        n_fft=n_fft,
        n_mels=n_mels,
        fmin=0.0,
        fmax=sr / 2.0,
        norm="slaney",
        htk=True
    )
    
    mel_spectrogram = np.dot(mel_basis, power_spectrogram)
    
    # 4. Logarithmic compression (dB conversion)
    log_mel = librosa.power_to_db(mel_spectrogram, top_db=top_db)
    
    return log_mel

if __name__ == "__main__":
    # Test with 2.0 seconds of synthetic audio
    duration_sec = 2.0
    sr = 16000
    num_samples = int(duration_sec * sr) # 32,000 samples = 64 KB in 16-bit PCM
    synthetic_signal = np.sin(2 * np.pi * 440 * np.linspace(0, duration_sec, num_samples)).astype(np.float32)
    
    log_mel = extract_log_mel_spectrogram(synthetic_signal, sr=sr)
    print("Reference Pipeline executed successfully.")
    print(f"Input audio: {num_samples} samples ({duration_sec}s @ {sr}Hz)")
    print(f"Extracted Log Mel Spectrogram Shape: {log_mel.shape} (Mel bands x Time frames)")
    print(f"Dynamic Range: min={log_mel.min():.2f} dB, max={log_mel.max():.2f} dB")
