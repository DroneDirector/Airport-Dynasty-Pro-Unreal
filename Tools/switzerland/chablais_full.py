# The Chablais site in full: (1) the game's whole map area (about 47 x 44 km, as the Iceland map covers) with the
# airport, its approaches, the road and the town; (2) a close map of the airport, today's road behind the terminal
# and the town, placed on the real ground: where the road as drawn today would climb too steeply, a new line for it
# along gentle ground (never steeper than 6 %), the town moved onto flat land, and the levelling the town needs.
import json, math
import numpy as np
from pyproj import Transformer
from scipy.ndimage import map_coordinates, zoom, gaussian_filter, maximum_filter, minimum_filter
from skimage.graph import route_through_array
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import LightSource, LinearSegmentedColormap
from matplotlib.patches import Polygon as MPoly
from shapely.geometry import LineString, Polygon as SPoly, Point
from shapely import affinity

ROOT = '/home/claude/airport-dynasty-pro-unreal'
AF = json.load(open(f'{ROOT}/Data/airfield.json')); TP = json.load(open(f'{ROOT}/Data/terminal-plan.json'))
RD = json.load(open(f'{ROOT}/Data/roads.json')); TB = json.load(open(f'{ROOT}/Data/town-buildings.json'))
SITE = {x['id']: x for x in json.load(open('/home/claude/sites/results_ok.json'))}['3']
cE, cN, HDG = SITE['cE'], SITE['cN'], SITE['hdg']
RW = AF['runway']; RY = RW['y']; RY2 = RY + 1035.0
MAXSLOPE = 0.06                                     # steepest a road may climb (6 %)

# #region ── Heights ──
with open('/home/claude/sites/DHM200.asc') as f:
    hdr = {}
    for _ in range(6):
        k, v = f.readline().split(); hdr[k.upper()] = float(v)
    Z = np.array(f.read().split(), dtype=np.float32).reshape(int(hdr['NROWS']), int(hdr['NCOLS']))
Z[Z == hdr['NODATA_VALUE']] = np.nan
X0, Y0, CS = hdr['XLLCORNER'], hdr['YLLCORNER'], hdr['CELLSIZE']; NR, NC = Z.shape; YTOP = Y0 + NR * CS
TO = Transformer.from_crs('EPSG:4326', 'EPSG:21781', always_xy=True)
LAKE = 372.0


def elev(E, N):
    c = (np.asarray(E) - X0) / CS - 0.5; r = (YTOP - np.asarray(N)) / CS - 0.5
    return map_coordinates(Z, [r, c], order=1, mode='constant', cval=np.nan)
# #endregion


def W(pts):
    """Airport frame -> LV03."""
    a = math.radians(HDG); u = np.array([math.sin(a), math.cos(a)]); v = np.array([-math.cos(a), math.sin(a)])
    P = np.asarray(pts, float).reshape(-1, 2)
    return np.c_[cE + P[:, 0] * u[0] + P[:, 1] * v[0], cN + P[:, 0] * u[1] + P[:, 1] * v[1]]


def rect(x0, y0, x1, y1): return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


RHONE = np.array([TO.transform(lo, la) for la, lo in [(46.215, 7.003), (46.245, 6.990), (46.275, 6.955), (46.300, 6.925),
                                                       (46.330, 6.905), (46.355, 6.890), (46.375, 6.870), (46.395, 6.855)]])

