# West Wing part 1 (to-do 234): the claim hall (x -80..0, z 16..52) and the arrivals hall (x -80..0, z -4..8) of
# make_arrivals_wing.py fitted out. Nothing of the wing is moved; only the cafe's plain placeholder counter there is gone (this
# cafe takes its place).
#   annex_westfit_solid   one model in the plan's metres: along the claim hall's north wall (backs 0.2 m off it, z 45.6..51.8,
#                         3.25 m high with their own ceilings) the oversize belt (its own closed feed housing with rubber flaps, a
#                         flat belt, its sign), lost and found (counter opening, staff front room, back store with shelving), the
#                         toilets (women, accessible WC, men: doorless baffled ways in), the arrivals duty free (glass front);
#                         in the arrivals hall the cafe (its back fitment with the coffee machine and shelves, serving counter,
#                         CAFE on the basalt wall), the information desk; every sign (make_concourse.py's band()/text())
#   annex_westfit_glass   the duty free's glass front, the cafe's pastry case
#   annex_westfit_atm     an ATM (instanced: two, backs to the customs wall z 8)
#   annex_westfit_sim     a SIM card kiosk with its backlit sign (instanced: two, south of the meeting rail)
#   reused, instanced     east_g_fit_cubicles / urinals / vanity / dryer / sink / counter / shelf / table / chair / ochair
#                         (Art/TerminalEast/make_groundfloor_fit.py), tf_wallbay_* / tf_gondola_* / tf_paypoint (make_taxfree.py)
#   westfit_place.json    the instanced pieces' places, the hinged doors (accessible WC, lost and found's store), the lamps
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_west_fitout.py
#   then Tools/import_annex.py annex_westfit_solid annex_westfit_glass annex_westfit_atm annex_westfit_sim,
#   Tools/terminal_edit.py (Tools/terminal_westfit_edit.py)
import bpy, bmesh, json, math, os
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
csrc = open(os.path.join(ROOT, 'Art', 'TerminalEast', 'make_concourse.py'), encoding='utf-8').read()
BLK, WHITE = 'MI_T_black', 'MI_T_signWhite'
S = Piece('annex_westfit_solid'); G = Piece('annex_westfit_glass'); SG = S
exec(csrc[csrc.index('# ---- letters ----'):csrc.index('# ---- walls with')])        # text(), band() (on SG)
TILE, PLA, OAK, WAL, BAS, MAR = 'MI_T_tile', 'MI_T_plaster', 'MI_T_oak', 'MI_T_walnut', 'MI_T_basalt', 'MI_T_marbleDark'
STEEL, CHROME, RUB, DARK, W, MIR, GLS = 'MI_T_steel', 'MI_T_chrome', 'MI_T_rubber', 'MI_T_dark', 'MI_T_white', 'MI_T_liftMirror', 'MI_T_glass'
FL = 0.01                                               # the wing's floor top
TOP, CL = 3.25, 3.2                                     # the north-strip rooms' walls' top, their ceilings' underside
ZF, ZB = 45.6, 51.8                                     # their fronts' outer face, their backs' outer face (the hall wall z 52)
PL_, DOORS, LAMPS = [], [], []

def B(P, x0, x1, y0, y1, z0, z1, m, sides='all'):
    """a box; one standing on the floor has no bottom face (nothing lies on the floor's face)"""
    if y0 <= FL + 0.0005 and sides == 'all': sides = ('+y', '-x', '+x', '-z', '+z')
    P.box(x0, x1, y0, y1, z0, z1, m, sides=sides)
