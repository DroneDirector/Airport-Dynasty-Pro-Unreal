# Very fast 3D concept render (owner 2026-11-22): the non-Schengen concourse - a cutaway from the forecourt side.
import bpy, math, os
from mathutils import Vector
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
def mat(n, c, e=0.0, a=1.0, r=0.5, m=0.0):
    M = bpy.data.materials.new(n); M.use_nodes = True; b = M.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = r; b.inputs['Metallic'].default_value = m
    if e: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = e
    if a < 1: b.inputs['Alpha'].default_value = a; M.blend_method = 'BLEND'
    return M
FLOOR = mat('floor', (0.82, 0.8, 0.76), r=0.25); WALL = mat('wall', (0.9, 0.9, 0.92)); DARK = mat('dark', (0.18, 0.2, 0.24))
WOOD = mat('wood', (0.25, 0.14, 0.08), r=0.4); GLASS = mat('glass', (0.6, 0.8, 0.9), a=0.25, r=0.05)
ROOF = mat('roof', (0.75, 0.77, 0.8), r=0.4, m=0.6); APR = mat('apron', (0.45, 0.46, 0.48), r=0.9); ROAD = mat('road', (0.18, 0.18, 0.2), r=0.9)
SHOPS = [mat('s%d' % i, c, e=0.25) for i, c in enumerate([(0.95, 0.75, 0.2), (0.9, 0.35, 0.25), (0.3, 0.6, 0.95), (0.4, 0.85, 0.5), (0.85, 0.5, 0.9), (0.95, 0.95, 0.95)])]
PINK = mat('pink', (1.0, 0.35, 0.6), e=0.25); GREEN = mat('green', (0.25, 0.85, 0.45), e=0.8); AC = mat('ac', (0.93, 0.94, 0.96), r=0.3)
LAMP = mat('lamp', (1, 0.97, 0.9), e=6)
# x along the building, y from the forecourt (0) toward the apron (+), z up; one 60 m slice round gate 14
def box(x0, y0, z0, x1, y1, z1, m, n='b'):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)); o = bpy.context.object
    o.scale = (x1 - x0, y1 - y0, z1 - z0); o.data.materials.append(m); o.name = n; return o
X0, X1 = -40, 40
box(X0 - 20, -30, -0.2, X1 + 20, 0, 0, ROAD); box(X0 - 20, 82, -0.2, X1 + 20, 150, 0, APR)
# ground floor (new halls), floor 2 slab
box(X0, 0, -0.2, X1, 68, 0, FLOOR); box(X0, 20, 6.3, X1, 68, 6.5, FLOOR)
box(X0, 0, 6.3, X1, 20, 6.5, PINK, 'corridor')     # arrivals corridor strip (pink)
# ground floor contents: passport booths, belts
for i in range(6): box(-30 + i * 10, 10, 0, -26 + i * 10, 13, 1.2, DARK)
for k in (-20, 15): box(k - 8, 35, 0, k + 8, 40, 0.9, DARK)
# floor 2: concourse shops (yellow etc.), seats, toilets
names = ['duty free', 'food court', 'lounge', 'toilets', 'seating', 'transfer']
w = (X1 - X0) / 6
for i, m in enumerate(SHOPS):
    x = X0 + i * w
    box(x + 0.5, 22, 6.5, x + w - 0.5, 34, 10.5, m)
    box(x + 0.5, 34, 6.5, x + w - 0.5, 34.3, 10.5, GLASS)
for i in range(16): box(X0 + 3 + i * 5, 40, 6.5, X0 + 5.5 + i * 5, 42, 7.0, WOOD)   # seat rows
box(X0, 52, 6.5, X1, 68, 6.55, mat('walk', (0.3, 0.2, 0.12), r=0.4))                 # walkway floor (dark wood)
box(X0, 59, 6.55, X1, 61, 6.7, DARK)                                                   # moving walkway
# gate 14 + glass wall onto the ramp + jetway + aircraft
box(-17, 68, 6.5, 17, 82, 6.6, mat('gate', (0.3, 0.2, 0.12)))
box(X0, 82, 6.5, X1, 82.2, 13, GLASS)
for i in range(10): box(-15 + i * 3.2, 72, 6.6, -13.5 + i * 3.2, 78, 7.1, WOOD)
bpy.ops.mesh.primitive_cylinder_add(radius=1.4, depth=18, location=(0, 90, 7.5), rotation=(math.radians(90), 0, 0)); bpy.context.object.data.materials.append(WALL)
bpy.ops.mesh.primitive_cylinder_add(radius=2.0, depth=40, location=(0, 108, 5.0), rotation=(0, math.radians(90), 0)); bpy.context.object.data.materials.append(AC)
box(-19, 103, 4.5, 19, 113, 5.0, AC)   # wings (simple)
# roof: a curved shell from the glass front (y 0, z 22) to the airside (y 82, z 15)
bm = bpy.data.meshes.new('roof'); verts = []; faces = []
N = 24
for j in range(N + 1):
    y = 40 + j * 42 / N; z = 22 + 3.5 * math.sin(math.pi * y / 82 * 0.9) - 7 * (y / 82)
    verts += [(X0, y, z), (X1, y, z)]
for j in range(N): faces.append((2 * j, 2 * j + 1, 2 * j + 3, 2 * j + 2))
bm.from_pydata(verts, [], faces); ro = bpy.data.objects.new('roof', bm); sc.collection.objects.link(ro); bm.materials.append(ROOF)
for x in range(X0, X1 + 1, 10):
    for y in range(44, 82, 12): box(x - 0.6, y - 0.2, 21.2 - 7 * (y / 82) + 3, x + 0.6, y + 0.2, 21.4 - 7 * (y / 82) + 3, LAMP)
# glass front with mullions, tree pillars
box(X0, -0.15, 0, X1, 0.15, 6.3, GLASS)
for x in range(X0, X1 + 1, 5): box(x - 0.08, -0.25, 0, x + 0.08, 0.25, 6.3, ROOF)

# the cut: drop the near gable so we look in from the east end
sun = bpy.data.lights.new('sun', 'SUN'); sun.energy = 3.5; so = bpy.data.objects.new('sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(35))
wd = bpy.data.worlds.new('w'); sc.world = wd; wd.use_nodes = True; wd.node_tree.nodes['Background'].inputs[0].default_value = (0.35, 0.45, 0.6, 1)
cam = bpy.data.cameras.new('c'); cam.lens = 30; co = bpy.data.objects.new('c', cam); sc.collection.objects.link(co); sc.camera = co
co.location = (105, -55, 48); co.rotation_euler = (Vector((0, 40, 4)) - co.location).to_track_quat('-Z', 'Y').to_euler()
sc.render.engine = 'CYCLES'; sc.cycles.samples = 40; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 1920, 1080; sc.view_settings.view_transform = 'Filmic'
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'NonSchengen-Quick-3D.png'); sc.render.filepath = out
bpy.ops.render.render(write_still=True); print('RENDERED', out)
