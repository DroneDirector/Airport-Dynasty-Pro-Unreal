# The cargo terminal's texture sets (photoreal materials, reference: docs/Reference/CargoTerminal/README.md).
# Every set is made here from CC0 scans already in the project plus our own drawn patterns (panel joints, standing seams,
# door sections, mullions, solar cells); nothing is downloaded. Sources (all CC0 1.0):
#   ambientCG (https://ambientcg.com): Paint004, Metal011, Metal027, Rubber004 (Art/GPU/Textures)
#   Poly Haven (https://polyhaven.com): concrete_floor_worn_001, brushed_concrete_04, asphalt_02 (Art/Ground/Scans)
# Each set: <Name>_Color.jpg (or .png with alpha for glass), <Name>_Normal.jpg (OpenGL, +y up), <Name>_ORM.jpg
# (r = occlusion, g = roughness, b = metalness: glTF's packing). Seamless; one texture tile = TILE[name] metres.
#   python Art/CargoTerminal/make_cargo_textures.py
import os
try:
    import numpy as np
    from PIL import Image
except ImportError:          # (Blender's Python reads only TILE from here)
    np = Image = None

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(HERE, 'Textures')
GPU = os.path.join(ROOT, 'Art', 'GPU', 'Textures')
SCAN = os.path.join(ROOT, 'Art', 'Ground', 'Scans')

# metres one texture tile covers (the Blender script reads this too, for the UVs)
TILE = dict(Cladding=4.0, CladdingBand=4.0, CladdingWhite=4.0, Roof=4.0, SectionalDoor=2.4, FireShutter=2.4,
            PersonnelDoor=2.0, FloorConcrete=6.0, FireWall=4.0, Asphalt=6.0, OfficeGlass=3.0, Solar=2.0,
            Galvanised=2.0, Aluminium=2.0, LeadSheet=2.0, Rubber=1.5, OfficeFloor=3.0,
            PaintGrey=2.0, PaintWhite=2.0, PaintOrange=2.0, PaintYellow=2.0, PaintTeal=2.0, PaintBlue=2.0,
            Cardboard=1.5, LineYellow=1.0, LineWhite=1.0, LineRed=1.0, LineGreen=1.0,
            CladdingAged=4.0, CladdingBandAged=4.0, Grime=0.6,   # (these three: make_cargo_aged_textures.py)
            SteelGalv=2.0, ChequerPlate=0.6)                     # (these two: steel_sets() below)


def load(path, size, gray=False):
    im = Image.open(path).convert('L' if gray else 'RGB').resize((size, size), Image.LANCZOS)
    return np.asarray(im, dtype=np.float32) / 255.0


def save(name, kind, arr, png=False):
    os.makedirs(OUT, exist_ok=True)
    a = np.clip(arr * 255.0 + 0.5, 0, 255).astype(np.uint8)
    im = Image.fromarray(a)
    p = os.path.join(OUT, '%s_%s.%s' % (name, kind, 'png' if png else 'jpg'))
    im.save(p, quality=90) if not png else im.save(p, optimize=True)


def detail(rgb, amount):
    """The scan's grain only, around 1.0 (its colour and overall shade dropped)."""
    g = rgb.mean(axis=2)
    return 1.0 + (g - g.mean()) / max(g.std(), 1e-4) * amount


def srgb(c):
    return np.array(c, dtype=np.float32)


def height_to_normal(h, px_per_m, depth_m):
    """A height field (0..1, times depth_m metres) to an OpenGL tangent-space normal map (tiles: wraps at the edges)."""
    hm = h * depth_m
    dx = (np.roll(hm, -1, 1) - np.roll(hm, 1, 1)) * px_per_m / 2.0
    dy = (np.roll(hm, 1, 0) - np.roll(hm, -1, 0)) * px_per_m / 2.0   # image rows run down: +v is up
    n = np.dstack([-dx, -dy, np.ones_like(hm)])
    return n / np.linalg.norm(n, axis=2, keepdims=True)


def blend_normals(a, b):
    """Two tangent-space normals (unit vectors) combined (whiteout blend)."""
    n = np.dstack([a[..., 0] + b[..., 0], a[..., 1] + b[..., 1], a[..., 2] * b[..., 2]])
    return n / np.linalg.norm(n, axis=2, keepdims=True)


def scan_normal(path, size, dx=False):
    n = load(path, size) * 2.0 - 1.0
    if dx: n[..., 1] *= -1.0                       # Poly Haven's DirectX maps: green flipped to OpenGL
    return n / np.linalg.norm(n, axis=2, keepdims=True)


