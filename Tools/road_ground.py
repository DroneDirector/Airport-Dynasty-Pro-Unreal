# The ground under the roads outside the airfield, read straight from the terrain tiles (owner, 2026-11-14): every road
# point every 10 m (and 12 m either side) and every car park every 10 m, outside the airfield's boundary (less 70 m), its
# height taken from the finest tile there (level 13: the ground a driver sees), turned into the game's own height (Unreal's
# z: the globe's georeference sits at the airport's height + 0.4 m, Globe.cpp, and the Earth curves away from it - about
# 2 m at 5 km). Written to Data/road-ground.json (and Content/Data), which RoadNetwork.cpp lays the roads on.
# MakeTerrain.py runs it at the end of every build; run it by hand after changing the roads (Tools/make_roads.py).
#   python Tools/road_ground.py [--tiles F:/ADP-Terrain/tiles-v2]
import argparse, gzip, json, math, struct
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
A_, F_ = 6378137.0, 1 / 298.257223563
E2 = F_ * (2 - F_)

def ecef(lon, lat, h):
    lo, la = math.radians(lon), math.radians(lat)
    n = A_ / math.sqrt(1 - E2 * math.sin(la) ** 2)
    return np.array([(n + h) * math.cos(la) * math.cos(lo), (n + h) * math.cos(la) * math.sin(lo), (n * (1 - E2) + h) * math.sin(la)])

def geodetic(p):
    x, y, z = p; lon = math.atan2(y, x); r = math.hypot(x, y); lat = math.atan2(z, r * (1 - E2))
    for _ in range(6):
        n = A_ / math.sqrt(1 - E2 * math.sin(lat) ** 2); h = r / math.cos(lat) - n; lat = math.atan2(z, r * (1 - E2 * n / (n + h)))
    return math.degrees(lon), math.degrees(lat), h

def zigzag(a):
    a = a.astype(np.int64); d = (a >> 1) ^ -(a & 1); return np.cumsum(d)

