# Places the airport (Data/airfield.json, with a second runway 1,035 m out for independent landings on both) in each
# candidate valley on swisstopo's DHM200 elevation model (200 m grid), finds the flattest spot and heading near the
# first guess, checks the ICAO obstacle surfaces against the mountains, and draws one map per site.
import json, math, sys
import numpy as np
from pyproj import Transformer
from scipy.ndimage import map_coordinates, maximum_filter, minimum_filter
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import LightSource, LinearSegmentedColormap
from matplotlib.patches import Polygon as MPoly

ROOT = '/home/claude/airport-dynasty-pro-unreal'
AF = json.load(open(f'{ROOT}/Data/airfield.json'))
TP = json.load(open(f'{ROOT}/Data/terminal-plan.json'))

# #region ── Elevation model (LV03 metres) ──
with open('/home/claude/sites/DHM200.asc') as f:
    hdr = {}
    for _ in range(6):
        k, v = f.readline().split(); hdr[k.upper()] = float(v)
    Z = np.array(f.read().split(), dtype=np.float32).reshape(int(hdr['NROWS']), int(hdr['NCOLS']))
Z[Z == hdr['NODATA_VALUE']] = np.nan
X0, Y0, CS = hdr['XLLCORNER'], hdr['YLLCORNER'], hdr['CELLSIZE']
NR, NC = Z.shape
YTOP = Y0 + NR * CS
TO = Transformer.from_crs('EPSG:4326', 'EPSG:21781', always_xy=True)


def elev(E, N):
    """Bilinear heights at LV03 points (arrays)."""
    c = (np.asarray(E) - X0) / CS - 0.5; r = (YTOP - np.asarray(N)) / CS - 0.5
    return map_coordinates(Z, [r, c], order=1, mode='constant', cval=np.nan)

# lakes: perfectly flat cells at the known lake levels
LAKES = [372.0, 406.0, 419.0, 395.5, 193.5, 429.0, 433.0, 558.0, 563.0, 447.0]
flat = (maximum_filter(np.nan_to_num(Z, nan=-1), 5) - minimum_filter(np.nan_to_num(Z, nan=-1), 5)) < 0.35
WATER = np.zeros_like(flat)
for lv in LAKES: WATER |= flat & (np.abs(Z - lv) < 1.2)
# #endregion

# #region ── The airport in its own frame (x along the runway, y towards it from the terminal) ──
import os
RW = AF['runway']; RY = RW['y']; SEP = float(os.environ.get('SEP', '1035')); RY2 = RY + SEP if SEP else RY
FOOT = (-2250, 2400, -200, max(RY2 + 170, AF['boundary']['y1']))
ONLY = os.environ.get('ONLY', '12345'); TAG = os.environ.get('TAG', '')          # what must be flat: everything, the safety areas and the 2nd runway


def layout_shapes():
    s = []
    s.append(('boundary', [(AF['boundary']['x0'], AF['boundary']['y0']), (AF['boundary']['x1'], AF['boundary']['y0']),
                           (AF['boundary']['x1'], AF['boundary']['y1']), (AF['boundary']['x0'], AF['boundary']['y1'])]))
    for y, nm in ((RY, 'runway'),) + (((RY2, 'runway2'),) if SEP else ()):
        s.append((nm, [(RW['x0'], y - 30), (RW['x1'], y - 30), (RW['x1'], y + 30), (RW['x0'], y + 30)]))
    for t in AF['taxiways']:
        s.append(('taxi', t['pts'], t.get('w', 23)))
    for a in AF['aprons']:
        s.append(('apron', [(a['x0'], a['y0']), (a['x1'], a['y0']), (a['x1'], a['y1']), (a['x0'], a['y1'])]))
    xs = [r['x0'] for r in TP['rooms']] + [r['x1'] for r in TP['rooms']]
    zs = [r['z0'] for r in TP['rooms']] + [r['z1'] for r in TP['rooms']]
    s.append(('terminal', [(min(xs), min(zs)), (max(xs), min(zs)), (max(xs), max(zs)), (min(xs), max(zs))]))
    return s
