# The cargo forklift (3.5 t counterbalance, pneumatic tyres, open overhead guard) from the owner's Meshy model
# (F:/ADP-Meshy/2026-11-26/CargoForklift, intake table Art/MeshyIntake/README.md). Blender 3.3, headless:
#   blender -b --python Art/CargoForklift/make_cargo_forklift.py
#   python Tools/import_model.py Art/CargoForklift/make_cargo_forklift.py Art/CargoForklift/cargoforklift.glb CargoForklift
# Axes (as the pushback tug): x forward (forks at +x), y to the left, z up; metres; origin on the ground halfway between
# the axles. Real size: x and z scaled alike (round wheels) to 4.0 m long with the forks, y to 1.5 m wide.
# Parts lifted out of the one Meshy shell (as Art/A320/make_a320_parts.py): every face whose corners all lie in the
# part's region goes to the part; both sides of each cut are closed (the caps poked 4 mm inwards so they never lie on
# each other). Faces reduced only where nobody notices: the plain body is cut down hard, its round details (cylinders,
# exhaust, gas bottle, steering column, lamps), the wheels, the mast and the controls far less; wheel outlines checked.
import bpy, bmesh, math, os, subprocess, sys
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
SRCDIR = 'F:/ADP-Meshy/2026-11-26/CargoForklift/Meshy_AI_Create_a_high_quality_1126092008_texture_obj/'
SRC = SRCDIR + 'Meshy_AI_Create_a_high_quality_1126092008_texture.obj'
WORK = 'F:/ADP-Export/forklift_work'
CACHE = WORK + '/raw.blend'                 # the OBJ once read in (40 s); big files stay on F:
TEX = os.path.join(HERE, 'tex')
S, SY = 2.1, 1.9                            # raw -> metres: length/height, width
os.makedirs(WORK, exist_ok=True); os.makedirs(TEX, exist_ok=True)

# ---------------------------------------------------------------- the textures (system Python, PIL)
if not os.path.exists(os.path.join(TEX, 'forklift_normal.png')) or '--maps' in sys.argv:
    subprocess.run(['python', os.path.join(HERE, 'make_forklift_maps.py'), SRCDIR + 'Meshy_AI_Create_a_high_quality_1126092008_texture.png', TEX], check=True)

# ---------------------------------------------------------------- the shell, in raw Meshy units (front -x, z up)
if os.path.exists(CACHE):
    bpy.ops.wm.open_mainfile(filepath=CACHE)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.obj(filepath=SRC)
    bpy.ops.wm.save_as_mainfile(filepath=CACHE)
sc = bpy.context.scene
src = [x for x in sc.objects if x.type == 'MESH'][0]
for x in list(sc.objects):
    if x is not src: bpy.data.objects.remove(x, do_unlink=True)
bpy.context.view_layer.objects.active = src; src.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
src.data.use_auto_smooth = False
if src.data.has_custom_normals:
    bpy.ops.mesh.customdata_custom_splitnormals_clear()


def V(o):
    a = np.empty(len(o.data.vertices) * 3, np.float64); o.data.vertices.foreach_get('co', a); return a.reshape(-1, 3)


def face_all(o, inside):
    """faces whose every corner is inside (inside: bool per vertex)"""
    me = o.data; nl = len(me.loops)
    lv = np.empty(nl, np.int64); me.loops.foreach_get('vertex_index', lv)
    ls = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_start', ls)
    return np.logical_and.reduceat(inside[lv], ls)


def only(o):
    if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o