# #region ── A 25 m working grid round the airport and town ──
GW = 13000; G = 25.0
gx0, gy1 = cE - GW, cN + GW                          # top-left corner
nx = ny = int(2 * GW / G)
GXs = gx0 + (np.arange(nx) + 0.5) * G; GYs = gy1 - (np.arange(ny) + 0.5) * G
GX, GY = np.meshgrid(GXs, GYs)
GZ = gaussian_filter(elev(GX.ravel(), GY.ravel()).reshape(GX.shape), 2.0)
gy_, gx_ = np.gradient(GZ, G)
SLOPE = np.hypot(gx_, gy_)
WATERG = np.abs(GZ - LAKE) < 1.0
# keep-out: the airport (with the second runway's area), the river and a strip either side of it
af = AF['boundary']
AIRPORT = SPoly(W(rect(af['x0'] - 150, af['y0'] - 30, af['x1'] + 150, RY2 + 250)))
from shapely import contains_xy, distance, points
KEEP = contains_xy(AIRPORT, GX, GY) | (distance(points(GX, GY), LineString(RHONE)) < 120) | WATERG
# the real villages and towns on the plain stay where they are: the new town goes on open land
VILLAGES = [('Aigle', 46.318, 6.969, 900), ('Roche', 46.360, 6.930, 500), ('Yvorne', 46.331, 6.958, 450), ('Villeneuve', 46.398, 6.928, 700),
            ('Noville', 46.382, 6.898, 450), ('Rennaz', 46.390, 6.917, 450), ('Vouvry', 46.336, 6.889, 550), ('Collombey', 46.271, 6.946, 700),
            ('Monthey', 46.255, 6.954, 1000), ('Ollon', 46.296, 6.993, 500), ('St-Triphon', 46.293, 6.973, 350), ('Chessel', 46.351, 6.896, 350),
            ('Illarsaz', 46.302, 6.925, 300), ('Muraz', 46.283, 6.930, 450), ('Vionnaz', 46.311, 6.900, 450), ('Bex', 46.250, 7.011, 900)]
VILL = np.zeros_like(KEEP)
for nm, la, lo, rr in VILLAGES:
    e_, n_ = TO.transform(lo, la); VILL |= np.hypot(GX - e_, GY - n_) < rr
# #endregion


# #region ── Today's roads, placed as they are ──
def road_world(rd): return W(rd['pts'])
TOWN_ROADS = [rd for rd in RD['roads'] if rd['id'].startswith('TOWN-') and not rd['id'].startswith(('TOWN-ROAD-', 'TOWN-ROUNDABOUT'))]
LINK = [rd for rd in RD['roads'] if rd['id'] in ('TOWN-ROAD-NORTH', 'TOWN-ROUNDABOUT', 'TOWN-ROAD-SOUTH')]
AIRPORT_ROADS = [rd for rd in RD['roads'] if rd['id'].startswith('AIRPORT-')]


def steep_parts(P):
    """Climb between successive points (fraction) and the steepest."""
    z = elev(P[:, 0], P[:, 1]); d = np.hypot(*np.diff(P, axis=0).T)
    s = np.abs(np.diff(z)) / np.maximum(d, 1)
    return s, float(np.nanmax(s)) if len(s) else 0.0


back = [rd for rd in AIRPORT_ROADS if rd['id'] == 'AIRPORT-BACK-TO-TOWN'][0]
BACKW = road_world(back)
s_back, max_back = steep_parts(BACKW)
link_max = max(steep_parts(road_world(rd))[1] for rd in LINK)
TOWNPTS = np.vstack([road_world(rd) for rd in TOWN_ROADS])
town_now_z = elev(TOWNPTS[:, 0], TOWNPTS[:, 1])
print('back road steepest %.1f %%, link %.1f %%, town ground range today %.0f m' % (max_back * 100, link_max * 100,
      np.nanmax(town_now_z) - np.nanmin(town_now_z)))
# #endregion


# #region ── Moving the town onto flat ground (its streets kept, the whole town turned and shifted as one) ──
TOWN_LOCAL = np.vstack([np.asarray(rd['pts'], float) for rd in TOWN_ROADS])
tc = TOWN_LOCAL.mean(axis=0)
entry_local = np.asarray([rd for rd in RD['roads'] if rd['id'] == 'TOWN-RB9'][0]['pts'], float).mean(axis=0)


def town_world(dx, dy, rot):
    """The town's streets (airport frame) turned by rot round their middle and moved to (dx, dy) in the airport frame."""
    a = math.radians(rot); R = np.array([[math.cos(a), -math.sin(a)], [math.sin(a), math.cos(a)]])
    return lambda P: W((np.asarray(P, float) - tc) @ R.T + np.array([dx, dy]))


