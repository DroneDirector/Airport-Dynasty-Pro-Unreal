# The land cover on the globe: ESA WorldCover 2021 10 m (CC BY 4.0, (c) ESA WorldCover project / contains modified
# Copernicus Sentinel data 2021) turned into picture tiles for Cesium, in natural colours (forest dark green, grass,
# farmland, towns grey, bare rock and sand, snow and ice, water), each pixel the average of the classes under it.
#   world:  levels 0-8 of the geographic TMS layout (2 x 1 root tiles; level 8 = about 300 m a pixel)
#   region: the North Atlantic (the Faroe Islands and Iceland) to about 19 m a pixel, in its own tiling scheme
#           (4 x 2 root tiles over REGION; its level r is the world's level r + 5)
# Tiles are JPEGs at <out>/<world|region>/<z>/<x>/<y>.jpg with y counted from the south (Cesium's {reverseY}).
# Prints a line every 500 MB of tiles written, with the file being worked on.
#   python Tools/MakeLandCover.py [--src F:/ADP-Terrain/worldcover] [--out F:/ADP-Terrain/landcover] [--workers 8]
import argparse, io, os, re, sys, time
from pathlib import Path
import numpy as np

REGION = (-28.125, 56.25, -5.625, 67.5)        # lon0, lat0, lon1, lat1: 4 x 2 world level-5 tiles
REGION_ROOT = (4, 2)
WORLD_MAX, REGION_MAX = 8, 7                    # region level 7 = world level 12

# class -> colour (natural, close to how it looks from the air)
LUT = np.zeros((256, 3), np.float32)
LUT[:] = (28, 52, 78)                           # no data: the sea
for c, rgb in {10: (40, 66, 34), 20: (104, 100, 62), 30: (118, 132, 72), 40: (152, 146, 96), 50: (132, 128, 122),
               60: (176, 164, 142), 70: (236, 240, 245), 80: (28, 52, 78), 90: (84, 104, 74), 95: (38, 72, 50),
               100: (122, 118, 86)}.items():
    LUT[c] = rgb
SNOW = np.array((236, 240, 245), np.float32)

def build_vrt(src, vrt):
    """One virtual mosaic of every WorldCover tile (3 x 3 degrees, 36000 px each), so any window reads across them."""
    px = 12000
    W, S, E, N = -180, -60, 180, 84
    items = []
    for f in sorted(src.glob('*_Map.tif')):
        m = re.search(r'_([NS])(\d\d)([EW])(\d\d\d)_Map', f.name)
        lat = int(m[2]) * (1 if m[1] == 'N' else -1); lon = int(m[4]) * (1 if m[3] == 'E' else -1)
        xo, yo = (lon - W) * px, (N - (lat + 3)) * px
        items.append(f'<SimpleSource><SourceFilename relativeToVRT="0">{f.as_posix()}</SourceFilename><SourceBand>1</SourceBand>'
                     f'<SrcRect xOff="0" yOff="0" xSize="36000" ySize="36000"/><DstRect xOff="{xo}" yOff="{yo}" xSize="36000" ySize="36000"/></SimpleSource>')
    vrt.write_text(f'<VRTDataset rasterXSize="{(E - W) * px}" rasterYSize="{(N - S) * px}"><SRS>EPSG:4326</SRS>'
                   f'<GeoTransform>{W}, {1 / px!r}, 0, {N}, 0, {-1 / px!r}</GeoTransform>'
                   f'<VRTRasterBand dataType="Byte" band="1"><NoDataValue>0</NoDataValue>{"".join(items)}</VRTRasterBand></VRTDataset>')
    return len(items)

_ds = None
def window_rgb(bounds, size, over):
    """The land cover over bounds (lon0, lat0, lon1, lat1) as size x size RGB, each pixel averaged from over x over samples."""
    global _ds
    import rasterio
    from rasterio.windows import from_bounds
    if _ds is None: _ds = rasterio.open(VRT)
    lon0, lat0, lon1, lat1 = bounds
    n = size * over
    cls = np.zeros((n, n), np.uint8)
    if lat1 > -60 and lat0 < 84:
        win = from_bounds(lon0, lat0, lon1, lat1, _ds.transform)
        cls = _ds.read(1, window=win, out_shape=(n, n), boundless=True, fill_value=0)
    rgb = LUT[cls]
    lats = lat1 - (np.arange(n) + 0.5) * (lat1 - lat0) / n
    rgb[(cls == 0) & (lats < -60)[:, None]] = SNOW         # Antarctica (not in WorldCover): ice
    rgb = rgb.reshape(size, over, size, over, 3).mean(axis=(1, 3))
    return rgb, bool((cls != 0).any() and (cls != 80).any())

