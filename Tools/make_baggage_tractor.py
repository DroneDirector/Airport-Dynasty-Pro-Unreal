# The electric baggage tow tractor - our own design, drawn to real-world standards along the lines of the compact European
# battery-electric closed-cab tractors (Charlatte T135/T137, Mulag Comet 3E/4E, TLD JST-25E class; numbers and photos in
# Blueprints/Vehicles/BaggageTractor/RESEARCH.md). No brand names or logos. It tows baggage carts / ULD dollies and our
# ground power unit (Art/GPU: towing eye IK_TowEye 0.40 m above the ground) on a multi-height rear pin hitch (eye slots
# at 280 / 400 / 490 mm like the real 3-height jaw), and has a front pin hitch for positioning. Built to the vehicle
# standard learned on the pushback tug (Tools/make_pushback_tug.py; helpers + checks shared in Tools/vehicle_kit.py).
# Shape: a yellow pressed-steel body tub (cab-forward: flat front face with the lamps, black rubber bumper band, front
# hitch in the middle), deep wheel arches over small industrial wheels (front 6.00-9 steering, rear 7.00-12 driven),
# a white closed cab on the front two thirds with a near-vertical windscreen, a real hinged driver door (left, hinged on
# its front edge, glass in it), fixed side glass, a rear window behind a steel mesh guard, the roof with an amber
# beacon; the rear deck behind the cab is the chequer-plate battery cover (hinged at its rear edge) over the 80 V
# traction battery; tail/brake/indicator/reverse lamps, emergency stop and charge socket in the rear face.
# Axes: x forward, y left, z up; metres; origin on the ground under the middle of the wheelbase.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_baggage_tractor.py [-- --wire | --beauty]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import importlib, vehicle_kit as K
importlib.reload(K)
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, lamp, pmat, ALL, IKS

ART = os.path.join(ROOT, 'Art', 'BaggageTractor'); os.makedirs(ART, exist_ok=True)
BP = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'BaggageTractor')
WIRE = os.path.join(BP, 'wire'); os.makedirs(WIRE, exist_ok=True)
PICS = os.path.join(ROOT, 'docs', 'Pictures', 'Vehicles', 'BaggageTractor'); os.makedirs(PICS, exist_ok=True)
DO_WIRE, DO_BEAUTY, DO_CHECK = '--wire' in sys.argv, '--beauty' in sys.argv, '--check' in sys.argv   # (--check: build + checks only)
K.reset()
IKP = {}                                   # IK point -> the part it belongs to (checked: it sits on that part)
def ik(name, loc, part=None):
    IKP['IK_' + name] = part; return K.ik(name, loc)

# ---------------------------------------------------------------- the main sizes (m)
BW = 0.70                                  # body half width (1400 over the body; mirrors extra)
Z_BOT, BELT = 0.14, 1.02                   # body underside (140 ground clearance) / belt line = rear deck height
XF, XR = 1.52, -1.46                       # body front face / rear face
NOSE_Z, NOSE_X = 0.90, 1.40                # the front face turns back over the nose to the windscreen
FLOOR = 0.62                               # cab floor
FX, RX = 0.72, -0.86                       # front (steering) / rear (driven) axle - wheelbase 1580
TRK2 = 0.56                                # wheel centres y +-0.56 (track 1120)
FTD, FTW, FRB = 0.52, 0.16, 0.114          # front tyre 6.00-9 class: 520 dia, 160 wide, 9 in rim
RTD, RTW, RRB = 0.62, 0.20, 0.152          # rear tyre 7.00-12 class: 620 dia, 200 wide, 12 in rim
FZ, RZ = FTD / 2, RTD / 2
F_ARCH_R, R_ARCH_R = 0.34, 0.37            # arch radii (the front steers 32 deg inside its arch - checked)
F_WELL_Y, R_WELL_Y = 0.28, 0.40            # the wheel wells' inner walls
KP_Y = 0.42                                # front kingpins (steering axes) y
STEER = 32
CAB_X0, CAB_Y, CAB_Z0, ROOF = -0.56, 0.69, 1.00, 1.96
def wsx(z): return 1.38 - 0.125 * (z - 1.00)          # the windscreen's outer line (7 deg rake)
WS_DEG = math.degrees(math.atan(0.125))
def xi(z): return wsx(z) - 0.0403                      # its inner line (40 mm wall)
DOOR_X0, DOOR_X1, DOOR_Z0, DOOR_Z1 = -0.34, 0.335, 0.63, 1.87
HX, HY = DOOR_X1 + 0.012, 0.712                        # the door's hinge axis (vertical)
WIN_Z0, WIN_Z1 = 1.12, 1.80                            # side glass

