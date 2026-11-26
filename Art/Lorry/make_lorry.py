# The lorry (3-axle road tractor + 3-axle closed box semi-trailer) from the owner's Meshy model
# (F:/ADP-Meshy/2026-11-26/Lorry, intake table Art/MeshyIntake/README.md). Blender 3.3, headless:
#   blender -b --python Art/Lorry/make_lorry.py
#   python Tools/import_model.py Art/Lorry/make_lorry.py Art/Lorry/lorry.glb Lorry
# The forklift's recipe (Art/CargoForklift/make_cargo_forklift.py): parts lifted out of the one Meshy shell (faces whose
# corners all lie in the part's region), cuts closed on both sides (caps sunk 4 mm), round things decimated far less than
# flat panels, the Meshy texture cleaned (make_lorry_maps.py: no badge, brand or text survives), melted details cut away
# and modelled again (lorry_details.py).
# Axes: x forward, y to the left, z up, metres. Real size: 16.5 m long, 2.55 m wide (trailer body; mirrors stick out),
# 4.0 m high (EU 96/53/EC). Body scaled per axis; every wheel scaled uniformly (stays round) and put back on its axle.
# Two units: 'Tractor' (origin on the ground halfway between the front axle and the tandem centre) and 'Trailer' (origin
# = the kingpin, on the fifth wheel: it turns about its local z there). Every other part carries its own pivot.
import bpy, bmesh, math, os, subprocess, sys
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
SRCDIR = 'F:/ADP-Meshy/2026-11-26/Lorry/Meshy_AI_Create_a_big_transpor_1126091946_texture_obj/'
SRC = SRCDIR + 'Meshy_AI_Create_a_big_transpor_1126091946_texture.obj'
WORK = 'F:/ADP-Export/lorry_work'
CACHE = WORK + '/raw.blend'
TEX = os.path.join(HERE, 'tex')
os.makedirs(WORK, exist_ok=True); os.makedirs(TEX, exist_ok=True)
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, '..', 'CargoForklift'))
import forklift_details as FD                     # the shared helpers: delete_faces, drop_crumbs, planar_snap, Part, lamp

if not os.path.exists(os.path.join(TEX, 'lorry_normal.png')) or '--maps' in sys.argv:
    subprocess.run(['python', '-I', os.path.join(HERE, 'make_lorry_maps.py'), SRCDIR + 'Meshy_AI_Create_a_big_transpor_1126091946_texture.png', TEX], check=True)

if os.path.exists(CACHE):
    bpy.ops.wm.open_mainfile(filepath=CACHE)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.obj(filepath=SRC); bpy.ops.wm.save_as_mainfile(filepath=CACHE)
sc = bpy.context.scene
src = [x for x in sc.objects if x.type == 'MESH'][0]
for x in list(sc.objects):
    if x is not src: bpy.data.objects.remove(x, do_unlink=True)
bpy.context.view_layer.objects.active = src; src.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
src.data.use_auto_smooth = False
if src.data.has_custom_normals: bpy.ops.mesh.customdata_custom_splitnormals_clear()
body = src; body.name = 'Tractor'
V = FD.V


def only(o):
    if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o


def face_all(o, inside):
    me = o.data; lv = np.empty(len(me.loops), np.int64); me.loops.foreach_get('vertex_index', lv)
    ls = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_start', ls)
    return np.logical_and.reduceat(inside[lv], ls)


def split(o, mask, name):
    n = int(mask.sum()); assert n > 0, 'nothing found for ' + name
    only(o); bpy.ops.object.mode_set(mode='EDIT'); bpy.context.tool_settings.mesh_select_mode = (False, False, True)
    bpy.ops.mesh.select_all(action='DESELECT')
    bm = bmesh.from_edit_mesh(o.data); bm.faces.ensure_lookup_table()
    for i in np.nonzero(mask)[0]: bm.faces[int(i)].select_set(True)
    bm.select_flush_mode(); bmesh.update_edit_mesh(o.data)
    bpy.ops.mesh.separate(type='SELECTED'); bpy.ops.object.mode_set(mode='OBJECT')
    new = [x for x in bpy.context.selected_objects if x is not o][-1]; new.name = name; new.data.name = name
    v = V(new); print('PART %-16s %7d faces  box %s %s' % (name, n, v.min(0).round(3), v.max(0).round(3)))
    return new


# ---------------------------------------------------------------- real size (raw Meshy units: front -x, +y = driver's right, z up)
v = V(body); x, y, z = v[:, 0], v[:, 1], v[:, 2]
s = (x > -0.30) & (x < 0.0) & (z > 0.0) & (z < 0.2)
SX = 16.5 / (x.max() - x.min()); SY = 2.55 / (y[s].max() - y[s].min()); SZ = 4.0 / (z.max() - z.min())
ZG = z.min()
print('SCALE x %.3f y %.3f z %.3f (wheels: %.3f all round)' % (SX, SY, SZ, SZ))