def wall(ax, a0, a1, u0, u1, y0, y1, m, holes=(), sides='all', P=None):
    """a wall: thickness a0..a1 across (x for ax 'x', z for ax 'z'), u0..u1 along, holes (u0, u1, y0, y1)"""
    P = P or S; cu = sorted({u0, u1} | {min(max(v, u0), u1) for h in holes for v in h[:2]})
    for ua, ub in zip(cu, cu[1:]):
        sp = [(y0, y1)]
        for h in holes:
            if h[0] <= ua + 1e-6 and ub <= h[1] + 1e-6: sp = [s for a, b in sp for s in ((a, min(b, h[2])), (max(a, h[3]), b)) if s[1] - s[0] > 1e-4]
        for ya, yb in sp:
            sd = sides
            if ya <= FL + 0.0005: sd = tuple(k for k in (('+y', '-x', '+x', '-z', '+z') if sides == 'all' else sides) if k != '-y')
            if ax == 'x': P.box(a0, a1, ya, yb, ua, ub, m, sides=sd)
            else: P.box(ua, ub, ya, yb, a0, a1, m, sides=sd)
def ceiling(x0, x1, z0, z1): S.box(x0, x1, CL, TOP, z0, z1, PLA, sides=('-y', '+y'))
def lining_z(x0, x1, z0, z1, h):                       # an oak door lining in a hole of a wall in a z plane (jambs 4 cm)
    B(S, x0, x0 + 0.04, FL, h - 0.09, z0, z1, OAK); B(S, x1 - 0.04, x1, FL, h - 0.09, z0, z1, OAK); S.box(x0, x1, h - 0.09, h, z0, z1, OAK)
def lining_x(x0, x1, z0, z1, h):
    B(S, x0, x1, FL, h - 0.09, z0, z0 + 0.04, OAK); B(S, x0, x1, FL, h - 0.09, z1 - 0.04, z1, OAK); S.box(x0, x1, h - 0.09, h, z0, z1, OAK)
def place(nm, asset, x, z, r=0, y=FL): PL_.append(['%s-%03d' % (nm, sum(1 for p in PL_ if p[0].startswith(nm + '-')) + 1), asset, round(x, 4), y, round(z, 4), r])

# ======================================================================================================== the oversize belt
# its feed housing (x -67.4..-65.4, clear of the lift x -71.2..-68.4 and the escalators' well), the belt coming out of its
# mouth through rubber flaps, flat, 0.42 m high, 8.6 m long to its end stop; the sign on a gantry behind
B(S, -67.4, -65.9, FL, 2.6, 46.6, 51.65, STEEL)
B(S, -65.9, -65.4, FL, 2.6, 46.6, 47.3, STEEL); B(S, -65.9, -65.4, FL, 2.6, 49.0, 51.65, STEEL)
S.box(-65.9, -65.4, 1.3, 2.6, 47.3, 49.0, STEEL, sides=('-y', '+y', '+x'))
B(S, -65.9, -57.0, FL, 0.38, 47.3, 49.0, STEEL, sides=('+y', '-x', '+x', '-z', '+z'))
S.box(-65.9, -57.0, 0.38, 0.42, 47.4, 48.9, RUB, sides=('+y', '+x', '-z', '+z'))
for za, zb in ((47.3, 47.4), (48.9, 49.0)): S.box(-65.4, -57.0, 0.38, 0.5, za, zb, STEEL, sides=('+y', '+x', '-z', '+z'))
S.box(-57.2, -57.0, 0.42, 0.6, 47.4, 48.9, RUB, sides=('+y', '-x', '+x'))                      # the end stop
for i in range(8):                                                                             # the flaps
    z0 = 47.32 + i * 0.208; S.box(-65.42, -65.405, 0.46, 1.3, z0, z0 + 0.2, RUB, sides=('+x', '-x', '-y', '-z', '+z'))
for x in (-64.6, -57.8): B(S, x - 0.05, x + 0.05, FL, 2.9, 49.6, 49.7, STEEL)
band('-z', 49.6, -64.8, -57.6, 2.4, 2.9, 'OVERSIZE BAGGAGE', 0.28, WHITE, d=0.03)

# ======================================================================================================== lost and found
# x -55..-46: the counter in its front opening, the staff's front room, a partition (open way through), the back store's
# shelving; its staff door on the east side (a hinged door)
wall('z', ZF, ZF + 0.15, -55.075, -45.925, FL, TOP, PLA, holes=[(-51.6, -48.4, 0.0, 2.4)])
for xc in (-55.0, -46.0):
    wall('x', xc - 0.075, xc + 0.075, ZF + 0.15, ZB - 0.15, FL, TOP, PLA, holes=[(49.6, 50.8, 0.0, 2.3)] if xc == -46.0 else ())
