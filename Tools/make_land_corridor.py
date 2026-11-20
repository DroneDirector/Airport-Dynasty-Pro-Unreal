# Our own land along the roads outside the airfield (owner, 2026-11-14), as the airfield's own ground inside it: the
# globe's terrain is drawn coarser far off (its levels differ by up to 8 m over the hills toward Vik), so a road laid on
# it floated or sank by distance; here the globe is cut away in a band along every road and the game draws the land
# itself, always at full detail, from the finest terrain tiles - the roads lie on it exactly, near or far. The band's
# edge is hidden: its outline wanders (no straight line), its last metres ease toward the globe's coarser ground, it
# carries the same land-cover colours as the globe, a skirt runs down under its rim (ALandCorridor), and moss, grass
# clumps and stones are set along it.
#   python Tools/make_land_corridor.py        (run by Tools/make_roads.py itself; again after a terrain rebuild)
# Areas of our own ground besides the roads (the town, when it is built) go into AREAS: each a polygon in plan metres,
# the band reaching round it as round a road.
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
AREAS = []          # [[(x, y), ...], ...] - e.g. the town's built area, once it is laid out
COARSE_Z = 11       # the globe's level seen at mid distance (what the edge eases to)
LC_Z = 7            # the land cover's North Atlantic level (Globe.cpp: 4 x 2 roots over -28.125..-5.625, 56.25..67.5)

SITE = None        # another airport site (--site <id>: Data/Sites/<id>; Site.h)
def site_dir(): return ROOT / 'Data' / 'Sites' / SITE if SITE else ROOT / 'Data'

def georef():
    F = json.loads((ROOT / 'Data' / 'airfield.json').read_text(encoding='utf-8')); ap = F['airport']; rw = F['runway']
    if SITE: ap = dict(ap, **json.loads((site_dir() / 'site.json').read_text(encoding='utf-8'))['airport'])
    AH = json.loads((site_dir() / 'terrain.json').read_text())['airportHeight']
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

# the globe's pictures, as Globe.cpp's sources (Settings > Imagery): the band carries the same one, baked per source so it
# matches whichever is shown (2026-11-14: the globe shows the Sentinel-2 photo by default - the black sand dark - not
# the land cover). Web Mercator tiles, {z}/{y from the north}/{x}; cached on F:.
SOURCES = {
    0: ('https://tiles.maps.eox.at/wmts/1.0.0/s2cloudless_3857/default/g/{z}/{y}/{x}.jpg', 15),
    1: ('https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/Landsat_WELD_CorrectedReflectance_TrueColor_Global_Annual/default/GoogleMapsCompatible_Level12/{z}/{y}/{x}.jpg', 12),
    2: ('https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/BlueMarble_NextGeneration/default/GoogleMapsCompatible_Level8/{z}/{y}/{x}.jpeg', 8),
}
CACHE = Path('F:/ADP-Terrain/imagery-cache')

def imagery_rgb(lon, lat, src):
    import urllib.request, io, time
    url, z = SOURCES[src]; n = 1 << z
    fx = (lon + 180.0) / 360.0 * n
    la = np.radians(lat); fy = (1.0 - np.log(np.tan(la) + 1.0 / np.cos(la)) / math.pi) / 2.0 * n
    tx = np.floor(fx).astype(int); ty = np.floor(fy).astype(int)
    rgb = np.zeros((len(lon), 3), np.float32)
    for key in set(zip(tx, ty)):
        f = CACHE / str(src) / str(z) / str(key[1]) / ('%d.img' % key[0])
        if not f.exists():
            f.parent.mkdir(parents=True, exist_ok=True)
            req = urllib.request.Request(url.format(z=z, y=key[1], x=key[0]), headers={'User-Agent': 'AirportDynastyPro-terrain-tool'})
            for attempt in range(4):
                try: f.write_bytes(urllib.request.urlopen(req, timeout=30).read()); break
                except Exception as e:
                    if attempt == 3: print('  tile %s failed: %s' % (key, e)); break
                    time.sleep(2 + 3 * attempt)
            time.sleep(0.15)
        if not f.exists(): continue
        a = np.asarray(Image.open(f).convert('RGB'), np.float32); S = a.shape[0]
        sel = (tx == key[0]) & (ty == key[1])
        px = np.clip((fx[sel] - key[0]) * S - 0.5, 0, S - 1.001); py = np.clip((fy[sel] - key[1]) * S - 0.5, 0, S - 1.001)
        x0 = px.astype(int); y0 = py.astype(int); ax = (px - x0)[:, None]; ay = (py - y0)[:, None]
        rgb[sel] = (a[y0, x0] * (1 - ax) + a[y0, x0 + 1] * ax) * (1 - ay) + (a[y0 + 1, x0] * (1 - ax) + a[y0 + 1, x0 + 1] * ax) * ay
    return rgb