# ---------------------------------------------------------------- wheels: centre and radius from the lower half of each tyre
AXLES = (('F', -0.7665), ('D1', -0.312), ('D2', -0.147), ('T1', 0.424), ('T2', 0.564), ('T3', 0.713))
WHEEL = {}
for key, cx0 in AXLES:
    for side, sg in (('R', 1.0), ('L', -1.0)):
        s = (np.abs(x - cx0) < 0.08) & (sg * y > 0.112) & (sg * y < 0.18) & (z < -0.19) & (z > ZG - 0.01)
        cx = (x[s].min() + x[s].max()) / 2; r = (x[s].max() - x[s].min()) / 2; cz = z[s].min() + r
        WHEEL[key + side] = (cx, cz, r, sg)
        print('WHEEL %s%s centre %.4f %.4f r %.4f raw (dia %.3f m)' % (key, side, cx, cz, r, 2 * r * SZ))
PARTS = {}
for k, (cx, cz, r, sg) in WHEEL.items():
    v = V(body); d = np.hypot(v[:, 0] - cx, v[:, 2] - cz)
    PARTS['Wheel_' + k] = split(body, face_all(body, (d < r + 0.004) & (sg * v[:, 1] >= 0.108)), 'Wheel_' + k)

# ---------------------------------------------------------------- tractor | trailer (the trailer sits on the fifth wheel at raw z -0.1035)
KP = -0.27                                      # kingpin, raw x: 0.36 m ahead of the tandem centre
ZT = -0.1035
v = V(body)
trailer = split(body, face_all(body, (v[:, 0] > -0.055) | ((v[:, 0] > -0.575) & (v[:, 2] > ZT))), 'Trailer')
PARTS['Trailer'] = trailer

# ---------------------------------------------------------------- cab doors (hinge at the front edge), trailer rear doors (hinges outboard)
XREAR = lambda zz: np.interp(zz, [-0.085, 0.03, 0.10, 0.15, 0.175], [-0.656, -0.658, -0.68, -0.715, -0.745])
v = V(body); HW = np.abs(v[(v[:, 0] > -0.75) & (v[:, 0] < -0.70) & (v[:, 2] > -0.05) & (v[:, 2] < 0.05), 1]).max()
DOOR = {}
for side, sg in (('R', 1.0), ('L', -1.0)):
    v = V(body)
    m = (v[:, 0] > -0.80) & (v[:, 0] < XREAR(v[:, 2])) & (v[:, 2] > -0.085) & (v[:, 2] < 0.172) & (sg * v[:, 1] > HW - 0.014)
    DOOR[side] = PARTS['Door_' + side] = split(body, face_all(body, m), 'Door_' + side)
v = V(trailer); XR = v[(v[:, 2] > 0) & (v[:, 2] < 0.2), 0].max()
THW = np.abs(v[(v[:, 0] > 0.5) & (v[:, 0] < 0.8) & (v[:, 2] > 0) & (v[:, 2] < 0.2), 1]).max()
ZTOP = v[:, 2].max()
for side, sg in (('R', 1.0), ('L', -1.0)):
    v = V(trailer)
    m = (v[:, 0] > XR - 0.007) & (v[:, 2] > -0.086) & (v[:, 2] < ZTOP - 0.008) & (sg * v[:, 1] > 0.0015) & (sg * v[:, 1] < THW - 0.007)
    PARTS['RearDoor_' + side] = split(trailer, face_all(trailer, m), 'RearDoor_' + side)


def close(o):
    """both sides of each cut closed; caps sunk 4 mm (raw 0.0005) so the two caps of a cut never meet"""
    only(o); bpy.ops.object.mode_set(mode='EDIT'); bm = bmesh.from_edit_mesh(o.data)
    res = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0); caps = res['faces']
    if caps:
        bmesh.ops.inset_region(bm, faces=caps, thickness=0.00004, depth=-0.0005, use_even_offset=True)
        bmesh.ops.poke(bm, faces=caps, offset=0.0, use_relative_offset=False)
    bmesh.update_edit_mesh(o.data); bpy.ops.object.mode_set(mode='OBJECT'); return len(caps)


print('CAPS', {o.name: close(o) for o in [body] + list(PARTS.values())})

# ---------------------------------------------------------------- windows: the dark glass faces out, the opening filled with a real glass part
IMG = np.array(bpy.data.images.load(SRCDIR + 'Meshy_AI_Create_a_big_transpor_1126091946_texture.png').pixels[:], np.float32).reshape(2048, 2048, 4)