import os
TARGET = os.environ.get('TOWNAT')
TARGET_P = np.array(TO.transform(6.920, 46.392)) if TARGET else None   # Rennaz / Villeneuve plain
best = None
for rot in range(0, 360, 15):
    for dx in np.arange(-9000, 9001, 400):
        for dy in np.arange(-6000, 6001, 400):
            T = town_world(dx, dy, rot); P = T(TOWN_LOCAL[::6])
            ix = ((P[:, 0] - gx0) / G).astype(int); iy = ((gy1 - P[:, 1]) / G).astype(int)
            if (ix < 0).any() or (iy < 0).any() or (ix >= nx).any() or (iy >= ny).any(): continue
            if KEEP[iy, ix].any() or (VILL[iy, ix].any() and not TARGET): continue
            if TARGET and math.hypot(*(T(tc[None, :])[0] - TARGET_P)) > 1600: continue
            if distance(points(P[:, 0], P[:, 1]), AIRPORT).min() < (800 if TARGET else 2500): continue   # a town of its own, as in Iceland (~6 km by road)
            z = GZ[iy, ix]; rng = float(z.max() - z.min()); sl = float(SLOPE[iy, ix].max())
            if sl > 0.06: continue
            # not under the final approaches (2 km either side of the runways' line, out to 8 km)
            loc = (np.asarray([dx, dy]) )
            # not right under a final approach: no street within 400 m of either runway's extended centre line, 6 km out
            a_ = math.radians(rot); Rm = np.array([[math.cos(a_), -math.sin(a_)], [math.sin(a_), math.cos(a_)]])
            Lp = (TOWN_LOCAL[::6] - tc) @ Rm.T + np.array([dx, dy])
            beyond = (Lp[:, 0] < RW['x0']) & (Lp[:, 0] > RW['x0'] - 6000) | (Lp[:, 0] > RW['x1']) & (Lp[:, 0] < RW['x1'] + 6000)
            if (beyond & ((np.abs(Lp[:, 1] - RY) < 400) | (np.abs(Lp[:, 1] - RY2) < 400))).any(): continue
            dist = math.hypot(dx - tc[0], dy - tc[1])
            score = rng + dist / 2000.0
            if best is None or score < best[0]: best = (score, dx, dy, rot, rng, sl)
score, TDX, TDY, TROT, TRNG, TSL = best
TOWN = town_world(TDX, TDY, TROT)
print('town moved to (%.0f, %.0f) turned %d°: ground range %.0f m, steepest %.1f %%' % (TDX, TDY, TROT, TRNG, TSL * 100))
# #endregion


# #region ── The road from behind the terminal to the town: today's line where it is gentle, a new line elsewhere ──
COST = 1.0 + (SLOPE / 0.02) ** 2
COST[SLOPE > MAXSLOPE] = 1e6
COST[KEEP] = 1e6


def cell(p): return (int((gy1 - p[1]) / G), int((p[0] - gx0) / G))


# keep the back road up to the first point where it gets too steep (or all of it)
bad = np.nonzero(s_back > MAXSLOPE)[0]
keep_n = (bad[0] if len(bad) else len(BACKW) - 1) + 1
start = BACKW[keep_n - 1]
goal = TOWN(entry_local[None, :])[0]
path, cost = route_through_array(COST, cell(start), cell(goal), fully_connected=True, geometric=True)
NEW = np.array([(gx0 + (c + 0.5) * G, gy1 - (r + 0.5) * G) for r, c in path])
for _ in range(4):                                   # round the corners (Chaikin)
    q = 0.75 * NEW[:-1] + 0.25 * NEW[1:]; r_ = 0.25 * NEW[:-1] + 0.75 * NEW[1:]
    NEW = np.vstack([NEW[:1], np.column_stack([q, r_]).reshape(-1, 2), NEW[-1:]])
s_new, max_new = steep_parts(NEW)
L_new = float(np.sum(np.hypot(*np.diff(NEW, axis=0).T)))
print('new road %.1f km, steepest %.1f %%' % (L_new / 1000, max_new * 100))
# #endregion


# #region ── Drawing ──
cmap = LinearSegmentedColormap.from_list('alp', [(0, '#5d8a4a'), (0.12, '#8fae63'), (0.25, '#c9c48a'), (0.42, '#b38b5d'),
                                                 (0.62, '#8c7a6b'), (0.8, '#d8d4cf'), (1, '#ffffff')])


