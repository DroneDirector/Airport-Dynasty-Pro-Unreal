# To-do 213/214/212, the Monday REDRAW (v2): shared numbers, the verified existing building (Data/holomap-walls.json, the
# game's own traced walls - not the out-of-date rooms of terminal-plan.json) and the whole AFTER geometry, one place for
# every sheet. Plan metres: x along the building, z = 0 the landside face, z grows airside. Sheets: 3840 x 2160, black.
import json, os, sys
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch, Polygon, Circle
from matplotlib.collections import LineCollection
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Terminal-Changes', 'v2'); os.makedirs(OUT, exist_ok=True)
WALLS = json.load(open(os.path.join(ROOT, 'Data', 'holomap-walls.json')))['floors']
PLAN = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
ROOF = json.load(open(os.path.join(HERE, 'roof_profile.json')))
BG, FG, DIM = '#05080c', '#f0f5fa', '#9fb0bf'
NEW, NS, OK, BELT, TUN, WARN, GLASS, STAFF, DF, TRF = ('#ffd166', '#ff6b9a', '#3ddc6a', '#3fd0ff', '#c58cff', '#ff8a5b', '#9fe8ff',
                                                       '#a99cff', '#ff9ff3', '#5ad1c8')
SCH, ARRS, ARRN = '#4da3ff', '#8de04a', '#ff6b9a'
W, H, DPI = 32, 18, 120


def sheet(no, title, sub='', foot='approved by the owner 2026-11-23, being built'):
    fig = plt.figure(figsize=(W, H), dpi=DPI); fig.patch.set_facecolor(BG)
    fig.text(0.012, 0.978, 'TERMINAL CHANGES v2.1 approved  ·  ' + no + '  ·  ' + title, color=FG, fontsize=26, weight='bold', va='top')
    if sub: fig.text(0.012, 0.942, sub, color=DIM, fontsize=16, va='top')
    fig.text(0.988, 0.008, 'Airport Dynasty Pro · to-dos 212-214, 216 · %s · metres' % foot,
             color=DIM, fontsize=12, ha='right')
    return fig


def check(fig, name):
    """every text against every other text and the sheet's edge: prints each overlap"""
    fig.canvas.draw(); r = fig.canvas.get_renderer(); T = []
    for t in fig.findobj(matplotlib.text.Text):
        if not t.get_visible() or not t.get_text().strip(): continue
        a = t.axes
        if a is not None and not a.axison and t in (a.title, a._left_title, a._right_title): continue
        if a is not None and t.get_clip_on():
            x, y = t.get_transform().transform(t.get_position())
            if not a.bbox.contains(x, y): continue
        T.append((t, t.get_window_extent(r)))
    n = 0
    for i in range(len(T)):
        b = T[i][1]
        if b.x0 < 0 or b.y0 < 0 or b.x1 > W * DPI or b.y1 > H * DPI:
            print('  OFF SHEET', name, repr(T[i][0].get_text()[:40])); n += 1
        for j in range(i + 1, len(T)):
            c = T[j][1]
            ox = min(b.x1, c.x1) - max(b.x0, c.x0); oy = min(b.y1, c.y1) - max(b.y0, c.y0)
            if ox > 2 and oy > 2:
                print('  OVERLAP', name, repr(T[i][0].get_text()[:34]), '/', repr(T[j][0].get_text()[:34])); n += 1
    print(name, 'texts', len(T), 'overlaps', n, 'smallest fonts', sorted({round(t.get_fontsize(), 1) for t, _ in T})[:3])


def save(fig, name):
    check(fig, name); p = os.path.join(OUT, name); fig.savefig(p, facecolor=BG, dpi=DPI); plt.close(fig); return p


def ax_plan(fig, rect, x0, x1, z0, z1):
    ax = fig.add_axes(rect); ax.set_facecolor(BG); ax.set_xlim(x0, x1); ax.set_ylim(z0, z1); ax.set_aspect('equal'); ax.axis('off')
    return ax


