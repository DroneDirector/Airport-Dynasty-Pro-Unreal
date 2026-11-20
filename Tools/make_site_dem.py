"""One height model for a site's map area, in EPSG:4326, from the best national data (Chablais Airport, 2026-11-20):
swissALTI3D 0.5 m (LV95, Tools/swiss_tiles.py) first, IGN RGE ALTI 1 m (Lambert-93 BIL tiles, Tools/ign_tiles.py) where
Switzerland ends. Heights above sea level, like GLO-90 (Tools/MakeTerrain.py adds the geoid and uses it as a national model).
  python -I Tools/make_site_dem.py --swiss D:/ADP-Terrain/Chablais/alti-0.5m --france D:/ADP-Terrain/Chablais/france/alti-1m
         --bbox 6.58 46.13 7.25 46.58 --res 0.00001 --out D:/ADP-Terrain/Chablais/chablais_dem_4326.tif"""
import argparse, glob, os, math
from concurrent.futures import ProcessPoolExecutor
import numpy as np
import rasterio
from rasterio.transform import from_origin
from rasterio.warp import reproject, Resampling, transform_bounds

NODATA = -32767.0
A = None


def sources(a):
    sw = {}
    for f in glob.glob(os.path.join(a.swiss, '*.tif')):
        k = os.path.basename(f).split('_')[2]          # 2558-1134 (km)
        e, n = (int(v) for v in k.split('-')); sw[(e, n)] = f
    fr = {}
    for f in glob.glob(os.path.join(a.france, '*.bil')):
        x, y = (int(v) for v in os.path.basename(f)[:-4].split('_')); fr[(x // 1000, y // 1000)] = f
    return sw, fr


def block(job):
    lon0, lat1, w, h = job
    a = A
    tr = from_origin(lon0, lat1, a.res, a.res)
    lon1, lat0 = lon0 + w * a.res, lat1 - h * a.res
    out = np.full((h, w), NODATA, np.float32)
    sw, fr = SRC
    # France first, Switzerland over it (the Swiss model wins where both are)
    x0, y0, x1, y1 = transform_bounds('EPSG:4326', 'EPSG:2154', lon0, lat0, lon1, lat1)
    for kx in range(int(x0 // 1000) - 1, int(x1 // 1000) + 1):
        for ky in range(int(y0 // 1000) - 1, int(y1 // 1000) + 1):
            f = fr.get((kx, ky))
            if not f: continue
            src = np.fromfile(f, '<f4').reshape(1000, 1000)
            src = np.where(src < -500, NODATA, src).astype(np.float32)
            tmp = np.full((h, w), NODATA, np.float32)
            reproject(src, tmp, src_transform=from_origin(kx * 1000, ky * 1000 + 1000, 1, 1), src_crs='EPSG:2154', src_nodata=NODATA,
                      dst_transform=tr, dst_crs='EPSG:4326', dst_nodata=NODATA, resampling=Resampling.bilinear)
            m = tmp > -500; out[m] = tmp[m]
    e0, n0, e1, n1 = transform_bounds('EPSG:4326', 'EPSG:2056', lon0, lat0, lon1, lat1)
    for ke in range(int(e0 // 1000) - 1, int(e1 // 1000) + 1):
        for kn in range(int(n0 // 1000) - 1, int(n1 // 1000) + 1):
            f = sw.get((ke, kn))
            if not f: continue
            with rasterio.open(f) as d:
                tmp = np.full((h, w), NODATA, np.float32)
                reproject(rasterio.band(d, 1), tmp, src_nodata=d.nodata, dst_transform=tr, dst_crs='EPSG:4326', dst_nodata=NODATA,
                          resampling=Resampling.bilinear)
            m = tmp > -500; out[m] = tmp[m]
    return job, out


def block_file(job, bdir):
    """One block made and written to its own file (the VRT ties them together)."""
    (bl, bt, w, h), arr = block(job)
    c = int(round((bl - A.bbox[0]) / A.res)); r = int(round((A.bbox[3] - bt) / A.res))
    p = os.path.join(bdir, '%d_%d.tif' % (r, c))
    if os.path.exists(p): return
    prof = dict(driver='GTiff', width=w, height=h, count=1, dtype='float32', crs='EPSG:4326', nodata=NODATA,
                transform=from_origin(bl, bt, A.res, A.res), tiled=True, blockxsize=512, blockysize=512, compress='deflate', predictor=3)
    with rasterio.open(p + '.part', 'w', **prof) as o: o.write(arr, 1)
    os.replace(p + '.part', p)


def init(a, src):
    global A, SRC
    A, SRC = a, src


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--swiss'); ap.add_argument('--france'); ap.add_argument('--out')
    ap.add_argument('--bbox', type=float, nargs=4); ap.add_argument('--res', type=float, default=0.00001)
    ap.add_argument('--block', type=int, default=2048); ap.add_argument('--workers', type=int, default=max(1, os.cpu_count() - 2))
    a = ap.parse_args()
    lon0, lat0, lon1, lat1 = a.bbox
    W = int(math.ceil((lon1 - lon0) / a.res)); H = int(math.ceil((lat1 - lat0) / a.res))
    src = sources(a)
    print('%d Swiss tiles, %d French tiles -> %d x %d' % (len(src[0]), len(src[1]), W, H), flush=True)
    prof = dict(driver='GTiff', width=W, height=H, count=1, dtype='float32', crs='EPSG:4326', nodata=NODATA,
                transform=from_origin(lon0, lat1, a.res, a.res), tiled=True, blockxsize=512, blockysize=512,
                compress='deflate', predictor=3, BIGTIFF='YES', num_threads='ALL_CPUS')
    jobs = []
    for r in range(0, H, a.block):
        for c in range(0, W, a.block):
            jobs.append((lon0 + c * a.res, lat1 - r * a.res, min(a.block, W - c), min(a.block, H - r)))
    # each block its own small GeoTIFF (written by the worker that made it), tied together by a VRT index at --out
    # (2026-11-20: one process writing the whole 3-gigapixel file stalled the pool); GDAL reads the VRT as one raster
    import time
    from concurrent.futures import as_completed
    bdir = os.path.splitext(a.out)[0] + '_blocks'; os.makedirs(bdir, exist_ok=True)
    t0 = time.time()
    with ProcessPoolExecutor(a.workers, initializer=init, initargs=(a, src)) as ex:
        futs = [ex.submit(block_file, j, bdir) for j in jobs]
        for i, f in enumerate(as_completed(futs)):
            f.result()
            if i % 10 == 0: print('%d / %d blocks, %.0f s' % (i + 1, len(jobs), time.time() - t0), flush=True)
    # the VRT: every block placed at its offset
    parts = []
    for (bl, bt, w, h) in jobs:
        c = int(round((bl - lon0) / a.res)); r = int(round((lat1 - bt) / a.res))
        fn = os.path.join(bdir, '%d_%d.tif' % (r, c)).replace(os.sep, '/')
        parts.append('<ComplexSource><SourceFilename relativeToVRT="0">%s</SourceFilename><SourceBand>1</SourceBand>'
                     '<SrcRect xOff="0" yOff="0" xSize="%d" ySize="%d"/><DstRect xOff="%d" yOff="%d" xSize="%d" ySize="%d"/>'
                     '<NODATA>%g</NODATA></ComplexSource>' % (fn, w, h, c, r, w, h, NODATA))
    vrt = ('<VRTDataset rasterXSize="%d" rasterYSize="%d"><SRS>EPSG:4326</SRS><GeoTransform>%.10f, %.10f, 0, %.10f, 0, %.10f</GeoTransform>'
           '<VRTRasterBand dataType="Float32" band="1"><NoDataValue>%g</NoDataValue>%s</VRTRasterBand></VRTDataset>'
           % (W, H, lon0, a.res, lat1, -a.res, NODATA, ''.join(parts)))
    open(a.out, 'w').write(vrt)
    print('done', a.out, flush=True)


if __name__ == '__main__':
    main()
