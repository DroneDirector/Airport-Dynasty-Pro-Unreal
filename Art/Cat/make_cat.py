# The cat (owner, 2026-11-13): a domestic shorthair to replace the pet step's stand-in and to live in the game, built
# like the MetaHumans' idea for animals: one shape and one skeleton for every cat, its looks by parameters (coat colour
# and pattern, eye colour, size, build slim to round, fur length). References: Art/Cat/reference (kept out of git).
# Part 1: the body (skin-modifier skeleton, voxel-merged with the head, ears, muzzle), eyes, nose, whiskers and the fur
# (strands with clumps, shorter on the face, longer on the belly and tail), the tabby coat; turntable renders to check.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Cat/make_cat.py -- <render_dir> [coat] [quick]
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix
from pathlib import Path

HERE = Path(__file__).resolve().parent
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = Path(argv[0]) if argv else HERE / 'renders'
COAT = argv[1] if len(argv) > 1 else 'brown_tabby'
QUICK = len(argv) > 2 and argv[2] == 'quick'
OUT.mkdir(parents=True, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

# ---------------------------------------------------------------- the coats (parameters, as the creator will set them)
COATS = {
    #               base fur           stripe / dark         belly / light        stripes 0..1  eye
    'brown_tabby': ((0.20, 0.13, 0.065), (0.025, 0.018, 0.012), (0.55, 0.45, 0.32), 0.85, (0.55, 0.62, 0.12)),
    'ginger':      ((0.55, 0.22, 0.05), (0.30, 0.09, 0.02), (0.80, 0.55, 0.32), 0.6,  (0.80, 0.55, 0.10)),
    'silver':      ((0.70, 0.70, 0.70), (0.08, 0.08, 0.08), (0.92, 0.92, 0.92), 0.85, (0.40, 0.65, 0.20)),
    'black':       ((0.025, 0.022, 0.02), (0.012, 0.01, 0.01), (0.05, 0.045, 0.04), 0.0, (0.85, 0.70, 0.08)),
    'grey':        ((0.32, 0.33, 0.35), (0.20, 0.21, 0.23), (0.45, 0.46, 0.48), 0.1,  (0.80, 0.60, 0.10)),
}
BASE, DARK, LIGHT, STRIPES, EYE = COATS[COAT]

# ---------------------------------------------------------------- the body: a skeleton of points, skinned
# the cat faces -y (towards Blender's front view), x is its left-right, z up; metres. A domestic cat: 46 cm nose to rump,
# 25 cm at the shoulder, the tail 28 cm
P = {}
def pt(name, x, y, z, r): P[name] = (Vector((x, y, z)), r)
pt('pelvis', 0, 0.13, 0.215, 0.047); pt('loin', 0, 0.06, 0.222, 0.05); pt('belly', 0, -0.01, 0.215, 0.053)
pt('chest', 0, -0.08, 0.215, 0.056); pt('withers', 0, -0.115, 0.235, 0.046); pt('neck', 0, -0.155, 0.27, 0.041)
for s, x in (('L', 0.042), ('R', -0.042)):
    pt('sh' + s, x * 0.85, -0.105, 0.185, 0.024); pt('el' + s, x * 0.8, -0.095, 0.115, 0.015); pt('wr' + s, x * 0.75, -0.105, 0.035, 0.0105); pt('pa' + s, x * 0.75, -0.122, 0.012, 0.013)
for s, x in (('L', 0.048), ('R', -0.048)):
    pt('hi' + s, x * 0.85, 0.11, 0.185, 0.034); pt('kn' + s, x * 0.85, 0.06, 0.12, 0.021); pt('ho' + s, x * 0.8, 0.15, 0.055, 0.0105); pt('hp' + s, x * 0.75, 0.128, 0.012, 0.013)
tail = []
for i in range(9):
    t = i / 8
    pt('ta%d' % i, 0, 0.165 + 0.16 * math.sin(t * 1.35), 0.205 - 0.21 * math.sin(t * 1.9) * (1 - 0.25 * t) + 0.0 * t, 0.0155 - 0.006 * t); tail.append('ta%d' % i)
EDGES = [('pelvis', 'loin'), ('loin', 'belly'), ('belly', 'chest'), ('chest', 'withers'), ('withers', 'neck')]
for s in 'LR':
    EDGES += [('withers', 'sh' + s), ('sh' + s, 'el' + s), ('el' + s, 'wr' + s), ('wr' + s, 'pa' + s)]
    EDGES += [('pelvis', 'hi' + s), ('hi' + s, 'kn' + s), ('kn' + s, 'ho' + s), ('ho' + s, 'hp' + s)]
EDGES += [('pelvis', 'ta0')] + [(tail[i], tail[i + 1]) for i in range(len(tail) - 1)]

# the body as metaballs: a capsule along every bone and a ball at every joint melt into one smooth animal (a metaball's
# surface sits inside its radius: about 0.62 of it at this threshold and stiffness)
mb = bpy.data.metaballs.new('CatMeta'); mb.resolution = 0.006; mb.render_resolution = 0.004; mb.threshold = 0.6
body = bpy.data.objects.new('CatBody', mb); sc.collection.objects.link(body)
K = 1.0 / 0.62
TORSO = ('pelvis', 'loin', 'belly', 'chest', 'withers')
for n, (c, r) in P.items():
    e = mb.elements.new(); e.co = c; e.stiffness = 2.0
    if n in TORSO: e.type = 'ELLIPSOID'; e.radius = K; e.size_x, e.size_y, e.size_z = r * 0.95, r * 1.25, r * 1.2
    else: e.type = 'BALL'; e.radius = r * K
for a_, b_ in EDGES:
    ca, ra = P[a_]; cb, rb = P[b_]
    d = cb - ca; L = d.length
    e = mb.elements.new(); e.type = 'CAPSULE'; e.co = (ca + cb) / 2; e.radius = min(ra, rb) * K * 0.95; e.size_x = L / 2; e.stiffness = 2.0
    e.rotation = d.normalized().to_track_quat('X', 'Z')
for sgn in (1, -1):
    e = mb.elements.new(); e.type = 'ELLIPSOID'; e.radius = K; e.stiffness = 2.0
    e.co = Vector((sgn * 0.036, 0.095, 0.16)); e.size_x, e.size_y, e.size_z = 0.026, 0.05, 0.05
    e.rotation = Vector((0, -0.05, -0.065)).normalized().to_track_quat('Z', 'X')
# ---------------------------------------------------------------- the head: cranium, cheeks, muzzle, chin, ears
def sphere(name, loc, scale, seg=48):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=seg // 2, location=loc)
    o = bpy.context.object; o.name = name; o.scale = scale; bpy.ops.object.transform_apply(scale=True); return o
H = Vector((0, -0.205, 0.315))
def blob(loc, size):
    e = mb.elements.new(); e.type = 'ELLIPSOID'; e.co = loc; e.radius = 1.0 * K; e.stiffness = 2.0
    e.size_x, e.size_y, e.size_z = size
for loc, size in ((H + Vector((0, 0.012, 0.01)), (0.048, 0.045, 0.043)), (H + Vector((0.026, -0.012, -0.012)), (0.026, 0.028, 0.024)),
                  (H + Vector((-0.026, -0.012, -0.012)), (0.026, 0.028, 0.024)), (H + Vector((0, -0.034, -0.016)), (0.021, 0.017, 0.015)),
                  (H + Vector((0.011, -0.044, -0.016)), (0.012, 0.011, 0.011)), (H + Vector((-0.011, -0.044, -0.016)), (0.012, 0.011, 0.011)),
                  (H + Vector((0, -0.031, -0.031)), (0.012, 0.012, 0.009))):
    blob(loc, size)
parts = []
def ear(name, side):
    # a cat's ear: a thin curved shell, wide at the base, pointed, tipped slightly out
    bm = bmesh.new(); n = 12
    base_c = H + Vector((side * 0.027, 0.004, 0.036)); tip = H + Vector((side * 0.044, 0.012, 0.087))
    ring = []
    for i in range(n + 1):
        a = math.pi * i / n; ring.append(base_c + Vector((side * -0.022 * math.cos(a), -0.013 * math.sin(a) - 0.002, 0.0)))
    back = [v + Vector((0, 0.006, 0)) for v in ring]
    vs_f = [bm.verts.new(v) for v in ring]; vs_b = [bm.verts.new(v) for v in back]
    t1 = bm.verts.new(tip); t2 = bm.verts.new(tip + Vector((0, 0.004, 0)))
    for i in range(n):
        bm.faces.new((vs_f[i], vs_f[i + 1], t1)); bm.faces.new((vs_b[i + 1], vs_b[i], t2))
    bm.faces.new((vs_f[0], vs_b[0], t2, t1)); bm.faces.new((vs_b[n], vs_f[n], t1, t2))
    for i in range(n): bm.faces.new((vs_f[i + 1], vs_f[i], vs_b[i], vs_b[i + 1]))
    em = bpy.data.meshes.new(name); bm.to_mesh(em); o = bpy.data.objects.new(name, em); sc.collection.objects.link(o)
    s = o.modifiers.new('S', 'SUBSURF'); s.levels = 2
    so = o.modifiers.new('T', 'SOLIDIFY'); so.thickness = 0.0025
    return o
ears = [ear('earL', 1), ear('earR', -1)]

# the metaballs to a mesh, then merged with the ears into one watertight skin (voxel remesh), smoothed
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.convert(target='MESH')
body = bpy.context.view_layer.objects.active
for o in [body] + parts + ears:
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    for m in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    o.select_set(False)
for o in parts + ears: o.select_set(True)
body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
rm = body.modifiers.new('Remesh', 'REMESH'); rm.mode = 'VOXEL'; rm.voxel_size = 0.0028; rm.adaptivity = 0.0
bpy.ops.object.modifier_apply(modifier='Remesh')
sm = body.modifiers.new('Smooth', 'CORRECTIVE_SMOOTH'); sm.iterations = 8; sm.factor = 0.5; sm.use_only_smooth = True
bpy.ops.object.modifier_apply(modifier='Smooth')
bpy.ops.object.shade_smooth()
body.name = 'Cat'
log = lambda m: print('ADP_CAT', m)
log('skin: %d verts' % len(body.data.vertices))

# the regions, for fur length and density: face (short), belly and tail (longer), paws and nose (very short)
def group(name, fn):
    g = body.vertex_groups.new(name=name)
    for v in body.data.vertices:
        w = fn(v.co)
        if w > 0: g.add([v.index], min(1.0, w), 'REPLACE')
    return g
face_d = lambda c: (c - (H + Vector((0, -0.04, -0.012)))).length
group('length', lambda c: max(0.05, min(1.0,
    (0.25 if face_d(c) < 0.03 else 0.45 if c.y < -0.17 and c.z > 0.27 else 0.7 if c.z < 0.06 else 1.0)
    * (1.25 if (c.z < 0.2 and -0.1 < c.y < 0.1) else 1.0) * (1.15 if c.y > 0.17 else 1.0))))
group('density', lambda c: 0.0 if (face_d(c) < 0.012 or (abs(c.x) > 0.025 and c.y < -0.18 and c.z > 0.34 and abs(c.y + 0.195) < 0.006)) else 1.0)

# ---------------------------------------------------------------- the eyes, the nose, the whiskers
def eye(side):
    c = H + Vector((side * 0.0195, -0.030, 0.010))
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.0105, location=(0, 0, 0))
    ball = bpy.context.object; ball.name = 'eye' + ('L' if side > 0 else 'R')
    ball.location = c; ball.rotation_euler = (0, 0, side * math.radians(16))
    m = bpy.data.materials.new('Eye'); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); sep = nt.nodes.new('ShaderNodeSeparateXYZ')
    nt.links.new(tc.outputs['Object'], sep.inputs[0])
    # the iris looking out of the ball's front (-y): radial gradient, a vertical slit pupil
    r = nt.nodes.new('ShaderNodeMath'); r.operation = 'SQRT'
    sq = nt.nodes.new('ShaderNodeVectorMath'); sq.operation = 'LENGTH'
    comb = nt.nodes.new('ShaderNodeCombineXYZ'); nt.links.new(sep.outputs['X'], comb.inputs['X']); nt.links.new(sep.outputs['Z'], comb.inputs['Z'])
    nt.links.new(comb.outputs[0], sq.inputs[0])
    slit = nt.nodes.new('ShaderNodeCombineXYZ'); mx = nt.nodes.new('ShaderNodeMath'); mx.operation = 'MULTIPLY'; mx.inputs[1].default_value = 4.0
    nt.links.new(sep.outputs['X'], mx.inputs[0]); nt.links.new(mx.outputs[0], slit.inputs['X']); nt.links.new(sep.outputs['Z'], slit.inputs['Z'])
    sl = nt.nodes.new('ShaderNodeVectorMath'); sl.operation = 'LENGTH'; nt.links.new(slit.outputs[0], sl.inputs[0])
    ramp = nt.nodes.new('ShaderNodeValToRGB'); nt.links.new(sq.outputs['Value'], ramp.inputs[0])
    e = ramp.color_ramp.elements; e[0].position = 0.0; e[0].color = (*EYE, 1); e[1].position = 0.0095; e[1].color = (EYE[0] * 0.45, EYE[1] * 0.4, EYE[2] * 0.2, 1)
    e.new(0.0098).color = (0.02, 0.02, 0.02, 1); e.new(0.0102).color = (0.95, 0.93, 0.9, 1)
    pup = nt.nodes.new('ShaderNodeValToRGB'); nt.links.new(sl.outputs['Value'], pup.inputs[0])
    pe = pup.color_ramp.elements; pe[0].position = 0.0; pe[0].color = (0.003, 0.003, 0.003, 1); pe[1].position = 0.0065; pe[1].color = (1, 1, 1, 1); pup.color_ramp.interpolation = 'EASE'
    mul = nt.nodes.new('ShaderNodeMixRGB'); mul.blend_type = 'MULTIPLY'; mul.inputs[0].default_value = 1.0
    nt.links.new(ramp.outputs[0], mul.inputs[1]); nt.links.new(pup.outputs[0], mul.inputs[2])
    nt.links.new(mul.outputs[0], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.05; b.inputs['Clearcoat'].default_value = 1.0; b.inputs['Clearcoat Roughness'].default_value = 0.0
    b.inputs['Specular'].default_value = 0.8
    ball.data.materials.append(m)
    return ball
eyes = [eye(1), eye(-1)]
# the eyes sit in sockets: carve the skin round them a little
for e_ in eyes:
    for v in body.data.vertices:
        d = (v.co - e_.location)
        if d.length < 0.0112: v.co = e_.location + d.normalized() * 0.0112

nose_m = bpy.data.materials.new('Nose'); nose_m.use_nodes = True
nb = nose_m.node_tree.nodes['Principled BSDF']; nb.inputs['Base Color'].default_value = (0.42, 0.18, 0.17, 1) if COAT != 'black' else (0.03, 0.025, 0.025, 1); nb.inputs['Roughness'].default_value = 0.45
bm = bmesh.new()
nc = H + Vector((0, -0.052, -0.004))
tri = [bm.verts.new(nc + Vector((-0.0065, 0, 0.003))), bm.verts.new(nc + Vector((0.0065, 0, 0.003))), bm.verts.new(nc + Vector((0, -0.002, -0.004)))]
top = [bm.verts.new(v.co + Vector((0, 0.006, 0.002))) for v in tri]
bm.faces.new(tri[::-1]); bm.faces.new(top)
for i in range(3): bm.faces.new((tri[i], tri[(i + 1) % 3], top[(i + 1) % 3], top[i]))
nm = bpy.data.meshes.new('Nose'); bm.to_mesh(nm); nose = bpy.data.objects.new('Nose', nm); sc.collection.objects.link(nose); nm.materials.append(nose_m)
nose.modifiers.new('S', 'SUBSURF').levels = 2

# ---------------------------------------------------------------- the skin and the fur's colour: the coat pattern
def coat_nodes(nt, from_hair):
    tc = nt.nodes.new('ShaderNodeTexCoord'); tc.from_instancer = from_hair
    co = tc.outputs['Object']
    sep = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(co, sep.inputs[0])
    # tabby: dark bands round the body and legs (waves along y and z), broken by noise; a lighter belly and muzzle
    wave = nt.nodes.new('ShaderNodeTexWave'); wave.wave_type = 'BANDS'; wave.bands_direction = 'Y'
    wave.inputs['Scale'].default_value = 7.5; wave.inputs['Distortion'].default_value = 9.0; wave.inputs['Detail'].default_value = 3.0
    wave.inputs['Detail Scale'].default_value = 1.8
    nt.links.new(co, wave.inputs['Vector'])
    noise = nt.nodes.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value = 45.0; noise.inputs['Detail'].default_value = 6.0
    nt.links.new(co, noise.inputs['Vector'])
    stripe = nt.nodes.new('ShaderNodeValToRGB'); nt.links.new(wave.outputs['Fac'], stripe.inputs[0])
    se = stripe.color_ramp.elements; se[0].position = 0.62; se[0].color = (0, 0, 0, 1); se[1].position = 0.8; se[1].color = (1, 1, 1, 1)
    strength = nt.nodes.new('ShaderNodeMath'); strength.operation = 'MULTIPLY'; strength.inputs[1].default_value = STRIPES
    nt.links.new(stripe.outputs[0], strength.inputs[0])
    mix = nt.nodes.new('ShaderNodeMixRGB'); mix.inputs[1].default_value = (*BASE, 1); mix.inputs[2].default_value = (*DARK, 1)
    nt.links.new(strength.outputs[0], mix.inputs[0])
    # light under the belly, chest and chin (low z, front), shaded by z
    # under the torso (between the fore and hind legs, below 18 cm) and the chest and chin bib (in front, up to the chin)
    belly = nt.nodes.new('ShaderNodeMapRange'); nt.links.new(sep.outputs['Z'], belly.inputs[0])
    belly.inputs['From Min'].default_value = 0.185; belly.inputs['From Max'].default_value = 0.155
    span = nt.nodes.new('ShaderNodeMapRange'); nt.links.new(sep.outputs['Y'], span.inputs[0])
    span.inputs['From Min'].default_value = -0.08; span.inputs['From Max'].default_value = 0.08; span.interpolation_type = 'SMOOTHERSTEP'
    span2 = nt.nodes.new('ShaderNodeMath'); span2.operation = 'PINGPONG'; span2.inputs[1].default_value = 0.5
    nt.links.new(span.outputs[0], span2.inputs[0])
    under = nt.nodes.new('ShaderNodeMath'); under.operation = 'MULTIPLY'; nt.links.new(belly.outputs[0], under.inputs[0])
    sp3 = nt.nodes.new('ShaderNodeMath'); sp3.operation = 'MULTIPLY'; sp3.inputs[1].default_value = 2.0; nt.links.new(span2.outputs[0], sp3.inputs[0])
    nt.links.new(sp3.outputs[0], under.inputs[1])
    bib = nt.nodes.new('ShaderNodeMapRange'); nt.links.new(sep.outputs['Y'], bib.inputs[0])
    bib.inputs['From Min'].default_value = -0.13; bib.inputs['From Max'].default_value = -0.17
    bibx = nt.nodes.new('ShaderNodeMath'); bibx.operation = 'ABSOLUTE'; nt.links.new(sep.outputs['X'], bibx.inputs[0])
    bibw = nt.nodes.new('ShaderNodeMapRange'); nt.links.new(bibx.outputs[0], bibw.inputs[0]); bibw.inputs['From Min'].default_value = 0.03; bibw.inputs['From Max'].default_value = 0.012
    bibz = nt.nodes.new('ShaderNodeMapRange'); nt.links.new(sep.outputs['Z'], bibz.inputs[0]); bibz.inputs['From Min'].default_value = 0.14; bibz.inputs['From Max'].default_value = 0.2
    b1 = nt.nodes.new('ShaderNodeMath'); b1.operation = 'MULTIPLY'; nt.links.new(bib.outputs[0], b1.inputs[0]); nt.links.new(bibw.outputs[0], b1.inputs[1])
    b2 = nt.nodes.new('ShaderNodeMath'); b2.operation = 'MULTIPLY'; nt.links.new(b1.outputs[0], b2.inputs[0]); nt.links.new(bibz.outputs[0], b2.inputs[1])
    lightm = nt.nodes.new('ShaderNodeMath'); lightm.operation = 'MAXIMUM'; nt.links.new(under.outputs[0], lightm.inputs[0]); nt.links.new(b2.outputs[0], lightm.inputs[1])
    belly = lightm
    mix2 = nt.nodes.new('ShaderNodeMixRGB'); nt.links.new(belly.outputs[0], mix2.inputs[0]); nt.links.new(mix.outputs[0], mix2.inputs[1]); mix2.inputs[2].default_value = (*LIGHT, 1)
    # a little salt and pepper in every coat (individual hairs differ)
    var = nt.nodes.new('ShaderNodeMixRGB'); var.blend_type = 'MULTIPLY'; var.inputs[0].default_value = 0.35
    nt.links.new(mix2.outputs[0], var.inputs[1]); nt.links.new(noise.outputs['Color'], var.inputs[2])
    return var.outputs[0]

skin_m = bpy.data.materials.new('CatSkin'); skin_m.use_nodes = True; nt = skin_m.node_tree; b = nt.nodes['Principled BSDF']
nt.links.new(coat_nodes(nt, False), b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.8
body.data.materials.append(skin_m)

fur_m = bpy.data.materials.new('CatFur'); fur_m.use_nodes = True; nt = fur_m.node_tree
for n in list(nt.nodes):
    if n.type == 'BSDF_PRINCIPLED': nt.nodes.remove(n)
hair = nt.nodes.new('ShaderNodeBsdfHairPrincipled'); hair.parametrization = 'COLOR'
hair.inputs['Roughness'].default_value = 0.32; hair.inputs['Radial Roughness'].default_value = 0.45; hair.inputs['Coat'].default_value = 0.15
# each hair a touch lighter at the tip (as a real cat's agouti hairs)
hi = nt.nodes.new('ShaderNodeHairInfo'); tip = nt.nodes.new('ShaderNodeMixRGB'); tip.blend_type = 'LIGHTEN'
cc = coat_nodes(nt, True)
tf = nt.nodes.new('ShaderNodeMath'); tf.operation = 'MULTIPLY'; tf.inputs[1].default_value = 0.12
nt.links.new(hi.outputs['Intercept'], tf.inputs[0]); nt.links.new(tf.outputs[0], tip.inputs[0])
nt.links.new(cc, tip.inputs[1]); tip.inputs[2].default_value = (*[min(1, c * 1.35 + 0.02) for c in BASE], 1)
nt.links.new(tip.outputs[0], hair.inputs['Color'])
nt.links.new(hair.outputs[0], nt.nodes['Material Output'].inputs['Surface'])
body.data.materials.append(fur_m)

# ---------------------------------------------------------------- the fur: strands, clumped
ps_mod = body.modifiers.new('Fur', 'PARTICLE_SYSTEM'); ps = ps_mod.particle_system; st = ps.settings
st.type = 'HAIR'; st.use_advanced_hair = True; st.hair_length = 0.016
st.count = 30000 if QUICK else 90000; st.hair_step = 5
st.child_type = 'INTERPOLATED'; st.child_nbr = 6; st.rendered_child_count = 8 if QUICK else 24
st.clump_factor = 0.35; st.clump_shape = -0.2; st.child_length = 1.0; st.child_length_threshold = 0.2
st.roughness_1 = 0.0015; st.roughness_1_size = 1.0; st.roughness_endpoint = 0.002; st.roughness_end_shape = 1.0
st.root_radius = 1.0; st.tip_radius = 0.0; st.radius_scale = 0.00055
st.material = 2  # CatFur
st.use_hair_bspline = True
ps.vertex_group_length = 'length'; ps.vertex_group_density = 'density'
# the hair lies down along the body: towards the tail, down the legs, back over the head
# (advanced hair: the length is about 4 x the start velocity, so these give ~15 mm hairs lying back along the body)
st.normal_factor = 0.0015; st.tangent_factor = 0.0; st.object_align_factor = (0.0, 0.0032, -0.0016)
# whiskers: a separate few long white hairs from the muzzle pads
wh = body.modifiers.new('Whiskers', 'PARTICLE_SYSTEM'); wps = wh.particle_system; ws = wps.settings
ws.type = 'HAIR'; ws.count = 24; ws.hair_length = 0.065; ws.hair_step = 6
ws.normal_factor = 0.3; ws.object_align_factor = (0.0, -0.01, 0.0); ws.root_radius = 1.0; ws.tip_radius = 0.0; ws.radius_scale = 0.00045
ws.roughness_1 = 0.004
ws.hair_length = 0.065
wm = bpy.data.materials.new('Whisker'); wm.use_nodes = True; wnt = wm.node_tree
for n in list(wnt.nodes):
    if n.type == 'BSDF_PRINCIPLED': wnt.nodes.remove(n)
whb = wnt.nodes.new('ShaderNodeBsdfHairPrincipled'); whb.parametrization = 'COLOR'; whb.inputs['Color'].default_value = (0.9, 0.88, 0.84, 1) if COAT != 'black' else (0.06, 0.06, 0.06, 1)
wnt.links.new(whb.outputs[0], wnt.nodes['Material Output'].inputs['Surface'])
body.data.materials.append(wm); ws.material = 3
wg = body.vertex_groups.new(name='whiskers')
for v in body.data.vertices:
    for s in (1, -1):
        if (v.co - (H + Vector((s * 0.016, -0.044, -0.014)))).length < 0.008 and v.co.x * s > 0: wg.add([v.index], 1.0, 'REPLACE')
wps.vertex_group_density = 'whiskers'

# ---------------------------------------------------------------- the stage: a white studio like the references
bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, 0)); floor = bpy.context.object
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.object.mode_set(mode='OBJECT')
bg = bpy.data.materials.new('Floor'); bg.use_nodes = True; bg.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.9, 0.9, 0.9, 1)
floor.data.materials.append(bg)
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.85, 0.86, 0.88, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.25
def light(name, loc, energy, size, rot):
    ld = bpy.data.lights.new(name, 'AREA'); ld.energy = energy; ld.size = size
    o = bpy.data.objects.new(name, ld); o.location = loc; o.rotation_euler = rot; sc.collection.objects.link(o)
