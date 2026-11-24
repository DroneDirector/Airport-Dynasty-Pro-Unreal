# Terminal east, part 3, step 2: the 13 enclosed Floor 1 rooms of make_groundfloor.py fitted out (its walls/floors untouched).
#   east_g_fit_signs     one model in the plan's metres: over every doorway, on the face outside the room, a dark band with the room's
#                        name in glowing letters (make_concourse.py's text()/band(), MI_T_signWhite)
#   east_g_fit_<piece>   instanced pieces (round x 0; their back at z 0 for the ones that stand at a wall, the front looking +z; y from
#                        the floor): the WC's cubicle rows, urinals, vanities, mirrors, dryers; sofas, chairs, desks, counters, shelving,
#                        trolley bays, lockers, benches, tables, the servery, the x-ray, the noticeboard; the lamps' housings
#   the door leaves      are NOT models: Tools/terminal_groundfloor_edit.py makes them as the terminal's own hinged doors (a 'D-...'
#                        node with 'hinge' nodes, as D-F1-OFFICE-1-1: they open as you use them, the closer shuts them), in the 1.2..1.8 m
#                        doorways' oak linings; the 8 m and 6 m openings of the trolley store and baggage service stay open
#   the lamps            the rooms already have the terminal's own lamps (make_terminal_east.py LAMPS: fitting, diffuser and light by the
#                        lamp system); each gets a housing here (white rim to the ceiling, a polished splayed reflector round its diffuser)
# Every piece is placed only if its footprint stays inside its room, clear of the columns, of the doorways' swing and of the other pieces.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalEast/make_groundfloor_fit.py
#   then Tools/import_east.py east_g_fit_..., Tools/terminal_edit.py (Tools/terminal_groundfloor_edit.py)
import bpy, bmesh, json, math, os, sys, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(ROOT, 'Art', 'TerminalAnnex', 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
gsrc = open(os.path.join(HERE, 'make_groundfloor.py'), encoding='utf-8').read()
TILE, CARPET, TERRAZZO, CONCRETE = 'MI_T_tile', 'MI_T_carpetGrey', 'MI_T_terrazzo', 'MI_T_concrete'
exec(gsrc[gsrc.index('def d(c, w=1.2)'):gsrc.index('def minus(')])                     # ROOMS: the 13 rooms, their doorways
csrc = open(os.path.join(HERE, 'make_concourse.py'), encoding='utf-8').read()
BLK, WHITE = 'MI_T_black', 'MI_T_signWhite'
SG = Piece('east_g_fit_signs')
exec(csrc[csrc.index('# ---- letters ----'):csrc.index('# ---- walls with')])        # text(), band() (on SG)
OAK, STEEL, CHROME, W, OFF, NAVY, METAL = 'MI_T_oak', 'MI_T_steel', 'MI_T_chrome', 'MI_T_white', 'MI_T_offwhite', 'MI_T_navy', 'MI_T_metal'
BAS, FAB, FABB, FABT, LEA, MIR, GLS, CED, DARK = 'MI_T_basalt', 'MI_T_fabricGrey', 'MI_T_fabricBlue', 'MI_T_fabricTeal', 'MI_T_leatherBlack', 'MI_T_liftMirror', 'MI_T_glass', 'MI_T_cedar', 'MI_T_dark'
GREEN = 'MI_T_signGreen'
T = 0.2; FY = 0.019                                     # the partitions; the rooms' finish top (0.018) and a millimetre
COLS = [(x, z) for a, x, y, z in json.load(open(os.path.join(HERE, 'place.json')))['cols'] if a == 'east_col_g']
LABEL = {'F1-EAST-WC': 'TOILETS', 'F1-EAST-FAMILY': 'FAMILY ROOM', 'F1-EAST-TROLLEY': 'TROLLEY STORE', 'F1-EAST-BAGSERV': 'BAGGAGE SERVICE',
         'F1-CUSTOMS-E-OFFICE': 'CUSTOMS', 'F1-PC-OFFICE': 'BORDER POLICE', 'F1-PC-SECOND': 'SECOND-LINE CHECKS', 'F1-CREW-CENTRE': 'CREW CENTRE',
         'F1-CANTEEN': 'STAFF CANTEEN', 'F1-GROUND-OFF': 'GROUND HANDLING', 'F1-LOCKERS': 'LOCKERS', 'F1-STAFFSEC': 'STAFF SECURITY', 'F1-POLICE': 'POLICE'}

# ======================================================================================================== the pieces
PCS = {}
def piece(nm):
    P = Piece('east_g_fit_' + nm); PCS[nm] = P; return P
def B(P, x0, x1, y0, y1, z0, z1, m, sides='all'):
    """a box; standing on the floor (y0 0) without its bottom face (nothing lies on the floor's face)"""
    if y0 <= 0.0005 and sides == 'all': sides = ('+y', '-x', '+x', '-z', '+z')
    P.box(x0, x1, y0, y1, z0, z1, m, sides=sides)
def legs(P, x0, x1, z0, z1, h, m=STEEL, t=0.035):
    for x in (x0, x1 - t):
        for z in (z0, z1 - t): B(P, x, x + t, 0, h, z, z + t, m)

def cubicles(n=4, w=1.0, d=1.6):                       # a row of n WC cubicles, back at the wall (z 0), doors on the front (z d)
    P = piece('cubicles'); x0 = -n * w / 2
    for i in range(n + 1):
        x = x0 + i * w; B(P, x - 0.0125, x + 0.0125, 0.15, 2.0, 0.0, d - 0.03, NAVY)
        for z in (0.1, d - 0.2): B(P, x - 0.02, x + 0.02, 0, 0.15, z, z + 0.04, CHROME)
    for i in range(n):
        a = x0 + i * w
        for u0, u1 in ((a + 0.0125, a + 0.17), (a + w - 0.17, a + w - 0.0125)): B(P, u0, u1, 0.15, 2.0, d - 0.03, d, NAVY)
        B(P, a + 0.175, a + w - 0.175, 0.15, 1.98, d - 0.02, d - 0.0, NAVY, sides=('-x', '+x', '-y', '+y', '+z', '-z'))
        B(P, a + w - 0.3, a + w - 0.22, 1.0, 1.05, d, d + 0.03, CHROME); B(P, a + w - 0.32, a + w - 0.2, 1.1, 1.13, d, d + 0.008, GREEN)   # the lock, its free/engaged light
        B(P, a + w / 2 - 0.19, a + w / 2 + 0.19, 0, 0.4, 0.05, 0.6, W)                                   # the pan
        B(P, a + w / 2 - 0.2, a + w / 2 + 0.2, 0.4, 0.43, 0.05, 0.62, W)                                 # its seat
        B(P, a + w / 2 - 0.25, a + w / 2 + 0.25, 0.9, 1.15, 0.0, 0.06, CHROME)                           # the flush plate
    B(P, x0 - 0.0125, -x0 + 0.0125, 1.98, 2.02, d - 0.035, d + 0.005, CHROME)                          # the head rail
def urinals(n=4, s=0.8):
    P = piece('urinals')
    for i in range(n):
        x = (i - (n - 1) / 2) * s
        B(P, x - 0.18, x + 0.18, 0.55, 1.2, 0.0, 0.34, W); B(P, x - 0.04, x + 0.04, 1.2, 1.32, 0.0, 0.06, CHROME)
        if i: B(P, x - s / 2 - 0.0125, x - s / 2 + 0.0125, 0.5, 1.6, 0.0, 0.55, NAVY)
def vanity(n=3, s=1.2):
    P = piece('vanity'); L = n * s / 2
    B(P, -L, L, 0.15, 0.82, 0.0, 0.52, OFF); B(P, -L + 0.05, L - 0.05, 0, 0.15, 0.0, 0.45, BLK); B(P, -L, L, 0.82, 0.86, 0.0, 0.56, BAS)
    B(P, -L, L, 1.1, 2.1, 0.0, 0.012, MIR, sides=('+z', '-x', '+x', '-y', '+y'))
    for i in range(n):
        x = (i - (n - 1) / 2) * s
        B(P, x - 0.22, x + 0.22, 0.86, 0.99, 0.12, 0.46, W); B(P, x - 0.025, x + 0.025, 0.86, 1.1, 0.05, 0.08, CHROME); B(P, x - 0.02, x + 0.02, 1.06, 1.09, 0.08, 0.2, CHROME)
def dryer():
    P = piece('dryer'); B(P, -0.15, 0.15, 1.1, 1.38, 0.0, 0.18, STEEL); B(P, -0.11, 0.11, 1.07, 1.1, 0.04, 0.15, DARK)
def sofa(w=1.8, m=FAB):
    P = piece('sofa%d' % round(w * 10)); legs(P, -w / 2 + 0.05, w / 2 - 0.05, 0.05, 0.85, 0.1, BLK)
    B(P, -w / 2, w / 2, 0.1, 0.42, 0.0, 0.88, m); B(P, -w / 2, w / 2, 0.42, 0.85, 0.0, 0.2, m)
    for x in (-w / 2, w / 2 - 0.16): B(P, x, x + 0.16, 0.42, 0.62, 0.2, 0.88, m)
def changer():
    P = piece('changer'); B(P, -0.6, 0.6, 0.1, 0.86, 0.0, 0.72, OFF); B(P, -0.55, 0.55, 0, 0.1, 0.0, 0.62, BLK); B(P, -0.6, 0.6, 0.86, 0.9, 0.0, 0.75, BAS)
    B(P, -0.5, 0.5, 0.9, 0.98, 0.08, 0.68, FABT)
    for x in (-0.6, 0.57): B(P, x, x + 0.03, 0.9, 1.05, 0.0, 0.75, OFF)
    B(P, -0.57, 0.57, 0.9, 1.05, 0.0, 0.03, OFF)
def sink():
    P = piece('sink'); B(P, -0.09, 0.09, 0, 0.68, 0.06, 0.24, W); B(P, -0.28, 0.28, 0.68, 0.86, 0.0, 0.44, W); B(P, -0.025, 0.025, 0.86, 1.08, 0.04, 0.07, CHROME)
def shelf(w=2.0, d=0.5, h=2.0):
    P = piece('shelf')
    for x in (-w / 2, w / 2 - 0.04):
        for z in (0.0, d - 0.04): B(P, x, x + 0.04, 0, h, z, z + 0.04, STEEL)
    for k, y in enumerate((0.12, 0.6, 1.08, 1.56, h - 0.03)):
        B(P, -w / 2 + 0.04, w / 2 - 0.04, y, y + 0.03, 0.0, d, METAL)
        if k < 3:
            for j, x in enumerate((-0.8, -0.3, 0.25)): B(P, x, x + 0.4 - 0.08 * j, y + 0.03, y + 0.33 - 0.06 * j, 0.06, d - 0.08, CED)
def bays(n=1, L=6.0):                                  # a trolley bay: two guide rails on posts, L long along x, 0.9 apart
    P = piece('bay')
    for z in (-0.45, 0.45):
        for x in [-L / 2 + 1.5 * k for k in range(int(L / 1.5) + 1)]: B(P, x - 0.03, x + 0.03, 0, 0.9, z - 0.03, z + 0.03, STEEL)
        B(P, -L / 2 - 0.04, L / 2 + 0.04, 0.9, 0.95, z - 0.035, z + 0.035, STEEL); B(P, -L / 2 - 0.04, L / 2 + 0.04, 0.4, 0.44, z - 0.02, z + 0.02, STEEL)
def counter(w=3.0):
    P = piece('counter'); B(P, -w / 2, w / 2, 0, 1.02, 0.0, 0.7, OAK); B(P, -w / 2 - 0.02, w / 2 + 0.02, 1.02, 1.06, -0.02, 0.74, BAS)
    B(P, -w / 2 + 0.1, w / 2 - 0.1, 0.72, 0.75, -0.3, 0.0, OAK)                                       # the staff's worktop behind
    for x in (-0.75, 0.75): B(P, x - 0.25, x + 0.25, 1.1, 1.42, 0.2, 0.23, BLK); B(P, x - 0.03, x + 0.03, 1.06, 1.1, 0.22, 0.26, STEEL)
def desk(w=1.6, d=0.8):
    P = piece('desk'); B(P, -w / 2, w / 2, 0.72, 0.75, 0.0, d, OAK)
    for x in (-w / 2 + 0.02, w / 2 - 0.05): B(P, x, x + 0.03, 0, 0.72, 0.03, d - 0.03, OFF)
    B(P, -w / 2 + 0.05, w / 2 - 0.05, 0.3, 0.72, 0.03, 0.05, OFF)
    B(P, -0.3, 0.3, 0.9, 1.25, 0.1, 0.125, BLK); B(P, -0.03, 0.03, 0.76, 0.9, 0.125, 0.155, STEEL); B(P, -0.12, 0.12, 0.75, 0.76, 0.08, 0.2, STEEL)
    B(P, -0.22, 0.22, 0.75, 0.77, 0.35, 0.5, DARK)
def ochair():                                          # an office chair, the seat's front at +z
    P = piece('ochair'); B(P, -0.3, 0.3, 0.02, 0.06, -0.03, 0.03, BLK); B(P, -0.03, 0.03, 0.026, 0.054, -0.3, 0.3, BLK)
    for x, z in ((-0.3, 0), (0.3, 0), (0, -0.3), (0, 0.3)): B(P, x - 0.025, x + 0.025, 0, 0.02, z - 0.025, z + 0.025, BLK)
    B(P, -0.025, 0.025, 0.06, 0.4, -0.025, 0.025, CHROME); B(P, -0.25, 0.25, 0.4, 0.48, -0.24, 0.24, FAB); B(P, -0.23, 0.23, 0.55, 1.05, -0.3, -0.24, FAB)
    B(P, -0.03, 0.03, 0.42, 0.55, -0.3, -0.24, BLK)
def chair(m=OAK):                                      # a four-legged chair, the seat's front at +z
    P = piece('chair'); legs(P, -0.21, 0.21, -0.21, 0.21, 0.44, STEEL, 0.025); B(P, -0.22, 0.22, 0.44, 0.47, -0.22, 0.22, m)
    for x in (-0.21, 0.185): B(P, x, x + 0.025, 0.47, 0.86, -0.21, -0.185, STEEL)
    B(P, -0.2, 0.2, 0.6, 0.86, -0.235, -0.21, m)
def filing():
    P = piece('filing'); B(P, -0.235, 0.235, 0, 1.32, 0.0, 0.62, METAL)
    for k in range(4): B(P, -0.06, 0.06, 0.25 + 0.32 * k, 0.28 + 0.32 * k, 0.62, 0.65, CHROME); B(P, -0.225, 0.225, 0.32 * k + 0.31, 0.32 * k + 0.315, 0.62, 0.626, DARK)
def bench(w=1.8):
    P = piece('bench'); legs(P, -w / 2 + 0.1, w / 2 - 0.1, 0.02, 0.38, 0.42, STEEL, 0.04)
    for z in (0.0, 0.135, 0.27): B(P, -w / 2, w / 2, 0.42, 0.46, z, z + 0.12, OAK)
def lockers(n=4, s=0.3, d=0.5, h=1.9):
    P = piece('lockers'); L = n * s / 2; B(P, -L, L, 0, 0.1, 0.02, d, BLK); B(P, -L, L, 0.1, h, 0.0, d - 0.02, METAL, sides=('+y', '-x', '+x', '-z', '+z', '-y'))
    for i in range(n):
        x = -L + i * s
        for y0, y1 in ((0.1, 0.995), (1.005, h)):
            B(P, x + 0.004, x + s - 0.004, y0 + 0.004, y1 - 0.004, d - 0.02, d, METAL, sides=('+z', '-x', '+x', '-y', '+y'))
            B(P, x + s - 0.07, x + s - 0.04, y0 + 0.35 * (y1 - y0), y0 + 0.35 * (y1 - y0) + 0.1, d, d + 0.025, CHROME)
            for k in range(3): B(P, x + 0.08, x + s - 0.08, y1 - 0.12 + 0.025 * k, y1 - 0.11 + 0.025 * k, d, d + 0.006, DARK, sides=('+z',))
def notice():
    P = piece('notice'); B(P, -0.9, 0.9, 1.0, 2.0, 0.0, 0.03, STEEL); B(P, -0.87, 0.87, 1.03, 1.97, 0.03, 0.035, CED, sides=('+z',))
    for x, y, w, h in ((-0.75, 1.55, 0.21, 0.3), (-0.45, 1.6, 0.3, 0.21), (-0.05, 1.45, 0.21, 0.3), (0.3, 1.62, 0.21, 0.3), (0.55, 1.2, 0.3, 0.21), (-0.6, 1.12, 0.21, 0.3)):
        B(P, x, x + w, y, y + h, 0.035, 0.041, W, sides=('+z', '-x', '+x', '-y', '+y'))
def table():
    P = piece('table'); B(P, -0.6, 0.6, 0.72, 0.75, -0.4, 0.4, OFF); B(P, -0.04, 0.04, 0.05, 0.72, -0.04, 0.04, STEEL)
    B(P, -0.3, 0.3, 0, 0.05, -0.25, 0.25, STEEL)
def servery(w=6.0):
    P = piece('servery'); B(P, -w / 2, w / 2, 0, 0.9, 0.0, 0.75, STEEL); B(P, -w / 2 - 0.02, w / 2 + 0.02, 0.9, 0.94, 0.0, 0.8, CHROME)
    for x in [-w / 2 + 0.3 + 1.35 * k for k in range(5)]: B(P, x - 0.015, x + 0.015, 0.94, 1.4, 0.55, 0.58, CHROME)
    B(P, -w / 2 + 0.3, w / 2 - 0.3, 1.4, 1.42, 0.3, 0.6, GLS)
    for k in range(4): x = -w / 2 + 0.6 + 1.35 * k; B(P, x, x + 0.6, 0.94, 0.99, 0.1, 0.45, CHROME)
def xray():                                            # an x-ray for bags, along x: the body round its tunnel, the belts in and out
    P = piece('xray'); X0, X1 = -1.1, 1.1
    B(P, X0, X1, 0, 0.75, -0.5, 0.5, OFF); B(P, X0, X1, 1.2, 1.5, -0.5, 0.5, OFF)
    for z0, z1 in ((-0.5, -0.32), (0.32, 0.5)): B(P, X0, X1, 0.75, 1.2, z0, z1, OFF)
    for x in (X0, X1 - 0.004): B(P, x, x + 0.004, 0.76, 1.19, -0.315, 0.315, BLK, sides=('-x', '+x'))           # the lead curtains
    B(P, X0 + 0.01, X1 - 0.01, 0.75, 0.77, -0.3, 0.3, BLK, sides=('+y',))
    for x0, x1 in ((-2.0, X0 - 0.005), (X1 + 0.005, 2.0)): B(P, x0, x1, 0, 0.74, -0.32, 0.32, STEEL); B(P, x0, x1, 0.74, 0.77, -0.3, 0.3, BLK)
    B(P, 0.2, 0.75, 1.5, 1.52, -0.3, 0.3, DARK)
def trim():                                            # the housing round one of the terminal's own 1.2 x 0.6 lamps (y 0 its middle)
    P = piece('lamptrim'); a, b, ya, yb = 0.66, 0.36, 0.022, 0.067
    B(P, -a, a, ya, yb, b - 0.02, b, W, sides=('-y', '+z', '-x', '+x')); B(P, -a, a, ya, yb, -b, -b + 0.02, W, sides=('-y', '-z', '-x', '+x'))
    B(P, a - 0.02, a, ya, yb, -b + 0.02, b - 0.02, W, sides=('-y', '+x')); B(P, -a, -a + 0.02, ya, yb, -b + 0.02, b - 0.02, W, sides=('-y', '-x'))
    i0, j0, i1, j1, y1 = a - 0.02, b - 0.02, 0.608, 0.308, -0.012      # the reflector: from the rim's inner edge down to 8 mm off the diffuser
    for sx, sz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if sx:
            q = [(sx * i0, ya, -j0), (sx * i0, ya, j0), (sx * i1, y1, j1), (sx * i1, y1, -j1)]
        else:
            q = [(-i0, ya, sz * j0), (i0, ya, sz * j0), (i1, y1, sz * j1), (-i1, y1, sz * j1)]
        f = P.face(q, CHROME); dx, dz = -sx, -sz; P.want.append((f, (-dx, -dz, -0.6)))
for f_ in (cubicles, urinals, vanity, dryer, changer, sink, shelf, bays, counter, desk, ochair, chair, filing, bench, lockers, notice, table, servery, xray, trim): f_()
sofa(1.8); sofa(0.8, FABB)

def bbox(P):
    xs = [-v.co.x for v in P.bm.verts]; zs = [-v.co.y for v in P.bm.verts]; return min(xs), max(xs), min(zs), max(zs)
FOOT = {k: bbox(P) for k, P in PCS.items()}

# ======================================================================================================== where they go
RB = {r[0]: r[1:5] for r in ROOMS}
DOORZ = []                                             # (x0, x1, z0, z1): kept free round every doorway, both sides
for name, x0, x1, z0, z1, m, doors in ROOMS:
    for side, hs in doors.items():
        at = {'-z': z0, '+z': z1, '-x': x0, '+x': x1}[side]
        for h0, h1 in hs:
            if side[1] == 'z': DOORZ.append((h0 - 0.4, h1 + 0.4, at - 1.6, at + 1.6))
            else: DOORZ.append((at - 1.6, at + 1.6, h0 - 0.4, h1 + 0.4))
PL_, DROP, TAKEN = [], [], {}
def rot(fp, r):
    a, b, c, e = fp
    return {0: (a, b, c, e), 90: (c, e, -b, -a), 180: (-b, -a, -e, -c), 270: (-e, -c, a, b)}[r % 360]
def fits(room, nm, x, z, r):
    a, b, c, e = rot(FOOT[nm], r); a, b, c, e = x + a, x + b, z + c, z + e
    rx0, rx1, rz0, rz1 = RB[room]
    if a < rx0 + 0.015 or b > rx1 - 0.015 or c < rz0 + 0.015 or e > rz1 - 0.015: return None, 'outside its room'
    if any(a < cx + 0.5 and cx - 0.5 < b and c < cz + 0.5 and cz - 0.5 < e for cx, cz in COLS): return None, 'a column'
    if nm != 'lamptrim' and any(a < p and q > c and a < q0 and b > p0 and c < q and e > p for p0, q0, p, q in DOORZ if True): pass
    if nm != 'lamptrim' and any(a < q0 and p0 < b and c < q and p < e for p0, q0, p, q in DOORZ): return None, 'a doorway'
    if any(a < t[1] + 0.02 and t[0] - 0.02 < b and c < t[3] + 0.02 and t[2] - 0.02 < e for t in TAKEN.get(room, [])): return None, 'another piece'
    return (a, b, c, e), ''
def group(room, items):
    """items: (piece, x, z, turn[, y]) - all of them, or none (a table without its chairs is no use)"""
    got = []
    for it in items:
        nm, x, z, r = it[:4]; fp, why = fits(room, nm, x, z, r)
        if not fp: DROP.append((room, nm, x, z, why)); return 0
        TAKEN.setdefault(room, []).append(fp); got.append(fp)
    for it in items:
        nm, x, z, r = it[:4]; y = it[4] if len(it) > 4 else FY
        PL_.append(['%s/FIT-%03d' % (room, sum(1 for p in PL_ if p[0].startswith(room + '/')) + 1), 'east_g_fit_' + nm, round(x, 4), round(y, 4), round(z, 4), r])
    return 1
def at_wall(room, side, u, nm, off=0.03, y=None):
    """a piece with its back at one of the room's walls, looking into the room"""
    x0, x1, z0, z1 = RB[room]
    x, z, r = {'-z': (u, z0 + off, 0), '+z': (u, z1 - off, 180), '-x': (x0 + off, u, 90), '+x': (x1 - off, u, 270)}[side]
    return group(room, [(nm, x, z, r) + ((y,) if y is not None else ())])
def desk_set(room, x, z, r=0, guest=False):
    """a desk (its knee side the way r turns +z) with its office chair, and a visitor's chair over the desk from it"""
    dx, dz = {0: (0, 1), 90: (1, 0), 180: (0, -1), 270: (-1, 0)}[r]
    it = [('desk', x, z, r), ('ochair', x + dx * 1.15, z + dz * 1.15, (r + 180) % 360)]
    if guest: it.append(('chair', x - dx * 0.45, z - dz * 0.45, r))
    return group(room, it)
def dining(room, x, z):
    return group(room, [('table', x, z, 0), ('chair', x - 0.3, z - 0.72, 0), ('chair', x + 0.3, z - 0.72, 0), ('chair', x - 0.3, z + 0.72, 180), ('chair', x + 0.3, z + 0.72, 180)])

# the toilets: cubicles along the back wall, urinals, two vanities and their dryers
R = 'F1-EAST-WC'
for u in (163.2, 167.6): at_wall(R, '+z', u, 'cubicles')
at_wall(R, '+z', 174.4, 'urinals')
at_wall(R, '-x', 46.0, 'vanity'); at_wall(R, '+x', 45.0, 'vanity'); at_wall(R, '-x', 49.0, 'dryer'); at_wall(R, '+x', 48.0, 'dryer')
R = 'F1-EAST-FAMILY'
at_wall(R, '+z', 181.0, 'sofa18'); at_wall(R, '+z', 185.0, 'sofa8'); at_wall(R, '+x', 46.0, 'changer'); at_wall(R, '-x', 47.0, 'sink'); at_wall(R, '-x', 44.5, 'dryer')
R = 'F1-EAST-TROLLEY'                                  # the trolleys' bays (empty: no luggage trolley model exists yet), shelving
for x in (196.8, 198.9, 201.0, 203.1): group(R, [('bay', x, 46.5, 90)])
for u in (192.0, 194.2): at_wall(R, '+z', u, 'shelf')
for u in (206.0, 208.2): at_wall(R, '+z', u, 'shelf')
for u in (44.5, 47.0): at_wall(R, '-x', u, 'shelf'); at_wall(R, '+x', u, 'shelf')
R = 'F1-EAST-BAGSERV'
group(R, [('counter', 219.0, 45.0, 180), ('ochair', 218.2, 46.1, 180), ('ochair', 219.8, 46.1, 180)])
for u in (212.5, 215.0, 223.0, 225.5): at_wall(R, '+z', u, 'shelf')
group(R, [('chair', 211.0, 44.0, 90), ('chair', 211.0, 44.8, 90)]); at_wall(R, '+x', 48.0, 'filing'); at_wall(R, '+x', 48.6, 'filing')
R = 'F1-CUSTOMS-E-OFFICE'
desk_set(R, 232.0, 47.6, 0, guest=True); at_wall(R, '+x', 50.5, 'filing'); at_wall(R, '+x', 49.9, 'filing')
R = 'F1-PC-OFFICE'
desk_set(R, 315.0, 6.2, 0); desk_set(R, 318.5, 6.2, 0); at_wall(R, '-x', 13.0, 'bench'); at_wall(R, '-x', 16.0, 'bench'); at_wall(R, '-x', 9.0, 'filing')
R = 'F1-PC-SECOND'
desk_set(R, 364.0, 46.0, 0, guest=True); at_wall(R, '+x', 48.0, 'filing')
R = 'F1-CREW-CENTRE'
for u in range(9, 24, 1): pass
for k in range(10): at_wall(R, '-x', 8.0 + 1.25 * k, 'lockers')
for x in (378.5, 383.0, 393.5, 398.0):
    group(R, [('sofa18', x, 17.6, 0), ('sofa18', x, 21.4, 180), ('table', x, 19.5, 0)])
at_wall(R, '-z', 381.0, 'notice'); at_wall(R, '-z', 395.0, 'notice')
for x in (378.5, 383.0, 393.5, 398.0): desk_set(R, x, 9.5, 0)
R = 'F1-CANTEEN'
at_wall(R, '+z', 410.5, 'servery'); at_wall(R, '+z', 429.5, 'servery')
for x in (408.5, 411.5, 414.5, 425.5, 428.5, 431.5):
    for z in (8.5, 11.5, 18.0, 21.0): dining(R, x, z)
R = 'F1-GROUND-OFF'
for x in (440.0, 443.5, 457.0, 460.5, 464.0):
    for z in (7.5, 12.0, 17.5, 22.0): desk_set(R, x, z, 0)
for u in (8.0, 9.0, 21.0, 22.0): at_wall(R, '+x', u, 'filing')
R = 'F1-LOCKERS'
for zb in (34.0, 43.0):
    for x in [375.0 + 1.25 * k for k in range(9)] + [391.4 + 1.25 * k for k in range(9)]:
        group(R, [('lockers', x, zb, 0), ('lockers', x, zb - 0.02, 180)])
for x in [375.0 + 1.25 * k for k in range(9)] + [391.4 + 1.25 * k for k in range(9)]: at_wall(R, '+z', x, 'lockers')
for x in (377.0, 382.0, 394.0, 399.0):
    for z in (36.2, 40.2, 45.0, 49.0): group(R, [('bench', x, z, 0)])
R = 'F1-STAFFSEC'
group(R, [('xray', 423.2, 31.0, 90)]); desk_set(R, 426.0, 30.0, 270)
for x in (409.0, 413.0, 428.0, 432.0): desk_set(R, x, 50.9, 180)
at_wall(R, '-z', 408.0, 'filing'); at_wall(R, '-z', 408.6, 'filing'); at_wall(R, '-z', 432.0, 'bench')
R = 'F1-POLICE'
for x in (448.0, 452.0, 456.0, 460.0): desk_set(R, x, 30.5, 0)
for x in (448.0, 452.0): desk_set(R, x, 36.0, 0)
for u in (44.0, 46.5, 49.0): at_wall(R, '+x', u, 'bench')
for x in [440.0 + 0.0 + 1.25 * k for k in range(6)]: at_wall(R, '+z', x, 'lockers')
at_wall(R, '-z', 465.0, 'notice'); at_wall(R, '+x', 30.0, 'filing'); at_wall(R, '+x', 30.6, 'filing')
# the lamps' housings, at the rooms' own lamps
LAMPS = json.load(open(os.path.join(HERE, 'place.json')))['lamps']; NTRIM = 0
for room in RB:
    p, y, pl = LAMPS[room]
    for k, (x, z) in enumerate(pl):
        PL_.append(['%s/LAMP-HOUSING-%03d' % (room, k + 1), 'east_g_fit_lamptrim', x, y, z, 0]); NTRIM += 1

# ======================================================================================================== the signs and the doors
DOORS, SEEN = [], set()
for name, x0, x1, z0, z1, m, doors in ROOMS:
    for side, hs in doors.items():
        at = {'-z': z0, '+z': z1, '-x': x0, '+x': x1}[side]; sg = 1 if side[0] == '+' else -1; face = at + sg * T; c = at + sg * T / 2
        for h0, h1 in hs:
            wide = h1 - h0 > 2.0; y0 = 3.25 if wide else 2.36
            u0, u1 = ((h0 + h1) / 2 - 2.0, (h0 + h1) / 2 + 2.0) if wide else (h0 - 0.25, h1 + 0.25)
            band(side, face, u0, u1, y0, y0 + 0.26, LABEL[name], 0.15, WHITE, d=0.03)
            key = (side[1], round(c, 2), round(h0, 2))
            if wide or key in SEEN: continue
            SEEN.add(key)
            DOORS.append(['D-F1-%s-%d' % (name[3:], sum(1 for q in DOORS if q[0].startswith('D-F1-%s-' % name[3:])) + 1), name,
                          round((h0 + h1) / 2, 4) if side[1] == 'z' else round(c, 4), round(c, 4) if side[1] == 'z' else round((h0 + h1) / 2, 4), side[1], round(h1 - h0, 3)])

# ======================================================================================================== out
for f_ in glob.glob(os.path.join(HERE, 'east_g_fit_*.fbx')): os.remove(f_)
used = {p[1] for p in PL_}
OUT = []
for P_ in [SG] + list(PCS.values()):
    if P_ is not SG and P_.name not in used: continue
    o = P_.finish()
    assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
    print('GFIT', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    OUT.append(o.name)
json.dump({'big': ['east_g_fit_signs'], 'place': PL_, 'doors': DOORS, 'models': OUT}, open(os.path.join(HERE, 'ground_fit_place.json'), 'w'), indent=0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'GroundFloorFit.blend'))
from collections import Counter
print('GFIT saved', len(OUT), 'models,', len(PL_), 'places (%d lamp housings),' % NTRIM, len(DOORS), 'doors')
print('GFIT per room', dict(Counter(p[0].split('/')[0] for p in PL_ if '/FIT-' in p[0])))
print('GFIT dropped', len(DROP)); [print('   ', d_) for d_ in DROP[:60]]
