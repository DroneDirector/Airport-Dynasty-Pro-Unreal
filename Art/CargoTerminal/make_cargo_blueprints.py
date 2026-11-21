# The cargo terminal's blueprint sheets (4K PNG, 3840 x 2160, large text), all drawn from cargo_layout.py:
#   1 ground-floor plan: every room and working area, walls, every door with its symbol, the security line, dimensions
#   2 room schedule: every room's size and area and the rule it is sized or placed by
#   3 detail plans of the office block's three floors (names in the rooms, doors, the three stairs)
#   4 detail plan of special cargo and the border control post
#   5 site plan: the cargo apron and its freighter stands, the building, the truck court and the moved fence
# Freight only - there is nothing for passengers here.
#   python3 make_cargo_blueprints.py   ->  /tmp/cargo_views/Cargo-1..5-*.png
import os, sys, textwrap
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, FancyArrowPatch, Arc

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import cargo_layout as CL
from cargo_layout import L, D

OUT = '/tmp/cargo_views'; os.makedirs(OUT, exist_ok=True)
BG, INK, DIM, LINE, WALL = '#071a33', '#eaf2ff', '#7f9bbd', '#9ec5f0', '#ffffff'
C = dict(off='#5aa9ff', exp='#2ed3b7', etv='#ffd166', imp='#ff9f5a', spc='#ff6b9a', bcp='#b48cff', land='#8bd17c')
GREEN, AMBER, RED, SEC = '#3ddc6a', '#ffb020', '#ff5a5a', '#ff4fd8'


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


def area(ax, x0, y0, x1, y1, col, kind, dx=0.0):
    x0 += dx; x1 += dx
    if kind == 'aisle':
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc='#0b223f', ec='none'))
        if x1 - x0 >= y1 - y0: ax.plot([x0, x1], [y0 + 0.2, y0 + 0.2], color='#ffd166', lw=1.0, ls='--'); ax.plot([x0, x1], [y1 - 0.2, y1 - 0.2], color='#ffd166', lw=1.0, ls='--')
        else: ax.plot([x0 + 0.2, x0 + 0.2], [y0, y1], color='#ffd166', lw=1.0, ls='--'); ax.plot([x1 - 0.2, x1 - 0.2], [y0, y1], color='#ffd166', lw=1.0, ls='--')
    else:
        a = {'cold': 0.34, 'dg': 0.30, 'stair': 0.45}.get(kind, 0.20)
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=col, ec='none', alpha=a))
        if kind == 'stair':
            for i in range(1, 8): ax.plot([x0 + (x1 - x0) * i / 8] * 2, [y0 + 0.4, y1 - 0.4], color=INK, lw=0.6, alpha=0.5)
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, ec=col, lw=0.9, alpha=0.7))


def walls(ax, rooms, skip_x, skip_y, lw=3.2, col=WALL, dx=0.0):
    for x0, y0, _, x1, y1, _ in CL.walls_from_rooms(rooms, 0.0, skip_x, skip_y):
        if x1 - x0 > y1 - y0: ax.plot([x0 + dx, x1 + dx], [(y0 + y1) / 2] * 2, color=col, lw=lw, solid_capstyle='butt')
        else: ax.plot([(x0 + x1) / 2 + dx] * 2, [y0, y1], color=col, lw=lw, solid_capstyle='butt')


def door_symbols(ax, doors, dx=0.0, lw=2.2):
    """Plan symbols: a personnel door as its leaf and swing arc (green), a wider door as a dashed line across the
    opening with jamb marks (amber, a sectional or rapid door)."""
    for cx, cy, o, w in doors:
        cx += dx; hw = w / 2
        if w <= 1.2:
            if o == 'h':
                hx, hy = cx - hw, cy
                ax.plot([hx, hx], [hy, hy + w], color=GREEN, lw=lw)
            else:
                hx, hy = cx, cy - hw
                ax.plot([hx, hx + w], [hy, hy], color=GREEN, lw=lw)
            ax.add_patch(Arc((hx, hy), 2 * w, 2 * w, theta1=0, theta2=90, color=GREEN, lw=lw * 0.7))
        else:
            if o == 'h':
                ax.plot([cx - hw, cx + hw], [cy, cy], color=AMBER, lw=lw * 1.3, ls=(0, (2, 1.4)))
                for s in (-1, 1): ax.add_patch(Rectangle((cx + s * hw - 0.25, cy - 0.45), 0.5, 0.9, fc=AMBER, ec='none'))
            else:
                ax.plot([cx, cx], [cy - hw, cy + hw], color=AMBER, lw=lw * 1.3, ls=(0, (2, 1.4)))
                for s in (-1, 1): ax.add_patch(Rectangle((cx - 0.45, cy + s * hw - 0.25), 0.9, 0.5, fc=AMBER, ec='none'))


