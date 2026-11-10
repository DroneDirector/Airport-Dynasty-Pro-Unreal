# Apron high-mast light: cinematic concept renders (Blender 3.3, Cycles).
#   blender -b --python mast.py -- <outdir> [samples]
import bpy, bmesh, math, sys, os
from mathutils import Vector, Euler

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = argv[0]; SAMPLES = int(argv[1]) if len(argv) > 1 else 160
HERE = os.path.dirname(os.path.abspath(__file__))
H = 30.0           # mast height (m)
R0, R1 = 0.36, 0.17 # pole radius at the foot of the shaft and at the top

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, color, metal=0.0, rough=0.4, emit=None, strength=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    return m

def noisy(m, scale=40.0, amount=0.12):
    """Subtle weathering: noise into the roughness and colour."""
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = scale; n.inputs['Detail'].default_value = 8
    mr = nt.nodes.new('ShaderNodeMapRange'); mr.inputs['To Min'].default_value = b.inputs['Roughness'].default_value - amount
    mr.inputs['To Max'].default_value = b.inputs['Roughness'].default_value + amount
    nt.links.new(n.outputs['Fac'], mr.inputs['Value']); nt.links.new(mr.outputs['Result'], b.inputs['Roughness'])
    return m

GALV = noisy(mat('Galvanised', (0.80, 0.81, 0.82), metal=0.85, rough=0.36), 4, 0.06)
PAINT = noisy(mat('WhitePaint', (0.86, 0.87, 0.88), metal=0.0, rough=0.35), 60, 0.08)
ALU = mat('Aluminium', (0.70, 0.72, 0.74), metal=1.0, rough=0.34)
DARK = mat('DarkAlu', (0.12, 0.13, 0.14), metal=0.8, rough=0.45)
BOX = mat('BoxPlastic', (0.90, 0.90, 0.88), rough=0.45)
CONC = noisy(mat('Concrete', (0.42, 0.40, 0.37), rough=0.85), 6, 0.1)
APRON = noisy(mat('Apron', (0.30, 0.30, 0.31), rough=0.8), 3, 0.12)
YELLOW = mat('Marking', (0.75, 0.55, 0.06), rough=0.6)

# the LED face: the owner's photo, glowing
LED = bpy.data.materials.new('LEDFace'); LED.use_nodes = True
nt = LED.node_tree; b = nt.nodes['Principled BSDF']
uvn = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Rotation'].default_value = (0, 0, math.radians(90)); mp.inputs['Location'].default_value = (1, 0, 0)
nt.links.new(uvn.outputs['UV'], mp.inputs['Vector'])
tex = nt.nodes.new('ShaderNodeTexImage'); nt.links.new(mp.outputs['Vector'], tex.inputs['Vector']); tex.image = bpy.data.images.load(os.path.join(HERE, 'led_panel.png'))
nt.links.new(tex.outputs['Color'], b.inputs['Base Color'])
ramp = nt.nodes.new('ShaderNodeValToRGB'); ramp.color_ramp.elements[0].position = 0.55; ramp.color_ramp.elements[1].position = 0.9
nt.links.new(tex.outputs['Color'], ramp.inputs['Fac'])
mix = nt.nodes.new('ShaderNodeMixRGB'); mix.blend_type = 'MULTIPLY'; mix.inputs['Fac'].default_value = 1
mix.inputs['Color2'].default_value = (1.0, 0.95, 0.88, 1)
nt.links.new(ramp.outputs['Color'], mix.inputs['Color1']); nt.links.new(mix.outputs['Color'], b.inputs['Emission'])
b.inputs['Emission Strength'].default_value = 12.0; b.inputs['Metallic'].default_value = 0.6; b.inputs['Roughness'].default_value = 0.2

def link(o, m=None):
    sc.collection.objects.link(o)
    if m: o.data.materials.append(m)
    return o

def shade_smooth(o, angle=40):
    for p in o.data.polygons: p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(angle)

# ---- the ground: the apron with markings ----
bpy.ops.mesh.primitive_plane_add(size=6000); g = bpy.context.object; g.data.materials.append(APRON)
for x, w in ((-9, 0.25), (9, 0.25)):
    bpy.ops.mesh.primitive_plane_add(size=1, location=(x, 0, 0.005)); l = bpy.context.object
    l.scale = (w, 120, 1); l.data.materials.append(YELLOW)

# ---- the plinth ----
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.18)); p = bpy.context.object
p.scale = (2.7, 2.7, 0.36); p.data.materials.append(CONC)
bev = p.modifiers.new('b', 'BEVEL'); bev.width = 0.03; bev.segments = 3

# ---- the foot: a flared, fluted skirt (8 ribs) blending into the round shaft ----
def foot():
    bm = bmesh.new(); seg = 160; rings = 40; hgt = 3.2
    verts = []
    for r in range(rings + 1):
        t = r / rings; z = 0.36 + t * hgt
        flare = (1 - t) ** 2.2
        base_r = R0 + 0.42 * flare
        ring = []
        for s in range(seg):
            a = 2 * math.pi * s / seg
            rib = 0.12 * flare ** 0.8 * max(0.0, math.cos(4 * a)) ** 3     # 8 ribs, fading upward
            rr = base_r + rib
            ring.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        verts.append(ring)
    for r in range(rings):
        for s in range(seg):
            bm.faces.new((verts[r][s], verts[r][(s + 1) % seg], verts[r + 1][(s + 1) % seg], verts[r + 1][s]))
    bm.faces.new(list(reversed(verts[0])))
    me = bpy.data.meshes.new('Foot'); bm.to_mesh(me); o = bpy.data.objects.new('Foot', me); link(o, PAINT); shade_smooth(o, 60)
    return 0.36 + hgt
