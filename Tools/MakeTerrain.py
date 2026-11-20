# The PC version's terrain, made once from the raw height data (Tools/GetTerrain.ps1) into a quantized-mesh terrain
# tileset that ships with the game (Cesium for Unreal reads it from disk: no streaming, no service).
#  - World: Copernicus DEM GLO-90 (1° tiles). Heights above sea level (EGM2008) are turned into heights above the
#    WGS84 ellipsoid with the EGM96 geoid grid (us_nga_egm96_15.tif, public domain), as Cesium needs.
#  - Tiles: Cesium's geographic TMS layout (level 0 is two tiles), every tile to level 6 (oceans flat at sea level),
#    deeper only where there is land. Each tile is a 2^n + 1 grid of heights turned into an adaptive triangle mesh
#    (Martini) with an error under a quarter of the grid spacing, then quantized-mesh with vertex normals and a water
#    mask (the sea, where the data has no land), gzipped.
#  - The airport: flat within 2.6 km of its origin, blending into the land by 4.4 km (the height used is written to
#    Data/terrain.json so the game puts its ground exactly there).
#  - layer.json lists which tiles exist on each level, so the globe never asks for tiles that aren't there.
# Credit (in the game): "Copernicus DEM GLO-90 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018
# provided under COPERNICUS by the European Union and ESA; all rights reserved."
#  - Near the airport the tiles are built 5 times more exactly (owner, 2026-11-14: the road to town showed 4-38 m steps -
#    each level's triangles were allowed an error of a quarter of its grid spacing, so the ground a few km off was that
#    far wrong): within the PRECISE box (about 25 km round the airport, the road and the town) from level 9 down, an error
#    of 1/20 of the spacing.
#  - Stopping and going on (owner: only what isn't done yet is remade): a run writes a job stamp in its output folder; run
#    the same command again with --resume and every tile made since that stamp is kept, the rest made; the stamp goes
#    when the run finishes.
#  - When the tiles near the airport change, Tools/road_ground.py is run at the end: the roads take their ground from them.
#   python Tools/MakeTerrain.py [--maxz 10] [--bbox lon0 lat0 lon1 lat1] [--minz 9] [--resume] [--workers 8]
#   the airport's precise box only:  python Tools/MakeTerrain.py --out F:/ADP-Terrain/tiles-v2 --bbox -19.2 63.25 -18.25 63.65 --minz 9 --resume
import argparse
import gzip
import io
import json
import math
import os
import re
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
import rasterio
from rasterio.windows import from_bounds
from pymartini import Martini, rescale_positions
import quantized_mesh_encoder as qme
from quantized_mesh_encoder.extensions import VertexNormalsExtension, WaterMaskExtension

ROOT = Path(__file__).resolve().parent.parent
SRC = Path('F:/ADP-Terrain/glo90')
GEOID = Path('F:/ADP-Terrain/geoid/us_nga_egm96_15.tif')
OUT = Path(os.environ.get('ADP_TERRAIN_OUT', 'F:/ADP-Terrain/tiles'))   # (--out; the workers read it from the environment)
AIRPORT = (-18.710348, 63.452335)   # lon, lat: the terminal's origin (Globe.cpp)
FLAT_IN, FLAT_OUT = 2600.0, 4400.0  # metres
FULL_TO = 6                          # every tile (land or sea) down to this level
FEATHER = 12.0                       # grid cells over which a national model fades into GLO-90 at its edges
PRECISE = (-19.2, 63.25, -18.25, 63.65)   # lon0, lat0, lon1, lat1: the airport, the road to town and the town, ~25 km round
PRECISE_FROM, PRECISE_ERR, ERR = 9, 0.05, 0.25   # (the triangles' error, as a share of the grid spacing)

#region Reading heights
CELLS = {}
for f in SRC.glob('*_DEM.tif'):
    m = re.search(r'_([NS])(\d\d)_00_([EW])(\d\d\d)_00_DEM', f.name)
    if m:
        lat = int(m.group(2)) * (1 if m.group(1) == 'N' else -1)
        lon = int(m.group(4)) * (1 if m.group(3) == 'E' else -1)
        CELLS[(lat, lon)] = f