# ---------------------------------------------------------------- materials (plain; CC0 photos swapped in below)
M_PAINT = pmat('BT_BodyPaint', (0.85, 0.62, 0.05), 0.2, 0.45)       # GSE yellow (the tug's paint)
M_CAB = pmat('BT_CabPaint', (0.92, 0.92, 0.90), 0.1, 0.35)          # white cab (the GPU's white)
M_STEEL = pmat('BT_Steel', (0.35, 0.36, 0.38), 1.0, 0.45)
M_DARK = pmat('BT_DarkSteel', (0.08, 0.08, 0.09), 0.6, 0.55)
M_RUB = pmat('BT_Tyre', (0.03, 0.03, 0.03), 0.0, 0.9)
M_BUMP = pmat('BT_BumperRubber', (0.03, 0.03, 0.03), 0.0, 0.8)
M_FLOOR = pmat('BT_FloorMat', (0.06, 0.06, 0.06), 0.0, 0.9)
M_PLAST = pmat('BT_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_VINYL = pmat('BT_SeatVinyl', (0.05, 0.05, 0.06), 0.0, 0.6)
M_TREAD = pmat('BT_TreadPlate', (0.5, 0.5, 0.52), 1.0, 0.4)
M_CHEV = pmat('BT_Chevrons', (0.9, 0.7, 0.05), 0.2, 0.5)
M_REDP = pmat('BT_RedPaint', (0.7, 0.04, 0.03), 0.0, 0.4)
M_YELP = pmat('BT_YellowPaint', (0.95, 0.75, 0.05), 0.0, 0.4)
M_BATT = pmat('BT_BatteryCase', (0.10, 0.11, 0.12), 0.0, 0.5)
M_GLASS = pmat('BT_Glass', (0.2, 0.3, 0.35), 0.0, 0.05)
M_MIRROR = pmat('BT_MirrorGlass', (0.8, 0.85, 0.9), 1.0, 0.02)
M_AMBER = pmat('BT_AmberLens', (1.0, 0.55, 0.05), 0.0, 0.2, 2.0)
M_RED = pmat('BT_RedLens', (0.9, 0.05, 0.03), 0.0, 0.2, 1.0)
M_WHITE = pmat('BT_WhiteLens', (0.95, 0.95, 0.9), 0.0, 0.1, 1.5)
M_SCREEN = pmat('BT_Screen', (0.05, 0.25, 0.35), 0.0, 0.1, 0.8)
M_GREEN = pmat('BT_GreenLens', (0.05, 0.8, 0.15), 0.0, 0.2, 1.0)

def two_tone(o, z, m_low):
    """Paints a part two colours split at height z (the door: white cab colour above the belt, body yellow below)."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z), plane_no=(0, 0, 1))
    bm.to_mesh(o.data); bm.free()
    o.data.materials.append(m_low); i = len(o.data.materials) - 1     # (a boolean cut may already have added a slot)
    for p in o.data.polygons:
        if p.center.z < z: p.material_index = i
    return o
def axis_part(o, c, ax):
    """Turns a part made round +z (at the origin) onto an axis ax through c."""
    rot = Vector(ax).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    o.data.transform(Matrix.Translation(Vector(c)) @ rot); o.data.update(); return o

# ================================================================ THE BODY (pressed steel tub, flat panels, small radii)
body = prism('Body', [(XR, Z_BOT), (XF - 0.06, Z_BOT), (XF, Z_BOT + 0.08), (XF, NOSE_Z), (NOSE_X, BELT), (XR, BELT)], -BW, BW, M_PAINT, bevel=0.02, segs=2)
# wheel arches: round over the wheel, open down to the ground, deep enough inwards for the front wheels to steer
for ax, az, r, wy in ((FX, FZ, F_ARCH_R, F_WELL_Y), (RX, RZ, R_ARCH_R, R_WELL_Y)):
    for s in (-1, 1):
        yc, d = s * (wy + 0.85) / 2, 0.85 - wy
        arch = [(ax - r, Z_BOT - 0.10)] + [(ax - r * math.cos(math.pi * j / 48), az + r * math.sin(math.pi * j / 48)) for j in range(49)] + [(ax + r, Z_BOT - 0.10)]
        cut(body, prism('ArchCut', arch, s * wy if s > 0 else -0.85, 0.85 if s > 0 else s * wy, M_DARK))   # (one closed outline: round top, open to the ground)
# the cab's lower half: hollow down to the floor (walls 60 mm)
cut(body, box('CabLowCut', ((CAB_X0 + 0.04 + 1.34) / 2, 0, (FLOOR + BELT + 0.10) / 2), (1.34 - CAB_X0 - 0.04, 1.28, BELT + 0.10 - FLOOR), M_DARK))
# the door opening (left) and the kick step pocket below it
cut(body, box('DoorCutLow', ((DOOR_X0 + DOOR_X1) / 2, 0.70, (DOOR_Z0 + BELT + 0.10) / 2), (DOOR_X1 - DOOR_X0, 0.30, BELT + 0.10 - DOOR_Z0), M_DARK))
cut(body, box('StepPocketCut', (-0.01, 0.705, 0.43), (0.48, 0.29, 0.26), M_DARK))
# the battery compartment under the rear deck
BAT_X0, BAT_X1, BAT_Y, BAT_Z0 = -1.34, -0.66, 0.56, 0.60
cut(body, box('BatteryPocketCut', ((BAT_X0 + BAT_X1) / 2, 0, (BAT_Z0 + 1.10) / 2), (BAT_X1 - BAT_X0, 2 * BAT_Y, 1.10 - BAT_Z0), M_DARK))
# the front grille recess (motor controller air) in the middle of the front face
cut(body, box('GrilleCut', (XF, 0, 0.69), (0.08, 0.28, 0.20), M_DARK))

def seal(o):
    """Closes the hair-thin slits an exact boolean can leave where it cuts through a small bevel strip."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.0002)
    bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(o.data); bm.free(); o.data.update(); return o
seal(body)

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
for s, sd in ((1, 'L'), (-1, 'R')):
    # front: kingpin carrier on an arm out of the well's inner wall (fixed), the spindle turns with the wheel (steering)
    box(f'FrontAxle_Arm_{sd}', (FX, s * (F_WELL_Y - 0.02 + KP_Y) / 2, FZ), (0.08, KP_Y - F_WELL_Y + 0.02, 0.07), M_DARK, bevel=0.006)
    cyl(f'Kingpin_{sd}', (FX, s * KP_Y, FZ), 0.032, 0.20, 'z', M_DARK, 24, bevel=0.004)
    cyl(f'Spindle_F{sd}', (FX, s * (KP_Y - 0.015 + TRK2 + 0.035 * FTW / 0.165) / 2, FZ), 0.028, TRK2 + 0.035 * FTW / 0.165 - KP_Y + 0.015, 'y', M_STEEL, 20)
    cyl(f'SteerArm_F{sd}', (FX - 0.06, s * KP_Y, FZ + 0.06), 0.014, 0.12, 'x', M_DARK, 12)          # (turns with the spindle)
    wheel('F' + sd, FX, s * TRK2, s, FZ, FRB, FTW, FZ)
    ik(f'Steer_F{sd}', (FX, s * KP_Y, FZ), f'Kingpin_{sd}')
    # rear: the drive axle's flange on the well wall and the stub through the hub
    cyl(f'RearAxle_Flange_{sd}', (RX, s * (R_WELL_Y + 0.006), RZ), 0.085, 0.022, 'y', M_DARK, 32, bevel=0.004)
    cyl(f'RearAxle_Stub_{sd}', (RX, s * (R_WELL_Y - 0.02 + TRK2 + 0.035 * RTW / 0.165) / 2, RZ), 0.042, TRK2 + 0.035 * RTW / 0.165 - R_WELL_Y + 0.02, 'y', M_STEEL, 24)
    wheel('R' + sd, RX, s * TRK2, s, RZ, RRB, RTW, RZ)

# ---------------------------------------------------------------- the front face: bumper band, grille, lamps, front hitch
bump = box('BumperFront', (XF + 0.0425, 0, 0.35), (0.095, 1.24, 0.18), M_BUMP, bevel=0.02, segs=3)
cut(bump, box('BumpSlot', (XF + 0.05, 0, 0.35), (0.2, 0.24, 0.3), M_DARK))
box('GrilleMesh', (XF - 0.04, 0, 0.69), (0.006, 0.29, 0.21), M_DARK)
gf = box('GrilleFrame', (XF + 0.004, 0, 0.69), (0.016, 0.31, 0.23), M_PLAST, bevel=0.004)
cut(gf, box('GfCut', (XF + 0.004, 0, 0.69), (0.05, 0.27, 0.19), M_DARK))
for k in range(5):
    z = 0.614 + k * 0.038
    turn(box(f'GrilleSlat_{k}', (XF - 0.012, 0, z), (0.030, 0.272, 0.010), M_PLAST), (XF - 0.012, 0, z), 'y', 35)
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'LowBeam_{nm}', (XF, s * 0.50, 0.74), 'x+', 0.14, 0.10, M_WHITE, M_DARK, M_PLAST)
    lamp(f'HighBeam_{nm}', (XF, s * 0.30, 0.74), 'x+', 0.09, 0.10, M_WHITE, M_DARK, M_PLAST)
    lamp(f'IndicatorF_{nm}', (XF, s * 0.50, 0.58), 'x+', 0.14, 0.06, M_AMBER, M_DARK, M_PLAST)
    lamp(f'SideMarker_F{nm}', (1.36, s * BW, 0.70), 'y' + ('+' if s > 0 else '-'), 0.07, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.03)
    lamp(f'SideMarker_R{nm}', (-1.34, s * BW, 0.75), 'y' + ('+' if s > 0 else '-'), 0.07, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.03)
# front pin hitch (for positioning carts / the GPU): a block bolted to the front face in the bumper's slot, two jaw plates,
# the hand pin through them (HitchPinFront*: pulled straight up)
box('HitchFront_Block', (XF + 0.02, 0, 0.40), (0.05, 0.20, 0.20), M_DARK, bevel=0.006)
for k, (z0, z1) in enumerate(((0.35, 0.37), (0.43, 0.45))):
    box(f'HitchFront_Jaw_{k}', ((XF + 0.035 + 1.74) / 2, 0, (z0 + z1) / 2), (1.74 - XF - 0.035, 0.17, z1 - z0), M_DARK, bevel=0.008, segs=2)
HFX = 1.68
cyl('HitchPinFront', (HFX, 0, 0.41), 0.016, 0.18, 'z', M_STEEL, 20)
cyl('HitchPinFront_Collar', (HFX, 0, 0.459), 0.026, 0.020, 'z', M_STEEL, 24, bevel=0.003)
torus('HitchPinFront_Ring', (HFX, 0, 0.497), 0.030, 0.006, 'y', M_STEEL, 32, 10)
# the mirrors: a plate on the flat body side ahead of the cab, a visible arm out and up, the mirror behind the windscreen line
for s, nm in ((1, 'L'), (-1, 'R')):
    box(f'MirrorPlate_{nm}', (1.15, s * (BW + 0.003), 0.80), (0.10, 0.012, 0.10), M_DARK, bevel=0.003)
    tube(f'MirrorArm_{nm}', [(1.15, s * (BW + 0.005), 0.80), (1.15, s * 0.84, 0.80), (1.15, s * 0.92, 0.90), (1.15, s * 0.93, 1.20)], 0.013, M_DARK, bez=False)
    box(f'Mirror_{nm}', (1.15, s * 0.93, 1.30), (0.05, 0.17, 0.24), M_PLAST, bevel=0.02, segs=3)
    box(f'MirrorGlass_{nm}', (1.15 - 0.025, s * 0.93, 1.30), (0.006, 0.145, 0.21), M_MIRROR)

# ---------------------------------------------------------------- the left side: kick step, hinges; right side: controller vent
box('StepTread', (-0.01, 0.633, 0.300), (0.46, 0.124, 0.030), M_TREAD, bevel=0.003)
box('StepNosing', (-0.01, 0.684, 0.318), (0.46, 0.020, 0.010), M_YELP)
fr = box('VentFrame', (-0.10, -(BW + 0.006), 0.78), (0.36, 0.016, 0.20), M_PLAST, bevel=0.004)
cut(fr, box('VfCut', (-0.10, -(BW + 0.006), 0.78), (0.32, 0.05, 0.16), M_DARK))
box('VentBack', (-0.10, -(BW - 0.001), 0.78), (0.33, 0.008, 0.17), M_DARK)
for k in range(5):
    z = 0.716 + k * 0.032
    turn(box(f'VentBlade_{k}', (-0.10, -(BW + 0.006), z), (0.324, 0.004, 0.030), M_PLAST), (-0.10, -(BW + 0.006), z), 'x', -40)