def write(name, col, nrm, rough, metal, ao=None, png_col=False):
    save(name, 'Color', col, png_col)
    save(name, 'Normal', nrm * 0.5 + 0.5)
    ao = np.ones_like(rough) if ao is None else ao
    save(name, 'ORM', np.dstack([ao, rough, np.broadcast_to(metal, rough.shape)]))


def lines(size, tile_m, every_m, width_m, axis, soft=0.6):
    """1 on joint lines (every every_m metres, width_m wide) along rows (axis 0: horizontal lines) or columns."""
    t = (np.arange(size) + 0.5) / size * tile_m
    d = np.abs(((t + every_m / 2) % every_m) - every_m / 2)       # distance to the nearest line
    prof = np.clip(1.0 - (d - width_m / 2) / (width_m * soft), 0, 1)
    return prof[:, None] * np.ones((1, size)) if axis == 0 else np.ones((size, 1)) * prof[None, :]


def main():
    paint = load(os.path.join(GPU, 'Paint004', 'Paint004_2K-JPG_Color.jpg'), 2048)
    paint_n = scan_normal(os.path.join(GPU, 'Paint004', 'Paint004_2K-JPG_NormalGL.jpg'), 2048)
    paint_r = load(os.path.join(GPU, 'Paint004', 'Paint004_2K-JPG_Roughness.jpg'), 2048, True)
    flat_n = np.dstack([np.zeros((2048, 2048)), np.zeros((2048, 2048)), np.ones((2048, 2048))]).astype(np.float32)
    soften = lambda n, k: (lambda m: m / np.linalg.norm(m, axis=2, keepdims=True))(np.dstack([n[..., 0] * k, n[..., 1] * k, n[..., 2]]))

    # sandwich-panel cladding: 1 m horizontal panels with a dark recessed joint, micro-ribs every 100 mm
    S = 2048; ppm = S / TILE['Cladding']
    joint = lines(S, 4.0, 1.0, 0.012, 0)
    rib = 0.5 + 0.5 * np.cos(2 * np.pi * ((np.arange(S) + 0.5) / S * 4.0) / 0.1)
    h = 0.6 + 0.4 * rib[:, None] * np.ones((1, S)) * 0.15 - joint
    n = blend_normals(height_to_normal(h, ppm, 0.006), soften(paint_n, 0.35))
    g = detail(paint, 0.04)
    for name, tint in (('Cladding', (0.74, 0.76, 0.77)), ('CladdingBand', (0.06, 0.22, 0.45)), ('CladdingWhite', (0.86, 0.88, 0.88))):
        col = srgb(tint)[None, None, :] * g[..., None] * (1.0 - 0.75 * joint[..., None])
        write(name, col, n, np.clip(0.42 + 0.15 * (paint_r - paint_r.mean()) + 0.3 * joint, 0, 1), 0.0)

    # standing-seam roof: raised seams every 0.5 m, running along v (the slope), dark grey coated steel
    m27 = load(os.path.join(GPU, 'Metal027', 'Metal027_2K-JPG_Color.jpg'), S)
    m27_n = scan_normal(os.path.join(GPU, 'Metal027', 'Metal027_2K-JPG_NormalGL.jpg'), S)
    m27_r = load(os.path.join(GPU, 'Metal027', 'Metal027_2K-JPG_Roughness.jpg'), S, True)
    seam = lines(S, 4.0, 0.5, 0.03, 1, soft=1.2)
    n = blend_normals(height_to_normal(seam, ppm, 0.05), soften(m27_n, 0.3))
    col = srgb((0.24, 0.26, 0.28))[None, None, :] * detail(m27, 0.05)[..., None] * (1.0 + 0.25 * seam[..., None])
    write('Roof', col, n, np.clip(0.5 + 0.2 * (m27_r - m27_r.mean()) - 0.1 * seam, 0, 1), 0.35)

    # sectional doors: 0.6 m sections with a hinge joint and fine stucco grain; the fire shutters 100 mm slats
    for name, tint, every, w in (('SectionalDoor', (0.92, 0.50, 0.06), 0.6, 0.010), ('FireShutter', (0.70, 0.08, 0.06), 0.1, 0.006),
                                 ('PersonnelDoor', (0.08, 0.42, 0.20), 9.0, 0.0)):
        tm = TILE[name]; p = S / tm
        jl = lines(S, tm, every, w, 0) if w else np.zeros((S, S), np.float32)
        hh = 1.0 - jl
        if name == 'SectionalDoor':        # each section's two shallow ribs
            t = (np.arange(S) + 0.5) / S * tm
            hh = hh + 0.25 * (np.abs(((t % every) - every / 2)) < 0.12)[:, None]
        n = blend_normals(height_to_normal(hh, p, 0.008), soften(paint_n, 0.6))
        col = srgb(tint)[None, None, :] * detail(paint, 0.05)[..., None] * (1.0 - 0.6 * jl[..., None])
        write(name, col, n, np.clip(0.38 + 0.15 * (paint_r - paint_r.mean()) + 0.3 * jl, 0, 1), 0.0)

    # sealed concrete floor: the worn floor scan lightened and sealed (glossier), a saw-cut joint at the 6 m tile edge
    cf = SCAN + '/concrete_floor_worn_001/concrete_floor_worn_001_'
    c = load(cf + 'diff_4k.jpg', S); r = load(cf + 'rough_4k.jpg', S, True); ao = load(cf + 'ao_4k.jpg', S, True)
    cut = lines(S, 6.0, 6.0, 0.006, 0) + lines(S, 6.0, 6.0, 0.006, 1)
    n = blend_normals(height_to_normal(1.0 - np.clip(cut, 0, 1), S / 6.0, 0.01), soften(scan_normal(cf + 'nor_dx_4k.jpg', S, True), 0.6))
    col = (c / c.mean() * 0.50) * (1.0 - 0.6 * np.clip(cut, 0, 1)[..., None])
    write('FloorConcrete', col, n, np.clip(0.30 + 0.35 * (r - r.mean()) + 0.4 * np.clip(cut, 0, 1), 0.08, 1), 0.0, ao)

    # fire walls: fair-faced concrete; the truck court: asphalt
    for name, base, lum in (('FireWall', 'brushed_concrete_04', 0.55), ('Asphalt', 'asphalt_02', 0.16)):
        bp = SCAN + '/%s/%s_' % (base, base)
        c = load(bp + 'diff_4k.jpg', S); r = load(bp + 'rough_4k.jpg', S, True); ao = load(bp + 'ao_4k.jpg', S, True)
        write(name, c / c.mean() * lum, scan_normal(bp + 'nor_dx_4k.jpg', S, True), np.clip(r, 0.3, 1), 0.0, ao)

    # office glazing: dark-tinted panes with dark grey aluminium mullions every 1.5 m and a transom at each 3 m tile
    G = 1024; tm = TILE['OfficeGlass']
    mull = np.clip(lines(G, tm, 1.5, 0.07, 1, 0.2) + lines(G, tm, 3.0, 0.12, 0, 0.2), 0, 1)
    col = srgb((0.10, 0.13, 0.15))[None, None, :] * (1 - mull[..., None]) + srgb((0.09, 0.09, 0.10))[None, None, :] * mull[..., None]
    alpha = 0.30 + 0.70 * mull
    n = height_to_normal(mull, G / tm, 0.02)
    write('OfficeGlass', np.dstack([col, alpha]), n, 0.04 + 0.4 * mull, 0.0, png_col=True)
    # (the ORM's metalness for the mullions: aluminium)
    save('OfficeGlass', 'ORM', np.dstack([np.ones_like(mull), 0.04 + 0.4 * mull, 0.9 * mull]))

    # solar panels: 156 mm cells, silver busbars, white frame lines between the cells
    tm = TILE['Solar']; t = (np.arange(G) + 0.5) / G * tm
    gap = np.clip(lines(G, tm, 0.158, 0.004, 0) + lines(G, tm, 0.158, 0.004, 1), 0, 1)
    bus = lines(G, tm, 0.052, 0.0015, 1)
    col = srgb((0.03, 0.05, 0.12))[None, None, :] * (1 - gap[..., None]) + srgb((0.75, 0.76, 0.78))[None, None, :] * gap[..., None]
    col = col * (1 - bus[..., None] * 0.0) + srgb((0.55, 0.56, 0.58))[None, None, :] * (bus * (1 - gap))[..., None] * 0.6
    write('Solar', col, height_to_normal(gap, G / tm, 0.002), np.clip(0.12 + 0.4 * gap, 0, 1), 0.2 * gap)

    # metals and rubber straight from their scans (1K), painted equipment from the paint scan, line paint
    small = lambda a: np.asarray(Image.fromarray(np.clip(a * 255, 0, 255).astype(np.uint8)).resize((G, G), Image.LANCZOS), np.float32) / 255
    for name, src, tint, metal in (('Galvanised', 'Metal027', None, 1.0), ('Aluminium', 'Metal011', None, 1.0), ('LeadSheet', 'Metal011', 0.45, 1.0),
                                   ('Rubber', 'Rubber004', None, 0.0)):
        sp = os.path.join(GPU, src, src + '_2K-JPG_')
        c = load(sp + 'Color.jpg', G); r = load(sp + 'Roughness.jpg', G, True)
        write(name, c * (tint or 1.0), scan_normal(sp + 'NormalGL.jpg', G), r, metal)
    pc, pn, pr = small(paint), np.dstack([small(paint_n[..., k] * 0.5 + 0.5) * 2 - 1 for k in range(3)]), small(paint_r)
    pn = pn / np.linalg.norm(pn, axis=2, keepdims=True)
    gd = detail(pc, 0.05)
    for name, tint, rough in (('PaintGrey', (0.42, 0.45, 0.47), 0.45), ('PaintWhite', (0.82, 0.83, 0.83), 0.4), ('PaintOrange', (0.85, 0.32, 0.04), 0.4),
                              ('PaintYellow', (0.95, 0.66, 0.04), 0.4), ('PaintTeal', (0.20, 0.50, 0.50), 0.4), ('PaintBlue', (0.10, 0.25, 0.50), 0.45),
                              ('OfficeFloor', (0.36, 0.38, 0.40), 0.55), ('Cardboard', (0.52, 0.38, 0.22), 0.85),
                              ('LineYellow', (0.90, 0.68, 0.05), 0.6), ('LineWhite', (0.85, 0.86, 0.85), 0.6), ('LineRed', (0.75, 0.10, 0.08), 0.6),
                              ('LineGreen', (0.10, 0.50, 0.22), 0.6)):
        write(name, srgb(tint)[None, None, :] * gd[..., None], soften(pn, 0.8), np.clip(rough + 0.15 * (pr - pr.mean()), 0, 1), 0.0)
    print('textures written:', len(os.listdir(OUT)))


