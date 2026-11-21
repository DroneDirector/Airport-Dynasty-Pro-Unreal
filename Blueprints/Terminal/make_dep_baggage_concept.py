# A 3D concept render of the departure baggage block (owner 2026-11-21: "a 3D render so I can see how it's supposed to
# look ... everything that was there before in that area gone"). Cut-away (no roof) of the old office/academy block, plan
# x 114-156, z 0-52 (Blender x = plan x - 135, y = plan z - 26), the overhead link from the Grand Hall, the CT lines, manual
# search, sorter, two make-up carousels, the cart lane with our baggage tractor and carts, the roller door to the baggage hall.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Blueprints/Terminal/make_dep_baggage_concept.py
import bpy, math, os
from mathutils import Vector
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'Blueprints', 'Terminal')
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(n, c, rough=0.6, metal=0.0, emit=0.0):
    m = bpy.data.materials.new(n); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    if emit: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = emit
    return m
FLOOR = mat('floor', (0.42, 0.44, 0.46), 0.8); EPOXY = mat('epoxy', (0.22, 0.30, 0.34), 0.35)
WALL = mat('wall', (0.86, 0.87, 0.88), 0.7); STEEL = mat('steel', (0.62, 0.64, 0.67), 0.35, 0.8)
BELT = mat('belt', (0.04, 0.04, 0.045), 0.7); CT = mat('ct', (0.92, 0.93, 0.95), 0.3)
CTBAND = mat('ctband', (0.1, 0.55, 0.8), 0.4); YEL = mat('yellow', (0.95, 0.72, 0.05), 0.5)
RED = mat('red', (0.75, 0.12, 0.1), 0.5); GLASS = mat('glass', (0.6, 0.75, 0.8), 0.05)
GREEN = mat('green', (0.15, 0.6, 0.25), 0.5); LINE = mat('line', (0.95, 0.85, 0.1), 0.6)
LAMP = mat('lamp', (1, 0.97, 0.9), 0.3, 0, 6.0); BAGS = [mat('bag%d' % i, c, 0.6) for i, c in enumerate(((0.1, 0.1, 0.12), (0.5, 0.08, 0.08), (0.08, 0.2, 0.45), (0.3, 0.3, 0.32), (0.55, 0.35, 0.15)))]

def box(n, c, s, m, rot=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c); o = bpy.context.object; o.name = n
    o.scale = s; o.rotation_euler[2] = rot; o.data.materials.append(m); return o
def cyl(n, c, r, d, m, axis='z', v=32):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=d, location=c, vertices=v); o = bpy.context.object; o.name = n
    if axis == 'x': o.rotation_euler[1] = math.pi / 2
    if axis == 'y': o.rotation_euler[0] = math.pi / 2
    o.data.materials.append(m); return o
def P(px, pz, h=0.0): return (px - 135.0, pz - 26.0, h)

# the slab and the block's outer walls (cut away: 4 m high, no roof), the hall's east wall stub and the overhead link
box('Slab', P(135, 26, -0.1), (42, 52, 0.2), FLOOR)
for (x, z, sx, sz) in ((114, 26, 0.3, 52), (156, 26, 0.3, 52), (135, 0, 42, 0.3), (135, 52, 42, 0.3)):
    box('Wall', P(x, z, 2.0), (sx, sz, 4.0), WALL)