ztop_foot = foot()

# the shaft: a long taper to the head
def shaft(z0, z1):
    bpy.ops.mesh.primitive_cone_add(vertices=64, radius1=R0, radius2=R1, depth=z1 - z0, location=(0, 0, (z0 + z1) / 2), end_fill_type='NOTHING')
    o = bpy.context.object; o.data.materials.append(PAINT); shade_smooth(o, 60)
    # the vertical seam line and a little bright edge, like the photos' tall highlight
    return o
shaft(ztop_foot, H)

# the cable junction boxes at the foot (white, with conduits into the plinth)
for i, ang in enumerate((-45, -90, -135)):
    a = math.radians(ang); rr = 1.25
    x, y = rr * math.cos(a), rr * math.sin(a)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.62)); bx = bpy.context.object
    bx.scale = (0.18, 0.12, 0.26); bx.rotation_euler = (0, 0, a); bx.data.materials.append(BOX)
    bv = bx.modifiers.new('b', 'BEVEL'); bv.width = 0.015; bv.segments = 2
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.035, depth=0.2, location=(x, y, 0.42)); c = bpy.context.object
    c.data.materials.append(DARK)
    # a short cable curve up into the pole
    cu = bpy.data.curves.new('cable%d' % i, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = 0.018; cu.bevel_resolution = 4
    sp = cu.splines.new('BEZIER'); sp.bezier_points.add(1)
    p0, p1 = sp.bezier_points
    p0.co = (x * 0.95, y * 0.95, 0.75); p0.handle_left = p0.co; p0.handle_right = (x * 0.9, y * 0.9, 1.0)
    p1.co = (x * 0.55, y * 0.55, 0.9); p1.handle_left = (x * 0.7, y * 0.7, 1.05); p1.handle_right = p1.co
    co = bpy.data.objects.new('cable%d' % i, cu); link(co, DARK)

# ---- the head: a ring on spokes with seven floodlights ----
HZ = H - 0.6
bpy.ops.mesh.primitive_torus_add(major_radius=1.75, minor_radius=0.06, major_segments=96, minor_segments=16, location=(0, 0, HZ))
ring = bpy.context.object; ring.data.materials.append(GALV)
bpy.ops.mesh.primitive_torus_add(major_radius=1.65, minor_radius=0.035, major_segments=96, minor_segments=12, location=(0, 0, HZ - 0.12))
link_ring2 = bpy.context.object; link_ring2.data.materials.append(GALV)
bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.32, depth=0.5, location=(0, 0, HZ)); hub = bpy.context.object; hub.data.materials.append(GALV)
bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=R1 * 1.1, depth=0.5, location=(0, 0, HZ + 0.4)); top = bpy.context.object; top.data.materials.append(GALV)
RED = mat('ObstructionRed', (0.6, 0.02, 0.02), rough=0.3, emit=(1.0, 0.05, 0.02), strength=8.0)
BOLT = mat('Bolt', (0.55, 0.56, 0.58), metal=1.0, rough=0.25)
RING_R = 1.75

def part(kind, parent, loc, scale=(1, 1, 1), rot=(0, 0, 0), m=GALV, bevel=0.0, **kw):
    if kind == 'cube': bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    elif kind == 'cyl': bpy.ops.mesh.primitive_cylinder_add(vertices=kw.get('v', 24), radius=kw['r'], depth=kw['d'], location=loc)
    elif kind == 'torus': bpy.ops.mesh.primitive_torus_add(major_radius=kw['R'], minor_radius=kw['r'], major_segments=32, minor_segments=8, location=loc)
    o = bpy.context.object; o.scale = scale; o.rotation_euler = rot; o.data.materials.append(m)
    if bevel: b = o.modifiers.new('b', 'BEVEL'); b.width = bevel; b.segments = 2
    if parent: o.parent = parent
    return o

# struts between the two ring tubes, and the eight spokes
for k in range(16):
    a = 2 * math.pi * k / 16
    part('cyl', None, (1.70 * math.cos(a), 1.70 * math.sin(a), HZ - 0.06), r=0.02, d=0.14, m=GALV)
for k in range(8):
    a = 2 * math.pi * (k + 0.5) / 8
    part('cube', None, (0.95 * math.cos(a), 0.95 * math.sin(a), HZ), scale=(1.35, 0.07, 0.12), rot=(0, 0, a), bevel=0.01)
# the head's crown: a cap, the lightning rod and the red obstruction light
part('cyl', None, (0, 0, HZ + 0.75), r=0.24, d=0.12, bevel=0.01)
part('cyl', None, (0, 0, HZ + 1.6), r=0.012, d=1.6, m=BOLT)
part('cyl', None, (0.18, 0, HZ + 0.95), r=0.07, d=0.25, m=DARK)
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.075, location=(0.18, 0, HZ + 1.12)); rl = bpy.context.object; rl.data.materials.append(RED)
pl = bpy.data.lights.new('Obstruction', 'POINT'); pl.energy = 0.8; pl.color = (1, 0.1, 0.05); pl.shadow_soft_size = 0.05
po = bpy.data.objects.new('Obstruction', pl); sc.collection.objects.link(po); po.location = (0.18, 0, HZ + 1.25)

