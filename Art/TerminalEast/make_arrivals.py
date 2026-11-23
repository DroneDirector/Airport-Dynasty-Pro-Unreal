# Terminal part 2b (owner approved 2026-11-23, blueprint v2 sheets 2c, 2d, 3a, 3c, 5b): gate 13's swing lobby, the non-Schengen
# arrivals' ways down at gates 13-16 and the closed corridor to passport control in. Numbers: arr_geom.py. The models:
#   placed four times (in the gate's own metres: u = plan x - the gate room's west wall), instanced
#     east_a_core, east_a_core_glass   Floor 2: the jetways' vestibule, the way down's glass, the lift nook; the ground lobby; the stair
#     east_a_csigns                    their signs
#     east_a_gfront, _glass            the ground window with the gate stair hall's own door
#   placed at gates 14-16 (instanced), each with its twin for gate 13 (its stair turned, three jetways, the swing doors F)
#     east_a_slab / _slab13            the view bay's floor with the hole (13: and the gate room's), the ground ceiling
#     east_a_plaster / _plaster13      the gate room's plaster: the opening to the view bay (13: doors F)
#     east_a_front / _front13 (_glass) the view bay's window with the real jetway doors' openings
#     east_a_side / _side13            the ground lobby's wall by the gate's stair
#   once:  east_a_white13 (gate 13's west wall and roof), east_a_swing (_glass) the swing lobby, east_a_corr_* the corridor,
#          east_a_signs, east_a_swing_non / _sch   the swing gate's switching signs (shown by ASwingGate, one or the other)
# The escalators, the stairs' cleated steps, the lifts' cars and the switching signs are the game's (ASwingGate, SwingGate.cpp);
# the doors, lift shafts and lamps are the terminal's own (Tools/terminal_arrivals_edit.py). Free: the escape stairs (sheet 9).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalEast/make_arrivals.py
#   then Tools/import_east.py east_a_..., Tools/terminal_edit.py, Tools/terminal_cut.py 10381 10382
import bpy, bmesh, json, math, os, sys, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
from arr_geom import *
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(ROOT, 'Art', 'TerminalAnnex', 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
src = open(os.path.join(HERE, 'make_concourse.py'), encoding='utf-8').read()
exec(src[src.index("W, T = 'MI_T_white'"):src.index("S = Piece('east_c_solid')")])
exec(src[src.index('FONT ='):src.index('# ---- walls with their steel skirting')])
CONC, CARPET, TERR = 'MI_T_concrete', 'MI_T_carpetBlue', 'MI_T_terrazzo'
ALL = []
def piece(n): P = Piece(n); ALL.append(P); return P
F1T, HD = 0.022, 3.145                                 # the new ground floors' top; a sliding door's top over its floor

# ---- helpers ----
def plate(P, x0, x1, z0, z1, yb, yt, holes, mt, mb=None, ms=None, inner=True):
    """a level plate with holes; mb None: only its top (yb ignored). Its sides ms (inner: the holes' too)."""
    xs = sorted({x0, x1} | {min(max(v, x0), x1) for h in holes for v in h[:2]}); zs = sorted({z0, z1} | {min(max(v, z0), z1) for h in holes for v in h[2:]})
    full = lambda a, b, c, d: x0 - 1e-6 <= a and b <= x1 + 1e-6 and z0 - 1e-6 <= c and d <= z1 + 1e-6 and not any(h[0] <= a + 1e-6 and b <= h[1] + 1e-6 and h[2] <= c + 1e-6 and d <= h[3] + 1e-6 for h in holes)
    for i in range(len(xs) - 1):
        for k in range(len(zs) - 1):
            a, b, c, d = xs[i], xs[i + 1], zs[k], zs[k + 1]
            if not full(a, b, c, d): continue
            if mt: P.box(a, b, yt, yt, c, d, mt, sides=('+y',))
            if mb: P.box(a, b, yb, yb, c, d, mb, sides=('-y',))
            if ms:
                for s, out, q in (('-x', i == 0, (xs[i - 1], a, c, d)), ('+x', i == len(xs) - 2, (b, xs[min(i + 2, len(xs) - 1)], c, d)), ('-z', k == 0, (a, b, zs[k - 1], c)), ('+z', k == len(zs) - 2, (a, b, d, zs[min(k + 2, len(zs) - 1)]))):
                    if out or (inner and not full(*q)): P.box(a, b, yb, yt, c, d, ms, sides=(s,))
def prism(P, pts, z0, z1, m, caps=True):
    """a polygon (u, y), anticlockwise, from z0 to z1"""
    n = len(pts); nb = lambda v: (-v[0], -v[2], v[1])
    for i in range(n):
        (a, b), (c, d) = pts[i], pts[(i + 1) % n]; L = math.hypot(c - a, d - b)
        P.want.append((P.face([(a, b, z0), (c, d, z0), (c, d, z1), (a, b, z1)], m), nb(((d - b) / L, -(c - a) / L, 0))))
    if caps:
        for z, s in ((z0, -1), (z1, 1)): P.want.append((P.face([(x, y, z) for x, y in pts], m), nb((0, 0, s))))
def box2(P, ax, at, t0, t1, u0, u1, y0, y1, m, sides='all'):
    if ax == 'z': P.box(u0, u1, y0, y1, at + t0, at + t1, m, sides=sides)
    else: P.box(at + t0, at + t1, y0, y1, u0, u1, m, sides=sides)
def gw(S, G, ax, at, u0, u1, y0, y1, doors=(), step=2.0, ends=(True, True), posts=()):
    """a glass wall in the terminal's mullions (8 x 14 cm); doors: (middle, clear width) - the sliding doors' places"""
    ops = [(c - w / 2 - 0.16, c + w / 2 + 0.16) for c, w in doors]; head = y0 + HD + 0.01
    cu = sorted({u0, u1} | {v for o in ops for v in o}); done = set()
    for u, v in zip(cu, cu[1:]):
        op = (u, v) in ops; n = 1 if op else max(1, round((v - u) / step))
        for i in range(n + 1):
            q = round(u + (v - u) * i / n, 4)
            if abs(q - u0) < 1e-6: q = u0 + 0.04
            elif abs(q - u1) < 1e-6: q = u1 - 0.04
            elif any(abs(q - o[0]) < 1e-3 for o in ops): q -= 0.04
            elif any(abs(q - o[1]) < 1e-3 for o in ops): q += 0.04
            if q in done or (q < u0 + 0.05 and not ends[0]) or (q > u1 - 0.05 and not ends[1]): continue
            done.add(q); box2(S, ax, at, -0.07, 0.07, q - 0.04, q + 0.04, y0, y1, MU)
        rails = [(y1 - 0.08, y1 - 0.004)] + ([(head, head + 0.07)] if op else [(y0, y0 + 0.06), (y0 + 3.16, y0 + 3.24)])
        for a, b in rails: box2(S, ax, at, -0.066, 0.066, u, v, a, b, MU)
        bot = head + 0.07 if op else y0 + 0.06
        box2(G, ax, at, -0.015, 0.015, u, v, bot, y1 - 0.08, GL, sides=('-' + ax, '+' + ax))
        if not op:                                     # two rows of white dots (the rule for glass walls)
            for y in (y0 + 0.95, y0 + 1.5):
                q = u + 0.12
                while q < v - 0.1:
                    for a, b in ((-0.019, -0.0155), (0.0155, 0.019)): box2(S, ax, at, a, b, q, q + 0.05, y, y + 0.05, W, sides=('-' + ax, '+' + ax))
                    q += 0.25
def wall(S, ax, at, u0, u1, y0, y1, holes=(), t=0.16, ms=(PL, PL), skirt=True, ends=True):
    """a plastered wall with its steel skirting; holes: (a, b, head)"""
    cu = sorted({u0, u1} | {v for h in holes for v in h[:2]})
    for u, v in zip(cu, cu[1:]):
        h = next((q[2] for q in holes if q[0] <= u + 1e-6 and v <= q[1] + 1e-6), None); lo = y0 if h is None else h
        if lo >= y1: continue
        e = () if h is None else ('-y',)
        for s, m in zip('-+', ms): box2(S, ax, at, -t / 2, t / 2, u, v, lo, y1, m, sides=(s + ax,))
        o = 'x' if ax == 'z' else 'z'; sd = e + tuple('-+'[i] + o for i, q in enumerate((u, v)) if (ends and abs(q - (u0, u1)[i]) < 1e-6) or (h is None and any(abs(q - k[j]) < 1e-6 for k in holes for j in (0, 1))))
        if sd: box2(S, ax, at, -t / 2, t / 2, u, v, lo, y1, ms[0], sides=sd)
        if skirt and h is None:
            for s in (-1, 1):
                a, b = sorted((s * t / 2, s * (t / 2 + 0.012))); box2(S, ax, at, a, b, u, v, y0, y0 + 0.1, STEEL, sides=('+y', '-x', '+x', '-z', '+z'))
def sloped(P, u0, y0, u1, y1, h, z0, z1, m): prism(P, [(u0, y0), (u1, y1), (u1, y1 + h), (u0, y0 + h)], z0, z1, m)

# ======================================================================================================== the core (u): Floor 2
S = piece('east_a_core'); G = piece('east_a_core_glass'); SG = piece('east_a_csigns')
YT = 11.36
gw(S, G, 'x', CW, VS, 81.84, F2, YT, ends=(False, False)); gw(S, G, 'z', VS, CW - 0.07, LU - 1.07, F2, YT, ends=(True, False))
gw(S, G, 'z', NS, LU + 1.07, CE + 0.07, F2, YT, ends=(False, True), step=1.3); gw(S, G, 'x', CE, NS + 0.071, VS - 0.071, F2, YT, ends=(False, False), step=2.3)
gw(S, G, 'x', CE, VS + 0.07, 81.84, F2, YT, [(ARR_Z, 5.0)], ends=(False, False))
gw(S, G, 'z', VS, CE - 0.07, 67.85, F2, YT, [(GATE_U, 5.0)], ends=(True, True), step=1.95)
# the hole's white edges (the ground ceiling, 5.36, to the carpet) and its steel trim; the lift's own threshold is its shaft
for s, a, b, c, d in (('+x', HX0, HX0, HZ[0], HZ[2]), ('-x', ST1, ST1, HZ[0], HZ[1]), ('-x', HX1, HX1, HZ[1], HZ[2]), ('+z', HX0, ST1, HZ[0], HZ[0]), ('-z', HX0, HX1, HZ[2], HZ[2]), ('-z', ST1, ST1, HZ[1], HZ[1])):
    if a != b or c != d: S.box(a, b, 5.36, 6.008, c, d, W, sides=(s,))
S.box(ST1, HX1, 5.36, 6.008, HZ[1], HZ[1], W, sides=('+z',))
# the stair: the solid steps under the game's cleated ones; its two guards (glass under a steel rail)
for k in range(30):
    z0 = ST0 + 0.38 * k; yt = 0.2 * (k + 1)
    S.box(z0 + 0.012, z0 + 0.38, 0.012, yt - 0.025, STZ[0], STZ[1], TERR)
sl = 0.2 / 0.38
for z in (STZ[0] + 0.05, STZ[1] - 0.05):
    ya, yb = 0.2, 6.0
    sloped(G, ST0, ya + 0.12, ST1 - 0.38, ya + 0.12 + sl * (ST1 - 0.38 - ST0), 0.72, z - 0.006, z + 0.006, GL)
    sloped(S, ST0 - 0.15, ya + 0.9 - sl * 0.15, ST1 - 0.38, ya + 0.9 + sl * (ST1 - 0.38 - ST0), 0.05, z - 0.025, z + 0.025, STEEL)
    for k in range(0, 30, 4):
        u = ST0 + 0.38 * k + 0.19; y = 0.2 * (k + 1) - 0.025; S.box(u - 0.02, u + 0.02, y, 0.2 + 0.9 + sl * (u - ST0) + 0.01, z - 0.02, z + 0.02, STEEL)
    S.box(ST1 - 0.38, ST1 + 0.3, 6.0 + 0.9 - 0.005, 6.0 + 0.9 + 0.045, z - 0.025, z + 0.025, STEEL); S.box(ST1 + 0.2, ST1 + 0.24, 6.008, 6.9, z - 0.02, z + 0.02, STEEL)
    G.box(ST1 - 0.36, ST1 + 0.2, 6.12, 6.84, z - 0.006, z + 0.006, GL)
S.box(ST0 - 0.6, ST0 - 0.2, F1T, F1T + 0.004, STZ[0], STZ[1], 'MI_T_yellow', sides=('+y',))            # the warning strips
S.box(ST1 + 0.3, ST1 + 0.7, 6.008, 6.014, STZ[0] + 0.1, STZ[1] - 0.1, 'MI_T_yellow', sides=('+y',))
# the signs on Floor 2
band('+x', CE + 0.075, 77.2, 81.2, 9.35, 9.95, 'ARRIVALS  ·  PASSPORT CONTROL', 0.28, AMBER, d=0.05); band('-x', CE - 0.075, 77.2, 81.2, 9.35, 9.95, 'NO ENTRY', 0.34, RED, d=0.05)
band('-z', VS - 0.075, GATE_U - 2.4, GATE_U + 2.4, 9.35, 9.95, 'TO THE AIRCRAFT', 0.34, d=0.05); band('+z', VS + 0.075, GATE_U - 2.4, GATE_U + 2.4, 9.35, 9.95, 'NO ENTRY', 0.34, RED, d=0.05)
band('+x', LU + 1.075, LZ - 0.7, LZ + 0.7, 8.4, 8.75, 'LIFT', 0.22, d=0.04); band('+x', LU + 1.075, LZ - 0.7, LZ + 0.7, 2.4, 2.75, 'LIFT', 0.22, d=0.04)
# ======================================================================================================== the core (u): the ground lobby
plate(S, LW + 0.08, LE - 0.08, 68.1, 81.84, 0, F1T, [], TERR); S.box(LE - 0.08, LE - 0.08, 0.012, F1T, 68.1, 81.84, TERR, sides=('+x',))
wall(S, 'x', LE, 68.1, 81.84, 0.012, 5.36)
wall(S, 'x', LW, 71.28, 81.84, 0.012, 5.36)
for z0, z1, y1 in ((STZ[0], STZ[1], 5.36), (STZ[1], 81.84, 4.85)): S.box(ST1 + 0.06, ST1 + 0.18, F1T, y1, z0, z1, PL, sides=('-x', '+x', '+y', '-z', '+z'))      # closed under the escalators and the stair's head
S.box(ST1 + 0.06, ST1 + 0.18, F1T, 5.36, VS - 0.02, STZ[0], PL, sides=('-x', '+x', '-z'))
for u in (LW + 0.09, LE - 0.09 - 0.03): S.box(u, u + 0.03, 1.0, 1.14, 71.5 if u < 40 else 68.2, 81.7, OAK)                # an oak rail along the walls
wl = 68.1                                              # the pier's wall: its oak portal round the doors, an LED line over it
for a, b in ((OUT_U - 2.0, OUT_U - 1.66), (OUT_U + 1.66, OUT_U + 2.0)): S.box(a, b, F1T, 3.5, wl, wl + 0.1, OAK, sides=('-x', '+x', '+z', '+y'))
S.box(OUT_U - 1.66, OUT_U + 1.66, 3.17, 3.5, wl, wl + 0.1, OAK, sides=('-y', '+z', '+y')); S.box(OUT_U - 2.0, OUT_U + 2.0, 3.5, 3.53, wl, wl + 0.06, LED, sides=('+y', '+z', '-x', '+x'))
band('+z', wl, OUT_U - 2.9, OUT_U + 2.9, 3.7, 4.25, 'PASSPORT CONTROL  ·  BAGGAGE CLAIM', 0.27, AMBER, d=0.1)
S.box(OUT_U - 0.2, OUT_U + 0.2, F1T, F1T + 0.003, 69.2, 70.2, 'MI_T_yellow', sides=('+y',))    # the way painted on the floor: an arrow to the doors
prism_y = [(OUT_U - 0.6, 69.2), (OUT_U, 68.6), (OUT_U + 0.6, 69.2)]
S.want.append((S.face([(x, F1T + 0.003, z) for x, z in prism_y], 'MI_T_yellow'), (0, 0, 1)))
band('-z', 81.83, 35.2, 37.0, 3.0, 3.35, 'EMERGENCY EXIT', 0.17, GREEN, d=0.04)

# ======================================================================================================== the windows
OM = [33.04 + 0.96 + 1.78947 * k for k in range(20)]   # the view bay's own mullions (u 34..68)
def front(P, Pg, jw):
    ops = [(c - 1.05, c + 1.05) for c in jw]; hd = 8.405
    for m in OM:
        y = 6.0; o = next((q for q in ops if q[0] - 0.1 < m < q[1] + 0.1), None)
        if o: y = hd + 0.06
        P.box(m - 0.04, m + 0.04, y, 11.4, 81.84, 81.98, MU)
    for m in (CE,): P.box(m - 0.04, m + 0.04, 6.0, 11.4, 81.84, 81.98, MU)
    for a, b in ops:
        for m in (a - 0.04, b + 0.04): P.box(m - 0.04, m + 0.04, 6.0, 11.4, 81.84, 81.98, MU)
        P.box(a, b, hd, hd + 0.06, 81.844, 81.976, MU); P.box(a, b, 5.99, 6.012, 81.83, 82.0, STEEL, sides=('+y', '-z', '+z'))
    cu = sorted({34.0, 68.0} | {v for o in ops for v in o})
    for u, v in zip(cu, cu[1:]): Pg.box(u, v, hd + 0.06 if (u, v) in ops else 6.0, 11.4, 81.89, 81.92, GL, sides=('-z', '+z'))
for tag, jw in (('', JW[14]), ('13', JW[13])):
    front(piece('east_a_front' + tag), piece('east_a_front%s_glass' % tag), jw)
P = piece('east_a_gfront'); Pg = piece('east_a_gfront_glass')
GM = [28.0, 31.4, 33.1, LW, 34.8, 37.2] + [39.0 + 1.8 * k for k in range(11)] + [58.8, 61.2, 62.9, 64.6, 66.3, 68.0]
a, b = STAIR_DOOR - 1.05, STAIR_DOOR + 1.05
for m in sorted(set(round(q, 3) for q in GM + [a - 0.04, b + 0.04])): P.box(m - 0.04, m + 0.04, 0.0, 5.4, 81.84, 81.98, MU)
P.box(a, b, 2.405, 2.465, 81.844, 81.976, MU)
for u, v, y in ((28.0, a, 0.0), (a, b, 2.465), (b, 34.8, 0.0)): Pg.box(u, v, y, 5.4, 81.89, 81.92, GL, sides=('-z', '+z'))

# ======================================================================================================== floors, ceilings, plaster (u)
CORE_H = [(HX0, ST1, HZ[0], HZ[1]), (HX0, HX1, HZ[1], HZ[2]), (LU - 1.02, LU + 1.02, LZ - 1.02, LZ + 1.02)]
OLD_S, OLD_L = (28.3, 31.1, 69.5, 80.5), (31.88, 33.92, 79.58, 81.62)
for tag, h_v, h_c in (('', [], [OLD_S, OLD_L]), ('13', [TURN], [TURN, OLD_L])):
    P = piece('east_a_slab' + tag)
    plate(P, 34.0, 68.0, 68.0, 82.0, 5.63, 5.99, CORE_H + h_v, CONC, CONC, CONC); plate(P, 34.0, 68.0, 68.0, 82.0, 0, 6.008, CORE_H + h_v, CARPET)
    if tag: plate(P, 0.0, 34.0, 68.0, 82.0, 5.63, 5.99, [TURN, OLD_L], CONC, CONC, CONC); plate(P, 0.0, 34.0, 68.0, 82.0, 0, 6.008, [TURN, OLD_L], CARPET)
    plate(P, 28.0, 68.0, 68.0, 82.0, 5.36, 5.36, CORE_H + h_c, None, OFF)
    Q = piece('east_a_plaster' + tag); za, zb = PARTY[13 if tag else 14]
    f0, f1 = DF - 3.17, DF + 3.17
    for z0, z1, y0 in (((68.0, 82.0, 6.0),) if not tag else ((68.0, f0, 6.0), (f0, f1, F2 + HD + 0.01), (f1, 82.0, 6.0))):
        Q.box(0.03, 0.15, y0, 11.4, z0, z1, PL, sides=('-x', '+x', '+y') + (('-y',) if y0 > 6 else ()) + (('-z',) if z0 in (68.0, f1) else ()) + (('+z',) if z1 in (82.0, f0) else ()))
    for z0, z1, y0 in ((68.0, za, 6.0), (za, zb, 9.21), (zb, 82.0, 6.0)):
        Q.box(33.9, 34.1, y0, 11.4, z0, z1, PL, sides=('-x', '+x', '+y', '-y') + (('-z',) if z0 != za else ()) + (('+z',) if z1 != zb else ()))
    for z0, z1, y0, y1 in ((za, za + 0.06, 6.012, 9.21), (zb - 0.06, zb, 6.012, 9.21)): Q.box(33.87, 34.13, y0, y1, z0, z1, OAK)                        # the opening's oak frame
    Q.box(33.87, 34.13, 9.15, 9.21, za + 0.06, zb - 0.06, OAK, sides=('-y', '-x', '+x'))
    if tag:                                            # gate 13's outer west wall and its roof, the doors F cut
        Wp = piece('east_a_white13')
        for z0, z1, y0 in ((68.0, f0, 6.0), (f0, f1, F2 + HD + 0.01), (f1, 82.0, 6.0)):
            Wp.box(-0.08, 0.0, y0, 12.0, z0, z1, W, sides=('-x', '+x', '+y') + (('-y',) if y0 > 6 else ()) + (('-z',) if z0 in (68.0, f1) else ()) + (('+z',) if z1 in (82.0, f0) else ()))
        Wp.box(0.0, 34.0, 11.4, 12.0, 68.0, 82.0, W)
        for z0, z1 in ((f0, f0), (f1, f1)): Wp.box(0.0, 0.03, F2, F2 + HD + 0.01, z0, z1, W, sides=('-z', '+z'))
        Wp.box(-0.08, 0.15, 5.99, 6.012, f0, f1, STEEL, sides=('+y', '-x', '+x'))
    # the ground lobby's wall beside the gate's stair hall (gate 13: the box under its turned stair)
    Z = piece('east_a_side' + tag)
    if not tag: wall(Z, 'x', LW, 68.1, 71.28, 0.012, 5.36, ends=False)
    else:
        wall(Z, 'z', 71.2, LW + 0.08, 40.5, 0.012, 5.36, ends=False); wall(Z, 'x', 40.42, 68.1, 71.12, 0.012, 5.36, ends=False)
        Z.box(40.5, 40.5, 0.012, 5.36, 71.12, 71.28, PL, sides=('+x',)); Z.box(LW - 0.08, LW - 0.08, 0.012, 5.36, 71.12, 71.28, PL, sides=('-x',)); Z.box(LW - 0.08, LW + 0.08, 0.012, 5.36, 71.12, 71.12, PL, sides=('-z',))

# ======================================================================================================== the swing lobby
S = piece('east_a_swing'); G = piece('east_a_swing_glass'); SG = piece('east_a_signs')
plate(S, SX0, SX1, 68.08, SZ1, 5.63, 5.99, [], CONC, None, CONC); plate(S, SX0, SX1, 68.08, SZ1, 0, 6.008, [], CARPET)
S.box(SX0, SX1, 5.4, 5.4, 68.08, SZ1, W, sides=('-y',)); S.box(SX0, SX1, 5.4, 6.0, SZ1, SZ1, W, sides=('+z',))
S.box(SX0, SX1, 11.4, 12.0, 68.08, SZ1, W, sides=('+y', '+z')); S.box(SX0, SX1, 11.36, 11.36, 68.1, 81.84, OFF, sides=('-y',))
n = 10; st = (SX1 - SX0) / n
for i in range(n + 1):
    m = min(max(SX0 + st * i, SX0 + 0.04), SX1 - 0.04); S.box(m - 0.04, m + 0.04, 6.0, 11.4, 81.84, 81.98, MU)
for y0, y1 in ((6.0, 6.06), (9.2, 9.28), (11.32, 11.396)): S.box(SX0 + 0.08, SX1 - 0.08, y0, y1, 81.846, 81.974, MU)
G.box(SX0, SX1, 6.0, 11.4, 81.89, 81.92, GL, sides=('-z', '+z'))
S.box(SX0 + 0.1, SX1 - 0.1, 6.95, 7.0, 81.6, 81.65, STEEL)
for i in range(1, n): S.box(SX0 + st * i - 0.02, SX0 + st * i + 0.02, 6.008, 6.95, 81.605, 81.645, STEEL)
# its side walls lined (the neighbours' white outsides), skirting, an LED line under the ceiling
for x, s in ((SX0, '+x'), (SX1, '-x')):
    a, b = (x, x + 0.03) if s == '+x' else (x - 0.03, x)
    for z0, z1, y0 in (((68.1, 81.84, 6.008),) if s == '+x' else ((68.1, DF - 3.17, 6.008), (DF - 3.17, DF + 3.17, F2 + HD + 0.01), (DF + 3.17, 81.84, 6.008))):
        S.box(a, b, y0, 11.36, z0, z1, PL, sides=(s, '-z', '+z', '-y'))
        if y0 < 7:
            p, q = (b, b + 0.012) if s == '+x' else (a - 0.012, a); S.box(p, q, 6.008, 6.11, z0, z1, STEEL, sides=('+y', s, '-z', '+z'))
    p, q = (b, b + 0.04) if s == '+x' else (a - 0.04, a); S.box(p, q, 11.3, 11.34, 68.2, 81.7, LED, sides=(s, '-y', '-z', '+z'))
# the walkway's wall x 237..242 put back round the doors C (the old faces go: Data/terminal-cuts.json "touch" on 10381 / 10382)
C0, C1, CH2 = DC - 1.36, DC + 1.36, F2 + 2.85
for a, b, y in ((237.0, C0, 6.0), (C0, C1, CH2), (C1, 242.0, 6.0)):
    dn = ('-y',) if y > 6 else ()
    S.box(a, b, y, 11.4, 67.85, 67.97, PL, sides=('-z',) + dn); S.box(a, b, y, 12.0, 68.0, 68.08, W, sides=('+z',) + dn)
for x, s in ((C0, '+x'), (C1, '-x')): S.box(x, x, 6.0, CH2, 67.85, 68.08, W, sides=(s,))
S.box(C0, C1, CH2, CH2, 67.97, 68.0, W, sides=('-y',)); S.box(C0, C1, 5.99, 6.012, 67.85, 68.08, STEEL, sides=('+y', '-z', '+z'))
# the doors B's sides and tops (the two open ways onto the walkway, 3.6 m high; the old lintel bar at 9.2..9.32 stays)
for c, a, b in DB:
    d0, d1 = c - 3.16, c + 3.16
    for u, v in ((a, d0), (d1, b)):
        S.box(u, u + 0.06, F2, 9.199, 67.93, 68.07, MU); S.box(v - 0.06, v, F2, 9.199, 67.93, 68.07, MU)
        for y0, y1 in ((F2, F2 + 0.06), (9.139, 9.199)): S.box(u + 0.06, v - 0.06, y0, y1, 67.934, 68.066, MU)
        G.box(u + 0.06, v - 0.06, F2 + 0.06, 9.139, 67.985, 68.015, GL, sides=('-z', '+z'))
    S.box(d0, d1, F2 + HD + 0.002, 9.199, 67.93, 68.07, MU)
    S.box(a, b, 9.321, 9.6, 67.96, 68.04, MU, sides=('-z', '+z'))
    S.box(a, b, 5.99, 6.0125, 67.9, 68.1, STEEL, sides=('+y', '-z', '+z'))
# the fixed signs: the lobby's own
band('-x', SX1 - 0.03, DF - 2.6, DF + 2.6, 10.2, 10.75, 'SWING LOBBY  ·  GATE 13', 0.3, d=0.06)

# ======================================================================================================== the corridor (plan x, z)
S = piece('east_a_corr')
plate(S, CX0, CX1, CZ0 + 0.1, CZ1, 0, F1T, [], TERR); plate(S, KX0 + 0.1, KX1 - 0.1, 52.1, CZ0 + 0.1, 0, F1T, [], TERR)
S.box(PC_X - PC_HALF, PC_X + PC_HALF, 0.0, F1T, 51.9, 52.1, STEEL, sides=('+y', '-z'))
for q in f1_z68(): S.box(q[0], q[1], 0.0, F1T, 67.9, 68.1, STEEL, sides=('+y',))
MS = (CONC, PL)                                        # outside concrete as the baggage hall's, inside plaster
YW = 4.3
wall(S, 'z', CZ0, CX0 - 0.1, CX1 + 0.1, 0.0, YW, [(KX0 + 0.1, KX1 - 0.1, CH)], t=0.2, ms=MS)
wall(S, 'x', CX0, CZ0 + 0.1, CZ1, 0.0, YW, t=0.2, ms=MS, ends=False); wall(S, 'x', CX1, CZ0 + 0.1, CZ1, 0.0, YW, t=0.2, ms=(PL, CONC), ends=False)
wall(S, 'x', KX0, 52.1, CZ0 - 0.1, 0.0, YW, t=0.2, ms=MS, ends=False); wall(S, 'x', KX1, 52.1, CZ0 - 0.1, 0.0, YW, t=0.2, ms=(PL, CONC), ends=False)
S.box(CX0 - 0.1, CX1 + 0.1, YW, YW, CZ0 - 0.1, CZ1, CONC, sides=('+y',)); S.box(KX0 - 0.1, KX1 + 0.1, YW, YW, 52.1, CZ0 - 0.1, CONC, sides=('+y',))
plate(S, CX0 + 0.1, CX1 - 0.1, CZ0 + 0.1, CZ1, CH, CH, [], None, OFF); plate(S, KX0 + 0.1, KX1 - 0.1, 52.1, CZ0 + 0.1, CH, CH, [], None, OFF)
# the pier's concrete wall lined (z 67.9 and the end at z 52.1), open at the doors
DO = sorted((a + OUT_U) for a in GATE.values()); RD = sorted(a + 14.0 for g, a in GATE.items() if g != 13)                         # (the ramp rooms' staff doors stay, alarmed)
ho = [(c - 1.66, c + 1.66, 3.16) for c in DO] + [(c - 1.15, c + 1.15, 2.4) for c in RD]
cu = sorted({CX0 + 0.1, CX1 - 0.1} | {v for h in ho for v in h[:2]})
for u, v in zip(cu, cu[1:]):
    h = next((q[2] for q in ho if abs(q[0] - u) < 1e-6), 0.0)
    S.box(u, v, h or F1T, CH, CZ1 - 0.04, CZ1, PL, sides=('-z', '-x', '+x') + (('-y',) if h else ()))
    if not h: S.box(u, v, F1T, 0.12, CZ1 - 0.052, CZ1 - 0.04, STEEL, sides=('+y', '-z', '-x', '+x')); S.box(u + 0.02, v - 0.02, 0.95, 1.09, CZ1 - 0.075, CZ1 - 0.04, OAK, sides=('+y', '-y', '-z', '-x', '+x'))
for u, v in ((KX0 + 0.1, PC_X - PC_HALF), (PC_X + PC_HALF, KX1 - 0.1)): S.box(u, v, F1T, CH, 52.1, 52.14, PL, sides=('+z', '-x', '+x'))
S.box(PC_X - PC_HALF, PC_X + PC_HALF, 3.16, CH, 52.1, 52.14, PL, sides=('+z', '-y'))
for u, v in ((CX0 + 0.12, KX0), (KX1, CX1 - 0.12)): S.box(u, v, 0.95, 1.09, CZ0 + 0.1, CZ0 + 0.135, OAK, sides=('+y', '-y', '+z', '-x', '+x'))             # oak rails along the walls
for x, s in ((KX0 + 0.1, 1), (KX1 - 0.1, -1)): a, b = sorted((x, x + s * 0.035)); S.box(a, b, 0.95, 1.09, 52.2, CZ0 - 0.05, OAK)
# LED lines in the ceiling's edges
for z0, z1 in ((CZ0 + 0.16, CZ0 + 0.2), (CZ1 - 0.2, CZ1 - 0.16)): S.box(CX0 + 0.3, CX1 - 0.3, CH - 0.02, CH - 0.001, z0, z1, LED, sides=('-y', '-z', '+z', '-x', '+x'))
for x0, x1 in ((KX0 + 0.16, KX0 + 0.2), (KX1 - 0.2, KX1 - 0.16)): S.box(x0, x1, CH - 0.02, CH - 0.001, 52.4, CZ0, LED, sides=('-y', '-x', '+x', '-z', '+z'))
# dark stone inlays across the floor every 9 m, a fine steel line along the middle (the way)
x = CX0 + 4.0
while x < CX1 - 2:
    if not KX0 - 0.5 < x < KX1 + 0.5: S.box(x, x + 0.3, F1T, F1T + 0.003, CZ0 + 0.3, CZ1 - 0.3, 'MI_T_marbleDark', sides=('+y',))
    x += 9.0
# the passport hall's side: an oak portal
zh = 51.9
for a, b in ((PC_X - 3.1, PC_X - 2.66), (PC_X + 2.66, PC_X + 3.1)): S.box(a, b, 0.01, 3.6, zh - 0.1, zh, OAK, sides=('-x', '+x', '-z', '+y'))
S.box(PC_X - 2.66, PC_X + 2.66, 3.17, 3.6, zh - 0.1, zh, OAK, sides=('-y', '-z', '+y'))
for a, b in ((PC_X - 3.1, PC_X - 2.66), (PC_X + 2.66, PC_X + 3.1)): S.box(a, b, F1T, 3.6, 52.14, 52.2, OAK, sides=('-x', '+x', '+z', '+y'))
S.box(PC_X - 2.66, PC_X + 2.66, 3.17, 3.6, 52.14, 52.2, OAK, sides=('-y', '+z', '+y'))
# the signs
band('+z', 52.2, PC_X - 2.6, PC_X + 2.6, 3.25, 3.55, 'PASSPORT CONTROL', 0.22, AMBER, d=0.04); band('-z', zh - 0.1, PC_X - 2.6, PC_X + 2.6, 3.25, 3.55, 'NO ENTRY', 0.22, RED, d=0.04)
band('+z', CZ0 + 0.1, KX0 + 0.1 - 2.4, KX0 - 0.1, 2.3, 2.75, 'PASSPORT CONTROL  →', 0.2, AMBER, d=0.05); band('+z', CZ0 + 0.1, KX1 + 0.1, KX1 + 2.3, 2.3, 2.75, '←  PASSPORT CONTROL', 0.2, AMBER, d=0.05)
for x, n_ in [(x, '-x') for x in (306.0, 324.0)] + [(x, '+x') for x in (362.0, 388.0, 414.0, 440.0, 466.0, 492.0, 514.0)]:            # hanging across the corridor, read as you walk
    s = 1 if n_ == '+x' else -1; SG.box(x - 0.04, x + 0.04, 3.1, 3.5, 64.1, 67.2, BLK); text(SG, 'PASSPORT CONTROL  ↑  BAGGAGE CLAIM', x + s * 0.04, 3.3, 65.65, n_, 0.19, AMBER, maxw=2.9)
    for z in (64.5, 66.8): SG.box(x - 0.015, x + 0.015, 3.5, CH, z - 0.015, z + 0.015, STEEL)
for g, a in GATE.items():
    c = a + OUT_U; band('-z', CZ1 - 0.04, c - 1.6, c + 1.6, 3.3, 3.62, 'GATE %d  ·  NO ENTRY' % g, 0.18, RED, d=0.04)
for c in RD: band('-z', CZ1 - 0.04, c - 1.0, c + 1.0, 2.55, 2.8, 'STAFF  ·  ALARMED', 0.13, GREEN, d=0.03)

# ======================================================================================================== the switching signs (ASwingGate shows one set)
def bands():
    for c, a, b in DB: SG.box(c - 3.2, c + 3.2, 9.7, 10.35, 67.75, 67.83, BLK)          # over the doors B, on the walkway
    SG.box(DC - 1.3, DC + 1.3, 9.05, 9.65, 67.75, 67.83, BLK); SG.box(SX1 - 0.11, SX1 - 0.03, 9.3, 9.95, DF - 3.0, DF + 3.0, BLK); SG.box(DC - 1.3, DC + 1.3, 9.05, 9.65, 68.12, 68.18, BLK)
bands()
for tag, b_, bm, c_, cm, f_, fm, in_ in (('non', 'GATE 13  ·  NON-SCHENGEN', AMBER, 'NO ENTRY', RED, 'GATE 13  ·  CLOSED THIS WAY', RED, 'NO ENTRY'),
                                         ('sch', 'GATE 13  ·  CLOSED THIS WAY', RED, 'GATE 13  ·  SCHENGEN', AMBER, 'GATE 13  ·  SCHENGEN', AMBER, 'TO ALL GATES 1-12')):
    P = piece('east_a_swing_' + tag); go, no = (GREEN, RED)
    lb, lc = (go, no) if tag == 'non' else (no, go)
    for c, a, b in DB:
        text(P, b_, c, 10.02, 67.75, '-z', 0.34, bm, maxw=6.1)
        for z0, z1 in ((67.78, 67.83), (68.17, 68.2)): P.box(c - 2.5, c + 2.5, 9.23, 9.29, z0, z1, lb)          # the lock's light bar
    text(P, c_, DC, 9.35, 67.75, '-z', 0.24, cm, maxw=2.4); text(P, in_, DC, 9.35, 68.18, '+z', 0.22, RED if tag == 'non' else WHITE, maxw=2.4)
    P.box(DC - 1.1, DC + 1.1, F2 + 2.9, F2 + 2.94, 67.8, 67.85, lc); P.box(DC - 1.1, DC + 1.1, F2 + 2.9, F2 + 2.94, 68.08, 68.12, lc)
    text(P, f_, SX1 - 0.11, 9.62, DF, '-x', 0.34, fm, maxw=5.7); P.box(SX1 - 0.07, SX1 - 0.03, 9.19, 9.25, DF - 2.5, DF + 2.5, lc)

# ======================================================================================================== out
BINX = [330.0, 372.0, 425.0, 480.0]
def split(o):
    bm = bmesh.new(); bm.from_mesh(o.data)
    for x in BINX: bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(-x, 0, 0), plane_no=(1, 0, 0), dist=1e-5)
    grp = {}; bm.faces.ensure_lookup_table(); bm.faces.index_update()
    for f in bm.faces: grp.setdefault(sum(-f.calc_center_median().x > b for b in BINX) + 1, []).append(f.index)
    out = []
    for n, idx in sorted(grp.items()):
        c = bm.copy(); c.faces.ensure_lookup_table(); c.faces.index_update(); keep = set(idx)
        bmesh.ops.delete(c, geom=[f for f in c.faces if f.index not in keep], context='FACES')
        used = sorted({f.material_index for f in c.faces}); mp = {u: k for k, u in enumerate(used)}
        for f in c.faces: f.material_index = mp[f.material_index]
        me = bpy.data.meshes.new('%s_%d' % (o.name, n)); c.to_mesh(me); c.free()
        for u in used: me.materials.append(o.data.materials[u])
        q = bpy.data.objects.new(me.name, me); bpy.context.scene.collection.objects.link(q); out.append(q)
    bm.free(); bpy.data.objects.remove(o); return out
for f_ in glob.glob(os.path.join(HERE, 'east_a_*.fbx')): os.remove(f_)
once = []
for P_ in ALL:
    o = P_.finish(); nm = o.name; objs = split(o) if nm == 'east_a_corr' else [o]
    if nm in ('east_a_swing', 'east_a_swing_glass', 'east_a_signs') or len(objs) > 1: once += [q.name for q in objs]
    for o in objs:
        assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
        print('ARR', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
        for x in bpy.context.selected_objects: x.select_set(False)
        o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
json.dump({'once': once}, open(os.path.join(HERE, 'arr_place.json'), 'w'))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Arrivals.blend'))
print('ARR saved')