_open = {}
def ds(path):
    d = _open.get(path)
    if d is None:
        if len(_open) > 64:
            for k in list(_open)[:32]: _open.pop(k).close()
        d = _open[path] = rasterio.open(path)
    return d

_geoid = None
def geoid(lons, lats):
    global _geoid
    if _geoid is None:
        g = rasterio.open(GEOID); _geoid = (g.read(1).astype(np.float32), g.transform)
    a, t = _geoid
    col = np.clip(((lons - t.c) / t.a).round().astype(int), 0, a.shape[1] - 1)
    row = np.clip(((lats - t.f) / t.e).round().astype(int), 0, a.shape[0] - 1)
    return a[row, col]

# the whole world at 1/40 of a degree (about 2.8 km), made once from every cell's overview: the coarse tiles (each
# covering degrees of the Earth) are drawn from it instead of reading thousands of files each
MOSAIC = Path('F:/ADP-Terrain/world-40.npy')
MRES = 40
_mosaic = None
def build_mosaic():
    if MOSAIC.exists(): return
    m = np.zeros((180 * MRES, 360 * MRES), np.float32)
    for i, ((la, lo), p) in enumerate(CELLS.items()):
        try:
            with rasterio.open(p) as d:
                a = d.read(1, out_shape=(MRES, MRES), resampling=rasterio.enums.Resampling.average).astype(np.float32)
                if d.nodata is not None: a[a == d.nodata] = 0
        except Exception as e:
            print('skipped %s: %s' % (p.name, e), flush=True); continue
        r0 = (89 - la) * MRES; c0 = (lo + 180) * MRES
        m[r0:r0 + MRES, c0:c0 + MRES] = a
        if i % 2000 == 0: print('world overview %d of %d' % (i, len(CELLS)), flush=True)
    np.save(MOSAIC, m)

def mosaic_heights(lons, lats):
    global _mosaic
    if _mosaic is None: _mosaic = np.load(MOSAIC, mmap_mode='r')
    fy = (90.0 - lats) * MRES - 0.5; fx = (lons + 180.0) * MRES - 0.5
    y0 = np.clip(np.floor(fy).astype(int), 0, 180 * MRES - 2); x0 = np.clip(np.floor(fx).astype(int), 0, 360 * MRES - 2)
    ty = np.clip(fy - y0, 0, 1)[:, None]; tx = np.clip(fx - x0, 0, 1)[None, :]
    a = np.asarray(_mosaic[y0.min():y0.max() + 2, x0.min():x0.max() + 2]); y0 -= y0.min(); x0 -= x0.min()
    return ((a[y0][:, x0] * (1 - tx) + a[y0][:, x0 + 1] * tx) * (1 - ty) + (a[y0 + 1][:, x0] * (1 - tx) + a[y0 + 1][:, x0 + 1] * tx) * ty).astype(np.float32)

