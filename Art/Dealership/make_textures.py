# Seamless (tileable), high-resolution textures for the dealership, coloured from the owner's reference photos:
# gravel beds (pebbles, palette taken from the photo), lawn, asphalt, concrete, beige cladding.
# Every texture is made on a torus (wraps at the edges), so it tiles without seams at any scale.
import numpy as np
from PIL import Image, ImageFilter
rng = np.random.default_rng(7)
U = '/mnt/user-data/uploads/'; T = '/home/claude/bld/tex/'
def save(a, n): Image.fromarray(np.clip(a * 255, 0, 255).astype(np.uint8)).save(T + n)

def tnoise(N, cells, octaves=5, seed=0):
    """Tileable value noise (bilinear on a wrapping lattice), summed over octaves, 0..1."""
    r = np.random.default_rng(seed); out = np.zeros((N, N)); amp, tot = 1.0, 0.0
    for o in range(octaves):
        c = cells * 2 ** o; g = r.random((c, c))
        x = np.arange(N) * c / N; i = x.astype(int); f = x - i; f = f * f * (3 - 2 * f)
        i1 = (i + 1) % c
        a = g[i][:, i] * (1 - f)[None, :] + g[i][:, i1] * f[None, :]
        b = g[i1][:, i] * (1 - f)[None, :] + g[i1][:, i1] * f[None, :]
        out += amp * (a * (1 - f)[:, None] + b * f[:, None]); tot += amp; amp *= 0.5
    return out / tot

# palette: pebble colours from the gravel bed in the photos (shrubs and lawn left out)
ph = np.asarray(Image.open(U + 'c4cc0289-image.jpg').convert('RGB').crop((0, 1790, 300, 1990)), float) / 255
px = ph.reshape(-1, 3); px = px[(px[:, 1] < px[:, 0] + 0.05) & (px.sum(1) > 0.3)]  # not green, not shadow
pal = px[rng.integers(0, len(px), 400)]

N = 2048
# 1) gravel: pebbles as wrapped Voronoi cells, each tinted from the photo palette, rounded height
pts = rng.random((2600, 2)) * N
yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
best = np.full((N, N), 1e9, np.float32); second = np.full((N, N), 1e9, np.float32); idx = np.zeros((N, N), np.int32)
for k, (px_, py_) in enumerate(pts):
    x0, y0 = int(px_) - 70, int(py_) - 70
    xs = (np.arange(x0, x0 + 140) % N); ys = (np.arange(y0, y0 + 140) % N)
    dx = np.minimum(np.abs(xs - px_), N - np.abs(xs - px_)); dy = np.minimum(np.abs(ys - py_), N - np.abs(ys - py_))
    d = np.sqrt(dy[:, None] ** 2 + dx[None, :] ** 2).astype(np.float32)
    sub_b = best[np.ix_(ys, xs)]; sub_s = second[np.ix_(ys, xs)]; sub_i = idx[np.ix_(ys, xs)]
    closer = d < sub_b
    sub_s = np.where(closer, sub_b, np.minimum(sub_s, d)); sub_i = np.where(closer, k, sub_i); sub_b = np.where(closer, d, sub_b)
    best[np.ix_(ys, xs)] = sub_b; second[np.ix_(ys, xs)] = sub_s; idx[np.ix_(ys, xs)] = sub_i
edge = np.clip((second - best) / 9.0, 0, 1)             # 0 at the gaps between pebbles
h = np.sqrt(edge)                                      # rounded tops
cols = pal[rng.integers(0, len(pal), len(pts))] * (0.85 + 0.3 * rng.random((len(pts), 1)))
col = cols[idx] * (0.55 + 0.45 * h[..., None])
col = col * (0.9 + 0.2 * tnoise(N, 64, 3, 1)[..., None])
col[edge < 0.08] *= 0.35                                # dark gaps
save(col, 'gravel_col.png'); save(np.repeat(h[..., None], 3, 2), 'gravel_h.png')

# 2) lawn: blades as fine streaky noise, colours from the lawn in the photos
lw = np.asarray(Image.open(U + 'f00a1783-image.jpg').convert('RGB').crop((0, 1790, 180, 1880)), float) / 255
g0 = lw.reshape(-1, 3).mean(0)
n1 = tnoise(N, 256, 3, 2); n2 = tnoise(N, 16, 4, 3); n3 = tnoise(N, 512, 2, 4)
lawn = g0[None, None, :] * (0.55 + 0.6 * n1[..., None]) * (0.8 + 0.4 * n2[..., None])
lawn[..., 0] += 0.06 * (n3 - 0.5); lawn[..., 1] += 0.05 * (n2 - 0.5)
save(lawn, 'lawn_col.png'); save(np.repeat(n1[..., None], 3, 2), 'lawn_h.png')

# 3) asphalt: dark binder with light aggregate specks, oil stains at a large scale
n = tnoise(N, 128, 4, 5); sp = rng.random((N, N)) > 0.985; big = tnoise(N, 4, 3, 6)
a = 0.10 + 0.05 * n + 0.18 * sp * rng.random((N, N)); a *= 0.85 + 0.3 * big
asph = np.stack([a, a * 0.99, a * 1.02], -1)
save(asph, 'asphalt_col.png'); save(np.repeat((0.6 * n + 0.4 * sp)[..., None], 3, 2), 'asphalt_h.png')

# 4) concrete (walk, kerbs, plinth): light grey, fine pores, soft clouding
n = tnoise(N, 64, 5, 7); pores = (rng.random((N, N)) > 0.995) * 0.25
c = 0.72 + 0.1 * (n - 0.5) - pores; conc = np.stack([c, c * 0.995, c * 0.97], -1)
save(conc, 'concrete_col.png'); save(np.repeat((n - pores)[..., None], 3, 2), 'concrete_h.png')

# 5) beige cladding panels: the colour of the sand panels in the photos, soft variation
bg = np.asarray(Image.open(U + 'e75d7c3b-image.jpg').convert('RGB').crop((140, 1200, 330, 1500)), float) / 255
b0 = bg.reshape(-1, 3).mean(0); n = tnoise(N, 32, 5, 8)
save(b0[None, None, :] * (0.93 + 0.14 * n[..., None]), 'beige_col.png')

# 6) fascia colour (the bronze-brown of the wavy panels) and the navy variant, for the material's base colour
fz = np.asarray(Image.open(U + '1ac281b2-image.jpg').convert('RGB').crop((150, 800, 950, 1150)), float) / 255
nv = np.asarray(Image.open(U + '80eeab15-image.jpg').convert('RGB').crop((100, 500, 800, 900)), float) / 255
np.save(T + 'fascia_rgb.npy', np.array([np.median(fz.reshape(-1, 3), 0), np.median(nv.reshape(-1, 3), 0), b0, g0]))
print('fascia', np.median(fz.reshape(-1, 3), 0), 'navy', np.median(nv.reshape(-1, 3), 0), 'beige', b0, 'lawn', g0)
