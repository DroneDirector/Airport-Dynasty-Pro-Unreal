# The terminal's east side, part 1 of 5: THE SHELL (to-do 213, owner 2026-11-23; blueprint v2 approved:
# docs/Blueprints/Terminal-Changes/v2, geometry Blueprints/Terminal/Changes/v2/v2_common.py) - x 156..551.2, z 0..52 under
# the existing wave roof (open grass until now), and the double-skin front x 114..156 with the link gallery x 107..162.
# In the terminal plan's metres and the terminal's own materials; every new part in Art/TerminalEast/:
#   east_solid_*, east_glass_*   floor (M_Marble, laid by world position: lines up with the Grand Hall's), the front at z 0 up
#                                to the wave roof (the Grand Hall's own mullions 8 x 14 cm and glass, MI_T_glassOuter), the
#                                second skin and the link gallery, the east end wall, the flat roof z 34..52 with the glass
#                                band under the wave's back edge, the partitions of sheets 2c/2d with their doorways, the
#                                ceilings (oak slats and LED lines as the west claim hall's), the Floor 2 slab x 228..551,
#                                z 20..52 at +6.01 (marble, its edge finished, one opening for the transfer stair / lift /
#                                escalators x 326..338, z 40..51) - parts 2-5 build on it, nothing to tear out
#   east_f2_plug                 the lid on that opening until the transfer stair is built (drop its node then)
#   east_f2_rail(_glass)         the Floor 2 edge's balustrade (z 20), in a piece of its own: part 4 opens it at the escape stairs
#                                (sheet 9: their places may still move)
#   east_pave                    the paving from the pavement's edge (z -8.2, 18 cm up) down to the doors (z -4.2), along the whole
#                                terminal: the arrivals wing, the escalator room, the Grand Hall and the east side (x -120..557)
#   east_prom_wall               the promenade's wall (z 6, x 228..470), in a piece of its own for the same reason: the escape
#                                stairs' doorways in it (STAIR_DOORS below, part 4: Art/TerminalEast/esc_geom.py)
#   east_col_g / _f2 / _hall     the round stone columns (instanced)
#   east_vest12_* / east_vest10_* the entrance vestibules (12 and 10 m; instanced), the main entrance's 5 m sliding doors
# The wave roof's underside: Tools/wave_roof.py (the roof's own numbers - run this again if the roof changes): the glass and
# the walls reach 5 cm into the roof along its curve (no gap).
# Everything is written down as boxes too (Saved/east-boxes.json) for Tools/check_east.py (the overlap check), and where the
# instanced parts, doors, canopies and lamps go (Art/TerminalEast/place.json, read by Tools/terminal_east_edit.py).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalEast/make_terminal_east.py
#   then Tools/import_east.py (in Unreal) and Tools/terminal_edit.py
# Blender axes for the game's import: (x, y, z) = (-plan x, -plan z, plan y).
import bpy, bmesh, json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(ROOT, 'Art', 'TerminalAnnex', 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])     # (Piece: one set of rules for every new part)
BOXES = []; TAG = ['']
_box = Piece.box
def box_rec(self, x0, x1, y0, y1, z0, z1, m, sides='all', uvs=(1.0, 1.0)):
    BOXES.append([self.name, TAG[0], round(x0, 3), round(x1, 3), round(y0, 3), round(y1, 3), round(z0, 3), round(z1, 3), m])
    _box(self, x0, x1, y0, y1, z0, z1, m, sides, uvs)
Piece.box = box_rec
S = Piece('east_solid')
GF = [Piece('east_glass_front_w'), Piece('east_glass_front_e')]; GS = Piece('east_glass_skin'); GI = Piece('east_glass_inner')
W, T = 'MI_T_white', 0.2
exec(src[src.index('def wall_x('):src.index('def glass_front_z')])
GL, MU, PL, MARBLE, OAK, CEDAR, BASALT, STONE = 'MI_T_glassOuter', 'MI_T_mullion', 'MI_T_plaster', 'M_Marble', 'MI_T_oak', 'MI_T_cedar', 'MI_T_basalt', 'M_PillarStone'
LED, OFF = 'MI_T_l_FFFFFF', 'MI_T_offwhite'

# ---- the numbers (v2_common.py) ----
X0, X1, ZN = 156.0, 551.2, 51.9                       # the office block's east wall, the pier's end, the gate walkway's south wall face
F2X0, F2Z0 = 228.0, 20.0                              # Floor 2's slab
CEIL, F2, RC, ROOF = 5.4, 6.01, 11.4, 12.0
ZB = 33.9                                             # the glass band under the wave's back edge (it ends at z 34)
BAY = 1.81                                            # the Grand Hall's own mullion spacing (its front: 1.75..1.92 below, 1.814 above +6)
TRANSOM = 5.4
GH_ENTR = ((188, 200), (290, 300), (396, 406), (514, 526))       # the four new entrances (v2.1: the first 2 m, the last 10 m east of v2, clear of the kerb pillars x 184 and 506; the last also clear of the escape stair x 516)
LINK_X0, LINK_X1 = 107.0, 162.0                       # the link gallery; it runs 6 m past the corner so it can turn in through the front
SK = -3.91                                            # the second skin's plane: the vestibules' own front (z -3.98..-3.84)
sys.path.insert(0, HERE); import esc_geom as ESC        # (part 4, the escape stairs: their numbers)
STAIR_DOORS = ESC.promenade_doors()                   # the escape stairs' doorways in the promenade wall (x 246 + 30 k up to 456: 1.8 m)
TRANSFER = (326.0, 338.0, 40.0, 51.0)

# ---- the wave roof's underside: the roof's own numbers (Tools/wave_roof.py, the one smooth wave of 2026-11-23) ----
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import wave_roof as WR
def soffit(x, z): return WR.height(x, z)
IN = 0.05                                             # how far the glass and walls reach into the roof

def put(P, pts, m, n, uvs=(1.0, 1.0)):
    """One face (plan points) that must face the plan direction n."""
    f = P.face(pts, m, uvs); P.want.append((f, (-n[0], -n[2], n[1])))
def rng(a, b, step):
    n = max(1, int(math.ceil((b - a) / step - 1e-6))); return [a + (b - a) * i / n for i in range(n + 1)]
def rec(P, x0, x1, y0, y1, z0, z1, m): BOXES.append([P.name, TAG[0], round(x0, 3), round(x1, 3), round(y0, 3), round(y1, 3), round(z0, 3), round(z1, 3), m])

def pane(P, put2, u0, u1, y0, top, m, step):
    """A pane's faces along u: a plain sheet per 1.8 m up to the roof's lowest point there, above it finely stepped along the
    curve (so the map's cut at walking height is a few long lines, not thousands of short ones)."""
    for a, b in (lambda r: zip(r, r[1:]))(rng(u0, u1, 1.8)):
        us = rng(a, b, step); ym = min(top(u) for u in us) - 0.1
        if ym <= y0 + 0.2: ym = y0
        else: put2(a, b, y0, y0, ym, ym)
        for c, d in zip(us, us[1:]): put2(c, d, ym, ym, top(c), top(d))