def outside_doors(ax, dx=0.0, only=None):
    ok = (lambda x: only[0] <= x <= only[1]) if only else (lambda x: True)
    t = 2.6
    for x in CL.AIR_DOORS:
        if ok(x): ax.add_patch(Rectangle((x - 3 + dx, -t / 2), 6, t, fc=AMBER, ec='none'))
    for x in CL.SPC_AIR_DOORS:
        if ok(x): ax.add_patch(Rectangle((x - 2 + dx, -t / 2), 4, t, fc=AMBER, ec='none'))
    for x in CL.HORSE_AIR_DOORS:
        if ok(x): ax.add_patch(Rectangle((x - 2.5 + dx, -t / 2), 5, t, fc=AMBER, ec='none'))
    for x in CL.DOCKS + CL.SPC_DOCKS:
        if ok(x): ax.add_patch(Rectangle((x - 1.5 + dx, D - t / 2), 3, t, fc=AMBER, ec='none'))
    for x in CL.DRIVE_IN:
        if ok(x): ax.add_patch(Rectangle((x - 2.5 + dx, D - t / 2), 5, t, fc=AMBER, ec='#ffffff', lw=1.2))
    for x in CL.ESC_AIR:          # escape doors open outwards
        if ok(x):
            ax.plot([x - 0.5 + dx, x - 0.5 + dx], [0, -1.6], color=GREEN, lw=2.2)
            ax.add_patch(Arc((x - 0.5 + dx, 0), 3.2, 3.2, theta1=270, theta2=360, color=GREEN, lw=1.6))
    for x in CL.ESC_LAND:
        if ok(x):
            ax.plot([x - 0.5 + dx, x - 0.5 + dx], [D, D + 1.6], color=GREEN, lw=2.2)
            ax.add_patch(Arc((x - 0.5 + dx, D), 3.2, 3.2, theta1=0, theta2=90, color=GREEN, lw=1.6))
    if ok(L):
        for y in CL.ESC_EAST:
            ax.plot([L + dx, L + 1.6 + dx], [y - 0.5, y - 0.5], color=GREEN, lw=2.2)
            ax.add_patch(Arc((L + dx, y - 0.5), 3.2, 3.2, theta1=0, theta2=90, color=GREEN, lw=1.6))


def fire_walls(ax, dx=0.0, only=None, lw=4.5):
    for x, _ in CL.FIRE_WALLS:
        if only and not (only[0] <= x <= only[1]): continue
        cuts = [(CL.A1[0], CL.A1[1], 's'), (CL.A2[0], CL.A2[1], 's')] if x in CL.FIRE_SHUTTERS else []
        cuts += [(y - 0.6, y + 0.6, 'd') for fx, y in CL.FW_DOORS if fx == x]
        cuts.sort(); y = 0.0
        for a, b, k in cuts + [(D, D, '')]:
            if a > y: ax.plot([x + dx, x + dx], [y, a], color=RED, lw=lw, solid_capstyle='butt')
            if k == 's': ax.plot([x + dx, x + dx], [a, b], color=RED, lw=lw * 0.6, ls=(0, (1.5, 1.2)))
            if k == 'd':
                ax.plot([x + dx, x + dx + 1.2], [a, a], color=GREEN, lw=2.2)
                ax.add_patch(Arc((x + dx, a), 2.4, 2.4, theta1=0, theta2=90, color=GREEN, lw=1.6))
            y = max(y, b)
    if not only or only[0] <= CL.BCP_SPLIT <= only[1]:
        ax.plot([CL.BCP_SPLIT + dx] * 2, [0, D], color=WALL, lw=lw * 0.9, solid_capstyle='butt')


def fit_text(ax, x0, y0, x1, y1, name, ppm, sizes=(13, 12, 11, 10, 9, 8), col=INK, bold=True):
    """Write name inside the box, wrapped, at the largest size whose measured size keeps clear of the walls;
    narrow tall rooms get the text turned sideways (ppm = pixels per metre)."""
    rend = ax.figure.canvas.get_renderer()
    wpx, hpx = (x1 - x0) * ppm - 22, (y1 - y0) * ppm - 18
    tries = [(s, 0) for s in sizes] + ([(s, 90) for s in sizes] if y1 - y0 > x1 - x0 else [])
    for s, rot in tries:
        bw, bh = (wpx, hpx) if rot == 0 else (hpx, wpx)
        for n in range(max(len(w) for w in name.split()), len(name) + 1):
            lines = textwrap.wrap(name, n, break_long_words=False)
            t = ax.text((x0 + x1) / 2, (y0 + y1) / 2, '\n'.join(lines), color=col, fontsize=s, ha='center', va='center',
                        fontweight='bold' if bold else 'normal', linespacing=1.12, rotation=rot,
                        bbox=dict(fc=BG, ec='none', pad=1.0, alpha=0.55))
            e = t.get_window_extent(rend); ww, hh = (e.width, e.height) if rot == 0 else (e.height, e.width)
            if ww <= bw and hh <= bh: return True
            t.remove()
            if ww > bw: break
    return False