wall('z', ZB - 0.15, ZB, -55.075, -45.925, FL, TOP, PLA)
wall('z', 48.6, 48.7, -54.925, -46.075, FL, CL, PLA, holes=[(-47.4, -46.5, 0.0, 2.2)], sides=('-y', '-x', '+x', '-z', '+z'))
lining_x(-46.075, -45.925, 49.6, 50.8, 2.3); DOORS.append(['D-F1-WESTFIT-LOSTFOUND', -46.0, 50.2, 'x', 1.2, 554])
ceiling(-54.925, -46.075, ZF + 0.15, ZB - 0.15)
wall('z', ZF - 0.02, ZF, -55.075, -45.925, FL, TOP, WAL, holes=[(-51.6, -48.4, 0.0, 2.4)], sides=('-z', '-x', '+x', '+y', '-y'))
band('-z', ZF - 0.02, -54.0, -46.6, 2.55, 2.95, 'LOST AND FOUND', 0.22, WHITE, d=0.03)
place('LOSTFOUND/COUNTER', 'east_g_fit_counter', -50.0, 46.48, 180)
for x in (-53.85, -51.8, -49.75, -47.7): place('LOSTFOUND/SHELF', 'east_g_fit_shelf', x, ZB - 0.15, 180)
for x in (-54.0, -51.9): place('LOSTFOUND/SHELF', 'east_g_fit_shelf', x, 48.6, 180)
place('LOSTFOUND/CHAIR', 'east_g_fit_ochair', -50.6, 47.3, 0); place('LOSTFOUND/CHAIR', 'east_g_fit_ochair', -49.4, 47.3, 0)
LAMPS += [(-50.5, 47.2), (-52.6, 50.2), (-49.4, 50.2)]

