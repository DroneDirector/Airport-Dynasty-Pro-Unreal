# The apron bus (airside passenger bus: terminal <-> remote stands) - our own design, drawn to real-world standards in the
# class of the electric low-floor apron buses (e.COBUS 3000 / COBUS Vega class: 13,920 long, 3,000 wide, 3,100 high, 2,400
# headroom, wheelbase 7,100, entrance step ~290 with kneeling, up to 110 passengers of which 8-14 seated, three extra-wide
# doors on both sides plus a front right door, batteries on the roof; sources in Blueprints/Vehicles/ApronBus/RESEARCH.md).
# No brand names or logos. Built to the vehicle standard (Tools/vehicle_kit.py): every part sits on, hangs on or is bolted
# to the part under it, bevelled edges, lamps set into their faces; IK points for the driver and for standing passengers.
# Axes: x forward, y left, z up; metres; origin on the ground under the middle of the bus. Driver front left.
#   python3 Tools/make_apron_bus.py [--wire] [--only=view,view]      (bpy 4.1+ as a module, or Blender 3.3)
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
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
def _local_run(args, **kw):                 # (the kit calls the PC's Python by its Windows path; here it runs locally)
    if isinstance(args, list) and str(args[0]).startswith('C:/'): args = [sys.executable if 'python' in sys.executable else 'python3'] + args[1:]
    return _run(args, **kw)
K.subprocess.run = _local_run
# #endregion

from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, lamp, ALL, IKS
ART = os.path.join(ROOT, 'Art', 'ApronBus'); os.makedirs(ART, exist_ok=True)
BP = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'ApronBus')
WIRE = os.path.join(BP, 'wire'); os.makedirs(WIRE, exist_ok=True)
DO_WIRE = '--wire' in sys.argv
K.reset()
IKP = {}
def ik(name, loc, part=None):
    IKP['IK_' + name] = part; return K.ik(name, loc)
def piv(name, loc, part=None):
    IKP['PIV_' + name] = part; return K.ik(name, loc, prefix='PIV_')

# #region ── The main sizes (m) ──
L2, W2 = 6.96, 1.50                        # half length / half width (13,920 x 3,000)
FX = 3.76; RX = FX - 7.10                  # front / rear axle (wheelbase 7,100; front overhang 3,200, rear 3,620)
TD, TW, RB = 0.957, 0.276, 0.2858          # 275/70 R22.5
TZ = TD / 2
TRF, TRR = 1.18, 1.07                      # wheel centre y: front single tyre, rear dual pair centre
FLOOR = 0.33                               # floor top (kneeling lowers the body 80)
SLAB0 = 0.18
CEIL = FLOOR + 2.40                        # 2,400 headroom
ROOF = 2.86                                # top of the body; batteries and A/C on it to 3,100
WAIST, WTOP = 1.00, 2.58                   # side window band
SK0 = 0.22                                 # skirt bottom
DOOR_W, DOOR_H = 1.90, 2.15                # clear opening of each passenger door (2 leaves of 0.95)
DOORS_L = [1.95, -1.55, -5.15]             # door centres, left side
DOORS_R = [5.55, 1.95, -1.55, -5.15]       # right side: the same three + the front right door
ARCH_R = 0.60                              # wheel arch opening radius (front), rear 0.62 + 0.12 wider for the duals
CAB_X0 = 5.35                              # driver's cab: x 5.35 .. front, y 0.25 .. 1.45 (front left)
# #endregion

