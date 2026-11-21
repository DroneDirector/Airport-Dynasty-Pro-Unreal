# The covered baggage cart - the trailer the bags ride on from the make-up hall to the aircraft, towed in trains of 4-6 by
# the baggage tractor (owner 2026-11-21: beautiful, high detail, our own design to real standards). Built to the vehicle
# standard learned on the pushback tug (shared helpers + checks: Tools/vehicle_kit.py, used as it is; structure as
# Tools/make_gpu.py).
# Real carts it follows (Blueprints/Vehicles/BaggageCart/RESEARCH.md): SPS BW-40 (3977 x 1650 x 1938, 1100 tow-bar, 40 mm
# eye, turntable steering, parking brake on the front wheels set when the tow-bar stands up, 4.00-8 wheels, canvas both
# sides, hot-dip galvanised), Handle-iT HI-ABT-006 (base 2500 x 1250, 400 mm wheels, 160 gsm tarpaulin curtains,
# turntable head), Maximo KDCBC (deck 2850 x 1530, bed 610 high), Metaltech cabin baggage trailer (180 deg turntable,
# lift-to-lock tow-bar).
# Shape: a galvanised chassis (two main members, perimeter channels, cross members) under a 10 mm chequer-plate floor
# 540 mm above the ground; six square corner/middle posts carry side top rails and a crowned aluminium roof on bows; a
# white ribbed steel bulkhead at the front (towards the tractor); a cargo net hooked across the open rear; a PVC curtain
# each side that rolls up onto a tube under the roof edge (straps + buckles to deck hooks, a pull strap in the middle);
# bag rails inside; a turntable-steered front axle with a dead-man drawbar (raised = tyre-tread brake shoes set) and a
# 40 mm towing eye at 400 mm; a rear coupling jaw with a lift pin at the same 400 mm so carts chain; reflectors set into
# the channels (white front, amber sides, red rear).
# Axes: x forward (the drawbar end +x), y left, z up; metres; origin on the ground under the middle of the body.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_baggage_cart.py [-- --wire | --beauty]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import importlib, vehicle_kit as K
importlib.reload(K)
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, lamp, pmat, ALL, IKS

ART = os.path.join(ROOT, 'Art', 'BaggageCart'); os.makedirs(ART, exist_ok=True)
WIRE = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'BaggageCart', 'wire'); os.makedirs(WIRE, exist_ok=True)
PICS = os.path.join(ROOT, 'docs', 'Pictures', 'Vehicles', 'BaggageCart'); os.makedirs(PICS, exist_ok=True)
DO_WIRE, DO_BEAUTY = '--wire' in sys.argv, '--beauty' in sys.argv
K.reset()
def ik(name, loc): return K.ik(name, loc)

# ---------------------------------------------------------------- the main sizes (m)
TD, TW = 0.415, 0.105                # tyre 4.00-8 (415 diameter, 105 wide; no modelled tread)
WZ = TD / 2                          # wheel centre height
TRK = 1.20                           # track (wheel centres y +-0.60)
FX, RX = 0.80, -0.80                 # front axle (turntable centre) / rear axle x - wheelbase 1.60
BX = 1.15; BY = 0.70                 # body: x -1.15..1.15 (2300), y +-0.70 (1400)
FZ0, FZ1 = 0.43, 0.535               # chassis members (bottom / top)
DECK = 0.540                         # floor top (10 mm chequer plate 0.530..0.540)
PX, PY, PS = 1.12, 0.67, 0.06        # post centres x +-1.12 (and 0), y +-0.67; 60 x 60 tube
PZ1 = 1.843                          # posts' tops (into the top rails)
TR0, TR1 = 1.840, 1.903              # side top rails (the roof sits on them)
ROOF_Y, ROOF_X, ROOF_T, CROWN = 0.80, 1.19, 0.012, 0.06
CT_Y, CT_Z, CT_R = 0.75, 1.84, 0.025           # the curtain tube (each side): outboard of the top rail, so the full roll
                                                 # (r 45 mm) clears the rail and stays above the 1780 load opening
CUR_Y = CT_Y + CT_R                              # the curtain hangs on the tube's outer side (y +-0.75)
CUR_Z1, CUR_Z0 = CT_Z, 0.545                     # curtain top (the roll) / hem centre
ROLL_R = 0.032                                   # the roll as modelled (closed: 2-3 turns on the tube)
CUR_K = 0.97                                     # how far the hem travels up (fraction of tube - hem)
ROLL_OPEN = 1.4                                  # its radial scale when fully open (r 45 mm)
OPEN_Z = 1.78                                    # the top of the clear load opening (sides and rear)
DB_P = Vector((1.30, 0.0, 0.30))                 # the drawbar hinge (pin along y)
EYE = Vector((2.01, 0.0, 0.40))                  # the towing eye centre (40 mm hole) - towing height 400
PIN = Vector((-1.27, 0.0, 0.40))                 # the rear coupling pin / the next cart's eye centre

# ---------------------------------------------------------------- materials (plain; CC0 photos swapped in below)
M_GALV = pmat('BC_Galvanised', (0.62, 0.64, 0.66), 1.0, 0.45)
M_ALU = pmat('BC_RoofAlu', (0.78, 0.79, 0.80), 1.0, 0.35)
M_STEEL = pmat('BC_Steel', (0.40, 0.41, 0.43), 1.0, 0.4)
M_DARK = pmat('BC_DarkSteel', (0.08, 0.08, 0.09), 0.6, 0.55)
M_WHITEP = pmat('BC_WhitePaint', (0.90, 0.90, 0.88), 0.0, 0.4)
M_FLOOR = pmat('BC_FloorPlate', (0.55, 0.56, 0.58), 1.0, 0.5)
M_CURTAIN = pmat('BC_CurtainPVC', (0.05, 0.10, 0.28), 0.0, 0.45)
M_STRAP = pmat('BC_Webbing', (0.03, 0.03, 0.035), 0.0, 0.7)
M_NET = pmat('BC_NetCord', (0.04, 0.04, 0.04), 0.0, 0.8)
M_RUB = pmat('BC_Tyre', (0.03, 0.03, 0.03), 0.0, 0.9)
M_RIM = pmat('BC_RimPaint', (0.75, 0.76, 0.78), 0.3, 0.45)
M_PLAST = pmat('BC_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_YELP = pmat('BC_YellowPaint', (0.95, 0.75, 0.05), 0.0, 0.4)
M_RED = pmat('BC_RedReflector', (0.9, 0.05, 0.03), 0.0, 0.2, 0.4)
M_AMBER = pmat('BC_AmberReflector', (1.0, 0.55, 0.05), 0.0, 0.2, 0.4)
M_WHITE = pmat('BC_WhiteReflector', (0.95, 0.95, 0.9), 0.0, 0.1, 0.3)

def prism_yz(name, pts_yz, x0, x1, m, bevel=0.0, segs=1, smooth=30):
    """An end outline (y, z) pushed along x0..x1 (the roof, the roof bows, the bulkhead bows)."""
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for y, z in pts_yz]; b = [bm.verts.new((x1, y, z)) for y, z in pts_yz]
    bm.faces.new(a[::-1]); bm.faces.new(b)
    n = len(pts_yz)
    for i in range(n): bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = K.link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = segs; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30); K.apply_mods(o)
    return K.finish(o, m, smooth)