# ======================================================================================================== the toilets
# x -44..-24: women (x -44..-35, 8 cubicles), the accessible WC (x -35..-32, its hinged door), men (x -32..-24, 4 cubicles,
# 4 urinals); the ways in without doors, a baffle wall behind each so nobody sees in
HO = [(-36.3, -35.2, 0.0, 2.3), (-34.1, -32.9, 0.0, 2.3), (-31.8, -30.7, 0.0, 2.3)]
wall('z', ZF, ZF + 0.15, -44.075, -23.925, FL, TOP, TILE, holes=HO)
for xc in (-44.0, -35.0, -32.0, -24.0): wall('x', xc - 0.075, xc + 0.075, ZF + 0.15, ZB - 0.15, FL, TOP, TILE)
wall('z', ZB - 0.15, ZB, -44.075, -23.925, FL, TOP, TILE)
for a, b in ((-43.925, -35.075), (-34.925, -32.075), (-31.925, -24.075)): ceiling(a, b, ZF + 0.15, ZB - 0.15)
# the walnut skin outside (front and ends), the signs on it
wall('z', ZF - 0.02, ZF, -44.095, -23.905, FL, TOP, WAL, holes=HO, sides=('-z', '-x', '+x', '+y', '-y'))
B(S, -44.095, -44.075, FL, TOP, ZF, ZB, WAL, sides=('-x', '+y', '+z')); B(S, -23.925, -23.905, FL, TOP, ZF, ZB, WAL, sides=('+x', '+y', '+z'))
band('-z', ZF - 0.02, -38.2, -35.2, 2.45, 2.85, 'WOMEN', 0.22, WHITE, d=0.03)
band('-z', ZF - 0.02, -34.7, -32.3, 2.45, 2.85, 'ACCESSIBLE WC', 0.16, WHITE, d=0.03)
band('-z', ZF - 0.02, -31.9, -29.0, 2.45, 2.85, 'MEN', 0.22, WHITE, d=0.03)
# the baffles (tile, 2.4 m high), 1.15 m inside the fronts
B(S, -37.5, -35.075, FL, 2.4, 46.9, 47.0, TILE); B(S, -31.925, -29.5, FL, 2.4, 46.9, 47.0, TILE)
# women
for x in (-41.85, -37.65): place('WC-W/CUBICLES', 'east_g_fit_cubicles', x, ZB - 0.15, 180)
place('WC-W/VANITY', 'east_g_fit_vanity', -40.6, ZF + 0.15, 0); place('WC-W/DRYER', 'east_g_fit_dryer', -43.4, ZF + 0.15, 0)
place('WC-W/DRYER', 'east_g_fit_dryer', -38.25, ZF + 0.15, 0)
# men
place('WC-M/CUBICLES', 'east_g_fit_cubicles', -29.85, ZB - 0.15, 180); place('WC-M/URINALS', 'east_g_fit_urinals', -25.9, ZB - 0.15, 180)
place('WC-M/VANITY', 'east_g_fit_vanity', -27.4, ZF + 0.15, 0); place('WC-M/DRYER', 'east_g_fit_dryer', -24.6, ZF + 0.15, 0)
# the accessible WC: its pan 0.45 m off the west wall, grab rails, a basin with its mirror, a dryer, the hinged door
B(S, -34.64, -34.26, FL, 0.41, 51.0, 51.65, W); S.box(-34.65, -34.25, 0.41, 0.44, 50.98, 51.6, W)
S.box(-34.75, -34.15, 0.9, 1.15, 51.59, 51.65, CHROME, sides=('-z', '-x', '+x', '-y', '+y'))
S.box(-34.885, -34.85, 0.8, 0.84, 50.6, 51.5, CHROME)
for z in (50.62, 51.44): S.box(-34.925, -34.885, 0.8, 0.84, z, z + 0.04, CHROME, sides=('+x', '-y', '+y', '-z', '+z'))
for x in (-34.13, -33.79): B(S, x - 0.02, x + 0.02, FL, 0.78, 50.5, 50.54, CHROME)              # the drop-down rail's legs
S.box(-34.15, -33.77, 0.78, 0.82, 50.5, 51.6, CHROME); S.box(-34.15, -33.77, 0.82, 0.86, 51.6, 51.65, CHROME, sides=('-z', '-x', '+x', '-y', '+y'))
place('WC-A/SINK', 'east_g_fit_sink', -32.7, ZB - 0.15, 180); place('WC-A/DRYER', 'east_g_fit_dryer', -33.45, ZB - 0.15, 180)
S.box(-33.0, -32.4, 1.0, 1.9, ZB - 0.162, ZB - 0.15, MIR, sides=('-z', '-x', '+x', '-y', '+y'))
lining_z(-34.1, -32.9, ZF, ZF + 0.15, 2.3); DOORS.append(['D-F1-WESTFIT-WC-ACCESSIBLE', -33.5, ZF + 0.075, 'z', 1.2, 521])
LAMPS += [(-41.0, 48.4), (-37.2, 48.4), (-33.5, 48.6), (-29.4, 48.4), (-26.2, 48.4)]

# ======================================================================================================== arrivals duty free
# x -21..-5: a glass front with its way in (x -14.5..-11.5), the lit wall bays, a liquor island, the pay point
wall('x', -21.075, -20.925, ZF, ZB, FL, TOP, PLA); wall('x', -5.075, -4.925, ZF, ZB, FL, TOP, PLA)
wall('z', ZB - 0.15, ZB, -20.925, -5.075, FL, TOP, PLA); S.box(-20.925, -5.075, 2.6, TOP, ZF, ZF + 0.15, PLA, sides=('-y', '+y', '-z', '+z'))
ceiling(-20.925, -5.075, ZF + 0.15, ZB - 0.15)
for a, b in ((-20.925, -14.5), (-11.5, -5.075)):
    n = 4; st = (b - a) / n; ms = [a + i * st for i in range(n + 1)]
    for i, x in enumerate(ms):
        x0, x1 = (x, x + 0.04) if i == 0 else ((x - 0.04, x) if i == n else (x - 0.02, x + 0.02))
        B(S, x0, x1, FL, 2.6, 45.64, 45.71, 'MI_T_mullion', sides=('-x', '+x', '-z', '+z'))
        if i < n:
            g0 = x1; g1 = ms[i + 1] - (0.04 if i + 1 == n else 0.02)
            G.box(g0, g1, FL, 2.6, 45.663, 45.687, GLS, sides=('-z', '+z'))