# #region ── Materials (plain; CC0 photo set later, as the other vehicles) ──
M_PAINT = pmat('AB_BodyPaint', (0.92, 0.93, 0.95), 0.1, 0.35)      # white body
M_STRIPE = pmat('AB_Stripe', (0.05, 0.35, 0.75), 0.1, 0.35)         # airline-neutral blue band
M_DARK = pmat('AB_DarkTrim', (0.06, 0.06, 0.07), 0.2, 0.5)
M_GLASS = pmat('AB_Glass', (0.20, 0.30, 0.35), 0.0, 0.05, 0, 0.35)
M_STEEL = pmat('AB_Steel', (0.45, 0.46, 0.48), 1.0, 0.4)
M_STAIN = pmat('AB_Stainless', (0.80, 0.81, 0.83), 1.0, 0.18)       # poles and rails
M_YELP = pmat('AB_YellowGrip', (0.95, 0.75, 0.05), 0.0, 0.45)
M_RUB = pmat('AB_Tyre', (0.03, 0.03, 0.03), 0.0, 0.9)
M_FLOOR = pmat('AB_FloorVinyl', (0.22, 0.24, 0.27), 0.0, 0.7)
M_SEAT = pmat('AB_SeatFabric', (0.10, 0.18, 0.35), 0.0, 0.8)
M_SHELL = pmat('AB_SeatShell', (0.15, 0.16, 0.18), 0.0, 0.5)
M_LINING = pmat('AB_CeilingLining', (0.85, 0.86, 0.88), 0.0, 0.6)
M_PLAST = pmat('AB_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_AMBER = pmat('AB_AmberLens', (1.0, 0.55, 0.05), 0.0, 0.2, 2.0)
M_RED = pmat('AB_RedLens', (0.9, 0.05, 0.03), 0.0, 0.2, 1.0)
M_WHITE = pmat('AB_WhiteLens', (0.95, 0.95, 0.9), 0.0, 0.1, 1.5)
M_LED = pmat('AB_LEDStrip', (0.95, 0.96, 1.0), 0.0, 0.2, 2.0)
M_SCREEN = pmat('AB_Screen', (0.05, 0.25, 0.35), 0.0, 0.1, 0.8)
M_DEST = pmat('AB_DestinationLED', (1.0, 0.65, 0.1), 0.0, 0.2, 1.5)
M_GREEN = pmat('AB_GreenButton', (0.05, 0.8, 0.15), 0.0, 0.2, 1.0)
M_REDP = pmat('AB_RedPaint', (0.7, 0.04, 0.03), 0.0, 0.4)
M_MIRROR = pmat('AB_MirrorGlass', (0.8, 0.85, 0.9), 1.0, 0.02)
M_BATT = pmat('AB_BatteryPod', (0.70, 0.72, 0.75), 0.3, 0.4)
M_LEATHER = pmat('AB_DriverSeat', (0.05, 0.05, 0.06), 0.0, 0.6)
# #endregion

# #region ── Wheels (lathed tyres and steel rims; rear duals) ──
TUG_PROF = [(0.290, -0.124), (0.318, -0.146), (0.360, -0.157), (0.410, -0.1585), (0.460, -0.154), (0.500, -0.142), (0.522, -0.124),
            (0.534, -0.100), (0.538, -0.060), (0.539, 0.0), (0.538, 0.060), (0.534, 0.100), (0.522, 0.124), (0.500, 0.142),
            (0.460, 0.154), (0.410, 0.1585), (0.360, 0.157), (0.318, 0.146), (0.290, 0.124), (0.286, 0.0)]
TUG_RIM = [(0.300, -0.130), (0.296, -0.122), (0.284, -0.118), (0.270, -0.070), (0.262, -0.050), (0.262, 0.050), (0.270, 0.070),
           (0.284, 0.118), (0.296, 0.122), (0.300, 0.130), (0.288, 0.132), (0.276, 0.122), (0.256, 0.060), (0.256, -0.060), (0.276, -0.122), (0.288, -0.132)]
def tyre(key, x, y, s, disc=True):
    R, rb, tw = TZ, RB, TW; q = tw / 0.165; k = rb / 0.135
    lathe(f'Tyre_{key}', [(rb + (r - 0.29) * ((R - rb) / 0.249), yy * (tw / 2 / 0.1585)) for r, yy in TUG_PROF], (x, y, TZ), M_RUB, 80)
    lathe(f'Rim_{key}', [(rb + (r - 0.29) * 0.58 * k, yy * 0.52 * q) for r, yy in TUG_RIM], (x, y, TZ), M_STEEL, 80)
    if disc:
        d = cyl(f'WheelDisc_{key}', (x, y + s * 0.035 * q, TZ), rb - 0.017 * k, 0.012, 'y', M_STEEL, 64, bevel=0.002)
        for j in range(8):
            a = j * 2 * math.pi / 8 + math.pi / 8
            cut(d, cyl('HandHole', (x + 0.17 * math.cos(a), y + s * 0.035 * q, TZ + 0.17 * math.sin(a)), 0.03, 0.05, 'y', M_DARK, 20))
        cyl(f'Hub_{key}', (x, y + s * (0.035 * q + 0.03), TZ), 0.11, 0.06, 'y', M_DARK, 40, bevel=0.01)
        cyl(f'HubCap_{key}', (x, y + s * (0.035 * q + 0.07), TZ), 0.07, 0.03, 'y', M_DARK, 32, bevel=0.008)
        for j in range(10):
            a = j * 2 * math.pi / 10
            cyl(f'Nut_{key}{j}', (x + 0.112 * math.cos(a), y + s * (0.035 * q + 0.067), TZ + 0.112 * math.sin(a)), 0.014, 0.02, 'y', M_STEEL, 6)
for s, sd in ((1, 'L'), (-1, 'R')):
    tyre('F' + sd, FX, s * TRF, s)
    tyre('RO' + sd, RX, s * (TRR + 0.155), s); tyre('RI' + sd, RX, s * (TRR - 0.155), s, disc=False)
    ik('Wheel_F' + sd, (FX, s * TRF, TZ), 'Hub_F' + sd); ik('Wheel_R' + sd, (RX, s * TRR, TZ), 'Hub_RO' + sd)
    cyl(f'Kingpin_{sd}', (FX, s * (TRF - 0.24), TZ), 0.05, 0.30, 'z', M_DARK, 24, bevel=0.006)
    cyl(f'Spindle_{sd}', (FX, s * (TRF - 0.12), TZ), 0.05, 0.26, 'y', M_STEEL, 24)
    ik('Steer_F' + sd, (FX, s * (TRF - 0.24), TZ), 'Kingpin_' + sd)
# portal axles (low floor): the beams run under the floor slab, drop boxes rise to the hubs inside the wheel housings
cyl('FrontAxleBeam', (FX, 0, 0.12), 0.05, 2 * 0.92, 'y', M_DARK, 24)
cyl('RearAxleTube', (RX, 0, 0.12), 0.06, 2 * 0.74, 'y', M_DARK, 32)
for s, sd in ((1, 'L'), (-1, 'R')):
    box(f'FrontDropBox_{sd}', (FX, s * 0.92, (0.07 + TZ) / 2), (0.20, 0.08, TZ - 0.07), M_DARK, bevel=0.015)
    box(f'RearDropBox_{sd}', (RX, s * 0.74, (0.06 + TZ) / 2), (0.26, 0.08, TZ - 0.06), M_DARK, bevel=0.015)
    cyl(f'RearStub_{sd}', (RX, s * 0.80, TZ), 0.06, 0.12, 'y', M_DARK, 24)
box('RearDiffHousing', (RX, 0.0, 0.12), (0.34, 0.40, 0.12), M_DARK, bevel=0.03, segs=2)
# the drive motor, inverter and pumps sit in a compartment in the rear right corner (inside, behind a panel)
box('MotorCompartment', (-6.52, -(W2 - 0.42), (FLOOR + 1.10) / 2), (0.62, 0.72, 1.10 - FLOOR), M_SHELL, bevel=0.02)
box('MotorCompartmentGrille', (-6.52 + 0.311, -(W2 - 0.42), 0.80), (0.004, 0.50, 0.30), M_DARK)
cyl('DriveShaftRear', (RX - 1.40, -0.30, 0.12), 0.035, 2.6, 'x', M_STEEL, 20) if False else None
# #endregion

# #region ── Floor, wheel housings, skirts with the arches and door openings ──
box('FloorSlab', (0, 0, (SLAB0 + FLOOR) / 2), (2 * L2 - 0.12, 2 * W2 - 0.06, FLOOR - SLAB0), M_DARK, bevel=0.01)
box('FloorVinyl', (0, 0, FLOOR + 0.002), (2 * L2 - 0.30, 2 * W2 - 0.20, 0.004), M_FLOOR)
HOUSE = []
for ax, r, nm in ((FX, ARCH_R, 'F'), (RX, ARCH_R + 0.06, 'R')):
    for s, sd in ((1, 'L'), (-1, 'R')):
        yi = s * (0.78 if nm == 'R' else 0.88)
        h = box(f'WheelHousing_{nm}{sd}', (ax, s * (W2 + abs(yi)) / 2 - s * 0.03, (FLOOR + 1.08) / 2), (2 * r + 0.30, W2 - abs(yi) - 0.06, 1.08 - FLOOR), M_SHELL, bevel=0.03)
        cut(h, cyl('HousingCut', (ax, s * W2, TZ), TZ + 0.06, 2 * (W2 - abs(yi)) + 0.2, 'y', M_DARK, 64))   # hollow over the wheel
        HOUSE.append((ax, yi, r, s))
def openings(s):
    ds = DOORS_L if s > 0 else DOORS_R
    return sorted([(x - DOOR_W / 2 - 0.06, x + DOOR_W / 2 + 0.06) for x in ds])
def spans(s, x0=-L2 + 0.16, x1=L2 - 0.16):
    out, x = [], x0
    for a, b in openings(s):
        if a > x: out.append((x, a))
        x = b
    if x < x1: out.append((x, x1))
    return out
for s, sd in ((1, 'L'), (-1, 'R')):
    y = s * (W2 - 0.025)
    for i, (a, b) in enumerate(spans(s)):
        sk = box(f'Skirt_{sd}{i}', ((a + b) / 2, y, (SK0 + WAIST) / 2), (b - a, 0.05, WAIST - SK0), M_PAINT, bevel=0.006)
        for ax, r in ((FX, ARCH_R), (RX, ARCH_R + 0.12)):
            if a - r < ax < b + r:
                cut(sk, cyl('ArchCut', (ax, y, TZ), r, 0.2, 'y', M_DARK, 64))
        box(f'Stripe_{sd}{i}', ((a + b) / 2, y + s * 0.027, 0.80), (b - a - 0.02, 0.006, 0.12), M_STRIPE)
        box(f'RubbingStrip_{sd}{i}', ((a + b) / 2, y + s * 0.035, 0.55), (b - a - 0.02, 0.02, 0.05), M_DARK, bevel=0.008)
        for ax, r in ((FX, ARCH_R), (RX, ARCH_R + 0.12)):     # the strip and stripe have the arch cut too
            if a - r < ax < b + r:
                for o in ALL[-2:]: cut(o, cyl('ArchCut', (ax, y, TZ), r + 0.02, 0.3, 'y', M_DARK, 64))
    for ax, r in ((FX, ARCH_R), (RX, ARCH_R + 0.12)):         # arch trim rings (rubber lip)
        tr = torus(f'ArchLip_{sd}{"F" if ax > 0 else "R"}', (ax, y + s * 0.02, TZ), r + 0.01, 0.02, 'y', M_DARK, 64, 10)
        cut(tr, box('LipCut', (ax, y, TZ - 0.6), (2 * r + 0.2, 0.3, 1.2 - 2 * (TZ - SK0) + 0.0), M_DARK))
        box(f'MudFlap_{sd}{"F" if ax > 0 else "R"}', (ax - r - 0.10, s * (W2 - 0.20), 0.40), (0.012, 0.30, 0.34), M_PLAST, bevel=0.004)
# #endregion

# #region ── Window band: pillars, glass, door headers, cant rail, roof ──
for s, sd in ((1, 'L'), (-1, 'R')):
    y = s * (W2 - 0.03)
    for i, (a, b) in enumerate(spans(s)):
        n = max(1, round((b - a) / 1.35)); xs = [a + (b - a) * k / n for k in range(n + 1)]
        for j, x in enumerate(xs):
            box(f'Pillar_{sd}{i}_{j}', (x, y, (WAIST + WTOP) / 2), (0.10, 0.06, WTOP - WAIST), M_DARK, bevel=0.008)
        for j in range(n):
            box(f'SideGlass_{sd}{i}_{j}', ((xs[j] + xs[j + 1]) / 2, y + s * 0.005, (WAIST + WTOP) / 2), (xs[j + 1] - xs[j] - 0.06, 0.02, WTOP - WAIST - 0.04), M_GLASS)
            if j % 2 == 0:       # tilting top light
                box(f'TiltLight_{sd}{i}_{j}', ((xs[j] + xs[j + 1]) / 2, y + s * 0.018, WTOP - 0.22), (xs[j + 1] - xs[j] - 0.12, 0.012, 0.02), M_DARK)
        box(f'WaistRail_{sd}{i}', ((a + b) / 2, y, WAIST + 0.01), (b - a, 0.07, 0.04), M_DARK, bevel=0.006)
    for x0, x1 in openings(s):
        box(f'DoorHeader_{sd}{(x0 + x1) / 2:+.1f}', ((x0 + x1) / 2, y, (FLOOR + DOOR_H + 0.02 + 2.64) / 2), (x1 - x0, 0.06, 2.64 - FLOOR - DOOR_H - 0.02), M_PAINT, bevel=0.006)
    box(f'CantRail_{sd}', (0, s * (W2 - 0.04), (WTOP + ROOF) / 2), (2 * L2 - 0.20, 0.08, ROOF - WTOP), M_PAINT, bevel=0.02, segs=2)
box('Roof', (0, 0, ROOF - 0.05), (2 * L2 - 0.12, 2 * W2 - 0.10, 0.10), M_PAINT, bevel=0.04, segs=3)
# door pillars (both sides of every door opening)
for s, sd in ((1, 'L'), (-1, 'R')):
    for x0, x1 in openings(s):
        for x in (x0, x1):
            box(f'DoorPost_{sd}{x:+.2f}', (x, s * (W2 - 0.04), (SK0 + WTOP) / 2), (0.12, 0.08, WTOP - SK0), M_DARK, bevel=0.01)
# #endregion

# #region ── Passenger doors: two glazed outward-swinging plug leaves each, rubber edges, warning light, step lighting ──
DOORS = []
for s, sd in ((1, 'L'), (-1, 'R')):
    for xc in (DOORS_L if s > 0 else DOORS_R):
        tag = f'{sd}{xc:+.2f}'
        for k, sg in enumerate((-1, 1)):
            lx = xc + sg * DOOR_W / 4; y = s * (W2 + 0.005)
            leaf = box(f'DoorLeaf_{tag}_{k}', (lx, y, FLOOR + DOOR_H / 2 + 0.02), (DOOR_W / 2 - 0.02, 0.05, DOOR_H - 0.02), M_PAINT, bevel=0.012)
            box(f'DoorGlass_{tag}_{k}', (lx, y + s * 0.027, FLOOR + 1.25), (DOOR_W / 2 - 0.16, 0.006, 1.55), M_GLASS)
            box(f'DoorGlassLow_{tag}_{k}', (lx, y + s * 0.027, FLOOR + 0.30), (DOOR_W / 2 - 0.16, 0.006, 0.32), M_GLASS)
            box(f'DoorEdgeRubber_{tag}_{k}', (xc + sg * 0.012, y, FLOOR + DOOR_H / 2 + 0.02), (0.024, 0.06, DOOR_H - 0.06), M_DARK)
            cyl(f'DoorPivot_{tag}_{k}', (xc + sg * (DOOR_W / 2 - 0.04), s * (W2 - 0.02), FLOOR + DOOR_H / 2), 0.025, DOOR_H - 0.10, 'z', M_STAIN, 16)
            box(f'DoorHandleBar_{tag}_{k}', (lx - sg * 0.25, s * (W2 - 0.035), FLOOR + 1.0), (0.03, 0.03, 0.70), M_STAIN, bevel=0.01)
            piv(f'Door_{tag}_{k}', (xc + sg * (DOOR_W / 2 - 0.04), s * (W2 - 0.02), FLOOR), f'DoorPivot_{tag}_{k}')
        lamp(f'DoorWarn_{tag}', (xc, s * (W2 - 0.01), FLOOR + DOOR_H + 0.18), 'y' + ('+' if s > 0 else '-'), 0.16, 0.05, M_AMBER, M_DARK, M_PLAST, depth=0.03)
        box(f'DoorStepEdge_{tag}', (xc, s * (W2 - 0.12), FLOOR + 0.008), (DOOR_W, 0.08, 0.008), M_YELP)
        box(f'DoorStepLight_{tag}', (xc, s * (W2 - 0.02), FLOOR - 0.06), (DOOR_W - 0.2, 0.02, 0.02), M_LED)
        # outside door open buttons (green) on the rear post
        cyl(f'DoorButtonOut_{tag}', (xc + DOOR_W / 2 + 0.06, s * (W2 + 0.002), FLOOR + 1.1), 0.025, 0.012, 'y', M_GREEN, 20)
        DOORS.append((xc, s))
# #endregion

# #region ── Front: windscreen, destination display, headlamps, bumper, wipers, mirrors; rear: window, lamps ──
FXF = L2
box('FrontLowerPanel', (FXF - 0.03, 0, (SK0 + 1.05) / 2), (0.06, 2 * W2 - 0.06, 1.05 - SK0), M_PAINT, bevel=0.03, segs=2)
box('FrontStripe', (FXF + 0.002, 0, 0.80), (0.006, 2 * W2 - 0.30, 0.12), M_STRIPE)
box('FrontBumper', (FXF + 0.05, 0, 0.34), (0.12, 2 * W2 - 0.10, 0.22), M_DARK, bevel=0.04, segs=3)
for s in (1, -1):
    box(f'FrontCornerPost_{"L" if s > 0 else "R"}', (FXF - 0.07, s * (W2 - 0.07), (1.05 + ROOF) / 2), (0.14, 0.14, ROOF - 1.05), M_PAINT, bevel=0.05, segs=3)
ws = box('Windscreen', (FXF - 0.03, 0, 1.80), (0.03, 2 * W2 - 0.30, 1.40), M_GLASS, bevel=0.01)
turn(ws, (FXF - 0.03, 0, 1.10), 'y', -4)
box('WindscreenFrameLow', (FXF - 0.05, 0, 1.08), (0.08, 2 * W2 - 0.28, 0.06), M_DARK, bevel=0.01)
box('DestinationHousing', (FXF - 0.06, 0, (2.52 + ROOF) / 2), (0.10, 2 * W2 - 0.28, ROOF - 2.52), M_DARK, bevel=0.02)
box('DestinationDisplay', (FXF - 0.0085, 0, 2.69), (0.006, 2 * W2 - 0.70, 0.22), M_DEST)
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'Headlamp_{nm}', (FXF, s * 1.12, 0.62), 'x+', 0.36, 0.14, M_WHITE, M_DARK, M_PLAST)
    lamp(f'DRL_{nm}', (FXF, s * 0.78, 0.62), 'x+', 0.22, 0.04, M_WHITE, M_DARK, M_PLAST, depth=0.03)
    lamp(f'IndicatorF_{nm}', (FXF, s * 1.12, 0.47), 'x+', 0.20, 0.06, M_AMBER, M_DARK, M_PLAST)
    lamp(f'MarkerTopF_{nm}', (FXF, s * 1.43, 2.70), 'x+', 0.08, 0.05, M_AMBER, M_DARK, M_PLAST, depth=0.03)
    # mirror arms ("rabbit ears") over the windscreen corners
    tube(f'MirrorArm_{nm}', [(FXF - 0.08, s * (W2 - 0.05), 2.40), (FXF + 0.30, s * (W2 + 0.05), 2.40), (FXF + 0.45, s * (W2 + 0.05), 2.10)], 0.022, M_DARK, bez=False)
    box(f'MirrorHead_{nm}', (FXF + 0.45, s * (W2 + 0.05), 1.88), (0.10, 0.26, 0.44), M_DARK, bevel=0.03, segs=2)
    box(f'MirrorGlass_{nm}', (FXF + 0.398, s * (W2 + 0.05), 1.88), (0.008, 0.22, 0.40), M_MIRROR)
    box(f'MirrorBase_{nm}', (FXF - 0.06, s * (W2 - 0.05), 2.40), (0.10, 0.10, 0.10), M_DARK, bevel=0.01)