def roof_under(y):
    """The roof's underside height across the cart: a crown over the load space, flat over the top rails and the overhang."""
    return 1.90 + CROWN * max(0.0, 1.0 - (y / 0.64) ** 2)
def join(name, objs, m):
    """Joins helper-built pieces into one part (the cargo net)."""
    for o in objs: ALL.remove(o)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]; bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.name = name; o.data.name = name
    o.data.materials.clear(); o.data.materials.append(m); o.select_set(False); ALL.append(o); return o

# ================================================================ CHASSIS (hot-dip galvanised steel)
for s, nm in ((1, 'L'), (-1, 'R')):
    box(f'MainMember_{nm}', (0, s * 0.40, (FZ0 + FZ1 - 0.002) / 2), (2.20, 0.08, FZ1 - 0.002 - FZ0), M_GALV, bevel=0.006, segs=1)
    box(f'DeckEdge_{nm}', (0, s * (BY - 0.03), (FZ0 + FZ1) / 2), (2 * BX, 0.06, FZ1 - FZ0), M_GALV, bevel=0.006, segs=1)
for e, x in (('F', BX - 0.03), ('B', -BX + 0.03)):
    box(f'DeckEnd_{e}', (x, 0, (FZ0 + 0.003 + FZ1 - 0.003) / 2), (0.06, 2 * (BY - 0.035), FZ1 - FZ0 - 0.006), M_GALV, bevel=0.006, segs=1)
for k, x in enumerate((-0.55, 0.0, 0.42)):
    box(f'CrossMember_{k}', (x, 0, (0.45 + FZ1 - 0.004) / 2), (0.06, 2 * (BY - 0.045), FZ1 - 0.004 - 0.45), M_GALV, bevel=0.004, segs=1)
# the floor: 10 mm chequer plate over the whole frame, its edges chamfered
box('FloorPlate', (0, 0, DECK - 0.005), (2 * BX - 0.004, 2 * BY - 0.004, 0.0105), M_FLOOR, bevel=0.003, segs=1)
# the turntable: a plate under the main members' front ends, the fixed upper ring under it
box('TurntablePlate', (FX, 0, 0.42475), (0.64, 0.92, 0.0135), M_GALV, bevel=0.004, segs=1)
cyl('TurntableTop', (FX, 0, 0.410), 0.30, 0.020, 'z', M_DARK, 64, bevel=0.003)
for k in range(10):
    a = k * math.pi / 5 + math.pi / 10
    cyl(f'TurntableBolt_{k}', (FX + 0.315 * math.cos(a), 0.315 * math.sin(a), 0.415), 0.010, 0.012, 'z', M_STEEL, 6)   # (heads under the plate, outside the rings)
# the rear axle: a square beam on two hangers under the main members (no springs on these carts)
box('RearAxle_Beam', (RX, 0, WZ), (0.08, 1.06, 0.08), M_DARK, bevel=0.006)
for s in (-1, 1):
    box(f'RearAxle_Hanger_{s}', (RX, s * 0.40, (0.243 + FZ0 + 0.003) / 2), (0.12, 0.08, FZ0 + 0.003 - 0.243), M_GALV, bevel=0.005, segs=1)
    prism(f'RearAxle_Gusset_{s}', [(RX - 0.058, FZ0 - 0.002), (RX - 0.16, FZ0 - 0.002), (RX - 0.058, 0.30)], s * 0.40 - 0.004, s * 0.40 + 0.004, M_GALV)
    prism(f'RearAxle_GussetF_{s}', [(RX + 0.058, FZ0 - 0.002), (RX + 0.16, FZ0 - 0.002), (RX + 0.058, 0.30)], s * 0.40 - 0.004, s * 0.40 + 0.004, M_GALV)

# ================================================================ FRONT AXLE (FrontAxle_*: steers about z through (FX, 0))
cyl('FrontAxle_Ring', (FX, 0, 0.389), 0.29, 0.020, 'z', M_DARK, 64, bevel=0.003)
box('FrontAxle_Bolster', (FX, 0, (0.245 + 0.382) / 2), (0.24, 0.56, 0.382 - 0.245), M_DARK, bevel=0.008)
box('FrontAxle_Beam', (FX, 0, WZ), (0.08, 1.06, 0.08), M_DARK, bevel=0.006)
# the drawbar yoke: two arms from the beam forward to the hinge, a tie between them
for s in (-1, 1):
    prism(f'FrontAxle_YokeArm_{s}', [(FX + 0.03, 0.24), (DB_P.x + 0.04, 0.26), (DB_P.x + 0.04, 0.345), (FX + 0.10, 0.36), (FX + 0.03, 0.36)], s * 0.16, s * 0.18, M_DARK, bevel=0.004)
    cyl(f'FrontAxle_ClevisNut_{s}', (DB_P.x, s * 0.185, DB_P.z), 0.020, 0.016, 'y', M_STEEL, 6)
box('FrontAxle_YokeTie', (1.10, 0, 0.34), (0.05, 0.34, 0.04), M_DARK, bevel=0.004)
cyl('FrontAxle_ClevisPin', tuple(DB_P), 0.014, 0.39, 'y', M_STEEL, 16)
# the dead-man brake: a shaft across in front of the tyres, carried in two bearings under the yoke; a shoe with a
# rubber pad 8 mm off each front tyre's tread; the lever up to the rod that the drawbar's cam pulls when it stands up
BSX = 1.065
cyl('FrontAxle_BrakeShaft', (BSX, 0, 0.21), 0.015, 1.24, 'y', M_STEEL, 16)
for s in (-1, 1):
    box(f'FrontAxle_BrakeBearing_{s}', (BSX, s * 0.17, 0.2375), (0.05, 0.04, 0.065), M_DARK, bevel=0.004)
    box(f'FrontAxle_BrakeShoe_{s}', (1.050, s * 0.60, 0.21), (0.04, 0.08, 0.12), M_DARK, bevel=0.004)
    box(f'FrontAxle_BrakePad_{s}', (1.0235, s * 0.60, 0.21), (0.015, 0.07, 0.11), M_RUB, bevel=0.003)
box('FrontAxle_BrakeLever', (BSX, 0, 0.238), (0.024, 0.03, 0.075), M_DARK, bevel=0.003)
tube('FrontAxle_BrakeRod', [(BSX, 0, 0.268), (DB_P.x - 0.05, 0, 0.250)], 0.008, M_STEEL, bez=False)

# ================================================================ WHEELS (4.00-8 on 8-inch split rims)
tug_prof = [(0.290, -0.124), (0.318, -0.146), (0.360, -0.157), (0.410, -0.1585), (0.460, -0.154), (0.500, -0.142), (0.522, -0.124),
            (0.534, -0.100), (0.538, -0.060), (0.539, 0.0), (0.538, 0.060), (0.534, 0.100), (0.522, 0.124), (0.500, 0.142),
            (0.460, 0.154), (0.410, 0.1585), (0.360, 0.157), (0.318, 0.146), (0.290, 0.124), (0.286, 0.0)]
