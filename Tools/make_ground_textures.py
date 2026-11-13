# The ground's own pictures (owner, 2026-11-13: photorealistic, from real photos - Art/Ground/_references): drawn here
# from scratch (no photos used as textures), tiling, for the master ground material (Tools/set_ground.py).
#   asphalt_*:   aged airport asphalt, 4 x 4 m: mid-grey (not black: weathered), light angular stones of 2-14 mm in a
#                dark binder, mottled from wear and repairs
#   concrete_*:  apron concrete, 4 x 4 m: light grey, fine sand-and-stone grain, the broom's fine lines, faint stains
#   cracks_asphalt: 16 x 16 m layer (R: sealant overbands - 3-4 cm black tar along long, gently wandering cracks, some
#                crossing; G: fine open cracks and branches; B: darker patched or stained areas)
#   cracks_concrete: 16 x 16 m layer (G: hairline cracks, corner breaks at slab joints; B: stains, tyre smudges)
#   tyre_rubber:  the runway's rubber build-up, 64 m along x 32 m across: long thin dark streaks along the runway,
#                denser in the middle, faint at the sides (the material places it in the touchdown zones)
#   python Tools/make_ground_textures.py   ->  Art/Ground/*.png
import math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parent.parent / 'Art' / 'Ground'
OUT.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(62)

def tile_noise(n, cells, octaves=5, seed=0):
    """Tiling value noise (wraps), 0..1."""
    r = np.random.default_rng(seed); out = np.zeros((n, n)); amp = 1.0; tot = 0
    for o in range(octaves):
        c = cells * (2 ** o)
        g = r.random((c, c))
        x = np.linspace(0, c, n, endpoint=False); i0 = np.floor(x).astype(int); f = x - i0; f = f * f * (3 - 2 * f)
        i1 = (i0 + 1) % c
        a = g[np.ix_(i0, i0)]; b = g[np.ix_(i0, i1)]; cc = g[np.ix_(i1, i0)]; d = g[np.ix_(i1, i1)]
        fx = f[None, :]; fy = f[:, None]
        out += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (cc * (1 - fx) + d * fx) * fy); tot += amp; amp *= 0.5
    return out / tot

def save(name, arr, mode='L'):
    a = np.clip(arr, 0, 1)
    if mode == 'L': Image.fromarray((a * 255).astype(np.uint8), 'L').save(OUT / f'{name}.png')
    else: Image.fromarray((a * 255).astype(np.uint8), 'RGB').save(OUT / f'{name}.png')
    print('made', name)

def normal_from_height(h, strength):
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-gx, -gy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5

def stones(n, metres, count, rmin_mm, rmax_mm, seed):
    """Angular stones (polygons), wrapped: a mask 0..1 and a per-stone tone."""
    r = np.random.default_rng(seed); px = n / (metres * 1000.0)
    m = Image.new('L', (n, n), 0); t = Image.new('L', (n, n), 0)
    dm, dt = ImageDraw.Draw(m), ImageDraw.Draw(t)
    for _ in range(count):
        cx, cy = r.random() * n, r.random() * n
        rad = (rmin_mm + (rmax_mm - rmin_mm) * r.random() ** 2.2) * px
        k = r.integers(5, 9); ang = np.sort(r.random(k)) * 2 * math.pi
        rr = rad * (0.6 + 0.4 * r.random(k))
        tone = int(120 + 135 * r.random())
        for ox in (-n, 0, n):
            for oy in (-n, 0, n):
                pts = [(cx + ox + rr[j] * math.cos(ang[j]), cy + oy + rr[j] * math.sin(ang[j])) for j in range(k)]
                dm.polygon(pts, fill=255); dt.polygon(pts, fill=tone)
    return np.asarray(m) / 255.0, np.asarray(t) / 255.0

