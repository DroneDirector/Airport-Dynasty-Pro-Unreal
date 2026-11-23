# The car park's entry and exit (owner's OK on docs/Blueprints/Terminal-Changes/v2/TCv2-2f, option 1, 2026-11-23): the lane
# barrier, the ticket column, the pay machine, the kerb islands, the steel guard rail, the give-way and the P sign. Simple
# real shapes, metres, at their own spot (z 0 = the tarmac); x = along the plan's x, y = the plan's y. Each one mesh, one .fbx;
# imported by Tools/set_carpark_props.py (Nanite), placed by Data/Sites/chablais/roads.json (Tools/carpark_exit.py).
#   blender --background --python Art/CarPark/make_carpark_props.py
import bpy, math, os
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
COL = {'CP_Yellow': (.85, .5, .02), 'CP_Dark': (.03, .032, .036), 'CP_White': (.8, .8, .8), 'CP_Red': (.6, .012, .012), 'CP_Steel': (.42, .43, .44),
       'CP_Concrete': (.4, .39, .37), 'CP_Screen': (.02, .12, .3), 'CP_Blue': (.004, .07, .4)}
M = {}
for n, c in COL.items():
    m = bpy.data.materials.new(n); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']; b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = .5; M[n] = m
P, DONE = [], []


def keep(o, mat):
    o.data.materials.clear(); o.data.materials.append(M[mat]); P.append(o); return o


def box(x0, y0, z0, x1, y1, z1, mat, bev=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x0+x1)/2, (y0+y1)/2, (z0+z1)/2)); o = bpy.context.object
    o.scale = (x1-x0, y1-y0, z1-z0); bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bev:
        k = o.modifiers.new('b', 'BEVEL'); k.width = bev; k.segments = 3; k.limit_method = 'ANGLE'; bpy.ops.object.modifier_apply(modifier='b')
    return keep(o, mat)


def cyl(at, depth, r, mat, n=24, rot=None):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=depth, vertices=n, location=at); o = bpy.context.object
    if rot: o.rotation_euler = rot; bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    return keep(o, mat)


def text(s, size, at, rot, mat, depth=.004):
    bpy.ops.object.text_add(location=at, rotation=rot); o = bpy.context.object
    o.data.body = s; o.data.size = size; o.data.align_x = 'CENTER'; o.data.align_y = 'CENTER'; o.data.extrude = depth
    bpy.ops.object.convert(target='MESH'); return keep(bpy.context.object, mat)


def poly(pts, y0, y1, mat):
    n = len(pts); me = bpy.data.meshes.new('p')
    me.from_pydata([(x, y0, z) for x, z in pts] + [(x, y1, z) for x, z in pts], [], [tuple(range(n)), tuple(range(2*n-1, n-1, -1))] + [(i, i+n, (i+1) % n+n, (i+1) % n) for i in range(n)])
    o = bpy.data.objects.new('p', me); bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
    return keep(o, mat)