def floodlight(angle_deg, tilt_deg, zoff):
    """One LED floodlight hung from the ring: a clamp round the tube with two U-bolts and a driver box, a square arm
    to a mounting plate, a U-yoke whose arms hold the lamp on pivot bolts (tilted), a cable back to the ring."""
    a = math.radians(angle_deg)
    root = bpy.data.objects.new('Mount%d' % angle_deg, None); sc.collection.objects.link(root)
    root.location = (RING_R * math.cos(a), RING_R * math.sin(a), HZ)
    root.rotation_euler = (0, 0, a)               # local x outward, y along the ring, z up
    # the clamp round the ring tube (the tube runs along local y)
    part('cube', root, (0, 0, 0), scale=(0.2, 0.22, 0.2), m=GALV, bevel=0.012)
    for yy in (-0.085, 0.085):
        part('torus', root, (0, yy, 0), rot=(math.radians(90), 0, 0), R=0.105, r=0.013, m=BOLT)
        for zz in (-0.06, 0.06):
            part('cyl', root, (-0.13, yy, zz), rot=(0, math.radians(90), 0), r=0.016, d=0.06, v=6, m=BOLT)   # nuts
    part('cube', root, (0, 0, 0.17), scale=(0.18, 0.16, 0.12), m=ALU, bevel=0.01)                            # driver box
    # the arm out to the mounting plate
    part('cube', root, (0.36, 0, zoff * 0.5), scale=(0.52, 0.09, 0.09), m=GALV, bevel=0.008)
    part('cube', root, (0.62, 0, zoff * 0.5), scale=(0.03, 0.16, 0.2), m=GALV, bevel=0.005)
    W, Hh, D = 1.2, 0.69, 0.16
    px = 1.08; pz = zoff * 0.5
    # the U-yoke: a bar across, two arms forward to the pivots
    part('cube', root, (0.66, 0, pz), scale=(0.06, W + 0.2, 0.08), m=GALV, bevel=0.006)
    for s in (-1, 1):
        part('cube', root, ((0.66 + px) / 2, s * (W / 2 + 0.07), pz), scale=(px - 0.66, 0.035, 0.09), m=GALV, bevel=0.006)
        part('cyl', root, (px, s * (W / 2 + 0.07), pz), rot=(math.radians(90), 0, 0), r=0.08, d=0.04, m=GALV)   # pivot disc
        part('cyl', root, (px, s * (W / 2 + 0.105), pz), rot=(math.radians(90), 0, 0), r=0.025, d=0.04, v=6, m=BOLT)  # pivot bolt
        # the tilt quadrant with its locking bolt
        part('cube', root, (px - 0.1, s * (W / 2 + 0.07), pz + 0.09), scale=(0.12, 0.012, 0.06), rot=(0, math.radians(-30), 0), m=GALV)
    # the lamp, turned on the pivots
    lamp = bpy.data.objects.new('Lamp%d' % angle_deg, None); sc.collection.objects.link(lamp); lamp.parent = root
    lamp.location = (px, 0, pz); lamp.rotation_euler = (0, math.radians(tilt_deg), 0)
    part('cube', lamp, (0, 0, 0), scale=(D, W, Hh), m=ALU, bevel=0.02)
    for s in (-1, 1):
        part('cyl', lamp, (0, s * (W / 2 + 0.02), 0), rot=(math.radians(90), 0, 0), r=0.05, d=0.05, m=GALV)    # trunnions
    # the front: a dark frame and the LED face
    part('cube', lamp, (D / 2 + 0.004, 0, 0), scale=(0.01, W * 0.98, Hh * 0.95), m=DARK)
    bpy.ops.mesh.primitive_plane_add(size=1, location=(D / 2 + 0.011, 0, 0)); f = bpy.context.object
    f.scale = (Hh * 0.86, W * 0.92, 1); f.rotation_euler = (0, math.radians(90), 0); f.data.materials.append(LED); f.parent = lamp
    for i in range(16):
        part('cube', lamp, (-D / 2 - 0.05, -W / 2 + 0.05 + i * (W - 0.1) / 15, 0), scale=(0.1, 0.012, Hh * 0.92), m=ALU)
    part('cyl', lamp, (-D / 2 - 0.02, W / 2 - 0.12, -Hh / 2 + 0.06), rot=(0, math.radians(90), 0), r=0.03, d=0.06, m=DARK)   # cable gland
    # the cable from the gland back to the driver box
    cu = bpy.data.curves.new('c%d' % angle_deg, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = 0.014; cu.bevel_resolution = 3
    sp = cu.splines.new('BEZIER'); sp.bezier_points.add(1); p0, p1 = sp.bezier_points
    t = math.radians(tilt_deg); gx, gz = -D / 2 - 0.05, -Hh / 2 + 0.06
    wx = px + gx * math.cos(t) + gz * math.sin(t); wz = pz - gx * math.sin(t) + gz * math.cos(t)
    p0.co = (wx, W / 2 - 0.12, wz); p0.handle_left = p0.co; p0.handle_right = (wx - 0.3, W / 2 - 0.1, wz - 0.25)
    p1.co = (0.05, 0.05, 0.15); p1.handle_left = (0.3, 0.2, -0.2); p1.handle_right = p1.co
    co = bpy.data.objects.new('c%d' % angle_deg, cu); sc.collection.objects.link(co); co.data.materials.append(DARK); co.parent = root
    # the light: a spot down the face's direction and a soft area light on the face
    ld = bpy.data.lights.new('L%d' % angle_deg, 'SPOT'); ld.energy = 26000; ld.spot_size = math.radians(70); ld.spot_blend = 0.6
    ld.shadow_soft_size = 0.35; ld.color = (1.0, 0.95, 0.88)
    lo = bpy.data.objects.new('L%d' % angle_deg, ld); sc.collection.objects.link(lo); lo.parent = lamp
    lo.location = (D / 2 + 0.06, 0, 0); lo.rotation_euler = (0, math.radians(-90), 0)
    al = bpy.data.lights.new('A%d' % angle_deg, 'AREA'); al.energy = 300; al.shape = 'RECTANGLE'; al.size = Hh * 0.85; al.size_y = W * 0.9
    ao = bpy.data.objects.new('A%d' % angle_deg, al); sc.collection.objects.link(ao); ao.parent = lamp
    ao.location = (D / 2 + 0.03, 0, 0); ao.rotation_euler = (0, math.radians(-90), 0)

# eight, every 45 degrees all the way round, a little higher and lower in turn
for k in range(8):
    floodlight(k * 45 + 22.5, 48 if k % 2 else 40, 0.12 if k % 2 else -0.12)

# fix the LED faces' UVs: a plane's default UVs already span 0..1, the photo is 2:1 like the face

# ---- the tileable galvanised metal on every steel part ----
ART = r'C:\Users\Simon\Documents\Airport-Dynasty-Pro-Unreal\Art\LightMast'
def metal_material(name, tint=(1, 1, 1), scale=0.6, stripes=False):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (scale, scale, scale)
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector'])
    def img(file, non_color=False):
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(ART, file))
        if non_color: t.image.colorspace_settings.name = 'Non-Color'
        t.projection = 'BOX'; t.projection_blend = 0.25; nt.links.new(mp.outputs['Vector'], t.inputs['Vector']); return t
    col = img('metal_basecolor.png'); rough = img('metal_roughness.png', True); nrm = img('metal_normal.png', True)
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6
    nt.links.new(nrm.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
    tintn = nt.nodes.new('ShaderNodeMixRGB'); tintn.blend_type = 'MULTIPLY'; tintn.inputs['Fac'].default_value = 1
    tintn.inputs['Color2'].default_value = (*tint, 1); nt.links.new(col.outputs['Color'], tintn.inputs['Color1'])
    b.inputs['Metallic'].default_value = 0.9
    color_out, rough_out, metal_val = tintn.outputs['Color'], rough.outputs['Color'], None
    if stripes:
        # painted bands along the rail (UV x = metres along it): 6 m panels, every other one striped yellow / black,
        # 0.25 m stripes standing upright and wrapping the W profile
        uv = nt.nodes.new('ShaderNodeUVMap'); sep = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(uv.outputs['UV'], sep.inputs['Vector'])
        def math_node(op, a, bval=None):
            n = nt.nodes.new('ShaderNodeMath'); n.operation = op
            nt.links.new(a, n.inputs[0]) if not isinstance(a, float) else None
            if bval is not None: n.inputs[1].default_value = bval
            return n
        panel = math_node('MODULO', sep.outputs['X'], 3.0); striped = math_node('LESS_THAN', panel.outputs[0], 1.5)
        band = math_node('MODULO', sep.outputs['X'], 0.5); yellow = math_node('LESS_THAN', band.outputs[0], 0.25)
        paint_col = nt.nodes.new('ShaderNodeMixRGB'); paint_col.inputs['Color1'].default_value = (0.02, 0.02, 0.02, 1)
        paint_col.inputs['Color2'].default_value = (0.85, 0.62, 0.02, 1); nt.links.new(yellow.outputs[0], paint_col.inputs['Fac'])
        mixc = nt.nodes.new('ShaderNodeMixRGB'); nt.links.new(striped.outputs[0], mixc.inputs['Fac'])
        nt.links.new(tintn.outputs['Color'], mixc.inputs['Color1']); nt.links.new(paint_col.outputs['Color'], mixc.inputs['Color2'])
        color_out = mixc.outputs['Color']
        # paint is not metal, and a little rougher
        mmet = nt.nodes.new('ShaderNodeMapRange'); nt.links.new(striped.outputs[0], mmet.inputs['Value'])
        mmet.inputs['To Min'].default_value = 0.9; mmet.inputs['To Max'].default_value = 0.0
        nt.links.new(mmet.outputs['Result'], b.inputs['Metallic'])
        mr = nt.nodes.new('ShaderNodeMixRGB'); nt.links.new(striped.outputs[0], mr.inputs['Fac'])
        nt.links.new(rough.outputs['Color'], mr.inputs['Color1']); mr.inputs['Color2'].default_value = (0.5, 0.5, 0.5, 1)
        rough_out = mr.outputs['Color']
    nt.links.new(color_out, b.inputs['Base Color']); nt.links.new(rough_out, b.inputs['Roughness'])
    return m

METAL = metal_material('GalvanisedSteel')
RAIL = metal_material('GuardrailSteel', stripes=True, scale=1.0)
for o in list(sc.objects):
    if o.type in ('MESH', 'CURVE') and o.data.materials:
        for i, mm in enumerate(o.data.materials):
            if mm and mm.name in ('WhitePaint', 'Galvanised'): o.data.materials[i] = METAL

def hexnut(loc, r=0.03, h=0.03, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=r, depth=h, location=loc, rotation=rot); n = bpy.context.object; n.data.materials.append(BOLT)
    bv = n.modifiers.new('b', 'BEVEL'); bv.width = r * 0.12; bv.segments = 1
    return n

# ---- the base: a flange plate on anchor bolts (double nuts, washers), an earthing strap ----
bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=1.12, depth=0.05, location=(0, 0, 0.385)); fl = bpy.context.object; fl.data.materials.append(METAL)
bv = fl.modifiers.new('b', 'BEVEL'); bv.width = 0.01; bv.segments = 2
for k in range(12):
    a = 2 * math.pi * (k + 0.5) / 12; x, y = 1.0 * math.cos(a), 1.0 * math.sin(a)
    if 0.78 + 0.12 * max(0.0, math.cos(4 * a)) ** 3 > 0.93: continue
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.022, depth=0.2, location=(x, y, 0.44)); rod = bpy.context.object; rod.data.materials.append(BOLT)
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.05, depth=0.008, location=(x, y, 0.414)); w_ = bpy.context.object; w_.data.materials.append(BOLT)
    hexnut((x, y, 0.435), 0.038, 0.035); hexnut((x, y, 0.475), 0.038, 0.035)
