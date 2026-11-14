# Our own land along the roads outside the airfield (owner, 2026-11-14), as the airfield's own ground inside it: the
# globe's terrain is drawn coarser far off (its levels differ by up to 8 m over the hills toward Vik), so a road laid on
# it floated or sank by distance; here the globe is cut away in a band along every road and the game draws the land
# itself, always at full detail, from the finest terrain tiles - the roads lie on it exactly, near or far. The band's
# edge is hidden: its outline wanders (no straight line), its last metres ease toward the globe's coarser ground, it
# carries the same land-cover colours as the globe, a skirt runs down under its rim (ALandCorridor), and moss, grass
# clumps and stones are set along it.
#   python Tools/make_land_corridor.py        (after Tools/make_roads.py and a terrain rebuild)
# Writes Data/land-corridor.json (the grid, the outline, the globe's cut) + land-corridor.bin (heights, float32, NaN
# off the band) + land-corridor.png (the colours), and the same into Content/Data.
import json, math, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy.interpolate import LinearNDInterpolator
from shapely.geometry import LineString, Polygon, box, Point
from shapely.ops import unary_union
from shapely.prepared import prep

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'Tools'))
import road_ground as rg   # the tile decoder and the georeference maths (the same as the roads')

TILES = Path('F:/ADP-Terrain/tiles-v2')
LANDCOVER = Path.home() / 'AppData' / 'Local' / 'AirportDynastyPro' / 'Terrain' / 'landcover' / 'region'
STEP = 2.5          # m between the band's grid points (MetaRoad's pieces are 2 m: the ground as fine near them)
HALF = 120.0        # m the band reaches either side of a road
WAVE = 28.0         # m its outline wanders in and out
CUT_IN = 22.0       # m inside our outline the globe's cut stops (our land overlaps the globe there: no hole, ever)
EASE = 30.0         # m over which the band's last stretch eases toward the globe's coarser ground
COARSE_Z = 11       # the globe's level seen at mid distance (what the edge eases to)
LC_Z = 7            # the land cover's North Atlantic level (Globe.cpp: 4 x 2 roots over -28.125..-5.625, 56.25..67.5)

def georef():
    F = json.loads((ROOT / 'Data' / 'airfield.json').read_text(encoding='utf-8')); ap = F['airport']; rw = F['runway']
    AH = json.loads((ROOT / 'Data' / 'terrain.json').read_text())['airportHeight']
    H = math.radians(ap['hdg']); mx, my = (rw['x0'] + rw['x1']) / 2, rw['y']
    east0 = -mx * math.sin(H) - my * math.sin(H - math.pi / 2); north0 = -mx * math.cos(H) - my * math.cos(H - math.pi / 2)
    lat0 = ap['lat'] + north0 / 111320.0; lon0 = ap['lon'] + east0 / (111320.0 * math.cos(math.radians(ap['lat'])))
    O = rg.ecef(lon0, lat0, AH + 0.4); lo, la = math.radians(lon0), math.radians(lat0)
    E = np.array([-math.sin(lo), math.cos(lo), 0]); N = np.array([-math.sin(la) * math.cos(lo), -math.sin(la) * math.sin(lo), math.cos(la)]); U = np.cross(E, N)
    return F, H, O, E, N, U

def plan_to_lonlat(X, Y, H, O, E, N):
    e = X * math.sin(H) + Y * math.sin(H - math.pi / 2); n = X * math.cos(H) + Y * math.cos(H - math.pi / 2)
    P = O[None, :] + E[None, :] * e[:, None] + N[None, :] * n[:, None]
    x, y, z = P[:, 0], P[:, 1], P[:, 2]; lon = np.arctan2(y, x); r = np.hypot(x, y); lat = np.arctan2(z, r * (1 - rg.E2))
    for _ in range(6):
        nn = rg.A_ / np.sqrt(1 - rg.E2 * np.sin(lat) ** 2); h = r / np.cos(lat) - nn; lat = np.arctan2(z, r * (1 - rg.E2 * nn / (nn + h)))
    return np.degrees(lon), np.degrees(lat)