def face_lum(o):
    me = o.data; uv = np.empty(len(me.loops) * 2); me.uv_layers[0].data.foreach_get('uv', uv); uv = uv.reshape(-1, 2)
    ls = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_start', ls)
    lt = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_total', lt)
    u = np.add.reduceat(uv, ls) / lt[:, None]
    px = IMG[(u[:, 1] * 2047).astype(int).clip(0, 2047), (u[:, 0] * 2047).astype(int).clip(0, 2047), :3]
    return px @ np.array([0.2126, 0.7152, 0.0722])


def face_centres(o):
    c = np.empty(len(o.data.polygons) * 3); o.data.polygons.foreach_get('center', c)
    n = np.empty(len(o.data.polygons) * 3); o.data.polygons.foreach_get('normal', n); return c.reshape(-1, 3), n.reshape(-1, 3)


def glaze2(o, mask, name):
    only(o); bpy.ops.object.mode_set(mode='EDIT'); bm = bmesh.from_edit_mesh(o.data); bm.faces.ensure_lookup_table()
    gone = [bm.faces[int(i)] for i in np.nonzero(mask)[0]]
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    bmesh.ops.delete(bm, geom=[q for q in bm.verts if not q.link_faces], context='VERTS')
    for f in bm.faces: f.select_set(False)
    caps = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)['faces']
    tri = bmesh.ops.triangulate(bm, faces=caps)['faces']
    for f in tri: f.select_set(True)
    bm.select_flush_mode(); bmesh.update_edit_mesh(o.data)
    bpy.ops.mesh.separate(type='SELECTED'); bpy.ops.object.mode_set(mode='OBJECT')
    g = [q for q in bpy.context.selected_objects if q is not o][-1]; g.name = g.data.name = name
    print('GLASS %-16s cut %d faces, glass %d faces' % (name, len(gone), len(g.data.polygons))); return g


L = face_lum(body); C, N = face_centres(body)
PARTS['Glass_Windscreen'] = glaze2(body, (C[:, 0] < -0.875) & (np.abs(C[:, 1]) < 0.14) & (C[:, 2] > 0.0) & (C[:, 2] < 0.106) & (N[:, 0] < -0.5) & (L < 0.16), 'Glass_Windscreen')
for side, sg in (('R', 1.0), ('L', -1.0)):
    o = DOOR[side]; L = face_lum(o); C, N = face_centres(o)
    PARTS['Door_%s_Glass' % side] = glaze2(o, (C[:, 0] > -0.795) & (C[:, 0] < XREAR(C[:, 2]) - 0.012) & (C[:, 2] > 0.0) & (C[:, 2] < 0.105) & (sg * N[:, 1] > 0.6) & (L < 0.16), 'Door_%s_Glass' % side)

# ---------------------------------------------------------------- crumbs, flat panels flat, faces down where nobody sees them
for o in (body, trailer): print('CRUMBS', o.name, FD.drop_crumbs(o, 60))
count = lambda: sum(len(o.data.polygons) for o in sc.objects if o.type == 'MESH')
print('FACES before', count())


def reduce(o, budget):
    n = len(o.data.polygons)
    if n <= budget: return
    only(o); md = o.modifiers.new('d', 'DECIMATE'); md.ratio = budget / n; md.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier='d')


