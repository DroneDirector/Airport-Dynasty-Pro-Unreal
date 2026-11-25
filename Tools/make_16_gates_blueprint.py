# 16 gates blueprint (owner, 2026-11-17): the terminal as built today (arrivals wing / baggage claim of 2026-11-16 and
# its fixes of 11-17) with the gate walkway lengthened at both ends:
#   west: 4 gates exactly like today's (2 jetway + 2 walk-out, A320 family), the walkway, and a huge TAX FREE on Floor 2
#         beside the baggage claim hall (Floor 1 under it empty for now), under the wave roof;
#   east: 4 wide-body gates - 1 x A380 (3 bridges, one to the upper deck) + 3 x B777 (2 bridges each) - flush with the
#         gate rooms and the walkway, the old A380 remote stand 9 taken into it; the landside from the offices to the
#         walkway's new end kept free (east wing, empty for now) under the wave roof;
#   moved out of the way: the car rental + its garage/parking (to a new site outside the airport boundary, beside the
#         main car park on the road to town) and the control tower (west of the new west gates).
# Black background, light lines (documents rule). Drawings only - nothing in the game changes.
#   python Tools/make_16_gates_blueprint.py   ->  docs/Blueprints/16-Gates-*.png
import json, os, math
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, Circle, FancyArrowPatch
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'Blueprints'); os.makedirs(OUT, exist_ok=True)
P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
AF = json.load(open(os.path.join(ROOT, 'Data', 'airfield.json')))
AF['stands'] = [s for s in AF['stands'] if s.get('built', True)]   # (remote stands not in use yet are left out)
BG, INK, DIM, OLDC, NEW, MOVE, GONE, YEL, RED, PLANE, ROOFC = ('#000000', '#e8edf2', '#5b6773', '#8d99a6', '#35d0e0',
    '#7dff8a', '#ff5a5a', '#ffd23f', '#ff4040', '#c9d6e3', '#4fa3ff')

# ---------------- the plan ----------------
WALK_Z0, WALK_Z1, GATE_Z1 = 52.0, 68.0, 82.0          # walkway (F2) / baggage hall (F1) and the gate rooms in front
MOD = 80.55                                            # one gate pair (jetway gate + walk-out gate) along the walkway
STAND_PAIR = 81.71                                     # the stands' pitch for one pair (as built: STAND-1 -62, STAND-2 19.71)
W_END = -80.0 - 2 * MOD                                # walkway's new west end  (-241.1)
# west pairs: as built, one and two modules further west
WEST = []
for k in (2, 1):
    r0 = -72.6 - k * MOD
    WEST.append(dict(ramp=(r0, r0 + 28), f2gate=(r0, r0 + 34), view=(r0 + 34, r0 + 68), f1gate=(r0 + 28, r0 + 68),
                     sj=-62 - k * STAND_PAIR, sw=-21.14 - k * STAND_PAIR))
# east: wide-body stands side by side, 7.5 m between wingtips (code E/F), the first 7.5 m clear of gate 8's A320
TYPES = {  # length, span, fuselage width, nose->wing root LE, root chord, tip sweep offset, tip chord, tail span, tail at
    'A320': (37.6, 35.8, 3.95, 12.6, 6.1, 7.0, 1.6, 12.5, 32.0),
    'A321': (44.5, 35.8, 3.95, 15.5, 6.1, 7.0, 1.6, 12.5, 39.0),
    'B777': (73.9, 64.8, 6.20, 27.0, 12.5, 15.5, 2.6, 21.5, 64.0),
    'A380': (72.7, 79.8, 7.14, 23.5, 17.5, 18.0, 4.0, 30.4, 61.5),
}
NOSE_WB = 95.0           # wide-body noses: tails end by y 169, clear of taxilane T (y 218) for an A380 taxiing past
E = []
x = 242.0 + 7.5
for i, kind in enumerate(('A380', 'B777', 'B777', 'B777')):
    s = TYPES[kind][1]; cx = x + s / 2; E.append(dict(kind=kind, x=cx, gate=13 + i)); x = cx + s / 2 + 7.5
E_END = x - 7.5 + 5.0                                  # walkway's new east end (~551)
TOWER_OLD, TOWER_NEW = (-205.0, 120.0), (-345.0, 120.0)
RENTAL_OLD = (156, -6.2, 312, 50.9)
RENTAL_NEW = (392, -250, 560, -178)                    # outside the boundary (y < -170), beside the main car park
TAXI_Y = 218.0

def rect(ax, x0, y0, x1, y1, ec=DIM, fc='none', lw=0.8, ls='-', z=2, alpha=1):
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=fc != 'none', fc=fc, ec=ec, lw=lw, ls=ls, zorder=z, alpha=alpha))
FSM = [1.0]          # text size factor for the sheet being drawn (big text on the close-up sheets)
STOPS = [True]
def label(ax, x, y, t, c=OLDC, fs=5.2, **k):
    k.setdefault('ha', 'center'); ax.text(x, y, t, color=c, fontsize=fs * FSM[0], va='center', zorder=9, **k)

