# The taxi bus (minibus taxi, "maxi taxi") that waits at the landside kerb outside the terminal with the buses - our own
# design in the class of the long high-roof passenger vans (5,900-5,930 long, ~2,000 wide, ~2,300-2,400 high, wheelbase
# ~3,660, 235/65 R16C tyres), 8 passengers + driver (the most a taxi may carry on a car licence), a sliding door on the
# kerb (right) side, two rear doors with a folding wheelchair ramp, the taxi sign on the roof. No brand names or logos.
# Built to the vehicle standard (Tools/vehicle_kit.py): bevelled body, lamps set into their faces, nothing floats, IK
# points for the driver and every passenger; the full interior (seats with belts and headrests, dashboard with the
# taximeter and card terminal, lining, lights, grab handles, luggage area, wheelchair tracks).
# Axes: x forward, y left, z up; metres; origin on the ground under the middle. Driver front left (right-hand traffic).
#   python3 Tools/make_taxi_bus.py [--wire] [--only=view,view]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import importlib, vehicle_kit as K
importlib.reload(K)

# #region ── Blender 4.1+ compatibility for the shared kit ──
def _finish(o, m, smooth=40):
    o.data.materials.clear(); o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    if hasattr(o.data, 'set_sharp_from_angle'): o.data.set_sharp_from_angle(angle=math.radians(smooth))
    else: o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(smooth)
    K.ALL.append(o); return o
K.finish = _finish
def pmat(name, col, metal=0.0, rough=0.5, emit=0.0, alpha=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission Color' if 'Emission Color' in b.inputs else 'Emission'].default_value = (*col, 1)
        b.inputs['Emission Strength'].default_value = emit
    if alpha < 1: b.inputs['Alpha'].default_value = alpha
    return m
_run = K.subprocess.run
def _local_run(args, **kw):
    if isinstance(args, list) and str(args[0]).startswith('C:/'): args = [sys.executable if 'python' in sys.executable else 'python3'] + args[1:]
    return _run(args, **kw)
K.subprocess.run = _local_run
# #endregion

from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, lamp, ALL, IKS
ART = os.path.join(ROOT, 'Art', 'TaxiBus'); os.makedirs(ART, exist_ok=True)
BP = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'TaxiBus')
WIRE = os.path.join(BP, 'wire'); os.makedirs(WIRE, exist_ok=True)
DO_WIRE = '--wire' in sys.argv
K.reset()
IKP = {}
def ik(name, loc, part=None):
    IKP['IK_' + name] = part; return K.ik(name, loc)
def piv(name, loc, part=None):
    IKP['PIV_' + name] = part; return K.ik(name, loc, prefix='PIV_')

# #region ── The main sizes (m) ──
L2, W2 = 2.965, 1.01                       # 5,930 x 2,020
FX = L2 - 1.00; RX = FX - 3.665            # front overhang 1,000, wheelbase 3,665, rear overhang 1,265
TD, TW, RB = 0.711, 0.235, 0.2032          # 235/65 R16C
TZ = TD / 2
TR = 0.86                                  # wheel centre y (track 1,720)
ARCH = 0.42
Z0 = 0.40                                  # body bottom (sills)
ROOFZ = 2.40                               # roof top (high roof)
FLOOR = 0.58                               # cabin floor top
WIN0, WIN1 = 1.38, 2.18                    # side window band
WALL = 0.04
# side outline (x, z): rear, roof, windscreen, bonnet, nose, front face
# modern one-box front (owner 2026-11-23): a short sloping bonnet runs into a strongly raked windscreen, rounded nose
OUT = [(-L2, Z0 + 0.02), (-L2, ROOFZ - 0.06), (-L2 + 0.06, ROOFZ), (0.95, ROOFZ), (1.12, 2.35), (2.10, 1.22),
       (2.62, 1.02), (2.86, 0.88), (2.95, 0.74), (L2, 0.55), (L2 - 0.03, 0.45), (L2 - 0.10, Z0)]
INN = [(-L2 + WALL, Z0 + WALL), (-L2 + WALL, ROOFZ - WALL), (0.92, ROOFZ - WALL), (1.07, 2.31), (2.06, 1.20), (2.06, Z0 + WALL)]
def apx(z): return 2.10 - (z - 1.22) * (0.98 / 1.13)     # the windscreen / A-pillar line
# #endregion

