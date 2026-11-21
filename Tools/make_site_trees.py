"""Trees where the real trees are (to-do 200; owner, 2026-11-20 night: "be absolutely sure ... where trees are actually
supposed to be, not houses or fields ... You can see a top-down view of the trees on the satellite photo").

Per 1 km swisstopo tile:
  1. the height of everything above the ground: swissSURFACE3D raster (0.5 m, the tops of trees and roofs) minus
     swissALTI3D (0.5 m, the bare ground) = a canopy height model;
  2. the 10 cm SWISSIMAGE photo read at 0.5 m: where it is plant-coloured (green, or the dark green of conifers);
  3. swissTLM3D building footprints (1.5 m round them) and roads (their width + 1 m) cut out;
  4. a tree = a crown top: a local highest point 3-50 m above the ground, on plants in the photo, off buildings and roads;
     crowns closer than their own radius merged (the higher one kept);
  5. its kind from the photo and the crown shape: conifers dark and pointed, broadleaf lighter and round;
  6. checked against swissTLM3D's single trees (Einzelbaum) - printed - and drawn over the photo for a look.
Nothing is placed in the airport (its fence + 120 m), on our own ground along the roads, or in the town we built.
Out: Data/Sites/<site>/trees.bin (float32 x, y, z (game plan, m), height, crown radius, kind (0 conifer, 1 broadleaf))
sorted into 256 m cells, trees.json (the cells), and docs/Pictures/Chablais/Trees/ (check pictures).
  python -I Tools/make_site_trees.py --site chablais --tiles 2550-2566 1126-1142 [--only 2559-1134] [--check 6]"""
import argparse, json, math, os, sys
from pathlib import Path
import numpy as np
import rasterio
from rasterio.enums import Resampling
from rasterio.features import rasterize
from scipy import ndimage
from scipy.spatial import cKDTree
import shapefile
from shapely.geometry import shape, box, Point
from shapely.prepared import prep
from shapely.ops import unary_union

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'Tools'))
SURF = 'F:/ADP-Terrain/chablais/surface-0.5m'
ALTI = 'F:/ADP-Terrain/chablais/alti-0.5m'
PHOTO = 'D:/ADP-Terrain/Chablais/photo-10cm'
TLM = 'F:/ADP-Terrain/chablais/tlm'
PX = 2000   # 0.5 m in a 1 km tile


def files(d, key):
    out = {}
    for f in os.listdir(d):
        if f.endswith('.tif'):
            k = f.split('_')[2] if '_' in f else ''
            # (newest year wins: names hold the year after the product name)
            yr = f.split('_')[1]
            if k not in out or yr > out[k][0]: out[k] = (yr, os.path.join(d, f))
    return {k: v[1] for k, v in out.items()}


def read(path, shape_=(PX, PX), bands=1):
    with rasterio.open(path) as d:
        a = d.read(list(range(1, bands + 1)) if bands > 1 else 1, out_shape=((bands,) if bands > 1 else ()) + shape_, resampling=Resampling.average)
        return a.astype(np.float32), d.nodata


CACHE = {}   # this area's buildings, roads and mapped single trees, read once (F:/ADP-Terrain/chablais/tlm/trees-area.pkl)


def load_area(e0, n0, e1, n1):
    """Buildings, roads (buffered to their width + 1 m) and swissTLM3D's single trees inside the area, read once and kept."""
    import pickle
    pk = f'{TLM}/trees-area-{e0}-{n0}-{e1}-{n1}.pkl'
    if os.path.exists(pk):
        CACHE.update(pickle.load(open(pk, 'rb'))); return
    def inside(bb): return not (bb[2] < e0 - 50 or bb[0] > e1 + 50 or bb[3] < n0 - 50 or bb[1] > n1 + 50)
    blds, roads, ref = [], [], []
    for side in ('WEST', 'OST'):
        p = f'{TLM}/TLM_BAUTEN/swissTLM3D_TLM_GEBAEUDE_FOOTPRINT_{side}.shp'
        if os.path.exists(p):
            for s in shapefile.Reader(p).iterShapes():
                if inside(s.bbox):
                    try: blds.append(shape(s.__geo_interface__).buffer(1.5))
                    except Exception: pass
        p = f'{TLM}/TLM_BB/swissTLM3D_TLM_EINZELBAUM_{side}.shp'
        if os.path.exists(p):
            for s in shapefile.Reader(p).iterShapes():
                if s.points:
                    x, y = s.points[0][:2]
                    if e0 <= x < e1 and n0 <= y < n1: ref.append((x, y))
    p = f'{TLM}/TLM_STRASSEN/swissTLM3D_TLM_STRASSE.shp'
    if os.path.exists(p):
        r = shapefile.Reader(p); flds = [f[0] for f in r.fields[1:]]
        wi = flds.index('OBJEKTART') if 'OBJEKTART' in flds else None
        for s, rec in zip(r.iterShapes(), r.iterRecords()):
            if not inside(s.bbox): continue
            kind = str(rec[wi]) if wi is not None else ''
            w = 3.0 if 'Weg' in kind else 6.0 if '4m' in kind or '6m' in kind else 9.0 if '8m' in kind or '10m' in kind else 14.0 if 'Autobahn' in kind or 'Autostr' in kind else 5.0
            try: roads.append(shape(s.__geo_interface__).buffer(w / 2 + 1.0))
            except Exception: pass
    d = {'off': blds + roads, 'offb': np.array([g.bounds for g in blds + roads]) if blds or roads else np.zeros((0, 4)), 'ref': np.array(ref) if ref else np.zeros((0, 2))}
    pickle.dump(d, open(pk, 'wb')); CACHE.update(d)
    print(f'area read: {len(blds)} buildings, {len(roads)} roads, {len(ref)} mapped single trees', flush=True)