def plane(ax, cx, ny, kind, col=PLANE, lw=0.7):
    """Simple top view of the aircraft, nose at (cx, ny) pointing to the terminal (-y)."""
    L, S, fw, wr, rc, sw, tc, ts, ta = TYPES[kind]; h = fw / 2
    body = [(cx - h, ny + 3), (cx - h * 0.6, ny + 0.6), (cx, ny), (cx + h * 0.6, ny + 0.6), (cx + h, ny + 3),
            (cx + h, ny + L * 0.82), (cx + h * 0.35, ny + L), (cx - h * 0.35, ny + L), (cx - h, ny + L * 0.82)]
    ax.add_patch(Polygon(body, closed=True, fill=False, ec=col, lw=lw, zorder=6))
    for sgn in (-1, 1):
        wing = [(cx + sgn * h, ny + wr), (cx + sgn * S / 2, ny + wr + sw), (cx + sgn * S / 2, ny + wr + sw + tc),
                (cx + sgn * h, ny + wr + rc)]
        ax.add_patch(Polygon(wing, closed=True, fill=False, ec=col, lw=lw, zorder=6))
        tail = [(cx + sgn * h * 0.5, ny + ta), (cx + sgn * ts / 2, ny + ta + ts * 0.3), (cx + sgn * ts / 2, ny + ta + ts * 0.3 + 2),
                (cx + sgn * h * 0.4, ny + ta + ts * 0.32)]
        ax.add_patch(Polygon(tail, closed=True, fill=False, ec=col, lw=lw, zorder=6))
        n_eng = 2 if kind == 'A380' else 1
        for e in range(n_eng):
            ex = cx + sgn * (h + (S / 2 - h) * (0.33 if e == 0 else 0.62))
            ey = ny + wr + sw * (0.33 if e == 0 else 0.62) - 3.5
            ew = 2.0 if kind == 'A320' else 3.4
            rect(ax, ex - ew / 2, ey, ex + ew / 2, ey + (4.4 if kind == 'A320' else 6.5), ec=col, lw=lw * 0.8, z=6)

def stand_marks(ax, cx, ny, kinds, lead=True, labels=None):
    """Ground markings: yellow lead-in line from taxilane T, stop bars per type, red stand safety line,
    equipment restraint line, wingtip clearance box."""
    L = max(TYPES[k][0] for k in kinds); S = max(TYPES[k][1] for k in kinds)
    if lead:
        ys = np.linspace(ny + L + 4, TAXI_Y, 20)
        ax.plot([cx] * 2, [ny - 1, ny + L + 4], color=YEL, lw=0.9, zorder=4)
        t = np.linspace(0, math.pi / 2, 16); r = TAXI_Y - (ny + L + 4)
        ax.plot(cx + r - r * np.cos(t), ny + L + 4 + r * np.sin(t), color=YEL, lw=0.9, zorder=4)
    for i, k in enumerate(labels or kinds):             # stop bars: each type's nose stop
        off = i * 2.2
        ax.plot([cx - 2.5, cx + 2.5], [ny - 1 - off, ny - 1 - off], color=YEL, lw=1.2, zorder=5)
    if STOPS[0]:
        ax.text(cx - 4, ny - 0.5, 'stop bars:\n' + '\n'.join(labels or kinds), color=YEL, fontsize=3.4 * FSM[0], va='top', ha='right', zorder=9,
                linespacing=1.05)
    rect(ax, cx - S / 2 - 3.75, ny - 4, cx + S / 2 + 3.75, ny + L + 3, ec=RED, lw=0.6, ls='--', z=3)   # stand safety line
    ax.plot([cx - S / 2 - 3.75, cx + S / 2 + 3.75], [ny - 6.5] * 2, color=RED, lw=0.6, zorder=3)        # equipment restraint

def jetway(ax, door_x, door_y, rot_x, rot_y=88.0, col=NEW, ls='-'):
    ax.plot([rot_x, rot_x], [GATE_Z1, rot_y], color=col, lw=2.2, solid_capstyle='butt', zorder=7)      # fixed link
    ax.add_patch(Circle((rot_x, rot_y), 2.2, fill=False, ec=col, lw=0.9, zorder=7))                     # rotunda
    ax.plot([rot_x, door_x], [rot_y, door_y], color=col, lw=2.6, ls=ls, solid_capstyle='butt', zorder=7)  # tunnel
    rect(ax, door_x - 0.3, door_y - 1.6, door_x + 2.6, door_y + 1.6, ec=col, fc=col, lw=0.4, z=8)        # cab

def a320_jetway(ax, sx, ny=120.0, col=NEW):
    jetway(ax, sx + 2.0, ny + 5.0, sx + 13.0, col=col)

def wb_jetways(ax, e):
    L, S, fw, *_ = TYPES[e['kind']]; h = fw / 2; cx = e['x']
    if e['kind'] == 'A380':
        doors = [(NOSE_WB + 10.3, '-'), (NOSE_WB + 22.5, '-'), (NOSE_WB + 14.5, '--')]       # M1L, M2L, U1L (upper deck)
        rots = [cx + 20, cx + 34, cx + 27]
    else:
        doors = [(NOSE_WB + 6.6, '-'), (NOSE_WB + 18.2, '-')]                                 # 1L, 2L
        rots = [cx + 18, cx + 31]
    for (dy, ls), rx in zip(doors, rots): jetway(ax, cx + h, dy, rx, col=NEW, ls=ls)

