# The roadside airport sign (owner, 2026-11-14: on the side of the road, facing the cars driving to the airport; two
# metal posts as its feet, frames behind it so you can see it is bolted on; very high detail; reference photos of the
# Faroese blue information signs on two posts, Wikimedia Commons). Built our own way:
#   - the panel: 3.0 x 1.6 m aluminium, 3 mm, rounded corners, its edge folded back 25 mm all round; the face (front,
#     -y) the sign picture (UV 0..1; the game writes the airport's name and distance onto it)
#   - two horizontal top-hat rails across its back (galvanised steel, as the posts and the street lights), the panel's lower edge 1.8 m above the ground
#   - two galvanised posts (89 mm tube) behind the rails, plastic caps, standing in concrete feet
#   - where each rail crosses a post: a U-bolt round the post, its saddle plate, washers and nuts on the rail
# Writes Art/RoadSign/RoadSign.blend, roadsign_parts.glb (for Tools/find_zfight.py) and roadsign.glb (one mesh, slots
# SignFace / Aluminium / Galvanised / Concrete / Bolt / DarkMetal).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/RoadSign/make_roadsign.py
import bpy, bmesh, math, os
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
LM = os.path.join(os.path.dirname(HERE), 'LightMast')
PW, PH, PT = 3.0, 1.6, 0.003        # the panel
Z0 = 1.8                             # its lower edge above the ground
FOLD = 0.025                         # the folded edge's depth
RAIL_Z = (0.32, 1.28)                # the rails' heights on the panel (from its lower edge)
RAIL_D, RAIL_H = 0.035, 0.07         # rail depth (back from the panel) and height
POST_X, POST_R = 0.9, 0.0445
POST_Y = PT + RAIL_D + POST_R + 0.002
UVS = 0.6

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    return m
def galv():
    m = bpy.data.materials.new('Galvanised'); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(LM, 'metal_basecolor.png'))
    nt.links.new(t.outputs['Color'], b.inputs['Base Color']); b.inputs['Metallic'].default_value = 0.9; b.inputs['Roughness'].default_value = 0.4
    return m