box('HallFloor', P(105, 18, -0.1), (18, 24, 0.2), mat('hall', (0.75, 0.72, 0.66), 0.5))
# 1-2. the overhead link: an enclosed conveyor bridge 3.5 m up from the check-in island end through the wall
box('LinkBox', P(105, 18.6, 3.5), (18.5, 1.6, 1.3), mat('linkclad', (0.8, 0.82, 0.85), 0.4))
for x in (98, 104, 110): box('LinkPost', P(x, 18.6, 1.4), (0.25, 0.25, 2.8), STEEL)
box('IslandEnd', P(93, 17.8, 0.5), (6, 2.2, 1.0), mat('desk', (0.9, 0.9, 0.92), 0.5))
# the incline down inside the block to the screening lines
box('Incline', P(117.5, 18.6, 1.9), (7, 1.2, 0.3), STEEL).rotation_euler[0] = 0
def belt(n, x0, z0, x1, z1, h=0.9, w=1.0):
    L = math.hypot(x1 - x0, z1 - z0); a = math.atan2(z1 - z0, x1 - x0)
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    box(n + 'Frame', P(cx, cz, h - 0.15), (L, w + 0.12, 0.25), STEEL, a)
    box(n + 'Belt', P(cx, cz, h + 0.0), (L, w, 0.04), BELT, a)
    for k in range(int(L // 2.5) + 1):
        t = k / max(1, int(L // 2.5)); x = x0 + (x1 - x0) * t; z = z0 + (z1 - z0) * t
        box(n + 'Leg', P(x, z, (h - 0.3) / 2), (0.12, 0.12, h - 0.3), STEEL)
# 3. two hold-baggage screening lines with CT scanners
for zz in (7.0, 13.0):
    belt('HBS%d' % zz, 115, zz, 141, zz)
    box('CT', P(127, zz, 1.1), (4.2, 2.4, 2.2), CT); box('CTband', P(127, zz, 1.6), (4.25, 2.45, 0.25), CTBAND)
    box('CTtunnel', P(127, zz, 1.0), (4.3, 1.2, 0.8), BELT)
belt('Feed', 114, 18.6, 118, 18.6, 1.4); belt('Split', 118, 7, 118, 18.6, 1.0)
belt('Merge', 141, 7, 141, 25, 1.0)
# 4. the level-3 manual search room (glass-fronted) and the early-bag store
box('SearchFloor', P(148.75, 8, 0.02), (13, 13.5, 0.04), EPOXY)
for (x, z, sx, sz) in ((142, 8, 0.15, 13.5), (148.75, 15, 13.5, 0.15)): box('SearchWall', P(x, z, 1.5), (sx, sz, 3.0), GLASS)
box('SearchTable', P(148, 8, 0.45), (3, 1.2, 0.9), STEEL); box('SearchTable2', P(152, 5, 0.45), (3, 1.2, 0.9), STEEL)
box('StoreRack', P(148.75, 19, 1.2), (12, 1.0, 2.4), STEEL)
for k in range(10): box('StoreBag', P(143.5 + k * 1.15, 19, 1.9), (0.7, 0.45, 0.5), BAGS[k % 5])
# 5. the sorter (a long belt with diverters) across the block
belt('Sorter', 115, 25, 156, 25, 1.0, 1.2)
for x in (124, 145): box('Diverter', P(x, 25.8, 1.3), (2.2, 0.3, 0.6), YEL)
# 6. two make-up carousels (flat-plate ovals) with chutes from the sorter
def carousel(cx, cz):
    L, R = 10.0, 2.2
    for s in (-1, 1): box('MuStraight', P(cx, cz + s * R, 0.75), (L, 1.1, 0.3), STEEL)
    for e in (-1, 1):
        for k in range(10):
            a = math.pi / 2 + e * (math.pi / 2) - (math.pi / 2) + k * math.pi / 9 * (1 if e > 0 else 1)
            ang = (-math.pi / 2 + k * math.pi / 9) if e > 0 else (math.pi / 2 + k * math.pi / 9)
            box('MuEnd', P(cx + e * L / 2 + R * math.cos(ang), cz + R * math.sin(ang), 0.75), (1.1, 1.1, 0.3), STEEL)
    box('MuIsland', P(cx, cz, 0.6), (L, 3.2, 1.2), mat('island', (0.2, 0.22, 0.25), 0.6))
    box('MuChute', P(cx, cz + 4.0, 1.4), (1.2, 4.0, 0.2), STEEL).rotation_euler[0] = -0.35
    for k in range(14):
        t = k / 14 * 2 * math.pi
        x = cx + (L / 2 + R) * 0.92 * math.cos(t); z = cz + R * 1.0 * math.sin(t)
        box('MuBag', P(x, z, 1.15), (0.65, 0.42, 0.45), BAGS[k % 5], t)
carousel(124, 33.5); carousel(145, 33.5)
# 7. the cart lane: painted lines, our tractor with two baggage carts beside each carousel
box('Lane', P(135, 44.5, 0.01), (41, 12, 0.02), EPOXY)
for z in (38.6, 50.6): box('LaneLine', P(135, z, 0.03), (40, 0.15, 0.02), LINE)
for k in range(6): box('Hatch', P(116 + k * 1.2, 51.3, 0.03), (0.15, 1.2, 0.02), LINE, 0.7)
def cart(x, z):
    box('CartDeck', P(x, z, 0.55), (2.3, 1.4, 0.1), STEEL); box('CartRoof', P(x, z, 1.95), (2.3, 1.5, 0.06), mat('cartroof', (0.75, 0.78, 0.8), 0.4, 0.6))
    for sx in (-1, 1):
        for sz in (-1, 1): box('CartPost', P(x + sx * 1.1, z + sz * 0.68, 1.25), (0.06, 0.06, 1.4), STEEL)
    box('CartFront', P(x + 1.12, z, 1.25), (0.05, 1.4, 1.4), WALL); box('CartCurtain', P(x, z - 0.72, 1.6), (2.2, 0.03, 0.6), mat('curtain', (0.1, 0.14, 0.35), 0.7))
    for sx in (-1, 1):
        for sz in (-1, 1): cyl('CartWheel', P(x + sx * 0.8, z + sz * 0.6, 0.2), 0.2, 0.12, BELT, 'y', 16)
    box('CartBar', P(x + 1.6, z, 0.4), (1.0, 0.08, 0.06), STEEL)
    for k in range(5): box('CartBag', P(x - 0.8 + k * 0.4, z + 0.1, 0.85), (0.38, 0.6, 0.5), BAGS[(k + int(x)) % 5])
glb = os.path.join(ROOT, 'Art', 'BaggageTractor', 'baggagetractor.glb')
for cx in (124, 145):
    cart(cx - 2.3, 41.5); cart(cx - 5.4, 41.5)
    if os.path.exists(glb):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=glb)
        new = [o for o in bpy.data.objects if o not in before]
        root = bpy.data.objects.new('Tractor', None); sc.collection.objects.link(root)
        for o in new:
            if o.parent is None: o.parent = root
        root.location = Vector(P(cx + 1.8, 41.5, 0.0))
# 8. the roller door to the baggage hall (8 m, open) and a crew member's height marker
box('DoorFrameL', P(128, 52, 2.5), (0.4, 0.5, 5.0), YEL); box('DoorFrameR', P(136, 52, 2.5), (0.4, 0.5, 5.0), YEL)
box('DoorRoll', P(132, 52, 4.7), (8.4, 0.6, 0.6), STEEL)
box('WallGap', P(132, 52, 2.0), (7.6, 0.4, 4.0), FLOOR).hide_render = True
# the wall where the door is: cut the opening by splitting the wall into two pieces (the plain wall above is replaced)
for o in [o for o in bpy.data.objects if o.name.startswith('Wall') and abs(o.location.y - (52 - 26)) < 0.01 and o.scale.x > 30]:
    bpy.data.objects.remove(o, do_unlink=True)
box('WallN_W', P((114 + 128) / 2, 52, 2.0), (14, 0.3, 4.0), WALL); box('WallN_E', P((136 + 156) / 2, 52, 2.0), (20, 0.3, 4.0), WALL)
# lights over the work areas
for x in range(118, 156, 6):
    for z in (10, 25, 33, 45): box('Lamp', P(x, z, 4.6), (1.2, 0.3, 0.06), LAMP)

# render: a high corner looking down into the block, sun + sky
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 3.5; so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(35))
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.62, 0.72, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.8
cam = bpy.data.cameras.new('Cam'); cam.lens = 26; co = bpy.data.objects.new('Cam', cam); sc.collection.objects.link(co); sc.camera = co
def look(frm, to):
    co.location = Vector(frm); d = Vector(to) - Vector(frm); co.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
sc.render.engine = 'CYCLES'; sc.cycles.samples = 96; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 3840, 2160
sc.view_settings.view_transform = 'Filmic'
for name, frm, to in (('Departure-Baggage-Concept-1', P(92, -14, 38), P(136, 28, 0)), ('Departure-Baggage-Concept-2', P(170, 72, 30), P(132, 26, 0))):
    look(frm, to); sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True); print('RENDERED', sc.render.filepath)
