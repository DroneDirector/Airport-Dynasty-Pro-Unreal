# The office wardrobe (owner, 2026-11-12): a tall two-door cabinet in every office, where you change your clothes.
# Our own model: 1.2 m wide, 0.6 m deep, 2.1 m high, walnut-veneered panels on a plinth, two doors on the front with
# long brushed-steel handles, inside a chrome rail with wooden hangers and a shelf above. The doors are their own objects
# (DoorL, DoorR, hinged at their outer edges) so the game can swing them open.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Wardrobe/make_wardrobe.py -- [render_dir]
import bpy, bmesh, math, os, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
RENDER = Path(argv[0]) if argv else None
W, D, H, PLINTH, T = 1.2, 0.6, 2.1, 0.08, 0.018   # width, depth, height, plinth height, panel thickness

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, col, rough=0.5, metal=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    return m
M = {'Walnut': mat('Walnut', (0.09, 0.045, 0.025), 0.4), 'WalnutDark': mat('WalnutDark', (0.05, 0.028, 0.016), 0.45),
     'Steel': mat('Steel', (0.7, 0.7, 0.72), 0.3, 1.0), 'Chrome': mat('Chrome', (0.85, 0.85, 0.87), 0.15, 1.0),
     'Inside': mat('Inside', (0.42, 0.3, 0.18), 0.6), 'Hanger': mat('Hanger', (0.3, 0.17, 0.08), 0.5)}

def box(name, x0, y0, z0, x1, y1, z1, m, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = bpy.context.object; o.name = name
    o.scale = ((x1 - x0), (y1 - y0), (z1 - z0)); o.location = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    bpy.ops.object.transform_apply(scale=True)
    if bevel > 0:
        mod = o.modifiers.new('Bevel', 'BEVEL'); mod.width = bevel; mod.segments = 3
    o.data.materials.append(m)
    return o

def cylinder(name, x, y, z, r, length, axis, m, verts=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=length, location=(x, y, z))
    o = bpy.context.object; o.name = name
    if axis == 'x': o.rotation_euler = (0, math.pi / 2, 0)
    elif axis == 'y': o.rotation_euler = (math.pi / 2, 0, 0)
    bpy.ops.object.transform_apply(rotation=True)
    o.data.materials.append(m)
    return o

# the carcass: y runs from the back (-D/2) to the front (D/2), the doors at the front
parts = []
parts.append(box('Plinth', -W / 2 + 0.03, -D / 2 + 0.03, 0, W / 2 - 0.03, D / 2 - 0.05, PLINTH, M['WalnutDark']))
parts.append(box('Bottom', -W / 2, -D / 2, PLINTH, W / 2, D / 2 - T, PLINTH + T, M['Walnut']))
parts.append(box('Top', -W / 2, -D / 2, H - T, W / 2, D / 2, H, M['Walnut'], 0.004))
parts.append(box('SideL', -W / 2, -D / 2, PLINTH, -W / 2 + T, D / 2, H, M['Walnut'], 0.003))
parts.append(box('SideR', W / 2 - T, -D / 2, PLINTH, W / 2, D / 2, H, M['Walnut'], 0.003))
parts.append(box('Back', -W / 2 + T, -D / 2, PLINTH, W / 2 - T, -D / 2 + 0.008, H - T, M['Inside']))
parts.append(box('Shelf', -W / 2 + T, -D / 2 + 0.01, H - 0.42, W / 2 - T, D / 2 - T - 0.02, H - 0.42 + 0.016, M['Inside']))
parts.append(box('Divider', -0.006, -D / 2 + 0.01, PLINTH + T, 0.006, D / 2 - T - 0.02, H - 0.42, M['Inside']))
# the rail and six hangers inside
parts.append(cylinder('Rail', 0, 0.02, H - 0.5, 0.012, W - 2 * T - 0.04, 'x', M['Chrome']))
for i in range(6):
    # a wooden hanger: the chrome hook and the straight wooden bar it holds (plain, so nothing turns the wrong way)
    x = -0.45 + i * 0.18
    top = H - 0.5
    hk = cylinder('HangerHook%d' % i, x, 0.02, top + 0.025, 0.004, 0.07, 'z', M['Chrome'], 12)
    neck = cylinder('HangerNeck%d' % i, x, 0.02, top - 0.05, 0.006, 0.1, 'z', M['Hanger'], 10)
    bar = cylinder('HangerBar%d' % i, x, 0.02, top - 0.1, 0.008, 0.42, 'x', M['Hanger'], 10)
    parts += [hk, neck, bar]
# the two doors, each its own object with its origin on its hinge edge (the outer edge), closed flush with the front
for side, nm in ((-1, 'DoorL'), (1, 'DoorR')):
    x0 = side * (W / 2 - T) if side < 0 else 0.003
    x1 = -0.003 if side < 0 else W / 2 - T
    door = box(nm, min(x0, x1), D / 2 - T, PLINTH + T + 0.003, max(x0, x1), D / 2, H - T - 0.003, M['Walnut'], 0.003)
    # the handle: a long vertical brushed-steel bar near the door's inner edge, on two short standoffs
    hx = (-0.06 if side < 0 else 0.06)
    bar = cylinder(nm + 'Handle', hx, D / 2 + 0.03, H * 0.52, 0.006, 0.36, 'z', M['Chrome'], 16)
    s1 = cylinder(nm + 'Stand1', hx, D / 2 + 0.015, H * 0.52 + 0.15, 0.004, 0.03, 'y', M['Chrome'], 12)
    s2 = cylinder(nm + 'Stand2', hx, D / 2 + 0.015, H * 0.52 - 0.15, 0.004, 0.03, 'y', M['Chrome'], 12)
    for o in (bar, s1, s2):
        o.parent = door; o.matrix_parent_inverse = door.matrix_world.inverted()
    # the hinge: the door's origin at its outer edge
    hinge_x = side * (W / 2 - T)
    bpy.context.view_layer.objects.active = door
    sc.cursor.location = (hinge_x, D / 2 - T / 2, (PLINTH + H) / 2)
    bpy.ops.object.select_all(action='DESELECT'); door.select_set(True)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    parts += [door, bar, s1, s2]

# three objects for the game: the body (everything fixed) and the two doors with their handles, each door's origin at
# its hinge; the game swings the doors itself
names = [o.name for o in parts]
for n in names:
    o = bpy.data.objects.get(n)
    if o is not None and o.parent is not None:
        bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
def join(group, new_name):
    objs = [bpy.data.objects[n] for n in group if n in bpy.data.objects]
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.object; o.name = new_name; o.data.name = new_name
    return o
body = join([n for n in names if not n.startswith('Door')], 'WardrobeBody')
doorL = join([n for n in names if n.startswith('DoorL')], 'WardrobeDoorL')
doorR = join([n for n in names if n.startswith('DoorR')], 'WardrobeDoorR')
for d, side in ((doorL, -1), (doorR, 1)):
    sc.cursor.location = (side * (W / 2 - T), D / 2 - T / 2, 0)
    bpy.ops.object.select_all(action='DESELECT'); d.select_set(True); bpy.context.view_layer.objects.active = d
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
sc.cursor.location = (0, 0, 0)
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
parts = [body, doorL, doorR]
bpy.ops.object.select_all(action='DESELECT')
for o in parts: o.select_set(True)
out = HERE / 'wardrobe.glb'
bpy.ops.export_scene.gltf(filepath=str(out), export_format='GLB', use_selection=True, export_apply=True, export_yup=True)
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / 'Wardrobe.blend'))
print('wrote', out)