light('Key', (0.6, -0.8, 0.9), 22, 0.8, (math.radians(50), 0, math.radians(35)))
light('Fill', (-0.8, -0.5, 0.5), 7, 1.2, (math.radians(65), 0, math.radians(-55)))
light('Rim', (0.0, 0.9, 0.7), 16, 0.6, (math.radians(-55), 0, math.radians(180)))

sc.render.engine = 'CYCLES'
prefs = bpy.context.preferences.addons['cycles'].preferences
for t in ('OPTIX', 'CUDA'):
    try:
        prefs.compute_device_type = t; prefs.get_devices()
        if any(d.type == t for d in prefs.devices): break
    except Exception: pass
for d in prefs.devices: d.use = True
sc.cycles.device = 'GPU'; sc.cycles.samples = 48 if QUICK else 220; sc.cycles.use_denoising = True
sc.render.resolution_x = 1200; sc.render.resolution_y = 1000
sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
sc.render.hair_type = 'STRIP'

cam_d = bpy.data.cameras.new('Cam'); cam_d.lens = 85
cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
target = Vector((0, -0.02, 0.17))
def shoot(name, az, el, dist):
    a, e = math.radians(az), math.radians(el)
    cam.location = target + Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e))) * dist
    cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = str(OUT / f'cat_{COAT}_{name}.png')
    bpy.ops.render.render(write_still=True); log('render ' + sc.render.filepath)
shoot('threequarter', -35, 12, 1.25)
shoot('side', -90, 6, 1.35)
if not QUICK:
    shoot('face', -12, 8, 0.55)
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / 'Cat.blend'))
log('DONE')