# ================================================================ THE CAB (white, pressed: rounded corners, flat panels)
shell = prism('CabShell', [(CAB_X0, CAB_Z0), (wsx(CAB_Z0), CAB_Z0), (wsx(ROOF), ROOF), (CAB_X0, ROOF)], -CAB_Y, CAB_Y, M_CAB, bevel=0.04, segs=4)
cut(shell, prism('ShellCut', [(CAB_X0 + 0.04, 0.97), (xi(0.97), 0.97), (xi(ROOF - 0.04), ROOF - 0.04), (CAB_X0 + 0.04, ROOF - 0.04)], -(CAB_Y - 0.04), CAB_Y - 0.04, M_DARK))
cut(shell, box('DoorCutUp', ((DOOR_X0 + DOOR_X1) / 2, 0.70, (DOOR_Z0 + DOOR_Z1) / 2), (DOOR_X1 - DOOR_X0, 0.30, DOOR_Z1 - DOOR_Z0), M_DARK))
def window(name, c, s, target=None, glass=None, deg=0.0):
    cu = box(name + 'Cut', c, s, M_DARK, bevel=0.03, segs=3)
    if deg: turn(cu, c, 'y', deg)
    cut(target or shell, cu)
    g = box(glass or name + 'Glass', c, tuple(0.010 if v < 0.2 else v + 0.02 for v in s), M_GLASS, bevel=0.004, segs=2)   # 10 mm pane, 10 mm into the frame all round
    if deg: turn(g, c, 'y', deg)
    return g
wz, wh = (WIN_Z0 + WIN_Z1) / 2, WIN_Z1 - WIN_Z0
window('WinFront', (wsx(1.49) - 0.02, 0, 1.49), (0.12, 1.12, 0.72), deg=-WS_DEG)
window('WinRear', (CAB_X0 + 0.02, 0, 1.51), (0.12, 0.92, 0.54))
for s, nm in ((1, 'L'), (-1, 'R')):
    window(f'Win{nm}Front', (0.83, s * (CAB_Y - 0.02), wz), (0.64, 0.12, wh))
window('WinRRear', (-0.01, -(CAB_Y - 0.02), wz), (0.58, 0.12, wh))
# the roof: a moulded cap overhanging 40 mm all round (sun/rain shade), the amber beacon on its base
box('RoofCap', ((CAB_X0 - 0.04 + 1.30) / 2, 0, ROOF + 0.015), (1.30 - CAB_X0 + 0.04, 2 * CAB_Y + 0.06, 0.05), M_CAB, bevel=0.02, segs=3)
RT = ROOF + 0.04
BX = -0.35
box('BeaconBase', (BX, 0, RT + 0.008), (0.18, 0.18, 0.024), M_DARK, bevel=0.005)
cyl('BeaconLens', (BX, 0, RT + 0.083), 0.070, 0.13, 'z', M_AMBER, 32, bevel=0.02)
cyl('BeaconCap', (BX, 0, RT + 0.155), 0.056, 0.02, 'z', M_DARK, 32)
# the rear window's steel mesh guard (bags are thrown onto the deck behind the cab): frame bolted round it, bars
GX = CAB_X0 - 0.013
for nm, c, sz in (('T', (GX, 0, 1.83), (0.03, 1.07, 0.03)), ('B', (GX, 0, 1.19), (0.03, 1.07, 0.03)),
                  ('L', (GX, 0.52, 1.51), (0.03, 0.03, 0.67)), ('R', (GX, -0.52, 1.51), (0.03, 0.03, 0.67))):
    box(f'RearGuard_Frame{nm}', c, sz, M_DARK, bevel=0.004)
for k in range(9): cyl(f'RearGuard_BarV_{k}', (GX - 0.003, -0.40 + k * 0.10, 1.51), 0.005, 0.64, 'z', M_DARK, 8)
for k in range(5): cyl(f'RearGuard_BarH_{k}', (GX - 0.003, 0, 1.29 + k * 0.11), 0.005, 1.04, 'y', M_DARK, 8)

# ---------------------------------------------------------------- the driver door (DoorDriver*: hinged on its FRONT edge, two
# hinges, opens outward; the glass moves with it). Leaf 40 mm, white above the belt, yellow below like the body.
leaf = box('DoorDriver', ((DOOR_X0 + DOOR_X1) / 2 - 0.0025, 0.675, (DOOR_Z0 + DOOR_Z1) / 2), (DOOR_X1 - DOOR_X0 - 0.011, 0.04, DOOR_Z1 - DOOR_Z0 - 0.010), M_CAB, bevel=0.008, segs=2)
cut(leaf, box('DoorWinCut', (-0.01, 0.675, 1.46), (0.54, 0.12, 0.66), M_DARK, bevel=0.03, segs=3))
two_tone(leaf, BELT, M_PAINT)
box('DoorDriver_Glass', (-0.01, 0.675, 1.46), (0.56, 0.010, 0.68), M_GLASS, bevel=0.004, segs=2)
box('DoorDriver_HandleOut', (-0.20, 0.715, 1.00), (0.14, 0.025, 0.035), M_STEEL, bevel=0.008)
for k, x in enumerate((-0.25, -0.15)): box(f'DoorDriver_HandleMount_{k}', (x, 0.704, 1.00), (0.02, 0.022, 0.03), M_STEEL)
cyl('DoorDriver_Lock', (-0.08, 0.697, 1.00), 0.011, 0.012, 'y', M_STEEL, 16)
box('DoorDriver_Trim', (-0.01, 0.652, 0.88), (0.58, 0.012, 0.40), M_PLAST, bevel=0.004)
box('DoorDriver_HandleIn', (-0.20, 0.639, 1.02), (0.10, 0.020, 0.03), M_PLAST, bevel=0.006)
box('DoorDriver_Pull', (0.02, 0.632, 0.96), (0.24, 0.034, 0.04), M_PLAST, bevel=0.012)
for k, z in enumerate((0.86, 1.62)):
    wall = BW if z < BELT else CAB_Y
    box(f'HingeDoor_{k}_Base', (HX + 0.03, wall + 0.006, z), (0.05, 0.014, 0.09), M_STEEL, bevel=0.002)
    cyl(f'HingeDoor_{k}', (HX, HY, z), 0.016, 0.10, 'z', M_STEEL, 16)
    box(f'DoorDriver_HingeLeaf_{k}', (HX - 0.038, 0.698, z), (0.048, 0.008, 0.09), M_STEEL, bevel=0.002)
DOOR_PIVOT = (HX, HY, (DOOR_Z0 + DOOR_Z1) / 2)
# grab handles: outside on the flat panel behind the door, inside on the front door post
for k, z in enumerate((1.20, 1.62)): cyl(f'GrabOutFoot_{k}', (-0.43, CAB_Y + 0.002, z), 0.025, 0.012, 'y', M_STEEL, 24, bevel=0.002)
tube('GrabHandleOut', [(-0.43, CAB_Y + 0.004, 1.20), (-0.43, CAB_Y + 0.06, 1.24), (-0.43, CAB_Y + 0.06, 1.58), (-0.43, CAB_Y + 0.004, 1.62)], 0.016, M_STEEL, bez=False)
for k, z in enumerate((1.30, 1.70)): cyl(f'GrabInFoot_{k}', (0.415, CAB_Y - 0.042, z), 0.022, 0.012, 'y', M_PLAST, 24, bevel=0.002)
tube('GrabHandleIn', [(0.415, CAB_Y - 0.044, 1.30), (0.415, CAB_Y - 0.09, 1.34), (0.415, CAB_Y - 0.09, 1.66), (0.415, CAB_Y - 0.044, 1.70)], 0.014, M_PLAST, bez=False)