# wipers: pivots under the screen, arm + blade parked flat along the screen foot (the right pair a little higher)
for s in (1, -1):
    y0 = s * 0.55; sd = 'L' if s > 0 else 'R'; zz = 1.16 if s > 0 else 1.21
    cyl(f'WiperPivot_{sd}', (FXF - 0.03, y0, zz), 0.025, 0.06, 'x', M_DARK, 16)
    box(f'WiperArm_{sd}', (FXF - 0.005, y0 - s * 0.24, zz), (0.02, 0.50, 0.025), M_DARK)
    box(f'WiperBlade_{sd}', (FXF + 0.0, y0 - s * 0.30, zz + 0.025), (0.02, 0.70, 0.025), M_PLAST)
    piv(f'Wiper_{sd}', (FXF - 0.03, y0, zz), f'WiperPivot_{sd}')
cyl('TowEyeFront', (FXF + 0.12, 0.0, 0.34), 0.035, 0.10, 'x', M_STEEL, 20)
# rear
RXF = -L2
box('RearLowerPanel', (RXF + 0.03, 0, (SK0 + 1.05) / 2), (0.06, 2 * W2 - 0.06, 1.05 - SK0), M_PAINT, bevel=0.03, segs=2)
box('RearStripe', (RXF - 0.002, 0, 0.80), (0.006, 2 * W2 - 0.30, 0.12), M_STRIPE)
box('RearBumper', (RXF - 0.05, 0, 0.34), (0.12, 2 * W2 - 0.10, 0.22), M_DARK, bevel=0.04, segs=3)
for s in (1, -1):
    box(f'RearCornerPost_{"L" if s > 0 else "R"}', (RXF + 0.07, s * (W2 - 0.07), (1.05 + ROOF) / 2), (0.14, 0.14, ROOF - 1.05), M_PAINT, bevel=0.05, segs=3)
