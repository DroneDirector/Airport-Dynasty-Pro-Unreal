# The A320's moving control surfaces (owner 2026-11-28, step B: "every part that has to do with the motion also animates
# correctly"): flaps (Fowler: aft on their tracks and down), slats, ailerons, elevators, rudder and the flap track fairings.
# The hinges are FlightGear's own (Models/Fuselage/fuselage.xml of the A320-family, GPL v2: x aft, y right, z up, metres),
# carried into the game model's frame exactly as Art/A320/make_a320_fg.py placed the parts (the rigid move found from the
# source's and the built model's own vertices).
#   blender -b Art/A320/A320-fg.blend --python Art/A320/a320_surfaces.py -- [render.png]
# Prints "SURF <name> pivot (cm) axis (game) trans (cm at full)" for SimTraffic.cpp Surfaces(): in the game frame
# (cm: Blender (x, y, z) m -> (x, -y, z) * 100); the axis already signed so FQuat(axis, +deg) is the surface's positive
# deflection: trailing edge DOWN for flaps, ailerons, elevators and fairings, leading edge DOWN for slats, the rudder's
# trailing edge to the LEFT. With a png: poses every part at full deflection and renders the check once.
import bpy, sys, math
import numpy as np
from mathutils import Matrix, Vector, Quaternion

SRC = 'F:/FlightGear-Aircraft/out/A320-200-CFM.glb'
PNG = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv and len(sys.argv) > sys.argv.index('--') + 1 else ''

# FlightGear: name, hinge p1, p2, kind (TE / LE / RUD / FAIR), translation (direction, metres at full flap) or None
FG = [
    ('FlapL1', (-0.40897, -1.96981, -1.4725), (-0.40885, -6.33455, -0.802401), 'TE', ((1, 0, -0.5), 0.30)),
    ('FlapL2', (-0.40519, -6.44122, -0.788081), (1.92808, -13.1844, -0.065162), 'TE', ((1, 0.2, -0.5), 0.27)),
    ('FlapR1', (-0.40897, 1.96981, -1.4725), (-0.40885, 6.33455, -0.802401), 'TE', ((1, 0, -0.5), 0.30)),
    ('FlapR2', (-0.40519, 6.44122, -0.788081), (1.92808, 13.1844, -0.065162), 'TE', ((1, -0.2, -0.5), 0.27)),
    ('SlatL1', (-3.79913, -5.19121, -1.27821), (-5.30738, -2.12168, -1.49252), 'LE', None),
    ('SlatL2', (1.76797, -16.34866, -0.00821), (-3.54786, -5.99527, -1.06999), 'LE', None),
    ('SlatR1', (-3.79913, 5.1891, -1.27821), (-5.30738, 2.11957, -1.49252), 'LE', None),
    ('SlatR2', (1.76797, 16.34650, -0.00821), (-3.54786, 5.99316, -1.06999), 'LE', None),
    ('AileronL', (1.44774, -13.2833, -0.005817), (2.53379, -16.2917, 0.293572), 'TE', None),
    ('AileronR', (1.44774, 13.2833, -0.005817), (2.53379, 16.2917, 0.293572), 'TE', None),
    ('ElevatorL', (15.5724, -0.909268, 0.658597), (17.3985, -6.1085, 1.48068), 'TE', None),
    ('ElevatorR', (17.3985, 6.1085, 1.48068), (15.5724, 0.909268, 0.658597), 'TE', None),
    ('Rudder', (15.6479, 0.001397, 3.88092), (16.4401, -0.000603, 5.71732), 'RUD', None),
    ('FairingL1', (-1.35375, -5.04032, -1.19595), (-1.35375, -4.6714, -1.24688), 'FAIR', None),
    ('FairingL2', (-0.75735, -8.46311, -0.789946), (-0.75735, -8.09297, -0.831006), 'FAIR', None),
    ('FairingL3', (0.60609, -12.0651, -0.344492), (0.60609, -11.7946, -0.374512), 'FAIR', None),
    ('FairingR1', (-1.35375, 5.04032, -1.19595), (-1.35375, 4.6714, -1.24688), 'FAIR', None),
    ('FairingR2', (-0.75735, 8.46311, -0.789946), (-0.75735, 8.09297, -0.831006), 'FAIR', None),
    ('FairingR3', (0.60609, 12.0651, -0.344492), (0.60609, 11.7946, -0.374512), 'FAIR', None),
]