def game_z(lon, lat, hh, O, U):
    lo, la = np.radians(lon), np.radians(lat)
    nn = rg.A_ / np.sqrt(1 - rg.E2 * np.sin(la) ** 2)
    P = np.stack([(nn + hh) * np.cos(la) * np.cos(lo), (nn + hh) * np.cos(la) * np.sin(lo), (nn * (1 - rg.E2) + hh) * np.sin(la)], 1)
    return (P - O[None, :]) @ U

def heights_at(lon, lat, tiles, z_want):
    """the ground at each point from the terrain tiles at level z_want (or the finest below it there)"""
    out = np.full(len(lon), np.nan)
    for z in range(z_want, 8, -1):
        todo = np.isnan(out)
        if not todo.any(): break
        s = 180.0 / (1 << z); tx = ((lon + 180) / s).astype(int); ty = ((lat + 90) / s).astype(int)
        for key in set(zip(tx[todo], ty[todo])):
            t = tiles.tile(z, int(key[0]), int(key[1]))
            if t is None: continue
            X, Y, Hh, T = t
            sel = todo & (tx == key[0]) & (ty == key[1])
            f = LinearNDInterpolator(np.stack([X, Y], 1), Hh)
            out[sel] = f(lon[sel], lat[sel])
    return out

def landcover_rgb(lon, lat):
    """the globe's own land-cover colour at each point (Data from the downloaded terrain's landcover/region level 7)"""
    n = 1 << LC_Z; W0, S0, dl, dt = -28.125, 56.25, 22.5 / (4 * n), 11.25 / (2 * n)
    fx = (lon - W0) / dl; fy = (lat - S0) / dt
    tx = np.floor(fx).astype(int); ty = np.floor(fy).astype(int)
    rgb = np.zeros((len(lon), 3), np.float32); cache = {}
    for key in set(zip(tx, ty)):
        p = LANDCOVER / str(LC_Z) / str(key[0]) / ('%d.jpg' % key[1])
        if not p.exists(): continue
        a = cache.setdefault(key, np.asarray(Image.open(p).convert('RGB'), np.float32))
        sel = (tx == key[0]) & (ty == key[1])
        px = np.clip((fx[sel] - key[0]) * 256 - 0.5, 0, 254.999); py = np.clip((1 - (fy[sel] - key[1])) * 256 - 0.5, 0, 254.999)   # (rows north first)
        x0 = px.astype(int); y0 = py.astype(int); ax = (px - x0)[:, None]; ay = (py - y0)[:, None]
        rgb[sel] = (a[y0, x0] * (1 - ax) + a[y0, x0 + 1] * ax) * (1 - ay) + (a[y0 + 1, x0] * (1 - ax) + a[y0 + 1, x0 + 1] * ax) * ay
    return rgb

def wavy(poly, amp, seed=7):
    """the outline wandering in and out by up to amp (smooth: a few long waves along it), so no straight edge shows"""
    rnd = np.random.default_rng(seed)
    def wav(ring):
        P = np.array(ring.coords)[:-1]
        seg = np.hypot(*np.diff(np.vstack([P, P[:1]]), axis=0).T); s = np.concatenate([[0], np.cumsum(seg)[:-1]])
        w = np.zeros(len(P))
        for L, a in ((420.0, 0.55), (170.0, 0.3), (65.0, 0.15)):
            ph = rnd.uniform(0, 2 * math.pi); w += a * np.sin(2 * math.pi * s / L + ph)
        nx = np.roll(P, -1, 0) - np.roll(P, 1, 0); nrm = np.stack([nx[:, 1], -nx[:, 0]], 1); nrm /= np.maximum(np.linalg.norm(nrm, axis=1)[:, None], 1e-9)
        return P + nrm * (w * amp)[:, None]
    dens = poly.segmentize(10.0)
    if dens.geom_type == 'Polygon': parts = [dens]
    else: parts = list(dens.geoms)
    out = []
    for p in parts:
        sh = Polygon(wav(p.exterior), [wav(r) for r in p.interiors]).buffer(0)
        out.append(sh)
    return unary_union(out)