# ---------------------------------------------------------------- asphalt (4 m)
N = 2048
mott = tile_noise(N, 3, 5, 1)            # wear and repair mottling
fine = tile_noise(N, 64, 4, 2)
sm, st = stones(N, 4.0, 90000, 1.5, 9.0, 3)
binder = 0.10 + 0.05 * fine
stone = 0.36 + 0.40 * st
base = binder * (1 - sm) + stone * sm
base = base * (0.78 + 0.42 * mott)        # weathered: the binder greys, worn spots lighter
alb = np.dstack([base * 0.98, base, base * 1.03])
save('asphalt_albedo', alb, 'RGB')
h = sm * (0.6 + 0.4 * st) + 0.15 * fine
save('asphalt_normal', normal_from_height(np.asarray(Image.fromarray((h * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.8))) / 255.0, 6.0), 'RGB')
save('asphalt_roughness', 0.78 + 0.18 * (1 - sm) - 0.08 * mott)

# ---------------------------------------------------------------- concrete (4 m)
grain = tile_noise(N, 128, 4, 11)
sm2, st2 = stones(N, 4.0, 60000, 0.6, 3.5, 12)
broom = tile_noise(N, 2, 1, 13)
yy = np.arange(N)[:, None]; xx = np.arange(N)[None, :]
lines = 0.5 + 0.5 * np.sin((yy + 6 * tile_noise(N, 8, 2, 14) * N / 64) * 2 * math.pi / 9.0)   # broom lines across
stain = tile_noise(N, 4, 5, 15)
cbase = 0.50 + 0.06 * grain - 0.03 * lines + 0.05 * sm2 * (st2 - 0.5)
cbase = cbase * (0.86 + 0.20 * stain)
save('concrete_albedo', np.dstack([cbase, cbase * 0.995, cbase * 0.97]), 'RGB')
ch = 0.3 * grain + 0.25 * lines + 0.3 * sm2
save('concrete_normal', normal_from_height(ch, 2.5), 'RGB')
save('concrete_roughness', 0.86 + 0.08 * grain - 0.05 * stain)

# ---------------------------------------------------------------- crack layers (16 m, 4096 px: 3.9 mm a pixel)
C = 4096; PX = C / 16.0      # pixels a metre
def wander(draw_list, start, heading, length_m, step_m, turn, seed):
    r = np.random.default_rng(seed); p = np.array(start, float); a = heading; pts = [tuple(p)]
    for _ in range(int(length_m / step_m)):
        a += r.normal(0, turn); p = p + step_m * PX * np.array([math.cos(a), math.sin(a)]); pts.append(tuple(p))
    draw_list.append(pts); return pts
def draw_wrapped(d, pts, width, fill):
    for ox in (-C, 0, C):
        for oy in (-C, 0, C):
            d.line([(x + ox, y + oy) for x, y in pts], fill=fill, width=int(max(1, width)), joint='curve')

seal = Image.new('L', (C, C), 0); ds = ImageDraw.Draw(seal)
open_ = Image.new('L', (C, C), 0); do = ImageDraw.Draw(open_)
r = np.random.default_rng(21)
cracks = []
# long cracks, mostly along and across the pavement (as reflective and joint cracks run), gently wandering, some crossing
for i in range(9):
    along = r.random() < 0.5
    ang = (0 if along else math.pi / 2) + r.normal(0, 0.25)
    pts = wander(cracks, (r.random() * C, r.random() * C), ang, 6 + r.random() * 14, 0.08, 0.035, 100 + i)
    w = (3.0 + 1.5 * r.random()) / 100 * PX          # 3-4.5 cm overband
    draw_wrapped(ds, pts, w, 255)
    draw_wrapped(do, pts, 0.004 * PX * 1.5, 255)     # the crack itself inside the band
# fine open branches and hairlines (unsealed)
for i in range(60):
    base_ = cracks[r.integers(len(cracks))]; s = base_[r.integers(len(base_))]
    pts = wander([], s, r.random() * 2 * math.pi, 0.3 + r.random() * 1.6, 0.03, 0.25, 300 + i)
    draw_wrapped(do, pts, 1 + r.integers(2), 200)
