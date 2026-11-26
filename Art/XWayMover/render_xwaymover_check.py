# Two Cycles check renders of the X-Way mover (front 3/4 and side) for the builder's own look, not for the owner:
#   blender -b Art/XWayMover/XWayMover.blend --python Art/XWayMover/render_xwaymover_check.py   -> F:/ADP-Renders/xwaymover/
import bpy, math, os
from mathutils import Vector
OUT = 'F:/ADP-Renders/xwaymover'; os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.samples = 48; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.6, 0.68, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.6
sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN')); sun.data.energy = 3.5; sun.rotation_euler = (math.radians(50), 0, math.radians(35)); sc.collection.objects.link(sun)
bpy.ops.mesh.primitive_plane_add(size=40); fl = bpy.context.object; m = bpy.data.materials.new('Floor'); m.use_nodes = True
m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.25, 0.25, 0.25, 1); fl.data.materials.append(m)
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam; cam.data.lens = 40
for name, pos in (('front34', (6.2, 4.6, 2.6)), ('side', (-0.3, 7.5, 1.3)), ('rear34', (-6.0, -4.2, 2.8))):
    cam.location = Vector(pos); d = Vector((0, 0, 0.7)) - cam.location; cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True)
print('RENDERS DONE')
