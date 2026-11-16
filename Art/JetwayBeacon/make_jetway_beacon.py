# The jetways' warning beacons (owner, 2026-11-16): a small amber flashing light on its own pole - two standing on the
# cab's roof at its front corners, one hanging under the cab on the same stand turned over - flashing while the jetway
# moves (real bridges: "flashing amber beacons ... mounted under the cab and near the wheels ... during bridge
# operations", Oshkosh AeroTech gate equipment). One mesh, its origin at the base plate's underside, the pole up +Z:
#   a bolted square base plate, a steel pole, a dark housing and the amber dome lens (its own slot: the game lights it).
# Nothing touches face to face (the hidden ends sit inside the part they meet), so no surfaces fight.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/JetwayBeacon/make_jetway_beacon.py
import bpy, os, math

HERE = os.path.dirname(os.path.abspath(__file__))
for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
MATS = {}
def mat(n):
    if n not in MATS: MATS[n] = bpy.data.materials.new(n)
    return MATS[n]
parts = []
def put(o, m):
    o.data.materials.append(mat(m)); parts.append(o); return o

PLATE, PT, POLE_R, POLE_TOP = 0.16, 0.012, 0.022, 0.40
HOUSE_R, HOUSE_H, LENS_R, LENS_H = 0.065, 0.03, 0.055, 0.07
# the base plate and its four bolts (their feet inside the plate)
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, PT / 2)); o = bpy.context.object; o.scale = (PLATE, PLATE, PT); put(o, 'JwBcnSteel')
for sx in (-1, 1):
    for sy in (-1, 1):
        bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.009, depth=0.012, location=(sx * 0.058, sy * 0.058, PT + 0.0)); put(bpy.context.object, 'JwBcnDark')
# the pole (its foot inside the plate, its top inside the housing)
bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=POLE_R, depth=POLE_TOP + 0.005 - 0.006, location=(0, 0, (0.006 + POLE_TOP + 0.005) / 2)); put(bpy.context.object, 'JwBcnSteel')
# a small gusset ring at the foot (its foot inside the plate)
bpy.ops.mesh.primitive_cone_add(vertices=20, radius1=0.04, radius2=POLE_R + 0.002, depth=0.05, location=(0, 0, 0.006 + 0.025)); put(bpy.context.object, 'JwBcnSteel')
# the housing
bpy.ops.mesh.primitive_cylinder_add(vertices=28, radius=HOUSE_R, depth=HOUSE_H, location=(0, 0, POLE_TOP + HOUSE_H / 2)); put(bpy.context.object, 'JwBcnDark')
# the lens: an open tube standing on the housing, a dome on top (its lower half inside the tube)
z0 = POLE_TOP + HOUSE_H
bpy.ops.mesh.primitive_cylinder_add(vertices=28, radius=LENS_R, depth=LENS_H, end_fill_type='NOTHING', location=(0, 0, z0 + LENS_H / 2)); put(bpy.context.object, 'JwBcnLens')
bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=14, radius=LENS_R * 0.985, location=(0, 0, z0 + LENS_H)); put(bpy.context.object, 'JwBcnLens')

for x in bpy.context.selected_objects: x.select_set(False)
for o in parts: o.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
ob = bpy.context.object; ob.name = 'SM_JwBeacon'
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
for p in ob.data.polygons: p.use_smooth = True
assert all(s.material for s in ob.material_slots), 'empty material slot'
print('BCN', ob.name, len(ob.data.polygons), 'faces', [m.name for m in ob.data.materials])
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'JetwayBeacon.blend'))
bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, 'SM_JwBeacon.fbx'), use_selection=True, object_types={'MESH'},
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'jetwaybeacon.glb'), use_selection=True, export_format='GLB')
print('BCN saved DONE')
