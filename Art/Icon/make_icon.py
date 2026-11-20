# The game's icon, rendered in Blender: an airliner climbing out at dusk past the control tower, runway lights below.
# Bold shapes and strong colour contrast so it still reads at 16-32 px.
# Run: python3 make_icon.py [size]  -> render.png (square) next to this file; finish_icon.py makes the icon files.
import bpy, bmesh, math, os, sys, addon_utils
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
SIZE = int(sys.argv[1]) if len(sys.argv) > 1 else 1024
addon_utils.enable('cycles')
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = int(sys.argv[2]) if len(sys.argv) > 2 else 48; sc.cycles.use_denoising = True
sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Punchy'
sc.render.resolution_x = sc.render.resolution_y = SIZE


# #region ── Materials ──
def principled(name, rgb, metal=0.0, rough=0.4, emit=0.0, ecol=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission Color'].default_value = (*(ecol or rgb), 1); b.inputs['Emission Strength'].default_value = emit
    return m


def emission(name, rgb, strength):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear()
    e = nt.nodes.new('ShaderNodeEmission'); e.inputs['Color'].default_value = (*rgb, 1); e.inputs['Strength'].default_value = strength
    o = nt.nodes.new('ShaderNodeOutputMaterial'); nt.links.new(e.outputs[0], o.inputs[0])
    return m


def livery():
    """White top, gold cheat line, navy belly - by height on the fuselage."""
    m = bpy.data.materials.new('Livery'); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Roughness'].default_value = 0.25; b.inputs['Metallic'].default_value = 0.15
    tc = nt.nodes.new('ShaderNodeTexCoord'); sx = nt.nodes.new('ShaderNodeSeparateXYZ')
    ramp = nt.nodes.new('ShaderNodeValToRGB')
    nt.links.new(tc.outputs['Object'], sx.inputs[0]); nt.links.new(sx.outputs['Z'], ramp.inputs[0])
    cr = ramp.color_ramp; cr.interpolation = 'CONSTANT'
    cr.elements[0].position = 0.0; cr.elements[0].color = (0.02, 0.05, 0.16, 1)
    cr.elements[1].position = 0.30; cr.elements[1].color = (1.0, 0.62, 0.12, 1)
    e = cr.elements.new(0.42); e.color = (0.92, 0.94, 0.97, 1)
    # object Z runs about -2.2 .. 2.2: map to 0..1
    mp = nt.nodes.new('ShaderNodeMapRange'); mp.inputs['From Min'].default_value = -2.2; mp.inputs['From Max'].default_value = 2.2
    nt.links.new(sx.outputs['Z'], mp.inputs['Value']); nt.links.new(mp.outputs['Result'], ramp.inputs[0])
    nt.links.new(ramp.outputs['Color'], b.inputs['Base Color'])
    return m


M_LIV = livery()
M_NAVY = principled('Navy', (0.02, 0.05, 0.16), 0.2, 0.3)
M_GOLD = principled('Gold', (1.0, 0.62, 0.12), 0.8, 0.25)
M_WHITE = principled('White', (0.9, 0.92, 0.95), 0.1, 0.3)
M_DARK = principled('Dark', (0.01, 0.012, 0.02), 0.0, 0.5)
M_TOWER = principled('Tower', (0.10, 0.12, 0.18), 0.1, 0.5)
M_CAB = principled('CabGlass', (0.1, 0.5, 0.7), 0.0, 0.1, emit=6.0, ecol=(0.35, 0.85, 1.0))
M_RUNWAY = principled('Runway', (0.02, 0.022, 0.03), 0.0, 0.62)
M_LAMP_W = emission('LampWhite', (1.0, 0.92, 0.75), 30)
M_LAMP_R = emission('LampRed', (1.0, 0.08, 0.05), 40)
M_LAMP_G = emission('LampGreen', (0.1, 1.0, 0.4), 25)
M_LAMP_B = emission('LampBlue', (0.15, 0.4, 1.0), 25)
# #endregion


def obj(name, bm, mat, smooth=True, sub=0):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(mat)
    if smooth: me.shade_smooth()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o)
    if sub:
        s = o.modifiers.new('s', 'SUBSURF'); s.levels = sub; s.render_levels = sub
    return o


