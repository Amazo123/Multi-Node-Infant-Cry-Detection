"""
End-to-End Multi-Node System Simulator (Software-in-the-Loop)
Project: Multi-Node Infant Cry Detection System with Wearable Caregiver Alert

Simulates:
1. Three physical cots (Cot 1, Cot 2, Cot 3) with acoustic propagation and room noise.
2. ESP32-C3 Node 1, 2, 3: I2S capture, 64 KB rolling buffer, 100ms RMS telemetry.
3. ESP32-S3 Central Hub: Cot candidate selection, 320-fragment audio pull,
   Log-Mel Spectrogram extraction, ONNX model inference.
4. Caregiver Wearable (ESP32-C3-MINI): BLE GATT event reception & Cot alert verification.
"""

import os
import sys
import time

if sys.stdout.encoding != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

import numpy as np
import onnxruntime as ort
import librosa

# ----------------- Configuration & Physics -----------------
SAMPLE_RATE = 16000
WINDOW_SEC = 2.0
WINDOW_SAMPLES = int(SAMPLE_RATE * WINDOW_SEC)  # 32,000 samples
RMS_INTERVAL_MS = 100
RMS_SAMPLES = int(SAMPLE_RATE * (RMS_INTERVAL_MS / 1000.0)) # 1,600 samples
RMS_TRIGGER_THRESHOLD = 0.18 # Calibrated energy threshold to trigger S3 audio request