seal = np.asarray(seal.filter(ImageFilter.GaussianBlur(1.6))) / 255.0
open_a = np.asarray(open_.filter(ImageFilter.GaussianBlur(0.6))) / 255.0
# patched and stained areas: rounded rectangles of fresher (darker) asphalt and soft stains
patch = Image.new('L', (C, C), 0); dp = ImageDraw.Draw(patch)
for i in range(6):
    cx, cy = r.random() * C, r.random() * C; w, h = (0.8 + r.random() * 2.5) * PX, (0.6 + r.random() * 1.8) * PX
    for ox in (-C, 0, C):
        for oy in (-C, 0, C): dp.rectangle((cx + ox, cy + oy, cx + ox + w, cy + oy + h), fill=int(120 + 100 * r.random()))
pat = np.asarray(patch.filter(ImageFilter.GaussianBlur(2))) / 255.0
stains = np.clip((tile_noise(C, 6, 5, 22) - 0.62) * 4, 0, 1) * 0.6
save('cracks_asphalt', np.dstack([seal, open_a, np.maximum(pat, stains)]), 'RGB')

# concrete: hairlines and the odd corner break, stains and tyre smudges
hair = Image.new('L', (C, C), 0); dh = ImageDraw.Draw(hair)
for i in range(22):
    pts = wander([], (r.random() * C, r.random() * C), r.random() * 2 * math.pi, 0.8 + r.random() * 3.5, 0.04, 0.12, 500 + i)
    draw_wrapped(dh, pts, 1 + r.integers(2), 230)
    for b in range(r.integers(0, 3)):
        s = pts[r.integers(len(pts))]
        draw_wrapped(dh, wander([], s, r.random() * 2 * math.pi, 0.2 + r.random() * 0.6, 0.03, 0.3, 700 + i * 5 + b), 1, 170)
hair_a = np.asarray(hair.filter(ImageFilter.GaussianBlur(0.5))) / 255.0
smudge = np.clip((tile_noise(C, 5, 5, 31) - 0.58) * 3, 0, 1) * 0.5
save('cracks_concrete', np.dstack([np.zeros((C, C)), hair_a, smudge]), 'RGB')

# ---------------------------------------------------------------- tyre rubber (64 m along x, 32 m across: 4096 x 2048)
W, H = 4096, 2048; pxm = W / 64.0
rub = Image.new('L', (W, H), 0); dr = ImageDraw.Draw(rub)
for i in range(1400):
    y = H / 2 + r.normal(0, H * 0.13)           # densest down the middle (the main gear tracks), fading out
    x0 = r.random() * W; L = (2 + r.random() * 18) * pxm
    ang = r.normal(0, 0.012); w = 1 + r.integers(0, 4)
    x1, y1 = x0 + L * math.cos(ang), y + L * math.sin(ang)
    tone = int(40 + 150 * r.random())
    for ox in (-W, 0, W): dr.line((x0 + ox, y, x1 + ox, y1), fill=tone, width=int(w))
rub_a = np.asarray(rub.filter(ImageFilter.GaussianBlur(1.2))) / 255.0
haze = np.exp(-((np.arange(H)[:, None] - H / 2) / (H * 0.16)) ** 2) * (0.35 + 0.3 * tile_noise(W, 8, 4, 41)[:H, :W] if False else 0.45)
rub_all = np.clip(rub_a * 0.9 + haze * 0.5, 0, 1)
Image.fromarray((rub_all * 255).astype(np.uint8), 'L').save(OUT / 'tyre_rubber.png'); print('made tyre_rubber')
# previews of each, as the eye sees them
prev = Image.new('RGB', (2048, 1024))
prev.paste(Image.open(OUT / 'asphalt_albedo.png').crop((0, 0, 512, 512)).resize((512, 512)), (0, 0))
prev.paste(Image.open(OUT / 'concrete_albedo.png').crop((0, 0, 512, 512)), (512, 0))
ca = (0.30 * (1 - np.dstack([seal] * 3) * 0.75) * (1 - np.dstack([open_a] * 3) * 0.5))[:1024, :1024]
prev.paste(Image.fromarray((np.clip(ca * 3, 0, 1) * 255).astype(np.uint8)).resize((512, 512)), (1024, 0))
prev.paste(Image.fromarray(((1 - rub_all[:, :2048] * 0.8) * 200).astype(np.uint8)).resize((512, 256)).convert('RGB'), (1536, 0))
prev.save(OUT / 'preview.png'); print('DONE')