def save(img, path):
    from PIL import Image
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(path, 'JPEG', quality=86, optimize=True)
    return path.stat().st_size

def world_bounds(z, x, y):
    d = 180.0 / 2 ** z
    return (-180 + x * d, -90 + y * d, -180 + (x + 1) * d, -90 + (y + 1) * d)

def region_bounds(z, x, y):
    dx = (REGION[2] - REGION[0]) / REGION_ROOT[0] / 2 ** z; dy = (REGION[3] - REGION[1]) / REGION_ROOT[1] / 2 ** z
    return (REGION[0] + x * dx, REGION[1] + y * dy, REGION[0] + (x + 1) * dx, REGION[1] + (y + 1) * dy)

def leaf_column(job):
    """One column of max-level tiles (read from the mosaic); returns (bytes written, last file)."""
    kind, z, x, ny, over, out = job
    total, last = 0, ''
    for y in range(ny):
        b = world_bounds(z, x, y) if kind == 'world' else region_bounds(z, x, y)
        img, _ = window_rgb(b, 256, over)
        p = Path(out) / kind / str(z) / str(x) / f'{y}.jpg'
        total += save(img, p); last = f'{kind}/{z}/{x}/{y}.jpg'
    return total, last

def parent_column(job):
    """One column of a coarser level, each tile the four below it averaged down."""
    from PIL import Image
    kind, z, x, ny, out = job
    total, last = 0, ''
    for y in range(ny):
        big = np.zeros((512, 512, 3), np.float32)
        for dx in (0, 1):
            for dy in (0, 1):
                q = Path(out) / kind / str(z + 1) / str(2 * x + dx) / f'{2 * y + dy}.jpg'
                big[(1 - dy) * 256:(2 - dy) * 256, dx * 256:(dx + 1) * 256] = np.asarray(Image.open(q), np.float32)
        img = big.reshape(256, 2, 256, 2, 3).mean(axis=(1, 3))
        total += save(img, Path(out) / kind / str(z) / str(x) / f'{y}.jpg'); last = f'{kind}/{z}/{x}/{y}.jpg'
    return total, last

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default='F:/ADP-Terrain/worldcover')
    ap.add_argument('--out', default='F:/ADP-Terrain/landcover')
    ap.add_argument('--workers', type=int, default=8)
    a = ap.parse_args()
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    vrt = out / 'worldcover.vrt'
    print('mosaic of', build_vrt(Path(a.src), vrt), 'WorldCover tiles', flush=True)
    os.environ['ADP_VRT'] = str(vrt)
    from concurrent.futures import ProcessPoolExecutor
    STEP = 500 * 1024 * 1024; done = 0; mark = STEP; t0 = time.time()
    def run(fn, jobs):
        global done, mark
        with ProcessPoolExecutor(max_workers=a.workers) as ex:
            for size, last in ex.map(fn, jobs):
                done += size
                while done >= mark:
                    print('%5.2f GB written  (%.0f s)  file %s' % (done / 1e9, time.time() - t0, last), flush=True); mark += STEP
    # world: level 8 read (4 x 4 samples a pixel), then 7..0 averaged down
    run(leaf_column, [('world', WORLD_MAX, x, 2 ** WORLD_MAX, 4, str(out)) for x in range(2 ** (WORLD_MAX + 1))])
    for z in range(WORLD_MAX - 1, -1, -1):
        run(parent_column, [('world', z, x, 2 ** z, str(out)) for x in range(2 ** (z + 1))])
        print('world level', z, 'done', flush=True)
    # the region: its level 7 read (2 x 2 samples a pixel: about the data's own 10 m), then down to 0
    run(leaf_column, [('region', REGION_MAX, x, REGION_ROOT[1] * 2 ** REGION_MAX, 2, str(out)) for x in range(REGION_ROOT[0] * 2 ** REGION_MAX)])
    for z in range(REGION_MAX - 1, -1, -1):
        run(parent_column, [('region', z, x, REGION_ROOT[1] * 2 ** z, str(out)) for x in range(REGION_ROOT[0] * 2 ** z)])
        print('region level', z, 'done', flush=True)
    print('done: %.2f GB, %.0f s' % (done / 1e9, time.time() - t0), flush=True)

VRT = os.environ.get('ADP_VRT', 'F:/ADP-Terrain/landcover/worldcover.vrt')