def steel_sets():
    """The terminal's own steel (2026-11-28). Metal027's colour scan is a dark, almost black zinc (mean sRGB 0.18): as a
    full metal it reflects next to nothing and the canopy columns, roller decks and staged ULD dollies rendered black.
    Hot-dip galvanised steel reads a light grey (about sRGB 0.64): its spangle grain is kept, the shade raised to that.
    ChequerPlate: diamond tread plate (the dock levellers): raised lozenges on a 30 mm pitch, alternating +-45 degrees,
    tops worn shiny, dirt in the grooves. Own names (not Galvanised): the CargoLamps import owns those texture assets.
      python Art/CargoTerminal/make_cargo_textures.py steel"""
    G = 1024
    sp = os.path.join(GPU, 'Metal027', 'Metal027_2K-JPG_')
    c = load(sp + 'Color.jpg', G); r = load(sp + 'Roughness.jpg', G, True); n = scan_normal(sp + 'NormalGL.jpg', G)
    grain = detail(c, 0.10)[..., None]
    galv = srgb((0.62, 0.64, 0.66))[None, None, :] * grain
    rough = np.clip(0.38 + 0.6 * (r - r.mean()), 0.2, 0.7)
    write('SteelGalv', galv, n, rough, 1.0)
    # tread plate: tile 0.6 m = 20 x 20 cells of 30 mm, one lozenge per cell, the next cell turned the other way
    tm = TILE['ChequerPlate']; t = (np.arange(G) + 0.5) / G * tm
    u, v = np.meshgrid(t, t[::-1])
    pitch = 0.03
    cu, cv = (u % pitch) / pitch - 0.5, (v % pitch) / pitch - 0.5
    flip = ((np.floor(u / pitch) + np.floor(v / pitch)) % 2) * 2 - 1
    a = (cu + flip * cv) / np.sqrt(2); b = (cu - flip * cv) / np.sqrt(2)           # along / across the lozenge
    e = (a / 0.36) ** 2 + (b / 0.07) ** 2                                           # 22 x 4 mm lozenge
    h = np.clip((1.0 - e) * 3.0, 0, 1)
    h = h * 0.85 + 0.15 * np.clip(1.0 - e * 0.8, 0, 1)
    nrm = blend_normals(height_to_normal(h, G / tm, 0.0018), n)
    wear = np.clip(h * 1.2, 0, 1)[..., None]
    col = srgb((0.50, 0.52, 0.54))[None, None, :] * grain * (1 - wear) + srgb((0.70, 0.71, 0.72))[None, None, :] * grain * wear
    dirt = np.clip(1.0 - h * 2.0, 0, 1)[..., None] * 0.18
    col = col * (1 - dirt)
    rgh = np.clip(0.48 - 0.18 * h + 0.3 * (r - r.mean()), 0.15, 0.8)
    write('ChequerPlate', col, nrm, rgh, 1.0, ao=np.clip(0.8 + 0.2 * h, 0, 1))
    print('steel sets written')


if __name__ == '__main__':
    import sys
    steel_sets() if sys.argv[1:] == ['steel'] else main()