def equipment(ax, dx=0.0, only=None):
    def r(x0, y0, x1, y1, fc='none', ec=LINE, lw=1.0, a=1.0):
        if only and not (only[0] <= x0 < only[1]): return
        ax.add_patch(Rectangle((x0 + dx, y0), x1 - x0, y1 - y0, fc=fc, ec=ec, lw=lw, alpha=a))
    y0t, y1t = CL.TRANSFER; r(CL.EXP[0] + 0.5, y0t, CL.IMP[1] - 0.5, y1t, ec='#c9d6e6')
    wd, sy0, sy1 = CL.STATION
    for cx in CL.BUILDUP + CL.BREAKDOWN:
        r(cx - wd / 2, sy0, cx + wd / 2, sy1, ec='#c9d6e6'); r(cx - wd / 2 - 0.2, sy1 + 0.3, cx + wd / 2 + 0.2, sy1 + 3.3, ec='#c9d6e6', lw=0.7)
    for zk, (xa, xb, rows) in CL.RACK_ROWS.items():
        for ry in rows: r(xa, ry, xb, ry + 2.4, fc='#f59a3a', ec='#f59a3a', a=0.55)
    for b in CL.XRAY: r(*b, fc='#59cdbf', ec='#59cdbf', a=0.7)
    for b in CL.SCALES: r(*b, ec='#c9d6e6', lw=0.7)
    for ya, yb in CL.ETV_RACKS:
        r(CL.ETV[0] + 1.5, ya, CL.ETV[1] - 1.0, yb, fc='#f59a3a', ec='#f59a3a', a=0.25)
        for x in CL.ETV_RACK_X: r(x, ya, x + 0.4, yb, fc='#f59a3a', ec='none', a=0.9)
    for ya in (7.5, 12.5, 17.5): r(CL.ETV[0] + 1, ya, CL.ETV[1] - 1, ya + 3.2, ec='#c9d6e6', lw=0.7)
    for x in CL.ETV_MACHINES: r(x - 1.8, CL.ETV_AISLE[0] + 0.2, x + 1.8, CL.ETV_AISLE[1] - 0.2, fc='#ffd21a', ec='#ffd21a', a=0.9)
    for b in CL.HORSE_STALLS + CL.CATTLE_STALLS: r(*b, ec='#d9c7ff', lw=1.0)
    for b in CL.RADIO_BAYS: r(*b, fc='#9aa3ad', ec='#ffd166', lw=1.4, a=0.8)


def security_line(ax, dx=0.0):
    x0, x1, y = CL.SECURITY_LINE
    ax.plot([x0 + dx, x1 + dx], [y, y], color=SEC, lw=2.6, ls=(0, (1, 1.2)))


def ground_plan_rooms(rooms=None):
    rooms = rooms if rooms is not None else CL.ROOMS
    return [(x0, y0, x1, y1, 5.0, doors, CL.door_width(n, zk, k)) for n, x0, y0, x1, y1, zk, k, doors, _ in rooms if k in CL.WALLED]


