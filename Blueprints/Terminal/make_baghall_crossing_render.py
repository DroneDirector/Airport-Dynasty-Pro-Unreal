# 3D close-up of the baggage hall where the departure trains and the arrivals conveyor meet (owner 2026-11-21: "make sure
# the departure transport doesn't glitch into the arrivals transport ... a 3D render right there"). Plan x 112-300, z 50-84:
# the arrivals trunk conveyor (z 63, from the west to its end at x 190, the loading station at Ramp services 4), the new
# departure block's roller door (x 128-136, z 52), the departure lane along the hall's south side (z 54-60), turning north
# only east of x 190 and out through Ramp services 13 (x 255-283, roller doors both ends) - no crossing.
import bpy, math, os
from mathutils import Vector
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
def mat(n, c, r=0.6, m=0.0, e=0.0):
    M = bpy.data.materials.new(n); M.use_nodes = True; b = M.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = r; b.inputs['Metallic'].default_value = m
    if e: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = e
    return M
FLOOR = mat('floor', (0.45, 0.47, 0.49), 0.8); WALL = mat('wall', (0.86, 0.87, 0.88), 0.7); STEEL = mat('steel', (0.62, 0.64, 0.67), 0.35, 0.8)
BELT = mat('belt', (0.04, 0.04, 0.045), 0.7); ARR = mat('arr', (0.2, 0.55, 0.85), 0.5); DEP = mat('dep', (0.2, 0.75, 0.3), 0.5)
YEL = mat('yel', (0.95, 0.72, 0.05), 0.5); LINE = mat('line', (0.95, 0.85, 0.1), 0.6); RED = mat('red', (0.8, 0.15, 0.1), 0.5)
LAMP = mat('lamp', (1, 0.97, 0.9), 0.3, 0, 5.0); APR = mat('apron', (0.55, 0.56, 0.57), 0.9)
BAGS = [mat('b%d' % i, c) for i, c in enumerate(((0.1, 0.1, 0.12), (0.5, 0.08, 0.08), (0.08, 0.2, 0.45), (0.3, 0.3, 0.32), (0.55, 0.35, 0.15)))]
def P(x, z, h=0.0): return (x - 205.0, z - 66.0, h)
def box(n, c, s, m, rot=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c); o = bpy.context.object; o.name = n; o.scale = s; o.rotation_euler[2] = rot; o.data.materials.append(m); return o
# the hall (z 52-68), the ramp services rooms (z 68-82) and the apron beyond
box('Floor', P(206, 67, -0.1), (190, 34, 0.2), FLOOR); box('Apron', P(206, 90, -0.12), (190, 16, 0.2), APR)
box('WallS', P(206, 52, 2.5), (190, 0.3, 5), WALL)
for x0, x1 in ((112, 169.05), (197.05, 255.4), (283.4, 300)): box('WallN', P((x0 + x1) / 2, 68, 2.5), (x1 - x0, 0.3, 5), WALL)
for x0, x1 in ((112, 169.05), (197.05, 255.4), (283.4, 300)): box('WallApron', P((x0 + x1) / 2, 82, 2.5), (x1 - x0, 0.3, 5), WALL)
for x in (169.05, 197.05, 255.4, 283.4): box('RampSide', P(x, 75, 2.5), (0.3, 14, 5), WALL)
# the arrivals trunk conveyor (kept): along z 63 to its end at x 190, the loading station at Ramp services 4
L = 190.05 - 112
box('TrunkFrame', P(112 + L / 2, 63, 0.55), (L, 1.1, 0.3), STEEL); box('TrunkBelt', P(112 + L / 2, 63, 0.72), (L, 0.9, 0.04), BELT)
for k in range(int(L // 3)): box('TrunkLeg', P(113 + k * 3, 63, 0.25), (0.12, 0.9, 0.5), STEEL)
box('TrunkEnd', P(190.3, 63, 0.6), (0.6, 1.4, 1.2), ARR)
for k in range(12): box('ArrBag', P(150 + k * 3.2, 63, 0.98), (0.7, 0.45, 0.45), BAGS[k % 5], 0.2 * k)
box('Station4', P(183, 65.5, 0.03), (8, 4.5, 0.02), mat('st', (0.2, 0.4, 0.7), 0.6))
box('ArrSign', P(176, 66.5, 2.5), (6, 0.2, 1.2), ARR)
# the departure block's roller door and the green departure lane: south side of the hall, then north east of x 190
box('DoorFrameL', P(128, 52, 2.5), (0.4, 0.5, 5.0), YEL); box('DoorFrameR', P(136, 52, 2.5), (0.4, 0.5, 5.0), YEL); box('DoorRoll', P(132, 52, 4.7), (8.4, 0.6, 0.6), STEEL)
box('LaneS', P(197, 56.5, 0.02), (130, 6, 0.02), DEP)
box('LaneN', P(269.4, 62, 0.02), (14, 17, 0.02), DEP)
box('LaneRamp', P(269.4, 75, 0.02), (14, 14, 0.02), DEP)
for z in (53.5, 59.5): box('LaneLine', P(197, z, 0.035), (130, 0.15, 0.02), LINE)
# a 'NO CROSSING' marking between the lane and the conveyor
for k in range(20): box('Hatch', P(140 + k * 2.5, 61.2, 0.035), (1.2, 0.15, 0.02), RED, 0.6)
# Ramp services 13 as the departure vehicle passage: roller doors both ends
for z in (68, 82):
    box('RD13L', P(263.4, z, 2.5), (0.4, 0.5, 5), YEL); box('RD13R', P(275.4, z, 2.5), (0.4, 0.5, 5), YEL); box('RD13Roll', P(269.4, z, 4.7), (12.4, 0.6, 0.6), STEEL)
box('DepSign', P(269.4, 67.5, 3.5), (6, 0.2, 1.0), DEP)
# a departure train on its lane: our tractor (glb) and two carts, heading east
def cart(x, z):
    box('CartDeck', P(x, z, 0.55), (2.3, 1.4, 0.1), STEEL); box('CartRoof', P(x, z, 1.95), (2.3, 1.5, 0.06), mat('cr', (0.75, 0.78, 0.8), 0.4, 0.6))
    for sx in (-1, 1):
        for sz in (-1, 1): box('Post', P(x + sx * 1.1, z + sz * 0.68, 1.25), (0.06, 0.06, 1.4), STEEL)
    box('Front', P(x + 1.12, z, 1.25), (0.05, 1.4, 1.4), WALL); box('Curtain', P(x, z - 0.72, 1.6), (2.2, 0.03, 0.6), mat('cu', (0.1, 0.14, 0.35)))
    for k in range(5): box('Bag', P(x - 0.8 + k * 0.4, z + 0.1, 0.85), (0.38, 0.6, 0.5), BAGS[k % 5])
for cx in (214, 217.2): cart(cx, 56.5)
glb = os.path.join(ROOT, 'Art', 'BaggageTractor', 'baggagetractor.glb')
if os.path.exists(glb):
    before = set(bpy.data.objects); bpy.ops.import_scene.gltf(filepath=glb)
    root = bpy.data.objects.new('Tractor', None); sc.collection.objects.link(root)
    for o in [o for o in bpy.data.objects if o not in before and o.parent is None and o is not root]: o.parent = root
    root.location = Vector(P(221.2, 56.5, 0.0))
for x in range(120, 300, 10): box('Lamp', P(x, 60, 4.9), (1.2, 0.3, 0.06), LAMP)
# render from above the hall, looking at where the lane passes the conveyor's end and turns north
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 3.0; so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(45), 0, math.radians(25))
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.62, 0.72, 1)
cam = bpy.data.cameras.new('Cam'); cam.lens = 24; co = bpy.data.objects.new('Cam', cam); sc.collection.objects.link(co); sc.camera = co
sc.render.engine = 'CYCLES'; sc.cycles.samples = 96; sc.cycles.use_denoising = True; sc.render.resolution_x, sc.render.resolution_y = 3840, 2160; sc.view_settings.view_transform = 'Filmic'
for name, frm, to in (('Baggage-Hall-Crossing-1', P(170, 30, 32), P(205, 62, 0)), ('Baggage-Hall-Crossing-2', P(235, 40, 14), P(195, 62, 0))):
    co.location = Vector(frm); co.rotation_euler = (Vector(to) - Vector(frm)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(ROOT, 'Blueprints', 'Terminal', name + '.png'); bpy.ops.render.render(write_still=True); print('RENDERED', sc.render.filepath)
