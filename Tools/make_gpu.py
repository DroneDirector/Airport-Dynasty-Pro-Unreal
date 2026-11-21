# The ground power unit (GPU) - a towable diesel 90 kVA / 400 Hz unit for the A320 (owner 2026-11-21: "create the ground
# power unit right away", high detail and beautiful; a crew member plugs the cable into the A320's receptacle, unplugs it,
# stows it, moves the unit). Built to the vehicle standard learned on the pushback tug (Tools/make_pushback_tug.py; the
# helpers and checks are shared in Tools/vehicle_kit.py).
# Real units it follows (Blueprints/Vehicles/GPU/RESEARCH.md): Guinault GA 90 (3300 x 1880 x 1790, white moulded canopy on a
# galvanised chassis tub with cable trays along both sides, fifth-wheel turntable steering, drawbar that stands up when
# parked), TLD GPU-409 (112 x 68 x 59 in = 2845 x 1727 x 1499 mm, galvanised chassis, fifth-wheel steering, no-drop
# drawbar), Hobart/ITW 4400 (cable trays, fifth-wheel steering). Output cable 20 m, 4-core, 45 mm, NATO 6-pin plug.
# Shape: low galvanised chassis tub (narrow at the front so the front axle can steer 40 deg), a bed plate over the wheels
# that is also the floor of the two side cable trays, a white moulded canopy with rounded edges (curved because the real
# one is moulded), sloped top-front with the radiator grille, engine door left, generator door right, the control panel
# behind a door at the rear (the operator stands behind the unit), the plug parked in its dummy socket on the front face,
# the cable coiled in the right tray, wheel chocks in the left tray, amber beacon on the roof, exhaust stack top-right.
# Axes: x forward (the drawbar end +x), y left, z up; metres; origin on the ground under the chassis middle.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_gpu.py [-- --wire | --beauty]
import bpy, math, os, sys
from mathutils import Vector, Matrix
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import importlib, vehicle_kit as K
importlib.reload(K)
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, lamp, pmat, ALL, IKS

ART = os.path.join(ROOT, 'Art', 'GPU'); os.makedirs(ART, exist_ok=True)
WIRE = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'GPU', 'wire'); os.makedirs(WIRE, exist_ok=True)
PICS = os.path.join(ROOT, 'docs', 'Pictures', 'Vehicles', 'GPU'); os.makedirs(PICS, exist_ok=True)
DO_WIRE, DO_BEAUTY = '--wire' in sys.argv, '--beauty' in sys.argv
K.reset()
def ik(name, loc): return K.ik(name, loc)

# ---------------------------------------------------------------- the main sizes (m)
TD, TW = 0.56, 0.165                 # tyre: 5.00-10 class, 560 diameter, 165 wide (no modelled tread)
TRK = 1.40                           # track (wheel centres y +-0.70)
FX, RX = 0.62, -0.78                 # front axle (turntable centre) / rear axle x - wheelbase 1.40
WZ = TD / 2                          # wheel centre height
TUB_Z0, TUB_Z1 = 0.40, 0.605         # chassis tub (under the bed plate)
BED_X0, BED_X1, BED_Y, BED_Z0, BED_Z1 = -1.15, 1.05, 0.855, 0.600, 0.640
CX0, CX1, CY, CZ0, CZ1 = -1.10, 1.00, 0.62, 0.636, 1.64      # canopy
SLOPE_Z = 1.36; SLOPE_X = 0.78                               # sloped top-front (radiator grille)
TRAY_TOP = 0.86
DOOR_X0, DOOR_X1, DOOR_Z0, DOOR_Z1 = -0.72, 0.48, 0.94, 1.52 # side doors (above the trays' coils)
PDOOR_Y, PDOOR_Z0, PDOOR_Z1 = 0.38, 0.84, 1.44                # rear control-panel door (y +-0.38)
POCKET = 0.40; PPOCKET = 0.16                                 # door recess depths
CAB_R = 0.0225                                                # output cable radius (45 mm 4-core)