def pane_z(P, z, x0, x1, y0, top, m=GL, t=0.015):
    """A pane (or wall) in the plane z from x0 to x1, y0 up to top(x)."""
    def q(a, b, ya, yb, yc, yd):
        put(P, [(a, ya, z - t), (b, yb, z - t), (b, yd, z - t), (a, yc, z - t)], m, (0, 0, -1)); put(P, [(a, ya, z + t), (b, yb, z + t), (b, yd, z + t), (a, yc, z + t)], m, (0, 0, 1))
    pane(P, q, x0, x1, y0, top, m, 0.2); rec(P, x0, x1, y0, max(top(x) for x in rng(x0, x1, 0.2)), z - t, z + t, m)
def pane_x(P, x, z0, z1, y0, top, m=GL, t=0.015):
    def q(a, b, ya, yb, yc, yd):
        put(P, [(x - t, ya, a), (x - t, yb, b), (x - t, yd, b), (x - t, yc, a)], m, (-1, 0, 0)); put(P, [(x + t, ya, a), (x + t, yb, b), (x + t, yd, b), (x + t, yc, a)], m, (1, 0, 0))
    pane(P, q, z0, z1, y0, top, m, 0.25); rec(P, x - t, x + t, y0, max(top(z) for z in rng(z0, z1, 0.25)), z0, z1, m)
def mull_z(z, x, y0, top, h=0.04, d=0.07):            # a mullion 8 x 14 cm, its head cut along the roof
    xa, xb = x - h, x + h; ya, yb = top(xa), top(xb)
    for s in (-1, 1):
        zz = z + s * d; put(S, [(xa, y0, zz), (xb, y0, zz), (xb, yb, zz), (xa, ya, zz)], MU, (0, 0, s))
    put(S, [(xa, y0, z - d), (xa, y0, z + d), (xa, ya, z + d), (xa, ya, z - d)], MU, (-1, 0, 0))
    put(S, [(xb, y0, z - d), (xb, y0, z + d), (xb, yb, z + d), (xb, yb, z - d)], MU, (1, 0, 0))
    rec(S, xa, xb, y0, max(ya, yb), z - d, z + d, MU)
def mull_x(x, z, y0, top, h=0.04, d=0.07):
    za, zb = z - h, z + h; ya, yb = top(za), top(zb)
    for s in (-1, 1):
        xx = x + s * d; put(S, [(xx, y0, za), (xx, y0, zb), (xx, yb, zb), (xx, ya, za)], MU, (s, 0, 0))
    put(S, [(x - d, y0, za), (x + d, y0, za), (x + d, ya, za), (x - d, ya, za)], MU, (0, 0, -1))
    put(S, [(x - d, y0, zb), (x + d, y0, zb), (x + d, yb, zb), (x - d, yb, zb)], MU, (0, 0, 1))
    rec(S, x - d, x + d, y0, max(ya, yb), za, zb, MU)
def bays(a, b):
    n = max(1, round((b - a) / BAY)); return [a + (b - a) * i / n for i in range(n + 1)]

def front_z(G, z, x0, x1, holes=()):
    """The Grand Hall's glass front in the plane z, floor to the wave roof; holes (a, b, head) at the floor: glass from the
    head up, a mullion each side, the bays between them even as the Grand Hall's."""
    top = lambda x: soffit(x, z) + IN
    ed = sorted({x0, x1} | {h[0] for h in holes} | {h[1] for h in holes}); done = set()
    for a, b in zip(ed, ed[1:]):
        hd = next((h[2] for h in holes if h[0] <= a and b <= h[1]), None)
        y0 = 0.01 if hd is None else hd
        xm = [a] + [q for q in (354.0,) if a < q < b] + [b]
        for p, q in zip(xm, xm[1:]): pane_z(G(0.5 * (p + q)), z, p, q, y0, top)
        for k, x in enumerate(bays(a, b)):
            xe = min(max(x, x0 + 0.046), x1 - 0.046); key = round(xe, 2)
            if key in done: continue
            edge = any(abs(x - q) < 1e-3 for h in holes for q in h[:2])
            done.add(key); mull_z(z, xe, 0.01 if hd is None or edge else y0, top)
        e0, e1 = max(a, x0 + 0.01), min(b, x1 - 0.01)       # (4 mm back from the mullions' faces: nothing on one plane)
        S.box(e0, e1, TRANSOM - 0.04, TRANSOM + 0.04, z - 0.066, z + 0.066, MU)
        if hd is None: S.box(e0, e1, 0.01, 0.06, z - 0.066, z + 0.066, MU)
        else: S.box(e0, e1, hd - 0.06, hd, z - 0.066, z + 0.066, MU)

# ======================================================================================================== the floor
TAG[0] = 'floor'
S.box(X0, X1 + 0.1, -0.02, 0.01, -0.1, ZN + 0.05, MARBLE, sides=('+y',))
S.box(X0, X1 + 0.1, -0.35, 0.01, -0.1, ZN + 0.05, 'MI_T_concrete', sides=('-z', '+x'))         # its edge, down into the ground
# ---- the paving between the road network's pavement (it ends 8.2 m out along the terminal, 18 cm up: Tools/make_roads.py) and
#      the doors: the pavement's own material, sloping down 4 % to the floors' level at the vestibules and the arrivals wing's
#      front (z -4.2), level from there; at both ends (x -85.0..-80.6, 551.4..555) the slope runs out so it meets the full
#      pavement with no step. (2026-11-23: along the Grand Hall and the arrivals wing too - the pavement used to stand 18 cm over
#      their floors and lay over the vestibules', the mats and the arrivals hall's first 4 m; west of the wing, x -120.4..-85,
#      it only carries the pavement on at its own height, where the stands narrow away)
PV = Piece('east_pave'); GW = 'MI_gwalk'; TOPP, LOW = 0.1975, 0.005
def sm1(t): return min(1.0, max(0.0, t))
def xf(x): return min(sm1((x + 85.0) / 4.4), sm1((555.0 - x) / 3.6))
# (in front of Entrance West's and East's revolving doors, x 50 and 101: their drums stand out to z -5.6, so the paving is level
#  from z -5.7 there and its slope, the same 4.4 %, starts at z -9.8 - the pavement ends 9.8 m out between x 42 and 109)
DRUMS = (50.0, 101.0)
def dip(x):
    t = max(sm1((4.9 - abs(x - c)) / 3.0) for c in DRUMS); return t * t * (3 - 2 * t)
def pave(x, z):
    d = dip(x); y = TOPP - (TOPP - LOW) * xf(x) * (sm1((z + 8.2) / 4.0) * (1 - d) + sm1((z + 9.8) / 4.1) * d)
    return RIM + (y - RIM) * rim(x, z)
# (it runs 55 cm over the pavement's edge - the road's kerb stone there tops out 5 mm over the pavement, at 19.5 cm, and showed as a
#  line - and its rim dives under the pavement (19 cm, measured in the game) in its last 35 cm: no lip, no line)
RIM = 0.186
def rim(x, z):
    z0 = -10.55 if 41.04 < x < 110.36 else -8.95
    e = min((x + 121.1) / 0.35, (557.7 - x) / 0.35, (z - z0) / 0.35)
    if z < -8.59: e = max(min(e, 1.0) if not 41.04 < x < 110.36 else -1, min((x - 41.05) / 0.35, (110.35 - x) / 0.35, (z + 10.55) / 0.35), min((z + 8.95) / 0.35, (x + 121.1) / 0.35, (557.7 - x) / 0.35))
    return sm1(e)