def split(o, mask, name):
    """the masked faces of o as their own object"""
    n = int(mask.sum()); assert n > 0, 'nothing found for ' + name
    only(o); bpy.ops.object.mode_set(mode='EDIT'); bpy.context.tool_settings.mesh_select_mode = (False, False, True)
    bpy.ops.mesh.select_all(action='DESELECT')
    bm = bmesh.from_edit_mesh(o.data); bm.faces.ensure_lookup_table()
    for i in np.nonzero(mask)[0]: bm.faces[int(i)].select_set(True)
    bm.select_flush_mode(); bmesh.update_edit_mesh(o.data)
    bpy.ops.mesh.separate(type='SELECTED'); bpy.ops.object.mode_set(mode='OBJECT')
    new = [x for x in bpy.context.selected_objects if x is not o][-1]
    new.name = name; new.data.name = name
    v = V(new); print('PART %-14s %7d faces  box %s %s' % (name, n, v.min(0).round(3), v.max(0).round(3)))
    return new


# the three loose pieces: the body, the carriage with its forks, the seat cushion
only(src); bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
isl = sorted([x for x in sc.objects if x.type == 'MESH'], key=lambda x: -len(x.data.polygons))
body, carriage, cushion = isl[0], isl[1], isl[2]
body.name = 'Body'; carriage.name = 'Carriage'; cushion.name = 'Seat'
assert V(carriage)[:, 0].min() < -0.9, 'the fork piece is not where it was'

# ---------------------------------------------------------------- wheels: centres from the outer sidewalls
v = V(body); x, y, z = v[:, 0], v[:, 1], v[:, 2]
ZG = min(V(o)[:, 2].min() for o in isl)
PARTS = {}
WHEEL = {}
for key, cx0, ycut in (('F', -0.245, 0.232), ('R', 0.697, 0.228)):
    for side, sg in (('R', 1.0), ('L', -1.0)):         # raw +y is the driver's right (he faces raw -x)
        s = (np.abs(x - cx0) < 0.22) & (sg * y > 0.365) & (z < -0.2)
        cx, cz = (x[s].min() + x[s].max()) / 2, (z[s].min() + z[s].max()) / 2
        r = max(x[s].max() - x[s].min(), z[s].max() - z[s].min()) / 2
        WHEEL[key + side] = (cx, cz, r, sg, ycut)
for k, (cx, cz, r, sg, ycut) in WHEEL.items():
    v = V(body); d = np.hypot(v[:, 0] - cx, v[:, 2] - cz)
    PARTS['Wheel_' + k] = split(body, face_all(body, (d < r + 0.006) & (sg * v[:, 1] >= ycut)), 'Wheel_' + k)
    if k[0] == 'R':      # the rear wheels steer: the stub/hub between wheel and frame is the knuckle
        v = V(body); d = np.hypot(v[:, 0] - cx, v[:, 2] - cz)
        PARTS['SteerKnuckle_' + k[1]] = split(body, face_all(body, (d < 0.085) & (sg * v[:, 1] >= 0.19) & (sg * v[:, 1] < ycut)), 'SteerKnuckle_' + k[1])

# ---------------------------------------------------------------- forks off the carriage; the mast off the body
for side, sg in (('R', 1.0), ('L', -1.0)):
    v = V(carriage)
    PARTS['Fork_' + side] = split(carriage, face_all(carriage, (v[:, 0] < -0.528) & (sg * v[:, 1] > 0.11) & (sg * v[:, 1] < 0.21)), 'Fork_' + side)
PARTS['Carriage'] = carriage
v = V(body)
mast = face_all(body, (v[:, 0] < -0.33) | ((v[:, 0] < -0.265) & (v[:, 2] > -0.06)))
m = split(body, mast, 'Mast')
v = V(m)
PARTS['MastInner'] = split(m, face_all(m, v[:, 0] < -0.442), 'MastInner')
m.name = m.data.name = 'MastOuter'; PARTS['MastOuter'] = m

