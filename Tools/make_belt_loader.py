# The electric BELT LOADER - our own design, drawn to real-world sizes of the self-propelled baggage conveyors that load a
# narrow-body's bulk holds (TLD NBL-E / Mallaghan / ACE / Charlatte CBL class; numbers in
# Blueprints/Vehicles/BeltLoader/RESEARCH.md). No brand names or logos. Battery-electric like our baggage tractor
# (Tools/make_baggage_tractor.py): yellow pressed-steel chassis, white half-cab with a hinged door on the left (the
# cold-climate cab of the European units), the conveyor boom down the middle of the chassis in a slot, raised about its
# lower (rear) hinge by two hydraulic rams lying in a tunnel under it; a hinged rubber bumper pad at the top end,
# handrails along the upper boom, operator controls at the bottom and the top end, a work lamp at the top.
# Built to the vehicle standard learned on the pushback tug (helpers + checks shared in Tools/vehicle_kit.py).
# Axes: x forward (the boom's top end is the front), y left, z up; metres; origin on the ground under the middle.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_belt_loader.py [-- --wire | --beauty | --check]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import importlib, vehicle_kit as K
importlib.reload(K)
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, lamp, pmat, ALL, IKS

ART = os.path.join(ROOT, 'Art', 'BeltLoader'); os.makedirs(ART, exist_ok=True)
BP = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'BeltLoader')
WIRE = os.path.join(BP, 'wire'); os.makedirs(WIRE, exist_ok=True)
PICS = os.path.join(ROOT, 'docs', 'Pictures', 'Vehicles', 'BeltLoader'); os.makedirs(PICS, exist_ok=True)
DO_WIRE, DO_BEAUTY, DO_CHECK = '--wire' in sys.argv, '--beauty' in sys.argv, '--check' in sys.argv
K.reset()
IKP = {}                                   # IK point -> the part it belongs to (checked: it sits on that part)
def ik(name, loc, part=None):
    IKP['IK_' + name] = part; return K.ik(name, loc)

# ---------------------------------------------------------------- the main sizes (m)
BW = 1.08                                  # chassis half width (2160 over the body)
Z_BOT, DECK = 0.20, 0.95                   # chassis underside (200 clearance) / side deck height
XF, XR = 2.40, -3.30                       # chassis front face / rear face
SLOT_Y, SLOT_Z = 0.43, 0.62                # the boom's slot down the middle (y +-0.43 above z 0.62)
TUN_Y, TUN_Z0, TUN_X0, TUN_X1 = 0.22, 0.28, -2.95, 1.95     # the rams' tunnel under the boom
FX, RX = 1.65, -1.65                       # front (steering) / rear (driven) axle - wheelbase 3300
TRK2 = 0.80                                # wheel centres y +-0.80 (track 1600)
TD, TW, RB = 0.68, 0.18, 0.127             # tyres 6.50-10 class: 680 dia, 180 wide, 10 in rim
TZ = TD / 2
F_ARCH_R, R_ARCH_R = 0.46, 0.42            # arch radii (the front steers 30 deg inside its arch - checked)
F_WELL_Y, R_WELL_Y = 0.48, 0.62            # the wheel wells' inner walls
KP_Y = 0.72                                # front kingpins y
STEER = 30
# the half-cab (left, beside the boom)
CAB_X0, CAB_X1, CAB_Y0, CAB_Y1, CAB_Z0, ROOF = -1.05, 0.70, 0.465, 1.075, 0.93, 2.00
FLOOR = 0.45
def wsx(z): return CAB_X1 - 0.08 * (z - CAB_Z0)       # the windscreen's outer line (4.6 deg rake)
WS_DEG = math.degrees(math.atan(0.08))
def xi(z): return wsx(z) - 0.0401
DOOR_X0, DOOR_X1, DOOR_Z0, DOOR_Z1 = -0.70, 0.40, 0.47, 1.90
HX, HY = DOOR_X1 + 0.012, CAB_Y1 + 0.017            # the door's hinge axis (vertical)
CYC = (CAB_Y0 + CAB_Y1) / 2
# the boom (rest position: belt level, lying in the slot)
XB0, XB1 = -3.80, 3.80                     # boom rear (bottom) end / front (top) end
BZ0, BZ1 = 0.70, 1.02                      # boom underside / side channel tops
CH_Y0, CH_Y1 = 0.32, 0.40                  # side channels (y)
BELT_W = 0.61                              # belt width (24 in)
DRUM_X, DRUM_R, DRUM_Z = 3.72, 0.10, 0.865 # end drums at +-3.72 (belt wraps them, 110 outer)
BELT_Z0, BELT_Z1 = 0.966, 0.976            # the belt's top run
PV = Vector((-3.20, 0.0, 0.76))            # THE BOOM HINGE (lower end): pivot, axis y
B_RF, T_RF = Vector((-0.40, 0, 0.42)), Vector((1.70, 0, 0.64))   # front ram: base (chassis) / top (boom) eye
B_RR, T_RR = Vector((-2.70, 0, 0.42)), Vector((-1.30, 0, 0.64))  # rear ram
MAX_DEG = 20.5                             # boom fully raised (front sill ~3.40 m)
SILL_PT = Vector((DRUM_X + 0.11, 0, BELT_Z1))   # the belt's top end (where bags leave into the hold)
def raised(p, deg):
    """A boom point after raising the boom by deg (nose up = a NEGATIVE turn about +y through the hinge)."""
    return PV + Matrix.Rotation(math.radians(-deg), 3, 'Y') @ (Vector(p) - PV)
def deg_for_sill(z):
    lo_, hi_ = 0.0, 30.0
    for _ in range(60):
        m = (lo_ + hi_) / 2
        if raised(SILL_PT, m).z < z: lo_ = m
        else: hi_ = m
    return round((lo_ + hi_) / 2, 2)
A320_DEG = deg_for_sill(2.10)

