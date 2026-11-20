"""A site's aerial photo as web-Mercator picture tiles for the globe (Chablais Airport, 2026-11-20): SWISSIMAGE 10 cm
(LV95 GeoTIFFs, Tools/swiss_tiles.py) first, IGN BD ORTHO 20 cm (Lambert-93 500 m JPEGs, Tools/ign_tiles.py) where
Switzerland ends. Out: <out>/{z}/{x}/{y}.jpg (XYZ, y from the north), 256 px.
  - the finest level (--zmax, 20 = ~10 cm here) only within --fine-km of the airport; --zall (19 = ~20 cm) everywhere;
    every coarser level made from the one under it (2 x 2 tiles into one), down to --zmin.
  python -I Tools/make_site_imagery.py --swiss D:/ADP-Terrain/Chablais/photo-10cm --france D:/ADP-Terrain/Chablais/france/photo-20cm
         --bbox 6.58 46.13 7.25 46.58 --center 6.906 46.362 --out D:/ADP-Terrain/Chablais/imagery [--resume]"""
import argparse, glob, io, math, os
from concurrent.futures import ProcessPoolExecutor
import numpy as np
import rasterio
from rasterio.transform import from_bounds as tr_from_bounds
from rasterio.warp import reproject, Resampling, transform_bounds
from PIL import Image

R = 6378137.0
META = 8          # tiles per side of a working block


