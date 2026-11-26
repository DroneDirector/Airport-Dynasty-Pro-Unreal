# The X-Way mover: an electric multi-directional ULD platform transporter (HUBTEX X-Way Mover / ULD Pallet Mover class)
# with a lifting roller deck for one 10 ft ULD and a stand-on operator platform at the rear. From the owner's Meshy model
# (F:/ADP-Meshy/2026-11-26/XWayMover, 1.08 M faces). Blender 3.3, headless:
#   blender -b --python Art/XWayMover/make_xwaymover.py
#   python Tools/import_model.py Art/XWayMover/make_xwaymover.py Art/XWayMover/xwaymover.glb XWayMover
# Recipe of the crate fetcher (Art/CrateFetcher/make_cratefetcher.py): Meshy's chassis shell kept (cleaned colour, flat
# panels decimated and snapped flat, cuts closed); everything Meshy melted is cut away and modelled again in
# xwaymover_details.py: the deck plate with its 12 rollers, the 4 wheels, the stand platform, handrail, control column and
# console, battery hatches, lamps. No brand, badge or text survives (make_xwaymover_maps.py).
# Real size, from the maker's published data (hubtex.com, air cargo handling, read 2026-11-26): X-Way Mover 70 / 140:
# 7,000 / 14,000 kg, 360 deg HX steering, "platform width of 2,500 mm" with an electro-hydraulic roller deck; ULD Pallet
# Mover: up to 10 ft containers (a 10 ft pallet is 3.18 x 2.44 m). Meshy's shell is short and narrow for that, so:
# width by 3.6 (2.58 m over the body, deck 2.50 m), length by 2.25 (deck 3.45 m for a 10 ft ULD, 4.28 m overall),
# height by 2.2 (deck top 0.92 m, stand floor 0.46 m, 0.71 m solid tyres on 15 in rims, an industrial 28x9-15 size).
# Axes as the pushback tug: x forward (the deck end), y left, z up, metres; origin on the ground halfway between the axles.
# Raw Meshy (after the default OBJ import): deck end -x, stand end +x, z up.
import bpy, bmesh, math, os, subprocess, sys
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
SRCDIR = 'F:/ADP-Meshy/2026-11-26/XWayMover/Meshy_AI_Creators_a_HUBTEX_X_W_1126092102_texture_obj/'
SRC = SRCDIR + 'Meshy_AI_Creators_a_HUBTEX_X_W_1126092102_texture.obj'
WORK = 'F:/ADP-Export/xwaymover_work'
CACHE = WORK + '/raw.blend'
TEX = os.path.join(HERE, 'tex')
os.makedirs(WORK, exist_ok=True); os.makedirs(TEX, exist_ok=True)
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, '..', 'CargoForklift')); sys.path.insert(0, os.path.join(HERE, '..', 'Lorry'))
import forklift_details as FD

if not os.path.exists(os.path.join(TEX, 'xw_normal.png')) or '--maps' in sys.argv:
    subprocess.run(['python', '-I', os.path.join(HERE, 'make_xwaymover_maps.py'), SRC[:-4] + '.png', TEX], check=True)

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


def drop(o, mask, what, any_=False):
    n = FD.delete_faces(o, lambda v: mask(v), any_); print('CUT %s: %d faces' % (what, n)); return n


# ---------------------------------------------------------------- real size
SX, SY, SZ = 2.25, 3.6, 2.2
v = V(body); x, y, z = v[:, 0], v[:, 1], v[:, 2]; ZG = z.min()
DECK_CUT, STAND_X = -0.035, 0.595          # raw: Meshy's deck layer (plate, rollers, brackets) above z -0.035; the stand end beyond x 0.595

# ---------------------------------------------------------------- wheels: centre and radius from the tyre's lower half
WHEEL = {}
for key, cx0 in (('F', -0.59), ('R', 0.36)):
    s = (np.abs(x - cx0) < 0.17) & (np.abs(y) > 0.30) & (z > ZG + 0.074) & (z < ZG + 0.082)   # a chord 7.8 cm (raw) above the ground
    cx = (x[s].min() + x[s].max()) / 2; c = (x[s].max() - x[s].min()) / 2; h = 0.078
    r = (c * c + h * h) / (2 * h); cz = ZG + r
    yy = np.abs(y[(np.abs(x - cx) < 0.06) & (z < ZG + 0.02)])
    WHEEL[key] = (cx, cz, r, yy.min(), yy.max())
    print('WHEEL %s centre %.4f %.4f r %.4f |y| %.3f..%.3f raw' % (key, cx, cz, r, yy.min(), yy.max()))