tug_rim = [(0.300, -0.130), (0.296, -0.122), (0.284, -0.118), (0.270, -0.070), (0.262, -0.050), (0.262, 0.050), (0.270, 0.070),
           (0.284, 0.118), (0.296, 0.122), (0.300, 0.130), (0.288, 0.132), (0.276, 0.122), (0.256, 0.060), (0.256, -0.060), (0.276, -0.122), (0.288, -0.132)]
for ax, grp, F in ((FX, 'FrontAxle_', 'F'), (RX, 'RearAxle_', 'R')):
    for s, sd in ((1, 'L'), (-1, 'R')):
        key = F + sd; y = s * TRK / 2
        cyl(f'{grp}Stub_{sd}', (ax, s * 0.5625, WZ), 0.020, 0.111, 'y', M_STEEL, 20)
        # the tyre: turned from its cross-section - bead on the rim, bulging sidewalls, rounded shoulders, crown (the tread
        # is the texture's, never modelled)
        lathe(f'Tyre_{key}', [(0.102 + (r - 0.29) * (0.1055 / 0.249), yy * (TW / 2 / 0.1585)) for r, yy in tug_prof], (ax, y, WZ), M_RUB, 72)
        lathe(f'Rim_{key}', [(0.102 + (r - 0.29) * 0.45, yy * 0.33) for r, yy in tug_rim], (ax, y, WZ), M_RIM, 72)
        disc = cyl(f'WheelDisc_{key}', (ax, y + s * 0.018, WZ), 0.088, 0.008, 'y', M_RIM, 64, bevel=0.002)
        for k in range(4):
            a = k * math.pi / 2 + math.pi / 4
            cut(disc, cyl(f'HandHole_{key}{k}', (ax + 0.064 * math.cos(a), y + s * 0.018, WZ + 0.064 * math.sin(a)), 0.012, 0.03, 'y', M_DARK, 20))
        cyl(f'Hub_{key}', (ax, y + s * 0.030, WZ), 0.036, 0.020, 'y', M_DARK, 32, bevel=0.004)
        cyl(f'HubCap_{key}', (ax, y + s * 0.043, WZ), 0.020, 0.010, 'y', M_DARK, 24, bevel=0.003)
        for k in range(4):
            a = k * math.pi / 2
            cyl(f'Nut_{key}{k}', (ax + 0.046 * math.cos(a), y + s * 0.0245, WZ + 0.046 * math.sin(a)), 0.007, 0.010, 'y', M_STEEL, 6)
        ik(f'Wheel_{key}', (ax, y, WZ))          # (the wheel group's hub centre: Tyre_/Rim_/WheelDisc_/Hub_/HubCap_/Nut_ + key)

# ================================================================ THE DRAWBAR (Drawbar*: turns about y through DB_P; lowered =
# towing (eye at 400), raised = parked + brake on; it steers the front axle through the yoke)
SLOPE = (EYE.z - DB_P.z) / (EYE.x - DB_P.x); ANG = math.degrees(math.atan(SLOPE))
zb = lambda x: DB_P.z + SLOPE * (x - DB_P.x)
cyl('Drawbar_PivotTube', tuple(DB_P), 0.030, 0.30, 'y', M_GALV, 24, bevel=0.003)
BEAM_X0 = 1.62
for s in (-1, 1):
    tube(f'Drawbar_Arm_{s}', [(DB_P.x + 0.005, s * 0.12, DB_P.z), (BEAM_X0 + 0.02, s * 0.018, zb(BEAM_X0 + 0.02))], 0.020, M_GALV, bez=False, bres=4)
bc = Vector(((BEAM_X0 + 1.965) / 2, 0, zb((BEAM_X0 + 1.965) / 2)))
turn(box('Drawbar_Beam', tuple(bc), (1.965 - BEAM_X0, 0.06, 0.06), M_GALV, bevel=0.007), tuple(bc), 'y', -ANG)
torus('Drawbar_Eye', tuple(EYE), 0.036, 0.016, 'z', M_STEEL)           # 40 mm towing eye (lunette)
box('Drawbar_EyeWeld', (1.965, 0, zb(1.965)), (0.036, 0.05, 0.034), M_STEEL, bevel=0.006)
# the hand loop over the beam behind the eye (IK_Drawbar_Handle)
tube('Drawbar_Handle', [(1.72, 0, zb(1.72) + 0.025), (1.74, 0, zb(1.74) + 0.085), (1.86, 0, zb(1.86) + 0.085), (1.88, 0, zb(1.88) + 0.025)], 0.013, M_YELP, bez=False)
# the brake cam on the pivot tube (pointing down; standing the drawbar up swings it forward and pulls the brake rod)
box('Drawbar_BrakeCam', (DB_P.x - 0.03, 0, DB_P.z - 0.04), (0.06, 0.03, 0.06), M_GALV, bevel=0.004)

# ================================================================ BODY: posts, top rails, gussets, bag rails
POSTS = {'FL': (PX, PY), 'FR': (PX, -PY), 'RL': (-PX, PY), 'RR': (-PX, -PY)}   # (corner posts only: one clear opening each side)
for nm, (x, y) in POSTS.items():
    box(f'Post_{nm}', (x, y, (DECK - 0.003 + PZ1) / 2), (PS, PS, PZ1 - DECK + 0.003), M_GALV, bevel=0.005, segs=1)
    box(f'PostShoe_{nm}', (x + (0.008 if x > 0 else -0.008), y + (0.008 if y > 0 else -0.008), DECK + 0.029), (PS + 0.012, PS + 0.012, 0.061), M_GALV, bevel=0.003, segs=1)
    # top gussets along the rail (each side of a middle post, the inner side of a corner post)
    for d in ((-1, 1) if x == 0 else ((-1,) if x > 0 else (1,))):
        x0 = x + d * PS / 2
        prism(f'GussetTop_{nm}_{d}', [(x0 - d * 0.003, TR0 - 0.017), (x0 + d * 0.10, TR0 - 0.017), (x0 - d * 0.003, OPEN_Z + 0.006)], y - 0.004, y + 0.004, M_GALV)
for s, nm in ((1, 'L'), (-1, 'R')):
    box(f'TopRail_{nm}', (0, s * PY, (TR0 - 0.02 + TR1) / 2), (2 * BX - 0.006, PS + 0.004, TR1 - TR0 + 0.02), M_GALV, bevel=0.005, segs=1)
for e, x in (('F', PX), ('B', -PX)):
    box(f'TopRail_{e}', (x, 0, (TR0 + 0.002 + TR1 - 0.006) / 2), (PS, 2 * (PY - PS / 2) + 0.006, TR1 - 0.006 - TR0 - 0.002), M_GALV, bevel=0.005, segs=1)

# ---------------------------------------------------------------- the roof: crowned aluminium sheet on end and middle bows
NP = 41
ys = [-ROOF_Y + 2 * ROOF_Y * i / (NP - 1) for i in range(NP)]
prism_yz('Roof', [(y, roof_under(y)) for y in ys] + [(y, roof_under(y) + ROOF_T) for y in reversed(ys)], -ROOF_X, ROOF_X, M_ALU, smooth=60)
for s, nm in ((1, 'L'), (-1, 'R')):
    cyl(f'RoofBead_{nm}', (0, s * ROOF_Y, 1.90 + ROOF_T / 2), 0.009, 2 * ROOF_X, 'x', M_ALU, 16)