# ================================================================ THE INTERIOR (real inside sizes: 1730 long floor, 1280 wide,
# 1300 floor to roof; every part on the floor, a wall or the roof)
box('FloorMat', ((CAB_X0 + 0.05 + 0.98) / 2, 0, FLOOR + 0.0035), (0.98 - CAB_X0 - 0.05, 1.24, 0.012), M_FLOOR)
for k in range(8): box(f'MatRib_{k}', ((CAB_X0 + 0.05 + 0.98) / 2, -0.49 + k * 0.14, FLOOR + 0.010), (0.98 - CAB_X0 - 0.11, 0.02, 0.006), M_FLOOR)
# the front dash shelf under the windscreen (switches, vents, radio) and the steering pedestal on the floor in front of the seat
prism('Dash', [(1.00, FLOOR - 0.005), (1.36, FLOOR - 0.005), (1.36, 0.90), (1.325, 1.13), (1.06, 1.13), (1.00, 1.06)], -0.645, 0.645, M_PLAST, bevel=0.01, segs=2)
for k in range(3): box(f'DefrostVent_{k}', (1.20, -0.35 + k * 0.35, 1.131), (0.03, 0.20, 0.006), M_DARK)
for k in range(5): box(f'DashSwitch_{k}', (0.995, 0.25 + k * 0.06, 1.00), (0.02, 0.025, 0.035), M_DARK, bevel=0.003)
box('Radio', (0.985, -0.40, 0.95), (0.06, 0.18, 0.06), M_DARK, bevel=0.006)
box('RadioHandset', (0.985, -0.56, 0.92), (0.04, 0.05, 0.10), M_PLAST, bevel=0.008)
prism('Pedestal', [(0.60, FLOOR - 0.005), (0.92, FLOOR - 0.005), (0.92, 0.88), (0.82, 1.06), (0.66, 1.07), (0.60, 1.00)], -0.13, 0.13, M_PLAST, bevel=0.012, segs=2)
box('Binnacle', (0.80, 0, 1.18), (0.10, 0.30, 0.24), M_PLAST, bevel=0.015, segs=2)
box('BinnacleDisplay', (0.748, 0, 1.20), (0.008, 0.20, 0.12), M_SCREEN)
for k, (y, m) in enumerate(((-0.12, M_GREEN), (0.12, M_AMBER))): cyl(f'BinnacleTell_{k}', (0.749, y, 1.10), 0.008, 0.008, 'x', m, 12)
cyl('KeySwitch', (0.80, -0.133, 0.92), 0.020, 0.016, 'y', M_STEEL, 24, bevel=0.003)
box('Key', (0.80, -0.150, 0.92), (0.012, 0.020, 0.030), M_STEEL)
cyl('EStopIn_Housing', (0.80, 0.135, 0.92), 0.028, 0.018, 'y', M_YELP, 24, bevel=0.003)
cyl('EStopIn_Button', (0.80, 0.150, 0.92), 0.024, 0.016, 'y', M_REDP, 24, bevel=0.004)
# the steering column out of the pedestal, raked 45 deg; the wheel (SteeringWheel*: turns about the column axis)
WC = Vector((0.46, 0.0, 1.25)); AX = Vector((-1.0, 0.0, 1.0)).normalized()      # wheel centre; the column axis (towards the driver)
COL_BOT = Vector((0.74, 0.0, 0.97))
WU, WV = Vector((0, 1, 0)), Vector((0.7071, 0, 0.7071))                          # the wheel plane: across (left) / up the plane
tube('SteeringColumn', [tuple(COL_BOT), tuple(WC - AX * 0.012)], 0.032, M_PLAST, bez=False)
axis_part(cyl('ColumnShroud', (0, 0, 0), 0.050, 0.16, 'z', M_PLAST, 24, bevel=0.01), WC - AX * 0.13, AX)
WR = 0.18
tube('SteeringWheel', [tuple(WC + WR * (math.cos(a) * WU + math.sin(a) * WV)) for a in [k * 2 * math.pi / 48 for k in range(49)]], 0.016, M_PLAST, bez=False)
axis_part(cyl('SteeringWheel_Hub', (0, 0, 0), 0.050, 0.05, 'z', M_PLAST, 24, bevel=0.01), WC, AX)
axis_part(cyl('SteeringWheel_Horn', (0, 0, 0), 0.036, 0.012, 'z', M_DARK, 24, bevel=0.003), WC + AX * 0.026, AX)
for k, a in enumerate((0.0, math.pi, -math.pi / 2)):
    d = math.cos(a) * WU + math.sin(a) * WV
    tube(f'SteeringWheel_Spoke_{k}', [tuple(WC + d * 0.025), tuple(WC + d * WR)], 0.012, M_PLAST, bez=False)   # (from inside the hub to the rim's centre line: joined at both ends)
# the direction (forward / neutral / reverse) stalk on the column's left
CP = COL_BOT + (WC - COL_BOT) * 0.62
tube('DirectionLever', [(CP.x, 0.02, CP.z), (CP.x - 0.02, 0.15, CP.z + 0.02)], 0.007, M_DARK, bez=False)
cyl('DirectionLever_Knob', (CP.x - 0.021, 0.165, CP.z + 0.021), 0.013, 0.04, 'y', M_PLAST, 16, bevel=0.004)
# pedals either side of the pedestal: brake (left), accelerator (right) - hinged at the floor
PEDALS = {}
for nm, y in (('Brake', 0.21), ('Accel', -0.21)):
    box(f'PedalPivot_{nm}', (0.76, y, FLOOR + 0.028), (0.06, 0.07, 0.04), M_DARK)
    tube(f'PedalArm_{nm}', [(0.76, y, FLOOR + 0.04), (0.71, y, FLOOR + 0.11)], 0.009, M_DARK, bez=False)
    pc = Vector((0.70, y, FLOOR + 0.14))
    turn(box(f'Pedal_{nm}', tuple(pc), (0.03, 0.10 if nm == 'Brake' else 0.07, 0.16), M_RUB, bevel=0.006), pc, 'y', -30)
    PEDALS[nm] = ((0.76, y, FLOOR + 0.03), 'y')
# the driver's seat: an industrial suspension seat - rails, bellows, pan, contoured cushion with bolsters, high back with
# lumbar and side wings, headrest, right armrest, lap belt
SX, SY, SZ = -0.09, 0.0, FLOOR
ss = K.ss
box('SeatRails', (SX, SY, SZ + 0.026), (0.42, 0.34, 0.04), M_DARK, bevel=0.005)
for k in range(5):
    z = SZ + 0.04 + 0.11 * (k + 0.5) / 5
    box(f'SeatBellows_{k}', (SX, SY, z), (0.40 * (1.0 if k % 2 else 0.92), 0.42 * (1.0 if k % 2 else 0.92), 0.11 / 5 * 1.04), M_RUB, bevel=0.010, segs=3)
box('SeatPan', (SX, SY, SZ + 0.165), (0.46, 0.48, 0.04), M_DARK, bevel=0.01)
def cushion_def(co, u, v, w):
    top = ss(-1.0, 1.0, w)
    co.z += top * (0.012 * (1 - v * v) + 0.03 * ss(0.55, 0.95, abs(v)))
    fr = ss(0.55, 1.0, u)
    co.z -= fr * fr * 0.04 * top; co.x -= fr * 0.012 * (1 - top)
    return co
box('SeatCushion', (SX + 0.01, SY, SZ + 0.235), (0.48, 0.50, 0.11), M_VINYL, bevel=0.035, segs=4, cuts=8, sub=1, deform=cushion_def, smooth=60)
REC = math.radians(12)
BACK_C = Vector((SX - 0.26, SY, SZ + 0.60))
def back_def(co, u, v, w):
    up = (w + 1) / 2
    co.x += 0.035 * math.exp(-((up - 0.30) / 0.16) ** 2) * ss(-1.0, 0.2, u)
    co.x += 0.05 * ss(0.5, 0.95, abs(v)) * ss(-1, 0, u)
    return co
back = box('SeatBack', tuple(BACK_C), (0.10, 0.50, 0.66), M_VINYL, bevel=0.04, segs=4, cuts=8, sub=1, deform=back_def, smooth=60)
back.data.transform(Matrix.Translation(BACK_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-BACK_C))
HEAD_C = BACK_C + Vector((-math.sin(REC) * 0.42, 0, math.cos(REC) * 0.42))
head = box('SeatHeadrest', tuple(HEAD_C), (0.09, 0.30, 0.17), M_VINYL, bevel=0.035, segs=4, cuts=4, sub=1, smooth=60)
head.data.transform(Matrix.Translation(HEAD_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-HEAD_C))
for s in (-1, 1): cyl(f'HeadrestPost_{s}', tuple(HEAD_C + Vector((0.02, s * 0.08, -0.10))), 0.008, 0.10, 'z', M_STEEL, 12)
box('SeatBackShell', tuple(BACK_C + Vector((-0.065, 0, 0))), (0.04, 0.46, 0.62), M_PLAST, bevel=0.012).data.transform(
    Matrix.Translation(BACK_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-BACK_C))
