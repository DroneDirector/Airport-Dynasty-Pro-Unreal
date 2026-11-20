# The cargo terminal's blueprint sheets (4K PNG, 3840 x 2160, large text), all drawn from cargo_layout.py:
#   1 ground-floor plan: every room and working area, walls with their openings, the equipment, dimensions
#   2 detail plans at a larger scale: the office block's three floors and the special-cargo block, names in the rooms
#   3 site plan: the cargo apron and its freighter stands, the building, the truck court and the moved fence
# Freight only - there is nothing for passengers here.
#   python3 make_cargo_blueprints.py   ->  /tmp/cargo_views/Cargo-1-Floor-Plan.png, -2-Detail-Plans.png, -3-Site-Plan.png
import os, sys, textwrap
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, FancyArrowPatch

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import cargo_layout as CL
from cargo_layout import L, D

OUT = '/tmp/cargo_views'; os.makedirs(OUT, exist_ok=True)
BG, INK, DIM, LINE, WALL = '#071a33', '#eaf2ff', '#7f9bbd', '#9ec5f0', '#ffffff'
C = dict(off='#5aa9ff', exp='#2ed3b7', etv='#ffd166', imp='#ff9f5a', spc='#ff6b9a', air='#c7a6ff', land='#8bd17c')
PX = 2.78                                     # pixels per point at dpi 200


# #region ── Drawing helpers ──
def sheet(title, sub):
    fig = plt.figure(figsize=(19.2, 10.8), dpi=200, facecolor=BG)
    fig.text(0.018, 0.955, title, color=INK, fontsize=30, fontweight='bold')
    fig.text(0.018, 0.918, sub, color=DIM, fontsize=15)
    return fig


def dim(ax, a, b, off, text, vertical=False, size=13, col=LINE):
    """A dimension line between a and b (x values, or y values if vertical) at the given offset."""
    ap = dict(arrowstyle='<|-|>', color=col, lw=1.6, mutation_scale=16, shrinkA=0, shrinkB=0)
    if not vertical:
        ax.annotate('', xy=(a, off), xytext=(b, off), arrowprops=ap)
        ax.text((a + b) / 2, off, text, color=col, fontsize=size, ha='center', va='center', fontweight='bold', bbox=dict(fc=BG, ec='none', pad=1.5))
    else:
        ax.annotate('', xy=(off, a), xytext=(off, b), arrowprops=ap)
        ax.text(off, (a + b) / 2, text, color=col, fontsize=size, ha='center', va='center', fontweight='bold', rotation=90,
                bbox=dict(fc=BG, ec='none', pad=1.5))


def area(ax, x0, y0, x1, y1, col, kind):
    if kind == 'aisle':
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc='#0b223f', ec='none'))
        if x1 - x0 >= y1 - y0: ax.plot([x0, x1], [y0 + 0.2, y0 + 0.2], color='#ffd166', lw=1.2, ls='--'); ax.plot([x0, x1], [y1 - 0.2, y1 - 0.2], color='#ffd166', lw=1.2, ls='--')
        else: ax.plot([x0 + 0.2, x0 + 0.2], [y0, y1], color='#ffd166', lw=1.2, ls='--'); ax.plot([x1 - 0.2, x1 - 0.2], [y0, y1], color='#ffd166', lw=1.2, ls='--')
    else:
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=col, ec='none', alpha=0.20 if kind != 'cold' else 0.32))
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, ec=col, lw=0.9, alpha=0.7))


def walls(ax, rooms, skip_x, skip_y, lw=3.2, col=WALL, dx=0.0):
    for x0, y0, _, x1, y1, _ in CL.walls_from_rooms(rooms, 0.0, skip_x, skip_y):
        if x1 - x0 > y1 - y0: ax.plot([x0 + dx, x1 + dx], [(y0 + y1) / 2] * 2, color=col, lw=lw, solid_capstyle='butt')
        else: ax.plot([(x0 + x1) / 2 + dx] * 2, [y0, y1], color=col, lw=lw, solid_capstyle='butt')