def heights(lon0, lat0, lon1, lat1, n, flatten=True):
    """An n x n grid of ellipsoid heights over the box (rows north to south), sea level where there is no land."""
    lons = np.linspace(lon0, lon1, n); lats = np.linspace(lat1, lat0, n)
    out = np.zeros((n, n), np.float32)
    coarse = (lat1 - lat0) / (n - 1) > 1.5 / MRES
    if coarse: out = mosaic_heights(lons, lats)
    # each 1° cell the box touches: read just the part needed, at about the resolution needed
    for la in (range(math.floor(lat0), math.ceil(lat1)) if not coarse else []):
        for lo in range(math.floor(lon0), math.ceil(lon1)):
            p = CELLS.get((la, lo))
            if p is None: continue
            ri = np.where((lats >= la) & (lats <= la + 1))[0]; ci = np.where((lons >= lo) & (lons <= lo + 1))[0]
            if not len(ri) or not len(ci): continue
            try:
                d = ds(p)
                # only the part of the cell this tile needs, at about the resolution needed
                bl, br = lons[ci].min(), lons[ci].max(); bb, bt = lats[ri].min(), lats[ri].max()
                px = abs(d.transform.a); py = abs(d.transform.e)
                bl, br, bb, bt = max(d.bounds.left, bl - px), min(d.bounds.right, br + px), max(d.bounds.bottom, bb - py), min(d.bounds.top, bt + py)
                win = from_bounds(bl, bb, br, bt, d.transform)
                shape = (max(2, min(int(round(win.height)) + 1, len(ri) * 2 + 1)), max(2, min(int(round(win.width)) + 1, len(ci) * 2 + 1)))
                a = d.read(1, window=win, out_shape=shape, resampling=rasterio.enums.Resampling.bilinear).astype(np.float32)
            except Exception as e:
                print('skipped %s: %s' % (p.name, e), flush=True)
                continue
            nod = d.nodata
            if nod is not None: a[a == nod] = 0
            fy = (bt - lats[ri]) / max(1e-12, bt - bb) * (shape[0] - 1)
            fx = (lons[ci] - bl) / max(1e-12, br - bl) * (shape[1] - 1)
            y0 = np.clip(np.floor(fy).astype(int), 0, shape[0] - 2); x0 = np.clip(np.floor(fx).astype(int), 0, shape[1] - 2)
            ty = np.clip(fy - y0, 0, 1)[:, None]; tx = np.clip(fx - x0, 0, 1)[None, :]
            v = (a[y0][:, x0] * (1 - tx) + a[y0][:, x0 + 1] * tx) * (1 - ty) + (a[y0 + 1][:, x0] * (1 - tx) + a[y0 + 1][:, x0 + 1] * tx) * ty
            out[np.ix_(ri, ci)] = v
    if not coarse: out = national_over(lons, lats, out)
    LON, LAT = np.meshgrid(lons, lats)
    land = out != 0
    out = out + geoid(LON, LAT)   # sea level too lies on the geoid
    # the airport's ground, flat, blending out
    dx = (LON - AIRPORT[0]) * 111320.0 * math.cos(math.radians(AIRPORT[1])); dy = (LAT - AIRPORT[1]) * 110574.0
    dist = np.hypot(dx, dy)
    if flatten and dist.min() < FLAT_OUT:
        k = np.clip((dist - FLAT_IN) / (FLAT_OUT - FLAT_IN), 0, 1); k = k * k * (3 - 2 * k)
        out = AIRPORT_H + (out - AIRPORT_H) * k
    if flatten:
        for s in SITES:
            if 'h' not in s: continue
            k = site_blend(s, LON, LAT)
            if k is not None: out = s['h'] + (out - s['h']) * k
    return out.astype(np.float32), land
#endregion

#region Other sites (Site.h): each airport flat inside its fence (plus a margin), blending into the land over blendM
SITES = []
def load_sites():
    af = json.loads((ROOT / 'Data' / 'airfield.json').read_text(encoding='utf-8'))
    b = af['boundary']; mx = (af['runway']['x0'] + af['runway']['x1']) / 2; my = af['runway']['y']
    out = []
    for f in sorted((ROOT / 'Content' / 'Data' / 'Sites').glob('*/site.json')):
        j = json.loads(f.read_text(encoding='utf-8')); ap = j['airport']; fl = j.get('flatten', {})
        out.append({'id': f.parent.name, 'dir': str(f.parent), 'lat': ap['lat'], 'lon': ap['lon'], 'hdg': ap['hdg'], 'mx': mx, 'my': my,
                    'box': (b['x0'], b['y0'], b['x1'], b['y1']), 'margin': fl.get('marginM', 120.0), 'blend': fl.get('blendM', 450.0)})
    return out

def site_plan(s, LON, LAT):
    """The plan's x, y (metres) of each point (the airfield's own axes, Globe.cpp)."""
    e = (LON - s['lon']) * 111320.0 * math.cos(math.radians(s['lat'])); n = (LAT - s['lat']) * 111320.0
    H = math.radians(s['hdg'])
    # e = dx sinH + dy sin(H-90), n = dx cosH + dy cos(H-90)  ->  dx = e sinH + n cosH, dy = e sin(H-90) + n cos(H-90)
    dx = e * math.sin(H) + n * math.cos(H); dy = e * math.sin(H - math.pi / 2) + n * math.cos(H - math.pi / 2)
    return dx + s['mx'], dy + s['my']