# ---------------- what is there today ----------------
OLD_ROOMS = {'F1-MUSEUM', 'F1-PLANT', 'F2-WC-M', 'F2-WC-F', 'F2-FAMILY'}
TODAY = {1: [('Escalator room', 0, 0, 36, 30), ('Plant room', 6, 30, 36, 52)],
         2: [('Landing', 0, 0, 21, 30), ('WC', 0, 0, 14, 5), ('WC', 0, 5, 14, 10), ('Family', 0, 10, 14, 22)]}
ARR_F1 = [('BAGGAGE CLAIM (5 belts)', -80, 16, 0, 52), ('Toilets', -80, 8, -56, 16), ('Customs', -56, 8, -24, 16),
          ('Customs office', -24, 8, 0, 16), ('ARRIVALS HALL', -80, -4, 0, 8), ('store', 0, 30, 6, 52)]
ARR_F2 = [('Arrivals corridor', -80, 40, -66, 52)]

def rooms_today(ax, f, small=True):
    for r in P['rooms']:
        if r['id'] in OLD_ROOMS: continue
        fl = 2 if r['id'].startswith('F2') else 1
        if fl != f: continue
        x0, z0, x1, z1 = r['x0'], r['z0'], r['x1'], r['z1']
        rect(ax, x0, z0, x1, z1, ec=DIM, lw=0.6)
        if small and (x1 - x0) > 9 and (z1 - z0) > 5:
            label(ax, (x0 + x1) / 2, (z0 + z1) / 2, r['name'].split(' (')[0], fs=3.6)
    for name, x0, z0, x1, z1 in TODAY[f]:
        rect(ax, x0, z0, x1, z1, ec=DIM, lw=0.6)
        if small: label(ax, (x0 + x1) / 2, (z0 + z1) / 2, name, fs=3.4)
    for name, x0, z0, x1, z1 in (ARR_F1 if f == 1 else ARR_F2):
        rect(ax, x0, z0, x1, z1, ec=OLDC, lw=0.8)
        if small: label(ax, (x0 + x1) / 2, (z0 + z1) / 2, name, fs=3.6 if x1 - x0 > 20 else 3.0, c=INK)

def new_parts(ax, f, small=True):
    # walkway (F2) / baggage hall (F1), both ends
    for x0, x1 in ((W_END, -80.0), (242.0, E_END)):
        rect(ax, x0, WALK_Z0, x1, WALK_Z1, ec=NEW, fc=NEW, lw=1.4, z=3, alpha=0.10)
        rect(ax, x0, WALK_Z0, x1, WALK_Z1, ec=NEW, lw=1.4, z=3)
        if small: label(ax, (x0 + x1) / 2, (WALK_Z0 + WALK_Z1) / 2,
                        'WALKWAY TO THE GATES (moving walkways)' if f == 2 else 'BAGGAGE HALL (sorting, under the walkway)', c=NEW, fs=4.2)
    # west gate pairs, as today's
    for i, g in enumerate(WEST):
        if f == 2:
            rect(ax, *g['f2gate'][:1], WALK_Z1, g['f2gate'][1], GATE_Z1, ec=NEW, lw=1.2, z=3)
            rect(ax, g['view'][0], WALK_Z1, g['view'][1], GATE_Z1, ec=NEW, lw=1.2, z=3)
            if small:
                label(ax, sum(g['f2gate']) / 2, 75, 'Gate %d\n(jetway)' % (9 + 2 * i), c=NEW, fs=3.8)
                label(ax, sum(g['view']) / 2, 75, 'View bay', c=NEW, fs=3.6)
        else:
            rect(ax, g['ramp'][0], WALK_Z1, g['ramp'][1], GATE_Z1, ec=NEW, lw=1.2, z=3)
            rect(ax, g['f1gate'][0], WALK_Z1, g['f1gate'][1], GATE_Z1, ec=NEW, lw=1.2, z=3)
            if small:
                label(ax, sum(g['ramp']) / 2, 75, 'Ramp\nservices', c=NEW, fs=3.6)
                label(ax, sum(g['f1gate']) / 2, 75, 'Gate %d\n(walk-out)' % (10 + 2 * i), c=NEW, fs=3.8)
    # east wide-body gate rooms (Floor 2: glass gate rooms; Floor 1: ramp services)
    for e in E:
        S = TYPES[e['kind']][1]; x0, x1 = e['x'] - S / 2 - 3.75, e['x'] + S / 2 + 3.75
        x0 = max(x0, 242.0); x1 = min(x1, E_END)
        rect(ax, x0, WALK_Z1, x1, GATE_Z1, ec=NEW, lw=1.2, z=3)
        if small: label(ax, (x0 + x1) / 2, 75, ('Gate %d  %s  (%s)' % (e['gate'], e['kind'], '3 bridges' if e['kind'] == 'A380' else '2 bridges'))
                        if f == 2 else 'Ramp services  (gate %d)' % e['gate'], c=NEW, fs=4.0)
    # the tax free (F2) and the empty floor under it, west of the claim hall
    rect(ax, W_END, 8, -80, WALK_Z0, ec=NEW, fc=NEW, lw=1.6, z=3, alpha=0.10 if f == 2 else 0.03)
    rect(ax, W_END, 8, -80, WALK_Z0, ec=NEW, lw=1.6, z=3)
    if small: label(ax, (W_END - 80) / 2, 30, 'TAX FREE  -  big walk-through shop, open onto the walkway\n161 x 44 m'
                    if f == 2 else 'empty for now (under the tax free)', c=NEW, fs=5.4 if f == 2 else 4.4, fontweight='bold' if f == 2 else None)
    # east wing: from the offices to the walkway's new end, kept free
    rect(ax, 156, 0, E_END, WALK_Z0, ec=NEW, lw=1.2, ls='--', z=3)
    if small: label(ax, (156 + E_END) / 2, 26, 'EAST WING  -  room kept free from the offices to the end of the new gates\n(empty for now; car rental moved out)',
                    c=NEW, fs=4.6)