bpy.ops.mesh.primitive_cube_add(size=1, location=(0.0, -1.16, 0.37)); es = bpy.context.object
es.scale = (0.03, 0.12, 0.006); es.rotation_euler = (math.radians(-35), 0, 0); es.data.materials.append(mat('Copper', (0.72, 0.42, 0.25), metal=1.0, rough=0.35))

# ---- the shaft: two bolted joint collars (where the sections meet) and a bolted access hatch ----
def r_at(z): return R0 + (R1 - R0) * (z - ztop_foot) / (H - ztop_foot)
for zc in (12.0, 21.0):
    rr = r_at(zc)
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=rr + 0.025, depth=0.16, location=(0, 0, zc)); cl = bpy.context.object; cl.data.materials.append(METAL)
    bv = cl.modifiers.new('b', 'BEVEL'); bv.width = 0.01; bv.segments = 2
    for k in range(10):
        a = 2 * math.pi * k / 10
        hexnut(((rr + 0.035) * math.cos(a), (rr + 0.035) * math.sin(a), zc), 0.016, 0.02, rot=(0, math.radians(90), a))
zh = ztop_foot + 0.9; rh = r_at(zh)
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -(rh + 0.012), zh)); hp = bpy.context.object
hp.scale = (0.26, 0.02, 0.7); hp.data.materials.append(METAL); bv = hp.modifiers.new('b', 'BEVEL'); bv.width = 0.01; bv.segments = 2
for dx in (-0.1, 0.1):
    for dz in (-0.3, 0.0, 0.3):
        hexnut((dx, -(rh + 0.03), zh + dz), 0.012, 0.015, rot=(math.radians(90), 0, 0))