band('-z', ZF, -16.2, -9.8, 2.7, 3.15, 'DUTY FREE', 0.3, WHITE, d=0.03)
for x, p in ((-18.9, 'liquor'), (-15.0, 'beauty'), (-11.1, 'lux'), (-7.2, 'market')): place('DUTYFREE/WALLBAY', 'tf_wallbay_' + p, x, ZB - 0.15, 180)
place('DUTYFREE/GONDOLA', 'tf_gondola_liquor', -13.4, 48.5, 0)
place('DUTYFREE/PAYPOINT', 'tf_paypoint', -19.6, 47.4, 90)
LAMPS += [(x, z) for x in (-17.0, -13.0, -9.0) for z in (47.4, 50.2)]

# ======================================================================================================== the cafe
# against the east basalt wall (x -0.2): the back fitment (cabinets, worktop, the coffee machine, grinder, open shelves with
# cups), the serving counter (pastry case), CAFE in lit letters on the wall, six tables with four chairs each
B(S, -0.85, -0.2, FL, 0.88, 0.8, 6.8, WAL, sides=('+y', '-x', '-z', '+z'))
S.box(-0.87, -0.2, 0.88, 0.92, 0.78, 6.82, MAR, sides=('+y', '-y', '-x', '-z', '+z'))
B(S, -0.75, -0.3, 0.92, 1.42, 2.2, 3.0, STEEL, sides=('+y', '-x', '+x', '-z', '+z'))              # the coffee machine
for z in (2.35, 2.6, 2.85): S.box(-0.82, -0.75, 1.12, 1.2, z - 0.05, z + 0.05, BLK, sides=('-x', '-y', '+y', '-z', '+z'))
S.box(-0.6, -0.35, 0.92, 1.5, 3.25, 3.5, BLK, sides=('+y', '-x', '+x', '-z', '+z'))                  # the grinder
for y in (1.7, 2.1):
    S.box(-0.5, -0.2, y, y + 0.04, 1.0, 6.6, WAL, sides=('+y', '-y', '-x', '-z', '+z'))
    for k in range(10): S.box(-0.4, -0.3, y + 0.04, y + 0.14, 1.3 + k * 0.5, 1.4 + k * 0.5, W, sides=('+y', '-x', '+x', '-z', '+z'))
band('-x', -0.2, 1.4, 6.2, 2.45, 3.25, 'CAFE', 0.55, WHITE, d=0.03)
B(S, -3.2, -2.4, FL, 1.02, 1.4, 6.2, OAK); S.box(-3.22, -2.38, 1.02, 1.06, 1.38, 6.22, MAR)
G.box(-3.15, -2.55, 1.06, 1.45, 3.8, 5.6, GLS, sides=('+y', '-x', '+x', '-z', '+z'))
S.box(-2.9, -2.6, 1.06, 1.3, 2.0, 2.4, BLK, sides=('+y', '-x', '+x', '-z', '+z'))                    # the till
for x in (-10.2, -7.6, -5.0):
    for z in (0.2, 3.2):
        place('CAFE/TABLE', 'east_g_fit_table', x, z, 0)
        for dx in (-0.3, 0.3): place('CAFE/CHAIR', 'east_g_fit_chair', x + dx, z - 0.62, 0); place('CAFE/CHAIR', 'east_g_fit_chair', x + dx, z + 0.62, 180)