def rule(name, kind, zk):
    """The rule a room is sized or placed by (short), for the room schedule."""
    n = name
    if 'Stair' in n: return 'VKF 16-15: one stair per 900 m² of floor'
    if zk == 'bcp':
        for k, t in (('Horse', 'EU 2019/1014 Art. 5: horses apart'), ('hoofed', 'EU 2019/1014 Art. 5: hoofed apart'),
                     ('Pets', 'EU 2019/1014 Art. 5: other animals'), ('Clinical', 'EU 2019/1014 Art. 5: restraint'),
                     ('Feed', 'EU 2019/1014 Art. 5: feed, bedding, litter'), ('Manure', 'EU 2019/1014 Art. 5: manure store'),
                     ('Vet staff', 'EU 2019/1014 Art. 5: showers'), ('Animal unloading', 'EU 2019/1014: roofed, own lane'),
                     ('chilled', 'EU 2019/1014 Art. 6: 3 temperatures'), ('frozen', 'EU 2019/1014 Art. 6: 3 temperatures'),
                     ('ambient', 'EU 2019/1014 Art. 6: 3 temperatures'), ('Detained', 'EU 2019/1014 Art. 3: lockable'),
                     ('Inspection room', 'EU 2019/1014: water, cooled'), ('Vet office', 'EU 2019/1014 Art. 4: records'),
                     ('Changing', 'EU 2019/1014 Art. 6: changing'), ('Covered unloading', 'EU 2019/1014 Art. 3: roofed')):
            if k in n: return t
    if 'Radioactive' in n: return 'IAEA SSR-6: groups ≥ 6 m apart'
    if kind == 'dg': return 'own fire room (EI 90), sump'
    if 'Pharma' in n: return 'EU GDP: mapped, alarmed'
    if 'HRCM' in n: return 'EU 2015/1998 6.7: extra screening'
    if 'screening' in n or 'Mail' in n: return 'EU 2015/1998 ch. 6: screened before secure'
    if 'Known-consignor' in n: return 'EU 2015/1998 6.4: secure supply chain'
    if 'unloading' in n.lower() and zk == 'exp': return 'unscreened side of the security line'
    if 'Customs' in n or 'Bonded' in n: return 'customs control, lockable'
    if 'WC' in n or 'break room' in n or 'lockers' in n or 'lounge' in n: return 'ArGV 3 Art. 29-33: near the workplaces'
    if 'battery' in n: return 'charging room ventilated, fire-rated'
    if 'sprinkler' in n: return 'sprinkler system for the racks over 9 m'
    if 'Human remains' in n: return 'separate, dignified, own door'
    if 'Valuables' in n: return 'strong room, CCTV'
    return ''
# #endregion


# #region ── Sheet 1: ground-floor plan ──
fig = sheet('CARGO TERMINAL  -  GROUND FLOOR PLAN',
            f'Freight only  ·  {L:.0f} x {D:.0f} m ({L * D:,.0f} m²)  ·  column grid 15 x 35 m  ·  aisles and corridors 6.5 m  ·  every door shown  ·  '
            'apron at the bottom  ·  metres')
ax = fig.add_axes([0.005, 0.30, 0.99, 0.585]); ax.set_facecolor(BG)
ax.set_xlim(-16, 481); ax.set_ylim(-38, 110); ax.set_aspect('equal'); ax.axis('off')
for name, x0, y0, x1, y1, zk, kind, doors, short in CL.ROOMS: area(ax, x0, y0, x1, y1, C[zk], kind)
equipment(ax)
gr = ground_plan_rooms()
walls(ax, gr, CL.SKIP_X, CL.SKIP_Y, lw=2.2)
fire_walls(ax)
ax.add_patch(Rectangle((0, 0), L, D, fill=False, ec=INK, lw=4))
door_symbols(ax, CL.doors_from_rooms(gr, CL.SKIP_X, CL.SKIP_Y), lw=1.8)
outside_doors(ax)
security_line(ax)
ax.text(37, (CL.A2[0] + CL.A2[1]) / 2, 'security line ↓ secure', color=SEC, fontsize=9.5, va='center', fontweight='bold')
for rr in CL.ROOMS:
    name, x0, y0, x1, y1, zk, kind, doors, short = rr
    if short:
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, short, color=INK, fontsize=12, ha='center', va='center', fontweight='bold', linespacing=1.1,
                bbox=dict(fc=BG, ec='none', pad=1.2, alpha=0.75))
    elif kind != 'aisle':
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, str(CL.NUM[id(rr)]), color=INK, fontsize=10.5 if x1 - x0 < 8 else 12, ha='center', va='center',
                fontweight='bold', bbox=dict(boxstyle='circle,pad=0.18', fc=BG, ec=C[zk], lw=1.4))
for zk, (a, b), _, _ in CL.ZONES: dim(ax, a, b, 90, f'{b - a:.0f}', size=13)
dim(ax, 0, L, -28, f'{L:.0f}', size=15)
dim(ax, 0, D, -9, f'{D:.0f}', vertical=True, size=14)
for a, b in ((0, 23.5), (23.5, 30), (30, 50.5), (50.5, 57), (57, 70)):
    dim(ax, a, b, 473, f'{b - a:g}', vertical=True, size=10.5)
ax.add_patch(Rectangle((CL.EXP[0], -12), CL.IMP[1] - CL.EXP[0], 11.7, fill=False, ec='#c7a6ff', lw=2, ls='--'))
ax.text(188, -6.2, 'AIRSIDE CANOPY 12 m deep, 9 m clear  ·  21 ULD doors 6 m wide  ·  roller decks to the dollies', color='#c7a6ff', fontsize=12.5,
        ha='center', va='center', fontweight='bold')
ax.text(232, 76, '24 truck docks every 13 m (3 x 3.5 m doors, 1.2 m high)  ·  8 sealed cool-chain / BCP docks  ·  2 drive-in doors  ·  canopy 6 m',
        color=C['land'], fontsize=12.5, ha='center', va='center', fontweight='bold')