# ---------------------------------------------------------------- controls and seat
# steering wheel: the ring's plane from the side view (top edge raw (0.0, 0.156), bottom (0.134, 0.078)), ring r ~0.078
SWC = np.array([0.067, 0.0, 0.117]); SWN = np.array([0.078, 0.0, 0.134]); SWN /= np.linalg.norm(SWN)
v = V(body); rel = v - SWC; h = rel @ SWN; pl = np.linalg.norm(rel - np.outer(h, SWN), axis=1)
PARTS['SteeringWheel'] = split(body, face_all(body, (np.abs(h) < 0.018) & (pl < 0.088)), 'SteeringWheel')
v = V(body)
lev = split(body, face_all(body, (v[:, 0] > 0.03) & (v[:, 0] < 0.11) & (v[:, 1] > -0.135) & (v[:, 1] < -0.035) & (v[:, 2] > -0.02) & (v[:, 2] < 0.075)), 'Levers')
v = V(body)
back = split(body, face_all(body, (v[:, 0] > 0.29) & (v[:, 0] < 0.385) & (np.abs(v[:, 1]) < 0.12) & (v[:, 2] > 0.0) & (v[:, 2] < 0.18)), 'SeatBack')
only(cushion); back.select_set(True); bpy.ops.object.join(); cushion.name = cushion.data.name = 'Seat'; PARTS['Seat'] = cushion
# the lever bank: each loose stick its own part (Lever_1.. from the driver's left)
only(lev); bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
levers = [o for o in sc.objects if o.name.startswith('Levers')]
levers = [o for o in levers if len(o.data.polygons) >= 40] + [o for o in levers if len(o.data.polygons) < 40]
small = [o for o in levers if len(o.data.polygons) < 40]
for o in small:                                      # crumbs go back to the body
    only(body); o.select_set(True); bpy.ops.object.join()
levers = sorted([o for o in levers if o not in small], key=lambda o: V(o)[:, 1].mean())
# a knob or grip that came loose from its stick goes back onto the stick whose outline (seen from above) it overlaps
def _ov(a, b, m=0.005):
    (la, ha), (lb, hb) = (V(a).min(0), V(a).max(0)), (V(b).min(0), V(b).max(0))
    return all(la[k] <= hb[k] + m and lb[k] <= ha[k] + m for k in (0, 1))
merged = True
while merged:
    merged = False
    for a in levers:
        for b in levers:
            if a is not b and _ov(a, b):
                big, sm = (a, b) if len(a.data.polygons) >= len(b.data.polygons) else (b, a)
                only(big); sm.select_set(True); bpy.ops.object.join(); levers.remove(sm); merged = True; break
        if merged: break
levers = sorted(levers, key=lambda o: V(o)[:, 1].mean())
for i, o in enumerate(levers):
    o.name = o.data.name = 'Lever_%d' % (i + 1); PARTS[o.name] = o
print('LEVERS', len(levers))

# ---------------------------------------------------------------- close the cuts (both sides, caps 4 mm apart)
def close(o):
    only(o); bpy.ops.object.mode_set(mode='EDIT')
    bm = bmesh.from_edit_mesh(o.data)
    edges = [e for e in bm.edges if e.is_boundary]
    n0 = len(bm.faces)
    res = bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    caps = res['faces']
    if caps:
        # sink each cap 4 mm into its own part behind a thin steep rim, so the two caps of a cut (and their edges) never meet
        bmesh.ops.inset_region(bm, faces=caps, thickness=0.0003, depth=-0.004, use_even_offset=True)
        bmesh.ops.poke(bm, faces=caps, offset=0.0, use_relative_offset=False)
    bmesh.update_edit_mesh(o.data); bpy.ops.object.mode_set(mode='OBJECT')
    return len(caps)
print('CAPS', {o.name: close(o) for o in [body] + list(PARTS.values())})

# ---------------------------------------------------------------- face budget: round things kept round
def count(): return sum(len(o.data.polygons) for o in sc.objects if o.type == 'MESH')
print('FACES before', count())