def fit_text(ax, x0, y0, x1, y1, name, ppm, sizes=(13, 12, 11, 10, 9, 8), col=INK, bold=True):
    """Write name inside the box, wrapped, at the largest size whose measured size keeps clear of the walls;
    narrow tall rooms get the text turned sideways (ppm = pixels per metre)."""
    rend = ax.figure.canvas.get_renderer()
    wpx, hpx = (x1 - x0) * ppm - 22, (y1 - y0) * ppm - 18
    tries = [(s, 0) for s in sizes] + ([(s, 90) for s in sizes] if y1 - y0 > x1 - x0 else [])
    for s, rot in tries:
        bw, bh = (wpx, hpx) if rot == 0 else (hpx, wpx)
        for n in range(max(len(w) for w in name.split()) , len(name) + 1):
            lines = textwrap.wrap(name, n, break_long_words=False)
            t = ax.text((x0 + x1) / 2, (y0 + y1) / 2, '\n'.join(lines), color=col, fontsize=s, ha='center', va='center',
                        fontweight='bold' if bold else 'normal', linespacing=1.12, rotation=rot)
            e = t.get_window_extent(rend); ww, hh = (e.width, e.height) if rot == 0 else (e.height, e.width)
            if ww <= bw and hh <= bh: return True
            t.remove()
            if ww > bw: break
    return False


def equipment(ax, dx=0.0, only=None):
    def r(x0, y0, x1, y1, fc='none', ec=LINE, lw=1.2, a=1.0):
        if only and not (only[0] <= x0 < only[1]): return
        ax.add_patch(Rectangle((x0 + dx, y0), x1 - x0, y1 - y0, fc=fc, ec=ec, lw=lw, alpha=a))
    y0t, y1t = CL.TRANSFER; r(CL.EXP[0] + 0.5, y0t, CL.IMP[1] - 0.5, y1t, ec='#c9d6e6')
    wd, sy0, sy1 = CL.STATION
    for cx in CL.BUILDUP + CL.BREAKDOWN:
        r(cx - wd / 2, sy0, cx + wd / 2, sy1, ec='#c9d6e6'); r(cx - wd / 2 - 0.2, sy1 + 0.3, cx + wd / 2 + 0.2, sy1 + 3.3, ec='#c9d6e6', lw=0.8)
    for zk, (xa, xb, rows) in CL.RACK_ROWS.items():
        for ry in rows: r(xa, ry, xb, ry + 2.4, fc='#f59a3a', ec='#f59a3a', a=0.55)
    for b in CL.XRAY: r(*b, fc='#59cdbf', ec='#59cdbf', a=0.7)
    for b in CL.SCALES: r(*b, ec='#c9d6e6')
    for ya, yb in ((22.0, 30.0), (40.0, 48.0)):
        r(CL.ETV[0] + 1.5, ya, CL.ETV[1] - 1.0, yb, fc='#f59a3a', ec='#f59a3a', a=0.25)
        for x in CL.ETV_RACK_X: r(x, ya, x + 0.4, yb, fc='#f59a3a', ec='none', a=0.9)
    for ya in (10.0, 15.5, 52.5, 58.0): r(CL.ETV[0] + 1, ya, CL.ETV[1] - 1, ya + 3.2, ec='#c9d6e6', lw=0.8)
    for x in CL.ETV_MACHINES: r(x - 1.8, 30.8, x + 1.8, 39.2, fc='#ffd21a', ec='#ffd21a', a=0.9)