def walls(ax, floor, col='#aab8c4', lw=1.1, skip=None):
    """the building as it stands (the game's traced walls); glass light blue"""
    s = [w for w in WALLS[str(floor)] if not (skip and skip(w))]
    if getattr(ax, 'v21', False) and floor == 1:                  # the old lanes' arches and the old stairs' rails: replaced
        old = lambda w: (52 < w[0] < 88 and 34.9 < w[1] < 36.5) or (197.1 < w[0] < 200.4 and 77 < w[1] < 81) or (283.5 < w[0] < 286.8 and 77 < w[1] < 81)
        s = [w for w in s if not old(w)]
    for g, c in ((0, col), (1, '#4aa8c8')):
        ax.add_collection(LineCollection([((w[0], w[1]), (w[2], w[3])) for w in s if w[4] == g], colors=c, linewidths=lw, zorder=4))


def box(ax, x0, z0, x1, z1, t='', fc='#141c24', ec='#5a6a7a', col=None, fs=12, lw=1.4, ls='-', rot=0, z=2, weight='normal', alpha=1.0):
    ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc=fc, ec=ec, lw=lw, ls=ls, zorder=z, alpha=alpha))
    if t:
        ax.text((x0 + x1) / 2, (z0 + z1) / 2, t, color=col or ec, ha='center', va='center', fontsize=fs, rotation=rot, weight=weight,
                zorder=z + 20, clip_on=True, linespacing=1.15)


def txt(ax, x, z, s, col=FG, fs=13, **k):
    k.setdefault('ha', 'center'); k.setdefault('va', 'center')
    ax.text(x, z, s, color=col, fontsize=fs, zorder=35, clip_on=True, linespacing=1.15, **k)


def num(ax, x, z, n, col=NEW, fs=13):
    ax.text(x, z, str(n), color='#05080c', fontsize=fs, weight='bold', ha='center', va='center', zorder=40, clip_on=True,
            bbox=dict(boxstyle='circle,pad=0.22', fc=col, ec='none'))


def dim(ax, a, b, at, t=None, vertical=False, col=NEW, fs=12):
    t = t if t is not None else ('%.1f' % abs(b - a))
    if vertical:
        ax.annotate('', xy=(at, a), xytext=(at, b), arrowprops=dict(arrowstyle='<->', color=col, lw=1.3), zorder=30)
        ax.text(at, (a + b) / 2, t, color=col, fontsize=fs, rotation=90, ha='center', va='center', bbox=dict(fc=BG, ec='none', pad=1),
                zorder=31, clip_on=True)
    else:
        ax.annotate('', xy=(a, at), xytext=(b, at), arrowprops=dict(arrowstyle='<->', color=col, lw=1.3), zorder=30)
        ax.text((a + b) / 2, at, t, color=col, fontsize=fs, ha='center', va='center', bbox=dict(fc=BG, ec='none', pad=1), zorder=31, clip_on=True)


def route(ax, pts, col, lw=4.0, ls='-', ms=26, z=25):
    xs, ys = zip(*pts)
    ax.plot(xs, ys, color=col, lw=lw, ls=ls, zorder=z, solid_capstyle='round', solid_joinstyle='round')
    (xa, ya), (xb, yb) = pts[-2], pts[-1]
    d = max(1e-6, ((xb - xa) ** 2 + (yb - ya) ** 2) ** 0.5)
    ax.annotate('', xy=(xb + (xb - xa) / d * 1.5, yb + (yb - ya) / d * 1.5), xytext=(xb - (xb - xa) / d * 0.1, yb - (yb - ya) / d * 0.1),
                arrowprops=dict(arrowstyle='-|>', color=col, lw=lw, mutation_scale=ms), zorder=z)