box('SeatRecliner', (SX - 0.21, SY - 0.252, SZ + 0.245), (0.08, 0.03, 0.08), M_DARK, bevel=0.01)
cyl('ArmHinge_R', (SX - 0.22, SY - 0.265, SZ + 0.42), 0.025, 0.07, 'y', M_DARK, 16)
box('Armrest_R', (SX + 0.0, SY - 0.28, SZ + 0.43), (0.42, 0.07, 0.06), M_VINYL, bevel=0.025, segs=3)
for s in (-1, 1): box(f'BeltAnchor_{s}', (SX - 0.12, SY + s * 0.243, SZ + 0.205), (0.03, 0.01, 0.06), M_STEEL)
box('BeltBuckle', (SX - 0.10, SY - 0.245, SZ + 0.26), (0.06, 0.025, 0.035), M_STEEL, bevel=0.006)
tube('BeltStrap', [(SX - 0.12, SY + 0.243, SZ + 0.22), (SX + 0.02, SY + 0.10, SZ + 0.32), (SX + 0.02, SY - 0.10, SZ + 0.32), (SX - 0.10, SY - 0.245, SZ + 0.26)], 0.004, M_PLAST)
# hand parking brake beside the seat (right), extinguisher on the back wall, roof liner, cab lamp, sun visor
box('ParkBrakeBase', (SX + 0.12, -0.40, FLOOR + 0.03), (0.12, 0.10, 0.05), M_DARK, bevel=0.005)
PB0, PB1 = Vector((SX + 0.12, -0.40, FLOOR + 0.05)), Vector((SX + 0.22, -0.40, FLOOR + 0.40))
tube('ParkBrake', [tuple(PB0), tuple(PB1)], 0.014, M_DARK, bez=False)
PBD = (PB1 - PB0).normalized()
axis_part(cyl('ParkBrake_Grip', (0, 0, 0), 0.020, 0.10, 'z', M_PLAST, 16, bevel=0.006), PB1 + PBD * 0.03, PBD)   # (on the shaft's end, along the shaft)
box('ExtinguisherBracket', (CAB_X0 + 0.055, -0.50, 0.85), (0.08, 0.10, 0.12), M_DARK)
cyl('Extinguisher', (CAB_X0 + 0.13, -0.50, 0.92), 0.055, 0.40, 'z', M_REDP, 24, bevel=0.02)
cyl('ExtinguisherValve', (CAB_X0 + 0.13, -0.50, 1.135), 0.018, 0.04, 'z', M_DARK, 12)
box('RoofLiner', ((CAB_X0 + 0.04 + xi(ROOF - 0.04)) / 2 - 0.013, 0, ROOF - 0.045), (xi(ROOF - 0.04) - CAB_X0 - 0.066, 1.27, 0.020), M_PLAST)
box('CabLamp', (0.20, 0, ROOF - 0.058), (0.20, 0.08, 0.016), M_WHITE, bevel=0.005)
box('SunVisor', (1.12, 0, 1.86), (0.18, 0.40, 0.012), M_PLAST, bevel=0.004)
for s in (-1, 1): box(f'SunVisorClip_{s}', (1.17, s * 0.18, 1.887), (0.02, 0.02, 0.054), M_PLAST)

# ================================================================ THE REAR: battery under the chequer-plate deck lid, rear face
# the 80 V traction battery: steel tray, cover, cell posts and links, the main cables to the charge socket
box('Battery_Tray', (-1.00, 0, 0.763), (0.62, 1.06, 0.33), M_DARK, bevel=0.01)
box('Battery_Top', (-1.00, 0, 0.932), (0.58, 1.02, 0.012), M_BATT)
for r in range(2):
    for c in range(8):
        cyl(f'Battery_Post_{r}{c}', (-1.20 + r * 0.40, -0.42 + c * 0.12, 0.945), 0.012, 0.020, 'z', M_STEEL, 12)
    for c in range(7):
        box(f'Battery_Link_{r}{c}', (-1.20 + r * 0.40, -0.36 + c * 0.12, 0.953), (0.016, 0.10, 0.006), M_STEEL)
for nm, y, m in (('Pos', 0.42, M_REDP), ('Neg', -0.42, M_PLAST)):
    tube(f'Battery_Cable{nm}', [(-1.20, y, 0.952), (-1.28, y, 0.97), (-1.33, y * 0.6, 0.93), (-1.36, y * 0.35 - 0.14, 0.84)], 0.012, m)
LID_X0, LID_X1, LID_Y, LID_Z0, LID_Z1 = -1.385, -0.62, 0.605, BELT - 0.002, BELT + 0.014
box('BatteryLid', ((LID_X0 + LID_X1) / 2, 0, (LID_Z0 + LID_Z1) / 2), (LID_X1 - LID_X0, 2 * LID_Y, LID_Z1 - LID_Z0), M_TREAD, bevel=0.004)
tube('BatteryLid_Handle', [(-0.70, 0.18, LID_Z1 - 0.004), (-0.68, 0.18, LID_Z1 + 0.04), (-0.68, -0.18, LID_Z1 + 0.04), (-0.70, -0.18, LID_Z1 - 0.004)], 0.012, M_STEEL, bez=False)
LHX, LHZ = -1.40, LID_Z1
for k, y in enumerate((0.40, -0.40)):
    box(f'HingeLid_{k}_Base', (-1.42, y, BELT + 0.002), (0.035, 0.09, 0.008), M_STEEL)
    cyl(f'HingeLid_{k}', (LHX, y, LHZ), 0.010, 0.09, 'y', M_STEEL, 16)
    box(f'BatteryLid_HingeLeaf_{k}', (-1.375, y, LID_Z1 + 0.002), (0.035, 0.09, 0.006), M_STEEL)
LID_PIVOT = (LHX, 0.0, LHZ)
# the rear face: multi-height pin hitch (eye slots centred 280 / 400 / 490), chevron panels, lamps, e-stop, charge socket
box('HitchRear_Plate', (XR - 0.0175, 0, 0.41), (0.045, 0.26, 0.38), M_DARK, bevel=0.006)
box('HitchRear_Web', (XR - 0.05, 0, 0.385), (0.04, 0.20, 0.31), M_DARK, bevel=0.004)
RJ = ((0.23, 0.25), (0.31, 0.37), (0.43, 0.46), (0.52, 0.54))
for k, (z0, z1) in enumerate(RJ):
    box(f'HitchRear_Jaw_{k}', ((XR - 0.035 + -1.68) / 2, 0, (z0 + z1) / 2), (1.68 + XR - 0.035, 0.20, z1 - z0), M_DARK, bevel=0.008, segs=2)
HRX = -1.62
cyl('HitchPinRear', (HRX, 0, 0.39), 0.018, 0.38, 'z', M_STEEL, 20)
cyl('HitchPinRear_Collar', (HRX, 0, 0.549), 0.028, 0.020, 'z', M_STEEL, 24, bevel=0.003)
torus('HitchPinRear_Ring', (HRX, 0, 0.590), 0.035, 0.007, 'y', M_STEEL, 32, 10)
for s, nm in ((1, 'L'), (-1, 'R')):
    box(f'ChevronRear_{nm}', (XR - 0.003, s * 0.41, 0.39), (0.008, 0.42, 0.26), M_CHEV)
    lamp(f'TailBrake_{nm}', (XR, s * 0.52, 0.86), 'x-', 0.12, 0.09, M_RED, M_DARK, M_PLAST)
    lamp(f'IndicatorR_{nm}', (XR, s * 0.52, 0.72), 'x-', 0.12, 0.08, M_AMBER, M_DARK, M_PLAST)
    lamp(f'Reverse_{nm}', (XR, s * 0.36, 0.86), 'x-', 0.10, 0.08, M_WHITE, M_DARK, M_PLAST)
    lamp(f'ReflectorRear_{nm}', (XR, s * 0.36, 0.72), 'x-', 0.10, 0.06, M_RED, M_DARK, M_PLAST, depth=0.024)
box('EStop_Housing', (XR - 0.016, 0.14, 0.82), (0.036, 0.08, 0.08), M_YELP, bevel=0.006)
cyl('EStop_Stem', (XR - 0.040, 0.14, 0.82), 0.016, 0.016, 'x', M_DARK, 20)
cyl('EStop_Button', (XR - 0.054, 0.14, 0.82), 0.030, 0.016, 'x', M_REDP, 32, bevel=0.005)
box('ChargeSocket', (XR - 0.023, -0.14, 0.82), (0.05, 0.12, 0.12), M_PLAST, bevel=0.008)
cyl('ChargeSocket_Bore', (XR - 0.049, -0.14, 0.82), 0.042, 0.006, 'x', M_DARK, 32)
cyl('ChargeSocket_CapHinge', (XR - 0.052, -0.14, 0.885), 0.006, 0.08, 'y', M_PLAST, 12)
box('ChargeSocket_Cap', (XR - 0.056, -0.14, 0.83), (0.010, 0.11, 0.10), M_YELP, bevel=0.004)
box('ChargeSocket_Status', (XR - 0.003, -0.14, 0.735), (0.008, 0.06, 0.02), M_GREEN)

for o in (shell, leaf): seal(o)

