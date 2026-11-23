# Sheets 9a-9d: the escape ways today and as proposed (check and drawing only, nothing built): distances by escape_lib.py
# (the real traced walls; VKF 16-15 of 18.09.2014, as of 01.08.2021).
from v2_common import *
import numpy as np
from matplotlib.colors import ListedColormap, BoundaryNorm
import escape_lib as E
_sheet = sheet


def sheet(*a, **k):                      # (sheet 9 is a proposal: not approved yet)
    f = _sheet(*a, **k); f.texts[0].set_text(f.texts[0].get_text().replace('v2.1 approved', 'v2.1')); return f


which = sys.argv[1:] or list('abcd')
GRN, ORG, RED, NW = '#1f9d4d', '#ff9f1c', '#ff2e2e', '#7dffb0'
CM = ListedColormap([GRN, ORG, RED]); NM = BoundaryNorm([0, 35, 50, 999], 3)
FOOT = 'check and drawing only, nothing built'
ARROW = '→'


def heat(ax, F_, D, alpha=0.55):
    Z = np.ma.masked_invalid(D.T)
    ax.imshow(Z, origin='lower', extent=(F_.x0, F_.x0 + F_.nx * E.S, F_.z0, F_.z0 + F_.nz * E.S), cmap=CM, norm=NM, alpha=alpha, zorder=1.8, interpolation='nearest', aspect='equal')


def lims(ax, x0, x1, z0, z1): ax.set_xlim(x0, x1); ax.set_ylim(z0, z1)


def key(fig, x, y, fs=15):
    for i, (c, t) in enumerate(((GRN, 'up to 35 m to a stair or a way outside: allowed'), (ORG, '35-50 m: only with two ways out apart and the fire authority\'s agreement'), (RED, 'over 50 m, or no way out: not allowed'))):
        fig.patches.append(Rectangle((x, y - i * 0.026), 0.014, 0.016, transform=fig.transFigure, fc=c, ec='none', alpha=0.8))
        fig.text(x + 0.019, y + 0.008 - i * 0.026, t, color=FG, fontsize=fs, va='center')


def mark(ax, x, z, t, col=RED, fs=13, dx=0, dz=0):
    ax.plot([x], [z], marker='x', color='#ffffff', ms=9, mew=2.5, zorder=45)
    ax.text(x + dx, z + dz, t, color='#ffffff', fontsize=fs, weight='bold', ha='center', va='center', zorder=46, clip_on=True, bbox=dict(fc='#05080c', ec=col, lw=1.6, pad=2.5))


def estair(ax, n, x0, z0, x1, z1, lx=None, lz=None, fs=13):
    box(ax, x0, z0, x1, z1, '', fc='#0c4420', ec=NW, lw=2, z=20)
    k = max(3, int(max(x1 - x0, z1 - z0) / 1.2))
    for i in range(1, k):
        if x1 - x0 > z1 - z0: ax.plot([x0 + (x1 - x0) * i / k] * 2, [z0, z1], color=NW, lw=0.7, zorder=21)
        else: ax.plot([x0, x1], [z0 + (z1 - z0) * i / k] * 2, color=NW, lw=0.7, zorder=21)
    num(ax, lx if lx is not None else (x0 + x1) / 2, lz if lz is not None else (z0 + z1) / 2, n, NW, fs=fs)


def door(ax, x, z, w=1.8, along='x', lw=6):
    if along == 'x': ax.plot([x - w / 2, x + w / 2], [z, z], color=NW, lw=lw, zorder=24)
    else: ax.plot([x, x], [z - w / 2, z + w / 2], color=NW, lw=lw, zorder=24)


def gst(ax, heads=False):
    for g, x in E.GX.items():
        box(ax, x + 28.3, 69.5, x + 31.1, 80.5, '', fc='#26303a', ec='#c9d6e2', lw=1.3, z=20)
        for k in range(1, 8): ax.plot([x + 28.3, x + 31.1], [69.5 + k * 1.375] * 2, color='#c9d6e2', lw=0.6, zorder=21)
        if heads: ax.plot([x + 28.5, x + 30.9], [68, 68], color=NW, lw=6, zorder=22)
    box(ax, 140, 43, 143, 51, '', fc='#26303a', ec='#c9d6e2', lw=1.3, z=20)