def legend(fig, x, y, items, cols=1, cw=0.24, lh=0.0245, fs=14):
    """a numbered list: (n, text[, colour])"""
    per = -(-len(items) // cols)
    for i, it in enumerate(items):
        cx, cy = x + (i // per) * cw, y - (i % per) * lh
        c = it[2] if len(it) > 2 else NEW
        fig.text(cx + 0.006, cy, str(it[0]), color='#05080c', fontsize=fs - 2, weight='bold', ha='center', va='center',
                 bbox=dict(boxstyle='circle,pad=0.2', fc=c, ec='none'))
        fig.text(cx + 0.017, cy, it[1], color=FG, fontsize=fs, va='center')


# ------------------------------------------------------------------------------------------------ the design hour (as TC-1)
A320, A380, B777, LF = 180, 520, 368, 0.85
DEP_NB = 12 * 0.67 * A320 * LF
DEP_WB = 4 * 0.33 * (A380 + 3 * B777) / 4.0 * LF
PH = DEP_NB + DEP_WB
SURGE = (A380 + B777) * LF

# ------------------------------------------------------------------------------------------------ the AFTER geometry
GATE_X0 = {13: 255.4, 14: 335.2, 15: 407.5, 16: 479.8}          # the wide-body gates' west walls (existing)
STAIRS = [246 + 30 * k for k in range(11)]                      # 11 escape stairs, 30 m apart (VKF 16-15: 35 m)
WALLX = 240.0                                                   # the glass passport wall across the walkway
BELTS_E = (261.0, 290.0)
# ground floor: (x0, z0, x1, z1)
G = {'link': (107, -4, 156, 0), 'east_hall': (156, 0, 228, 40), 'promenade': (228, 0, 470, 6), 'customs': (228, 6, 236, 40),
     'customs_office': (228, 40, 236, 52), 'claim': (236, 6, 312, 52), 'pc_line': (312, 20, 324, 52), 'pc_office': (312, 6, 324, 20),
     'pc_hall': (324, 6, 372, 52), 'transfer_up': (326, 40, 338, 51), 'second_line': (358, 40, 371, 51),
     'df_east': (279, 8, 303, 19), 'wc_claim': (263, 8, 274, 19), 'lf_east': (249.5, 8, 260, 16), 'os_east': (300, 42, 311, 51),
     'corridor': (294, 63.4, 526, 67.9), 'cross': (340, 52, 346, 63.4), 'inputs': (286, 52.6, 314, 55.4),
     'staff_lane': (156, 40, 160, 52), 'wc_hall': (160, 40, 178, 52), 'family': (178, 40, 190, 52), 'trolley': (190, 40, 210, 52),
     'bagserv': (210, 40, 228, 52), 'crew': (372, 6, 404, 26), 'canteen': (404, 6, 436, 26), 'ground_off': (436, 6, 470, 26),
     'lockers': (372, 26, 404, 52), 'staffsec': (404, 26, 436, 52), 'police': (436, 26, 470, 52), 'coach': (470, 0, 551.2, 52)}
# floor 2
F = {'queue': (231, 24, 248, 52), 'line': (248, 24, 256, 52), 'dutyfree': (258, 24, 290, 52), 'wc1': (292, 40, 305, 52),
     'shop1': (292, 24, 305, 38), 'transfer': (307, 24, 340, 52), 'food': (342, 24, 398, 52), 'prayer': (400, 24, 424, 37),
     'wc2': (400, 39, 424, 52), 'lounge': (426, 24, 470, 52), 'seating': (472, 24, 520, 52), 'shops2': (522, 24, 551.2, 38),
     'wc3': (522, 40, 551.2, 52), 'strip': (228, 20, 307, 24), 'exit_lane': (228, 24, 231, 52), 'gallery': (307, 20, 551.2, 24),
     'lobby': (237.05, 68, 255.4, 82), 'slab': (228, 20, 551.2, 52)}
AISLE4 = (194.5, 82.2, 303, 84.8)                               # gate 4's closed arrivals gallery, glass, outside the windows
VEST4 = (188, 78, 196.8, 82)                                    # gate 4's jetway vestibule (its jetway: x 191.05)
JETWAYS = {4: (191.05,), 13: (309.4, 314.4, 321.4), 14: (389.2, 399.2), 15: (461.5, 471.5), 16: (533.8, 543.8)}   # Data/babylon-jetways.json
OPEN_F2 = ((231.5, 239.3), (262, 286), (344, 396), (474, 518))  # the walkway's south wall opened (z 52)
ENTR_E = ((186, 198), (290, 300), (396, 406), (504, 516))       # new entrance vestibules, 4 m deep
WEST = {'df': (-58, 43, -20, 51.5), 'lf': (-78.5, 17, -70, 24), 'wc': (-78.5, 25, -70, 33), 'os': (-6, 42, -0.8, 51),
        'trol1': (-68, 17, -60, 18.6), 'trol2': (-30, 17, -22, 18.6), 'trol3': (-16, 17, -8, 18.6),
        'cafe': (-78.5, -3.3, -72, 1.2), 'info': (-60, -3, -56, -0.5), 'kiosk': (-26, -3.3, -20, -0.3), 'atm': (-12, -3.3, -6, -0.3),
        'rail': (-57, 4.2, -32, 4.8)}
HALL = {'k1': (57.5, 3, 65, 4), 'k2': (84.5, 3, 92, 4), 'drop': (101, 15.6, 113, 19.1), 'serv': (108, 23, 113.5, 28.5),
        'eg1': (59, 28.3, 65, 29.4), 'eg2': (75, 28.3, 81, 29.4), 'tr1': (43, 0.8, 48, 2), 'tr2': (108.5, 0.8, 113.5, 2)}
EH = {'checkin': (160, 24, 190, 28), 'os': (191, 24, 195, 28), 'cafe': (158, 2, 171, 11), 'shops': (173, 2, 184, 7),
      'sim': (200, 2, 207, 6), 'atm': (208, 2, 215, 6), 'rental': (194, 34, 218, 38.5), 'info': (158, 32, 168, 38),
      'firstaid': (170, 32, 180, 38), 'rail': (220.5, 21, 221.5, 39.5)}


def area(b): return abs((b[2] - b[0]) * (b[3] - b[1]))


def core_f2(g):      # the arrivals core in a wide-body gate: the jetway vestibule and the way down
    a = GATE_X0[g]; return (a + 49, 77, a + 67.5, 82), (a + 34.2, 74.5, a + 49, 82)


def core_f1(g):      # the arrivals lobby on the ground and its door into the corridor
    a = GATE_X0[g]; return (a + 34.5, 68.1, a + 52, 81.9), (a + 40, 67.6, a + 45, 68.4)


AREA_CORE = 18.5 * 5 + 14.8 * 7.5
# v2.1 (owner's decisions, 2026-11-23) - new numbers only here:
# the stairs of gates 4 and 13 turned along the walkway wall (2.8 x 11 m as today, foot WEST, top EAST): (x0, z0, x1, z1)
STAIR_T = {4: (198.4, 68.3, 209.4, 71.1), 13: (284.7, 68.3, 295.7, 71.1)}
# the openings in the inner walls gate room / view bay: gate -> (x, z0, z1); 14-16 as v2
PARTY = {4: (203.05, 71.4, 79.2), 13: (289.4, 71.3, 74.4), 14: (369.2, 69.5, 73.5), 15: (441.5, 69.5, 73.5), 16: (513.8, 69.5, 73.5)}
DOOR_G13_STAIR = (284.5, 286.3, 82.0)                           # ground floor: the gate 13 stair hall's own door onto the apron
JW_DOOR_W = 1.9                                                 # the jetway doors cut into the view bays' glass, at JETWAYS' x, z 82
DEAD_DOORS = {13: 277.4, 14: 357.2, 15: 429.5, 16: 501.8}       # the doors without a jetway: closed with glass
# security, 7 lanes in the same room (x 50-90, z 30-42): the lanes' belt middles (today PXLANES 53, 59.2, 65.3, 71.5, 77.7, 83.8)
LANE_BELT_X = [round(52.5 + 5.78333 * k, 2) for k in range(7)]
LANE_Z = (32.0, 40.0)


def fills(ax, floor):
    """tints of today's rooms, from the real building (annex scripts + traced walls)"""
    def b(x0, z0, x1, z1, fc): box(ax, x0, z0, x1, z1, '', fc=fc, ec='none', z=1)
    if floor == 1:
        b(-80, -4, 0, 52, '#16212b'); b(0, 0, 42, 30, '#0f2018'); b(42, 0, 114, 30, '#1b2733'); b(50, 30, 90, 42, '#182a1d')
        b(50, 42, 90, 52, '#0f2018'); b(90, 30, 114, 52, '#0f2018'); b(0, 30, 50, 52, '#131a20'); b(114, 0, 156, 52, '#241e0f')
        b(-241.1, 52, 551.2, 68, '#1a170e'); b(-241.1, 8, -80, 52, '#0e1318')
        for v in ((44, 56), (67, 83), (95, 107)): b(v[0], -4, v[1], 0, '#1b2733')
    else:
        b(-241.1, 52, 551.2, 68, '#141c24'); b(-241.1, 8, -119.15, 52, '#1f1830'); b(-119.15, 8, -80, 52, '#16212b')
        b(0, 0, 21, 30, '#0f2018'); b(6, 30, 72, 52, '#1d1a10'); b(72, 30, 114, 52, '#24180f'); b(114, 0, 156, 52, '#1a1a28')
        b(-80, 34.5, -68.2, 52, '#16212b')


def gates(ax, floor, fs=12, nums=True, x0=-250, x1=560):
    for r in PLAN['rooms']:
        rid = r['id']
        if r['f'] != floor or r['x1'] < x0 or r['x0'] > x1: continue
        if not any(k in rid for k in (('GATE', 'RAMP') if floor == 1 else ('GATE', 'VIEW'))): continue
        grn = 'GATE' in rid or 'VIEW' in rid or rid.endswith('B')
        box(ax, r['x0'], r['z0'], r['x1'], r['z1'], '', fc='#0e2216' if grn else '#1a170e', ec='none', z=1)
        if nums and 'GATE' in rid:
            txt(ax, (r['x0'] + r['x1']) / 2, 75, 'G' + rid.split('-')[-1], col=OK, fs=fs, weight='bold')


def train_route(ax, lw=2.5):
    ax.plot(*zip(*[(134.4, 44), (134.4, 53.5), (137.9, 57), (265, 57), (269, 61), (269, 92)]), color='#ff5050', lw=lw, ls=(0, (6, 2)), zorder=12)


# ------------------------------------------------------------------------------------------------ the AFTER drawn (no labels)
def new(ax, b, **k):
    k.setdefault('fc', '#2a2410'); k.setdefault('ec', NEW); box(ax, *b, **k)


def ns(ax, b, **k):
    k.setdefault('fc', '#2a0f1c'); k.setdefault('ec', NS); box(ax, *b, **k)


def sec_lanes(ax, n, lw=1.0, fc='#3a3410', ec=NEW):
    """the security lanes: the belt 0.8 x 8 m, the x-ray, the arch (as the game's model LANE-k, 3.0 m wide + its officer's desk)"""
    X = LANE_BELT_X if n == 7 else [53.0, 59.2, 65.3, 71.5, 77.7, 83.8]
    for x in X:
        box(ax, x - 0.4, 32, x + 0.4, 40, '', fc=fc, ec=ec, lw=lw, z=6); box(ax, x - 0.7, 34.6, x + 0.7, 36.4, '', fc='#2b3a48', ec=ec, lw=lw, z=7)
        box(ax, x + 1.1, 35.3, x + 2.3, 35.9, '', fc='#2b3a48', ec=ec, lw=lw, z=7); box(ax, x - 1.5, 34.7, x - 0.95, 35.3, '', fc='#55606c', ec=ec, lw=lw * 0.6, z=7)


def after_f1(ax, lw=1.6):
    """the new ground floor, unlabelled"""
    L = dict(lw=lw); ax.v21 = True
    for b in WEST.values(): new(ax, b, **L)                                                         # the west claim wing
    for cx in (-60.8, -48.15, -35.5, -22.85, -10.2): box(ax, cx - 2.45, 21, cx + 2.45, 41, '', fc='#233140', ec=BELT, lw=1)
    box(ax, -44, 8.2, -36, 15.8, '', fc='#10301a', ec=OK, lw=1); box(ax, -54, 8.2, -48, 15.8, '', fc='#301010', ec='#ff5050', lw=1)
    for xs in (47.5, 74.5): box(ax, xs, 15.6, xs + 25, 19.1, '', fc='#2b3a48', ec='#c9d6e2', lw=0.8)  # the Grand Hall
    for b in HALL.values(): new(ax, b, **L)
    new(ax, G['link'], **L)
    ax.plot([114, 156], [-4, -4], color=GLASS, lw=4, zorder=6); ax.plot([114, 156], [0, 0], color=GLASS, lw=1.5, ls=':', zorder=6)
    new(ax, G['east_hall'], fc='#23200d', **L)                                                      # the east
    for k in ('wc_hall', 'family', 'trolley', 'bagserv', 'staff_lane'): new(ax, G[k], fc='#1c1f24', **L)
    new(ax, G['promenade'], **L)
    ns(ax, G['customs'], fc='#1a2430', **L); ns(ax, G['customs_office'], fc='#1a2430', **L)
    box(ax, 228.6, 24, 235.4, 32, '', fc='#10301a', ec=OK, lw=1, z=3); box(ax, 228.6, 33.5, 235.4, 39, '', fc='#301010', ec='#ff5050', lw=1, z=3)
    ns(ax, G['claim'], **L)
    for cx in BELTS_E: box(ax, cx - 4, 22, cx + 4, 50, '', fc='#233140', ec=BELT, lw=1.4, z=3)
    for k in ('df_east', 'wc_claim', 'lf_east', 'os_east'): new(ax, G[k], z=3, **L)
    ns(ax, G['pc_line'], fc='#3a1428', **L); ns(ax, G['pc_office'], fc='#1a2430', **L); ns(ax, G['pc_hall'], **L)
    for k in range(6): box(ax, 313.5, 20.8 + k * 2.9, 322.5, 22.9 + k * 2.9, '', fc='#55203a', ec=NS, lw=1, z=4)
    for k in range(8): box(ax, 314.5, 39.1 + k * 1.6, 321.5, 39.8 + k * 1.6, '', fc=NS, ec=NS, lw=0.6, z=4)
    for k in range(16):
        box(ax, 341.4 + (k % 8) * 1.9, 31 + (k // 8) * 3.5, 342.6 + (k % 8) * 1.9, 32 + (k // 8) * 3.5, '', fc=NEW, ec=NEW, lw=0.5, z=4)
    box(ax, *G['transfer_up'], '', fc='#123230', ec=TRF, lw=lw, z=4); box(ax, *G['second_line'], '', fc='#1a2430', ec=NS, lw=lw, z=4)
    for k in ('crew', 'canteen', 'ground_off', 'lockers', 'staffsec', 'police'): box(ax, *G[k], '', fc='#1c1c2e', ec=STAFF, lw=lw)
    new(ax, G['coach'], fc='#23200d', **L)
    for x in STAIRS: box(ax, x - 1.3, 7, x + 1.3, 20, '', fc='#0f3a1c', ec=OK, lw=1.2, z=6)
    sec_lanes(ax, 7)
    for x0, z0, x1, z1 in STAIR_T.values(): box(ax, x0, z0, x1, z1, '', fc='#3a3410', ec=NEW, lw=1.2, z=8)
    ax.plot([DOOR_G13_STAIR[0], DOOR_G13_STAIR[1]], [82, 82], color=OK, lw=5, zorder=14)
    ns(ax, G['corridor'], fc='#45152c', z=6, **L); ns(ax, G['cross'], fc='#45152c', z=6, **L)         # arrivals kept apart
    for g in GATE_X0:
        a, d = core_f1(g); ns(ax, a, fc='#45152c', z=6, **L); box(ax, *d, '', fc=NS, ec=NS, z=7)
    new(ax, G['inputs'], z=6, fc='#0f2c38', ec=BELT, **L)
    for cx, xi in zip(BELTS_E, (292, 306)):
        ax.plot([xi, xi, cx, cx], [52.6, 51.2, 51.2, 47], color=TUN, lw=2.2, ls=(0, (4, 2)), zorder=8)
    ax.plot([48, 116, 116], [18.6, 18.6, 24], color=TUN, lw=2.2, ls=(0, (4, 2)), zorder=8)
    ax.plot([195, 151], [23.5, 23.5], color=TUN, lw=2.2, ls=(0, (4, 2)), zorder=8)
    for b in EH.values(): new(ax, b, z=4, **L)
    ax.plot([156, 551.2], [0, 0], color=GLASS, lw=4, zorder=6)
    for d0, d1 in ENTR_E: box(ax, d0, -4, d1, 0, '', fc='#1b3a4a', ec=GLASS, lw=1.2, z=6)


def gate_stairs(ax, turned=False):
    """the gates' own stairs down (the game's layout: 2.8 x 11 m at the gate room's x0 + 28.3); turned: gates 4 and 13 as built"""
    if turned:
        for x0, z0, x1, z1 in STAIR_T.values():
            box(ax, x0, z0, x1, z1, '', fc='#3a3410', ec=NEW, lw=1.4, z=8)
            for k in range(1, 9): ax.plot([x0 + k * 1.222] * 2, [z0, z1], color=NEW, lw=0.6, zorder=9)
    for a in (-72.6, 7.95, 88.5, 169.05) + tuple(GATE_X0.values()):
        if turned and a in (169.05, 255.4): continue
        box(ax, a + 28.3, 69.5, a + 31.1, 80.5, '', fc='#26303a', ec='#8fa0ae', lw=1, z=8)
        for k in range(1, 8): ax.plot([a + 28.3, a + 31.1], [69.5 + k * 1.375] * 2, color='#8fa0ae', lw=0.6, zorder=9)


def after_f2(ax, lw=1.6):
    """the new floor 2, unlabelled"""
    box(ax, *F['slab'], '', fc='#15130a', ec=NEW, lw=lw + 0.6)
    box(ax, WALLX, 52.2, 551.2, 67.8, '', fc='#2a0f1c', ec='none', z=1.5)
    for g in GATE_X0: a = GATE_X0[g]; box(ax, a, 68, a + 68, 82, '', fc='#24101a', ec='none', z=1.5)
    box(ax, *F['queue'], '', fc='#14283d', ec=SCH, lw=lw); ns(ax, F['line'], fc='#3a1428', lw=lw)
    for k in range(4): box(ax, 249, 25 + k * 2.9, 255, 27.2 + k * 2.9, '', fc='#55203a', ec=NS, lw=1, z=4)
    for k in range(8): box(ax, 249.6, 37.6 + k * 1.6, 254.4, 38.3 + k * 1.6, '', fc=NS, ec=NS, lw=0.6, z=4)
    for k in range(8): box(ax, 234 + (k % 4) * 2.4, 27 + (k // 4) * 3.5, 235.2 + (k % 4) * 2.4, 28 + (k // 4) * 3.5, '', fc=NEW, ec=NEW, lw=0.5, z=4)
    new(ax, F['dutyfree'], fc='#33163a', ec=DF, lw=lw)
    for k in ('shop1', 'shops2'): new(ax, F[k], lw=lw)
    for k in ('wc1', 'wc2', 'wc3'): new(ax, F[k], fc='#10222e', ec=BELT, lw=lw)
    box(ax, *F['transfer'], '', fc='#123230', ec=TRF, lw=lw)
    for k in ('food', 'lounge', 'seating', 'prayer'): new(ax, F[k], lw=lw)
    box(ax, *F['strip'], '', fc='#123230', ec=TRF, lw=lw, z=3); box(ax, *F['exit_lane'], '', fc='#123230', ec=TRF, lw=lw, z=3)
    box(ax, *F['gallery'], '', fc='#20200f', ec=NEW, lw=lw, z=3)
    for x in STAIRS: box(ax, x - 1.3, 14, x + 1.3, 20, '', fc='#0f3a1c', ec=OK, lw=1.2, z=6)
    ax.plot([WALLX, WALLX], [52, 68], color=GLASS, lw=6, zorder=12)
    new(ax, F['lobby'], fc='#3a3410', lw=lw + 0.6, z=5)
    for g in GATE_X0:
        v, c = core_f2(g); ns(ax, v, fc='#55203a', z=7, lw=lw); ns(ax, c, fc='#45152c', z=7, lw=lw)
    ns(ax, AISLE4, fc='#45152c', z=8, lw=lw, ls='--'); ns(ax, VEST4, fc='#55203a', z=8, lw=lw)
    for x, z0, z1 in PARTY.values(): ax.plot([x, x], [z0, z1], color='#1a0c14', lw=6, zorder=9); ax.plot([x, x], [z0, z1], color=OK, lw=2.5, ls=':', zorder=10)
    gate_stairs(ax, True)
    for g in (13, 14, 15, 16):                                    # the jetway doors in the view bays' glass; the dead doors glazed
        for x in JETWAYS[g]: ax.plot([x - JW_DOOR_W / 2, x + JW_DOOR_W / 2], [82, 82], color=OK, lw=6, zorder=14)
        ax.plot([DEAD_DOORS[g] - 1, DEAD_DOORS[g] + 1], [82, 82], color=GLASS, lw=3, zorder=14)
    for a, b in OPEN_F2 + ((228.3, 230.7),):
        ax.plot([a, b], [52, 52], color='#05080c', lw=7, zorder=9); ax.plot([a, b], [52, 52], color=OK, lw=3, ls=':', zorder=10)
