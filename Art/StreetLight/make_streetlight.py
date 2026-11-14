# The road street light (owner, 2026-11-14: "very good quality, both texture-wise and in the shape, like the light mast";
# reference photos of Faroese street lights, Wikimedia Commons: a tall slim tapered galvanised pole with a smooth
# swan-neck bend at the top and a flat LED head over the road). Built our own way:
#   - root-mounted pole, 8.5 m to the light, the arm reaching 1.6 m out over the road (+y)
#   - hot-dip galvanised steel (the light mast's tileable metal), a sleeve and weld seam at the foot, a bolted service
#     hatch on the back, a small aluminium number tag, a concrete collar round the foot
#   - the head: a slim die-cast housing (dark grey) with cooling fins on top, the arm's clamp, a flat lens underneath
#     with the LED board behind it (the owner's lamp picture: led_panel.png)
# Writes Art/StreetLight/StreetLight.blend, streetlight_parts.glb (every part its own node: for Tools/find_zfight.py)
# and streetlight.glb (one mesh, material slots Galvanised / DarkMetal / LEDFace / Concrete / Bolt / Aluminium).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/StreetLight/make_streetlight.py
import bpy, bmesh, math, os
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
LM = os.path.join(os.path.dirname(HERE), 'LightMast')

H0 = 6.9            # the straight shaft's top (m)
BEND_R = 1.25       # the swan neck's bend radius
BEND_DEG = 84.0     # how far it turns (from upright to nearly level, the end tipped 6 degrees up)
R_FOOT, R_TOP, R_END = 0.076, 0.042, 0.034   # pole radius: foot, top of the straight, end of the arm
UVS = 0.6           # the metal's texture scale (UV units per metre), as the light mast's

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    return m

def galv():
    m = bpy.data.materials.new('Galvanised'); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    uv = nt.nodes.new('ShaderNodeTexCoord')
    def img(f, nc=False):
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(LM, f))
        if nc: t.image.colorspace_settings.name = 'Non-Color'
        nt.links.new(uv.outputs['UV'], t.inputs['Vector']); return t
    c = img('metal_basecolor.png'); r = img('metal_roughness.png', True); n = img('metal_normal.png', True)
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6
    nt.links.new(n.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
    nt.links.new(c.outputs['Color'], b.inputs['Base Color']); nt.links.new(r.outputs['Color'], b.inputs['Roughness'])
    b.inputs['Metallic'].default_value = 0.9
    return m

def led():
    m = bpy.data.materials.new('LEDFace'); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(LM, 'led_panel.png'))
    nt.links.new(t.outputs['Color'], b.inputs['Base Color'])
    b.inputs['Emission'].default_value = (1.0, 0.95, 0.88, 1); b.inputs['Emission Strength'].default_value = 2.0
    return m

GALV = galv()
DARK = mat('DarkMetal', (0.1, 0.11, 0.12), 0.7, 0.5)
LED = led()
CONC = mat('Concrete', (0.36, 0.35, 0.33), 0.0, 0.9)
BOLT = mat('Bolt', (0.55, 0.56, 0.58), 0.9, 0.35)
ALU = mat('Aluminium', (0.62, 0.64, 0.66), 0.85, 0.45)

PARTS = []
def finish(o, m, smooth=True, angle=50):
    if o.name not in sc.collection.objects: sc.collection.objects.link(o)
    o.data.materials.clear(); o.data.materials.append(m)
    if smooth:
        for p in o.data.polygons: p.use_smooth = True
        o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(angle)
    PARTS.append(o); return o

def box_uv(o, scale=UVS):
    """Simple metre-scaled box mapping for small parts (the metal's grain the same size everywhere)."""
    me = o.data
    if not me.uv_layers: me.uv_layers.new()
    uvl = me.uv_layers.active.data
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            v = o.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            a, b = [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax]
            uvl[li].uv = (a * scale, b * scale)

