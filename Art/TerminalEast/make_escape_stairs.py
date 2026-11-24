# Terminal east, part 4: the closed escape stairs (sheet 9c, Blueprints/Terminal/Changes/v2/s9_escape.py; numbers: esc_geom.py).
# ONE stair box, made round x 0 in the plan's z (it is placed at x 246 + 30 k, 11 times, by Tools/terminal_escape_edit.py):
#   east_s_box     the fire-rated box: 20 cm concrete walls from the floor, low (5.44) over its foot, tall (8.7) where it climbs past
#                  Floor 2's edge (z 13.8..21.5); concrete landings at the top (Floor 2, +6.01) ; three doorways with steel frames:
#                  the head (z 21.5, from Floor 2's gallery), the foot to the hall (its east side, z 7..8.2, 1.2 m) and the foot's
#                  way out (z 6.13, 1.8 m: the promenade wall's own doorway; in the coach station straight to the hall's front);
#                  a green lit exit sign over each doorway
#   east_s_stair   the steel stair: a straight run of 35 risers of 17.1 cm, treads 27 cm (2.04 m wide, 1.98 m between the rails),
#                  18 down from the top landing, a 1.8 m steel mid landing at +2.93, 17 down to the foot; channel stringers held
#                  1 cm off the walls on brackets, a steel handrail on each wall at 0.9 m
# The door leaves are the terminal's own hinged doors (Tools/terminal_escape_edit.py, as part 3's): steel, a vision panel, a kick
# plate, closed (alarmed, no passenger use). Stair 21 (x 551-555 outside the east end) is NOT built here: see the report.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalEast/make_escape_stairs.py
#   then Tools/import_east.py east_s_box east_s_stair, Tools/terminal_edit.py
import bpy, bmesh, json, math, os, sys, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
import esc_geom as E
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(ROOT, 'Art', 'TerminalAnnex', 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
CON, STEEL, DARK, GREEN, BLK = 'MI_T_concrete', 'MI_T_steel', 'MI_T_dark', 'MI_T_signGreen', 'MI_T_black'
Y0, F2 = 0.01, 6.01
HW, T = E.HW, E.WT; IN = HW - T                       # 1.1: the walls' inner faces
Z0, ZT, Z1, LOW, HIGH = E.Z0, E.ZT, E.Z1, E.LOW, E.HIGH
DW, DH, SW = E.DOOR_W, E.DOOR_H, E.SIDE_W
NR, G = 35, 0.27; R = (F2 - Y0) / NR                   # risers, going
ZTOP = 19.5                                            # the top landing's edge (it runs to the north wall, 21.3)
ZM1 = ZTOP - 17 * G; ZM0 = ZM1 - 1.8; YM = F2 - 18 * R  # the mid landing
ZF = ZM0 - 16 * G                                      # the foot's first riser (8.79)
SZ0, SZ1 = Z0 + T, ZF                                  # the foot's landing (the floor)
SDZ = (SZ0 + 0.6, SZ0 + 0.6 + SW)                      # the side door to the hall, on the foot's landing


def put(P, pts, m, n):
    f = P.face(pts, m); P.want.append((f, (-n[0], -n[2], n[1])))


def prism(P, x0, x1, poly, m):
    """A solid along x from x0 to x1 whose section is the polygon poly [(z, y)...] (counter-clockwise seen from +x)."""
    put(P, [(x1, y, z) for z, y in poly], m, (1, 0, 0)); put(P, [(x0, y, z) for z, y in reversed(poly)], m, (-1, 0, 0))
    for (za, ya), (zb, yb) in zip(poly, poly[1:] + poly[:1]):
        dz, dy = zb - za, yb - ya; L = math.hypot(dz, dy)
        put(P, [(x0, ya, za), (x1, ya, za), (x1, yb, zb), (x0, yb, zb)], m, (0, -dz / L, dy / L))


def ccw(poly):                                          # (z, y) seen from +x: z to the right is -plan z... make it counter-clockwise
    a = sum(za * yb - zb * ya for (za, ya), (zb, yb) in zip(poly, poly[1:] + poly[:1]))
    return poly if a < 0 else list(reversed(poly))


def wall_z(P, z0, z1, x0, x1, y0, y1, holes=(), m=CON):
    """A wall across (plan z z0..z1) from x0 to x1, y0..y1, with holes (a, b, h0, h1): boxes round each hole."""
    xs = sorted({x0, x1} | {v for h in holes for v in h[:2]})
    for a, b in zip(xs, xs[1:]):
        h = next((h for h in holes if h[0] <= a and b <= h[1]), None)
        if h is None: P.box(a, b, y0, y1, z0, z1, m); continue
        if h[2] > y0: P.box(a, b, y0, h[2], z0, z1, m)
        if h[3] < y1: P.box(a, b, h[3], y1, z0, z1, m)


def wall_x(P, x0, x1, z0, z1, y0, y1, holes=(), m=CON):
    zs = sorted({z0, z1} | {v for h in holes for v in h[:2]})
    for a, b in zip(zs, zs[1:]):
        h = next((h for h in holes if h[0] <= a and b <= h[1]), None)
        if h is None: P.box(x0, x1, y0, y1, a, b, m); continue
        if h[2] > y0: P.box(x0, x1, y0, h[2], a, b, m)
        if h[3] < y1: P.box(x0, x1, h[3], y1, a, b, m)


def frame_z(P, zc, a, b, y0, h, t=T):                   # a steel frame in a doorway of a wall across (z), jambs and head 4 cm
    for p, q in ((a, a + 0.04), (b - 0.04, b)): P.box(p, q, y0, y0 + h, zc - t / 2 - 0.01, zc + t / 2 + 0.01, STEEL)
    P.box(a + 0.04, b - 0.04, y0 + h - 0.04, y0 + h, zc - t / 2 - 0.01, zc + t / 2 + 0.01, STEEL)


def frame_x(P, xc, a, b, y0, h, t=T):
    for p, q in ((a, a + 0.04), (b - 0.04, b)): P.box(xc - t / 2 - 0.01, xc + t / 2 + 0.01, y0, y0 + h, p, q, STEEL)
    P.box(xc - t / 2 - 0.01, xc + t / 2 + 0.01, y0 + h - 0.04, y0 + h, a + 0.04, b - 0.04, STEEL)


def sign(P, x, y, z, n):                               # a lit exit sign 40 x 15 cm on a black plate, standing off the face (n: +-1 along z or 'x')
    if n in (1, -1):
        P.box(x - 0.22, x + 0.22, y, y + 0.17, z + n * 0.012 - 0.006, z + n * 0.012 + 0.006, BLK, sides=('-x', '+x', '-y', '+y', '+z' if n > 0 else '-z'))
        P.box(x - 0.2, x + 0.2, y + 0.01, y + 0.16, z + n * 0.02 - 0.002, z + n * 0.02 + 0.002, GREEN, sides=('-x', '+x', '-y', '+y', '+z' if n > 0 else '-z'))
    else:
        s = 1 if n == '+x' else -1
        P.box(x + s * 0.012 - 0.006, x + s * 0.012 + 0.006, y, y + 0.17, z - 0.22, z + 0.22, BLK, sides=('-z', '+z', '-y', '+y', n))
        P.box(x + s * 0.02 - 0.002, x + s * 0.02 + 0.002, y + 0.01, y + 0.16, z - 0.2, z + 0.2, GREEN, sides=('-z', '+z', '-y', '+y', n))


# ======================================================================================================== the box
B = Piece('east_s_box')
HEAD = (-DW / 2, DW / 2, F2, F2 + DH); FOOT = (-DW / 2, DW / 2, Y0, Y0 + DH); SIDE = (SDZ[0], SDZ[1], Y0, Y0 + DH)
# the two side walls: low part to the roof's top (5.44) the whole length, the tall part over it from z 13.8
wall_x(B, -HW, -IN, Z0, Z1, Y0, LOW)
wall_x(B, IN, HW, Z0, Z1, Y0, LOW, holes=[SIDE])
for x0, x1 in ((-HW, -IN), (IN, HW)): B.box(x0, x1, LOW, HIGH, ZT, Z1, CON, sides=('+y', '-z', '+z', '-x', '+x'))
# the ends: the foot's wall (z 6.13..6.33) and the head's (z 21.3..21.5) between the side walls
wall_z(B, Z0, Z0 + T, -IN, IN, Y0, LOW, holes=[FOOT])
wall_z(B, Z1 - T, Z1, -IN, IN, Y0, HIGH, holes=[HEAD])
# the roofs (24 cm) and the step between them (z 13.8..14.0)
B.box(-IN, IN, LOW - 0.24, LOW, Z0 + T, ZT, CON, sides=('+y', '-y'))
B.box(-IN, IN, LOW - 0.24, HIGH, ZT, ZT + T, CON, sides=('+y', '-y', '-z', '+z'))
B.box(-IN, IN, HIGH - 0.24, HIGH, ZT + T, Z1 - T, CON, sides=('+y', '-y'))
# the top landing: a concrete slab from the north wall to the first riser, flush with Floor 2 (25 cm)
B.box(-IN, IN, F2 - 0.25, F2, ZTOP, Z1 - T, CON, sides=('+y', '-y', '-z'))
# the doorways' steel frames, the head's threshold
frame_z(B, Z1 - T / 2, HEAD[0], HEAD[1], F2, DH); frame_z(B, Z0 + T / 2, FOOT[0], FOOT[1], Y0, DH); frame_x(B, HW - T / 2, SIDE[0], SIDE[1], Y0, DH)
B.box(HEAD[0] + 0.04, HEAD[1] - 0.04, F2, F2 + 0.006, Z1 - T, Z1, STEEL, sides=('+y', '-z', '+z'))
# the exit signs over the doorways: outside each, inside over the way out
sign(B, 0.0, F2 + DH + 0.12, Z1, 1); sign(B, 0.0, Y0 + DH + 0.12, Z0, -1); sign(B, HW, Y0 + DH + 0.12, sum(SDZ) / 2, '+x')
sign(B, 0.0, Y0 + DH + 0.12, Z0 + T, 1)

# ======================================================================================================== the steel stair
S = Piece('east_s_stair')
TX = IN - 0.08                                         # the treads' ends (2.04 m), the stringers outside them, 1 cm off the walls
SX0, SX1 = IN - 0.065, IN - 0.01


def flight(z_top, y_top, n):                           # n treads down southwards from the nosing at z_top, y_top (risers of R)
    for i in range(1, n + 1):
        y = y_top - i * R; za = z_top - i * G; zb = z_top - (i - 1) * G + 0.02
        S.box(-TX, TX, y - 0.04, y, za, zb, DARK, sides=('+y', '-y', '-z', '+z', '-x', '+x'))
        S.box(-TX, TX, y - 0.06, y - 0.04, zb - 0.05, zb - 0.01, STEEL, sides=('-y', '-z', '+z', '-x', '+x'))      # the nosing's angle
    zl, yl = z_top - n * G, y_top - (n + 1) * R        # the flight's foot: the landing below (n + 1 risers)
    d = 0.3 / math.cos(math.atan2(R, G))               # the stringer: 30 cm deep under the nosing line, 4 mm off the floor below
    for s in (-1, 1):
        poly = ccw([(z_top + 0.02, y_top + 0.05), (z_top + 0.02, y_top - d), (zl + 0.3, yl + 0.004), (zl, yl + 0.004), (zl, yl + R + 0.05)])
        prism(S, s * SX0 if s > 0 else -SX1, s * SX1 if s > 0 else -SX0, poly, STEEL)
    return zl, yl


# top: the first riser at the landing's edge
flight(ZTOP, F2, 17)
# the mid landing: a steel chequer plate on two channels along the walls and a cross channel at each end
S.box(-TX, TX, YM - 0.04, YM, ZM0, ZM1, DARK, sides=('+y', '-y', '-z', '+z', '-x', '+x'))
for s in (-1, 1): S.box(-SX1 if s < 0 else SX0, -SX0 if s < 0 else SX1, YM - 0.3, YM + 0.05, ZM0 + 0.025, ZM1 - 0.005, STEEL)   # 5 mm short of the stringers' ends
for z0, z1 in ((ZM0 + 0.025, ZM0 + 0.1), (ZM1 - 0.1, ZM1 - 0.02)): S.box(-TX, TX, YM - 0.3, YM - 0.04, z0, z1, STEEL, sides=('-y', '-z', '+z', '-x', '+x'))
flight(ZM0, YM, 16)
# brackets: the stringers held to the walls (1 cm), every 1.5 m
nb = 0
for z in [ZTOP - 0.6 - 1.5 * k for k in range(4)] + [ZM0 + 0.9] + [ZM0 - 0.8 - 1.5 * k for k in range(3)]:
    y = (F2 - (ZTOP - z) / G * R) if z > ZM1 else YM if z > ZM0 else (YM - (ZM0 - z) / G * R)
    for s in (-1, 1): S.box(-IN if s < 0 else SX1, -SX1 if s < 0 else IN, y - 0.25, y - 0.05, z - 0.06, z + 0.06, STEEL, sides=('-y', '+y', '-z', '+z', '-x' if s < 0 else '+x', '+x' if s < 0 else '-x')); nb += 1
# the handrails: 4 cm on brackets 7 cm off each wall, 0.9 m over the nosings, straight on along the landings
HR0, HR1 = IN - 0.11, IN - 0.07


def rail(za, ya, zb, yb):                              # a straight rail piece from (za, ya) down to (zb, yb), its top at y
    for s in (-1, 1):                                  # cut plumb at its ends, so the pieces meet without overlapping
        prism(S, -HR1 if s < 0 else HR0, -HR0 if s < 0 else HR1, ccw([(za, ya), (zb, yb), (zb, yb - 0.045), (za, ya - 0.045)]), STEEL)


H = 0.9
rail(Z1 - T - 0.05, F2 + H, ZTOP, F2 + H)
rail(ZTOP, F2 + H, ZM1, YM + H)
rail(ZM1, YM + H, ZM0, YM + H)
rail(ZM0, YM + H, ZF, Y0 + H)
rail(ZF, Y0 + H, SDZ[1] + 0.1, Y0 + H)
for z, y in ((Z1 - T - 0.35, F2 + H), (ZTOP - 2.0, F2 + H - 2.0 / G * R), (ZM1 - 0.9, YM + H), (ZM0 - 2.4, YM + H - 2.4 / G * R), (ZF - 0.3, Y0 + H)):
    for s in (-1, 1): S.box(-IN if s < 0 else HR1, -HR1 if s < 0 else IN, y - 0.12, y - 0.09, z - 0.02, z + 0.02, STEEL, sides=('-y', '+y', '-z', '+z', '-x' if s < 0 else '+x', '+x' if s < 0 else '-x'))

# ======================================================================================================== out
OUT = []
for P in (B, S):
    o = P.finish()
    assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
    print('ESC', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    OUT.append(o.name)
# where they go, and the door leaves (as part 3: id, room, x, z, axis, clear width incl. the 4 cm frames; y the floor)
ROOM = lambda x: 'F1-CLAIM-E' if x < 312 else 'F1-PC-HALL' if x < 372 else 'F1-CREW-CENTRE' if x < 404 else 'F1-CANTEEN' if x < 436 else 'F1-GROUND-OFF' if x < 470 else 'F1-COACH-STATION'
PL, DOORS = [], []
for k, x in enumerate(E.STAIRS):
    n = 'ESC-%02d' % (k + 1)
    for a in OUT: PL.append(['%s/%s' % (n, a[7:].upper()), a, x, 0.0, 0.0])
    DOORS.append(['D-F2-%s-HEAD' % n, 'F2', x, F2, Z1 - T / 2, 'z', DW])
    DOORS.append(['D-F1-%s-OUT' % n, 'F1-PROMENADE' if x < 470 else ROOM(x), x, Y0, Z0 + T / 2, 'z', DW])
    DOORS.append(['D-F1-%s-HALL' % n, ROOM(x), x + HW - T / 2, Y0, sum(SDZ) / 2, 'x', SW])
json.dump({'place': PL, 'doors': DOORS, 'models': OUT, 'stairs': E.STAIRS}, open(os.path.join(HERE, 'esc_place.json'), 'w'), indent=0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'EscapeStairs.blend'))
print('ESC saved', OUT, len(PL), 'places', len(DOORS), 'doors; mid landing', round(YM, 3), 'z', round(ZM0, 2), round(ZM1, 2), 'foot', round(ZF, 2), 'brackets', nb)
