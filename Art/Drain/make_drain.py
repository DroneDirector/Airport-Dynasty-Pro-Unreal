# A road gully grating (owner, 2026-11-14: "water drainage on the roads, like those in the real world, at the real-world
# distances"; reference photos: Wikimedia Commons, storm drains / gully gratings): a 500 x 500 mm cast-iron grating in
# its cast frame, set flush in the gutter by the kerb, a narrow concrete bedding round it. Built our own way:
#   - the frame: 580 x 580 mm, 40 mm rim, its top 6 mm proud of the road
#   - the grating: 11 bars along the road (water runs along the gutter into them), two cross ribs under them, its hinge
#     lugs; the slots show the dark sump below (a black floor 25 mm down, its walls)
#   - the bedding: 60 mm of concrete round the frame, 3 mm proud
# Origin: the middle of the grating at the road's surface; x across the road (+x toward the kerb), y along it.
# Writes Art/Drain/Drain.blend, drain_parts.glb (Tools/find_zfight.py) and drain.glb (one mesh, slots CastIron / Sump /
# Concrete).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Drain/make_drain.py
import bpy, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    return m
IRON, SUMP, CONC = mat('CastIron', (0.12, 0.11, 0.10), 0.6, 0.62), mat('Sump', (0.015, 0.014, 0.013), 0.0, 0.95), mat('Concrete', (0.36, 0.35, 0.33), 0.0, 0.9)
PARTS = []

def box(name, cx, cy, cz, sx, sy, sz, m, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz)); o = bpy.context.object
    o.scale = (sx, sy, sz); bpy.ops.object.transform_apply(scale=True)
    if bevel:
        bv = o.modifiers.new('b', 'BEVEL'); bv.width = bevel; bv.segments = 2; bpy.ops.object.modifier_apply(modifier='b')
    o.name = name; o.data.materials.append(m)
    me = o.data; me.uv_layers.new(); uvl = me.uv_layers.active.data
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            uvl[li].uv = [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax][0] * 4, [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax][1] * 4
    PARTS.append(o); return o

# the bedding: four concrete strips round the frame (none under it: the sump is open there)
B0, B1 = 0.29, 0.35
for sx, sy, w, h in ((0, B0 + (B1 - B0) / 2, 2 * B1, B1 - B0), (0, -(B0 + (B1 - B0) / 2), 2 * B1, B1 - B0),
                     (B0 + (B1 - B0) / 2, 0, B1 - B0, 2 * B0 - 0.002), (-(B0 + (B1 - B0) / 2), 0, B1 - B0, 2 * B0 - 0.002)):
    box('Bedding', sx, sy, (0.003 - 0.08) / 2, w, h, 0.083, CONC, 0.004)
# the frame: a 40 mm rim round the grating's seat
F0, F1 = 0.25, 0.292   # (2 mm into the bedding: never face to face - Tools/find_zfight.py)
for sx, sy, w, h in ((0, (F0 + F1) / 2, 2 * F1, F1 - F0), (0, -(F0 + F1) / 2, 2 * F1, F1 - F0),
                     ((F0 + F1) / 2, 0, F1 - F0, 2 * F0 - 0.002), (-(F0 + F1) / 2, 0, F1 - F0, 2 * F0 - 0.002)):
    box('Frame', sx, sy, (0.006 - 0.066) / 2, w, h, 0.072, IRON, 0.003)
# the sump under the grating: its floor and four walls (dark)
sump = box('Sump', 0, 0, -0.0185, 0.47, 0.47, 0.035, SUMP)   # (one open box: no walls meeting walls)
import bmesh
bm = bmesh.new(); bm.from_mesh(sump.data)
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z > 0.9], context='FACES')
bmesh.ops.reverse_faces(bm, faces=bm.faces[:])   # (seen from inside, through the slots)
bm.to_mesh(sump.data); bm.free()
# the grating: its outer bar ring, 11 bars along the road with 25 mm slots between, two cross ribs (under the bar tops)
G = 0.245
for sx, sy, w, h in ((0, G - 0.0125, 2 * G, 0.025), (0, -(G - 0.0125), 2 * G, 0.025), (G - 0.0125, 0, 0.025, 2 * G - 0.052), (-(G - 0.0125), 0, 0.025, 2 * G - 0.052)):
    box('GrateRing', sx, sy, -0.008 + 0.0045, w, h, 0.025, IRON, 0.002)
for k in range(11):
    x = -0.2 + k * 0.04
    box('GrateBar', x, 0, (0.0075 - 0.0145) / 2, 0.018, 2 * G - 0.03, 0.022, IRON, 0.0025)
for y in (-0.08, 0.08):
    box('GrateRib', 0, y, -0.0165, 2 * G - 0.04, 0.016, 0.017, IRON)
# the hinge lugs on the kerb side
for y in (-0.12, 0.12):
    box('Lug', G + 0.008, y, -0.003, 0.02, 0.04, 0.014, IRON, 0.002)   # (2 mm into the ring)

bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'drain_parts.glb'), export_format='GLB', use_selection=False, export_apply=True)
for o in PARTS: o.select_set(True)
bpy.context.view_layer.objects.active = PARTS[0]
bpy.ops.object.join()
one = bpy.context.object; one.name = 'Drain'; one.data.name = 'Drain'
assert all(s.material for s in one.material_slots), 'an empty material slot'
print('SLOTS', [s.material.name for s in one.material_slots])
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'drain.glb'), export_format='GLB', use_selection=False, export_apply=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Drain.blend'))
print('DRAIN DONE')