def roof(ax):
    for x0, x1 in ((W_END - 4, -80), (156, E_END + 4)):
        rect(ax, x0, -10, x1, GATE_Z1 + 2, ec=ROOFC, lw=0.9, ls=(0, (6, 3)), z=5)
    label(ax, (W_END - 80) / 2, -7, 'wave roof (same profile as the terminal\'s) over the whole new west end', c=ROOFC, fs=3.8)
    label(ax, (156 + E_END) / 2, -7, 'wave roof (same profile) over the whole new east end', c=ROOFC, fs=3.8)

def frame(ax, title, xl, yl):
    ax.set_facecolor(BG); ax.set_aspect('equal'); ax.set_xlim(*xl); ax.set_ylim(*yl)
    ax.tick_params(colors=DIM, labelsize=5 * min(FSM[0], 2.2))
    for sp in ax.spines.values(): sp.set_color(DIM)
    ax.set_title(title, color=INK, fontsize=10 * min(FSM[0], 2.0), loc='left')

# ================= 1: site plan, top down =================
def draw_site(ax):
    # the apron, taxilane, links
    for a in AF['aprons']:
        if a['id'] in ('APRON-T', 'APRON-9'):
            rect(ax, a['x0'], a['y0'], a['x1'], a['y1'], ec=GONE if a['id'] == 'APRON-9' else DIM, lw=0.6, ls='--' if a['id'] == 'APRON-9' else '-', z=1)
    rect(ax, -265, 82, 570, 238, ec=MOVE, lw=0.9, ls='-.', z=1)
    label(ax, -240, 232, 'apron extended  x -265 .. 570', c=MOVE, fs=4.5)
    ax.plot([-140, 372], [TAXI_Y] * 2, color=DIM, lw=4, alpha=0.6, zorder=1)
    ax.plot([-255, -140], [TAXI_Y] * 2, color=MOVE, lw=4, alpha=0.6, zorder=1); ax.plot([372, 560], [TAXI_Y] * 2, color=MOVE, lw=4, alpha=0.6, zorder=1)
    label(ax, 470, 224, 'taxilane T lengthened (both ends)', c=MOVE, fs=4.5)
    for t in AF['taxiways']:
        if t['id'] in ('P1', 'P2', 'P3'): ax.plot([p[0] for p in t['pts']], [min(p[1], 250) for p in t['pts']], color=DIM, lw=4, alpha=0.6, zorder=1)
    # buildings today
    rect(ax, 0, 0, 156, 52, ec=OLDC, lw=1.0); label(ax, 78, 26, 'TERMINAL (today)\nGrand Hall, security, offices', fs=5)
    rect(ax, -80, -4, 0, 52, ec=OLDC, lw=1.0); label(ax, -40, 24, 'ARRIVALS WING\n(baggage claim, 2026-11-16)', fs=5)
    rect(ax, -80, WALK_Z0, 242, WALK_Z1, ec=OLDC, lw=1.0); label(ax, 81, 60, 'walkway to the gates (today)', fs=4.5)
    for r in P['rooms']:
        if r['id'].startswith('F2-GATE') or r['id'].startswith('F2-VIEW'): rect(ax, r['x0'], r['z0'], r['x1'], r['z1'], ec=OLDC, lw=0.8)
    for s in AF['stands']:
        if s['id'] in ('STAND-%d' % i for i in range(1, 9)):
            stand_marks(ax, s['x'], s['nose'], ['A320', 'A321'] if s['jetway'] else ['A320'])
            plane(ax, s['x'], s['nose'], 'A320', col=OLDC)
            if s['jetway']: a320_jetway(ax, s['x'], s['nose'], col=OLDC)
            n = int(s['id'].split('-')[1]); label(ax, s['x'], s['nose'] + 47, 'G%d' % n, c=INK, fs=5)
    # today's parts that move or go
    rect(ax, *RENTAL_OLD, ec=GONE, lw=1.0, ls='--'); label(ax, 234, 22, 'CAR RENTAL + garage (today)\n-> moved', c=GONE, fs=4.6)
    rect(ax, 174, -16, 282, -3.5, ec=GONE, lw=0.8, ls='--')
    s9 = next(s for s in AF['stands'] if s['id'] == 'STAND-9')
    rect(ax, s9['x'] - 40, s9['nose'], s9['x'] + 40, s9['nose'] + s9['len'], ec=GONE, lw=0.8, ls=':')
    label(ax, s9['x'] + 60, 46, 'remote A380 stand 9 (today)\n-> becomes gate 13 (A380 with bridges)', c=GONE, fs=4.2)
    ax.add_patch(Circle(TOWER_OLD, 9, fill=False, ec=GONE, lw=1.0, ls='--', zorder=5)); rect(ax, -232, 104, -180, 146, ec=GONE, lw=0.7, ls='--')
    label(ax, -206, 99, 'control tower (today) -> moved', c=GONE, fs=4.2)
    ax.add_patch(Circle(TOWER_NEW, 9, fill=False, ec=MOVE, lw=1.4, zorder=5)); rect(ax, TOWER_NEW[0] - 27, 104, TOWER_NEW[0] + 25, 146, ec=MOVE, lw=0.8)
    label(ax, TOWER_NEW[0], 152, 'CONTROL TOWER (moved 140 m west)\nsame tower, lot and road', c=MOVE, fs=4.4)
    ax.add_patch(FancyArrowPatch((-215, 120), (-334, 120), arrowstyle='-|>', mutation_scale=8, color=MOVE, lw=0.8, zorder=6))
    # the new parts
    new_parts(ax, 2, small=False); roof(ax)
    for i, g in enumerate(WEST):
        stand_marks(ax, g['sj'], 120.0, ['A320', 'A321']); plane(ax, g['sj'], 120.0, 'A320'); a320_jetway(ax, g['sj'])
        stand_marks(ax, g['sw'], 122.0, ['A320']); plane(ax, g['sw'], 122.0, 'A320')
        ax.plot([g['sw'] - 6, g['sw'] - 6], [GATE_Z1, 122], color=NEW, lw=0.6, ls=':', zorder=4)      # walk-out path
        label(ax, g['sj'], 167, 'G%d' % (9 + 2 * i), c=NEW, fs=5.5, fontweight='bold'); label(ax, g['sw'], 169, 'G%d' % (10 + 2 * i), c=NEW, fs=5.5, fontweight='bold')
    for e in E:
        kinds = ['A380', 'B777-9', 'B777-300ER', 'A350-1000'] if e['kind'] == 'A380' else ['B777-9', 'B777-300ER', 'B787-10']
        stand_marks(ax, e['x'], NOSE_WB, [e['kind'], 'B777'], labels=kinds)
        plane(ax, e['x'], NOSE_WB, e['kind']); wb_jetways(ax, e)
        label(ax, e['x'], 182, 'G%d  %s' % (e['gate'], 'A380' if e['kind'] == 'A380' else 'B777'), c=NEW, fs=5.8, fontweight='bold')
    # landside: roads, car parks, the car rental's new site
    ax.plot([-744, 537], [-47, -47], color=DIM, lw=6, alpha=0.5, zorder=1); label(ax, -300, -42, 'forecourt road', fs=4.5)
    ax.plot([-560, 582], [-255, -260], color=DIM, lw=6, alpha=0.5, zorder=1); label(ax, -300, -262, 'road back to town', fs=4.5)
    rect(ax, -93, -247, 367, -54.45, ec=DIM, lw=0.7); label(ax, 137, -150, 'main car park (today)', fs=5)
    ax.plot([-400, 600], [-170, -170], color='#a070ff', lw=0.8, ls='--', zorder=2); label(ax, 480, -165, 'airport boundary (y -170)', c='#a070ff', fs=4.5)
    rect(ax, *RENTAL_NEW, ec=MOVE, fc=MOVE, lw=1.4, alpha=0.08); rect(ax, *RENTAL_NEW, ec=MOVE, lw=1.4)
    rect(ax, RENTAL_NEW[0] + 4, RENTAL_NEW[1] + 30, RENTAL_NEW[0] + 60, RENTAL_NEW[3] - 4, ec=MOVE, lw=0.9)
    label(ax, RENTAL_NEW[0] + 32, (RENTAL_NEW[1] + 30 + RENTAL_NEW[3] - 4) / 2, 'rental\nhall', c=MOVE, fs=4.4)
    rect(ax, RENTAL_NEW[0] + 66, RENTAL_NEW[1] + 6, RENTAL_NEW[2] - 4, RENTAL_NEW[3] - 4, ec=MOVE, lw=0.9)
    label(ax, (RENTAL_NEW[0] + 66 + RENTAL_NEW[2] - 4) / 2, (RENTAL_NEW[1] + RENTAL_NEW[3]) / 2, 'rental garage\n(3 levels) + chargers', c=MOVE, fs=4.4)
    label(ax, (RENTAL_NEW[0] + RENTAL_NEW[2]) / 2, RENTAL_NEW[3] + 8, 'CAR RENTAL - NEW SITE (outside the boundary)', c=MOVE, fs=5, fontweight='bold')
    ax.plot([RENTAL_NEW[0] + 20, RENTAL_NEW[0] + 20, 370], [RENTAL_NEW[3], -150, -150], color=MOVE, lw=0.8, ls=':', zorder=2)
    label(ax, 390, -140, 'covered walk to the terminal', c=MOVE, fs=4)