for e, x in (('F', PX), ('B', -PX), ('M0', -0.56), ('M1', 0.56)):
    yb = [-(PY + 0.02) + 2 * (PY + 0.02) * i / 30 for i in range(31)]
    if e in ('F', 'B'):        # end bows close the crown down to the top cross rail
        pts = [(y, roof_under(y) + 0.003) for y in yb] + [(PY + 0.02, TR1 - 0.008), (-(PY + 0.02), TR1 - 0.008)]
        prism_yz(f'RoofBow_{e}', pts, x - PS / 2 + 0.004, x + PS / 2 - 0.004, M_GALV)
    else:                      # middle bows: a 30 mm arched flat under the sheet, ends on the side top rails
        pts = [(y, roof_under(y) + 0.003) for y in yb] + [(y, roof_under(y) - 0.03) for y in reversed(yb)]
        prism_yz(f'RoofBow_{e}', pts, x - 0.02, x + 0.02, M_GALV)

# ---------------------------------------------------------------- the front bulkhead (towards the tractor): white steel,
# pressed horizontal ribs on its front face, a blank ID plate (the fleet number is written at run time)
box('Bulkhead', (PX, 0, (DECK - 0.003 + TR0 + 0.003) / 2), (0.02, 2 * (PY - PS / 2) + 0.01, TR0 - DECK + 0.006), M_WHITEP, bevel=0.002, segs=1)
for k, z in enumerate((0.72, 0.92, 1.12, 1.32, 1.52)):
    box(f'BulkheadRib_{k}', (PX + 0.016, 0, z), (0.016, 1.16, 0.03), M_WHITEP, bevel=0.005, segs=2)
box('IDPlate', (PX + 0.0125, 0, 1.68), (0.007, 0.30, 0.12), M_GALV, bevel=0.002)
for k, (y, z) in enumerate(((-0.13, 1.635), (0.13, 1.635), (-0.13, 1.725), (0.13, 1.725))):
    cyl(f'IDPlateRivet_{k}', (PX + 0.0165, y, z), 0.006, 0.004, 'x', M_STEEL, 12)

# ---------------------------------------------------------------- the rear net gate (NetGate*): a cargo net on a light galvanised
# tube frame, hung on two hinges on the left rear post (axis vertical), a latch bolt into a keeper on the right rear post,
# a handle by the latch. Opens outward (negative deg about +z) up to -180 deg, clearing the whole rear opening.
GX = -PX - PS / 2 - 0.025                       # the gate's plane (behind the rear posts' faces) = the hinge axis x
GH = Vector((GX, PY + PS / 2 + 0.005, 0.0))     # the hinge axis (vertical) through (GX, 0.705)
GY0, GY1, GZ0, GZ1, GR = -(PY - 0.002), PY - 0.002, 0.625, 1.80, 0.018     # frame tube centre lines; tube radius
tube('NetGate_Frame', [(GX, GY1, GZ0), (GX, GY0, GZ0), (GX, GY0, GZ1), (GX, GY1, GZ1)], GR, M_GALV, bez=False, bres=3)
cyl('NetGate_FrameHinge', (GX, GY1, (GZ0 + GZ1) / 2), GR, GZ1 - GZ0 + 2 * GR - 0.004, 'z', M_GALV, 20)
cords = []
for k in range(7):
    y = -0.60 + k * 0.20
    cords.append(tube(f'NetV_{k}', [(GX, y, GZ0 + 0.01), (GX, y, GZ1 - 0.01)], 0.005, M_NET, bez=False, bres=2))
for k in range(7):
    z = GZ0 + 0.16 + k * 0.165
    cords.append(tube(f'NetH_{k}', [(GX, GY0 + 0.01, z), (GX, GY1 - 0.01, z)], 0.005, M_NET, bez=False, bres=2))
join('NetGate_Net', cords, M_NET)
HZS = (0.85, 1.55)
for k, z in enumerate(HZS):
    box(f'GateHinge_{k}_Base', (-PX - PS / 2 - 0.006, PY + PS / 2 - 0.015, z - 0.02), (0.020, 0.038, 0.05), M_STEEL, bevel=0.002)   # leaf on the post's rear face
    box(f'GateHinge_{k}_Arm', (-PX - PS / 2 - 0.0135, GH.y - 0.004, z - 0.02), (0.019, 0.016, 0.03), M_STEEL, bevel=0.002)          # leaf out to the knuckle
    cyl(f'GateHinge_{k}', (GX, GH.y, z - 0.02), 0.013, 0.04, 'z', M_STEEL, 16)                                                    # fixed knuckle
    cyl(f'NetGate_Knuckle_{k}', (GX, GH.y, z + 0.022), 0.0115, 0.04, 'z', M_STEEL, 16)                                            # the gate's knuckle
    box(f'NetGate_HingeLeaf_{k}', (GX, (GH.y + GY1) / 2, z + 0.022), (0.010, GH.y - GY1, 0.03), M_STEEL, bevel=0.002)
# the latch: a keeper on the right rear post, the bolt in it (part of the gate), the handle loop by it
LZ = 1.20
box('GateKeeper', (-PX - PS / 2 - 0.016, -(PY + PS / 2) + 0.012, LZ), (0.036, 0.024, 0.06), M_STEEL, bevel=0.003)
cyl('NetGate_LatchBolt', (GX, -(PY + 0.0125), LZ), 0.008, 0.045, 'y', M_STEEL, 12)
cyl('NetGate_LatchBox', (GX, GY0 + 0.012, LZ), 0.015, 0.05, 'y', M_GALV, 16)
tube('NetGate_Handle', [(GX - 0.008, GY0, LZ - 0.10), (GX - 0.07, GY0, LZ - 0.12), (GX - 0.07, GY0, LZ - 0.20), (GX - 0.008, GY0, LZ - 0.22)], 0.011, M_YELP, bez=True)
ik('NetGate_Handle', (GX - 0.07, GY0, LZ - 0.16)); ik('NetGate_Latch', (GX - 0.02, -(PY + 0.012), LZ)); ik('NetGate_Hinge', (GX, GH.y, 1.20))
ik('Stand_Rear', (-1.95, -0.35, 0.0))