# ================================================================ IK + pivots for the game (from the parts' own positions)
ik('Seat_Driver', (SX + 0.02, SY, SZ + 0.30), 'SeatCushion'); ik('Back_Driver', (SX - 0.22, SY, SZ + 0.58), 'SeatBack')
ik('Hand_L', tuple(WC + WR * (math.cos(math.radians(30)) * WU + math.sin(math.radians(30)) * WV)), 'SteeringWheel')   # (WU = +y = the driver's left: 10 o'clock)
ik('Hand_R', tuple(WC + WR * (math.cos(math.radians(150)) * WU + math.sin(math.radians(150)) * WV)), 'SteeringWheel')  # (2 o'clock, y negative)
ik('SteeringWheel_Centre', tuple(WC), 'SteeringWheel_Hub')
ik('Foot_Brake', (0.70, 0.21, FLOOR + 0.15), 'Pedal_Brake'); ik('Foot_Accel', (0.70, -0.21, FLOOR + 0.15), 'Pedal_Accel')
ik('Hand_Direction', (CP.x - 0.021, 0.165, CP.z + 0.021), 'DirectionLever_Knob'); ik('Hand_Key', (0.80, -0.155, 0.92), 'Key')
ik('Hand_ParkBrake', tuple(PB1 + PBD * 0.04), 'ParkBrake_Grip')
ik('Step', (-0.01, 0.63, 0.315), 'StepTread')
ik('Grab_Outside', (-0.43, CAB_Y + 0.06, 1.41), 'GrabHandleOut'); ik('Grab_Inside', (0.415, CAB_Y - 0.09, 1.50), 'GrabHandleIn')
ik('Door_Outside', (-0.20, 0.735, 1.00), 'DoorDriver_HandleOut'); ik('Door_Inside', (-0.20, 0.625, 1.02), 'DoorDriver_HandleIn')
ik('Door_Hinge', DOOR_PIVOT)
ik('HitchPin_Rear', (HRX, 0, 0.625), 'HitchPinRear_Ring'); ik('HitchPin_Front', (HFX, 0, 0.527), 'HitchPinFront_Ring')
ik('Hitch_Rear', (HRX, 0, 0.40), 'HitchPinRear'); ik('Hitch_Rear_Low', (HRX, 0, 0.28), 'HitchPinRear'); ik('Hitch_Rear_High', (HRX, 0, 0.49), 'HitchPinRear')
ik('Hitch_Front', (HFX, 0, 0.40), 'HitchPinFront')
ik('BatteryLid_Handle', (-0.68, 0.0, LID_Z1 + 0.04), 'BatteryLid_Handle'); ik('BatteryLid_Hinge', LID_PIVOT)
ik('Charge_Socket', (XR - 0.06, -0.14, 0.82), 'ChargeSocket'); ik('EStop', (XR - 0.065, 0.14, 0.82), 'EStop_Button')
ik('Beacon', (BX, 0, RT + 0.083))
ik('Stand_Door', (-0.05, 1.20, 0.0)); ik('Stand_HitchRear', (-2.05, 0.40, 0.0)); ik('Stand_HitchFront', (2.15, 0.40, 0.0)); ik('Stand_Battery', (-1.00, 1.15, 0.0))

# ================================================================ checks (owner 2026-11-21: nothing floats, mesh contact, no bad
# overhang, lamps clear, nothing crowds the arches, moving parts swing clear)
SKIP_OH = ('StepNosing', 'GrabHandleOut', 'GrabHandleIn', 'BatteryLid_Handle', 'Key', 'BinnacleDisplay', 'ChargeSocket_Cap', 'ChargeSocket_CapHinge', 'ChargeSocket_Status',
           'DirectionLever', 'DirectionLever_Knob', 'SteeringColumn', 'ColumnShroud', 'Extinguisher', 'ExtinguisherValve', 'RadioHandset')
OH_PREFIX = ('Hinge', 'DoorDriver_Hinge', 'DoorDriver_Handle', 'DoorDriver_Pull', 'DoorDriver_Lock', 'BatteryLid_Hinge', 'Nut_', 'Hub', 'Spindle', 'SteerArm', 'Kingpin', 'FrontAxle', 'RearAxle',
             'HitchPin', 'HitchRear_Jaw', 'HitchFront_Jaw', 'Mirror', 'GrilleSlat', 'VentBlade', 'RearGuard', 'Battery_', 'Steering', 'Pedal', 'Seat', 'Headrest', 'Arm', 'Belt',
             'ParkBrake', 'EStop', 'GrabOutFoot', 'GrabInFoot', 'SunVisorClip', 'DashSwitch', 'DefrostVent', 'BinnacleTell', 'MatRib', 'KeySwitch')
fl, cl, oh, lo = K.checks('BT', ground_ok=('Tyre',), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)
for n in oh: print('BT overhang', n, [m.name for m in ALL if m.name != n and K.touch(K.bbox(bpy.data.objects[n]), K.bbox(m)) and K.diag(K.bbox(m)) > 0.4])
crowd = K.arch_crowd([(FX, FZ, F_ARCH_R), (RX, RZ, R_ARCH_R)], BW, skip_prefix=('Body', 'Tyre', 'Rim', 'WheelDisc', 'Hub', 'Nut', 'Spindle', 'RearAxle', 'FrontAxle', 'Kingpin', 'SteerArm'))
for o in [o for o in ALL if o.name.startswith(('MirrorArm', 'Grab', 'Steering', 'DirectionLever', 'PedalArm', 'BeltStrap', 'ParkBrake', 'Battery_Cable', 'BatteryLid_Handle'))]:
    seal(o)                                                      # (the tubes' end caps: weld them on)
# every interior assembly joined part to part, like the pushback tug (owner 2026-11-21): a real mesh contact for each pair
JOIN = [('SteeringWheel_Hub', 'SteeringWheel_Spoke_%d' % k) for k in range(3)] + [('SteeringWheel', 'SteeringWheel_Spoke_%d' % k) for k in range(3)] + [
    ('SteeringWheel_Hub', 'SteeringWheel_Horn'), ('SteeringWheel_Hub', 'SteeringColumn'), ('SteeringColumn', 'Pedestal'), ('ColumnShroud', 'SteeringColumn'),
    ('DirectionLever', 'SteeringColumn'), ('DirectionLever_Knob', 'DirectionLever'), ('Binnacle', 'Pedestal'), ('BinnacleDisplay', 'Binnacle'),
    ('KeySwitch', 'Pedestal'), ('Key', 'KeySwitch'), ('EStopIn_Housing', 'Pedestal'), ('EStopIn_Button', 'EStopIn_Housing'), ('Pedestal', 'FloorMat'),
    ('PedalPivot_Brake', 'FloorMat'), ('PedalArm_Brake', 'PedalPivot_Brake'), ('Pedal_Brake', 'PedalArm_Brake'),
    ('PedalPivot_Accel', 'FloorMat'), ('PedalArm_Accel', 'PedalPivot_Accel'), ('Pedal_Accel', 'PedalArm_Accel'),
    ('SeatRails', 'FloorMat'), ('SeatBellows_0', 'SeatRails'), ('SeatBellows_4', 'SeatPan'), ('SeatCushion', 'SeatPan'), ('SeatBack', 'SeatCushion'), ('SeatBackShell', 'SeatBack'),
    ('SeatHeadrest', 'HeadrestPost_1'), ('HeadrestPost_1', 'SeatBack'), ('ArmHinge_R', 'SeatBack'), ('Armrest_R', 'ArmHinge_R'), ('SeatRecliner', 'SeatCushion'),
    ('BeltAnchor_1', 'SeatPan'), ('BeltAnchor_-1', 'SeatPan'), ('BeltStrap', 'BeltBuckle'), ('BeltBuckle', 'SeatCushion'), ('BeltStrap', 'BeltAnchor_1'),
    ('ParkBrakeBase', 'FloorMat'), ('ParkBrake', 'ParkBrakeBase'), ('ParkBrake_Grip', 'ParkBrake'),
    ('ExtinguisherBracket', 'Body'), ('Extinguisher', 'ExtinguisherBracket'), ('ExtinguisherValve', 'Extinguisher'),
    ('CabLamp', 'RoofLiner'), ('RoofLiner', 'CabShell'), ('SunVisorClip_1', 'SunVisor'), ('SunVisorClip_1', 'RoofLiner'),
    ('Radio', 'Dash'), ('RadioHandset', 'Dash'), ('DashSwitch_0', 'Dash'), ('DefrostVent_0', 'Dash'),
    ('DoorDriver_HandleIn', 'DoorDriver_Trim'), ('DoorDriver_Trim', 'DoorDriver'), ('DoorDriver_Pull', 'DoorDriver_Trim'),
    ('GrabHandleIn', 'GrabInFoot_0'), ('GrabInFoot_0', 'CabShell'), ('BatteryLid_Handle', 'BatteryLid'),
    ('Battery_CablePos', 'Battery_Post_07'), ('Battery_CableNeg', 'Battery_Post_00'), ('Battery_Tray', 'Body'), ('Battery_Top', 'Battery_Tray')]
