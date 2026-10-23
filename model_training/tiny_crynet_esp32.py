import torch
import torch.nn as nn
import torch.nn.functional as F

class BSConvU(nn.Module):
    """
    Blueprint Separable Convolution (Pointwise -> Depthwise)
    Reduces parameter and FLOP complexity significantly compared to standard conv.
    """
    def __init__(self, in_channels, out_channels, kernel_size=3, stride=1, padding=1, bias=True):
        super().__init__()
        self.pw = nn.Conv2d(in_channels, out_channels, kernel_size=1, stride=1, padding=0, bias=False)
        self.dw = nn.Conv2d(out_channels, out_channels, kernel_size=kernel_size, stride=stride, 
                            padding=padding, groups=out_channels, bias=bias)

    def forward(self, x):
        return self.dw(self.pw(x))

class CCALayer(nn.Module):
    """
    Contrast-aware Channel Attention:
    Combines standard deviation (contrast) and global average pooling to emphasize harmonic vocal structures.
    """
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
    """
    Optimized TinyCryNet for ESP32-S3 (2.0-second window, 16kHz, 128 Mel bands).
    Features:
    - Multi-scale BSConv feature hierarchy
    - Contrast-Aware Channel Attention (CCA)
    - Lightweight GRU/Conv temporal aggregator (avoids dual 2D LSTMs for fast INT8 hardware acceleration)
    - Fully exportable to ONNX, TFLite (INT8), and ESP-DL C-arrays.
    """
    def __init__(self, in_channels=1, classes_num=2, temporal_type="gru"):
        super().__init__()
        self.temporal_type = temporal_type
        
        # Scale 1: 128 mel x 80 frames -> 64 mel x 40 frames
        self.block1 = CompactConvBlock(in_channels, 32)
        # Scale 2: 64 mel x 40 frames -> 32 mel x 20 frames
        self.block2 = CompactConvBlock(32, 64)
        # Scale 3: 32 mel x 20 frames -> 16 mel x 10 frames
        self.block3 = CompactConvBlock(64, 128)
        # Scale 4 & 5: Feature extraction without downsampling
        self.block4 = CompactConvBlock(128, 128)
        self.block5 = CompactConvBlock(128, 128)
        
        # Multi-scale fusion
        self.fuse_conv = nn.Conv2d(128 * 3, 128, kernel_size=1)
        self.cca = CCALayer(128)
        
        # Temporal Modeling (lightweight GRU or 1D Conv)
        if temporal_type == "gru":
            # Across time steps after frequency pooling
            self.temporal = nn.GRU(input_size=128, hidden_size=64, num_layers=1, batch_first=True, bidirectional=True)
            fc_in = 128
        else:
            self.temporal = nn.Sequential(
                nn.Conv1d(128, 64, kernel_size=3, padding=1),
                nn.ReLU(),
                nn.AdaptiveAvgPool1d(1)
            )
            fc_in = 64

        self.classifier = nn.Sequential(
            nn.Linear(fc_in, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, classes_num)
        )

    def forward(self, x):
        # x: (B, 1, Time, Freq) or (B, Time, Freq)
        if x.dim() == 3:
            x = x.unsqueeze(1)
            
        x1 = self.block1(x, pool_size=(2, 2))
        x2 = self.block2(x1, pool_size=(2, 2))
        x3 = self.block3(x2, pool_size=(2, 2))
        x4 = self.block4(x3, pool_size=None)
        x5 = self.block5(x4, pool_size=None)
        
        # Multi-scale skip concat
        fused = torch.cat([x3, x4, x5], dim=1)
        fused = self.fuse_conv(fused)
        fused = self.cca(fused)
        
        # Pool frequency dimension: (B, C, Time, Freq) -> (B, C, Time)
        f_pooled = torch.mean(fused, dim=3)
        
        if self.temporal_type == "gru":
            # (B, C, Time) -> (B, Time, C)
            seq = f_pooled.permute(0, 2, 1)
            gru_out, _ = self.temporal(seq)
            # Pool time
            t_pooled = torch.mean(gru_out, dim=1)
        else:
            t_pooled = self.temporal(f_pooled).squeeze(-1)
            
        out = self.classifier(t_pooled)
        return out

if __name__ == "__main__":
    # Test shape for 2-second audio (16kHz, hop 400 -> 81 frames, 128 mel bins)
    # Frame count: ceil(32000 / 400) + 1 = 81 with center-padded STFT
    sample_input = torch.randn(2, 1, 81, 128)

    # 1. Test GRU variant (higher accuracy, requires ONNX runtime)
    model_gru = TinyCryNet_ESP32(temporal_type="gru")
    output_gru = model_gru(sample_input)
    params_gru = sum(p.numel() for p in model_gru.parameters() if p.requires_grad)
    print(f"[GRU Variant]  Input: {sample_input.shape} -> Output: {output_gru.shape} | Params: {params_gru:,}")

    # 2. Test Conv1D variant (TFLite Micro compatible, recommended for ESP32-S3)
    model_conv = TinyCryNet_ESP32(temporal_type="conv1d")
    output_conv = model_conv(sample_input)
    params_conv = sum(p.numel() for p in model_conv.parameters() if p.requires_grad)
    print(f"[Conv1D Variant] Input: {sample_input.shape} -> Output: {output_conv.shape} | Params: {params_conv:,}")

    print(f"\nRecommendation: Use 'conv1d' for ESP32-S3 TFLite deployment (fully supported ops).")
    print(f"                Use 'gru' for desktop ONNX inference only.")
