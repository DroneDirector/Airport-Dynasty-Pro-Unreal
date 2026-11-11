# The fire truck's cab textures (owner, 2026-11-11: "photoreal detail and yet tileable and seamless"), all drawn here
# from periodic noise and periodic cell patterns, so every map wraps with no seam:
#   T_Cab_Leather      black dashboard leather: pebbled grain (periodic Voronoi cells), creases, soft sheen (+ normal)
#   T_Cab_Stitch       a row of thread stitches along u, the holes pressed in, transparent between (RGBA)
#   T_Cab_PedalRubber  ribbed pedal rubber with worn tips (+ normal)
#   T_Cab_Headliner    the roof lining: a fine woven fabric, light warm grey (+ normal)
#   T_Cab_WallPanel    the inner wall panels: hard plastic with a fine stipple grain (+ normal)
#   T_Cab_FloorMat     the floor: heavy rubber mat with raised coin dots (+ normal)
# python Art/FireTruck/make_cab_textures.py  ->  Art/FireTruck/T_Cab_*.png
import os
import numpy as np
from PIL import Image
from scipy.spatial import cKDTree

HERE = os.path.dirname(os.path.abspath(__file__))
N = 2048
RNG = np.random.default_rng(11)


def save(a, name, mode='RGB'):
    Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), mode).save(os.path.join(HERE, name))


def periodic_noise(n, falloff, seed):
    """Seamless fractal noise: white noise shaped in the frequency domain (an FFT field wraps by construction)."""
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(n)
    fx, fy = np.meshgrid(f, f); r = np.sqrt(fx * fx + fy * fy); r[0, 0] = 1
    spec = (rng.normal(size=(n, n)) + 1j * rng.normal(size=(n, n))) / r ** falloff
    spec[0, 0] = 0
    a = np.real(np.fft.ifft2(spec))
    return (a - a.min()) / (a.max() - a.min())


