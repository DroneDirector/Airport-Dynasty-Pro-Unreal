# The hall's tree columns' stone (the owner's photo, Art/PillarStone/source.jpg) made seamless: the photo blended with
# itself shifted by half (each taken where the other has its edge), so every edge meets its opposite edge; the
# photo's light patches evened out. Plus a faint normal map from the stone's veins and pits.
#   python Tools/make_pillar_stone.py  ->  Art/PillarStone/T_PillarStone_D.png (2048 x 4096), T_PillarStone_N.png
import numpy as np
from PIL import Image
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'PillarStone'
W, H = 2048, 4096          # high resolution (the photo is small: fine grain is added at full size, below)
a = np.asarray(Image.open(D / 'source.jpg').convert('RGB').resize((W, H), Image.LANCZOS), np.float32)

def blur(x, s):
    fy = np.fft.fftfreq(x.shape[0])[:, None]; fx = np.fft.fftfreq(x.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(x) * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2)))).astype(np.float32)

# the photo's JPEG blocks smoothed away first (they would show enlarged five times)
def blur3(x, s): return np.dstack([blur(x[..., c], s) for c in range(3)])
a = blur3(a, 3.0)
b = np.roll(a, (H // 2, W // 2), axis=(0, 1))
# weight: 1 in the middle of the photo, 0 at its edges (where the shifted copy has its middle)
wy = 1 - np.abs(np.linspace(-1, 1, H))[:, None]; wx = 1 - np.abs(np.linspace(-1, 1, W))[None, :]
w = np.clip(np.minimum(wy, wx) * 3.0, 0, 1)[..., None]
out = a * w + b * (1 - w)
L = out.mean(2)
lo = blur(L, 160)
out = np.clip(out * ((lo.mean() / np.maximum(lo, 1)) ** 0.8)[..., None], 0, 255)
# the stone's own fine texture at full resolution: a fine grain, a coarser mottling, scattered tiny pits; seamless
rng = np.random.default_rng(11)
grain = blur(rng.standard_normal((H, W)).astype(np.float32), 0.7)
mott = blur(rng.standard_normal((H, W)).astype(np.float32), 6.0)
pits = blur((rng.random((H, W)) > 0.9996).astype(np.float32), 0.9) * 40.0
detail = grain / (grain.std() + 1e-6) * 1.8 + mott / (mott.std() + 1e-6) * 1.8 - np.clip(pits, 0, 1) * 10.0
out = np.clip(out + detail[..., None], 0, 255)
Image.fromarray(out.round().astype(np.uint8)).save(D / 'T_PillarStone_D.png')
g = out.mean(2) / 255.0
h = g - blur(g, 6)
dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 5.0; dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 5.0
n = np.dstack([-dx, dy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
Image.fromarray(((n * 0.5 + 0.5) * 255).round().astype(np.uint8)).save(D / 'T_PillarStone_N.png')
print('pillar stone maps', (W, H))