def base(ax, E0, E1, N0, N1, up=1, vex=2.0):
    c0 = int((E0 - X0) / CS); c1 = int((E1 - X0) / CS); r0 = int((YTOP - N1) / CS); r1 = int((YTOP - N0) / CS)
    sub = Z[r0:r1, c0:c1]
    nan = np.isnan(sub)
    fill = np.where(nan, np.nanmin(sub), sub)
    if up > 1: fill = zoom(fill, up, order=3); nan = zoom(nan.astype(float), up, order=0) > 0.5
    ext = [X0 + c0 * CS, X0 + c1 * CS, YTOP - r1 * CS, YTOP - r0 * CS]
    rgb = LightSource(azdeg=315, altdeg=40).shade(fill, cmap=cmap, vert_exag=vex, blend_mode='soft', vmin=LAKE - 5,
                                                  vmax=min(np.nanmax(fill), 3300), dx=CS / up, dy=CS / up)
    rgb[np.abs(fill - LAKE) < 0.8] = [0.33, 0.55, 0.78, 1]
    rgb[nan] = [0.55, 0.55, 0.58, 1]
    ax.imshow(rgb, extent=ext, origin='upper', interpolation='bilinear')
    return fill, ext


def airport(ax, lw=1.0, detail=True):
    def poly(pts, **k): ax.add_patch(MPoly(W(pts), closed=True, **k))
    b = AF['boundary']
    poly(rect(b['x0'], b['y0'], b['x1'], b['y1']), fill=False, ec='#ff5fc8', lw=2.2 * lw, ls='--')
    for y, col, ls in ((RY, 'white', '-'), (RY2, '#7fe3ff', '--')):
        poly(rect(RW['x0'], y - 30, RW['x1'], y + 30), fc='#262626', ec=col, lw=1.2 * lw, ls=ls)
    if not detail: return
    for a in AF['aprons']: poly(rect(a['x0'], a['y0'], a['x1'], a['y1']), fc='#9aa3a8', ec='white', lw=0.6)
    for t in AF['taxiways']:
        c = W(t['pts']); ax.plot(c[:, 0], c[:, 1], color='#e8e8e8', lw=1.6 * lw)
    for rm in TP['rooms']:
        if rm.get('f', 1) == 1: poly(rect(rm['x0'], rm['z0'], rm['x1'], rm['z1']), fc='#45e6ff', ec='none')
    cb = AF['cargoBuilding']; poly(rect(cb['x0'], cb['y0'], cb['x1'], cb['y1']), fc='#e0a43a', ec='none')


def label(ax, p, t, col='white', size=15):
    ax.text(p[0], p[1], t, color=col, fontsize=size, fontweight='bold', ha='center', va='center',
            bbox=dict(boxstyle='round,pad=0.3', fc='#0d1b2edd', ec='none'))


def finish(fig, ax, E0, E1, N0, N1, bar, title, notes):
    ax.set_xlim(E0, E1); ax.set_ylim(N0, N1); ax.set_aspect('equal'); ax.set_xticks([]); ax.set_yticks([])
    w = E1 - E0; h = N1 - N0
    ax.annotate('N', xy=(E1 - w * 0.04, N1 - h * 0.04), xytext=(E1 - w * 0.04, N1 - h * 0.14), ha='center', fontsize=22,
                fontweight='bold', color='white', arrowprops=dict(arrowstyle='-|>', color='white', lw=3))
    x0, y0 = E0 + w * 0.03, N0 + h * 0.04
    ax.plot([x0, x0 + bar], [y0, y0], color='white', lw=6)
    ax.text(x0 + bar / 2, y0 + h * 0.02, f'{bar / 1000:g} km', color='white', ha='center', fontsize=17, fontweight='bold')
    fig.text(0.02, 0.957, title, color='white', fontsize=26, fontweight='bold')
    fig.text(0.02, 0.018, notes, color='#cfe3ff', fontsize=15)


# 1) the whole map area
HALF_E, HALF_N = 23700, 22200
E0, E1, N0, N1 = cE - HALF_E, cE + HALF_E, cN - HALF_N, cN + HALF_N
fig = plt.figure(figsize=(20, 20.5), dpi=105); fig.patch.set_facecolor('#0d1b2e')
ax = fig.add_axes([0.02, 0.1, 0.96, 0.84])
fill, ext = base(ax, E0, E1, N0, N1, up=1, vex=1.6)
gxs = np.linspace(ext[0], ext[1], fill.shape[1]); gys = np.linspace(ext[3], ext[2], fill.shape[0])
cs = ax.contour(gxs, gys, fill, levels=np.arange(500, 4500, 500), colors='k', linewidths=0.45, alpha=0.4)
ax.clabel(cs, fmt='%d m', fontsize=10)
ax.plot(RHONE[:, 0], RHONE[:, 1], color='#2f6fd6', lw=3)
for ry, col in ((RY, '#ffe066'), (RY2, '#7fe3ff')):
    for xt, d in ((RW['x0'], 1), (RW['x1'], -1)):
        pts = [(xt - d * 60, ry - 140), (xt - d * 15060, ry - 140 - 2250), (xt - d * 15060, ry + 140 + 2250), (xt - d * 60, ry + 140)]
        ax.add_patch(MPoly(W(pts), closed=True, fill=False, ec=col, lw=1.2, alpha=0.8))