# ---------------------------------------------------------------- materials (plain; CC0 photos swapped in below)
M_CANOPY = pmat('GPU_Canopy', (0.92, 0.92, 0.90), 0.0, 0.35)
M_GALV = pmat('GPU_Galvanised', (0.62, 0.64, 0.66), 1.0, 0.45)
M_STEEL = pmat('GPU_Steel', (0.40, 0.41, 0.43), 1.0, 0.4)
M_DARK = pmat('GPU_DarkSteel', (0.08, 0.08, 0.09), 0.6, 0.55)
M_ENGINE = pmat('GPU_EnginePaint', (0.12, 0.13, 0.14), 0.3, 0.5)
M_RUB = pmat('GPU_Tyre', (0.03, 0.03, 0.03), 0.0, 0.9)
M_CABLE = pmat('GPU_CableRubber', (0.02, 0.02, 0.02), 0.0, 0.7)
M_PLAST = pmat('GPU_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_REDP = pmat('GPU_RedPaint', (0.7, 0.04, 0.03), 0.0, 0.4)
M_YELP = pmat('GPU_YellowPaint', (0.95, 0.75, 0.05), 0.0, 0.4)
M_GLASS = pmat('GPU_Glass', (0.2, 0.3, 0.35), 0.0, 0.05)
M_AMBER = pmat('GPU_BeaconLens', (1.0, 0.55, 0.05), 0.0, 0.2, 2.0)
M_RED = pmat('GPU_RedLens', (0.9, 0.05, 0.03), 0.0, 0.2, 1.0)
M_WHITE = pmat('GPU_WhiteLens', (0.95, 0.95, 0.9), 0.0, 0.1, 0.6)
M_GREEN = pmat('GPU_GreenLens', (0.05, 0.8, 0.15), 0.0, 0.2, 1.0)
M_SCREEN = pmat('GPU_Screen', (0.05, 0.25, 0.35), 0.0, 0.1, 0.8)

# ================================================================ CHASSIS (galvanised steel)
# the tub: rear part full width (+-0.50), front part narrow (+-0.30) so the front wheels steer 40 deg under the bed plate
box('ChassisTubRear', ((-1.10 + -0.10) / 2, 0, (TUB_Z0 + TUB_Z1) / 2), (1.00, 1.00, TUB_Z1 - TUB_Z0), M_GALV, bevel=0.012, segs=1)
box('ChassisTubFront', ((-0.105 + 1.00) / 2, 0, (TUB_Z0 + TUB_Z1) / 2), (1.105, 0.60, TUB_Z1 - TUB_Z0), M_GALV, bevel=0.012, segs=1)
# stiffening ribs pressed along the tub sides (welded flat bars)
for s in (-1, 1):
    box(f'TubRib_{s}', (-0.60, s * 0.502, 0.50), (0.90, 0.012, 0.03), M_GALV, bevel=0.003)
    box(f'TubRibFront_{s}', (0.47, s * 0.302, 0.50), (0.60, 0.012, 0.03), M_GALV, bevel=0.003)
# the bed plate: over the wheels, the floor of both cable trays and the canopy's base
box('BedPlate', ((BED_X0 + BED_X1) / 2, 0, (BED_Z0 + BED_Z1) / 2), (BED_X1 - BED_X0, 2 * BED_Y, BED_Z1 - BED_Z0), M_GALV, bevel=0.006, segs=1)
# the cable trays: outer walls with a rolled lip, end walls; drain slots are in the floor texture
for s, nm in ((1, 'L'), (-1, 'R')):
    box(f'TrayWall_{nm}', ((BED_X0 + BED_X1) / 2, s * 0.855, (BED_Z1 - 0.004 + TRAY_TOP) / 2), (BED_X1 - BED_X0 - 0.01, 0.012, TRAY_TOP - BED_Z1 + 0.004), M_GALV, bevel=0.003)
    cyl(f'TrayLip_{nm}', ((BED_X0 + BED_X1) / 2, s * 0.855, TRAY_TOP), 0.011, BED_X1 - BED_X0 - 0.01, 'x', M_GALV, 16)
    for e, x in (('F', BED_X1 - 0.012), ('B', BED_X0 + 0.012)):
        box(f'TrayEnd_{nm}{e}', (x, s * 0.735, (BED_Z1 - 0.004 + TRAY_TOP) / 2), (0.012, 0.236, TRAY_TOP - BED_Z1 + 0.004), M_GALV, bevel=0.003)
    # tray wall stiffeners (vertical pressed ribs on the outside, on flat plate between the ends)
    for k, x in enumerate((-0.55, 0.05, 0.60)):
        box(f'TrayRib_{nm}{k}', (x, s * 0.862, 0.75), (0.03, 0.006, 0.16), M_GALV, bevel=0.002)
# the rear bumper: a galvanised channel on two brackets off the tub's rear face (the tail lamps are set into it)
box('BumperRear', (-1.18, 0, 0.445), (0.06, 1.60, 0.29), M_GALV, bevel=0.008, segs=1)
for s in (-1, 1): box(f'BumperBracket_{s}', (-1.125, s * 0.35, 0.47), (0.056, 0.08, 0.12), M_GALV, bevel=0.004)
# lifting / tie-down lugs on the tub sides (welded plates with a hole)
for nm, x, y in (('RL', -0.98, 0.50), ('RR', -0.98, -0.50), ('FL', 0.85, 0.30), ('FR', 0.85, -0.30)):
    s = 1 if y > 0 else -1
    lug = box(f'TieDownLug_{nm}', (x, y + s * 0.022, 0.47), (0.10, 0.05, 0.10), M_GALV, bevel=0.01)
    cut(lug, cyl(f'TieDownHole_{nm}', (x, y + s * 0.03, 0.47), 0.022, 0.12, 'y', M_GALV, 24))

# ================================================================ RUNNING GEAR
# the turntable (fifth wheel): its top ring bolted under the tub, the lower ring turns with the front axle (FrontAxle_*,
# steering about z through (FX, 0)); the drawbar hinges on the axle's yoke
cyl('TurntableTop', (FX, 0, 0.385), 0.28, 0.034, 'z', M_DARK, 48, bevel=0.004)
for k in range(8):
    a = k * math.pi / 4 + math.pi / 8
    cyl(f'TurntableBolt_{k}', (FX + 0.255 * math.cos(a), 0.255 * math.sin(a), 0.366), 0.010, 0.012, 'z', M_STEEL, 6)
cyl('FrontAxle_Ring', (FX, 0, 0.352), 0.28, 0.036, 'z', M_DARK, 48, bevel=0.004)
box('FrontAxle_Bolster', (FX, 0, 0.328), (0.24, 0.40, 0.024), M_DARK, bevel=0.004)
box('FrontAxle_Beam', (FX, 0, 0.281), (0.10, 1.16, 0.084), M_DARK, bevel=0.008)
box('FrontAxle_DrawbarYoke', (0.98, 0, 0.3125), (0.24, 0.26, 0.065), M_DARK, bevel=0.008)
DB_P = Vector((1.14, 0.0, 0.30))                               # the drawbar's hinge (pin axis along y)
for s in (-1, 1):
    box(f'FrontAxle_Clevis_{s}', (1.13, s * 0.10, 0.30), (0.14, 0.02, 0.11), M_DARK, bevel=0.006)
    cyl(f'FrontAxle_ClevisNut_{s}', (DB_P.x, s * 0.116, DB_P.z), 0.020, 0.016, 'y', M_STEEL, 6)
cyl('FrontAxle_ClevisPin', tuple(DB_P), 0.016, 0.25, 'y', M_STEEL, 16)
for i, (ax, grp) in enumerate(((FX, 'FrontAxle_'), (RX, 'RearAxle_'))):
    if grp == 'RearAxle_':
        box('RearAxle_Beam', (RX, 0, 0.281), (0.10, 1.16, 0.084), M_DARK, bevel=0.008)
        for s in (-1, 1): box(f'RearAxle_Hanger_{s}', (RX, s * 0.40, 0.36), (0.16, 0.06, 0.10), M_DARK, bevel=0.006)
    for s, sd in ((1, 'L'), (-1, 'R')):
        key = ('F' if i == 0 else 'R') + sd
        y = s * TRK / 2
        cyl(f'{grp}Stub_{sd}', (ax, s * 0.635, WZ), 0.028, 0.22, 'y', M_STEEL, 20)
        # the tyre: a real tyre turned from its cross-section - bead on the rim, sidewalls bulging to the widest point at
        # mid-height, rounded shoulders, crown (no modelled tread: the tread comes from the texture)
        tug_prof = [(0.290, -0.124), (0.318, -0.146), (0.360, -0.157), (0.410, -0.1585), (0.460, -0.154), (0.500, -0.142), (0.522, -0.124),
                    (0.534, -0.100), (0.538, -0.060), (0.539, 0.0), (0.538, 0.060), (0.534, 0.100), (0.522, 0.124), (0.500, 0.142),
                    (0.460, 0.154), (0.410, 0.1585), (0.360, 0.157), (0.318, 0.146), (0.290, 0.124), (0.286, 0.0)]
        prof = [(0.135 + (r - 0.29) * (0.145 / 0.249), yy * (TW / 2 / 0.1585)) for r, yy in tug_prof]
        lathe(f'Tyre_{key}', prof, (ax, y, WZ), M_RUB, 72)
        # the rim: split rim with flanges over the beads and the drop centre
        tug_rim = [(0.300, -0.130), (0.296, -0.122), (0.284, -0.118), (0.270, -0.070), (0.262, -0.050), (0.262, 0.050), (0.270, 0.070),
                   (0.284, 0.118), (0.296, 0.122), (0.300, 0.130), (0.288, 0.132), (0.276, 0.122), (0.256, 0.060), (0.256, -0.060), (0.276, -0.122), (0.288, -0.132)]
        lathe(f'Rim_{key}', [(0.135 + (r - 0.29) * 0.58, yy * 0.52) for r, yy in tug_rim], (ax, y, WZ), M_STEEL, 72)
        disc = cyl(f'WheelDisc_{key}', (ax, y + s * 0.035, WZ), 0.118, 0.010, 'y', M_STEEL, 64, bevel=0.002)
        for k in range(5):
            a = k * 2 * math.pi / 5 + math.pi / 5
            cut(disc, cyl(f'HandHole_{key}{k}', (ax + 0.088 * math.cos(a), y + s * 0.035, WZ + 0.088 * math.sin(a)), 0.016, 0.04, 'y', M_DARK, 20))
        cyl(f'Hub_{key}', (ax, y + s * 0.052, WZ), 0.052, 0.030, 'y', M_DARK, 32, bevel=0.006)
        cyl(f'HubCap_{key}', (ax, y + s * 0.072, WZ), 0.030, 0.016, 'y', M_DARK, 24, bevel=0.004)
        for k in range(5):
            a = k * 2 * math.pi / 5
            cyl(f'Nut_{key}{k}', (ax + 0.068 * math.cos(a), y + s * 0.0435, WZ + 0.068 * math.sin(a)), 0.009, 0.014, 'y', M_STEEL, 6)
        ik(f'Wheel_{key}', (ax, y, WZ))          # (the wheel group's hub centre: Tyre_/Rim_/WheelDisc_/Hub_/HubCap_/Nut_ + key)

# ================================================================ THE DRAWBAR (Drawbar*: turns about y through DB_P to stand up;
# it steers the front axle). Lowered (towing) position, resting on its parking support.
cyl('Drawbar_PivotTube', tuple(DB_P), 0.034, 0.176, 'y', M_GALV, 24, bevel=0.004)
EYE = Vector((2.05, 0.0, 0.40)); BEAM_X0 = 1.74
for s in (-1, 1):
    tube(f'Drawbar_Arm_{s}', [(DB_P.x + 0.01, s * 0.065, DB_P.z), (BEAM_X0 + 0.03, s * 0.012, 0.39)], 0.024, M_GALV, bez=False, bres=4)
box('Drawbar_Beam', ((BEAM_X0 + 1.985) / 2, 0, 0.39), (1.985 - BEAM_X0, 0.07, 0.07), M_GALV, bevel=0.008)
torus('Drawbar_Eye', tuple(EYE), 0.058, 0.021, 'z', M_STEEL)           # lunette: 76 mm hole
box('Drawbar_EyeWeld', (1.99, 0, 0.395), (0.03, 0.075, 0.046), M_STEEL, bevel=0.006)
# the hand loop over the beam (the crew pull it by this; IK_Drawbar_Handle)
tube('Drawbar_Handle', [(1.58, 0, 0.375), (1.60, 0, 0.48), (1.80, 0, 0.48), (1.82, 0, 0.405)], 0.016, M_STEEL, bez=False)
# the parking support: a square leg in a sleeve under the beam, a locking pin, the foot on the ground
box('Drawbar_ParkSleeve', (1.90, 0, 0.30), (0.06, 0.06, 0.13), M_GALV, bevel=0.004)
box('Drawbar_ParkLeg', (1.90, 0, 0.185), (0.04, 0.04, 0.35), M_GALV, bevel=0.003)
cyl('Drawbar_ParkPin', (1.90, 0, 0.31), 0.007, 0.10, 'y', M_STEEL, 12)
torus('Drawbar_ParkPinRing', (1.90, 0.050, 0.294), 0.018, 0.003, 'x', M_STEEL, 24, 8)
box('Drawbar_ParkFoot', (1.90, 0, 0.008), (0.12, 0.12, 0.016), M_GALV, bevel=0.003)

# ================================================================ THE CANOPY (moulded: rounded edges, flat panels)
canopy_xz = [(CX0, CZ0), (CX1, CZ0), (CX1, SLOPE_Z), (SLOPE_X, CZ1), (CX0, CZ1)]
canopy = prism('Canopy', canopy_xz, -CY, CY, M_CANOPY, bevel=0.07, segs=5)
# door recesses: the doors close flush over real pockets (engine left, generator right, control panel at the rear)
for s in (-1, 1):
    cut(canopy, box('PocketCut', ((DOOR_X0 + DOOR_X1) / 2, s * (CY - POCKET / 2 + 0.05), (DOOR_Z0 + DOOR_Z1) / 2), (DOOR_X1 - DOOR_X0, POCKET + 0.10, DOOR_Z1 - DOOR_Z0), M_DARK))
cut(canopy, box('PocketCut', (CX0 + PPOCKET / 2 - 0.05, 0, (PDOOR_Z0 + PDOOR_Z1) / 2), (PPOCKET + 0.10, 2 * PDOOR_Y, PDOOR_Z1 - PDOOR_Z0), M_DARK))
# the radiator grille on the sloped top-front: a frame set into the slope, slats across, a mesh behind
SL_N = Vector((SLOPE_Z - CZ1, 0, SLOPE_X - CX1)).normalized() * -1      # the slope's outward normal
SL_T = (Vector((SLOPE_X, 0, CZ1)) - Vector((CX1, 0, SLOPE_Z))).normalized()
SL_MID = (Vector((SLOPE_X, 0, CZ1)) + Vector((CX1, 0, SLOPE_Z))) / 2
SL_ANG = math.degrees(math.atan2(SL_N.x, SL_N.z))
def on_slope(name, t, n, size, m, bevel=0.0):
    c = SL_MID + SL_T * t + SL_N * n
    o = box(name, tuple(c), size, m, bevel=bevel); return turn(o, c, 'y', SL_ANG)
cut(canopy, on_slope('GrilleCut', 0.0, 0.0, (0.17, 0.96, 0.05), M_DARK))
on_slope('GrilleMesh', 0.0, -0.021, (0.18, 0.97, 0.006), M_DARK)
on_slope('GrilleFrame', 0.0, -0.004, (0.20, 0.99, 0.02), M_PLAST, 0.004)
cut(bpy.data.objects['GrilleFrame'], on_slope('GrilleFrameCut', 0.0, 0.0, (0.164, 0.954, 0.06), M_DARK))
for k in range(5):
    on_slope(f'GrilleSlat_{k}', -0.068 + k * 0.034, -0.008, (0.010, 0.962, 0.024), M_PLAST)

# ---------------------------------------------------------------- side doors: DoorEngineL (left, engine) and DoorGenR (right,
# generator) - leaves flush in their openings, hung on two hinges at the FRONT edge (hinge axis vertical), handle at the back
HX = DOOR_X1 + 0.008                                         # hinge axis x
DOORS = {}
for s, nm in ((1, 'DoorEngineL'), (-1, 'DoorGenR')):
    yo = s * CY
    box(nm, ((DOOR_X0 + DOOR_X1) / 2 - 0.002, s * (CY - 0.015), (DOOR_Z0 + DOOR_Z1) / 2), (DOOR_X1 - DOOR_X0 - 0.008, 0.03, DOOR_Z1 - DOOR_Z0 - 0.008), M_CANOPY, bevel=0.006, segs=2)
    # louvred air vent: a frame on the leaf, angled blades
    LV_X0, LV_X1, LV_Z0, LV_Z1 = -0.50, 0.20, 1.04, 1.40
    fr = box(f'{nm}_LouvreFrame', ((LV_X0 + LV_X1) / 2, s * (CY + 0.004), (LV_Z0 + LV_Z1) / 2), (LV_X1 - LV_X0 + 0.04, 0.016, LV_Z1 - LV_Z0 + 0.04), M_PLAST, bevel=0.004)
    cut(fr, box('LvCut', ((LV_X0 + LV_X1) / 2, s * (CY + 0.004), (LV_Z0 + LV_Z1) / 2), (LV_X1 - LV_X0, 0.05, LV_Z1 - LV_Z0), M_DARK))
    box(f'{nm}_LouvreBack', ((LV_X0 + LV_X1) / 2, s * (CY - 0.003), (LV_Z0 + LV_Z1) / 2), (LV_X1 - LV_X0 + 0.01, 0.008, LV_Z1 - LV_Z0 + 0.01), M_DARK)
    for k in range(8):
        z = LV_Z0 + 0.025 + k * 0.044
        o = box(f'{nm}_Louvre_{k}', ((LV_X0 + LV_X1) / 2, s * (CY + 0.004), z), (LV_X1 - LV_X0 + 0.006, 0.004, 0.040), M_PLAST)
        turn(o, ((LV_X0 + LV_X1) / 2, s * (CY + 0.004), z), 'x', s * 40)
    # handle: a recessed T-handle with its lock, near the free (rear) edge
    HZ = (DOOR_Z0 + DOOR_Z1) / 2
    box(f'{nm}_HandleBase', (DOOR_X0 + 0.09, s * (CY + 0.002), HZ), (0.05, 0.008, 0.16), M_PLAST, bevel=0.003)
    box(f'{nm}_Handle', (DOOR_X0 + 0.09, s * (CY + 0.012), HZ), (0.030, 0.016, 0.12), M_STEEL, bevel=0.006)
    cyl(f'{nm}_Lock', (DOOR_X0 + 0.09, s * (CY + 0.004), HZ + 0.10), 0.012, 0.012, 'y', M_STEEL, 16)
    # hinges: knuckles on the canopy (static), leaves screwed onto the door
    for k, z in enumerate((DOOR_Z0 + 0.12, DOOR_Z1 - 0.12)):
        box(f'Hinge{nm[4:]}_{k}_Base', (HX + 0.031, s * (CY + 0.002), z), (0.05, 0.008, 0.09), M_STEEL, bevel=0.002)
        cyl(f'Hinge{nm[4:]}_{k}', (HX, s * (CY + 0.010), z), 0.012, 0.10, 'z', M_STEEL, 16)
        box(f'{nm}_HingeLeaf_{k}', (HX - 0.031, s * (CY + 0.002), z), (0.05, 0.008, 0.09), M_STEEL, bevel=0.002)
    DOORS[nm] = ((HX, s * (CY + 0.010), (DOOR_Z0 + DOOR_Z1) / 2), 'z', 'opens outward: %s deg about +z' % ('negative' if s > 0 else 'positive'))
    ik(nm + '_Handle', (DOOR_X0 + 0.09, s * (CY + 0.03), HZ))
    ik(nm + '_Hinge', DOORS[nm][0])
# ---------------------------------------------------------------- the rear door over the control panel (DoorPanel): hinged on
# its left edge (seen from behind: +y), a window to read the display through
PHY = PDOOR_Y + 0.008
pd = box('DoorPanel', (CX0 + 0.015, 0, (PDOOR_Z0 + PDOOR_Z1) / 2), (0.03, 2 * PDOOR_Y - 0.008, PDOOR_Z1 - PDOOR_Z0 - 0.008), M_CANOPY, bevel=0.006, segs=2)
cut(pd, box('PdWinCut', (CX0 + 0.015, 0, 1.30), (0.06, 0.46, 0.16), M_DARK, bevel=0.01, segs=2))
box('DoorPanel_Window', (CX0 + 0.015, 0, 1.30), (0.008, 0.47, 0.17), M_GLASS)
box('DoorPanel_HandleBase', (CX0 - 0.002, -PDOOR_Y + 0.08, 1.12), (0.008, 0.05, 0.16), M_PLAST, bevel=0.003)
box('DoorPanel_Handle', (CX0 - 0.012, -PDOOR_Y + 0.08, 1.12), (0.016, 0.030, 0.12), M_STEEL, bevel=0.006)
cyl('DoorPanel_Lock', (CX0 - 0.004, -PDOOR_Y + 0.08, 1.22), 0.012, 0.012, 'x', M_STEEL, 16)
for k, z in enumerate((PDOOR_Z0 + 0.10, PDOOR_Z1 - 0.10)):
    box(f'HingePanel_{k}_Base', (CX0 - 0.002, PHY + 0.031, z), (0.008, 0.05, 0.09), M_STEEL, bevel=0.002)
    cyl(f'HingePanel_{k}', (CX0 - 0.010, PHY, z), 0.012, 0.10, 'z', M_STEEL, 16)
    box(f'DoorPanel_HingeLeaf_{k}', (CX0 - 0.002, PHY - 0.031, z), (0.008, 0.05, 0.09), M_STEEL, bevel=0.002)
DOORS['DoorPanel'] = ((CX0 - 0.010, PHY, (PDOOR_Z0 + PDOOR_Z1) / 2), 'z', 'opens outward: negative deg about +z')
ik('DoorPanel_Handle', (CX0 - 0.03, -PDOOR_Y + 0.08, 1.12)); ik('DoorPanel_Hinge', DOORS['DoorPanel'][0])

# ---------------------------------------------------------------- the control panel (in the rear recess; faces -x)
PX = CX0 + PPOCKET                                            # the recess back wall
box('PanelPlate', (PX - 0.008, 0, (PDOOR_Z0 + PDOOR_Z1) / 2), (0.020, 2 * PDOOR_Y - 0.06, PDOOR_Z1 - PDOOR_Z0 - 0.06), M_PLAST, bevel=0.004)
PF = PX - 0.018                                               # the panel's face
box('PanelDisplayBezel', (PF - 0.006, 0, 1.30), (0.020, 0.24, 0.13), M_PLAST, bevel=0.006)
box('PanelDisplay', (PF - 0.016, 0, 1.30), (0.004, 0.20, 0.10), M_SCREEN)
BTN = (('Start', 0.24, 1.10, M_GREEN), ('Stop', 0.14, 1.10, M_RED), ('ContactorOn', -0.14, 1.10, M_GREEN), ('ContactorOff', -0.24, 1.10, M_RED))
for nm, y, z, m in BTN:
    cyl(f'PanelBtn_{nm}_Bezel', (PF - 0.006, y, z), 0.026, 0.016, 'x', M_STEEL, 24, bevel=0.002)
    cyl(f'PanelBtn_{nm}', (PF - 0.016, y, z), 0.019, 0.012, 'x', m, 24, bevel=0.003)
    ik(f'Panel_{nm}', (PF - 0.022, y, z))
cyl('PanelKeySwitch', (PF - 0.006, 0.0, 1.10), 0.022, 0.020, 'x', M_STEEL, 24, bevel=0.003)
box('PanelKey', (PF - 0.021, 0.0, 1.10), (0.012, 0.006, 0.03), M_STEEL)
cyl('PanelSelector', (PF - 0.006, 0.0, 0.98), 0.028, 0.020, 'x', M_PLAST, 24, bevel=0.003)
box('PanelSelectorKnob', (PF - 0.022, 0.0, 0.98), (0.014, 0.012, 0.045), M_PLAST, bevel=0.003)
for k, y in enumerate((0.22, -0.22)):                        # voltmeter / frequency meter
    cyl(f'PanelGauge_{k}', (PF - 0.006, y, 0.96), 0.045, 0.020, 'x', M_PLAST, 32, bevel=0.003)
    cyl(f'PanelGaugeFace_{k}', (PF - 0.016, y, 0.96), 0.036, 0.004, 'x', M_SCREEN, 32)
cyl('PanelHourMeter', (PF - 0.006, 0.10, 0.94), 0.025, 0.020, 'x', M_PLAST, 24, bevel=0.003)
cyl('PanelLampTest', (PF - 0.006, -0.10, 0.94), 0.015, 0.020, 'x', M_WHITE, 16)
# emergency stop OUTSIDE on the rear face (reachable with the door shut): yellow housing, red mushroom
box('EStopHousing', (CX0 - 0.008, -0.47, 1.20), (0.036, 0.08, 0.08), M_YELP, bevel=0.006)
cyl('EStopStem', (CX0 - 0.032, -0.47, 1.20), 0.016, 0.016, 'x', M_DARK, 20)
cyl('EStopButton', (CX0 - 0.046, -0.47, 1.20), 0.030, 0.016, 'x', M_REDP, 32, bevel=0.005)
ik('Panel_EStop', (CX0 - 0.056, -0.47, 1.20))
ik('Stand_Panel', (CX0 - 0.55, 0.0, 0.0))

# ---------------------------------------------------------------- inside the pockets (seen when the doors open)
EY = CY - POCKET                                              # the pockets' back walls (y +-0.22)
PZ0 = DOOR_Z0                                                 # pocket floors
box('EngineBlock', (-0.15, EY + 0.10, PZ0 + 0.166), (0.70, 0.21, 0.34), M_ENGINE, bevel=0.02, segs=2)
box('EngineValveCover', (-0.15, EY + 0.10, PZ0 + 0.375), (0.64, 0.16, 0.08), M_ENGINE, bevel=0.02, segs=2)
cyl('EngineOilFilter', (0.02, EY + 0.225, PZ0 + 0.14), 0.045, 0.12, 'z', M_YELP, 24, bevel=0.005)
cyl('EnginePulley', (0.212, EY + 0.10, PZ0 + 0.15), 0.075, 0.03, 'x', M_STEEL, 32, bevel=0.003)
cyl('EngineAlternator', (0.13, EY + 0.25, PZ0 + 0.29), 0.055, 0.12, 'x', M_STEEL, 24, bevel=0.005)
cyl('EngineAirFilter', (-0.20, EY + 0.12, PZ0 + 0.47), 0.06, 0.36, 'x', M_PLAST, 32, bevel=0.01)
tube('EngineIntakeHose', [(0.0, EY + 0.12, PZ0 + 0.47), (0.08, EY + 0.12, PZ0 + 0.45), (0.10, EY + 0.12, PZ0 + 0.40)], 0.028, M_PLAST, bez=False)
cyl('EngineDipstick', (-0.42, EY + 0.208, PZ0 + 0.30), 0.006, 0.18, 'z', M_YELP, 8)
# generator side (right pocket)
cyl('GenHousing', (-0.20, -(EY + 0.15), PZ0 + 0.22), 0.16, 0.70, 'x', M_ENGINE, 48, bevel=0.01)
for k in range(6): cyl(f'GenRib_{k}', (-0.50 + k * 0.12, -(EY + 0.15), PZ0 + 0.22), 0.172, 0.02, 'x', M_ENGINE, 48, bevel=0.004)
cyl('GenEndBell', (0.185, -(EY + 0.15), PZ0 + 0.22), 0.14, 0.08, 'x', M_STEEL, 48, bevel=0.01)
box('GenTerminalBox', (-0.18, -(EY + 0.15), PZ0 + 0.42), (0.24, 0.16, 0.10), M_ENGINE, bevel=0.01)
tube('GenOutputCable', [(-0.06, -(EY + 0.15), PZ0 + 0.43), (0.20, -(EY + 0.12), PZ0 + 0.50), (DOOR_X1 + 0.005, -(EY + 0.10), PZ0 + 0.50)], 0.016, M_CABLE)
box('Battery', (0.355, -(EY + 0.11), PZ0 + 0.096), (0.22, 0.20, 0.20), M_PLAST, bevel=0.008)
for k, y in enumerate((-(EY + 0.05), -(EY + 0.17))): cyl(f'BatteryPost_{k}', (0.30, y, PZ0 + 0.205), 0.010, 0.025, 'z', M_STEEL, 12)
box('CoolantTank', (0.33, -(EY + 0.08), PZ0 + 0.40), (0.18, 0.17, 0.16), M_CANOPY, bevel=0.02, segs=2)
cyl('CoolantCap', (0.33, -(EY + 0.08), PZ0 + 0.495), 0.03, 0.02, 'z', M_PLAST, 20)

# ---------------------------------------------------------------- the left side: fuel filler (cap in a cup), behind the door
cup = cyl('FuelFillerCup', (-0.92, CY - 0.01, 1.22), 0.065, 0.04, 'y', M_PLAST, 32, bevel=0.004)
cut(cup, cyl('FuelCupCut', (-0.92, CY + 0.025, 1.22), 0.055, 0.04, 'y', M_PLAST, 32))
cyl('FuelFillerCap', (-0.92, CY - 0.002, 1.22), 0.045, 0.02, 'y', M_YELP, 32, bevel=0.004)
box('FuelFillerCapGrip', (-0.92, CY + 0.010, 1.22), (0.012, 0.012, 0.07), M_YELP, bevel=0.003)
ik('Fuel_Filler', (-0.92, CY + 0.03, 1.22))
# the right side behind the door: the engine's air outlet louvre (fixed)
fr = box('OutletFrame', (-0.92, -(CY + 0.004), 1.22), (0.14, 0.016, 0.36), M_PLAST, bevel=0.004)
cut(fr, box('OfCut', (-0.92, -(CY + 0.004), 1.22), (0.10, 0.05, 0.32), M_DARK))
box('OutletBack', (-0.92, -(CY - 0.003), 1.22), (0.11, 0.008, 0.33), M_DARK)
for k in range(7):
    z = 1.08 + k * 0.046
    turn(box(f'OutletBlade_{k}', (-0.92, -(CY + 0.004), z), (0.104, 0.004, 0.042), M_PLAST), (-0.92, -(CY + 0.004), z), 'x', -40)

# ---------------------------------------------------------------- the front face: extinguisher box, the plug's dummy socket,
# the cable gland, white reflectors on the tray ends
box('ExtinguisherBox', (CX1 + 0.06, 0.30, 1.02), (0.13, 0.17, 0.42), M_REDP, bevel=0.02, segs=3)
box('ExtinguisherLid', (CX1 + 0.127, 0.30, 1.02), (0.006, 0.13, 0.36), M_REDP, bevel=0.003)
box('ExtinguisherLatch', (CX1 + 0.134, 0.30, 1.16), (0.012, 0.04, 0.03), M_DARK, bevel=0.003)
for z in (0.88, 1.16): box(f'ExtinguisherStrap_{z}', (CX1 + 0.06, 0.30, z), (0.134, 0.176, 0.02), M_DARK)
PS = Vector((CX1, -0.24, 1.05))                               # the dummy (parking) socket
sock = box('PlugParkSocket', (PS.x + 0.035, PS.y, PS.z), (0.09, 0.16, 0.16), M_PLAST, bevel=0.012, segs=2)
box('PlugParkSocketFlange', (PS.x + 0.004, PS.y, PS.z), (0.012, 0.19, 0.19), M_PLAST, bevel=0.004)
cut(sock, cyl('SockBore', (PS.x + 0.08, PS.y, PS.z), 0.062, 0.08, 'x', M_PLAST, 40))
cyl('PlugParkSocketPins', (PS.x + 0.045, PS.y, PS.z), 0.05, 0.012, 'x', M_DARK, 32)
GL = Vector((CX1, -0.45, 0.78))                               # the cable gland (the cable leaves the unit here)
cyl('CableGland', (GL.x + 0.012, GL.y, GL.z), 0.040, 0.06, 'x', M_PLAST, 32, bevel=0.004)
cyl('CableGlandNut', (GL.x + 0.050, GL.y, GL.z), 0.034, 0.02, 'x', M_PLAST, 6)
ik('Cable_Exit', (GL.x + 0.06, GL.y, GL.z))
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'ReflectorFront_{nm}', (BED_X1 - 0.006, s * 0.735, 0.76), 'x+', 0.07, 0.05, M_WHITE, M_DARK, M_PLAST, depth=0.02)