# ---------------------------------------------------------------- the side curtains (PVC, roll up onto a tube under the roof edge)
CURT = {}
for s, nm in ((1, 'L'), (-1, 'R')):
    # the tube + its brackets on the corner posts (fixed)
    for e, x in (('F', PX), ('B', -PX)):
        box(f'CurtainBracket_{nm}{e}', (x, s * (BY + 0.0485), CT_Z), (PS, 0.103, 0.08), M_GALV, bevel=0.004)
    cyl(f'CurtainTube_{nm}', (0, s * CT_Y, CT_Z), CT_R, 2 * (PX + 0.005), 'x', M_GALV, 24)
    # the end seals: PVC flaps on the corner posts closing the slot between the post and the hanging curtain (fixed)
    for e, sx in (('F', 1), ('B', -1)):
        box(f'CurtainSeal_{nm}{e}', (sx * (PX - PS / 2 + 0.009), s * (BY + 0.0415), (DECK + 0.004 + 1.795) / 2), (0.008, 0.087, 1.795 - DECK - 0.004), M_CURTAIN)
    # the roll (CurtainRoll_*): as modelled = closed (2-3 turns left on the tube); scales up round the tube's axis as it opens
    cyl(f'CurtainRoll_{nm}', (0, s * CT_Y, CT_Z), ROLL_R, 2 * (PX - PS / 2) - 0.004, 'x', M_CURTAIN, 32)
    # the curtain sheet: pleats (soft vertical folds, outward only), 2 mm PVC
    XL = PX - PS / 2 - 0.006; NXS = 110; NZS = 26
    bm = bmesh.new(); grid = []
    for j in range(NZS + 1):
        z = CUR_Z1 - (CUR_Z1 - (CUR_Z0 - 0.004)) * j / NZS
        amp = 0.007 * (0.25 + 0.75 * K.ss(CUR_Z1, CUR_Z1 - 0.30, z))
        row = []
        for i in range(NXS + 1):
            x = -XL + 2 * XL * i / NXS
            row.append(bm.verts.new((x, s * (CUR_Y + 0.001 + amp * (0.5 + 0.5 * math.sin(2 * math.pi * x / 0.22))), z)))
        grid.append(row)
    for j in range(NZS):
        for i in range(NXS): bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    cur = K.link(f'Curtain_{nm}', bm)
    md = cur.modifiers.new('t', 'SOLIDIFY'); md.thickness = 0.002; md.offset = 0.0; K.apply_mods(cur); K.finish(cur, M_CURTAIN, 60)
    # the hem: a weighted tube sewn into the bottom edge
    cyl(f'Curtain_{nm}_Hem', (0, s * (CUR_Y + 0.004), CUR_Z0), 0.012, 2 * XL, 'x', M_CURTAIN, 20)
    # tie-down straps with buckles, hooked under hooks on the deck channel (below the floor: never in the opening)
    for k, x in enumerate((-0.85, -0.30, 0.30, 0.85)):
        box(f'Curtain_{nm}_Strap_{k}', (x, s * (CUR_Y + 0.020), 0.545), (0.035, 0.004, 0.15), M_STRAP)
        box(f'Curtain_{nm}_Buckle_{k}', (x, s * (CUR_Y + 0.020), 0.585), (0.045, 0.010, 0.035), M_STEEL, bevel=0.002)
        hy = CUR_Y + 0.020 - BY
        box(f'DeckHook_{nm}{k}', (x, s * (BY + hy / 2 - 0.002), 0.480), (0.034, hy + 0.004, 0.02), M_STEEL, bevel=0.003)
        box(f'DeckHookTip_{nm}{k}', (x, s * (CUR_Y + 0.020), 0.4865), (0.04, 0.016, 0.037), M_STEEL, bevel=0.003)
        ik(f'Curtain_{nm}_Strap_{k}', (x, s * (CUR_Y + 0.025), 0.585))     # (the hand on the buckle: unhook before rolling up, hook after pulling down)
    # the pull strap in the middle of the hem (the loader pushes the roll up by it / pulls the hem down by it)
    tube(f'Curtain_{nm}_PullStrap', [(0, s * (CUR_Y + 0.004), 0.60), (0, s * (CUR_Y + 0.035), 0.63), (0, s * (CUR_Y + 0.035), 0.70), (0, s * (CUR_Y + 0.004), 0.73)], 0.008, M_YELP, bez=True)
    CURT[nm] = {'sheet_pivot_z': CUR_Z1, 'roll_axis': [0.0, s * CT_Y, CT_Z]}
    # the hands: closed (pull strap / hem, two hands 0.25 m apart) and open (the hem bar tucked under the full roll,
    # reached from the ground to pull it down)
    ik(f'Curtain_{nm}_Handle', (0, s * (CUR_Y + 0.035), 0.665))
    ik(f'Curtain_{nm}_HandL', (0.125 * s, s * (CUR_Y + 0.016), CUR_Z0 + 0.01)); ik(f'Curtain_{nm}_HandR', (-0.125 * s, s * (CUR_Y + 0.016), CUR_Z0 + 0.01))
    ZUP = CUR_Z0 + CUR_K * (CT_Z - CUR_Z0)     # the hem bar when open (tucked under the roll)
    ik(f'Curtain_{nm}_HandleUp', (0, s * (CUR_Y + 0.02), ZUP))
    ik(f'Curtain_{nm}_HandLUp', (0.125 * s, s * (CUR_Y + 0.016), ZUP)); ik(f'Curtain_{nm}_HandRUp', (-0.125 * s, s * (CUR_Y + 0.016), ZUP))
    ik(f'Stand_Load_{nm}', (0.0, s * 1.30, 0.0)); ik(f'Stand_Curtain_{nm}', (0.0, s * 1.15, 0.0))

# ---------------------------------------------------------------- the rear coupling jaw (welded to the rear channel; fixed)
# and its lift pin (CouplerRear_Pin*: lifts +z 100 mm to open; the next cart's eye then drops in)
box('CouplerRear_Web', (-BX - 0.006, 0, 0.43), (0.018, 0.18, 0.18), M_GALV, bevel=0.003, segs=1)
for s in (-1, 1):
    prism(f'CouplerRear_Gusset_{s}', [(-BX - 0.012, 0.345), (-BX - 0.012, FZ0 + 0.002), (-1.05, FZ0 + 0.002)], s * 0.085 - 0.006, s * 0.085 + 0.006, M_GALV)
prism('CouplerRear_JawTop', [(-BX - 0.01, 0.43), (-1.33, 0.43), (-1.37, 0.455), (-1.37, 0.47), (-1.33, 0.445), (-BX - 0.01, 0.445)], -0.07, 0.07, M_GALV, bevel=0.003)
prism('CouplerRear_JawBottom', [(-BX - 0.01, 0.355), (-1.33, 0.355), (-1.37, 0.33), (-1.37, 0.345), (-1.33, 0.37), (-BX - 0.01, 0.37)], -0.07, 0.07, M_GALV, bevel=0.003)
cyl('CouplerRear_Guide', (PIN.x, 0, 0.473), 0.024, 0.06, 'z', M_GALV, 24, bevel=0.003)
cyl('CouplerRear_Pin', (PIN.x, 0, 0.430), 0.0155, 0.19, 'z', M_STEEL, 20, bevel=0.002)
cyl('CouplerRear_PinCollar', (PIN.x, 0, 0.5225), 0.022, 0.015, 'z', M_STEEL, 20, bevel=0.002)
torus('CouplerRear_PinHandle', (PIN.x, 0, 0.5585), 0.030, 0.007, 'x', M_YELP, 32, 10)

# ---------------------------------------------------------------- reflectors, each a housing set into the channel web
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'ReflectorFront_{nm}', (BX, s * 0.50, 0.4825), 'x+', 0.09, 0.035, M_WHITE, M_DARK, M_PLAST, depth=0.02)
    lamp(f'ReflectorRear_{nm}', (-BX, s * 0.50, 0.4825), 'x-', 0.09, 0.035, M_RED, M_DARK, M_PLAST, depth=0.02)
    for k, x in enumerate((1.0, -1.0)):
        lamp(f'ReflectorSide_{nm}{k}', (x, s * BY, 0.4825), 'y' + ('+' if s > 0 else '-'), 0.08, 0.035, M_AMBER, M_DARK, M_PLAST, depth=0.02)