def pq(xa, xb, za, zb):
    q = [(xa, pave(xa, za), za), (xb, pave(xb, za), za), (xb, pave(xb, zb), zb), (xa, pave(xa, zb), zb)]
    f = PV.face(q, GW); PV.want.append((f, (0, 0, 1)))
    for l, (x, y, z) in zip(f.loops, q): l[PV.uv].uv = (x / 10.0, z / 10.0)
    rec(PV, xa, xb, min(v[1] for v in q), max(v[1] for v in q), za, zb, GW)
WEST_END = -121.1
_xs = [WEST_END, -120.75, -110.0, -100.0, -92.0] + [-85.0 + 0.55 * i for i in range(9)] + [-80.2] + [-70.0 + 10.0 * i for i in range(7)] + [0.0] + [10.0 * i for i in range(1, 12)] + [113.9 + 10.0 * i for i in range(44)] + [41.05, 41.4, 110.0, 110.35] + [c + 0.5 * i for c in DRUMS for i in range(-10, 11)] + [551.3, 551.4] + [551.4 + 0.45 * i for i in range(1, 9)] + [557.35, 557.7]
_xs = sorted({round(v, 3) for v in _xs}); _zs = [-8.95, -8.6, -8.2, -7.2, -6.2, -5.2, -4.2]
def z_back(xa, xb):                                    # how far in: past the building's ends 10 cm past its line; to the arrivals wing's floor (z -4); to the front's floors
    return 0.1 if xb <= -80.19 or xb > 551.31 else -3.96 if xb <= 0.0 else 0.04 if xb <= 110.0 else -0.1
for xa, xb in zip(_xs, _xs[1:]):
    zz = _zs + [z_back(xa, xb)]
    if 41.04 <= xa and xb <= 110.36: zz = [-10.55, -10.2, -9.8, -9.3, -8.95] + _zs[1:5] + [-5.7] + _zs[5:] + [z_back(xa, xb)]
    for za, zb in zip(zz, zz[1:]): pq(xa, xb, za, zb)
for xa, xb in zip(_xs, _xs[1:]):                       # (its free edges beside the building's two ends, down into the ground)
    if z_back(xa, xb) == 0.1: put(PV, [(xa, -0.35, 0.1), (xb, -0.35, 0.1), (xb, pave(xb, 0.1), 0.1), (xa, pave(xa, 0.1), 0.1)], GW, (0, 0, 1))
# the link gallery's floor: level with the East Hall's and Entrance East's (its 4 % ramp up to the pavement's old height there
# is gone with the step, 2026-11-23)
LY = 0.011
for xa, xb, ze in ((107.0, 114.0, 0.0), (114.0, X0, -0.08), (X0, LINK_X1, -0.1)):          # (to the Grand Hall's floor, the bag block's wall, the East Hall's floor)
    put(S, [(xa, LY, -4.0), (xb, LY, -4.0), (xb, LY, ze), (xa, LY, ze)], MARBLE, (0, 1, 0)); rec(S, xa, xb, -0.02, LY, -4.0, ze, MARBLE)
S.box(LINK_X0, LINK_X1, -0.35, 0.01, -4.0, -0.1, 'MI_T_concrete', sides=('-z', '+x'))
# ======================================================================================================== the front (z 0)
TAG[0] = 'front'
HOLES = [(X0 + 0.16, LINK_X1 - 0.16, 3.2)]           # the link gallery's way in
for a, b in GH_ENTR:
    c = (a + b) / 2; HOLES.append((c - 2.6, c + 2.6, 3.2))
front_z(lambda x: GF[0] if x < 354.0 else GF[1], 0.0, X0, X1, HOLES)
# ======================================================================================================== the double skin (x 114..156)
TAG[0] = 'skin'
front_z(lambda x: GS, SK, 114.0, X0, ())
for xx in (114.02, X0):                               # its ends, over the gallery's roof, up to the wave
    te = lambda z, xx=xx: soffit(xx, z) + IN
    pane_x(GS, xx, SK + 0.07, -0.1 if xx > 150 else -0.07, 3.8, te); mull_x(xx, SK / 2, 3.8, te)
# ---- the link gallery: its roof (the vestibules': white 3.4..3.8, off-white ceiling 3.36), its own front x 107..114, 156..162
TAG[0] = 'link'
for a, b, z0 in ((LINK_X0, 114.0, -4.0), (114.0, X0, SK + 0.07), (X0, LINK_X1, -4.0)):
    S.box(a, b, 3.4, 3.8, z0, -0.08, W, sides=('+y', '-y') + (('-z',) if z0 == -4.0 else ()) + (('+x',) if b == LINK_X1 else ()))
    S.box(a, b, 3.36, 3.4, z0 + (0.02 if z0 == -4.0 else 0.0), -0.08, OFF, sides=('-y',))
for a, b in ((LINK_X0, 114.0), (X0, LINK_X1)):
    GS.box(a + 0.04, b, 0.01, 3.4, SK - 0.015, SK + 0.015, GL, sides=('-z', '+z'))
    n = max(1, round((b - a) / 1.5)); st = (b - a) / n
    for i in range(n + 1):
        x = a + i * st
        if min(abs(x - 114.0), abs(x - X0), abs(x - LINK_X0)) < 0.01: continue   # (the skin's own end mullions; Entrance East's)
        S.box(x - 0.04, x + 0.04, 0.01, 3.4, SK - 0.07, SK + 0.07, MU)
GS.box(LINK_X1 - 0.105, LINK_X1 - 0.075, 0.01, 3.4, SK, -0.07, GL, sides=('-x', '+x'))   # its east end (x 162)
for z in (-4.0, -2.0): S.box(LINK_X1 - 0.16, LINK_X1 - 0.045, 0.01, 3.4, z - 0.04, z + 0.04, MU)
# Entrance East's frame rebuilt without its middle post (it stood in the way through, x 107 z -2; its end pane goes too:
# Tools/terminal_east_edit.py drops F1-ENTR-E/w#459 and STRUCTURE-mullion)
for x in (95.0, 96.5, 98.0, 104.0, 105.5, 107.0): S.box(x - 0.04, x + 0.04, 0.0, 3.4, SK - 0.07, SK + 0.07, MU)
for z in (-4.0, -2.0, 0.0): S.box(95.02, 95.16, 0.0, 3.4, z - 0.04, z + 0.04, MU)
for z in (-4.0, 0.0): S.box(106.84, 106.975, 0.0, 3.4, z - 0.04, z + 0.04, MU)
# the wall side (the bag block's white facade at z -0.08, x 114..156): oak battens over a dark backing, an LED line over them
TAG[0] = 'link battens'
S.box(114.3, 155.7, 0.11, 3.2, -0.115, -0.105, 'MI_T_dark', sides=('-z',))
x = 114.3
while x + 0.06 <= 155.7:
    S.box(x, x + 0.06, 0.11, 3.2, -0.155, -0.11, OAK, sides=('-z', '-x', '+x')); x += 0.12