box('RearWindow', (RXF + 0.03, 0, 1.75), (0.03, 2 * W2 - 0.30, 1.30), M_GLASS)
box('RearTopPanel', (RXF + 0.06, 0, (2.40 + ROOF) / 2), (0.10, 2 * W2 - 0.28, ROOF - 2.40), M_PAINT, bevel=0.02)
box('RearWindowFrameLow', (RXF + 0.05, 0, 1.08), (0.08, 2 * W2 - 0.28, 0.06), M_DARK, bevel=0.01)
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'TailBrake_{nm}', (RXF, s * 1.15, 0.65), 'x-', 0.22, 0.20, M_RED, M_DARK, M_PLAST)
    lamp(f'IndicatorR_{nm}', (RXF, s * 1.15, 0.43), 'x-', 0.22, 0.08, M_AMBER, M_DARK, M_PLAST)
    lamp(f'Reverse_{nm}', (RXF, s * 0.80, 0.62), 'x-', 0.14, 0.08, M_WHITE, M_DARK, M_PLAST)
    lamp(f'MarkerTopR_{nm}', (RXF, s * 1.43, 2.65), 'x-', 0.08, 0.05, M_RED, M_DARK, M_PLAST, depth=0.03)
lamp('ThirdBrake', (RXF, 0.0, 2.55), 'x-', 0.60, 0.05, M_RED, M_DARK, M_PLAST, depth=0.03)
cyl('TowEyeRear', (RXF - 0.12, 0.0, 0.34), 0.035, 0.10, 'x', M_STEEL, 20)
for s, nm in ((1, 'L'), (-1, 'R')):           # side marker lamps along the skirt (not on the doors or arches)
    for i, (a, b) in enumerate(spans(s)):
        if b - a > 1.2:
            x = (a + b) / 2
            if any(abs(x - ax) < 0.9 for ax in (FX, RX)): x = a + 0.35
            lamp(f'SideMarker_{nm}{i}', (x, s * W2, 0.40), 'y' + ('+' if s > 0 else '-'), 0.08, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.025)
