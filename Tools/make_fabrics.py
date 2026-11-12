# The fabric library for the people's clothes (owner, 2026-11-12: every garment must look like the real material:
# sharp weave, the right shine, wear). Ten fabrics, each drawn here from scratch (nothing from elsewhere) as a tiling
# set of pictures the garments' materials use: colour (albedo), the surface's relief (normal map) and its roughness.
# 2048 x 2048, tiling, about 4 cm of cloth across a tile for the fine weaves (jersey, satin) and 8-10 cm for the
# coarse ones (denim, knit, canvas, leather), so the threads read at close range and never blur into a flat colour.
#   python Tools/make_fabrics.py [name ...]   ->   Art/Fabrics/<name>/{albedo,normal,roughness}.png
import sys
import numpy as np
from PIL import Image
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'Art' / 'Fabrics'
N = 2048
yy, xx = np.mgrid[0:N, 0:N] / N          # 0..1 across the tile


def tile_noise(scale, seed, octaves=4, persistence=0.5):
    """Smooth noise 0..1 that tiles both ways (made on a torus)."""
    r = np.random.default_rng(seed)
    out, amp, total = np.zeros((N, N)), 1.0, 0.0
    for o in range(octaves):
        n = max(2, int(scale * (2 ** o)))
        g = r.random((n, n))
        gy = np.linspace(0, n, N, endpoint=False); gx = gy
        iy, ix = gy.astype(int) % n, gx.astype(int) % n
        fy, fx = gy - np.floor(gy), gx - np.floor(gx)
        fy = fy[:, None] * fy[:, None] * (3 - 2 * fy[:, None]); fx = fx[None, :] * fx[None, :] * (3 - 2 * fx[None, :])
        iy1, ix1 = (iy + 1) % n, (ix + 1) % n
        a = g[iy][:, ix]; b = g[iy][:, ix1]; c = g[iy1][:, ix]; d = g[iy1][:, ix1]
        out += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy)
        total += amp; amp *= persistence
    return out / total


def thread(coord, count, width=0.5, sharp=2.0):
    """A row of round threads: 1 on a thread's crown, 0 in the gap; count threads across the tile."""
    t = (coord * count) % 1.0
    return np.clip(1 - (np.abs(t - 0.5) / (width / 2)) ** sharp, 0, 1)


def normal_from_height(h, strength):
    """A tangent-space normal map from a height field (tiling)."""
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength
    nx, ny, nz = -dx, dy, np.ones_like(h)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.dstack([nx / l * 0.5 + 0.5, ny / l * 0.5 + 0.5, nz / l * 0.5 + 0.5])


def save(name, albedo, height, rough, nstrength):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    Image.fromarray((np.clip(albedo, 0, 1) * 255).astype(np.uint8), 'RGB').save(d / 'albedo.png')
    Image.fromarray((np.clip(normal_from_height(height, nstrength), 0, 1) * 255).astype(np.uint8), 'RGB').save(d / 'normal.png')
    Image.fromarray((np.clip(rough, 0, 1) * 255).astype(np.uint8), 'L').save(d / 'roughness.png')
    print('wrote', d)


def tint(h, col, shade=0.35):
    """A colour darkened in the weave's hollows."""
    c = np.asarray(col)[None, None, :]
    return c * (1 - shade + shade * h[..., None])


# ---- the fabrics
def cotton_jersey():
    # knit jersey (a tee): columns of V-shaped loops, soft, matte; a little fuzz; 4 cm across the tile (≈ 24 wales)
    wales, courses = 24, 34
    u, v = xx * wales, yy * courses
    vshape = np.abs(((u % 1.0) - 0.5)) * 2                       # the V of each loop across a wale
    loop = np.clip(1 - np.abs(((v + vshape * 0.5) % 1.0) - 0.5) * 2.2, 0, 1)
    h = 0.55 * loop + 0.3 * thread(xx, wales, 0.75, 1.5) + 0.15 * tile_noise(6, 1, 3)
    fuzz = tile_noise(300, 2, 2) * 0.08
    alb = tint(h, (0.86, 0.86, 0.84), 0.3) + fuzz[..., None] - 0.04
    rough = 0.82 + 0.1 * (1 - h) + 0.05 * tile_noise(40, 3)
    save('cotton_jersey', alb, h, rough, 2.2)


def denim():
    # twill: warp over two, under one, the diagonal ridge; indigo warp, white weft showing in the hollows; 8 cm across
    count = 64
    u, v = xx * count, yy * count
    diag = ((u + v * 1.0) % 3.0)                                 # the twill's step
    warp_up = diag < 2.0                                         # where the indigo warp lies on top
    warp = thread(xx, count, 0.85, 1.4); weft = thread(yy, count, 0.85, 1.4)
    h = np.where(warp_up, 0.6 + 0.4 * warp, 0.25 + 0.3 * weft) + 0.08 * tile_noise(10, 4, 3)
    indigo, white = np.array((0.11, 0.17, 0.33)), np.array((0.82, 0.8, 0.74))
    wear = tile_noise(3, 5, 4) * 0.5 + tile_noise(60, 6, 2) * 0.2       # the fading on the high threads
    col = np.where(warp_up[..., None], indigo[None, None, :] * (1 + 0.9 * wear[..., None] * warp[..., None]), white[None, None, :] * (0.7 + 0.3 * weft[..., None]))
    rough = 0.75 + 0.15 * (1 - h) - 0.1 * wear
    save('denim', col, h, rough, 3.0)


