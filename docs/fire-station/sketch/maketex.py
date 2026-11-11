# Design sketch: photo-based textures for the fire station (drawing aid only).
# The fire station's textures: colours and grain measured from the owner's reference photos (ref/), drawn at high
# resolution (the photos are too small to cut tiles from). Warmth matched between the references: one daylight grade.
import numpy as np
from PIL import Image
from pathlib import Path

R = Path(__file__).parent / 'ref'; O = Path(__file__).parent / 'tex'; O.mkdir(exist_ok=True)
rng = np.random.default_rng(3)


def blur(x, s):
    fy = np.fft.fftfreq(x.shape[0])[:, None]; fx = np.fft.fftfreq(x.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(x) * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2)))).astype(np.float32)


def noise(h, w, s): n = blur(rng.standard_normal((h, w)).astype(np.float32), s); return n / (n.std() + 1e-6)


def mean(photo, box): return np.asarray(Image.open(R / photo).convert('RGB').crop(box), np.float32).reshape(-1, 3).mean(0)


def normal_from(h, k):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * k; dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * k
    n = np.dstack([-dx, dy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).round().astype(np.uint8)


def save(name, col, height=None, k=4.0):
    Image.fromarray(np.clip(col, 0, 255).round().astype(np.uint8)).save(O / f'{name}_D.png')
    if height is not None: Image.fromarray(normal_from(height, k)).save(O / f'{name}_N.png')
    print(name, np.clip(col, 0, 255).reshape(-1, 3).mean(0).round(1))


def grade(c, warm=1.0):
    """one daylight grade for all the references: a touch warm, never washed out"""
    c = np.asarray(c, np.float32)
    return c * np.array([1.0 + 0.03 * warm, 1.0, 1.0 - 0.04 * warm])


S = 2048
# 1) board-and-batten fibre-cement cladding (r8): mid grey with a granite-like speckle, raised battens in pairs.
#    One tile = 1.2 m wide, 1.2 m tall: battens at 0.30 m spacing, alternate gaps 0.26/0.34 m (as the photo).
base = grade(mean('r8_facade.jpg', (590, 360, 640, 520)))
spk = noise(S, S, 0.6) * 9 + noise(S, S, 2.0) * 5 + (rng.random((S, S)) > 0.985) * rng.normal(0, 30, (S, S))
x = np.arange(S) / S * 1.2
pos = np.array([0.0, 0.26, 0.60, 0.86])
d = np.min(np.abs(((x[:, None] - pos[None, :]) + 0.6) % 1.2 - 0.6), axis=1)    # distance to the nearest batten (m)
batten = np.clip(1 - d / 0.022, 0, 1) ** 0.5                                      # 44 mm battens
edge = np.exp(-((d - 0.022) / 0.004) ** 2)                                         # their shadowed edge
h = np.repeat((batten * 1.0)[None, :], S, 0) + noise(S, S, 1.0) * 0.02
col = base[None, None, :] * (1 + spk[..., None] / 255 * 2.2) * (1 + 0.05 * np.repeat(batten[None, :], S, 0)[..., None] - 0.12 * np.repeat(edge[None, :], S, 0)[..., None])
col *= (1 + noise(S, S, 120)[..., None] * 0.02)       # faint weathering
save('T_FS_Board', col, h, 6.0)

# 2) the silver metal panels (r1): cool light grey composite panels, 1.0 m wide x 3.0 m tall, 12 mm open joints.
#    One tile = 2 m x 3 m (2 panels).
base = grade(mean('r1_sanjose.jpg', (560, 470, 640, 560)), 0.3)
W2, H2 = 1366, 2048
u = np.arange(W2) / W2 * 2.0; v = np.arange(H2) / H2 * 3.0
ju = np.min(np.abs(((u[:, None] - np.array([0.0, 1.0])[None, :]) + 1.0) % 2.0 - 1.0), axis=1)
jv = np.abs(((v + 1.5) % 3.0) - 1.5)
joint = np.maximum(np.clip(1 - ju / 0.006, 0, 1)[None, :], np.clip(1 - jv / 0.006, 0, 1)[:, None])
oil = np.tile(np.linspace(-1, 1, W2 // 2)[None, :] ** 2, (1, 2))[:, :W2]              # each panel's soft "oil-can" sheen
col = base[None, None, :] * (1 + noise(H2, W2, 3.0)[..., None] * 0.012 - 0.04 * oil[..., None] + noise(H2, W2, 200)[..., None] * 0.015)
col = col * (1 - 0.75 * joint[..., None]) + 25 * joint[..., None]
save('T_FS_Silver', col, -joint * 1.0, 6.0)

# 3) the dark charcoal wing (r1): smooth fair-faced dark concrete / fibre-cement, a soft mottle, panel joints 2.4 m.
base = grade(np.array([93.0, 94.0, 96.0]))   # the wing's charcoal as seen in r1 (the crops catch sky and planting)
u = np.arange(S) / S * 2.4
jj = np.clip(1 - np.abs(((u + 1.2) % 2.4) - 1.2) / 0.005, 0, 1)
col = base[None, None, :] * (1 + noise(S, S, 40)[..., None] * 0.03 + noise(S, S, 4)[..., None] * 0.02 + noise(S, S, 0.7)[..., None] * 0.015)
col = col * (1 - 0.5 * jj[None, :, None])
save('T_FS_Charcoal', col, -np.repeat(jj[None, :], S, 0), 3.0)

# 4) the red sectional doors (r4): signal red, panels 0.55 m tall with their ribs; tile 1 m wide x 2.2 m (4 panels)
base = grade(mean('r4_red_doors.jpg', (330, 600, 480, 700)), 0.5)
W4, H4 = 1024, 2253
v = np.arange(H4) / H4 * 2.2
t = (v % 0.55) / 0.55
prof = np.where(t < 0.04, -1.0, np.where(t < 0.5, 0.0, np.where(t < 0.54, 0.4, 0.0)))     # the joint, a fine rib
col = base[None, None, :] * (1 + 0.08 * prof[:, None, None] + noise(H4, W4, 2)[..., None] * 0.012)
col = col * np.where(prof[:, None, None] < -0.5, 0.55, 1.0)
save('T_FS_RedDoor', col, np.repeat(prof[:, None], W4, 1), 5.0)

# 5) the apron and forecourt concrete (r4): light grey slabs 5 m, sealed joints, faint tyre marks; tile 5 m
base = grade(mean('r4_red_doors.jpg', (150, 880, 600, 960)))
u = np.arange(S) / S * 5.0
jj = np.clip(1 - np.abs(((u + 2.5) % 5.0) - 2.5) / 0.012, 0, 1)
jo = np.maximum(jj[None, :], jj[:, None])
col = base[None, None, :] * (0.97 + noise(S, S, 60)[..., None] * 0.03 + noise(S, S, 1.0)[..., None] * 0.035 + (rng.random((S, S)) > 0.995)[..., None] * -0.08)
col = col * (1 - 0.55 * jo[..., None])
save('T_FS_Apron', col, -jo + noise(S, S, 1.5) * 0.03, 3.0)

# 6) the training tower's light fair-faced concrete (r4): formwork panels 2.5 x 1.25 m with tie holes
base = grade(mean('r4_red_doors.jpg', (1580, 200, 1700, 400)))
u = np.arange(S) / S * 2.5; v = np.arange(S) / S * 2.5
ju = np.clip(1 - np.abs(((u + 1.25) % 2.5) - 1.25) / 0.004, 0, 1); jv = np.clip(1 - np.abs(((v + 0.625) % 1.25) - 0.625) / 0.004, 0, 1)
jo = np.maximum(ju[None, :], jv[:, None])
yy, xx = np.mgrid[0:S, 0:S] / S * 2.5
ties = np.zeros((S, S), np.float32)
for cx in (0.3125, 0.9375, 1.5625, 2.1875):
    for cy in (0.3125, 0.9375, 1.5625, 2.1875):
        ties += np.exp(-(((xx - cx) ** 2 + (yy - cy) ** 2) / 0.00012))
col = base[None, None, :] * (1 + noise(S, S, 25)[..., None] * 0.035 + noise(S, S, 1.2)[..., None] * 0.02) * (1 - 0.3 * jo[..., None]) * (1 - 0.45 * ties[..., None])
save('T_FS_Tower', col, -jo - ties, 3.0)

# 7) the hall floor inside: sealed light concrete, glossy, saw-cut joints 6 m; tile 6 m
base = grade(mean('r5_inside1.jpg', (300, 560, 700, 680)))
u = np.arange(S) / S * 6.0
jj = np.clip(1 - np.abs(((u + 3) % 6) - 3) / 0.006, 0, 1)
jo = np.maximum(jj[None, :], jj[:, None])
col = base[None, None, :] * 0.92 * (1 + noise(S, S, 50)[..., None] * 0.04 + noise(S, S, 0.8)[..., None] * 0.02) * (1 - 0.5 * jo[..., None])
save('T_FS_HallFloor', col, -jo, 2.0)