# #region ── The airliner (built along +X, nose at +X, 40 m long) ──
def fuselage():
    bm = bmesh.new(); n = 24; rings = []
    prof = [(-20.0, 0.25, 1.55), (-18.5, 0.6, 1.25), (-16, 1.25, 0.75), (-12, 1.9, 0.2), (-6, 2.05, 0.0),
            (8, 2.05, 0.0), (13, 1.95, -0.05), (16, 1.6, -0.15), (18, 1.05, -0.3), (19.3, 0.5, -0.45), (19.9, 0.12, -0.55)]
    for x, r, zc in prof:
        rings.append([bm.verts.new((x, r * math.cos(2 * math.pi * i / n), zc + r * 1.05 * math.sin(2 * math.pi * i / n)))
                      for i in range(n)])
    for a in range(len(rings) - 1):
        for i in range(n):
            j = (i + 1) % n; bm.faces.new((rings[a][i], rings[a][j], rings[a + 1][j], rings[a + 1][i]))
    bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj('Fuselage', bm, M_LIV, sub=2)


def slab(name, pts, thick, mat, z=0.0, axis='Z'):
    """A flat wing-like slab from an outline [(a, b)], tapered thickness."""
    bm = bmesh.new()
    if axis == 'Z':
        lo = [bm.verts.new((a, b, z - thick / 2)) for a, b in pts]; hi = [bm.verts.new((a, b, z + thick / 2)) for a, b in pts]
    else:   # vertical (fin): a along X, b up
        lo = [bm.verts.new((a, -thick / 2, b)) for a, b in pts]; hi = [bm.verts.new((a, thick / 2, b)) for a, b in pts]
    bm.faces.new(lo); bm.faces.new(list(reversed(hi)))
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = obj(name, bm, mat, smooth=True)
    bv = o.modifiers.new('b', 'BEVEL'); bv.width = thick * 0.45; bv.segments = 4; bv.limit_method = 'NONE'
    return o


def nacelle(name, x, y, z):
    bm = bmesh.new(); n = 24; rings = []
    for xx, r in ((2.4, 0.95), (1.8, 1.12), (-0.5, 1.10), (-2.2, 0.85), (-2.9, 0.55)):
        rings.append([bm.verts.new((x + xx, y + r * math.cos(2 * math.pi * i / n), z + r * math.sin(2 * math.pi * i / n))) for i in range(n)])
    for a in range(len(rings) - 1):
        for i in range(n):
            j = (i + 1) % n; bm.faces.new((rings[a][i], rings[a][j], rings[a + 1][j], rings[a + 1][i]))
    bm.faces.new(list(reversed(rings[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = obj(name, bm, M_NAVY, sub=1)
    bm = bmesh.new(); bmesh.ops.create_circle(bm, cap_ends=True, segments=24, radius=0.92)
    for v in bm.verts: v.co = Vector((x + 2.38, y + v.co.x, z + v.co.y))
    obj(name + 'Intake', bm, M_DARK, smooth=False)
    bm = bmesh.new(); bmesh.ops.create_circle(bm, cap_ends=False, segments=24, radius=1.0)
    ring = bm.verts[:]
    for v in ring: v.co = Vector((x + 2.42, y + v.co.x, z + v.co.y))
    bmesh.ops.extrude_edge_only(bm, edges=bm.edges[:])
    return o


parts = [fuselage()]
for s in (1, -1):
    wing = [(4.0, s * 1.6), (-3.5, s * 1.6), (-9.0, s * 17.0), (-6.8, s * 17.0)]
    parts.append(slab('Wing', wing, 0.55, M_WHITE, z=-1.0))
    parts.append(slab('Winglet', [(-6.8, s * 17.0), (-9.0, s * 17.0), (-10.2, s * 17.3), (-9.0, s * 17.3)], 0.25, M_GOLD, z=-1.0))
    tail = [(-15.0, s * 0.8), (-18.0, s * 0.8), (-20.5, s * 6.6), (-19.2, s * 6.6)]
    parts.append(slab('Stabiliser', tail, 0.3, M_WHITE, z=0.9))
    parts.append(nacelle('Engine', 0.5, s * 6.2, -2.1))
    pyl = slab('Pylon', [(1.6, 0), (-2.5, 0), (-3.8, 1.0), (-0.4, 1.0)], 0.35, M_WHITE, axis='Y')
    pyl.location = (0, s * 6.2, -2.1 + 0.6); parts.append(pyl)
fin = slab('Fin', [(-13.5, 1.7), (-17.8, 1.7), (-20.6, 8.6), (-18.3, 8.6)], 0.35, M_NAVY, axis='Y')
parts.append(fin)
parts.append(slab('FinTip', [(-18.3, 8.6), (-20.6, 8.6), (-20.75, 8.95), (-18.5, 8.95)], 0.36, M_GOLD, axis='Y'))
# the fin's gold crown (the dynasty): three points, on both sides of the fin
for s in (1, -1):
    crown = [(-19.6, 4.0), (-17.2, 4.0), (-16.9, 6.0), (-17.7, 5.0), (-18.4, 6.4), (-19.1, 5.0), (-19.9, 6.0)]
    c = slab('Crown', crown, 0.06, M_GOLD, axis='Y'); c.location = (0, s * 0.19, 0)
    c.modifiers[0].width = 0.02
# windscreen (dark) and the cockpit's window band
bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=1.0)
for v in bm.verts: v.co = Vector((16.2 + v.co.x * 1.6, v.co.y * 1.45, 0.75 + v.co.z * 0.45))
parts.append(obj('Windscreen', bm, M_DARK))
# navigation and beacon lights
for s, mat in ((1, M_LAMP_R), (-1, M_LAMP_G)):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.28)
    for v in bm.verts: v.co += Vector((-10.0, s * 17.15, -0.6))
    parts.append(obj('NavLight', bm, mat))
for z in (2.2, -2.25):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.25)
    for v in bm.verts: v.co += Vector((-2.0, 0, z))
    parts.append(obj('Beacon', bm, M_LAMP_R))
for s in (1, -1):    # landing lights glowing at the wing roots
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.32)
    for v in bm.verts: v.co += Vector((2.2, s * 2.2, -1.15))
    parts.append(obj('LandingLight', bm, M_LAMP_W))

