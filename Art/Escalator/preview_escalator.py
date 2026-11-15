# Preview pictures of the escalators: both units side by side with their steps laid on the path (as the game will
# move them), lit, from the hall, from the side, close at the bottom comb and from the top landing.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Escalator/preview_escalator.py -- <out folder> [phase 0..1]
import bpy, json, math, os, sys
from mathutils import Vector, Euler
a = sys.argv[sys.argv.index('--') + 1:]
OUT = a[0]; PH = float(a[1]) if len(a) > 1 else 0.0
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.open_mainfile(filepath=os.path.join(HERE, 'Escalator.blend'))
J = json.load(open(os.path.join(os.path.dirname(os.path.dirname(HERE)), 'Data', 'escalator.json')))
P = J['path']; PITCH = J['pitch']; L = P[-1][0]
def at(s):
    for i in range(1, len(P)):
        if P[i][0] >= s:
            t = (s - P[i - 1][0]) / max(1e-9, P[i][0] - P[i - 1][0])
            return P[i - 1][1] + (P[i][1] - P[i - 1][1]) * t, P[i - 1][2] + (P[i][2] - P[i - 1][2]) * t
    return P[-1][1], P[-1][2]
step = bpy.data.objects['SM_EscStep']; body = bpy.data.objects['SM_EscBody']; ramp = bpy.data.objects['SM_EscRamp']
ramp.hide_render = True
sc = bpy.context.scene
UNIT = 1.62
for k, dx in enumerate((0.0, UNIT)):
    if k:
        b2 = body.copy(); b2.location.x = dx; sc.collection.objects.link(b2)
    n = int(L / PITCH)
    for i in range(n):
        s = (i * PITCH + PH * PITCH) % L
        u, hh = at(s)
        o = step.copy(); o.location = (dx, u, hh); sc.collection.objects.link(o)
step.hide_render = True
# floors: the hall at 0 with the bottom pit opening, the landing slab at +6
def slab(name, x0, x1, y0, y1, z0, z1, col):
    bpy.ops.mesh.primitive_cube_add(location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    o = bpy.context.active_object; o.scale = ((x1 - x0) / 2, (y1 - y0) / 2, (z1 - z0) / 2); o.name = name
    m = bpy.data.materials.new(name); m.use_nodes = True; m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*col, 1)
    m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.35; o.data.materials.append(m)
slab('hall', -8, 10, -12, -1.56, -0.3, -0.004, (0.78, 0.76, 0.72))
slab('landing', -8, 10, 15.32, 24, 5.4, 5.996, (0.78, 0.76, 0.72))
sc.render.engine = 'BLENDER_EEVEE_NEXT'
sc.render.resolution_x, sc.render.resolution_y = 1600, 900
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.8, 0.82, 0.86, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.8
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); sun.data.energy = 3.5; sun.rotation_euler = (0.9, 0.15, 0.6); sc.collection.objects.link(sun)
# glass see-through
g = bpy.data.materials.get('M_EscGlass')
if g:
    b = g.node_tree.nodes['Principled BSDF']; b.inputs['Alpha'].default_value = 0.25
    try: g.surface_render_method = 'BLENDED'
    except Exception: g.blend_method = 'BLEND'
lt = bpy.data.materials.get('M_EscLight')
if lt: lt.node_tree.nodes['Principled BSDF'].inputs['Emission Color'].default_value = (0.8, 0.9, 1, 1); lt.node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value = 4
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
views = {'newel': ((-1.6, -2.6, 1.5), (-0.62, -1.2, 0.85), 55), 'hall': ((0.8, -7.0, 1.7), (0.8, 6, 2.8), 40), 'side': ((-12, 6.0, 4.0), (0.8, 6.5, 2.6), 45),
         'comb': ((0.25, -1.6, 1.0), (0.2, 0.4, 0.0), 50), 'top': ((0.6, 18.5, 7.7), (0.8, 9, 3.5), 45)}
os.makedirs(OUT, exist_ok=True)
for n, (loc, tgt, lens) in views.items():
    cam.location = loc; cam.data.lens = lens * 0.6
    cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, 'esc_%s.png' % n); bpy.ops.render.render(write_still=True)
print('PREVIEW done')