def openings(ax, dx=0.0, only=None):
    ok = (lambda x: only[0] <= x <= only[1]) if only else (lambda x: True)
    for x in CL.AIR_DOORS:
        if ok(x): ax.add_patch(Rectangle((x - 3 + dx, -1.3), 6, 2.6, fc=C['air'], ec='none'))
    for x in CL.SPC_AIR_DOORS:
        if ok(x): ax.add_patch(Rectangle((x - 2 + dx, -1.3), 4, 2.6, fc=C['spc'], ec='none'))
    for x in CL.DOCKS + CL.SPC_DOCKS:
        if ok(x): ax.add_patch(Rectangle((x - 1.5 + dx, D - 1.3), 3, 2.6, fc=C['land'], ec='none'))
    for x in CL.DRIVE_IN:
        if ok(x): ax.add_patch(Rectangle((x - 2.5 + dx, D - 1.3), 5, 2.6, fc='#ffffff', ec='none'))
# #endregion


# #region ── Sheet 1: ground-floor plan ──
fig = sheet('CARGO TERMINAL  -  GROUND FLOOR PLAN',
            'Freight only  ·  390 x 70 m (27,300 m²)  ·  column grid 15 x 35 m  ·  forklift aisles 6 m, corridors 4-6 m, '
            'room openings 4 m  ·  apron at the bottom  ·  metres')
ax = fig.add_axes([0.005, 0.345, 0.99, 0.545]); ax.set_facecolor(BG)
ax.set_xlim(-16, 406); ax.set_ylim(-36, 95); ax.set_aspect('equal'); ax.axis('off')
ppm = 3840 * 0.99 / 422
for name, x0, y0, x1, y1, zk, kind, doors, short in CL.ROOMS: area(ax, x0, y0, x1, y1, C[zk], kind)
equipment(ax)
gr = [(x0, y0, x1, y1, 5.0, doors) for _, x0, y0, x1, y1, zk, k, doors, _ in CL.ROOMS if k in ('room', 'cold')]
walls(ax, gr, (0.0, L, CL.EXP[0], CL.ETV[0], CL.ETV[1], CL.SPC[0]), (0.0, D), lw=2.6)
for x in (CL.EXP[0], CL.ETV[0], CL.ETV[1], CL.SPC[0]):                     # fire walls with their two shutters
    for a, b in ((0, 25), (30, 52.5), (57.5, D)): ax.plot([x, x], [a, b], color='#ff6b6b', lw=4.5, solid_capstyle='butt')
ax.add_patch(Rectangle((0, 0), L, D, fill=False, ec=INK, lw=4))
openings(ax)
for name, x0, y0, x1, y1, zk, kind, doors, short in CL.ROOMS:
    r = (name, x0, y0, x1, y1, zk, kind, doors, short)
    if short:
        ax.text((x0 + x1) / 2, (y0 + y1) / 2 + (0 if '\n' in short or y1 - y0 < 12 else 2.5), short, color=INK, fontsize=13.5, ha='center',
                va='center', fontweight='bold', linespacing=1.1, bbox=dict(fc=BG, ec='none', pad=1.2, alpha=0.75))
    elif kind == 'aisle' and x1 - x0 > 50:
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, name, color='#ffd166', fontsize=10.5, ha='center', va='center')
    elif kind != 'aisle':
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, str(CL.NUM[id(CL.ROOMS[CL.ROOMS.index(r)])]), color=INK, fontsize=12.5, ha='center',
                va='center', fontweight='bold', bbox=dict(boxstyle='circle,pad=0.25', fc=BG, ec=C[zk], lw=1.6))
zs = [(a, b) for _, (a, b), _, _ in CL.ZONES]
for a, b in zs: dim(ax, a, b, 82, f'{b - a:.0f}', size=14)
dim(ax, 0, L, -27, '390', size=15)
dim(ax, 0, D, -9, '70', vertical=True, size=14)
dim(ax, 0, 35, 398, '35', vertical=True, size=12.5); dim(ax, 35, D, 398, '35', vertical=True, size=12.5)
ax.add_patch(Rectangle((CL.EXP[0], -12), CL.IMP[1] - CL.EXP[0], 11.7, fill=False, ec=C['air'], lw=2, ls='--'))
ax.text(190, -6.2, 'AIRSIDE CANOPY 12 m deep, 9 m clear  ·  21 ULD doors 6 m wide, one every 15 m  ·  roller decks to the dollies',
        color=C['air'], fontsize=13, ha='center', va='center', fontweight='bold')