def site_sheet(name, title, xl, yl, fsm, size, stops=True):
    FSM[0] = fsm; STOPS[0] = stops
    fig = plt.figure(figsize=size, facecolor=BG); ax = fig.add_axes([0.04, 0.05, 0.93, 0.88])
    frame(ax, title, xl, yl); draw_site(ax)
    fig.savefig(os.path.join(OUT, name), dpi=150, facecolor=BG); plt.close(fig)
site_sheet('16-Gates-1-Site-Plan.png', 'SITE PLAN, TOP DOWN - 16 gates (8 today + 4 west + 4 east wide-body), north (apron) up',
           (-400, 600), (-275, 250), 1.9, (24, 13.5), stops=False)
site_sheet('16-Gates-1a-West-End.png', 'CLOSE-UP: NEW WEST END - gates 9-12, tax free, tower moved', (-385, -30), (-20, 240), 3.0, (24, 18))
site_sheet('16-Gates-1b-East-End.png', 'CLOSE-UP: NEW EAST END - gate 13 A380, gates 14-16 B777', (228, 592), (-20, 240), 3.0, (24, 17.5))
site_sheet('16-Gates-1c-Car-Rental.png', 'CLOSE-UP: CAR RENTAL - new site outside the airport boundary', (120, 600), (-280, 60), 2.8, (24, 17.5))
FSM[0] = 1.0
fig = plt.figure(figsize=(24, 13.5), facecolor=BG)
nx = fig.add_axes([0.03, 0.02, 0.94, 0.94]); nx.set_facecolor(BG); nx.axis('off')
NOTES = [
 'WHAT CHANGES (each change, why)',
 '1  Walkway lengthened 161 m WEST to x -241 (2 gate pairs exactly like today\'s: gates 9 + 11 with jetways, 10 + 12 walk-out, A320 family; same 80.55 m module, stands on the same 81.7 m pitch).',
 '2  Walkway lengthened 309 m EAST to x %.0f: gate 13 = A380 (3 bridges: main deck M1L, M2L + one to the upper deck), gates 14-16 = B777 (2 bridges each: 1L, 2L).' % E_END,
 '    Gate rooms sit flush on the walkway; each bridge leaves the gate room straight out to its rotunda; noses at y 95 so the tails stay clear of taxilane T for an A380 passing.',
 '3  Ground markings per stand: yellow lead-in line from taxilane T, stop bars per type (A320/A321; A380 / B777-9 / B777-300ER / A350-1000; B777-9 / B777-300ER / B787-10),',
 '    red stand safety line (span + 3.75 m each side), red equipment restraint line in front of the nose. Wide-body stands 7.5 m wingtip to wingtip (code E/F).',
 '4  TAX FREE: Floor 2, x -241 .. -80, beside the baggage claim (161 x 44 m), walk-through, open onto the walkway; Floor 1 under it empty for now.',
 '5  East wing: from the offices (x 156) to the new end kept free and roofed (empty for now) - the car rental and its garage/parking stood there.',
 '6  CAR RENTAL moved whole (hall, desks, garage, chargers, cars, its parking) to x 392..560, y -250..-178: outside the airport boundary, beside the main car park,',
 '    on the road back to town (seen first when leaving), covered walk along the car park to the terminal. 7  Wave roof: same profile, over the whole new west and east ends.',
 '8  Control tower + its lot and road moved 140 m west (x -345): the new west stands are where it stands today.  9  Remote A380 stand 9 and its apron become gate 13.',
 '10 Apron extended (x -265 .. 570) and taxilane T lengthened to match.  11  Gate numbers: today\'s 1-8 unchanged; new 9-12 west, 13-16 east.   12  The baggage claim (5 belts) stays as built.',
]
for i, t in enumerate(NOTES):
    nx.text(0.0, 1 - i * 0.077, t, color=INK if i else NEW, fontsize=15.5 if i else 22, fontweight='bold' if i == 0 else None, va='top', transform=nx.transAxes)
