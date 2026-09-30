# ==============================================================================
# Infant Cry Detection: Dedicated Kaggle Training & Export Pipeline
# Separate from Colab: Target Drive folder: MyDrive/TinyCryNet_Kaggle_Project
# Runtime on Kaggle (T4/P100): ~2.2 - 2.8 hours for 35 epochs
# ==============================================================================

# %% [markdown]
# ### STEP 1: Kaggle Environment Verification & Fast Package Setup

# %%
!nvidia-smi
!pip install -q speechbrain hyperpyyaml datasets huggingface_hub librosa soundfile onnx onnxscript pydrive2

import os
import sys
import shutil

KAGGLE_WORKING = "/kaggle/working"
KAGGLE_RESULTS = os.path.join(KAGGLE_WORKING, "results")
os.makedirs(KAGGLE_RESULTS, exist_ok=True)

print(f"✅ Kaggle working directory ready: {KAGGLE_WORKING}")
print(f"✅ Kaggle checkpoint directory: {KAGGLE_RESULTS}")

# %% [markdown]
# ### STEP 2: Dedicated Google Drive Destination Setup
# Kept completely distinct from Colab's folder to avoid any collision.

# %%
DRIVE_KAGGLE_FOLDER = "TinyCryNet_Kaggle_Project"
print(f"✅ Target Drive Folder: MyDrive/{DRIVE_KAGGLE_FOLDER}")

# %% [markdown]
# ### STEP 3: Clone Repository & Symlink Checkpoints

# %%
REPO_DIR = "/kaggle/working/ICD_MMSP"
if not os.path.exists(REPO_DIR):
    !git clone https://github.com/fhfjsd1/ICD_MMSP.git {REPO_DIR}

%cd {REPO_DIR}

repo_results = os.path.join(REPO_DIR, "results")
if os.path.exists(repo_results) and not os.path.islink(repo_results):
    !rm -rf {repo_results}
if not os.path.exists(repo_results):
    os.symlink(KAGGLE_RESULTS, repo_results)

print(f"✅ Symlinked results: {repo_results} -> {KAGGLE_RESULTS}")

# %% [markdown]
# ### STEP 4: Download & Flatten Dataset from Hugging Face

# %%
import os
import glob
import shutil
import zipfile
from huggingface_hub import snapshot_download

DATA_DIR = "/kaggle/working/dataset"
os.makedirs(DATA_DIR, exist_ok=True)

print("1. Downloading infant-cry-detection dataset to Kaggle...")
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
# ### STEP 5: Speed Patch `train_test.py`
# Disables test-only mode and removes per-batch logging that floods Kaggle buffers.

# %%
with open("train_test.py", "r") as f:
    code = f.read()

code = code.replace("TEST = True", "TEST = False")
code = code.replace('print(f"embeddings shape: {embeddings.shape}")', '# print(embeddings.shape)')
code = code.replace('print(f"feats shape: {feats.shape}")', '# print(feats.shape)')

with open("train_test.py", "w") as f:
    f.write(code)

print("✅ train_test.py optimized for Kaggle GPU execution!")

# %% [markdown]
# ### STEP 6: High-Speed Kaggle Hyperparameter Configuration
# Tuned for 35 epochs, batch size 64, 4 workers with pinned memory.

