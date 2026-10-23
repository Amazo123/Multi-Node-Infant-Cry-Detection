import numpy as np
import librosa
import onnxruntime as ort

session = ort.InferenceSession('model_artifacts/tiny_crynet_2s.onnx')

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
        center=True
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
    # Return exactly 81 time frames x 128 bins
    return log_mel.T[:81, :128].astype(np.float32)

t = np.linspace(0, DURATION_SEC, TOTAL_SAMPLES, endpoint=False)

# 1. Whistle clean
whistle_clean = 0.3 * np.sin(2 * np.pi * 1800 * t)

# 2. Whistle with breath noise
wind_noise = np.convolve(np.random.randn(TOTAL_SAMPLES), np.ones(50)/50, mode='same') * 0.2
whistle_breath = whistle_clean + wind_noise

# 3. Infant cry
f0 = 450 + 50 * np.sin(2 * np.pi * 2.5 * t)
phase = 2 * np.pi * np.cumsum(f0) / SAMPLE_RATE
cry = 0.25 * np.sin(phase) + \
      0.18 * np.sin(2 * phase) + \
      0.12 * np.sin(3 * phase) + \
      0.08 * np.sin(4 * phase) + \
      0.05 * np.sin(5 * phase)
envelope = np.clip(np.sin(2 * np.pi * 0.8 * t), 0, 1) ** 0.5
cry = cry * envelope + 0.02 * np.random.randn(TOTAL_SAMPLES)

def run_model(name, pcm):
    mel = get_log_mel(pcm)
    # Zero-mean normalization
    mel_norm = mel - np.mean(mel)
    # Input shape: [1, 1, 81, 128]
    inp = mel_norm[np.newaxis, np.newaxis, :, :]
    out = session.run(None, {'input': inp})[0] # shape [1, 2]
    # Softmax
    exp_out = np.exp(out[0] - np.max(out[0]))
    probs = exp_out / np.sum(exp_out)
    print(f"[{name}] Raw Logits: {out[0]} -> Probabilities: Non-Cry: {probs[0]*100:.2f}%, Cry: {probs[1]*100:.2f}%")

run_model("Whistle (Clean)", whistle_clean)
run_model("Whistle (Breath Noise)", whistle_breath)
run_model("Infant Cry", cry)
