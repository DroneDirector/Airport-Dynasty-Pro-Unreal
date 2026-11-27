# The A320's opening parts (to-do 311, owner 2026-11-27): every panel, door and control surface that opens or moves is its
# own named part with its hinge, so the game can open it later. OWNER RULE (Meshy): the model's shape never changes - the
# parts are only separated out of his mesh (A320-whole.glb), and where a part is not a piece of its own it is cut along its
# panel line with a flat plane through the existing faces: every vertex stays where it was, closed = exactly his shape.
# Runs the gear script first (make_a320_parts.py: NoseWheels, NoseLeg, MainWheels_L/R, MainLeg_L/R), then:
#   gear: NoseStrut, MainStrut_L/R, SideBrace_L/R (whole legs fold: mains inward into the belly, nose forward) and the
#         gear doors, with dark wells behind them so nothing is see-through
#   engines (CFM56-5B shape: short fan duct, core nozzle and plug outside - pivoting-door reverser): fan cowls, 4 reverser
#         doors per engine, and inside the cowls our own fan case, gearbox, oil tank, pipes, harnesses and mounts
#   cargo doors (forward, aft, bulk - right side) with a lined hold behind each
#   flight controls: ailerons, elevators, rudder, spoilers 1-5, flaps (inboard, outboard), slats 1-5 per wing
# Hinges, axes and real open angles / travel go to Data/a320-panels.json; each part says where its cut line came from
# (texture: a panel line drawn on his texture; web: published A320 layout and measures).
# Frame: Blender metres, nose tyre at the origin, nose +y, up +z; Blender x is mirrored by the loader: Blender +x is the
# real aircraft's LEFT (port, engine 1), Blender -x its RIGHT (starboard, cargo doors). _L / _R follow the existing gear
# parts (MainWheels_L is Blender -x). Angles: right-hand rule about the axis; every axis is signed so the listed positive
# angle is the opening / deploy direction named in the part's note.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/A320/make_a320_panels.py
#   (ADP_RENDER=1: also the two 3840x2160 pictures, closed and open, to docs/Pictures/a320-panels/)
import bpy, bmesh, os, json, math
import numpy as np
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
exec(compile(open(os.path.join(HERE, 'make_a320_parts.py')).read(), 'make_a320_parts.py', 'exec'))
o = bpy.data.objects['A320']
PANELS = []          # json entries
def V(*a): return Vector(a)
def unit(v): v = Vector(v); return v / v.length

# ------------------------------------------------------------------ gear: the rest of each leg, out as whole pieces
def zone_main(s):
    def t(lo, hi):
        c = (lo + hi) / 2
        if s > 0 and not (lo[0] > 1.9 and hi[0] < 5.4): return False
        if s < 0 and not (hi[0] < -1.9 and lo[0] > -5.4): return False
        if not (lo[1] > -14.3 and hi[1] < -11.3 and c[1] < -12.0 and lo[2] > 0.3 and hi[2] < 3.45 and hi[2] - lo[2] > 0.3): return False
        if math.hypot(c[0] - s * 5.45, c[2] - 1.97) < 1.45 and lo[1] > -12.0: return False     # (the engine's exhaust)
        return True
    return t
def brace(s, yes):
    z = zone_main(s)
    return lambda lo, hi: z(lo, hi) and ((hi[0] - lo[0] > 0.7) == yes)
redo(brace(-1, True), 'SideBrace_L'); redo(brace(1, True), 'SideBrace_R')
redo(brace(-1, False), 'MainStrut_L'); redo(brace(1, False), 'MainStrut_R')
redo(lambda lo, hi: abs(lo[0]) < 0.75 and abs(hi[0]) < 0.75 and lo[1] > -1.4 and hi[1] < 1.3 and lo[2] > 0.3 and hi[2] < 2.55 and hi[2] - lo[2] > 0.25, 'NoseStrut')
def verts(name):
    ob = bpy.data.objects.get(name)
    if not ob: return None
    return np.array([v.co[:] for v in ob.data.vertices])

# ------------------------------------------------------------------ cutting: planes through the faces, then by side
bm = bmesh.new(); bm.from_mesh(o.data)
F0 = list(bm.faces)
cen0 = np.array([f.calc_center_median()[:] for f in F0])
POOLS = {}
def pool(key, lo, hi):
    if key not in POOLS:
        m = np.all((cen0 >= np.array(lo)) & (cen0 <= np.array(hi)), axis=1)
        POOLS[key] = [F0[i] for i in np.nonzero(m)[0]]
    return key
def bisect(key, lo, hi, p, n):
    fs = [f for f in POOLS[key] if f.is_valid]
    fs = [f for f in fs if all(lo[i] - 0.3 <= f.calc_center_median()[i] <= hi[i] + 0.3 for i in range(3))]
    if not fs: return
    geom = list(set(fs) | {e for f in fs for e in f.edges} | {v for f in fs for v in f.verts})
    r = bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-5, plane_co=Vector(p), plane_no=unit(n), clear_inner=False, clear_outer=False)
    POOLS[key] = [f for f in POOLS[key] if f.is_valid] + [g for g in r['geom'] if isinstance(g, bmesh.types.BMFace)]
    POOLS[key] = list(set(POOLS[key]))
SPECS = []
def part(name, key, lo, hi, planes, keep, meta):
    for p, n in planes: bisect(key, lo, hi, p, n)
    SPECS.append((name, lo, hi, keep, meta))

def side_of(p, n): # mask: centroid on the +n side of the plane
    return lambda C: (C - np.array(p)) @ np.array(unit(n)) > 0

