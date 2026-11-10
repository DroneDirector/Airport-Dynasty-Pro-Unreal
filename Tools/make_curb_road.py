# The road and its curb outside the terminal: original textures made from scratch here (procedural), matching the
# look the owner showed (a stock preview, used only as a reference for the look, never its pixels).
#  - T_Road_D / T_Road_N: light grey asphalt: a fine dark binder with stone chips of many sizes and shades, a few
#    pale aggregate specks, faint wear patches; seamless both ways (made on a wrapping grid). 2048 x 2048.
#  - T_Curb_D / T_Curb_N: concrete curb stones: exposed-aggregate concrete, a stone joint at each end of the tile, the
#    dark joint line where the road meets the curb along its top edge (that edge is the road side); seamless along
#    its length. 2048 x 512 (one stone, 1 m by 25 cm, the joint line included).
#   python Tools/make_curb_road.py
import numpy as np
from PIL import Image
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'CurbRoad'
D.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(2026)

def blur(x, sy, sx=None):
    sx = sy if sx is None else sx
    fy = np.fft.fftfreq(x.shape[0])[:, None]; fx = np.fft.fftfreq(x.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(x) * np.exp(-2 * np.pi ** 2 * ((sx * fx) ** 2 + (sy * fy) ** 2)))).astype(np.float32)

def noise(h, w, s):
    n = blur(rng.standard_normal((h, w)).astype(np.float32), s)
    return n / (n.std() + 1e-6)

def chips(h, w, count, rmin, rmax):
    """Stone chips: soft-edged blobs of random size, as a height field (0..1), wrapping round the edges."""
    out = np.zeros((h, w), np.float32)
    ys = rng.integers(0, h, count); xs = rng.integers(0, w, count); rs = rng.uniform(rmin, rmax, count)
    for y, x, r in zip(ys, xs, rs):
        R = int(np.ceil(r * 1.6))
        yy, xx = np.mgrid[-R:R + 1, -R:R + 1]
        # a slightly irregular stone, not a perfect disc
        k = np.clip(1.0 - np.sqrt((xx / (r * rng.uniform(0.7, 1.3))) ** 2 + (yy / (r * rng.uniform(0.7, 1.3))) ** 2), 0, 1) ** 0.6
        iy = (np.arange(y - R, y + R + 1) % h)[:, None]; ix = (np.arange(x - R, x + R + 1) % w)[None, :]
        out[iy, ix] = np.maximum(out[iy, ix], k * rng.uniform(0.5, 1.0))
    return out

def normal_map(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-dx, dy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return Image.fromarray(((n * 0.5 + 0.5) * 255).round().astype(np.uint8))

# ---- the asphalt ------------------------------------------------------------------------------------------------
# (as the owner's reference: a mid-light grey, densely textured, the stones barely lighter or darker than the binder,
# only a few pale grains; no cloudy patches)
N = 2048
binder = 0.50 + 0.035 * noise(N, N, 0.9) + 0.008 * noise(N, N, 60)
big = chips(N, N, 30000, 2.0, 5.5)
small = chips(N, N, 110000, 0.8, 2.0)
tone_big = 0.50 + 0.07 * noise(N, N, 2.0)
tone_small = 0.50 + 0.06 * noise(N, N, 1.5)
lum = binder * (1 - big * 0.8) + tone_big * big * 0.8
lum = lum * (1 - small * 0.6) + tone_small * small * 0.6
# the shadows between the stones (the binder sits a little lower): darkest where no stone is
gaps = np.clip(1 - (big + small), 0, 1)
lum = lum - gaps * 0.06
pale = (rng.random((N, N)) > 0.99965).astype(np.float32)
lum = lum + np.clip(blur(pale, 1.0) * 14, 0, 1) * 0.22
pits = (rng.random((N, N)) > 0.998).astype(np.float32)
lum = lum - np.clip(blur(pits, 0.8) * 25, 0, 1) * 0.14
lum = np.clip(lum * 1.08, 0, 1)
rgb = np.dstack([lum * 1.0, lum * 0.995, lum * 1.01]) * 245
Image.fromarray(np.clip(rgb, 0, 255).round().astype(np.uint8)).save(D / 'T_Road_D.png')
normal_map(big * 0.9 + small * 0.5 - gaps * 0.3 - blur(pits, 0.8) * 3, 1.4).save(D / 'T_Road_N.png')

# ---- the curb ---------------------------------------------------------------------------------------------------
H, W = 512, 2048
JOINT_ROWS = 40                                 # the dark joint where the road meets the curb (about 2 cm, clearly seen)
conc = 0.66 + 0.04 * noise(H, W, 0.9) + 0.01 * noise(H, W, 30)
agg = chips(H, W, 22000, 1.5, 5.0)
agg_tone = 0.58 + 0.12 * noise(H, W, 1.6)
lum = conc * (1 - agg * 0.85) + agg_tone * agg * 0.85
# the top edge: a worn darker band, then the joint itself (dark, slightly uneven)
y = np.arange(H)[:, None].astype(np.float32)
edge = np.clip(1 - (y - JOINT_ROWS) / 22.0, 0, 1) * (y >= JOINT_ROWS)
lum = lum * (1 - 0.18 * edge)
wob = (blur(rng.standard_normal((1, W)).astype(np.float32), 0, 30) * 6).astype(np.float32)
joint = (y < JOINT_ROWS + wob).astype(np.float32)
lum = lum * (1 - joint) + (0.09 + 0.03 * noise(H, W, 1.5)) * joint
# a stone joint at each end of the tile (the seam): a dark gap, its edges slightly chipped
x = np.arange(W)[None, :].astype(np.float32)
gap = (np.minimum(x, W - x) < 9 + 3 * noise(H, W, 6)).astype(np.float32)
lum = lum * (1 - gap) + 0.11 * gap
lum = lum * (1 - 0.12 * np.clip((y - JOINT_ROWS) / (H - JOINT_ROWS), 0, 1))   # a touch darker towards the bottom
lum = np.clip(lum * 1.05, 0, 1)
rgb = np.dstack([lum * 1.0, lum * 0.99, lum * 0.975]) * 245
Image.fromarray(np.clip(rgb, 0, 255).round().astype(np.uint8)).save(D / 'T_Curb_D.png')
normal_map(agg * 0.6 - joint * 2.0 - gap * 2.0, 3.0).save(D / 'T_Curb_N.png')
print('road', (N, N), 'curb', (W, H))
