# Sheet 5 (to-do 213): 3D concept renders of the AFTER proposal - massing model, not the game's assets. The Grand Hall's
# wave roof continued east over the whole east wing, the same full-height glass and tree pillars, the link gallery, the
# East Hall inside. Plan x -> Blender X (minus 200), plan z (airside) -> Blender Y, height -> Z.
#   "C:\Program Files\Blender Foundation\Blender 3.3\blender.exe" -b --python Blueprints/Terminal/Changes/render_concept.py
import bpy, math, os, sys
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Terminal-Changes')
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
OX = 200.0


def mat(n, c, r=0.6, m=0.0, e=0.0, a=1.0):
    M = bpy.data.materials.new(n); M.use_nodes = True; b = M.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = r; b.inputs['Metallic'].default_value = m
    if e: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = e
    if a < 1.0:
        b.inputs['Alpha'].default_value = a; M.blend_method = 'BLEND'
        if 'Transmission' in b.inputs: b.inputs['Transmission'].default_value = 0.6
    return M


ROOFM = mat('roof', (0.82, 0.84, 0.86), 0.45, 0.6); ROOFU = mat('roofunder', (0.78, 0.62, 0.45), 0.6)   # zinc top, wood underside
GLASS = mat('glass', (0.55, 0.72, 0.8), 0.04, 0, 0, 0.28); MUL = mat('mullion', (0.15, 0.16, 0.18), 0.4, 0.7)
TERR = mat('terrazzo', (0.8, 0.78, 0.74), 0.3); WALL = mat('wall', (0.9, 0.9, 0.88), 0.7); CEDAR = mat('cedar', (0.42, 0.26, 0.15), 0.7)
PAVE = mat('pave', (0.6, 0.6, 0.58), 0.85); ASPH = mat('asph', (0.09, 0.09, 0.1), 0.9); GRASS = mat('grass', (0.18, 0.3, 0.12), 0.95)
WHITE = mat('white', (0.93, 0.94, 0.95), 0.35); DESK = mat('desk', (0.93, 0.93, 0.95), 0.4); DARK = mat('dark', (0.12, 0.18, 0.28), 0.5)
YEL = mat('yel', (0.95, 0.75, 0.1), 0.5, 0, 0.6); SIGN = mat('sign', (0.1, 0.35, 0.7), 0.4, 0, 3.0); GRN = mat('grn', (0.1, 0.7, 0.3), 0.4, 0, 3.0)
PIER = mat('pier', (0.7, 0.72, 0.74), 0.6); WOOD = mat('wood', (0.55, 0.38, 0.22), 0.6); LAMP = mat('lamp', (1, 0.96, 0.88), 0.3, 0, 6.0)
PEOPLE = [mat('p%d' % i, c) for i, c in enumerate(((0.2, 0.25, 0.4), (0.6, 0.15, 0.15), (0.15, 0.4, 0.25), (0.45, 0.4, 0.3), (0.3, 0.3, 0.35), (0.7, 0.6, 0.2)))]
SKIN = mat('skin', (0.75, 0.58, 0.45), 0.6)


def under(x, x0): return 16.2 + 3.9 * math.cos(2 * math.pi * (x - (x0 + 51.0)) / 30.0)


def P(x, z, h=0.0): return (x - OX, z, h)


def box(x0, z0, x1, z1, h0, h1, m, name='B'):
    bpy.ops.mesh.primitive_cube_add(size=1, location=P((x0 + x1) / 2, (z0 + z1) / 2, (h0 + h1) / 2))
    o = bpy.context.object; o.name = name; o.scale = (x1 - x0, z1 - z0, h1 - h0); o.data.materials.append(m); return o


def mesh(name, V, F, m):
    me = bpy.data.meshes.new(name); me.from_pydata(V, [], F); me.update()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    return o


def wave_roof(xa, xb, ph, z0=-16.0, z1=34.0, th=2.4, step=0.5):
    n = int((xb - xa) / step) + 1; V = []; Fu = []; Ft = []
    for i in range(n):
        x = xa + (xb - xa) * i / (n - 1); u = under(x, ph)
        V += [P(x, z0, u), P(x, z1, u), P(x, z1, u + th), P(x, z0, u + th)]
    for i in range(n - 1):
        a, b = 4 * i, 4 * (i + 1)
        Fu.append((a, b, b + 1, a + 1))                                         # underside
        Ft += [(a + 3, a + 2, b + 2, b + 3), (a, a + 3, b + 3, b), (a + 1, b + 1, b + 2, a + 2)]
    Ft += [(0, 1, 2, 3), (4 * (n - 1) + 3, 4 * (n - 1) + 2, 4 * (n - 1) + 1, 4 * (n - 1))]
    mesh('RoofTop', V, Ft, ROOFM); mesh('RoofUnder', V, Fu, ROOFU)


