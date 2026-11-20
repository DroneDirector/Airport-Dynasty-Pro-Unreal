# The airport's ground layout (runway, taxiways, aprons, stands, terminal, cargo, hangars, roads, car parks, the
# future second runway) drawn on the real terrain of each site that fits, with 10 m height lines to show how flat it is.
import json, math
import numpy as np
from pyproj import Transformer
from scipy.ndimage import map_coordinates, zoom
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import LightSource, LinearSegmentedColormap
from matplotlib.patches import Polygon as MPoly
from shapely.geometry import LineString, box

ROOT = '/home/claude/airport-dynasty-pro-unreal'
AF = json.load(open(f'{ROOT}/Data/airfield.json'))
TP = json.load(open(f'{ROOT}/Data/terminal-plan.json'))
with open('/home/claude/sites/DHM200.asc') as f:
    hdr = {}
    for _ in range(6):
        k, v = f.readline().split(); hdr[k.upper()] = float(v)
    Z = np.array(f.read().split(), dtype=np.float32).reshape(int(hdr['NROWS']), int(hdr['NCOLS']))
X0, Y0, CS = hdr['XLLCORNER'], hdr['YLLCORNER'], hdr['CELLSIZE']; NR, NC = Z.shape; YTOP = Y0 + NR * CS
TO = Transformer.from_crs('EPSG:4326', 'EPSG:21781', always_xy=True)
RIVERS = {
    'rhine': [(47.065, 9.490), (47.120, 9.505), (47.170, 9.495), (47.205, 9.505), (47.235, 9.520), (47.255, 9.535),
              (47.290, 9.560), (47.320, 9.585), (47.360, 9.620), (47.385, 9.650), (47.410, 9.650), (47.430, 9.660),
              (47.455, 9.655), (47.500, 9.640)],
    'rhone_chablais': [(46.215, 7.003), (46.245, 6.990), (46.275, 6.955), (46.300, 6.925), (46.330, 6.905), (46.355, 6.890),
                       (46.375, 6.870), (46.395, 6.855)],
}
RW = AF['runway']; RY = RW['y']; SEP = 1035.0; RY2 = RY + SEP


def to_world(cE, cN, h, pts):
    a = math.radians(h); u = np.array([math.sin(a), math.cos(a)]); v = np.array([-math.cos(a), math.sin(a)])
    P = np.asarray(pts, float).reshape(-1, 2)
    return np.c_[cE + P[:, 0] * u[0] + P[:, 1] * v[0], cN + P[:, 0] * u[1] + P[:, 1] * v[1]]


def rect(x0, y0, x1, y1): return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def strip(pts, w):
    return list(LineString(pts).buffer(w / 2, cap_style=2, join_style=2).exterior.coords)

HANGAR = {'S': (40, 32), 'M': (60, 45), 'L': (85, 62), 'XL': (110, 80)}


def plane(ax, W, x, nose, d, span, ln, col='white', lw=1.6):
    """A simple aircraft symbol: nose at (x, nose), the tail len metres away from the terminal side."""
    tail = nose - d * ln
    f = W([(x, nose), (x, tail)]); ax.plot(f[:, 0], f[:, 1], color=col, lw=lw * 1.6, solid_capstyle='round')
    wy = nose - d * ln * 0.45; w = W([(x - span / 2, wy - d * 4), (x, wy + d * 3), (x + span / 2, wy - d * 4)])
    ax.plot(w[:, 0], w[:, 1], color=col, lw=lw)
    ty = tail + d * 4; t = W([(x - span * 0.18, ty - d * 2), (x, ty), (x + span * 0.18, ty - d * 2)])
    ax.plot(t[:, 0], t[:, 1], color=col, lw=lw * 0.8)


cmap = LinearSegmentedColormap.from_list('alp', [(0, '#5d8a4a'), (0.12, '#8fae63'), (0.25, '#c9c48a'), (0.42, '#b38b5d'),
                                                 (0.62, '#8c7a6b'), (0.8, '#d8d4cf'), (1, '#ffffff')])