# ---------------------------------------------------------------- the output cable: 20 m of 45 mm 4-core rubber cable,
# 5 loops coiled in the right tray (CableCoil_0..4, a helix: each loop rises one cable-width), the run from the gland
# down into the tray (CableRun_Gland), the run up from the top loop to the plug (CableRun_Plug)
CC_Y = -0.74; CC_W = 0.075; CC_X0, CC_X1 = -0.95, 0.85; CC_DZ = 0.041; CC_Z0 = BED_Z1 + CAB_R - 0.003
def stadium(t):
    """A point on the coil's flat loop (t 0..1). Each loop starts and ends at the front-most point of the oval (heading
    inwards, +y), so the loops join one above the other into one continuous helix."""
    L1 = CC_X1 - CC_X0; arc = math.pi * CC_W; tot = 2 * L1 + 2 * arc; d = (t * tot + 2 * L1 + 1.5 * arc) % tot
    if d < L1: return (CC_X1 - d, CC_Y + CC_W)
    d -= L1
    if d < arc: a = d / CC_W; return (CC_X0 - CC_W * math.sin(a), CC_Y + CC_W * math.cos(a))
    d -= arc
    if d < L1: return (CC_X0 + d, CC_Y - CC_W)
    d -= L1; a = d / CC_W; return (CC_X1 + CC_W * math.sin(a), CC_Y - CC_W * math.cos(a))