def joined(a, b):
    ta, tb = K.bvh(bpy.data.objects[a]), K.bvh(bpy.data.objects[b])
    if ta.overlap(tb): return True
    oa = bpy.data.objects[a]
    return any((h := tb.find_nearest(oa.matrix_world @ v.co))[0] is not None and h[3] < 0.001 for v in oa.data.vertices)
not_joined = [(a, b) for a, b in JOIN if not joined(a, b)]
print('BT CHECK interior joints: %d of %d pairs not joined %s' % (len(not_joined), len(JOIN), not_joined))
open_edges = []
for o in ALL:
    bm = bmesh.new(); bm.from_mesh(o.data); n = sum(1 for e in bm.edges if not e.is_manifold); bm.free()
    if n: open_edges.append((o.name, n))
print('BT CHECK open (non-manifold) edges: %d parts %s' % (len(open_edges), open_edges[:20]))
print('BT CHECK arch clearance: %d parts crowd an arch %s' % (len(crowd), crowd))
WHEEL_F = lambda sd: tuple(p + 'F' + sd for p in ('Tyre_', 'Rim_', 'WheelDisc_', 'Hub_', 'HubCap_', 'Nut_', 'Spindle_', 'SteerArm_'))
steer_hits = set()
for sd, s in (('L', 1), ('R', -1)):
    for d in (-STEER, -STEER / 2, STEER / 2, STEER):
        steer_hits |= K.sweep(WHEEL_F(sd), (FX, s * KP_Y, FZ), 'z', [d], static_skip_prefix=('Kingpin_' + sd, 'FrontAxle_Arm_' + sd))
print('BT CHECK steering sweep +-%d deg: %d hits %s' % (STEER, len(steer_hits), sorted(steer_hits)[:12]))
door_hits = K.sweep('DoorDriver', DOOR_PIVOT, 'z', [-25, -50, -75, -95], static_skip_prefix=('HingeDoor',))
print('BT CHECK door opens 25/50/75/95: %d hits %s' % (len(door_hits), sorted(door_hits)[:12]))
lid_hits = K.sweep('BatteryLid', LID_PIVOT, 'y', [-20, -45, -70, -100], static_skip_prefix=('HingeLid',))
print('BT CHECK battery lid opens 20/45/70/100: %d hits %s' % (len(lid_hits), sorted(lid_hits)[:12]))
def lift_hits(prefix, dz, skip):
    mov = [o for o in ALL if o.name.startswith(prefix)]; T = Matrix.Translation((0, 0, dz)); hits = set()
    stat = {o.name: K.bvh(o) for o in ALL if not o.name.startswith(prefix) and not o.name.startswith(skip)}
    for o in mov:
        t = BVHTree.FromPolygons([T @ (o.matrix_world @ v.co) for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
        for n, tr in stat.items():
            if t.overlap(tr): hits.add((o.name, n))
    return hits
pin_hits = lift_hits('HitchPinRear', 0.40, ('HitchRear_Jaw',)) | lift_hits('HitchPinFront', 0.20, ('HitchFront_Jaw',))
print('BT CHECK hitch pins pulled out: %d hits %s' % (len(pin_hits), sorted(pin_hits)))
ik_off = []
for e in IKS:
    part = IKP.get(e.name)
    if not part: continue
    o = bpy.data.objects[part]; tr = K.bvh(o); hit = tr.find_nearest(e.location)
    if hit[0] is None or hit[3] > 0.06: ik_off.append((e.name, part, round(hit[3], 3) if hit[0] is not None else None))
print('BT CHECK IK points on their parts: %d off %s' % (len(ik_off), ik_off))
# the GPU's towing eye fits the middle jaw slot (eye ring 158 x 42 mm, slot 60 high)
gpu = json.load(open(os.path.join(ROOT, 'Art', 'GPU', 'gpu-parts.json')))
eye_z = gpu['ik']['IK_TowEye'][2]
slot = [(RJ[k][1], RJ[k + 1][0]) for k in range(3)]
fits = [s for s in slot if s[0] + 0.021 <= eye_z + 1e-6 and eye_z + 0.021 <= s[1] + 1e-6]
print('BT CHECK GPU towing eye at %.3f fits a rear slot: %s (slots %s)' % (eye_z, bool(fits), slot))

# ---------------------------------------------------------------- the moving parts, IK points, sizes -> json for the main session
def bb_all(objs):
    lo_ = Vector((1e9,) * 3); hi_ = Vector((-1e9,) * 3)
    for o in objs:
        a, b = K.bbox(o); lo_ = Vector(tuple(min(lo_[i], a[i]) for i in range(3))); hi_ = Vector(tuple(max(hi_[i], b[i]) for i in range(3)))
    return lo_, hi_
LO, HI = bb_all(ALL)
BLO, BHI = bb_all([o for o in ALL if not o.name.startswith(('Mirror',))])
r4 = lambda v: [round(c, 4) for c in v]
info = {'size_m': {'x_min': round(LO.x, 3), 'x_max': round(HI.x, 3), 'length': round(HI.x - LO.x, 3), 'width_mirrors': round(HI.y - LO.y, 3),
                   'width_body': round(2 * BW, 3), 'height_beacon': round(HI.z, 3), 'height_roof': round(ROOF + 0.04, 3), 'body_length': round(XF - XR, 3),
                   'wheelbase': round(FX - RX, 3), 'track': round(2 * TRK2, 3), 'front_tyre': [FTD, FTW], 'rear_tyre': [RTD, RTW],
                   'floor': FLOOR, 'deck': BELT, 'seat_cushion_top': round(SZ + 0.30, 3), 'ground_clearance': Z_BOT, 'front_axle_x': FX, 'rear_axle_x': RX},
        'doors': {'DoorDriver': {'pivot': r4(DOOR_PIVOT), 'axis': 'z', 'parts': 'DoorDriver* (leaf, glass, handles, trim, hinge leaves)',
                                 'note': 'hinged on its front edge; opens outward with a NEGATIVE rotation about +z, up to -95 deg (checked clear)'}},
        'battery_lid': {'pivot': r4(LID_PIVOT), 'axis': 'y', 'parts': 'BatteryLid*', 'note': 'hinged at its rear edge; lifts with a NEGATIVE rotation about +y, up to -100 deg (checked clear)'},
        'hitch_pins': {'HitchPinRear': {'parts': 'HitchPinRear*', 'move': 'translate +z', 'out_m': 0.40, 'grip': 'IK_HitchPin_Rear'},
                       'HitchPinFront': {'parts': 'HitchPinFront*', 'move': 'translate +z', 'out_m': 0.20, 'grip': 'IK_HitchPin_Front'}},
        'hitches': {'rear_eye_centres_z': [0.28, 0.40, 0.49], 'rear_main_z': 0.40, 'rear_pin_x': HRX, 'rear_slot_gap': 0.06, 'front_z': 0.40, 'front_pin_x': HFX, 'front_slot_gap': 0.06,
                    'gpu_tow_eye_z': eye_z, 'gpu_fits_slot': bool(fits)},
        'steering': {'front_wheels': {'FL': {'pivot': [FX, KP_Y, FZ]}, 'FR': {'pivot': [FX, -KP_Y, FZ]}}, 'axis': 'z', 'max_deg': STEER,
                     'parts_per_wheel': 'Tyre_/Rim_/WheelDisc_/Hub_/HubCap_/Nut_/Spindle_/SteerArm_ + FL|FR',
                     'steering_wheel': {'parts': 'SteeringWheel*', 'centre': r4(WC), 'axis': r4(AX), 'note': 'turns about the column axis (towards the driver)'}},
        'pedals': {k: {'pivot': list(v[0]), 'axis': v[1]} for k, v in PEDALS.items()},
        'wheels': {k: r4(v) for k, v in WHEELS.items()},
        'lamps': sorted(o.name for o in ALL if o.name.endswith('_Lens') or o.name == 'BeaconLens'),
        'ik': {e.name: r4(e.location) for e in IKS},
        'checks': {'interior_not_joined': not_joined, 'open_edges': open_edges, 'floating': fl, 'lamp_clash': cl, 'overhang': oh, 'mesh_loose': lo, 'arch_crowd': crowd, 'steer_hits': len(steer_hits), 'door_hits': len(door_hits),
                   'lid_hits': len(lid_hits), 'pin_hits': len(pin_hits), 'ik_off': ik_off}}

# ================================================================ CC0 photo materials (Art/BaggageTractor/Textures/SOURCES.md)
TEX = os.path.join(ART, 'Textures')
if not (DO_WIRE or DO_CHECK):
    K.uv_cube()
    K.photo(M_PAINT, TEX, 'Paint004', tile=1.5, tint=(1.0, 0.50, 0.015))
    K.photo(M_CAB, TEX, 'Paint004', tile=1.5, tint=(0.97, 0.97, 0.95))
    K.photo(M_STEEL, TEX, 'Metal027', tile=0.6)
    K.photo(M_DARK, TEX, 'Metal027', tile=0.6, tint=(0.25, 0.25, 0.27))
    K.photo(M_RUB, TEX, 'Rubber004', tile=0.5, tint=(0.35, 0.35, 0.35), metal=0.0)
    K.photo(M_BUMP, TEX, 'Rubber004', tile=0.4, tint=(0.28, 0.28, 0.28), metal=0.0, rough_mul=0.9)
    K.photo(M_FLOOR, TEX, 'Rubber001', tile=0.5, metal=0.0)
    K.photo(M_PLAST, TEX, 'Plastic006', tile=0.4, tint=(0.35, 0.35, 0.37), metal=0.0)
    K.photo(M_BATT, TEX, 'Plastic006', tile=0.4, tint=(0.22, 0.24, 0.27), metal=0.0)
    K.photo(M_VINYL, TEX, 'Leather026', tile=0.35, metal=0.0)
    K.photo(M_TREAD, TEX, 'DiamondPlate008A', tile=0.5)
    K.photo(M_CHEV, TEX, 'PaintedMetal016', tile=0.9)
    K.photo(M_REDP, TEX, 'Paint004', tile=0.8, tint=(1.0, 0.06, 0.04), metal=0.0)
    K.photo(M_YELP, TEX, 'Paint004', tile=0.8, tint=(1.0, 0.62, 0.02), metal=0.0)
    K.glass(M_GLASS)

ZF = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, os.path.join(ART, 'BaggageTractor.blend'), os.path.join(ART, 'baggagetractor.glb'), ZF, 'BT', 'BaggageTractor') if not (DO_WIRE or DO_BEAUTY or DO_CHECK) else 'skipped'
info['checks']['flicker'] = last
print('BT: %d parts, %d IK/pivot points, %d faces, flicker: %s' % (len(ALL), len(IKS), sum(len(o.data.polygons) for o in ALL), last))
if not (DO_WIRE or DO_BEAUTY or DO_CHECK):
    a = K.checks('BT after flicker fix', ground_ok=('Tyre',), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)
    info['checks']['after_flicker_fix'] = {'floating': a[0], 'lamp_clash': a[1], 'overhang': a[2], 'mesh_loose': a[3]}
    info['faces'] = sum(len(o.data.polygons) for o in ALL); info['parts'] = len(ALL)

# ================================================================ pictures
def open_parts(prefix, pivot, axis, deg):
    """Swings a moving group for a picture (after the export, so the exported model stays closed)."""
    R = Matrix.Translation(Vector(pivot)) @ Matrix.Rotation(math.radians(deg), 4, axis.upper()) @ Matrix.Translation(-Vector(pivot))
    for o in ALL:
        if o.name.startswith(prefix): o.data.transform(R); o.data.update()
if DO_BEAUTY:
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; ee = sc.eevee
    ee.use_gtao = True; ee.gtao_distance = 0.4; ee.use_ssr = True; ee.use_ssr_refraction = True; ee.use_soft_shadows = True
    ee.shadow_cube_size = '2048'; ee.shadow_cascade_size = '4096'; ee.taa_render_samples = 96; ee.use_bloom = True; ee.bloom_intensity = 0.02
    sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
    w = bpy.data.worlds.new('WB'); w.use_nodes = True; bg = w.node_tree.nodes['Background']; bg.inputs['Color'].default_value = (0.55, 0.65, 0.8, 1); bg.inputs['Strength'].default_value = 0.9; sc.world = w
    gm = pmat('Apron', (0.32, 0.32, 0.31), 0, 0.85)
    ground = box('Ground', (0, 0, -0.01), (30, 30, 0.02), gm); ALL.remove(ground)
    sd = bpy.data.lights.new('Sun', 'SUN'); sd.energy = 4.5; sd.angle = 0.02; so = bpy.data.objects.new('Sun', sd); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(35))
    cd = bpy.data.cameras.new('BCam'); bc = bpy.data.objects.new('BCam', cd); sc.collection.objects.link(bc); sc.camera = bc
    def bshot(name, loc, at, lens=45, res=(2400, 1500)):
        bc.location = loc; bc.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens
        sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(PICS, name + '.png'); bpy.ops.render.render(write_still=True); print('BT: picture', name)
    bshot('BaggageTractor-Final-Front', (5.0, 3.6, 1.45), (0.25, 0.05, 0.95), 35, (3840, 2160))
    bshot('BaggageTractor-Final-Back', (-4.8, -3.4, 1.75), (-0.2, 0.0, 0.95), 35, (3840, 2160))
    bshot('BaggageTractor-Close-Hitch', (-2.9, -1.2, 0.95), (-1.55, 0.0, 0.50), 40)
    bshot('BaggageTractor-Close-Mirror', (2.2, 1.9, 1.55), (1.15, 0.80, 1.05), 40)
    open_parts('DoorDriver', DOOR_PIVOT, 'z', -80)
    bshot('BaggageTractor-Close-Cab', (0.05, 2.25, 1.75), (0.30, 0.0, 1.05), 28, (3840, 2160))
    open_parts('BatteryLid', LID_PIVOT, 'y', -95)
    bshot('BaggageTractor-Close-Battery', (-2.6, 1.9, 2.1), (-1.0, 0.0, 0.85), 35)

