# Renders a pet model from every side (side, other side, front, back, top, three-quarter front and back) in plain clay,
# so its shape can be judged all round against the photos (owner: "a 3D model ... from the front and the back as well
# ... all of its legs and around its head"). Writes <out>_<view>.png and one sheet <out>_views.png.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Pets/render_views.py -- <model.ply|.glb|.blend> <out prefix>
import bpy, math, os, sys
from mathutils import Vector
a = sys.argv[sys.argv.index('--') + 1:]
src, out = a[0], a[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
if src.endswith('.ply'):
    bpy.ops.wm.ply_import(filepath=src)
    ob = bpy.context.selected_objects[0]; ob.scale = (0.01, 0.01, 0.01)        # (cm -> m)
    bpy.ops.object.transform_apply(scale=True); bpy.ops.object.shade_smooth()
elif src.endswith('.glb'):
    bpy.ops.import_scene.gltf(filepath=src)
elif src.endswith('.obj'):
    bpy.ops.wm.obj_import(filepath=src)                  # (Meshy's models: one textured mesh)
    for o in bpy.context.selected_objects: print('OBJ', o.name, len(o.data.vertices), 'verts', len(o.data.polygons), 'faces', tuple(round(d, 3) for d in o.dimensions))
else:
    bpy.ops.wm.open_mainfile(filepath=src)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
lo = Vector((min(min((o.matrix_world @ Vector(c))[i] for c in o.bound_box) for o in obs) for i in range(3)))
hi = Vector((max(max((o.matrix_world @ Vector(c))[i] for c in o.bound_box) for o in obs) for i in range(3)))
ctr = (lo + hi) / 2; size = max(hi - lo)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'TEXTURE' if src.endswith(('.obj', '.glb', '.blend')) else 'SINGLE'
sc.display.shading.single_color = (0.62, 0.6, 0.57); sc.display.shading.show_cavity = True
sc.display.shading.cavity_type = 'BOTH'; sc.display.shading.show_shadows = False
sc.render.resolution_x, sc.render.resolution_y = 1400, 1000
sc.render.film_transparent = False
w = bpy.data.worlds.new('w'); sc.world = w
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = size * 1.12
views = {'side_left': (0, 1, 0), 'side_right': (0, -1, 0), 'front': (1, 0, 0), 'back': (-1, 0, 0), 'top': (0, 0, 1),
         'three_quarter_front': (1, 0.9, 0.45), 'three_quarter_back': (-1, -0.9, 0.45)}
files = []
for name, d in views.items():
    d = Vector(d).normalized(); cam.location = ctr + d * size * 3
    up = Vector((1, 0, 0)) if name == 'top' else Vector((0, 0, 1))
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y' if name != 'top' else 'X').to_euler()
    if name == 'top': cam.rotation_euler = (0, 0, math.pi / 2)
    sc.render.filepath = '%s_%s.png' % (out, name); bpy.ops.render.render(write_still=True); files.append(sc.render.filepath)
print('VIEWS', files)