# ---- the pole: one sweep from the foot up the straight, round the swan neck, out to the head ----
def pole_path():
    pts = []   # (position, tangent, radius, distance along)
    n_st = 46
    for i in range(n_st + 1):
        z = H0 * i / n_st
        pts.append((Vector((0, 0, z)), Vector((0, 0, 1)), R_FOOT + (R_TOP - R_FOOT) * (z / H0)))
    n_b = 36
    for i in range(1, n_b + 1):
        a = math.radians(BEND_DEG) * i / n_b
        p = Vector((0, BEND_R * (1 - math.cos(a)), H0 + BEND_R * math.sin(a)))
        t = Vector((0, math.sin(a), math.cos(a)))
        pts.append((p, t, R_TOP + (R_END - R_TOP) * i / n_b))
    a = math.radians(BEND_DEG); end = pts[-1][0]; t = pts[-1][1]
    for i in range(1, 9):
        pts.append((end + t * (0.26 * i / 8), t, R_END))
    return pts
PATH = pole_path()
ARM_END, ARM_DIR = PATH[-1][0], PATH[-1][1]

def sweep(name, path, seg=48, cap_bottom=True, cap_top=True, extra_r=0.0):
    bm = bmesh.new(); rings = []; dist = 0.0; prev = None
    uvs = []
    for (p, t, r) in path:
        if prev is not None: dist += (p - prev).length
        prev = p
        side = Vector((1, 0, 0))                     # the bend is in the y-z plane: x stays the ring's side axis
        up = t.cross(side).normalized()
        ring = []
        for s in range(seg):
            a = 2 * math.pi * s / seg
            ring.append(bm.verts.new(p + (side * math.cos(a) + up * math.sin(a)) * (r + extra_r)))
        rings.append((ring, dist, r + extra_r))
    uv_l = bm.loops.layers.uv.new()
    for k in range(len(rings) - 1):
        ra, da, rra = rings[k]; rb, db, rrb = rings[k + 1]
        for s in range(seg):
            f = bm.faces.new((ra[s], ra[(s + 1) % seg], rb[(s + 1) % seg], rb[s]))
            us = [s / seg, (s + 1) / seg, (s + 1) / seg, s / seg]; ds = [da, da, db, db]; rs = [rra, rra, rrb, rrb]
            for lp, u, d, rr in zip(f.loops, us, ds, rs):
                lp[uv_l].uv = (u * 2 * math.pi * rr * UVS, d * UVS)
    if cap_bottom: bm.faces.new(list(reversed(rings[0][0])))
    if cap_top: bm.faces.new(rings[-1][0])
    bm.normal_update()
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return bpy.data.objects.new(name, me)
pole = finish(sweep('Pole', PATH), GALV, angle=60)

# the sleeve at the foot (the root's collar) and its weld seam
def ring_tube(name, z0, z1, r0, r1, m, seg=48):
    o = sweep(name, [(Vector((0, 0, z0)), Vector((0, 0, 1)), r0), (Vector((0, 0, z1)), Vector((0, 0, 1)), r1)], seg)
    return finish(o, m, angle=40)
ring_tube('Sleeve', -0.25, 0.42, R_FOOT + 0.012, R_FOOT + 0.010, GALV)
bpy.ops.mesh.primitive_torus_add(major_radius=R_FOOT + 0.006, minor_radius=0.0065, major_segments=48, minor_segments=8, location=(0, 0, 0.42))
weld = bpy.context.object; box_uv(weld); finish(weld, GALV)

# the concrete collar the pole stands in (sunk into the ground so it sits on any slope; top 4 cm above it)
bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=0.2, depth=0.44, location=(0, 0, -0.18))
col = bpy.context.object; bv = col.modifiers.new('b', 'BEVEL'); bv.width = 0.015; bv.segments = 2
bpy.ops.object.modifier_apply(modifier='b'); box_uv(col, 1.0); finish(col, CONC, angle=35)