def face_mat():
    m = bpy.data.materials.new('SignFace'); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(HERE, 'T_RoadSign_AirportBase.png'))
    nt.links.new(t.outputs['Color'], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.4
    return m
FACE, ALU, GALV = face_mat(), mat('Aluminium', (0.66, 0.68, 0.70), 0.85, 0.42), galv()
CONC, BOLT, DARK = mat('Concrete', (0.36, 0.35, 0.33), 0.0, 0.9), mat('Bolt', (0.55, 0.56, 0.58), 0.9, 0.35), mat('DarkMetal', (0.08, 0.08, 0.09), 0.0, 0.6)

PARTS = []
def finish(o, mats, smooth=True, angle=40):
    if o.name not in sc.collection.objects: sc.collection.objects.link(o)
    o.data.materials.clear()
    for m in (mats if isinstance(mats, list) else [mats]): o.data.materials.append(m)
    if smooth:
        for p in o.data.polygons: p.use_smooth = True
        o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(angle)
    PARTS.append(o); return o
def box_uv(o, scale=UVS):
    me = o.data
    if not me.uv_layers: me.uv_layers.new()
    uvl = me.uv_layers.active.data
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            v = o.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            uvl[li].uv = [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax][0] * scale, [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax][1] * scale

def rounded_rect(w, h, r, seg=10):
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for k in range(seg + 1):
            a = math.radians(a0 + 90 * k / seg); pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts

# ---- the panel: the face, the sheet's back, and the folded edge round it ----
def panel():
    outline = rounded_rect(PW, PH, 0.06)
    bm = bmesh.new(); uv = bm.loops.layers.uv.new()
    zc = Z0 + PH / 2
    front = [bm.verts.new((x, 0.0, zc + y)) for x, y in outline]
    back = [bm.verts.new((x, PT, zc + y)) for x, y in outline]
    fold = [bm.verts.new((x, FOLD, zc + y)) for x, y in outline]
    inner = [bm.verts.new((x * (1 - 0.012 / (PW / 2)), FOLD, zc + y * (1 - 0.012 / (PH / 2)))) for x, y in outline]
    innerb = [bm.verts.new((v.co.x, PT + 0.0005, v.co.z)) for v in inner]
    ff = bm.faces.new(list(reversed(front)))                       # the face, looking -y
    for lp in ff.loops: lp[uv].uv = (lp.vert.co.x / PW + 0.5, (lp.vert.co.z - Z0) / PH)
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((front[i], front[j], fold[j], fold[i]))       # the folded edge, outside
        bm.faces.new((fold[i], fold[j], inner[j], inner[i]))       # its lip
        bm.faces.new((inner[i], inner[j], innerb[j], innerb[i]))   # its inside
    bm.faces.new(innerb)                                           # the sheet's back, inside the fold
    bmesh.ops.recalc_face_normals(bm, faces=[f for f in bm.faces if f is not ff])
    ff.normal_update()
    if ff.normal.y > 0: ff.normal_flip()                           # (the face looks out at the road, -y)
    me = bpy.data.meshes.new('Panel'); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new('Panel', me); sc.collection.objects.link(o)
    # the face SignFace (slot 0), everything else aluminium (slot 1)
    finish(o, [FACE, ALU], angle=30)
    for p in o.data.polygons: p.material_index = 0 if (p.normal.y < -0.9 and p.center.y < 0.001) else 1
    # metre-scaled UVs for the aluminium parts (the face keeps its 0..1)
    uvl = o.data.uv_layers.active.data
    for p in o.data.polygons:
        if p.material_index == 1:
            for li in p.loop_indices:
                v = o.data.vertices[o.data.loops[li].vertex_index].co
                uvl[li].uv = (v.x * 2.0 + v.y * 2.0, v.z * 2.0)
    return o
panel()

# ---- the rails: top-hat extrusions across the back (flanges on the panel, the hat toward the posts) ----
def rail(zr):
    z = Z0 + zr
    prof = [(-0.0015, -RAIL_H / 2 - 0.012), (0.0035, -RAIL_H / 2 - 0.012), (0.0035, -RAIL_H / 2 + 0.002), (RAIL_D, -RAIL_H / 2 + 0.006),
            (RAIL_D, RAIL_H / 2 - 0.006), (0.0035, RAIL_H / 2 - 0.002), (0.0035, RAIL_H / 2 + 0.012), (-0.0015, RAIL_H / 2 + 0.012)]
    bm = bmesh.new(); L = PW - 0.16
    a = [bm.verts.new((-L / 2, PT + y, z + h)) for y, h in prof]
    b = [bm.verts.new((L / 2, PT + y, z + h)) for y, h in prof]
    for i in range(len(prof) - 1): bm.faces.new((a[i], a[i + 1], b[i + 1], b[i]))
    bm.faces.new((a[-1], a[0], b[0], b[-1]))
    bm.faces.new(list(reversed(a))); bm.faces.new(b)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new('Rail'); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new('Rail', me); sc.collection.objects.link(o); box_uv(o, UVS)
    return finish(o, GALV, smooth=False)   # (owner: the whole frame in the light posts' galvanised steel)
for zr in RAIL_Z: rail(zr)

# ---- the posts: galvanised tube, a cap on top, in a concrete foot ----
TOP = Z0 + PH - 0.08
for sx in (-1, 1):
    x = sx * POST_X
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=POST_R, depth=TOP + 0.45, location=(x, POST_Y, (TOP - 0.45) / 2))
    p = bpy.context.object
    # cylindrical metre UVs
    me = p.data; me.uv_layers.new(); uvl = me.uv_layers.active.data
    for poly in me.polygons:
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            uvl[li].uv = ((math.atan2(v.y, v.x) / (2 * math.pi) + 0.5) * 2 * math.pi * POST_R * UVS * 4, (v.z + p.location.z) * UVS)
    finish(p, GALV, angle=50)
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=POST_R + 0.003, depth=0.03, location=(x, POST_Y, TOP + 0.012))
    cap = bpy.context.object; bv = cap.modifiers.new('b', 'BEVEL'); bv.width = 0.006; bv.segments = 3; bpy.ops.object.modifier_apply(modifier='b')
    box_uv(cap, 4.0); finish(cap, DARK, angle=60)
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=0.2, depth=0.5, location=(x, POST_Y, -0.21))
    ft = bpy.context.object; bv = ft.modifiers.new('b', 'BEVEL'); bv.width = 0.015; bv.segments = 2; bpy.ops.object.modifier_apply(modifier='b')
    box_uv(ft, 1.0); finish(ft, CONC, angle=35)