for key, (cx, cz, r, y0, y1) in WHEEL.items():     # Meshy's tyres go: all four are modelled again, round
    drop(body, lambda v, cx=cx, cz=cz, r=r, y0=y0: (np.hypot(v[:, 0] - cx, v[:, 2] - cz) < r + 0.006) & (np.abs(v[:, 1]) > y0 - 0.006), 'tyres ' + key)

# ---------------------------------------------------------------- the melted superstructure goes (modelled again in the details)
drop(body, lambda v: (v[:, 2] > DECK_CUT) & (v[:, 0] < STAND_X + 0.01), 'deck plate, rollers, brackets')
drop(body, lambda v: (v[:, 0] > STAND_X) & (v[:, 2] > -0.215), 'stand floor, handrail, control column, console', True)
for o in (body,): print('CRUMBS', o.name, FD.drop_crumbs(o, 80))

# battery hatch (light grey panel between the wheels, each side) and lamps: found by their colour in Meshy's picture
IMG = np.array(bpy.data.images.load(SRC[:-4] + '.png').pixels[:], np.float32)
RES = int(math.sqrt(len(IMG) // 4)); IMG = IMG.reshape(RES, RES, 4)


def face_rgb(o):
    me = o.data; uv = np.empty(len(me.loops) * 2); me.uv_layers[0].data.foreach_get('uv', uv); uv = uv.reshape(-1, 2)
    ls = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_start', ls)
    lt = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('loop_total', lt)
    u = np.add.reduceat(uv, ls) / lt[:, None]
    return IMG[(u[:, 1] * (RES - 1)).astype(int).clip(0, RES - 1), (u[:, 0] * (RES - 1)).astype(int).clip(0, RES - 1), :3]


def face_centres(o):
    c = np.empty(len(o.data.polygons) * 3); o.data.polygons.foreach_get('center', c)
    n = np.empty(len(o.data.polygons) * 3); o.data.polygons.foreach_get('normal', n); return c.reshape(-1, 3), n.reshape(-1, 3)


rgb = face_rgb(body); C, N = face_centres(body); lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
sat = (rgb.max(1) - rgb.min(1)) / np.maximum(rgb.max(1), 1e-3)
HATCH = {}
for side, sg in (('L', -1), ('R', 1)):        # raw -y = the machine's left (y is flipped into the game)
    m = (sg * C[:, 1] > 0.33) & (sg * N[:, 1] > 0.8) & (lum > 0.30) & (sat < 0.2) & (C[:, 0] > WHEEL['F'][0] + 0.14) & (C[:, 0] < WHEEL['R'][0] - 0.14) & (C[:, 2] < DECK_CUT)
    if m.sum() > 50:
        q = C[m]; lo, hi = np.percentile(q, 3, 0), np.percentile(q, 97, 0)
        HATCH[side] = (lo[0], hi[0], lo[2], hi[2], (sg * q[:, 1]).max())
        print('HATCH %s raw x %.3f..%.3f z %.3f..%.3f faces %d' % (side, lo[0], hi[0], lo[2], hi[2], m.sum()))
LAMPS = []
for end, sx in (('front', -1), ('rear', 1)):
    m = (sx * C[:, 0] > 0.90) & (sx * N[:, 0] > 0.6) & (rgb[:, 0] > 0.35) & (rgb[:, 0] > 2.0 * rgb[:, 1])
    for sg in (-1, 1):
        q = C[m & (sg * C[:, 1] > 0.05)]
        if len(q) > 8:
            LAMPS.append((end, sx, np.median(q, 0), np.percentile(q, 5, 0), np.percentile(q, 95, 0)))
            print('LAMP %s side %d raw centre %s' % (end, sg, np.median(q, 0).round(3)))


def close(o):
    only(o); bpy.ops.object.mode_set(mode='EDIT'); bm = bmesh.from_edit_mesh(o.data)
    res = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0); caps = res['faces']
    if caps:
        bmesh.ops.inset_region(bm, faces=caps, thickness=0.00004, depth=-0.0005, use_even_offset=True)
        bmesh.ops.poke(bm, faces=caps, offset=0.0, use_relative_offset=False)
    bmesh.update_edit_mesh(o.data); bpy.ops.object.mode_set(mode='OBJECT'); return len(caps)


count = lambda: sum(len(o.data.polygons) for o in sc.objects if o.type == 'MESH')
print('FACES before', count())
only(body); md = body.modifiers.new('d', 'DECIMATE'); md.ratio = min(1.0, 150000 / len(body.data.polygons)); md.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier='d')
print('FACES after', count())

