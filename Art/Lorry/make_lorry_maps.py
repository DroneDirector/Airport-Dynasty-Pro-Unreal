# The lorry's texture set from the owner's Meshy colour picture (system Python: PIL + numpy), the forklift's recipe
# (Art/CargoForklift/make_forklift_maps.py):   python make_lorry_maps.py <meshy colour.png> <out dir>
# Writes lorry_base.png (cleaned colour), lorry_normal.png (fine grain), one metal/rough picture per material (G rough, B metal):
#   mr_cab      blue cab paint, chrome/bright metal where light grey, black plastic and steel where dark
#   mr_trailer  white painted aluminium side panels, galvanised steel where grey, black frame where dark
#   mr_tyre     rubber where dark, painted steel / alloy rims where light
# Cleaning: every class (blue paint, white/grey, dark) takes its own median colour times the picture's fine detail squeezed
# to +-6 % (baked light, smears and blotches go). Then every small patch of one class lying inside another (lettering,
# badges, stickers, made-up logos) takes the surrounding class: in a 21 px window a pixel whose class holds under 25 % of
# the window becomes the window's main class. No brand, badge or text survives; big areas and their edges stay.
import os, sys
import numpy as np
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
c = np.asarray(Image.open(src).convert('RGB')).astype(np.float32) / 255.0


def blur(a, r):
    q = np.pad(a, r + 1, mode='edge').cumsum(0).cumsum(1); k = 2 * r + 1
    return (q[k:, k:] - q[:-k, k:] - q[k:, :-k] + q[:-k, :-k])[:a.shape[0], :a.shape[1]] / (k * k)


L3 = np.array([0.2126, 0.7152, 0.0722], np.float32)
lum = c @ L3
mx, mn = c.max(-1), c.min(-1)
sat = (mx - mn) / np.maximum(mx, 1e-3)
blue = (c[..., 2] > c[..., 0] * 1.4) & (sat > 0.3) & (lum > 0.06)
light = ~blue & (lum > 0.42)
dark = ~blue & ~light
orig_lum = lum.copy()
classes = [blue, light, dark]
# small islands of one class inside another (letters, badges, stickers) -> the window's main class
share = [blur(m.astype(np.float32), 10) for m in classes]
main = np.argmax(np.stack(share, -1), -1)
own = np.select(classes, share, 1.0)
swap = own < 0.25
for k in range(3):
    m = swap & (main == k)
    for j in range(3):
        classes[j] = classes[j] & ~m
    classes[k] = classes[k] | m
print('maps: small patches repainted %.2f%% of the picture' % (swap.mean() * 100))
for mask in classes:
    if mask.sum() < 100: continue
    w = mask.astype(np.float32)
    loc = np.stack([blur(c[..., k] * w, 10) for k in range(3)], -1) / (blur(w, 10)[..., None] + 1e-4)
    det = np.clip(lum / np.maximum(loc @ L3, 1e-3), 0.0, 3.0)
    det = 1.0 + np.clip(det - 1.0, -0.06, 0.06)
    med = np.median(c[mask & ~swap][::17], 0)
    c[mask] = med[None, :] * det[mask][:, None]
c = np.clip(c, 0, 1)
Image.fromarray((c * 255 + 0.5).astype(np.uint8)).save(os.path.join(out, 'lorry_base.png'))
blue, light, dark = classes
grime = np.clip((blur(orig_lum, 3) - orig_lum) * 0.8, -0.05, 0.05)
fine = np.random.default_rng(3).normal(0, 0.012, lum.shape).astype(np.float32)


def mr(name, table):
    r = np.zeros_like(lum); m = np.zeros_like(lum)
    for mask, (rough, metal) in zip((blue, light, dark), table):
        r[mask] = rough; m[mask] = metal
    r = np.clip(r + grime + fine, 0.06, 1.0)
    Image.fromarray((np.stack([np.ones_like(r), r, m], -1) * 255).astype(np.uint8)).resize((1024, 1024), Image.LANCZOS) \
        .save(os.path.join(out, 'lorry_mr_%s.png' % name))


#              blue paint     light            dark
mr('cab',     [(0.28, 0.0), (0.18, 0.90), (0.55, 0.15)])
mr('trailer', [(0.30, 0.0), (0.40, 0.0),  (0.55, 0.30)])
mr('tyre',    [(0.35, 0.0), (0.32, 0.85), (0.88, 0.0)])

h = orig_lum - blur(orig_lum, 6)
gx = np.zeros_like(h); gy = np.zeros_like(h)
gx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) * 0.5; gy[1:-1] = (h[2:] - h[:-2]) * 0.5
n = np.stack([-gx * 0.9, gy * 0.9, np.ones_like(h)], -1); n /= np.linalg.norm(n, axis=-1, keepdims=True)
Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8)).save(os.path.join(out, 'lorry_normal.png'))
print('maps: blue %.0f%% light %.0f%% dark %.0f%%' % (blue.mean() * 100, light.mean() * 100, dark.mean() * 100))