airport(ax, 1.0, detail=False)
ax.plot(BACKW[:keep_n, 0], BACKW[:keep_n, 1], color='#ffb000', lw=2.5); ax.plot(NEW[:, 0], NEW[:, 1], color='#ffb000', lw=2.5)
for rd in TOWN_ROADS:
    P = TOWN(rd['pts']); ax.plot(P[:, 0], P[:, 1], color='#ffb000', lw=1.2)
for name, la, lo in (('Lake Geneva', 46.43, 6.80), ('Montreux', 46.433, 6.911), ('Villeneuve', 46.398, 6.928), ('Aigle', 46.318, 6.969),
                     ('Monthey', 46.255, 6.954), ('Bex', 46.250, 7.011), ('St-Maurice', 46.217, 7.003), ('Dents du Midi', 46.160, 6.923),
                     ('Grand Muveran', 46.236, 7.124), ('Leysin', 46.343, 7.012), ('Vevey', 46.460, 6.843), ('Le Bouveret', 46.383, 6.857)):
    p = TO.transform(lo, la)
    if E0 < p[0] < E1 and N0 < p[1] < N1: label(ax, p, name, '#ffffff' if name not in ('Dents du Midi', 'Grand Muveran') else '#ffe9b0', 13)
label(ax, W([(0, -900)])[0] + np.array([2600, 0]), 'your airport', '#45e6ff', 16)
label(ax, TOWN(tc[None, :])[0] + np.array([2400, 900]), 'your town', '#ffb000', 15)
finish(fig, ax, E0, E1, N0, N1, 5000, 'Chablais: the whole map area (about 47 x 44 km, like the Iceland map)',
       'Pink: the airport fence. White and cyan bars: runway 1 and the future runway 2. Yellow / cyan outlines: the 15 km approach paths.\n'
       'Orange: the road from behind the terminal to your town (moved onto flat ground). Blue: the Rhône (approximate).\n'
       'Heights: swisstopo DHM200 (200 m grid).')
fig.savefig('/home/claude/sites/out/Chablais-whole-map' + ('-Rennaz' if TARGET else '') + '.png', facecolor=fig.get_facecolor()); plt.close(fig)

# 2) close-up: airport, roads, town, slopes, levelling
OLDT = W(TOWN_LOCAL)
pE = np.r_[W(rect(af['x0'], af['y0'], af['x1'], RY2))[:, 0], NEW[:, 0], TOWN(TOWN_LOCAL)[:, 0], OLDT[:, 0]]
pN = np.r_[W(rect(af['x0'], af['y0'], af['x1'], RY2))[:, 1], NEW[:, 1], TOWN(TOWN_LOCAL)[:, 1], OLDT[:, 1]]
E0, E1, N0, N1 = pE.min() - 1500, pE.max() + 1500, pN.min() - 1500, pN.max() + 1500
w, h = E1 - E0, N1 - N0
if w / h < 1.3: m = (1.3 * h - w) / 2; E0 -= m; E1 += m
else: m = (w / 1.3 - h) / 2; N0 -= m; N1 += m
fig = plt.figure(figsize=(22, 18.5), dpi=105); fig.patch.set_facecolor('#0d1b2e')
ax = fig.add_axes([0.02, 0.12, 0.96, 0.82])
fill, ext = base(ax, E0, E1, N0, N1, up=8, vex=2.5)
gxs = np.linspace(ext[0], ext[1], fill.shape[1]); gys = np.linspace(ext[3], ext[2], fill.shape[0])
ax.contour(gxs, gys, fill, levels=np.arange(400, 3000, 100), colors='k', linewidths=0.4, alpha=0.3)
# ground too steep for a road (over 6 %), hatched red
ax.contourf(GX, GY, SLOPE, levels=[MAXSLOPE, 99], colors=['#ff2020'], alpha=0.16)
ax.contour(GX, GY, SLOPE, levels=[MAXSLOPE], colors='#ff4040', linewidths=0.8, alpha=0.8)
ax.plot(RHONE[:, 0], RHONE[:, 1], color='#2f6fd6', lw=6)
airport(ax, 1.3)
for rd in AIRPORT_ROADS:
    P = road_world(rd); ax.plot(P[:, 0], P[:, 1], color='#ffb000', lw=2.4)
