# The fire truck's tiling textures (our own, drawn here): diamond tread plate for the steps and the roof walkway, a fine
# rubber grain for the tyres, perforated black mesh for the rear grille, red/white warning chevrons for the rear corner
# boards, each with a normal map where the surface has relief. All tile seamlessly.
# python Art/FireTruck/make_truck_textures.py  ->  Art/FireTruck/T_Truck_*.png
import math, os, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
N = 1024


def save(a, name):
    Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(HERE, name))


def normal_from_height(h, strength):
    """A tangent-space normal map (OpenGL green up) from a height field 0..1 that wraps."""
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-dx, dy, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return (n * 0.5 + 0.5) * 255


def diamond_plate():
    """Aluminium tread plate: raised lozenges in a herringbone, brushed between them."""
    y, x = np.mgrid[0:N, 0:N] / N
    h = np.zeros((N, N))
    cells = 8
    for cy in range(cells):
        for cx in range(cells):
            u0, v0 = (cx + 0.5) / cells, (cy + 0.5) / cells
            ang = math.radians(45 if (cx + cy) % 2 else -45)
            du = (x - u0 + 0.5) % 1 - 0.5; dv = (y - v0 + 0.5) % 1 - 0.5
            a = du * math.cos(ang) + dv * math.sin(ang); b = -du * math.sin(ang) + dv * math.cos(ang)
            lz = np.clip(1 - (np.abs(a) / 0.045) ** 2 - (np.abs(b) / 0.011) ** 2, 0, 1)
            h = np.maximum(h, np.sqrt(lz))
    rng = np.random.default_rng(3)
    brush = np.repeat(rng.normal(0, 1, (1, N)), N, 0) * 0.5 + rng.normal(0, 1, (N, N)) * 0.2
    base = 170 + h * 40 + brush * 6
    save(np.dstack([base, base + 2, base + 5]), 'T_Truck_DiamondPlate.png')
    save(normal_from_height(h, 6.0), 'T_Truck_DiamondPlate_N.png')


def rubber():
    """Tyre rubber: very dark, a faint grain and mould flecks."""
    rng = np.random.default_rng(7)
    g = rng.normal(0, 1, (N, N))
    img = Image.fromarray(((g * 0.5 + 0.5) * 255).clip(0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
    g = np.asarray(img).astype(float) / 255
    base = 12 + g * 9
    save(np.dstack([base, base, base + 1]), 'T_Truck_Rubber.png')
    save(normal_from_height(g, 1.5), 'T_Truck_Rubber_N.png')


def perforated():
    """Black perforated grille: round holes in a staggered grid (holes dark, the sheet satin black)."""
    img = Image.new('L', (N, N), 40); d = ImageDraw.Draw(img)
    s = N // 16; r = s * 0.33
    for j in range(17):
        for i in range(17):
            cx = i * s + (s / 2 if j % 2 else 0); cy = j * s
            for ox in (-N, 0, N):
                for oy in (-N, 0, N):
                    d.ellipse([cx - r + ox, cy - r + oy, cx + r + ox, cy + r + oy], fill=4)
    a = np.asarray(img).astype(float)
    save(np.dstack([a, a, a + 1]), 'T_Truck_Perforated.png')
    save(normal_from_height(a / 40.0, 2.0), 'T_Truck_Perforated_N.png')


def chevrons():
    """Red and white diagonal warning stripes (45 degrees, four pairs per tile)."""
    y, x = np.mgrid[0:N, 0:N] / N
    band = ((x + y) * 4) % 1 < 0.5
    rgb = np.where(band[..., None], np.array([196, 18, 22]), np.array([236, 236, 230]))
    save(rgb.astype(float), 'T_Truck_Chevrons.png')


if __name__ == '__main__':
    diamond_plate(); rubber(); perforated(); chevrons()
    print('truck textures written')