S.box(114.3, 155.7, 0.01, 0.11, -0.15, -0.08, 'MI_T_steel', sides=('-z', '+y')); S.box(114.3, 155.7, 3.2, 3.24, -0.15, -0.08, LED, sides=('-z', '-y'))
S.box(114.3, 155.7, 3.24, 3.36, -0.16, -0.08, W, sides=('-z', '-y'))
for a, b in ((114.2, 114.3), (155.7, 155.8)): S.box(a, b, 0.01, 3.36, -0.16, -0.08, OAK, sides=('-z', '-x', '+x'))
# ======================================================================================================== the east end wall (x 551.2)
TAG[0] = 'east end'
tz = lambda z: soffit(X1, z) + IN
pane_x(GF[1], X1, 0.0, ZB, 0.01, tz); pane_x(GF[1], X1, ZB, ZN, 0.01, lambda z: 11.62)
for z in bays(0.0, ZN):
    z = min(max(z, 0.04), ZN - 0.04); mull_x(X1, z, 0.01, tz if z <= ZB - 0.04 else (lambda q: 11.62))
for y0, y1 in ((0.01, 0.06), (TRANSOM - 0.04, TRANSOM + 0.04)): S.box(X1 - 0.066, X1 + 0.066, y0, y1, 0.071, ZN, MU)
S.box(X1 - 0.062, X1 + 0.062, CEIL + 0.04, F2, F2Z0, ZN, MU)                                # the slab's edge behind the glass
# ======================================================================================================== the west wall over the offices' roof (x 156, +12 up)
TAG[0] = 'west over the offices'
tw = lambda z: soffit(X0, z) + IN
pane_x(S, X0, 0.0, ZB + 0.08, 12.0, tw, m=W, t=0.1)
for z, n in ((0.0, -1), (ZB + 0.08, 1)):
    put(S, [(X0 - 0.1, 12.0, z), (X0 + 0.1, 12.0, z), (X0 + 0.1, tw(z), z), (X0 - 0.1, tw(z), z)], W, (0, 0, n))
# ======================================================================================================== the flat roof z 34..52 with the glass band
TAG[0] = 'flat roof'
RX0, RX1, RZ0 = X0 + 0.1, X1 + 0.15, 33.55
S.box(RX0, RX1, 11.98, ROOF, RZ0, ZN, 'MI_T_roofZinc', sides=('+y', '-z', '+x'))
S.box(RX0, RX1, 11.62, 11.98, RZ0, ZN, W, sides=('-y', '-z', '+x'))
S.box(RX1 - 0.12, RX1 + 0.02, 11.9, 12.12, ZB + 0.2, ZN, 'MI_T_black')                      # the east edge's coping
TAG[0] = 'glass band'                                                                       # glass on the roof's front edge up to the wave's underside
tb = lambda x: soffit(x, ZB) + IN
for a, b in ((RX0, 354.0), (354.0, X1)): pane_z(GF[0] if a < 300 else GF[1], ZB, a, b, ROOF, tb)
for x in bays(RX0, X1): mull_z(ZB, min(max(x, RX0 + 0.04), X1 - 0.04), ROOF, tb, d=0.05)
S.box(RX0, X1, ROOF, ROOF + 0.06, ZB - 0.046, ZB + 0.046, MU)
TAG[0] = 'roof ceiling'                                                                     # oak slats over a dark plenum, an LED line every 3 m
COLS_F2 = [257.0, 274.0, 291.0, 306.0, 324.0, 341.0, 356.0, 370.0, 384.5, 399.0, 425.0, 448.0, 471.0, 496.0, 521.0, 536.5]
COLS_HALL = [169.0, 187.0, 219.2]
ZC = 34.4
def minus(sp, a, b): return [s for p, q in sp for s in ((p, min(q, a)), (max(p, b), q)) if s[1] - s[0] > 0.05]
def slats(x0, x1, z0, z1, yb, cols=(), w=0.06, pitch=0.12, plenum=0.3, holes=()):
    z = z0 + 0.05
    while z + w <= z1:
        sp = [(x0, x1)]
        for cx, cz, r in cols:
            if abs(z + w / 2 - cz) < r: sp = minus(sp, cx - r, cx + r)
        for h in holes:
            if h[2] < z + w and z < h[3]: sp = minus(sp, h[0], h[1])
        for a, b in sp: S.box(a, b, yb - 0.12, yb, z, z + w, OAK, sides=('-y', '-z', '+z', '+x', '-x'))
        z += pitch
    zs = sorted({z0, z1} | {h[2] for h in holes} | {h[3] for h in holes})
    for za, zb in zip(zs, zs[1:]):
        sp = [(x0, x1)]
        for h in holes:
            if h[2] <= za and zb <= h[3]: sp = minus(sp, h[0], h[1])
        for a, b in sp: S.box(a, b, yb + plenum, yb + plenum + 0.02, za, zb, 'MI_T_dark', sides=('-y',))
    z = z0 + 1.5
    while z < z1 - 0.3:
        sp = [(x0, x1)]
        for h in holes:
            if h[2] < z < h[3]: sp = minus(sp, h[0], h[1])
        for a, b in sp: S.box(a, b, yb - 0.135, yb - 0.121, z - 0.02, z + 0.02, LED, sides=('-y', '-z', '+z'))
        z += 3.0
C2 = [(x, ZC, 0.38) for x in COLS_F2] + [(x, ZC, 0.43) for x in COLS_HALL]
slats(X0 + 0.1, F2X0 - 0.08, 33.75, 39.9, RC, C2, w=0.1, pitch=0.2, plenum=0.14)
slats(F2X0 + 0.08, X1 - 0.08, 33.75, ZN, RC, C2, w=0.1, pitch=0.2, plenum=0.14)
S.box(X0 + 0.1, X1 - 0.08, 11.24, 11.62, RZ0, 33.75, W, sides=('-z', '-y', '+z')); S.box(X0 + 0.1, X1 - 0.08, 11.36, 11.4, RZ0 - 0.02, RZ0, LED, sides=('-z', '-y', '+y'))
# ======================================================================================================== Floor 2's slab (x 228..551.2, z 20..52)
TAG[0] = 'slab F2'
def plate(x0, x1, z0, z1, holes):
    xs = sorted({x0, x1} | {h[0] for h in holes} | {h[1] for h in holes}); zs = sorted({z0, z1} | {h[2] for h in holes} | {h[3] for h in holes})
    for xa, xb in zip(xs, xs[1:]):
        for za, zb in zip(zs, zs[1:]):
            if any(h[0] <= xa and xb <= h[1] and h[2] <= za and zb <= h[3] for h in holes): continue
            S.box(xa, xb, 5.99, F2, za, zb, MARBLE, sides=('+y',)); S.box(xa, xb, 5.75, 5.99, za, zb, 'MI_T_concrete', sides=('-y',))