def wheel_outline(o, cx, cz):
    """outer radius in 5-degree steps (raw): spread = jaggedness of the silhouette"""
    v = V(o); a = np.degrees(np.arctan2(v[:, 2] - cz, v[:, 0] - cx)); d = np.hypot(v[:, 0] - cx, v[:, 2] - cz)
    b = ((a + 180) // 5).astype(int); out = np.zeros(72)
    np.maximum.at(out, b.clip(0, 71), d)
    return out

# (Blender's collapse decimation reads the vertex group's weight one way or the other: find out on a test sphere)
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=5)
t = bpy.context.object; g = t.vertex_groups.new(name='keep')
g.add([p.index for p in t.data.vertices if p.co.z > 0], 1.0, 'REPLACE')
md = t.modifiers.new('d', 'DECIMATE'); md.ratio = 0.2; md.vertex_group = 'keep'; md.vertex_group_factor = 10.0
bpy.ops.object.modifier_apply(modifier='d')
up = sum(1 for p in t.data.vertices if p.co.z > 0.05); dn = sum(1 for p in t.data.vertices if p.co.z < -0.05)
KEEP_INVERT = up < dn
print('DECIMATE vertex group: weight 1 keeps faces' if not KEEP_INVERT else 'DECIMATE vertex group: weight 0 keeps faces', up, dn)
bpy.data.objects.remove(t, do_unlink=True)


def reduce(o, budget, keep_mask=None):
    n = len(o.data.polygons)
    if n <= budget: return
    only(o)
    md = o.modifiers.new('d', 'DECIMATE'); md.ratio = budget / n; md.use_collapse_triangulate = True
    if keep_mask is not None:
        g = o.vertex_groups.new(name='keep')
        idx = np.nonzero(keep_mask if not KEEP_INVERT else ~keep_mask)[0].tolist()
        g.add(idx, 1.0, 'REPLACE'); md.vertex_group = 'keep'; md.vertex_group_factor = 10.0
    bpy.ops.object.modifier_apply(modifier='d')
    for gg in list(o.vertex_groups): o.vertex_groups.remove(gg)


before = {k: wheel_outline(PARTS['Wheel_' + k], WHEEL[k][0], WHEEL[k][1]) for k in WHEEL}
v = V(body); x, y, z = v[:, 0], v[:, 1], v[:, 2]
round_details = ((x > 0.55) & (z > 0.03)) \
    | ((x > -0.33) & (x < -0.12) & (z > -0.14) & (z < 0.02) & (np.abs(y) < 0.26)) \
    | ((x > -0.06) & (x < 0.10) & (np.abs(y) < 0.07) & (z > -0.25) & (z < 0.12)) \
    | ((x > 0.28) & (x < 0.36) & (z > 0.40)) \
    | ((x > -0.15) & (x < -0.05) & (z > 0.10) & (z < 0.26))          # exhaust + gas bottle, tilt cylinders, column, beacon, mirrors
reduce(body, 150000, round_details)
for k in WHEEL: reduce(PARTS['Wheel_' + k], 22000)
for k in ('MastOuter', 'MastInner'): reduce(PARTS[k], 14000)
reduce(PARTS['Carriage'], 9000)
for k in ('Fork_L', 'Fork_R'): reduce(PARTS[k], 2000)
for k in ('SteerKnuckle_L', 'SteerKnuckle_R'): reduce(PARTS[k], 2500)
reduce(PARTS['Seat'], 7000); reduce(PARTS['SteeringWheel'], 5000)
for o in levers: reduce(o, 1500)
for k in WHEEL:
    a, b = before[k], wheel_outline(PARTS['Wheel_' + k], WHEEL[k][0], WHEEL[k][1])
    print('WHEEL %s outline radius spread before %.4f after %.4f, largest change %.4f raw (%.1f mm)'
          % (k, a.std(), b.std(), np.abs(a - b).max(), np.abs(a - b).max() * S * 1000))
print('FACES after', count())

# ---------------------------------------------------------------- materials: Meshy colour + real PBR per kind
def img(name, colour):
    im = bpy.data.images.load(os.path.join(TEX, name), check_existing=True)
    im.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    return im


