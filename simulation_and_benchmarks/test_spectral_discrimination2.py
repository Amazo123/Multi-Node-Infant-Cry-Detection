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
    mel_spec = np.maximum(mel_spec, 1e-10)
    log_mel = 10.0 * np.log10(mel_spec)
    log_mel = np.maximum(log_mel, -80.0)
    return log_mel.T[:81, :128].astype(np.float32)

t = np.linspace(0, DURATION_SEC, TOTAL_SAMPLES, endpoint=False)

# 1. Pure Breath Wind (blowing on mic)
wind = np.convolve(np.random.randn(TOTAL_SAMPLES), np.ones(80)/80, mode='same') * 0.4

# 2. Adult Voice / Speech (F0 ~ 130 Hz, harmonics at 260, 390, 520, etc.)
f0_adult = 130 + 10 * np.sin(2 * np.pi * 3 * t)
phase_a = 2 * np.pi * np.cumsum(f0_adult) / SAMPLE_RATE
adult_voice = 0.3 * np.sin(phase_a) + 0.2 * np.sin(2*phase_a) + 0.15 * np.sin(3*phase_a)
adult_voice *= np.clip(np.sin(2 * np.pi * 3 * t), 0, 1)

# 3. Whistle at 1000 Hz
whistle_1k = 0.3 * np.sin(2 * np.pi * 1000 * t) + np.convolve(np.random.randn(TOTAL_SAMPLES), np.ones(50)/50, mode='same') * 0.1

# 4. Whistle at 1800 Hz with heavy breath noise
whistle_clean = 0.3 * np.sin(2 * np.pi * 1800 * t)
wind_noise = np.convolve(np.random.randn(TOTAL_SAMPLES), np.ones(50)/50, mode='same') * 0.2
whistle_heavy_breath = whistle_clean + wind_noise

# 5. Infant Cry
f0 = 450 + 50 * np.sin(2 * np.pi * 2.5 * t)
phase = 2 * np.pi * np.cumsum(f0) / SAMPLE_RATE
cry = 0.25 * np.sin(phase) + \
      0.18 * np.sin(2 * phase) + \
      0.12 * np.sin(3 * phase) + \
      0.08 * np.sin(4 * phase) + \
      0.05 * np.sin(5 * phase)
envelope = np.clip(np.sin(2 * np.pi * 0.8 * t), 0, 1) ** 0.5
cry = cry * envelope + 0.02 * np.random.randn(TOTAL_SAMPLES)

def score_sound(name, mel_spec):
    max_per_frame = np.max(mel_spec, axis=1)
    active_idx = np.where(max_per_frame > -52.0)[0]
    n_active = len(active_idx)
    if n_active < 8:
        print(f"[{name}] -> REJECTED: Transient ({n_active} active frames)")
        return 0.05
    
    avg_spec = np.mean(mel_spec[active_idx], axis=0) # shape (128,)
    linear_pwr = 10.0 ** (avg_spec / 10.0)
    total_pwr = np.sum(linear_pwr)
    
    peak_bin = np.argmax(avg_spec)
    
    # Top 3 bin energy ratio
    b_min = max(0, peak_bin - 1)
    b_max = min(127, peak_bin + 1)
    top3_pwr = np.sum(linear_pwr[b_min:b_max+1])
    top3_ratio = top3_pwr / (total_pwr + 1e-12)
    
    # Count local peaks
    peaks = []
    max_linear = np.max(linear_pwr)
    for m in range(1, 127):
        if linear_pwr[m] > linear_pwr[m-1] and linear_pwr[m] > linear_pwr[m+1]:
            if linear_pwr[m] > 0.05 * max_linear:
                peaks.append(m)
                
    num_peaks = len(peaks)
    
    # Band energies
    f0_db = np.mean(avg_spec[18:36]) # Infant F0 band: ~300 - 750 Hz (bins 18 to 35)
    formants_db = np.mean(avg_spec[36:70]) # Formants: 750 - 2200 Hz
    
    # --- DISCRIMINATION LOGIC ---
    is_pure_tone = (top3_ratio > 0.55) or (num_peaks <= 1)
    is_wind_rumble = (peak_bin < 14)
    
    score = 0.10 # Base score for any active sound
    
    if is_pure_tone:
        print(f"[{name}] -> REJECTED AS WHISTLE / PURE TONE (Top-3 Ratio: {top3_ratio*100:.1f}%, Peaks: {num_peaks}, PeakBin: {peak_bin})")
        return 0.08
        
    if is_wind_rumble:
        print(f"[{name}] -> REJECTED AS WIND RUMBLE / LOW-FREQ NOISE (PeakBin: {peak_bin})")
        return 0.08
        
    # Infant Cry Criteria:
    # Criterion A: Peak in Infant Vocal Range (bins 18 to 45, ~300 Hz to 1000 Hz)
    if 18 <= peak_bin <= 45:
        score += 0.35
    elif 14 <= peak_bin <= 55:
        score += 0.20
        
    # Criterion B: Rich Harmonic Formants (at least 3 distinct peaks)
    if num_peaks >= 4:
        score += 0.35
    elif num_peaks >= 2:
        score += 0.20
        
    # Criterion C: Broad Spectral Harmonic Dispersion (top-3 < 45%)
    if top3_ratio < 0.45:
        score += 0.15
        
    # Criterion D: High energy in both F0 and formant bands
    if f0_db > -40.0 and formants_db > -45.0:
        score += 0.10
        
    score = min(0.96, max(0.05, score))
    print(f"[{name}] -> Cry Probability: {score*100:.1f}% (Top3: {top3_ratio*100:.1f}%, Peaks: {num_peaks}, PeakBin: {peak_bin})")
    return score

score_sound("Wind Rumble", get_log_mel(wind))
score_sound("Adult Voice", get_log_mel(adult_voice))
score_sound("Whistle 1000Hz", get_log_mel(whistle_1k))
score_sound("Whistle (Heavy Breath)", get_log_mel(whistle_heavy_breath))
score_sound("Infant Cry", get_log_mel(cry))