plate(F2X0 + 0.1, X1 - 0.07, F2Z0, ZN + 0.05, [TRANSFER] + [(x - ESC.HW, x + ESC.HW, F2Z0, ESC.Z1) for x in ESC.STAIRS])   # (part 4: the stair boxes stand in it)
a, b, c, d = TRANSFER                                                                       # the opening's edges, white
for (xa, xb, za, zb, sd) in ((a - 0.02, a, c, d, '+x'), (b, b + 0.02, c, d, '-x'), (a, b, c - 0.02, c, '+z'), (a, b, d, d + 0.02, '-z')):
    S.box(xa, xb, 5.26, F2, za, zb, W, sides=(sd,))
for a, b in ESC.gaps(F2X0 - 0.1, X1 - 0.08):           # its south edge over the halls (z 20), stopping 2 cm off the stair boxes
    e = ('-x',) if a < F2X0 else ('-x', '+x') if b < X1 - 0.1 else ('-x',)
    S.box(a, b, 5.24, F2 + 0.02, F2Z0 - 0.06, F2Z0, W, sides=('-z', '-y', '+y') + e + (('+x',) if a < F2X0 else ()))
    S.box(max(a, F2X0), b, 5.5, 5.54, F2Z0 - 0.08, F2Z0 - 0.06, LED, sides=('-z', '-y', '+y') + (('-x', '+x') if a > F2X0 else ('+x',) if b < X1 - 0.1 else ()))
TAG[0] = 'balustrade'                                                                       # steel shoe, glass, black trim (the Grand Hall landing's)
def rail(P_, Q_, x0, x1, z=F2Z0 + 0.1):
    P_.box(x0, x1, F2, F2 + 0.12, z - 0.08, z + 0.08, 'MI_T_steel'); Q_.box(x0, x1, F2 + 0.1, F2 + 1.07, z - 0.006, z + 0.006, 'MI_T_glass', sides=('-z', '+z'))
    P_.box(x0, x1, F2 + 1.07, F2 + 1.115, z - 0.03, z + 0.03, 'MI_T_black'); P_.box(x0, x1, F2 + 1.115, F2 + 1.13, z - 0.02, z + 0.02, 'MI_T_black')
R = Piece('east_f2_rail'); RG = Piece('east_f2_rail_glass')
for a, b in ESC.gaps(F2X0 + 0.15, X1 - 0.1): rail(R, RG, a, b)                              # (part 4: open at the stair boxes, 2.64 m)
TAG[0] = 'x 228 glass'                                                                      # the East Hall's side: glass from the slab to the roof
XG = F2X0
tg = lambda z: soffit(XG, z) + IN
pane_x(GI, XG, F2Z0, RZ0, F2, tg); pane_x(GI, XG, 33.75, 40.0, F2, lambda z: 11.62); pane_x(GI, XG, RZ0, 33.75, F2, lambda z: 11.24)
for z in bays(F2Z0, 40.0):
    z = min(max(z, F2Z0 + 0.04), 39.96)
    if RZ0 - 0.1 < z < 33.85: z = 33.65
    mull_x(XG, z, F2, tg if z < 33.5 else (lambda q: 11.24) if z < 33.8 else (lambda q: 11.62))
S.box(XG - 0.066, XG + 0.066, F2, F2 + 0.06, F2Z0, 40.0, MU); S.box(XG - 0.066, XG + 0.066, RC - 0.04, RC + 0.04, F2Z0, RZ0 - 0.03, MU)
wall_x(XG, 40.1, ZN, F2, 11.62, m_in=PL)
# ======================================================================================================== the partitions, ground floor
TAG[0] = 'partitions'
PH = 5.75                                             # under the slab: up to it; in front (z < 20): to 5.4, capped
D = 2.25
def wx(x, z0, z1, holes=(), m=PL, hi=PH):
    for a, b, y in ((z0, min(z1, F2Z0), CEIL), (max(z0, F2Z0), z1, hi)):
        if b - a > 0.01: wall_x(x, a, b, 0.01, y, m_in=m, holes=[h for h in holes if h[0] >= a - 0.01 and h[1] <= b + 0.01])
def door(c, w=1.2, h=D): return (c - w / 2, c + w / 2, 0.0, h)
# z 40: the East Hall's back wall, full height to the flat roof's ceiling, with the rooms behind it
wall_z(40.0, X0 + 0.1, F2X0 + 0.1, 0.01, F2, m=PL, holes=[(156.4, 159.7, 0, 3.0), door(163.0), door(169.0), door(175.0), door(184.0, 1.6), (196.0, 204.0, 0, 3.0), (216.0, 222.0, 0, 3.0)])
wall_z(40.0, X0 + 0.1, F2X0 + 0.1, F2, 11.62, m=PL)
for x in (160.0, 178.0, 190.0, 210.0): wall_x(x, 40.1, ZN, 0.01, CEIL, m_in=PL)
# customs: x 228 and x 236, green z 24..32, red z 33.5..39; its office z 40..52; its closed end z 6..20
EX = [(24.0, 32.0, 0, 3.0), (33.5, 39.0, 0, 3.0)]
wx(228.0, 6.1, 39.9, EX, hi=F2); wx(228.0, 40.1, ZN, (), hi=F2); wx(236.0, 6.1, ZN, EX)
wall_z(40.0, 228.1, 235.9, 0.01, PH, m=PL, holes=[door(232.0)])
# z 6: the promenade's wall x 228..470 (a piece of its own), the staff rooms' doors
Z6 = [door(x, 1.8) for x in STAIR_DOORS] + [door(x, 1.8) for x in (388.0, 420.0, 450.0)]
PW = Piece('east_prom_wall'); _S = S; S = PW; wall_z(6.0, 227.9, 470.1, 0.01, CEIL, m=PL, holes=Z6); S = _S
# passport control in: x 312 (booths z 20.6..37.6, e-gates 38.8..51.5), x 324 the same; the border police office z 6..20
PCW = [(20.4, 37.6, 0, 3.4), (38.8, 51.5, 0, 3.4)]
wx(312.0, 6.1, ZN, PCW); wx(324.0, 6.1, ZN, PCW + [door(13.0)])
wall_z(19.8, 312.1, 323.9, 0.01, CEIL, m=PL, holes=[door(318.0)])
# the second-line checks' room
wall_z(41.0, 357.9, 371.9, 0.01, PH, m=PL, holes=[door(364.0)]); wall_x(358.0, 41.1, 50.9, 0.01, PH, m_in=PL); wall_z(51.0, 357.9, 371.9, 0.01, PH, m=PL)
# the staff rooms
wx(372.0, 6.1, ZN, [door(30.0, 1.6)]); wx(404.0, 6.1, ZN, [door(15.0, 1.6), door(38.0, 1.6)]); wx(436.0, 6.1, ZN, [door(15.0, 1.6), door(38.0, 1.6)])
for a, b in ((372.1, 403.9), (404.1, 435.9), (436.1, 469.9)): c = round((a + b) / 2); wall_z(26.0, a, b, 0.01, PH, m=PL, holes=[door(388.0 if a < 400 else 420.0 if a < 430 else 450.0, 1.6)])
wx(470.0, 6.1, ZN, ())
TAG[0] = 'linings'
# the coach station's west wall: the arrivals wing's dark stacked stone
S.box(470.1, 470.16, 0.01, CEIL, 5.9, F2Z0 - 0.06, BASALT, sides=('+x', '-z', '+y')); S.box(470.1, 470.16, 0.01, 5.24, F2Z0 - 0.06, ZN, BASALT, sides=('+x',))
TAG[0] = 'promenade wall'
S = PW
# the promenade's wall: cedar piers every 14 m, an oak band along its top, a steel skirting, an LED line under the band
def z6_spans(x0, x1, y0, e=0.08):
    sp = [(x0, x1)]
    for h in Z6:
        if h[3] > y0: sp = minus(sp, h[0] - e, h[1] + e)
    return sp