def mat(name, mr):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    b = nt.nodes.new('ShaderNodeTexImage'); b.image = img('forklift_base.png', True)
    nt.links.new(b.outputs['Color'], p.inputs['Base Color'])
    r = nt.nodes.new('ShaderNodeTexImage'); r.image = img('forklift_mr_%s.png' % mr, False)
    sp = nt.nodes.new('ShaderNodeSeparateRGB'); nt.links.new(r.outputs['Color'], sp.inputs['Image'])
    nt.links.new(sp.outputs['G'], p.inputs['Roughness']); nt.links.new(sp.outputs['B'], p.inputs['Metallic'])
    n = nt.nodes.new('ShaderNodeTexImage'); n.image = img('forklift_normal.png', False)
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 1.0
    nt.links.new(n.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], p.inputs['Normal'])
    return m


M = {k: mat('M_Forklift_' + k.capitalize(), k) for k in ('body', 'tyre', 'steel', 'plastic')}
KIND = {'Body': 'body', 'Wheel_': 'tyre', 'SteerKnuckle': 'steel', 'Fork': 'steel', 'Carriage': 'steel', 'Mast': 'steel',
        'Seat': 'plastic', 'SteeringWheel': 'plastic', 'Lever': 'plastic'}
ALL = [body] + list(PARTS.values())
for o in ALL:
    k = next(v for kk, v in KIND.items() if o.name.startswith(kk))
    o.data.materials.clear(); o.data.materials.append(M[k])

# ---------------------------------------------------------------- to game metres: x forward, y left, z up
XM = (WHEEL['FR'][0] + WHEEL['RR'][0]) / 2
T = Matrix(((-S, 0, 0, XM * S), (0, -SY, 0, 0), (0, 0, S, -ZG * S), (0, 0, 0, 1)))
def G(p): return np.array(T @ Vector(p))
for o in ALL:
    o.data.transform(T); o.data.update()
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(40)
    o.data.polygons.foreach_set('use_smooth', [True] * len(o.data.polygons))
body.data.name = 'CargoForklift'


def pivot(o, p, rot=None):
    p = Vector(p); W = Matrix.Translation(p) @ (rot.to_4x4() if rot else Matrix.Identity(4))
    o.data.transform(W.inverted()); o.matrix_world = W


def box(o): v = V(o); return v.min(0), v.max(0)


INFO = []
for k, (cx, cz, r, sg, ycut) in WHEEL.items():
    o = PARTS['Wheel_' + k]; lo, hi = box(o)
    side = 'L' if sg < 0 else 'R'; nm = 'Wheel_%s%s' % (k[0], side)
    o.name = o.data.name = nm
    c = G((cx, 0, cz)); pivot(o, (c[0], (lo[1] + hi[1]) / 2, c[2]))
    INFO.append('%s axle %.3f %.3f %.3f  dia %.3f' % (nm, c[0], (lo[1] + hi[1]) / 2, c[2], 2 * r * S))
for side in ('L', 'R'):
    o = PARTS['SteerKnuckle_' + side]; lo, hi = box(o); c = (lo + hi) / 2
    pivot(o, (c[0], c[1], c[2])); INFO.append('SteerKnuckle_%s kingpin (vertical) %.3f %.3f %.3f' % (side, *c))
fa = G((WHEEL['FR'][0], 0, WHEEL['FR'][1]))
pivot(PARTS['MastOuter'], (fa[0], 0, fa[2])); INFO.append('MastOuter tilt pin on the front axle %.3f 0 %.3f (y axis)' % (fa[0], fa[2]))
lo, hi = box(PARTS['MastInner']); pivot(PARTS['MastInner'], ((lo[0] + hi[0]) / 2, 0, lo[2])); INFO.append('MastInner foot %.3f 0 %.3f, lifts along z' % ((lo[0] + hi[0]) / 2, lo[2]))
lo, hi = box(PARTS['Carriage']); pivot(PARTS['Carriage'], (lo[0], 0, lo[2])); INFO.append('Carriage back-bottom %.3f 0 %.3f, lifts along z' % (lo[0], lo[2]))
# (raw +y is the driver's right and becomes game -y, his right: the raw side names stay)
for side in ('L', 'R'):
    o = bpy.data.objects['Fork_' + side]; lo, hi = box(o); p = (lo[0], (lo[1] + hi[1]) / 2, hi[2])
    pivot(o, p); INFO.append('Fork_%s hook %.3f %.3f %.3f, slides along y' % (side, *p))