NLOOP = 5
for k in range(NLOOP):
    pts = []
    for j in range(161):
        t = j / 160; x, y = stadium(t); pts.append((x, y, CC_Z0 + (k + t) * CC_DZ))
    tube(f'CableCoil_{k}', pts, CAB_R, M_CABLE, bez=False, bres=4)
COIL_FRONT = CC_X1 + CC_W
COIL_TOP = Vector((COIL_FRONT, CC_Y, CC_Z0 + NLOOP * CC_DZ))
# from the gland forward, out over the tray's front wall, down through the hollow middle of the coil to the bottom loop
tube('CableRun_Gland', [(GL.x + 0.055, GL.y, GL.z), (1.12, -0.47, 0.84), (1.15, -0.62, 0.96), (1.04, -0.77, 1.03), (0.87, -0.745, 1.00),
                        (0.80, CC_Y, 0.90), (0.80, CC_Y, 0.72), (0.85, CC_Y, CC_Z0), (COIL_FRONT - 0.003, CC_Y, CC_Z0)], CAB_R, M_CABLE, res=10, bres=4)
# the plug: a NATO-style 6-pin aircraft connector (head 120 mm), grip rings, the strain-relief boot, a D-handle on top;
# parked nose-first (-x) in the dummy socket. CablePlug* = the plug group; its nose centre and axis are written below.
PN = Vector((PS.x + 0.05, PS.y, PS.z))                       # the plug's nose (pins) inside the socket bore
cyl('CablePlug', (PN.x + 0.075, PN.y, PN.z), 0.060, 0.15, 'x', M_PLAST, 40, bevel=0.006)
for k in range(3): cyl(f'CablePlug_Ring_{k}', (PN.x + 0.10 + k * 0.022, PN.y, PN.z), 0.064, 0.010, 'x', M_PLAST, 40, bevel=0.002)
cyl('CablePlug_Boot', (PN.x + 0.205, PN.y, PN.z), 0.050, 0.12, 'x', M_PLAST, 32, r2=0.050)
cyl('CablePlug_BootTip', (PN.x + 0.285, PN.y, PN.z), 0.026, 0.05, 'x', M_PLAST, 24)
# (taper the boot: scale its far end down)
bo = bpy.data.objects['CablePlug_Boot']
for v in bo.data.vertices:
    f = (v.co.x - (PN.x + 0.145)) / 0.12
    v.co.y = PN.y + (v.co.y - PN.y) * (1 - 0.4 * f); v.co.z = PN.z + (v.co.z - PN.z) * (1 - 0.4 * f)