fig.savefig(os.path.join(OUT, '16-Gates-0-What-Changes.png'), dpi=150, facecolor=BG); plt.close(fig)

# ================= 2: floor plans =================
FSM[0] = 2.9
for f, half, xl in ((2, 'West', (-250, 165)), (2, 'East', (145, 560)), (1, 'West', (-250, 165)), (1, 'East', (145, 560))):
    fig = plt.figure(figsize=(24, 7.8), facecolor=BG); ax = fig.add_axes([0.04, 0.08, 0.93, 0.80])
    frame(ax, 'FLOOR %d, %s HALF - top down (cyan = new, grey = today)' % (f, half.upper()), xl, (-14, 90))
    rooms_today(ax, f); new_parts(ax, f)
    for s in P['stairs']:
        fl = 1 if s['from'].startswith('F1') else 2
        if fl == f: rect(ax, s['x0'], s['z0'], s['x1'], s['z1'], ec=DIM, fc='#2a3440', lw=0.4)
    rect(ax, *RENTAL_OLD, ec=GONE, lw=0.8, ls='--'); label(ax, 234, 46, 'car rental moved out', c=GONE, fs=4)
    label(ax, xl[0] + 3, -10, 'KERB / FORECOURT (landside)', fs=4, ha='left'); label(ax, xl[0] + 3, 87, 'APRON (airside)', fs=4, ha='left')
    fig.savefig(os.path.join(OUT, '16-Gates-2-Floor-%d-%s.png' % (f, half)), dpi=150, facecolor=BG); plt.close(fig)

# ================= 3: elevation from the apron + section through gate 13 =================
def wave(x0, x1, base=12.0, amp=4.2, per=80.0):
    xs = np.linspace(x0, x1, 400); return xs, base + 4.3 + amp * np.sin((xs - x0) / per * 2 * math.pi)