lo, hi = box(PARTS['Seat']); seat_top = None
v = V(PARTS['Seat']); cush = v[v[:, 0] > (lo[0] + hi[0]) / 2]          # (game +x is forward: the cushion is the front half)
pivot(PARTS['Seat'], ((lo[0] + hi[0]) / 2, 0, lo[2]))
sw_c = G(SWC); sw_n = np.array(T.to_3x3() @ Vector(SWN)); sw_n /= np.linalg.norm(sw_n)
if sw_n[2] < 0: sw_n = -sw_n
zax = Vector(sw_n); xax = Vector((0, 1, 0)).cross(zax).normalized(); yax = zax.cross(xax)
R = Matrix((xax, yax, zax)).transposed()
pivot(PARTS['SteeringWheel'], sw_c, R)
vsw = np.array([PARTS['SteeringWheel'].matrix_world @ q.co for q in PARTS['SteeringWheel'].data.vertices])
rr = np.linalg.norm((vsw - sw_c) - np.outer((vsw - sw_c) @ sw_n, sw_n), axis=1).max()
INFO.append('SteeringWheel hub %.3f %.3f %.3f, turns about its local z (%.2f %.2f %.2f), rim r %.3f' % (*sw_c, *sw_n, rr))
lever_tops = []
for o in levers:
    lo, hi = box(o); v = V(o); top = v[v[:, 2].argmax()]
    pivot(o, ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2])); lever_tops.append(top)
    INFO.append('%s base %.3f %.3f %.3f, knob %.3f %.3f %.3f, tips about y' % (o.name, (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2], *top))