ax.text(190, 75.5, '24 truck docks every 13 m (3 x 3.5 m doors, 1.2 m dock height)  ·  3 sealed cool-chain docks  ·  1 drive-in door  ·  canopy 6 m',
        color=C['land'], fontsize=13, ha='center', va='center', fontweight='bold')
ax.text(-14, -33, 'APRON  ↓', color=DIM, fontsize=12.5, va='center'); ax.text(-14, 92, 'TRUCK COURT  ↑', color=DIM, fontsize=12.5, va='center')
ax.text(366, -33, 'red: fire walls with fire shutters', color='#ff9a9a', fontsize=12, va='center', ha='center')
for i, rr in enumerate(CL.NUMBERED):
    col, row = i // 7, i % 7
    fig.text(0.012 + col * 0.248, 0.312 - row * 0.0285, f'{i + 1:>2}', color=C[rr[5]], fontsize=13.5, va='center', fontweight='bold')
    fig.text(0.031 + col * 0.248, 0.312 - row * 0.0285, rr[0], color=INK, fontsize=13.5, va='center')
x = 0.012
for zk, _, _, t in CL.ZONES:
    fig.patches.append(Rectangle((x, 0.068), 0.011, 0.022, transform=fig.transFigure, fc=C[zk], ec='none'))
    fig.text(x + 0.015, 0.079, t, color=INK, fontsize=14, va='center'); x += 0.04 + 0.0074 * len(t)
fig.text(0.763, 0.312 - 5 * 0.0285, 'orange: racks  ·  teal: X-ray / EDS', color=DIM, fontsize=13, va='center')
fig.text(0.763, 0.312 - 6 * 0.0285, 'yellow dashes: kept-clear aisles', color=DIM, fontsize=13, va='center')
fig.text(0.012, 0.032, 'EXPORT: trucks → acceptance → weigh → screening → build-up → ETV store → aircraft.    '
         'IMPORT: aircraft → ETV store → break-down → customs → release → trucks.', color=DIM, fontsize=13.5, va='center')
fig.savefig(os.path.join(OUT, 'Cargo-1-Floor-Plan.png'), facecolor=BG); plt.close(fig)
# #endregion

# #region ── Sheet 2: detail plans (office block ground, 1st and 2nd floor; special-cargo block) ──
fig = sheet('CARGO TERMINAL  -  DETAIL PLANS: OFFICES AND SPECIAL CARGO',
            'Office block 30 x 70 m, three floors (14 m)  ·  special-cargo block 45 x 70 m (12 m)  ·  airside at the bottom  ·  metres')
ax = fig.add_axes([0.01, 0.02, 0.98, 0.86]); ax.set_facecolor(BG)
ax.set_xlim(-10, 196); ax.set_ylim(-14, 84); ax.set_aspect('equal'); ax.axis('off')
ppm = 3840 * 0.98 / 206
plans = [('GROUND FLOOR', 0.0, [(n, x0, y0, x1, y1, k, d) for n, x0, y0, x1, y1, zk, k, d, _ in CL.ROOMS if zk == 'off'], 'off'),
         ('1st FLOOR  (+4.6 m)', 44.0, CL.UPPER[1], 'off'), ('2nd FLOOR  (+9.2 m)', 88.0, CL.UPPER[2], 'off'),
         ('SPECIAL CARGO  ·  ground', 132.0 - CL.SPC[0], [(n, x0, y0, x1, y1, k, d) for n, x0, y0, x1, y1, zk, k, d, _ in CL.ROOMS if zk == 'spc'], 'spc')]