# ---- wings (s = +1 Blender +x, s = -1 Blender -x)
def yTE(a): return -13.85 if a < 6.25 else -13.85 - 0.158 * (a - 6.25)
def yLE(a): return -10.84 - 0.328 * (a - 9.0)
def zU(a): return 3.33 + 0.09 * a
def wing_ctrl(s, sd, name, a0, a1, chord, kind, chord_front=None, src='web', note='', motion=None):
    key = pool('wing%d' % s, (min(s * 1.9, s * 16.8), -17, 2.4), (max(s * 1.9, s * 16.8), -5, 5.4))
    lo = (min(s * a0, s * a1), -17, 2.4); hi = (max(s * a0, s * a1), -5, 5.4)
    inb = a0 < 6.25
    nTE = (0, 1, 0) if inb else (s * 0.158, 1, 0)
    def off(c, a): return (s * a, yTE(a) + c / (1.0 if inb else math.cos(math.atan(0.158))), 0)
    am = (a0 + a1) / 2
    planes = [((s * a0, 0, 0), (1, 0, 0)), ((s * a1, 0, 0), (1, 0, 0)), (off(chord, am), nTE)]
    if chord_front: planes.append((off(chord_front, am), nTE))
    def keep(C, N, s=s, a0=a0, a1=a1, chord=chord, chord_front=chord_front):
        ax = s * C[:, 0]
        te = np.where(ax < 6.25, -13.85, -13.85 - 0.158 * (ax - 6.25))
        band = (C[:, 2] > 3.33 + 0.09 * ax - 0.4) & (C[:, 2] < 3.33 + 0.09 * ax + 0.35) & (np.abs(N[:, 0]) < 0.6)
        m = (ax > a0) & (ax < a1) & band & (C[:, 1] > te - 0.06)
        # the hinge line, as a distance normal to the trailing edge
        k = 1.0 if a0 < 6.25 else math.cos(math.atan(0.158))
        d = (C[:, 1] - te) * k
        if chord_front: return m & (d > chord) & (d < chord_front) & (N[:, 2] > 0.3)
        return m & (d < chord)
    hc = chord_front if chord_front else chord
    pa = Vector(off(hc, a0)); pb = Vector(off(hc, a1))
    pa.z = zU(a0) - 0.12; pb.z = zU(a1) - 0.12
    if kind == 'spoiler': pa.z = zU(a0) - 0.02; pb.z = zU(a1) - 0.02
    part(name + sd, 'wing%d' % s, lo, hi, planes, keep, dict(kind=kind, hinge=(pa, pb), src=src, note=note, motion=motion))

def slat(s, sd, name, a0, a1, chord=0.42):
    key = pool('wing%d' % s, (min(s * 1.9, s * 16.8), -17, 2.4), (max(s * 1.9, s * 16.8), -5, 5.4))
    lo = (min(s * a0, s * a1), -15, 2.4); hi = (max(s * a0, s * a1), -5, 5.4)
    k = math.cos(math.atan(0.328)); n = (s * 0.328, 1, 0)
    am = (a0 + a1) / 2
    planes = [((s * a0, 0, 0), (1, 0, 0)), ((s * a1, 0, 0), (1, 0, 0)), ((s * am, yLE(am) - chord / k, 0), n)]
    def keep(C, N, s=s, a0=a0, a1=a1, chord=chord):
        ax = s * C[:, 0]
        band = (C[:, 2] > 3.33 + 0.09 * ax - 0.75) & (C[:, 2] < 3.33 + 0.09 * ax + 0.35) & (np.abs(N[:, 0]) < 0.6)
        return (ax > a0) & (ax < a1) & band & ((C[:, 1] - (-10.84 - 0.328 * (ax - 9.0))) * k > -chord)
    pa = V(s * a0, yLE(a0) - chord / k, zU(a0) - 0.25); pb = V(s * a1, yLE(a1) - chord / k, zU(a1) - 0.25)
    part(name + sd, 'wing%d' % s, lo, hi, planes, keep, dict(kind='slat', hinge=(pa, pb), src='web', note='slat: forward and down along its tracks', motion=dict(deploy_deg=27.0, travel_m=0.55, travel_dir='forward-down along the chord')))

for s, sd in ((1, '_R'), (-1, '_L')):
    side = 'Blender %sx' % ('+' if s > 0 else '-')
    wing_ctrl(s, sd, 'FlapInner', 2.35, 6.2, 1.25, 'flap', src='texture+web', note='inner end at the wing root fairing, outer end on the panel line drawn at the trailing-edge kink (texture); chord web',
              motion=dict(deploy_deg=40.0, travel_m=1.1, travel_dir='aft along the chord (Fowler), then the angle', settings={'1+F': 10, '2': 15, '3': 20, 'FULL': 40}))
    wing_ctrl(s, sd, 'FlapOuter', 6.3, 11.85, 1.0, 'flap', src='texture+web', note='ends on the panel lines drawn on the lower skin at the kink and at the aileron (texture); chord web',
              motion=dict(deploy_deg=35.0, travel_m=1.0, travel_dir='aft along the chord (Fowler), then the angle', settings={'1+F': 10, '2': 15, '3': 20, 'FULL': 35}))
    wing_ctrl(s, sd, 'Aileron', 11.95, 14.95, 0.72, 'aileron', src='texture', note='both ends on the panel lines drawn on the lower skin; chord web',
              motion=dict(range_deg=[-25.0, 25.0], positive='trailing edge down', droop_deg=5.0))
    wing_ctrl(s, sd, 'Spoiler1', 3.0, 5.9, 1.25, 'spoiler', chord_front=1.85, src='web', note='upper skin ahead of the flap; positive = up',
              motion=dict(open_deg=50.0, roll_deg=None, ground_deg=50.0))
    for i in range(4):
        a0 = 6.45 + i * 1.325
        wing_ctrl(s, sd, 'Spoiler%d' % (i + 2), a0, a0 + 1.325, 1.0, 'spoiler', chord_front=1.6, src='web', note='upper skin ahead of the outer flap; positive = up',
                  motion=dict(open_deg=50.0, roll_deg=35.0, speedbrake_deg=40.0 if i < 3 else None))
    slat(s, sd, 'Slat1', 3.2, 4.8)
    for i, (a0, a1) in enumerate(((6.4, 8.8), (8.8, 11.2), (11.2, 13.6), (13.6, 15.9))):
        slat(s, sd, 'Slat%d' % (i + 2), a0, a1)