# ---- the head: a flange where it sits on the shaft, with bolts; bolts at the spoke ends ----
bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=0.42, depth=0.04, location=(0, 0, HZ - 0.27)); hf = bpy.context.object; hf.data.materials.append(METAL)
for k in range(12):
    a = 2 * math.pi * k / 12; hexnut((0.36 * math.cos(a), 0.36 * math.sin(a), HZ - 0.24), 0.018, 0.025)
for k in range(8):
    a = 2 * math.pi * (k + 0.5) / 8
    for rr in (0.4, 1.55):
        hexnut((rr * math.cos(a), rr * math.sin(a), HZ + 0.075), 0.016, 0.02)

# ---- the guardrail round the base: W-beam on posts with spacer blocks, button-head bolts ----
def w_profile():
    """The W-beam's cross-section (depth y outward, height z), 310 mm high, 85 mm deep, 3 mm sheet."""
    pts = [(0.0, -0.155), (0.02, -0.15), (0.075, -0.1), (0.085, -0.07), (0.075, -0.04), (0.03, 0.0),
           (0.075, 0.04), (0.085, 0.07), (0.075, 0.1), (0.02, 0.15), (0.0, 0.155)]
    out = [(y, z) for y, z in pts]; back = [(y - 0.004, z) for y, z in reversed(pts)]
    return out + back

def rounded_square(half, rad, step=0.08):
    path = []
    corners = [(half - rad, half - rad, 0), (-(half - rad), half - rad, 90), (-(half - rad), -(half - rad), 180), (half - rad, -(half - rad), 270)]
    for i, (cx, cy, a0) in enumerate(corners):
        nx, ny, _ = corners[(i + 1) % 4]
        for k in range(10):
            a = math.radians(a0 + 90 * k / 9); path.append((cx + rad * math.cos(a), cy + rad * math.sin(a)))
        # the straight run to the next corner
        sx, sy = cx + rad * math.cos(math.radians(a0 + 90)), cy + rad * math.sin(math.radians(a0 + 90))
        ex, ey = nx + rad * math.cos(math.radians(a0 + 90)), ny + rad * math.sin(math.radians(a0 + 90))
        L = math.hypot(ex - sx, ey - sy); n = max(2, int(L / step))
        for k in range(1, n): path.append((sx + (ex - sx) * k / n, sy + (ey - sy) * k / n))
    return path

_before_guard = set(sc.objects)
GR_HALF, GR_RAD, GR_Z = 3.2, 0.9, 0.62
path = rounded_square(GR_HALF, GR_RAD)
prof = w_profile()
bm = bmesh.new(); uvl = bm.loops.layers.uv.new('UVMap')
rings = []; dist = [0.0]
for i in range(len(path)):
    if i: dist.append(dist[-1] + math.hypot(path[i][0] - path[i - 1][0], path[i][1] - path[i - 1][1]))