def tlm_off(e0, n0, e1, n1):
    b = CACHE['offb']
    if not len(b): return []
    k = np.nonzero(~((b[:, 2] < e0) | (b[:, 0] > e1) | (b[:, 3] < n0) | (b[:, 1] > n1)))[0]
    return [CACHE['off'][i] for i in k]


def single_trees(e0, n0, e1, n1):
    r = CACHE['ref']
    return r[(r[:, 0] >= e0) & (r[:, 0] < e1) & (r[:, 1] >= n0) & (r[:, 1] < n1)] if len(r) else r


def tile_trees(key, fs, fa, fp, keep_out):
    e0 = int(key.split('-')[0]) * 1000; n0 = int(key.split('-')[1]) * 1000; e1, n1 = e0 + 1000, n0 + 1000
    dsm, nd1 = read(fs[key]); dtm, nd2 = read(fa[key])
    bad = (dsm < -100) | (dtm < -100)
    if nd1 is not None: bad |= dsm == nd1
    if nd2 is not None: bad |= dtm == nd2
    chm = np.where(bad, 0, dsm - dtm); chm = np.clip(chm, 0, 60)
    # the photo at 0.5 m
    if key in fp:
        rgb, _ = read(fp[key], bands=3)
        R, G, B = rgb[0], rgb[1], rgb[2]
    else:
        R = G = B = np.full((PX, PX), 128, np.float32)
    V = np.maximum(np.maximum(R, G), B)
    exg = 2 * G - R - B
    # plants: green (green above red and blue), also dark crowns in shade - but always green above blue: grey or bluish
    # is rock, scree, roofs and roads in shadow (measured 11-20: shaded scree G-B -9, shaded conifers +7.5, lit forest +35)
    veg = (((exg > 8) & (G > B)) | ((V < 70) & (G - B > 1) & (exg > 3))) & (V < 215)
    # buildings and roads out
    tr = rasterio.transform.from_bounds(e0, n0, e1, n1, PX, PX)
    shapes_ = [(g, 1) for g in tlm_off(e0, n0, e1, n1)]
    off = rasterize(shapes_, out_shape=(PX, PX), transform=tr, fill=0, dtype='uint8').astype(bool) if shapes_ else np.zeros((PX, PX), bool)
    ok = veg & ~off & (chm >= 3.0) & (chm <= 50.0)
    # a little smoothing so one crown has one top
    chs = ndimage.gaussian_filter(chm, 1.0)
    mx = ndimage.maximum_filter(chs, size=5)
    peaks = np.argwhere((chs == mx) & ok & (chs >= 3.0))
    if not len(peaks): return np.zeros((0, 6)), (e0, n0), 0, 0
    h = chs[peaks[:, 0], peaks[:, 1]]
    # crowns: radius from the height (small trees ~1.5 m, big ones up to ~7 m), the higher of two too-close tops kept
    rad = np.clip(0.9 + 0.11 * h, 1.3, 7.0)
    order = np.argsort(-h); xy = np.stack([e0 + (peaks[:, 1] + 0.5) * 0.5, n1 - (peaks[:, 0] + 0.5) * 0.5], 1)
    kd = cKDTree(xy); keep = np.ones(len(h), bool); gone = np.zeros(len(h), bool)
    for i in order:
        if gone[i]: continue
        for j in kd.query_ball_point(xy[i], rad[i] * 0.85):
            if j != i and h[j] <= h[i]: gone[j] = True
    keep = ~gone
    xy, h, rad, pk = xy[keep], h[keep], rad[keep], peaks[keep]
    # keep-out (the airport, our own ground, the town)
    if keep_out is not None:
        inside = np.array([keep_out.contains(Point(p[0], p[1])) for p in xy]) if len(xy) else np.zeros(0, bool)
        xy, h, rad, pk = xy[~inside], h[~inside], rad[~inside], pk[~inside]
    # kind: conifers dark and pointed (the top well above the ring round it), broadleaf lighter and round
    ring = ndimage.uniform_filter(chm, size=7)[pk[:, 0], pk[:, 1]] if len(pk) else np.zeros(0)
    sharp = (h - ring) / np.maximum(h, 1)
    vv = ndimage.uniform_filter(V, size=5)[pk[:, 0], pk[:, 1]] if len(pk) else np.zeros(0)
    eg = ndimage.uniform_filter(exg, size=5)[pk[:, 0], pk[:, 1]] if len(pk) else np.zeros(0)
    conifer = ((vv < 75) & (sharp > 0.12)) | ((vv < 60) & (eg < 25)) | (sharp > 0.3)
    z_ground = dtm[pk[:, 0], pk[:, 1]] if len(pk) else np.zeros(0)
    out = np.stack([xy[:, 0], xy[:, 1], z_ground, h, rad, np.where(conifer, 0, 1)], 1) if len(xy) else np.zeros((0, 6))
    return out, (e0, n0), int(veg.sum()), int(off.sum())


