# Taxiway lights (to-do 40e, owner 2026-11-13 / -16; ICAO Annex 14): two fittings, origin at ground level, up +Z.
#   SM_TwyEdge   - elevated edge light: a round base flange bolted to the ground, a frangible stem (with its break
#                  groove), the lamp housing and the blue glass dome on top - about 35 cm high
#   SM_TwyCentre - inset centreline light: a flush round steel fitting 30 cm across, raised 1 cm, with its two small
#                  prism windows (the green lenses) facing along the taxiway (+Y / -Y)
# Lenses in their own slot (JwBcnLens - lit by the game, coloured per mesh copy). Nothing face to face.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/AirfieldLights/make_airfield_lights.py
import bpy, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
MATS = {}
def mat(n):
    if n not in MATS: MATS[n] = bpy.data.materials.get(n) or bpy.data.materials.new(n)
    return MATS[n]
def build(name, make):
    for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
    parts = []
    def cyl(z0, z1, r, m, v=20, r2=None, fill='NGON', loc=(0, 0), rot=(0, 0, 0)):
        d = z1 - z0
        if r2 is None: bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=r, depth=d, location=(loc[0], loc[1], (z0 + z1) / 2), rotation=rot, end_fill_type=fill)
        else: bpy.ops.mesh.primitive_cone_add(vertices=v, radius1=r, radius2=r2, depth=d, location=(loc[0], loc[1], (z0 + z1) / 2), rotation=rot)
        o = bpy.context.object; o.data.materials.append(mat(m)); parts.append(o)
    def box(cx, cy, cz, sx, sy, sz, m):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz)); o = bpy.context.object; o.scale = (sx, sy, sz); o.data.materials.append(mat(m)); parts.append(o)
    def sphere(z, r, m):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=10, radius=r, location=(0, 0, z)); o = bpy.context.object; o.data.materials.append(mat(m)); parts.append(o)
    make(cyl, box, sphere)
    for x in bpy.context.selected_objects: x.select_set(False)
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    ob = bpy.context.object; ob.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    assert all(s.material for s in ob.material_slots), 'empty material slot'
    print('LIGHT', name, len(ob.data.polygons), 'faces', [m.name for m in ob.data.materials])
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, name + '.blend'))
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, name.lower() + '.glb'), use_selection=True, export_format='GLB')

def edge(cyl, box, sphere):
    cyl(-0.02, 0.02, 0.09, 'JwBcnDark', v=24)                    # base flange (its foot in the ground)
    for a in range(4):
        x, y = 0.065 * math.cos(a * math.pi / 2 + 0.785), 0.065 * math.sin(a * math.pi / 2 + 0.785)
        cyl(0.01, 0.032, 0.008, 'JwBcnSteel', v=6, loc=(x, y))    # bolts (feet inside the flange)
    cyl(0.01, 0.14, 0.022, 'JwBcnSteel', v=16)                   # lower stem
    cyl(0.135, 0.15, 0.017, 'JwBcnDark', v=16)                   # the frangible break groove
    cyl(0.145, 0.27, 0.022, 'JwBcnSteel', v=16)                  # upper stem
    cyl(0.265, 0.30, 0.05, 'JwBcnDark', v=20, r2=0.045)          # lamp housing
    cyl(0.295, 0.33, 0.042, 'JwBcnLens', v=20, fill='NOTHING')   # the blue glass, its side
    sphere(0.33, 0.041, 'JwBcnLens')                             # and its dome (lower half inside the glass)

def centre(cyl, box, sphere):
    cyl(-0.03, 0.006, 0.15, 'JwBcnSteel', v=28)                  # the fitting, flush (6 mm proud)
    cyl(0.004, 0.012, 0.12, 'JwBcnDark', v=28, r2=0.10)          # its raised top (bottom inside the fitting)
    for s in (1, -1):
        box(0, s * 0.055, 0.012, 0.05, 0.03, 0.012, 'JwBcnLens')  # the prism windows, along the taxiway (half inside the top)

build('SM_TwyEdge', edge)
build('SM_TwyCentre', centre)
print('LIGHTS DONE')