# #region ── Materials ──
M_PAINT = pmat('TB_BodyPaint', (0.95, 0.80, 0.10), 0.2, 0.3)        # taxi yellow
M_CHECK = pmat('TB_CheckerBand', (0.05, 0.05, 0.06), 0.1, 0.4)
M_DARK = pmat('TB_DarkTrim', (0.06, 0.06, 0.07), 0.2, 0.5)
M_GLASS = pmat('TB_Glass', (0.12, 0.16, 0.18), 0.0, 0.05, 0, 0.35)
M_PRIV = pmat('TB_PrivacyGlass', (0.04, 0.05, 0.06), 0.0, 0.05, 0, 0.6)
M_STEEL = pmat('TB_Steel', (0.55, 0.56, 0.58), 1.0, 0.35)
M_CHROME = pmat('TB_Chrome', (0.85, 0.86, 0.88), 1.0, 0.12)
M_RUB = pmat('TB_Tyre', (0.03, 0.03, 0.03), 0.0, 0.9)
M_PLAST = pmat('TB_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_GREY = pmat('TB_GreyPlastic', (0.30, 0.31, 0.33), 0.0, 0.6)
M_FLOOR = pmat('TB_FloorRubber', (0.10, 0.10, 0.11), 0.0, 0.85)
M_SEAT = pmat('TB_SeatFabric', (0.12, 0.13, 0.16), 0.0, 0.85)
M_LEATHER = pmat('TB_SeatBolster', (0.06, 0.06, 0.07), 0.0, 0.6)
M_LINING = pmat('TB_Headliner', (0.62, 0.63, 0.64), 0.0, 0.85)
M_PANEL = pmat('TB_TrimPanel', (0.22, 0.23, 0.25), 0.0, 0.7)
M_BELT = pmat('TB_SeatBelt', (0.08, 0.08, 0.09), 0.0, 0.7)
M_AMBER = pmat('TB_AmberLens', (1.0, 0.55, 0.05), 0.0, 0.2, 2.0)
M_RED = pmat('TB_RedLens', (0.9, 0.05, 0.03), 0.0, 0.2, 1.0)
M_WHITE = pmat('TB_WhiteLens', (0.95, 0.95, 0.9), 0.0, 0.1, 1.5)
M_LED = pmat('TB_LED', (0.95, 0.96, 1.0), 0.0, 0.2, 2.0)
M_SIGN = pmat('TB_TaxiSign', (1.0, 0.92, 0.55), 0.0, 0.2, 2.5)
M_SCREEN = pmat('TB_Screen', (0.05, 0.25, 0.35), 0.0, 0.1, 0.8)
M_METER = pmat('TB_MeterRed', (1.0, 0.15, 0.08), 0.0, 0.2, 1.5)
M_MIRROR = pmat('TB_MirrorGlass', (0.8, 0.85, 0.9), 1.0, 0.02)
M_PLATE = pmat('TB_PlateBlank', (0.92, 0.92, 0.92), 0.0, 0.4)
M_YEL = pmat('TB_YellowMark', (0.95, 0.75, 0.05), 0.0, 0.45)
# #endregion

# #region ── Wheels: 235/65 R16C tyres, steel rims with covers, 6 nuts; front steering knuckles, rear beam axle ──
TPROF = [(0.290, -0.124), (0.318, -0.146), (0.360, -0.157), (0.410, -0.1585), (0.460, -0.154), (0.500, -0.142), (0.522, -0.124),
         (0.534, -0.100), (0.538, -0.060), (0.539, 0.0), (0.538, 0.060), (0.534, 0.100), (0.522, 0.124), (0.500, 0.142),
         (0.460, 0.154), (0.410, 0.1585), (0.360, 0.157), (0.318, 0.146), (0.290, 0.124), (0.286, 0.0)]
def wheel(key, x, y, s):
    lathe(f'Tyre_{key}', [(RB + (r - 0.29) * ((TZ - RB) / 0.249), yy * (TW / 2 / 0.1585)) for r, yy in TPROF], (x, y, TZ), M_RUB, 72)
    cyl(f'Rim_{key}', (x, y, TZ), RB + 0.012, TW - 0.04, 'y', M_STEEL, 64, bevel=0.006)
    cv = cyl(f'WheelCover_{key}', (x, y + s * (TW / 2 - 0.02 + 0.012), TZ), RB - 0.005, 0.024, 'y', M_GREY, 64, bevel=0.008)
    for j in range(10):                                   # the cover's spokes are slots
        a = j * 2 * math.pi / 10
        cut(cv, box('Slot', (x + 0.13 * math.cos(a), y + s * (TW / 2 + 0.0), TZ + 0.13 * math.sin(a)), (0.05, 0.05, 0.022), M_DARK))
        turn(bpy.data.objects[f'WheelCover_{key}'], (x, y, TZ), 'y', 0) if False else None
    cyl(f'HubCap_{key}', (x, y + s * (TW / 2 + 0.006), TZ), 0.055, 0.02, 'y', M_CHROME, 32, bevel=0.006)
    for j in range(6):
        a = j * 2 * math.pi / 6
        cyl(f'Nut_{key}{j}', (x + 0.085 * math.cos(a), y + s * (TW / 2 + 0.004), TZ + 0.085 * math.sin(a)), 0.012, 0.022, 'y', M_CHROME, 6)
for s, sd in ((1, 'L'), (-1, 'R')):
    wheel('F' + sd, FX, s * TR, s); wheel('R' + sd, RX, s * TR, s)
    ik('Wheel_F' + sd, (FX, s * TR, TZ), 'Tyre_F' + sd); ik('Wheel_R' + sd, (RX, s * TR, TZ), 'Tyre_R' + sd)
    cyl(f'Knuckle_{sd}', (FX, s * (TR - TW / 2 - 0.03), TZ), 0.06, 0.06, 'y', M_DARK, 24, bevel=0.008)
    cyl(f'Strut_{sd}', (FX - 0.02, s * (TR - TW / 2 - 0.06), TZ + 0.30), 0.035, 0.56, 'z', M_DARK, 20)
    cyl(f'Spring_{sd}', (FX - 0.02, s * (TR - TW / 2 - 0.06), TZ + 0.42), 0.065, 0.26, 'z', M_STEEL, 24)
    piv('Steer_F' + sd, (FX, s * (TR - TW / 2 - 0.03), TZ), 'Knuckle_' + sd)
    box(f'LeafSpring_{sd}', (RX, s * (TR - 0.20), TZ + 0.08), (1.20, 0.07, 0.04), M_DARK, bevel=0.01)
    cyl(f'Damper_{sd}', (RX - 0.18, s * (TR - 0.27), TZ + 0.06), 0.025, 0.30, 'z', M_DARK, 16)
cyl('RearAxle', (RX, 0, TZ), 0.05, 2 * (TR - TW / 2 + 0.02), 'y', M_DARK, 24)
box('RearDiff', (RX, 0, TZ), (0.24, 0.28, 0.24), M_DARK, bevel=0.03, segs=2)
box('DriveShaftTunnelGuard', (0.0, 0, Z0 - 0.06), (3.20, 0.24, 0.10), M_DARK, bevel=0.02) if False else None
cyl('DriveShaft', ((RX + FX - 0.4) / 2, 0, TZ + 0.02), 0.035, abs(FX - 0.4 - RX) - 0.12, 'x', M_STEEL, 20)
box('EngineSump', (FX - 0.10, 0, Z0 - 0.05), (0.60, 0.50, 0.12), M_DARK, bevel=0.02)
box('BatteryTray', (-0.40, 0.0, Z0 - 0.06), (1.60, 0.90, 0.14), M_DARK, bevel=0.02)      # traction battery under the floor
box('ExhaustOrCoolerBox', (-2.35, -0.55, Z0 - 0.03), (0.40, 0.30, 0.10), M_DARK, bevel=0.02)
# #endregion

# #region ── Body shell: the outline pushed across, hollowed, wheel arches, door and window openings ──
shell = prism('BodyShell', OUT, -W2, W2, M_PAINT, bevel=0.07, segs=3)
cut(shell, prism('Hollow', INN, -W2 + WALL, W2 - WALL, M_DARK))
for ax in (FX, RX):
    for s in (1, -1):
        cut(shell, cyl('ArchCut', (ax, s * 0.86, TZ), ARCH, 0.42, 'y', M_DARK, 64))
# fixed side windows (left: all three; right: behind the sliding door) and the rear quarter
SIDEWIN = {1: [(-0.30, 0.80), (-1.55, -0.42), (-2.78, -1.67)], -1: [(-1.55, -0.50), (-2.78, -1.67)]}
for s, wins in SIDEWIN.items():
    for i, (a, b) in enumerate(wins):
        cut(shell, box('WinCut', ((a + b) / 2, s * (W2 - 0.02), (WIN0 + WIN1) / 2), (b - a, 0.2, WIN1 - WIN0), M_DARK))
# front door openings (both sides) and the sliding door (right)
ZP = 1.22 + (2.10 - 1.87) * 1.13 / 0.98            # where the door's front edge meets the A-pillar
FDOOR = [(0.98, Z0 + 0.06), (1.52, Z0 + 0.06), (1.52, 0.82), (1.84, 0.82), (1.84, ZP), (apx(2.24) - 0.03, 2.24), (0.98, 2.24)]
SD0, SD1, SDZ1 = -0.38, 0.82, 2.24
for s in (1, -1):
    y0, y1 = (W2 - 0.08, W2 + 0.1) if s > 0 else (-W2 - 0.1, -W2 + 0.08)
    cut(shell, prism('DoorCut', FDOOR, y0, y1, M_DARK))
cut(shell, box('SlideCut', ((SD0 + SD1) / 2, -W2, (Z0 + 0.06 + SDZ1) / 2), (SD1 - SD0, 0.2, SDZ1 - Z0 - 0.06), M_DARK))
# windscreen opening (the A-pillars stay) and the rear doors' opening
WS = [(apx(1.40) - 0.10, 1.40), (apx(1.40) + 0.06, 1.40), (apx(2.20) + 0.06, 2.20), (apx(2.20) - 0.10, 2.20)]
cut(shell, prism('WSCut', WS, -0.88, 0.88, M_DARK))
cut(shell, box('RearCut', (-L2, 0, (Z0 + 0.08 + 2.26) / 2), (0.2, 1.80, 2.26 - Z0 - 0.08), M_DARK))
# arch liners (inside the arches), arch lips
for ax, nm in ((FX, 'F'), (RX, 'R')):
    for s, sd in ((1, 'L'), (-1, 'R')):
        ln = cyl(f'ArchLiner_{nm}{sd}', (ax, s * 0.84, TZ), ARCH + 0.005, 0.30, 'y', M_PLAST, 64)
        cut(ln, cyl('LinerIn', (ax, s * 0.84, TZ), ARCH - 0.02, 0.4, 'y', M_DARK, 64))
        cut(ln, box('LinerLow', (ax, s * 0.84, Z0 - 0.5), (1.2, 0.5, 1.0), M_DARK))
        lip = torus(f'ArchLip_{nm}{sd}', (ax, s * (W2 - 0.005), TZ), ARCH + 0.01, 0.02, 'y', M_PLAST, 64, 10)
        cut(lip, box('LipLow', (ax, s * W2, Z0 - 0.5), (1.2, 0.3, 1.0), M_DARK))
        box(f'MudFlap_{nm}{sd}', (ax - ARCH - 0.02, s * (W2 - 0.12), Z0 - 0.10), (0.012, 0.20, 0.22), M_PLAST, bevel=0.004)
# #endregion

# #region ── Doors: two hinged front doors, the sliding door on its three tracks, two rear doors with windows ──
def shrink(pts, d):
    cx = sum(p[0] for p in pts) / len(pts); cz = sum(p[1] for p in pts) / len(pts)
    out = []
    for x, z in pts:
        vx, vz = cx - x, cz - z; n = math.hypot(vx, vz) or 1
        out.append((x + vx / n * d, z + vz / n * d))
    return out
ZW = 1.22 + (2.10 - 1.86) * 1.13 / 0.98
FDW = [(1.04, WIN0), (1.78, WIN0), (1.78, ZW), (apx(2.18) - 0.06, 2.18), (1.04, 2.18)]
for s, sd in ((1, 'L'), (-1, 'R')):
    yo = s * W2; yi = s * (W2 - WALL)
    dr = prism(f'FrontDoor_{sd}', shrink(FDOOR, 0.006), min(yo, yi), max(yo, yi), M_PAINT, bevel=0.01)
    cut(dr, prism('FDWin', FDW, -W2 - 0.1, W2 + 0.1, M_DARK))
    prism(f'FrontDoorGlass_{sd}', shrink(FDW, -0.004), s * (W2 - 0.026), s * (W2 - 0.014), M_GLASS)
    box(f'FrontDoorHandle_{sd}', (1.10, s * (W2 + 0.006), 1.18), (0.18, 0.014, 0.04), M_PLAST, bevel=0.006)
    box(f'FrontDoorTrim_{sd}', (1.38, s * (W2 - WALL - 0.008), 0.95), (0.70, 0.016, 0.60), M_PANEL, bevel=0.006)
    box(f'FrontDoorArmrest_{sd}', (1.30, s * (W2 - WALL - 0.04), 1.08), (0.36, 0.05, 0.05), M_PANEL, bevel=0.01)
    box(f'FrontDoorPocket_{sd}', (1.42, s * (W2 - WALL - 0.035), 0.70), (0.50, 0.04, 0.12), M_PANEL, bevel=0.008)
    for z in (0.75, 1.20):
        cyl(f'DoorHinge_{sd}{z}', (1.86, s * (W2 - 0.01), z), 0.018, 0.10, 'z', M_DARK, 16)
    piv(f'FrontDoor_{sd}', (1.86, s * (W2 - 0.01), 0.75), f'FrontDoor_{sd}')
    ik(f'FrontDoorHandleOut_{sd}', (1.10, s * (W2 + 0.02), 1.18), f'FrontDoorHandle_{sd}')
    # door mirror: arm, housing, glass, indicator repeater
    box(f'MirrorArm_{sd}', (1.74, s * (W2 + 0.07), 1.44), (0.10, 0.16, 0.06), M_PLAST, bevel=0.015)
    box(f'MirrorHousing_{sd}', (1.72, s * (W2 + 0.24), 1.52), (0.12, 0.26, 0.34), M_PLAST, bevel=0.03, segs=2)
    box(f'MirrorGlass_{sd}', (1.659, s * (W2 + 0.24), 1.52), (0.004, 0.22, 0.30), M_MIRROR)
    box(f'MirrorRepeater_{sd}', (1.75, s * (W2 + 0.24), 1.355), (0.05, 0.16, 0.012), M_AMBER)
# sliding door (right) with its window, handle, step, and the tracks (top, waist, bottom)
sdw = box('SlidingDoor', ((SD0 + SD1) / 2, -(W2 - WALL / 2), (Z0 + 0.065 + SDZ1) / 2), (SD1 - SD0 - 0.012, WALL, SDZ1 - Z0 - 0.071), M_PAINT, bevel=0.01)
cut(sdw, box('SDWin', ((SD0 + SD1) / 2 + 0.02, -W2, (WIN0 + WIN1) / 2), (SD1 - SD0 - 0.16, 0.2, WIN1 - WIN0), M_DARK))
box('SlidingDoorGlass', ((SD0 + SD1) / 2 + 0.02, -(W2 - 0.02), (WIN0 + WIN1) / 2), (SD1 - SD0 - 0.15, 0.012, WIN1 - WIN0 + 0.01), M_PRIV)
box('SlidingDoorHandle', (SD1 - 0.12, -(W2 + 0.006), 1.18), (0.04, 0.014, 0.20), M_PLAST, bevel=0.006)
box('SlidingDoorTrim', ((SD0 + SD1) / 2, -(W2 - WALL - 0.008), 0.95), (SD1 - SD0 - 0.1, 0.016, 0.60), M_PANEL, bevel=0.006)
box('SlidingDoorGrabInside', (SD1 - 0.12, -(W2 - WALL - 0.03), 1.15), (0.04, 0.04, 0.30), M_PANEL, bevel=0.01)
box('SlideTrackWaist', ((-2.20 + SD0) / 2, -(W2 + 0.004), 1.30), (SD0 + 2.20, 0.012, 0.035), M_DARK, bevel=0.004)
box('SlideTrackTop', ((SD0 + SD1) / 2 - 0.4, -(W2 - WALL - 0.02), SDZ1 + 0.03), (SD1 - SD0 + 0.8, 0.04, 0.03), M_DARK)
box('SlideTrackBottom', ((SD0 + SD1) / 2 - 0.4, -(W2 - WALL - 0.02), FLOOR - 0.03), (SD1 - SD0 + 0.8, 0.04, 0.03), M_DARK)
box('EntryStep', ((SD0 + SD1) / 2, -(W2 - 0.10), Z0 - 0.04), (SD1 - SD0 - 0.10, 0.28, 0.06), M_DARK, bevel=0.01)
box('EntryStepEdge', ((SD0 + SD1) / 2, -(W2 + 0.035), Z0 - 0.04), (SD1 - SD0 - 0.12, 0.03, 0.062), M_YEL)
box('EntryStepWell', ((SD0 + SD1) / 2, -(W2 - 0.20), (Z0 + FLOOR) / 2), (SD1 - SD0 - 0.12, 0.30, FLOOR - Z0), M_FLOOR)   # inner step block
piv('SlidingDoor', (SD1, -(W2 - 0.02), 1.30), 'SlidingDoor'); ik('SlidingDoorHandleOut', (SD1 - 0.12, -(W2 + 0.02), 1.18), 'SlidingDoorHandle')
ik('EntryStep', ((SD0 + SD1) / 2, -(W2 - 0.10), Z0 - 0.01), 'EntryStep')
# rear doors (split 50/50), windows, handle, hinges, rear wiper on the left door
for s, sd in ((1, 'L'), (-1, 'R')):
    yc = s * 0.45
    rd = box(f'RearDoor_{sd}', (-L2 + WALL / 2, yc, (Z0 + 0.085 + 2.255) / 2), (WALL, 0.892, 2.255 - Z0 - 0.09), M_PAINT, bevel=0.01)
    cut(rd, box('RDWin', (-L2, yc, 1.80), (0.2, 0.70, 0.66), M_DARK))
    box(f'RearDoorGlass_{sd}', (-L2 + 0.02, yc, 1.80), (0.012, 0.71, 0.67), M_PRIV)
    for z in (0.75, 1.40, 2.05):
        cyl(f'RearHinge_{sd}{z}', (-L2 - 0.012, s * 0.90, z), 0.016, 0.09, 'z', M_DARK, 16)
    piv(f'RearDoor_{sd}', (-L2 - 0.012, s * 0.90, 0.75), f'RearDoor_{sd}')
    box(f'RearDoorTrim_{sd}', (-L2 + WALL + 0.008, yc, 0.95), (0.016, 0.80, 0.70), M_PANEL, bevel=0.006)
box('RearDoorHandle', (-L2 - 0.006, -0.08, 1.10), (0.014, 0.16, 0.04), M_PLAST, bevel=0.006)
cyl('RearWiperPivot', (-L2 - 0.01, 0.45, 2.17), 0.02, 0.03, 'x', M_DARK, 16)
wa = box('RearWiperArm', (-L2 - 0.02, 0.45, 1.86), (0.012, 0.02, 0.60), M_PLAST); turn(wa, (-L2 - 0.02, 0.45, 2.17), 'x', 35)
box('RearCamera', (-L2 - 0.012, 0.0, 2.24), (0.024, 0.06, 0.03), M_PLAST, bevel=0.005)
# #endregion

# #region ── Glazing, pillars, the windscreen, side trim, the checker band ──
for s, wins in SIDEWIN.items():
    for i, (a, b) in enumerate(wins):
        box(f'SideGlass_{"L" if s > 0 else "R"}{i}', ((a + b) / 2, s * (W2 - 0.02), (WIN0 + WIN1) / 2), (b - a + 0.01, 0.012, WIN1 - WIN0 + 0.01), M_PRIV)
WSG = [(apx(1.36) - 0.022, 1.36), (apx(1.36) - 0.008, 1.36), (apx(2.24) - 0.008, 2.24), (apx(2.24) - 0.022, 2.24)]
prism('Windscreen', WSG, -0.90, 0.90, M_GLASS)
for s, sd in ((1, 'L'), (-1, 'R')):
    rs = box(f'RubbingStrip_{sd}', (-0.70, s * (W2 + 0.006), 0.62), (3.80, 0.014, 0.08), M_PLAST, bevel=0.006)
    cut(rs, cyl('StripArch', (RX, s * W2, TZ), ARCH + 0.03, 0.2, 'y', M_DARK, 48))
    box(f'CheckerBand_{sd}', (-0.75, s * (W2 + 0.002), 1.24), (4.30, 0.006, 0.10), M_CHECK)
    box(f'SillGuard_{sd}', (-0.30, s * (W2 - 0.01), Z0 + 0.03), (2.80, 0.03, 0.06), M_PLAST) if s > 0 else None
    lamp(f'SideMarker_{sd}', (-0.10 if s > 0 else -1.0, s * W2, 0.52), 'y' + ('+' if s > 0 else '-'), 0.07, 0.03, M_AMBER, M_DARK, M_PLAST, depth=0.025)
    lamp(f'FrontRepeater_{sd}', (2.30, s * W2, 0.95), 'y' + ('+' if s > 0 else '-'), 0.06, 0.03, M_AMBER, M_DARK, M_PLAST, depth=0.025)
box('ChargeFlap', (2.20, 1.0, 1.02), (0.16, 0.024, 0.12), M_PAINT, bevel=0.006) if False else None
# #endregion

# #region ── Front: swept headlamps with indicator strips, slim upper grille, rounded bumper with plate and lower intake ──
def nose(z):                                           # x of the nose surface at height z (between 0.74 and 1.02)
    if z >= 0.88: return 2.62 + (1.02 - z) / 0.14 * 0.24
    return 2.86 + (0.88 - z) / 0.14 * 0.09
def on_nose(z0, z1, out, inn=0.03, n=4):              # an outline lying on the nose surface, out m proud of it
    zs = [z1 + (z0 - z1) * i / n for i in range(n + 1)]
    return [(nose(z) + out, z) for z in zs] + [(nose(z) - inn, z) for z in reversed(zs)]
for s_, sd in ((1, 'L'), (-1, 'R')):
    ya, yb = sorted((s_ * 0.46, s_ * 0.95))
    prism(f'HeadlampHousing_{sd}', on_nose(0.79, 1.00, 0.003), ya, yb, M_DARK, bevel=0.006)
    ya2, yb2 = sorted((s_ * 0.50, s_ * 0.80))
    prism(f'Headlamp_{sd}', on_nose(0.83, 0.97, 0.007, 0.02), ya2, yb2, M_WHITE)
    ya3, yb3 = sorted((s_ * 0.82, s_ * 0.92))
    prism(f'IndicatorF_{sd}', on_nose(0.81, 0.95, 0.007, 0.02), ya3, yb3, M_AMBER)
    ya4, yb4 = sorted((s_ * 0.50, s_ * 0.80))
    prism(f'DRLStrip_{sd}', on_nose(0.805, 0.82, 0.008, 0.02, 1), ya4, yb4, M_LED)
prism('UpperGrille', on_nose(0.80, 0.90, 0.004), -0.40, 0.40, M_DARK, bevel=0.004)
for j, z in enumerate((0.83, 0.87)):
    prism(f'UpperGrilleBar_{j}', on_nose(z - 0.008, z + 0.008, 0.008, 0.01, 1), -0.38, 0.38, M_PLAST)
BUMP = [(L2 - 0.06, 0.40), (L2 + 0.05, 0.41), (L2 + 0.08, 0.44), (L2 + 0.08, 0.62), (L2 + 0.03, 0.68), (L2 - 0.06, 0.70)]
bp = prism('FrontBumper', BUMP, -0.985, 0.985, M_PAINT, bevel=0.03, segs=3)
for s_ in (1, -1):                                     # the bumper's corners swept back round the wheels
    cut(bp, cyl('BumperCorner', (L2 - 0.42, s_ * 0.985, 0.55), 0.42, 0.5, 'z', M_DARK, 48)) if False else None
box('LowerIntake', (L2 + 0.083, 0, 0.475), (0.012, 1.20, 0.05), M_DARK, bevel=0.004)
for j in range(5):
    box(f'LowerIntakeBar_{j}', (L2 + 0.089, -0.48 + j * 0.24, 0.475), (0.006, 0.02, 0.046), M_PLAST)
box('PlateFront', (L2 + 0.085, 0, 0.565), (0.01, 0.52, 0.11), M_PLATE, bevel=0.003)
for s_, sd in ((1, 'L'), (-1, 'R')):
    lamp(f'FogLamp_{sd}', (L2 + 0.08, s_ * 0.62, 0.50), 'x+', 0.12, 0.05, M_WHITE, M_DARK, M_PLAST, depth=0.04)
cyl('TowCover_R', (L2 + 0.081, -0.40, 0.60), 0.025, 0.012, 'x', M_PLAST, 20)
# the front corners rounded in plan (radius 350): the nose, bumper and lamps wrap round to the sides
def zprism(name, pts_xy, z0, z1, m):
    bm = bmesh.new()
    a = [bm.verts.new((x, y, z0)) for x, y in pts_xy]; b = [bm.verts.new((x, y, z1)) for x, y in pts_xy]
    bm.faces.new(a[::-1]); bm.faces.new(b)
    for i in range(len(pts_xy)): bm.faces.new((a[i], a[(i + 1) % len(pts_xy)], b[(i + 1) % len(pts_xy)], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return K.finish(K.link(name, bm), m, 30)
RC = 0.35
ROUND = ['BodyShell', 'FrontBumper', 'HeadlampHousing_L', 'HeadlampHousing_R', 'Headlamp_L', 'Headlamp_R', 'IndicatorF_L', 'IndicatorF_R',
         'DRLStrip_L', 'DRLStrip_R']
for s_ in (1, -1):
    cx, cy = L2 - RC, s_ * (W2 - RC)
    arc = [(cx + RC * math.cos(math.radians(a)), cy + s_ * RC * math.sin(math.radians(a))) for a in range(0, 91, 6)]
    pts = [(cx, s_ * (W2 + 0.3)), (L2 + 0.3, s_ * (W2 + 0.3)), (L2 + 0.3, cy)] + arc
    if s_ < 0: pts = pts[::-1]
    for nm in ROUND:
        o = bpy.data.objects.get(nm)
        if o and (o.location.y * s_ >= 0 or nm in ('BodyShell', 'FrontBumper')):
            cut(o, zprism('CornerCut', pts, -0.2, 1.45, M_DARK))
# cowl and wipers at the windscreen foot
WZ = 1.215
box('CowlPanel', (apx(WZ) + 0.04, 0, WZ), (0.10, 1.80, 0.03), M_PLAST, bevel=0.008)
for s_, sd in ((1, 'L'), (-1, 'R')):
    px_, pz_ = apx(WZ) + 0.04, WZ + 0.02
    cyl(f'WiperPivot_{sd}', (px_, s_ * 0.42 - 0.18, pz_), 0.02, 0.05, 'z', M_DARK, 16)
    box(f'WiperArm_{sd}', (px_ - 0.02, s_ * 0.42 - 0.18 + 0.30, pz_ + 0.02), (0.02, 0.60, 0.015), M_PLAST)
    box(f'WiperBlade_{sd}', (px_ - 0.03, s_ * 0.42 - 0.18 + 0.35, pz_ + 0.031), (0.02, 0.62, 0.012), M_DARK)
    piv(f'Wiper_{sd}', (px_, s_ * 0.42 - 0.18, pz_), f'WiperPivot_{sd}')
# #endregion

# #region ── Rear: tail lamp columns, third brake lamp, bumper with step, plate, tow bar ──
for s, sd in ((1, 'L'), (-1, 'R')):
    lamp(f'TailBrake_{sd}', (-L2, s * 0.955, 1.20), 'x-', 0.07, 0.26, M_RED, M_DARK, M_PLAST, depth=0.05)
    lamp(f'IndicatorR_{sd}', (-L2, s * 0.955, 0.98), 'x-', 0.07, 0.12, M_AMBER, M_DARK, M_PLAST, depth=0.05)
    lamp(f'Reverse_{sd}', (-L2, s * 0.955, 0.84), 'x-', 0.07, 0.10, M_WHITE, M_DARK, M_PLAST, depth=0.05)
lamp('ThirdBrake', (-L2, 0.0, 2.33), 'x-', 0.40, 0.035, M_RED, M_DARK, M_PLAST, depth=0.03)
box('RearBumper', (-L2 - 0.04, 0, 0.52), (0.10, 1.96, 0.22), M_PLAST, bevel=0.035, segs=3)
box('RearStepPlate', (-L2 - 0.10, 0, 0.58), (0.10, 0.90, 0.025), M_DARK, bevel=0.006)
box('PlateRear', (-L2 - 0.095, 0, 0.47), (0.01, 0.52, 0.11), M_PLATE, bevel=0.003)
lamp('PlateLight', (-L2 - 0.09, 0, 0.545), 'x-', 0.10, 0.015, M_WHITE, M_DARK, M_PLAST, depth=0.02) if False else None
# #endregion

# #region ── Roof: the taxi sign, antenna fin, rails ──
box('TaxiSignBase', (0.70, 0, ROOFZ + 0.015), (0.32, 0.62, 0.03), M_PLAST, bevel=0.008)
sign = prism('TaxiSign', [(0.56, ROOFZ + 0.03), (0.84, ROOFZ + 0.03), (0.80, ROOFZ + 0.19), (0.60, ROOFZ + 0.19)], -0.30, 0.30, M_SIGN, bevel=0.01)
box('TaxiSignTop', (0.70, 0, ROOFZ + 0.195), (0.21, 0.58, 0.012), M_PLAST)
box('AntennaFin', (-2.40, 0, ROOFZ + 0.04), (0.18, 0.06, 0.08), M_PLAST, bevel=0.025, segs=2)
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'RoofRail_{sd}', (-0.85, s * 0.86, ROOFZ + 0.02), (3.50, 0.04, 0.04), M_PLAST, bevel=0.01)
    for j, x in enumerate((-2.50, -0.80, 0.90)):
        box(f'RoofRailFoot_{sd}{j}', (x, s * 0.86, ROOFZ + 0.004), (0.12, 0.06, 0.012), M_PLAST, bevel=0.003)
# #endregion

# #region ── Cabin shell inside: floor, wheel housings, lining, headliner, lights, vents, grab handles ──
fl = box('FloorBoard', ((-L2 + WALL + 1.88) / 2, 0, (Z0 + WALL + FLOOR) / 2), (1.88 + L2 - WALL - 0.01, 2 * (W2 - WALL) - 0.01, FLOOR - Z0 - WALL), M_DARK)
for ax in (FX, RX):
    for s in (1, -1):
        cut(fl, cyl('FloorArch', (ax, s * 0.84, TZ), ARCH + 0.01, 0.36, 'y', M_DARK, 48))
box('FloorMat', (-0.55, 0, FLOOR + 0.003), (2.40, 1.70, 0.006), M_FLOOR)
for s, sd in ((1, 'L'), (-1, 'R')):
    for j in (0, 1):                                   # wheelchair anchor tracks in the floor
        box(f'FloorTrack_{sd}{j}', (-2.10 if j else -1.10, s * 0.38, FLOOR + 0.004), (0.70 if j else 0.50, 0.035, 0.008), M_STEEL)
    hh = box(f'RearWheelHouse_{sd}', (RX, s * (W2 - WALL - 0.14), (FLOOR + 0.84) / 2), (0.94, 0.28, 0.84 - FLOOR), M_PANEL, bevel=0.03)
    hf = box(f'FrontWheelHouse_{sd}', (FX - 0.12, s * (W2 - WALL - 0.17), (FLOOR + 0.90) / 2), (0.62, 0.34, 0.90 - FLOOR), M_PANEL, bevel=0.03)
    box(f'LowerLining_{sd}', (-0.95, s * (W2 - WALL - 0.008), (FLOOR + WIN0) / 2), (3.80, 0.016, WIN0 - FLOOR), M_PANEL) if s > 0 else \
        box(f'LowerLining_{sd}', (-1.62, s * (W2 - WALL - 0.008), (FLOOR + WIN0) / 2), (2.46, 0.016, WIN0 - FLOOR), M_PANEL)
    box(f'WindowLedge_{sd}', (-0.95 if s > 0 else -1.62, s * (W2 - WALL - 0.03), WIN0 - 0.01), (3.80 if s > 0 else 2.46, 0.06, 0.03), M_PANEL, bevel=0.008)
    box(f'AirDuct_{sd}', (-0.95, s * (W2 - WALL - 0.08), ROOFZ - WALL - 0.08), (3.70, 0.16, 0.14), M_LINING, bevel=0.03)
    for j in range(5):
        box(f'AirVent_{sd}{j}', (-2.30 + j * 0.80, s * (W2 - WALL - 0.16), ROOFZ - WALL - 0.10), (0.10, 0.008, 0.05), M_PLAST)
        lamp(f'ReadingLight_{sd}{j}', (-2.30 + j * 0.80 + 0.2, s * (W2 - WALL - 0.08), ROOFZ - WALL - 0.15), 'x+', 0.05, 0.03, M_LED, M_LINING, M_PLAST, depth=0.02) if False else None
    for x in ((-2.0, -0.9, 0.3) if s > 0 else (-2.0, -0.9)):
        box(f'GrabHandle_{sd}{x:+.1f}', (x, s * (W2 - WALL - 0.13), ROOFZ - WALL - 0.17), (0.24, 0.03, 0.03), M_PLAST, bevel=0.01)
        for k in (-1, 1):
            box(f'GrabHandleFoot_{sd}{x:+.1f}{k}', (x + k * 0.11, s * (W2 - WALL - 0.13), ROOFZ - WALL - 0.16), (0.03, 0.03, 0.02), M_PLAST)
        ik(f'GrabHandle_{sd}{x:+.1f}', (x, s * (W2 - WALL - 0.13), ROOFZ - WALL - 0.18), f'GrabHandle_{sd}{x:+.1f}')
box('Headliner', (-0.98, 0, ROOFZ - WALL - 0.008), (3.76, 2 * (W2 - WALL) - 0.02, 0.016), M_LINING)
for j in range(4):
    box(f'CeilingLight_{j}', (-2.4 + j * 0.95, 0, ROOFZ - WALL - 0.02), (0.30, 0.10, 0.012), M_LED)
box('GrabPost_SlidingDoor', (SD1 + 0.05, -(W2 - WALL - 0.03), (FLOOR + 2.10) / 2), (0.03, 0.03, 2.10 - FLOOR), M_YEL, bevel=0.01) if False else None
cyl('GrabPole_Entry', (SD0 - 0.06, -(W2 - WALL - 0.06), (FLOOR + ROOFZ - WALL) / 2), 0.016, ROOFZ - WALL - FLOOR, 'z', M_YEL, 20)
ik('GrabPole_Entry', (SD0 - 0.06, -(W2 - WALL - 0.06), 1.40), 'GrabPole_Entry')
# #endregion

# #region ── Seats: driver, front passenger, row 2 (2), row 3 (2), rear bench (3) = 8 passengers; belts, headrests ──
def seat(key, x, y, w=0.46, z=FLOOR, bench_part=False, fold_ok=False, color=M_SEAT):
    H = 0.44                                         # cushion top above the floor
    if not bench_part:
        box(f'SeatPedestal_{key}', (x, y, z + (H - 0.12) / 2), (0.36, w - 0.12, H - 0.12), M_DARK, bevel=0.01)
    box(f'SeatCushion_{key}', (x + 0.02, y, z + H - 0.06), (0.50, w, 0.12), color, bevel=0.03, segs=2)
    for k in (-1, 1):
        box(f'SeatBolster_{key}{k}', (x + 0.02, y + k * (w / 2 - 0.03), z + H + 0.01), (0.46, 0.05, 0.03), M_LEATHER, bevel=0.012)
    bk = box(f'SeatBack_{key}', (x - 0.21, y, z + H + 0.33), (0.12, w, 0.66), color, bevel=0.03, segs=2)
    turn(bk, (x - 0.21, y, z + H), 'y', -12)
    hr = box(f'Headrest_{key}', (x - 0.31, y, z + H + 0.78), (0.10, w * 0.58, 0.20), color, bevel=0.03, segs=2)
    turn(hr, (x - 0.21, y, z + H), 'y', -12)
    for k in (-1, 1):
        po = cyl(f'HeadrestPost_{key}{k}', (x - 0.29, y + k * 0.07, z + H + 0.66), 0.006, 0.08, 'z', M_CHROME, 10)
        turn(po, (x - 0.21, y, z + H), 'y', -12)
    # three-point belt: shoulder (outboard) to the buckle (inboard), the lap part to the outboard hip
    so = 1 if y > 0 else -1
    sh = Vector((x - 0.30, y + so * (w / 2 - 0.06), z + H + 0.62)); hp = Vector((x - 0.12, y + so * (w / 2 - 0.02), z + H + 0.02))
    bu = Vector((x - 0.12, y - so * (w / 2 - 0.04), z + H + 0.04))
    tube(f'BeltDiagonal_{key}', [tuple(sh), (x - 0.10, y, z + H + 0.30), tuple(bu)], 0.012, M_BELT, bez=False)
    tube(f'BeltLap_{key}', [tuple(bu), (x - 0.06, y, z + H + 0.06), tuple(hp)], 0.012, M_BELT, bez=False)
    box(f'Buckle_{key}', tuple(bu), (0.04, 0.03, 0.06), M_PLAST, bevel=0.006)
    ik(f'Seat_{key}', (x + 0.02, y, z + H), f'SeatCushion_{key}'); ik(f'Back_{key}', (x - 0.20, y, z + H + 0.35), f'SeatBack_{key}')
    ik(f'Head_{key}', (x - 0.25, y, z + H + 0.75), f'Headrest_{key}'); ik(f'Buckle_{key}', tuple(bu), f'Buckle_{key}')
DRV = (1.05, 0.52); PAX = (1.05, -0.52)
seat('Driver', *DRV, w=0.52, color=M_LEATHER)
seat('FrontPassenger', *PAX, w=0.52)
R2, R3, R4 = 0.32, -0.56, -1.48
for x, row in ((R2, 'R2'), (R3, 'R3')):
    seat(row + 'Window', x, 0.66, w=0.46); seat(row + 'Middle', x, 0.17, w=0.46)
# rear bench (three places across, one frame on four feet)
box('BenchFrame', (R4, 0, FLOOR + 0.20), (0.40, 1.70, 0.04), M_DARK, bevel=0.01)
for k in (-1, 1):
    for j in (-1, 1):
        box(f'BenchFoot_{k}{j}', (R4 + j * 0.15, k * 0.55, FLOOR + 0.09), (0.05, 0.05, 0.18), M_DARK)
    box(f'BenchSideSupport_{k}', (R4, k * 0.84, FLOOR + 0.31), (0.36, 0.04, 0.18), M_DARK)
for k, y in enumerate((0.58, 0.0, -0.58)):
    seat(f'R4_{k}', R4, y, w=0.56, z=FLOOR - 0.0, bench_part=True)
for nm, (x, y) in {'R2Window': (R2, 0.66), 'R2Middle': (R2, 0.17), 'R3Window': (R3, 0.66), 'R3Middle': (R3, 0.17)}.items():
    turn(box(f'SeatBackHandle_{nm}', (x - 0.285, y, FLOOR + 0.44 + 0.55), (0.03, 0.18, 0.025), M_PLAST, bevel=0.006), (x - 0.21, y, FLOOR + 0.44), 'y', -12)
# luggage area behind the bench: floor tie-down rings and a net
for j, (x, y) in enumerate([(-2.70, 0.70), (-2.70, -0.70), (-2.05, 0.70), (-2.05, -0.70)]):
    torus(f'TieRing_{j}', (x, y, FLOOR + 0.02), 0.025, 0.005, 'x', M_STEEL, 20, 6)
    box(f'TieRingBase_{j}', (x, y, FLOOR + 0.003), (0.05, 0.05, 0.006), M_STEEL)
box('LuggageNetBar', (-1.86, 0, FLOOR + 1.30), (0.03, 1.80, 0.03), M_STEEL) if False else None
# folding wheelchair ramp, stowed upright against the rear doors
for s in (1, -1):
    box(f'RampChannel_{"L" if s > 0 else "R"}', (-L2 + WALL + 0.10, s * 0.40, FLOOR + 0.70), (0.05, 0.26, 1.40), M_STEEL, bevel=0.008)
    for j in range(8):
        box(f'RampTread_{"L" if s > 0 else "R"}{j}', (-L2 + WALL + 0.13, s * 0.40, FLOOR + 0.12 + j * 0.17), (0.012, 0.24, 0.02), M_DARK)
box('RampHingeBar', (-L2 + WALL + 0.10, 0, FLOOR + 0.03), (0.06, 1.10, 0.05), M_STEEL, bevel=0.01)
piv('Ramp', (-L2 + WALL + 0.10, 0, FLOOR + 0.03), 'RampHingeBar')
# #endregion

# #region ── Driver's place: dashboard, wheel, pedals, gear selector, handbrake, taximeter, card terminal, mirrors ──
DX = 1.86
dash = prism('Dashboard', [(1.44, 1.05), (1.58, 1.22), (2.00, 1.27), (2.05, 1.20), (2.05, 0.88), (1.62, 0.88), (1.50, 0.96)],
             -(W2 - WALL), W2 - WALL, M_PANEL, bevel=0.02, segs=2)
box('ConsoleLower', (1.58, 0, (FLOOR + 0.92) / 2), (0.42, 0.28, 0.92 - FLOOR), M_PANEL, bevel=0.02)
box('InstrumentCluster', (1.62, DRV[1], 1.215), (0.012, 0.30, 0.10), M_SCREEN) if False else None
cl = box('ClusterHood', (1.60, DRV[1], 1.24), (0.16, 0.36, 0.06), M_PLAST, bevel=0.02)
box('ClusterScreen', (1.535, DRV[1], 1.17), (0.008, 0.30, 0.10), M_SCREEN)
box('CentreScreen', (1.50, 0.0, 1.13), (0.012, 0.24, 0.15), M_SCREEN)
for j in range(4):
    box(f'DashVent_{j}', (1.47, (-0.80, -0.20, 0.20, 0.80)[j], 1.08), (0.012, 0.14, 0.06), M_PLAST)
box('Taximeter', (1.66, 0.0, 1.255), (0.10, 0.20, 0.06), M_PLAST, bevel=0.01)
box('TaximeterDisplay', (1.609, 0.0, 1.26), (0.004, 0.16, 0.035), M_METER)
box('CardTerminalArm', (1.48, -0.20, 0.98), (0.04, 0.04, 0.12), M_DARK)
box('CardTerminal', (1.44, -0.20, 1.06), (0.04, 0.09, 0.16), M_PLAST, bevel=0.008)
box('CardTerminalScreen', (1.419, -0.20, 1.09), (0.003, 0.07, 0.06), M_SCREEN)
ik('CardTerminal', (1.41, -0.20, 1.06), 'CardTerminal')
box('GearSelector', (1.52, 0.18, 0.95), (0.05, 0.04, 0.08), M_PLAST, bevel=0.01)
box('HandbrakeLever', (1.36, 0.18, 0.74), (0.24, 0.04, 0.04), M_PLAST, bevel=0.012) if False else None
box('ParkBrakeSwitch', (1.48, 0.12, 0.92), (0.02, 0.04, 0.03), M_PLAST)
col_a = math.radians(24)
c0 = Vector((1.62, DRV[1], 1.13)); c1 = c0 + Vector((-0.285 * math.cos(col_a), 0, 0.285 * math.sin(col_a)))
cc = (c0 + c1) / 2
turn(cyl('SteeringColumn', (cc.x, DRV[1], cc.z), 0.035, (c1 - c0).length, 'x', M_PLAST, 20), (cc.x, DRV[1], cc.z), 'y', -(180 - math.degrees(col_a)))
wc = c1
rot = math.degrees(col_a)               # the wheel square to the column
turn(torus('SteeringWheel', (wc.x, DRV[1], wc.z), 0.19, 0.016, 'x', M_PLAST, 56, 12), (wc.x, DRV[1], wc.z), 'y', rot)
turn(cyl('SteeringHub', (wc.x, DRV[1], wc.z), 0.065, 0.06, 'x', M_PLAST, 28, bevel=0.012), (wc.x, DRV[1], wc.z), 'y', rot)
for j in range(3):
    a = math.radians(90 + j * 120) if j else math.radians(-90); c = (wc.x, DRV[1] + 0.12 * math.cos(a), wc.z + 0.12 * math.sin(a))
    sp = turn(box(f'SteeringSpoke_{j}', c, (0.016, 0.14, 0.03), M_PLAST, bevel=0.004), c, 'x', math.degrees(a))
    turn(sp, (wc.x, DRV[1], wc.z), 'y', rot)
for nm, yy, h in (('Accelerator', DRV[1] - 0.13, 0.20), ('Brake', DRV[1] + 0.05, 0.10)):
    p = box(f'Pedal_{nm}', (1.72, yy, FLOOR + 0.13), (0.025, 0.07 if nm == 'Accelerator' else 0.10, h), M_PLAST, bevel=0.005)
    turn(p, (1.72, yy, FLOOR + 0.03), 'y', 22)
    box(f'PedalPost_{nm}', (1.745, yy, FLOOR + 0.10), (0.03, 0.02, 0.20), M_DARK)
box('FootRest', (1.70, DRV[1] + 0.22, FLOOR + 0.08), (0.03, 0.08, 0.14), M_PLAST)
box('InteriorMirrorStem', (apx(2.24) - 0.10, 0, 2.27), (0.03, 0.03, 0.08), M_PLAST) if False else None
box('InteriorMirror', (1.17, 0, 2.20), (0.04, 0.26, 0.07), M_PLAST, bevel=0.012)
box('InteriorMirrorStem', (1.17, 0, 2.26), (0.03, 0.03, 0.08), M_PLAST)
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'SunVisor_{sd}', (0.98, s * 0.50, 2.30), (0.20, 0.62, 0.02), M_LINING, bevel=0.006)
    box(f'SunVisorClip_{sd}', (0.90, s * 0.50, ROOFZ - WALL - 0.03), (0.03, 0.04, 0.06), M_PLAST)
box('OverheadConsole', (0.78, 0, ROOFZ - WALL - 0.04), (0.26, 0.30, 0.06), M_PLAST, bevel=0.015)
box('DomeLight', (0.78, 0, ROOFZ - WALL - 0.072), (0.12, 0.16, 0.006), M_LED)
box('TaxiSignSwitch', (0.80, 0.12, ROOFZ - WALL - 0.072), (0.03, 0.03, 0.006), M_METER)
box('FireExtinguisher', (1.10, -0.18, FLOOR + 0.18), (0.10, 0.10, 0.34), M_RED, bevel=0.04, segs=2) if False else None
ik('DriverHandL', (wc.x, DRV[1] + 0.17, wc.z), 'SteeringWheel'); ik('DriverHandR', (wc.x, DRV[1] - 0.17, wc.z), 'SteeringWheel')
ik('Accelerator', (1.71, DRV[1] - 0.13, FLOOR + 0.15), 'Pedal_Accelerator'); ik('Brake', (1.71, DRV[1] + 0.05, FLOOR + 0.13), 'Pedal_Brake')
ik('Taximeter', (1.60, 0.0, 1.28), 'Taximeter'); ik('GearSelector', (1.52, 0.18, 1.00), 'GearSelector')
for j, (x, y) in enumerate([(-2.40, 0.0), (-2.40, 0.45)]):
    ik(f'Luggage{j}', (x, y, FLOOR), 'FloorBoard')
ik('WheelchairSpace', (-2.30, 0.0, FLOOR), 'FloorBoard')
# #endregion

# #region ── Checks, save and export ──
fl_, cl_, ov_, lo_ = K.checks('TAXIBUS', ground_ok=('Tyre',), overhang_skip_prefix=(
    'Belt', 'Buckle', 'Headrest', 'Steering', 'Pedal', 'Wiper', 'Mirror', 'Seat', 'Bench', 'GrabHandle', 'Nut', 'HubCap', 'WheelCover',
    'Knuckle', 'Strut', 'Spring', 'LeafSpring', 'Damper', 'RearAxle', 'DriveShaft', 'Arch', 'MudFlap', 'Door', 'FrontDoor', 'Sliding',
    'SlideTrack', 'Entry', 'Rear', 'Ramp', 'Tie', 'Taxi', 'Antenna', 'RoofRail', 'Card', 'Gear', 'ParkBrake', 'Sun', 'Overhead', 'Dome',
    'Interior', 'Cluster', 'Centre', 'Dash', 'Foot', 'Floor', 'Checker', 'Rubbing', 'Air', 'Ceiling', 'Headliner', 'Window', 'Lower',
    'Plate', 'Grille', 'Bumper', 'Cowl', 'Tow', 'Engine', 'Battery', 'Exhaust', 'Console', 'Side', 'Front', 'Fog', 'Indicator', 'Headlamp', 'Third', 'Tail', 'Reverse'))
GLB = os.path.join(ART, 'taxibus.glb'); BLEND = os.path.join(ART, 'TaxiBus.blend'); zf = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, BLEND, GLB, zf, 'TAXIBUS', 'TaxiBus')
json.dump(dict(parts=len(ALL), ik=[o.name for o in IKS], ik_parts=IKP, floating=fl_, overhang=ov_, loose=lo_, flicker=last),
          open(os.path.join(ART, 'taxibus-parts.json'), 'w'), indent=1)
