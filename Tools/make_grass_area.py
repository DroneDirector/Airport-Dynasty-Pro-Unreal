# Where the grass grows outside the airfield (owner, 2026-11-15: "cover the custom ground ... by grass, not the road or
# the curb ... and the normal globe around the airport, several kilometres ... the exact same grass ... at least 30 m
# away from the ocean, and also if there's a big water"). Two things, from one map:
#  - Data/grass-area.png + .json: a map in plan metres (5 m a pixel) of where the airfield's grass may grow on the
#    globe and on our own land: a ring RING m out round the airfield, and all our land along the roads and the town
#    (its outline from Data/land-corridor.json, and MARGIN m round it) - never within WATER_GAP m of the sea, a lake or
#    a river (the land cover's water). AGrassField reads it (GrassField.cpp): the airfield's own grass, near you.
#  - Content/Data/grass-tiles/{z}/{x}/{y}.png: the globe's grass colour over the same area (Globe.cpp: an overlay on
#    the satellite picture, Cesium's geographic tiles, y counted from the south) - the airfield ground grass's own
#    colour (its scan Art/Ground/Scans/grass_ground, tinted as MI_ggrass: Tools/set_ground.py), a gentle variation in
#    it, fading into the satellite picture over FADE m at its edge, clear over water. From afar the land reads as the
#    same grass where the blades stop.
#   python Tools/make_grass_area.py
import json, math, shutil, sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'Tools'))
import make_land_corridor as LC

RING = 5000.0        # m round the airfield's boundary
MARGIN = 300.0       # m round our own land (its band along the roads, the town)
WATER_GAP = 30.0     # m kept clear of water
FADE = 250.0         # m the globe's grass colour fades into the satellite picture
STEP = 5.0           # m a pixel of the map
TINT = (0.66, 1.16, 0.50)   # MI_ggrass's Color (Tools/set_ground.py)
ZMIN, ZMAX = 0, 5    # the colour tiles' levels, one root over the area (5: about 2 m a pixel)