def site_blend(s, LON, LAT):
    x, y = site_plan(s, LON, LAT)
    x0, y0, x1, y1 = s['box']; m = s['margin']
    ox = np.maximum(np.maximum(x0 - m - x, x - x1 - m), 0); oy = np.maximum(np.maximum(y0 - m - y, y - y1 - m), 0)
    d = np.hypot(ox, oy)
    if d.min() >= s['blend']: return None
    k = np.clip(d / s['blend'], 0, 1)
    return k * k * (3 - 2 * k)

def site_height(s):
    """The site's airport level: the median ground inside its fence (ellipsoid metres)."""
    x0, y0, x1, y1 = s['box']; pts = []
    lat0 = s['lat']
    for x in np.linspace(x0, x1, 40):
        for y in np.linspace(y0, y1, 14):
            dx, dy = x - s['mx'], y - s['my']; H = math.radians(s['hdg'])
            e = dx * math.sin(H) + dy * math.sin(H - math.pi / 2); n = dx * math.cos(H) + dy * math.cos(H - math.pi / 2)
            la = lat0 + n / 111320.0; lo = s['lon'] + e / (111320.0 * math.cos(math.radians(lat0)))
            d = 0.00002
            hh, _ = heights(lo - d, la - d, lo + d, la + d, 3, flatten=False)
            pts.append(float(hh[1, 1]))
    return float(np.median(pts))
#endregion

#region National models (sharper than GLO-90 where they exist)
# Faroe Islands: Umhvørvisstovan's DSM hæddarmodell 2015, 10 m (FOTM, EPSG:5316), from foroyakort.fo; credit "Data from
# the Faroese Environment Agency, Umhvørvisstovan: DSM hæddarmodell 2015, November 2026". Heights above sea level, like
# GLO-90 (the geoid is added the same way).
NATIONAL = [
    (Path('F:/ADP-Terrain/faroes/FO_DSM_2015_FOTM_10M.tif'), Path('F:/ADP-Terrain/faroes/fo_dsm_4326.tif'), 0.0002, 13),
    # Iceland: ÍslandsDEM v1.0, 10 m (ISN93 / Lambert, EPSG:3057), Landmælingar Íslands, CC BY 4.0; credit "ÍslandsDEM v1.0
    # © Landmælingar Íslands (National Land Survey of Iceland), CC BY 4.0". Heights above sea level.
    (Path('F:/ADP-Terrain/iceland/IslandsDEMv1.0_10x10m_isn93_zmasl.tif'), Path('F:/ADP-Terrain/iceland/iceland_4326.tif'), 0.00025, 13),
    # Chablais Airport (Switzerland, 2026-11-20): swissALTI3D 0.5 m + IGN RGE ALTI 1 m on the French side, merged into one
    # model at 0.00001 deg (~1 m) by Tools/make_site_dem.py (made directly in EPSG:4326: no source to reproject here);
    # credit "© swisstopo, © IGN". Down to level 17 (~1.2 m between the mesh's points).
    (Path('-'), Path('D:/ADP-Terrain/Chablais/chablais_dem_4326.tif'), 0.00001, 17),
]

def prepare_national():
    from rasterio.warp import calculate_default_transform, reproject, Resampling
    for src, dst, res, _ in NATIONAL:
        if str(src) == '-' or not src.exists() or dst.exists(): continue
        with rasterio.open(src) as s:
            tr, w, h = calculate_default_transform(s.crs, 'EPSG:4326', s.width, s.height, *s.bounds, resolution=res)
            prof = s.profile.copy(); prof.update(crs='EPSG:4326', transform=tr, width=w, height=h, nodata=-32767.0, compress='deflate', tiled=True)
            with rasterio.open(dst, 'w', **prof) as o:
                reproject(rasterio.band(s, 1), rasterio.band(o, 1), src_transform=s.transform, src_crs=s.crs, dst_transform=tr, dst_crs='EPSG:4326', resampling=Resampling.bilinear, src_nodata=s.nodata, dst_nodata=-32767.0)
        print('prepared', dst.name, flush=True)