# ---- tail: elevators and rudder (hinge lines from published A320 chord ratios: no lines on the texture there)
def yTEh(a): return -30.92 - 0.293 * (a - 2.5)
for s, sd in ((1, '_R'), (-1, '_L')):
    A = Vector((s * 1.0, yTEh(1.0) + 0.95, 0)); B = Vector((s * 6.0, yTEh(6.0) + 0.55, 0))
    d = B - A; n = Vector((-d.y, d.x, 0)).normalized()
    if n.y < 0: n = -n
    key = pool('ht%d' % s, (min(0, s * 7), -33, 3.9), (max(0, s * 7), -25, 6.2))
    lo = (min(s * 0.85, s * 6.3), -33, 3.9); hi = (max(s * 0.85, s * 6.3), -25, 6.2)
    def keepE(C, N, s=s, A=A, n=n):
        ax = s * C[:, 0]; zu = 4.62 + 0.143 * ax
        return (ax > 0.85) & (ax < 6.3) & (C[:, 2] > zu - 0.45) & (C[:, 2] < zu + 0.08) & (np.abs(N[:, 0]) < 0.6) & ((C - np.array(A)) @ np.array(n) < 0)
    pa = A.copy(); pb = B.copy(); pa.z = 4.62 + 0.143 * 1.0 - 0.12; pb.z = 4.62 + 0.143 * 6.0 - 0.1
    part('Elevator' + sd, key, lo, hi, [((s * 0.85, 0, 0), (1, 0, 0)), ((s * 6.3, 0, 0), (1, 0, 0)), (A, n)], keepE,
         dict(kind='elevator', hinge=(pa, pb), src='web', note='positive = trailing edge down', motion=dict(range_deg=[-30.0, 17.0], positive='trailing edge down')))
def yTEv(z): return -30.94 - 0.34 * (z - 7.0)
A = Vector((0, yTEv(6.6) + 1.3, 6.6)); B = Vector((0, yTEv(11.7) + 0.72, 11.7))
d = B - A; n = Vector((0, -d.z, d.y)).normalized()
if n.y < 0: n = -n
key = pool('vt', (-0.6, -33.5, 6.0), (0.6, -25, 12.0))
part('Rudder', key, (-0.6, -33.5, 6.6), (0.6, -25, 11.7), [((0, 0, 6.6), (0, 0, 1)), ((0, 0, 11.7), (0, 0, 1)), (A, n)],
     lambda C, N: (np.abs(C[:, 0]) < 0.36) & (C[:, 2] > 6.6) & (C[:, 2] < 11.7) & ((C - np.array(A)) @ np.array(n) < 0),
     dict(kind='rudder', hinge=(A, B), src='web', note='positive = trailing edge to Blender +x', motion=dict(range_deg=[-30.0, 30.0])))

# ---- engines: fan cowls (texture lines at y -7.22 and -8.52) and the reverser's 4 pivoting doors
ECX, ECZ, ER = 5.50, 1.945, 1.33     # (centre and radius fitted to the fan cowl skin)
def ring_keep(s, y0, y1, p0, p1):
    def keep(C, N, s=s):              # the cowl is a thick shell: both its skins, outside the duct wall (r ~1.0)
        dx = C[:, 0] - s * ECX; dz = C[:, 2] - ECZ
        phi = np.degrees(np.arctan2(dx * s, dz)); r = np.hypot(dx, dz)          # phi: from the top, + outboard
        return (C[:, 1] > y0) & (C[:, 1] < y1) & (r > 1.12) & (r < 1.7) & (phi > p0) & (phi < p1)
    return keep
def rplane(s, phi):   # plane through the engine's axis at angle phi (outboard positive)
    f = math.radians(phi); u = Vector((s * math.sin(f), 0, math.cos(f)))
    return ((s * ECX, -8, ECZ), Vector((u.z, 0, -u.x)))
for s, en in ((1, 'Eng1'), (-1, 'Eng2')):
    key = pool(en, (s * ECX - 1.7, -10.2, 0.2), (s * ECX + 1.7, -6.0, 3.6))
    lo = (s * ECX - 1.7, -10.2, 0.2); hi = (s * ECX + 1.7, -6.0, 3.6)
    for half, (p0, p1, ph) in (('Out', (25, 181, 25)), ('In', (-181, -25, -25))):
        f = math.radians(ph); hp = Vector((s * ECX + s * ER * math.sin(f), -7.87, ECZ + ER * math.cos(f)))
        part('FanCowl_%s_%s' % (en, half), key, lo, hi, [((0, -7.22, 0), (0, 1, 0)), ((0, -8.52, 0), (0, 1, 0)), rplane(s, ph), rplane(s, 180)],
             ring_keep(s, -8.52, -7.22, p0, p1), dict(kind='fancowl', hinge=(hp + V(0, 0.65, 0), hp - V(0, 0.65, 0)), src='texture',
             note='front and rear edges on the panel lines drawn on the nacelle (texture); hinged beside the pylon, split at the bottom (web)', motion=dict(open_deg=55.0, hold_open_rods=2)))
    for i, pc in enumerate((-130, -50, 50, 130)):
        f = math.radians(pc); u = Vector((s * math.sin(f), 0, math.cos(f)))
        pv = Vector((s * ECX, -9.05, ECZ)) + u * ER; t = Vector((u.z, 0, -u.x))
        part('ReverserDoor_%s_%d' % (en, i + 1), key, lo, hi, [((0, -8.62, 0), (0, 1, 0)), ((0, -9.72, 0), (0, 1, 0)), rplane(s, pc - 28), rplane(s, pc + 28)],
             ring_keep(s, -9.72, -8.62, pc - 28, pc + 28), dict(kind='reverser', hinge=(pv - t * 0.5, pv + t * 0.5), src='web',
             note='CFM56-5B pivoting door: rear edge into the fan stream, front edge out', motion=dict(open_deg=32.0)))