# ---- the clamps: a U-bolt round the post, through a saddle on the rail, washers and nuts ----
def hexnut(loc, r, h, axis_y=True):
    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=r, depth=h, location=loc, rotation=(math.radians(90), 0, 0) if axis_y else (0, 0, 0))
    n = bpy.context.object; bv = n.modifiers.new('b', 'BEVEL'); bv.width = r * 0.12; bv.segments = 1; bpy.ops.object.modifier_apply(modifier='b')
    box_uv(n, 4.0); return finish(n, BOLT, smooth=False)
for sx in (-1, 1):
    x = sx * POST_X
    for zr in RAIL_Z:
        z = Z0 + zr
        for dz in (-0.022, 0.022):
            # the U-bolt: a bar bent round the back of the post, its legs running forward through the rail
            bpy.ops.mesh.primitive_torus_add(major_radius=POST_R + 0.006, minor_radius=0.005, major_segments=32, minor_segments=8,
                                             location=(x, POST_Y, z + dz), rotation=(0, 0, 0))
            u = bpy.context.object
            bm = bmesh.new(); bm.from_mesh(u.data)
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.y < -0.001], context='VERTS')
            bm.to_mesh(u.data); bm.free()
            box_uv(u, 4.0); finish(u, BOLT, angle=60)
            for lx in (-1, 1):
                leg_x = x + lx * (POST_R + 0.006)
                y0, y1 = PT + RAIL_D + 0.008, POST_Y   # (from inside the nut)
                bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=0.005, depth=y1 - y0, location=(leg_x, (y0 + y1) / 2, z + dz), rotation=(math.radians(90), 0, 0))
                lg = bpy.context.object; box_uv(lg, 4.0); finish(lg, BOLT, angle=60)
        # the saddle plate on the rail's hat, and its four nuts with washers
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, PT + RAIL_D + 0.0015, z))
        sd = bpy.context.object; sd.scale = (0.13, 0.006, 0.075); bpy.ops.object.transform_apply(scale=True)
        bv = sd.modifiers.new('b', 'BEVEL'); bv.width = 0.002; bv.segments = 1; bpy.ops.object.modifier_apply(modifier='b')
        box_uv(sd, 4.0); finish(sd, GALV, smooth=False)
        for lx in (-1, 1):
            for dz in (-0.022, 0.022):
                px = x + lx * (POST_R + 0.006)
                bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.011, depth=0.002, location=(px, PT + RAIL_D + 0.0045, z + dz), rotation=(math.radians(90), 0, 0))
                ws = bpy.context.object; box_uv(ws, 4.0); finish(ws, BOLT, smooth=False)
                hexnut((px, PT + RAIL_D + 0.00625, z + dz), 0.0085, 0.0075)   # (its back hidden in the saddle, 1 mm clear of every face: Tools/find_zfight.py)

# ---- the outputs ----
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'roadsign_parts.glb'), export_format='GLB', use_selection=False, export_apply=True)
for o in PARTS: o.select_set(True)
bpy.context.view_layer.objects.active = PARTS[0]
bpy.ops.object.join()
one = bpy.context.object; one.name = 'RoadSign'; one.data.name = 'RoadSign'
assert all(s.material for s in one.material_slots), 'an empty material slot'
print('SLOTS', [s.material.name for s in one.material_slots])
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'roadsign.glb'), export_format='GLB', use_selection=False, export_apply=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'RoadSign.blend'))
print('ROADSIGN DONE')