box('SideDisplayFront_R', (4.35, -(W2 + 0.0), 2.30), (0.70, 0.02, 0.18), M_DEST) if False else None
# #endregion

# #region ── Roof: battery pods (the batteries ride on the roof), A/C unit, roof hatches, beacon ──
for i, x in enumerate((-4.6, -2.3, 2.3, 4.6)):
    box(f'BatteryPod_{i}', (x, 0, ROOF + 0.11), (2.0, 2.2, 0.22), M_BATT, bevel=0.05, segs=3)
    for j in range(3):
        box(f'BatteryPodRib_{i}{j}', (x - 0.6 + j * 0.6, 0, ROOF + 0.225), (0.04, 2.0, 0.012), M_STEEL)
box('ACUnit', (0.0, 0, ROOF + 0.12), (2.2, 2.0, 0.24), M_BATT, bevel=0.06, segs=3)
for j in range(2):
    cyl(f'ACFan_{j}', (-0.5 + j * 1.0, 0, ROOF + 0.245), 0.30, 0.012, 'z', M_DARK, 40)
for i, x in enumerate((-1.45, 1.45)):
    box(f'RoofHatch_{i}', (x, 0.0, ROOF + 0.03), (0.70, 0.70, 0.06), M_DARK, bevel=0.02)
cyl('BeaconBase', (5.9, 0, ROOF + 0.02), 0.09, 0.04, 'z', M_DARK, 28)
cyl('Beacon', (5.9, 0, ROOF + 0.11), 0.08, 0.14, 'z', M_AMBER, 28, bevel=0.02)
cyl('ChargePortCover', (-6.2, -(W2 + 0.002), 1.60), 0.10, 0.012, 'y', M_DARK, 32) if False else None
box('ChargeFlap', (-6.30, -(W2 + 0.0), 0.75), (0.30, 0.012, 0.22), M_DARK, bevel=0.004)
# #endregion

# #region ── Inside: ceiling lining, light strips, A/C ducts, poles, rails, straps, seats over the arches, racks, screens ──
box('CeilingLining', (0, 0, CEIL + 0.01), (2 * L2 - 0.30, 2 * W2 - 0.20, 0.02), M_LINING)
for s in (1, -1):
    box(f'LightStrip_{"L" if s > 0 else "R"}', (-0.2, s * 1.10, CEIL - 0.015), (2 * L2 - 3.0, 0.10, 0.03), M_LED)
    box(f'ACDuct_{"L" if s > 0 else "R"}', (-0.2, s * 1.30, CEIL - 0.08), (2 * L2 - 3.0, 0.24, 0.16), M_LINING, bevel=0.03)