# ---------------------------------------------------------------- material: cleaned Meshy colour + PBR
def img(name, colour):
    im = bpy.data.images.load(os.path.join(TEX, name), check_existing=True)
    im.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'; return im


def texmat(name, mr):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    b = nt.nodes.new('ShaderNodeTexImage'); b.image = img('xw_base.png', True); nt.links.new(b.outputs['Color'], p.inputs['Base Color'])
    r = nt.nodes.new('ShaderNodeTexImage'); r.image = img('xw_mr_%s.png' % mr, False)
    sp = nt.nodes.new('ShaderNodeSeparateRGB'); nt.links.new(r.outputs['Color'], sp.inputs['Image'])
    nt.links.new(sp.outputs['G'], p.inputs['Roughness']); nt.links.new(sp.outputs['B'], p.inputs['Metallic'])
    n = nt.nodes.new('ShaderNodeTexImage'); n.image = img('xw_normal.png', False)
    nm = nt.nodes.new('ShaderNodeNormalMap'); nt.links.new(n.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], p.inputs['Normal'])
    return m


body.data.materials.clear(); body.data.materials.append(texmat('M_XWayMover_Body', 'body'))

# ---------------------------------------------------------------- to game metres
XM = (WHEEL['F'][0] + WHEEL['R'][0]) / 2
T = Matrix(((-SX, 0, 0, XM * SX), (0, -SY, 0, 0), (0, 0, SZ, -ZG * SZ), (0, 0, 0, 1)))
G = lambda p: np.array(T @ Vector(p))
body.data.transform(T)
body.data.use_auto_smooth = True; body.data.auto_smooth_angle = math.radians(32)
body.data.polygons.foreach_set('use_smooth', [True] * len(body.data.polygons)); body.data.update()
print('FLAT', body.name, 'vertices snapped', FD.planar_snap(body, cell=0.4, tol=0.012))

GEO = dict(SX=SX, SY=SY, SZ=SZ, G=G, WHEEL=WHEEL, XM=XM, ZG=ZG, DECK_CUT=DECK_CUT, STAND_X=STAND_X, HATCH=HATCH, LAMPS=LAMPS, close=close)
import xwaymover_details as XD
INFO, IKS = XD.build(sc, body, GEO)
print('CAPS body', close(body))

# ---------------------------------------------------------------- save, check, export
col = bpy.data.collections.new('XWayMover'); sc.collection.children.link(col)
ALL = [o for o in sc.objects if o.type == 'MESH']
for o in ALL + IKS:
    for c in list(o.users_collection): c.objects.unlink(o)
    col.objects.link(o)
for o in ALL:
    assert len(o.data.materials) and all(o.data.materials), 'empty material slot on ' + o.name
    o.data.name = o.name
print('\n'.join('INFO ' + s for s in INFO))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'XWayMover.blend'))
for o in sc.objects: o.select_set(o in ALL or o in IKS)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'xwaymover.glb'), export_format='GLB', use_selection=True, export_apply=True,
                          export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO')
v = np.concatenate([np.array([o.matrix_world @ q.co for q in o.data.vertices]) for o in ALL])
print('SIZE %.3f x %.3f x %.3f m (x %.3f..%.3f), faces %d, parts %d' % (*(v.max(0) - v.min(0)), v[:, 0].min(), v[:, 0].max(), count(), len(ALL)))
print('DONE')
