# The jetway's other lamps (owner, 2026-11-16: "follow SAE standards and the 3 other mentioned categories"; research:
# Syracuse airport PBB spec 34 77 13 - two LED floodlights under the cab lighting the apron ~10 m ahead, one under the
# tunnel lighting the wheel carriage; ShinMaywa patent EP4501798A1 / SAE AS7995 - a docking lamp the cabin crew see
# through the door window):
#   SM_JwFlood - an LED floodlight: a flat finned housing on a U-bracket, its white lens facing -Z (down), origin at the
#                bracket's mounting plate (the plate on +Z, screwed to the underside it hangs from)
#   SM_JwLamp  - a small round signal lamp: a dark square back plate, a short hood, its lens facing +Y (imported: -Y),
#                origin at the back of the plate (the wall)
# Lenses in their own slot (JwBcnLens: the game lights them, colour per copy of the mesh). Nothing face to face.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/JetwayBeacon/make_jetway_lamps.py
import bpy, os, math

HERE = os.path.dirname(os.path.abspath(__file__))
MATS = {}
def mat(n):
    if n not in MATS: MATS[n] = bpy.data.materials.get(n) or bpy.data.materials.new(n)
    return MATS[n]

def build(name, make):
    for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
    parts = []
    def box(cx, cy, cz, sx, sy, sz, m):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz)); o = bpy.context.object; o.scale = (sx, sy, sz); o.data.materials.append(mat(m)); parts.append(o)
    def cyl(cx, cy, cz, r, d, m, rot=(0, 0, 0), v=24, fill='NGON'):
        bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=r, depth=d, location=(cx, cy, cz), rotation=rot, end_fill_type=fill); o = bpy.context.object; o.data.materials.append(mat(m)); parts.append(o)
    make(box, cyl)
    for x in bpy.context.selected_objects: x.select_set(False)
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    ob = bpy.context.object; ob.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    assert all(s.material for s in ob.material_slots), 'empty material slot'
    print('LAMP', name, len(ob.data.polygons), 'faces', [m.name for m in ob.data.materials])
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, name + '.blend'))
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, name.lower() + '.glb'), use_selection=True, export_format='GLB')

def flood(box, cyl):
    # mounting plate on top (+Z at 0), U-bracket arms down, the housing between them, fins on its back, lens underneath
    box(0, 0, -0.006, 0.16, 0.10, 0.012, 'JwBcnSteel')
    for x in (-0.135, 0.135): box(x, 0, -0.07, 0.012, 0.05, 0.13, 'JwBcnSteel')                 # bracket arms (tops inside the plate's ends... below it)
    box(0, 0, -0.012 - 0.003, 0.282, 0.05, 0.006, 'JwBcnSteel')                                   # bracket crossbar (overlaps the arms' tops)
    box(0, 0, -0.13, 0.25, 0.20, 0.06, 'JwBcnDark')                                               # the housing (its sides inside the arms)
    for i in range(6): box(-0.1 + i * 0.04, 0, -0.092, 0.008, 0.18, 0.02, 'JwBcnDark')            # cooling fins (feet inside the housing)
    box(0, 0, -0.163, 0.22, 0.17, 0.01, 'JwBcnLens')                                              # the lens (its top inside the housing)

def lamp(box, cyl):
    box(0, 0.006, 0, 0.14, 0.012, 0.14, 'JwBcnDark')                                             # back plate on the wall (+Y away from it)
    cyl(0, 0.035, 0, 0.05, 0.05, 'JwBcnDark', rot=(math.pi / 2, 0, 0))                            # the lamp body
    cyl(0, 0.064, 0, 0.044, 0.012, 'JwBcnLens', rot=(math.pi / 2, 0, 0))                          # the lens (its back inside the body)
    box(0, 0.075, 0.045, 0.11, 0.05, 0.006, 'JwBcnDark')                                          # the sun hood over it (its back inside the body)

build('SM_JwFlood', flood)
build('SM_JwLamp', lamp)
print('LAMPS saved DONE')