def _resolve_model_path():
    candidates = [
        "tiny_crynet_2s.onnx",
        os.path.join(os.path.dirname(__file__), "tiny_crynet_2s.onnx"),
        os.path.join(os.path.dirname(__file__), "..", "model_artifacts", "tiny_crynet_2s.onnx"),
        os.path.join(os.path.dirname(__file__), "model_artifacts", "tiny_crynet_2s.onnx"),
        os.path.join(os.path.dirname(__file__), "..", "Project_Model_Trained_Files", "tiny_crynet_2s.onnx"),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return "tiny_crynet_2s.onnx"

MODEL_PATH = _resolve_model_path()


# Physical Cot Spacing (Linear arrangement along nursery wall)
# Cot 1 at x=0m, Cot 2 at x=2m, Cot 3 at x=4m
COT_POSITIONS = {1: 0.0, 2: 2.0, 3: 4.0}

def acoustic_attenuation(source_x, sensor_x):
    """Computes acoustic inverse-square sound pressure drop with distance floor"""
    dist = max(0.5, abs(source_x - sensor_x))
    # Sound pressure drops roughly 1/dist
    return 1.0 / dist

# ----------------- Edge Sensing Node Simulation (ESP32-C3) -----------------
class SimulatedC3Node:
    def __init__(self, node_id, position_x):
        self.node_id = node_id
        self.position_x = position_x
        self.ring_buffer = np.zeros(WINDOW_SAMPLES, dtype=np.float32)
        self.write_idx = 0
        self.battery_mv = 3850 + np.random.randint(-50, 50)
        self.seq_num = 0

    def ingest_pcm_stream(self, pcm_chunk):
        """Streams audio into 2.0s circular ring buffer"""
        chunk_len = len(pcm_chunk)
        for i in range(chunk_len):
            self.ring_buffer[self.write_idx] = pcm_chunk[i]
            self.write_idx = (self.write_idx + 1) % WINDOW_SAMPLES

    def compute_100ms_rms(self):
        """Calculates RMS of the most recent 100ms audio"""
        recent = np.zeros(RMS_SAMPLES, dtype=np.float32)
        for i in range(RMS_SAMPLES):
            pos = (self.write_idx - 1 - i) % WINDOW_SAMPLES
            recent[i] = self.ring_buffer[pos]
        rms = np.sqrt(np.mean(recent ** 2))
        return float(rms)

    def get_telemetry_packet(self):
        """Tier 1: 12-byte Telemetry Packet"""
        self.seq_num += 1
        rms = self.compute_100ms_rms()
        return {
            "type": "TELEMETRY",
            "node_id": self.node_id,
            "seq": self.seq_num,
            "rms": rms,
            "battery_mv": self.battery_mv
        }

    def pull_64kb_audio_burst(self):
        """Tier 2: 320 Fragments x 200 bytes PCM Audio"""
        ordered_audio = np.zeros(WINDOW_SAMPLES, dtype=np.float32)
        for i in range(WINDOW_SAMPLES):
            pos = (self.write_idx + i) % WINDOW_SAMPLES
            ordered_audio[i] = self.ring_buffer[pos]
            
        # Convert float [-1.0, 1.0] to int16 PCM (64,000 bytes)
        int16_pcm = (ordered_audio * 32767.0).astype(np.int16)
        raw_bytes = int16_pcm.tobytes()

        # Split into 320 packets (200 bytes each)
        fragments = []
        for f in range(320):
            frag = raw_bytes[f * 200 : (f + 1) * 200]
            fragments.append({"frag_idx": f, "data": frag})
        return fragments

# ----------------- Central Hub Simulation (ESP32-S3) -----------------
class SimulatedS3CentralHub:
    def __init__(self, model_onnx_path):
        self.model_onnx_path = model_onnx_path
        self.session = ort.InferenceSession(model_onnx_path)
        self.input_name = self.session.get_inputs()[0].name
        self.output_name = self.session.get_outputs()[0].name
        self.mel_basis = librosa.filters.mel(sr=16000, n_fft=512, n_mels=128, norm="slaney", htk=True)

    def select_candidate_cot(self, telemetry_reports):
        """
        Cot Identification: Compares 100ms RMS amplitude across all nodes.
        Selects the candidate cot with the highest acoustic energy.
        """
        best_node = None
        max_rms = -1.0
        for pkt in telemetry_reports:
            if pkt["rms"] > max_rms:
                max_rms = pkt["rms"]
                best_node = pkt["node_id"]

        spike_detected = (max_rms >= RMS_TRIGGER_THRESHOLD)
        return best_node, max_rms, spike_detected

    def reassemble_audio_buffer(self, fragments):
        """Reassembles 320 fragments into single continuous PCM array"""
        reassembled_bytes = bytearray(64000)
        for frag in fragments:
            idx = frag["frag_idx"]
            reassembled_bytes[idx * 200 : (idx + 1) * 200] = frag["data"]
            
        int16_array = np.frombuffer(reassembled_bytes, dtype=np.int16)
        float_pcm = int16_array.astype(np.float32) / 32767.0
        return float_pcm

    def compute_log_mel_spectrogram(self, pcm_audio):
        """Matches ESP-DSP 81x128 feature extractor"""
        stft = librosa.stft(pcm_audio, n_fft=512, hop_length=400, win_length=512, window="hann", center=True)
        power_spec = np.abs(stft) ** 2
        mel_spec = np.dot(self.mel_basis, power_spec)
        log_mel = librosa.power_to_db(mel_spec, top_db=80.0)
        # Shape: (128 mels, 81 frames) -> transpose to (81 frames, 128 mels)
        transposed = log_mel.T[:81, :128]
        # Reshape to (1, 1, 81, 128) for TinyCryNet ONNX input
        feat = transposed.reshape(1, 1, 81, 128).astype(np.float32)
        # Sentence normalization (match training pipeline)
        feat = feat - np.mean(feat)
        return feat

    def run_tiny_crynet_inference(self, log_mel_tensor):
        """Runs trained ONNX model"""
        raw_logits = self.session.run([self.output_name], {self.input_name: log_mel_tensor})[0]
        exp_vals = np.exp(raw_logits - np.max(raw_logits, axis=-1, keepdims=True))
        probs = exp_vals / np.sum(exp_vals, axis=-1, keepdims=True)
        # Class 0: cry, Class 1: non-cry (as confirmed in label encoder: positive=0, negative=1)
        cry_confidence = probs[0][0] 
        return cry_confidence

# ----------------- Caregiver Wearable Simulation (ESP32-C3-MINI) -----------------
class SimulatedWearable:
    def __init__(self):
        self.vibrating = False
        self.oled_display = "MONITORING..."

    def receive_ble_alert(self, cot_id, confidence):
        self.vibrating = True
        self.oled_display = f"🚨 COT 0{cot_id} - CRY DETECTED! ({confidence*100:.1f}%)"
        print(f"\n=======================================================")
        print(f"⌚ [CAREGIVER WEARABLE BUZZING] Motor ON (AO3400A)")
        print(f"   OLED Screen Render: \n   ┌─────────────────────────────────────┐")
        print(f"   │      🚨 COT 0{cot_id} CRY ALERT! 🚨        │")
        print(f"   │      Confidence: {confidence*100:.1f}%              │")
        print(f"   └─────────────────────────────────────┘")
        print(f"=======================================================\n")

# ----------------- Main Simulation Test Bench -----------------
def generate_synthetic_acoustic_scene(cry_origin_cot=2, duration_sec=3.0):
    """
    Generates a 3-second multi-channel acoustic scenario:
    - 0.0s to 1.0s: Ambient room background noise.
    - 1.0s to 3.0s: Baby crying at Cot 2 (origin).
    Propagates sound to Cot 1, 2, and 3 according to distance attenuation.
    """
    total_samples = int(duration_sec * SAMPLE_RATE)
    time_axis = np.linspace(0, duration_sec, total_samples, endpoint=False)
    
    # 1. Base Ambient Noise (Nursery air conditioner / quiet room)
    background_noise = np.random.normal(0, 0.015, total_samples).astype(np.float32)

    # 2. Infant Cry Audio Signal (Fundamental frequency 450Hz + harmonics)
    cry_signal = np.zeros(total_samples, dtype=np.float32)
    cry_mask = time_axis >= 1.0 # Cry starts at 1.0 second mark
    
    # Harmonic cry model: 450Hz fundamental + 900Hz + 1350Hz with modulations
    t_cry = time_axis[cry_mask]
    f0 = 450.0 + 30.0 * np.sin(2 * np.pi * 3.5 * t_cry) # pitch wobble
    cry_audio = (
        0.50 * np.sin(2 * np.pi * f0 * t_cry) +
        0.30 * np.sin(2 * np.pi * 2 * f0 * t_cry) +
        0.20 * np.sin(2 * np.pi * 3 * f0 * t_cry)
    )
    # Apply envelope
    envelope = np.sin(np.pi * np.linspace(0, 1, len(t_cry))) ** 0.5
    cry_signal[cry_mask] = (cry_audio * envelope * 0.70).astype(np.float32)

    # 3. Propagate to each Cot based on physical geometry
    source_x = COT_POSITIONS[cry_origin_cot]
    channel_audio = {}
    for cot_id, pos_x in COT_POSITIONS.items():
        atten = acoustic_attenuation(source_x, pos_x)
        # Sensed audio = attenuated cry + background noise
        channel_audio[cot_id] = (cry_signal * atten + background_noise).astype(np.float32)

    return channel_audio, total_samples

def run_simulation():
    print("\n=======================================================")
    print("   👶 Multi-Node Infant Cry Detection SIL Simulator    ")
    print("=======================================================")
    
    if not os.path.exists(MODEL_PATH):
        print(f"❌ Error: Model {MODEL_PATH} not found in current directory!")
        return

    # Initialize Hardware Nodes
    nodes = {
        1: SimulatedC3Node(node_id=1, position_x=COT_POSITIONS[1]),
        2: SimulatedC3Node(node_id=2, position_x=COT_POSITIONS[2]),
        3: SimulatedC3Node(node_id=3, position_x=COT_POSITIONS[3])
    }
    hub = SimulatedS3CentralHub(MODEL_PATH)
    wearable = SimulatedWearable()

    # Generate Scenario: Infant begins crying at Cot 2
    TRUE_CRY_COT = 2
    print(f"🎯 Scenario: Baby starts crying at COT 0{TRUE_CRY_COT} at t=1.00s")
    print(f"   Cot Layout: [Cot 1 (0m)] <--- 2m ---> [Cot 2 (2m)] <--- 2m ---> [Cot 3 (4m)]\n")

    audio_streams, total_samples = generate_synthetic_acoustic_scene(cry_origin_cot=TRUE_CRY_COT, duration_sec=3.0)

    # Step through time in 100ms chunks (simulating real-time clock)
    chunk_size = RMS_SAMPLES # 1,600 samples per 100ms
    num_steps = total_samples // chunk_size
    alert_triggered = False

    print("⏱️ Simulation Timeline:")
    for step in range(num_steps):
        sim_time = (step + 1) * 0.10
        start_idx = step * chunk_size
        end_idx = start_idx + chunk_size

        # 1. Each C3 Node ingests its audio chunk
        telemetry_reports = []
        for cot_id, node in nodes.items():
            pcm_chunk = audio_streams[cot_id][start_idx:end_idx]
            node.ingest_pcm_stream(pcm_chunk)
            telemetry_reports.append(node.get_telemetry_packet())

        # Log periodic RMS levels
        rms_str = " | ".join([f"Cot {r['node_id']}: {r['rms']:.3f}" for r in telemetry_reports])
        print(f"t={sim_time:.2f}s | RMS: [{rms_str}]", end="")

        # 2. Central S3 Hub evaluates candidate cot
        candidate_cot, peak_rms, spike = hub.select_candidate_cot(telemetry_reports)

        if not spike:
            print(" -> Status: Quiet (Below Threshold)")
        else:
            print(f" -> ⚡ SPIKE on Cot {candidate_cot} (RMS: {peak_rms:.3f})")
            print(f"\n[HUB ACTION] S3 initiates Tier 2 Audio Pull from Candidate Cot {candidate_cot}...")
            t_pull_start = time.time()

            # 3. Pull 64 KB Audio (320 Fragments) over ESP-NOW
            fragments = nodes[candidate_cot].pull_64kb_audio_burst()
            reassembled_audio = hub.reassemble_audio_buffer(fragments)
            t_pull_ms = (time.time() - t_pull_start) * 1000.0

            # 4. DSP Log-Mel Feature Extraction (ESP-DSP equivalent)
            t_dsp_start = time.time()
            log_mel = hub.compute_log_mel_spectrogram(reassembled_audio)
            t_dsp_ms = (time.time() - t_dsp_start) * 1000.0

            # 5. Neural Network Inference
            t_inf_start = time.time()
            confidence = hub.run_tiny_crynet_inference(log_mel)
            t_inf_ms = (time.time() - t_inf_start) * 1000.0

            print(f"   • Reassembled: 64,000 bytes ({len(fragments)} frags) in {t_pull_ms:.1f}ms")
            print(f"   • DSP Extraction (81x128 Log-Mel): {t_dsp_ms:.1f}ms")
            print(f"   • TinyCryNet Inference Time: {t_inf_ms:.1f}ms")
            print(f"   • ML Cry Confidence: {confidence*100:.1f}%")

            # 6. Check Decision Threshold & Alert Wearable
            if confidence >= 0.40:
                print(f"   🎯 VERDICT: Cry Confirmed on COT 0{candidate_cot}!")
                wearable.receive_ble_alert(candidate_cot, confidence)
                alert_triggered = True
                
                # Check localization accuracy
                if candidate_cot == TRUE_CRY_COT:
                    print(f"✅ SUCCESS: Localization 100% Correct (True Cot: {TRUE_CRY_COT}, Predicted: {candidate_cot})")
                else:
                    print(f"❌ FAIL: Cot Mismatch! (True Cot: {TRUE_CRY_COT}, Predicted: {candidate_cot})")
                break
            else:
                print("   ℹ️ Dismissed as transient non-cry noise.")

    if not alert_triggered:
        print("\n⚠️ Simulation ended without detection.")

if __name__ == "__main__":
    run_simulation()