# the service hatch on the back (away from the road, -y), 4 mm proud, following the pole's curve, two screws
def hatch():
    z0, z1, half = 0.62, 0.98, math.radians(26)
    bm = bmesh.new(); rows = []
    for zi in range(9):
        z = z0 + (z1 - z0) * zi / 8; r = R_FOOT + (R_TOP - R_FOOT) * (z / H0) + 0.004
        row = []
        for ai in range(9):
            a = -math.pi / 2 - half + 2 * half * ai / 8
            row.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
        rows.append(row)
    for zi in range(8):
        for ai in range(8):
            bm.faces.new((rows[zi][ai], rows[zi][ai + 1], rows[zi + 1][ai + 1], rows[zi + 1][ai]))
    bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=0.003)
    me = bpy.data.meshes.new('Hatch'); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new('Hatch', me); sc.collection.objects.link(o); box_uv(o); return finish(o, GALV, angle=30)
hatch()
for z in (0.66, 0.94):
    r = R_FOOT + (R_TOP - R_FOOT) * (z / H0) + 0.0085
    bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.006, depth=0.004, location=(0, -r, z), rotation=(math.radians(90), 0, 0))
    s = bpy.context.object; box_uv(s); finish(s, BOLT, angle=30)

# the number tag (aluminium) on the road side at 1.6 m, riveted
z = 1.6; r = R_FOOT + (R_TOP - R_FOOT) * (z / H0)
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, r + 0.0035, z)); tag = bpy.context.object
tag.scale = (0.05, 0.003, 0.07); bpy.ops.object.transform_apply(scale=True); box_uv(tag, 4.0); finish(tag, ALU, smooth=False)

# ---- the head: a slim die-cast housing at the arm's end, level, reaching on out over the road ----
HEAD_L, HEAD_W, HEAD_T = 0.64, 0.27, 0.075
hp = ARM_END + Vector((0, 0.02, 0.0))    # the clamp sits round the arm's end
def head_shell():
    # a rounded plan (superellipse), thick at the clamp, thinning to the front, the top crowned
    bm = bmesh.new(); seg = 64; layers = []
    for k, (zt, shrink) in enumerate(((0.0, 1.0), (0.012, 1.0), (HEAD_T * 0.65, 0.985), (HEAD_T * 0.92, 0.94), (HEAD_T, 0.86))):
        ring = []
        for s in range(seg):
            a = 2 * math.pi * s / seg
            cx, cy = math.cos(a), math.sin(a)
            px = math.copysign(abs(cx) ** 0.45, cx) * HEAD_W / 2 * shrink
            py = math.copysign(abs(cy) ** 0.45, cy) * HEAD_L / 2 * shrink
            frac = (py / (HEAD_L / 2) + 1) / 2          # 0 at the clamp end, 1 at the front
            thick = 1.0 - 0.45 * frac                   # thinner at the front
            ring.append(bm.verts.new((px, py + HEAD_L / 2 - 0.06, zt * thick)))
        layers.append(ring)
    for k in range(len(layers) - 1):
        for s in range(seg):
            bm.faces.new((layers[k][s], layers[k][(s + 1) % seg], layers[k + 1][(s + 1) % seg], layers[k + 1][s]))
    bm.faces.new(layers[-1])
    # the underside: a rim (left open in the middle for the lens, which sits 4 mm up inside)
    inner = []
    for s in range(seg):
        v = layers[0][s].co
        inner.append(bm.verts.new((v.x * 0.86, (v.y - (HEAD_L / 2 - 0.06)) * 0.86 + (HEAD_L / 2 - 0.06), 0.0)))
    for s in range(seg):
        bm.faces.new((layers[0][(s + 1) % seg], layers[0][s], inner[s], inner[(s + 1) % seg]))
    up = [bm.verts.new((v.co.x, v.co.y, 0.006)) for v in inner]
    for s in range(seg):
        bm.faces.new((inner[(s + 1) % seg], inner[s], up[s], up[(s + 1) % seg]))
    bm.faces.new(list(reversed(up)))   # (the cavity's roof, above the lens)
    bm.normal_update()
    me = bpy.data.meshes.new('Head'); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new('Head', me); o.location = hp - Vector((0, 0, HEAD_T * 0.55)); sc.collection.objects.link(o)
    bpy.context.view_layer.update(); box_uv(o, 2.0)
    return finish(o, DARK, angle=35), inner