# ---------------------------------------------------------------- IK + pivots for the game
ik('TowEye', tuple(EYE)); ik('Drawbar_Handle', (1.80, 0.0, zb(1.80) + 0.085)); ik('Drawbar_Pivot', tuple(DB_P))
ik('FrontAxle_Pivot', (FX, 0.0, 0.389))
ik('Coupler_Rear', tuple(PIN)); ik('Coupler_Pin', (PIN.x, 0.0, 0.5585 + 0.037))
ik('Load_Min', (-PX + PS / 2 + 0.02, -(PY - PS / 2 - 0.035), DECK)); ik('Load_Max', (PX - PS / 2 - 0.02, PY - PS / 2 - 0.035, OPEN_Z))
ik('Stand_Drawbar', (2.25, 0.40, 0.0)); ik('Stand_Coupler', (-1.75, 0.45, 0.0))

# ================================================================ checks
SKIP_OH = ('FrontAxle_BrakeRod', 'Drawbar_EyeWeld', 'Drawbar_PivotTube', 'Drawbar_BrakeCam', 'CouplerRear_Guide', 'CouplerRear_PinCollar',
           'CouplerRear_Web', 'FrontAxle_YokeTie', 'FrontAxle_BrakeLever')
OH_PREFIX = ('FrontAxle_Stub', 'RearAxle_Stub', 'RearAxle_Hanger', 'RearAxle_Gusset', 'Nut_', 'Hub', 'Drawbar_Arm', 'Drawbar_Handle', 'FrontAxle_Clevis', 'FrontAxle_Brake',
             'TurntableBolt', 'Curtain_', 'DeckHook', 'NetHook', 'CurtainBracket', 'CurtainSeal', 'PostShoe', 'GussetTop', 'CouplerRear_', 'IDPlateRivet', 'FrontAxle_YokeArm', 'NetGate', 'GateHinge', 'GateKeeper')