SHAPES = layout_shapes()


def to_world(cE, cN, h, pts):
    """Airport frame -> LV03: x along heading h (true), y to its left."""
    a = math.radians(h); u = np.array([math.sin(a), math.cos(a)]); v = np.array([-math.cos(a), math.sin(a)])
    P = np.asarray(pts, float)
    return cE + P[:, 0:1] * u[0] + P[:, 1:2] * v[0], cN + P[:, 0:1] * u[1] + P[:, 1:2] * v[1]


GX, GY = np.meshgrid(np.arange(FOOT[0], FOOT[1] + 1, 100.0), np.arange(FOOT[2], FOOT[3] + 1, 100.0))
GP = np.c_[GX.ravel(), GY.ravel()]


# rivers that must stay outside the airport (approximate lines, lat/lon), and for the Rheintal the Rhine is also the
# border: the whole airport on its Swiss (west) side
import shapely
from shapely.geometry import LineString, Polygon as SPoly
RIVERS = {
    'rhine': [(47.065, 9.490), (47.120, 9.505), (47.170, 9.495), (47.205, 9.505), (47.235, 9.520), (47.255, 9.535),
              (47.290, 9.560), (47.320, 9.585), (47.360, 9.620), (47.385, 9.650), (47.410, 9.650), (47.430, 9.660),
              (47.455, 9.655), (47.500, 9.640)],
    'rhone_chablais': [(46.215, 7.003), (46.245, 6.990), (46.275, 6.955), (46.300, 6.925), (46.330, 6.905), (46.355, 6.890),
                       (46.375, 6.870), (46.395, 6.855)],
    'linth': [(47.135, 9.060), (47.160, 9.020), (47.185, 8.990), (47.205, 8.965), (47.225, 8.940)],
    'rhone_valais': [(46.100, 7.080), (46.120, 7.110), (46.135, 7.135), (46.160, 7.170), (46.180, 7.220), (46.195, 7.260),
                     (46.210, 7.300), (46.225, 7.350)],
}
RIV = {}
for k, pts in RIVERS.items():
    xy = [TO.transform(lo, la) for la, lo in pts]
    line = LineString(xy)
    # the side west / left of the line, closed far away
    (x0_, y0_), (x1_, y1_) = xy[0], xy[-1]
    dx, dy = x1_ - x0_, y1_ - y0_; L = math.hypot(dx, dy); nx, ny = -dy / L * 60000, dx / L * 60000
    left = SPoly(xy + [(x1_ + nx, y1_ + ny), (x0_ + nx, y0_ + ny)])
    RIV[k] = (line, left)
SITE_RIVER = {}


def river_ok(E, N, river, must_left):
    line, left = RIV[river]
    d = shapely.distance(shapely.points(E, N), line)
    if (d < 250).any(): return False
    inl = shapely.contains_xy(left, E, N)
    if must_left: return bool(inl.all())
    return bool(inl.all() or (~inl).all())


def footprint_score(cE, cN, h, river=None):
    E, N = to_world(cE, cN, h, GP)
    if river and not river_ok(E.ravel(), N.ravel(), river[0], river[1]): return 1e9, None
    z = elev(E.ravel(), N.ravel())
    if np.isnan(z).any(): return 1e9, None
    c = (E.ravel() - X0) / CS; r = (YTOP - N.ravel()) / CS
    wet = WATER[np.clip(r.astype(int), 0, NR - 1), np.clip(c.astype(int), 0, NC - 1)].mean()
    rng = np.nanpercentile(z, 99) - np.nanpercentile(z, 1)
    return rng + 400 * wet, z
# #endregion


