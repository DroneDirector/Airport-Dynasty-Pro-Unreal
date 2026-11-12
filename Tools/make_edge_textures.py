# The soft edges where pavement meets the ground (owner, 2026-11-12, after his reference shots): drawn here from
# noise, our own, nothing from elsewhere. Each picture is a band laid ALONG the edge: it tiles left to right (every
# 4 m in the game), and runs across from the pavement side (top) over the edge (the middle) to the ground side
# (bottom), see-through where nothing has crept, so the pavement and the grass show through it.
#   edge_concrete.png     a concrete slab's edge: a thin dark soil seam right at the edge, dirt and grime creeping a
#                         little way onto the concrete (darker, uneven, a touch of moss), the grass side worn and browner
#   edge_asphalt.png      an asphalt edge: rougher and crumblier, the asphalt breaking into crumbs and small cracks on
#                         the ground side, pale dust on the asphalt side, soil and worn grass beyond
#   edge_asphalt_end.png  the same with bigger cracks and breaks (the runway's ends)
#   python Tools/make_edge_textures.py   ->   Art/Edges/*.png (1024 x 512, RGBA)
import numpy as np
from PIL import Image, ImageFilter
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'Art' / 'Edges'
OUT.mkdir(parents=True, exist_ok=True)
W, H = 1024, 512
rng = np.random.default_rng(2611)
yy, xx = np.mgrid[0:H, 0:W]
V = yy / (H - 1)          # 0 at the pavement side, 1 at the ground side; the edge itself at 0.5