# ---- cargo doors (right side = Blender -x; no lines on the texture there: A320 layout, Airbus AC 2-3)
CARGO = [('CargoDoorFwd', -3.72, -1.9, 2.55, 3.75, 120.0, 'out'), ('CargoDoorAft', -18.42, -16.6, 2.5, 3.7, 120.0, 'out'), ('CargoDoorBulk', -20.55, -19.6, 2.65, 3.55, 95.0, 'in')]
for nm, y0, y1, z0, z1, ang, way in CARGO:
    key = pool(nm, (-2.4, y0 - 0.4, z0 - 0.4), (-0.6, y1 + 0.4, z1 + 0.4))
    part(nm, key, (-2.4, y0, z0 - 0.2), (-0.6, y1, z1 + 0.2), [((0, y0, 0), (0, 1, 0)), ((0, y1, 0), (0, 1, 0)), ((0, 0, z0), (0, 0, 1)), ((0, 0, z1), (0, 0, 1))],
         lambda C, N, y0=y0, y1=y1, z0=z0, z1=z1: (C[:, 0] < -0.8) & (N[:, 0] < -0.25) & (C[:, 1] > y0) & (C[:, 1] < y1) & (C[:, 2] > z0) & (C[:, 2] < z1),
         dict(kind='cargo', hinge=None, src='web', note='hinged at the top; %s and up' % ('outward' if way == 'out' else 'inward (plug door)'), motion=dict(open_deg=ang), way=way, box=(y0, y1, z0, z1)))

# ---- gear doors: main wheel-well doors (belly, hinged near the keel) and the nose gear's forward doors (hinged outboard)
mw = verts('MainWheels_L'); WY = float((mw[:, 1].min() + mw[:, 1].max()) / 2) if mw is not None else -12.95
GDOORS = []
for s, sd in ((-1, '_L'), (1, '_R')):
    y0, y1 = WY - 1.05, WY + 1.05
    key = pool('mgd' + sd, (min(s * 0.05, s * 2.0), y0 - 0.3, 1.5), (max(s * 0.05, s * 2.0), y1 + 0.3, 2.9))
    part('MainGearDoor' + sd, key, (min(s * 0.1, s * 1.9), y0, 1.5), (max(s * 0.1, s * 1.9), y1, 2.9),
         [((s * 0.1, 0, 0), (1, 0, 0)), ((s * 1.9, 0, 0), (1, 0, 0)), ((0, y0, 0), (0, 1, 0)), ((0, y1, 0), (0, 1, 0))],
         lambda C, N, s=s, y0=y0, y1=y1: (s * C[:, 0] > 0.1) & (s * C[:, 0] < 1.9) & (C[:, 1] > y0) & (C[:, 1] < y1) & (C[:, 2] < 2.9) & (N[:, 2] < -0.3),
         dict(kind='geardoor', hinge='inner', side=s, src='web', note='main wheel-well door, hinged along its keel edge, opens down', motion=dict(open_deg=90.0), x=(0.1, 1.9), y=(y0, y1)))
    GDOORS.append('MainGearDoor' + sd)
for s, sd in ((-1, '_L'), (1, '_R')):
    key = pool('ngd' + sd, (min(0, s * 0.8), 0.6, 1.6), (max(0, s * 0.8), 3.1, 2.9))
    part('NoseGearDoor' + sd, key, (min(0, s * 0.55), 0.96, 1.6), (max(0, s * 0.55), 2.75, 2.9),
         [((0, 0, 0), (1, 0, 0)), ((s * 0.55, 0, 0), (1, 0, 0)), ((0, 0.96, 0), (0, 1, 0)), ((0, 2.75, 0), (0, 1, 0))],
         lambda C, N, s=s: (s * C[:, 0] > 0) & (s * C[:, 0] < 0.55) & (C[:, 1] > 0.96) & (C[:, 1] < 2.75) & (C[:, 2] < 2.9) & (N[:, 2] < -0.3),
         dict(kind='geardoor', hinge='outer', side=s, src='web', note='nose gear forward door, hinged along its outer edge, opens down (closed with the gear down)', motion=dict(open_deg=88.0), x=(0.0, 0.55), y=(0.96, 2.75)))
    GDOORS.append('NoseGearDoor' + sd)

# ---- which face goes where (one pass: centroids and normals), then each part out of the mesh
bm.faces.ensure_lookup_table()
C = np.array([f.calc_center_median()[:] for f in bm.faces]); N = np.array([f.normal[:] for f in bm.faces])
lay = bm.faces.layers.int.new('adp_part')
ids = np.zeros(len(C), int)
for k, (name, lo, hi, keep, meta) in enumerate(SPECS):
    m = np.all((C >= np.array(lo) - 0.05) & (C <= np.array(hi) + 0.05), axis=1) & keep(C, N) & (ids == 0)
    ids[m] = k + 1
for f, i in zip(bm.faces, ids): f[lay] = int(i)
bm.to_mesh(o.data); bm.free()
bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
bpy.ops.object.mode_set(mode='EDIT')
for k, (name, lo, hi, keep, meta) in enumerate(SPECS):
    eb = bmesh.from_edit_mesh(o.data); L = eb.faces.layers.int['adp_part']
    for v in eb.verts: v.select_set(False)
    for e in eb.edges: e.select_set(False)
    n = 0
    for f in eb.faces:
        sel = f[L] == k + 1; f.select_set(sel); n += sel
    eb.select_flush_mode(); bmesh.update_edit_mesh(o.data)
    meta['faces'] = n
    if n == 0: print('PART %s: no faces' % name); continue
    bpy.ops.mesh.separate(type='SELECTED')
    new = [x for x in bpy.context.selected_objects if x is not o and x.name not in [s[0] for s in SPECS]]
    new = [x for x in bpy.data.objects if x.type == 'MESH' and x.name.startswith('A320.')][0]
    new.name = name; new.data.name = name