for k in range(17):
    for a, b in z6_spans(242.0 + 14 * k, min(244.0 + 14 * k, 470.0), 0.01, 0.1): S.box(a, b, 0.01, 4.0, 5.84, 5.9, CEDAR, sides=('-z', '-x', '+x'))
for k in range(17):                                    # (2026-11-23: a slim light on each pier - a black plate, a glowing slot: no real light of its own)
    c = 243.0 + 14 * k
    if c < 470.0 and not any(h[0] - 0.2 < c < h[1] + 0.2 for h in Z6): S.box(c - 0.09, c + 0.09, 1.5, 3.5, 5.815, 5.84, 'MI_T_black', sides=('-z', '-x', '+x', '-y', '+y')); S.box(c - 0.035, c + 0.035, 1.56, 3.44, 5.775, 5.815, LED, sides=('-z', '-x', '+x', '-y', '+y'))
S.box(227.9, 470.1, 4.0, CEIL, 5.82, 5.9, OAK, sides=('-z', '-y', '-x', '+x')); S.box(228.0, 470.1, 4.0, 4.03, 5.80, 5.82, LED, sides=('-z', '-y', '+y'))
for a, b in z6_spans(227.9, 470.1, 0.01): S.box(a, b, 0.01, 0.12, 5.885, 5.9, 'MI_T_steel', sides=('-z', '+y', '-x', '+x'))
S.box(227.9, 470.16, CEIL, CEIL + 0.04, 5.8, 6.1, W)                                        # its cap
S = _S; TAG[0] = 'linings'
for x in (228.0, 236.0, 312.0, 324.0, 372.0): S.box(x - 0.1, x + 0.1, CEIL, CEIL + 0.04, 6.1, F2Z0 - 0.06, W, sides=('+y', '-x', '+x'))
S.box(469.9, 470.16, CEIL, CEIL + 0.04, 6.1, F2Z0 - 0.06, W, sides=('+y', '-x', '+x'))
# the gate walkway's south wall (its glossy white outside, z 51.9) lined with the halls' own plaster where it is inside now (its
# gloss mirrored the lamps and dark rooms in blotches - 2026-11-23): 15 mm, a steel skirting
# holes (x0, x1, y0, y1) at the floor, as wall_z's: the lining stops at the jambs and goes on over the head, no skirting under it
def north(x0, x1, y0, y1, holes=()):
    ed = sorted({x0, x1} | {v for h in holes for v in h[:2] if x0 < v < x1})
    for a, b in zip(ed, ed[1:]):
        h = next((h for h in holes if h[0] <= a and b <= h[1]), None)
        if h is None: S.box(a, b, y0, y1, 51.885, 51.9, PL, sides=('-z',)); S.box(a, b, y0, y0 + 0.1, 51.87, 51.885, 'MI_T_steel', sides=('-z', '+y'))
        elif h[3] < y1: S.box(a, b, h[3], y1, 51.885, 51.9, PL, sides=('-z', '-y'))
# (terminal part 2b: the arrivals corridor's one-way doors into the passport hall, AR.f1_z52(); the lining stops at their oak portal,
# Art/TerminalEast/make_arrivals.py 'the passport hall's side': 44 cm jambs, its head at 3.6, standing on the wall face)
sys.path.insert(0, HERE); import arr_geom as AR
PORTAL = [(a - 0.44, b + 0.44, y0, 3.6) for a, b, y0, y1 in AR.f1_z52()]
for a, b in ((236.1, 311.9), (312.1, 323.9), (324.1, 371.9), (470.16, X1 - 0.08)): north(a, b, 0.01, CEIL, PORTAL if a == 324.1 else ())
# (Floor 2: opened onto the walkway, terminal part 2a - Art/TerminalEast/conc_geom.py)
sys.path.insert(0, HERE); import conc_geom as CG2
_ed = sorted({F2X0 + 0.1, X1 - 0.08} | {v for a, b, h in CG2.holes() for v in (a, b)})
for a, b in zip(_ed, _ed[1:]):
    _h = next((h for p, q, h in CG2.holes() if p <= a and b <= q), None)
    if _h is None: north(a, b, F2, RC)
    else: S.box(a, b, _h, RC, 51.885, 51.9, PL, sides=('-z', '-y'))          # (its edges: behind the openings' own frames)
# the East Hall's back wall: oak battens to 5.4 between the doorways over a dark backing, as the link gallery's
Z40 = [(156.1, 159.8, 3.1), (162.3, 163.7, 2.35), (168.3, 169.7, 2.35), (174.3, 175.7, 2.35), (183.1, 184.9, 2.35), (195.9, 204.1, 3.1), (215.9, 222.1, 3.1)]
def z40_bottom(x): return next((h for a, b, h in Z40 if a < x + 0.06 and x < b), 0.11)
x = 156.3
while x + 0.06 <= 227.8:
    S.box(x, x + 0.06, z40_bottom(x), CEIL, 39.845, 39.89, OAK, sides=('-z', '-x', '+x', '-y', '+y')); x += 0.12
ex = sorted({156.3, 227.8} | {max(156.3, v) for a, b, h in Z40 for v in (a, b)})
for a, b in zip(ex, ex[1:]):
    y = z40_bottom((a + b) / 2 - 0.03); S.box(a, b, y, CEIL, 39.885, 39.9, 'MI_T_dark', sides=('-z', '-y'))
    if y < 0.2: S.box(a, b, 0.01, 0.11, 39.85, 39.9, 'MI_T_steel', sides=('-z', '+y', '-x', '+x'))
S.box(156.3, 227.8, CEIL, CEIL + 0.04, 39.84, 39.9, LED, sides=('-z', '+y', '-y'))
# ======================================================================================================== the ceilings, ground floor
TAG[0] = 'ceilings'
COLS_G = ([(x, 20.6) for x in (249.0, 259.5, 270.0, 282.0, 290.0, 302.0, 331.0, 343.0, 355.0, 388.0, 420.0, 453.0, 483.0, 496.5, 510.0, 523.5, 537.0)]
          + [(x, ZC) for x in (247.6, 255.6, 266.2, 275.5, 284.8, 295.4, 303.8, 338.6, 359.5, 388.0, 420.0, 453.0, 483.0, 496.5, 510.0, 523.5, 537.0)])
