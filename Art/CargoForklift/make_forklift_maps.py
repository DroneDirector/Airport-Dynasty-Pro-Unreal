# The cargo forklift's texture set from the owner's Meshy colour picture (system Python: PIL + numpy).
#   python make_forklift_maps.py <meshy colour.png> <out dir>
# Writes: forklift_base.png (brand decals painted out, Art/CargoForklift/paint_out_decals.py), forklift_normal.png (fine
# grain from the picture's own light/dark detail), and one metal/rough picture per material (glTF: G rough, B metal):
#   mr_body   painted steel (yellow and black paint), chrome/stainless where the picture is bare light grey
#   mr_tyre   rubber where dark, painted steel rims where light grey
#   mr_steel  worn fork/mast steel where dark (bright scratches shinier), chrome rods where light grey
#   mr_plastic black plastic and vinyl (seat, steering wheel, lever knobs)
import os, subprocess, sys
import numpy as np
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
base = os.path.join(out, 'forklift_base.png')
subprocess.run([sys.executable, os.path.join(here, 'paint_out_decals.py'), src, base], check=True)
c = np.asarray(Image.open(base).convert('RGB')).astype(np.float32) / 255.0


def blur(a, r):
    q = np.pad(a, r + 1, mode='edge').cumsum(0).cumsum(1); k = 2 * r + 1
    return (q[k:, k:] - q[:-k, k:] - q[k:, :-k] + q[:-k, :-k])[:a.shape[0], :a.shape[1]] / (k * k)


lum = c @ np.array([0.2126, 0.7152, 0.0722], np.float32)
mx, mn = c.max(-1), c.min(-1)
sat = (mx - mn) / np.maximum(mx, 1e-3)
yellow = (c[..., 0] > 0.35) & (c[..., 1] > 0.25) & (c[..., 2] < 0.6 * c[..., 0])
grey = ~yellow & (sat < 0.18) & (lum > 0.33)
dark = ~yellow & ~grey
# clean paint: baked shadows, smears and blotches go. In each class (yellow paint, light grey metal, black) the colour is
# the class's own median, times the picture's fine detail (pixel / its class's local mean) squeezed to +-6 %: wear and
# edges stay faintly, light and shadow painted in by the generator do not. Stickers (red, white, blue) stay as they are.
sticker = (sat > 0.45) & ~yellow | (lum > 0.72) & (sat < 0.12)
orig_lum = lum.copy()
for mask in (yellow & ~sticker, grey & ~sticker, dark & ~sticker):
    w = mask.astype(np.float32)
    loc = np.stack([blur(c[..., k] * w, 10) for k in range(3)], -1) / (blur(w, 10)[..., None] + 1e-4)
    det = np.clip(lum / np.maximum(loc @ np.array([0.2126, 0.7152, 0.0722], np.float32), 1e-3), 0.0, 3.0)
    det = 1.0 + np.clip(det - 1.0, -0.06, 0.06)
    med = np.median(c[mask][::17], 0)
    c[mask] = (med[None, :] * det[mask][:, None])
c = np.clip(c, 0, 1)
Image.fromarray((c * 255 + 0.5).astype(np.uint8)).save(base)
lum = c @ np.array([0.2126, 0.7152, 0.0722], np.float32)
grime = np.clip((blur(orig_lum, 3) - orig_lum) * 0.8, -0.05, 0.05)       # dirt in the dents a little rougher
rng = np.random.default_rng(3)
fine = rng.normal(0, 0.012, lum.shape).astype(np.float32)        # paint grain


def mr(name, table):
    r = np.zeros_like(lum); m = np.zeros_like(lum)
    for mask, (rough, metal) in zip((yellow, grey, dark), table):
        rr = rough(lum) if callable(rough) else np.full_like(lum, rough)
        r[mask] = rr[mask]; m[mask] = metal
    r = np.clip(r + grime + fine, 0.08, 1.0)
    img = np.stack([np.ones_like(r), r, m], -1)
    Image.fromarray((img * 255).astype(np.uint8)).resize((1024, 1024), Image.LANCZOS).save(os.path.join(out, 'forklift_mr_%s.png' % name))


#            yellow paint     light grey                    dark
mr('body',    [(0.42, 0.0), (0.26, 0.85),                 (0.58, 0.0)])
mr('tyre',    [(0.45, 0.0), (0.40, 0.30),                 (0.90, 0.0)])
mr('steel',   [(0.45, 0.0), (0.16, 0.95),                 (lambda l: 0.62 - l * 0.8, 0.70)])
mr('plastic', [(0.45, 0.0), (0.30, 0.80),                 (0.52, 0.0)])

# normal map: height = the picture's fine light/dark detail (high-pass), slopes to a tangent-space normal
h = orig_lum - blur(orig_lum, 6)
gx = np.zeros_like(h); gy = np.zeros_like(h)
gx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) * 0.5; gy[1:-1] = (h[2:] - h[:-2]) * 0.5
k = 0.9
n = np.stack([-gx * k, gy * k, np.ones_like(h)], -1)
n /= np.linalg.norm(n, axis=-1, keepdims=True)
Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8)).save(os.path.join(out, 'forklift_normal.png'))
print('maps: yellow %.0f%% grey %.0f%% dark %.0f%%' % (yellow.mean() * 100, grey.mean() * 100, dark.mean() * 100))