def wool_suiting():
    # fine plain weave with a slight nap, matte, dark; 4 cm across (≈ 90 threads); the fibres' fuzz on top
    count = 90
    warp = thread(xx, count, 0.8, 1.6); weft = thread(yy, count, 0.8, 1.6)
    over = ((np.floor(xx * count) + np.floor(yy * count)) % 2) == 0
    h = np.where(over, 0.55 + 0.45 * warp, 0.55 + 0.45 * weft) * 0.6 + 0.4 * tile_noise(200, 7, 2)
    alb = tint(h, (0.13, 0.13, 0.15), 0.25) + (tile_noise(400, 8, 1) * 0.03)[..., None]
    rough = 0.9 + 0.08 * tile_noise(80, 9)
    save('wool_suiting', alb, h, rough, 1.4)


def nylon_shell():
    # ripstop: a fine plain weave with a grid of thicker threads every 5 mm, a sheen; 5 cm across
    count = 140
    warp = thread(xx, count, 0.9, 1.3); weft = thread(yy, count, 0.9, 1.3)
    over = ((np.floor(xx * count) + np.floor(yy * count)) % 2) == 0
    rip = np.maximum(thread(xx, 10, 0.08, 1.2), thread(yy, 10, 0.08, 1.2))
    h = np.where(over, 0.5 + 0.5 * warp, 0.5 + 0.5 * weft) * 0.7 + 0.3 * rip
    alb = tint(h, (0.12, 0.2, 0.38), 0.2)
    rough = 0.38 + 0.12 * (1 - h) + 0.04 * tile_noise(30, 10)
    save('nylon_shell', alb, h, rough, 1.2)


def hivis_polyester():
    # a knit polyester mesh (the hi-vis vest): a honeycomb of small holes, fluorescent yellow-green; 5 cm across
    cols, rows = 40, 46
    u = xx * cols; v = yy * rows
    off = (np.floor(v) % 2) * 0.5
    cell = np.sqrt(((u + off) % 1.0 - 0.5) ** 2 + ((v % 1.0 - 0.5) * 0.87) ** 2)
    hole = np.clip((cell - 0.22) / 0.12, 0, 1)                   # 1 on the yarn, 0 in the hole
    h = hole * (0.7 + 0.3 * tile_noise(80, 11, 2))
    alb = tint(h, (0.78, 0.95, 0.05), 0.5)
    rough = 0.6 + 0.2 * (1 - h)
    save('hivis_polyester', alb, h, rough, 2.5)


def fleece():
    # brushed fleece: no visible weave, a soft pile of fine fibres; 6 cm across
    pile = tile_noise(500, 12, 2) * 0.5 + tile_noise(120, 13, 3) * 0.3 + tile_noise(8, 14, 3) * 0.2
    h = pile
    alb = tint(h, (0.35, 0.36, 0.4), 0.3)
    rough = 0.95 - 0.05 * h
    save('fleece', alb, h, rough, 1.0)


def knit():
    # a chunky sweater knit: big V loops in columns, cabled texture; 10 cm across (≈ 12 wales)
    wales, courses = 12, 16
    u, v = xx * wales, yy * courses
    vshape = np.abs(((u % 1.0) - 0.5)) * 2
    loop = np.clip(1 - np.abs(((v + vshape * 0.6) % 1.0) - 0.5) * 2.0, 0, 1)
    yarn = thread(xx, wales, 0.8, 1.3)
    h = 0.6 * loop * yarn + 0.25 * yarn + 0.15 * tile_noise(200, 15, 2)
    alb = tint(h, (0.55, 0.47, 0.36), 0.45)
    rough = 0.92 + 0.05 * tile_noise(50, 16)
    save('knit', alb, h, rough, 3.5)


def leather():
    # grained leather: cells of the grain (a Voronoi-like pattern from noise), creases, a soft sheen; 8 cm across
    cells = tile_noise(70, 17, 1)
    grain = np.abs(cells - np.roll(cells, 3, axis=0)) + np.abs(cells - np.roll(cells, 3, axis=1))
    grain = 1 - np.clip(grain * 6, 0, 1)
    crease = 1 - np.clip(np.abs(tile_noise(4, 18, 3) - 0.5) * 12, 0, 1) * 0.25
    h = grain * 0.7 + tile_noise(300, 19, 2) * 0.3
    h = h * crease
    alb = tint(h, (0.2, 0.12, 0.08), 0.5)
    rough = 0.45 + 0.25 * (1 - h) + 0.05 * tile_noise(40, 20)
    save('leather', alb, h, rough, 2.0)


def canvas():
    # heavy cotton canvas (work trousers, bags): a coarse plain weave; 8 cm across
    count = 48
    warp = thread(xx, count, 0.9, 1.3); weft = thread(yy, count, 0.9, 1.3)
    over = ((np.floor(xx * count) + np.floor(yy * count)) % 2) == 0
    h = np.where(over, 0.5 + 0.5 * warp, 0.5 + 0.5 * weft) * 0.8 + 0.2 * tile_noise(12, 21, 3)
    dirt = tile_noise(3, 22, 4) * 0.12
    alb = tint(h, (0.62, 0.55, 0.4), 0.4) - dirt[..., None]
    rough = 0.88 + 0.06 * (1 - h)
    save('canvas', alb, h, rough, 2.6)


def satin():
    # satin (a blouse, a tie): long floats of fine warp threads, a strong directional sheen; 4 cm across
    count = 200
    warp = thread(xx, count, 0.95, 1.1)
    h = 0.7 + 0.3 * warp + 0.05 * tile_noise(20, 23, 3)
    alb = tint(h, (0.62, 0.08, 0.14), 0.15)
    rough = 0.22 + 0.08 * (1 - warp) + 0.03 * tile_noise(60, 24)
    save('satin', alb, h, rough, 0.6)


ALL = {f.__name__: f for f in (cotton_jersey, denim, wool_suiting, nylon_shell, hivis_polyester, fleece, knit, leather, canvas, satin)}
for name in (sys.argv[1:] or ALL):
    ALL[name]()