class Tiles:
    def __init__(self, root): self.root = Path(root); self.cache = {}
    def tile(self, z, x, y):
        k = (z, x, y)
        if k in self.cache: return self.cache[k]
        p = self.root / str(z) / str(x) / f'{y}.terrain'
        if not p.exists(): self.cache[k] = None; return None
        b = gzip.decompress(p.read_bytes())
        minh, maxh = struct.unpack_from('<ff', b, 24); n = struct.unpack_from('<I', b, 88)[0]
        arr = np.frombuffer(b, '<u2', 3 * n, 92)
        u, v, h = zigzag(arr[:n]), zigzag(arr[n:2 * n]), zigzag(arr[2 * n:])
        off = 92 + 6 * n
        if n > 65536: off += (4 - off % 4) % 4; tc = struct.unpack_from('<I', b, off)[0]; idx = np.frombuffer(b, '<u4', tc * 3, off + 4).astype(np.int64)
        else: off += (2 - off % 2) % 2; tc = struct.unpack_from('<I', b, off)[0]; idx = np.frombuffer(b, '<u2', tc * 3, off + 4).astype(np.int64)
        hi, ind = 0, np.empty(len(idx), np.int64)
        for i, c in enumerate(idx):
            ind[i] = hi - c
            if c == 0: hi += 1
        s = 180.0 / (1 << z)
        t = (-180 + x * s + u / 32767.0 * s, -90 + y * s + v / 32767.0 * s, minh + h / 32767.0 * (maxh - minh), ind.reshape(-1, 3))
        self.cache[k] = t; return t
    def height(self, lon, lat):
        for z in range(13, 8, -1):   # the finest level there
            s = 180.0 / (1 << z); t = self.tile(z, int((lon + 180) / s), int((lat + 90) / s))
            if t is None: continue
            X, Y, H, T = t
            x1, y1, x2, y2, x3, y3 = X[T[:, 0]], Y[T[:, 0]], X[T[:, 1]], Y[T[:, 1]], X[T[:, 2]], Y[T[:, 2]]
            d = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3); d[d == 0] = 1e-30
            l1 = ((y2 - y3) * (lon - x3) + (x3 - x2) * (lat - y3)) / d; l2 = ((y3 - y1) * (lon - x3) + (x1 - x3) * (lat - y3)) / d; l3 = 1 - l1 - l2
            k = np.where((l1 >= -1e-9) & (l2 >= -1e-9) & (l3 >= -1e-9))[0]
            if len(k): i = k[0]; return float(l1[i] * H[T[i, 0]] + l2[i] * H[T[i, 1]] + l3[i] * H[T[i, 2]])
        return None

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--tiles', default='F:/ADP-Terrain/tiles-v2'); a = ap.parse_args()
    F = json.loads((ROOT / 'Data' / 'airfield.json').read_text(encoding='utf-8')); ap_ = F['airport']; rw = F['runway']
    AH = json.loads((ROOT / 'Data' / 'terrain.json').read_text())['airportHeight']
    # the origin, as Globe.cpp places it: the runway's middle at the airport's coordinates, the plan turned to its heading
    H = math.radians(ap_['hdg']); mx, my = (rw['x0'] + rw['x1']) / 2, rw['y']
    east0 = -mx * math.sin(H) - my * math.sin(H - math.pi / 2); north0 = -mx * math.cos(H) - my * math.cos(H - math.pi / 2)
    lat0 = ap_['lat'] + north0 / 111320.0; lon0 = ap_['lon'] + east0 / (111320.0 * math.cos(math.radians(ap_['lat'])))
    h0 = AH + 0.4
    O = ecef(lon0, lat0, h0); lo, la = math.radians(lon0), math.radians(lat0)
    E = np.array([-math.sin(lo), math.cos(lo), 0]); N = np.array([-math.sin(la) * math.cos(lo), -math.sin(la) * math.sin(lo), math.cos(la)]); U = np.cross(E, N)
    tiles = Tiles(a.tiles)
    def game_height(x, y):
        """A plan point's ground, as the game's z: from its place on the Earth (east/north in the georeference's plane)."""
        e = x * math.sin(H) + y * math.sin(H - math.pi / 2); n = x * math.cos(H) + y * math.cos(H - math.pi / 2)
        p = O + E * e + N * n
        lon, lat, _ = geodetic(p)
        hh = tiles.height(lon, lat)
        if hh is None: return None
        return float(np.dot(ecef(lon, lat, hh) - O, U))   # (the Earth's curve included)
    R = json.loads((ROOT / 'Data' / 'roads.json').read_text(encoding='utf-8'))
    B = F['boundary']
    out_ = lambda x, y: min(x - B['x0'], B['x1'] - x, y - B['y0'], B['y1'] - y) < 70.0
    want = []
    for r in R['roads']:
        P = r['pts']
        for i in range(len(P) - 1):
            dx, dy = P[i + 1][0] - P[i][0], P[i + 1][1] - P[i][1]; L = math.hypot(dx, dy)
            if L < 0.01: continue
            nx, ny = -dy / L, dx / L
            for k in range(int(L // 10) + 1):
                for o in (-12.0, 0.0, 12.0):
                    x, y = P[i][0] + dx * k * 10 / L + nx * o, P[i][1] + dy * k * 10 / L + ny * o
                    if out_(x, y): want.append((x, y))
    for lot in R.get('lots', []):
        x = lot['x0'] - 10
        while x <= lot['x1'] + 10:
            y = lot['y0'] - 10
            while y <= lot['y1'] + 10:
                if out_(x, y): want.append((x, y))
                y += 10
            x += 10
    pts = []
    for x, y in want:
        z = game_height(x, y)
        if z is not None: pts.append([round(x, 1), round(y, 1), round(z, 2)])
    # the ground the game actually draws, where it was measured (ADPHUD -RoadGroundProbe: Data/road-ground-probe.json,
    # 2026-11-14): the tiles' heights read here differ from it by up to 3 m, more toward the town (the road floated
    # there), in a way no single cause explained - so every point is corrected to the measurements: the difference at
    # the measured points, spread to the rest by distance (the 6 nearest within 400 m)
    PR = ROOT / 'Data' / 'road-ground-probe.json'
    if PR.exists() and pts:
        from scipy.spatial import cKDTree
        M = np.array(json.loads(PR.read_text())['pts']); Q = np.array(pts)
        t = cKDTree(Q[:, :2]); d, i = t.query(M[:, :2]); ok = d < 3.0
        Mx, Md = M[ok, :2], M[ok, 2] - Q[i[ok], 2]
        tm = cKDTree(Mx); dd, ii = tm.query(Q[:, :2], k=min(6, len(Mx)))
        w = 1.0 / np.maximum(dd, 1.0) ** 2; w[dd > 400.0] = 0.0
        corr = np.where(w.sum(1) > 0, (w * Md[ii]).sum(1) / np.maximum(w.sum(1), 1e-12), Md[ii[:, 0]])
        for k in range(len(pts)): pts[k][2] = round(pts[k][2] + float(corr[k]), 2)
        print('road_ground: corrected to %d measured points (difference %.2f to %.2f m)' % (len(Mx), Md.min(), Md.max()))
    J = json.dumps({'source': 'Tools/road_ground.py (terrain tiles %s, level 13)' % a.tiles, 'pts': pts})
    for d in (ROOT / 'Data', ROOT / 'Content' / 'Data'): (d / 'road-ground.json').write_text(J)
    zs = [p[2] for p in pts]
    print('road_ground: %d of %d points from the tiles (ground %.1f to %.1f m)' % (len(pts), len(want), min(zs), max(zs)))

if __name__ == '__main__':
    main()