bpy.ops.object.mode_set(mode='OBJECT')
for x in bpy.data.objects:
    if x.type == 'MESH' and 'adp_part' in x.data.attributes: x.data.attributes.remove(x.data.attributes['adp_part'])

# ------------------------------------------------------------------ our own parts: materials, wells, holds, liners, engine inside
def pmat(name, col, metal=0.0, rough=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    m.diffuse_color = (*col, 1); return m
M_WELL = pmat('A320_WellPaint', (0.42, 0.44, 0.40), 0.1, 0.65)      # wheel wells: grey-green primer, grimy
M_HOLD = pmat('A320_HoldLiner', (0.55, 0.56, 0.55), 0.0, 0.8)       # cargo hold: glass-fibre liner panels
M_INNER = pmat('A320_DoorInner', (0.62, 0.66, 0.60), 0.2, 0.55)     # inner skin of doors and cowls: green-grey primer
M_ACOU = pmat('A320_CowlAcoustic', (0.30, 0.31, 0.32), 0.3, 0.6)    # fan cowl inner: grey composite
M_CASE = pmat('A320_FanCase', (0.50, 0.52, 0.54), 0.85, 0.42)       # fan case: bare aluminium
M_AGB = pmat('A320_Gearbox', (0.18, 0.19, 0.20), 0.6, 0.5)          # gearbox: dark cast magnesium
M_TANK = pmat('A320_OilTank', (0.70, 0.71, 0.72), 0.9, 0.35)        # oil tank: polished aluminium
M_PIPE = pmat('A320_Pipe', (0.78, 0.76, 0.72), 1.0, 0.3)            # stainless pipes
M_HARN = pmat('A320_Harness', (0.48, 0.34, 0.18), 0.0, 0.75)        # braided harness, tan-brown
M_MOUNT = pmat('A320_Mount', (0.25, 0.26, 0.27), 0.9, 0.45)         # titanium mounts

def mkobj(name, bmx, mat):
    me = bpy.data.meshes.new(name); bmx.to_mesh(me); bmx.free()
    for p in me.polygons: p.use_smooth = True
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob); me.materials.append(mat); return ob
def open_box(name, x0, x1, y0, y1, z0, z1, openface, mat):
    """a lined box with one side open, faces looking inward (seen through the opening)"""
    b = bmesh.new()
    vs = {}
    for i, x in enumerate((x0, x1)):
        for j, y in enumerate((y0, y1)):
            for k, z in enumerate((z0, z1)): vs[i, j, k] = b.verts.new((x, y, z))
    sides = {'-x': [(0, 0, 0), (0, 1, 0), (0, 1, 1), (0, 0, 1)], '+x': [(1, 0, 0), (1, 0, 1), (1, 1, 1), (1, 1, 0)],
             '-y': [(0, 0, 0), (0, 0, 1), (1, 0, 1), (1, 0, 0)], '+y': [(0, 1, 0), (1, 1, 0), (1, 1, 1), (0, 1, 1)],
             '-z': [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)], '+z': [(0, 0, 1), (0, 1, 1), (1, 1, 1), (1, 0, 1)]}
    for k, q in sides.items():
        if k != openface: b.faces.new([vs[t] for t in q])     # (wound outward)
    bmesh.ops.reverse_faces(b, faces=b.faces)                     # inward
    return mkobj(name, b, mat)
OWN = []        # json: our own parts
def liner(src, name, off, mat):
    ob = bpy.data.objects[src]; b = bmesh.new(); b.from_mesh(ob.data)
    for v in b.verts: v.co -= v.normal * off
    bmesh.ops.reverse_faces(b, faces=b.faces)
    for f in b.faces: f.material_index = 0
    li = mkobj(name, b, mat); OWN.append(dict(name=name, moves_with=src, what='inner skin %.0f cm inside %s' % (off * 100, src))); return li

# gear wells: dark boxes above the doors, their open side 2 cm above the highest point of the door skin
for nm in GDOORS:
    meta = [s[4] for s in SPECS if s[0] == nm][0]; v = verts(nm)
    if v is None: continue
    s = meta['side']; zt = float(v[:, 2].max()) + 0.02
    xa, xb = sorted((s * meta['x'][0], s * meta['x'][1]))
    depth = 1.75 if nm.startswith('Main') else 0.95
    open_box(nm.replace('Door', 'Well'), xa + 0.01, xb - 0.01, meta['y'][0] + 0.01, meta['y'][1] - 0.01, zt, zt + depth, '-z', M_WELL)
    OWN.append(dict(name=nm.replace('Door', 'Well'), moves_with=None, what='wheel well lining behind %s (fixed)' % nm))
    liner(nm, nm + '_Inner', 0.03, M_INNER)
# cargo holds: lined boxes inside each door, the outer edges 2.5 cm inside the skin
for nm, y0, y1, z0, z1, ang, way in CARGO:
    v = verts(nm)
    if v is None: continue
    xs = float(v[:, 0].max()) + 0.025      # (the door's innermost point: the hold starts there)
    zx = 0.9 if way == 'in' else 0.0
    open_box(nm.replace('Door', 'Hold'), xs, xs + 1.4, y0 + 0.01, y1 - 0.01, z0 + 0.01, z1 - 0.01 + zx, '-x', M_HOLD)
    OWN.append(dict(name=nm.replace('Door', 'Hold'), moves_with=None, what='cargo hold lining behind %s (fixed)' % nm))
    liner(nm, nm + '_Inner', 0.04, M_INNER)
    meta = [s_[4] for s_ in SPECS if s_[0] == nm][0]
    top = v[v[:, 2] > z1 - 0.04]; hx = float(top[:, 0].mean())
    meta['hinge'] = (V(hx, y0, z1), V(hx, y1, z1))