ax.text(-14, -35, 'APRON  ↓', color=DIM, fontsize=12, va='center'); ax.text(-14, 107, 'TRUCK COURT  ↑', color=DIM, fontsize=12, va='center')
ax.text(435, 82, 'BCP access lane ↑', color=C['bcp'], fontsize=11.5, ha='center', va='center', fontweight='bold')
x = 0.012
for zk, _, _, t in CL.ZONES:
    fig.patches.append(Rectangle((x, 0.262), 0.010, 0.020, transform=fig.transFigure, fc=C[zk], ec='none'))
    fig.text(x + 0.014, 0.272, t, color=INK, fontsize=13, va='center'); x += 0.036 + 0.0068 * len(t)
lg = fig.add_axes([0.0, 0.17, 1.0, 0.07]); lg.set_xlim(0, 3840); lg.set_ylim(0, 150); lg.axis('off'); lg.set_facecolor(BG)
items = [('personnel / escape door 1.0 m, with swing', 'p'), ('sectional / rapid door 3-4.5 m', 's'), ('ULD, dock, drive-in doors', 'o'),
         ('fire wall', 'f'), ('fire shutter 6.5 m', 'fs'), ('security line', 'sec')]
xx = 40
for t, k in items:
    if k == 'p': lg.plot([xx, xx], [40, 110], color=GREEN, lw=3); lg.add_patch(Arc((xx, 40), 140, 140, theta1=0, theta2=90, color=GREEN, lw=2.5))
    if k == 's': lg.plot([xx, xx + 70], [75, 75], color=AMBER, lw=4, ls=(0, (2, 1.4)))
    if k == 'o': lg.add_patch(Rectangle((xx, 55), 70, 40, fc=AMBER, ec='none'))
    if k == 'f': lg.plot([xx, xx + 70], [75, 75], color=RED, lw=6)
    if k == 'fs': lg.plot([xx, xx + 70], [75, 75], color=RED, lw=4, ls=(0, (1.5, 1.2)))
    if k == 'sec': lg.plot([xx, xx + 70], [75, 75], color=SEC, lw=4, ls=(0, (1, 1.2)))
    lg.text(xx + 95, 75, t, color=INK, fontsize=13, va='center'); xx += 150 + 18.5 * len(t)
notes = ['Escape: at most 50 m to two exits (VKF 16-15)  ·  escape doors every 30 m on both long faces, in the fire walls and the east end  ·  '
         'three office stairs (one per 900 m²)',
         'Aisles 6.5 m = two 2.5 m loads + 2 x 0.5 m + 0.4 m (Suva)  ·  WC and break room in every hall (ArGV 3)  ·  south of the security line '
         'only screened cargo (EU 2015/1998)',
         'Border control post to EU 2019/1014  ·  dangerous goods one fire-rated room per class  ·  radioactive groups ≥ 6 m apart (IAEA SSR-6)  ·  '
         'numbered rooms: see sheet 2']
for i, t in enumerate(notes): fig.text(0.012, 0.135 - i * 0.04, t, color=DIM, fontsize=13.2, va='center')
fig.savefig(os.path.join(OUT, 'Cargo-1-Floor-Plan.png'), facecolor=BG); plt.close(fig)
# #endregion

# #region ── Sheet 2: room schedule ──
fig = sheet('CARGO TERMINAL  -  ROOM SCHEDULE', 'Every room and working area on the ground floor: number (as on the plans), size, area and the rule it is '
            'sized or placed by  ·  metres')
rows = [r for r in CL.ROOMS if r[6] != 'aisle']
half = (len(rows) + 1) // 2
rh = 0.80 / half
for col, part in enumerate((rows[:half], rows[half:])):
    x0 = 0.012 + col * 0.497
    hy = 0.885
    for t, dxp in (('No', 0.0), ('Room', 0.024), ('Size', 0.214), ('m²', 0.262), ('Rule / reason', 0.294)):
        fig.text(x0 + dxp, hy, t, color=DIM, fontsize=12.5, fontweight='bold', va='center')
    for i, r in enumerate(part):
        name, a, b, c, d_, zk, kind, doors, short = r
        y = hy - (i + 1) * rh
        if i % 2 == 0: fig.patches.append(Rectangle((x0 - 0.004, y - rh / 2), 0.488, rh, transform=fig.transFigure, fc='#0c2648', ec='none'))
        n = str(CL.NUM[id(r)]) if id(r) in CL.NUM else '·'
        fig.text(x0, y, n, color=C[zk], fontsize=12, fontweight='bold', va='center')
        fig.text(x0 + 0.024, y, name if len(name) <= 44 else name[:43] + '…', color=INK, fontsize=12, va='center')
        fig.text(x0 + 0.214, y, f'{c - a:g} x {d_ - b:g}', color=INK, fontsize=12, va='center')
        fig.text(x0 + 0.262, y, f'{(c - a) * (d_ - b):,.0f}', color=INK, fontsize=12, va='center')
        fig.text(x0 + 0.294, y, rule(name, kind, zk), color=DIM, fontsize=11, va='center')