tube('CablePlug_Handle', [(PN.x + 0.05, PN.y, PN.z + 0.052), (PN.x + 0.07, PN.y, PN.z + 0.115), (PN.x + 0.19, PN.y, PN.z + 0.115), (PN.x + 0.21, PN.y, PN.z + 0.045)], 0.013, M_PLAST, bez=False)
PLUG_TAIL = Vector((PN.x + 0.31, PN.y, PN.z))
tube('CableRun_Plug', [tuple(COIL_TOP + Vector((0, -0.003, 0))), (COIL_FRONT + 0.035, -0.70, COIL_TOP.z + 0.04), (1.08, -0.62, 0.98), (1.16, -0.50, 1.00), (1.30, -0.40, 1.00),
                       (1.42, -0.28, 1.02), (1.40, PN.y - 0.005, PN.z - 0.005), tuple(PLUG_TAIL - Vector((0.01, 0, 0)))], CAB_R, M_CABLE, res=10, bres=4)
ik('Plug_Grip', (PN.x + 0.13, PN.y, PN.z + 0.115)); ik('Plug_Grip2', (PN.x + 0.24, PN.y, PN.z - 0.05))
ik('Stand_Plug', (CX1 + 0.70, -0.55, 0.0))

# ---------------------------------------------------------------- the roof: amber beacon on its base, lifting eye, exhaust stack
BX, BYY = -0.30, 0.0
box('BeaconBase', (BX, BYY, CZ1 + 0.008), (0.18, 0.18, 0.024), M_DARK, bevel=0.005)
cyl('BeaconLens', (BX, BYY, CZ1 + 0.083), 0.072, 0.13, 'z', M_AMBER, 32, bevel=0.02)
cyl('BeaconCap', (BX, BYY, CZ1 + 0.155), 0.058, 0.02, 'z', M_DARK, 32)
box('LiftEyeBase', (0.30, 0.0, CZ1 + 0.006), (0.16, 0.10, 0.02), M_STEEL, bevel=0.004)
eye = box('LiftEye', (0.30, 0.0, CZ1 + 0.065), (0.12, 0.025, 0.11), M_STEEL, bevel=0.012, segs=3)
cut(eye, cyl('LiftEyeHole', (0.30, 0.0, CZ1 + 0.075), 0.026, 0.06, 'y', M_STEEL, 24))
EXX, EXY = -0.85, -0.40
cyl('ExhaustCollar', (EXX, EXY, CZ1 + 0.012), 0.065, 0.03, 'z', M_DARK, 32, bevel=0.004)
cyl('ExhaustPipe', (EXX, EXY, CZ1 + 0.075), 0.040, 0.16, 'z', M_STEEL, 24)
cyl('ExhaustRainCapHinge', (EXX + 0.045, EXY, CZ1 + 0.158), 0.006, 0.05, 'y', M_STEEL, 12)
turn(cyl('ExhaustRainCap', (EXX, EXY, CZ1 + 0.165), 0.046, 0.006, 'z', M_STEEL, 24), (EXX + 0.045, EXY, CZ1 + 0.158), 'y', 12)