def out(name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in P: o.select_set(True)
    bpy.context.view_layer.objects.active = P[0]; bpy.ops.object.join(); o = bpy.context.object
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    o.name = o.data.name = name
    bpy.ops.object.shade_flat()
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    P.clear(); DONE.append(o)


# the barrier: its post at 0, the arm 3.35 m along +x at 0.95 m, red and white, the rubber lip under it, a counterweight
box(-.16, -.16, 0, .16, .16, 1.08, 'CP_Yellow', .02); box(-.19, -.19, 0, .19, .19, .05, 'CP_Dark', .006); box(-.165, -.165, 1.08, .165, .165, 1.12, 'CP_Dark', .01)
box(-.05, -.163, .55, .05, -.1605, .8, 'CP_Dark')
box(-.34, -.24, .88, .14, -.18, 1.02, 'CP_Dark', .01); cyl((0, -.2, .95), .09, .06, 'CP_Steel', rot=(math.pi/2, 0, 0))
x = .14
for i in range(8): box(x, -.24, .905, x + .40, -.19, .995, 'CP_Red' if i % 2 == 0 else 'CP_White', .004); x += .4
box(.14, -.225, .88, 3.34, -.205, .905, 'CP_Dark')
out('cpbarrier')

# the ticket column (a reader, its slot, the help button): it faces -x (the driver's window)
box(-.14, -.17, 0, .14, .17, 1.42, 'CP_Yellow', .03); box(-.17, -.2, 0, .17, .2, .04, 'CP_Dark', .006)
box(-.15, -.13, .78, -.141, .13, 1.36, 'CP_Dark', .003); box(-.154, -.09, 1.18, -.1505, .09, 1.31, 'CP_Screen'); box(-.156, -.06, 1.06, -.1505, .06, 1.085, 'CP_Steel')
cyl((-.154, .05, .92), .012, .03, 'CP_Red', rot=(0, math.pi/2, 0)); cyl((-.154, -.05, .92), .012, .03, 'CP_White', rot=(0, math.pi/2, 0))
box(-.19, -.18, 1.42, .16, .18, 1.46, 'CP_Dark', .01)
out('cpticket')

# the pay machine: it faces +y (the pavement), 0.75 m wide, its P on top
UP, BK = (math.pi/2, 0, math.pi), (math.pi/2, 0, 0)
box(-.375, -.22, 0, .375, .22, 1.78, 'CP_Dark', .02); box(-.40, -.245, 0, .40, .245, .06, 'CP_Steel', .008)
box(-.3, .221, .7, .3, .226, 1.62, 'CP_Steel', .002); box(-.2, .2265, 1.2, .2, .231, 1.52, 'CP_Screen'); box(.06, .2265, 1.0, .2, .2295, 1.14, 'CP_Dark')
box(-.2, .2265, 1.04, -.04, .2295, 1.07, 'CP_Dark'); box(-.2, .2265, .78, .2, .236, .94, 'CP_Dark', .004)
box(-.375, -.06, 1.78, .375, .06, 2.16, 'CP_Blue', .01)
text('P', .34, (0, .064, 1.97), UP, 'CP_White'); text('P', .34, (0, -.064, 1.97), BK, 'CP_White')
text('CASH  CARD', .07, (0, .2285, 1.57), UP, 'CP_Dark', .001)
out('cppay')

# kerb islands: 1 m of the narrow one (0.3 wide), 1 m of the wide one (2.9 wide), 15 cm high; the yellow nose, 0.5 m
box(-.15, -.5, -.1, .15, .5, .15, 'CP_Concrete'); out('cpkerb')
box(-1.45, -.5, -.1, 1.45, .5, .15, 'CP_Concrete'); out('cpisland')
box(-.152, -.25, -.1, .152, .25, .152, 'CP_Yellow'); out('cpkerbend')

# 2 m of steel guard rail (a W beam on two posts), along y, its beam toward +x
for yy in (-.5, .5): box(-.05, yy - .03, -.2, .05, yy + .03, .78, 'CP_Steel')
pts = [(.052, .44), (.09, .47), (.13, .52), (.09, .57), (.055, .6), (.09, .63), (.13, .68), (.09, .73), (.052, .76)]
d = .004
for (xa, za), (xb, zb) in zip(pts, pts[1:]):
    me = bpy.data.meshes.new('w')
    me.from_pydata([(xa, -1, za), (xb, -1, zb), (xb, 1, zb), (xa, 1, za), (xa - d, -1, za), (xb - d, -1, zb), (xb - d, 1, zb), (xa - d, 1, za)], [],
                   [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (3, 2, 6, 7)])
    o = bpy.data.objects.new('w', me); bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
    keep(o, 'CP_Steel')
out('cprail')

# the give-way sign (0.9 m, point down, white with the red edge) and the P sign (0.6 m), facing +y, 2.2 m up
cyl((0, 0, 1.45), 2.9, .03, 'CP_Steel', 16); s, h, z = .45, .78, 2.1
T = ((-s, z + h), (0, z), (s, z + h)); f, zc = .68, z + h * 2/3
poly(T, .035, .045, 'CP_Red'); poly([(x * f, zc + (zz - zc) * f) for x, zz in T], .0452, .048, 'CP_White'); poly(T, .031, .0348, 'CP_Steel')
out('cpgiveway')
cyl((0, 0, 1.45), 2.9, .03, 'CP_Steel', 16); box(-.3, .035, 2.25, .3, .045, 2.85, 'CP_Blue', .004); text('P', .5, (0, .05, 2.55), UP, 'CP_White', .003)
out('cpsign')
for i, o in enumerate(DONE): o.location.x = i * 4
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'CarPark.blend'))
for o in DONE: print('CP', o.name, len(o.data.polygons), [m.name for m in o.data.materials])
