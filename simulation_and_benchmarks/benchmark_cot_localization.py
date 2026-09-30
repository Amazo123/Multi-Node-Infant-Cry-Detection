"""
Cot Localization & Noise Stress-Test Benchmark
Project: Multi-Node Infant Cry Detection System with Wearable Caregiver Alert
Student: Amazing Ayomikun Adelowo (22/05/04/0029)

Runs 150 Monte-Carlo simulations testing:
- Directional acoustic propagation across Cot 1 (0m), Cot 2 (2m), and Cot 3 (4m).
- Sweeps Signal-to-Noise Ratios (SNR): +20 dB (quiet), +10 dB, +5 dB, 0 dB, -5 dB (extreme noise).
- Generates Cot Confusion Matrix and prints academic performance table.
"""

import sys
if sys.stdout.encoding != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

import numpy as np

# Physical Geometry: Cot 1 (0m), Cot 2 (2m), Cot 3 (4m)
COT_POSITIONS = {1: 0.0, 2: 2.0, 3: 4.0}
SAMPLE_RATE = 16000
DURATION_SEC = 2.0
TOTAL_SAMPLES = int(SAMPLE_RATE * DURATION_SEC) # 32,000 samples

def acoustic_attenuation(source_x, sensor_x):
    dist = max(0.5, abs(source_x - sensor_x))
    return 1.0 / dist

def generate_cry_waveform(duration_sec=2.0):
    """Generates synthetic infant cry harmonic waveform (450Hz fundamental + wobbles)"""
    t = np.linspace(0, duration_sec, int(SAMPLE_RATE * duration_sec), endpoint=False)
    f0 = 450.0 + 35.0 * np.sin(2 * np.pi * 3.5 * t)
    harmonic = (
        0.50 * np.sin(2 * np.pi * f0 * t) +
        0.30 * np.sin(2 * np.pi * 2 * f0 * t) +
        0.20 * np.sin(2 * np.pi * 3 * f0 * t)
    )
    envelope = np.sin(np.pi * np.linspace(0, 1, len(t))) ** 0.5
    return (harmonic * envelope * 0.70).astype(np.float32)

def run_cot_localization_benchmark(num_trials_per_snr=30):
    snr_levels_db = [20, 15, 10, 5, 0, -5]
    print("\n=========================================================================")
    print("   📊 Multi-Node Cot Localization & Acoustic Stress-Test Benchmark      ")
    print("   Total Trials: 180 (30 trials per SNR level across Cot 1, 2, 3)        ")
    print("=========================================================================\n")

    overall_confusion_matrix = np.zeros((3, 3), dtype=int)
    results_by_snr = {}

    for snr_db in snr_levels_db:
        correct_count = 0
        total_trials = 0
        snr_cm = np.zeros((3, 3), dtype=int)

        # SNR linear scale: SNR = 10 * log10(P_signal / P_noise)
        # P_noise = P_signal / 10^(SNR/10)
        signal = generate_cry_waveform()
        signal_power = np.mean(signal ** 2)
        noise_power = signal_power / (10.0 ** (snr_db / 10.0))
        noise_std = np.sqrt(noise_power)

        for trial in range(num_trials_per_snr):
            # Pick ground truth crying cot uniformly from [1, 2, 3]
            true_cot = np.random.choice([1, 2, 3])
            source_x = COT_POSITIONS[true_cot]

            measured_rms = {}
            for cot_id, pos_x in COT_POSITIONS.items():
                atten = acoustic_attenuation(source_x, pos_x)
                attenuated_cry = signal * atten
                # Gaussian acoustic environmental noise
                room_noise = np.random.normal(0, noise_std, TOTAL_SAMPLES).astype(np.float32)
                received_pcm = attenuated_cry + room_noise

                # 100ms window RMS calculation at the sensor
                recent_samples = received_pcm[-1600:]
                rms = np.sqrt(np.mean(recent_samples ** 2))
                measured_rms[cot_id] = rms

            # Hub Candidate Selection: Highest amplitude wins
            predicted_cot = max(measured_rms, key=measured_rms.get)

            # Update matrices (0-indexed)
            snr_cm[true_cot - 1][predicted_cot - 1] += 1
            overall_confusion_matrix[true_cot - 1][predicted_cot - 1] += 1

            if predicted_cot == true_cot:
                correct_count += 1
            total_trials += 1

        accuracy = (correct_count / total_trials) * 100.0
        results_by_snr[snr_db] = accuracy

    # Print Academic Performance Table
    print(f"{'Acoustic Condition':<25} | {'SNR (dB)':<10} | {'Localization Accuracy':<22}")
    print("-" * 65)
    for snr_db in snr_levels_db:
        if snr_db >= 15:
            cond = "Quiet Nursery / Night"
        elif snr_db >= 5:
            cond = "Normal Daytime Ward"
        elif snr_db == 0:
            cond = "Elevated Ward Noise / Speech"
        else:
            cond = "Severe Multi-Source Noise"

        acc = results_by_snr[snr_db]
        print(f"{cond:<25} | {snr_db:>+4} dB   | {acc:>6.1f}%")

    print("\n=========================================================================")
    print("   🎯 Aggregate Cot Confusion Matrix (Predicted vs Actual Across All SNRs)")
    print("=========================================================================")
    print("                  Predicted Cot 1    Predicted Cot 2    Predicted Cot 3")
    for i in range(3):
        row = overall_confusion_matrix[i]
        total_in_class = np.sum(row)
        pct = (row[i] / total_in_class) * 100.0 if total_in_class > 0 else 0.0
        print(f"Actual Cot 0{i+1} :   {row[0]:>10}         {row[1]:>10}         {row[2]:>10}   (Recall: {pct:.1f}%)")
    print("=========================================================================\n")

if __name__ == "__main__":
    run_cot_localization_benchmark(num_trials_per_snr=30)
