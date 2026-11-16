# The jetway's yellow light bar over the cab's doorway (owner, 2026-11-16, from a real jetway video): a slim aluminium
# housing 1.2 m long with a yellow LED lens along its front, on two small brackets that hold it off the wall. Its
# length along +X, the lens facing +Y (imported: Unreal -Y), its origin at the back of the brackets (the wall), the
# middle of its length. Lens in its own slot (the game lights it). Nothing touches face to face.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/JetwayBeacon/make_jetway_lightbar.py
import bpy, os

HERE = os.path.dirname(os.path.abspath(__file__))
for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
MATS = {}
def mat(n):
    if n not in MATS: MATS[n] = bpy.data.materials.new(n)
    return MATS[n]
parts = []
def box(cx, cy, cz, sx, sy, sz, m):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz)); o = bpy.context.object; o.scale = (sx, sy, sz)
    o.data.materials.append(mat(m)); parts.append(o)

L, H, D = 1.20, 0.07, 0.06          # the bar: length, height, depth
OFF = 0.05                           # the brackets' standoff from the wall
for x in (-0.45, 0.45):
    box(x, OFF / 2, 0, 0.04, OFF + 0.002, 0.05, 'JwBcnDark')          # brackets (their front ends inside the housing)
box(0, OFF + D / 2, 0, L, D, H, 'JwBcnSteel')                            # the housing
box(0, OFF + D + 0.004, 0, L - 0.04, 0.012, H - 0.025, 'JwBcnLens')      # the lens strip (its back inside the housing)
for x in (-L / 2 - 0.006, L / 2 + 0.006):
    box(x, OFF + D / 2, 0, 0.016, D + 0.004, H + 0.004, 'JwBcnDark')     # end caps (overlapping the housing's ends)

for x in bpy.context.selected_objects: x.select_set(False)
for o in parts: o.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
ob = bpy.context.object; ob.name = 'SM_JwLightBar'
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
assert all(s.material for s in ob.material_slots), 'empty material slot'
print('BAR', ob.name, len(ob.data.polygons), 'faces', [m.name for m in ob.data.materials])
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'JetwayLightBar.blend'))
bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, 'SM_JwLightBar.fbx'), use_selection=True, object_types={'MESH'},
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'jetwaylightbar.glb'), use_selection=True, export_format='GLB')
print('BAR saved DONE')