# #region ── ICAO Annex 14 obstacle surfaces, code 4 precision approach (both runways, both ends) ──
def ols_check(cE, cN, h, ref):
    """Highest terrain above each surface, and where; ref = aerodrome elevation."""
    res = {}
    span = 16000
    xs = np.arange(-span - 2000, span + 2001, 100.0); ys = np.arange(-span / 2 - 4000, span / 2 + 5001, 100.0)
    XX, YY = np.meshgrid(xs, ys); P = np.c_[XX.ravel(), YY.ravel()]
    E, N = to_world(cE, cN, h, P); z = elev(E.ravel(), N.ravel()).reshape(XX.shape)
    out = {}
    for name, y0 in (('main', RY), ('second', RY2)):
        for end, xt, d in (('west end (landing towards the other end)', RW['x0'], 1), ('east end', RW['x1'], -1)):
            # approach from outside this end: s = distance outward from 60 m before the threshold
            s = (xt - XX) * d - 60.0
            half = 140 + 0.15 * np.clip(s, 0, None)
            inside = (s >= 0) & (s <= 15000) & (np.abs(YY - y0) <= half)
            surf = np.where(s <= 3000, 0.02 * s, np.where(s <= 6600, 60 + 0.025 * (s - 3000), 150.0))
            pen = np.where(inside, z - (ref + surf), -np.inf)
            i = np.nanargmax(pen); out[(name, end)] = (float(pen.flat[i]), float(s.flat[i]))
    # inner horizontal (45 m, 4 km round the strips) and the conical (5 % up to +145 m)
    dx = np.clip(np.maximum(RW['x0'] - 60 - XX, XX - RW['x1'] - 60), 0, None)
    dy = np.clip(np.maximum(RY - YY, YY - RY2), 0, None) - 140
    dist = np.hypot(dx, np.clip(dy, 0, None))
    ih = np.where(dist <= 4000, 45.0, np.where(dist <= 6000, 45 + 0.05 * (dist - 4000), np.inf))
    pen = z - (ref + ih)
    m = np.isfinite(pen) & (dist <= 6000)
    out['ring'] = (float(np.nanmax(np.where(m, pen, -np.inf))), float(np.nanmean(pen[m] > 0)))
    # nearest terrain 300 m above the airport, measured sideways from the runways' middle
    hi = z > ref + 300
    dd = np.where(hi, np.hypot(np.clip(np.abs(XX - 80) - 2000, 0, None), np.abs(YY - (RY + RY2) / 2)), np.inf)
    out['mtn'] = float(dd.min())
    out['grid'] = (XX, YY, z)
    return out
# #endregion

SITES = [
    ('1', 'Rheintal - Buchs / Sennwald (SG)', 47.225, 9.475, 20, ('rhine', True)),
    ('2', 'Rheintal north - Oberriet / Altstaetten (SG)', 47.350, 9.585, 25, ('rhine', True)),
    ('3', 'Chablais plain - Aigle / Monthey (VD/VS)', 46.330, 6.920, 335, ('rhone_chablais', False)),
    ('4', 'Linth plain - Bilten / Benken (GL/SG/SZ)', 47.190, 9.000, 300, ('linth', False)),
    ('5', 'Valais Rhone valley - Saxon / Riddes (VS)', 46.150, 7.180, 60, ('rhone_valais', False)),
]


# #region ── Searching: flat enough (and dry), then the longest straight-in approaches from both ends ──
SS = np.arange(0, 15001, 250.0); FR = np.linspace(-1, 1, 9)
APP = []
for end, xt, d in (('W', RW['x0'], 1), ('E', RW['x1'], -1)):
    for s_ in SS:
        half = 140 + 0.15 * s_
        for f_ in FR:
            APP.append((end, s_, xt - d * (s_ + 60), RY + f_ * half))
APP_END = np.array([1 if a[0] == 'E' else 0 for a in APP]); APP_S = np.array([a[1] for a in APP])
APP_P = np.array([(a[2], a[3]) for a in APP])
APP_SURF = np.where(APP_S <= 3000, 0.02 * APP_S, np.where(APP_S <= 6600, 60 + 0.025 * (APP_S - 3000), 150.0))


def clear_runs(cE, cN, h, ref, tol=15.0):
    """How far out (m) each straight-in approach stays clear: terrain more than tol above the approach surface,
    measured from each threshold's own height."""
    E, N = to_world(cE, cN, h, APP_P)
    z = elev(E.ravel(), N.ravel())
    Et, Nt = to_world(cE, cN, h, [(RW['x0'], RY), (RW['x1'], RY)])
    zt = elev(Et.ravel(), Nt.ravel())
    out = []
    for e in (0, 1):
        m = (APP_END == e)
        pen = z[m] - (zt[e] + APP_SURF[m])
        bad = pen > tol
        out.append(float(APP_S[m][bad].min()) if bad.any() else 15000.0)
    return out


