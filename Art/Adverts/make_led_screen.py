# The terminal's LED advert screen (to-do 52): a slim wall-hung LED cabinet, 16:9, 1.6 x 0.9 m (the game scales it to
# each wall: 3 to 8 m wide): a dark aluminium frame 4 cm round, 9 cm deep, its back against the wall at the origin, its
# face looking along +X; the LED face its own slot (AdScreen) with UVs 0..1 across it (u to the viewer's right, v down).
# Nothing face to face (the face sits 2 mm proud of the frame's front).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Adverts/make_led_screen.py
import bpy, bmesh, os
HERE = os.path.dirname(os.path.abspath(__file__))
for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
def mat(n): return bpy.data.materials.get(n) or bpy.data.materials.new(n)
W, H, D, B = 1.6, 0.9, 0.09, 0.04
parts = []
def box(cx, cy, cz, sx, sy, sz, m):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz)); o = bpy.context.object; o.scale = (sx, sy, sz); o.data.materials.append(mat(m)); parts.append(o)
# the cabinet (back box) and its frame lip round the face
box(D / 2 - 0.01, 0, 0, D - 0.02, W, H, 'AdFrame')
box(D - 0.005, 0, H / 2 - B / 2, 0.01, W, B, 'AdFrame'); box(D - 0.005, 0, -H / 2 + B / 2, 0.01, W, B, 'AdFrame')
box(D - 0.005, W / 2 - B / 2, 0, 0.01, B, H - 2 * B, 'AdFrame'); box(D - 0.005, -W / 2 + B / 2, 0, 0.01, B, H - 2 * B, 'AdFrame')
# the LED face: one quad inside the frame, 2 mm in front of the cabinet
me = bpy.data.meshes.new('face'); bm = bmesh.new()
x = D - 0.008; y0, y1, z0, z1 = -W / 2 + B, W / 2 - B, -H / 2 + B, H / 2 - B
vs = [bm.verts.new(v) for v in ((x, y0, z1), (x, y1, z1), (x, y1, z0), (x, y0, z0))]
f = bm.faces.new(vs); uv = bm.loops.layers.uv.new('UVMap')   # (the cabinet's own layer's name: joined into one, not a second layer)
for l, (u, v) in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))): l[uv].uv = (u, 1 - v)   # (Blender's v up: the game's v down)
bm.normal_update()
if f.normal.x < 0: f.normal_flip()
bm.to_mesh(me); bm.free()
fo = bpy.data.objects.new('face', me); bpy.context.collection.objects.link(fo); me.materials.append(mat('AdScreen')); parts.append(fo)
for o in bpy.context.selected_objects: o.select_set(False)
for o in parts: o.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join(); ob = bpy.context.object; ob.name = 'SM_LedScreen'
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
assert all(s.material for s in ob.material_slots)
print('LED', len(ob.data.polygons), [m.name for m in ob.data.materials])
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'SM_LedScreen.blend'))
bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, 'SM_LedScreen.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'sm_ledscreen.glb'), use_selection=True, export_format='GLB')