# engine inside (own model, CFM56-type core cowl area): the space between the fan duct and the cowl skin
def tube_arc(b, c, r, y, a0, a1, tr, seg=40, ring=8):
    rows = []
    for i in range(seg + 1):
        a = math.radians(a0 + (a1 - a0) * i / seg); u = Vector((math.sin(a), 0, math.cos(a)))
        row = []
        for j in range(ring):
            t = 2 * math.pi * j / ring
            row.append(b.verts.new(c + u * (r + tr * math.cos(t)) + Vector((0, tr * math.sin(t), 0))))
        rows.append(row)
    for i in range(seg):
        for j in range(ring):
            b.faces.new([rows[i][j], rows[i][(j + 1) % ring], rows[i + 1][(j + 1) % ring], rows[i + 1][j]])
    for row in (rows[0], rows[-1]): b.faces.new(row if row is rows[-1] else row[::-1])
def cyl_y(b, c, r, y0, y1, seg=48, inward=False):
    vs0 = []; vs1 = []
    for i in range(seg):
        a = 2 * math.pi * i / seg; d = Vector((math.sin(a) * r, 0, math.cos(a) * r))
        vs0.append(b.verts.new(c + d + Vector((0, y0, 0)))); vs1.append(b.verts.new(c + d + Vector((0, y1, 0))))
    for i in range(seg):
        q = [vs0[i], vs0[(i + 1) % seg], vs1[(i + 1) % seg], vs1[i]]
        b.faces.new(q[::-1] if inward else q)
    return vs0, vs1
def box_at(b, c, u, size):    # a box sat on the case at direction u (x-z), sizes (tangential, along y, radial)
    t = Vector((u.z, 0, -u.x)); r = bmesh.ops.create_cube(b, size=1.0)
    M = Matrix(((t.x * size[0], 0, u.x * size[2]), (0, size[1], 0), (t.z * size[0], 0, u.z * size[2]))).to_4x4()
    M.translation = c
    bmesh.ops.transform(b, matrix=M, verts=r['verts'])
ENG = {}
def r3(v): return [round(float(x), 4) for x in v]
co_all = np.array([v.co[:] for v in o.data.vertices])
for s, en in ((1, 'Eng1'), (-1, 'Eng2')):
    # the nacelle is a single skin with no duct behind it: centre fitted to the fan cowls, everything kept 4 cm inside their
    # innermost point between the fan face and the reverser (y -7.6 .. -8.45), so nothing reaches the skin
    cv = np.vstack([verts('FanCowl_%s_%s' % (en, h)) for h in ('Out', 'In')]); cv = cv[(cv[:, 1] < -7.6) & (cv[:, 1] > -8.45) & (np.hypot(cv[:, 0] - s * ECX, cv[:, 2] - ECZ) > 1.25)]
    A_ = np.c_[2 * cv[:, 0], 2 * cv[:, 2], np.ones(len(cv))]; q_ = np.linalg.lstsq(A_, (cv[:, [0, 2]] ** 2).sum(1), rcond=None)[0]
    c = Vector((float(q_[0]), 0, float(q_[1])))
    rout = float(np.hypot(cv[:, 0] - c.x, cv[:, 2] - c.z).min()); rin = 0.0
    rout = 1.20; rc = 1.06; room = rout - 0.02 - rc   # between the bypass duct wall (r ~1.0-1.05) and the cowl's inner skin (r 1.2+)
    ENG[en] = dict(centre=r3(c), cowl_min_r=round(rout, 3), case_r=round(rc, 3), y=[-8.5, -7.6])
    print('ENGINE %s: centre %.3f %.3f, cowl min r %.3f, case r %.3f' % (en, c.x, c.z, rout, rc))
    def ph(deg): return math.radians(deg) * 1.0
    def U(deg): a = math.radians(deg); return Vector((s * math.sin(a), 0, math.cos(a)))
    # fan case with its stiffening flanges
    b = bmesh.new(); cyl_y(b, c, rc, -8.5, -7.6); cyl_y(b, c, rc - 0.01, -8.5, -7.6, inward=True)
    for yf in (-8.47, -7.63): tube_arc(b, c + Vector((0, yf, 0)), rc + 0.012, 0, -180, 180, 0.012, seg=64, ring=6)
    mkobj('EngCase_%s' % en, b, M_CASE)
    k = 0.6           # (smaller units where the room is tight)
    # accessory gearbox at 8 o'clock with starter and IDG pads, oil tank at 3 o'clock, fwd mount at the top
    b = bmesh.new(); box_at(b, c + U(-150) * (rc + 0.08 * k) + Vector((0, -7.95, 0)), U(-150), (0.55 * k, 0.75, 0.15 * k)); mkobj('EngGearbox_%s' % en, b, M_AGB)
    b = bmesh.new()
    for dy, rr in ((-7.62, 0.07), (-8.25, 0.09)):
        u = U(-150); t = Vector((u.z, 0, -u.x)); p = c + u * (rc + 0.12 * k + 0.02) + Vector((0, dy, 0))
        rr *= k; r_ = bmesh.ops.create_cone(b, cap_ends=True, segments=20, radius1=rr, radius2=rr, depth=0.09 * k)
        M = Matrix((tuple(t), (0, 1, 0), tuple(u))).transposed().to_4x4() @ Matrix.Rotation(0, 4, 'X'); M.translation = p
        bmesh.ops.transform(b, matrix=M, verts=r_['verts'])
    mkobj('EngStarterIDG_%s' % en, b, M_AGB)
    b = bmesh.new(); u = U(100); p = c + u * (rc + 0.10 * k) + Vector((0, -7.9, 0))
    r_ = bmesh.ops.create_cone(b, cap_ends=True, segments=24, radius1=0.09 * k, radius2=0.09 * k, depth=0.5)
    M = Matrix.Rotation(math.radians(90), 4, 'X'); M.translation = p; bmesh.ops.transform(b, matrix=M, verts=r_['verts']); mkobj('EngOilTank_%s' % en, b, M_TANK)
    b = bmesh.new(); box_at(b, c + U(0) * (rc + room * 0.5 + 0.0) + Vector((0, -7.75, 0)), U(0), (0.32, 0.18, room - 0.01)); mkobj('EngMount_%s' % en, b, M_MOUNT)
    # pipes and harness rings round the case, clear of the gearbox, the tank and the mount
    b = bmesh.new()
    for yy, a0, a1 in ((-7.7, -125, 82), (-8.05, -128, 82), (-8.42, -125, 85)): tube_arc(b, c + Vector((0, yy, 0)), rc + 0.035, 0, a0 * s, a1 * s, 0.014)
    mkobj('EngPipes_%s' % en, b, M_PIPE)
    b = bmesh.new()
    for yy, a0, a1 in ((-7.75, -128, 86), (-8.25, -125, 80)): tube_arc(b, c + Vector((0, yy, 0)), rc + 0.03, 0, a0 * s, a1 * s, 0.011)
    mkobj('EngHarness_%s' % en, b, M_HARN)
    for n_ in ('EngCase', 'EngGearbox', 'EngStarterIDG', 'EngOilTank', 'EngMount', 'EngPipes', 'EngHarness'):
        OWN.append(dict(name='%s_%s' % (n_, en), moves_with=None, what='engine inside (own model), seen with the fan cowls open'))
    for half in ('Out', 'In'): liner('FanCowl_%s_%s' % (en, half), 'FanCowl_%s_%s_Inner' % (en, half), 0.03, M_ACOU)