# ======================================================================================================== information
# south of the meeting rail's east part, facing the way out of customs: the counter, the staff behind it, a basalt back
# panel with INFORMATION
B(S, -31.0, -27.0, FL, 1.05, 0.2, 0.95, OAK); S.box(-31.03, -26.97, 1.05, 1.09, 0.17, 0.98, MAR)
S.box(-30.9, -27.1, 0.72, 0.75, -0.1, 0.2, OAK, sides=('+y', '-y', '-x', '+x', '-z'))
B(S, -31.4, -26.6, FL, 2.9, -1.6, -1.4, BAS)
band('+z', -1.4, -31.2, -26.8, 2.2, 2.7, 'INFORMATION', 0.3, WHITE, d=0.03)
band('+z', 0.95, -30.4, -27.6, 0.55, 0.85, 'INFORMATION', 0.16, WHITE, d=0.02)
for x in (-29.9, -28.1): place('INFO/CHAIR', 'east_g_fit_ochair', x, -0.55, 0)

# ======================================================================================================== ATM, SIM kiosk
A = Piece('annex_westfit_atm')                          # back at z 0, its front at +z, from the floor
A.box(-0.4, 0.4, 0.0, 1.6, 0.0, 0.55, STEEL, sides=('+y', '-x', '+x', '-z', '+z'))
A.box(-0.3, 0.3, 0.95, 1.5, 0.55, 0.57, DARK, sides=('+z', '-x', '+x', '-y', '+y'))
A.box(-0.18, 0.18, 1.18, 1.42, 0.57, 0.575, 'MI_T_screenBlack', sides=('+z', '-x', '+x', '-y', '+y'))
A.box(-0.25, 0.25, 0.95, 1.0, 0.57, 0.75, STEEL, sides=('+y', '-y', '-x', '+x', '+z'))
A.box(-0.12, 0.12, 1.0, 1.012, 0.6, 0.72, BLK, sides=('+y', '-x', '+x', '-z', '+z'))
A.box(0.2, 0.26, 1.2, 1.23, 0.57, 0.58, 'MI_T_l_FFFFFF', sides=('+z', '-x', '+x', '-y', '+y'))
A.box(-0.4, 0.4, 1.6, 1.9, 0.0, 0.55, 'MI_T_navy', sides=('+y', '-x', '+x', '-z', '+z'))
text(A, 'ATM', 0.0, 1.75, 0.55, '+z', 0.17, WHITE)
K = Piece('annex_westfit_sim')                          # back at z 0 (the staff's side), the customers at +z
K.box(-0.7, 0.7, 0.0, 1.05, 0.0, 0.6, 'MI_T_whiteGloss', sides=('+y', '-x', '+x', '-z', '+z'))
K.box(-0.72, 0.72, 1.05, 1.09, -0.02, 0.62, MAR)
K.box(-0.04, 0.04, 1.09, 2.2, 0.05, 0.13, STEEL, sides=('-x', '+x', '-z', '+z'))
K.box(-0.65, 0.65, 2.2, 2.7, 0.02, 0.16, 'MI_T_l_FFFFFF')
for nn, zz in (('+z', 0.16), ('-z', 0.02)): text(K, 'SIM CARDS', 0.0, 2.45, zz, nn, 0.2, 'MI_T_navy', maxw=1.15)
for x in (-58.5, -54.5): place('ARRIVALS/SIM', 'annex_westfit_sim', x, -1.0, 0)
for x in (-22.4, -21.4): place('ARRIVALS/ATM', 'annex_westfit_atm', x, 7.895, 180)

# ======================================================================================================== out
OUT = []
for P_ in (S, G, A, K):
    o = P_.finish()
    assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
    print('WFIT', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    OUT.append(o.name)
json.dump({'big': ['annex_westfit_solid', 'annex_westfit_glass'], 'place': PL_, 'doors': DOORS, 'lamps': [[x, CL - 0.03, z] for x, z in LAMPS], 'models': OUT},
          open(os.path.join(HERE, 'westfit_place.json'), 'w'), indent=0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'WestFitout.blend'))
print('WFIT saved', len(OUT), 'models,', len(PL_), 'places,', len(DOORS), 'doors,', len(LAMPS), 'lamps')
