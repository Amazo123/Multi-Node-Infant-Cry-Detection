import numpy as np

def sigmoid(z):
    return 1.0 / (1.0 + np.exp(-z))

def compute_ml_prob(top3_ratio, num_peaks, peak_bin, f0_db, formant_db, active_frames):
    # Acoustic Features:
    # 1. Harmonic Multi-Peak Factor: Infant cry has 3-6 harmonics; whistle has 1.
    f_peaks = (num_peaks - 2.0) # <0 for 1 peak (whistle), >0 for 3+ peaks (cry)
    
    # 2. Spectral Concentration Factor: Whistle is >0.60, Cry is <0.45.
    # We measure dispersion: (0.50 - top3_ratio)
    f_dispersion = (0.50 - top3_ratio) * 10.0 # whistle (0.89): -3.9; cry (0.35): +1.5
    
    # 3. Peak Bin Infant Vocal Range Factor:
    # Centered around infant F0 (bin 28 ~ 500 Hz).
    dist_f0 = abs(peak_bin - 28)
    f_vocal_range = 2.0 - (dist_f0 / 12.0) # >0 if near 28, <0 if far (like bin 1 or bin 64)
    
    # 4. Formant Energy Resonance:
    f_formants = (formant_db + 45.0) / 10.0 # >0 if formant energy is strong (> -45 dB)
    
    # 5. Low Frequency Wind Rumble Penalty:
    f_wind_penalty = -3.0 if peak_bin < 14 else 0.0
    
    # Linear Model (Logit):
    z = -1.0 + (1.2 * f_peaks) + (0.8 * f_dispersion) + (0.9 * f_vocal_range) + (0.5 * f_formants) + f_wind_penalty
    
    prob = sigmoid(z)
    print(f"z={z:+.2f} -> Cry Prob: {prob*100:.1f}%")
    return prob

print("--- User's Actual Whistle Test Data ---")
# PeakBin=1, Top-3=89.3%, Peaks=1, F0=-29.5dB, Formants=-35.3dB
compute_ml_prob(top3_ratio=0.893, num_peaks=1, peak_bin=1, f0_db=-29.5, formant_db=-35.3, active_frames=45)

print("\n--- Clean High Whistle (1800Hz) ---")
# PeakBin=64, Top-3=98%, Peaks=1, Formants=-60dB
compute_ml_prob(top3_ratio=0.98, num_peaks=1, peak_bin=64, f0_db=-70, formant_db=-60, active_frames=45)

print("\n--- Infant Cry Sample ---")
# PeakBin=25, Top-3=35.7%, Peaks=4, F0=-21dB, Formants=-22dB
compute_ml_prob(top3_ratio=0.357, num_peaks=4, peak_bin=25, f0_db=-21.0, formant_db=-22.0, active_frames=65)

print("\n--- Adult Speech Sample ---")
# PeakBin=7, Top-3=48%, Peaks=3, Formants=-48dB
compute_ml_prob(top3_ratio=0.48, num_peaks=3, peak_bin=7, f0_db=-35.0, formant_db=-48.0, active_frames=35)