def col(w): return RED if w > 50.04 else ORG if w > 35.04 else '#3ddc6a'


def wrap(t, n):
    ln, cur = [], ''
    for s in t.split():
        if len(cur) + len(s) > n: ln.append(cur); cur = s
        else: cur = (cur + ' ' + s).strip()
    return '\n'.join(ln + [cur])


LIST2 = [('landing (escalator top)', (0, 10, 16, 30)), ('its toilets', (0, 0, 14, 10)), ('link to the walkway', (6, 30, 14, 52)), ('Food Walk', (14, 30, 56, 44)), ('eateries', (14, 44, 56, 52)), ('landing and bar', (56, 30, 114, 52)),
         ('offices (staff)', (114, 0, 156, 52)), ('gate walkway', (-241, 52, 551, 68)), ('tax free', (-241, 12, -119.5, 52)), ('escalator hall', (-118.5, 8.5, -80.5, 52)), ('gate rooms (10)', None), ('view bays (10)', None)]


def table(F_, D):
    out = []
    for nm, b in LIST2:
        if b: w = F_.worst(D, b); s = F_.share(D, b)
        else:
            bs = [(x, 68.4, x + 33.7, 82) if 'gate' in nm else (x + 34.3, 68.4, x + 67.7, 82) for x in E.GX.values()]
            w = max((F_.worst(D, q) for q in bs), key=lambda q: q[0]); s = float(np.mean([F_.share(D, q) for q in bs]))
        out.append((nm, w[0], s, w[1], w[2]))
    return out


def names(ax, i):
    if i == 0:
        for x, z, t, fs in ((-180, 30, 'TAX FREE', 16), (-99, 26, 'ESCALATOR\nHALL', 13), (27, 38, 'FOOD WALK', 13), (88, 40, 'LANDING, BAR', 13), (8, 20, 'LANDING', 10), (128, 20, 'OFFICES', 12), (-35, 60, 'GATE WALKWAY', 13)): txt(ax, x, z, t, FG, fs, weight='bold')
        txt(ax, -35, 25, 'claim hall below (no Floor 2)', DIM, 13); txt(ax, 28, 12, 'escalator room\n(open below)', DIM, 12); txt(ax, 78, 14, 'Grand Hall (open below)', DIM, 12)
    else: txt(ax, 350, 60, 'GATE WALKWAY', FG, 13, weight='bold'); txt(ax, 350, 30, 'no Floor 2 today (the new concourse: sheet 9c)', DIM, 15)


if set('ab') & set(which):
    F0 = E.floor2_today(); D0 = F0.solve(E.EXITS2); T0 = table(F0, D0)
    F1, ex1 = E.floor2_after(); D1 = F1.solve(ex1); T1 = table(F1, D1)
    for a, b in zip(T0, T1): print('%-26s %.1f -> %.1f  (over 35 m: %.0f%% -> %.0f%%) at %.0f,%.0f' % (a[0], a[1], b[1], a[2], b[2], a[3], a[4]))
    al = (np.isfinite(D0)).sum(); print('all Floor 2: over 35 m %.0f%%, over 50 m %.0f%% -> %.1f%% / %.1f%%' % (100 * (D0[np.isfinite(D0)] > 35).mean(), 100 * (D0[np.isfinite(D0)] > 50).mean(), 100 * (D1[np.isfinite(D1)] > 35).mean(), 100 * (D1[np.isfinite(D1)] > 50).mean()))

