# The PC version's terrain, made once from the raw height data (Tools/GetTerrain.ps1) into a quantized-mesh terrain
# tileset that ships with the game (Cesium for Unreal reads it from disk: no streaming, no service).
#  - World: Copernicus DEM GLO-90 (1° tiles). Heights above sea level (EGM2008) are turned into heights above the
#    WGS84 ellipsoid with the EGM96 geoid grid (us_nga_egm96_15.tif, public domain), as Cesium needs.
#  - Tiles: Cesium's geographic TMS layout (level 0 is two tiles), every tile to level 6 (oceans flat at sea level),
#    deeper only where there is land. Each tile is a 2^n + 1 grid of heights turned into an adaptive triangle mesh
#    (Martini) with an error under a quarter of the grid spacing, then quantized-mesh with vertex normals, gzipped.
#  - The airport: flat within 2.6 km of its origin, blending into the land by 4.4 km (the height used is written to
#    Data/terrain.json so the game puts its ground exactly there).
#  - layer.json lists which tiles exist on each level, so the globe never asks for tiles that aren't there.
# Credit (in the game): "Copernicus DEM GLO-90 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018
# provided under COPERNICUS by the European Union and ESA; all rights reserved."
#   python Tools/MakeTerrain.py [--maxz 10] [--bbox lon0 lat0 lon1 lat1] [--workers 8]
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
from quantized_mesh_encoder.extensions import VertexNormalsExtension

ROOT = Path(__file__).resolve().parent.parent
SRC = Path('F:/ADP-Terrain/glo90')
GEOID = Path('F:/ADP-Terrain/geoid/us_nga_egm96_15.tif')
OUT = Path(os.environ.get('ADP_TERRAIN_OUT', 'F:/ADP-Terrain/tiles'))   # (--out; the workers read it from the environment)
AIRPORT = (-18.710348, 63.452335)   # lon, lat: the terminal's origin (Globe.cpp)
FLAT_IN, FLAT_OUT = 2600.0, 4400.0  # metres
FULL_TO = 6                          # every tile (land or sea) down to this level

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
    return out.astype(np.float32), land
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
]

def prepare_national():
    from rasterio.warp import calculate_default_transform, reproject, Resampling
    for src, dst, res, _ in NATIONAL:
        if not src.exists() or dst.exists(): continue
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
        sub = out[np.ix_(ri, ci)]; sub[ok] = v[ok]; out[np.ix_(ri, ci)] = sub
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
    hT = np.ascontiguousarray(h.T)   # Martini reads the grid as [x][y]
    mesh = Martini(n).create_tile(hT)
    v, tri = mesh.get_mesh(max(0.5, spacing * 0.25))
    # the vertices with their heights, in degrees (rows run north to south in h, so y is flipped)
    pos = rescale_positions(v, hT, bounds=[lon0, lat0, lon1, lat1], flip_y=True).astype(np.float64).reshape(-1)
    buf = io.BytesIO()
    qme.encode(buf, pos, tri, bounds=(lon0, lat0, lon1, lat1), extensions=(VertexNormalsExtension(positions=pos, indices=tri),))
    p = OUT / str(z) / str(x) / f'{y}.terrain'
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(gzip.compress(buf.getvalue(), 6))
    return z, x, y

AIRPORT_H = 0.0
def init(h):
    global AIRPORT_H
    AIRPORT_H = h
#endregion

def main():
    global AIRPORT_H, OUT
    ap = argparse.ArgumentParser()
    ap.add_argument('--maxz', type=int, default=10)
    ap.add_argument('--bbox', type=float, nargs=4)
    ap.add_argument('--out', default=str(OUT))
    ap.add_argument('--workers', type=int, default=max(1, os.cpu_count() - 2))
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
    (ROOT / 'Data' / 'terrain.json').write_text(json.dumps({'airportHeight': round(AIRPORT_H, 2), 'note': 'WGS84 ellipsoid metres; Tools/MakeTerrain.py'}) + '\n')
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
                if a.bbox and (lon1 < a.bbox[0] or lon0 > a.bbox[2] or lat1 < a.bbox[1] or lat0 > a.bbox[3]) and z > 2: continue
                if z > FULL_TO and not has_land(z, x, y): continue
                lvl.append((x, y))
        jobs += [(z, x, y) for x, y in lvl]
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
    print('%d tiles to make' % len(jobs), flush=True)
    done = 0
    with ProcessPoolExecutor(a.workers, initializer=init, initargs=(AIRPORT_H,)) as ex:
        for _ in ex.map(make, jobs, chunksize=64):
            done += 1
            if done % max(1, len(jobs) // 20) == 0: print('%d%% (%d of %d)' % (done * 100 // len(jobs), done, len(jobs)), flush=True)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'layer.json').write_text(json.dumps({
        'tilejson': '2.1.0', 'name': 'Airport Dynasty Pro terrain', 'version': '1.0.0', 'format': 'quantized-mesh-1.0',
        'scheme': 'tms', 'projection': 'EPSG:4326', 'tiles': ['{z}/{x}/{y}.terrain'], 'extensions': ['octvertexnormals'],
        'bounds': [-180, -90, 180, 90], 'minzoom': 0, 'maxzoom': deepest, 'available': available}))   # (the credits are the game's own credit line, not the globe's)
    print('Terrain done: %d tiles' % len(jobs), flush=True)

if __name__ == '__main__':
    main()