def normal_from_height(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-dx, dy, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return (n * 0.5 + 0.5) * 255


def voronoi_cells(n, count, seed):
    """Distance to the nearest and second-nearest of `count` random points on a torus (periodic): pebble cells."""
    rng = np.random.default_rng(seed)
    pts = rng.random((count, 2)) * n
    tree = cKDTree(pts, boxsize=n)
    yy, xx = np.mgrid[0:n, 0:n]
    d, _ = tree.query(np.column_stack([xx.ravel() + 0.5, yy.ravel() + 0.5]), k=2)
    return d[:, 0].reshape(n, n), d[:, 1].reshape(n, n)


def leather():
    d1, d2 = voronoi_cells(N, 9000, 1)
    edge = np.clip((d2 - d1) / 6.0, 0, 1)                     # 0 in the creases between pebbles
    pebble = np.sqrt(edge)
    big = periodic_noise(N, 1.6, 2)                           # broad soft folds
    fine = periodic_noise(N, 0.9, 3)                          # pores
    h = pebble * 0.8 + big * 0.35 + fine * 0.08
    h = (h - h.min()) / (h.max() - h.min())
    tone = 14 + h * 16 + (big - 0.5) * 10                    # near-black with a warm cast, lighter on the pebble tops
    save(np.dstack([tone * 1.05, tone, tone * 0.95]), 'T_Cab_Leather.png')
    save(normal_from_height(h, 9.0), 'T_Cab_Leather_N.png')


def stitch():
    """512 x 64: four stitches along u, each a twisted thread lying in a pressed groove; alpha 0 between."""
    W, H = 512, 64
    img = np.zeros((H, W, 4))
    yy, xx = np.mgrid[0:H, 0:W]
    per = W / 4
    for k in range(4):
        x0 = k * per + per * 0.12; x1 = x0 + per * 0.66
        cx, cy = (x0 + x1) / 2, H / 2
        u = (xx - cx) / ((x1 - x0) / 2); v = (yy - cy) / (H * 0.22)
        inside = (u ** 2 + v ** 2) <= 1
        twist = 0.75 + 0.25 * np.cos((xx - x0) * 0.35 + v * 2.2)
        shade = (1 - v ** 2).clip(0, 1) ** 0.5 * twist
        thread = np.array([196, 190, 176])                    # light grey-beige thread
        img[inside, :3] = (thread * shade[inside, None]) + 30
        img[inside, 3] = 255
    save(img, 'T_Cab_Stitch.png', 'RGBA')


def pedal_rubber():
    n = 1024
    yy, xx = np.mgrid[0:n, 0:n] / n
    ribs = (np.cos(yy * np.pi * 2 * 16) * 0.5 + 0.5) ** 3     # 16 ribs per tile across v
    wear = periodic_noise(n, 1.4, 5)
    grain = periodic_noise(n, 0.7, 6)
    h = ribs * 0.9 + grain * 0.1
    tone = 10 + ribs * 10 + (wear > 0.62) * ribs * 18 + grain * 6
    save(np.dstack([tone, tone, tone + 1]), 'T_Cab_PedalRubber.png')
    save(normal_from_height(h, 8.0), 'T_Cab_PedalRubber_N.png')


def headliner():
    n = 1024
    yy, xx = np.mgrid[0:n, 0:n]
    p = 8                                                     # weave cell in pixels
    warp = np.sin(xx / p * np.pi) ** 2; weft = np.sin(yy / p * np.pi) ** 2
    over = ((xx // p + yy // p) % 2).astype(float)
    h = over * warp + (1 - over) * weft
    fuzz = periodic_noise(n, 0.6, 7); mott = periodic_noise(n, 1.8, 8)
    h = h * 0.7 + fuzz * 0.3
    tone = 150 + h * 26 + (mott - 0.5) * 14
    save(np.dstack([tone * 1.02, tone, tone * 0.96]), 'T_Cab_Headliner.png')
    save(normal_from_height(h, 2.5), 'T_Cab_Headliner_N.png')


def wall_panel():
    n = 1024
    d1, d2 = voronoi_cells(n, 14000, 9)
    stip = np.clip((d2 - d1) / 2.5, 0, 1)
    soft = periodic_noise(n, 1.5, 10)
    h = stip * 0.7 + soft * 0.3
    tone = 34 + h * 10 + (soft - 0.5) * 6
    save(np.dstack([tone, tone * 1.01, tone * 1.03]), 'T_Cab_WallPanel.png')
    save(normal_from_height(h, 3.0), 'T_Cab_WallPanel_N.png')


def floor_mat():
    n = 1024
    yy, xx = np.mgrid[0:n, 0:n]
    s = n / 12                                                # 12 coin dots per tile each way
    cx = (xx % s) - s / 2; cy = (yy % s) - s / 2
    r = np.sqrt(cx * cx + cy * cy) / (s * 0.32)
    coin = np.clip(1 - r ** 6, 0, 1)
    dirt = periodic_noise(n, 1.6, 12); grain = periodic_noise(n, 0.7, 13)
    h = coin * 0.85 + grain * 0.15
    tone = 13 + coin * 8 + (dirt - 0.5) * 10 + grain * 4
    save(np.dstack([tone, tone, tone]), 'T_Cab_FloorMat.png')
    save(normal_from_height(h, 7.0), 'T_Cab_FloorMat_N.png')


def vinyl():
    """Soft-touch padded dash vinyl: charcoal, a fine even grain (small periodic cells), a soft sheen variation."""
    n = 1024
    d1, d2 = voronoi_cells(n, 30000, 21)
    g = np.clip((d2 - d1) / 2.0, 0, 1) ** 0.7
    soft = periodic_noise(n, 1.7, 22); fine = periodic_noise(n, 0.8, 23)
    h = g * 0.75 + fine * 0.25
    tone = 30 + h * 9 + (soft - 0.5) * 7
    save(np.dstack([tone, tone * 1.01, tone * 1.04]), 'T_Cab_Vinyl.png')
    save(normal_from_height(h, 3.5), 'T_Cab_Vinyl_N.png')


if __name__ == '__main__':
    leather(); stitch(); pedal_rubber(); headliner(); wall_panel(); floor_mat(); vinyl()
    print('cab textures written')