fig.text(0.012, 0.03, 'Office block upper floors (sheet 3): canteen, offices, training, meeting rooms, the operations control centre, WCs and the three stairs on every floor.',
         color=DIM, fontsize=13, va='center')
fig.savefig(os.path.join(OUT, 'Cargo-2-Room-Schedule.png'), facecolor=BG); plt.close(fig)
# #endregion

# #region ── Sheet 3: office block, three floors ──
fig = sheet('CARGO TERMINAL  -  OFFICE BLOCK, THREE FLOORS', 'Office block 30 x 70 m (2,100 m² a floor)  ·  4 m corridor  ·  three stairs (one per 900 m²)  ·  '
            'every door with its swing  ·  airside at the bottom  ·  metres')
ax = fig.add_axes([0.01, 0.02, 0.98, 0.86]); ax.set_facecolor(BG)
ax.set_xlim(-10, 186); ax.set_ylim(-14, 84); ax.set_aspect('equal'); ax.axis('off')
ppm = 3840 * 0.98 / 196
plans = [('GROUND FLOOR', 0.0, [(n, x0, y0, x1, y1, k, d) for n, x0, y0, x1, y1, zk, k, d, _ in CL.ROOMS if zk == 'off']),
         ('1st FLOOR  (+4.6 m)', 46.0, CL.UPPER[1]), ('2nd FLOOR  (+9.2 m)', 92.0, CL.UPPER[2])]
for title, dx, rooms in plans:
    for n, x0, y0, x1, y1, k, d in rooms: area(ax, x0, y0, x1, y1, C['off'], k, dx)
    rr = [(x0, y0, x1, y1, 5.0, d, 1.0) for n, x0, y0, x1, y1, k, d in rooms if k in CL.WALLED]
    walls(ax, rr, (0.0, 30.0), (0.0, D), lw=3, dx=dx)
    ax.add_patch(Rectangle((dx, 0), 30, D, fill=False, ec=INK, lw=4))
    door_symbols(ax, CL.doors_from_rooms(rr, (0.0, 30.0), (0.0, D)), dx, lw=2.6)
    for n, x0, y0, x1, y1, k, d in rooms:
        if k == 'aisle':
            ax.text((x0 + x1) / 2 + dx, (y0 + y1) / 2, n, color='#ffd166', fontsize=11, ha='center', va='center', rotation=90)
        else:
            fit_text(ax, x0 + dx, y0 + (0.8 if k == 'stair' else 0), x1 + dx, y1 - (0.8 if k == 'stair' else 0), n, ppm)
    ax.text(15 + dx, 79.5, title, color=C['off'], fontsize=17, ha='center', va='center', fontweight='bold')
    dim(ax, dx, 13 + dx, -4, '13', size=12); dim(ax, 13 + dx, 17 + dx, -9.5, '4', size=11); dim(ax, 17 + dx, 30 + dx, -4, '13', size=12)
outside_doors(ax, only=(0, 29))
ax.add_patch(Rectangle((10, D - 0.4), 10, 0.8, fc=GREEN, ec='none'))
ax.text(15, 74.5, 'entrance ↑', color=DIM, fontsize=12, ha='center', va='center')
dim(ax, 0, D, -6, '70', vertical=True, size=13)
tx = 132
for i, t in enumerate(['Escape routes (VKF 16-15):', '· every room ≤ 35 m to a stair', '  or an exit', '· three stairs, 1.2 m flights',
                       '· doors at least 0.9 m clear', '· corridor 4 m (1.2 m needed)', '', 'Social rooms (ArGV 3):',
                       '· WC on every floor', '· lockers, showers, canteen,', '  first aid & rest room', '', 'Doors:',
                       '· green: door leaf and swing', '· all 1.0 m clear, 2.1 m high']):
    ax.text(tx, 68 - i * 4.6, t, color=INK if t.endswith(':') else DIM, fontsize=13.5, va='center', fontweight='bold' if t.endswith(':') else 'normal')
fig.savefig(os.path.join(OUT, 'Cargo-3-Office-Floors.png'), facecolor=BG); plt.close(fig)
# #endregion

# #region ── Sheet 4: special cargo and the border control post ──
fig = sheet('CARGO TERMINAL  -  SPECIAL CARGO AND BORDER CONTROL POST', 'Special cargo 60 x 70 m  ·  border control post 60 x 70 m: animals unit and '
            'animal products unit, each one integrated block  ·  every door shown  ·  airside at the bottom  ·  metres')