def wheel_outline(o, cx, cz):
    v = V(o); a = np.degrees(np.arctan2(v[:, 2] - cz, v[:, 0] - cx)); d = np.hypot(v[:, 0] - cx, v[:, 2] - cz)
    out = np.zeros(72); np.maximum.at(out, ((a + 180) // 5).astype(int).clip(0, 71), d); return out


before = {k: wheel_outline(PARTS['Wheel_' + k], WHEEL[k][0], WHEEL[k][1]) for k in WHEEL}
reduce(body, 125000); reduce(trailer, 42000)
for k in WHEEL: reduce(PARTS['Wheel_' + k], 9000)
for k in ('L', 'R'): reduce(PARTS['Door_' + k], 7000); reduce(PARTS['RearDoor_' + k], 2500)
for k in WHEEL:
    a, b = before[k], wheel_outline(PARTS['Wheel_' + k], WHEEL[k][0], WHEEL[k][1])
    print('WHEEL %s outline: largest change %.1f mm, spread %.1f mm' % (k, np.abs(a - b).max() * SZ * 1000, b.std() * SZ * 1000))
print('FACES after', count())

# ---------------------------------------------------------------- materials: cleaned Meshy colour + PBR per kind
def img(name, colour):
    im = bpy.data.images.load(os.path.join(TEX, name), check_existing=True)
    im.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'; return im


def texmat(name, mr):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    b = nt.nodes.new('ShaderNodeTexImage'); b.image = img('lorry_base.png', True); nt.links.new(b.outputs['Color'], p.inputs['Base Color'])
    r = nt.nodes.new('ShaderNodeTexImage'); r.image = img('lorry_mr_%s.png' % mr, False)
    sp = nt.nodes.new('ShaderNodeSeparateRGB'); nt.links.new(r.outputs['Color'], sp.inputs['Image'])
    nt.links.new(sp.outputs['G'], p.inputs['Roughness']); nt.links.new(sp.outputs['B'], p.inputs['Metallic'])
    n = nt.nodes.new('ShaderNodeTexImage'); n.image = img('lorry_normal.png', False)
    nm = nt.nodes.new('ShaderNodeNormalMap'); nt.links.new(n.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], p.inputs['Normal'])
    return m


MC, MT, MY = texmat('M_Lorry_Cab', 'cab'), texmat('M_Lorry_TrailerPanels', 'trailer'), texmat('M_Lorry_Tyre', 'tyre')
MG = FD.material('M_Lorry_Glass', (0.10, 0.12, 0.13), 0.03, 0.0)
MG.blend_method = 'BLEND'; MG.node_tree.nodes['Principled BSDF'].inputs['Alpha'].default_value = 0.28
for o in [body] + list(PARTS.values()):
    m = MG if 'Glass' in o.name else MY if o.name.startswith('Wheel_') else MT if o.name.startswith(('Trailer', 'RearDoor')) else MC
    o.data.materials.clear(); o.data.materials.append(m)

# ---------------------------------------------------------------- to game metres
XM = (WHEEL['FR'][0] + (WHEEL['D1R'][0] + WHEEL['D2R'][0]) / 2) / 2
T = Matrix(((-SX, 0, 0, XM * SX), (0, -SY, 0, 0), (0, 0, SZ, -ZG * SZ), (0, 0, 0, 1)))
G = lambda p: np.array(T @ Vector(p))
for k, (cx, cz, r, sg) in WHEEL.items():           # round: about its own centre by SZ in x and z, then onto its axle
    o = PARTS['Wheel_' + k]; c = Vector((cx, 0, cz))
    o.data.transform(Matrix.Translation(Vector(G((cx, 0, cz))) * Vector((1, 0, 1))) @ Matrix(((-SZ, 0, 0, 0), (0, -SY, 0, 0), (0, 0, SZ, 0), (0, 0, 0, 1))) @ Matrix.Translation(-c))
for o in [body] + [p for k, p in PARTS.items() if not k.startswith('Wheel_')]: o.data.transform(T)
for o in [body] + list(PARTS.values()):
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32)
    o.data.polygons.foreach_set('use_smooth', [True] * len(o.data.polygons)); o.data.update()
for o in (body, trailer, *[PARTS['Door_' + k] for k in 'LR'], *[PARTS['RearDoor_' + k] for k in 'LR']):
    print('FLAT', o.name, 'vertices snapped', FD.planar_snap(o, cell=0.4, tol=0.012))

GEO = dict(SX=SX, SY=SY, SZ=SZ, G=G, WHEEL=WHEEL, KP=KP, ZT=ZT, HW=HW, XR=XR, THW=THW, ZTOP=ZTOP, XREAR=XREAR)
import lorry_details as LD
INFO, IKS = LD.build(sc, body, trailer, PARTS, GEO)
import lorry_cab as LC                              # the modelled cab, interior, fenders (owner: Meshy cab crumpled)
I2, K2 = LC.build(sc, body, trailer, PARTS); INFO += I2; IKS += K2

# ---------------------------------------------------------------- save, check, export
col = bpy.data.collections.new('Lorry'); sc.collection.children.link(col)
ALL = [o for o in sc.objects if o.type == 'MESH']
for o in ALL + IKS:
    for c in list(o.users_collection): c.objects.unlink(o)
    col.objects.link(o)
for o in ALL:
    assert len(o.data.materials) and all(o.data.materials), 'empty material slot on ' + o.name
    o.data.name = o.name
print('\n'.join('INFO ' + s for s in INFO))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Lorry.blend'))
for o in sc.objects: o.select_set(o in ALL or o in IKS)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'lorry.glb'), export_format='GLB', use_selection=True, export_apply=True,
                          export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO')
v = np.concatenate([np.array([o.matrix_world @ q.co for q in o.data.vertices]) for o in ALL])
print('SIZE %.3f x %.3f x %.3f m, faces %d, parts %d' % (*(v.max(0) - v.min(0)), count(), len(ALL)))
print('DONE')