for title, dx, rooms, zk in plans:
    xa = min(r[1] for r in rooms); xb = max(r[3] for r in rooms)
    for n, x0, y0, x1, y1, k, d in rooms: area(ax, x0 + dx, y0, x1 + dx, y1, C[zk], k)
    if zk == 'spc':
        equipment(ax, dx, only=(CL.SPC[0], CL.SPC[1]))
    walls(ax, [(x0, y0, x1, y1, 5.0, d) for n, x0, y0, x1, y1, k, d in rooms if k in ('room', 'cold')], (xa, xb), (0.0, D), lw=3, dx=dx)
    ax.add_patch(Rectangle((xa + dx, 0), xb - xa, D, fill=False, ec=INK, lw=4))
    for n, x0, y0, x1, y1, k, d in rooms:
        if k == 'aisle':
            ax.text((x0 + x1) / 2 + dx, (y0 + y1) / 2, n, color='#ffd166', fontsize=11, ha='center', va='center',
                    rotation=90 if y1 - y0 > x1 - x0 else 0)
        else:
            fit_text(ax, x0 + dx, y0, x1 + dx, y1, n + (' (cold)' if k == 'cold' and '°C' not in n else ''), ppm)
    ax.text((xa + xb) / 2 + dx, 79.5, title, color=C[zk], fontsize=17, ha='center', va='center', fontweight='bold')
    if zk == 'off':
        dim(ax, xa + dx, 13 + dx, -4, '13', size=12); dim(ax, 13 + dx, 17 + dx, -9.5, '4', size=11); dim(ax, 17 + dx, 30 + dx, -4, '13', size=12)
    else:
        openings(ax, dx, only=(CL.SPC[0], CL.SPC[1]))
        dim(ax, xa + dx, xb + dx, -6, '45', size=13)
        ys = [0, 13, 17, 28, 34, 46, 56, 70]
        for a, b in zip(ys, ys[1:]): dim(ax, a, b, xb + dx + 4, f'{b - a}', vertical=True, size=11.5)
dim(ax, 0, D, -6, '70', vertical=True, size=13)
ax.text(15, 74, 'entrance ↑', color=DIM, fontsize=12, ha='center', va='center')
ax.text(15 + 88, -12.5, 'control centre looks out over the apron', color=DIM, fontsize=12, ha='center', va='center')
ax.text(154.5, -11.5, 'airside doors straight to the aircraft: animals, perishables, frozen', color=C['spc'], fontsize=12, ha='center', va='center')
fig.savefig(os.path.join(OUT, 'Cargo-2-Detail-Plans.png'), facecolor=BG); plt.close(fig)
# #endregion

# #region ── Sheet 3: site plan (local metres: building 0..390 x 0..70, apron at -Y) ──
fig = sheet('CARGO TERMINAL  -  SITE PLAN',
            'On the far side of the runway from the passenger terminal  ·  plan x 200-590, y 1231-1301  ·  dimensions in metres  ·  airside at the bottom')
ax = fig.add_axes([0.03, 0.10, 0.94, 0.79]); ax.set_facecolor(BG)
ax.set_xlim(-120, 520); ax.set_ylim(-190, 230); ax.set_aspect('equal'); ax.axis('off')
ax.add_patch(Rectangle((-40, -165), 500, 165, fc='#0d2a4d', ec=DIM, lw=1.5))
ax.add_patch(Rectangle((-120, -157.5), 640, 25, fc='#14365f', ec='none'))
ax.text(462, -145, 'TAXILANE K', color=INK, fontsize=12, va='center', fontweight='bold')
for x, n in ((0, 'Q1  to taxiway B'), (420, 'Q2  to taxiway B')):
    ax.add_patch(Rectangle((x - 12.5, -190), 25, 33, fc='#14365f', ec='none'))
    ax.text(x + 16, -180, n, color=INK, fontsize=11.5, va='center')
for i, sx in enumerate((60.0, 147.5, 235.0, 322.5)):
    ax.plot([sx, sx], [-132, -15], color='#ffd166', lw=2.2)
    ax.add_patch(Circle((sx, -15 - 38), 40, fill=False, ec='#ffd166', lw=1.0, ls=':'))
    ax.text(sx, -138, f'C{i + 1}', color='#ffd166', fontsize=16, ha='center', va='top', fontweight='bold')