dist.append(dist[-1] + math.hypot(path[0][0] - path[-1][0], path[0][1] - path[-1][1]))
for i, (px, py) in enumerate(path + [path[0]]):
    j = i % len(path)
    ox, oy = px, py; L = math.hypot(ox, oy); ox, oy = ox / L, oy / L       # outward from the mast
    # outward normal of the path (perpendicular to its direction)
    qx, qy = path[(j + 1) % len(path)]; rx, ry = path[j - 1]
    tx, ty = qx - rx, qy - ry; tl = math.hypot(tx, ty); tx, ty = tx / tl, ty / tl
    nx, ny = ty, -tx
    if nx * ox + ny * oy < 0: nx, ny = -nx, -ny
    rings.append([bm.verts.new((px + nx * y, py + ny * y, GR_Z + z)) for y, z in prof])
plen = [0.0]
for k in range(1, len(prof)): plen.append(plen[-1] + math.hypot(prof[k][0] - prof[k - 1][0], prof[k][1] - prof[k - 1][1]))
for i in range(len(rings) - 1):
    for k in range(len(prof) - 1):
        f = bm.faces.new((rings[i][k], rings[i + 1][k], rings[i + 1][k + 1], rings[i][k + 1]))
        for loop, (ii, kk) in zip(f.loops, ((i, k), (i + 1, k), (i + 1, k + 1), (i, k + 1))):
            loop[uvl].uv = (dist[ii], plen[kk])
me = bpy.data.meshes.new('Guardrail'); bm.to_mesh(me); rail = bpy.data.objects.new('Guardrail', me); link(rail, RAIL); shade_smooth(rail, 50)

# posts every ~1.6 m along the straight runs, with spacer blocks and two bolts each
POSTMAT = METAL
for i in range(4):
    a = math.radians(90 * i)
    for t in (-1.6, 0.0, 1.6):
        # a point on side i: the side faces outward along (cos a, sin a)
        cx, cy = GR_HALF * math.cos(a) - t * math.sin(a), GR_HALF * math.sin(a) + t * math.cos(a)
        inx, iny = -math.cos(a), -math.sin(a)
        px_, py_ = cx + inx * 0.25, cy + iny * 0.25
        bpy.ops.mesh.primitive_cube_add(size=1, location=(px_, py_, 0.4)); po = bpy.context.object
        po.scale = (0.15, 0.1, 0.8); po.rotation_euler = (0, 0, a); po.data.materials.append(POSTMAT)
        bv = po.modifiers.new('b', 'BEVEL'); bv.width = 0.008; bv.segments = 1
        bpy.ops.mesh.primitive_cube_add(size=1, location=(cx + inx * 0.1, cy + iny * 0.1, GR_Z)); sp_ = bpy.context.object
        sp_.scale = (0.2, 0.14, 0.33); sp_.rotation_euler = (0, 0, a); sp_.data.materials.append(POSTMAT)
        for dz in (-0.07, 0.07):
            bpy.ops.mesh.primitive_uv_sphere_add(radius=0.022, segments=12, ring_count=6,
                location=(cx + math.cos(a) * 0.035, cy + math.sin(a) * 0.035, GR_Z + dz)); bh = bpy.context.object
            bh.scale = (1, 1, 0.6); bh.rotation_euler = (0, math.radians(90), a); bh.data.materials.append(BOLT)
        # base plate with four anchor bolts
        bpy.ops.mesh.primitive_cube_add(size=1, location=(px_, py_, 0.012)); bp_ = bpy.context.object
        bp_.scale = (0.26, 0.22, 0.024); bp_.rotation_euler = (0, 0, a); bp_.data.materials.append(POSTMAT)
        for ux in (-0.09, 0.09):
            for uy in (-0.07, 0.07):
                hexnut((px_ + ux * math.cos(a) - uy * math.sin(a), py_ + ux * math.sin(a) + uy * math.cos(a), 0.035), 0.016, 0.02)

GUARD_OBJS = set(sc.objects) - _before_guard

