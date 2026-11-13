# The crew-neck t-shirt (owner, 2026-11-13; reference Art/Clothes/_references/Tee: a plain cotton crew-neck from front,
# side and back): tailored on the player's body with the suit's tools (Art/Clothes/CEO_SuitClassic/make_ceo_suit.py's
# helpers, so every fix learnt there carries over): a relaxed body falling straight from the chest to just below the
# belt, short sleeves square to the arm half-way to the elbow, a 2 cm ribbed crew collar round the neck; single-layer
# cloth (two-sided fabric). The front print is its own material slot (Print_<name>) laid flat over the chest with its
# own UVs, so any print can be swapped in later without touching the tee.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Clothes/Tee/make_tee.py
import bpy, bmesh, math
from mathutils import Vector
from pathlib import Path

HERE = Path(__file__).resolve().parent
SUIT = HERE.parent / 'CEO_SuitClassic' / 'make_ceo_suit.py'
_src = SUIT.read_text(encoding='utf-8')
_g = {'__file__': str(HERE / 'make_tee.py'), '__name__': 'tee'}
exec(_src[:_src.index('# ---------------------------------------------------------------- the shirt')], _g)   # body, fitting tools
globals().update({k: v for k, v in _g.items() if not k.startswith('__')})
NAME = 'Tee'
log = lambda *a: print('TEE', *a)

# ---------------------------------------------------------------- the body of the tee
HEM = 0.90                     # just below the belt (the reference hangs over the waistband)
def tee_keep(i):
    w = W[i]
    legs_ok = CO[i].z > 0.97 or wsum(w, ('thigh', 'calf', 'foot', 'toes')) < 0.3
    return CO[i].z > HEM - 0.05 and wsum(w, ('hand', 'lowerarm')) < 0.2 and legs_ok and wsum(w, ('head',)) < 0.3
tee = cut('Tee', tee_keep)
trim(tee, Vector((0, 0, HEM)), Vector((0, 0, 1)))
# the sleeves: cut square to the upper arm, 45 % of the way from the shoulder to the elbow
for s_ in ('l', 'r'):
    sh = bone('upperarm_' + s_); el = bone('lowerarm_' + s_); d = (el - sh).normalized()
    c = sh + (el - sh) * 0.45
    bm = bmesh.new(); bm.from_mesh(tee.data)
    geom = [f for f in bm.faces if (f.calc_center_median() - sh).dot(d) > 0.02 and (f.calc_center_median() - sh).length < 0.40]
    bmesh.ops.bisect_plane(bm, geom=list({v for f in geom for v in f.verts}) + list({e for f in geom for e in f.edges}) + geom, plane_co=c, plane_no=-d, clear_outer=False, clear_inner=False)
    side_ = 1 if sh.x > 0 else -1   # only the arm (out past the armpit), never the torso below it
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if (f.calc_center_median() - c).dot(d) > 0 and f.calc_center_median().x * side_ > abs(sh.x) * 0.95 and (f.calc_center_median() - sh).length < 0.40], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(tee.data); bm.free()
# relaxed: the suit shirt's proven steps (smooth, offset, smooth, out of the skin), with more ease
smooth(tee, 14, 0.45)
offset(tee, lambda c: 0.014)
smooth(tee, 8, 0.35)
log('tee off the skin at', keep_out(tee, BODY_BVH, 0.010), 'points')
clean_edges(tee)
# the crew collar: a 2 cm ribbed band round the base of the neck, from the tee's own neckline
def highest(loops): return max(loops, key=lambda l: sum(v.co.z for v in l) / len(l))
def rib(frac):
    def move(c):
        d = Vector((c.x - NECK.x, c.y - NECK.y, 0.0)); a_ = math.atan2(d.x, -d.y)
        z = c.z + 0.02 * frac
        r = lerp(d.length, neck_surface(a_, z) + 0.008, frac)
        return Vector((NECK.x + math.sin(a_) * r, NECK.y - math.cos(a_) * r, z))
    return move