def glass_front(xa, xb, z, ph, top=None, step=0.5):
    n = int((xb - xa) / step) + 1; V = []; F = []
    for i in range(n):
        x = xa + (xb - xa) * i / (n - 1); t = under(x, ph) if top is None else top
        V += [P(x, z, 0.0), P(x, z, t)]
    for i in range(n - 1): F.append((2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1))
    mesh('Glass', V, F, GLASS)
    x = xa
    while x <= xb + 0.01:
        t = under(x, ph) if top is None else top
        box(x - 0.06, z - 0.12, x + 0.06, z + 0.12, 0, t, MUL, 'Mullion'); x += 2.5
    box(xa, z - 0.1, xb, z + 0.1, 5.35, 5.45, MUL, 'Transom')


def cyl_between(a, b, r, m):
    a, b = Vector(a), Vector(b); d = b - a
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=d.length, location=(a + b) / 2, vertices=16)
    o = bpy.context.object; o.rotation_euler = d.to_track_quat('Z', 'Y').to_euler(); o.data.materials.append(WHITE); return o


def tree(x, ph, z=-9.0):
    cyl_between(P(x, z, 0), P(x, z, 8), 0.45, WHITE)
    for dx, dz in ((-4.5, -3), (4.5, -3), (-4.5, 3), (4.5, 3)):
        cyl_between(P(x, z, 8), P(x + dx, z + dz, under(x + dx, ph)), 0.2, WHITE)


def person(x, z, k):
    bpy.ops.mesh.primitive_cylinder_add(radius=0.17, depth=0.9, location=P(x, z, 0.45), vertices=10); bpy.context.object.data.materials.append(PEOPLE[(k + 2) % 6])
    bpy.ops.mesh.primitive_cylinder_add(radius=0.23, depth=0.65, location=P(x, z, 1.22), vertices=10); bpy.context.object.data.materials.append(PEOPLE[k % 6])
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.13, location=P(x, z, 1.68), segments=10, ring_count=6); bpy.context.object.data.materials.append(SKIN)


# ---- ground, forecourt
box(-300, -120, 620, 140, -0.2, 0, GRASS, 'Ground')
box(-90, -21.5, 560, -4, 0, 0.15, PAVE, 'Pavement'); box(-90, -40, 560, -21.5, 0.0, 0.02, ASPH, 'Road')
for x in range(-80, 560, 6): box(x, -31, x + 3, -30.8, 0.02, 0.03, WHITE, 'Lane')
# ---- the roof: arrivals copy, then the Grand Hall's wave continued east to the end
wave_roof(-78, 24, -78); wave_roof(24, 551.2, 24)
for xs0 in (-78, 24): [tree(xs0 + o, xs0) for o in (32, 70)]
for i, tr in enumerate(range(150, 551, 60)): tree(tr + (-4 if i % 2 == 0 else 4), 24)
# ---- the buildings behind the glass
box(-241, 52, 551.2, 68, 0, 12, PIER, 'Pier'); box(-241, 52, 551.2, 68, 12, 12.3, WALL, 'PierRoof')
for gx in (-55.6, 25, 105.5, 186, -216.7, -136.2, 272.4, 352.2, 424.5, 496.8): box(gx - 17, 68, gx + 17, 82, 0, 11.5, PIER, 'Gate')
box(-80, -4, 0, 52, 0, 12, WALL, 'ArrivalsWing'); box(-80.2, -4.2, 0, -4, 5.4, 11.6, CEDAR, 'Cedar')
glass_front(-80, 0, -4.3, 0, top=5.4)
box(0, 0, 42, 52, 0, 12, WALL, 'EscRoom'); box(114, 0, 156, 52, 0, 12, WALL, 'BagBlock')
box(42, 30, 114, 52, 0, 12, WALL, 'Security'); box(156, 40, 228, 52, 0, 12, WALL, 'EastHallBack')
box(228, 6, 551.2, 52, 0, 12, WALL, 'EastWingRooms')
box(470, 0, 551.2, 52, 0, 0.05, TERR, 'CoachFloor'); bpy.data.objects['EastWingRooms'].scale.x = 470 - 228; bpy.data.objects['EastWingRooms'].location.x = (228 + 470) / 2 - OX
box(470, 40, 551.2, 52, 0, 12, WALL, 'CoachBack')
for x0, x1 in ((42, 114), (107, 156), (156, 228), (228, 470)): box(x0, -4 if x0 == 107 else 0, x1, 40 if x0 == 156 else 30, 0, 0.05, TERR, 'Floor')
box(34, 34, 551.2, 52, 12, 12.4, WALL, 'FlatRoof')
# shop fronts along the promenade (z 6) and in the East Hall
for x in range(232, 466, 14): box(x, 5.8, x + 11, 6.0, 0.3, 4.2, GLASS, 'Shop'); box(x, 5.9, x + 11, 6.1, 4.3, 5.0, SIGN, 'ShopSign')
# ---- the glass facades (the Grand Hall's, to the wave)
glass_front(42, 114, 0, 24); glass_front(107, 156, -4, 24); glass_front(156, 551.2, 0, 24)
for v0, v1 in ((44, 56), (67, 83), (95, 107), (185, 197), (290, 300), (395, 405), (500, 512)):
    box(v0, -4 if v0 < 110 else -3, v1, 0, 0, 3.8, GLASS, 'Vestibule'); box(v0, -4 if v0 < 110 else -3, v1, 0, 3.8, 4.0, MUL, 'VestRoof')