# ---- EXPORT=1: the whole mast (guardrail included) as one mesh for Unreal, its material slots named for the game ----
if os.environ.get('EXPORT') == '1':
    for o in list(sc.objects):
        if o.type == 'LIGHT' or o == g or (o.type == 'MESH' and o.data.materials and o.data.materials[0].name == 'Marking'):
            bpy.data.objects.remove(o, do_unlink=True)
    # the LED faces' picture turned as in the renders (the mapping baked into their UVs)
    for o in sc.objects:
        if o.type == 'MESH' and o.data.materials and o.data.materials[0].name == 'LEDFace':
            for l in o.data.uv_layers[0].data: u, v = l.uv; l.uv = (1 - v, u)
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    meshes = []
    for o in list(sc.objects):
        if o.type not in ('MESH', 'CURVE') or o.hide_render: continue
        me = bpy.data.meshes.new_from_object(o.evaluated_get(deps))     # modifiers and curve bevels applied
        me.transform(o.matrix_world)
        if not me.uv_layers: me.uv_layers.new(name='UVMap')
        else: me.uv_layers[0].name = 'UVMap'
        mname = me.materials[0].name if len(me.materials) else ''
        if mname not in ('GuardrailSteel', 'LEDFace'):
            # box projection, as the renders' materials did: each face mapped from its main direction, 1.67 m a tile
            uvd = me.uv_layers[0].data
            for poly in me.polygons:
                nx, ny, nz = (abs(c) for c in poly.normal)
                for li in poly.loop_indices:
                    co = me.vertices[me.loops[li].vertex_index].co
                    if nz >= nx and nz >= ny: uvd[li].uv = (co.x * 0.6, co.y * 0.6)
                    elif nx >= ny: uvd[li].uv = (co.y * 0.6, co.z * 0.6)
                    else: uvd[li].uv = (co.x * 0.6, co.z * 0.6)
        no = bpy.data.objects.new(o.name + '_x', me); sc.collection.objects.link(no); meshes.append(no)
        # thin parts (cables, the lightning rod, U-bolts, long thin rods) go without Nanite: they'd thin out oddly
        d = sorted(o.dimensions)
        no['thin'] = o.type == 'CURVE' or o.name.startswith('Torus') or (d[1] < 0.04 and d[2] > 0.5)
    for o in list(sc.objects):
        if o not in meshes: bpy.data.objects.remove(o, do_unlink=True)
    def join(objs, name):
        bpy.ops.object.select_all(action='DESELECT')
        for o in objs: o.select_set(True)
        bpy.context.view_layer.objects.active = objs[0]
        bpy.ops.object.join()
        j = bpy.context.object; j.name = name; j.data.name = name; return j
    thin_objs = [o for o in meshes if o['thin']]; main_objs = [o for o in meshes if not o['thin']]
    thin = join(thin_objs, 'LightMastThin')
    lm = join(main_objs, 'LightMast')
    print('THIN', len(thin.data.polygons), 'faces', flush=True)
    for slot in thin.material_slots:
        if slot.material and not slot.material.name.startswith('MI_LM_'):
            slot.material.name = 'MI_LM_' + {'GalvanisedSteel': 'Galvanised', 'Galvanised': 'Galvanised', 'Bolt': 'Bolt', 'DarkAlu': 'DarkMetal'}.get(slot.material.name.split('.')[0], slot.material.name)
    names = {'GalvanisedSteel': 'Galvanised', 'Galvanised': 'Galvanised', 'WhitePaint': 'Galvanised', 'GuardrailSteel': 'Guardrail',
             'Bolt': 'Bolt', 'Aluminium': 'Aluminium', 'DarkAlu': 'DarkMetal', 'BoxPlastic': 'Plastic', 'Concrete': 'Concrete',
             'Copper': 'Copper', 'LEDFace': 'LEDFace', 'ObstructionRed': 'ObstructionLight'}
    for slot in lm.material_slots:
        if slot.material and not slot.material.name.startswith('MI_LM_'): slot.material.name = 'MI_LM_' + names.get(slot.material.name.split('.')[0], slot.material.name)
    # CHECK (owner's rule): every mesh's material slots filled before export, or stop
    for ob in (lm, thin):
        empty = [i for i, sl in enumerate(ob.material_slots) if not sl.material]
        if empty or not ob.material_slots: raise SystemExit('EXPORT STOPPED: %s has empty material slots %s' % (ob.name, empty))
        print('CHECK %s: %d material slots, none empty' % (ob.name, len(ob.material_slots)), flush=True)
    out = os.path.join(ART, 'lightmast.glb')
    bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_apply=True, export_yup=True, use_selection=False)
    print('EXPORTED', out, len(lm.data.polygons), 'faces', [s.material.name for s in lm.material_slots if s.material], flush=True)
    import sys as _s; _s.exit(0)
# ---- the world: blue hour sky, haze ----
w = bpy.data.worlds.new('Dusk'); sc.world = w; w.use_nodes = True
wn = w.node_tree; bg = wn.nodes['Background']
tcd = wn.nodes.new('ShaderNodeTexCoord'); sep = wn.nodes.new('ShaderNodeSeparateXYZ')
wn.links.new(tcd.outputs['Generated'], sep.inputs['Vector'])
cr = wn.nodes.new('ShaderNodeValToRGB'); e = cr.color_ramp.elements
e[0].position = 0.0; e[0].color = (0.035, 0.11, 0.42, 1)    # the horizon: lighter blue
e[1].position = 0.6; e[1].color = (0.006, 0.025, 0.14, 1)  # overhead: deep blue
e2 = e.new(-0.2); e2.color = (0.02, 0.05, 0.18, 1)
mrz = wn.nodes.new('ShaderNodeMapRange'); mrz.inputs['From Min'].default_value = -1; mrz.inputs['From Max'].default_value = 1
mrz.inputs['To Min'].default_value = -1; mrz.inputs['To Max'].default_value = 1
wn.links.new(sep.outputs['Z'], mrz.inputs['Value']); wn.links.new(mrz.outputs['Result'], cr.inputs['Fac'])
wn.links.new(cr.outputs['Color'], bg.inputs['Color'])
bg.inputs['Strength'].default_value = 0.55
# blue-hour fill (the sky Nishita is very dark below the horizon)
bpy.ops.object.light_add(type='SUN', rotation=(math.radians(70), 0, math.radians(30))); sun = bpy.context.object
sun.data.energy = 0.35; sun.data.color = (0.8, 0.84, 0.92); sun.data.angle = math.radians(20)

# haze for the light beams: a volume box round the mast
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 40)); vol = bpy.context.object; vol.scale = (260, 260, 100)
vm = bpy.data.materials.new('Haze'); vm.use_nodes = True; vn = vm.node_tree
vn.nodes.remove(vn.nodes['Principled BSDF'])
pv = vn.nodes.new('ShaderNodeVolumePrincipled'); pv.inputs['Density'].default_value = 0.0022; pv.inputs['Anisotropy'].default_value = 0.55
pv.inputs['Color'].default_value = (0.92, 0.92, 0.92, 1)
vn.links.new(pv.outputs['Volume'], vn.nodes['Material Output'].inputs['Volume']); vol.data.materials.append(vm)
vol.visible_shadow = False