extrude_loop(tee, highest, rib(0.5)); extrude_loop(tee, highest, rib(1.0))
subdivide(tee, 1)
log('tee out of the skin after smoothing at', keep_out(tee, BODY_BVH, 0.009), 'points')
dress(tee, material('Fabric_cotton_jersey', (0.93, 0.93, 0.91), 0.8))

# ---------------------------------------------------------------- the print: its own slot, its own UVs, over the chest
PW, PH, PZ = 0.30, 0.30, 1.30          # 30 x 30 cm, centred on the chest (a typical front print)
pm = material('Print_northern_skies', (1, 1, 1), 0.8)
tee.data.materials.append(pm); pi = len(tee.data.materials) - 1
bm = bmesh.new(); bm.from_mesh(tee.data)
uvl = bm.loops.layers.uv.active
for f in bm.faces:
    c = f.calc_center_median()
    if c.y < -0.03 and abs(c.x) < PW / 2 + 0.02 and abs(c.z - PZ) < PH / 2 + 0.02:
        f.material_index = pi
        for lp in f.loops:
            lp[uvl].uv = ((lp.vert.co.x + PW / 2) / PW, (lp.vert.co.z - (PZ - PH / 2)) / PH)   # flat front projection
bm.to_mesh(tee.data); bm.free()
log('print faces', sum(1 for p in tee.data.polygons if p.material_index == pi))

# ---------------------------------------------------------------- export, a look
log('Tee', len(tee.data.vertices), 'verts', tuple(round(d, 3) for d in tee.dimensions))
bpy.ops.object.select_all(action='DESELECT'); tee.select_set(True); bpy.context.view_layer.objects.active = tee
bpy.ops.export_scene.fbx(filepath=str(HERE / (NAME + '.fbx')), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', add_leaf_bones=False)
import json
json.dump({m.name: list(m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value)[:3] for m in tee.data.materials}, open(HERE / (NAME + '.json'), 'w'), indent=1)
# preview: the print on in Blender too
img = bpy.data.images.load(str(HERE / 'print_northern_skies.png'))
nt = pm.node_tree; tx = nt.nodes.new('ShaderNodeTexImage'); tx.image = img; tx.extension = 'CLIP'
mixn = nt.nodes.new('ShaderNodeMixRGB'); mixn.inputs[1].default_value = (0.93, 0.93, 0.91, 1)
nt.links.new(tx.outputs['Alpha'], mixn.inputs[0]); nt.links.new(tx.outputs['Color'], mixn.inputs[2])
nt.links.new(mixn.outputs[0], nt.nodes['Principled BSDF'].inputs['Base Color'])
src.data.materials.clear(); src.data.materials.append(material('Skin', (0.55, 0.38, 0.30), 0.5))
for o in list(sc.objects):
    if o.type == 'MESH' and o not in (tee, src): bpy.data.objects.remove(o, do_unlink=True)
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50; tgt = Vector((0, -0.05, 1.15))
for nm, lv, e in (('Key', (-2, -3, 3), 900), ('Fill', (2.5, -2, 1.5), 350), ('Rim', (0, 3, 2.5), 500)):
    ld = bpy.data.lights.new(nm, 'AREA'); ld.energy = e; ld.size = 2
    lo = bpy.data.objects.new(nm, ld); lo.location = lv; lo.rotation_euler = (Vector((0, 0, 1)) - Vector(lv)).to_track_quat('-Z', 'Y').to_euler(); sc.collection.objects.link(lo)
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.4
sc.render.engine = 'BLENDER_EEVEE'; sc.render.resolution_x = 900; sc.render.resolution_y = 1000
for nm, off in (('preview', Vector((-0.6, -2.2, 0.15))), ('preview_back', Vector((0.8, 2.0, 0.15)))):
    cam.location = tgt + off; cam.rotation_euler = (tgt - cam.location).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = str(HERE / (nm + '.png')); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / (NAME + '.blend')))
log('DONE')
