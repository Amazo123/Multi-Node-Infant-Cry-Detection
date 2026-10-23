# ==============================================================================
# Infant Cry Detection: Fast Google Colab Script with Google Drive Real-Time Backup
# Runtime optimized to complete in ~2.5 to 3.5 hours (well under the 5-hour limit)
# ==============================================================================

# %% [markdown]
# ### STEP 1: GPU Check, Dependencies & Google Drive Mount
# Ensures all checkpoints are saved directly to Google Drive in real-time.

# %%
!nvidia-smi
!pip install -q speechbrain hyperpyyaml datasets huggingface_hub librosa soundfile onnx onnxscript

import os
import sys
import shutil
from google.colab import drive

# 1. Mount Google Drive
print("Mounting Google Drive...")
drive.mount('/content/drive')

# 2. Setup permanent storage directory on Google Drive
DRIVE_PROJECT_DIR = "/content/drive/MyDrive/TinyCryNet_Project"
DRIVE_CKPT_DIR = os.path.join(DRIVE_PROJECT_DIR, "results")
os.makedirs(DRIVE_CKPT_DIR, exist_ok=True)

print(f"✅ Permanent checkpoint backup directory: {DRIVE_CKPT_DIR}")

# %% [markdown]
# ### STEP 2: Clone Repository & Symlink Checkpoints to Google Drive

# %%
REPO_DIR = "/content/ICD_MMSP"
if not os.path.exists(REPO_DIR):
    !git clone https://github.com/fhfjsd1/ICD_MMSP.git {REPO_DIR}

%cd {REPO_DIR}

# Symlink results directory directly to Google Drive
# Whenever SpeechBrain writes an epoch checkpoint, it writes directly to Drive!
repo_results = os.path.join(REPO_DIR, "results")
if os.path.exists(repo_results) and not os.path.islink(repo_results):
    !rm -rf {repo_results}
if not os.path.exists(repo_results):
    os.symlink(DRIVE_CKPT_DIR, repo_results)

print(f"✅ Checkpoint symlink configured: {repo_results} -> {DRIVE_CKPT_DIR}")

