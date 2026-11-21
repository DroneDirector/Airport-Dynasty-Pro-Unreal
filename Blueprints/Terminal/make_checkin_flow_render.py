# 3D concept renders of Checkin-Security-Flow-v1 (owner 2026-11-21: "a 3D render ... so I really understand how it looks,
# 3-4 images"). The Grand Hall (plan x 42-114, z 0-30) cut away above 4 m: the glass wall at x 42, the columns, the 20
# desks with their weigh belts and drop hatches, the two 4 m walk-throughs at x 62 and x 78, security behind (6 lanes, its
# two doors), the service tunnel under the floor (shown in a cut-away strip) with bags on the collector, passengers.
import bpy, math, os
from mathutils import Vector
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
def mat(n, c, r=0.6, m=0.0, e=0.0, a=1.0):
    M = bpy.data.materials.new(n); M.use_nodes = True; b = M.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = r; b.inputs['Metallic'].default_value = m
    if e: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = e
    if a < 1.0: b.inputs['Alpha'].default_value = a; M.blend_method = 'BLEND'
    return M
TERR = mat('terrazzo', (0.78, 0.76, 0.72), 0.35); WALL = mat('wall', (0.9, 0.9, 0.9), 0.7); STEEL = mat('steel', (0.65, 0.67, 0.7), 0.3, 0.8)
DESK = mat('desk', (0.92, 0.93, 0.95), 0.4); DESKF = mat('deskfront', (0.12, 0.2, 0.32), 0.5); BELT = mat('belt', (0.04, 0.04, 0.045), 0.7)
GLASS = mat('glass', (0.65, 0.8, 0.88), 0.05, 0, 0, 0.35); HATCH = mat('hatch', (0.25, 0.25, 0.27), 0.5, 0.5); YEL = mat('yel', (0.95, 0.75, 0.1), 0.5)
GREEN = mat('green', (0.2, 0.7, 0.35), 0.5); SCAN = mat('scan', (0.88, 0.9, 0.93), 0.35); CONC = mat('conc', (0.5, 0.5, 0.5), 0.9)
TUN = mat('tunnel', (0.35, 0.3, 0.42), 0.8); SIGN = mat('sign', (0.1, 0.3, 0.6), 0.4, 0, 2.0); LAMP = mat('lamp', (1, 0.97, 0.9), 0.3, 0, 5.0)
BAGS = [mat('b%d' % i, c) for i, c in enumerate(((0.1, 0.1, 0.12), (0.5, 0.08, 0.08), (0.08, 0.2, 0.45), (0.3, 0.3, 0.32), (0.55, 0.35, 0.15)))]
PEOPLE = [mat('p%d' % i, c) for i, c in enumerate(((0.2, 0.25, 0.4), (0.6, 0.15, 0.15), (0.15, 0.4, 0.25), (0.45, 0.4, 0.3), (0.3, 0.3, 0.35)))]
def P(x, z, h=0.0): return (x - 78.0, z - 15.0, h)
def box(n, c, s, m, rot=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c); o = bpy.context.object; o.name = n; o.scale = s; o.rotation_euler[2] = rot; o.data.materials.append(m); return o
def cyl(n, c, r, d, m):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=d, location=c, vertices=20); o = bpy.context.object; o.name = n; o.data.materials.append(m); return o
def person(x, z, k, bag=False):
    cyl('Legs', P(x, z, 0.45), 0.16, 0.9, PEOPLE[(k + 3) % 5]); cyl('Body', P(x, z, 1.2), 0.22, 0.65, PEOPLE[k % 5])
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.13, location=P(x, z, 1.68)); bpy.context.object.data.materials.append(mat('skin%d' % k, (0.75, 0.58, 0.45)))
    if bag: box('Trolley', P(x + 0.35, z - 0.3, 0.35), (0.38, 0.25, 0.6), BAGS[k % 5])
# the hall: floor with a cut-away strip along the tunnel (z 19-20.5) so the tunnel shows, walls low, the glass wall
box('FloorA', P(78, 9.5, -0.1), (72, 19, 0.2), TERR); box('FloorB', P(78, 25.25, -0.1), (72, 9.5, 0.2), TERR)
box('FloorStripW', P(60, 19.75, -0.1), (36, 1.5, 0.2), TERR)                 # (the west half of the strip closed; the east half cut open)
box('TunnelFloor', P(96, 19.75, -2.3), (36, 1.6, 0.1), CONC); box('TunnelWallN', P(96, 20.6, -1.2), (36, 0.1, 2.2), TUN); box('TunnelWallS', P(96, 18.9, -1.2), (36, 0.1, 2.2), TUN)
box('Collector', P(96, 19.75, -1.4), (36, 0.8, 0.08), BELT); box('CollectorFrame', P(96, 19.75, -1.55), (36, 0.9, 0.2), STEEL)
for k in range(10): box('TunBag', P(80 + k * 3.3, 19.75, -1.15), (0.65, 0.42, 0.45), BAGS[k % 5], 0.15 * k)
for x in range(80, 114, 6): box('TunLamp', P(x, 20.5, -0.4), (1.0, 0.1, 0.05), LAMP)
box('WallS', P(78, 0, 2), (72, 0.3, 4), WALL); box('WallE', P(114, 15, 2), (0.3, 30, 4), WALL)
box('GlassWall', P(42, 15, 2.7), (0.06, 30, 5.4), GLASS)
for z in range(0, 31, 3): box('Mullion', P(42, z, 2.7), (0.14, 0.14, 5.4), STEEL)
for (x, z) in ((46, 6), (46, 22), (75, 6), (75, 22), (104, 6), (104, 22)): cyl('Column', P(x, z, 2.7), 0.45, 5.4, WALL)
# security behind (z 30-42): its south wall with the two 6 m doors, 6 lanes
for (x0, x1) in ((50, 59), (65, 75), (81, 90)): box('SecWall', P((x0 + x1) / 2, 30, 1.6), (x1 - x0, 0.3, 3.2), WALL)
for xd in (62, 78): box('SecDoorGlass', P(xd, 30, 1.25), (6, 0.08, 2.5), GLASS); box('SecDoorSign', P(xd, 30.1, 2.9), (4, 0.1, 0.5), mat('secsign', (0.2, 0.7, 0.35), 0.4, 0, 2.5))
box('SecFloor', P(70, 36, -0.08), (40, 12, 0.16), mat('secfloor', (0.55, 0.58, 0.6), 0.5))
for k in range(6):
    x = 53 + k * 6.2
    box('Lane', P(x, 35.5, 0.45), (0.8, 6, 0.9), BELT); box('Xray', P(x, 35.5, 0.9), (1.2, 2.2, 1.7), SCAN)
    box('Arch', P(x + 1.6, 35.5, 1.1), (0.15, 0.9, 2.2), STEEL); box('ArchTop', P(x + 1.6, 35.5, 2.2), (0.15, 0.9, 0.15), STEEL)