# ---------------------------------------------------------------- IK points, from the same numbers as the parts
IKS = []
def ik(name, p):
    e = bpy.data.objects.new('IK_' + name, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
    e.location = Vector(p); sc.collection.objects.link(e); IKS.append(e); INFO.append('IK_%s %.3f %.3f %.3f' % (name, *p))
ik('Seat_Driver', (cush[:, 0].mean(), 0.0, cush[:, 2].max()))
bv = V(PARTS['Seat']); bv = np.array([PARTS['Seat'].matrix_world @ Vector(q) for q in bv])
ik('Back_Driver', (bv[:, 0].min() + 0.02, 0.0, cush[:, 2].max() + 0.30))
yl = Vector(yax) if Vector(yax).y > 0 else -Vector(yax)
for side, sg in (('L', 1), ('R', -1)):
    ik('Hand_' + side, tuple(Vector(sw_c) + sg * yl * rr * 0.97))
for i, t in enumerate(lever_tops): ik('Hand_Lever_%d' % (i + 1), t)
bv = V(body)
# floor in front of the seat box: highest body point there (raw x -0.05..0.09, |y| < 0.15, below the dash)
fl = bv[(bv[:, 0] > G((0.09, 0, 0))[0]) & (bv[:, 0] < G((-0.05, 0, 0))[0]) & (np.abs(bv[:, 1]) < 0.15 * SY) & (bv[:, 2] < G((0, 0, -0.10))[2])]
fl = fl[np.abs(fl[:, 1]) > 0.05 * SY]                                  # off the steering column / console in the middle
if len(fl) > 50:                                                        # the floor plate is the most crowded height there
    hz, ez = np.histogram(fl[:, 2], bins=np.arange(fl[:, 2].min(), fl[:, 2].max() + 0.02, 0.02)); floor_z = ez[hz.argmax()] + 0.01
else: floor_z = G((0, 0, -0.175))[2]
if abs(floor_z - G((0, 0, -0.175))[2]) > 0.12: floor_z = G((0, 0, -0.175))[2]   # the measured cab floor (raw z -0.175)
px = G((-0.03, 0, 0))[0]
ik('Foot_Accel', (px, -0.12, floor_z + 0.08)); ik('Foot_Brake', (px, 0.10, floor_z + 0.08))
# the step on the driver's left (game +y): the body's outermost point at the side step (raw x 0..0.17, z -0.30..-0.27)
st = bv[(bv[:, 0] < G((0.0, 0, 0))[0]) & (bv[:, 0] > G((0.17, 0, 0))[0]) & (bv[:, 2] > G((0, 0, -0.30))[2]) & (bv[:, 2] < G((0, 0, -0.27))[2]) & (bv[:, 1] > 0)]
ik('Step', (st[:, 0].mean(), st[:, 1].max() - 0.06, st[:, 2].max()) if len(st) else tuple(G((0.085, -0.30, -0.27))))
gh = bv[(bv[:, 0] < G((-0.13, 0, 0))[0]) & (bv[:, 0] > G((-0.04, 0, 0))[0]) & (bv[:, 2] > G((0, 0, 0.0))[2]) & (bv[:, 2] < G((0, 0, 0.08))[2]) & (bv[:, 1] > 0.2 * SY)]
ik('Grab_Outside', tuple(gh.mean(0)) if len(gh) else tuple(G((-0.09, -0.29, 0.04))))
print('\n'.join('INFO ' + s for s in INFO))

# ---------------------------------------------------------------- save, check, export
col = bpy.data.collections.new('CargoForklift'); sc.collection.children.link(col)
ALL = [body] + [o for o in sc.objects if o.type == 'MESH' and o is not body]
for o in ALL + IKS:
    for c in list(o.users_collection): c.objects.unlink(o)
    col.objects.link(o)
for o in ALL: assert len(o.data.materials) and all(o.data.materials), 'empty material slot on ' + o.name
for side in ('L', 'R'):                                             # the knuckles' faces lie on the axle housing: shrink 2 %
    bpy.data.objects['SteerKnuckle_' + side].data.transform(Matrix.Scale(0.98, 4))   # about their middle (the kingpin pivot)
# a knuckle face lying on a body face (the Meshy skin is doubled there) is hidden by it anyway: drop it, so nothing flickers
from mathutils.bvhtree import BVHTree
dg = bpy.context.evaluated_depsgraph_get(); tree = BVHTree.FromObject(body, dg)
for side in ('L', 'R'):
    k = bpy.data.objects['SteerKnuckle_' + side]; mw = k.matrix_world; bm = bmesh.new(); bm.from_mesh(k.data); gone = []
    for f in bm.faces:
        c = mw @ f.calc_center_median(); hit = tree.find_nearest(c, 0.003)
        if hit[0] is not None and abs(hit[1].dot((mw.to_3x3() @ f.normal).normalized())) > 0.97: gone.append(f)
    bmesh.ops.delete(bm, geom=gone, context='FACES'); bm.to_mesh(k.data); bm.free(); print('KNUCKLE', side, 'dropped', len(gone))
for o in ALL[1:]: o.data.name = o.name                            # mesh names = part names (not the Meshy file name)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'CargoForklift.blend'))
for o in sc.objects: o.select_set(o in ALL or o in IKS)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'cargoforklift.glb'), export_format='GLB', use_selection=True, export_apply=True,
                          export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO')
v = np.concatenate([np.array([o.matrix_world @ q.co for q in o.data.vertices]) for o in ALL])
print('SIZE %.3f x %.3f x %.3f m, faces %d, parts %d' % (*(v.max(0) - v.min(0)), count(), len(ALL)))
print('DONE')
