import numpy as np
import librosa

SAMPLE_RATE = 16000
FFT_SIZE = 512
HOP_SIZE = 400
MEL_BINS = 128
DURATION_SEC = 2.0
TOTAL_SAMPLES = int(SAMPLE_RATE * DURATION_SEC) # 32,000

def get_log_mel(pcm_audio):
    stft = librosa.stft(
        pcm_audio,
        n_fft=FFT_SIZE,
        hop_length=HOP_SIZE,
        win_length=FFT_SIZE,
        window="hann",
        center=False
    )
    power_spec = np.abs(stft) ** 2
    mel_basis = librosa.filters.mel(
        sr=SAMPLE_RATE,
        n_fft=FFT_SIZE,
        n_mels=MEL_BINS,
        fmin=0.0,
        fmax=SAMPLE_RATE / 2.0,
        norm="slaney",
        htk=True
    )
    mel_spec = np.dot(mel_basis, power_spec)
    # Convert to dB clamped at -80 dB
    mel_spec = np.maximum(mel_spec, 1e-10)
    log_mel = 10.0 * np.log10(mel_spec)
    log_mel = np.maximum(log_mel, -80.0)
    # Output shape: (81, 128)
    return log_mel.T[:81, :128].astype(np.float32)

t = np.linspace(0, DURATION_SEC, TOTAL_SAMPLES, endpoint=False)

# 1. Synthesize Whistle (1800 Hz) with breath rumble (blowing near mic)
whistle_clean = 0.3 * np.sin(2 * np.pi * 1800 * t)
# Breath wind: low-pass noise < 500 Hz
wind_noise = np.convolve(np.random.randn(TOTAL_SAMPLES), np.ones(50)/50, mode='same') * 0.15
whistle_with_breath = whistle_clean + wind_noise

# 2. Synthesize Infant Cry:
# F0 ~ 450 Hz with pitch modulation, plus 4 harmonics
f0 = 450 + 50 * np.sin(2 * np.pi * 2.5 * t)
phase = 2 * np.pi * np.cumsum(f0) / SAMPLE_RATE
cry = 0.25 * np.sin(phase) + \
      0.18 * np.sin(2 * phase) + \
      0.12 * np.sin(3 * phase) + \
      0.08 * np.sin(4 * phase) + \
      0.05 * np.sin(5 * phase)
# Envelope: 2 cry expirations
envelope = np.clip(np.sin(2 * np.pi * 0.8 * t), 0, 1) ** 0.5
cry = cry * envelope + 0.02 * np.random.randn(TOTAL_SAMPLES)

# 3. Clap: short 20ms burst
clap = np.zeros(TOTAL_SAMPLES)
clap[4000:4320] = np.random.randn(320) * 0.8

# Extract features
mel_whistle_clean = get_log_mel(whistle_clean)
mel_whistle_breath = get_log_mel(whistle_with_breath)
mel_cry = get_log_mel(cry)
mel_clap = get_log_mel(clap)

def analyze(name, mel_spec):
    # Active frames (> -52 dB max)
    max_per_frame = np.max(mel_spec, axis=1)
    active_idx = np.where(max_per_frame > -52.0)[0]
    n_active = len(active_idx)
    if n_active < 8:
        print(f"[{name}] REJECTED: Transient (<8 active frames: {n_active})")
        return
    
    avg_spec = np.mean(mel_spec[active_idx], axis=0) # shape (128,)
    # Linear power
    linear_pwr = 10.0 ** (avg_spec / 10.0)
    total_pwr = np.sum(linear_pwr)
    
    peak_bin = np.argmax(avg_spec)
    peak_val = avg_spec[peak_bin]
    
    # Top 3 bin energy ratio
    b_min = max(0, peak_bin - 1)
    b_max = min(127, peak_bin + 1)
    top3_pwr = np.sum(linear_pwr[b_min:b_max+1])
    top3_ratio = top3_pwr / (total_pwr + 1e-12)
    
    # Peak count above 8% of max peak linear power
    # Local maxima
    peaks = []
    max_linear = np.max(linear_pwr)
    for m in range(1, 127):
        if linear_pwr[m] > linear_pwr[m-1] and linear_pwr[m] > linear_pwr[m+1]:
            if linear_pwr[m] > 0.05 * max_linear:
                peaks.append(m)
                
    # F0 band (bins 12-28) avg dB
    f0_db = np.mean(avg_spec[12:29])
    # Formants (bins 30-65) avg dB
    formants_db = np.mean(avg_spec[30:66])
    
    print(f"[{name}]:")
    print(f"  Active Frames: {n_active}")
    print(f"  Peak Bin: {peak_bin} (dB: {peak_val:.1f})")
    print(f"  Top-3 Bin Concentration: {top3_ratio*100:.1f}%")
    print(f"  Distinct Local Peaks (>5% max): {len(peaks)} -> {peaks}")
    print(f"  F0 band dB: {f0_db:.1f} dB | Formants band dB: {formants_db:.1f} dB")
    
analyze("Whistle (Clean)", mel_whistle_clean)
analyze("Whistle (With Breath Noise)", mel_whistle_breath)
analyze("Infant Cry", mel_cry)
analyze("Hand Clap", mel_clap)
