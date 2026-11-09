# The long walkway's strip lamps (beside the glass vault): the owner's photo of the lamp's face (Art/WalkLamp/source.jpg)
# made seamless along the lamp's length. Across the lamp the photo stays whole (its frame along both long edges, the
# prismatic diffuser between); along it, a whole number of the diffuser's pyramids (96 px each in the photo) is cut
# from the middle, the photo's brighter centre evened out and the ends cross-faded so it repeats without a seam.
# 2048 x 2048 (power-of-two sides, as Unreal wants for mip maps): one tile is the lamp's whole 30 cm width by 20 cm of
# its length (the material keeps that aspect).
#   python Tools/make_walk_lamp.py  ->  Art/WalkLamp/T_WalkLamp_E.png
import numpy as np
from PIL import Image
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'WalkLamp'
src = np.asarray(Image.open(D / 'source.jpg').convert('RGB')).astype(np.float32)
H, Wsrc = src.shape[:2]
P = 96                                   # the pyramids' pitch in the photo
n = 16                                   # (the middle of the photo only, where the pyramids face the camera)
x0 = (Wsrc - n * P) // 2
# the exact repeat length: where, about 48 pitches on, the diffuser's columns match its first columns best
g = src.mean(2)
ref = g[200:H - 200, x0:x0 + P]
ref = ref - ref.mean()
best, W = -1e18, n * P
for w in range(n * P - 20, n * P + 21):
    c = g[200:H - 200, x0 + w:x0 + w + P]
    s = float(((c - c.mean()) * ref).sum())
    if s > best: best, W = s, w
img = src[:, x0:x0 + W + P].copy()

# even out the photo's lighting (brighter in the middle) in both directions; keep the lamp's profile across it
def blur2(a, s):
    fy = np.fft.fftfreq(a.shape[0])[:, None]; fx = np.fft.fftfreq(a.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(a) * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2))))
L = img[:, :W].mean(2)
lo = blur2(L, 120)
across = lo.mean(1, keepdims=True)
# the frame rows (dark) and the diffuser: only the diffuser is evened out
gain = np.where(across > 90, across / np.maximum(lo, 1), 1.0)
img[:, :W] *= gain[..., None]
img[:, W:] *= gain[:, :P, None]
out = img[:, :W].copy()
t = np.linspace(0, 1, P)[None, :, None]
out[:, :P] = img[:, W:W + P] * (1 - t) + img[:, :P] * t
out = np.clip(out, 0, 255)
Image.fromarray(out.round().astype(np.uint8)).resize((2048, 2048), Image.LANCZOS).save(D / 'T_WalkLamp_E.png')
print('walk lamp:', (W, H), '-> 2048x2048')
