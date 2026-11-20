"""Where could the globe's terrain poke through our own ground? (owner, 2026-11-20: "our custom mesh must not glitch through
the terrain mesh"). For a site: the land along the roads (land-corridor.bin) where the globe is NOT cut away under it (its
rim: outside the cut polygons) and the ring round the airfield's fence, against the terrain at every level the game may
draw there (12..17). Prints the worst spots; 0 at or under our ground is safe (the globe sits 0.4 m under our z = 0).
  python -I Tools/check_ground_poke.py --site chablais"""
import json, math, sys
from pathlib import Path
import numpy as np
from shapely.geometry import Polygon, Point, box
from shapely.prepared import prep
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'Tools'))
import make_land_corridor as lc
import road_ground as rg

lc.SITE = sys.argv[sys.argv.index('--site') + 1] if '--site' in sys.argv else None
F, H, O, E, N, U = lc.georef()
SD = lc.site_dir()
head = json.loads((SD / 'land-corridor.json').read_text())
Z = np.fromfile(SD / 'land-corridor.bin', '<f4').reshape(head['ny'], head['nx'])
from shapely.ops import unary_union
cut = prep(unary_union([Polygon(c) for c in head['cut']]))
tiles = rg.Tiles('F:/ADP-Terrain/tiles-v2', 17)

def terrain_z(X, Y, z):
    lon, lat = lc.plan_to_lonlat(np.array(X), np.array(Y), H, O, E, N)
    out = []
    for lo, la in zip(lon, lat):
        tiles.finest = z
        s = 180.0 / (1 << z); t = tiles.tile(z, int((lo + 180) / s), int((la + 90) / s))
        h = None
        if t is not None:
            Xs, Ys, Hs, T = t
            x1, y1, x2, y2, x3, y3 = Xs[T[:, 0]], Ys[T[:, 0]], Xs[T[:, 1]], Ys[T[:, 1]], Xs[T[:, 2]], Ys[T[:, 2]]
            d = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3); d[d == 0] = 1e-30
            l1 = ((y2 - y3) * (lo - x3) + (x3 - x2) * (la - y3)) / d; l2 = ((y3 - y1) * (lo - x3) + (x1 - x3) * (la - y3)) / d; l3 = 1 - l1 - l2
            k = np.where((l1 >= -1e-9) & (l2 >= -1e-9) & (l3 >= -1e-9))[0]
            if len(k): i = k[0]; h = l1[i] * Hs[T[i, 0]] + l2[i] * Hs[T[i, 1]] + l3[i] * Hs[T[i, 2]]
        out.append(np.nan if h is None else float(lc.game_z(np.array([lo]), np.array([la]), np.array([h]), O, U)[0]))
    return np.array(out)

# 1) the land's rim: every 4th grid point that has land and lies outside the globe's cut
ys, xs = np.nonzero(~np.isnan(Z))
pts = [(head['x0'] + i * head['step'], head['y0'] + j * head['step'], Z[j, i]) for j, i in zip(ys[::4], xs[::4])]
rim = [p for p in pts if not cut.contains(Point(p[0], p[1]))]
print('land rim: %d points outside the globe cut' % len(rim), flush=True)
# 2) the ring round the fence: 0..150 m outside it, every 25 m (our ground there: the terrain itself - nothing of ours; the
# fence line: our airfield ground at its edge, 0)
B = F['boundary']; fence = box(B['x0'], B['y0'], B['x1'], B['y1'])
ring = []
for x in np.arange(B['x0'], B['x1'] + 1, 25.0):
    for y in (B['y0'], B['y1']): ring.append((x, y, 0.0))
for y in np.arange(B['y0'], B['y1'] + 1, 25.0):
    for x in (B['x0'], B['x1']): ring.append((x, y, 0.0))
report = {}
for name, P in (('land rim', rim), ('fence line', ring)):
    P = np.array(P)
    worst = []
    for z in range(12, 18):
        t = terrain_z(P[:, 0], P[:, 1], z)
        poke = t - P[:, 2]
        k = np.nanargmax(poke)
        worst.append((z, float(np.nanmax(poke)), int(np.nansum(poke > 0.05)), float(P[k, 0]), float(P[k, 1])))
        print('%-10s level %d: worst %+.2f m above our ground, %d of %d points above it (worst at plan %.0f, %.0f)' % (name, z, worst[-1][1], worst[-1][2], len(P), P[k, 0], P[k, 1]), flush=True)
    report[name] = worst
json.dump(report, open(SD / 'ground-poke-check.json', 'w'), indent=1)
