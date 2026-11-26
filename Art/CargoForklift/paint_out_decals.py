# Paints the made-up brand decals ("CARRGO MAX" and its sub-line) out of the Meshy colour texture: inside each box the
# minority colour class (letters) takes the blurred colour of the majority class (the band or the yellow paint) round it.
# Capacity plates ("3.5 TONNE") and warning stickers stay.  python paint_out_decals.py <src.png> <out.png>
import sys, numpy as np
from PIL import Image, ImageFilter
OX, OY = 640, 1120
BOXES = [(40, 66, 268, 98), (36, 42, 236, 66), (98, 142, 134, 342), (128, 176, 162, 366)]   # (crop-relative x0 y0 x1 y1)
im = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(np.float32)
for x0, y0, x1, y1 in BOXES:
    x0 += OX; x1 += OX; y0 += OY; y1 += OY
    p = im[y0:y1, x0:x1]
    yel = (p[..., 0] > 120) & (p[..., 1] > 80) & (p[..., 2] < 0.6 * p[..., 0])
    maj = yel if yel.mean() > 0.5 else ~yel
    # majority colour field: average of majority pixels over a wide window (normalised blur), keeps the grime gradient
    w = maj.astype(np.float32)
    def blur(a, r=9):    # box blur by summed areas (edge-clamped)
        q = np.pad(a, r + 1, mode='edge').cumsum(0).cumsum(1); k = 2 * r + 1
        return (q[k:, k:] - q[:-k, k:] - q[k:, :-k] + q[:-k, :-k])[:a.shape[0], :a.shape[1]] / (k * k)
    num = np.stack([blur(p[..., c] * w) for c in range(3)], -1); den = blur(w)[..., None] + 1e-4
    fill = num / den
    noise = np.random.default_rng(7).normal(0, 4, p.shape)
    grow = np.asarray(Image.fromarray((~maj * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
    p[grow] = np.clip(fill[grow] + noise[grow], 0, 255)
Image.fromarray(im.astype(np.uint8)).save(sys.argv[2])
print('decals painted out:', len(BOXES), 'boxes')
