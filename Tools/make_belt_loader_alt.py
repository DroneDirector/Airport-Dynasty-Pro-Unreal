# The electric baggage belt loader (the conveyor that carries bags and parcels from the baggage carts up into the
# aircraft's hold) - our own design, drawn to real-world standards along the lines of the self-propelled electric belt
# loaders (TLD NBL-E class: about 8.1 m long, 2.1 m wide, 7.8 m boom, 600 mm belt, 1,200 kg on the belt; Mallaghan SkyBelt
# class: front sill height 0.84 to 4.19 m; numbers and photos in Blueprints/Vehicles/BeltLoaderAlt/RESEARCH.md). No brand
# names or logos. Built to the vehicle standard learned on the pushback tug (helpers + checks in Tools/vehicle_kit.py):
# every part sits on, hangs on or is bolted to the part under it, bevelled steel edges, lamps set into their faces.
# How it works (and how the game adjusts it to any aircraft): the boom (belt + side channels + handrails + front bumper)
# rides on two lift arms. The REAR arm is pinned to the boom; the FRONT arm carries a roller that runs in a track under the
# boom. Each arm is lifted by its own hydraulic cylinder, so the boom's rear end and front end are set separately:
# front (belt top at the door sill) 0.84 - 4.19 m, rear (where the bags go on from the carts) 0.85 - 1.30 m. The pose is
# worked out by solve(front, rear) below - the same sums are written to Art/BeltLoaderAlt/beltloader-rig.json for Unreal,
# which sets the front height from the aircraft's door-sill height (A320: forward hold ~2.11-2.18 m, Airbus AC doc).
# The model is saved in the A320 forward-hold pose (front 2.10 m, rear 0.95 m).
# Axes: x forward, y left, z up; metres; origin on the ground under the middle of the wheelbase.
#   python3 Tools/make_belt_loader_alt.py [--wire]          (bpy 5.x as a module, or Blender 3.3: blender -b --python ...)
import bpy, bmesh, math, os, sys, json, subprocess
from mathutils import Vector, Matrix
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import importlib, vehicle_kit as K
importlib.reload(K)

# #region ── Blender 4.1+ compatibility for the shared kit (auto-smooth and the Emission input were renamed) ──
def _finish(o, m, smooth=40):
    o.data.materials.clear(); o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    if hasattr(o.data, 'set_sharp_from_angle'): o.data.set_sharp_from_angle(angle=math.radians(smooth))
    else: o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(smooth)
    K.ALL.append(o); return o
K.finish = _finish
def pmat(name, col, metal=0.0, rough=0.5, emit=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission Color' if 'Emission Color' in b.inputs else 'Emission'].default_value = (*col, 1)
        b.inputs['Emission Strength'].default_value = emit
    return m
# #endregion

from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, lamp, ALL, IKS
ART = os.path.join(ROOT, 'Art', 'BeltLoaderAlt'); os.makedirs(ART, exist_ok=True)
BP = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'BeltLoaderAlt')
WIRE = os.path.join(BP, 'wire'); os.makedirs(WIRE, exist_ok=True)
DO_WIRE = '--wire' in sys.argv
K.reset()
IKP = {}
def ik(name, loc, part=None):
    IKP['IK_' + name] = part; return K.ik(name, loc)
def piv(name, loc, part=None):
    IKP['PIV_' + name] = part; return K.ik(name, loc, prefix='PIV_')

# #region ── The main sizes (m) ──
FX, RX = 1.50, -1.50                       # front (steering) / rear (driven) axle - wheelbase 3000
TRK2 = 0.86                                # wheel centres y +-0.86 (track 1720)
TD, TW, RB = 0.696, 0.225, 0.1905          # 225/70 R15: 696 dia, 225 wide, 15 in rim
TZ = TD / 2
CH_X0, CH_X1 = -2.35, 2.35                 # chassis rear / front face (4700)
CH_Y = 0.95                                # chassis half width (1900; 2100 over the fenders)
DECK = 0.78                                # the driver's deck (left)
CRADLE = 0.55                              # the boom's cradle (right), where the boom rests low
BY0, BY1 = -0.83, -0.13                    # the boom's sides (700 wide), right of the middle; the driver sits left of it
BYC = (BY0 + BY1) / 2
BL = 7.80                                  # boom (belt) length
BD = 0.26                                  # belt top to the boom's bottom chord
SIDE_UP = 0.10                             # the side channels stand 100 above the belt (guides)
BELT_W = 0.60
# lift arms (pivots on the chassis cradle, at the boom's middle line)
P_R = Vector((-2.25, 0.0, CRADLE)); A_R = 1.00; S_R = 1.35        # rear arm: pivot, length, its pin 1350 from the boom's rear end
P_F = Vector((-0.20, 0.0, CRADLE)); A_F = 2.70                      # front arm: pivot, length (its roller runs under the boom)
C_R = Vector((-1.35, 0.0, 0.36)); C_F = Vector((-1.15, 0.0, 0.36)) # the cylinders' feet
FRONT_H, REAR_H = 2.10, 0.95               # the pose saved: A320 forward hold
# #endregion


# #region ── The lift solver (the same sums go to beltloader-rig.json) ──
def solve(front, rear):
    """Boom pose for a front belt-top height and a rear belt-top height: returns the boom angle, the boom's rear end (belt
    top), the rear pin, the front roller point, both arm angles and both cylinders' lengths."""
    th = math.asin((front - rear) / BL)
    u = Vector((math.cos(th), 0, math.sin(th))); n = Vector((-math.sin(th), 0, math.cos(th)))
    bz = rear + S_R * math.sin(th) - BD * math.cos(th)                    # rear pin height
    dz = bz - P_R.z; assert abs(dz) < A_R, 'rear arm cannot reach'
    bx = P_R.x + math.sqrt(A_R ** 2 - dz ** 2)
    B = Vector((bx, 0, bz)); R = B - S_R * u + BD * n                     # boom rear end (belt top)
    # front roller: where the bottom chord (through B, along u) meets the front arm's circle
    d = B - P_F; b = 2 * d.dot(u); c = d.dot(d) - A_F ** 2; disc = b * b - 4 * c
    assert disc >= 0, 'front arm cannot reach'
    t = (-b + math.sqrt(disc)) / 2; T = B + t * u
    ar = math.atan2(bz - P_R.z, bx - P_R.x); af = math.atan2(T.z - P_F.z, T.x - P_F.x)
    return dict(th=th, u=u, n=n, R=R, B=B, T=T, ar=ar, af=af, F=R + BL * u)
POSE = solve(FRONT_H, REAR_H)
# #endregion