# vertical poles: two rows by the aisle every ~1.6 m, and at each door's sides
POLES = []
for s in (1, -1):
    for x in [-6.1, -4.3, -2.7, -0.5, 0.9, 2.9, 4.4]:
        POLES.append((x, s * 0.62))
for xc, s in DOORS:
    for sg in (-1, 1): POLES.append((xc + sg * (DOOR_W / 2 + 0.05), s * (W2 - 0.30)))
POLES = sorted(set((round(x, 2), round(y, 2)) for x, y in POLES))
for i, (x, y) in enumerate(POLES):
    if x > CAB_X0 - 0.1 and y > 0.2: continue
    cyl(f'Pole_{i}', (x, y, (FLOOR + CEIL) / 2), 0.0175, CEIL - FLOOR, 'z', M_YELP if i % 2 else M_STAIN, 20)
    cyl(f'PoleFoot_{i}', (x, y, FLOOR + 0.02), 0.04, 0.04, 'z', M_STAIN, 20, bevel=0.008)
    cyl(f'PoleTop_{i}', (x, y, CEIL - 0.02), 0.04, 0.04, 'z', M_STAIN, 20, bevel=0.008)
    if i % 3 == 0:
        box(f'PoleButton_{i}', (x + 0.03, y, FLOOR + 1.35), (0.04, 0.05, 0.08), M_REDP, bevel=0.01)
    ik(f'Hold_Pole{i}', (x, y, FLOOR + 1.55), f'Pole_{i}')
# ceiling grab rails over the aisle rows with hanging straps
for s, sd in ((1, 'L'), (-1, 'R')):
    cyl(f'CeilingRail_{sd}', (-0.6, s * 0.62, CEIL - 0.14), 0.0175, 2 * L2 - 3.8, 'x', M_STAIN, 20)
    for j in range(7):
        x = -6.2 + j * 1.85
        cyl(f'RailHanger_{sd}{j}', (x, s * 0.62, CEIL - 0.07), 0.012, 0.14, 'z', M_STAIN, 12)
    for j in range(22):
        x = -6.0 + j * 0.48
        if any(abs(x - px) < 0.10 for px, py in POLES): continue
        tube(f'Strap_{sd}{j}', [(x, s * 0.62, CEIL - 0.16), (x, s * 0.62, CEIL - 0.42)], 0.008, M_PLAST, bez=False)
        torus(f'StrapLoop_{sd}{j}', (x, s * 0.62, CEIL - 0.48), 0.06, 0.012, 'y', M_YELP, 24, 8)
        ik(f'Hold_Strap{sd}{j}', (x, s * 0.62, CEIL - 0.53), f'StrapLoop_{sd}{j}')
# seats: three inward-facing seats on each wheel housing (the only seats; the rest is standing room, as real apron buses)
SEATS = []
for (ax, yi, r, s) in HOUSE:
    if ax > 0 and s > 0: continue                      # front-left housing is under the driver's cab
    for k in (-1, 0, 1):
        x = ax + k * 0.48; z = 1.08
        box(f'SeatPan_{ax:+.0f}{s:+d}{k}', (x, s * (W2 - 0.34), z + 0.04), (0.44, 0.42, 0.08), M_SHELL, bevel=0.02)
        box(f'SeatCushion_{ax:+.0f}{s:+d}{k}', (x, s * (W2 - 0.34), z + 0.10), (0.42, 0.40, 0.05), M_SEAT, bevel=0.02, segs=2)
        box(f'SeatBack_{ax:+.0f}{s:+d}{k}', (x, s * (W2 - 0.12), z + 0.40), (0.42, 0.06, 0.60), M_SEAT, bevel=0.02, segs=2)
        ik(f'Seat_{ax:+.0f}{s:+d}{k}', (x, s * (W2 - 0.34), z + 0.125), f'SeatCushion_{ax:+.0f}{s:+d}{k}')
    box(f'SeatGrabRail_{ax:+.0f}{s:+d}', (ax, s * (W2 - 0.56), 1.40), (1.50, 0.03, 0.03), M_STAIN, bevel=0.01)
    for k in (-1, 1):
        box(f'SeatGrabPost_{ax:+.0f}{s:+d}{k}', (ax + k * 0.74, s * (W2 - 0.56), 1.26), (0.03, 0.03, 0.30), M_STAIN, bevel=0.01)
    box(f'HousingStep_{ax:+.0f}{s:+d}', (ax, s * (abs(yi) - 0.06), FLOOR + 0.12), (2 * r + 0.30, 0.12, 0.24), M_SHELL, bevel=0.02)
# luggage racks by the middle doors (both sides): two shelves in a frame
for i, (x, s) in enumerate(((0.10, 1), (0.10, -1))):
    for k in (-1, 1):
        box(f'RackPost_{i}{k}', (x + k * 0.45, s * (W2 - 0.30), (FLOOR + 1.50) / 2), (0.04, 0.04, 1.50 - FLOOR), M_STAIN, bevel=0.01)
        box(f'RackPostBack_{i}{k}', (x + k * 0.45, s * (W2 - 0.08), (FLOOR + 1.50) / 2), (0.04, 0.04, 1.50 - FLOOR), M_STAIN, bevel=0.01)
    for z in (FLOOR + 0.05, 0.95, 1.50):
        box(f'RackShelf_{i}{z:.2f}', (x, s * (W2 - 0.19), z), (0.94, 0.26, 0.03), M_STAIN, bevel=0.006)
    ik(f'Rack_{i}', (x, s * (W2 - 0.19), 0.97), f'RackShelf_{i}0.95')
# wheelchair / pram space: fold-down rest and belt by the front right door
box('WheelchairBackrest', (4.20, -(W2 - 0.08), 1.00), (0.80, 0.06, 0.70), M_SHELL, bevel=0.02)
box('WheelchairRail', (4.20, -(W2 - 0.12), 1.45), (0.80, 0.03, 0.03), M_STAIN)
# flight information screens hanging from the ceiling, and the interior door buttons
for i, x in enumerate((-3.0, 2.0)):
    box(f'FIDSBracket_{i}', (x, 0, CEIL - 0.08), (0.06, 0.06, 0.16), M_DARK)
    box(f'FIDSScreen_{i}', (x, 0, CEIL - 0.33), (0.05, 0.62, 0.36), M_DARK, bevel=0.01)
    box(f'FIDSFace_{i}', (x + 0.027, 0, CEIL - 0.33), (0.004, 0.56, 0.31), M_SCREEN)
    box(f'FIDSFaceB_{i}', (x - 0.027, 0, CEIL - 0.33), (0.004, 0.56, 0.31), M_SCREEN)