ax = fig.add_axes([0.005, 0.02, 0.99, 0.86]); ax.set_facecolor(BG)
ax.set_xlim(331, 498); ax.set_ylim(-12, 79); ax.set_aspect('equal'); ax.axis('off')
ppm = 3840 * 0.99 / 167
sub = [r for r in CL.ROOMS if r[5] in ('spc', 'bcp')]
for name, x0, y0, x1, y1, zk, kind, doors, short in sub: area(ax, x0, y0, x1, y1, C[zk], kind)
equipment(ax, only=(CL.SPC[0], L))
gr2 = ground_plan_rooms(sub)
walls(ax, gr2, CL.SKIP_X, CL.SKIP_Y, lw=3)
fire_walls(ax, only=(CL.SPC[0], L), lw=6)
ax.add_patch(Rectangle((CL.SPC[0], 0), L - CL.SPC[0], D, fill=False, ec=INK, lw=4))
door_symbols(ax, CL.doors_from_rooms(gr2, CL.SKIP_X, CL.SKIP_Y), lw=2.6)
outside_doors(ax, only=(CL.SPC[0] + 1, L))
for name, x0, y0, x1, y1, zk, kind, doors, short in sub:
    if kind == 'aisle':
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, name, color='#ffd166', fontsize=11.5, ha='center', va='center')
    else:
        fit_text(ax, x0, y0 + (1.5 if y0 == 0 else 0), x1, y1 - (1.5 if y1 == D else 0), name, ppm, sizes=(13, 12, 11, 10, 9, 8, 7.5))
ax.text(375, 75, 'SPECIAL CARGO', color=C['spc'], fontsize=17, ha='center', fontweight='bold')
ax.text(420, 75, 'BCP  ·  ANIMALS', color=C['bcp'], fontsize=17, ha='center', fontweight='bold')
ax.text(450, 75, 'BCP  ·  PRODUCTS', color=C['bcp'], fontsize=17, ha='center', fontweight='bold')
dim(ax, CL.SPC[0], CL.BCP[0], -6, '60', size=13); dim(ax, CL.BCP[0], CL.BCP_SPLIT, -6, '30', size=13); dim(ax, CL.BCP_SPLIT, L, -6, '30', size=13)
ys = [0, 23.5, 30, 40, 50.5, 57, 70]
for a, b in zip(ys, ys[1:]): dim(ax, a, b, L + 9, f'{b - a:g}', vertical=True, size=12)
ax.text(338, 35, 'import ←', color=DIM, fontsize=13, ha='center', va='center', rotation=90)
ax.text(375, -10.3, 'cold rooms open straight onto the apron', color=DIM, fontsize=12.5, ha='center', va='center')
ax.text(435, -10.3, 'horse containers and crates: 5 m doors', color=DIM, fontsize=12.5, ha='center', va='center')
ax.text(486, 35, 'no forklift opening between special cargo\nand the border control post (biosecurity)',
        color=DIM, fontsize=12, ha='center', va='center', rotation=90)
fig.savefig(os.path.join(OUT, 'Cargo-4-Special-Cargo-and-BCP.png'), facecolor=BG); plt.close(fig)
# #endregion

# #region ── Sheet 5: site plan (local metres: building 0..L x 0..70, apron at -Y) ──
fig = sheet('CARGO TERMINAL  -  SITE PLAN',
            f'On the far side of the runway from the passenger terminal  ·  plan x 200-{200 + L:.0f}, y 1231-1301  ·  dimensions in metres  ·  airside at the bottom')
ax = fig.add_axes([0.02, 0.09, 0.96, 0.80]); ax.set_facecolor(BG)
ax.set_xlim(-125, 600); ax.set_ylim(-195, 232); ax.set_aspect('equal'); ax.axis('off')
ax.add_patch(Rectangle((-30, -165), 480, 165, fc='#0d2a4d', ec=DIM, lw=1.5))
ax.add_patch(Rectangle((-125, -157.5), 725, 25, fc='#14365f', ec='none'))
ax.text(540, -145, 'TAXILANE K', color=INK, fontsize=12, va='center', fontweight='bold')
for x, n in ((0, 'Q1  to taxiway B'), (420, 'Q2  to taxiway B')):
    ax.add_patch(Rectangle((x - 12.5, -195), 25, 38, fc='#14365f', ec='none'))
    ax.text(x + 16, -184, n, color=INK, fontsize=11.5, va='center')
for i, sx in enumerate((60.0, 147.5, 235.0, 322.5)):
    ax.plot([sx, sx], [-132, -15], color='#ffd166', lw=2.2)
    ax.add_patch(Circle((sx, -15 - 38), 40, fill=False, ec='#ffd166', lw=1.0, ls=':'))
    ax.text(sx, -138, f'C{i + 1}', color='#ffd166', fontsize=16, ha='center', va='top', fontweight='bold')
