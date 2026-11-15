# Close-up renders of a pet's head (front, three-quarter, side, from below) in clay, like the face photos are taken.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Pets/render_head.py -- <model.ply> <out prefix> <nose x y z cm>
import bpy, math, sys
from mathutils import Vector
a = sys.argv[sys.argv.index('--') + 1:]
src, out = a[0], a[1]; nose = Vector([float(v) / 100 for v in a[2:5]])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.ply_import(filepath=src); ob = bpy.context.selected_objects[0]; ob.scale = (0.01,) * 3
bpy.ops.object.transform_apply(scale=True); bpy.ops.object.shade_smooth()
sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'SINGLE'; sc.display.shading.single_color = (0.62, 0.6, 0.57)
sc.display.shading.show_cavity = True; sc.display.shading.cavity_type = 'BOTH'
sc.render.resolution_x, sc.render.resolution_y = 1000, 1000
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 85
target = nose + Vector((-0.09, 0, 0.02))          # (about the middle of the head)
for name, d in {'front': (1, 0, 0.12), 'three_quarter': (1, 0.85, 0.2), 'side': (0, 1, 0.05), 'below': (1, 0, -0.45)}.items():
    d = Vector(d).normalized(); cam.location = target + d * 0.75
    cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = '%s_head_%s.png' % (out, name); bpy.ops.render.render(write_still=True)
print('HEAD done')