# ---- render settings ----
sc.render.engine = 'CYCLES'
prefs = bpy.context.preferences.addons['cycles'].preferences
for dev_type in ('OPTIX', 'CUDA'):
    try:
        prefs.compute_device_type = dev_type; prefs.get_devices()
        if any(d.type == dev_type for d in prefs.devices):
            for d in prefs.devices: d.use = d.type == dev_type
            sc.cycles.device = 'GPU'; break
    except Exception: pass
sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
sc.cycles.volume_step_rate = 4.0; sc.cycles.max_bounces = 6
sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'; sc.view_settings.exposure = 0.0

if os.environ.get('DAY') == '1':
    for o in sc.objects:
        if o.type == 'LIGHT' and o.data.type in ('SPOT', 'AREA'): o.hide_render = True
    vol.hide_render = True
    LED.node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value = 0.0
    sun.data.energy = 3.5; sun.data.color = (1.0, 0.97, 0.92); sun.data.angle = math.radians(3)
    sun.rotation_euler = (math.radians(50), 0, math.radians(35))
    bg.inputs['Strength'].default_value = 1.0
    e[0].color = (0.55, 0.62, 0.72, 1); e[1].color = (0.3, 0.42, 0.62, 1); e2.color = (0.35, 0.36, 0.38, 1)
cam_data = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_data); sc.collection.objects.link(cam); sc.camera = cam
target = bpy.data.objects.new('Target', None); sc.collection.objects.link(target)
tc = cam.constraints.new('TRACK_TO'); tc.target = target; tc.track_axis = 'TRACK_NEGATIVE_Z'; tc.up_axis = 'UP_Y'

SHOTS = [
    # name, camera position, look at, lens (mm), portrait?
    ('01_wide_blue_hour', (34, -52, 3.0), (0, 0, 15), 32, False),
    ('02_looking_up_from_base', (2.6, -2.4, 1.1), (0, 0, H - 1), 16, True),
    ('03_base_closeup', (2.6, -4.6, 1.5), (0, -0.4, 0.8), 30, False),
    ('04_lamp_head_closeup', (5.5, -7.5, H + 1.5), (0, 0, HZ), 40, False),
    ('05_floodlight_detail', (4.3, 2.6, HZ - 1.4), (2.3, 1.1, HZ), 38, False),
    ('06_top_down', (0.01, 0.01, H + 22), (0, 0, 0), 28, False),
    ('07_full_mast_portrait', (16, -22, 6), (0, 0, 14), 28, True),
]
only = argv[2].split(',') if len(argv) > 2 else None
for name, pos, look, lens, portrait in ([] if os.environ.get('GUARD') == '1' else SHOTS):
    if only and name[:2] not in only: continue
    cam.location = pos; target.location = look; cam_data.lens = lens; cam_data.clip_end = 2000
    sc.render.resolution_x, sc.render.resolution_y = (1080, 1920) if portrait else (1920, 1080)
    sc.render.resolution_percentage = int(os.environ.get('PCT', '100'))
    sc.render.filepath = os.path.join(OUT, name.replace('_blue_hour', '') + ('_day' if os.environ.get('DAY') == '1' else '') + '.png')
    bpy.ops.render.render(write_still=True)
    print('RENDERED', name, flush=True)

# ---- the guardrail alone (GUARD=1): daylight, everything else hidden, four straight-on views ----
if os.environ.get('GUARD') == '1':
    keep = GUARD_OBJS | {g}
    for o in sc.objects:
        if o.type in ('MESH', 'CURVE') and o not in keep: o.hide_render = True
        if o.type == 'LIGHT' and o.name != sun.name: o.hide_render = True
    vol.hide_render = True
    sun.data.energy = 3.5; sun.data.color = (1.0, 0.97, 0.92); sun.data.angle = math.radians(3)
    sun.rotation_euler = (math.radians(50), 0, math.radians(35))
    bg.inputs['Strength'].default_value = 1.0
    e[0].color = (0.55, 0.62, 0.72, 1); e[1].color = (0.3, 0.42, 0.62, 1); e2.color = (0.35, 0.36, 0.38, 1)
    sc.view_settings.exposure = 0.0
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    sc.render.resolution_percentage = int(os.environ.get('PCT', '100'))
    cam_data.sensor_fit = 'HORIZONTAL'; cam_data.sensor_width = 36; cam_data.lens = 50
    face = GR_HALF + 0.09
    views = []
    for name, ang, t in (('guardrail_side_1', 0, 0.8), ('guardrail_side_2', 90, -0.8), ('guardrail_side_3', 180, 0.8), ('guardrail_side_4', 270, -0.8)):
        a = math.radians(ang); nx, ny = math.cos(a), math.sin(a); tx, ty = -ny, nx
        px, py = face * nx + t * tx, face * ny + t * ty
        views.append((name, (px + 2.6 * nx, py + 2.6 * ny, 0.45), (px, py, 0.42)))
    c = (GR_HALF - GR_RAD) + (GR_RAD + 0.09) * math.cos(math.radians(45))
    views.append(('guardrail_corner', (c + 2.4, c + 2.4, 0.5), (c, c, 0.42)))
    for name, pos, look in views:
        cam.location = pos; target.location = look
        sc.render.filepath = os.path.join(OUT, name + '.png')
        bpy.ops.render.render(write_still=True); print('RENDERED', name, flush=True)