print('TAXIBUS saved', len(ALL), 'parts', len(IKS), 'IK/pivots, flicker', last)
# #endregion

# #region ── The wireframe views (every line an edge of the real mesh; Cycles + Freestyle, runs without a GPU) ──
if DO_WIRE:
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.samples = 8; sc.cycles.use_denoising = True; sc.cycles.device = 'CPU'
    w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.035, 0.08, 0.16, 1)
    w.node_tree.nodes['Background'].inputs['Strength'].default_value = 1.6; sc.world = w
    sc.view_settings.view_transform = 'Standard'
    sc.render.use_freestyle = True; sc.render.line_thickness_mode = 'ABSOLUTE'; sc.render.line_thickness = 0.9
    fs = bpy.context.view_layer.freestyle_settings; fs.crease_angle = math.radians(179.5)
    ls = fs.linesets[0] if len(fs.linesets) else fs.linesets.new('All')
    ls.select_by_visibility = True; ls.visibility = 'VISIBLE'; ls.select_by_edge_types = True
    ls.select_crease = True; ls.select_silhouette = True; ls.select_border = True
    if ls.linestyle is None: ls.linestyle = bpy.data.linestyles.new('Wire')
    ls.linestyle.color = (0.82, 0.9, 1.0); ls.linestyle.thickness = 0.9
    flat = bpy.data.materials.new('WireBlue'); flat.use_nodes = True
    flat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.10, 0.25, 0.55, 1)
    flat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.8
    bpy.context.view_layer.material_override = flat
    for nm, loc, e in (('Key', (4, 5, 7), 3.0), ('Fill', (-5, -4, 4), 1.2)):
        ld = bpy.data.lights.new(nm, 'SUN'); ld.energy = e; lo2 = bpy.data.objects.new(nm, ld); sc.collection.objects.link(lo2)
        lo2.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
    only = [a for a in sys.argv if a.startswith('--only=')]
    only = only[0][7:].split(',') if only else None
    VIEWS = {}
    def shoot(name, loc, at, lens=50, ortho=0, res=(2400, 1500), hide=(), rot=None):
        if only and name not in only: return
        for o in ALL: o.hide_render = o.name.startswith(hide) if hide else False
        cam.location = loc
        cam.rotation_euler = rot if rot else (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        cam_d.type = 'ORTHO' if ortho else 'PERSP'; cam_d.lens = lens; cam_d.ortho_scale = ortho or 1
        sc.render.resolution_x, sc.render.resolution_y = res
        sc.render.filepath = os.path.join(WIRE, name + '.png'); bpy.ops.render.render(write_still=True)
        VIEWS[name] = dict(loc=list(loc), at=list(at), ortho=ortho, res=list(res)); print('view', name, flush=True)
        for o in ALL: o.hide_render = False
    ROOFOFF = ('BodyShell', 'Headliner', 'CeilingLight', 'AirDuct', 'AirVent', 'GrabHandle', 'Taxi', 'Antenna', 'RoofRail', 'OverheadConsole',
               'DomeLight', 'SunVisor', 'Interior', 'SideGlass', 'Windscreen', 'FrontDoor', 'SlidingDoor', 'RearDoor', 'Mirror', 'RearWiper',
               'RearCamera', 'ThirdBrake', 'CheckerBand', 'GrabPole', 'WindowLedge', 'LowerLining', 'RubbingStrip', 'SideMarker', 'FrontRepeater',
               'SlideTrack', 'DoorHinge', 'RearHinge')
    shoot('1-three-quarter-front', (8.2, -6.2, 3.4), (0.3, 0, 1.15), 45)
    shoot('2-three-quarter-rear', (-8.0, 6.4, 3.4), (-0.3, 0, 1.15), 45)
    shoot('3-side-right', (0, -30, 1.25), (0, 0, 1.25), 50, 6.6, (2400, 1000))
    shoot('4-side-left', (0, 30, 1.25), (0, 0, 1.25), 50, 6.6, (2400, 1000))
    shoot('5-front', (30, 0, 1.25), (0, 0, 1.25), 50, 2.75, (1400, 1400))
    shoot('6-rear', (-30, 0, 1.25), (0, 0, 1.25), 50, 2.75, (1400, 1400))
    shoot('7-top', (0, 0, 30), (0, 0, 0), 50, 6.6, (2400, 1000), rot=(0, 0, 0))
    shoot('8-interior-from-above', (0, 0, 30), (0, 0, 0), 50, 6.4, (2400, 1000), hide=ROOFOFF, rot=(0, 0, 0))
    shoot('9-cutaway', (5.2, -4.6, 4.6), (-0.3, 0, 0.9), 40, hide=ROOFOFF)
    shoot('10-drivers-place', (1.0, -0.50, 1.85), (1.55, 0.42, 1.12), 22, hide=('BodyShell', 'Headliner', 'OverheadConsole', 'DomeLight', 'SunVisor', 'Interior', 'Taxi'))
    shoot('11-passenger-seats', (0.55, -0.80, 1.70), (-1.30, 0.45, 0.85), 20, hide=ROOFOFF)
    shoot('12-sliding-door-and-step', (0.8, -4.2, 1.4), (0.2, -1.0, 1.0), 40)
    shoot('13-rear-doors-and-ramp', (-5.6, -2.6, 1.9), (-2.9, 0, 1.2), 40, hide=('RearDoor_L', 'RearDoorGlass_L', 'RearDoorTrim_L', 'RearWiper'))
    json.dump(VIEWS, open(os.path.join(WIRE, 'views.json'), 'w'), indent=1)
# #endregion
