import onnxruntime as ort
import numpy as np

session = ort.InferenceSession('model_artifacts/tiny_crynet_2s.onnx')

# Let's test with random inputs of different scales to see if the output changes at all!
for scale in [0.001, 1.0, 10.0, 100.0]:
    inp = np.random.randn(1, 1, 81, 128).astype(np.float32) * scale
    out = session.run(None, {'input': inp})[0]
    print(f"Scale {scale}: Output = {out}")