def draw(site, name, cE, cN, hh, river, path):
    W = lambda pts: to_world(cE, cN, hh, pts)
    HW, HH = 4600, 3300                                           # half the map's width and height (metres)
    pad = 2000
    c0 = int((cE - HW - pad - X0) / CS); c1 = int((cE + HW + pad - X0) / CS)
    r0 = int((YTOP - (cN + HH + pad)) / CS); r1 = int((YTOP - (cN - HH - pad)) / CS)
    sub = zoom(Z[r0:r1, c0:c1], 8, order=3)                      # 25 m cells, smoothly interpolated for drawing
    ext = [X0 + c0 * CS, X0 + c1 * CS, YTOP - r1 * CS, YTOP - r0 * CS]
    fig = plt.figure(figsize=(22, 17), dpi=110); fig.patch.set_facecolor('#0d1b2e')
    ax = fig.add_axes([0.02, 0.15, 0.96, 0.79])
    ls = LightSource(azdeg=315, altdeg=40)
    rgb = ls.shade(sub, cmap=cmap, vert_exag=3.0, blend_mode='soft', vmin=np.nanmin(sub), vmax=min(np.nanmax(sub), 3000), dx=25, dy=25)
    ax.imshow(rgb, extent=ext, origin='upper', interpolation='bilinear')
    gx = np.linspace(ext[0], ext[1], sub.shape[1]); gy = np.linspace(ext[3], ext[2], sub.shape[0])
    lo = math.floor(np.nanmin(sub) / 10) * 10
    cs = ax.contour(gx, gy, sub, levels=np.arange(lo, lo + 200, 10), colors='#1d3b1d', linewidths=0.7, alpha=0.55)
    ax.clabel(cs, fmt='%d m', fontsize=11)
    ax.contour(gx, gy, sub, levels=np.arange(lo + 200, 4000, 100), colors='k', linewidths=0.5, alpha=0.35)
    if river:
        xy = np.array([TO.transform(lo_, la) for la, lo_ in RIVERS[river]])
        ax.plot(xy[:, 0], xy[:, 1], color='#2f6fd6', lw=7, alpha=0.85, solid_capstyle='round')
    def poly(pts, **k): ax.add_patch(MPoly(W(pts), closed=True, **k))
    # airport fence today, and the area the second runway adds
    b = AF['boundary']
    poly(rect(b['x0'], b['y0'], b['x1'], b['y1']), fill=False, ec='#ff5fc8', lw=2.8, ls='--')
    poly(rect(b['x0'], b['y1'], b['x1'], RY2 + 200), fill=False, ec='#7fe3ff', lw=2.2, ls=':')
    # grass strips round the runways, then the runways (with shoulders) and their markings
    for y, col in ((RY, 'white'), (RY2, '#7fe3ff')):
        poly(rect(RW['x0'] - 60, y - 140, RW['x1'] + 60, y + 140), fc='#7fb069', ec='none', alpha=0.55)
        poly(rect(RW['x0'], y - 37.5, RW['x1'], y + 37.5), fc='#5c5c5c', ec='none')
        poly(rect(RW['x0'], y - 30, RW['x1'], y + 30), fc='#262626', ec=col, lw=1.2, ls='-' if col == 'white' else '--')
        c = W([(RW['x0'] + 300, y), (RW['x1'] - 300, y)]); ax.plot(c[:, 0], c[:, 1], color='white', lw=0.9, ls=(0, (6, 6)))
        for e in RW['ends']:
            xt = e['x'] + e['dir'] * e['disp']
            t = W([(xt, y - 28), (xt, y + 28)]); ax.plot(t[:, 0], t[:, 1], color='white', lw=2.2)
    # the second runway's own taxiway and its links round the main runway's ends (future)
    tw2 = RY2 - 190
    by = max(p[1] for t in AF['taxiways'] if t['id'] == 'B' for p in t['pts'])
    for pts in ([(RW['x0'] + 100, tw2), (RW['x1'] - 100, tw2)], [(RW['x0'] + 100, tw2), (RW['x0'] + 100, RY2)],
                [(RW['x1'] - 100, tw2), (RW['x1'] - 100, RY2)], [(0, tw2), (0, RY2)],
                [(-1600, by), (-1600, tw2)], [(1250, by), (1250, tw2)], [(1950, by), (1950, tw2)]):
        poly(strip(pts, 23), fc='#3d4b55', ec='#7fe3ff', lw=0.8, ls='--')
    # aprons, taxiways, roads, car parks, pads
    for a in AF['aprons']: poly(rect(a['x0'], a['y0'], a['x1'], a['y1']), fc='#9aa3a8', ec='white', lw=0.8)
    for t in AF['taxiways']:
        poly(strip(t['pts'], t.get('w', 23)), fc='#4a4f55', ec='#e8e8e8', lw=0.6)
        c = W(t['pts']); ax.plot(c[:, 0], c[:, 1], color='#ffd84a', lw=0.9)
    for r in AF['roads']: poly(strip(r['pts'], r.get('w', 7)), fc='#2e2e2e', ec='#cfcfcf', lw=0.5)
    for l in AF['lots']: poly(rect(l['x0'], l['y0'], l['x1'], l['y1']), fc='#6d6d6d', ec='white', lw=0.6)
    for p in AF['pads']: poly(rect(p['x0'], p['y0'], p['x1'], p['y1']), fc='#8a8f93', ec='white', lw=0.6)
    cb = AF['cargoBuilding']; poly(rect(cb['x0'], cb['y0'], cb['x1'], cb['y1']), fc='#e0a43a', ec='white', lw=1.0)
    for hg in AF['hangars']:
        w_, d_ = HANGAR.get(hg['size'], (60, 45)); poly(rect(hg['x'] - w_ / 2, hg['y'], hg['x'] + w_ / 2, hg['y'] + d_), fc='#c98b4a', ec='white', lw=1.0)
    # the terminal: its ground-floor rooms
    for rm in TP['rooms']:
        if rm.get('f', 1) != 1: continue
        poly(rect(rm['x0'], rm['z0'], rm['x1'], rm['z1']), fc='#45e6ff', ec='#0d4a5c', lw=0.4)
    for s in AF['stands']:
        plane(ax, W, s['x'], s['nose'], s.get('dir', -1), s.get('span', 36), s.get('len', 40))
    # labels
    def lab(x, y, t, col='white', size=15, rot=0):
        p = W([(x, y)])[0]
        ax.text(p[0], p[1], t, color=col, fontsize=size, fontweight='bold', ha='center', va='center',
                bbox=dict(boxstyle='round,pad=0.25', fc='#0d1b2ecc', ec='none'))
    n1 = int(round(hh / 10)) % 36 or 36; n2 = (n1 + 18) % 36 or 36
    lab(RW['x0'] - 380, RY, f'{n1:02d}'); lab(RW['x1'] + 380, RY, f'{n2:02d}')
    lab(0, RY2 + 260, 'future runway 2 (1,035 m out)', '#7fe3ff', 14)
    lab(150, -330, 'terminal', '#45e6ff', 14)
    lab((cb['x0'] + cb['x1']) / 2, cb['y1'] + 70, 'cargo', '#e0a43a', 13)
    lab(AF['hangars'][0]['x'], AF['hangars'][0]['y'] + 130, 'hangars', '#c98b4a', 13)
    ax.set_xlim(cE - HW, cE + HW); ax.set_ylim(cN - HH, cN + HH); ax.set_aspect('equal'); ax.set_xticks([]); ax.set_yticks([])
    ax.annotate('N', xy=(cE + HW - 450, cN + HH - 300), xytext=(cE + HW - 450, cN + HH - 1100), ha='center', fontsize=22,
                fontweight='bold', color='white', arrowprops=dict(arrowstyle='-|>', color='white', lw=3))
    x0, y0 = cE - HW + 300, cN - HH + 300
    ax.plot([x0, x0 + 1000], [y0, y0], color='white', lw=6); ax.text(x0 + 500, y0 + 160, '1 km', color='white', ha='center', fontsize=17, fontweight='bold')
    fig.text(0.02, 0.955, name, color='white', fontsize=27, fontweight='bold')
    fig.text(0.02, 0.025,
             'Pink dashes: the airport fence today.  Cyan: the future second runway, its taxiway and its area.  Thin green lines: ground height every 10 m\n'
             '(one line across the airport means it is within about 10 m).  Blue: the river (approximate).  White symbols: the stands with their aircraft.\n'
             'Heights: swisstopo DHM200 (200 m grid, smoothed for drawing). Motorways, railways and buildings are not shown yet.',
             color='#cfe3ff', fontsize=15)
    fig.savefig(path, facecolor=fig.get_facecolor()); plt.close(fig)


R = {r['id']: r for r in json.load(open('/home/claude/sites/results_ok.json'))}
for sid, name, river in (('2', '2. Rheintal north - Oberriet / Altstaetten (SG): runways 02/20, 404 m', 'rhine'),
                         ('3', '3. Chablais plain - Aigle / Monthey (VD/VS): runways 34/16, 374 m', 'rhone_chablais'),
                         ('1', '1. Rheintal - Buchs / Sennwald (SG): runways 02/20, 433 m (approaches need curves)', 'rhine')):
    r = R[sid]
    draw(sid, name, r['cE'], r['cN'], r['hdg'], river, f'/home/claude/sites/out/Layout-{sid}.png')
    print('drew', sid, flush=True)