for xc, s in DOORS:
    box(f'DoorButtonIn_{xc:+.2f}{s:+d}', (xc - DOOR_W / 2 - 0.06, s * (W2 - 0.083), FLOOR + 1.10), (0.06, 0.012, 0.08), M_GREEN)
    box(f'EmergencyRelease_{xc:+.2f}{s:+d}', (xc + DOOR_W / 2 + 0.06, s * (W2 - 0.083), FLOOR + 2.05), (0.08, 0.012, 0.10), M_REDP)
# #endregion

# #region ── Driver's cab (front left): partition with door, dashboard, wheel, seat, pedals, door controls ──
CY0, CY1 = 0.25, W2 - 0.06
box('CabPartitionBack', (CAB_X0, (CY0 + CY1) / 2, (FLOOR + 1.20) / 2), (0.04, CY1 - CY0, 1.20 - FLOOR), M_SHELL, bevel=0.01)
box('CabPartitionBackGlass', (CAB_X0, (CY0 + CY1) / 2, (1.20 + 2.10) / 2), (0.02, CY1 - CY0 - 0.06, 0.90), M_GLASS)
box('CabPartitionBackTop', (CAB_X0, (CY0 + CY1) / 2, 2.13), (0.04, CY1 - CY0, 0.06), M_DARK, bevel=0.01)
box('CabPartitionSide', ((CAB_X0 + FXF - 0.10) / 2 + 0.40, CY0, (FLOOR + 1.20) / 2), (FXF - 0.10 - CAB_X0 - 0.80, 0.04, 1.20 - FLOOR), M_SHELL, bevel=0.01)
box('CabDoor', (CAB_X0 + 0.40, CY0, (FLOOR + 1.20) / 2), (0.76, 0.03, 1.20 - FLOOR - 0.04), M_SHELL, bevel=0.01)
box('CabDoorHandle', (CAB_X0 + 0.70, CY0 - 0.024, 0.95), (0.10, 0.02, 0.03), M_STAIN)
piv('CabDoor', (CAB_X0 + 0.02, CY0, FLOOR), 'CabDoor')
cyl('CabPartitionPost', (CAB_X0, CY0, (FLOOR + 2.16) / 2), 0.03, 2.16 - FLOOR, 'z', M_DARK, 16)
dash = prism('Dashboard', [(FXF - 0.10, 0.35), (FXF - 0.10, 1.08), (FXF - 0.45, 1.08), (FXF - 0.70, 0.95), (FXF - 0.70, 0.35)], CY0 + 0.04, CY1, M_SHELL, bevel=0.02, segs=2)
box('DashCluster', (FXF - 0.62, 0.85, 1.02), (0.02, 0.36, 0.14), M_SCREEN)
box('DashDoorPanel', (FXF - 0.55, 0.40, 1.04), (0.20, 0.22, 0.03), M_PLAST, bevel=0.006)
for j in range(4):
    cyl(f'DoorSwitch_{j}', (FXF - 0.55, 0.32 + j * 0.05, 1.06), 0.012, 0.014, 'z', M_GREEN if j % 2 == 0 else M_REDP, 12)
SX, SY, SZ = 5.70, 0.85, 0.82
box('DriverSeatBase', (SX, SY, (FLOOR + SZ - 0.10) / 2), (0.36, 0.36, SZ - 0.10 - FLOOR), M_DARK, bevel=0.01)
box('DriverSeatCushion', (SX, SY, SZ - 0.05), (0.50, 0.50, 0.11), M_LEATHER, bevel=0.035, segs=3)
sb = box('DriverSeatBack', (SX - 0.25, SY, SZ + 0.36), (0.10, 0.48, 0.70), M_LEATHER, bevel=0.035, segs=3)
turn(sb, (SX - 0.25, SY, SZ), 'y', -14)
box('DriverHeadrest', (SX - 0.27, SY, SZ + 0.79), (0.08, 0.26, 0.18), M_LEATHER, bevel=0.03, segs=2)
turn(bpy.data.objects['DriverHeadrest'], (SX - 0.25, SY, SZ), 'y', -14)
col_a = math.radians(30)
c0 = Vector((FXF - 0.62, SY, 0.92)); c1 = c0 + Vector((-0.36 * math.cos(col_a), 0, 0.36 * math.sin(col_a)))
L_ = (c1 - c0).length; cc = (c0 + c1) / 2
turn(cyl('SteeringColumn', (cc.x, SY, cc.z), 0.035, L_, 'x', M_DARK, 20), (cc.x, SY, cc.z), 'y', -(180 - math.degrees(col_a)))
wc = c1 + Vector((-0.02 * math.cos(col_a), 0, 0.02 * math.sin(col_a)))
WDEG = -(180 - math.degrees(col_a)) + 90 - 90
turn(torus('SteeringWheel', (wc.x, SY, wc.z), 0.25, 0.018, 'x', M_PLAST, 56, 12), (wc.x, SY, wc.z), 'y', -(90 - math.degrees(col_a)))
turn(cyl('SteeringHub', (wc.x, SY, wc.z), 0.06, 0.05, 'x', M_PLAST, 24, bevel=0.01), (wc.x, SY, wc.z), 'y', -(90 - math.degrees(col_a)))
for j in range(3):
    a = math.radians(90 + j * 120); c = (wc.x, SY + 0.15 * math.cos(a), wc.z + 0.15 * math.sin(a))
    sp = turn(box(f'SteeringSpoke_{j}', c, (0.018, 0.20, 0.025), M_PLAST, bevel=0.004), c, 'x', math.degrees(a))
    turn(sp, (wc.x, SY, wc.z), 'y', -(90 - math.degrees(col_a)))