def main():
    F, H, O, E, N, U = georef()
    B = F['boundary']; field = box(min(B['x0'], B['x1']), min(B['y0'], B['y1']), max(B['x0'], B['x1']), max(B['y0'], B['y1']))
    R = json.loads((ROOT / 'Data' / 'roads.json').read_text(encoding='utf-8'))
    lines = [LineString([(p[0], p[1]) for p in r['pts']]) for r in R['roads'] if len(r['pts']) > 1]
    lines = [l for l in lines if not field.buffer(-60).contains(l)]
    band = unary_union([l.buffer(HALF, quad_segs=12) for l in lines])
    band = wavy(band, WAVE).difference(field.buffer(-40))        # (the airfield's own ground inside: ours already)
    band = band.simplify(1.0)
    cut = band.buffer(-CUT_IN).simplify(2.0)
    x0, y0, x1, y1 = band.bounds; x0 = math.floor(x0 / STEP) * STEP; y0 = math.floor(y0 / STEP) * STEP
    nx = int(math.ceil((x1 - x0) / STEP)) + 1; ny = int(math.ceil((y1 - y0) / STEP)) + 1
    print('band %.1f km2, grid %d x %d at %.1f m' % (band.area / 1e6, nx, ny, STEP), flush=True)
    GX, GY = np.meshgrid(x0 + np.arange(nx) * STEP, y0 + np.arange(ny) * STEP)   # rows: y
    pb = prep(band.buffer(STEP * 1.5))
    inside = np.array([pb.contains(Point(x, y)) for x, y in zip(GX.ravel(), GY.ravel())]).reshape(ny, nx)
    X = GX[inside]; Y = GY[inside]
    lon, lat = plan_to_lonlat(X, Y, H, O, E, N)
    tiles = rg.Tiles(TILES)
    print('heights at %d points...' % len(X), flush=True)
    hf = heights_at(lon, lat, tiles, 13)
    hc = heights_at(lon, lat, tiles, COARSE_Z)
    zf = game_z(lon, lat, hf, O, U); zc = game_z(lon, lat, np.where(np.isnan(hc), hf, hc), O, U)
    # the last EASE metres toward the coarser level (what the globe shows round it from mid distance)
    d_edge = np.array([band.exterior.distance(Point(x, y)) if band.geom_type == 'Polygon' else band.boundary.distance(Point(x, y)) for x, y in zip(X, Y)])
    t = np.clip(1.0 - d_edge / EASE, 0, 1); t = t * t * (3 - 2 * t)
    z = zf * (1 - t) + zc * t
    Z = np.full((ny, nx), np.nan, np.float32); Z[inside] = z
    # the colours: the globe's land cover, one pixel a grid point
    rgb = landcover_rgb(lon, lat)
    img = np.zeros((ny, nx, 3), np.uint8); img[inside] = np.clip(rgb, 0, 255).astype(np.uint8)
    # (outside the band, the nearest colour: no dark rim from filtering at the edge)
    from scipy.ndimage import distance_transform_edt
    _, (iy, ix) = distance_transform_edt(~inside, return_indices=True); img = img[iy, ix]
    def rings(g):
        geoms = [g] if g.geom_type == 'Polygon' else list(g.geoms)
        return [[[round(x, 2), round(y, 2)] for x, y in p.exterior.coords] for p in geoms if p.area > 400]
    head = {'source': 'Tools/make_land_corridor.py (terrain %s, level 13; edge eased to level %d)' % (TILES, COARSE_Z),
            'x0': x0, 'y0': y0, 'step': STEP, 'nx': nx, 'ny': ny, 'cut': rings(cut), 'outline': rings(band)}
    for d in (ROOT / 'Data', ROOT / 'Content' / 'Data'):
        (d / 'land-corridor.json').write_text(json.dumps(head))
        Z.astype('<f4').tofile(d / 'land-corridor.bin')
        Image.fromarray(img[::-1]).save(d / 'land-corridor.png')   # (row 0 = the north edge, y1, as images run)
    print('land corridor: %d points, ground %.1f to %.1f m, %d cut polygons' % (inside.sum(), np.nanmin(Z), np.nanmax(Z), len(head['cut'])))

if __name__ == '__main__':
    main()