head, _ = head_shell()
# the lens: flat, 3 mm up inside the rim, the LED board picture (UV 0..1 over it)
bm = bmesh.new(); seg = 64; vs = []
for s in range(seg):
    a = 2 * math.pi * s / seg; cx, cy = math.cos(a), math.sin(a)
    px = math.copysign(abs(cx) ** 0.45, cx) * HEAD_W / 2 * 0.85; py = math.copysign(abs(cy) ** 0.45, cy) * HEAD_L / 2 * 0.85
    vs.append(bm.verts.new((px, py + HEAD_L / 2 - 0.06, 0.003)))
f = bm.faces.new(list(reversed(vs)))
uvl = bm.loops.layers.uv.new()
for lp in f.loops:
    v = lp.vert.co; lp[uvl].uv = ((v.x / (HEAD_W * 0.85) + 0.5), ((v.y - (HEAD_L / 2 - 0.06)) / (HEAD_L * 0.85) + 0.5))
me = bpy.data.meshes.new('Lens'); bm.to_mesh(me); bm.free()
lens = bpy.data.objects.new('Lens', me); lens.location = head.location.copy(); sc.collection.objects.link(lens); finish(lens, LED, smooth=False)
# the cooling fins on top: thin ribs along the head, over its back two thirds
for i in range(7):
    x = (i - 3) * 0.03
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, hp.y + 0.13, hp.z + HEAD_T * 0.45 + 0.008))
    fin = bpy.context.object; fin.scale = (0.004, 0.3, 0.018); bpy.ops.object.transform_apply(scale=True)
    bv = fin.modifiers.new('b', 'BEVEL'); bv.width = 0.0015; bv.segments = 1; bpy.ops.object.modifier_apply(modifier='b')
    box_uv(fin, 2.0); finish(fin, DARK, smooth=False)
# the clamp round the arm's end, with its two bolts
bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=R_END + 0.012, depth=0.11, location=ARM_END - ARM_DIR * 0.04)
cl = bpy.context.object; cl.rotation_euler = Vector((0, 0, 1)).rotation_difference(ARM_DIR).to_euler()
bv = cl.modifiers.new('b', 'BEVEL'); bv.width = 0.004; bv.segments = 2; bpy.ops.object.modifier_apply(modifier='b')
box_uv(cl, 2.0); finish(cl, DARK, angle=35)
for dy in (-0.07, -0.01):
    p = ARM_END + ARM_DIR * dy + Vector((0, 0, R_END + 0.014))   # (sunk 2 mm into the clamp: never on its face)
    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.008, depth=0.008, location=p)
    b_ = bpy.context.object; box_uv(b_); finish(b_, BOLT, smooth=False)

# ---- the outputs ----
for o in PARTS: o.select_set(False)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'streetlight_parts.glb'), export_format='GLB', use_selection=False, export_apply=True)
for o in PARTS: o.select_set(True)
bpy.context.view_layer.objects.active = pole
bpy.ops.object.join()
one = bpy.context.object; one.name = 'StreetLight'; one.data.name = 'StreetLight'
assert all(s.material for s in one.material_slots), 'an empty material slot'
print('SLOTS', [s.material.name for s in one.material_slots])
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'streetlight.glb'), export_format='GLB', use_selection=False, export_apply=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'StreetLight.blend'))
print('STREETLIGHT DONE head at', tuple(round(c, 3) for c in hp))