# ──────────────────────────────────────────────────────────────────────
# CRITICAL FIX: Write TinyCryNet_ESP32 into the cloned repo so
# SpeechBrain trains the CORRECT model (not the 12M-param proposed_model)
# ──────────────────────────────────────────────────────────────────────
tiny_crynet_module = '''
import torch
import torch.nn as nn
import torch.nn.functional as F

class BSConvU(nn.Module):
    def __init__(self, in_channels, out_channels, kernel_size=3, stride=1, padding=1, bias=True, **kwargs):
        super().__init__()
        self.pw = nn.Conv2d(in_channels, out_channels, kernel_size=1, stride=1, padding=0, bias=False)
        self.dw = nn.Conv2d(out_channels, out_channels, kernel_size=kernel_size, stride=stride,
                            padding=padding, groups=out_channels, bias=bias)
    def forward(self, x):
        return self.dw(self.pw(x))

class CCALayer(nn.Module):
    def __init__(self, channel, reduction=8):
        super().__init__()
        self.avg_pool = nn.AdaptiveAvgPool2d(1)
        self.conv_du = nn.Sequential(
            nn.Conv2d(channel, max(8, channel // reduction), 1, padding=0, bias=True),
            nn.ReLU(inplace=True),
            nn.Conv2d(max(8, channel // reduction), channel, 1, padding=0, bias=True),
            nn.Sigmoid(),
        )
    def forward(self, x):
        mean = x.mean(dim=(2, 3), keepdim=True)
        var = ((x - mean) ** 2).mean(dim=(2, 3), keepdim=True)
        contrast = torch.sqrt(var + 1e-8)
        y = contrast + self.avg_pool(x)
        return x * self.conv_du(y)

class CompactConvBlock(nn.Module):
    def __init__(self, in_channels, out_channels, kernel_size=3, stride=1, padding=1):
        super().__init__()
        self.conv1 = BSConvU(in_channels, out_channels, kernel_size=kernel_size, stride=stride, padding=padding)
        self.bn1 = nn.BatchNorm2d(out_channels)
        self.conv2 = BSConvU(out_channels, out_channels, kernel_size=kernel_size, stride=1, padding=padding)
        self.bn2 = nn.BatchNorm2d(out_channels)
    def forward(self, x, pool_size=(2, 2)):
        x = F.relu(self.bn1(self.conv1(x)))
        x = F.relu(self.bn2(self.conv2(x)))
        if pool_size is not None:
            x = F.avg_pool2d(x, kernel_size=pool_size)
        return x

class TinyCryNet_ESP32(nn.Module):
    """Edge-optimized infant cry classifier for ESP32-S3 deployment."""
    def __init__(self, in_channels=1, classes_num=2, activation="logsoftmax", temporal_type="conv1d"):
        super().__init__()
        self.activation = activation
        self.temporal_type = temporal_type
        self.block1 = CompactConvBlock(in_channels, 32)
        self.block2 = CompactConvBlock(32, 64)
        self.block3 = CompactConvBlock(64, 128)
        self.block4 = CompactConvBlock(128, 128)
        self.block5 = CompactConvBlock(128, 128)
        self.fuse_conv = nn.Conv2d(128 * 3, 128, kernel_size=1)
        self.cca = CCALayer(128)
        if temporal_type == "gru":
            self.temporal = nn.GRU(input_size=128, hidden_size=64, num_layers=1, batch_first=True, bidirectional=True)
            fc_in = 128
        else:
            self.temporal = nn.Sequential(nn.Conv1d(128, 64, kernel_size=3, padding=1), nn.ReLU(), nn.AdaptiveAvgPool1d(1))
            fc_in = 64
        self.classifier = nn.Sequential(
            nn.Linear(fc_in, 64), nn.ReLU(), nn.Dropout(0.3), nn.Linear(64, classes_num)
        )
    def forward(self, x):
        if x.dim() == 3:
            x = x.unsqueeze(1)
        x1 = self.block1(x, pool_size=(2, 2))
        x2 = self.block2(x1, pool_size=(2, 2))
        x3 = self.block3(x2, pool_size=(2, 2))
        x4 = self.block4(x3, pool_size=None)
        x5 = self.block5(x4, pool_size=None)
        fused = torch.cat([x3, x4, x5], dim=1)
        fused = self.fuse_conv(fused)
        fused = self.cca(fused)
        f_pooled = torch.mean(fused, dim=3)
        if self.temporal_type == "gru":
            seq = f_pooled.permute(0, 2, 1)
            gru_out, _ = self.temporal(seq)
            t_pooled = torch.mean(gru_out, dim=1)
        else:
            t_pooled = self.temporal(f_pooled).squeeze(-1)
        out = self.classifier(t_pooled)
        if self.activation == "logsoftmax":
            out = F.log_softmax(out, dim=-1)
        return out
'''

tiny_crynet_path = os.path.join(REPO_DIR, "model", "tiny_crynet_esp32.py")
with open(tiny_crynet_path, "w") as f:
    f.write(tiny_crynet_module)
print(f"✅ Wrote TinyCryNet_ESP32 model into: {tiny_crynet_path}")

# %% [markdown]
# ### STEP 3: Fast Download & Flatten Dataset from Hugging Face

# %%
import os
import glob
import shutil
import zipfile
from huggingface_hub import snapshot_download

DATA_DIR = "/content/dataset"
os.makedirs(DATA_DIR, exist_ok=True)

print("1. Downloading infant-cry-detection dataset...")
snapshot_download(repo_id="hoyyu1/infant-cry-detection", repo_type="dataset", local_dir=DATA_DIR)

print("2. Unzipping audio archives...")
for split in ["negative", "positive"]:
    zip_path = os.path.join(DATA_DIR, f"audio/{split}.zip")
    target_dir = os.path.join(DATA_DIR, f"audio/{split}")
    if os.path.exists(zip_path) and (not os.path.exists(target_dir) or len(os.listdir(target_dir)) == 0):
        os.makedirs(target_dir, exist_ok=True)
        with zipfile.ZipFile(zip_path, 'r') as zf:
            zf.extractall(target_dir)
        print(f"Unzipped {split} audio.")

rir_zip = os.path.join(DATA_DIR, "rir.zip")
rir_dir = os.path.join(DATA_DIR, "rir")
if os.path.exists(rir_zip) and (not os.path.exists(rir_dir) or len(os.listdir(rir_dir)) == 0):
    os.makedirs(rir_dir, exist_ok=True)
    with zipfile.ZipFile(rir_zip, 'r') as zf:
        zf.extractall(rir_dir)
    print("Unzipped RIR responses.")

