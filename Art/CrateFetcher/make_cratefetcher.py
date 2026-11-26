# The crate fetcher: a heavy rough-terrain forklift with a front mast, fork carriage and a telescopic boom from the rear
# that lifts the inner mast, closed cab on the left. From the owner's Meshy model (F:/ADP-Meshy/2026-11-26/CrateFetcher,
# intake table Art/MeshyIntake/README.md). Blender 3.3, headless:
#   blender -b --python Art/CrateFetcher/make_cratefetcher.py
#   python Tools/import_model.py Art/CrateFetcher/make_cratefetcher.py Art/CrateFetcher/cratefetcher.glb CrateFetcher
# Recipe of the lorry (Art/Lorry/make_lorry.py): parts lifted out of the one Meshy shell (faces whose corners all lie in the
# part's region), cuts closed (caps sunk), flat panels decimated and snapped flat, round things (wheels, cylinders, lamps,
# steering wheel) modelled again round, texture cleaned (make_cratefetcher_maps.py: no badge, brand or text survives),
# melted details cut away and modelled (cratefetcher_details.py).
# Real size, from the published data of the class "heavy rough-terrain / container forklift, 12-16 t" (makers' data
# sheets of the class: width over tyres 2.8-3.0 m, tyres 1.3-1.6 m, cab roof 2.8-3.1 m, length to the fork face 6-6.5 m,
# forks 1.8-2.4 m). Meshy's cab is small for the machine, so the cab sets the scale: x and z by 4.3 (cab inside 1.25 m high,
# roof 2.83 m, tyres 1.44 m front / 1.50 m rear, 8.2 m long with 2.2 m tines), y by 3.6 (2.83 m over the tyres, 3.10 m over the mirrors). Mast top 6.1 m: a little tall for the
# class (the boom head sits on it). Wheels and every modelled round part stay perfectly round.
# Axes as the pushback tug / forklift: x forward (forks at +x), y left, z up, metres; origin on the ground halfway
# between the axles. Raw Meshy: front -x, +y = the machine's right, z up (after the default OBJ import).
import bpy, bmesh, math, os, subprocess, sys
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
SRCDIR = 'F:/ADP-Meshy/2026-11-26/CrateFetcher/Meshy_AI_Create_an_Airport_veh_1126092120_texture_obj/'
SRC = SRCDIR + 'Meshy_AI_Create_an_Airport_veh_1126092120_texture.obj'
WORK = 'F:/ADP-Export/cratefetcher_work'
CACHE = WORK + '/raw.blend'
TEX = os.path.join(HERE, 'tex')
os.makedirs(WORK, exist_ok=True); os.makedirs(TEX, exist_ok=True)
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, '..', 'CargoForklift')); sys.path.insert(0, os.path.join(HERE, '..', 'Lorry'))
import forklift_details as FD

if not os.path.exists(os.path.join(TEX, 'cf_normal.png')) or '--maps' in sys.argv:
    subprocess.run(['python', '-I', os.path.join(HERE, 'make_cratefetcher_maps.py'), SRC[:-4] + '.png', TEX], check=True)

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
body = src; body.name = body.data.name = 'Body'
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


def drop(o, mask, what):
    n = FD.delete_faces(o, lambda v: mask(v)); print('CUT %s: %d faces' % (what, n)); return n


# ---------------------------------------------------------------- real size
SX, SY, SZ = 4.3, 3.6, 4.3
v = V(body); x, y, z = v[:, 0], v[:, 1], v[:, 2]; ZG = z.min()

# ---------------------------------------------------------------- wheels: centre and radius from the lower half of the tyres
WHEEL = {}
for key, cx0, ylo in (('F', -0.52, 0.17), ('R', 0.48, 0.26)):
    s = (np.abs(x - cx0) < 0.2) & (np.abs(y) > ylo) & (z < -0.53) & (z > ZG - 0.01)
    cx = (x[s].min() + x[s].max()) / 2; r = (x[s].max() - x[s].min()) / 2; cz = z[s].min() + r
    WHEEL[key] = (cx, cz, r, np.abs(y[s]).min(), np.abs(y[s]).max())
    print('WHEEL %s centre %.4f %.4f r %.4f |y| %.3f..%.3f raw (dia %.3f m)' % (key, cx, cz, r, *WHEEL[key][3:], 2 * r * SZ))