# #region ── Materials (plain; CC0 photo set swapped in later, as the other vehicles) ──
M_PAINT = pmat('BL_BodyPaint', (0.85, 0.62, 0.05), 0.2, 0.45)       # GSE yellow (the tug's paint)
M_BOOM = pmat('BL_BoomGalv', (0.62, 0.64, 0.66), 0.9, 0.45)         # galvanised boom
M_STEEL = pmat('BL_Steel', (0.35, 0.36, 0.38), 1.0, 0.45)
M_DARK = pmat('BL_DarkSteel', (0.08, 0.08, 0.09), 0.6, 0.55)
M_RUB = pmat('BL_Tyre', (0.03, 0.03, 0.03), 0.0, 0.9)
M_BELT = pmat('BL_BeltRubber', (0.04, 0.04, 0.045), 0.0, 0.85)
M_BUMP = pmat('BL_BumperRubber', (0.03, 0.03, 0.03), 0.0, 0.8)
M_PLAST = pmat('BL_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_VINYL = pmat('BL_SeatVinyl', (0.05, 0.05, 0.06), 0.0, 0.6)
M_TREAD = pmat('BL_TreadPlate', (0.5, 0.5, 0.52), 1.0, 0.4)
M_CHROME = pmat('BL_ChromeRod', (0.85, 0.86, 0.88), 1.0, 0.12)
M_REDP = pmat('BL_RedPaint', (0.7, 0.04, 0.03), 0.0, 0.4)
M_YELP = pmat('BL_YellowPaint', (0.95, 0.75, 0.05), 0.0, 0.4)
M_BATT = pmat('BL_BatteryCase', (0.10, 0.11, 0.12), 0.0, 0.5)
M_MIRROR = pmat('BL_MirrorGlass', (0.8, 0.85, 0.9), 1.0, 0.02)
M_AMBER = pmat('BL_AmberLens', (1.0, 0.55, 0.05), 0.0, 0.2, 2.0)
M_RED = pmat('BL_RedLens', (0.9, 0.05, 0.03), 0.0, 0.2, 1.0)
M_WHITE = pmat('BL_WhiteLens', (0.95, 0.95, 0.9), 0.0, 0.1, 1.5)
M_SCREEN = pmat('BL_Screen', (0.05, 0.25, 0.35), 0.0, 0.1, 0.8)
M_GREEN = pmat('BL_GreenButton', (0.05, 0.8, 0.15), 0.0, 0.2, 1.0)
M_HOSE = pmat('BL_HydraulicHose', (0.02, 0.02, 0.02), 0.0, 0.6)
# #endregion


# #region ── Part helpers: beams between two points, boom-frame parts ──
def beam(name, p0, p1, y, wy, h, m, bevel=0.006):
    """A box beam from p0 to p1 (in the x-z plane at y), wy wide across, h deep."""
    p0, p1 = Vector(p0), Vector(p1); L = (p1 - p0).length; c = (p0 + p1) / 2
    o = box(name, (c.x, y, c.z), (L, wy, h), m, bevel=bevel)
    a = math.atan2(p1.z - p0.z, p1.x - p0.x)
    return turn(o, (c.x, y, c.z), 'y', -math.degrees(a))
def rod(name, p0, p1, y, r, m, verts=24):
    p0, p1 = Vector(p0), Vector(p1); L = (p1 - p0).length; c = (p0 + p1) / 2
    o = cyl(name, (c.x, y, c.z), r, L, 'x', m, verts)
    a = math.atan2(p1.z - p0.z, p1.x - p0.x)
    return turn(o, (c.x, y, c.z), 'y', -math.degrees(a))
BOOM_PARTS = []
def boomframe(fn):
    """Builds parts in the boom's own frame (x = along the belt from its rear end, z = up from the belt top) and moves
    them onto the boom's pose."""
    n0 = len(ALL); fn(); P = POSE
    M = Matrix.Translation(P['R']) @ Matrix.Rotation(-P['th'], 4, 'Y')
    for o in ALL[n0:]:
        o.data.transform(M); o.data.update(); BOOM_PARTS.append(o)
def bp(s, w=0.0): return Vector(POSE['R'] + s * POSE['u'] + w * POSE['n'])     # a point on the boom
# #endregion


# #region ── Wheels (lathed tyres and dished rims, the tractor's profiles scaled to 225/70 R15) ──
TUG_PROF = [(0.290, -0.124), (0.318, -0.146), (0.360, -0.157), (0.410, -0.1585), (0.460, -0.154), (0.500, -0.142), (0.522, -0.124),
            (0.534, -0.100), (0.538, -0.060), (0.539, 0.0), (0.538, 0.060), (0.534, 0.100), (0.522, 0.124), (0.500, 0.142),
            (0.460, 0.154), (0.410, 0.1585), (0.360, 0.157), (0.318, 0.146), (0.290, 0.124), (0.286, 0.0)]
TUG_RIM = [(0.300, -0.130), (0.296, -0.122), (0.284, -0.118), (0.270, -0.070), (0.262, -0.050), (0.262, 0.050), (0.270, 0.070),
           (0.284, 0.118), (0.296, 0.122), (0.300, 0.130), (0.288, 0.132), (0.276, 0.122), (0.256, 0.060), (0.256, -0.060), (0.276, -0.122), (0.288, -0.132)]
def wheel(key, x, y, s):
    R, rb, tw, zc = TZ, RB, TW, TZ; q = tw / 0.165; k = rb / 0.135
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
    ik(f'Wheel_{key}', (x, y, zc), f'Hub_{key}')
WELL_Y = 0.66                              # the wheel wells' inner walls (front wheels steer 35 deg inside)
for s, sd in ((1, 'L'), (-1, 'R')):
    for ax, tag in ((FX, 'F'), (RX, 'R')):
        stub_y0 = WELL_Y - 0.02; stub_y1 = TRK2 + 0.035 * TW / 0.165
        if tag == 'F':
            cyl(f'Kingpin_{sd}', (ax, s * (WELL_Y + 0.05), TZ), 0.034, 0.22, 'z', M_DARK, 24, bevel=0.004)
            box(f'KingpinBoss_{sd}', (ax, s * (WELL_Y + 0.015), TZ), (0.10, 0.05, 0.16), M_DARK, bevel=0.006)
            cyl(f'Spindle_F{sd}', (ax, s * (WELL_Y + 0.05 + stub_y1) / 2, TZ), 0.03, stub_y1 - WELL_Y - 0.05, 'y', M_STEEL, 20)
            cyl(f'SteerArm_F{sd}', (ax - 0.08, s * (WELL_Y + 0.05), TZ + 0.07), 0.016, 0.16, 'x', M_DARK, 12)
            ik(f'Steer_F{sd}', (ax, s * (WELL_Y + 0.05), TZ), f'Kingpin_{sd}')
        else:
            cyl(f'RearAxle_Flange_{sd}', (ax, s * (WELL_Y + 0.006), TZ), 0.09, 0.022, 'y', M_DARK, 32, bevel=0.004)
            cyl(f'RearAxle_Stub_{sd}', (ax, s * (stub_y0 + stub_y1) / 2, TZ), 0.045, stub_y1 - stub_y0, 'y', M_STEEL, 24)
        wheel(tag + sd, ax, s * TRK2, s)
# the tie rod between the steering arms (behind the front axle)
rod('TieRod', (FX - 0.08, 0, TZ + 0.07), (FX - 0.08, 0, TZ + 0.07), 0, 0.001, M_DARK) if False else None
cyl('TieRod', (FX - 0.08, 0, TZ + 0.07), 0.015, 2 * (WELL_Y + 0.05), 'y', M_DARK, 12)
# #endregion


# #region ── The chassis: rails and cross members, the boom's low cradle, the driver's deck, fenders, ends ──
for s in (1, -1):
    box(f'ChassisRail_{"L" if s > 0 else "R"}', (0, s * 0.56, 0.42), (CH_X1 - CH_X0 - 0.10, 0.10, 0.22), M_DARK, bevel=0.008)
for i, x in enumerate((-2.20, -1.20, 0.0, 1.20, 2.20)):
    box(f'CrossMember_{i}', (x, 0, 0.42), (0.10, 1.22, 0.18), M_DARK, bevel=0.006)
# the cradle (right half): a pressed tray the boom rests in, its floor at CRADLE
cr = box('Cradle', (0.0, (BY0 - 0.05 + BY1 + 0.05) / 2, (0.52 + CRADLE) / 2), (CH_X1 - CH_X0, BY1 - BY0 + 0.10, CRADLE - 0.52 + 0.001), M_PAINT, bevel=0.01)
for s, y in ((1, BY1 + 0.075), (-1, BY0 - 0.075)):
    box(f'CradleWall_{"L" if s > 0 else "R"}', (0.0, y, (0.52 + DECK) / 2), (CH_X1 - CH_X0, 0.05, DECK - 0.52), M_PAINT, bevel=0.01)
# the driver's deck (left half) - chequer plate on the body
box('BodyLeft', (0.0, (BY1 + 0.10 + CH_Y) / 2, (0.52 + DECK - 0.006) / 2), (CH_X1 - CH_X0, CH_Y - BY1 - 0.10, DECK - 0.006 - 0.52), M_PAINT, bevel=0.015)
box('DeckPlate', (-0.55, (BY1 + 0.11 + CH_Y - 0.01) / 2, DECK - 0.003), (2.2, CH_Y - BY1 - 0.12, 0.008), M_TREAD, bevel=0.002)
# the right body side under the boom (a skirt panel with the lift cylinder openings left clear)
box('SkirtRight', (0.0, BY0 - 0.12, 0.66), (CH_X1 - CH_X0, 0.04, 0.24), M_PAINT, bevel=0.01)
# front and rear end panels with rubber bumpers
for x, sg, nm in ((CH_X1, 1, 'Front'), (CH_X0, -1, 'Rear')):
    box(f'EndPanel_{nm}', (x - sg * 0.03, 0.0, 0.60), (0.06, 2 * CH_Y, 0.42), M_PAINT, bevel=0.012)
    box(f'Bumper_{nm}', (x + sg * 0.045, 0.0, 0.46), (0.09, 2 * CH_Y - 0.10, 0.14), M_BUMP, bevel=0.025, segs=3)
# fenders over the four wheels (arched bands, chequer plate on top, anti-slip for stepping)
def arch_band(name, ax, zc, r0, r1, y0, y1, m, a0=0, a1=180, seg=32, z_floor=None):
    """A curved band round the y axis (a fender): quads only, so it shades cleanly; legs run down to z_floor."""
    bm = bmesh.new(); rows = []
    angs = [a0 + (a1 - a0) * j / seg for j in range(seg + 1)]
    pts = [(ax + r * math.cos(math.radians(a)), zc + r * math.sin(math.radians(a))) for a in angs for r in (r0,)]
    def ring(r): return [(ax + r * math.cos(math.radians(a)), zc + r * math.sin(math.radians(a))) for a in angs]
    inn, out = ring(r0), ring(r1)
    if z_floor is not None:
        inn = [(inn[0][0], z_floor)] + inn + [(inn[-1][0], z_floor)]; out = [(out[0][0], z_floor)] + out + [(out[-1][0], z_floor)]
    V = {}
    for k, (pl, tag) in enumerate(((inn, 'i'), (out, 'o'))):
        for j, (x, z) in enumerate(pl):
            V[(tag, j, 0)] = bm.verts.new((x, y0, z)); V[(tag, j, 1)] = bm.verts.new((x, y1, z))
    n = len(inn)
    for j in range(n - 1):
        bm.faces.new((V[('i', j, 0)], V[('i', j + 1, 0)], V[('i', j + 1, 1)], V[('i', j, 1)]))
        bm.faces.new((V[('o', j, 0)], V[('o', j, 1)], V[('o', j + 1, 1)], V[('o', j + 1, 0)]))
        for e in (0, 1): bm.faces.new((V[('i', j, e)], V[('o', j, e)], V[('o', j + 1, e)], V[('i', j + 1, e)]))
    for j in (0, n - 1): bm.faces.new((V[('i', j, 0)], V[('i', j, 1)], V[('o', j, 1)], V[('o', j, 0)]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return K.finish(K.link(name, bm), m, 30)
for ax, tag in ((FX, 'F'), (RX, 'R')):
    for s, sd in ((1, 'L'), (-1, 'R')):
        r = TZ + 0.09
        y0, y1 = sorted((s * (CH_Y - 0.02), s * 1.05))
        arch_band(f'Fender_{tag}{sd}', ax, TZ, r, r + 0.06, y0, y1, M_PAINT, 0, 180, 32, z_floor=0.52)
        ty0, ty1 = sorted((s * CH_Y, s * 1.04))
        arch_band(f'FenderTread_{tag}{sd}', ax, TZ, r + 0.058, r + 0.066, ty0, ty1, M_TREAD, 60, 120, 12)
        box(f'WellWall_{tag}{sd}', (ax, s * (WELL_Y - 0.01), 0.55), (2 * r, 0.02, 0.30), M_DARK)
# #endregion


# #region ── Power: 80 V lithium battery box, drive motor, hydraulic pump and tank, controller, charge socket ──
box('BatteryBox', (-0.55, 0.52, 0.40), (1.30, 0.62, 0.34), M_BATT, bevel=0.01)
for i in range(2):
    box(f'BatteryLid_{i}', (-0.86 + i * 0.64, 0.52, 0.575), (0.60, 0.60, 0.012), M_STEEL, bevel=0.003)
    box(f'BatteryLidHandle_{i}', (-0.86 + i * 0.64, 0.52, 0.59), (0.14, 0.03, 0.02), M_DARK, bevel=0.004)
box('BatteryVent', (-0.55, CH_Y + 0.002, 0.40), (0.50, 0.006, 0.08), M_DARK) if False else None
box('MotorController', (0.75, 0.55, 0.62), (0.50, 0.40, 0.18), M_DARK, bevel=0.01)
cyl('DriveMotor', (RX + 0.45, -0.10, 0.36), 0.13, 0.36, 'y', M_DARK, 32, bevel=0.01)
box('DiffHousing', (RX, 0.0, TZ), (0.26, 0.40, 0.24), M_DARK, bevel=0.03, segs=3)
cyl('RearAxleTube', (RX, 0.0, TZ), 0.05, 2 * WELL_Y, 'y', M_DARK, 24)
cyl('HydraulicTank', (1.95, 0.55, 0.62), 0.14, 0.60, 'y', M_STEEL, 32, bevel=0.01)
cyl('HydraulicTankCap', (1.95, 0.55, 0.775), 0.035, 0.03, 'z', M_DARK, 20)
box('PumpUnit', (1.95, 0.20, 0.62), (0.30, 0.24, 0.26), M_DARK, bevel=0.01)
cyl('PumpMotor', (1.95, 0.20, 0.79), 0.08, 0.10, 'z', M_DARK, 24)
lamp('ChargeSocket', (CH_X0, 0.12, 0.68), 'x-', 0.12, 0.10, M_PLAST, M_DARK, M_PLAST, depth=0.05)
# #endregion


# #region ── Lift arms, pins and hydraulic cylinders (the boom's two supports) ──
P = POSE
AY = (BYC - 0.26, BYC + 0.26)              # each arm is a pair of plates at these y
for k, y in enumerate(AY):
    beam(f'RearArm_{k}', P_R, P['B'], y, 0.04, 0.14, M_PAINT)
    beam(f'FrontArm_{k}', P_F, P['T'], y, 0.05, 0.16, M_PAINT)
    box(f'RearArmFoot_{k}', (P_R.x, y, (0.52 + P_R.z) / 2 + 0.01), (0.16, 0.05, P_R.z - 0.52 + 0.02), M_DARK, bevel=0.005)
    box(f'FrontArmFoot_{k}', (P_F.x, y, (0.52 + P_F.z) / 2 + 0.01), (0.18, 0.06, P_F.z - 0.52 + 0.02), M_DARK, bevel=0.005)
cyl('RearArmPivotPin', (P_R.x, BYC, P_R.z), 0.025, 0.62, 'y', M_STEEL, 20)
cyl('FrontArmPivotPin', (P_F.x, BYC, P_F.z), 0.03, 0.64, 'y', M_STEEL, 20)
cyl('RearArmCrossTube', (P_R.x + 0.6 * (P['B'].x - P_R.x), BYC, P_R.z + 0.6 * (P['B'].z - P_R.z)), 0.035, 0.56, 'y', M_DARK, 20)
fmid = P_F + 0.55 * (P['T'] - P_F)
cyl('FrontArmCrossTube', (fmid.x, BYC, fmid.z), 0.04, 0.56, 'y', M_DARK, 20)
cyl('FrontArmTopTube', (P['T'].x, BYC, P['T'].z), 0.03, 0.62, 'y', M_STEEL, 20)
for k, y in enumerate(AY):
    cyl(f'FrontRoller_{k}', (P['T'].x, y, P['T'].z + 0.0), 0.055, 0.07, 'y', M_DARK, 32, bevel=0.004)
cyl('RearBoomPin', (P['B'].x, BYC, P['B'].z), 0.03, 0.64, 'y', M_STEEL, 20)
def ram(name, foot, end, rb=0.055, rr=0.03):
    """A hydraulic cylinder: barrel from its foot pin, chrome rod out of it to the end pin (the rod slides in the game)."""
    foot, end = Vector(foot), Vector(end); L = (end - foot).length; d = (end - foot) / L
    lb = min(0.62, L * 0.62)
    rod(name + '_Barrel', foot, foot + d * lb, BYC, rb, M_PAINT, 32)
    rod(name + '_Gland', foot + d * (lb - 0.03), foot + d * (lb + 0.02), BYC, rb + 0.008, M_DARK, 32)
    rod(name + '_Rod', foot + d * (lb - 0.10), end, BYC, rr, M_CHROME, 24)
    cyl(name + '_FootEye', (foot.x, BYC, foot.z), rb + 0.012, 0.08, 'y', M_DARK, 24)
    cyl(name + '_RodEye', (end.x, BYC, end.z), rr + 0.016, 0.06, 'y', M_DARK, 24)
    tube(name + '_Hose', [(foot.x + 0.10 * d.x, BYC + 0.07, foot.z + 0.10 * d.z), (foot.x - 0.05, BYC + 0.12, foot.z - 0.05), (0.75, 0.40, 0.55)], 0.01, M_HOSE)
    piv(name + '_Foot', foot, name + '_FootEye'); piv(name + '_End', end, name + '_RodEye')
rear_end = P_R + 0.6 * (P['B'] - P_R)
ram('RearLiftCyl', C_R, rear_end)
ram('FrontLiftCyl', C_F, fmid, 0.065, 0.036)
for k, y in enumerate((BYC - 0.06, BYC + 0.06)):
    box(f'CylFootBracket_R{k}', (C_R.x, y, (C_R.z + 0.30) / 2), (0.12, 0.02, C_R.z - 0.30 + 0.07), M_DARK, bevel=0.004)
    box(f'CylFootBracket_F{k}', (C_F.x, y, (C_F.z + 0.30) / 2), (0.12, 0.02, C_F.z - 0.30 + 0.07), M_DARK, bevel=0.004)
box('CylFootBase', ((C_R.x + C_F.x) / 2, BYC, 0.32), (0.40, 0.20, 0.05), M_DARK, bevel=0.004)
piv('RearArm', P_R, 'RearArmPivotPin'); piv('FrontArm', P_F, 'FrontArmPivotPin')
piv('BoomPin', P['B'], 'RearBoomPin'); piv('FrontRoller', P['T'], 'FrontArmTopTube')
# #endregion


# #region ── The boom: side channels with lightening slots, roller bed, the belt and its drums, guides, bumpers, handrails ──
def build_boom():
    wy = BY1 - BY0
    for s, y in ((1, BY1 - 0.03), (-1, BY0 + 0.03)):
        ch = box(f'Boom_Channel_{"L" if s > 0 else "R"}', (BL / 2, y, (SIDE_UP - BD) / 2), (BL, 0.06, SIDE_UP + BD), M_BOOM, bevel=0.008)
        for i in range(11):
            cut(ch, box('Slot', (0.75 + i * 0.63, y + s * 0.03, -0.09), (0.36, 0.03, 0.09), M_DARK, bevel=0.02, segs=4))
        box(f'Boom_ChannelLip_{"L" if s > 0 else "R"}', (BL / 2, y - s * 0.02, SIDE_UP + 0.003), (BL - 0.02, 0.10, 0.008), M_BOOM, bevel=0.002)
    # bottom chord track (the front roller runs in it) and cross members
    for k, y in enumerate((BYC - 0.26, BYC + 0.26)):
        box(f'Boom_Track_{k}', (BL / 2, y, -BD + 0.03), (BL - 0.4, 0.10, 0.06), M_STEEL, bevel=0.004)
    for i in range(9):
        box(f'Boom_Cross_{i}', (0.45 + i * 0.86, BYC, -BD + 0.08), (0.08, wy - 0.12, 0.10), M_BOOM, bevel=0.004)
    box('Boom_RollerBed', (BL / 2, BYC, -0.03), (BL - 0.40, BELT_W + 0.02, 0.03), M_STEEL, bevel=0.003)
    # the belt: top run, return run below and the wrap round each drum
    box('Boom_Belt', (BL / 2, BYC, -0.006), (BL - 0.22, BELT_W, 0.012), M_BELT, bevel=0.002)
    box('Boom_BeltReturn', (BL / 2, BYC, -0.20), (BL - 0.30, BELT_W, 0.012), M_BELT, bevel=0.002)
    for x, r, nm in ((0.12, 0.10, 'DriveDrum'), (BL - 0.10, 0.09, 'IdlerDrum')):
        cyl(f'Boom_{nm}', (x, BYC, -r - 0.012 + 0.0), r, BELT_W + 0.04, 'y', M_STEEL, 40)
        cyl(f'Boom_{nm}Shaft', (x, BYC, -r - 0.012), 0.025, wy - 0.01, 'y', M_DARK, 20)
        lathe(f'Boom_{nm}Wrap', [(r + 0.012, -BELT_W / 2), (r + 0.012, BELT_W / 2), (r + 0.001, BELT_W / 2), (r + 0.001, -BELT_W / 2)], (x, BYC, -r - 0.012), M_BELT, 40)
    # the belt's drive motor on the rear drum (outside the right channel)
    cyl('Boom_BeltMotor', (0.12, BY0 - 0.055, -0.11), 0.07, 0.12, 'y', M_DARK, 28, bevel=0.006)
    # cleats: low rubber bars across the belt every 0.6 m (grip on the slope)
    for i in range(12):
        box(f'Boom_Cleat_{i}', (0.35 + i * 0.62, BYC, 0.006), (0.03, BELT_W - 0.06, 0.012), M_BELT, bevel=0.004)
    # the left fixed guide (makes the boom 780 wide) and the rear loading flares (bags slide in from the carts)
    box('Boom_LeftGuide', (BL / 2 + 0.6, BY1 + 0.03, 0.17), (BL - 1.6, 0.03, 0.16), M_BOOM, bevel=0.004)
    for i in range(7):
        box(f'Boom_LeftGuideBracket_{i}', (1.7 + i * 0.95, BY1 - 0.005, 0.15), (0.05, 0.10, 0.05), M_BOOM, bevel=0.003)
    for s, y in ((1, BY1 + 0.02), (-1, BY0 - 0.02)):
        f = box(f'Boom_RearFlare_{"L" if s > 0 else "R"}', (0.35, y + s * 0.08, 0.16), (0.60, 0.03, 0.14), M_BOOM, bevel=0.004)
        turn(f, (0.65, y, 0.16), 'z', s * 18)
        box(f'Boom_RearFlareFoot_{"L" if s > 0 else "R"}', (0.62, y, 0.095), (0.08, 0.05, 0.03), M_BOOM, bevel=0.003)
    # front end: rubber bumper pad across the nose (touches the door sill), anti-pinch edge, nose plate
    box('Boom_NosePlate', (BL + 0.02, BYC, -0.08), (0.04, BY1 - BY0, 0.36), M_BOOM, bevel=0.006)
    box('Boom_FrontBumper', (BL + 0.10, BYC, -0.02), (0.12, BY1 - BY0 + 0.12, 0.16), M_BUMP, bevel=0.04, segs=4)
    box('Boom_AntiPinchEdge', (BL + 0.17, BYC, -0.02), (0.03, BY1 - BY0 + 0.06, 0.08), M_YELP, bevel=0.01, segs=2)
    # rear end: a roller nose over the drive drum and the rear bumper
    box('Boom_RearBumper', (-0.03, BYC, -0.13), (0.10, BY1 - BY0, 0.14), M_BUMP, bevel=0.03, segs=3)
    # handrails on both sides of the front 3 m (raised: 1.0 m above the belt), folding at their foot hinges
    for s, y in ((1, BY1 - 0.03), (-1, BY0 + 0.03)):
        sd = 'L' if s > 0 else 'R'
        xs = [BL - 3.0, BL - 2.0, BL - 1.0, BL - 0.15]
        for j, x in enumerate(xs):
            box(f'Boom_RailHinge_{sd}{j}', (x, y - s * 0.02, SIDE_UP + 0.022), (0.06, 0.05, 0.04), M_DARK, bevel=0.005)
            cyl(f'Boom_RailPost_{sd}{j}', (x, y, SIDE_UP + 0.04 + 0.48), 0.021, 0.96, 'z', M_YELP, 16)
        for z, nm in ((SIDE_UP + 1.00, 'Top'), (SIDE_UP + 0.55, 'Mid')):
            cyl(f'Boom_Rail{nm}_{sd}', ((xs[0] + xs[-1]) / 2, y, z), 0.021, xs[-1] - xs[0] + 0.04, 'x', M_YELP, 16)
    # the front control box (belt in / out, boom up / down, emergency stop) on the left rail's front post
    x = BL - 0.40
    box('Boom_ControlBox', (x, BY1 + 0.06, SIDE_UP + 0.80), (0.20, 0.09, 0.26), M_YELP, bevel=0.012)
    box('Boom_ControlBoxArm', (x, BY1 + 0.006, SIDE_UP + 0.80), (0.06, 0.03, 0.06), M_DARK)
    for j, (zz, mm) in enumerate(((0.88, M_GREEN), (0.80, M_WHITE), (0.72, M_WHITE))):
        cyl(f'Boom_CtlButton_{j}', (x - 0.04, BY1 + 0.11, SIDE_UP + zz), 0.016, 0.012, 'y', mm, 16)
        cyl(f'Boom_CtlButton_{j}b', (x + 0.04, BY1 + 0.11, SIDE_UP + zz), 0.016, 0.012, 'y', M_WHITE if j else M_REDP, 16)
    cyl('Boom_EStopFront', (x, BY1 + 0.07, SIDE_UP + 0.94), 0.03, 0.03, 'z', M_REDP, 24, bevel=0.006)
    # work light under the nose (lights the hold's floor) and an amber strobe on the right rail's front post
    lamp('Boom_WorkLight', (BL - 0.25, BYC, -BD + 0.0), 'x+', 0.14, 0.08, M_WHITE, M_DARK, M_PLAST)
    box('Boom_WorkLightBracket', (BL - 0.32, BYC, -BD + 0.06), (0.10, 0.16, 0.06), M_DARK, bevel=0.004)
    cyl('Boom_StrobeBase', (BL - 0.15, BY0 + 0.03, SIDE_UP + 1.03), 0.035, 0.03, 'z', M_DARK, 20)
    cyl('Boom_Strobe', (BL - 0.15, BY0 + 0.03, SIDE_UP + 1.07), 0.03, 0.05, 'z', M_AMBER, 20)
    # emergency stops at the rear corners of the boom
    for s, y in ((1, BY1 + 0.03), (-1, BY0 - 0.03)):
        box(f'Boom_EStopRearBox_{"L" if s > 0 else "R"}', (0.30, y + s * 0.025, 0.02), (0.10, 0.05, 0.10), M_YELP, bevel=0.008)
        cyl(f'Boom_EStopRear_{"L" if s > 0 else "R"}', (0.30, y + s * 0.06, 0.02), 0.028, 0.025, 'y', M_REDP, 24, bevel=0.006)
boomframe(build_boom)
piv('BoomRearEnd', POSE['R'], 'Boom_RearBumper'); piv('BoomTip', POSE['F'], 'Boom_NosePlate')
piv('BeltDrive', bp(0.12, -0.11), 'Boom_DriveDrum'); piv('BeltIdler', bp(BL - 0.10, -0.10), 'Boom_IdlerDrum')
ik('BoomFrontPanel', bp(BL - 0.40, SIDE_UP + 0.80) + Vector((0, BY1 + 0.11, 0)), 'Boom_ControlBox')
ik('StandOnBeltFront', bp(BL - 1.2, 0.0) + Vector((0, BYC, 0)), 'Boom_Belt')
# #endregion


# #region ── The operator station (left): seat, steering, pedals, dash, boom controls, protection grid and roof guard ──
SX, SY, SZ = -0.55, 0.52, 1.10             # seat cushion top
box('SeatRiser', (SX, SY, (DECK + SZ - 0.12) / 2), (0.40, 0.40, SZ - 0.12 - DECK), M_DARK, bevel=0.01)
for i in range(4):
    box(f'SeatBellows_{i}', (SX, SY, DECK + 0.06 + i * 0.05), (0.42 - 0.02 * (i % 2), 0.42 - 0.02 * (i % 2), 0.04), M_PLAST, bevel=0.012)
box('SeatCushion', (SX, SY, SZ - 0.06), (0.48, 0.48, 0.12), M_VINYL, bevel=0.035, segs=4)
seatb = box('SeatBack', (SX - 0.25, SY, SZ + 0.27), (0.10, 0.46, 0.56), M_VINYL, bevel=0.035, segs=4)
turn(seatb, (SX - 0.25, SY, SZ), 'y', -12)
for s in (1, -1):
    box(f'SeatBolster_{"L" if s > 0 else "R"}', (SX, SY + s * 0.22, SZ + 0.01), (0.44, 0.05, 0.08), M_VINYL, bevel=0.02, segs=3)
box('Armrest_L', (SX - 0.02, SY + 0.27, SZ + 0.20), (0.36, 0.07, 0.06), M_VINYL, bevel=0.02, segs=3)
box('Armrest_R', (SX - 0.02, SY - 0.27, SZ + 0.20), (0.36, 0.07, 0.06), M_VINYL, bevel=0.02, segs=3)
for s in (1, -1):
    box(f'ArmrestPost_{"L" if s > 0 else "R"}', (SX - 0.16, SY + s * 0.27, SZ + 0.08), (0.04, 0.04, 0.20), M_DARK, bevel=0.004)
# boom joystick and belt switches on the right armrest (the boom is on that side)
box('BoomConsole', (SX + 0.10, SY - 0.27, SZ + 0.25), (0.16, 0.10, 0.05), M_PLAST, bevel=0.01)
cyl('BoomJoystickBoot', (SX + 0.08, SY - 0.27, SZ + 0.285), 0.025, 0.02, 'z', M_PLAST, 20)
cyl('BoomJoystick', (SX + 0.08, SY - 0.27, SZ + 0.33), 0.010, 0.08, 'z', M_DARK, 16)
cyl('BoomJoystickKnob', (SX + 0.08, SY - 0.27, SZ + 0.38), 0.022, 0.04, 'z', M_PLAST, 20, bevel=0.008)
for j in range(3):
    cyl(f'BeltSwitch_{j}', (SX + 0.14, SY - 0.30 + j * 0.03, SZ + 0.28), 0.008, 0.012, 'z', (M_GREEN, M_WHITE, M_REDP)[j], 12)
# dash pod, steering column and wheel, display, pedals
DX = 0.05
dash = prism('DashPod', [(DX - 0.10, DECK), (DX + 0.18, DECK), (DX + 0.18, DECK + 0.62), (DX + 0.02, DECK + 0.74), (DX - 0.10, DECK + 0.62)], SY - 0.26, SY + 0.26, M_PAINT, bevel=0.015, segs=2)
box('DashDisplay', (DX - 0.03, SY, DECK + 0.67), (0.02, 0.18, 0.10), M_SCREEN) if False else None
disp = box('DashDisplayBezel', (DX - 0.035, SY + 0.12, DECK + 0.66), (0.02, 0.16, 0.10), M_PLAST, bevel=0.006)
box('DashDisplay', (DX - 0.046, SY + 0.12, DECK + 0.66), (0.004, 0.13, 0.075), M_SCREEN)
col_a = math.radians(35)
c0 = Vector((DX - 0.10, SY, DECK + 0.58)); c1 = c0 + Vector((-0.30 * math.cos(col_a), 0, 0.30 * math.sin(col_a)))
rod('SteeringColumn', c0 + Vector((0.03, 0, -0.02)), c1, SY, 0.028, M_DARK)
wc = c1 + Vector((-0.02 * math.cos(col_a), 0, 0.02 * math.sin(col_a)))
WDEG = -(180 - math.degrees(col_a))        # turns a part made round x onto the column's axis
rim = turn(torus('SteeringWheel', (wc.x, SY, wc.z), 0.19, 0.016, 'x', M_PLAST, 48, 12), (wc.x, SY, wc.z), 'y', WDEG)
turn(cyl('SteeringHub', (wc.x, SY, wc.z), 0.045, 0.05, 'x', M_PLAST, 24, bevel=0.01), (wc.x, SY, wc.z), 'y', WDEG)
for j in range(3):
    a = math.radians(90 + j * 120)
    c = (wc.x, SY + 0.11 * math.cos(a), wc.z + 0.11 * math.sin(a))
    sp = turn(box(f'SteeringSpoke_{j}', c, (0.016, 0.15, 0.022), M_PLAST, bevel=0.004), c, 'x', math.degrees(a))
    turn(sp, (wc.x, SY, wc.z), 'y', WDEG)
for j, (yy, nm) in enumerate(((SY - 0.10, 'Accelerator'), (SY + 0.08, 'Brake'))):
    ped = box(f'Pedal_{nm}', (DX - 0.16, yy, DECK + 0.10), (0.03, 0.08 if j else 0.07, 0.16 if j == 0 else 0.10), M_PLAST, bevel=0.005)
    turn(ped, (DX - 0.16, yy, DECK + 0.02), 'y', 20)
    box(f'PedalArm_{nm}', (DX - 0.11, yy, DECK + 0.08), (0.10, 0.02, 0.02), M_DARK)
box('ParkBrake', (SX + 0.15, SY + 0.30, DECK + 0.30), (0.04, 0.04, 0.32), M_DARK, bevel=0.006)
box('ParkBrakeGrip', (SX + 0.15, SY + 0.30, DECK + 0.49), (0.05, 0.05, 0.08), M_PLAST, bevel=0.012)
# protection grid between the operator and the boom (mesh panel in a frame) and the roof guard on 4 posts
GX0, GX1, ROOF = -1.05, 0.30, 2.05
for x in (GX0, GX1):
    for y, nm in ((CH_Y - 0.04, 'L'), (BY1 + 0.11, 'R')):
        box(f'GuardPost_{"F" if x > 0 else "B"}{nm}', (x, y, (DECK + ROOF) / 2), (0.05, 0.05, ROOF - DECK), M_PAINT, bevel=0.006)
box('GuardRoofFrame_F', (GX1, (CH_Y - 0.04 + BY1 + 0.11) / 2, ROOF + 0.02), (0.06, CH_Y - BY1 - 0.15 + 0.05, 0.05), M_PAINT, bevel=0.006)
box('GuardRoofFrame_B', (GX0, (CH_Y - 0.04 + BY1 + 0.11) / 2, ROOF + 0.02), (0.06, CH_Y - BY1 - 0.15 + 0.05, 0.05), M_PAINT, bevel=0.006)
for y, nm in ((CH_Y - 0.04, 'L'), (BY1 + 0.11, 'R')):
    box(f'GuardRoofSide_{nm}', ((GX0 + GX1) / 2, y, ROOF + 0.02), (GX1 - GX0 + 0.05, 0.06, 0.05), M_PAINT, bevel=0.006)
for i in range(1, 9):
    x = GX0 + (GX1 - GX0) * i / 9
    box(f'GuardRoofBar_{i}', (x, (CH_Y - 0.04 + BY1 + 0.11) / 2, ROOF + 0.02), (0.02, CH_Y - BY1 - 0.15, 0.02), M_DARK)
box('GuardMeshFrame', ((GX0 + GX1) / 2, BY1 + 0.11, (DECK + 0.10 + ROOF - 0.10) / 2 + 0.25), (GX1 - GX0 - 0.05, 0.03, ROOF - DECK - 0.70), M_PAINT, bevel=0.004) if False else None
for i in range(10):
    z = DECK + 0.30 + i * 0.09
    box(f'GuardMeshBar_{i}', ((GX0 + GX1) / 2, BY1 + 0.11, z), (GX1 - GX0 - 0.05, 0.012, 0.012), M_DARK)
for i in range(1, 7):
    x = GX0 + (GX1 - GX0) * i / 7
    box(f'GuardMeshPost_{i}', (x, BY1 + 0.11, DECK + 0.30 + 0.405), (0.012, 0.012, 0.83), M_DARK)
# the access step and grab handle on the left, a convex mirror on the front-left guard post
box('StepBracket', (SX, CH_Y + 0.05, 0.51), (0.42, 0.14, 0.14), M_DARK, bevel=0.004)
box('StepTread', (SX, CH_Y + 0.14, 0.44), (0.46, 0.18, 0.03), M_TREAD, bevel=0.003)
box('StepNosing', (SX, CH_Y + 0.225, 0.457), (0.46, 0.012, 0.012), M_YELP)
tube('GrabHandle', [(GX0, CH_Y - 0.015, DECK + 0.30), (GX0 + 0.02, CH_Y + 0.05, DECK + 0.40), (GX0 + 0.02, CH_Y + 0.05, DECK + 0.90), (GX0, CH_Y - 0.015, DECK + 1.00)], 0.014, M_YELP, bez=False)
tube('MirrorArm', [(GX1, CH_Y - 0.015, 1.70), (GX1 + 0.15, CH_Y + 0.10, 1.70), (GX1 + 0.20, CH_Y + 0.12, 1.75)], 0.012, M_DARK, bez=False)
cyl('MirrorBody', (GX1 + 0.21, CH_Y + 0.12, 1.80), 0.10, 0.05, 'x', M_PLAST, 32, bevel=0.01)
cyl('MirrorGlass', (GX1 + 0.18, CH_Y + 0.12, 1.80), 0.085, 0.01, 'x', M_MIRROR, 32)
# #endregion


# #region ── Lamps, beacon, reflectors, emergency stops, tow hitches ──
for s, nm in ((1, 'L'), (-1, 'R')):
    lamp(f'Headlamp_{nm}', (CH_X1, s * 0.72, 0.66), 'x+', 0.16, 0.10, M_WHITE, M_DARK, M_PLAST)
    lamp(f'IndicatorF_{nm}', (CH_X1, s * 0.48, 0.66), 'x+', 0.10, 0.07, M_AMBER, M_DARK, M_PLAST)
    lamp(f'TailBrake_{nm}', (CH_X0, s * 0.74, 0.66), 'x-', 0.14, 0.09, M_RED, M_DARK, M_PLAST)
    lamp(f'IndicatorR_{nm}', (CH_X0, s * 0.52, 0.66), 'x-', 0.10, 0.07, M_AMBER, M_DARK, M_PLAST)
    lamp(f'SideMarker_F{nm}', (FX + 0.55, s * CH_Y, 0.66), 'y' + ('+' if s > 0 else '-'), 0.07, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.03)
    lamp(f'SideMarker_R{nm}', (RX - 0.55, s * CH_Y, 0.66), 'y' + ('+' if s > 0 else '-'), 0.07, 0.04, M_AMBER, M_DARK, M_PLAST, depth=0.03)
    # emergency stops on the chassis front corners (the operator walking round can reach one from any side)
    box(f'EStopBox_F{nm}', (CH_X1 - 0.08, s * (CH_Y + 0.025), 0.70), (0.10, 0.05, 0.10), M_YELP, bevel=0.008)
    cyl(f'EStop_F{nm}', (CH_X1 - 0.08, s * (CH_Y + 0.06), 0.70), 0.028, 0.025, 'y', M_REDP, 24, bevel=0.006)
lamp('Reverse_C', (CH_X0, -0.30, 0.66), 'x-', 0.10, 0.07, M_WHITE, M_DARK, M_PLAST)
# rotating beacon on a post at the rear-left (seen over the boom from behind)
box('BeaconPost', (GX0, CH_Y - 0.04, ROOF + 0.10), (0.05, 0.05, 0.12), M_DARK, bevel=0.005)
cyl('BeaconBase', (GX0, CH_Y - 0.04, ROOF + 0.175), 0.07, 0.03, 'z', M_DARK, 28, bevel=0.006)
cyl('Beacon', (GX0, CH_Y - 0.04, ROOF + 0.25), 0.06, 0.12, 'z', M_AMBER, 28, bevel=0.02)
# rear tow hitch (pin jaw) and the front tow eye
box('HitchRear_Block', (CH_X0 - 0.02, 0, 0.42), (0.06, 0.24, 0.20), M_DARK, bevel=0.006)
for k, z in enumerate((0.36, 0.48)):
    box(f'HitchRear_Jaw_{k}', (CH_X0 - 0.15, 0, z), (0.22, 0.18, 0.025), M_DARK, bevel=0.008, segs=2)
cyl('HitchRearPin', (CH_X0 - 0.20, 0, 0.42), 0.017, 0.20, 'z', M_STEEL, 20)
torus('HitchRearPin_Ring', (CH_X0 - 0.20, 0, 0.535), 0.032, 0.006, 'y', M_STEEL, 32, 10)
box('TowEyeFront_Block', (CH_X1 + 0.02, 0.30, 0.42), (0.06, 0.16, 0.14), M_DARK, bevel=0.006)
torus('TowEyeFront', (CH_X1 + 0.10, 0.30, 0.42), 0.05, 0.016, 'z', M_STEEL, 32, 12)
for x, nm in ((CH_X1, 'F'), (CH_X0, 'R')):
    for s in (1, -1):
        cyl(f'Reflector_{nm}{"L" if s > 0 else "R"}', (x + (0.003 if x > 0 else -0.003), s * 0.86, 0.50), 0.035, 0.008, 'x', M_RED if x < 0 else M_WHITE, 24)
# #endregion


# #region ── IK points for the operator ──
ik('Seat', (SX, SY, SZ), 'SeatCushion'); ik('SeatBack', (SX - 0.25, SY, SZ + 0.35), 'SeatBack')
for s, nm in ((1, 'L'), (-1, 'R')):
    ik(f'Hand{nm}_Wheel', (wc.x, SY + s * 0.17, wc.z), 'SteeringWheel')
ik('Accelerator', (DX - 0.17, SY - 0.10, DECK + 0.12), 'Pedal_Accelerator'); ik('Brake', (DX - 0.17, SY + 0.08, DECK + 0.10), 'Pedal_Brake')
ik('BoomJoystick', (SX + 0.08, SY - 0.27, SZ + 0.38), 'BoomJoystickKnob'); ik('ParkBrake', (SX + 0.15, SY + 0.30, DECK + 0.49), 'ParkBrakeGrip')
ik('Step', (SX, CH_Y + 0.14, 0.455), 'StepTread'); ik('GrabHandle', (GX0 + 0.02, CH_Y + 0.05, DECK + 0.65), 'GrabHandle')
# #endregion


# #region ── Checks, the rig file, save and export ──
fl, cl, ov, lo = K.checks('BELTLOADER', ground_ok=('Tyre',), overhang_skip_prefix=('Boom_Cleat', 'GuardMesh', 'GuardRoofBar', 'SeatBellows', 'Kingpin', 'SteerArm', 'RearAxle_Flange', 'RearLiftCyl',
    'FrontLiftCyl', 'CylFootBracket', 'Steering', 'PedalArm', 'MirrorArm', 'BeaconPost', 'HitchRear', 'TowEyeFront', 'Boom_StrobeBase'))   # (joints by design)
rig = dict(
    note='Belt loader lift rig (metres; x forward, z up, origin on the ground mid-wheelbase). Set front (belt top at the '
         'door sill) and rear (belt top at the loading end) heights; solve: th = asin((front-rear)/boom_length); rear pin '
         'z = rear + pin_from_rear*sin(th) - belt_to_chord*cos(th); pin x = rear_arm_pivot.x + sqrt(rear_arm^2 - (pin z - '
         'rear_arm_pivot.z)^2); the front roller is where the bottom chord through the pin (direction th) meets the front '
         "arm's circle (far root). Rotate the arms to the pin / roller, the boom about the pin, slide the cylinder rods.",
    boom_length=BL, belt_to_chord=BD, pin_from_rear=S_R, rear_arm=A_R, front_arm=A_F,
    rear_arm_pivot=list(P_R), front_arm_pivot=list(P_F), rear_cyl_foot=list(C_R), front_cyl_foot=list(C_F),
    rear_cyl_on_arm=0.6, front_cyl_on_arm=0.55, front_range=[0.84, 4.19], rear_range=[0.85, 1.30],
    pose_saved=dict(front=FRONT_H, rear=REAR_H, boom_deg=round(math.degrees(POSE['th']), 2)),
    presets={'A320 forward hold': [2.10, 0.95], 'A320 aft hold': [2.20, 0.95], 'A320 bulk hold': [2.20, 0.95],
             'lowest (regional jets)': [0.84, 0.85], 'highest (wide-body bulk doors)': [4.19, 1.20]},
    parts_on_boom=[o.name for o in BOOM_PARTS])
for k, (f, r) in rig['presets'].items(): solve(f, r)          # (every preset must solve)
json.dump(rig, open(os.path.join(ART, 'beltloader-rig.json'), 'w'), indent=1)
GLB = os.path.join(ART, 'beltloader.glb'); BLEND = os.path.join(ART, 'BeltLoader.blend'); zf = os.path.join(ART, 'zfight-check.json')
_run = K.subprocess.run
def _local_run(args, **kw):                 # (the kit calls the PC's Python by its Windows path; here the same script runs locally)
    if isinstance(args, list) and str(args[0]).startswith('C:/'): args = [sys.executable if 'python' in sys.executable else 'python3'] + args[1:]
    return _run(args, **kw)
K.subprocess.run = _local_run
last = K.save_export(ROOT, BLEND, GLB, zf, 'BELTLOADER', 'BeltLoaderAlt')
print('BELTLOADER FLICKER final:', last)
json.dump(dict(parts=len(ALL), ik=[o.name for o in IKS], ik_parts=IKP, floating=fl, overhang=ov, loose=lo),
          open(os.path.join(ART, 'beltloader-parts.json'), 'w'), indent=1)
print('BELTLOADER saved', len(ALL), 'parts', len(IKS), 'IK/pivots; pose', rig['pose_saved'])
# #endregion


# #region ── The wireframe views (every line an edge of the real mesh; Cycles + Freestyle so it runs without a GPU) ──
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
    def shoot(name, loc, at, lens=50, ortho=0, res=(2400, 1500), hide=()):
        if only and name not in only: return
        for o in ALL: o.hide_render = o.name.startswith(hide) if hide else False
        cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        cam_d.type = 'ORTHO' if ortho else 'PERSP'; cam_d.lens = lens; cam_d.ortho_scale = ortho or 1
        sc.render.resolution_x, sc.render.resolution_y = res
        sc.render.filepath = os.path.join(WIRE, name + '.png'); bpy.ops.render.render(write_still=True)
        VIEWS[name] = dict(loc=list(loc), at=list(at), ortho=ortho, res=list(res)); print('view', name, flush=True)
        for o in ALL: o.hide_render = False
    MID = (1.2, -0.1, 1.2)
    shoot('1-three-quarter-front', (9.5, 7.0, 4.6), MID, 40)
    shoot('2-three-quarter-rear', (-8.0, -6.5, 4.4), (0.8, -0.2, 1.1), 40)
    shoot('3-side-left', (1.3, 30, 1.6), (1.3, 0, 1.6), 50, 9.6, (2400, 1350))
    shoot('4-side-right', (1.3, -30, 1.6), (1.3, 0, 1.6), 50, 9.6, (2400, 1350))
    shoot('5-front', (30, 0, 1.75), (0, 0, 1.75), 50, 4.0, (1400, 1400))
    shoot('6-rear', (-30, 0, 1.75), (0, 0, 1.75), 50, 4.0, (1400, 1400))
    t = bpy.data.objects.new('TopTarget', None)
    cam_d.type = 'ORTHO'
    if not only or '7-top' in only:
        cam.location = (1.3, 0, 30); cam.rotation_euler = (0, 0, 0)
        cam_d.ortho_scale = 9.6; sc.render.resolution_x, sc.render.resolution_y = 2400, 900
        sc.render.filepath = os.path.join(WIRE, '7-top.png'); bpy.ops.render.render(write_still=True); print('view 7-top', flush=True)
        VIEWS['7-top'] = dict(loc=[1.3, 0, 30], ortho=9.6, res=[2400, 900], rot_z=0)
    shoot('8-operator-station', (0.9, 2.6, 2.6), (-0.45, 0.45, 1.15), 32, hide=('GuardRoof',))
    RW = ('SkirtRight', 'CradleWall_R', 'Fender_FR', 'Fender_RR', 'FenderTread_FR', 'FenderTread_RR', 'Tyre_FR', 'Tyre_RR', 'Rim_FR', 'Rim_RR',
          'WheelDisc_FR', 'WheelDisc_RR', 'Hub_FR', 'Hub_RR', 'HubCap_FR', 'HubCap_RR', 'Nut_FR', 'Nut_RR', 'WellWall_FR', 'WellWall_RR')
    shoot('9-lift-arms-and-cylinders', (-0.6, -3.4, 0.65), (-0.6, -0.45, 0.95), 26, hide=RW)
    tip = POSE['F']
    shoot('10-boom-front-and-controls', (tip.x + 2.4, 1.4, tip.z + 1.5), (tip.x - 0.7, -0.48, tip.z + 0.35), 55)
    shoot('11-rear-loading-end-and-hitch', (-5.6, -2.6, 2.5), (-2.4, -0.35, 0.8), 55)
    shoot('12-underside', (0.4, -5.2, -1.3), (0.2, 0, 0.45), 30)
    json.dump(VIEWS, open(os.path.join(WIRE, 'views.json'), 'w'), indent=1)
# #endregion