# ---------------------------------------------------------------- rear lights (set into the bumper face) + side reflectors
RF = -1.21
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'TailBrake_{nm}', (RF, s * 0.62, 0.445), 'x-', 0.12, 0.10, M_RED, M_DARK, M_PLAST)
    lamp(f'IndicatorRear_{nm}', (RF, s * 0.44, 0.445), 'x-', 0.10, 0.10, M_AMBER, M_DARK, M_PLAST)
    lamp(f'ReflectorRear_{nm}', (RF, s * 0.27, 0.445), 'x-', 0.09, 0.06, M_RED, M_DARK, M_PLAST, depth=0.024)
    for k, x in enumerate((-0.90, 0.80)):
        lamp(f'ReflectorSide_{nm}{k}', (x, s * 0.861, 0.75), 'y' + ('+' if s > 0 else '-'), 0.07, 0.045, M_AMBER, M_DARK, M_PLAST, depth=0.02)
box('NumberPlateHolder', (RF - 0.002, 0.0, 0.445), (0.008, 0.30, 0.12), M_PLAST, bevel=0.003)

# ---------------------------------------------------------------- the left tray: two rubber wheel chocks with rope handles
for k, x0 in enumerate((-0.80, -0.35)):
    prism(f'WheelChock_{k}', [(x0, BED_Z1 - 0.004), (x0 + 0.26, BED_Z1 - 0.004), (x0 + 0.20, BED_Z1 + 0.14), (x0 + 0.11, BED_Z1 + 0.14)], 0.665, 0.815, M_RUB, bevel=0.012, segs=2)
    tube(f'WheelChockRope_{k}', [(x0 + 0.13, 0.74, BED_Z1 + 0.135), (x0 + 0.14, 0.74, BED_Z1 + 0.20), (x0 + 0.17, 0.74, BED_Z1 + 0.20), (x0 + 0.18, 0.74, BED_Z1 + 0.135)], 0.006, M_CABLE, bez=True)