def side(ax, cx, kind, col, ground=0.0):
    L, S, fw, *_ = TYPES[kind]
    H = {'A320': (4.1, 1.5, 11.8), 'B777': (6.2, 1.9, 18.5), 'A380': (8.4, 1.7, 24.1)}[kind]
    rect(ax, cx - fw / 2, H[1], cx + fw / 2, H[1] + H[0], ec=col, lw=0.8, z=6)           # fuselage seen nose-on
    ax.add_patch(Polygon([(cx - 0.4, H[1] + H[0]), (cx, H[2]), (cx + 0.4, H[1] + H[0])], closed=True, fill=False, ec=col, lw=0.8, zorder=6))
    ax.plot([cx - S / 2, cx + S / 2], [H[1] + 0.8] * 2, color=col, lw=0.8, zorder=6)
FSM[0] = 2.2
fig = plt.figure(figsize=(24, 14), facecolor=BG)
ax = fig.add_axes([0.03, 0.55, 0.94, 0.38]); frame(ax, 'ELEVATION FROM THE APRON (looking south)  -  heights x 3', (-260, 570), (-2, 35))
ax.set_aspect(3)
rect(ax, -80, 0, 242, 11.4, ec=OLDC, lw=1); xs, ys = wave(-80, 242); ax.plot(xs, ys, color=OLDC, lw=1)
for x0, x1 in ((W_END, -80), (242, E_END)):
    rect(ax, x0, 0, x1, 11.4, ec=NEW, lw=1.4); ax.plot([x0, x1], [6, 6], color=NEW, lw=0.5, ls=':')
    xs, ys = wave(x0, x1); ax.plot(xs, ys, color=ROOFC, lw=1.4)
for s in AF['stands']:
    if s['id'] in ('STAND-%d' % i for i in range(1, 9)): side(ax, s['x'], 'A320', OLDC)
for g in WEST: side(ax, g['sj'], 'A320', NEW); side(ax, g['sw'], 'A320', NEW)
for e in E: side(ax, e['x'], e['kind'], NEW); label(ax, e['x'], -1, 'G%d %s' % (e['gate'], e['kind']), c=NEW, fs=5)
label(ax, -160, 33, 'new west end + wave roof', c=ROOFC, fs=6); label(ax, 400, 33, 'new east end + wave roof', c=ROOFC, fs=6)
label(ax, 80, 33, 'today', fs=6)
ax.text(-258, 6.2, 'Floor 2 +6.0', color=DIM, fontsize=5 * FSM[0]); ax.text(-258, 11.6, 'roof +11.4 / wave above', color=DIM, fontsize=5 * FSM[0])
# section through gate 13 (A380), looking east: landside left, apron right
ax = fig.add_axes([0.03, 0.06, 0.94, 0.42]); frame(ax, 'SECTION THROUGH GATE 13 (A380), looking east - bridges and door heights', (-15, 175), (-9, 30))
rect(ax, 0, 0, 52, 11.4, ec=NEW, lw=1.2, ls='--'); label(ax, 26, 3, 'east wing (empty for now)', c=NEW, fs=6)
rect(ax, 52, 0, 68, 5.4, ec=NEW, lw=1.2); label(ax, 60, 2.6, 'baggage hall', c=NEW, fs=5.5)
rect(ax, 52, 6, 68, 11.4, ec=NEW, lw=1.2); label(ax, 60, 8.6, 'walkway', c=NEW, fs=5.5)
rect(ax, 68, 0, 82, 5.4, ec=NEW, lw=1.2); label(ax, 75, 2.6, 'ramp services', c=NEW, fs=5)
rect(ax, 68, 6, 82, 11.4, ec=NEW, lw=1.2); label(ax, 75, 8.6, 'gate 13', c=NEW, fs=5.5)
xs = np.linspace(-6, 86, 300); ax.plot(xs, 16.5 + 3.5 * np.sin(xs / 46 * math.pi), color=ROOFC, lw=1.4)
# A380 side view (nose towards the building)
n = NOSE_WB; L = 72.7
body = [(n, 4.5), (n + 2, 2.4), (n + 8, 1.7), (n + 60, 1.7), (n + L - 2, 6.5), (n + L - 1, 9.6), (n + 55, 10.1), (n + 12, 10.1), (n + 3, 8.4), (n, 6.0)]
ax.add_patch(Polygon(body, closed=True, fill=False, ec=PLANE, lw=1, zorder=6))
ax.add_patch(Polygon([(n + 58, 10.1), (n + 66, 24.1), (n + 71, 24.1), (n + L - 1, 9.6)], closed=True, fill=False, ec=PLANE, lw=1, zorder=6))
ax.plot([n, n + L], [0, 0], color=DIM, lw=0.6)
for dy, sill, lab in ((10.3, 5.2, 'M1L main deck +5.2'), (22.5, 5.2, 'M2L +5.2'), (14.5, 8.0, 'U1L upper deck +8.0')):
    ax.plot([82, n + dy], [6.0, sill], color=NEW, lw=2.4, solid_capstyle='butt', zorder=7)
    ax.text(n + dy + 1, sill + 0.4, lab, color=NEW, fontsize=5 * FSM[0], zorder=9)
ax.text(84, 27.5, 'Bridges leave the gate room (+6.0) and slope gently:\ndown 0.8 m to the main deck, up 2.0 m to the upper deck', color=INK, fontsize=6 * FSM[0], va='top')
ax.plot([TAXI_Y - 0, TAXI_Y], [0, 0], color=DIM)
ax.text(n + 30, -1.2, 'tail end y %.0f - taxilane T at y 218:\nan A380 passing behind stays clear' % (n + L), color=OLDC, fontsize=5.5 * FSM[0], va='top')
ax.set_aspect(2.2)
fig.savefig(os.path.join(OUT, '16-Gates-3-Elevation-Section.png'), dpi=150, facecolor=BG); plt.close(fig)