# ---- inside: the Grand Hall islands, the East Hall (check-in, cafe, rental, meeting rail, signs), the coach station benches
for xs in (47.5, 74.5): box(xs, 15.6, xs + 25, 19.1, 0, 1.1, DESK, 'Island'); box(xs, 18.4, xs + 25, 19.1, 1.1, 2.6, DARK, 'IslandBack')
box(101, 15.6, 113, 18.0, 0, 1.4, DESK, 'BagDrop')
box(160, 18, 190, 22, 0, 1.1, DESK, 'EastIsland'); box(160, 21.3, 190, 22, 1.1, 2.6, DARK, 'EastIslandBack')
box(160, 22.5, 190, 23.0, 3.2, 4.2, SIGN, 'CheckinSign')
box(158, 2, 172, 10, 0, 1.1, WOOD, 'Cafe'); box(196, 2, 226, 3.2, 0, 1.1, DESK, 'Rental'); box(196, 2.0, 226, 2.4, 2.6, 3.4, YEL, 'RentalSign')
box(200, 12, 200.1, 30, 0.9, 1.0, WHITE, 'Rail'); box(200, 12, 226, 12.1, 0.9, 1.0, WHITE, 'Rail2')
box(227.6, 12, 228, 18, 3.4, 4.2, GRN, 'Exit'); box(227.8, 12, 228, 18, 0, 3.2, GLASS, 'CustomsDoor')
for x in range(480, 545, 8):
    for z in (12, 20, 28): box(x, z, x + 5, z + 0.6, 0, 0.45, WOOD, 'Bench')
box(500, 39.5, 530, 40, 4, 6, SIGN, 'CoachBoard')
for x in range(46, 551, 8):
    for z in (10, 26): box(x, z, x + 1.4, z + 0.4, 9.5, 9.6, LAMP, 'Lamp')
import random
random.seed(7); k = 0
for (xa, xb, za, zb, n) in ((45, 110, 3, 14, 30), (160, 226, 4, 34, 45), (228, 465, 1, 5, 40), (475, 548, 3, 36, 25), (-75, 550, -18, -6, 50)):
    for _ in range(n): person(random.uniform(xa, xb), random.uniform(za, zb), k); k += 1
# ---- light, camera
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 3.2; sun.angle = math.radians(1.5)
so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(-150))
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.68, 0.85, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 1.0
cam = bpy.data.cameras.new('Cam'); co = bpy.data.objects.new('Cam', cam); sc.collection.objects.link(co); sc.camera = co
sc.render.engine = 'CYCLES'; sc.cycles.samples = int(os.environ.get('TC_SAMPLES', '48')); sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 3840, 2160; sc.view_settings.view_transform = 'Filmic'
try:
    sc.cycles.device = 'GPU'; bpy.context.preferences.addons['cycles'].preferences.compute_device_type = 'OPTIX'
    bpy.context.preferences.addons['cycles'].preferences.get_devices()
    for d in bpy.context.preferences.addons['cycles'].preferences.devices: d.use = True
except Exception as e:
    print('GPU off', e)
SHOTS = (
    ('TC-5a-Render-Forecourt', P(175, -62, 9), P(168, 10, 10.5), 22),      # from across the road: Grand Hall, link gallery, East Hall
    ('TC-5b-Render-Whole-Front', P(600, -150, 40), P(250, 10, 6), 30),   # from the east: the whole front, one roof, one facade
    ('TC-5c-Render-East-Hall', P(222, 3, 1.7), P(170, 28, 6.5), 18),     # inside the East Hall: glass to the wave, check-in, meeting rail
    ('TC-5d-Render-Grand-Hall-Link', P(118, -3, 1.7), P(160, -1, 3.5), 20),  # the link gallery looking east toward the East Hall
)
only = os.environ.get('TC_SHOT')
for name, frm, to, lens in SHOTS:
    if only and only not in name: continue
    cam.lens = lens; co.location = Vector(frm); co.rotation_euler = (Vector(to) - Vector(frm)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True); print('RENDERED', sc.render.filepath)
