# The sea round the airport as a water surface that meets the real coastline: the open sea (and the lagoons joined to
# it) from ESA WorldCover 2021 (10 m, CC BY 4.0: "ESA WorldCover project 2021 / Contains modified Copernicus Sentinel
# data (2021) processed by ESA WorldCover consortium"), the water class (80) joined to the open sea; lakes and rivers
# stay as they are. Anything outside the land tiles is open sea.
# The surface: large squares out at sea (at most 1.28 km, so the Earth's curve never shows through), smaller ones
# towards the shore, and the coastline itself traced smoothly through each 20 m square (marching squares), so the edge
# follows the beach and not a staircase. Each corner carries how close it is to the shore (1 at the waterline, 0 from
# 150 m out), which the water's material uses for the breaking waves and the foam.
# Output: Content/Data/ocean.bin: "ADPO", version, count of vertices, count of triangles, then per vertex lon, lat
# (float64) and shore (float32), then the triangles (uint32 x 3).
#   python Tools/MakeOcean.py [--lat 63.457988 --lon -18.713703 --km 30]
import argparse, struct, glob, os, math
import numpy as np
import rasterio
from rasterio.windows import from_bounds
from scipy import ndimage

ap = argparse.ArgumentParser()
ap.add_argument('--lat', type=float, default=63.457988)
ap.add_argument('--lon', type=float, default=-18.713703)
ap.add_argument('--km', type=float, default=30.0)
ap.add_argument('--wc', default='F:/ADP-Terrain/worldcover')
ap.add_argument('--out', default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Content', 'Data', 'ocean.bin'))
a = ap.parse_args()

dlat = a.km / 111.32
dlon = a.km / (111.32 * math.cos(math.radians(a.lat)))
S, N, W, E = a.lat - dlat, a.lat + dlat, a.lon - dlon, a.lon + dlon
RES = 1 / 12000.0           # WorldCover: 1/12000 of a degree per pixel (about 10 m north-south)
H, Wd = int(round((N - S) / RES)), int(round((E - W) / RES))
print('region', S, N, W, E, 'pixels', Wd, 'x', H)

# ---- the water mask (10 m) ----------------------------------------------------------------------------------------
cls = np.full((H, Wd), 80, np.uint8)          # no land tile: open sea
for lat0 in range(int(math.floor(S / 3) * 3), int(math.floor(N / 3) * 3) + 1, 3):
    for lon0 in range(int(math.floor(W / 3) * 3), int(math.floor(E / 3) * 3) + 1, 3):
        name = 'ESA_WorldCover_10m_2021_v200_%s%02d%s%03d_Map.tif' % ('N' if lat0 >= 0 else 'S', abs(lat0), 'E' if lon0 >= 0 else 'W', abs(lon0))
        p = os.path.join(a.wc, name)
        if not os.path.exists(p): continue
        with rasterio.open(p) as r:
            s, n, w, e = max(S, lat0), min(N, lat0 + 3), max(W, lon0), min(E, lon0 + 3)
            if s >= n or w >= e: continue
            win = from_bounds(w, s, e, n, r.transform)
            d = r.read(1, window=win.round_offsets().round_lengths(), boundless=True, fill_value=80)
            r0 = int(round((N - n) / RES)); c0 = int(round((w - W) / RES))
            hh, ww = min(d.shape[0], H - r0), min(d.shape[1], Wd - c0)
            cls[r0:r0 + hh, c0:c0 + ww] = d[:hh, :ww]
            print('read', name, d.shape)
water = cls == 80
lab, n = ndimage.label(water)
edge = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
# the open sea: the water touching the region's edge with the most area (and any other big edge-touching body)
sizes = ndimage.sum(water, lab, index=list(edge)) if edge else []
keep = [l for l, sz in zip(edge, sizes) if sz > 2e5]
sea = np.isin(lab, keep)
print('sea pixels', int(sea.sum()), 'of', sea.size)

# ---- corners every 20 m: smoothed sea fraction, distance to land -------------------------------------------------------
F = 2
seaf = ndimage.uniform_filter(sea.astype(np.float32), size=3)
corner = seaf[::F, ::F]
CH, CW = corner.shape
dist = ndimage.distance_transform_edt(sea) * 10.0      # metres from land (10 m pixels)
shore_px = np.clip(1.0 - dist / 150.0, 0, 1).astype(np.float32)
shore = shore_px[::F, ::F]
cell_deg = RES * F
def lonlat(i, j): return W + j * cell_deg, N - i * cell_deg

verts, vid, tris = [], {}, []
def V(i, j):
    k = (i, j)
    if k not in vid:
        lo, la = lonlat(i, j); vid[k] = len(verts); verts.append((lo, la, float(shore[min(i, CH - 1), min(j, CW - 1)])))
    return vid[k]
def VE(i0, j0, i1, j1, t):
    # a point on a cell edge (the coastline crossing), its shore value 1
    k = ('e', i0, j0, i1, j1, round(t, 3))
    if k not in vid:
        lo0, la0 = lonlat(i0, j0); lo1, la1 = lonlat(i1, j1)
        vid[k] = len(verts); verts.append((lo0 + (lo1 - lo0) * t, la0 + (la1 - la0) * t, 1.0))
    return vid[k]

full = corner > 0.5
cellw = full[:-1, :-1] & full[1:, :-1] & full[:-1, 1:] & full[1:, 1:]     # all four corners in the sea
cellm = (full[:-1, :-1] | full[1:, :-1] | full[:-1, 1:] | full[1:, 1:]) & ~cellw   # the coastline passes through
# the quadtree over whole-sea cells: squares of 1, 2, 4 … 64 cells (20 m … 1.28 km), largest first
used = np.zeros_like(cellw)
ci, cj = cellw.shape
for size in (64, 32, 16, 8, 4, 2, 1):
    for i in range(0, ci - size + 1, size):
        for j in range(0, cj - size + 1, size):
            if used[i, j] or not cellw[i:i + size, j:j + size].all() or used[i:i + size, j:j + size].any(): continue
            used[i:i + size, j:j + size] = True
            a0, b0, c0, d0 = V(i, j), V(i, j + size), V(i + size, j + size), V(i + size, j)
            tris += [(a0, c0, b0), (a0, d0, c0)]
# the coastline's cells: the part in the sea, its edge where the smoothed fraction crosses one half
for i, j in zip(*np.nonzero(cellm)):
    cs = [(i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j)]
    poly = []
    for k in range(4):
        p, q = cs[k], cs[(k + 1) % 4]
        vp, vq = corner[p], corner[q]
        if vp > 0.5: poly.append(V(*p))
        if (vp > 0.5) != (vq > 0.5):
            t = (0.5 - vp) / (vq - vp)
            poly.append(VE(p[0], p[1], q[0], q[1], t) if (p < q) else VE(q[0], q[1], p[0], p[1], 1 - t))
    for k in range(1, len(poly) - 1): tris.append((poly[0], poly[k + 1], poly[k]))

os.makedirs(os.path.dirname(a.out), exist_ok=True)
with open(a.out, 'wb') as f:
    f.write(b'ADPO'); f.write(struct.pack('<III', 1, len(verts), len(tris)))
    f.write(np.array([(v[0], v[1]) for v in verts], np.float64).tobytes())
    f.write(np.array([v[2] for v in verts], np.float32).tobytes())
    f.write(np.array(tris, np.uint32).tobytes())
print('ocean', len(verts), 'vertices', len(tris), 'triangles ->', a.out, os.path.getsize(a.out) // 1024, 'KB')