# ================= 4: 3D view =================
def box(ax3, x0, z0, x1, z1, h0, h1, col, a=0.18, ec=None):
    X = [x0, x1]; Y = [z0, z1]
    v = [[(x0, z0, h0), (x1, z0, h0), (x1, z1, h0), (x0, z1, h0)], [(x0, z0, h1), (x1, z0, h1), (x1, z1, h1), (x0, z1, h1)],
         [(x0, z0, h0), (x1, z0, h0), (x1, z0, h1), (x0, z0, h1)], [(x0, z1, h0), (x1, z1, h0), (x1, z1, h1), (x0, z1, h1)],
         [(x0, z0, h0), (x0, z1, h0), (x0, z1, h1), (x0, z0, h1)], [(x1, z0, h0), (x1, z1, h0), (x1, z1, h1), (x1, z0, h1)]]
    ax3.add_collection3d(Poly3DCollection(v, facecolor=col, edgecolor=ec or col, linewidths=0.4, alpha=a))
def roof3(ax3, x0, x1, z0, z1, col):
    xs = np.linspace(x0, x1, 60); zs = np.linspace(z0, z1, 6); Xg, Zg = np.meshgrid(xs, zs)
    H = 12 + 4.3 + 4.2 * np.sin((Xg - x0) / 80.0 * 2 * math.pi) * 0.5 + 2.0 * np.sin((Zg - z0) / (z1 - z0) * math.pi)
    ax3.plot_wireframe(Xg, Zg, H, color=col, linewidth=0.35, alpha=0.8)
def plane3(ax3, cx, ny, kind, col):
    L, S, fw, wr, rc, sw, tc, ts, ta = TYPES[kind]; h = fw / 2; z = 4.0
    pts = [(cx, ny), (cx + h, ny + 3), (cx + h, ny + wr), (cx + S / 2, ny + wr + sw), (cx + S / 2, ny + wr + sw + tc), (cx + h, ny + wr + rc),
           (cx + h * 0.5, ny + ta), (cx + ts / 2, ny + ta + ts * 0.3), (cx + ts / 2, ny + ta + ts * 0.3 + 2), (cx, ny + L)]
    full = pts + [(2 * cx - x, y) for x, y in reversed(pts[1:-1])]
    ax3.plot([p[0] for p in full] + [full[0][0]], [p[1] for p in full] + [full[0][1]], [z] * (len(full) + 1), color=col, lw=0.6)
fig = plt.figure(figsize=(23.4, 13), facecolor=BG)
ax3 = fig.add_axes([-0.04, -0.10, 1.08, 1.16], projection='3d', facecolor=BG)
fig.text(0.02, 0.96, '3D VIEW FROM THE NORTH-EAST, ABOVE THE APRON  (grey = today, cyan = new, green = moved)  -  heights x 2', color=INK, fontsize=22)
box(ax3, 0, 0, 156, 52, 0, 11.4, '#8d99a6'); box(ax3, -80, -4, 0, 52, 0, 11.4, '#8d99a6')
box(ax3, -80, WALK_Z0, 242, GATE_Z1, 0, 11.4, '#8d99a6')
box(ax3, W_END, WALK_Z0, -80, GATE_Z1, 0, 11.4, NEW, 0.28); box(ax3, 242, WALK_Z0, E_END, GATE_Z1, 0, 11.4, NEW, 0.28)
box(ax3, W_END, 8, -80, WALK_Z0, 0, 11.4, NEW, 0.22); box(ax3, 156, 0, E_END, WALK_Z0, 0, 11.4, NEW, 0.12)
roof3(ax3, -80, 156, -10, 84, '#8d99a6'); roof3(ax3, W_END - 4, -80, -10, 84, ROOFC); roof3(ax3, 156, E_END + 4, -10, 84, ROOFC)
for s in AF['stands']:
    if s['id'] in ('STAND-%d' % i for i in range(1, 9)): plane3(ax3, s['x'], s['nose'], 'A320', '#8d99a6')
for g in WEST: plane3(ax3, g['sj'], 120, 'A320', NEW); plane3(ax3, g['sw'], 122, 'A320', NEW)
for e in E: plane3(ax3, e['x'], NOSE_WB, e['kind'], NEW)
box(ax3, TOWER_NEW[0] - 5, TOWER_NEW[1] - 5, TOWER_NEW[0] + 5, TOWER_NEW[1] + 5, 0, 22, MOVE, 0.35)
box(ax3, RENTAL_NEW[0], RENTAL_NEW[1], RENTAL_NEW[2], RENTAL_NEW[3], 0, 9, MOVE, 0.25)
ax3.set_xlim(-360, 580); ax3.set_ylim(-260, 200); ax3.set_zlim(0, 60)
ax3.set_box_aspect((940, 460, 110), zoom=1.12); ax3.view_init(elev=28, azim=-58)
ax3.axis('off')
fig.savefig(os.path.join(OUT, '16-Gates-4-3D-View.png'), dpi=150, facecolor=BG); plt.close(fig)
print('done', OUT, 'east end %.1f' % E_END, [(e['gate'], e['kind'], round(e['x'], 1)) for e in E], [(round(g['sj'], 1), round(g['sw'], 1)) for g in WEST])