# (tube_arc's 'a0 * s' runs the arcs the right way round on both engines: for s = -1 the angles flip sign, so swap them)
for ob in bpy.data.objects:
    pass

# ------------------------------------------------------------------ hinges: pivot, axis signed so + = open
def rot_about(p, axis, deg, q):
    R = Matrix.Rotation(math.radians(deg), 4, Vector(axis).normalized())
    return p + (R @ (q - p).to_4d()).to_3d() if False else p + (Matrix.Rotation(math.radians(deg), 3, Vector(axis).normalized()) @ (q - p))
def signed(p, axis, q, want):
    a = Vector(axis).normalized(); moved = rot_about(p, a, 10, q) - q
    return a if moved.dot(want) > 0 else -a
def r3(v): return [round(float(x), 4) for x in v]
for name, lo, hi, keep, meta in SPECS:
    v = verts(name)
    if v is None: continue
    lo_, hi_ = v.min(0), v.max(0); cen = Vector(((lo_ + hi_) / 2).tolist())
    kind = meta['kind']; hg = meta['hinge']
    if kind == 'geardoor':
        s = meta['side']; xe = s * (meta['x'][0] if meta['hinge'] == 'inner' else meta['x'][1])
        zt = float(v[np.abs(v[:, 0] - xe) < 0.06][:, 2].mean())
        pa, pb = V(xe, meta['y'][0], zt), V(xe, meta['y'][1], zt)
    else:
        pa, pb = hg
    piv = (pa + pb) / 2; ax = (pb - pa).normalized()
    if kind in ('flap', 'aileron', 'elevator'):
        q = Vector((cen.x, lo_[1], cen.z)); want = V(0, 0, -1)
    elif kind == 'spoiler': q = Vector((cen.x, lo_[1], cen.z)); want = V(0, 0, 1)
    elif kind == 'slat': q = Vector((cen.x, hi_[1], cen.z)); want = V(0, 0, -1)
    elif kind == 'rudder': q = Vector((0, lo_[1], cen.z)); want = V(1, 0, 0)
    elif kind == 'fancowl': q = Vector((cen.x, cen.y, lo_[2])); want = Vector((cen.x - piv.x, 0, 0)).normalized()
    elif kind == 'reverser': q = piv + V(0, -0.5, 0); s = 1 if piv.x > 0 else -1; want = (Vector((s * ECX, piv.y, ECZ)) - piv).normalized()
    elif kind == 'cargo': q = Vector((cen.x, cen.y, lo_[2])); want = V(-1, 0, 0) if meta['way'] == 'out' else V(1, 0, 0)
    elif kind == 'geardoor': q = Vector((cen.x, cen.y, cen.z)); want = V(0, 0, -1)
    ax = signed(piv, ax, q, want)
    meta['pivot'] = piv; meta['axis'] = ax
    PANELS.append(dict(name=name, kind=kind, pivot=r3(piv), axis=r3(ax), hinge_line=[r3(pa), r3(pb)], source=meta['src'], note=meta['note'],
                       motion=meta['motion'], faces=meta['faces'], box=[r3(lo_), r3(hi_)]))

# ---- the gear: legs fold about their top pivots
GEAR = []
def leg(name, follows, want):
    v = verts(name)
    if v is None: return None
    lg = verts(follows); piv = Vector(((lg.min(0) + lg.max(0)) / 2).tolist()); piv.z = float(v[:, 2].max()) - 0.1
    return piv
pn = leg('NoseStrut', 'NoseLeg', None)
if pn is not None:
    ax = signed(pn, V(1, 0, 0), pn + V(0, 0, -1.5), V(0, 1, 0))
    GEAR.append(dict(name='NoseGear', parts=['NoseStrut', 'NoseLeg', 'NoseWheels'], pivot=r3(pn), axis=r3(ax), up_deg=88.0, note='folds forward into the nose well'))
