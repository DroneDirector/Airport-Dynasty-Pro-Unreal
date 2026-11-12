# The runway windsock (ICAO Annex 14 practice): a 6 m galvanised mast on a concrete pad, hinged at the foot so it can
# be lowered for servicing, a free swivel on top carrying the sock's frame (the mouth ring and its two arms), four small
# floodlights round the top that light the sock at night, and a red obstruction light above the swivel. The sock itself
# (3.6 m long, 0.9 m mouth, 0.5 m tail, five bands orange / white / orange / white / orange) is here at rest, fully
# out, for the look and the measurements: in the game it is drawn by the code (WindSocks.cpp) from these same numbers,
# so it can fill, droop and flutter with the wind.
#   blender -b --python Art/WindSock/make_windsock.py -- [render_dir]
# Writes Art/WindSock/WindSock.blend and windsock.glb (the meshes WindSockMast and WindSockFrame; the sock is not
# exported). Units: metres, +X downwind (the way the sock points), Z up, the mast's axis at the origin.
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
RENDER = argv[0] if argv else None

# ---- the measurements (WindSocks.cpp uses the same) ----
MAST_H = 6.0          # the swivel's centre above the ground
RING_X = 0.62         # the mouth ring's centre, out from the mast's axis
RING_R = 0.45         # the mouth's radius (0.9 m across)
TAIL_R = 0.25         # the tail's radius
SOCK_L = 3.6          # the sock's length
BANDS = 5

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, color, metal=0.0, rough=0.5, emit=None, strength=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    return m

GALV = mat('Galvanised', (0.80, 0.81, 0.82), 0.85, 0.36)
DARK = mat('DarkMetal', (0.12, 0.13, 0.14), 0.8, 0.45)
CONC = mat('Concrete', (0.42, 0.40, 0.37), 0.0, 0.85)
LENS = mat('LampLens', (0.95, 0.95, 0.9), 0.0, 0.1, (1.0, 0.95, 0.85), 2.0)
RED = mat('ObstructionLight', (0.8, 0.05, 0.03), 0.0, 0.2, (1.0, 0.05, 0.02), 4.0)
ORANGE = mat('SockOrange', (0.95, 0.30, 0.03), 0.0, 0.75)
WHITE = mat('SockWhite', (0.86, 0.86, 0.84), 0.0, 0.75)

def obj(name, me, m_list, parent=None):
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o)
    for m in m_list: o.data.materials.append(m)
    if parent: o.parent = parent
    return o

def bm_to(name, bm, mats):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = True
    return obj(name, me, mats)

def tube(bm, a, b, r0, r1, seg=24, mat_i=0, caps=True):
    """A round tube (or cone) from a to b."""
    a, b = Vector(a), Vector(b); d = (b - a).normalized()
    q = d.to_track_quat('Z', 'Y'); M = q.to_matrix()
    ra, rb = [], []
    for k in range(seg):
        t = 2 * math.pi * k / seg; v = M @ Vector((math.cos(t), math.sin(t), 0))
        ra.append(bm.verts.new(a + v * r0)); rb.append(bm.verts.new(b + v * r1))
    for k in range(seg):
        f = bm.faces.new((ra[k], ra[(k + 1) % seg], rb[(k + 1) % seg], rb[k])); f.material_index = mat_i
    if caps:
        f = bm.faces.new(list(reversed(ra))); f.material_index = mat_i
        f = bm.faces.new(rb); f.material_index = mat_i