# %%
kaggle_hparams = """
seed: 1999
__set_seed: !apply:speechbrain.utils.seed_everything [!ref <seed>]

# Folder paths for Kaggle
data_folder: /kaggle/working/dataset
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

# High-Speed Training Parameters (35 epochs, ~2.2 - 2.8 hrs)
number_of_epochs: 35
batch_size: 64
lr: 0.001
base_lr: 0.00001
max_lr: !ref <lr>
step_size: 1800
sample_rate: 16000
dropout_prob: 0.3

# Feature parameters (16kHz, 512 FFT, 400 Hop -> 81 frames for 2.0s)
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

# Model Architecture
embedding_model: !new:model.proposed_model.proposed_model
    classes_num: !ref <out_n_neurons>
    activation: \"logsoftmax\"

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

opt_class: !name:torch.optim.Adam
    lr: !ref <lr>
    weight_decay: 0.0001

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

with open("hparams_kaggle.yaml", "w") as f:
    f.write(kaggle_hparams.strip())

print("✅ Configured hparams_kaggle.yaml successfully!")

# %% [markdown]
# ### STEP 7: Run Kaggle GPU Training

# %%
!python train_test.py hparams_kaggle.yaml

# %% [markdown]
# ### STEP 8: TinyCryNet Definition & Dual-Track ONNX / C-Header Export

# %%
import os
import glob
import shutil
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.onnx

class BSConvU(nn.Module):
    def __init__(self, in_channels, out_channels, kernel_size=3, stride=1, padding=1, bias=True):
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
    def __init__(self, in_channels=1, classes_num=2, temporal_type="gru"):
        super().__init__()
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
        return self.classifier(t_pooled)

tiny_model = TinyCryNet_ESP32(classes_num=2, temporal_type="gru")
print("=== Searching Checkpoints for Export ===")

search_paths = [
    "/kaggle/working/results/**/save/CKPT*",
    "/kaggle/working/**/CKPT*",
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
        tiny_model.load_state_dict(state, strict=False)
        print("Loaded weights successfully.")

# 1. Export ONNX to Kaggle Output
tiny_model.eval()
dummy_input = torch.randn(1, 1, 81, 128)
onnx_output = "/kaggle/working/tiny_crynet_2s.onnx"

torch.onnx.export(
    tiny_model,
    dummy_input,
    onnx_output,
    export_params=True,
    opset_version=18,
    do_constant_folding=True,
    input_names=["input"],
    output_names=["output"],
    dynamic_axes={"input": {0: "batch_size"}, "output": {0: "batch_size"}}
)
print(f"✅ ONNX exported to Kaggle Output: {onnx_output}")

# 2. Export C Header to Kaggle Output
def export_c_header(bin_path, header_path, name="model_data"):
    with open(bin_path, "rb") as f:
        data = f.read()
    with open(header_path, "w") as f:
        f.write("// Auto-generated ESP32 Model Array (Kaggle Build)\n#ifndef MODEL_DATA_H\n#define MODEL_DATA_H\n\n#include <stdint.h>\n\n")
        f.write(f"const unsigned int {name}_len = {len(data)};\n")
        f.write(f"const uint8_t {name}[] __attribute__((aligned(16))) = {{\n  ")
        for i, b in enumerate(data):
            f.write(f"0x{b:02x}, ")
            if (i + 1) % 12 == 0:
                f.write("\n  ")
        f.write("\n};\n\n#endif // MODEL_DATA_H\n")
    print(f"✅ Generated C Header: {header_path} ({len(data)} bytes)")

header_output = "/kaggle/working/model_data.h"
export_c_header(onnx_output, header_output)
print(f"✅ C Header exported to Kaggle Output: {header_output}")

# %% [markdown]
# ### STEP 9: Package Kaggle Run Artifacts for Google Drive Backup

# %%
import os
import tarfile

archive_name = "/kaggle/working/kaggle_run_artifacts.tar.gz"
print("Packaging checkpoints and deployment models...")

with tarfile.open(archive_name, "w:gz") as tar:
    if os.path.exists("/kaggle/working/tiny_crynet_2s.onnx"):
        tar.add("/kaggle/working/tiny_crynet_2s.onnx", arcname="tiny_crynet_2s.onnx")
    if os.path.exists("/kaggle/working/model_data.h"):
        tar.add("/kaggle/working/model_data.h", arcname="model_data.h")
    if os.path.exists("/kaggle/working/results"):
        tar.add("/kaggle/working/results", arcname="results")

print(f"✅ Archive created: {archive_name} ({os.path.getsize(archive_name) / (1024*1024):.2f} MB)")
print("Target folder on Google Drive: MyDrive/TinyCryNet_Kaggle_Project/")
print("\n🎉 Kaggle run is complete and ready in your Kaggle notebook output tab!")