CG = [(x, z, 0.38) for x, z in COLS_G]
SH = [(x - ESC.HW - 0.01, x + ESC.HW + 0.01, F2Z0, ESC.Z1 + 0.01) for x in ESC.STAIRS]      # (part 4: round the stair boxes)
slats(236.1, 311.9, F2Z0, ZN, CEIL, CG, holes=SH)                                                  # the claim hall
slats(312.1, 323.9, F2Z0, ZN, CEIL)                                                        # the booths
slats(324.1, 371.9, F2Z0, 40.9, CEIL, CG, holes=SH + [(TRANSFER[0] - 0.02, TRANSFER[1] + 0.02, TRANSFER[2] - 0.02, 40.9)])   # the passport hall
slats(324.1, 357.9, 40.9, ZN, CEIL, CG, holes=[(TRANSFER[0] - 0.02, TRANSFER[1] + 0.02, 40.9, TRANSFER[3] + 0.02)])
slats(470.16, X1 - 0.08, F2Z0, ZN, CEIL, CG, holes=SH)                                             # the coach station
def ceil(x0, x1, z0, z1):                              # (a second face at 5.40, as the terminal's own rooms: the lamps' check looks up from 5.38)
    S.box(x0, x1, 5.36, 5.4, z0, z1, OFF, sides=('-y',)); S.box(x0, x1, 5.4, 5.41, z0, z1, OFF, sides=('-y',))
for a, b in ((156.1, 159.9), (160.1, 177.9), (178.1, 189.9), (190.1, 209.9), (210.1, 227.9)): ceil(a, b, 40.1, ZN)
for r in ((228.1, 235.9, 6.1, 39.9), (228.1, 235.9, 40.1, ZN), (312.1, 323.9, 6.1, 19.7), (358.1, 371.9, 41.1, 50.9), (357.9, 371.9, 51.1, ZN),
          (372.1, 403.9, 6.1, 25.9), (404.1, 435.9, 6.1, 25.9), (436.1, 469.9, 6.1, 25.9), (372.1, 403.9, 26.1, ZN), (404.1, 435.9, 26.1, ZN), (436.1, 469.9, 26.1, ZN)):
    for q in ESC.cut(*r): ceil(*q)
# the lids of the rooms in front of the slab (z 6..20), seen from Floor 2's edge: white, level
for a, b in ((228.1, 235.9), (312.1, 323.9), (372.1, 469.9)):
    for q in ESC.cut(a, b, 6.1, F2Z0 - 0.06): S.box(q[0], q[1], 5.4, 5.44, q[2], q[3], W, sides=('+y',))
# ======================================================================================================== the separate pieces
def column(name, y0, y1, r=0.3):
    P = Piece(name); n = 32
    def rg(r0, r1, a, b, m):
        for k in range(n):
            a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n; am = (a0 + a1) / 2
            put(P, [(r0 * math.cos(a0), a, r0 * math.sin(a0)), (r0 * math.cos(a1), a, r0 * math.sin(a1)), (r1 * math.cos(a1), b, r1 * math.sin(a1)), (r1 * math.cos(a0), b, r1 * math.sin(a0))], m,
                (math.cos(am), 0, math.sin(am)) if abs(b - a) > 1e-4 else (0, 1, 0))
    rg(r + 0.06, r + 0.06, y0, y0 + 0.3, STONE); rg(r + 0.06, r, y0 + 0.3, y0 + 0.3, STONE); rg(r, r, y0 + 0.3, y1, STONE)
    for f in P.bm.faces: f.smooth = abs(f.calc_center_median().z - (y0 + 0.3)) > 1e-3
    bmesh.ops.remove_doubles(P.bm, verts=P.bm.verts, dist=1e-5)
    return P
PIECES = [S] + GF + [GS, GI]
PIECES += [column('east_col_g', 0.01, 5.75), column('east_col_f2', F2, 11.62), column('east_col_hall', 0.01, 11.62, 0.35)]
TAG[0] = 'plug'
P = Piece('east_f2_plug'); PIECES.append(P)            # (in the plan's metres: placed as it is)
a, b, c, d = TRANSFER
P.box(a, b, 5.99, F2, c, d, MARBLE, sides=('+y',)); P.box(a, b, 5.75, 5.99, c, d, 'MI_T_concrete', sides=('-y',)); P.box(a, b, CEIL - 0.02, CEIL, c, d, OFF, sides=('-y',))
PIECES += [R, RG, PW, PV]
for w in (12, 10):                                     # the vestibules, round their middle (x 0), z -4..0: as Entrance West/East's
    TAG[0] = 'vestibule'
    P = Piece('east_vest%d_solid' % w); Q = Piece('east_vest%d_glass' % w); PIECES += [P, Q]
    h = w / 2.0
    P.box(-h, h, -0.02, 0.012, -4.0, -0.1, MARBLE, sides=('+y',)); P.box(-h, h, -0.35, 0.012, -4.0, -0.1, 'MI_T_concrete', sides=('-z', '-x', '+x'))
    P.box(-h, h, 3.4, 3.8, -4.0, -0.07, W, sides=('+y', '-y', '-z', '-x', '+x')); P.box(-h, h, 3.36, 3.4, -3.98, -0.07, OFF, sides=('-y',))
    for a, b in ((-h, -2.6), (2.6, h)):
        Q.box(a, b, 0.0, 3.4, SK - 0.015, SK + 0.015, GL, sides=('-z', '+z'))
        n = max(1, round((b - a) / 1.75)); st = (b - a) / n
        for i in range(n + 1):
            x = a + i * st
            if abs(x) > 2.61: P.box(x - 0.04, x + 0.04, 0.0, 3.4, SK - 0.07, SK + 0.07, MU)     # (at the doors: their own frame)
    P.box(-2.6, 2.6, 3.14, 3.4, -3.97, -3.85, PL)
    for s in (-1, 1):
        Q.box(s * (h - 0.09) - 0.015, s * (h - 0.09) + 0.015, 0.0, 3.4, -3.9, -0.07, GL, sides=('-x', '+x'))
        for z in (-4.0, -2.0): P.box(s * (h - 0.0925) - 0.0675, s * (h - 0.0925) + 0.0675, 0.0, 3.4, z - 0.04, z + 0.04, MU)
# ======================================================================================================== where things go
def lamprows(x0, x1, zs, dx, m=2.7):                   # (row by row: every other one lights, so each row lights evenly)
    n = max(1, round((x1 - x0 - 2 * m) / dx) + 1)
    return [[round(x0 + m + (x1 - x0 - 2 * m) * i / (n - 1), 2), z] for z in zs for i in range(n)]
def lampgrid(x0, x1, z0, z1, dx, dz, m=2.5):
    nx = max(1, round((x1 - x0 - 2 * m) / dx) + 1); nz = max(1, round((z1 - z0 - 2 * m) / dz) + 1)
    xs = [(x0 + x1) / 2] if nx == 1 else [x0 + m + (x1 - x0 - 2 * m) * i / (nx - 1) for i in range(nx)]
    zs = [(z0 + z1) / 2] if nz == 1 else [z0 + m + (z1 - z0 - 2 * m) * i / (nz - 1) for i in range(nz)]
    return [[round(x, 2), round(z, 2)] for x in xs for z in zs]
