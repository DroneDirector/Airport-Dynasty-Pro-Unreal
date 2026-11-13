# The clothes pipeline's test garment (2026-11-13): a fitted long-sleeved shirt made from the player's own body
# (Art/Clothes/_bodies/player_body.fbx, exported from /Game/People/Built/MH_900_player): the torso and arms taken by
# their bone weights, pushed out to sit on the skin, given thickness, exported as a static mesh in the body's own space
# for Unreal's cloth import (Tools/make_outfit.py). Proves the route Blender -> cloth asset -> MetaHuman outfit.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Clothes/make_test_shirt.py
import bpy, bmesh
from pathlib import Path
HERE = Path(__file__).resolve().parent
OUT = HERE / 'TestShirt'; OUT.mkdir(exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(HERE / '_bodies' / 'player_body.fbx'))
body = bpy.data.objects['SKM_MH_900_player_BodyMesh_LOD0']
M = body.matrix_world.copy()

SHIRT = ('spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'clavicle_l', 'clavicle_r', 'upperarm', 'lowerarm', 'pelvis', 'neck_01')
STOP = ('hand_', 'thigh', 'neck_02', 'head', 'calf', 'foot')
gi = {g.index: g.name for g in body.vertex_groups}
def score(v):
    s = sum(e.weight for e in v.groups if any(gi[e.group].startswith(k) for k in SHIRT))
    t = sum(e.weight for e in v.groups if any(gi[e.group].startswith(k) for k in STOP))
    return s, t
keep = set()
for v in body.data.vertices:
    s, t = score(v)
    z = (M @ v.co).z
    if s > 0.5 and t < 0.25 and z > 0.86:     # down to just below the belt line
        keep.add(v.index)

bm = bmesh.new(); bm.from_mesh(body.data)
bm.verts.ensure_lookup_table()
drop = [f for f in bm.faces if not all(v.index in keep for v in f.verts)]
bmesh.ops.delete(bm, geom=drop, context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.transform(M)
bm.normal_update()
for v in bm.verts: v.co += v.normal * 0.006          # on the skin, not in it
me = bpy.data.meshes.new('TestShirt'); bm.to_mesh(me); bm.free()
shirt = bpy.data.objects.new('TestShirt', me); bpy.context.scene.collection.objects.link(shirt)
mat = bpy.data.materials.new('Shirt'); mat.use_nodes = True
mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.85, 0.87, 0.92, 1)
me.materials.append(mat)
bpy.context.view_layer.objects.active = shirt
sub = shirt.modifiers.new('Smooth', 'CORRECTIVE_SMOOTH'); sub.iterations = 4; sub.use_only_smooth = True
bpy.ops.object.modifier_apply(modifier='Smooth')
sol = shirt.modifiers.new('Thickness', 'SOLIDIFY'); sol.thickness = 0.002; sol.offset = 1.0
bpy.ops.object.modifier_apply(modifier='Thickness')
bpy.ops.object.shade_smooth()
print('SHIRT verts', len(me.vertices), 'faces', len(me.polygons), 'dims', tuple(round(d, 3) for d in shirt.dimensions))

for o in list(bpy.context.scene.objects):
    if o != shirt: bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.select_all(action='DESELECT'); shirt.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(OUT / 'TestShirt.fbx'), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', add_leaf_bones=False)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'TestShirt.blend'))
print('SHIRT DONE')