if RENDER:
    RENDER.mkdir(parents=True, exist_ok=True)
    sc.render.engine = 'CYCLES'; sc.cycles.samples = 64; sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = 900, 1100
    bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0, 0)); floor = bpy.context.object
    floor.data.materials.append(mat('Floor', (0.5, 0.5, 0.5), 0.7))
    bpy.ops.mesh.primitive_plane_add(size=8, location=(0, -D / 2 - 0.02, 2)); wall = bpy.context.object
    wall.rotation_euler = (math.pi / 2, 0, 0); wall.data.materials.append(mat('Wall', (0.85, 0.84, 0.8), 0.9))
    bpy.ops.object.light_add(type='AREA', location=(1.5, 2.5, 2.8)); l = bpy.context.object; l.data.energy = 160; l.data.size = 2
    l.rotation_euler = (math.radians(-40), math.radians(25), 0)
    sc.world = bpy.data.worlds.new('W'); sc.world.use_nodes = True
    sc.world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.5
    bpy.ops.object.camera_add(location=(1.6, 2.6, 1.5), rotation=(math.radians(78), 0, math.radians(148))); sc.camera = bpy.context.object
    sc.render.filepath = str(RENDER / 'wardrobe_closed.png'); bpy.ops.render.render(write_still=True)
    for nm, sgn in (('DoorL', -1), ('DoorR', 1)):
        bpy.data.objects['Wardrobe' + nm].rotation_euler = (0, 0, sgn * math.radians(-95))
    sc.render.filepath = str(RENDER / 'wardrobe_open.png'); bpy.ops.render.render(write_still=True)
    print('rendered')