if DO_WIRE:
    shoot = K.wire_setup()
    AT = (0.05, 0.0, 1.0)
    W = lambda n: os.path.join(WIRE, n + '.png')
    VIEWS = {}
    def ortho(name, loc, at, scale, res):
        shoot(W(name), loc, at, 50, scale, res=res)
        from bpy_extras.object_utils import world_to_camera_view
        sc = bpy.context.scene; bpy.context.view_layer.update()
        def px(p):
            v = world_to_camera_view(sc, sc.camera, Vector(p)); return [round(v.x * res[0], 2), round((1 - v.y) * res[1], 2)]
        # (the sheet maps metres to this picture's pixels: px(p) = O + p.x (X - O) + p.y (Y - O) + p.z (Z - O))
        VIEWS[name] = {'loc': list(loc), 'at': list(at), 'ortho': scale, 'res': list(res), 'O': px((0, 0, 0)), 'X': px((1, 0, 0)), 'Y': px((0, 1, 0)), 'Z': px((0, 0, 1))}
    shoot(W('1-three-quarter-front'), (5.6, 4.4, 2.9), AT, 45)
    shoot(W('2-three-quarter-rear'), (-5.4, -4.4, 3.0), AT, 45)
    ortho('3-side-left', (0.0, 12, 1.05), (0.0, 0, 1.05), 4.4, (2400, 1500))
    ortho('4-side-right', (0.0, -12, 1.05), (0.0, 0, 1.05), 4.4, (2400, 1500))
    ortho('5-front', (12, 0, 1.08), (0, 0, 1.08), 2.6, (1500, 1300))
    ortho('6-rear', (-12, 0, 1.08), (0, 0, 1.08), 2.6, (1500, 1300))
    ortho('7-top', (0.0, 0, 14), (0.0, 0, 0), 4.4, (2400, 1500))
    CUT = tuple(o.name for o in ALL if o.name.startswith(('CabShell', 'RoofCap', 'RoofLiner', 'CabLamp', 'Beacon', 'Win', 'DoorDriver', 'SunVisor', 'RearGuard', 'GrabHandleIn', 'GrabInFoot', 'Mirror')))
    shoot(W('8-cab-interior'), (1.9, 1.9, 2.55), (0.25, 0.0, 0.95), 30, hide=CUT)
    shoot(W('9-seat-and-controls'), (1.35, -1.05, 1.75), (0.20, 0.0, 1.0), 30, hide=CUT)
    shoot(W('10-battery-bay'), (-2.4, 1.8, 2.2), (-1.0, 0.0, 0.85), 35, hide=tuple(o.name for o in ALL if o.name.startswith('BatteryLid')))
    shoot(W('11-rear-hitch'), (-2.6, -1.0, 0.95), (-1.55, 0.0, 0.45), 40)
    shoot(W('12-front-hitch-lamps'), (2.7, -1.0, 0.95), (1.55, 0.0, 0.55), 40)
    shoot(W('13-mirror-and-hinges'), (1.4, 1.75, 1.35), (0.75, 0.75, 1.10), 35)
    shoot(W('14-underside'), (0.3, -3.2, -0.9), (0.0, 0, 0.30), 35)
    info['wire_views'] = VIEWS

JP = os.path.join(ART, 'baggagetractor-parts.json')
if not DO_WIRE and os.path.exists(JP):
    old = json.load(open(JP)); info.setdefault('wire_views', old.get('wire_views'))
if not (DO_BEAUTY or DO_CHECK) or not os.path.exists(JP):
    json.dump(info, open(JP, 'w'), indent=1)
print('BT size', info['size_m'])