fl, cl, oh, lo = K.checks('BC', ground_ok=('Tyre',), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)
FRONT_GRP = ('FrontAxle_', 'Tyre_F', 'Rim_F', 'WheelDisc_F', 'Hub_F', 'HubCap_F', 'Nut_F', 'Drawbar')
steer_hits = K.sweep(FRONT_GRP, (FX, 0, 0), 'z', (-80, -60, -40, -20, 20, 40, 60, 80), static_skip_prefix=('TurntableTop',))
print('BC CHECK steering sweep +-80 deg: %d hits %s' % (len(steer_hits), sorted(steer_hits)[:12]))
db_hits = K.sweep('Drawbar', tuple(DB_P), 'y', (-20, -45, -70, -82), static_skip_prefix=('FrontAxle_',))
print('BC CHECK drawbar raised to -82 deg: %d hits %s' % (len(db_hits), sorted(db_hits)[:12]))
# the next cart's towing eye fits the jaw round the pin without touching it (a test eye, removed again)
te = torus('TestEye', tuple(PIN), 0.036, 0.016, 'z', M_STEEL); ALL.remove(te); tt = K.bvh(te)
eye_hits = sorted(o.name for o in ALL if K.touch(K.bbox(te), K.bbox(o)) and tt.overlap(K.bvh(o)))
bpy.data.objects.remove(te, do_unlink=True)
print('BC CHECK next cart eye in the jaw: %d hits %s' % (len(eye_hits), eye_hits))
# the pin lifted 100 mm clears the eye's path (only its own guide/collar stay round it)
from mathutils.bvhtree import BVHTree
T = Matrix.Translation((0, 0, 0.10)); pin_hits = set()
for o in ALL:
    if not o.name.startswith('CouplerRear_Pin'): continue
    t = BVHTree.FromPolygons([T @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
    for p2 in ALL:
        if p2.name.startswith(('CouplerRear_Pin', 'CouplerRear_Guide', 'CouplerRear_JawTop')): continue
        if t.overlap(K.bvh(p2)): pin_hits.add((o.name, p2.name))
print('BC CHECK pin lifted: %d hits %s' % (len(pin_hits), sorted(pin_hits)))

# the net gate swings open to -180 deg about its hinge without touching anything fixed (its own fixed knuckles excepted)
gate_hits = K.sweep('NetGate', tuple(GH), 'z', (-10, -30, -60, -90, -120, -150, -180), static_skip_prefix=('GateHinge',))
print('BC CHECK net gate open to -180 deg: %d hits %s' % (len(gate_hits), sorted(gate_hits)[:12]))
# the open state (curtains rolled up, gate open): nothing in the side or rear load openings
def open_state(o):
    """The matrix that puts a part where it is with both curtains fully up and the gate open (None: hidden then)."""
    n = o.name
    for nm, sg in (('L', 1), ('R', -1)):
        ax = Vector((0, sg * CT_Y, CT_Z))
        if n == f'Curtain_{nm}': return Matrix.Translation((0, 0, CT_Z)) @ Matrix.Diagonal((1, 1, 1 - CUR_K, 1)) @ Matrix.Translation((0, 0, -CT_Z))
        if n == f'Curtain_{nm}_Hem': return Matrix.Translation((0, 0, CUR_K * (CT_Z - CUR_Z0)))
        if n.startswith(f'Curtain_{nm}_'): return None          # straps, buckles, pull strap: wrapped into the roll
        if n == f'CurtainRoll_{nm}': return Matrix.Translation(ax) @ Matrix.Diagonal((1, ROLL_OPEN, ROLL_OPEN, 1)) @ Matrix.Translation(-ax)
    if n.startswith('NetGate'): return Matrix.Translation(GH) @ Matrix.Rotation(math.radians(-180), 4, 'Z') @ Matrix.Translation(-GH)
    return Matrix.Identity(4)
OPENINGS = {'side_L': ((-PX + PS / 2 + 0.001, PX - PS / 2 - 0.001), (PY - PS / 2, 0.86), (DECK + 0.004, OPEN_Z)),
            'side_R': ((-PX + PS / 2 + 0.001, PX - PS / 2 - 0.001), (-0.86, -(PY - PS / 2)), (DECK + 0.004, OPEN_Z)),
            'rear': ((-1.45, -PX - PS / 2 - 0.001), (-(PY - PS / 2) + 0.001, PY - PS / 2 - 0.001), (0.62, OPEN_Z))}
open_hits = []
for on, (xr, yr, zr) in OPENINGS.items():
    bmo = bmesh.new(); bmesh.ops.create_cube(bmo, size=1.0)
    for v in bmo.verts: v.co = Vector(((xr[0] + xr[1]) / 2 + v.co.x * (xr[1] - xr[0]), (yr[0] + yr[1]) / 2 + v.co.y * (yr[1] - yr[0]), (zr[0] + zr[1]) / 2 + v.co.z * (zr[1] - zr[0])))
    ob = BVHTree.FromBMesh(bmo); bmo.free()
    for o in ALL:
        M = open_state(o)
        if M is None: continue
        t = BVHTree.FromPolygons([M @ (o.matrix_world @ v.co) for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
        vs = [M @ (o.matrix_world @ v.co) for v in o.data.vertices]
        inside = any(xr[0] < v.x < xr[1] and yr[0] < v.y < yr[1] and zr[0] < v.z < zr[1] for v in vs[:: max(1, len(vs) // 2000)])
        if inside or t.overlap(ob): open_hits.append((on, o.name))
print('BC CHECK load openings clear when open (%s): %d hits %s' % (', '.join('%s %dx%d' % (k, round((v[0][1] - v[0][0] if k != 'rear' else v[1][1] - v[1][0]) * 1000), round((v[2][1] - v[2][0]) * 1000)) for k, v in OPENINGS.items()), len(open_hits), open_hits[:12]))

# ---------------------------------------------------------------- the moving parts, IK points, sizes -> json for the main session
def bb_all():
    lo_ = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
    for o in ALL:
        a, b = K.bbox(o); lo_ = Vector(tuple(min(lo_[i], a[i]) for i in range(3))); hi = Vector(tuple(max(hi[i], b[i]) for i in range(3)))
    return lo_, hi
LO, HI = bb_all()
r4 = lambda v: [round(c, 4) for c in v]
info = {'size_m': {'x_min': round(LO.x, 3), 'x_max': round(HI.x, 3), 'length': round(HI.x - LO.x, 3), 'width': round(HI.y - LO.y, 3), 'height': round(HI.z, 3),
                   'body_length': 2 * BX, 'deck_width': 2 * BY, 'deck_height': DECK, 'wheelbase': FX - RX, 'track': TRK, 'tyre': '4.00-8, %d x %d' % (TD * 1000, TW * 1000),
                   'load_space': [round(2 * (PX - PS / 2 - 0.02), 3), round(2 * (PY - PS / 2 - 0.035), 3), round(OPEN_Z - DECK, 3)]},
        'tow_eye': {'centre': r4(EYE), 'hole_diameter_mm': 40, 'height_m': EYE.z},
        'coupler_rear': {'pin_axis': r4(PIN), 'height_m': PIN.z, 'pin_parts': 'CouplerRear_Pin*', 'pin_open': 'translate +z 0.10 m (0 = closed)', 'jaw': 'CouplerRear_* (fixed, welded)'},
        'drawbar': {'parts': 'Drawbar*', 'pivot': r4(DB_P), 'axis': 'y', 'lowered_deg': 0, 'note': 'lowered (towing) = 0; raise with a NEGATIVE rotation about +y up to -82 deg (stands up, checked clear); raised = parking brake on (dead-man), the front axle may not steer while raised'},
        'front_axle': {'parts': 'FrontAxle_* + front wheel groups (key FL, FR)', 'pivot': [FX, 0.0, 0.389], 'axis': 'z', 'range_deg': 80, 'note': 'steered by the drawbar (turntable); +deg = turn left'},
        'brake': {'parts': 'FrontAxle_BrakeShoe_*/BrakePad_* (8 mm off the tyres)', 'note': 'visual: set while the drawbar is raised; optional 3 deg turn of FrontAxle_Brake* about y through (%.3f, 0, 0.21) to touch the tyres' % BSX},
        'wheels': {e.name[9:]: r4(e.location) for e in IKS if e.name.startswith('IK_Wheel_')},
        'wheel_radius_m': WZ,
        'curtains': {nm: {'sheet': f'Curtain_{nm}', 'sheet_pivot_z': c['sheet_pivot_z'], 'roll': f'CurtainRoll_{nm}', 'roll_axis_point': r4(c['roll_axis']), 'roll_axis': 'x',
                          'animate': 'open fraction f (0 closed .. 1 open), after the 4 straps are unhooked: Curtain_%s scale z = 1 - %.2f f about z = %.3f; Curtain_%s_Hem + _PullStrap + _Strap_* + _Buckle_* translate +z by %.3f f; hide _Strap_*/_Buckle_*/_PullStrap at f > 0.9 (wrapped into the roll; the hem bar stays visible under the roll as the handle); CurtainRoll_%s scale y and z about its axis = 1 + %.1f f (0.032 -> 0.045 m radius). Fully open: everything at or above z %.3f, the side opening %d x %d clear (checked)' % (nm, CUR_K, CUR_Z1, nm, CUR_K * (CT_Z - CUR_Z0), nm, ROLL_OPEN - 1, CUR_Z0 + CUR_K * (CT_Z - CUR_Z0) - 0.012, round((2 * PX - PS) * 1000), round((OPEN_Z - DECK) * 1000)),
                          'ik_hands': {'straps': [f'IK_Curtain_{nm}_Strap_{k}' for k in range(4)], 'closed': [f'IK_Curtain_{nm}_Handle', f'IK_Curtain_{nm}_HandL', f'IK_Curtain_{nm}_HandR'], 'open': [f'IK_Curtain_{nm}_HandleUp', f'IK_Curtain_{nm}_HandLUp', f'IK_Curtain_{nm}_HandRUp'], 'stand': f'IK_Stand_Curtain_{nm}', 'note': 'the hand IK points ride with the hem: interpolate closed -> open with f'}}
                     for nm, c in CURT.items()},
        'net_gate': {'parts': 'NetGate* (frame, net, knuckles, hinge leaves, latch box + bolt, handle)', 'pivot': r4(GH), 'axis': 'z', 'range_deg': [0, -180],
                     'note': 'closed = 0; opens outward with a NEGATIVE rotation about +z (hinges on the left rear post); at -180 it lies out to the left of the rear, the whole rear opening clear (checked). Lift the latch (IK_NetGate_Latch) then pull the handle (IK_NetGate_Handle); stand at IK_Stand_Rear.',
                     'fixed_parts': 'GateHinge_* (knuckles + leaves on the post), GateKeeper'},
        'load_openings_mm': {'side_each': [round((2 * PX - PS) * 1000), round((OPEN_Z - DECK) * 1000)], 'rear': [round((2 * PY - PS) * 1000), round((OPEN_Z - DECK) * 1000)]},
        'ik': {e.name: r4(e.location) for e in IKS},
        'checks': {'floating': fl, 'lamp_clash': cl, 'overhang': oh, 'mesh_loose': lo, 'steer_hits': len(steer_hits), 'drawbar_hits': len(db_hits), 'eye_hits': eye_hits, 'pin_hits': len(pin_hits), 'gate_hits': len(gate_hits), 'opening_hits': open_hits}}
if not (DO_WIRE or DO_BEAUTY): json.dump(info, open(os.path.join(ART, 'baggagecart-parts.json'), 'w'), indent=1)   # (the --wire/--beauty runs never overwrite the export's json)
print('BC size', info['size_m'])

# ================================================================ CC0 photo materials (Art/BaggageCart/Textures/SOURCES.md)
TEX = os.path.join(ART, 'Textures')
if not DO_WIRE:
    K.uv_cube()
    K.photo(M_GALV, TEX, 'Metal011', tile=0.8, tint=(0.95, 0.97, 1.0))
    K.photo(M_ALU, TEX, 'Metal011', tile=1.6, tint=(1.15, 1.15, 1.15), rough_mul=0.8)
    K.photo(M_STEEL, TEX, 'Metal027', tile=0.6)
    K.photo(M_DARK, TEX, 'Metal027', tile=0.6, tint=(0.25, 0.25, 0.27))
    K.photo(M_WHITEP, TEX, 'Paint004', tile=1.0, tint=(0.95, 0.95, 0.93), metal=0.0)
    K.photo(M_FLOOR, TEX, 'DiamondPlate008A', tile=0.5)
    K.photo(M_CURTAIN, TEX, 'Fabric074', tile=0.35, tint=(0.16, 0.20, 0.42), metal=0.0, rough_mul=0.75, normal=0.6)
    K.photo(M_STRAP, TEX, 'Fabric074', tile=0.12, tint=(0.10, 0.10, 0.10), metal=0.0)
    K.photo(M_NET, TEX, 'Fabric074', tile=0.08, tint=(0.08, 0.08, 0.08), metal=0.0)
    K.photo(M_RUB, TEX, 'Rubber004', tile=0.4, tint=(0.35, 0.35, 0.35), metal=0.0)
    K.photo(M_RIM, TEX, 'Paint004', tile=0.6, tint=(0.80, 0.81, 0.83), metal=0.3)
    K.photo(M_PLAST, TEX, 'Plastic006', tile=0.4, tint=(0.35, 0.35, 0.37), metal=0.0)
    K.photo(M_YELP, TEX, 'Paint004', tile=0.8, tint=(1.0, 0.62, 0.02), metal=0.0)

ZF = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, os.path.join(ART, 'baggagecart.blend'), os.path.join(ART, 'baggagecart.glb'), ZF, 'BC', 'BaggageCart') if not (DO_WIRE or DO_BEAUTY) else 'skipped'
print('BC: %d parts, %d IK/pivot points, %d faces, flicker: %s' % (len(ALL), len(IKS), sum(len(o.data.polygons) for o in ALL), last))
if not (DO_WIRE or DO_BEAUTY):
    f2 = K.checks('BC after flicker fix', ground_ok=('Tyre',), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)
    info['checks']['flicker'] = last; info['checks']['after_flicker_fix'] = {'floating': f2[0], 'lamp_clash': f2[1], 'overhang': f2[2], 'mesh_loose': f2[3]}
    info['parts'] = sorted(o.name for o in ALL); info['faces'] = sum(len(o.data.polygons) for o in ALL)
    json.dump(info, open(os.path.join(ART, 'baggagecart-parts.json'), 'w'), indent=1)

# ================================================================ pictures
if DO_BEAUTY:
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; ee = sc.eevee
    ee.use_gtao = True; ee.gtao_distance = 0.4; ee.use_ssr = True; ee.use_soft_shadows = True; ee.shadow_cube_size = '2048'; ee.shadow_cascade_size = '4096'; ee.taa_render_samples = 96
    ee.use_bloom = True; ee.bloom_intensity = 0.02
    sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
    w = bpy.data.worlds.new('WB'); w.use_nodes = True; bg = w.node_tree.nodes['Background']; bg.inputs['Color'].default_value = (0.55, 0.65, 0.8, 1); bg.inputs['Strength'].default_value = 0.9; sc.world = w
    gm = pmat('Apron', (0.32, 0.32, 0.31), 0, 0.85)
    ground = box('Ground', (0, 0, -0.01), (30, 30, 0.02), gm); ALL.remove(ground)
    sd = bpy.data.lights.new('Sun', 'SUN'); sd.energy = 4.5; sd.angle = 0.02; so = bpy.data.objects.new('Sun', sd); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(35))
    cd = bpy.data.cameras.new('BCam'); bcam = bpy.data.objects.new('BCam', cd); sc.collection.objects.link(bcam); sc.camera = bcam
    def bshot(name, loc, at, lens=45, res=(2400, 1500)):
        bcam.location = loc; bcam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens
        sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(PICS, name + '.png'); bpy.ops.render.render(write_still=True); print('BC: picture', name)
    # the final renders: a slightly odd angle, low, front and back, 4K
    bshot('BaggageCart-Final-Front', (5.0, -3.6, 1.45), (0.35, 0.0, 0.95), 35, (3840, 2160))
    bshot('BaggageCart-Final-Back', (-4.7, 3.4, 1.75), (-0.2, 0.0, 0.95), 35, (3840, 2160))
    bshot('BaggageCart-Close-Drawbar', (2.9, -1.4, 0.95), (1.55, 0.0, 0.35), 40)
    bshot('BaggageCart-Close-Coupler', (-2.2, -0.9, 0.85), (-1.25, 0.0, 0.42), 40)
    bshot('BaggageCart-Close-CurtainStraps', (0.6, -2.2, 0.85), (0.3, -0.72, 0.60), 40)
    # the left curtain rolled up (open for loading): sheet + hem/straps hidden, the roll at its full size
    for o in ALL:
        M = open_state(o)
        if M is None: o.hide_render = True
        elif M != Matrix.Identity(4): o.data.transform(M); o.data.update()
    bshot('BaggageCart-Open-Left', (2.6, 4.6, 2.2), (0.0, 0.3, 1.0), 32)
    bshot('BaggageCart-Open-Rear', (-4.2, -2.6, 1.9), (-0.6, 0.2, 1.0), 32)
    bshot('BaggageCart-Open', (-4.4, -3.3, 2.5), (-0.3, 0.15, 1.0), 30, (3840, 2160))   # both curtains up + the net gate open, 4K

if DO_WIRE:
    shoot = K.wire_setup()
    AT = (0.4, 0.0, 0.95)
    W = lambda n: os.path.join(WIRE, n + '.png')
    shoot(W('1-three-quarter-front'), (5.6, -4.8, 3.0), AT, 45)
    shoot(W('2-three-quarter-rear'), (-5.4, 4.8, 3.0), AT, 45)
    shoot(W('3-side-left'), (0.4, 12, 1.0), (0.4, 0, 1.0), 50, 3.8, res=(2400, 1350))
    shoot(W('4-side-right'), (0.4, -12, 1.0), (0.4, 0, 1.0), 50, 3.8, res=(2400, 1350))
    shoot(W('5-front'), (12, 0, 1.0), (0, 0, 1.0), 50, 2.3, res=(1500, 1250))
    shoot(W('6-rear'), (-12, 0, 1.0), (0, 0, 1.0), 50, 2.3, res=(1500, 1250))
    shoot(W('7-top'), (0.4, 0, 14), (0.4, 0, 0), 50, 3.8, res=(2400, 1350))
    shoot(W('8-drawbar-turntable'), (2.6, -1.6, 1.1), (1.25, 0.0, 0.30), 35)
    shoot(W('9-rear-coupler'), (-2.2, -1.0, 0.9), (-1.25, 0.0, 0.40), 40)
    shoot(W('10-inside-curtain-off'), (0.2, -2.6, 1.6), (0.0, 0.0, 1.05), 30, hide=tuple(o.name for o in ALL if o.name.startswith('Curtain_R')))
    shoot(W('11-underside'), (0.4, -3.2, -0.9), (0.2, 0, 0.35), 35)
    # open for loading: both curtains rolled up, the net gate swung open
    for o in ALL:
        M = open_state(o)
        if M is None: o.data.transform(Matrix.Translation((0, 0, -50)))
        elif M != Matrix.Identity(4): o.data.transform(M); o.data.update()
    shoot(W('12-open-for-loading'), (-4.6, -4.0, 2.6), (-0.2, 0.0, 1.0), 32)