def box(bm, c, s, mat_i=0, rot=None):
    c = Vector(c); R = rot or Matrix.Identity(3)
    vs = [bm.verts.new(c + R @ Vector((sx * s[0] / 2, sy * s[1] / 2, sz * s[2] / 2))) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    for f in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        bm.faces.new([vs[i] for i in f]).material_index = mat_i

def torus(bm, centre, axis, R, r, seg=40, rseg=10, mat_i=0):
    c = Vector(centre); M = Vector(axis).normalized().to_track_quat('Z', 'Y').to_matrix()
    rings = []
    for i in range(seg):
        t = 2 * math.pi * i / seg; o = M @ Vector((math.cos(t), math.sin(t), 0)); ring = []
        for j in range(rseg):
            u = 2 * math.pi * j / rseg
            ring.append(bm.verts.new(c + o * (R + r * math.cos(u)) + M @ Vector((0, 0, r * math.sin(u)))))
        rings.append(ring)
    for i in range(seg):
        for j in range(rseg):
            bm.faces.new((rings[i][j], rings[(i + 1) % seg][j], rings[(i + 1) % seg][(j + 1) % rseg], rings[i][(j + 1) % rseg])).material_index = mat_i

# ---- the mast: pad, hinged foot, pole, lamp ring, swivel housing (fixed; one instance per windsock, Nanite) ----
bm = bmesh.new()
box(bm, (0, 0, -0.15), (1.4, 1.4, 0.40), 2)                          # the concrete pad, 10 cm proud of the ground
box(bm, (0, 0, 0.07), (0.50, 0.50, 0.03), 0)                         # the base plate
for sx in (-1, 1):
    for sy in (-1, 1): tube(bm, (sx * 0.19, sy * 0.19, 0.05), (sx * 0.19, sy * 0.19, 0.14), 0.018, 0.018, 8, 1)   # anchor bolts
for sy in (-1, 1): box(bm, (0, sy * 0.075, 0.25), (0.20, 0.02, 0.32), 0)   # the hinge cheeks (the mast lowers on these)
tube(bm, (0, -0.11, 0.30), (0, 0.11, 0.30), 0.025, 0.025, 12, 1)    # the hinge pin
tube(bm, (0, 0, 0.12), (0, 0, 0.55), 0.065, 0.065, 24, 0)           # the foot sleeve
tube(bm, (0, 0, 0.55), (0, 0, MAST_H - 0.32), 0.055, 0.045, 24, 0)  # the pole, tapering
tube(bm, (0, 0, 1.0), (0, 0, 1.08), 0.070, 0.070, 24, 0)            # a collar (the counterweight clamp)
# the lamp ring below the swivel, four floodlights looking up and out at the sock's sweep
LAMP_Z = MAST_H - 0.55
tube(bm, (0, 0, LAMP_Z - 0.03), (0, 0, LAMP_Z + 0.03), 0.30, 0.30, 32, 0)
for k in range(4):
    a = math.radians(45 + 90 * k); d = Vector((math.cos(a), math.sin(a), 0))
    box(bm, d * 0.17 + Vector((0, 0, LAMP_Z)), (0.26, 0.04, 0.03), 0, Matrix.Rotation(a, 3, 'Z'))
    p = d * 0.34 + Vector((0, 0, LAMP_Z + 0.08)); up = (d * 0.35 + Vector((0, 0, 1))).normalized()
    tube(bm, p, p + up * 0.16, 0.07, 0.08, 16, 1)                    # the lamp housing
    tube(bm, p + up * 0.16, p + up * 0.165, 0.075, 0.075, 16, 3)     # its lens
tube(bm, (0, 0, MAST_H - 0.32), (0, 0, MAST_H - 0.12), 0.07, 0.07, 24, 1)   # the swivel bearing housing
mast = bm_to('WindSockMast', bm, [GALV, DARK, CONC, LENS])

# ---- the frame: turns on the swivel (one per windsock, turned by the game into the wind) ----
bm = bmesh.new()
tube(bm, (0, 0, MAST_H - 0.12), (0, 0, MAST_H + RING_R + 0.10), 0.035, 0.03, 16, 0)   # the swivel post
torus(bm, (RING_X, 0, MAST_H), (1, 0, 0), RING_R, 0.016, 48, 8, 0)                     # the mouth ring
for z in (MAST_H + RING_R, MAST_H - RING_R):                                            # the two arms to the ring
    tube(bm, (0, 0, z if z > MAST_H else MAST_H - 0.10), (RING_X, 0, z), 0.018, 0.018, 10, 0)
tube(bm, (-0.25, 0, MAST_H + 0.05), (0, 0, MAST_H + 0.05), 0.02, 0.02, 10, 0)          # the counterweight arm
tube(bm, (-0.36, 0, MAST_H + 0.05), (-0.25, 0, MAST_H + 0.05), 0.05, 0.05, 16, 1)     # the counterweight
tube(bm, (0, 0, MAST_H + RING_R + 0.10), (0, 0, MAST_H + RING_R + 0.13), 0.05, 0.05, 16, 1)
tube(bm, (0, 0, MAST_H + RING_R + 0.13), (0, 0, MAST_H + RING_R + 0.24), 0.045, 0.04, 16, 2)   # the red light
frame = bm_to('WindSockFrame', bm, [GALV, DARK, RED])

# ---- the sock at rest, fully out (not exported; the game draws it) ----
bm = bmesh.new()
for b in range(BANDS):
    s0, s1 = b / BANDS, (b + 1) / BANDS
    tube(bm, (RING_X + SOCK_L * s0, 0, MAST_H), (RING_X + SOCK_L * s1, 0, MAST_H),
         RING_R + (TAIL_R - RING_R) * s0, RING_R + (TAIL_R - RING_R) * s1, 32, b % 2, caps=False)
sock = bm_to('WindSock_Rest', bm, [ORANGE, WHITE])
sock['note'] = 'Drawn by the game (WindSocks.cpp) from these measurements; not exported.'

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'WindSock.blend'))
# the glb: the mast and the frame only, each at the mast's axis
for o in sc.objects: o.select_set(o.name in ('WindSockMast', 'WindSockFrame'))
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'windsock.glb'), export_format='GLB', use_selection=True, export_apply=True)
print('WINDSOCK MODEL DONE')

if RENDER:
    os.makedirs(RENDER, exist_ok=True)
    sc.render.engine = 'CYCLES'; sc.cycles.samples = 96; sc.render.resolution_x, sc.render.resolution_y = 1400, 1050
    w = bpy.data.worlds.new('Sky'); sc.world = w; w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.68, 0.85, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.8
    bpy.ops.mesh.primitive_plane_add(size=60); g = bpy.context.active_object; g.data.materials.append(mat('Grass', (0.12, 0.2, 0.06), 0, 0.9))
    bpy.ops.object.light_add(type='SUN', rotation=(math.radians(50), 0, math.radians(30))); bpy.context.active_object.data.energy = 4
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
    for name, at, look, lens in (('windsock_side', (4.0, -9.5, 3.2), (2.0, 0, 4.4), 40), ('windsock_top', (-1.6, -2.2, 6.9), (0.5, 0, 6.0), 28)):
        cam.location = at; cam.data.lens = lens; cam.rotation_euler = (Vector(look) - Vector(at)).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = os.path.join(RENDER, name + '.png'); bpy.ops.render.render(write_still=True)
    print('WINDSOCK RENDER DONE')