print("3. Flattening any nested folders...")
for category in ["negative", "positive"]:
    cat_dir = os.path.join(DATA_DIR, f"audio/{category}")
    nested_wavs = glob.glob(f"{cat_dir}/**/*.wav", recursive=True)
    moved = 0
    for wav in nested_wavs:
        if os.path.dirname(wav) != cat_dir:
            shutil.move(wav, os.path.join(cat_dir, os.path.basename(wav)))
            moved += 1
    count = len(glob.glob(f"{cat_dir}/*.wav"))
    print(f"Verified {count} wavs in {category} (flattened {moved} nested files)")

meta_dir = os.path.join(DATA_DIR, "metadata")
os.makedirs(meta_dir, exist_ok=True)
clean_csv = os.path.join(REPO_DIR, "data/metadata/all28479.csv")
dest_csv = os.path.join(meta_dir, "all28479.csv")
if os.path.exists(clean_csv):
    shutil.copyfile(clean_csv, dest_csv)
    print(f"Copied metadata CSV -> {dest_csv}")
else:
    candidates = glob.glob(f"{REPO_DIR}/**/all28479.csv", recursive=True)
    if candidates:
        shutil.copyfile(candidates[0], dest_csv)
        print(f"Copied {candidates[0]} -> {dest_csv}")

# %% [markdown]
# ### STEP 4: Speed Patch `train_test.py`
# Removes per-batch print statements that cause severe I/O slowdown in Colab.

# %%
with open("train_test.py", "r") as f:
    code = f.read()

# Switch from test to train mode
code = code.replace("TEST = True", "TEST = False")

# Silence high-frequency batch stdout logs
code = code.replace('print(f"embeddings shape: {embeddings.shape}")', '# print(embeddings.shape)')
code = code.replace('print(f"feats shape: {feats.shape}")', '# print(feats.shape)')

with open("train_test.py", "w") as f:
    f.write(code)

print("✅ train_test.py optimized for high-throughput execution!")

# %% [markdown]
# ### STEP 5: High-Throughput Hyperparameter Configuration
# Batch size 64 + 4 DataLoader workers reduces runtime from >5 hrs down to ~2.5 - 3.5 hrs.

# %%
fast_hparams = """
seed: 1999
__set_seed: !apply:speechbrain.utils.seed_everything [!ref <seed>]

# Folder paths
data_folder: /content/dataset
data_folder_rir: !ref <data_folder>/rir
audio_data_folder: !ref <data_folder>/audio
output_folder: !ref ./results/<seed>
save_folder: !ref <output_folder>/save
train_log: !ref <output_folder>/train_log.txt
csv_name: all28479.csv

# Logging
use_tensorboard: True
tensorboard_logs_folder: !ref <output_folder>/tb_logs/

# Manifest files
noise_annotation: !ref <data_folder>/metadata/noise.csv
noise_annotation_v: !ref <data_folder>/metadata/noise_v.csv
rir_annotation: !ref <data_folder>/metadata/rir.csv
train_annotation: !ref <data_folder>/metadata/train.json
valid_annotation: !ref <data_folder>/metadata/valid.json

train_fold_nums: [1, 2, 3, 4]
valid_fold_nums: [5]
skip_manifest_creation: True
ckpt_interval_minutes: 10

# Optimized Training Parameters (50 epochs for better convergence)
number_of_epochs: 50
batch_size: 64
lr: 0.001
base_lr: 0.00001
max_lr: !ref <lr>
step_size: 1800
sample_rate: 16000
dropout_prob: 0.3

# Feature extraction (16kHz, 512 FFT, 400 Hop -> 81 frames for 2.0s)
n_mels: 128
n_fft: 512
win_length: 512
hop_length: 400
amp_to_db: True
normalize: True
out_n_neurons: 2

# Parallelized DataLoaders
dataloader_options:
    batch_size: !ref <batch_size>
    shuffle: True
    num_workers: 4
    pin_memory: True

# Audio Feature Extraction
compute_features: !new:torchaudio.transforms.MelSpectrogram
    sample_rate: !ref <sample_rate>
    n_fft: !ref <n_fft>
    win_length: !ref <win_length>
    hop_length: !ref <hop_length>
    normalized: True
    center: True
    pad_mode: \"reflect\"
    power: 2.0
    norm: \"slaney\"
    n_mels: !ref <n_mels>
    mel_scale: \"htk\"

# ──────────────────────────────────────────────────────────────────────
# CRITICAL FIX: Train TinyCryNet_ESP32 DIRECTLY (NOT proposed_model)
# ──────────────────────────────────────────────────────────────────────
embedding_model: !new:model.tiny_crynet_esp32.TinyCryNet_ESP32
    classes_num: !ref <out_n_neurons>
    activation: \"logsoftmax\"
    temporal_type: \"conv1d\"

classifier: !new:torch.nn.Identity

mean_var_norm: !new:speechbrain.processing.features.InputNormalization
    norm_type: sentence
    std_norm: False

modules:
    compute_features: !ref <compute_features>
    embedding_model: !ref <embedding_model>
    classifier: !ref <classifier>
    mean_var_norm: !ref <mean_var_norm>

compute_cost: !name:speechbrain.nnet.losses.nll_loss
compute_error: !name:speechbrain.nnet.losses.classification_error

opt_class: !name:torch.optim.AdamW
    lr: !ref <lr>
    weight_decay: 0.01

lr_annealing: !new:speechbrain.nnet.schedulers.CyclicLRScheduler
    base_lr: !ref <base_lr>
    max_lr: !ref <max_lr>
    step_size: !ref <step_size>

epoch_counter: !new:speechbrain.utils.epoch_loop.EpochCounter
    limit: !ref <number_of_epochs>

train_logger: !new:speechbrain.utils.train_logger.FileTrainLogger
    save_file: !ref <train_log>

error_stats: !name:speechbrain.utils.metric_stats.MetricStats
    metric: !name:speechbrain.nnet.losses.classification_error
        reduction: batch

checkpointer: !new:speechbrain.utils.checkpoints.Checkpointer
    checkpoints_dir: !ref <save_folder>
    recoverables:
        embedding_model: !ref <embedding_model>
        normalizer: !ref <mean_var_norm>
        counter: !ref <epoch_counter>
"""