def national_over(lons, lats, out):
    """Where a national model covers the grid, its heights replace GLO-90's (before the geoid is added)."""
    for _, p, _, _ in NATIONAL:
        if not p.exists(): continue
        d = ds(p); b = d.bounds
        if lons.max() < b.left or lons.min() > b.right or lats.max() < b.bottom or lats.min() > b.top: continue
        ri = np.where((lats >= b.bottom) & (lats <= b.top))[0]; ci = np.where((lons >= b.left) & (lons <= b.right))[0]
        bl, br, bb, bt = lons[ci].min(), lons[ci].max(), lats[ri].min(), lats[ri].max()
        px, py = abs(d.transform.a), abs(d.transform.e)
        bl, br, bb, bt = max(b.left, bl - px), min(b.right, br + px), max(b.bottom, bb - py), min(b.top, bt + py)
        win = from_bounds(bl, bb, br, bt, d.transform)
        shape = (max(2, min(int(round(win.height)) + 1, len(ri) * 2 + 1)), max(2, min(int(round(win.width)) + 1, len(ci) * 2 + 1)))
        a = d.read(1, window=win, out_shape=shape, resampling=rasterio.enums.Resampling.bilinear).astype(np.float32)
        fy = (bt - lats[ri]) / max(1e-12, bt - bb) * (shape[0] - 1); fx = (lons[ci] - bl) / max(1e-12, br - bl) * (shape[1] - 1)
        y = np.clip(np.round(fy).astype(int), 0, shape[0] - 1); x = np.clip(np.round(fx).astype(int), 0, shape[1] - 1)
        v = a[y][:, x]; ok = v > -500
        sub = out[np.ix_(ri, ci)]
        # feathered in over its edges (no step where the national model meets GLO-90): full weight from FEATHER grid
        # cells inside its data, fading to GLO-90 at the edge; the box edges count as edges only where the model ends
        from scipy.ndimage import distance_transform_edt
        pad = np.pad(ok, 1, constant_values=True)
        w = np.clip(distance_transform_edt(pad)[1:-1, 1:-1] / FEATHER, 0, 1)
        w[~ok] = 0
        vv = np.where(ok, v, 0)
        sub = sub * (1 - w) + vv * w
        out[np.ix_(ri, ci)] = sub
    return out

#endregion

#region Tiles
def tile_box(z, x, y):
    s = 180.0 / (1 << z)
    return -180.0 + x * s, -90.0 + y * s, -180.0 + (x + 1) * s, -90.0 + (y + 1) * s

def has_land(z, x, y):
    lon0, lat0, lon1, lat1 = tile_box(z, x, y)
    for la in range(math.floor(lat0), math.ceil(lat1)):
        for lo in range(math.floor(lon0), math.ceil(lon1)):
            if (la, lo) in CELLS: return True
    return False