ax.text(191, -95, 'CARGO APRON  -  4 code F freighter stands (747-8F / 777F)', color=INK, fontsize=14,
        ha='center', va='center', fontweight='bold', bbox=dict(fc='#0d2a4d', ec='none', pad=2))
ax.add_patch(Rectangle((-40, -40), 500, 40, fc='#123a68', ec='none'))
ax.text(80, -21, 'ULD staging lanes & airside service road (40 m)', color=INK, fontsize=12, va='center')
for n, x0, y0, x1, y1 in CL.OUTSIDE:
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc='none', ec=C['air'], lw=1.6, ls='--'))
ax.text(470, -26, 'empty ULD store', color=C['air'], fontsize=11, va='center')
ax.text(470, -8, 'ULD repair shed', color=C['air'], fontsize=11, va='center')
ax.text(-45, -21, 'dollies & GSE\n(charging)', color=C['air'], fontsize=11, va='center', ha='right')
for zk, (x0, x1), _, _ in CL.ZONES:
    ax.add_patch(Rectangle((x0, 0), x1 - x0, 70, fc=C[zk], ec=INK, lw=1.4, alpha=0.35))
ax.add_patch(Rectangle((0, 0), 390, 70, fill=False, ec=INK, lw=3))
ax.text(195, 35, 'CARGO TERMINAL  390 x 70 m', color=INK, fontsize=18, ha='center', va='center', fontweight='bold')
ax.add_patch(Rectangle((-10, 70), 400, 140, fc='#10304f', ec='none'))
ax.text(195, 105, 'TRUCK COURT  (turning depth 52 m)', color=INK, fontsize=14, ha='center', va='center', fontweight='bold')
ax.add_patch(Rectangle((40, 180), 300, 22, fill=False, ec=C['land'], lw=1.6))
ax.text(190, 191, 'lorry parking  -  60 bays', color=C['land'], fontsize=12.5, ha='center', va='center')
ax.add_patch(Rectangle((-8, 90), 36, 5, fill=False, ec=C['off'], lw=1.6))
ax.text(10, 140, 'staff\ncars', color=C['off'], fontsize=11, ha='center', va='center')
ax.plot([-10, 150], [210, 210], color='#ff6b6b', lw=3); ax.plot([175, 400], [210, 210], color='#ff6b6b', lw=3)
ax.plot([-120, 520], [89, 89], color='#ff6b6b', lw=1.8, ls=':')
ax.text(405, 93, "today's fence (plan y 1320):\nmoved out", color='#ff9a9a', fontsize=12, va='bottom')
ax.text(-115, 213, 'new fence (plan y 1441)', color='#ff9a9a', fontsize=12, va='bottom')
ax.add_patch(Rectangle((152, 196), 8, 6, fc=C['off'], ec='none'))
ax.text(158, 222, 'security gate + gatehouse', color=INK, fontsize=12, ha='right', va='center')
ax.add_patch(FancyArrowPatch((163, 226), (163, 200), arrowstyle='-|>', mutation_scale=20, color=INK, lw=2))
ax.text(170, 222, "  ← access road to the landside roads (round the runway's east end)", color=DIM, fontsize=11.5, ha='left', va='center')
fig.text(0.02, 0.04, 'Room is kept for the future 2nd runway: its taxiway lies about 50 m beyond the new fence.  Stand C1-C4 circles: 80 m wingspan.', color=DIM, fontsize=13)
dim(ax, 0, 390, -4, '390', size=12)
dim(ax, -165, 0, -75, '165 apron', vertical=True, size=12)
dim(ax, 0, 70, -75, '70', vertical=True, size=12)
dim(ax, 70, 210, -75, '140 court', vertical=True, size=12)
dim(ax, 60, 147.5, -118, '87.5 m between stands', size=11)
fig.savefig(os.path.join(OUT, 'Cargo-3-Site-Plan.png'), facecolor=BG); plt.close(fig)
# #endregion
print('sheets written')