plane = bpy.data.objects.new('Airliner', None); sc.collection.objects.link(plane)
for p in parts: p.parent = plane
plane.location = (-3.5, 36.0, 17.5)
plane.rotation_euler = (math.radians(-14), math.radians(-13), math.radians(-128))   # climbing, banking, toward the viewer's left
# #endregion


# #region ── Tower, runway and its lights, terminal line ──
def cyl(name, x, y, z0, z1, r0, r1, mat, n=32):
    bm = bmesh.new()
    a = [bm.verts.new((x + r0 * math.cos(2 * math.pi * i / n), y + r0 * math.sin(2 * math.pi * i / n), z0)) for i in range(n)]
    b = [bm.verts.new((x + r1 * math.cos(2 * math.pi * i / n), y + r1 * math.sin(2 * math.pi * i / n), z1)) for i in range(n)]
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(list(reversed(a))); bm.faces.new(b)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj(name, bm, mat)

TX, TY = 21.0, 62.0
cyl('TowerShaft', TX, TY, 0, 30, 2.4, 1.9, M_TOWER)
cyl('TowerCollar', TX, TY, 29.5, 31.0, 1.9, 4.2, M_TOWER)
cyl('TowerCab', TX, TY, 31.0, 34.2, 4.2, 4.9, M_CAB)
cyl('TowerMullion', TX, TY, 31.0, 31.35, 4.25, 4.3, M_TOWER)
cyl('TowerRoof', TX, TY, 34.2, 35.3, 5.2, 3.4, M_TOWER)
cyl('TowerMast', TX, TY, 35.3, 40.0, 0.25, 0.12, M_TOWER, 12)
bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.45)
for v in bm.verts: v.co += Vector((TX, TY, 40.2))
obj('TowerBeacon', bm, M_LAMP_R)

bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=1)
for v in bm.verts: v.co = Vector((v.co.x * 22, 300 + v.co.y * 340, 0))
obj('Runway', bm, M_RUNWAY, smooth=False)
bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=1)
for v in bm.verts: v.co = Vector((v.co.x * 900, 300 + v.co.y * 700, -0.05))
obj('Ground', bm, principled('Grass', (0.008, 0.015, 0.02), 0.0, 0.9), smooth=False)