def make(job):
    z, x, y = job
    n = 65 if z < 8 else 129
    lon0, lat0, lon1, lat1 = tile_box(z, x, y)
    h, land = heights(lon0, lat0, lon1, lat1, n)
    spacing = (lat1 - lat0) * 110574.0 / (n - 1)
    # the grid as it is, rows north to south (2026-11-14: it was handed over transposed, h.T, since the first build - every
    # tile's heights flipped about its diagonal, up to 208 m wrong on the coarse levels; checked: as is, the mesh matches
    # the land to 0.00 m)
    hT = np.ascontiguousarray(h)
    mesh = Martini(n).create_tile(hT)
    near = z >= PRECISE_FROM and not (lon1 < PRECISE[0] or lon0 > PRECISE[2] or lat1 < PRECISE[1] or lat0 > PRECISE[3])
    v, tri = mesh.get_mesh(max(0.25 if near else 0.5, spacing * (PRECISE_ERR if near else ERR)))
    # the vertices with their heights, in degrees (rows run north to south in h, so y is flipped)
    pos = rescale_positions(v, hT, bounds=[lon0, lat0, lon1, lat1], flip_y=True).astype(np.float64).reshape(-1)
    buf = io.BytesIO()
    # the sea (where the height data has no land: exactly 0 above sea level): a 256 x 256 water mask, north row first,
    # or one byte when the tile is all land (0) or all sea (255); Cesium draws water there
    sea = ~land
    if sea.all(): wm = 255
    elif not sea.any(): wm = 0
    else:
        iy = np.clip(np.round(np.linspace(0, n - 1, 256)).astype(int), 0, n - 1)
        wm = (sea[np.ix_(iy, iy)] * 255).astype(np.uint8)
    qme.encode(buf, pos, tri, bounds=(lon0, lat0, lon1, lat1), extensions=(VertexNormalsExtension(positions=pos, indices=tri), WaterMaskExtension(data=wm)))
    p = OUT / str(z) / str(x) / f'{y}.terrain'
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(gzip.compress(buf.getvalue(), 6))
    return z, x, y

AIRPORT_H = 0.0
def init(h, sites=None):
    global AIRPORT_H, SITES
    AIRPORT_H = h
    if sites is not None: SITES = sites
#endregion

