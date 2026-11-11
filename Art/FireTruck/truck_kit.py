# Shared modelling helpers for the fire trucks (make_firetruck.py variants, make_panther.py). Import after
# bpy.ops.wm.read_factory_settings(): it takes the scene's collection at import.
import bpy, bmesh, math
from mathutils import Vector, Quaternion

coll = bpy.context.scene.collection


def mat(name, color, metal=0.0, rough=0.5, emit=None, strength=0.0, alpha=1.0, coat=0.0):
    m = bpy.data.materials.get(name)
    if m: return m
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if coat: b.inputs['Clearcoat'].default_value = coat; b.inputs['Clearcoat Roughness'].default_value = 0.05
    if emit: b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    if alpha < 1.0:
        b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND'; m.shadow_method = 'HASHED'; m.use_backface_culling = False
    return m


M = {
    'paint': mat('TruckPaint', (0.62, 0.78, 0.02), 0.25, 0.28, coat=0.9),
    'black': mat('TruckBlack', (0.012, 0.012, 0.013), 0.1, 0.35, coat=0.5),
    'satin': mat('TruckSatin', (0.03, 0.032, 0.035), 0.2, 0.6),
    'rubber': mat('TruckRubber', (0.018, 0.018, 0.018), 0.0, 0.9),
    'rim': mat('TruckRim', (0.025, 0.026, 0.028), 0.6, 0.45),
    'chrome': mat('TruckChrome', (0.85, 0.86, 0.88), 1.0, 0.12),
    'alu': mat('TruckAlu', (0.62, 0.64, 0.66), 0.9, 0.32),
    'glass': mat('TruckGlass', (0.05, 0.07, 0.08), 0.0, 0.03, alpha=0.32),
    'lensClear': mat('TruckLensHead', (0.9, 0.92, 0.95), 0.0, 0.05),
    'lensHigh': mat('TruckLensHigh', (0.9, 0.92, 0.95), 0.0, 0.05),
    'lensRed': mat('TruckLensBrake', (0.55, 0.02, 0.02), 0.0, 0.1),
    'lensAmberL': mat('TruckLensTurnL', (0.9, 0.42, 0.02), 0.0, 0.1),
    'lensAmberR': mat('TruckLensTurnR', (0.9, 0.42, 0.02), 0.0, 0.1),
    'lensBlue': mat('TruckLensBlue', (0.02, 0.12, 0.85), 0.0, 0.1),
    'lensWhite': mat('TruckLensReverse', (0.85, 0.85, 0.85), 0.0, 0.1),
    'seat': mat('TruckSeat', (0.05, 0.055, 0.06), 0.0, 0.8),
    'dash': mat('TruckDash', (0.06, 0.065, 0.07), 0.0, 0.55),
    'screen': mat('TruckScreen', (0.02, 0.05, 0.08), 0.0, 0.1, (0.15, 0.35, 0.55), 1.5),
    'floor': mat('TruckFloor', (0.05, 0.05, 0.05), 0.0, 0.85),
    'decal': mat('TruckLettering', (0.01, 0.01, 0.012), 0.0, 0.4),
    'reflect': mat('TruckReflective', (0.92, 0.92, 0.9), 0.3, 0.25),
    'nozzle': mat('TruckNozzle', (0.12, 0.12, 0.13), 0.8, 0.3),
}


def finish(ob, material, smooth=True, angle=40):
    for m in (material if isinstance(material, (list, tuple)) else [material]): ob.data.materials.append(m)
    if smooth:
        for p in ob.data.polygons: p.use_smooth = True
        ob.data.use_auto_smooth = True; ob.data.auto_smooth_angle = math.radians(angle)
    return ob


def bevel(ob, w, seg=3):
    b = ob.modifiers.new('bevel', 'BEVEL'); b.width = w; b.segments = seg; b.limit_method = 'ANGLE'
    return ob


def box_uvs(bm):
    """UVs for a mesh that has none: each face projected on the plane it faces most, 1 UV unit = 1 metre (textures
    then tile at real size; a material that wants another tile size scales them)."""
    if bm.loops.layers.uv: return
    uv = bm.loops.layers.uv.new('UVMap')
    for f in bm.faces:
        n = f.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            co = l.vert.co
            l[uv].uv = (co.y, co.z) if ax == 0 else ((co.x, co.z) if ax == 1 else (co.x, co.y))


def new_obj(name, bm, material, bev=0.0, seg=3, angle=40):
    bm.normal_update(); box_uvs(bm)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); coll.objects.link(ob); finish(ob, material, True, angle)
    if bev: bevel(ob, bev, seg)
    return ob


def box(name, x0, y0, z0, x1, y1, z1, material, bev=0.0, seg=3):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector((abs(x1 - x0), abs(y1 - y0), abs(z1 - z0))), verts=bm.verts)
    ob = new_obj(name, bm, material, bev, seg); ob.location = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    return ob


def _extrude(name, pts3a, pts3b, material, bev, seg):
    bm = bmesh.new()
    a = [bm.verts.new(p) for p in pts3a]; b = [bm.verts.new(p) for p in pts3b]
    bm.faces.new(a); bm.faces.new(list(reversed(b)))
    n = len(a)
    for i in range(n): bm.faces.new((a[i], b[i], b[(i + 1) % n], a[(i + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, material, bev, seg)


def side(name, pts, x0, x1, material, bev=0.0, seg=3):
    """A side profile (y, z points) run across x from x0 to x1."""
    return _extrude(name, [(x0, y, z) for y, z in pts], [(x1, y, z) for y, z in pts], material, bev, seg)


def slab(name, outline, z0, z1, material, bev=0.0, seg=3):
    """A plan outline (x, y points) run up from z0 to z1."""
    return _extrude(name, [(x, y, z0) for x, y in outline], [(x, y, z1) for x, y in outline], material, bev, seg)


def strut(name, p0, p1, r, material):
    """A square bar from p0 to p1 (the cab's pillars and sills)."""
    p0, p1 = Vector(p0), Vector(p1); d = p1 - p0
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector((2 * r, 2 * r, d.length)), verts=bm.verts)
    ob = new_obj(name, bm, material, r * 0.4, 2)
    ob.rotation_mode = 'QUATERNION'; ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    ob.location = (p0 + p1) / 2
    return ob


def cyl(name, r, depth, at, material, axis='x', verts=48, bev=0.0):
    rot = {'x': (0, math.pi / 2, 0), 'y': (math.pi / 2, 0, 0), 'z': (0, 0, 0)}[axis]
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=at, rotation=rot)
    ob = bpy.context.active_object; ob.name = name
    finish(ob, material, True, 35)
    if bev: bevel(ob, bev, 3)
    return ob


def text(name, s, size, at, rot, material, align='CENTER', depth=0.004):
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = s; cu.size = size; cu.extrude = depth; cu.align_x = align; cu.align_y = 'CENTER'
    ob = bpy.data.objects.new(name, cu); coll.objects.link(ob); ob.location = at; ob.rotation_euler = rot
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.object.convert(target='MESH'); ob = bpy.context.active_object; ob.data.materials.clear(); ob.data.materials.append(material)
    return ob