# today's link road and town, as drawn now (faded red where they'd go)
for rd in LINK + TOWN_ROADS:
    P = road_world(rd); ax.plot(P[:, 0], P[:, 1], color='#ff6060', lw=1.6, ls=':', alpha=0.9)
ax.plot(BACKW[:keep_n, 0], BACKW[:keep_n, 1], color='#ffb000', lw=3.5)
ax.plot(NEW[:, 0], NEW[:, 1], color='#ffb000', lw=3.5)
# the town in its new place, and the levelling it needs
TP_ = TOWN(TOWN_LOCAL); hull = SPoly(TP_).convex_hull.buffer(60)
hx, hy = hull.exterior.xy
zt = elev(TP_[:, 0], TP_[:, 1]); level = float(np.nanmedian(zt))
inside = contains_xy(hull, GX, GY)
dz = np.where(inside, GZ - level, np.nan)
cut = np.nanmax(dz); fillm = -np.nanmin(dz)
ax.contourf(GX, GY, np.abs(np.nan_to_num(dz)), levels=[2, 999], colors=['#ff9b30'], alpha=0.35)
ax.plot(hx, hy, color='#ffb000', lw=2, ls='--')
for rd in TOWN_ROADS:
    P = TOWN(rd['pts']); ax.plot(P[:, 0], P[:, 1], color='#ffd27a', lw=1.5)
for nm, b in TB['buildings'].items():
    p = TOWN([b['o']])[0]; ax.plot(p[0], p[1], 's', color='white', ms=7)
label(ax, TOWN(tc[None, :])[0] + np.array([2300 if TARGET else 0, 0 if TARGET else 950]), 'your town, built where Rennaz / Villeneuve are' if TARGET else 'your town (moved onto flat ground)', '#ffb000', 15)
oldc = W(tc[None, :])[0]; label(ax, oldc + np.array([0, 900]), 'the town as drawn today would land here, on the mountain', '#ff8080', 13)
mid = NEW[len(NEW) // 2]
if not TARGET: label(ax, mid + np.array([700, 0]), f'new road {L_new / 1000:.1f} km, never over {max(max_new, 0.001) * 100:.0f} %', '#ffb000', 13)
label(ax, W([(0, -330)])[0] + np.array([900, 300]), 'terminal', '#45e6ff', 13)
for name, la, lo in (('Villeneuve', 46.398, 6.928), ('Aigle', 46.318, 6.969), ('Roche', 46.360, 6.930), ('Monthey', 46.255, 6.954),
                     ('Noville', 46.382, 6.898), ('Vouvry', 46.336, 6.889), ('Yvorne', 46.331, 6.958), ('Collombey', 46.271, 6.946)):
    p = TO.transform(lo, la)
    if E0 < p[0] < E1 and N0 < p[1] < N1: label(ax, p, name, 'white', 12)
finish(fig, ax, E0, E1, N0, N1, 1000, 'Chablais: the airport, the road behind the terminal and the town, fitted to the ground',
       f'Orange: the airport\'s own roads as drawn today, the road behind the terminal (kept as far as it stays gentle), then the new line to the town '
       f'({L_new / 1000:.1f} km, steepest {max_new * 100:.1f} %).\n'
       f'Red dots: the road and town as drawn today, if nothing were changed (today\'s link climbs up to {link_max * 100:.0f} %). '
       f'Red tint: ground steeper than {MAXSLOPE * 100:.0f} % - no road goes there.\n'
       f'Orange shading inside the town line: ground to be levelled by more than 2 m (at most {cut:.0f} m cut, {fillm:.0f} m fill). '
       'Heights: swisstopo DHM200 (200 m grid). Motorway, railway and villages not shown yet.')
fig.savefig('/home/claude/sites/out/Chablais-roads-and-town' + ('-Rennaz' if TARGET else '') + '.png', facecolor=fig.get_facecolor()); plt.close(fig)
print('cut %.1f fill %.1f' % (cut, fillm))
# #endregion