for sd, s in (('_L', -1), ('_R', 1)):
    pm = leg('MainStrut' + sd, 'MainLeg' + sd, None)
    if pm is None: continue
    ax = signed(pm, V(0, 1, 0), pm + V(0, 0, -1.5), V(-s, 0, 0))
    GEAR.append(dict(name='MainGear' + sd, parts=['MainStrut' + sd, 'MainLeg' + sd, 'MainWheels' + sd], pivot=r3(pm), axis=r3(ax), up_deg=88.0,
                     note='folds inward, wheels into the belly well'))
    vb = verts('SideBrace' + sd)
    if vb is not None:
        inner = vb[np.argsort(np.abs(vb[:, 0]))[:20]].mean(0); bp = Vector(inner.tolist())
        GEAR.append(dict(name='SideBrace' + sd, parts=['SideBrace' + sd], pivot=r3(bp), axis=r3(ax), up_deg=88.0,
                         note='folding side stay: turned about its inboard (fixed) end with the leg - an approximation until the two links are split'))
GD = [p for p in PANELS if p['kind'] == 'geardoor']
seq_up = [dict(t=[0.0, 2.0], parts=[p['name'] for p in GD], action='open'),
          dict(t=[2.0, 8.0], parts=[g['name'] for g in GEAR], action='fold up'),
          dict(t=[8.0, 10.0], parts=[p['name'] for p in GD], action='close')]
seq_dn = [dict(t=[0.0, 3.0], parts=[p['name'] for p in GD], action='open'),
          dict(t=[3.0, 12.0], parts=[g['name'] for g in GEAR], action='fold down (free fall, damped)'),
          dict(t=[12.0, 15.0], parts=[p['name'] for p in GD], action='close')]

# ------------------------------------------------------------------ out: glb, json, blend, pictures
for x in bpy.data.objects:
    if x.type == 'MESH': assert all(s.material for s in x.material_slots) and len(x.material_slots), 'empty material slot on ' + x.name
DOC = dict(model='Art/A320/A320.glb (made by Art/A320/make_a320_panels.py from the owner\'s Meshy model; shape unchanged)',
           frame='Blender metres: nose tyre at the origin, nose +y, up +z. The loader mirrors x: Blender +x is the aircraft\'s left (port, engine 1), -x its right (starboard, cargo doors). _L/_R as the existing gear parts: _L = Blender -x.',
           rotation='right-hand rule about axis through pivot; the axis is signed so a positive angle is the opening / deploy direction (flaps, ailerons, elevators: trailing edge down; spoilers: up; slats: leading edge down; rudder: trailing edge to +x; doors and cowls: open)',
           engine='CFM56-5B shape (short fan duct, core nozzle and plug outside): fan cowls on the pylon hinge, a pivoting-door thrust reverser (4 doors)',
           engine_inside=ENG, parts=PANELS, own_parts=OWN,
           gear=dict(parts=GEAR, doors=[p['name'] for p in GD], sequence_up=seq_up, sequence_down=seq_dn, up_s=10.0, down_s=15.0,
                     note='gear down = legs at 0 deg, doors closed; up = legs at up_deg, doors closed. The struts/legs/wheels of each leg move together about its pivot; the wells are lined (NoseGearWell_*, MainGearWell_*).'))
json.dump(DOC, open(os.path.join(ROOT, 'Data', 'a320-panels.json'), 'w'), indent=1)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'A320.glb'), export_format='GLB', use_selection=False,
                          export_apply=True, export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'A320-panels.blend'), compress=True)
print('A320: %d parts cut, %d own parts, %d gear groups' % (len(PANELS), len(OWN), len(GEAR)))
for p in PANELS: print('PART %-24s %-9s faces %6d src %-12s pivot %s' % (p['name'], p['kind'], p['faces'], p['source'], p['pivot']))
for g in GEAR: print('GEAR %s pivot %s axis %s' % (g['name'], g['pivot'], g['axis']))

if os.environ.get('ADP_RENDER') == '1':
    out = os.path.join(ROOT, 'docs', 'Pictures', 'a320-panels'); os.makedirs(out, exist_ok=True)
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading
    sh.light = 'STUDIO'; sh.color_type = 'TEXTURE'; sh.show_cavity = True
    sc.render.resolution_x, sc.render.resolution_y = 3840, 2160
    cam = bpy.data.cameras.new('cam'); co_ = bpy.data.objects.new('cam', cam); sc.collection.objects.link(co_); sc.camera = co_
    cam.lens = 35; cam.clip_end = 400
    co_.location = (-24, 14, 7.5); co_.rotation_euler = (Vector((0, -12, 2.5)) - co_.location).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(out, 'A320-panels-closed.png'); bpy.ops.render.render(write_still=True)
    def turn(name, piv, ax, deg, move=None):
        for nm in [name, name + '_Inner']:
            ob = bpy.data.objects.get(nm)
            if not ob: continue
            p = Vector(piv); R = Matrix.Rotation(math.radians(deg), 4, Vector(ax))
            ob.matrix_world = Matrix.Translation(p) @ R @ Matrix.Translation(-p) @ ob.matrix_world
            if move: ob.matrix_world = Matrix.Translation(Vector(move)) @ ob.matrix_world
    for p in PANELS:
        m = p['motion']; k = p['kind']
        if k in ('flap',): turn(p['name'], p['pivot'], p['axis'], m['deploy_deg'], (0, -m['travel_m'], -0.1))
        elif k == 'slat': turn(p['name'], p['pivot'], p['axis'], m['deploy_deg'], (0, m['travel_m'], -0.05))
        elif k in ('aileron', 'elevator'): turn(p['name'], p['pivot'], p['axis'], 15)
        elif k == 'rudder': turn(p['name'], p['pivot'], p['axis'], 20)
        elif k == 'geardoor': turn(p['name'], p['pivot'], p['axis'], m['open_deg'])
        else: turn(p['name'], p['pivot'], p['axis'], m['open_deg'])
    sc.render.filepath = os.path.join(out, 'A320-panels-open.png'); bpy.ops.render.render(write_still=True)
    print('RENDERED')
print('DONE')