built = {o.name: o for o in bpy.data.objects if o.type == 'MESH'}
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=SRC)
src = {o.name.rsplit('.', 1)[0] if o.name not in built else o.name: o for o in set(bpy.data.objects) - before if o.type == 'MESH'}
A, Bv = [], []
for n in ('Rudder', 'ElevatorL', 'AileronR', 'SlatL1', 'Hstabs'):
    if n in src and n in built and len(src[n].data.vertices) == len(built[n].data.vertices):
        Ms, Mb = src[n].matrix_world, built[n].matrix_world
        A += [tuple(Ms @ v.co) for v in src[n].data.vertices]; Bv += [tuple(Mb @ v.co) for v in built[n].data.vertices]
A, Bv = np.array(A), np.array(Bv)
ca, cb = A.mean(0), Bv.mean(0)
U, S, Vt = np.linalg.svd((A - ca).T @ (Bv - cb))
D = np.diag([1, 1, np.sign(np.linalg.det(Vt.T @ U.T))])
R = Vt.T @ D @ U.T; t = cb - R @ ca
err = np.abs((A @ R.T + t) - Bv).max()
print('SURF fit: %d vertices, worst %.4f m' % (len(A), err))
for o in set(bpy.data.objects) - before: bpy.data.objects.remove(o)
Rm = Matrix(R.tolist())
def place(p): return Rm @ Vector(p) + Vector(t.tolist())
def game(v): return (v.x * 100, -v.y * 100, v.z * 100)


def pts(o): return [o.matrix_world @ v.co for v in o.data.vertices]


out = {}
for name, p1, p2, kind, tr in FG:
    o = built.get(name)
    if not o: print('SURF missing', name); continue
    a, b = place(p1), place(p2); ax = (b - a).normalized()
    vs = pts(o)
    near = min(min((v - a).length for v in vs), min((v - b).length for v in vs))
    # the edge that moves: aft (Blender -y) for trailing surfaces, forward for slats; its motion under +1 deg
    def off(v): return (v - a - ax * (v - a).dot(ax)).length
    if kind == 'LE': tip = max(vs, key=lambda v: v.y + 0.001 * off(v))
    else: tip = max(vs, key=lambda v: -v.y + 0.001 * off(v))
    q = Quaternion(ax, math.radians(1.0)); d = q @ (tip - a) - (tip - a)
    s = (1 if d.x < 0 else -1) if kind == 'RUD' else (1 if d.z < 0 else -1)
    gax = Vector((ax.x, -ax.y, ax.z)) * -s   # (the y mirror turns the sense: the game's FQuat(gax, +deg))
    tv = (0, 0, 0)
    if tr:
        dv = (Rm @ Vector(tr[0])).normalized() * tr[1]; tv = game(dv)
    out[name] = (a, ax * s, tr and (Rm @ Vector(tr[0])).normalized() * tr[1])
    pv = game(a)
    print('SURF %-10s pivot (%.1f, %.1f, %.1f) axis (%.5f, %.5f, %.5f) trans (%.1f, %.1f, %.1f) hinge-to-mesh %.3f m' %
          (name, pv[0], pv[1], pv[2], gax.x, gax.y, gax.z, tv[0], tv[1], tv[2], near))

if PNG:
    full = {'Flap': 40, 'Slat': 27, 'AileronL': 20, 'AileronR': -25, 'Elevator': -30, 'Rudder': 25, 'Fairing': 22.5}
    for name, (a, ax, tr) in out.items():
        deg = next(v for k, v in full.items() if name.startswith(k))
        if name in ('FairingL2', 'FairingL3', 'FairingR2', 'FairingR3'): deg = 19.0
        M = Matrix.Translation(a) @ Quaternion(ax, math.radians(deg)).to_matrix().to_4x4() @ Matrix.Translation(-a)
        if tr: M = Matrix.Translation(tr) @ M
        o = built[name]; vs0 = pts(o); o.matrix_world = M @ o.matrix_world
        tip = max(range(len(vs0)), key=lambda k: (vs0[k] - a - ax * (vs0[k] - a).dot(ax)).length)
        d = M @ vs0[tip] - vs0[tip]
        print('SURF posed %-10s %6.1f deg: farthest point moves (%.2f, %.2f, %.2f) m (Blender: -y aft, -x left)' % (name, deg, d.x, d.y, d.z))
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_cavity = True
    sc.render.resolution_x, sc.render.resolution_y = 1000, 640
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
    sc.world = sc.world or bpy.data.worlds.new('W')
    # (views: the left wing from behind and above; the tail from behind, left and above; the right wing from below, ahead)
    for i, (at, to, lens) in enumerate((((-22, -38, 16), (-8, -14, 3.5), 35), ((-14, -31, 6.5), (0, -30, 6.5), 45), ((16, 4, -1), (8, -14, 3.5), 30))):
        cam.location = at; cam.data.lens = lens
        cam.rotation_euler = (Vector(to) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = PNG.replace('.png', '_%d.png' % i); bpy.ops.render.render(write_still=True)
    print('SURF rendered', PNG)