def check_picture(key, fp, T, ref, path):
    """The photo with every placed tree drawn over it (green: broadleaf, blue: conifer; white rings: swissTLM3D's single trees)."""
    from PIL import Image, ImageDraw
    e0 = int(key.split('-')[0]) * 1000; n1 = int(key.split('-')[1]) * 1000 + 1000
    rgb, _ = read(fp[key], shape_=(2500, 2500), bands=3)
    im = Image.fromarray(np.clip(rgb.transpose(1, 2, 0), 0, 255).astype(np.uint8)); d = ImageDraw.Draw(im)
    s = 2.5   # px per m
    for x, y in ref:
        u, v = (x - e0) * s, (n1 - y) * s; d.ellipse([u - 6, v - 6, u + 6, v + 6], outline=(255, 255, 255), width=1)
    for x, y, _, h, r, k in T:
        u, v = (x - e0) * s, (n1 - y) * s; rr = max(2, r * s * 0.6)
        d.ellipse([u - rr, v - rr, u + rr, v + rr], outline=(60, 160, 255) if k == 0 else (90, 255, 90), width=2)
    im.save(path)


KEEP = None


def worker_init(area, parts):
    global KEEP
    load_area(*area)
    KEEP = prep(unary_union(parts))


def worker_tile(k, fs, fa, fp, check, pic):
    out = tile_trees(k, fs, fa, fp, KEEP)
    if check and k in fp:
        e0, n0 = out[1]
        check_picture(k, fp, out[0], single_trees(e0, n0, e0 + 1000, n0 + 1000), os.path.join(pic, f'Trees-check-{k}.png'))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--site', default='chablais'); ap.add_argument('--tiles', nargs=4, type=int, default=[2550, 2566, 1126, 1142])
    ap.add_argument('--only'); ap.add_argument('--check', type=int, default=6)
    a = ap.parse_args()
    import make_land_corridor as lc
    import road_ground as rg
    from pyproj import Transformer
    lc.SITE = a.site
    F, H, O, E, N, U = lc.georef(); SD = lc.site_dir()
    to_ll = Transformer.from_crs(2056, 4326, always_xy=True); to_lv = Transformer.from_crs(4326, 2056, always_xy=True)
    # the keep-out: the airport fence + 120 m, the land along our roads (its cut), the town (its blocks)
    ko = []
    B = F['boundary']; plan_box = [(B['x0'] - 120, B['y0'] - 120), (B['x1'] + 120, B['y0'] - 120), (B['x1'] + 120, B['y1'] + 120), (B['x0'] - 120, B['y1'] + 120)]
    def plan_poly_to_lv(pts):
        X = np.array([p[0] for p in pts], float); Y = np.array([p[1] for p in pts], float)
        lo, la = lc.plan_to_lonlat(X, Y, H, O, E, N); e, n = to_lv.transform(lo, la)
        from shapely.geometry import Polygon
        return Polygon(list(zip(e, n)))
    ko.append(plan_poly_to_lv(plan_box))
    # our roads (their full width + 2 m) and car parks - NOT the whole custom ground: real trees on it stay (2026-11-21)
    from shapely.geometry import LineString
    rj = SD / 'roads.json'
    if rj.exists():
        R = json.loads(rj.read_text(encoding='utf-8'))
        for rd in R.get('roads', []):
            pts = rd.get('pts') or []
            if len(pts) < 2: continue
            def lw(v):   # (a lane's width may change along the road: [[at, w], ...] or [w, ...] - its widest)
                if isinstance(v, (int, float)): return float(v)
                fl = [x[-1] if isinstance(x, (list, tuple)) else x for x in (v or [])]
                return float(max(fl)) if fl else 3.0
            w = sum(lw(l.get('w', 3.0)) for side in ('left', 'right') for l in (rd.get('lanes') or {}).get(side, []))
            X = np.array([p[0] for p in pts], float); Y = np.array([p[1] for p in pts], float)
            lo_, la_ = lc.plan_to_lonlat(X, Y, H, O, E, N); e_, n_ = to_lv.transform(lo_, la_)
            ko.append(LineString(list(zip(e_, n_))).buffer(max(w, 7.0) / 2 + 2.0))
        for lt in R.get('lots', []):
            ko.append(plan_poly_to_lv([(lt['x0'] - 2, lt['y0'] - 2), (lt['x1'] + 2, lt['y0'] - 2), (lt['x1'] + 2, lt['y1'] + 2), (lt['x0'] - 2, lt['y1'] + 2)]))
    for area in getattr(lc, 'AREAS', []) or []: ko.append(plan_poly_to_lv(area))   # (the town, once laid out)
    keep_out_parts = [k.buffer(1) for k in ko]
    fs, fa, fp = files(SURF, None), files(ALTI, None), files(PHOTO, None)
    keys = [a.only] if a.only else [f'{e}-{n}' for e in range(a.tiles[0], a.tiles[1] + 1) for n in range(a.tiles[2], a.tiles[3] + 1)]
    keys = [k for k in keys if k in fs and k in fa]
    print(f'{len(keys)} tiles with surface + ground heights ({sum(k in fp for k in keys)} with the 10 cm photo)', flush=True)
    allT = []; stats = []
    pic = ROOT / 'docs' / 'Pictures' / 'Chablais' / 'Trees'; pic.mkdir(parents=True, exist_ok=True)
    checks = set(keys[:: max(1, len(keys) // max(1, a.check))][:a.check]) | ({'2559-1135', '2560-1134', '2562-1136'} & set(keys))
    es = [int(k.split('-')[0]) * 1000 for k in keys]; ns = [int(k.split('-')[1]) * 1000 for k in keys]
    AREA = (min(es), min(ns), max(es) + 1000, max(ns) + 1000)
    load_area(*AREA)
    from concurrent.futures import ProcessPoolExecutor
    ex = ProcessPoolExecutor(max(1, min(len(keys), (os.cpu_count() or 4) - 2)), initializer=worker_init, initargs=(AREA, keep_out_parts))
    futs = [ex.submit(worker_tile, k, fs, fa, fp, k in checks, str(pic)) for k in keys]
    for i, (k, fu) in enumerate(zip(keys, futs)):
        T, (e0, n0), nveg, noff = fu.result()
        ref = single_trees(e0, n0, e0 + 1000, n0 + 1000)
        # how many of the mapped single trees have a placed tree within 3 m
        hit = 0
        if len(ref) and len(T): hit = int((cKDTree(T[:, :2]).query(ref, distance_upper_bound=3.0)[0] < 3.0).sum())
        stats.append((k, len(T), len(ref), hit)); allT.append(T)
        print(f'  {k}: {len(T)} trees ({int((T[:, 5] == 0).sum()) if len(T) else 0} conifers), swissTLM3D single trees {len(ref)}, of them found {hit} ({100 * hit / max(1, len(ref)):.0f}%)  [{i + 1}/{len(keys)}]', flush=True)
    T = np.concatenate([t for t in allT if len(t)]) if any(len(t) for t in allT) else np.zeros((0, 6))
    print(f'{len(T)} trees in all', flush=True)
    # LV95 + ground -> the game's plan (x, y) and height (z on the globe's terrain): lon/lat, ellipsoid height (geoid)
    lo, la = to_ll.transform(T[:, 0], T[:, 1])
    import MakeTerrain as mt
    hh = T[:, 2] + mt.geoid(lo, la)
    # the ground the game draws: our custom ground where it is (land-corridor.bin), else the globe's finest terrain
    # (level 17, its own triangles - the airport's levelling and blend included), else the laser ground
    tiles = rg.Tiles('F:/ADP-Terrain/tiles-v2', 17)
    zt = np.full(len(lo), np.nan)
    import matplotlib.tri as mtri
    z = 17; s_ = 180.0 / (1 << z); tx = ((lo + 180) / s_).astype(int); ty = ((la + 90) / s_).astype(int)
    keys_ = {}
    for i_, k_ in enumerate(zip(tx, ty)): keys_.setdefault(k_, []).append(i_)
    for n_, (k_, idx) in enumerate(keys_.items()):
        t = tiles.tile(z, int(k_[0]), int(k_[1]))
        if t is None: continue
        Xs, Ys, Hs, Tr = t
        try:
            f = mtri.LinearTriInterpolator(mtri.Triangulation(Xs, Ys, Tr), Hs)
            idx = np.array(idx); v = f(lo[idx], la[idx]); ok = ~np.ma.getmaskarray(v)
            zt[idx[ok]] = np.asarray(v)[ok]
        except Exception as ex_: pass
        if n_ % 2000 == 0: print(f'  heights: {n_}/{len(keys_)} terrain tiles', flush=True)
    hh = np.where(np.isnan(zt), hh, zt)
    zg = lc.game_z(lo, la, hh, O, U)
    # plan from lon/lat: e, n along the airport's east/north axes, then turned by the runway heading
    lor, lar = np.radians(lo), np.radians(la)
    nn = rg.A_ / np.sqrt(1 - rg.E2 * np.sin(lar) ** 2)
    Pe = np.stack([(nn + hh) * np.cos(lar) * np.cos(lor), (nn + hh) * np.cos(lar) * np.sin(lor), (nn * (1 - rg.E2) + hh) * np.sin(lar)], 1) - O[None, :]
    e, n = Pe @ E, Pe @ N
    X = e * math.sin(H) + n * math.cos(H); Y = -e * math.cos(H) + n * math.sin(H)
    # on our custom ground: its own height (land-corridor.bin, plan grid)
    lcj = SD / 'land-corridor.json'
    if lcj.exists():
        hd = json.loads(lcj.read_text()); Zc = np.fromfile(SD / 'land-corridor.bin', '<f4').reshape(hd['ny'], hd['nx'])
        gi = ((X - hd['x0']) / hd['step']).round().astype(int); gj = ((Y - hd['y0']) / hd['step']).round().astype(int)
        inn = (gi >= 0) & (gi < hd['nx']) & (gj >= 0) & (gj < hd['ny'])
        zc = np.full(len(X), np.nan); zc[inn] = Zc[gj[inn], gi[inn]]
        on = ~np.isnan(zc); zg = np.where(on, zc, zg)
        print(f'  {int(on.sum())} trees on our custom ground (its height)', flush=True)
    G = np.stack([X, Y, zg - 0.3, T[:, 3], T[:, 4], T[:, 5]], 1).astype(np.float32)   # (the trunk 0.3 m into the ground)
    # into 256 m cells (the game draws the cells near you at full detail)
    cx = np.floor(G[:, 0] / 256).astype(int); cy = np.floor(G[:, 1] / 256).astype(int)
    order = np.lexsort((cy, cx)); G = G[order]; cx = cx[order]; cy = cy[order]
    cells = []; start = 0
    for j in range(1, len(G) + 1):
        if j == len(G) or cx[j] != cx[start] or cy[j] != cy[start]:
            cells.append([int(cx[start]), int(cy[start]), start, j - start]); start = j
    G.tofile(SD / 'trees.bin')
    json.dump({'note': 'Tools/make_site_trees.py: float32 x, y, z (plan m), height m, crown radius m, kind (0 conifer, 1 broadleaf); cells [cx, cy, first, count] of 256 m',
               'count': int(len(G)), 'cell': 256, 'cells': cells, 'stats': stats}, open(SD / 'trees.json', 'w'))
    print(f'wrote {len(G)} trees in {len(cells)} cells to {SD / "trees.bin"} ({(SD / "trees.bin").stat().st_size / 1e6:.0f} MB)', flush=True)
    # (the editor and the game read the site's data from Content/Data/Sites first: copied there too)
    import shutil
    cd = ROOT / 'Content' / 'Data' / 'Sites' / a.site; cd.mkdir(parents=True, exist_ok=True)
    for f in ('trees.bin', 'trees.json'): shutil.copy(SD / f, cd / f)


if __name__ == '__main__':
    main()