for key, (cx, cz, r, y0, y1) in WHEEL.items():     # Meshy's tyres go: all are modelled again round (details)
    drop(body, lambda v, cx=cx, cz=cz, r=r, y0=y0: (np.hypot(v[:, 0] - cx, v[:, 2] - cz) < r + 0.004) & (np.abs(v[:, 1]) > y0 - 0.006), 'tyres ' + key)

# ---------------------------------------------------------------- moving parts (raw units)
PARTS = {}
v = V(body)
for side, sg in (('R', 1.0), ('L', -1.0)):
    PARTS['Fork_' + side] = split(body, face_all(body, (v[:, 0] < -0.44) & (v[:, 2] > 0.13) & (v[:, 2] < 0.235) & (sg * v[:, 1] > 0.12) & (sg * v[:, 1] < 0.30)), 'Fork_' + side)
    v = V(body)
PARTS['Carriage'] = split(body, face_all(body, (v[:, 0] < -0.44) & (v[:, 2] > 0.13) & (v[:, 2] < 0.60) & (np.abs(v[:, 1]) < 0.4)), 'Carriage'); v = V(body)
PARTS['MastInner'] = split(body, face_all(body, (v[:, 0] > -0.447) & (v[:, 0] < -0.322) & (np.abs(v[:, 1]) < 0.136) & (v[:, 2] > -0.11)), 'MastInner'); v = V(body)
PARTS['MastOuter'] = split(body, face_all(body, (v[:, 0] > -0.49) & (v[:, 0] < -0.29) & (np.abs(v[:, 1]) < 0.20) & (v[:, 2] > -0.37) & (v[:, 2] < 0.135)), 'MastOuter')

# boom: along the line from the rear pivot towards the mast head (fitted below), lift cylinder underneath it
LC0, LC1 = np.array([0.185, -0.285]), np.array([-0.01, 0.20])                         # lift cylinder foot, head (x, z)
def segd(v, a, b):
    p = v[:, [0, 2]]; d = b - a; t = ((p - a) @ d) / (d @ d); q = a + np.clip(t, 0, 1)[:, None] * d
    return np.hypot(*(p - q).T), t
v = V(body); dl, tl = segd(v, LC0, LC1)
drop(body, lambda v: (segd(v, LC0, LC1)[0] < 0.036) & (np.abs(v[:, 1]) < 0.06) & (segd(v, LC0, LC1)[1] > -0.06) & (segd(v, LC0, LC1)[1] < 1.08), 'lift cylinder (modelled again)')
BA, BB = np.array([0.43, -0.12]), np.array([-0.25, 0.66])                              # boom: rear pin region .. head
v = V(body); db, tb = segd(v, BA, BB)
bm_ = face_all(body, (db < 0.12) & (np.abs(v[:, 1]) < 0.125) & (v[:, 0] > -0.322) & (v[:, 2] > -0.25) & (tb > -0.05))
boom = split(body, bm_, 'BoomBase')
v = V(boom); db, tb = segd(v, BA, BB)
PARTS['BoomTele'] = split(boom, face_all(boom, tb > 0.62), 'BoomTele'); PARTS['BoomBase'] = boom

# cab door (left side of the machine = raw -y), hinge at the front edge
DOOR = PARTS['Door'] = split(body, face_all(body, (lambda v: (v[:, 1] < -0.352) & (v[:, 0] > -0.125) & (v[:, 0] < 0.115) & (v[:, 2] > -0.41) & (v[:, 2] < -0.075))(V(body))), 'Door')


def close(o):
    only(o); bpy.ops.object.mode_set(mode='EDIT'); bm = bmesh.from_edit_mesh(o.data)
    res = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0); caps = res['faces']
    if caps:
        bmesh.ops.inset_region(bm, faces=caps, thickness=0.00004, depth=-0.0005, use_even_offset=True)
        bmesh.ops.poke(bm, faces=caps, offset=0.0, use_relative_offset=False)
    bmesh.update_edit_mesh(o.data); bpy.ops.object.mode_set(mode='OBJECT'); return len(caps)