def photo_rgb(lon, lat):
    """The site's own aerial photo (Tools/make_site_imagery.py tiles, numbered inside its rectangle), its finest level."""
    S = json.loads((site_dir() / 'site.json').read_text(encoding='utf-8')); D = Path(S['imageryDev'])
    T = json.loads((D / 'tiles.json').read_text()); z0 = T['webLevel0']; L = T['maxLevel']; z = z0 + L; n0 = 1 << z0; n = 1 << z
    def web(lo, la): return (lo + 180.0) / 360.0, (1.0 - np.log(np.tan(np.radians(la)) + 1.0 / np.cos(np.radians(la))) / math.pi) / 2.0
    wx, wy = web(T['west'] + 1e-9, T['north'] - 1e-9); X0 = int(wx * n0); Y0 = int(wy * n0); k = 1 << L
    fx, fy = web(lon, lat); fx = fx * n; fy = fy * n
    tx = np.floor(fx).astype(int); ty = np.floor(fy).astype(int)
    rgb = np.zeros((len(lon), 3), np.float32); have = np.zeros(len(lon), bool)
    for key in set(zip(tx, ty)):
        f = D / str(L) / str(key[0] - X0 * k) / ('%d.jpg' % (key[1] - Y0 * k))
        if not f.exists(): continue
        a = np.asarray(Image.open(f).convert('RGB'), np.float32); Sz = a.shape[0]
        sel = (tx == key[0]) & (ty == key[1])
        px = np.clip((fx[sel] - key[0]) * Sz - 0.5, 0, Sz - 1.001); py = np.clip((fy[sel] - key[1]) * Sz - 0.5, 0, Sz - 1.001)
        x0 = px.astype(int); y0 = py.astype(int); ax = (px - x0)[:, None]; ay = (py - y0)[:, None]
        rgb[sel] = (a[y0, x0] * (1 - ax) + a[y0, x0 + 1] * ax) * (1 - ay) + (a[y0 + 1, x0] * (1 - ax) + a[y0 + 1, x0 + 1] * ax) * ay; have[sel] = True
    print('  photo: %d of %d points' % (have.sum(), len(lon)), flush=True)
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
    R = json.loads((site_dir() / 'roads.json').read_text(encoding='utf-8'))
    lines = [LineString([(p[0], p[1]) for p in r['pts']]) for r in R['roads'] if len(r['pts']) > 1]
    lines = [l for l in lines if not field.buffer(-60).contains(l)]
    band = unary_union([l.buffer(HALF, quad_segs=12) for l in lines] + [Polygon(a).buffer(HALF * 0.5) for a in AREAS])
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
    hf = heights_at(lon, lat, tiles, 17 if SITE else 13)
    hc = heights_at(lon, lat, tiles, COARSE_Z)
    zf = game_z(lon, lat, hf, O, U); zc = game_z(lon, lat, np.where(np.isnan(hc), hf, hc), O, U)
    # the last EASE metres toward the coarser level (what the globe shows round it from mid distance)
    d_edge = np.array([band.exterior.distance(Point(x, y)) if band.geom_type == 'Polygon' else band.boundary.distance(Point(x, y)) for x, y in zip(X, Y)])
    t = np.clip(1.0 - d_edge / EASE, 0, 1); t = t * t * (3 - 2 * t)
    z = zf * (1 - t) + zc * t
    Z = np.full((ny, nx), np.nan, np.float32); Z[inside] = z
    # the colours: the globe's land cover, one pixel a grid point
    from scipy.ndimage import distance_transform_edt
    _, (iy, ix) = distance_transform_edt(~inside, return_indices=True)
    pics = {}
    site_rgb = photo_rgb(lon, lat) if SITE else None   # (a site with its own photo: that, whatever the globe's source)
    for src in (0, 1, 2, 3):
        rgb = site_rgb if SITE else imagery_rgb(lon, lat, src) if src in SOURCES else landcover_rgb(lon, lat)
        img = np.zeros((ny, nx, 3), np.uint8); img[inside] = np.clip(rgb, 0, 255).astype(np.uint8)
        pics[src] = img[iy, ix]    # (outside the band, the nearest colour: no dark rim from filtering at the edge)
        print('  colours from source %d' % src, flush=True)
    def rings(g):
        geoms = [g] if g.geom_type == 'Polygon' else list(g.geoms)
        return [[[round(x, 2), round(y, 2)] for x, y in p.exterior.coords] for p in geoms if p.area > 400]
    head = {'source': 'Tools/make_land_corridor.py (terrain %s, level 13; edge eased to level %d)' % (TILES, COARSE_Z),
            'x0': x0, 'y0': y0, 'step': STEP, 'nx': nx, 'ny': ny, 'cut': rings(cut), 'outline': rings(band)}
    for d in ((site_dir(), ROOT / 'Content' / 'Data' / 'Sites' / SITE) if SITE else (ROOT / 'Data', ROOT / 'Content' / 'Data')):
        (d / 'land-corridor.json').write_text(json.dumps(head))
        Z.astype('<f4').tofile(d / 'land-corridor.bin')
        for src, img in pics.items():   # (row 0 = the north edge, y1, as images run); -0 the default (Sentinel-2)
            Image.fromarray(img[::-1]).save(d / ('land-corridor-%d.png' % src))
        (d / 'land-corridor.png').unlink(missing_ok=True)
    print('land corridor: %d points, ground %.1f to %.1f m, %d cut polygons' % (inside.sum(), np.nanmin(Z), np.nanmax(Z), len(head['cut'])))

if __name__ == '__main__':
    if '--site' in sys.argv: SITE = sys.argv[sys.argv.index('--site') + 1]
    main()