def main():
    global AIRPORT_H, OUT
    ap = argparse.ArgumentParser()
    ap.add_argument('--maxz', type=int, default=10)
    ap.add_argument('--bbox', type=float, nargs=4)
    ap.add_argument('--out', default=str(OUT))
    ap.add_argument('--workers', type=int, default=max(1, os.cpu_count() - 2))
    ap.add_argument('--minz', type=int, default=0, help='only the levels from this one down (with --bbox)')
    ap.add_argument('--resume', action='store_true', help='keep the tiles this job already made (its stamp), make the rest')
    ap.add_argument('--write-layer', action='store_true', help='with --bbox: still write layer.json (the whole world\'s list)')
    a = ap.parse_args()
    os.environ['ADP_TERRAIN_OUT'] = a.out
    OUT = Path(a.out)
    build_mosaic()
    prepare_national()
    # the airport's height: the ground round its origin (median within 1 km), on the ellipsoid
    lon, lat = AIRPORT
    d = 1000.0 / 111320.0
    init(0.0)
    hh, _ = heights(lon - d / math.cos(math.radians(lat)), lat - d, lon + d / math.cos(math.radians(lat)), lat + d, 33, flatten=False)
    AIRPORT_H = float(np.median(hh))
    # the other sites' airport levels, each written to its own terrain.json (the game puts its ground there: Site.h)
    global SITES
    SITES = load_sites()
    for s in SITES:
        s['h'] = round(site_height(s), 2)
        Path(s['dir'], 'terrain.json').write_text(json.dumps({'airportHeight': s['h'], 'note': 'WGS84 ellipsoid metres; Tools/MakeTerrain.py'}) + '\n')
        print('Site %s: airport ground at %.2f m (ellipsoid)' % (s['id'], s['h']), flush=True)
    if not a.bbox: (ROOT / 'Data' / 'terrain.json').write_text(json.dumps({'airportHeight': round(AIRPORT_H, 2), 'note': 'WGS84 ellipsoid metres; Tools/MakeTerrain.py'}) + '\n')
    print('Airport ground at %.2f m (ellipsoid)' % AIRPORT_H, flush=True)

    available = []
    jobs = []
    deepest = max([a.maxz] + [n[3] for n in NATIONAL if n[1].exists()])
    for z in range(deepest + 1):
        lvl = []
        xs = range(1 << (z + 1)); ys = range(1 << z)
        if z > a.maxz:
            # only over the national models
            boxes = [ds(n[1]).bounds for n in NATIONAL if n[1].exists() and n[3] >= z]
            s = 180.0 / (1 << z)
            cand = set()
            for b in boxes:
                for x in range(int((b.left + 180) // s), int((b.right + 180) // s) + 1):
                    for y in range(int((b.bottom + 90) // s), int((b.top + 90) // s) + 1): cand.add((x, y))
            pairs = sorted(cand)
        else:
            pairs = [(x, y) for x in xs for y in ys]
        for x, y in pairs:
            if True:
                lon0, lat0, lon1, lat1 = tile_box(z, x, y)
                if z > FULL_TO and not has_land(z, x, y): continue
                lvl.append((x, y))
                if a.bbox and (lon1 < a.bbox[0] or lon0 > a.bbox[2] or lat1 < a.bbox[1] or lat0 > a.bbox[3]) and z > 2: continue
                jobs.append((z, x, y))
        # the level's tiles as ranges: runs along x in each row
        ranges = []
        byy = {}
        for x, y in lvl: byy.setdefault(y, []).append(x)
        for y, xs in sorted(byy.items()):
            xs.sort(); s = p = xs[0]
            for x in xs[1:] + [None]:
                if x is not None and x == p + 1: p = x; continue
                ranges.append({'startX': s, 'startY': y, 'endX': p, 'endY': y})
                if x is not None: s = p = x
        available.append(ranges)
    if a.minz: jobs = [j for j in jobs if j[0] >= a.minz]
    # the job stamp: the same command run again with --resume keeps every tile made since it (a stopped run goes on)
    import hashlib, time
    OUT.mkdir(parents=True, exist_ok=True)
    stamp = OUT / ('.job-%s' % hashlib.sha1(json.dumps([a.bbox, a.minz, a.maxz]).encode()).hexdigest()[:12])
    if a.resume and stamp.exists():
        since = stamp.stat().st_mtime
        def done_already(j):
            q = OUT / str(j[0]) / str(j[1]) / f'{j[2]}.terrain'
            return q.exists() and q.stat().st_mtime >= since
        before = len(jobs); jobs = [j for j in jobs if not done_already(j)]
        print('resuming: %d of %d tiles already made in this job' % (before - len(jobs), before), flush=True)
    else:
        stamp.write_text(time.strftime('%Y-%m-%d %H:%M:%S')); time.sleep(1.1)   # (tiles made from now on are newer than it)
    print('%d tiles to make' % len(jobs), flush=True)
    done = 0
    with ProcessPoolExecutor(a.workers, initializer=init, initargs=(AIRPORT_H, SITES)) as ex:
        for _ in ex.map(make, jobs, chunksize=64):
            done += 1
            if done % max(1, len(jobs) // 20) == 0: print('%d%% (%d of %d)' % (done * 100 // len(jobs), done, len(jobs)), flush=True)
    stamp.unlink(missing_ok=True)   # (the job is done: the next run starts afresh)
    # the roads' ground comes from the tiles near the airport: measured again from the new ones
    try:
        import subprocess; subprocess.run([sys.executable, str(ROOT / 'Tools' / 'road_ground.py'), '--tiles', str(OUT)], check=False)
    except Exception as e: print('road_ground.py:', e, flush=True)
    # a --bbox run only remakes the tiles in its box: the existing layer.json (the whole world's) stays as it is
    if a.bbox and (OUT / 'layer.json').exists() and not a.write_layer:
        print('Terrain done: %d tiles (box only; layer.json kept)' % len(jobs), flush=True); return
    (OUT / 'layer.json').write_text(json.dumps({
        'tilejson': '2.1.0', 'name': 'Airport Dynasty Pro terrain', 'version': '1.0.0', 'format': 'quantized-mesh-1.0',
        'scheme': 'tms', 'projection': 'EPSG:4326', 'tiles': ['{z}/{x}/{y}.terrain'], 'extensions': ['octvertexnormals', 'watermask'],
        'bounds': [-180, -90, 180, 90], 'minzoom': 0, 'maxzoom': deepest, 'available': available}))   # (the credits are the game's own credit line, not the globe's)
    print('Terrain done: %d tiles' % len(jobs), flush=True)

if __name__ == '__main__':
    main()