ax.text(191, -95, 'CARGO APRON  -  4 code F freighter stands (747-8F / 777F)', color=INK, fontsize=14,
        ha='center', va='center', fontweight='bold', bbox=dict(fc='#0d2a4d', ec='none', pad=2))
ax.add_patch(Rectangle((-40, -40), L + 100, 40, fc='#123a68', ec='none'))
ax.text(80, -21, 'ULD staging lanes & airside service road (40 m)', color=INK, fontsize=12, va='center')
for n, x0, y0, x1, y1 in CL.OUTSIDE:
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc='none', ec='#c7a6ff', lw=1.6, ls='--'))
ax.text(516, -26, 'empty ULD store', color='#c7a6ff', fontsize=11, va='center')
ax.text(516, -8, 'ULD repair shed', color='#c7a6ff', fontsize=11, va='center')
ax.text(-45, -21, 'dollies & GSE\n(charging)', color='#c7a6ff', fontsize=11, va='center', ha='right')
for zk, (x0, x1), _, _ in CL.ZONES:
    ax.add_patch(Rectangle((x0, 0), x1 - x0, 70, fc=C[zk], ec=INK, lw=1.4, alpha=0.35))
ax.add_patch(Rectangle((0, 0), L, 70, fill=False, ec=INK, lw=3))
ax.text(L / 2, 35, f'CARGO TERMINAL  {L:.0f} x {D:.0f} m', color=INK, fontsize=18, ha='center', va='center', fontweight='bold')
ax.add_patch(Rectangle((-10, 70), L + 20, 140, fc='#10304f', ec='none'))
ax.text(200, 105, 'TRUCK COURT  (turning depth 52 m)', color=INK, fontsize=14, ha='center', va='center', fontweight='bold')
ax.add_patch(Rectangle((40, 180), 300, 22, fill=False, ec=C['land'], lw=1.6))
ax.text(190, 191, 'lorry parking  -  60 bays', color=C['land'], fontsize=12.5, ha='center', va='center')
ax.add_patch(Rectangle((CL.BCP_LANE[0], 92), CL.BCP_LANE[1] - CL.BCP_LANE[0], 116, fc='none', ec=GREEN, lw=1.8, hatch='//'))
ax.text(435, 150, 'BCP\naccess\nlane', color=GREEN, fontsize=12.5, ha='center', va='center', fontweight='bold', bbox=dict(fc='#10304f', ec='none', pad=2))
ax.add_patch(Rectangle((-8, 90), 36, 5, fill=False, ec=C['off'], lw=1.6))
ax.text(10, 140, 'staff\ncars', color=C['off'], fontsize=11, ha='center', va='center')
ax.plot([-10, 150], [210, 210], color='#ff6b6b', lw=3); ax.plot([175, L + 10], [210, 210], color='#ff6b6b', lw=3)
ax.plot([-125, 600], [89, 89], color='#ff6b6b', lw=1.8, ls=':')
ax.text(L + 14, 93, "today's fence (plan y 1320):\nmoved out", color='#ff9a9a', fontsize=12, va='bottom')
ax.text(-120, 213, 'new fence (plan y 1441)', color='#ff9a9a', fontsize=12, va='bottom')
ax.add_patch(Rectangle((152, 196), 8, 6, fc=C['off'], ec='none'))
ax.text(158, 222, 'security gate + gatehouse', color=INK, fontsize=12, ha='right', va='center')
ax.add_patch(FancyArrowPatch((163, 226), (163, 200), arrowstyle='-|>', mutation_scale=20, color=INK, lw=2))
ax.text(170, 222, "  ← access road to the landside roads (round the runway's east end)", color=DIM, fontsize=11.5, ha='left', va='center')
fig.text(0.02, 0.04, f'The building grew from 390 to {L:.0f} m for the special-cargo rooms and the border control post (plan x 590 → {200 + L:.0f}).  '
         'Room is kept for the future 2nd runway beyond the new fence.  Stand circles: 80 m wingspan.', color=DIM, fontsize=13)
dim(ax, 0, L, -4, f'{L:.0f}', size=12)
dim(ax, -165, 0, -112, '165 apron', vertical=True, size=12)
dim(ax, 0, 70, -112, '70', vertical=True, size=12)
dim(ax, 70, 210, -112, '140 court', vertical=True, size=12)
dim(ax, 60, 147.5, -118, '87.5 m between stands', size=11)
fig.savefig(os.path.join(OUT, 'Cargo-5-Site-Plan.png'), facecolor=BG); plt.close(fig)
# #endregion
print('sheets written')