def lamp(x, y, mat, r=0.35):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=r)
    for v in bm.verts: v.co += Vector((x, y, r))
    return obj('Lamp', bm, mat)

y = 10.0
while y < 640:
    r = 0.22 + y * 0.0035
    lamp(-11.0, y, M_LAMP_W, r); lamp(11.0, y, M_LAMP_W, r)
    if int(y) % 2 == 0: lamp(0.0, y, M_LAMP_W, r * 0.8)
    y += 9.0 + y * 0.06
for x in (-14, -16, -18, 14, 16, 18):
    lamp(x, 30.0, M_LAMP_G, 0.4)
for x in range(-60, -24, 6):   # taxiway blue lights off to the left
    lamp(x, 60.0 + (x + 60) * 0.3, M_LAMP_B, 0.35)
# #endregion


# #region ── Dusk sky (a gradient wall) with the sun low behind, lights ──
bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=1)
for v in bm.verts: v.co = Vector((v.co.x * 1400, 640, 300 + v.co.y * 340))
sky = obj('Sky', bm, bpy.data.materials.new('SkyGrad'), smooth=False)
m = sky.data.materials[0]; m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
tc = nt.nodes.new('ShaderNodeTexCoord'); sx = nt.nodes.new('ShaderNodeSeparateXYZ'); ramp = nt.nodes.new('ShaderNodeValToRGB')
em = nt.nodes.new('ShaderNodeEmission'); out = nt.nodes.new('ShaderNodeOutputMaterial')
nt.links.new(tc.outputs['Generated'], sx.inputs[0]); nt.links.new(sx.outputs['Z'], ramp.inputs[0])
nt.links.new(ramp.outputs[0], em.inputs['Color']); nt.links.new(em.outputs[0], out.inputs[0])
em.inputs['Strength'].default_value = 1.0
cr = ramp.color_ramp
cr.elements[0].position = 0.07; cr.elements[0].color = (1.0, 0.55, 0.16, 1)
cr.elements[1].position = 0.50; cr.elements[1].color = (0.008, 0.02, 0.075, 1)
e = cr.elements.new(0.16); e.color = (0.95, 0.32, 0.20, 1)
e = cr.elements.new(0.29); e.color = (0.30, 0.10, 0.32, 1)
# the sun just over the horizon
bm = bmesh.new(); bmesh.ops.create_circle(bm, cap_ends=True, segments=48, radius=26)
for v in bm.verts: v.co = Vector((-30 + v.co.x, 630, 14 + v.co.y))
obj('Sun', bm, emission('SunMat', (1.0, 0.75, 0.40), 6), smooth=False)

w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.05, 0.06, 0.12, 1)
w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.5
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 4.5; sun.color = (1.0, 0.62, 0.35); sun.angle = math.radians(3)
so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so)
so.rotation_euler = (Vector((0, 0, 0)) - Vector((-30, 640, 40))).to_track_quat('-Z', 'Y').to_euler()
fill = bpy.data.lights.new('Fill', 'SPOT'); fill.energy = 2500000; fill.spot_size = math.radians(19); fill.shadow_soft_size = 6
fill.color = (0.75, 0.85, 1.0)
fo = bpy.data.objects.new('Fill', fill); sc.collection.objects.link(fo); fo.location = (8, -6, 9)
fo.rotation_euler = (Vector((-3.5, 36, 17.5)) - fo.location).to_track_quat('-Z', 'Y').to_euler()
rim = bpy.data.lights.new('Rim', 'AREA'); rim.energy = 60000; rim.size = 40; rim.color = (1.0, 0.6, 0.3)
ro = bpy.data.objects.new('Rim', rim); sc.collection.objects.link(ro); ro.location = (-30, 110, 45)
ro.rotation_euler = (Vector((-3.5, 36, 17.5)) - ro.location).to_track_quat('-Z', 'Y').to_euler()
# #endregion

cd = bpy.data.cameras.new('Cam'); cd.lens = 32
cam = bpy.data.objects.new('Cam', cd); sc.collection.objects.link(cam); sc.camera = cam
cam.location = (2.0, -14.0, 3.0)
cam.rotation_euler = (Vector((0.0, 60.0, 17.0)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
sc.render.filepath = os.path.join(HERE, 'render.png')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Icon.blend'))
bpy.ops.render.render(write_still=True)
print('rendered', flush=True)