# the desks: 20 (2 removed opposite each security door), each with its weigh belt and the drop hatch behind
REMOVED = {6, 7, 13, 14}
for isl, xs in enumerate((47.5, 74.5)):
    for k in range(12):
        idx = isl * 12 + k
        if idx in REMOVED: continue
        x = xs + 1.05 + k * 2.08
        box('Desk', P(x, 16.4, 0.55), (1.7, 0.75, 1.1), DESK); box('DeskFront', P(x, 16.0, 0.5), (1.7, 0.04, 0.9), DESKF)
        box('Screen', P(x, 16.4, 1.35), (0.5, 0.05, 0.4), mat('scr%d' % idx, (0.3, 0.6, 0.9), 0.3, 0, 1.5))
        box('WeighBelt', P(x, 17.6, 0.45), (0.85, 1.2, 0.08), BELT); box('WeighFrame', P(x, 17.6, 0.3), (0.95, 1.3, 0.5), STEEL)
        box('Hatch', P(x, 18.65, 0.42), (0.75, 0.5, 0.84), HATCH); box('HatchFlap', P(x, 18.9, 0.6), (0.7, 0.04, 0.5), BELT)
        if k % 3 == 0: box('DeskBag', P(x, 17.6, 0.72), (0.55, 0.38, 0.45), BAGS[(idx + 1) % 5])
    box('Banner', P(xs + 12.5, 18.8, 3.6), (24, 0.25, 1.4), SIGN)
# the walk-throughs: yellow floor marks
for xd in (62, 78):
    box('WalkMark', P(xd, 17.4, 0.005), (4.0, 4.2, 0.01), mat('walk%d' % xd, (0.95, 0.8, 0.2), 0.6))
    for z in (21.5, 24, 26.5): box('Arrow', P(xd, z, 0.01), (0.6, 1.2, 0.01), YEL)
# queue posts and passengers: queues on the entrance side, a few walking through to security
for isl, xs in enumerate((47.5, 74.5)):
    for k in range(0, 12, 2): cyl('QPost', P(xs + 1.0 + k * 2.08, 12.5, 0.5), 0.04, 1.0, STEEL)
k = 0
for (x, z, b) in ((50, 14.3, True), (53, 14.2, True), (57, 13.8, True), (68, 14.0, True), (85, 14.2, True), (90, 13.9, True), (96, 14.1, True),
                  (62, 18.0, False), (61.5, 23.5, False), (78.3, 21.0, False), (77.6, 27.0, False), (75, 6, True), (60, 5, True), (100, 7, True)):
    person(x, z, k, b); k += 1
for x in range(48, 112, 8):
    for z in (8, 24): box('Lamp', P(x, z, 5.3), (1.4, 0.4, 0.06), LAMP)
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 2.6; so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(48), 0, math.radians(30))
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs[0].default_value = (0.6, 0.66, 0.74, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.9
cam = bpy.data.cameras.new('Cam'); co = bpy.data.objects.new('Cam', cam); sc.collection.objects.link(co); sc.camera = co
sc.render.engine = 'CYCLES'; sc.cycles.samples = 96; sc.cycles.use_denoising = True; sc.render.resolution_x, sc.render.resolution_y = 3840, 2160; sc.view_settings.view_transform = 'Filmic'
SHOTS = (
    ('Checkin-Render-1-Overview', P(78, -26, 34), P(78, 17, 0), 22),       # from above the entrances: the whole hall, the walk-throughs, security
    ('Checkin-Render-2-Walkthrough', P(62, 8.5, 1.65), P(62, 32, 1.4), 20),  # eye level: through the walk-through to the security door
    ('Checkin-Render-3-Desk', P(70.5, 13.2, 2.3), P(68.3, 17.8, 0.5), 30),    # a desk: the bag on its weigh belt, the hatch behind
    ('Checkin-Render-4-Tunnel', P(104, 13, 4.5), P(94, 19.75, -1.3), 24),     # the cut-away: the tunnel under the floor, bags on the collector
)
for name, frm, to, lens in SHOTS:
    cam.lens = lens; co.location = Vector(frm); co.rotation_euler = (Vector(to) - Vector(frm)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(ROOT, 'Blueprints', 'Terminal', name + '.png'); bpy.ops.render.render(write_still=True); print('RENDERED', sc.render.filepath)