LAMPS = {                                              # name: (floor node, y, places) - the terminal's own lamps (1.2 x 0.6 m)
    'F1-LINK': ('F1', 3.33, [[x, -2.0] for x in (110.5, 116, 122, 128, 134, 140, 146, 152, 159)]),
    'F1-EAST-WC': ('F1', 5.33, lampgrid(160, 178, 40, 52, 5, 5)), 'F1-EAST-FAMILY': ('F1', 5.33, lampgrid(178, 190, 40, 52, 5, 5)),
    'F1-EAST-TROLLEY': ('F1', 5.33, lampgrid(190, 210, 40, 52, 6, 5)), 'F1-EAST-BAGSERV': ('F1', 5.33, lampgrid(210, 228, 40, 52, 6, 5)), 'F1-EAST-LANE': ('F1', 5.33, [[158, 43], [158, 49]]),
    'F1-CUSTOMS-E': ('F1', 5.33, [[232, z] for z in (9, 14, 19, 25, 31, 36)]), 'F1-CUSTOMS-E-OFFICE': ('F1', 5.33, [[232, 43], [232, 49]]),
    'F1-CLAIM-E': ('F1', 5.22, lampgrid(236, 312, 20, 52, 8, 6, 4)), 'F1-PC-LINE': ('F1', 5.22, [[318, z] for z in (23, 29, 35, 41, 47)]), 'F1-PC-OFFICE': ('F1', 5.33, lampgrid(312, 324, 6, 20, 6, 6, 3)),
    'F1-PC-HALL': ('F1', 5.22, lampgrid(324, 372, 20, 40, 8, 6, 4) + [[342, 46], [350, 46]]), 'F1-PC-SECOND': ('F1', 5.33, [[361.5, 46], [367.5, 46]]),
    'F1-CREW-CENTRE': ('F1', 5.33, lampgrid(372, 404, 6, 26, 8, 7, 4)), 'F1-CANTEEN': ('F1', 5.33, lampgrid(404, 436, 6, 26, 8, 7, 4)), 'F1-GROUND-OFF': ('F1', 5.33, lampgrid(436, 470, 6, 26, 8, 7, 4)),
    'F1-LOCKERS': ('F1', 5.33, lampgrid(372, 404, 26, 52, 8, 8, 4)), 'F1-STAFFSEC': ('F1', 5.33, lampgrid(404, 436, 26, 52, 8, 8, 4)), 'F1-POLICE': ('F1', 5.33, lampgrid(436, 470, 26, 52, 8, 8, 4)),
    'F1-COACH-STATION': ('F1', 5.22, lampgrid(470, 551, 20, 52, 9, 7, 4)),
    'F1-EAST-HALL': ('F1', 11.22, lamprows(156, 228, (37.0,), 5.5)), 'F2-CONCOURSE': ('F2', 11.22, lamprows(228, 551, (39.5, 47.5), 5.4)),   # (2026-11-23: every 11 m one of three rows lit a wall spot every 22 m)
}
for _v in LAMPS.values():                              # (part 4: no lamp where a stair box stands)
    if _v[0] == 'F1': _v[2][:] = [p for p in _v[2] if not any(a < p[0] < b and c < p[1] < d for a, b, c, d in (ESC.rect(x, 0.75) for x in ESC.STAIRS))]
PLACE = {'cols': [['east_col_g', x, 0, z] for x, z in COLS_G] + [['east_col_f2', x, 0, ZC] for x in COLS_F2] + [['east_col_hall', x, 0, ZC] for x in COLS_HALL],
         'entrances': [[(a + b) / 2.0, b - a] for a, b in GH_ENTR], 'lamps': LAMPS}
json.dump(PLACE, open(os.path.join(HERE, 'place.json'), 'w'), indent=0)
json.dump(BOXES, open(os.path.join(ROOT, 'Saved', 'east-boxes.json'), 'w'))
FLAT = []
for P_ in [S] + GF + [GS, GI]:
    for f in P_.bm.faces:
        ys_ = [v.co.z for v in f.verts]
        if max(ys_) - min(ys_) < 1e-4:
            xs_ = [-v.co.x for v in f.verts]; zs_ = [-v.co.y for v in f.verts]
            FLAT.append([P_.name, round(min(xs_), 3), round(max(xs_), 3), round(ys_[0], 3), round(min(zs_), 3), round(max(zs_), 3), P_.mats[f.material_index]])
json.dump(FLAT, open(os.path.join(ROOT, 'Saved', 'annex-flat-east.json'), 'w'))
# the shell's one big solid in pieces (2026-11-23): 395 x 52 m in one model, Lumen's cards and distance field of it were so
# coarse that its light was patchy; by what it is (floor, Floor 2's slab, ground ceilings, the roof, the front's frames, walls)
# and 66 m along the building
BINX = [X0 + (X1 - X0) / 6.0 * i for i in range(1, 6)]
def kind(f, m):
    ys = [v.co.z for v in f.verts]; lo, hi = min(ys), max(ys)
    if m == MU: return 'frame'
    if hi < 0.3 and m in (MARBLE, 'MI_T_concrete'): return 'floor'
    if lo > 10.9: return 'roof'
    if lo > 5.7 and hi < 6.1: return 'f2'
    if lo > 5.0 and hi < 5.75: return 'ceil'
    return 'wall'
def split(o):
    bm = bmesh.new(); bm.from_mesh(o.data)
    for x in BINX: bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(-x, 0, 0), plane_no=(1, 0, 0), dist=1e-5)
    grp = {}; bm.faces.ensure_lookup_table(); bm.faces.index_update()
    for f in bm.faces:
        x = -f.calc_center_median().x; grp.setdefault((kind(f, o.data.materials[f.material_index].name), sum(x > b_ for b_ in BINX) + 1), []).append(f.index)
    out = []
    for (kd, n), idx in sorted(grp.items()):
        c = bm.copy(); c.faces.ensure_lookup_table(); c.faces.index_update(); keep = set(idx)
        bmesh.ops.delete(c, geom=[f for f in c.faces if f.index not in keep], context='FACES')
        used = sorted({f.material_index for f in c.faces}); mp = {u: k for k, u in enumerate(used)}
        for f in c.faces: f.material_index = mp[f.material_index]
        me = bpy.data.meshes.new('%s_%s%d' % (o.name, kd, n)); c.to_mesh(me); c.free()
        for u in used: me.materials.append(o.data.materials[u])
        q = bpy.data.objects.new(me.name, me); bpy.context.scene.collection.objects.link(q); out.append(q)
    bm.free(); bpy.data.objects.remove(o); return out
import glob
for f_ in glob.glob(os.path.join(HERE, 'east_solid*.fbx')): os.remove(f_)
SOLIDS = []
for P_ in PIECES:
    o = P_.finish(); objs = split(o) if o.name == 'east_solid' else [o]
    if len(objs) > 1: SOLIDS = [q.name for q in objs]
    for o in objs:
        assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
        print('EAST', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
        for x in bpy.context.selected_objects: x.select_set(False)
        o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                                 apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
PLACE['solid'] = SOLIDS
json.dump(PLACE, open(os.path.join(HERE, 'place.json'), 'w'), indent=0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'TerminalEast.blend'))
print('EAST saved; lamps', sum(len(v[2]) for v in LAMPS.values()))