for j, (yy, nm) in enumerate(((SY - 0.12, 'Accelerator'), (SY + 0.08, 'Brake'))):
    ped = box(f'Pedal_{nm}', (FXF - 0.80, yy, FLOOR + 0.12), (0.03, 0.08, 0.18 if j == 0 else 0.10), M_PLAST, bevel=0.005)
    turn(ped, (FXF - 0.80, yy, FLOOR + 0.03), 'y', 25)
    box(f'PedalArm_{nm}', (FXF - 0.74, yy, FLOOR + 0.12), (0.12, 0.02, 0.02), M_DARK)
    box(f'PedalPlinth_{nm}', (FXF - 0.68, yy, FLOOR + 0.06), (0.05, 0.05, 0.12), M_DARK)
box('SunVisor', (FXF - 0.25, 0.85, 2.32), (0.30, 0.70, 0.02), M_SHELL, bevel=0.004)
box('SunVisorHinge', (FXF - 0.12, 0.85, 2.35), (0.04, 0.70, 0.04), M_DARK)
box('SunVisorMount', (FXF - 0.12, 0.85, (2.35 + CEIL) / 2), (0.04, 0.06, CEIL - 2.35), M_DARK)
box('FireExtinguisher', (CAB_X0 + 0.10, 1.30, FLOOR + 0.25), (0.14, 0.14, 0.50), M_REDP, bevel=0.06, segs=3)
ik('DriverSeat', (SX, SY, SZ), 'DriverSeatCushion'); ik('DriverBack', (SX - 0.30, SY, SZ + 0.40), 'DriverSeatBack')
for s, nm in ((1, 'L'), (-1, 'R')): ik(f'DriverHand{nm}', (wc.x, SY + s * 0.23, wc.z), 'SteeringWheel')
ik('Accelerator', (FXF - 0.81, SY - 0.12, FLOOR + 0.14), 'Pedal_Accelerator'); ik('Brake', (FXF - 0.81, SY + 0.08, FLOOR + 0.12), 'Pedal_Brake')
ik('DoorControls', (FXF - 0.55, 0.40, 1.08), 'DashDoorPanel'); ik('CabDoorHandle', (CAB_X0 + 0.70, CY0 - 0.04, 0.95), 'CabDoorHandle')
for i, (x, y) in enumerate([(-4.0, 0.0), (-1.0, 0.3), (3.2, -0.3), (-5.6, -0.4)]):
    ik(f'StandSpot{i}', (x, y, FLOOR), 'FloorVinyl')
# #endregion

# #region ── Checks, save and export ──
fl, cl, ov, lo = K.checks('APRONBUS', ground_ok=('Tyre',), overhang_skip_prefix=(
    'Strap', 'PoleButton', 'Kingpin', 'Spindle', 'MirrorArm', 'MirrorBase', 'Wiper', 'TowEye', 'Steering', 'PedalArm', 'PedalPlinth',
    'DoorButtonOut', 'DoorPivot', 'DoorHandleBar', 'SeatGrab', 'Rack', 'FIDS', 'RailHanger', 'SunVisor', 'BatteryPodRib', 'ACFan',
    'BeaconBase', 'MudFlap', 'DoorEdgeRubber', 'DoorStep', 'TiltLight', 'Arch', 'CabDoorHandle', 'DriverHeadrest', 'Stripe', 'RubbingStrip', 'WaistRail', 'MarkerTop', 'DoorButtonIn', 'EmergencyRelease', 'DashDoorPanel'))
GLB = os.path.join(ART, 'apronbus.glb'); BLEND = os.path.join(ART, 'ApronBus.blend'); zf = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, BLEND, GLB, zf, 'APRONBUS', 'ApronBus')
json.dump(dict(parts=len(ALL), ik=[o.name for o in IKS], ik_parts=IKP, floating=fl, overhang=ov, loose=lo, flicker=last),
          open(os.path.join(ART, 'apronbus-parts.json'), 'w'), indent=1)
print('APRONBUS saved', len(ALL), 'parts', len(IKS), 'IK/pivots, flicker', last)
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
        ld = bpy.data.lights.new(nm, 'SUN'); ld.energy = e; lo_ = bpy.data.objects.new(nm, ld); sc.collection.objects.link(lo_)
        lo_.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
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
    ROOFOFF = ('Roof', 'CantRail', 'BatteryPod', 'ACUnit', 'ACFan', 'RoofHatch', 'Beacon', 'CeilingLining', 'LightStrip', 'ACDuct', 'DestinationHousing',
               'DestinationDisplay', 'MarkerTop', 'RearTopPanel', 'FIDS')
    shoot('1-three-quarter-front', (15.5, -11.5, 6.0), (0.6, 0, 1.4), 40)
    shoot('2-three-quarter-rear', (-15.0, 11.5, 6.0), (-0.4, 0, 1.4), 40)
    shoot('3-side-right', (0, -40, 1.55), (0, 0, 1.55), 50, 15.6, (2400, 900))
    shoot('4-side-left', (0, 40, 1.55), (0, 0, 1.55), 50, 15.6, (2400, 900))
    shoot('5-front', (40, 0, 1.6), (0, 0, 1.6), 50, 3.8, (1400, 1400))
    shoot('6-rear', (-40, 0, 1.6), (0, 0, 1.6), 50, 3.8, (1400, 1400))
    shoot('7-top', (0, 0, 40), (0, 0, 0), 50, 15.6, (2400, 900), rot=(0, 0, 0))
    shoot('8-inside-from-above', (0, 0, 40), (0, 0, 0), 50, 15.0, (2400, 900), hide=ROOFOFF, rot=(0, 0, 0))
    shoot('9-cutaway', (11.0, -9.0, 9.5), (0.0, 0.0, 0.8), 40, hide=ROOFOFF)
    shoot('10-drivers-cab', (CAB_X0 + 0.15, 0.55, 2.35), (FXF - 0.55, 0.85, 1.0), 22, hide=ROOFOFF + ('CabPartition', 'CabDoor'))
    shoot('11-standing-area-and-seats', (-6.0, 0.6, 2.2), (0.0, -0.3, 1.2), 28, hide=ROOFOFF)
    shoot('12-door-closeup', (1.95 + 1.8, -5.0, 1.6), (1.95, -1.5, 1.3), 40)
    shoot('13-wheels-and-arch', (-3.34 - 1.4, -4.6, 0.9), (-3.34, -1.3, 0.6), 40)
    json.dump(VIEWS, open(os.path.join(WIRE, 'views.json'), 'w'), indent=1)
# #endregion