def main():
    F, H, O, E, N, U = LC.georef()
    B = F['boundary']
    cor = json.loads((ROOT / 'Data' / 'land-corridor.json').read_text(encoding='utf-8'))
    outline = [np.array(r, float) for r in cor['outline']]
    allx = np.concatenate([o[:, 0] for o in outline] + [np.array([B['x0'] - RING, B['x1'] + RING])])
    ally = np.concatenate([o[:, 1] for o in outline] + [np.array([B['y0'] - RING, B['y1'] + RING])])
    x0, x1 = allx.min() - MARGIN - FADE - 50, allx.max() + MARGIN + FADE + 50
    y0, y1 = ally.min() - MARGIN - FADE - 50, ally.max() + MARGIN + FADE + 50
    nx, ny = int((x1 - x0) / STEP) + 1, int((y1 - y0) / STEP) + 1
    print('grass map %d x %d (%.1f x %.1f km)' % (nx, ny, (x1 - x0) / 1000, (y1 - y0) / 1000))
    X = x0 + (np.arange(nx) + 0.5) * STEP; Y = y0 + (np.arange(ny) + 0.5) * STEP
    # the ring: distance outside the airfield's box, up to RING (inside the box: the airfield's own grass, not ours)
    dx = np.maximum(np.maximum(B['x0'] - X, X - B['x1']), 0)[None, :]; dy = np.maximum(np.maximum(B['y0'] - Y, Y - B['y1']), 0)[:, None]
    d_air = np.hypot(dx, dy)
    inside_air = (d_air == 0)
    # our land: its outline filled, then MARGIN round it
    from PIL import ImageDraw
    im = Image.new('L', (nx, ny), 0); dr = ImageDraw.Draw(im)
    for o in outline: dr.polygon([((px - x0) / STEP, (py - y0) / STEP) for px, py in o], fill=255)
    land = np.asarray(im) > 0
    GX, GY = np.meshgrid(X, Y)
    d_land = ndimage.distance_transform_edt(~land) * STEP
    # the area, and its distance to the edge (for the colour's fade)
    area = ((d_air <= RING) & ~inside_air) | (d_land <= MARGIN)
    # water, from the land cover: the sea and lakes and rivers; 30 m kept clear
    lon, lat = LC.plan_to_lonlat(GX.ravel(), GY.ravel(), H, O, E, N)
    rgb = LC.landcover_rgb(lon, lat).reshape(ny, nx, 3)
    # (the game's land-cover palette draws water dark blue, about (28, 52, 78): Tools/MakeLandCover.py)
    water = (np.abs(rgb[..., 0] - 28) < 22) & (np.abs(rgb[..., 1] - 52) < 22) & (np.abs(rgb[..., 2] - 78) < 26) & (rgb[..., 2] > rgb[..., 0] + 25)
    water = ndimage.binary_opening(water, iterations=1)       # (single specks are the picture's noise, not water)
    d_water = ndimage.distance_transform_edt(~water) * STEP
    grass = area & (d_water >= WATER_GAP)
    print('grass %.1f km2, water near it %.2f km2' % (grass.sum() * STEP * STEP / 1e6, (water & (d_land <= MARGIN + 500)).sum() * STEP * STEP / 1e6))
    # the map for the blades (row 0 = y0, the plan's south... as the json says: pixel (i, j) is at x0 + i*step, y0 + j*step)
    Image.fromarray((grass * 255).astype(np.uint8)).save(ROOT / 'Data' / 'grass-area.png', optimize=True)
    (ROOT / 'Data' / 'grass-area.json').write_text(json.dumps({'source': 'Tools/make_grass_area.py', 'x0': x0, 'y0': y0, 'step': STEP, 'nx': nx, 'ny': ny,
        'ring': RING, 'margin': MARGIN, 'waterGap': WATER_GAP}, indent=1), encoding='utf-8')
    for f in ('grass-area.png', 'grass-area.json'): shutil.copyfile(ROOT / 'Data' / f, ROOT / 'Content' / 'Data' / f)
    # the colour's strength: full on the grass, fading out over FADE m beyond it; nothing over water (with its gap)
    d_out = ndimage.distance_transform_edt(~grass) * STEP
    alpha = np.clip(1.0 - d_out / FADE, 0, 1); alpha = alpha * alpha * (3 - 2 * alpha)
    alpha[d_water < WATER_GAP] = np.clip((d_water[d_water < WATER_GAP] - WATER_GAP * 0.4) / (WATER_GAP * 0.6), 0, 1) * alpha[d_water < WATER_GAP]
    # (inside the airfield too: where the land rises above the airfield's own ground, it shows - green, not black sand)
    alpha[inside_air] = 1.0
    # the colour: the airfield grass scan's own mean colour, tinted as MI_ggrass
    scan = np.asarray(Image.open(ROOT / 'Art' / 'Ground' / 'Scans' / 'grass_ground' / 'grass_ground_diff_4k.jpg').convert('RGB').resize((256, 256)), np.float32) / 255.0
    lin = np.where(scan <= 0.04045, scan / 12.92, ((scan + 0.055) / 1.055) ** 2.4).mean((0, 1)) * np.array(TINT)
    srgb = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(np.clip(lin, 0, None), 1 / 2.4) - 0.055)
    print('grass colour (sRGB)', np.round(srgb * 255).astype(int))
    # tiles: Cesium geographic (2 x 1 roots), y from the south
    out = ROOT / 'Content' / 'Data' / 'grass-tiles'
    if out.exists(): shutil.rmtree(out)
    corners_lon, corners_lat = LC.plan_to_lonlat(np.array([x0, x1, x0, x1]), np.array([y0, y0, y1, y1]), H, O, E, N)
    W_, E_, S_, N_ = corners_lon.min(), corners_lon.max(), corners_lat.min(), corners_lat.max()
    He = math.radians(F['airport']['hdg'])
    rng = np.random.default_rng(3)
    noise_big = ndimage.gaussian_filter(rng.standard_normal((ny // 8 + 2, nx // 8 + 2)), 6)
    noise_big /= max(1e-6, noise_big.std())
    # (Cesium numbers the tiles inside the rectangle it is given: one root tile over the whole area, every tile of every
    # level written - a missing one stops the layer - 2026-11-15)
    count = 0
    for z in range(0, ZMAX + 1):
        n_ = 1 << z
        sx, sy = (E_ - W_) / n_, (N_ - S_) / n_
        for tx in range(n_):
            for ty in range(n_):
                lo0, la0 = W_ + tx * sx, S_ + ty * sy
                u = (np.arange(256) + 0.5) / 256
                LON, LAT = np.meshgrid(lo0 + u * sx, la0 + (1 - u) * sy)            # (rows north first)
                lo, la = np.radians(LON.ravel()), np.radians(LAT.ravel())
                nn = LC.rg.A_ / np.sqrt(1 - LC.rg.E2 * np.sin(la) ** 2)
                P = np.stack([nn * np.cos(la) * np.cos(lo), nn * np.cos(la) * np.sin(lo), nn * (1 - LC.rg.E2) * np.sin(la)], 1) - O[None, :]
                e = P @ E; n = P @ N
                PX = e * math.sin(He) + n * math.cos(He); PY = -e * math.cos(He) + n * math.sin(He)
                ix = ((PX - x0) / STEP).astype(int); iy = ((PY - y0) / STEP).astype(int)
                ok = (ix >= 0) & (ix < nx) & (iy >= 0) & (iy < ny)
                a = np.zeros(len(ix)); a[ok] = alpha[iy[ok], ix[ok]]
                v = np.zeros(len(ix))
                if ok.any(): v[ok] = ndimage.map_coordinates(noise_big, [(PY[ok] - y0) / STEP / 8.0, (PX[ok] - x0) / STEP / 8.0], order=1, mode='nearest')   # (smooth: no squares)
                col = np.clip(srgb[None, :] * (1.0 + 0.06 * v[:, None]), 0, 1)
                img = np.concatenate([col * 255, a[:, None] * 255], 1).reshape(256, 256, 4).astype(np.uint8)
                p = out / str(z) / str(tx); p.mkdir(parents=True, exist_ok=True)
                Image.fromarray(img, 'RGBA').save(p / ('%d.png' % ty), optimize=True)
                count += 1
        print('  level %d done (%d tiles so far)' % (z, count))
    (out / 'tiles.json').write_text(json.dumps({'west': W_, 'south': S_, 'east': E_, 'north': N_, 'minLevel': ZMIN, 'maxLevel': ZMAX}, indent=1), encoding='utf-8')
    print('grass colour tiles: %d, %.1f MB' % (count, sum(f.stat().st_size for f in out.rglob('*.png')) / 1e6))


if __name__ == '__main__':
    main()