def search(E0, N0, river=None, radius=4000, step=400):
    cands = []
    for hh in range(0, 360, 3):
        for de in np.arange(-radius, radius + 1, step):
            for dn in np.arange(-radius, radius + 1, step):
                sc, z = footprint_score(E0 + de, N0 + dn, hh, river)
                if sc <= 14: cands.append((sc, E0 + de, N0 + dn, hh, float(np.nanmin(z))))
    best = None
    for sc, cE, cN, hh, ref in cands:
        w, e = clear_runs(cE, cN, hh, ref)
        key = (min(w, e), max(w, e), -sc)
        if best is None or key > best[0]: best = (key, cE, cN, hh, ref, sc, w, e)
    return best, len(cands)
# #endregion

results = []
for sid, name, lat, lon, h0, river in SITES:
    if sid not in ONLY: continue
    E0, N0 = TO.transform(lon, lat)
    best, ncand = search(E0, N0, None if os.environ.get('NORIVER') else river)
    if best is None:
        print(sid, name, 'no flat spot found', flush=True); continue
    key, cE, cN, hh, ref0, sc, cw, ce = best
    _, zf = footprint_score(cE, cN, hh)
    ref = float(np.nanmin(zf))
    E, N = to_world(cE, cN, hh, [(RW['x0'], RY), (RW['x1'], RY)])
    zr = elev(E.ravel(), N.ravel())
    o = ols_check(cE, cN, hh, ref)
    lonc, latc = Transformer.from_crs('EPSG:21781', 'EPSG:4326', always_xy=True).transform(cE, cN)
    r = dict(id=sid, name=name, cE=cE, cN=cN, hdg=hh, lat=latc, lon=lonc, elev=ref, rng=float(np.nanmax(zf) - np.nanmin(zf)),
             p99=float(np.nanpercentile(zf, 99) - np.nanpercentile(zf, 1)), slope=float(abs(zr[1] - zr[0]) / 4000 * 100),
             wet=float(sc - (np.nanpercentile(zf, 99) - np.nanpercentile(zf, 1))) / 400, clearW=cw, clearE=ce, ncand=ncand, ols={str(k): v for k, v in o.items() if k != 'grid'})
    results.append((r, o))
    print(sid, name, f"cands {ncand} clear W {cw:.0f} E {ce:.0f} hdg {hh:.0f} elev {ref:.0f} range {r['rng']:.1f} (p {r['p99']:.1f}) slope {r['slope']:.2f}% water {r['wet']:.2f}",
          'mtn', round(o['mtn']), 'ring', [round(x, 2) for x in o['ring']],
          {k: (round(v[0]), round(v[1])) for k, v in o.items() if isinstance(k, tuple)}, flush=True)

json.dump([r for r, o in results], open('/home/claude/sites/results.json', 'w'), indent=1, default=float)
import pickle; pickle.dump([(r, {k: v for k, v in o.items() if k != 'grid'}) for r, o in results], open('/home/claude/sites/res.pkl', 'wb'))


# #region ── Maps ──
TO_LL = Transformer.from_crs('EPSG:21781', 'EPSG:4326', always_xy=True)
cmap = LinearSegmentedColormap.from_list('alp', [(0, '#5d8a4a'), (0.12, '#8fae63'), (0.25, '#c9c48a'), (0.42, '#b38b5d'),
                                                 (0.62, '#8c7a6b'), (0.8, '#d8d4cf'), (1, '#ffffff')])


def coverage(cE, cN, h):
    E, N = to_world(cE, cN, h, APP_P)
    z = elev(E.ravel(), N.ravel())
    return [float(np.isnan(z[APP_END == e]).mean()) for e in (0, 1)]