# ---------------------------------------------------------------- windows: dark glass faces out, the openings filled with real glass
IMG = np.array(bpy.data.images.load(SRC[:-4] + '.png').pixels[:], np.float32)
RES = int(math.sqrt(len(IMG) // 4)); IMG = IMG.reshape(RES, RES, 4)


def face_lum(o):
    me = o.data; uv = np.empty(len(me.loops) * 2); me.uv_layers[0].data.foreach_get('uv', uv); uv = uv.reshape(-1, 2)
    ls = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_start', ls)
    lt = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_total', lt)
    u = np.add.reduceat(uv, ls) / lt[:, None]
    px = IMG[(u[:, 1] * (RES - 1)).astype(int).clip(0, RES - 1), (u[:, 0] * (RES - 1)).astype(int).clip(0, RES - 1), :3]
    return px @ np.array([0.2126, 0.7152, 0.0722])


def face_centres(o):
    c = np.empty(len(o.data.polygons) * 3); o.data.polygons.foreach_get('center', c)
    n = np.empty(len(o.data.polygons) * 3); o.data.polygons.foreach_get('normal', n); return c.reshape(-1, 3), n.reshape(-1, 3)


def glaze(o, mask, name):
    only(o); bpy.ops.object.mode_set(mode='EDIT'); bm = bmesh.from_edit_mesh(o.data); bm.faces.ensure_lookup_table()
    gone = [bm.faces[int(i)] for i in np.nonzero(mask)[0]]
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    bmesh.ops.delete(bm, geom=[q for q in bm.verts if not q.link_faces], context='VERTS')
    for f in bm.faces: f.select_set(False)
    caps = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)['faces']
    caps = [f for f in caps if f.calc_area() > 2e-4]                                   # only window-sized openings become glass
    tri = bmesh.ops.triangulate(bm, faces=caps)['faces']
    for f in tri: f.select_set(True)
    bm.select_flush_mode(); bmesh.update_edit_mesh(o.data)
    bpy.ops.mesh.separate(type='SELECTED'); bpy.ops.object.mode_set(mode='OBJECT')
    g = [q for q in bpy.context.selected_objects if q is not o][-1]; g.name = g.data.name = name
    print('GLASS %-16s cut %d faces, glass %d faces' % (name, len(gone), len(g.data.polygons))); return g


L = face_lum(DOOR); C, N = face_centres(DOOR)
PARTS['Door_Glass'] = glaze(DOOR, (C[:, 0] > -0.108) & (C[:, 0] < 0.098) & (C[:, 2] > -0.285) & (C[:, 2] < -0.092) & (np.abs(N[:, 1]) > 0.6) & (L < 0.14), 'Door_Glass')
L = face_lum(body); C, N = face_centres(body)
cab = (C[:, 0] > -0.18) & (C[:, 0] < 0.18) & (C[:, 1] > -0.37) & (C[:, 1] < -0.115) & (C[:, 2] > -0.30) & (C[:, 2] < -0.07)
PARTS['Glass_Cab'] = glaze(body, cab & (L < 0.14) & (np.abs(N[:, 2]) < 0.7), 'Glass_Cab')
print('CAPS', {o.name: close(o) for o in [body] + [p for k, p in PARTS.items() if 'Glass' not in k]})

# ---------------------------------------------------------------- crumbs, reduce (flat panels only: round parts are modelled again)
for o in (body, boom, PARTS['MastOuter'], PARTS['MastInner'], PARTS['Carriage']): print('CRUMBS', o.name, FD.drop_crumbs(o, 60))
count = lambda: sum(len(o.data.polygons) for o in sc.objects if o.type == 'MESH')
print('FACES before', count())


def reduce(o, budget):
    n = len(o.data.polygons)
    if n <= budget: return
    only(o); md = o.modifiers.new('d', 'DECIMATE'); md.ratio = budget / n; md.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier='d')


