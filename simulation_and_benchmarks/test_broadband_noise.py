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

# 1. Background Music / TV chatter (multitone + noise)
tv_chatter = 0.1 * np.random.randn(TOTAL_SAMPLES)
for freq in [250, 410, 620, 890, 1150, 1420, 1800, 2300, 2900, 3400, 4100, 5000]:
    tv_chatter += 0.05 * np.sin(2 * np.pi * freq * t + np.random.rand())

# 2. White noise / HVAC hiss
hvac = 0.15 * np.random.randn(TOTAL_SAMPLES)

# 3. Raw DC-biased silence (18000 counts / 32768 ≈ 0.55)
dc_biased_silence = np.ones(TOTAL_SAMPLES) * 0.55 + 0.002 * np.random.randn(TOTAL_SAMPLES)

# 4. DC-biased silence AFTER DC removal (mean subtraction)
dc_removed_silence = dc_biased_silence - np.mean(dc_biased_silence)

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

# Now test discrimination logic
def evaluate(name, mel_spec):
    max_per_frame = np.max(mel_spec, axis=1)
    active_idx = np.where(max_per_frame > -52.0)[0]
    n_active = len(active_idx)
    if n_active < 8:
        print(f"[{name}] -> REJECTED: Low energy / Transient ({n_active} active frames)")
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

    print(f"[{name}] Raw stats: PeakBin={peak_bin}, Top3={top3_ratio*100:.1f}%, Peaks={num_peaks}, F0_db={f0_db:.1f}, Formant_db={formants_db:.1f}")

    # Rejection conditions:
    # 1. Monochromatic pure tone / whistle: top3 > 55% or single peak
    if top3_ratio > 0.55 or num_peaks <= 1:
        print(f"  -> REJECTED as Whistle / Pure Tone")
        return 0.08
    
    # 2. Wind rumble / breath noise / mechanical bump (peak_bin < 14, <220 Hz)
    if peak_bin < 14:
        print(f"  -> REJECTED as Low Frequency Noise / Wind Rumble")
        return 0.08

    # 3. Diffuse broadband noise (HVAC, white noise, TV background chatter):
    # If there are excessive peaks (>14) and power is flatly distributed across bins (top3 < 10%)
    if num_peaks > 12 or top3_ratio < 0.08:
        print(f"  -> REJECTED as Broadband Ambient Noise / Chatter (Too many peaks: {num_peaks} or flat spectrum: {top3_ratio*100:.1f}%)")
        return 0.10

    # Infant Cry Scoring:
    # A true infant cry has:
    # - Peak bin in vocal range (bins 18 to 45, ~300-1000 Hz)
    # - 3 to 8 harmonic formant peaks
    # - Top-3 concentration between 10% and 50%
    # - Strong energy in both F0 and formant bands
    score = 0.10
    if 18 <= peak_bin <= 45:
        score += 0.35
    elif 14 <= peak_bin <= 55:
        score += 0.20

    # Optimal harmonic peak count: 3 to 8 peaks
    if 3 <= num_peaks <= 7:
        score += 0.35
    elif num_peaks == 2 or num_peaks == 8:
        score += 0.20

    if 0.10 <= top3_ratio <= 0.45:
        score += 0.15

    if f0_db > -40.0 and formants_db > -45.0:
        score += 0.10

    score = min(0.96, max(0.05, score))
    print(f"  -> Confirmed Cry Probability: {score*100:.1f}%")
    return score

print("--- Testing without DC removal on silence ---")
evaluate("DC Biased Silence (No Removal)", get_log_mel(dc_biased_silence))

print("\n--- Testing with DC removal on silence ---")
evaluate("DC Removed Silence", get_log_mel(dc_removed_silence))

print("\n--- Testing Other Sounds ---")
evaluate("TV Chatter / Multi-speaker", get_log_mel(tv_chatter))
evaluate("HVAC Noise", get_log_mel(hvac))
evaluate("Infant Cry", get_log_mel(cry))
