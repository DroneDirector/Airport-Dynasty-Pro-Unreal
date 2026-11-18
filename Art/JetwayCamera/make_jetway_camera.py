# The jetways' CCTV cameras (owner, 2026-11-18, to-do 115 part 4; docs/Jetway-HMI-Plan.md). Real boarding bridges carry
# cab cameras and cameras round the bogie (Oshkosh AeroTech gate equipment: "CCTV cameras and monitors", cab cameras;
# CIMC-TianDa: cameras on the cab), built as the usual airport CCTV is: an outdoor bullet camera with a sun shield on a
# short bracket, and an indoor ceiling dome. Two meshes, each with its origin at its mounting plate:
#   SM_JwCamBullet - the plate flat on a surface (+Z up from it), the camera looking along +X
#   SM_JwCamDome   - the plate against a ceiling (the dome hangs down -Z)
# Nothing touches face to face (the hidden ends sit inside the part they meet), so no surfaces fight.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/JetwayCamera/make_jetway_camera.py
import bpy, os, math

HERE = os.path.dirname(os.path.abspath(__file__))
MATS = {}
def mat(n):
    if n not in MATS: MATS[n] = bpy.data.materials.new(n)
    return MATS[n]

def build(name, steps):
    for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
    parts = []
    def put(o, m): o.data.materials.append(mat(m)); parts.append(o); return o
    steps(put)
    for x in bpy.context.selected_objects: x.select_set(False)
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    ob = bpy.context.object; ob.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    for p in ob.data.polygons: p.use_smooth = True
    assert all(s.material for s in ob.material_slots), 'empty material slot'
    print('CAM', name, len(ob.data.polygons), 'faces', [m.name for m in ob.data.materials])
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, name.lower() + '.glb'), use_selection=True, export_format='GLB')
    return ob

def bullet(put):
    # the bolted plate, the bracket's arm up to a ball joint, the housing (a rounded tube), its sun shield over it
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.045, depth=0.010, location=(0, 0, 0.005)); put(bpy.context.object, 'JwCamBody')
    for a in range(4):
        r = math.radians(45 + 90 * a)
        bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.005, depth=0.008, location=(0.032 * math.cos(r), 0.032 * math.sin(r), 0.010)); put(bpy.context.object, 'JwCamDark')
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.014, depth=0.11, location=(0, 0, 0.008 + 0.055)); put(bpy.context.object, 'JwCamBody')
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.022, location=(0, 0, 0.12)); put(bpy.context.object, 'JwCamBody')
    # the housing: 22 cm long, its back end rounded, along +X, its middle 15 cm up
    Z = 0.155
    bpy.ops.mesh.primitive_cylinder_add(vertices=28, radius=0.040, depth=0.20, location=(0.05, 0, Z), rotation=(0, math.radians(90), 0)); put(bpy.context.object, 'JwCamBody')
    bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=14, radius=0.0395, location=(-0.05, 0, Z)); o = put(bpy.context.object, 'JwCamBody'); o.scale = (0.45, 1, 1)
    # its neck down into the ball
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.012, depth=0.05, location=(0.0, 0, 0.135)); put(bpy.context.object, 'JwCamBody')
    # the front: a dark bezel ring, the IR ring and the lens (just inside the bezel)
    bpy.ops.mesh.primitive_cylinder_add(vertices=28, radius=0.036, depth=0.012, location=(0.152, 0, Z), rotation=(0, math.radians(90), 0)); put(bpy.context.object, 'JwCamDark')
    bpy.ops.mesh.primitive_cylinder_add(vertices=28, radius=0.016, depth=0.006, location=(0.158, 0, Z), rotation=(0, math.radians(90), 0)); put(bpy.context.object, 'JwCamLens')
    for a in range(8):
        r = math.radians(22.5 + 45 * a)
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.0035, depth=0.004, location=(0.158, 0.026 * math.cos(r), Z + 0.026 * math.sin(r)), rotation=(0, math.radians(90), 0)); put(bpy.context.object, 'JwCamLens')
    # the sun shield: a thin curved plate over the top, reaching 4 cm past the front
    bm = bpy.data.meshes.new('shield'); vs = []; fs = []; N = 14; L0, L1 = -0.04, 0.20; R = 0.050; T = 0.003
    for i in range(N + 1):
        a = math.radians(-20 + 220 * i / N)   # (from a little below one side, over the top, to the other)
        for x in (L0, L1):
            for rr in (R, R + T): vs.append((x, rr * math.cos(a), Z + rr * math.sin(a)))
    def v(i, xi, ri): return i * 4 + xi * 2 + ri
    for i in range(N):
        fs += [(v(i, 0, 1), v(i, 1, 1), v(i + 1, 1, 1), v(i + 1, 0, 1)), (v(i, 0, 0), v(i + 1, 0, 0), v(i + 1, 1, 0), v(i, 1, 0)),
               (v(i, 0, 0), v(i, 0, 1), v(i + 1, 0, 1), v(i + 1, 0, 0)), (v(i, 1, 0), v(i + 1, 1, 0), v(i + 1, 1, 1), v(i, 1, 1))]
    fs += [(v(0, 0, 0), v(0, 1, 0), v(0, 1, 1), v(0, 0, 1)), (v(N, 0, 0), v(N, 0, 1), v(N, 1, 1), v(N, 1, 0))]
    bm.from_pydata(vs, [], fs); bm.update()
    o = bpy.data.objects.new('shield', bm); bpy.context.scene.collection.objects.link(o); put(o, 'JwCamBody')

def dome(put):
    # the ceiling plate (a shallow round base), the trim ring, the smoked half dome below it
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.075, depth=0.030, location=(0, 0, -0.015)); put(bpy.context.object, 'JwCamBody')
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.062, depth=0.010, location=(0, 0, -0.033)); put(bpy.context.object, 'JwCamDark')
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.055, location=(0, 0, -0.034)); o = put(bpy.context.object, 'JwCamLens')
    # (only its lower half shows: the upper half sits inside the ring and the plate)

build('SM_JwCamBullet', bullet)
build('SM_JwCamDome', dome)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'JetwayCamera.blend'))
print('CAM DONE')