def lonlat_to_tile(lon, lat, z):
    n = 1 << z
    x = (lon + 180.0) / 360.0 * n
    y = (1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * n
    return x, y


def tile_bounds_3857(z, x, y):
    n = 1 << z; s = 2 * math.pi * R / n
    x0 = -math.pi * R + x * s; y1 = math.pi * R - y * s
    return x0, y1 - s, x0 + s, y1


A = None; SRC = None


def init(a, src):
    global A, SRC
    A, SRC = a, src


def sources(a):
    sw = {}
    for f in glob.glob(os.path.join(a.swiss, '*.tif')):
        k = os.path.basename(f).split('_')[2]; e, n = (int(v) for v in k.split('-')); sw[(e, n)] = f
    fr = {}
    for f in glob.glob(os.path.join(a.france, '*.jpg')):
        x, y = (int(v) for v in os.path.basename(f)[:-4].split('_')); fr[(x, y)] = f
    return sw, fr


def meta(job):
    """One block of META x META tiles at level z: the photo warped into it, cut into tiles."""
    z, mx, my = job
    a = A
    x0, _, _, y1 = tile_bounds_3857(z, mx, my)
    _, y0, x1, _ = tile_bounds_3857(z, mx + META - 1, my + META - 1)
    px = META * 256
    dst_tr = tr_from_bounds(x0, y0, x1, y1, px, px)
    out = np.zeros((3, px, px), np.uint8); have = np.zeros((px, px), bool)
    sw, fr = SRC
    # Switzerland
    e0, n0, e1, n1 = transform_bounds('EPSG:3857', 'EPSG:2056', x0, y0, x1, y1)
    for ke in range(int(e0 // 1000), int(e1 // 1000) + 1):
        for kn in range(int(n0 // 1000), int(n1 // 1000) + 1):
            f = sw.get((ke, kn))
            if not f: continue
            try:
                with rasterio.open(f) as d:
                    tmp = np.zeros((3, px, px), np.uint8)
                    reproject(rasterio.band(d, (1, 2, 3)), tmp, dst_transform=dst_tr,
                              dst_crs='EPSG:3857', resampling=Resampling.lanczos, src_nodata=0, dst_nodata=0)
            except Exception as e:
                print('skip', f, e, flush=True); continue
            m = tmp.max(0) > 0; m &= ~have
            out[:, m] = tmp[:, m]; have |= m
    # France (where Switzerland left a gap)
    if not have.all():
        fx0, fy0, fx1, fy1 = transform_bounds('EPSG:3857', 'EPSG:2154', x0, y0, x1, y1)
        for kx in range(int(fx0 // 500) * 500, int(fx1 // 500) * 500 + 1, 500):
            for ky in range(int(fy0 // 500) * 500, int(fy1 // 500) * 500 + 1, 500):
                f = fr.get((kx, ky))
                if not f: continue
                try:
                    img = np.asarray(Image.open(f).convert('RGB')).transpose(2, 0, 1)
                except Exception as e:
                    print('skip', f, e, flush=True); continue
                tmp = np.zeros((3, px, px), np.uint8)
                reproject(img, tmp, src_transform=tr_from_bounds(kx, ky, kx + 500, ky + 500, img.shape[2], img.shape[1]), src_crs='EPSG:2154',
                          dst_transform=dst_tr, dst_crs='EPSG:3857', resampling=Resampling.lanczos, src_nodata=0, dst_nodata=0)
                m = tmp.max(0) > 0; m &= ~have
                out[:, m] = tmp[:, m]; have |= m
    n = 0
    for i in range(META):
        for j in range(META):
            t = out[:, j * 256:(j + 1) * 256, i * 256:(i + 1) * 256]
            if not have[j * 256:(j + 1) * 256, i * 256:(i + 1) * 256].any(): continue
            p = os.path.join(a.out, str(z), str(mx + i), '%d.jpg' % (my + j))
            os.makedirs(os.path.dirname(p), exist_ok=True)
            Image.fromarray(t.transpose(1, 2, 0)).save(p, quality=86, optimize=True); n += 1
    return n


def down(job):
    """One tile at level z from its four children at z + 1."""
    z, x, y = job
    a = A
    canvas = Image.new('RGB', (512, 512)); any_ = False
    for dx in (0, 1):
        for dy in (0, 1):
            p = os.path.join(a.out, str(z + 1), str(2 * x + dx), '%d.jpg' % (2 * y + dy))
            if os.path.exists(p):
                canvas.paste(Image.open(p), (dx * 256, dy * 256)); any_ = True
    if not any_: return 0
    p = os.path.join(a.out, str(z), str(x), '%d.jpg' % y)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    canvas.resize((256, 256), Image.LANCZOS).save(p, quality=86, optimize=True)
    return 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--swiss'); ap.add_argument('--france'); ap.add_argument('--out')
    ap.add_argument('--bbox', type=float, nargs=4); ap.add_argument('--center', type=float, nargs=2)
    ap.add_argument('--zall', type=int, default=19); ap.add_argument('--zmax', type=int, default=20); ap.add_argument('--zmin', type=int, default=8)
    ap.add_argument('--fine-km', type=float, default=7.0)
    ap.add_argument('--workers', type=int, default=max(1, os.cpu_count() - 2)); ap.add_argument('--resume', action='store_true')
    a = ap.parse_args()
    src = sources(a)
    print('%d Swiss photos, %d French photos' % (len(src[0]), len(src[1])), flush=True)
    lon0, lat0, lon1, lat1 = a.bbox
    with ProcessPoolExecutor(a.workers, initializer=init, initargs=(a, src)) as ex:
        for z in range(a.zmax, a.zmin - 1, -1):
            if z > a.zall:
                # the finest level only round the airport and the town
                d = a.fine_km / 111.32; c = a.center
                b = (c[0] - d / math.cos(math.radians(c[1])), c[1] - d, c[0] + d / math.cos(math.radians(c[1])), c[1] + d)
            else:
                b = a.bbox
            tx0, ty0 = lonlat_to_tile(b[0], b[3], z); tx1, ty1 = lonlat_to_tile(b[2], b[1], z)
            tx0, ty0, tx1, ty1 = int(tx0), int(ty0), int(tx1), int(ty1)
            if z >= a.zall:
                jobs = [(z, x, y) for x in range(tx0 - tx0 % META, tx1 + 1, META) for y in range(ty0 - ty0 % META, ty1 + 1, META)]
                if a.resume:
                    jobs = [j for j in jobs if not os.path.exists(os.path.join(a.out, str(z), str(j[1] + META - 1), '%d.jpg' % (j[2] + META - 1)))]
                fn = meta
            else:
                jobs = [(z, x, y) for x in range(tx0, tx1 + 1) for y in range(ty0, ty1 + 1)]
                fn = down
            print('level %d: %d jobs' % (z, len(jobs)), flush=True)
            n = 0
            for i, k in enumerate(ex.map(fn, jobs, chunksize=4 if fn is meta else 64)):
                n += k
                if i % max(1, len(jobs) // 20) == 0: print('  level %d: %d / %d jobs, %d tiles' % (z, i + 1, len(jobs), n), flush=True)
    print('done', flush=True)


if __name__ == '__main__':
    main()