with open("hparams_fast.yaml", "w") as f:
    f.write(fast_hparams.strip())

print("✅ Configured hparams_fast.yaml successfully!")

# %% [markdown]
# ### STEP 6: Run Fast Training
# All checkpoints write automatically to Google Drive in real-time.

# %%
!python train_test.py hparams_fast.yaml

# %% [markdown]
# ### STEP 7: ONNX Export → TFLite Conversion → ESP32 C-Header Generation

# %%
!pip install -q onnx2tf tensorflow onnx_graphsurgeon sng4onnx flatbuffers

import os
import sys
import glob
import shutil
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.onnx

# Import TinyCryNet_ESP32 from the repo (the SAME architecture we trained)
sys.path.insert(0, REPO_DIR)
from model.tiny_crynet_esp32 import TinyCryNet_ESP32

print("=== STEP 7: Loading Trained Weights & Exporting ===")

tiny_model = TinyCryNet_ESP32(classes_num=2, activation="logsoftmax", temporal_type="conv1d")

search_paths = [
    "/content/drive/MyDrive/TinyCryNet_Project/results/**/save/CKPT*",
    "/content/ICD_MMSP/results/**/save/CKPT*",
    "results/**/save/CKPT*"
]

all_ckpts = []
for p in search_paths:
    all_ckpts.extend(glob.glob(p, recursive=True))
all_ckpts = sorted(list(set(all_ckpts)))

if all_ckpts:
    latest_ckpt_dir = all_ckpts[-1]
    print(f"Found latest checkpoint: {latest_ckpt_dir}")
    ckpt_files = glob.glob(os.path.join(latest_ckpt_dir, "*.ckpt")) + glob.glob(os.path.join(latest_ckpt_dir, "*.pt"))
    target_ckpt = next((f for f in ckpt_files if "embedding" in f.lower() or "model" in f.lower()), ckpt_files[0] if ckpt_files else None)
    if target_ckpt:
        print(f"Loading weights from: {target_ckpt}")
        state = torch.load(target_ckpt, map_location="cpu")
        if isinstance(state, dict):
            state = state.get("model_state_dict", state.get("state_dict", state))
        missing, unexpected = tiny_model.load_state_dict(state, strict=False)
        if missing:
            print(f"⚠️ Missing keys: {missing}")
        if unexpected:
            print(f"⚠️ Unexpected keys: {unexpected}")
        print(f"✅ Loaded weights with {len(missing)} missing, {len(unexpected)} unexpected keys.")
else:
    print("❌ No checkpoints found! Model will have random weights.")

tiny_model.eval()
params = sum(p.numel() for p in tiny_model.parameters())
print(f"Model parameters: {params:,}")