for k, b in (('Body', 120000), ('BoomBase', 26000), ('BoomTele', 14000), ('MastOuter', 14000), ('MastInner', 16000), ('Carriage', 14000),
             ('Fork_L', 2500), ('Fork_R', 2500), ('Door', 7000)):
    reduce(body if k == 'Body' else PARTS[k], b)
print('FACES after', count())

# ---------------------------------------------------------------- materials: cleaned Meshy colour + PBR per kind
def img(name, colour):
    im = bpy.data.images.load(os.path.join(TEX, name), check_existing=True)
    im.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'; return im


def texmat(name, mr):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    b = nt.nodes.new('ShaderNodeTexImage'); b.image = img('cf_base.png', True); nt.links.new(b.outputs['Color'], p.inputs['Base Color'])
    r = nt.nodes.new('ShaderNodeTexImage'); r.image = img('cf_mr_%s.png' % mr, False)
    sp = nt.nodes.new('ShaderNodeSeparateRGB'); nt.links.new(r.outputs['Color'], sp.inputs['Image'])
    nt.links.new(sp.outputs['G'], p.inputs['Roughness']); nt.links.new(sp.outputs['B'], p.inputs['Metallic'])
    n = nt.nodes.new('ShaderNodeTexImage'); n.image = img('cf_normal.png', False)
    nm = nt.nodes.new('ShaderNodeNormalMap'); nt.links.new(n.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], p.inputs['Normal'])
    return m


MB, MS = texmat('M_CrateFetcher_Body', 'body'), texmat('M_CrateFetcher_Steel', 'steel')
MG = FD.material('M_CrateFetcher_Glass', (0.10, 0.12, 0.13), 0.03, 0.0)
MG.blend_method = 'BLEND'; MG.node_tree.nodes['Principled BSDF'].inputs['Alpha'].default_value = 0.28
for o in [body] + list(PARTS.values()):
    m = MG if 'Glass' in o.name else MS if o.name.startswith(('Mast', 'Boom', 'Carriage', 'Fork')) else MB
    o.data.materials.clear(); o.data.materials.append(m)

# ---------------------------------------------------------------- to game metres
XM = (WHEEL['F'][0] + WHEEL['R'][0]) / 2
T = Matrix(((-SX, 0, 0, XM * SX), (0, -SY, 0, 0), (0, 0, SZ, -ZG * SZ), (0, 0, 0, 1)))
G = lambda p: np.array(T @ Vector(p))
for o in [body] + list(PARTS.values()): o.data.transform(T)
for o in [body] + list(PARTS.values()):
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32)
    o.data.polygons.foreach_set('use_smooth', [True] * len(o.data.polygons)); o.data.update()
for o in (body, DOOR, PARTS['Carriage'], PARTS['MastOuter']):
    print('FLAT', o.name, 'vertices snapped', FD.planar_snap(o, cell=0.4, tol=0.012))

GEO = dict(SX=SX, SY=SY, SZ=SZ, G=G, WHEEL=WHEEL, LC0=LC0, LC1=LC1, BA=BA, BB=BB, XM=XM, ZG=ZG)
import cratefetcher_details as CD
INFO, IKS = CD.build(sc, body, PARTS, GEO)

# ---------------------------------------------------------------- save, check, export
col = bpy.data.collections.new('CrateFetcher'); sc.collection.children.link(col)
ALL = [o for o in sc.objects if o.type == 'MESH']
for o in ALL + IKS:
    for c in list(o.users_collection): c.objects.unlink(o)
    col.objects.link(o)
for o in ALL:
    assert len(o.data.materials) and all(o.data.materials), 'empty material slot on ' + o.name
    o.data.name = o.name
print('\n'.join('INFO ' + s for s in INFO))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'CrateFetcher.blend'))
for o in sc.objects: o.select_set(o in ALL or o in IKS)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'cratefetcher.glb'), export_format='GLB', use_selection=True, export_apply=True,
                          export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO')
v = np.concatenate([np.array([o.matrix_world @ q.co for q in o.data.vertices]) for o in ALL])
print('SIZE %.3f x %.3f x %.3f m, faces %d, parts %d' % (*(v.max(0) - v.min(0)), count(), len(ALL)))
print('DONE')