# ---------------------------------------------------------------- materials (plain; CC0 photos swapped in below)
M_PAINT = pmat('BL_BodyPaint', (0.85, 0.62, 0.05), 0.2, 0.45)       # GSE yellow (the tractor's paint)
M_CAB = pmat('BL_CabPaint', (0.92, 0.92, 0.90), 0.1, 0.35)          # white cab + roof (the tractor's white)
M_STEEL = pmat('BL_Steel', (0.35, 0.36, 0.38), 1.0, 0.45)
M_DARK = pmat('BL_DarkSteel', (0.08, 0.08, 0.09), 0.6, 0.55)
M_RUB = pmat('BL_Tyre', (0.03, 0.03, 0.03), 0.0, 0.9)
M_BELT = pmat('BL_BeltRubber', (0.035, 0.035, 0.035), 0.0, 0.85)
M_BUMP = pmat('BL_BumperRubber', (0.03, 0.03, 0.03), 0.0, 0.8)
M_FLOOR = pmat('BL_FloorMat', (0.06, 0.06, 0.06), 0.0, 0.9)
M_PLAST = pmat('BL_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_VINYL = pmat('BL_SeatVinyl', (0.05, 0.05, 0.06), 0.0, 0.6)
M_TREAD = pmat('BL_TreadPlate', (0.5, 0.5, 0.52), 1.0, 0.4)
M_CHEV = pmat('BL_Chevrons', (0.9, 0.7, 0.05), 0.2, 0.5)
M_REDP = pmat('BL_RedPaint', (0.7, 0.04, 0.03), 0.0, 0.4)
M_YELP = pmat('BL_YellowPaint', (0.95, 0.75, 0.05), 0.0, 0.4)
M_CHROME = pmat('BL_RamRod', (0.8, 0.8, 0.82), 1.0, 0.12)
M_GLASS = pmat('BL_Glass', (0.2, 0.3, 0.35), 0.0, 0.05)
M_MIRROR = pmat('BL_MirrorGlass', (0.8, 0.85, 0.9), 1.0, 0.02)
M_AMBER = pmat('BL_AmberLens', (1.0, 0.55, 0.05), 0.0, 0.2, 2.0)
M_RED = pmat('BL_RedLens', (0.9, 0.05, 0.03), 0.0, 0.2, 1.0)
M_WHITE = pmat('BL_WhiteLens', (0.95, 0.95, 0.9), 0.0, 0.1, 1.5)
M_SCREEN = pmat('BL_Screen', (0.05, 0.25, 0.35), 0.0, 0.1, 0.8)
M_GREEN = pmat('BL_GreenLens', (0.05, 0.8, 0.15), 0.0, 0.2, 1.0)

def seal(o, dist=0.0002):
    """Closes the hair-thin slits an exact boolean can leave where it cuts through a small bevel strip."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=dist)
    bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(o.data); bm.free(); o.data.update(); return o
def two_tone(o, z, m_low):
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z), plane_no=(0, 0, 1))
    bm.to_mesh(o.data); bm.free()
    o.data.materials.append(m_low); i = len(o.data.materials) - 1
    for p in o.data.polygons:
        if p.center.z < z: p.material_index = i
    return o
def axis_part(o, c, ax):
    """Turns a part made round +z (at the origin) onto an axis ax through c."""
    rot = Vector(ax).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    o.data.transform(Matrix.Translation(Vector(c)) @ rot); o.data.update(); return o
def rod_between(name, a, b, r, m, verts=24, bevel=0.0):
    a, b = Vector(a), Vector(b); return axis_part(cyl(name, (0, 0, 0), r, (b - a).length, 'z', m, verts, bevel=bevel), (a + b) / 2, b - a)

# ================================================================ THE CHASSIS (one pressed/welded steel body, flat panels)
body = prism('Body', [(XR, Z_BOT), (XF - 0.06, Z_BOT), (XF, Z_BOT + 0.08), (XF, DECK), (XR, DECK)], -BW, BW, M_PAINT, bevel=0.02, segs=2)
cut(body, box('SlotCut', ((XR + XF) / 2, 0, (SLOT_Z + 1.30) / 2), (XF - XR + 0.4, 2 * SLOT_Y, 1.30 - SLOT_Z), M_DARK))
cut(body, box('TunnelCut', ((TUN_X0 + TUN_X1) / 2, 0, (TUN_Z0 + SLOT_Z + 0.04) / 2), (TUN_X1 - TUN_X0, 2 * TUN_Y, SLOT_Z + 0.04 - TUN_Z0), M_DARK))
for ax, r, wy in ((FX, F_ARCH_R, F_WELL_Y), (RX, R_ARCH_R, R_WELL_Y)):
    for s in (-1, 1):
        arch = [(ax - r, Z_BOT - 0.10)] + [(ax - r * math.cos(math.pi * j / 48), TZ + r * math.sin(math.pi * j / 48)) for j in range(49)] + [(ax + r, Z_BOT - 0.10)]
        cut(body, prism('ArchCut', arch, wy if s > 0 else -(BW + 0.15), BW + 0.15 if s > 0 else -wy, M_DARK))
cut(body, box('CabLowCut', ((CAB_X0 + CAB_X1) / 2, (CAB_Y0 + 0.04 + CAB_Y1 - 0.04) / 2, (FLOOR + DECK + 0.10) / 2), (CAB_X1 - CAB_X0 - 0.08, CAB_Y1 - CAB_Y0 - 0.08, DECK + 0.10 - FLOOR), M_DARK))
cut(body, box('DoorCutLow', ((DOOR_X0 + DOOR_X1) / 2, BW, (DOOR_Z0 + DECK + 0.10) / 2), (DOOR_X1 - DOOR_X0, 0.30, DECK + 0.10 - DOOR_Z0), M_DARK))
STEP_X = (DOOR_X0 + DOOR_X1) / 2
cut(body, box('StepPocketCut', (STEP_X, BW, 0.33), (0.48, 0.29, 0.18), M_DARK))     # (a recessed step box: 40 mm lip under it, the body's bottom edge stays whole)
seal(body, 0.001)

# ---------------------------------------------------------------- wheels (real lathed tyres - no modelled tread; dished rims)
TUG_PROF = [(0.290, -0.124), (0.318, -0.146), (0.360, -0.157), (0.410, -0.1585), (0.460, -0.154), (0.500, -0.142), (0.522, -0.124),
            (0.534, -0.100), (0.538, -0.060), (0.539, 0.0), (0.538, 0.060), (0.534, 0.100), (0.522, 0.124), (0.500, 0.142),
            (0.460, 0.154), (0.410, 0.1585), (0.360, 0.157), (0.318, 0.146), (0.290, 0.124), (0.286, 0.0)]
TUG_RIM = [(0.300, -0.130), (0.296, -0.122), (0.284, -0.118), (0.270, -0.070), (0.262, -0.050), (0.262, 0.050), (0.270, 0.070),
           (0.284, 0.118), (0.296, 0.122), (0.300, 0.130), (0.288, 0.132), (0.276, 0.122), (0.256, 0.060), (0.256, -0.060), (0.276, -0.122), (0.288, -0.132)]
WHEELS = {}
def wheel(key, x, y, s, R, rb, tw, zc):
    q = tw / 0.165; k = rb / 0.135
    lathe(f'Tyre_{key}', [(rb + (r - 0.29) * ((R - rb) / 0.249), yy * (tw / 2 / 0.1585)) for r, yy in TUG_PROF], (x, y, zc), M_RUB, 72)
    lathe(f'Rim_{key}', [(rb + (r - 0.29) * 0.58 * k, yy * 0.52 * q) for r, yy in TUG_RIM], (x, y, zc), M_STEEL, 72)
    disc = cyl(f'WheelDisc_{key}', (x, y + s * 0.035 * q, zc), rb - 0.017 * k, 0.010, 'y', M_STEEL, 64, bevel=0.002)
    for j in range(5):
        a = j * 2 * math.pi / 5 + math.pi / 5
        cut(disc, cyl('HandHole', (x + 0.088 * k * math.cos(a), y + s * 0.035 * q, zc + 0.088 * k * math.sin(a)), 0.016 * k, 0.04, 'y', M_DARK, 20))
    cyl(f'Hub_{key}', (x, y + s * (0.035 * q + 0.017), zc), 0.052 * k, 0.030, 'y', M_DARK, 32, bevel=0.006)
    cyl(f'HubCap_{key}', (x, y + s * (0.035 * q + 0.037), zc), 0.030 * k, 0.016, 'y', M_DARK, 24, bevel=0.004)
    for j in range(5):
        a = j * 2 * math.pi / 5
        cyl(f'Nut_{key}{j}', (x + 0.068 * k * math.cos(a), y + s * (0.035 * q + 0.0085), zc + 0.068 * k * math.sin(a)), 0.009, 0.014, 'y', M_STEEL, 6)
    WHEELS[key] = (x, y, zc); ik(f'Wheel_{key}', (x, y, zc), f'Hub_{key}')
Q = TW / 0.165
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'FrontAxle_Arm_{sd}', (FX, s * (F_WELL_Y - 0.02 + KP_Y) / 2, TZ), (0.09, KP_Y - F_WELL_Y + 0.02, 0.08), M_DARK, bevel=0.006)
    cyl(f'Kingpin_{sd}', (FX, s * KP_Y, TZ), 0.032, 0.20, 'z', M_DARK, 24, bevel=0.004)
    cyl(f'Spindle_F{sd}', (FX, s * (KP_Y - 0.015 + TRK2 + 0.035 * Q) / 2, TZ), 0.028, TRK2 + 0.035 * Q - KP_Y + 0.015, 'y', M_STEEL, 20)
    cyl(f'SteerArm_F{sd}', (FX - 0.07, s * KP_Y, TZ + 0.06), 0.014, 0.14, 'x', M_DARK, 12)
    wheel('F' + sd, FX, s * TRK2, s, TZ, RB, TW, TZ)
    ik(f'Steer_F{sd}', (FX, s * KP_Y, TZ), f'Kingpin_{sd}')
    cyl(f'RearAxle_Flange_{sd}', (RX, s * (R_WELL_Y + 0.006), TZ), 0.085, 0.022, 'y', M_DARK, 32, bevel=0.004)
    cyl(f'RearAxle_Stub_{sd}', (RX, s * (R_WELL_Y - 0.02 + TRK2 + 0.035 * Q) / 2, TZ), 0.042, TRK2 + 0.035 * Q - R_WELL_Y + 0.02, 'y', M_STEEL, 24)
    wheel('R' + sd, RX, s * TRK2, s, TZ, RB, TW, TZ)

# ---------------------------------------------------------------- the front face: bumper band, lamps, tow lug
box('BumperFront', (XF + 0.0425, 0, 0.38), (0.095, 1.90, 0.16), M_BUMP, bevel=0.02, segs=3)
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'LowBeam_{nm}', (XF, s * 0.82, 0.74), 'x+', 0.14, 0.10, M_WHITE, M_DARK, M_PLAST)
    lamp(f'HighBeam_{nm}', (XF, s * 0.62, 0.74), 'x+', 0.09, 0.10, M_WHITE, M_DARK, M_PLAST)
    lamp(f'IndicatorF_{nm}', (XF, s * 0.82, 0.58), 'x+', 0.14, 0.06, M_AMBER, M_DARK, M_PLAST)
    sg = 'y' + ('+' if s > 0 else '-')
    lamp(f'SideMarker_F{nm}', (2.20, s * BW, 0.78), sg, 0.07, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.03)
    lamp(f'SideMarker_M{nm}', (-0.97, s * BW, 0.78), sg, 0.07, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.03)
    lamp(f'SideMarker_R{nm}', (-3.10, s * BW, 0.78), sg, 0.07, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.03)
# front towing lug (recovery): a plate under the bumper band with its eye
lug = box('TowLug_Front', (XF + 0.035, 0, 0.26), (0.08, 0.12, 0.10), M_DARK, bevel=0.006)
cut(lug, cyl('TowLugHole', (XF + 0.05, 0, 0.25), 0.022, 0.2, 'y', M_DARK, 20))

# ---------------------------------------------------------------- the boom's hinge brackets (static) and rest pads
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'HingeBoom_Bracket_{sd}', (PV.x, s * (SLOT_Y - 0.0095), 0.74), (0.18, 0.023, 0.20), M_DARK, bevel=0.004)   # (welded to the slot wall)
    cyl(f'HingeBoom_Pin_{sd}', (PV.x, s * 0.385, PV.z), 0.025, 0.12, 'y', M_STEEL, 20)
    for k, x in enumerate((-2.40, 2.20)):
        box(f'BoomRestPad_{k}{sd}', (x, s * (CH_Y0 + CH_Y1) / 2, 0.657), (0.16, 0.08, 0.082), M_BUMP, bevel=0.006)   # (2 mm under the boom: no flicker)
# ram base brackets on the tunnel floor (static clevises + pins)
def ram_base(nm, B):
    box(f'RamBase{nm}_Plate', (B.x, 0, TUN_Z0 + 0.010), (0.20, 0.16, 0.024), M_DARK, bevel=0.003)
    for s in (-1, 1): box(f'RamBase{nm}_Ear_{s}', (B.x, s * 0.0485, (TUN_Z0 + 0.02 + B.z + 0.05) / 2), (0.12, 0.015, B.z + 0.05 - TUN_Z0 - 0.02), M_DARK, bevel=0.003)
    cyl(f'RamBase{nm}_Pin', (B.x, 0, B.z), 0.018, 0.13, 'y', M_STEEL, 16)
ram_base('Front', B_RF); ram_base('Rear', B_RR)

# ================================================================ THE BOOM (conveyor; every part named Boom*/BeltSurface/BumperPad*/WorkLamp*
# turns about PV with the boom; built at its rest position)
for s, sd in ((1, 'L'), (-1, 'R')):
    prism(f'Boom_Channel{sd}', [(XB0, BZ0 + 0.10), (XB0 + 0.10, BZ0), (XB1 - 0.10, BZ0), (XB1, BZ0 + 0.10), (XB1, BZ1), (XB0, BZ1)],
          s * CH_Y0 if s > 0 else -CH_Y1, CH_Y1 if s > 0 else -CH_Y0, M_PAINT, bevel=0.006, segs=1)
box('Boom_Pan', (0, 0, BZ0 + 0.010), (XB1 - XB0 - 0.24, 2 * CH_Y0 + 0.004, 0.012), M_DARK)
box('Boom_Bed', (0, 0, BELT_Z0 - 0.010), (2 * DRUM_X - 0.26, 2 * CH_Y0 + 0.004, 0.012), M_STEEL)       # the slider bed under the top run
for k in range(8):
    x = -3.30 + k * 0.94
    box(f'Boom_Cross_{k}', (x, 0, BZ0 + 0.033), (0.06, 2 * CH_Y0 + 0.004, 0.036), M_DARK)
for nm, x in (('F', DRUM_X), ('R', -DRUM_X)):
    cyl(f'Boom_Drum{nm}', (x, 0, DRUM_Z), DRUM_R, 0.62, 'y', M_STEEL, 40, bevel=0.004)
    cyl(f'Boom_DrumShaft{nm}', (x, 0, DRUM_Z), 0.025, 2 * CH_Y1 + 0.06, 'y', M_STEEL, 16)
    for s in (-1, 1):
        box(f'Boom_Bearing{nm}_{s}', (x, s * (CH_Y1 + 0.017), DRUM_Z), (0.12, 0.036, 0.10), M_DARK, bevel=0.008)
        cyl(f'Boom_BearingCap{nm}_{s}', (x, s * (CH_Y1 + 0.040), DRUM_Z), 0.030, 0.012, 'y', M_STEEL, 16)
    cyl(f'Boom_BeltWrap{nm}', (x, 0, DRUM_Z), DRUM_R + 0.011, BELT_W, 'y', M_BELT, 48)
box('BeltSurface', (0, 0, (BELT_Z0 + BELT_Z1) / 2), (2 * DRUM_X, BELT_W, BELT_Z1 - BELT_Z0), M_BELT)
box('Boom_BeltReturn', (0, 0, DRUM_Z - DRUM_R - 0.006), (2 * DRUM_X, BELT_W, 0.010), M_BELT)
for k, x in enumerate((-2.80, -1.40, 0.0, 1.40, 2.80)):
    cyl(f'Boom_Idler_{k}', (x + 0.30, 0, DRUM_Z - DRUM_R - 0.011 - 0.022), 0.022, 2 * CH_Y0 + 0.004, 'y', M_STEEL, 16)
# rubber edge skirts over the belt's edges (bags stay on the belt)
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'Boom_Skirt{sd}', (0, s * (CH_Y0 - 0.010), BELT_Z1 + 0.004 + 0.03), (2 * DRUM_X - 0.30, 0.024, 0.06), M_BUMP, bevel=0.004)
# ram brackets under the boom pan (clevis ears + pins)
def boom_bracket(nm, T):
    box(f'Boom_RamPlate{nm}', (T.x, 0, BZ0 - 0.001), (0.20, 0.16, 0.012), M_DARK, bevel=0.002)
    for s in (-1, 1): box(f'Boom_RamEar{nm}_{s}', (T.x, s * 0.0485, (T.z - 0.05 + BZ0 - 0.006) / 2), (0.12, 0.015, BZ0 - 0.006 - T.z + 0.05), M_DARK, bevel=0.003)
    cyl(f'Boom_RamPin{nm}', (T.x, 0, T.z), 0.018, 0.13, 'y', M_STEEL, 16)
boom_bracket('Front', T_RF); boom_bracket('Rear', T_RR)
# hinge bosses on the channels round the hinge pins
for s, sd in ((1, 'L'), (-1, 'R')):
    cyl(f'Boom_HingeBoss_{sd}', (PV.x, s * (CH_Y1 + 0.003), PV.z), 0.050, 0.008, 'y', M_DARK, 24)
# handrails along the upper boom (both sides): foot plates on the channel tops, posts, top + knee rails
RAIL_X = (0.90, 1.75, 2.60, 3.45); RAIL_TOP, RAIL_MID = BZ1 + 0.70, BZ1 + 0.36
for s, sd in ((1, 'L'), (-1, 'R')):
    y = s * (CH_Y0 + CH_Y1) / 2
    for k, x in enumerate(RAIL_X):
        box(f'Boom_RailFoot{sd}{k}', (x, y, BZ1 + 0.005), (0.08, 0.07, 0.012), M_DARK, bevel=0.002)
        cyl(f'Boom_RailPost{sd}{k}', (x, y, (BZ1 + RAIL_TOP) / 2 + 0.004), 0.019, RAIL_TOP - BZ1 + 0.004, 'z', M_YELP, 16)
    for nm, z in (('Top', RAIL_TOP), ('Mid', RAIL_MID)):
        cyl(f'Boom_Rail{nm}{sd}', ((RAIL_X[0] + RAIL_X[-1]) / 2, y, z), 0.019, RAIL_X[-1] - RAIL_X[0], 'x', M_YELP, 16)
# the top end: bumper brackets + pin (boom), the hinged bumper pad (BumperPad*: turns about its pin, keeps itself upright)
BPV = Vector((3.86, 0, 0.755))
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'Boom_BumperBracket_{sd}', (3.76, s * (CH_Y1 + 0.0065), 0.755), (0.28, 0.015, 0.11), M_DARK, bevel=0.003)
cyl('Boom_BumperPin', (BPV.x, 0, BPV.z), 0.020, 2 * (CH_Y1 + 0.015) + 0.06, 'y', M_STEEL, 16)
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'BumperPad_Arm_{sd}', (BPV.x + 0.01, s * (CH_Y1 + 0.024), (BPV.z + 0.035 + 0.64) / 2), (0.06, 0.016, BPV.z + 0.035 - 0.64), M_DARK, bevel=0.003)
box('BumperPad_Frame', (BPV.x + 0.045, 0, 0.64), (0.06, 2 * (CH_Y1 + 0.030), 0.10), M_DARK, bevel=0.006)
box('BumperPad', (BPV.x + 0.11, 0, 0.64), (0.08, 2 * (CH_Y1 + 0.04), 0.20), M_BUMP, bevel=0.025, segs=3)
BUMP_CONTACT = Vector((BPV.x + 0.15, 0, 0.64))
# top-end operator controls (left channel's outer face) and the work lamp (right channel top)
def control_box(nm, x):
    cb = box(f'Boom_Controls{nm}', (x, CH_Y1 + 0.029, 0.865), (0.17, 0.06, 0.17), M_YELP, bevel=0.008)
    for k, (dz, m) in enumerate(((0.045, M_GREEN), (0.0, M_PLAST), (-0.045, M_PLAST))):
        cyl(f'Boom_Controls{nm}_Btn{k}', (x - 0.035, CH_Y1 + 0.064, 0.865 + dz), 0.011, 0.012, 'y', m, 16)
    cyl(f'Boom_Controls{nm}_Toggle', (x + 0.03, CH_Y1 + 0.064, 0.89), 0.008, 0.014, 'y', M_STEEL, 12)
    cyl(f'Boom_Controls{nm}_EStopRing', (x + 0.03, CH_Y1 + 0.064, 0.83), 0.022, 0.010, 'y', M_YELP, 20)
    cyl(f'Boom_Controls{nm}_EStop', (x + 0.03, CH_Y1 + 0.072, 0.83), 0.018, 0.014, 'y', M_REDP, 20, bevel=0.003)
    return (x, CH_Y1 + 0.075, 0.865)
CTRL_TOP = control_box('Top', 3.20)
CTRL_BOT = control_box('Bottom', -3.50)
box('WorkLamp_Foot', (3.62, -(CH_Y0 + CH_Y1) / 2, BZ1 + 0.004), (0.10, 0.07, 0.012), M_DARK)
box('WorkLamp_Stem', (3.62, -(CH_Y0 + CH_Y1) / 2, BZ1 + 0.05), (0.03, 0.03, 0.09), M_DARK)
box('WorkLamp_Body', (3.62, -(CH_Y0 + CH_Y1) / 2, BZ1 + 0.14), (0.10, 0.13, 0.10), M_DARK, bevel=0.01)
lamp('WorkLamp', (3.67, -(CH_Y0 + CH_Y1) / 2, BZ1 + 0.14), 'x+', 0.08, 0.06, M_WHITE, M_DARK, M_PLAST, depth=0.03)
# small bag guides at the bottom (loading) end: low side wings so bags put on don't slide off
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'Boom_LoadGuide{sd}', (-3.55, s * (CH_Y1 - 0.004), BZ1 + 0.07), (0.40, 0.012, 0.14), M_PAINT, bevel=0.004)
# reflectors on the channel sides (front + rear end)
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'Boom_Reflector{sd}', (2.70, s * (CH_Y1 + 0.002), 0.86), (0.10, 0.006, 0.04), M_AMBER)

# ================================================================ THE RAMS (RamFront* / RamRear*: barrel turns about its base pin,
# rod about its top pin; the game aims both at each other as the boom turns)
RAMS = {}
def ram(nm, B, T, rb, rr):
    lmax = (raised(T, MAX_DEG) - B).length; lrest = (T - B).length
    Lb = Lr = round((lmax + 0.26) / 2, 3)
    assert Lb < lrest - 0.04, (nm, Lb, lrest)
    d = (T - B).normalized()
    eye = cyl(f'Ram{nm}_BarrelEye', tuple(B), 0.045, 0.078, 'y', M_DARK, 24)
    rod_between(f'Ram{nm}_BarrelNeck', B, B + d * 0.10, 0.030, M_DARK, 20)            # (fits between the base ears)
    rod_between(f'Ram{nm}_Barrel', B + d * 0.085, B + d * Lb, rb, M_PAINT, 32, bevel=0.004)
    rod_between(f'Ram{nm}_BarrelGland', B + d * (Lb - 0.03), B + d * (Lb + 0.012), rb + 0.006, M_DARK, 32, bevel=0.003)
    rod_between(f'Ram{nm}_BarrelPort', B + d * 0.17 + Vector((0, 0, rb - 0.01)), B + d * 0.17 + Vector((0, 0, rb + 0.025)), 0.012, M_STEEL, 12)
    rod_between(f'Ram{nm}_Rod', T - d * Lr, T - d * 0.03, rr, M_CHROME, 24)
    cyl(f'Ram{nm}_RodEye', tuple(T), rr * 1.25, 0.078, 'y', M_DARK, 24)
    RAMS[nm] = {'base_pivot': [round(c, 4) for c in B], 'top_pivot_boom_local': [round(c, 4) for c in T], 'axis': 'y',
                'barrel_len': Lb, 'rod_len': Lr, 'length_rest': round(lrest, 4), 'length_max': round(lmax, 4)}
ram('Front', B_RF, T_RF, 0.060, 0.032)
ram('Rear', B_RR, T_RR, 0.050, 0.028)

# ================================================================ THE HALF-CAB (white, left, beside the boom)
shell = prism('CabShell', [(CAB_X0, CAB_Z0), (wsx(CAB_Z0), CAB_Z0), (wsx(ROOF), ROOF), (CAB_X0, ROOF)], CAB_Y0, CAB_Y1, M_CAB, bevel=0.035, segs=4)
cut(shell, prism('ShellCut', [(CAB_X0 + 0.04, CAB_Z0 - 0.03), (xi(CAB_Z0 - 0.03), CAB_Z0 - 0.03), (xi(ROOF - 0.04), ROOF - 0.04), (CAB_X0 + 0.04, ROOF - 0.04)], CAB_Y0 + 0.04, CAB_Y1 - 0.04, M_DARK))
cut(shell, box('DoorCutUp', ((DOOR_X0 + DOOR_X1) / 2, CAB_Y1, (DOOR_Z0 + DOOR_Z1) / 2), (DOOR_X1 - DOOR_X0, 0.30, DOOR_Z1 - DOOR_Z0), M_DARK))
def window(name, c, s, target=None, deg=0.0):
    cu = box(name + 'Cut', c, s, M_DARK, bevel=0.03, segs=3)
    if deg: turn(cu, c, 'y', deg)
    cut(target or shell, cu)
    g = box(name + 'Glass', c, tuple(0.010 if v < 0.2 else v + 0.02 for v in s), M_GLASS, bevel=0.004, segs=2)
    if deg: turn(g, c, 'y', deg)
    return g
window('WinFront', (wsx(1.48) - 0.02, CYC, 1.48), (0.12, 0.44, 0.80), deg=-WS_DEG)
window('WinRear', (CAB_X0 + 0.02, CYC, 1.52), (0.12, 0.40, 0.56))
window('WinInner', (-0.18, CAB_Y0 + 0.02, 1.50), (1.36, 0.12, 0.66))
box('RoofCap', ((CAB_X0 - 0.04 + wsx(ROOF) + 0.04) / 2, CYC, ROOF + 0.015), (wsx(ROOF) - CAB_X0 + 0.08, CAB_Y1 - CAB_Y0 + 0.08, 0.05), M_CAB, bevel=0.02, segs=3)
RT = ROOF + 0.04
BX = -0.62
box('BeaconBase', (BX, CYC, RT + 0.008), (0.18, 0.18, 0.024), M_DARK, bevel=0.005)
cyl('BeaconLens', (BX, CYC, RT + 0.083), 0.070, 0.13, 'z', M_AMBER, 32, bevel=0.02)
cyl('BeaconCap', (BX, CYC, RT + 0.155), 0.056, 0.02, 'z', M_DARK, 32)
# the driver door (DoorDriver*: hinged on its FRONT edge, opens outward; glass moves with it)
DYC = CAB_Y1 - 0.02
leaf = box('DoorDriver', ((DOOR_X0 + DOOR_X1) / 2 - 0.0025, DYC, (DOOR_Z0 + DOOR_Z1) / 2), (DOOR_X1 - DOOR_X0 - 0.011, 0.04, DOOR_Z1 - DOOR_Z0 - 0.010), M_CAB, bevel=0.008, segs=2)
cut(leaf, box('DoorWinCut', (-0.15, DYC, 1.46), (0.86, 0.12, 0.66), M_DARK, bevel=0.03, segs=3))
two_tone(leaf, DECK, M_PAINT)
box('DoorDriver_Glass', (-0.15, DYC, 1.46), (0.88, 0.010, 0.68), M_GLASS, bevel=0.004, segs=2)
box('DoorDriver_HandleOut', (-0.50, CAB_Y1 + 0.020, 1.00), (0.14, 0.025, 0.035), M_STEEL, bevel=0.008)
for k, x in enumerate((-0.55, -0.45)): box(f'DoorDriver_HandleMount_{k}', (x, CAB_Y1 + 0.009, 1.00), (0.02, 0.022, 0.03), M_STEEL)
cyl('DoorDriver_Lock', (-0.38, CAB_Y1 + 0.002, 1.00), 0.011, 0.012, 'y', M_STEEL, 16)
box('DoorDriver_Trim', (-0.15, DYC - 0.025, 0.90), (0.96, 0.012, 0.42), M_PLAST, bevel=0.004)
box('DoorDriver_HandleIn', (-0.50, DYC - 0.039, 1.02), (0.10, 0.020, 0.03), M_PLAST, bevel=0.006)
box('DoorDriver_Pull', (-0.20, DYC - 0.046, 0.96), (0.24, 0.034, 0.04), M_PLAST, bevel=0.012)
for k, z in enumerate((0.78, 1.62)):
    box(f'HingeDoor_{k}_Base', (HX + 0.03, BW + 0.006 if z < DECK else CAB_Y1 + 0.006, z), (0.05, 0.014, 0.09), M_STEEL, bevel=0.002)
    cyl(f'HingeDoor_{k}', (HX, HY, z), 0.016, 0.10, 'z', M_STEEL, 16)
    box(f'DoorDriver_HingeLeaf_{k}', (HX - 0.038, CAB_Y1 + 0.003, z), (0.048, 0.008, 0.09), M_STEEL, bevel=0.002)
DOOR_PIVOT = (HX, HY, (DOOR_Z0 + DOOR_Z1) / 2)
# kick step in its pocket under the door, grab handles (outside behind the door, inside on the front post)
box('StepTread', (STEP_X, (BW - 0.15 + BW - 0.005) / 2, 0.253), (0.49, 0.145, 0.030), M_TREAD, bevel=0.003)   # (5 mm into the pocket's walls)
box('StepNosing', (STEP_X, BW - 0.016, 0.271), (0.49, 0.020, 0.010), M_YELP)
GX = -0.88
for k, z in enumerate((1.20, 1.62)): cyl(f'GrabOutFoot_{k}', (GX, CAB_Y1 + 0.002, z), 0.025, 0.012, 'y', M_STEEL, 24, bevel=0.002)
tube('GrabHandleOut', [(GX, CAB_Y1 + 0.004, 1.20), (GX, CAB_Y1 + 0.06, 1.24), (GX, CAB_Y1 + 0.06, 1.58), (GX, CAB_Y1 + 0.004, 1.62)], 0.016, M_STEEL, bez=False)
GIX = 0.50
for k, z in enumerate((1.30, 1.70)): cyl(f'GrabInFoot_{k}', (GIX, CAB_Y1 - 0.042, z), 0.022, 0.012, 'y', M_PLAST, 24, bevel=0.002)
tube('GrabHandleIn', [(GIX, CAB_Y1 - 0.044, 1.30), (GIX, CAB_Y1 - 0.09, 1.34), (GIX, CAB_Y1 - 0.09, 1.66), (GIX, CAB_Y1 - 0.044, 1.70)], 0.014, M_PLAST, bez=False)
# mirrors: a plate on the flat body side ahead of the door, a visible arm out and up, the mirror behind the windscreen line
for s, nm in ((1, 'L'), (-1, 'R')):
    box(f'MirrorPlate_{nm}', (0.60, s * (BW + 0.003), 0.75), (0.10, 0.012, 0.10), M_DARK, bevel=0.003)
    tube(f'MirrorArm_{nm}', [(0.60, s * (BW + 0.005), 0.75), (0.60, s * 1.16, 0.75), (0.60, s * 1.20, 0.85), (0.60, s * 1.20, 1.28)], 0.013, M_DARK, bez=False)
    box(f'Mirror_{nm}', (0.60, s * 1.20, 1.38), (0.05, 0.15, 0.22), M_PLAST, bevel=0.02, segs=3)
    box(f'MirrorGlass_{nm}', (0.60 - 0.025, s * 1.20, 1.38), (0.006, 0.125, 0.19), M_MIRROR)

# ================================================================ THE CAB INTERIOR (inside 1670 x 530, 1550 floor to roof)
IY0, IY1 = CAB_Y0 + 0.04, CAB_Y1 - 0.04
box('FloorMat', ((CAB_X0 + 0.05 + 0.66) / 2, CYC, FLOOR + 0.0035), (0.66 - CAB_X0 - 0.05, IY1 - IY0 - 0.01, 0.012), M_FLOOR)
for k in range(4): box(f'MatRib_{k}', ((CAB_X0 + 0.05 + 0.66) / 2, IY0 + 0.08 + k * 0.12, FLOOR + 0.010), (0.66 - CAB_X0 - 0.11, 0.02, 0.006), M_FLOOR)
prism('Dash', [(0.42, FLOOR - 0.005), (xi(FLOOR) + 0.005, FLOOR - 0.005), (xi(0.95) + 0.003, 0.95), (xi(1.12) + 0.002, 1.12), (0.50, 1.12), (0.42, 1.04)], IY0 - 0.004, IY1 + 0.004, M_PLAST, bevel=0.01, segs=2)
for k in range(2): box(f'DefrostVent_{k}', (0.58, CYC - 0.12 + k * 0.24, 1.121), (0.03, 0.16, 0.006), M_DARK)
for k in range(4): box(f'DashSwitch_{k}', (0.418, IY1 - 0.06 - k * 0.05, 1.00), (0.02, 0.025, 0.035), M_DARK, bevel=0.003)
SYC = 0.80                                  # seat / column centre line (y)
prism('Pedestal', [(0.18, FLOOR - 0.005), (0.44, FLOOR - 0.005), (0.44, 0.78), (0.36, 0.94), (0.24, 0.95), (0.18, 0.88)], SYC - 0.11, SYC + 0.11, M_PLAST, bevel=0.012, segs=2)
box('Binnacle', (0.34, SYC, 1.04), (0.10, 0.19, 0.20), M_PLAST, bevel=0.015, segs=2)
box('BinnacleDisplay', (0.288, SYC, 1.06), (0.008, 0.15, 0.10), M_SCREEN)
for k, (dy, m) in enumerate(((-0.075, M_GREEN), (0.075, M_AMBER))): cyl(f'BinnacleTell_{k}', (0.289, SYC + dy, 0.975), 0.007, 0.008, 'x', m, 12)
cyl('KeySwitch', (0.34, SYC - 0.113, 0.82), 0.020, 0.016, 'y', M_STEEL, 24, bevel=0.003)
box('Key', (0.34, SYC - 0.130, 0.82), (0.012, 0.020, 0.030), M_STEEL)
cyl('EStopIn_Housing', (0.34, SYC + 0.115, 0.82), 0.028, 0.018, 'y', M_YELP, 24, bevel=0.003)
cyl('EStopIn_Button', (0.34, SYC + 0.130, 0.82), 0.024, 0.016, 'y', M_REDP, 24, bevel=0.004)
WC = Vector((0.02, SYC, 1.10)); AX = Vector((-1.0, 0.0, 1.0)).normalized()
COL_BOT = Vector((0.27, SYC, 0.85))
WU, WV = Vector((0, 1, 0)), Vector((0.7071, 0, 0.7071))
tube('SteeringColumn', [tuple(COL_BOT), tuple(WC - AX * 0.012)], 0.030, M_PLAST, bez=False)
axis_part(cyl('ColumnShroud', (0, 0, 0), 0.046, 0.14, 'z', M_PLAST, 24, bevel=0.01), WC - AX * 0.12, AX)
WR = 0.16
tube('SteeringWheel', [tuple(WC + WR * (math.cos(a) * WU + math.sin(a) * WV)) for a in [k * 2 * math.pi / 48 for k in range(49)]], 0.015, M_PLAST, bez=False)
axis_part(cyl('SteeringWheel_Hub', (0, 0, 0), 0.046, 0.05, 'z', M_PLAST, 24, bevel=0.01), WC, AX)
axis_part(cyl('SteeringWheel_Horn', (0, 0, 0), 0.033, 0.012, 'z', M_DARK, 24, bevel=0.003), WC + AX * 0.026, AX)
for k, a in enumerate((0.0, math.pi, -math.pi / 2)):
    d = math.cos(a) * WU + math.sin(a) * WV
    tube(f'SteeringWheel_Spoke_{k}', [tuple(WC + d * 0.025), tuple(WC + d * WR)], 0.011, M_PLAST, bez=False)
CP = COL_BOT + (WC - COL_BOT) * 0.62
tube('DirectionLever', [(CP.x, SYC + 0.02, CP.z), (CP.x - 0.02, SYC + 0.13, CP.z + 0.02)], 0.007, M_DARK, bez=False)
cyl('DirectionLever_Knob', (CP.x - 0.021, SYC + 0.145, CP.z + 0.021), 0.013, 0.036, 'y', M_PLAST, 16, bevel=0.004)
PEDALS = {}
for nm, dy, w in (('Brake', 0.17, 0.08), ('Accel', -0.17, 0.07)):
    y = SYC + dy
    box(f'PedalPivot_{nm}', (0.30, y, FLOOR + 0.028), (0.06, 0.07, 0.04), M_DARK)
    tube(f'PedalArm_{nm}', [(0.30, y, FLOOR + 0.04), (0.25, y, FLOOR + 0.11)], 0.009, M_DARK, bez=False)
    pc = Vector((0.24, y, FLOOR + 0.14))
    turn(box(f'Pedal_{nm}', tuple(pc), (0.03, w, 0.16), M_RUB, bevel=0.006), pc, 'y', -30)
    PEDALS[nm] = ((0.30, y, FLOOR + 0.03), 'y')
# the driver's seat: industrial suspension seat (rails, bellows, pan, contoured cushion with bolsters, high back with
# lumbar and side wings, headrest, lap belt) - 440 wide to fit the half-cab
SX, SY, SZ = -0.50, SYC, FLOOR
ss = K.ss
box('SeatRails', (SX, SY, SZ + 0.026), (0.40, 0.32, 0.04), M_DARK, bevel=0.005)
for k in range(5):
    z = SZ + 0.04 + 0.11 * (k + 0.5) / 5
    box(f'SeatBellows_{k}', (SX, SY, z), (0.38 * (1.0 if k % 2 else 0.92), 0.38 * (1.0 if k % 2 else 0.92), 0.11 / 5 * 1.04), M_RUB, bevel=0.010, segs=3)
box('SeatPan', (SX, SY, SZ + 0.165), (0.44, 0.42, 0.04), M_DARK, bevel=0.01)
def cushion_def(co, u, v, w):
    top = ss(-1.0, 1.0, w)
    co.z += top * (0.012 * (1 - v * v) + 0.03 * ss(0.55, 0.95, abs(v)))
    fr = ss(0.55, 1.0, u)
    co.z -= fr * fr * 0.04 * top; co.x -= fr * 0.012 * (1 - top)
    return co
box('SeatCushion', (SX + 0.01, SY, SZ + 0.235), (0.46, 0.44, 0.11), M_VINYL, bevel=0.035, segs=4, cuts=8, sub=1, deform=cushion_def, smooth=60)
REC = math.radians(12)
BACK_C = Vector((SX - 0.25, SY, SZ + 0.60))
def back_def(co, u, v, w):
    up = (w + 1) / 2
    co.x += 0.035 * math.exp(-((up - 0.30) / 0.16) ** 2) * ss(-1.0, 0.2, u)
    co.x += 0.05 * ss(0.5, 0.95, abs(v)) * ss(-1, 0, u)
    return co
back = box('SeatBack', tuple(BACK_C), (0.10, 0.44, 0.66), M_VINYL, bevel=0.04, segs=4, cuts=8, sub=1, deform=back_def, smooth=60)
back.data.transform(Matrix.Translation(BACK_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-BACK_C))
HEAD_C = BACK_C + Vector((-math.sin(REC) * 0.42, 0, math.cos(REC) * 0.42))
head = box('SeatHeadrest', tuple(HEAD_C), (0.09, 0.28, 0.16), M_VINYL, bevel=0.035, segs=4, cuts=4, sub=1, smooth=60)
head.data.transform(Matrix.Translation(HEAD_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-HEAD_C))
for s in (-1, 1): cyl(f'HeadrestPost_{s}', tuple(HEAD_C + Vector((0.02, s * 0.07, -0.10))), 0.008, 0.10, 'z', M_STEEL, 12)
box('SeatBackShell', tuple(BACK_C + Vector((-0.065, 0, 0))), (0.04, 0.40, 0.62), M_PLAST, bevel=0.012).data.transform(
    Matrix.Translation(BACK_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-BACK_C))
box('SeatRecliner', (SX - 0.20, SY - 0.228, SZ + 0.245), (0.08, 0.025, 0.08), M_DARK, bevel=0.008)
for s in (-1, 1): box(f'BeltAnchor_{s}', (SX - 0.12, SY + s * 0.213, SZ + 0.205), (0.03, 0.01, 0.06), M_STEEL)
box('BeltBuckle', (SX - 0.10, SY - 0.215, SZ + 0.26), (0.06, 0.022, 0.035), M_STEEL, bevel=0.006)
tube('BeltStrap', [(SX - 0.12, SY + 0.213, SZ + 0.22), (SX + 0.02, SY + 0.09, SZ + 0.32), (SX + 0.02, SY - 0.09, SZ + 0.32), (SX - 0.10, SY - 0.215, SZ + 0.26)], 0.004, M_PLAST)
# the boom console on the inner wall at the driver's right hand: joystick (boom up/down), belt switch, e-stop
box('CabBoomConsole', (SX + 0.10, IY0 + 0.029, SZ + 0.38), (0.30, 0.06, 0.10), M_PLAST, bevel=0.01)
box('CabBoomConsole_Bracket', (SX + 0.10, IY0 + 0.004, SZ + 0.30), (0.20, 0.012, 0.10), M_DARK)
JS = Vector((SX + 0.16, IY0 + 0.030, SZ + 0.43))
cyl('CabBoomJoystick_Boot', tuple(JS + Vector((0, 0, 0.004))), 0.022, 0.016, 'z', M_RUB, 16)
cyl('CabBoomJoystick', tuple(JS + Vector((0, 0, 0.05))), 0.007, 0.08, 'z', M_DARK, 12)
cyl('CabBoomJoystick_Knob', tuple(JS + Vector((0, 0, 0.10))), 0.016, 0.04, 'z', M_PLAST, 16, bevel=0.006)
box('CabBoomConsole_BeltSwitch', (SX + 0.03, IY0 + 0.030, SZ + 0.434), (0.03, 0.02, 0.012), M_GREEN)
box('CabBoomConsole_Display', (SX + 0.08, IY0 + 0.061, SZ + 0.38), (0.10, 0.004, 0.05), M_SCREEN)
box('RoofLiner', ((CAB_X0 + 0.04 + xi(ROOF - 0.04)) / 2 - 0.013, CYC, ROOF - 0.045), (xi(ROOF - 0.04) - CAB_X0 - 0.066, IY1 - IY0 + 0.01, 0.020), M_PLAST)
box('CabLamp', (-0.20, CYC, ROOF - 0.058), (0.20, 0.08, 0.016), M_WHITE, bevel=0.005)
box('SunVisor', (0.45, CYC, 1.90), (0.16, 0.36, 0.012), M_PLAST, bevel=0.004)
for s in (-1, 1): box(f'SunVisorClip_{s}', (0.50, CYC + s * 0.16, 1.927), (0.02, 0.02, 0.054), M_PLAST)

# ================================================================ THE DECKS: right (battery hatch, hydraulic tank, extinguisher),
# left rear (tool box), the right side panel (vent, charge socket)
box('BatteryHatch', (-0.10, -0.77, DECK + 0.006), (2.00, 0.56, 0.016), M_TREAD, bevel=0.004)
tube('BatteryHatch_Handle', [(0.70, -0.62, DECK + 0.010), (0.72, -0.62, DECK + 0.05), (0.72, -0.92, DECK + 0.05), (0.70, -0.92, DECK + 0.010)], 0.012, M_STEEL, bez=False)
for k, x in enumerate((-0.80, 0.60)):
    box(f'BatteryHatch_Latch_{k}', (x, -1.02, DECK + 0.017), (0.06, 0.05, 0.010), M_STEEL)
box('HydTank', (1.42, -0.76, DECK + 0.198), (0.48, 0.42, 0.40), M_DARK, bevel=0.015)
cyl('HydTank_Filler', (1.30, -0.66, DECK + 0.415), 0.035, 0.04, 'z', M_YELP, 20, bevel=0.004)
box('HydTank_Gauge', (1.48, -0.97 - 0.003, DECK + 0.22), (0.05, 0.008, 0.18), M_SCREEN)
for k, y in enumerate((-0.62, -0.90)): box(f'HydTank_Strap_{k}', (1.42, y, DECK + 0.20), (0.50, 0.03, 0.41), M_STEEL)
tube('HydHose_0', [(1.20, -0.66, DECK + 0.08), (1.05, -0.60, DECK + 0.05), (0.95, -0.52, DECK + 0.003)], 0.014, M_PLAST)
box('ExtinguisherBracket', (-2.60, -0.85, DECK + 0.048), (0.14, 0.14, 0.10), M_DARK)
cyl('Extinguisher', (-2.60, -0.85, DECK + 0.30), 0.065, 0.46, 'z', M_REDP, 24, bevel=0.02)
cyl('ExtinguisherValve', (-2.60, -0.85, DECK + 0.55), 0.018, 0.05, 'z', M_DARK, 12)
for k, z in enumerate((DECK + 0.16, DECK + 0.40)): box(f'ExtinguisherStrap_{k}', (-2.60, -0.85, z), (0.135, 0.135, 0.02), M_DARK)
TB = box('ToolBox', (-2.55, 0.77, DECK + 0.158), (0.70, 0.48, 0.32), M_PAINT, bevel=0.01)
box('ToolBox_Lid', (-2.55, 0.77, DECK + 0.330 - 0.002), (0.72, 0.50, 0.025), M_TREAD, bevel=0.004)
box('ToolBox_Latch', (-2.55, 0.77 + 0.243, DECK + 0.29), (0.06, 0.012, 0.06), M_STEEL)
box('DeckPlate_FrontL', (1.55, 0.77, DECK + 0.004), (1.60, 0.56, 0.010), M_TREAD)
fr = box('VentFrame', (-0.10, -(BW + 0.006), 0.62), (0.36, 0.016, 0.20), M_PLAST, bevel=0.004)
cut(fr, box('VfCut', (-0.10, -(BW + 0.006), 0.62), (0.32, 0.05, 0.16), M_DARK))
box('VentBack', (-0.10, -(BW - 0.001), 0.62), (0.33, 0.008, 0.17), M_DARK)
for k in range(5):
    z = 0.556 + k * 0.032
    turn(box(f'VentBlade_{k}', (-0.10, -(BW + 0.006), z), (0.324, 0.004, 0.030), M_PLAST), (-0.10, -(BW + 0.006), z), 'x', -40)
box('ChargeSocket', (0.22, -(BW + 0.023), 0.66), (0.12, 0.05, 0.12), M_PLAST, bevel=0.008)
cyl('ChargeSocket_Bore', (0.22, -(BW + 0.049), 0.66), 0.042, 0.006, 'y', M_DARK, 32)
cyl('ChargeSocket_CapHinge', (0.22, -(BW + 0.052), 0.725), 0.006, 0.08, 'x', M_PLAST, 12)
box('ChargeSocket_Cap', (0.22, -(BW + 0.056), 0.67), (0.11, 0.010, 0.10), M_YELP, bevel=0.004)
box('ChargeSocket_Status', (0.22, -(BW + 0.003), 0.575), (0.06, 0.008, 0.02), M_GREEN)

# ================================================================ THE REAR FACE: lamps, chevrons, e-stop
for s, nm in ((1, 'L'), (-1, 'R')):
    box(f'ChevronRear_{nm}', (XR - 0.003, s * 0.75, 0.38), (0.008, 0.50, 0.24), M_CHEV)
    lamp(f'TailBrake_{nm}', (XR, s * 0.80, 0.78), 'x-', 0.12, 0.09, M_RED, M_DARK, M_PLAST)
    lamp(f'IndicatorR_{nm}', (XR, s * 0.80, 0.64), 'x-', 0.12, 0.08, M_AMBER, M_DARK, M_PLAST)
    lamp(f'Reverse_{nm}', (XR, s * 0.60, 0.78), 'x-', 0.10, 0.08, M_WHITE, M_DARK, M_PLAST)
    lamp(f'ReflectorRear_{nm}', (XR, s * 0.60, 0.64), 'x-', 0.10, 0.06, M_RED, M_DARK, M_PLAST, depth=0.024)
box('EStop_Housing', (XR - 0.016, 0.0, 0.42), (0.036, 0.08, 0.08), M_YELP, bevel=0.006)
cyl('EStop_Stem', (XR - 0.040, 0.0, 0.42), 0.016, 0.016, 'x', M_DARK, 20)
cyl('EStop_Button', (XR - 0.054, 0.0, 0.42), 0.030, 0.016, 'x', M_REDP, 32, bevel=0.005)

for o in (shell, leaf): seal(o)

# ================================================================ IK + pivots (from the parts' own positions)
ik('Seat_Driver', (SX + 0.02, SY, SZ + 0.30), 'SeatCushion'); ik('Back_Driver', (SX - 0.21, SY, SZ + 0.58), 'SeatBack')
ik('Hand_L', tuple(WC + WR * (math.cos(math.radians(30)) * WU + math.sin(math.radians(30)) * WV)), 'SteeringWheel')
ik('Hand_R', tuple(WC + WR * (math.cos(math.radians(150)) * WU + math.sin(math.radians(150)) * WV)), 'SteeringWheel')
ik('SteeringWheel_Centre', tuple(WC), 'SteeringWheel_Hub')
ik('Foot_Brake', (0.24, SYC + 0.17, FLOOR + 0.15), 'Pedal_Brake'); ik('Foot_Accel', (0.24, SYC - 0.17, FLOOR + 0.15), 'Pedal_Accel')
ik('Hand_Direction', (CP.x - 0.021, SYC + 0.145, CP.z + 0.021), 'DirectionLever_Knob'); ik('Hand_Key', (0.34, SYC - 0.135, 0.82), 'Key')
ik('Hand_BoomLever', tuple(JS + Vector((0, 0, 0.10))), 'CabBoomJoystick_Knob')
ik('Step', (STEP_X, BW - 0.07, 0.268), 'StepTread')
ik('Grab_Outside', (GX, CAB_Y1 + 0.06, 1.41), 'GrabHandleOut'); ik('Grab_Inside', (GIX, CAB_Y1 - 0.09, 1.50), 'GrabHandleIn')
ik('Door_Outside', (-0.50, CAB_Y1 + 0.04, 1.00), 'DoorDriver_HandleOut'); ik('Door_Inside', (-0.50, DYC - 0.055, 1.02), 'DoorDriver_HandleIn')
ik('Door_Hinge', DOOR_PIVOT)
# boom-local points (move with the boom: apply the boom's turn about IK_Boom_Hinge)
ik('Boom_Hinge', tuple(PV))
ik('Belt_Bottom', (-3.55, 0, BELT_Z1), 'BeltSurface'); ik('Belt_Top', (3.60, 0, BELT_Z1), 'BeltSurface')
ik('Controls_Bottom', CTRL_BOT, 'Boom_ControlsBottom'); ik('Controls_Top', CTRL_TOP, 'Boom_ControlsTop')
ik('Bumper_Pivot', tuple(BPV)); ik('Bumper_Contact', tuple(BUMP_CONTACT), 'BumperPad')
ik('Stand_Top', (4.30, 0.0, BELT_Z1 - 0.03))          # the hold loader inside the hold door (boom-local: the hold floor ~ belt top)
ik('Rail_Top_L', (2.18, (CH_Y0 + CH_Y1) / 2, RAIL_TOP), 'Boom_RailTopL'); ik('Rail_Top_R', (2.18, -(CH_Y0 + CH_Y1) / 2, RAIL_TOP), 'Boom_RailTopR')
ik('WorkLamp', (3.67, -(CH_Y0 + CH_Y1) / 2, BZ1 + 0.14), 'WorkLamp_Lens')
ik('Ram_Front_Base', tuple(B_RF), 'RamBaseFront_Pin'); ik('Ram_Front_Top', tuple(T_RF), 'Boom_RamPinFront')
ik('Ram_Rear_Base', tuple(B_RR), 'RamBaseRear_Pin'); ik('Ram_Rear_Top', tuple(T_RR), 'Boom_RamPinRear')
# ground stand points (vehicle-local): putting bags on at the bottom end, the controls, the door
ik('Stand_Load', (-4.25, 0.25, 0.0)); ik('Stand_ControlsBottom', (-3.50, 0.80, 0.0)); ik('Stand_Door', (-0.15, 1.55, 0.0))
ik('Charge_Socket', (0.22, -(BW + 0.06), 0.66), 'ChargeSocket'); ik('EStop', (XR - 0.065, 0.0, 0.42), 'EStop_Button')
ik('Beacon', (BX, CYC, RT + 0.083))
BOOM_PREFIX = ('Boom', 'BeltSurface', 'BumperPad', 'WorkLamp')

# ================================================================ checks
SKIP_OH = ('StepNosing', 'GrabHandleOut', 'GrabHandleIn', 'BatteryHatch_Handle', 'Key', 'BinnacleDisplay', 'ChargeSocket_Cap', 'ChargeSocket_CapHinge', 'ChargeSocket_Status',
           'DirectionLever', 'DirectionLever_Knob', 'SteeringColumn', 'ColumnShroud', 'Extinguisher', 'ExtinguisherValve', 'TowLug_Front', 'HydHose_0', 'HydTank_Filler', 'Binnacle')
OH_PREFIX = ('Hinge', 'DoorDriver_Hinge', 'DoorDriver_Handle', 'DoorDriver_Pull', 'DoorDriver_Lock', 'Nut_', 'Hub', 'Spindle', 'SteerArm', 'Kingpin', 'FrontAxle', 'RearAxle',
             'Mirror', 'VentBlade', 'Steering', 'Pedal', 'Seat', 'Headrest', 'Belt', 'EStop', 'GrabOutFoot', 'GrabInFoot', 'SunVisorClip', 'DashSwitch', 'DefrostVent',
             'BinnacleTell', 'MatRib', 'KeySwitch', 'Ram', 'Boom_Ram', 'Boom_Bearing', 'Boom_DrumShaft', 'Boom_Idler', 'Boom_Rail', 'Boom_BumperBracket', 'Boom_BumperPin',
             'BumperPad', 'Boom_Controls', 'WorkLamp', 'BoomRestPad', 'Boom_HingeBoss', 'Boom_BedPost', 'CabBoomJoystick', 'CabBoomConsole', 'HydTank', 'ExtinguisherStrap',
             'BatteryHatch_Latch', 'ToolBox', 'Boom_Reflector', 'Boom_LoadGuide')
fl, cl, oh, lo = K.checks('BL', ground_ok=('Tyre',), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)
for n in oh: print('BL overhang', n, [m.name for m in ALL if m.name != n and K.touch(K.bbox(bpy.data.objects[n]), K.bbox(m)) and K.diag(K.bbox(m)) > 0.4])
crowd = K.arch_crowd([(FX, TZ, F_ARCH_R), (RX, TZ, R_ARCH_R)], BW, skip_prefix=('Body', 'Tyre', 'Rim', 'WheelDisc', 'Hub', 'Nut', 'Spindle', 'RearAxle', 'FrontAxle', 'Kingpin', 'SteerArm', 'BumperFront'))
for o in [o for o in ALL if o.name.startswith(('MirrorArm', 'Grab', 'Steering', 'DirectionLever', 'PedalArm', 'BeltStrap', 'BatteryHatch_Handle', 'HydHose'))]:
    seal(o)
JOIN = [('SteeringWheel_Hub', 'SteeringWheel_Spoke_%d' % k) for k in range(3)] + [('SteeringWheel', 'SteeringWheel_Spoke_%d' % k) for k in range(3)] + [
    ('SteeringWheel_Hub', 'SteeringWheel_Horn'), ('SteeringWheel_Hub', 'SteeringColumn'), ('SteeringColumn', 'Pedestal'), ('ColumnShroud', 'SteeringColumn'),
    ('DirectionLever', 'SteeringColumn'), ('DirectionLever_Knob', 'DirectionLever'), ('Binnacle', 'Pedestal'), ('BinnacleDisplay', 'Binnacle'),
    ('KeySwitch', 'Pedestal'), ('Key', 'KeySwitch'), ('EStopIn_Housing', 'Pedestal'), ('EStopIn_Button', 'EStopIn_Housing'), ('Pedestal', 'FloorMat'),
    ('PedalPivot_Brake', 'FloorMat'), ('PedalArm_Brake', 'PedalPivot_Brake'), ('Pedal_Brake', 'PedalArm_Brake'),
    ('PedalPivot_Accel', 'FloorMat'), ('PedalArm_Accel', 'PedalPivot_Accel'), ('Pedal_Accel', 'PedalArm_Accel'),
    ('SeatRails', 'FloorMat'), ('SeatBellows_0', 'SeatRails'), ('SeatBellows_4', 'SeatPan'), ('SeatCushion', 'SeatPan'), ('SeatBack', 'SeatCushion'), ('SeatBackShell', 'SeatBack'),
    ('SeatHeadrest', 'HeadrestPost_1'), ('HeadrestPost_1', 'SeatBack'), ('SeatRecliner', 'SeatCushion'),
    ('BeltAnchor_1', 'SeatPan'), ('BeltAnchor_-1', 'SeatPan'), ('BeltStrap', 'BeltBuckle'), ('BeltBuckle', 'SeatCushion'), ('BeltStrap', 'BeltAnchor_1'),
    ('CabBoomConsole_Bracket', 'Body'), ('CabBoomConsole', 'CabBoomConsole_Bracket'), ('CabBoomJoystick_Boot', 'CabBoomConsole'), ('CabBoomJoystick', 'CabBoomJoystick_Boot'), ('CabBoomJoystick_Knob', 'CabBoomJoystick'),
    ('CabLamp', 'RoofLiner'), ('RoofLiner', 'CabShell'), ('SunVisorClip_1', 'SunVisor'), ('SunVisorClip_1', 'RoofLiner'),
    ('DashSwitch_0', 'Dash'), ('DefrostVent_0', 'Dash'), ('Dash', 'FloorMat'),
    ('DoorDriver_HandleIn', 'DoorDriver_Trim'), ('DoorDriver_Trim', 'DoorDriver'), ('DoorDriver_Pull', 'DoorDriver_Trim'),
    ('GrabHandleIn', 'GrabInFoot_0'), ('GrabInFoot_0', 'CabShell'),
    # the boom's own joints and the rams
    ('BeltSurface', 'Boom_BeltWrapF'), ('BeltSurface', 'Boom_BeltWrapR'), ('Boom_BeltReturn', 'Boom_BeltWrapF'), ('Boom_DrumF', 'Boom_DrumShaftF'), ('Boom_DrumShaftF', 'Boom_ChannelL'),
    ('Boom_BearingF_1', 'Boom_ChannelL'), ('Boom_Idler_0', 'Boom_ChannelL'), ('Boom_Bed', 'Boom_ChannelL'), ('Boom_Pan', 'Boom_ChannelR'), ('Boom_Cross_0', 'Boom_Pan'),
    ('Boom_RailPostL0', 'Boom_RailFootL0'), ('Boom_RailFootL0', 'Boom_ChannelL'), ('Boom_RailTopL', 'Boom_RailPostL0'), ('Boom_RailMidL', 'Boom_RailPostL1'),
    ('Boom_BumperPin', 'Boom_BumperBracket_L'), ('Boom_BumperBracket_L', 'Boom_ChannelL'), ('BumperPad_Arm_L', 'Boom_BumperPin'), ('BumperPad_Frame', 'BumperPad_Arm_L'), ('BumperPad', 'BumperPad_Frame'),
    ('Boom_ControlsTop', 'Boom_ChannelL'), ('Boom_ControlsBottom', 'Boom_ChannelL'), ('WorkLamp_Foot', 'Boom_ChannelR'), ('WorkLamp_Stem', 'WorkLamp_Foot'), ('WorkLamp_Body', 'WorkLamp_Stem'),
    ('HingeBoom_Pin_L', 'Boom_ChannelL'), ('HingeBoom_Pin_L', 'HingeBoom_Bracket_L'), ('HingeBoom_Bracket_L', 'Body'), ('BoomRestPad_0L', 'Body'),
    ('RamFront_BarrelEye', 'RamBaseFront_Pin'), ('RamBaseFront_Pin', 'RamBaseFront_Ear_1'), ('RamBaseFront_Ear_1', 'RamBaseFront_Plate'), ('RamBaseFront_Plate', 'Body'),
    ('RamFront_BarrelNeck', 'RamFront_BarrelEye'), ('RamFront_Barrel', 'RamFront_BarrelNeck'), ('RamFront_Rod', 'RamFront_Barrel'), ('RamFront_RodEye', 'RamFront_Rod'), ('RamFront_RodEye', 'Boom_RamPinFront'),
    ('Boom_RamPinFront', 'Boom_RamEarFront_1'), ('Boom_RamEarFront_1', 'Boom_RamPlateFront'), ('Boom_RamPlateFront', 'Boom_Pan'),
    ('RamRear_BarrelEye', 'RamBaseRear_Pin'), ('RamRear_BarrelNeck', 'RamRear_BarrelEye'), ('RamRear_Barrel', 'RamRear_BarrelNeck'), ('RamRear_Rod', 'RamRear_Barrel'), ('RamRear_RodEye', 'Boom_RamPinRear'), ('Boom_RamPlateRear', 'Boom_Pan'),
    ('HydTank', 'Body'), ('Extinguisher', 'ExtinguisherBracket'), ('ExtinguisherBracket', 'Body'), ('ToolBox', 'Body'), ('ToolBox_Lid', 'ToolBox'), ('BatteryHatch', 'Body')]
def joined(a, b):
    ta, tb = K.bvh(bpy.data.objects[a]), K.bvh(bpy.data.objects[b])
    if ta.overlap(tb): return True
    oa = bpy.data.objects[a]
    return any((h := tb.find_nearest(oa.matrix_world @ v.co))[0] is not None and h[3] < 0.001 for v in oa.data.vertices)
not_joined = [(a, b) for a, b in JOIN if not joined(a, b)]
print('BL CHECK joints: %d of %d pairs not joined %s' % (len(not_joined), len(JOIN), not_joined))
open_edges = []
for o in ALL:
    bm = bmesh.new(); bm.from_mesh(o.data); n = sum(1 for e in bm.edges if not e.is_manifold); bm.free()
    if n: open_edges.append((o.name, n))
print('BL CHECK open (non-manifold) edges: %d parts %s' % (len(open_edges), open_edges[:20]))
if '--diag' in sys.argv:
    bm = bmesh.new(); bm.from_mesh(bpy.data.objects['Body'].data)
    print('BL DIAG body open edges', [(tuple(round(c, 3) for c in (e.verts[0].co + e.verts[1].co) / 2), len(e.link_faces), round(e.calc_length(), 4)) for e in bm.edges if not e.is_manifold][:40]); bm.free()
print('BL CHECK arch clearance: %d parts crowd an arch %s' % (len(crowd), crowd))
WHEEL_F = lambda sd: tuple(p + 'F' + sd for p in ('Tyre_', 'Rim_', 'WheelDisc_', 'Hub_', 'HubCap_', 'Nut_', 'Spindle_', 'SteerArm_'))
steer_hits = set()
for sd, s in (('L', 1), ('R', -1)):
    for d in (-STEER, -STEER / 2, STEER / 2, STEER):
        steer_hits |= K.sweep(WHEEL_F(sd), (FX, s * KP_Y, TZ), 'z', [d], static_skip_prefix=('Kingpin_' + sd, 'FrontAxle_Arm_' + sd))
print('BL CHECK steering sweep +-%d deg: %d hits %s' % (STEER, len(steer_hits), sorted(steer_hits)[:12]))
door_hits = K.sweep('DoorDriver', DOOR_PIVOT, 'z', [-25, -50, -75, -95], static_skip_prefix=('HingeDoor',))
print('BL CHECK door opens 25/50/75/95: %d hits %s' % (len(door_hits), sorted(door_hits)[:12]))
# the boom raised step by step: the boom group against everything that stays put (rams follow, so they are checked apart)
boom_hits = K.sweep(BOOM_PREFIX, tuple(PV), 'y', [-3, -A320_DEG, -14, -MAX_DEG], static_skip_prefix=('Ram', 'HingeBoom', 'BoomRestPad'))
print('BL CHECK boom raise 3/%.1f/14/%.1f: %d hits %s' % (A320_DEG, MAX_DEG, len(boom_hits), sorted(boom_hits)[:12]))
# the bumper pad swinging on its pin (keeps upright as the boom tilts): +-25 deg
bump_hits = K.sweep('BumperPad', tuple(BPV), 'y', [-15, 8, 15, 22], static_skip_prefix=('Boom_BumperPin',))
print('BL CHECK bumper pad swing -15..+22: %d hits %s' % (len(bump_hits), sorted(bump_hits)[:12]))
# the rams in their raised poses: barrel turned about its base, rod following its top eye
def pose_matrices(deg):
    """The transforms of the boom group, each ram's barrel and rod, the bumper pad, for the boom raised by deg."""
    Rb = Matrix.Translation(PV) @ Matrix.Rotation(math.radians(-deg), 4, 'Y') @ Matrix.Translation(-PV)
    out = {'boom': Rb}
    for nm, B, T in (('Front', B_RF, T_RF), ('Rear', B_RR, T_RR)):
        T2 = Rb @ T; a0 = math.atan2((T - B).z, (T - B).x); a1 = math.atan2((T2 - B).z, (T2 - B).x)
        Rr = Matrix.Translation(B) @ Matrix.Rotation(-(a1 - a0), 4, 'Y') @ Matrix.Translation(-B)
        out['Ram%s_Barrel' % nm] = Rr
        out['Ram%s_Rod' % nm] = Matrix.Translation(T2 - Rr @ T) @ Rr
    BP2 = Rb @ BPV
    out['BumperPad'] = Matrix.Translation(BP2) @ Matrix.Rotation(math.radians(deg), 4, 'Y') @ Matrix.Translation(-BP2) @ Rb
    return out
def pose_of(name, P):
    if name.startswith('BumperPad'): return P['BumperPad']
    for k in ('RamFront_Barrel', 'RamFront_Rod', 'RamRear_Barrel', 'RamRear_Rod'):
        if name.startswith(k): return P[k]
    if name.startswith(BOOM_PREFIX): return P['boom']
    return None
ram_hits = set()
for deg in (A320_DEG, MAX_DEG):
    P = pose_matrices(deg)
    trees = {}
    for o in ALL:
        M = pose_of(o.name, P) or Matrix.Identity(4)
        trees[o.name] = BVHTree.FromPolygons([M @ (o.matrix_world @ v.co) for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
    for o in ALL:
        if not o.name.startswith(('RamFront', 'RamRear')): continue
        nm = 'Front' if 'Front' in o.name else 'Rear'
        for n, tr in trees.items():
            if n == o.name or n.startswith(('Ram' + nm, 'RamBase' + nm, 'Boom_RamPin' + nm, 'Boom_RamEar' + nm, 'Boom_RamPlate' + nm)): continue
            if trees[o.name].overlap(tr): ram_hits.add((o.name, n, deg))
print('BL CHECK rams in raised poses: %d hits %s' % (len(ram_hits), sorted(ram_hits)[:12]))
ik_off = []
for e in IKS:
    part = IKP.get(e.name)
    if not part: continue
    o = bpy.data.objects[part]; tr = K.bvh(o); hit = tr.find_nearest(e.location)
    if hit[0] is None or hit[3] > 0.06: ik_off.append((e.name, part, round(hit[3], 3) if hit[0] is not None else None))
print('BL CHECK IK points on their parts: %d off %s' % (len(ik_off), ik_off))
sill_min = round(SILL_PT.z, 3); sill_max = round(raised(SILL_PT, MAX_DEG).z, 3)
print('BL sill range %.3f .. %.3f m, A320 bulk 2.10 m at %.2f deg' % (sill_min, sill_max, A320_DEG))

# ---------------------------------------------------------------- the moving parts, IK points, sizes -> json
def bb_all(objs):
    lo_ = Vector((1e9,) * 3); hi_ = Vector((-1e9,) * 3)
    for o in objs:
        a, b = K.bbox(o); lo_ = Vector(tuple(min(lo_[i], a[i]) for i in range(3))); hi_ = Vector(tuple(max(hi_[i], b[i]) for i in range(3)))
    return lo_, hi_
LO, HI = bb_all(ALL)
BLO, BHI = bb_all([o for o in ALL if not o.name.startswith(('Mirror',))])
r4 = lambda v: [round(c, 4) for c in v]
BOOM_LOCAL_IK = ['IK_Belt_Bottom', 'IK_Belt_Top', 'IK_Controls_Bottom', 'IK_Controls_Top', 'IK_Bumper_Pivot', 'IK_Bumper_Contact', 'IK_Stand_Top', 'IK_Rail_Top_L',
                 'IK_Rail_Top_R', 'IK_WorkLamp', 'IK_Ram_Front_Top', 'IK_Ram_Rear_Top']
info = {'size_m': {'x_min': round(LO.x, 3), 'x_max': round(HI.x, 3), 'length': round(HI.x - LO.x, 3), 'width_mirrors': round(HI.y - LO.y, 3),
                   'width_body': round(2 * BW, 3), 'height_beacon': round(HI.z, 3), 'height_roof': round(ROOF + 0.04, 3), 'chassis_length': round(XF - XR, 3),
                   'boom_length': round(XB1 - XB0, 3), 'belt_length_centres': round(2 * DRUM_X, 3), 'belt_width': BELT_W, 'wheelbase': round(FX - RX, 3), 'track': round(2 * TRK2, 3),
                   'tyre': [TD, TW], 'floor': FLOOR, 'deck': DECK, 'seat_cushion_top': round(SZ + 0.30, 3), 'ground_clearance': Z_BOT, 'front_axle_x': FX, 'rear_axle_x': RX,
                   'belt_top_rest': BELT_Z1, 'sill_min': sill_min, 'sill_max': sill_max},
        'boom': {'parts': 'Boom*, BeltSurface, BumperPad* (+ its own swing), WorkLamp*', 'pivot': r4(PV), 'axis': 'y',
                 'note': 'raises with a NEGATIVE rotation about +y through the lower hinge; 0 = rest (belt level), -%.1f deg = fully raised' % MAX_DEG,
                 'range_deg': [0, -MAX_DEG], 'a320_bulk_hold_deg': -A320_DEG, 'a320_sill_m': 2.10,
                 'sill_point_rest': r4(SILL_PT), 'deg_for_sill': 'solve z(PV + Ry(-a) (SILL_PT - PV)) = sill'},
        'belt': {'part': 'BeltSurface', 'IK_Belt_Bottom': r4(IKS[[e.name for e in IKS].index('IK_Belt_Bottom')].location),
                 'IK_Belt_Top': r4(IKS[[e.name for e in IKS].index('IK_Belt_Top')].location), 'width': BELT_W, 'usable_width': round(2 * (CH_Y0 - 0.022), 3),
                 'speed_mps_real': 0.45, 'note': 'boom-local (rest pose); bags pinned along Bottom->Top, offset up by half the bag height; UV scrolls along +x'},
        'rams': {'RamFront': {**RAMS['Front'], 'parts_barrel': 'RamFront_Barrel* (turns about base_pivot)', 'parts_rod': 'RamFront_Rod* (eye stays on the boom top pivot)'},
                 'RamRear': {**RAMS['Rear'], 'parts_barrel': 'RamRear_Barrel*', 'parts_rod': 'RamRear_Rod*'},
                 'note': 'barrel angle = atan2 of (boom top pivot now - base) minus the rest angle, NEGATIVE about +y for a rise; rod = same turn, translated so its eye sits on the top pivot'},
        'bumper': {'parts': 'BumperPad*', 'pivot_boom_local': r4(BPV), 'axis': 'y', 'range_deg': [-15, 22],
                   'note': 'hangs upright: turn it +boom_angle (opposite the boom) about its pin after the boom turn; contact IK_Bumper_Contact'},
        'doors': {'DoorDriver': {'pivot': r4(DOOR_PIVOT), 'axis': 'z', 'parts': 'DoorDriver* (leaf, glass, handles, trim, hinge leaves)',
                                 'note': 'hinged on its front edge; opens outward with a NEGATIVE rotation about +z, up to -95 deg (checked clear)'}},
        'steering': {'front_wheels': {'FL': {'pivot': [FX, KP_Y, TZ]}, 'FR': {'pivot': [FX, -KP_Y, TZ]}}, 'axis': 'z', 'max_deg': STEER,
                     'parts_per_wheel': 'Tyre_/Rim_/WheelDisc_/Hub_/HubCap_/Nut_/Spindle_/SteerArm_ + FL|FR',
                     'steering_wheel': {'parts': 'SteeringWheel*', 'centre': r4(WC), 'axis': r4(AX)}},
        'boom_joystick': {'parts': 'CabBoomJoystick, CabBoomJoystick_Knob', 'pivot': r4(JS), 'axis': 'y', 'range_deg': [-20, 20]},
        'pedals': {k: {'pivot': list(v[0]), 'axis': v[1]} for k, v in PEDALS.items()},
        'wheels': {k: r4(v) for k, v in WHEELS.items()},
        'lamps': sorted(o.name for o in ALL if o.name.endswith('_Lens') or o.name == 'BeaconLens'),
        'ik': {e.name: r4(e.location) for e in IKS}, 'ik_boom_local': BOOM_LOCAL_IK, 'ik_bumper_local': ['IK_Bumper_Contact'],
        'checks': {'joints_not_joined': not_joined, 'open_edges': open_edges, 'floating': fl, 'lamp_clash': cl, 'overhang': oh, 'mesh_loose': lo, 'arch_crowd': crowd,
                   'steer_hits': len(steer_hits), 'door_hits': len(door_hits), 'boom_hits': sorted(boom_hits), 'bumper_hits': len(bump_hits), 'ram_hits': sorted(ram_hits), 'ik_off': ik_off}}

# ================================================================ CC0 photo materials (Art/BeltLoader/Textures/SOURCES.md)
TEX = os.path.join(ART, 'Textures')
if not (DO_WIRE or DO_CHECK):
    K.uv_cube()
    K.photo(M_PAINT, TEX, 'Paint004', tile=1.5, tint=(1.0, 0.50, 0.015))
    K.photo(M_CAB, TEX, 'Paint004', tile=1.5, tint=(0.97, 0.97, 0.95))
    K.photo(M_STEEL, TEX, 'Metal027', tile=0.6)
    K.photo(M_DARK, TEX, 'Metal027', tile=0.6, tint=(0.25, 0.25, 0.27))
    K.photo(M_RUB, TEX, 'Rubber004', tile=0.5, tint=(0.35, 0.35, 0.35), metal=0.0)
    K.photo(M_BELT, TEX, 'Rubber004', tile=0.35, tint=(0.30, 0.30, 0.30), metal=0.0)
    K.photo(M_BUMP, TEX, 'Rubber004', tile=0.4, tint=(0.28, 0.28, 0.28), metal=0.0, rough_mul=0.9)
    K.photo(M_FLOOR, TEX, 'Rubber001', tile=0.5, metal=0.0)
    K.photo(M_PLAST, TEX, 'Plastic006', tile=0.4, tint=(0.35, 0.35, 0.37), metal=0.0)
    K.photo(M_VINYL, TEX, 'Leather026', tile=0.35, metal=0.0)
    K.photo(M_TREAD, TEX, 'DiamondPlate008A', tile=0.5)
    K.photo(M_CHEV, TEX, 'PaintedMetal016', tile=0.9)
    K.photo(M_REDP, TEX, 'Paint004', tile=0.8, tint=(1.0, 0.06, 0.04), metal=0.0)
    K.photo(M_YELP, TEX, 'Paint004', tile=0.8, tint=(1.0, 0.62, 0.02), metal=0.0)
    K.photo(M_CHROME, TEX, 'Metal027', tile=0.4, tint=(1.6, 1.6, 1.6), metal=1.0, rough_mul=0.35)
    K.glass(M_GLASS)

ZF = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, os.path.join(ART, 'BeltLoader.blend'), os.path.join(ART, 'beltloader.glb'), ZF, 'BL', 'BeltLoader') if not (DO_WIRE or DO_BEAUTY or DO_CHECK) else 'skipped'
info['checks']['flicker'] = last
print('BL: %d parts, %d IK/pivot points, %d faces, flicker: %s' % (len(ALL), len(IKS), sum(len(o.data.polygons) for o in ALL), last))
if not (DO_WIRE or DO_BEAUTY or DO_CHECK):
    a = K.checks('BL after flicker fix', ground_ok=('Tyre',), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)
    info['checks']['after_flicker_fix'] = {'floating': a[0], 'lamp_clash': a[1], 'overhang': a[2], 'mesh_loose': a[3]}
    info['faces'] = sum(len(o.data.polygons) for o in ALL); info['parts'] = len(ALL)

# ================================================================ pictures
def open_parts(prefix, pivot, axis, deg):
    R = Matrix.Translation(Vector(pivot)) @ Matrix.Rotation(math.radians(deg), 4, axis.upper()) @ Matrix.Translation(-Vector(pivot))
    for o in ALL:
        if o.name.startswith(prefix): o.data.transform(R); o.data.update()
REST = {}
def raise_boom(deg):
    """Poses the boom, rams and bumper pad for a picture, always from the rest pose (after the export, so the exported
    model stays at rest)."""
    P = pose_matrices(deg)
    for o in ALL:
        M = pose_of(o.name, P)
        if M is None: continue
        if o.name not in REST: REST[o.name] = [v.co.copy() for v in o.data.vertices]
        for v, c in zip(o.data.vertices, REST[o.name]): v.co = c
        o.data.transform(M); o.data.update()
if DO_BEAUTY:
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; ee = sc.eevee
    ee.use_gtao = True; ee.gtao_distance = 0.4; ee.use_ssr = True; ee.use_ssr_refraction = True; ee.use_soft_shadows = True
    ee.shadow_cube_size = '2048'; ee.shadow_cascade_size = '4096'; ee.taa_render_samples = 96; ee.use_bloom = True; ee.bloom_intensity = 0.02
    sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
    w = bpy.data.worlds.new('WB'); w.use_nodes = True; bg = w.node_tree.nodes['Background']; bg.inputs['Color'].default_value = (0.55, 0.65, 0.8, 1); bg.inputs['Strength'].default_value = 0.9; sc.world = w
    gm = pmat('Apron', (0.32, 0.32, 0.31), 0, 0.85)
    ground = box('Ground', (0, 0, -0.01), (40, 40, 0.02), gm); ALL.remove(ground)
    sd = bpy.data.lights.new('Sun', 'SUN'); sd.energy = 4.5; sd.angle = 0.02; so = bpy.data.objects.new('Sun', sd); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(35))
    cd = bpy.data.cameras.new('BCam'); bc = bpy.data.objects.new('BCam', cd); sc.collection.objects.link(bc); sc.camera = bc
    def bshot(name, loc, at, lens=45, res=(2400, 1500)):
        bc.location = loc; bc.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens
        sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(PICS, name + '.png'); bpy.ops.render.render(write_still=True); print('BL: picture', name)
    bshot('BeltLoader-Final-Front', (8.6, 6.3, 2.6), (0.2, 0.0, 0.95), 35, (3840, 2160))
    bshot('BeltLoader-Final-Back', (-8.6, -5.6, 3.1), (-0.3, 0.0, 0.95), 35, (3840, 2160))
    bshot('BeltLoader-Close-TopEnd', (5.4, -2.2, 1.6), (3.6, 0.0, 0.85), 40)
    bshot('BeltLoader-Close-Mirror', (2.2, 2.6, 1.55), (0.60, 1.10, 1.05), 40)
    open_parts('DoorDriver', DOOR_PIVOT, 'z', -80)
    bshot('BeltLoader-Close-Cab', (-0.2, 2.6, 1.85), (0.0, 0.75, 1.0), 28)
    open_parts('DoorDriver', DOOR_PIVOT, 'z', 80)
    raise_boom(A320_DEG)
    bshot('BeltLoader-Final-Raised', (5.5, 8.6, 2.4), (0.6, 0.0, 1.30), 32, (3840, 2160))
    bshot('BeltLoader-Close-Rams', (0.5, -4.2, 0.9), (0.0, 0.0, 0.85), 30)

if DO_WIRE:
    shoot = K.wire_setup()
    AT = (0.0, 0.0, 1.0)
    W = lambda n: os.path.join(WIRE, n + '.png')
    VIEWS = {}
    def ortho(name, loc, at, scale, res, hide=()):
        shoot(W(name), loc, at, 50, scale, res=res, hide=hide)
        from bpy_extras.object_utils import world_to_camera_view
        sc = bpy.context.scene; bpy.context.view_layer.update()
        def px(p):
            v = world_to_camera_view(sc, sc.camera, Vector(p)); return [round(v.x * res[0], 2), round((1 - v.y) * res[1], 2)]
        VIEWS[name] = {'loc': list(loc), 'at': list(at), 'ortho': scale, 'res': list(res), 'O': px((0, 0, 0)), 'X': px((1, 0, 0)), 'Y': px((0, 1, 0)), 'Z': px((0, 0, 1))}
    shoot(W('1-three-quarter-front'), (9.0, 7.0, 4.2), AT, 45)
    shoot(W('2-three-quarter-rear'), (-9.0, -7.0, 4.4), AT, 45)
    ortho('3-side-left', (0.0, 20, 1.2), (0.0, 0, 1.2), 9.2, (2600, 1200))
    ortho('4-side-right', (0.0, -20, 1.2), (0.0, 0, 1.2), 9.2, (2600, 1200))
    ortho('5-front', (20, 0, 1.15), (0, 0, 1.15), 3.2, (1500, 1300))
    ortho('6-rear', (-20, 0, 1.15), (0, 0, 1.15), 3.2, (1500, 1300))
    ortho('7-top', (0.0, 0, 20), (0.0, 0, 0), 9.2, (2600, 1200))
    CUT = tuple(o.name for o in ALL if o.name.startswith(('CabShell', 'RoofCap', 'RoofLiner', 'CabLamp', 'Beacon', 'Win', 'DoorDriver', 'SunVisor', 'GrabHandleIn', 'GrabInFoot', 'Mirror')))
    shoot(W('8-cab-interior'), (1.6, 2.9, 2.75), (-0.2, 0.75, 0.95), 30, hide=CUT)
    shoot(W('9-seat-and-controls'), (0.9, 0.15, 1.75), (-0.25, 0.80, 0.95), 30, hide=CUT)
    shoot(W('10-top-end'), (5.6, -2.0, 1.8), (3.6, 0.0, 0.85), 40)
    shoot(W('11-bottom-end'), (-5.6, 2.2, 1.7), (-3.4, 0.2, 0.80), 40)
    shoot(W('12-mirror-and-hinges'), (1.7, 2.6, 1.40), (0.45, 1.05, 1.10), 35)
    BODY = tuple(o.name for o in ALL if o.name.startswith(('Body',)))
    shoot(W('13-rams-and-hinge'), (-0.6, -4.6, 1.3), (-0.8, 0.0, 0.60), 32, hide=BODY)
    shoot(W('14-underside'), (0.3, -5.0, -1.2), (0.0, 0, 0.30), 35)
    raise_boom(A320_DEG)
    ortho('15-side-raised-A320', (0.0, 20, 1.6), (0.0, 0, 1.6), 9.2, (2600, 1300))
    raise_boom(MAX_DEG)
    ortho('16-side-raised-max', (0.0, 20, 1.8), (0.0, 0, 1.8), 9.2, (2600, 1400))
    info['wire_views'] = VIEWS

JP = os.path.join(ART, 'beltloader-parts.json')
if not DO_WIRE and os.path.exists(JP):
    old = json.load(open(JP)); info.setdefault('wire_views', old.get('wire_views'))
if not (DO_BEAUTY or DO_CHECK) or not os.path.exists(JP):
    json.dump(info, open(JP, 'w'), indent=1)
print('BL size', info['size_m'])
