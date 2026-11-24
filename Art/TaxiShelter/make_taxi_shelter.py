# The taxi rank's shelter (to-do 234, design sheet Blueprints/Terminal/Changes/v2/s2_ground.py, SHEET 2e: "shelter +
# sign", 12 x 2.5 m on the pavement by each rank). A plain cantilever shelter, built our own way:
#   - three 120 mm square steel posts on the back line (the terminal side), base plates bolted to the paving
#   - a steel arm on each post reaching out toward the kerb, the flat roof on them: 12 x 2.5 m, 120 mm deep, 2.62 m clear
#   - the sign board on two stubs above the roof's middle: 2.6 x 0.7 m, yellow, TAXI in dark letters on both faces
# Origin: the middle of the footprint on the ground; +x toward the terminal (the road prop's "out": 1), y along the road.
# Writes Art/TaxiShelter/TaxiShelter.blend, taxishelter_parts.glb (Tools/find_zfight.py) and taxishelter.glb (one mesh,
# slots Steel / Roof / SignYellow / SignDark).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/TaxiShelter/make_taxi_shelter.py
import bpy, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)

def mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    return m
STEEL, ROOF = mat('Steel', (0.20, 0.21, 0.22), 0.7, 0.45), mat('Roof', (0.52, 0.53, 0.54), 0.3, 0.55)
YEL, DARK = mat('SignYellow', (0.86, 0.60, 0.04), 0.0, 0.5), mat('SignDark', (0.025, 0.025, 0.03), 0.0, 0.6)
PARTS = []

def uv(o):
    me = o.data; me.uv_layers.new(); uvl = me.uv_layers.active.data
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            a, b = [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax]
            uvl[li].uv = (a, b)

def box(name, x0, x1, y0, y1, z0, z1, m, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)); o = bpy.context.object
    o.scale = (x1 - x0, y1 - y0, z1 - z0); bpy.ops.object.transform_apply(scale=True)
    if bevel:
        bv = o.modifiers.new('b', 'BEVEL'); bv.width = bevel; bv.segments = 2; bpy.ops.object.modifier_apply(modifier='b')
    o.name = name; o.data.materials.append(m); uv(o); PARTS.append(o); return o

H = 2.62                       # the roof's underside (clear height)
PX = 1.0                       # the posts' line (the terminal side)
for y in (-5.4, 0.0, 5.4):
    box('Plate', PX - 0.15, PX + 0.15, y - 0.15, y + 0.15, -0.005, 0.012, STEEL, 0.004)
    box('Post', PX - 0.06, PX + 0.06, y - 0.06, y + 0.06, 0.008, H - 0.14, STEEL, 0.005)           # (4 mm into the plate)
    box('Arm', -1.10, PX + 0.08, y - 0.05, y + 0.05, H - 0.15, H + 0.01, STEEL, 0.004)              # (1 cm into the post and the roof)
box('Roof', -1.25, 1.25, -6.0, 6.0, H, H + 0.12, ROOF, 0.01)
# the sign: two stubs from the roof, the board on them
for y in (-0.9, 0.9):
    box('Stub', -0.03, 0.03, y - 0.03, y + 0.03, H + 0.11, H + 0.20, STEEL, 0.003)                   # (1 cm into roof and board)
box('Board', -0.06, 0.06, -1.3, 1.3, H + 0.19, H + 0.89, YEL, 0.006)
# TAXI on both faces, 3 mm proud of the board (2 mm deep: never face to face with it)
for side, rz in ((-1, -90.0), (1, 90.0)):
    bpy.ops.object.text_add(location=(side * 0.064, 0.0, H + 0.54), rotation=(math.radians(90.0), 0.0, math.radians(rz)))
    t = bpy.context.object; t.data.body = 'TAXI'; t.data.size = 0.48; t.data.extrude = 0.001
    t.data.align_x = 'CENTER'; t.data.align_y = 'CENTER'
    bpy.ops.object.convert(target='MESH'); t = bpy.context.object
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    t.name = 'Letters'; t.data.materials.clear(); t.data.materials.append(DARK); uv(t); PARTS.append(t)

bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'taxishelter_parts.glb'), export_format='GLB', use_selection=False, export_apply=True)
bpy.ops.object.select_all(action='DESELECT')
for o in PARTS: o.select_set(True)
bpy.context.view_layer.objects.active = PARTS[0]
bpy.ops.object.join()
one = bpy.context.object; one.name = 'TaxiShelter'; one.data.name = 'TaxiShelter'
assert all(s.material for s in one.material_slots), 'an empty material slot'
print('SLOTS', [s.material.name for s in one.material_slots], 'TRIS', sum(len(p.vertices) - 2 for p in one.data.polygons),
      'BOUNDS', [tuple(round(c, 3) for c in v) for v in (one.bound_box[0], one.bound_box[6])])
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'taxishelter.glb'), export_format='GLB', use_selection=False, export_apply=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'TaxiShelter.blend'))