# Step 7a: Export ONNX
dummy_input = torch.randn(1, 1, 81, 128)
local_onnx = "tiny_crynet_2s.onnx"
drive_onnx = "/content/drive/MyDrive/TinyCryNet_Project/tiny_crynet_2s.onnx"

torch.onnx.export(
    tiny_model, dummy_input, local_onnx,
    export_params=True, opset_version=18, do_constant_folding=True,
    input_names=["input"], output_names=["output"],
    dynamic_axes={"input": {0: "batch_size"}, "output": {0: "batch_size"}}
)
shutil.copyfile(local_onnx, drive_onnx)
print(f"✅ ONNX exported: {local_onnx} ({os.path.getsize(local_onnx) / 1024:.1f} KB)")

# Step 7b: Convert ONNX → TFLite
tflite_output = "tiny_crynet_2s.tflite"
drive_tflite = "/content/drive/MyDrive/TinyCryNet_Project/tiny_crynet_2s.tflite"

try:
    import subprocess
    result = subprocess.run([
        "python", "-m", "onnx2tf",
        "-i", local_onnx, "-o", "tflite_export", "-osd",
    ], capture_output=True, text=True, timeout=120)
    print(result.stdout[-500:] if result.stdout else "")
    tflite_candidates = glob.glob("tflite_export/**/*.tflite", recursive=True)
    if tflite_candidates:
        shutil.copyfile(tflite_candidates[0], tflite_output)
        print(f"✅ TFLite model: {tflite_output} ({os.path.getsize(tflite_output) / 1024:.1f} KB)")
    else:
        raise FileNotFoundError("onnx2tf did not produce a .tflite file")
except Exception as e:
    print(f"⚠️ onnx2tf failed ({e}), falling back to tf.lite.TFLiteConverter...")
    import onnx
    from onnx_tf.backend import prepare
    import tensorflow as tf
    onnx_model = onnx.load(local_onnx)
    tf_rep = prepare(onnx_model)
    tf_rep.export_graph("tf_saved_model")
    converter = tf.lite.TFLiteConverter.from_saved_model("tf_saved_model")
    tflite_model = converter.convert()
    with open(tflite_output, "wb") as f:
        f.write(tflite_model)
    print(f"✅ TFLite model (fallback): {tflite_output} ({len(tflite_model) / 1024:.1f} KB)")

if os.path.exists(tflite_output):
    shutil.copyfile(tflite_output, drive_tflite)

# Step 7c: Export TFLite as C header
def export_c_header(bin_path, header_path, name="model_data"):
    with open(bin_path, "rb") as f:
        data = f.read()
    with open(header_path, "w") as f:
        f.write("// Auto-generated ESP32 TFLite Model Array\n")
        f.write("// Architecture: TinyCryNet_ESP32 (conv1d temporal, FP32)\n")
        f.write(f"// Input: [1, 1, 81, 128] | Output: [1, 2]\n")
        f.write(f"// Size: {len(data):,} bytes\n\n")
        f.write("#ifndef MODEL_DATA_H\n#define MODEL_DATA_H\n\n#include <stdint.h>\n\n")
        f.write(f"const unsigned int {name}_len = {len(data)};\n")
        f.write(f"const uint8_t {name}[] __attribute__((aligned(16))) = {{\n  ")
        for i, b in enumerate(data):
            f.write(f"0x{b:02x}, ")
            if (i + 1) % 12 == 0:
                f.write("\n  ")
        f.write("\n};\n\n#endif // MODEL_DATA_H\n")
    print(f"✅ Generated C Header: {header_path} ({len(data):,} bytes)")

local_header = "model_data.h"
drive_header = "/content/drive/MyDrive/TinyCryNet_Project/model_data.h"
if os.path.exists(tflite_output):
    export_c_header(tflite_output, local_header)
    print(f"✅ C Header contains TFLite binary (ready for ESP32-S3 TFLite Micro)")
else:
    export_c_header(local_onnx, local_header)
    print(f"⚠️ C Header contains ONNX binary (manual TFLite conversion needed)")
shutil.copyfile(local_header, drive_header)

print(f"\n📊 Export Summary:")
print(f"   ONNX:   {os.path.getsize(local_onnx) / 1024:.1f} KB")
if os.path.exists(tflite_output):
    print(f"   TFLite: {os.path.getsize(tflite_output) / 1024:.1f} KB")
print(f"   C Hdr:  {os.path.getsize(local_header) / 1024:.1f} KB")
print("\n🎉 Training and export complete! Copy model_data.h to firmware/central_hub_s3/ to flash.")