# ---------------------------------------------------------------- IK + pivots for the game
ik('Drawbar_Handle', (1.70, 0.0, 0.48)); ik('TowEye', tuple(EYE)); ik('ParkSupport', (1.90, 0.07, 0.31)); ik('ParkSupport_Foot', (1.90, 0.0, 0.0))
ik('Drawbar_Pivot', tuple(DB_P)); ik('FrontAxle_Pivot', (FX, 0.0, 0.352))
ik('Stand_Drawbar', (2.25, 0.35, 0.0)); ik('Stand_DoorEngineL', (-0.40, 1.30, 0.0)); ik('Stand_DoorGenR', (-0.40, -1.30, 0.0))

# ================================================================ checks
SKIP_OH = ('CableRun_Gland', 'CableRun_Plug', 'ExhaustRainCap', 'ExhaustRainCapHinge', 'ExtinguisherLatch', 'PlugParkSocketPins',
           'Drawbar_EyeWeld', 'Drawbar_PivotTube', 'Drawbar_Beam', 'Drawbar_Handle', 'CableGlandNut', 'Drawbar_ParkPin', 'Drawbar_ParkPinRing', 'Drawbar_ParkSleeve', 'Drawbar_ParkLeg', 'Drawbar_ParkFoot')
OH_PREFIX = ('FrontAxle_Stub', 'RearAxle_Stub', 'Hinge', 'CablePlug', 'Door', 'TrayLip', 'TrayEnd', 'GrilleSlat', 'WheelChockRope', 'Panel', 'EStop', 'Nut_', 'Hub', 'Drawbar_Arm', 'FrontAxle_Clevis', 'TurntableBolt', 'Engine', 'Gen', 'Battery', 'Coolant', 'OutletBlade', 'ExhaustPipe', 'ExhaustHeat')
fl, cl, oh, lo = K.checks('GPU', ground_ok=('Tyre', 'Drawbar_ParkFoot'), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)
for n in oh: print('GPU overhang', n, [m.name for m in ALL if m.name != n and K.touch(K.bbox(bpy.data.objects[n]), K.bbox(m)) and K.diag(K.bbox(m)) > 0.4])
# the front wheels steer 40 deg each way without touching the chassis (turntable steering): sweep the front tyres
from mathutils.bvhtree import BVHTree
STATIC = [o for o in ALL if not o.name.startswith(('FrontAxle_', 'Drawbar', 'Tyre_F', 'Rim_F', 'WheelDisc_F', 'Hub_F', 'HubCap_F', 'Nut_F', 'TurntableTop', 'TurntableBolt'))]
ST = {o.name: K.bvh(o) for o in STATIC}
steer_hits = set()
for deg in (-40, -25, 25, 40):
    R = Matrix.Translation((FX, 0, 0)) @ Matrix.Rotation(math.radians(deg), 4, 'Z') @ Matrix.Translation((-FX, 0, 0))
    for o in ALL:
        if not o.name.startswith(('Tyre_F', 'Drawbar_Arm', 'Drawbar_Beam', 'FrontAxle_Beam')): continue
        t = BVHTree.FromPolygons([R @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
        for n, tr in ST.items():
            if t.overlap(tr): steer_hits.add((o.name, n, deg))
print('GPU CHECK steering sweep +-40 deg: %d hits %s' % (len(steer_hits), sorted(steer_hits)[:12]))
# the drawbar stands up (90 deg about y through its pivot) without touching anything fixed
R = Matrix.Translation(DB_P) @ Matrix.Rotation(math.radians(-80), 4, 'Y') @ Matrix.Translation(-DB_P)
db_hits = set()
for o in ALL:
    if not o.name.startswith('Drawbar'): continue
    t = BVHTree.FromPolygons([R @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
    for n, tr in ST.items():
        if t.overlap(tr) and not n.startswith('FrontAxle'): db_hits.add((o.name, n))
print('GPU CHECK drawbar raised: %d hits %s' % (len(db_hits), sorted(db_hits)[:12]))
# the doors swing 100 deg open without touching anything (except their own parts)
door_hits = set()
for nm, (piv, ax, _) in DOORS.items():
    s = -1 if nm in ('DoorEngineL', 'DoorPanel') else 1
    for deg in (30, 60, 100):
        R = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(s * deg), 4, 'Z') @ Matrix.Translation(-Vector(piv))
        for o in ALL:
            if not o.name.startswith(nm): continue
            t = BVHTree.FromPolygons([R @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
            for n, tr in ST.items():
                if n.startswith(nm) or n.startswith('Hinge'): continue
                if t.overlap(tr): door_hits.add((o.name, n, deg))
print('GPU CHECK doors open 30/60/100: %d hits %s' % (len(door_hits), sorted(door_hits)[:12]))
# the cable runs touch only the cable's own parts (coils, gland, plug, socket) - never pass through the unit
cable_hits = set()
for o in ALL:
    if not o.name.startswith('CableRun'): continue
    t = K.bvh(o)
    for p2 in ALL:
        if p2.name.startswith(('CableRun', 'CableCoil', 'CableGland', 'CablePlug', 'PlugParkSocket', 'BedPlate')): continue   # (the bottom run lies on the tray floor)
        a, b = K.bbox(o), K.bbox(p2)
        if K.touch(a, b) and t.overlap(K.bvh(p2)): cable_hits.add((o.name, p2.name))
print('GPU CHECK cable runs: %d hits %s' % (len(cable_hits), sorted(cable_hits)))

# ---------------------------------------------------------------- the moving parts, IK points, sizes -> json for the main session
import json
def bb_all():
    lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
    for o in ALL:
        a, b = K.bbox(o); lo = Vector(tuple(min(lo[i], a[i]) for i in range(3))); hi = Vector(tuple(max(hi[i], b[i]) for i in range(3)))
    return lo, hi
LO, HI = bb_all()
info = {'size_m': {'x_min': round(LO.x, 3), 'x_max': round(HI.x, 3), 'length': round(HI.x - LO.x, 3), 'width': round(HI.y - LO.y, 3), 'height': round(HI.z, 3)},
        'doors': {k: {'pivot': [round(c, 4) for c in v[0]], 'axis': v[1], 'note': v[2]} for k, v in DOORS.items()},
        'drawbar': {'pivot': [round(c, 4) for c in DB_P], 'axis': 'y', 'note': 'parts Drawbar*; lowered = 0; stands up with a negative rotation about +y (up to -80 deg, checked clear)'},
        'front_axle': {'pivot': [FX, 0.0, 0.352], 'axis': 'z', 'note': 'parts FrontAxle_* + front wheels; steer +-40 deg'},
        'wheels': {k: [round(c, 4) for c in e.location] for k, e in ((e.name[3:], e) for e in IKS if e.name.startswith('IK_Wheel_'))},
        'plug': {'nose': [round(c, 4) for c in PN], 'axis_into_socket': [-1, 0, 0], 'parts': 'CablePlug*'},
        'cable_gland': [round(c, 4) for c in (GL.x + 0.06, GL.y, GL.z)], 'cable_coils': ['CableCoil_%d' % k for k in range(NLOOP)],
        'cable_length_m': round(NLOOP * (2 * (CC_X1 - CC_X0) + 2 * math.pi * CC_W) + 2.2, 1),
        'ik': {e.name: [round(c, 4) for c in e.location] for e in IKS},
        'checks': {'floating': fl, 'lamp_clash': cl, 'overhang': oh, 'mesh_loose': lo, 'steer_hits': len(steer_hits), 'drawbar_hits': len(db_hits), 'door_hits': len(door_hits), 'cable_hits': len(cable_hits)}}
json.dump(info, open(os.path.join(ART, 'gpu-parts.json'), 'w'), indent=1)
print('GPU size', info['size_m'])

# ================================================================ CC0 photo materials (Art/GPU/Textures/SOURCES.md)
TEX = os.path.join(ART, 'Textures')
if not DO_WIRE:
    K.uv_cube()
    K.photo(M_CANOPY, TEX, 'Plastic013A', tile=1.2, tint=(0.97, 0.97, 0.96), metal=0.0, rough_mul=0.9)
    K.photo(M_GALV, TEX, 'Metal011', tile=0.8, tint=(0.95, 0.97, 1.0))
    K.photo(M_STEEL, TEX, 'Metal027', tile=0.6)
    K.photo(M_DARK, TEX, 'Metal027', tile=0.6, tint=(0.25, 0.25, 0.27))
    K.photo(M_ENGINE, TEX, 'Paint004', tile=1.0, tint=(0.16, 0.17, 0.19), metal=0.2)
    K.photo(M_RUB, TEX, 'Rubber004', tile=0.5, tint=(0.35, 0.35, 0.35), metal=0.0)
    K.photo(M_CABLE, TEX, 'Rubber004', tile=0.25, tint=(0.22, 0.22, 0.22), metal=0.0, rough_mul=0.8)
    K.photo(M_PLAST, TEX, 'Plastic006', tile=0.4, tint=(0.35, 0.35, 0.37), metal=0.0)
    K.photo(M_REDP, TEX, 'Paint004', tile=0.8, tint=(1.0, 0.06, 0.04), metal=0.0)
    K.photo(M_YELP, TEX, 'Paint004', tile=0.8, tint=(1.0, 0.62, 0.02), metal=0.0)
    M_TREAD = K.photo(bpy.data.materials.new('GPU_TrayFloor'), TEX, 'DiamondPlate008A', tile=0.5)
    M_CHEV = K.photo(bpy.data.materials.new('GPU_Chevrons'), TEX, 'PaintedMetal016', tile=0.9)
    bpy.data.objects['BumperRear'].data.materials[0] = M_CHEV

ZF = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, os.path.join(ART, 'GPU.blend'), os.path.join(ART, 'gpu.glb'), ZF, 'GPU', 'GPU') if not (DO_WIRE or DO_BEAUTY) else 'skipped'
print('GPU: %d parts, %d IK/pivot points, %d faces, flicker: %s' % (len(ALL), len(IKS), sum(len(o.data.polygons) for o in ALL), last))
if not (DO_WIRE or DO_BEAUTY):
    K.checks('GPU after flicker fix', ground_ok=('Tyre', 'Drawbar_ParkFoot'), overhang_skip=SKIP_OH, overhang_skip_prefix=OH_PREFIX)

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
    cd = bpy.data.cameras.new('BCam'); bc = bpy.data.objects.new('BCam', cd); sc.collection.objects.link(bc); sc.camera = bc
    def bshot(name, loc, at, lens=45, res=(2400, 1500)):
        bc.location = loc; bc.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens
        sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(PICS, name + '.png'); bpy.ops.render.render(write_still=True); print('GPU: picture', name)
    # the final renders: a slightly odd angle, low, front and back, 4K
    bshot('GPU-Final-Front', (4.6, -3.3, 1.35), (0.35, -0.05, 0.85), 35, (3840, 2160))
    bshot('GPU-Final-Back', (-4.4, 3.2, 1.6), (-0.1, 0.05, 0.85), 35, (3840, 2160))
    bshot('GPU-Close-Plug', (2.3, -1.6, 1.45), (1.05, -0.40, 0.95), 40)
    bshot('GPU-Close-Panel', (-2.4, -0.9, 1.5), (-1.05, 0.0, 1.10), 40)

if DO_WIRE:
    shoot = K.wire_setup()
    AT = (0.4, 0.0, 0.85)
    W = lambda n: os.path.join(WIRE, n + '.png')
    shoot(W('1-three-quarter-front'), (5.2, -4.6, 2.9), AT, 45)
    shoot(W('2-three-quarter-rear'), (-5.0, 4.6, 2.9), AT, 45)
    shoot(W('3-side-left'), (0.5, 12, 0.9), (0.5, 0, 0.9), 50, 3.9, res=(2400, 1300))
    shoot(W('4-side-right'), (0.5, -12, 0.9), (0.5, 0, 0.9), 50, 3.9, res=(2400, 1300))
    shoot(W('5-front'), (12, 0, 0.9), (0, 0, 0.9), 50, 2.2, res=(1500, 1200))
    shoot(W('6-rear'), (-12, 0, 0.9), (0, 0, 0.9), 50, 2.2, res=(1500, 1200))
    shoot(W('7-top'), (0.5, 0, 14), (0.5, 0, 0), 50, 3.9, res=(2400, 1300))
    shoot(W('8-panel-and-doors-open'), (-2.6, -1.4, 1.7), (-1.0, 0.0, 1.1), 35, hide=('DoorPanel', 'DoorPanel_Window', 'DoorPanel_Handle', 'DoorPanel_HandleBase', 'DoorPanel_Lock', 'DoorPanel_HingeLeaf_0', 'DoorPanel_HingeLeaf_1'))
    shoot(W('9-plug-and-cable'), (2.4, -1.8, 1.6), (1.0, -0.45, 0.9), 35)
    shoot(W('10-engine-bay'), (-0.1, 2.4, 1.6), (-0.1, 0.3, 1.2), 35, hide=tuple(o.name for o in ALL if o.name.startswith('DoorEngineL')))
    shoot(W('11-generator-bay'), (-0.1, -2.4, 1.6), (-0.1, -0.3, 1.2), 35, hide=tuple(o.name for o in ALL if o.name.startswith('DoorGenR')))
    shoot(W('12-underside'), (0.6, -3.0, -0.9), (0.4, 0, 0.35), 35)