if 'a' in which:
    fig = sheet('SHEET 9a', 'ESCAPE WAYS, FLOOR 2 TODAY: HOW FAR TO A STAIR', 'Every point\'s way to the nearest stair, round the real walls. Escalators and lifts do not count. Stairs today: 10 at the gates, 1 staff stair (grey).', foot=FOOT)
    for i, (x0, x1) in enumerate(((-246, 158), (152, 556))):
        ax = ax_plan(fig, [0.008, 0.625 - i * 0.285, 0.984, 0.285], x0, x1, -8, 90); heat(ax, F0, D0); walls(ax, 2, lw=1.4); gst(ax); lims(ax, x0, x1, -8, 90)
        for g, x in E.GX.items():
            if x0 < x < x1: txt(ax, x + 34, 85.5, 'GATE %d' % g, FG, 13, weight='bold')
        for nm, w, s, wx, wz in T0:
            if x0 < wx < x1 and nm not in ('gate rooms (10)', 'gate walkway', 'link to the walkway'): mark(ax, wx + (1 if wx < 2 else 0), wz, '%.0f m' % w, dz=4 if wz < 40 else -4, dx=-5 if nm == 'Food Walk' else (5 if nm == 'landing and bar' else 0))
        names(ax, i)
    key(fig, 0.014, 0.315)
    fig.text(0.014, 0.225, 'THE FARTHEST POINT OF EACH AREA (to the nearest stair; share of the area over 35 m)', color=NEW, fontsize=16, weight='bold')
    for j, (nm, w, s, wx, wz) in enumerate(T0):
        x, y = 0.014 + (j // 4) * 0.167, 0.192 - (j % 4) * 0.03
        fig.text(x, y, nm, color=FG, fontsize=14.5); fig.text(x + 0.122, y, '%.0f m' % w, color=col(w), fontsize=15, weight='bold', ha='right'); fig.text(x + 0.127, y, '%.0f %%' % s, color=DIM, fontsize=13.5)
    L = ['THE RULE: VKF Brandschutzrichtlinie 16-15, as of 01.08.2021 (read in the original, lexfind.ch/tol/24622/de):',
         '2.4.2  floors over 900 m² without level ways out: at least TWO stairs. A room for more than 100 people: two.',
         '2.4.3  whole way out: 35 m to ONE stair or exit; 50 m if at least TWO stairs or exits, apart from each other, are reached.',
         '2.4.4  inside one unit: 35 m. In extra-tall rooms 50 m, if the fire authority agrees and there are several directions.',
         '2.3  straight line in the rooms, round the walls between them; furniture not counted; inside the stair not measured.',
         '2.4.5-7  stairs and corridors 1.2 m or more, doors 0.9 m; over 200 people: 0.6 m of stair per 60 people.',
         '2.4.1  each stair leads on its own to a safe place outdoors. Only stairs are named as vertical ways out, no escalators or lifts.']
    for j, t in enumerate(L): fig.text(0.525, 0.318 - j * 0.0275, t, color=NEW if j == 0 else FG, fontsize=14, weight='bold' if j == 0 else 'normal')
    fig.text(0.014, 0.065, 'ALSO: the 10 gate stairs stand open in the gate rooms (no fire doors): as built they are not real escape stairs yet. The stair beside the arrivals escalators ends inside the claim hall, far from the kerb doors: not counted.', color=WARN, fontsize=14.5)
    fig.text(0.014, 0.037, 'The staff stair (x 140-143) serves the offices only; no door leads to it from the passengers\' side. The offices (2 184 m²) have one stair: two are needed.', color=WARN, fontsize=14.5)
    save(fig, 'TCv2-9a-Escape-Floor2-Today.png')

if 'b' in which:
    fig = sheet('SHEET 9b', 'ESCAPE WAYS, FLOOR 2 PROPOSED: 20 CLOSED EMERGENCY STAIRS', 'Alarmed, never used by passengers (green, numbered). Bright green on walls: alarmed escape doors. The escalators stay the passengers\' only way up.', foot=FOOT)
    for i, (x0, x1) in enumerate(((-250, 156), (152, 558))):
        ax = ax_plan(fig, [0.008, 0.625 - i * 0.285, 0.984, 0.285], x0, x1, -8, 90); heat(ax, F1, D1); walls(ax, 2, lw=1.4); gst(ax, True); lims(ax, x0, x1, -8, 90)
        for g, x in E.GX.items():
            if x0 < x < x1: txt(ax, x + 34, 85.5, 'GATE %d' % g, FG, 13, weight='bold')
        for n, x, w in E.GAPST:
            if n == 6: estair(ax, n, x - 2.1, 82.4, x + 2.1, 89)
            else: estair(ax, n, x - 2.1, 69.0, x + 2.1, 76.0)
        estair(ax, 10, -245.6, 56, -241.6, 64); estair(ax, 11, 551.8, 56, 555.8, 64)
        for n, x, kd in E.SOUTHST:
            if kd == 'landing': estair(ax, n, 1.5, -3.0, 16, -0.6)
            else: estair(ax, n, x - 5.5, 4.6, x + 5.5, 7.4)
            if kd == 'tf':
                for q in (-0.9, 0.9): ax.plot([x + q, x + q], [8, 11.7], color=NW, lw=2, zorder=22)
        estair(ax, 19, 116, -3.4, 128, -1.6); estair(ax, 20, 156.2, 0.8, 157.8, 12, lx=154 if i == 0 else 161, lz=6)
        for x, z in E.BACK: door(ax, x, z)
        door(ax, 150, 34.5, 1.4); door(ax, -119.15, 32, 2, 'z')
        for nm, w, s, wx, wz in T1:
            if x0 < wx < x1 and nm in ('landing (escalator top)', 'Food Walk', 'landing and bar', 'offices (staff)', 'tax free', 'escalator hall'): mark(ax, wx + (1 if wx < 2 else 0), wz, '%.0f m' % w, col=ORG if w > 35 else '#3ddc6a', dz=-4 if nm in ('Food Walk', 'landing and bar') else 4, dx=-5 if nm == 'Food Walk' else (6 if nm == 'landing and bar' else 0))
        names(ax, i)
    key(fig, 0.014, 0.318)
    legend(fig, 0.014, 0.225, [('1-9', 'In the open gaps between the gate blocks: 4.2 x 7 m, doors from the walkway and both neighbours, down to the apron', NW),
                               ('10-11', 'Beyond the walkway\'s two ends (x -241 and x 551)', NW), ('12-15', 'South front of the tax free; a 1.8 m passage through its store strip at each', NW),
                               ('16-17', 'South front of the escalator hall, outdoors', NW), ('18', 'Landing: door in the front glass at x 15; a closed stair outside along the front, x 1.5-16, z -3 to -0.6', NW),
                               ('19-20', 'Offices: in the double skin\'s gap (x 116-128) and along the east wall (x 156-158)', NW)], lh=0.0285, fs=13.5)
    fig.text(0.56, 0.335, 'FARTHEST POINT, TODAY %s WITH THE STAIRS' % ARROW, color=NEW, fontsize=15.5, weight='bold')
    for j, (nm, w, s, wx, wz) in enumerate(T1):
        x, y = 0.56 + (j // 6) * 0.215, 0.311 - (j % 6) * 0.0225; o = T0[j]
        fig.text(x, y, nm, color=FG, fontsize=14); fig.text(x + 0.130, y, '%.0f m' % o[1], color=col(o[1]), fontsize=14, ha='right'); fig.text(x + 0.139, y, ARROW, color=DIM, fontsize=14); fig.text(x + 0.178, y, '%.0f m' % w, color=col(w), fontsize=14, weight='bold', ha='right')
    L = ['WHAT STILL FAILS: the south band of Food Walk, landing and bar (z 30 to about 38): up to %.0f m. Under it are security, the lounge, toilets and the' % max(T1[3][1], T1[5][1]),
         'Grand Hall: no stair fits without taking a room. It has two directions and stays under 50 m: it needs the fire authority\'s agreement.',
         'Every gate stair: fire glass (EI 60), a door from the walkway at its head, its own door onto the apron at its foot (z 82, 1.5 m away).',
         'Each new stair: 1.8 m wide (1.2 m is the least), fire-rated box, escape handles that always open (SN EN 179 / 1125), alarm to security, lit signs.',
         'WIDTH: 10 gate stairs x 2.8 m = 28 m = 2 800 people; the 20 new ones 36 m = 3 600 (0.6 m per 60). Over 2.4 m a stair needs a middle rail (3.5.1).',
         'Taken away inside: only four 1.8 m passages through the tax free\'s store strip. Not touched: Grand Hall, escalators, landing, belts, gates.']
    for j, t in enumerate(L): fig.text(0.56, 0.158 - j * 0.0245, t, color=ORG if j < 2 else FG, fontsize=12.8)
    fig.text(0.014, 0.032, 'Stair 6 stands in front of the new swing lobby (in the gap until the lobby is built). Doors from airside to landside: alarmed, one way out only, CCTV.', color=DIM, fontsize=13.5)
    save(fig, 'TCv2-9b-Escape-Floor2-Proposed.png')

if 'c' in which:
    fig = sheet('SHEET 9c', 'ESCAPE WAYS, THE NEW EAST PART: CONCOURSE (FLOOR 2) AND GROUND FLOOR', 'Left as drawn on sheets 2 and 3, right as proposed. Room doors are not dimensioned on those sheets: 3 m openings to the south assumed.', foot='drawing only, not built')
    RF, RG = F, G; res = {}
    for r, fl in enumerate((2, 1)):
        for after in (False, True):
            Fi, ex = (E.concourse if fl == 2 else E.ground_east)(after); D = Fi.solve(ex)
            x0, x1, z0, z1 = (226, 558, 8, 92) if fl == 2 else (150, 558, -6, 56)
            ax = ax_plan(fig, [0.006 + 0.497 * after, 0.655 - r * 0.25, 0.49, 0.235], x0, x1, z0, z1)
            (after_f2 if fl == 2 else after_f1)(ax, lw=0.7); heat(ax, Fi, D, 0.6); ax.images[-1].set_zorder(8.5); walls(ax, fl, lw=0.8); lims(ax, x0, x1, z0, z1)
            if fl == 2: gst(ax, after)
            for x in STAIRS: box(ax, x - 1.3, 14 if fl == 2 else 7, x + 1.3, 21.5 if after else 20, '', fc='#0c4420', ec=NW, lw=1.6, z=22)
            if after and fl == 2:
                for x in E.CDOORS_N: door(ax, x, 52, lw=5)
                for x in E.CDOORS_S: door(ax, x, 24, lw=5)
                for n, x, w in E.GAPST[5:]: estair(ax, n, x - 2.1, 82.4 if n == 6 else 69.0, x + 2.1, 89 if n == 6 else 76.0, fs=10)
                estair(ax, 11, 551.4, 56, 555.4, 64, fs=10); estair(ax, 21, 551.4, 34, 555.4, 43, fs=10)
            if after and fl == 1:
                for x in range(170, 551, 20): door(ax, x, 0, lw=5)
                for x in STAIRS: door(ax, x, 21.5, 2.4, lw=4)
            RR = RF if fl == 2 else RG; rows = []
            for nm, k in ((('passport queue', 'queue'), ('passport line', 'line'), ('duty free', 'dutyfree'), ('toilets 1', 'wc1'), ('transfer', 'transfer'), ('food court', 'food'), ('toilets 2', 'wc2'), ('lounge', 'lounge'), ('seating', 'seating'), ('toilets 3', 'wc3')) if fl == 2 else
                          (('East Hall', 'east_hall'), ('its toilets', 'wc_hall'), ('bag service', 'bagserv'), ('claim hall', 'claim'), ('passport hall', 'pc_hall'), ('crew centre', 'crew'), ('lockers', 'lockers'), ('staff security', 'staffsec'), ('police', 'police'), ('coach station', 'coach'))):
                b = RR[k]; w = Fi.worst(D, (b[0] + .4, b[1] + .4, b[2] - .4, b[3] - .4)); rows.append((nm, w))
                if w and w[0] > 35.04: mark(ax, w[1], w[2], '%.0f' % w[0], col=RED if w[0] > 50 else ORG, fs=10)
            res[(fl, after)] = rows; print(fl, after, [(n, w and round(w[0], 1)) for n, w in rows])
            fig.text(0.010 + 0.497 * after, 0.905 - r * 0.258, ('FLOOR 2, ' if fl == 2 else 'GROUND FLOOR, ') + ('PROPOSED' if after else 'AS DRAWN'), color=NEW if after else FG, fontsize=16, weight='bold')
    key(fig, 0.014, 0.385)
    for q, fl in enumerate((2, 1)):
        fig.text(0.012 + q * 0.25, 0.292, ('CONCOURSE' if fl == 2 else 'EAST GROUND FLOOR') + ': farthest point, drawn %s proposed' % ARROW, color=NEW, fontsize=14, weight='bold')
        for j, (a, b) in enumerate(zip(res[(fl, False)], res[(fl, True)])):
            y = 0.266 - j * 0.0235; x = 0.012 + q * 0.25
            fig.text(x, y, a[0], color=FG, fontsize=14); fig.text(x + 0.130, y, '%.0f m' % a[1][0], color=col(a[1][0]), fontsize=14, ha='right'); fig.text(x + 0.139, y, ARROW, color=DIM, fontsize=14); fig.text(x + 0.178, y, '%.0f m' % b[1][0], color=col(b[1][0]), fontsize=14, weight='bold', ha='right')
    L = ['The 11 stairs stay where they are (x 246 + 30 k), but each box grows 1.5 m: z 7-21.5 (was 20). That alone cuts the straight line at z 52 from 35.3 to 34.0 m.',
         'Floor 2: alarmed doors at z 52 from the toilets, transfer, lounge and shops to the walkway, and more doors to the gallery (bright green).',
         'The walkway gets stairs 6-9 and 11 (sheet 9b) and doors at the gate stairs\' heads; stair 21 at the concourse\'s east end (x 551-555).',
         'Ground: a door from each hall into the stair box at z 21.5 (inside a stair nothing is measured); escape doors in the front glass every 20 m.',
         'Staff rooms, north row (36-38 m): a 2 m fire corridor at z 26-28 (x 372-470) to the stair boxes: 23 m in the room + 20 m, under 50.',
         'East Hall 41 m: 7-19 m tall, several directions: the 50 m clause (2.4.4). Its toilets and bag service (46-53 m): a protected lane to the front is needed.',
         'To check when the rooms are drawn in detail: real doors, people per room, the closed arrivals corridor under the walkway (a way out every 35 m).']
    for j, t in enumerate(L):
        y = 0.388 - j * 0.0475; fig.text(0.517, y, str(j + 1), color='#05080c', fontsize=13, weight='bold', ha='center', va='top', bbox=dict(boxstyle='circle,pad=0.2', fc=NW, ec='none'))
        fig.text(0.53, y + 0.003, wrap(t, 108), color=FG, fontsize=13.8, va='top', linespacing=1.25)
    save(fig, 'TCv2-9c-Escape-East-Part.png')

if 'd' in which:
    fig = sheet('SHEET 9d', 'ESCAPE WAYS, GROUND FLOOR: SECURITY, AIRSIDE LOBBY, CORRIDOR, ESCALATOR ROOM', 'Ways out to the open air. Today all of them lead back through security and the Grand Hall to the three entrances.', foot=FOOT)
    A = (('Grand Hall', (42.4, 0, 114, 29.6)), ('security', (50.4, 30.4, 89.6, 41.6)), ('airside lobby', (50.4, 42.4, 89.6, 51.6)), ('airside corridor', (4.4, 44.4, 50, 51.6)), ('way down', (4.4, 30.4, 15.6, 44)), ('escalator room', (0.4, 0.4, 41.6, 29.6)), ('lounge', (90.4, 30.4, 113.6, 51.6)), ('staff / service room', (16.4, 30.4, 35.6, 43.6)), ('hall toilets', (36.4, 30.4, 49.6, 43.6)))
    rs = []
    for after in (False, True):
        Fi, ex = E.floor1(after); D = Fi.solve(ex); x0 = 0.006 + 0.497 * after
        ax = ax_plan(fig, [x0, 0.45, 0.49, 0.44], -4, 118, -8, 56); heat(ax, Fi, D); walls(ax, 1, lw=1.8); lims(ax, -4, 118, -8, 56)
        for y in (12.33, 16.05): box(ax, 16, y - 1.6, 29.4, y + 1.6, '', fc='#2b3a48', ec='#c9d6e2', lw=1, z=12)
        txt(ax, 22.7, 14.2, 'escalators', FG, 12)
        r = [(n, Fi.worst(D, b)) for n, b in A]; rs.append(r); print('F1', after, [(n, w and round(w[0], 1)) for n, w in r])
        for n, w in r:
            if n != 'Grand Hall': mark(ax, min(112, max(1, w[1])), w[2], '%.0f m' % w[0], col=col(w[0]), dz=-3 if w[2] > 40 else 3, dx=3 if w[1] < 8 else (-3 if w[1] > 105 else 0))
        for t, x, z in (('ESCALATOR ROOM', 20, 24), ('GRAND HALL', 78, 20), ('SECURITY', 70, 36.5), ('AIRSIDE LOBBY', 70, 45), ('LOUNGE', 102, 42), ('CORRIDOR', 28, 48), ('WAY DOWN', 10, 42.5), ('service', 26, 38)): txt(ax, x, z, t, FG, 13, weight='bold')
        if after:
            for x in (17.5, 28, 39): door(ax, x, 0, lw=7); num(ax, x, -3, 'A', NW)
            for x in (62, 88, 110.5): door(ax, x, 0, lw=7); num(ax, x, -3, 'B', NW)
            for pts in ([(34.2, 44), (34.2, 28.2), (40.2, 28.2), (40.2, 0)], [(35.8, 44), (35.8, 29.8), (41.8, 29.8), (41.8, 0)]): ax.plot(*zip(*pts), color=NW, lw=2.4, zorder=24)
            num(ax, 35, 34, 'C', NW)
            for x, n in ((102, 'D'), (88, 'E'), (55, 'E')): door(ax, x, 30, lw=7); num(ax, x, 27.4, n, NW)
            for x in (53, 87): door(ax, x, 42, lw=7); num(ax, x, 39.4, 'F', NW)
        fig.text(x0 + 0.004, 0.905, 'PROPOSED' if after else 'TODAY', color=NEW if after else FG, fontsize=18, weight='bold')
    key(fig, 0.014, 0.425, fs=13.5)
    fig.text(0.014, 0.325, 'FARTHEST POINT, TODAY %s PROPOSED' % ARROW, color=NEW, fontsize=15, weight='bold')
    for j, (a, b) in enumerate(zip(*rs)):
        y = 0.296 - j * 0.025
        fig.text(0.014, y, a[0], color=FG, fontsize=15); fig.text(0.165, y, '%.0f m' % a[1][0], color=col(a[1][0]), fontsize=15, ha='right'); fig.text(0.176, y, ARROW, color=DIM, fontsize=15); fig.text(0.222, y, '%.0f m' % b[1][0], color=col(b[1][0]), fontsize=15, weight='bold', ha='right')
    legend(fig, 0.33, 0.418, [('A', 'Three alarmed doors in the escalator room\'s glass front, straight out to the forecourt (x 17.5, 28, 39)', NW), ('B', 'Three alarmed doors in the Grand Hall\'s front (x 62, 88, 110.5), between the entrances', NW),
                              ('C', 'A protected passage, 1.6 m (fire walls, the rules of a stair: its inside is not measured), from the corridor to the front', NW), ('D', 'Lounge to the Grand Hall (x 102): one way out, alarmed, CCTV', NW),
                              ('E', 'Security to the Grand Hall: two more doors (x 55, x 88)', NW), ('F', 'Airside lobby to security, against the way in (x 53, x 87): only on alarm', NW)], lh=0.0325, fs=14.5)
    lo, sec, lng = rs[1][2][1][0], rs[1][1][1][0], rs[1][6][1][0]
    L = ['WHAT STILL FAILS: security %.0f m (under 50, two directions); the airside lobby %.0f m and the lounge %.0f m. Behind them is the baggage hall with its two belts along its' % (sec, lo, lng),
         'whole length (z 56 and 63): a passage out to the apron would cut them. The Grand Hall they escape through is up to 20 m tall: the fire authority can accept 50 m there',
         '(2.4.4) or a calculation (2.1.3). Without that the lounge needs its own way out (the toilets: 50 m). Passage C costs about 70 m² at the edge of the service and escalator rooms.',
         'THE ESCALATORS STAY AND WORK. Checked in the code: ADPGameMode.cpp places both pairs of the escalator room (and the arrivals pair) in every normal game one second after',
         'the start (only the test switch -NoEscalators leaves them out); PaxFlow.cpp sends every departing passenger up through WayUpstairs: corridor, escalator room, escalators.',
         'None of the doors and passages here is a passengers\' way: all are closed, alarmed and one way out.']
    for j, t in enumerate(L): fig.text(0.27, 0.205 - j * 0.0285, t, color=ORG if j < 3 else (NW if j < 5 else FG), fontsize=14.2)
    save(fig, 'TCv2-9d-Escape-Ground-Airside.png')