def noise(scale, seed, octaves=4):
    """Smooth noise 0..1 that tiles left to right (made on a torus along x)."""
    r = np.random.default_rng(seed)
    out = np.zeros((H, W))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        n = max(2, int(W / scale * (2 ** o)))
        m = max(2, int(H / scale * (2 ** o)))
        g = r.random((m + 1, n + 1))
        g[:, -1] = g[:, 0]                        # the same at both ends: it tiles along x
        gy = np.linspace(0, m, H); gx = np.linspace(0, n, W, endpoint=False)
        iy, ix = gy.astype(int), gx.astype(int); fy, fx = gy - iy, gx - ix
        iy = np.minimum(iy, m - 1); ix = np.minimum(ix, n - 1)
        fy = fy[:, None] * (3 - 2 * fy[:, None]); fx = fx[None, :] * (3 - 2 * fx[None, :])
        a = g[iy][:, ix]; b = g[iy][:, ix + 1]; c = g[iy + 1][:, ix]; d = g[iy + 1][:, ix + 1]
        out += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy)
        total += amp; amp *= 0.5
    return out / total


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def cracks(seed, count, length, width, wobble):
    """Dark cracks wandering from the edge out onto the ground side (a few onto the pavement): mostly straight runs
    with sharp turns now and then, some branching, each drawn as runs that never cross the picture's wrap."""
    img = Image.new('L', (W, H), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(img)
    r = np.random.default_rng(seed)
    def run(x, y, ang, steps, w):
        pts = [(x, y)]
        for _ in range(int(steps)):
            if r.random() < 0.07: ang += r.uniform(-0.9, 0.9)      # a sharp turn
            else: ang += r.uniform(-wobble, wobble) * 0.15
            nx, ny = x + np.cos(ang) * 3, y + np.sin(ang) * 3
            if nx < 0 or nx >= W:                                   # at the wrap: stop this run, carry on from the other side
                d.line(pts, fill=255, width=int(w)); pts = [(nx % W, ny)]
            else: pts.append((nx, ny))
            x, y = nx % W, ny
            if r.random() < 0.02 and w > 1.5: run(x, y, ang + r.choice([-1, 1]) * r.uniform(0.6, 1.3), steps * 0.4, w * 0.7)
        if len(pts) > 1: d.line(pts, fill=255, width=int(max(1, w)))
    for _ in range(count):
        x, y = r.uniform(0, W), H * r.uniform(0.46, 0.54)
        ang = (-np.pi / 2 if r.random() < 0.78 else np.pi / 2) + r.uniform(-0.9, 0.9)   # mostly up into the asphalt
        run(x, y, ang, length, width)
    return np.asarray(img.filter(ImageFilter.GaussianBlur(0.6))) / 255.0


def write(name, rgb, alpha):
    a = np.clip(alpha, 0, 1)
    img = np.dstack([np.clip(rgb, 0, 1) * 255, a * 255]).astype(np.uint8)
    Image.fromarray(img, 'RGBA').save(OUT / name)
    print('wrote', OUT / name)


# ---- concrete: the soil seam, grime creeping onto the slab, the worn grass side
def concrete():
    n1, n2, n3 = noise(180, 11), noise(60, 12), noise(22, 13, 3)
    edge = 0.5 + (n1 - 0.5) * 0.03                          # the edge wanders a hair
    seam = np.exp(-((V - edge) / 0.03) ** 2)                # the dark soil line, ~3 cm wide of a 2.6 m band
    grime_reach = 0.10 + 0.14 * n1                          # how far the dirt creeps onto the slab
    grime = smooth(edge - grime_reach, edge, V) * (0.35 + 0.65 * n2) * (V < edge)
    moss = np.clip(n3 - 0.72, 0, 1) * 3.5 * grime
    worn = smooth(edge, edge + 0.08 + 0.25 * n1, 1 - (V - edge)) * (V > edge)   # the ground side, browner near the edge
    worn = np.clip(worn, 0, 1) * (0.4 + 0.6 * n2)
    soil = np.exp(-((V - edge) / 0.06) ** 2) * (V > edge) * (0.5 + 0.5 * n3)
    rgb = np.zeros((H, W, 3))
    grime_col = np.array([0.22, 0.2, 0.17]); moss_col = np.array([0.25, 0.32, 0.12]); soil_col = np.array([0.16, 0.12, 0.08]); worn_col = np.array([0.36, 0.3, 0.16])
    alpha = np.zeros((H, W))
    for col, k in ((grime_col, grime * 0.75), (moss_col, moss * 0.8), (soil_col, np.maximum(seam, soil) * 0.95), (worn_col, worn * 0.55)):
        rgb = rgb * (1 - k[..., None]) + col * k[..., None]
        alpha = alpha + k * (1 - alpha)
    write('edge_concrete.png', rgb, alpha)


# ---- asphalt: crumbs and cracks, dust, soil and worn grass
def asphalt(name, crack_count, crack_len, crack_w, crumb):
    n1, n2, n3 = noise(160, 21), noise(40, 22), noise(14, 23, 3)
    edge = 0.5 + (n1 - 0.5) * 0.06                          # a rougher, more wandering edge
    cr = cracks(31, crack_count, crack_len, crack_w, 0.5)
    crumbs = (np.clip(n3 - 0.55, 0, 1) * 2.2) * smooth(edge - 0.02, edge + 0.12 + 0.1 * n1, V) * (V > edge - 0.02) * crumb
    crumbs = crumbs * smooth(edge + 0.35, edge + 0.1, V)    # none far out on the grass
    dust = smooth(edge - 0.16 - 0.1 * n1, edge, V) * (V < edge) * (0.3 + 0.7 * n2) * 0.5
    soil = np.exp(-((V - edge) / 0.05) ** 2) * (V > edge - 0.01) * (0.5 + 0.5 * n3)
    worn = np.clip(smooth(edge, edge + 0.1 + 0.25 * n1, 1 - (V - edge)) * (V > edge), 0, 1) * (0.4 + 0.6 * n2)
    rgb = np.zeros((H, W, 3))
    asph = np.array([0.05, 0.05, 0.055]); dust_col = np.array([0.5, 0.47, 0.42]); soil_col = np.array([0.15, 0.11, 0.07]); worn_col = np.array([0.35, 0.29, 0.15]); crack_col = np.array([0.03, 0.03, 0.03])
    alpha = np.zeros((H, W))
    for col, k in ((dust_col, dust), (soil_col, soil * 0.9), (worn_col, worn * 0.55), (asph, crumbs * 0.95), (crack_col, cr * 0.9)):
        rgb = rgb * (1 - k[..., None]) + col * k[..., None]
        alpha = alpha + k * (1 - alpha)
    write(name, rgb, alpha)


concrete()
asphalt('edge_asphalt.png', 12, 26, 1.6, 1.0)
asphalt('edge_asphalt_end.png', 20, 48, 2.4, 1.3)