def draw(r, path):
    cE, cN, hh = r['cE'], r['cN'], r['hdg']
    W, H = 34000, 26000
    c0 = int((cE - W / 2 - X0) / CS); c1 = int((cE + W / 2 - X0) / CS)
    r0 = int((YTOP - (cN + H / 2)) / CS); r1 = int((YTOP - (cN - H / 2)) / CS)
    sub = Z[max(r0, 0):r1, max(c0, 0):c1].copy(); wat = WATER[max(r0, 0):r1, max(c0, 0):c1]
    ext = [X0 + max(c0, 0) * CS, X0 + c1 * CS, YTOP - r1 * CS, YTOP - max(r0, 0) * CS]
    fig, ax = plt.subplots(figsize=(22, 13.5), dpi=110)
    fig.patch.set_facecolor('#0d1b2e')
    ls = LightSource(azdeg=315, altdeg=40)
    vmin, vmax = np.nanmin(sub), min(np.nanmax(sub), 3600)
    filled = np.where(np.isnan(sub), vmin, sub)
    rgb = ls.shade(filled, cmap=cmap, vert_exag=2.0, blend_mode='soft', vmin=vmin, vmax=vmax, dx=CS, dy=CS)
    rgb[wat] = [0.33, 0.55, 0.78, 1]
    rgb[np.isnan(sub)] = [0.55, 0.55, 0.58, 1]
    ax.imshow(rgb, extent=ext, origin='upper', interpolation='bilinear')
    cs = ax.contour(np.linspace(ext[0], ext[1], sub.shape[1]), np.linspace(ext[3], ext[2], sub.shape[0]), filled,
                    levels=np.arange(500, 4000, 500), colors='k', linewidths=0.5, alpha=0.45)
    ax.clabel(cs, fmt='%d m', fontsize=10)
    # approach surfaces (15 km each end): clear part solid, beyond the first obstacle dashed
    for ry, col in ((RY, '#ffe066'),) + (((RY2, '#7fe3ff'),) if SEP else ()):
        for xt, d, clear in ((RW['x0'], 1, r['clearW']), (RW['x1'], -1, r['clearE'])):
            for lo, hi, st in ((0, clear, '-'), (clear, 15000, ':')):
                if hi <= lo: continue
                pts = [(xt - d * (60 + lo), ry - 140 - .15 * lo), (xt - d * (60 + hi), ry - 140 - .15 * hi),
                       (xt - d * (60 + hi), ry + 140 + .15 * hi), (xt - d * (60 + lo), ry + 140 + .15 * lo)]
                E, N = to_world(cE, cN, hh, pts)
                ax.add_patch(MPoly(np.c_[E.ravel(), N.ravel()], closed=True, fill=False, ec=col, lw=1.4, ls=st, alpha=0.9))
    for k, (line, left) in RIV.items():
        xy = np.array(line.coords); ax.plot(xy[:, 0], xy[:, 1], color='#2f6fd6', lw=3, alpha=0.85)
    def layout(ax, lwk=1.0):
      for sh in SHAPES:
        kind, pts = sh[0], sh[1]
        if kind == 'taxi':
            E, N = to_world(cE, cN, hh, pts); ax.plot(E.ravel(), N.ravel(), color='#f2f2f2', lw=1.1 * lwk, alpha=0.9); continue
        E, N = to_world(cE, cN, hh, pts); P = np.c_[E.ravel(), N.ravel()]
        sty = {'boundary': dict(fill=False, ec='#ff5fc8', lw=2.2 * lwk, ls='--'), 'runway': dict(fc='#1a1a1a', ec='white', lw=1.5 * lwk),
               'runway2': dict(fc='#1a1a1a', ec='#7fe3ff', lw=1.5 * lwk, ls='--'), 'apron': dict(fc='#bbbbbb', ec='white', lw=0.6 * lwk),
               'terminal': dict(fc='#45e6ff', ec='white', lw=1.0 * lwk)}[kind]
        ax.add_patch(MPoly(P, closed=True, **sty))
    layout(ax)
    # the zoomed-in airport
    ix = fig.add_axes([0.70, 0.565, 0.27, 0.36]); ix.imshow(rgb, extent=ext, origin='upper', interpolation='bilinear')
    for k, (line, left) in RIV.items():
        xy = np.array(line.coords); ix.plot(xy[:, 0], xy[:, 1], color='#2f6fd6', lw=4, alpha=0.85)
    layout(ix, 1.6)
    ix.set_xlim(cE - 3600, cE + 3600); ix.set_ylim(cN - 3600, cN + 3600); ix.set_aspect('equal'); ix.set_xticks([]); ix.set_yticks([])
    for sp in ix.spines.values(): sp.set_edgecolor('white'); sp.set_linewidth(2.5)
    ix.set_title('the airport, zoomed in', color='white', fontsize=15)
    ax.set_xlim(cE - W / 2, cE + W / 2); ax.set_ylim(cN - H / 2, cN + H / 2); ax.set_aspect('equal')
    ax.set_xticks([]); ax.set_yticks([])
    # north arrow and scale (5 km)
    ax.annotate('N', xy=(cE + W / 2 - 1500, cN + H / 2 - 1200), xytext=(cE + W / 2 - 1500, cN + H / 2 - 3800), ha='center',
                fontsize=20, fontweight='bold', color='white', arrowprops=dict(arrowstyle='-|>', color='white', lw=3))
    x0, y0 = cE - W / 2 + 1200, cN - H / 2 + 1200
    ax.plot([x0, x0 + 5000], [y0, y0], color='white', lw=5); ax.text(x0 + 2500, y0 + 500, '5 km', color='white', ha='center', fontsize=16, fontweight='bold')
    rw1 = f"{(hh % 360) / 10:02.0f}"
    ax.set_title(f"{r['id']}. {r['name']}", color='white', fontsize=24, fontweight='bold', loc='left', pad=14)
    cov = r['nodata']
    txt = (f"Airport elevation  {r['elev']:.0f} m ({r['elev'] * 3.281:.0f} ft)      Runways {int(round(hh / 10)) % 36 or 36:02d}/{(int(round(hh / 10)) + 18) % 36 or 36:02d}  (true {hh:.0f}°)\n"
           f"Ground under the whole airport (both runways): {r['p99']:.0f} m from lowest to highest, runway slope {r['slope']:.2f} %\n"
           f"Straight-in approach clear (ICAO surface): from the {r['dirW']} {r['clearW'] / 1000:.1f} km, from the {r['dirE']} {r['clearE'] / 1000:.1f} km\n"
           f"Mountains 300 m above the airport begin {r['ols']['mtn'] / 1000:.1f} km to the side   ·   centre {r['lat']:.4f} N, {r['lon']:.4f} E")
    if r.get('note'): txt += '\n' + r['note']
    if max(cov) > 0.02: txt += f"\n(grey = outside Switzerland, no height data there: {max(cov) * 100:.0f} % of an approach not checked)"
    fig.text(0.02, 0.012, txt, color='white', fontsize=15, va='bottom', family='DejaVu Sans')
    plt.subplots_adjust(left=0.02, right=0.68, top=0.93, bottom=0.17)
    fig.savefig(path, facecolor=fig.get_facecolor()); plt.close(fig)


def compass(h):
    return ['north', 'north-east', 'east', 'south-east', 'south', 'south-west', 'west', 'north-west'][int(((h % 360) + 22.5) // 45) % 8]


for r, o in results:
    r['nodata'] = coverage(r['cE'], r['cN'], r['hdg'])
    # the 'W' end is the x0 end: aircraft landing there come from beyond it, i.e. from the opposite of the heading
    r['dirW'] = compass(r['hdg'] + 180); r['dirE'] = compass(r['hdg'])
    if os.environ.get('NORIVER'): r['note'] = 'Does NOT fit your rule: no flat spot with the river outside the airport - the river (blue) would have to be moved.'
    r['sep'] = SEP; draw(r, f"/home/claude/sites/Site-{r['id']}{TAG}.png")
    print('drew', r['id'], r['nodata'], flush=True)
json.dump([r for r, o in results], open(f'/home/claude/sites/results{TAG}.json', 'w'), indent=1, default=float)
# #endregion
