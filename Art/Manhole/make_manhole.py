# A round manhole cover in the pavement (owner, 2026-11-14: "the round man covers on pavements, every now and then, a very
# big distance between them"; reference photos: Wikimedia Commons, drain covers). Built our own way:
#   - the cover: 650 mm cast iron, its top 5 mm proud of the pavement, a raised pattern of three rings and 24 spokes
#     (the grip), two lifting slots (dark, sunk)
#   - the frame: an 800 mm cast ring round it; a 100 mm concrete bedding round that
# Origin: the cover's middle at the pavement's surface. Writes Art/Manhole/Manhole.blend, manhole_parts.glb
# (Tools/find_zfight.py) and manhole.glb (one mesh, slots CastIron / Sump / Concrete).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Manhole/make_manhole.py
import bpy, bmesh, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    return m
IRON, SUMP, CONC = mat('CastIron', (0.12, 0.11, 0.10), 0.6, 0.6), mat('Sump', (0.015, 0.014, 0.013), 0.0, 0.95), mat('Concrete', (0.36, 0.35, 0.33), 0.0, 0.9)
PARTS = []

def uvs(o, k=4.0):
    me = o.data
    if not me.uv_layers: me.uv_layers.new()
    uvl = me.uv_layers.active.data
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            v = o.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            a, b = [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax]
            uvl[li].uv = (a * k, b * k)

def ring(name, r0, r1, z0, z1, m, seg=64):
    """A flat ring (r0 > 0) or disc (r0 = 0) from z0 to z1."""
    bm = bmesh.new()
    if r0 <= 0:
        bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r1, radius2=r1, depth=z1 - z0)
    else:
        outer = bmesh.ops.create_cone(bm, cap_ends=False, segments=seg, radius1=r1, radius2=r1, depth=z1 - z0)
        bm2 = bmesh.new(); bmesh.ops.create_cone(bm2, cap_ends=False, segments=seg, radius1=r0, radius2=r0, depth=z1 - z0)
        bmesh.ops.reverse_faces(bm2, faces=bm2.faces[:])
        me2 = bpy.data.meshes.new('tmp'); bm2.to_mesh(me2); bm2.free(); bm.from_mesh(me2)
        tops = [v for v in bm.verts if v.co.z > 0]; bots = [v for v in bm.verts if v.co.z < 0]
        def cap(vs, up):
            vs = sorted(vs, key=lambda v: (math.atan2(v.co.y, v.co.x), math.hypot(v.co.x, v.co.y)))
            outer_v = [v for v in vs if math.hypot(v.co.x, v.co.y) > (r0 + r1) / 2]; inner_v = [v for v in vs if math.hypot(v.co.x, v.co.y) <= (r0 + r1) / 2]
            outer_v.sort(key=lambda v: math.atan2(v.co.y, v.co.x)); inner_v.sort(key=lambda v: math.atan2(v.co.y, v.co.x))
            n = len(outer_v)
            for i in range(n):
                j = (i + 1) % n
                f = bm.faces.new((outer_v[i], outer_v[j], inner_v[j], inner_v[i]))
                if (f.normal.z > 0) != up: f.normal_flip()
        cap(tops, True); cap(bots, False)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); o.location.z = (z0 + z1) / 2
    bpy.context.view_layer.update(); o.data.materials.append(m); uvs(o); PARTS.append(o)
    for p in o.data.polygons: p.use_smooth = abs(p.normal.z) < 0.5
    return o

ring('Bedding', 0.398, 0.5, -0.08, 0.003, CONC)          # (2 mm under the frame's outside: never face to face)
ring('Frame', 0.322, 0.4, -0.07, 0.006, IRON)
ring('Cover', 0.0, 0.325, -0.03, 0.005, IRON, 72)
for r in (0.11, 0.2, 0.29):
    ring('PatternRing', r - 0.008, r + 0.008, 0.0035, 0.009, IRON, 72)
for k in range(24):
    a = 2 * math.pi * (k + 0.5) / 24
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.2 * math.cos(a), 0.2 * math.sin(a), 0.005))   # (spokes 1.5 mm lower than the rings: never level with them)
    o = bpy.context.object; o.scale = (0.19, 0.012, 0.005); o.rotation_euler = (0, 0, a); bpy.ops.object.transform_apply(scale=True, rotation=True)
    o.name = 'Spoke'; o.data.materials.append(IRON); uvs(o); PARTS.append(o)
# the two lifting slots: dark, sunk through the cover's top (their floors 12 mm down)
for sx in (-1, 1):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(sx * 0.255, 0, -0.00495))   # (its dark top 1.3 mm over the cover's: never level)
    o = bpy.context.object; o.scale = (0.022, 0.06, 0.0225); bpy.ops.object.transform_apply(scale=True)
    o.name = 'LiftSlot'; o.data.materials.append(SUMP); uvs(o); PARTS.append(o)

bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'manhole_parts.glb'), export_format='GLB', use_selection=False, export_apply=True)
for o in PARTS: o.select_set(True)
bpy.context.view_layer.objects.active = PARTS[0]
bpy.ops.object.join()
one = bpy.context.object; one.name = 'Manhole'; one.data.name = 'Manhole'
assert all(s.material for s in one.material_slots), 'an empty material slot'
print('SLOTS', [s.material.name for s in one.material_slots])
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'manhole.glb'), export_format='GLB', use_selection=False, export_apply=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Manhole.blend'))
print('MANHOLE DONE')
