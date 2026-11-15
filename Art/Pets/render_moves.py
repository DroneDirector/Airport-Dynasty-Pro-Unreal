# A move sheet for a pet: chosen frames of each of its moves, seen from the side and three-quarter front, with its
# texture, on one picture per move (Models/Views/<Name>_<Move>.png) - to check the moves before they go to the game.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Pets/render_moves.py -- <Dogs|Cats> <Name> [Move ...]
import bpy, math, os, sys
from mathutils import Vector
a = sys.argv[sys.argv.index('--') + 1:]
KIND, NAME, ONLY = a[0], a[1], a[2:]
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'Models', 'Views')
bpy.ops.wm.open_mainfile(filepath=os.path.join(HERE, 'Models', KIND, NAME, NAME + '.blend'))
sc = bpy.context.scene
rig = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in bpy.data.objects if o.type == 'MESH'][0]
L = mesh.dimensions.y; H = mesh.dimensions.z
sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'TEXTURE'
sc.display.shading.show_cavity = True; sc.display.shading.cavity_type = 'BOTH'
sc.render.resolution_x, sc.render.resolution_y = 560, 420
w = bpy.data.worlds.new('w'); sc.world = w
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = L * 1.45
# a ground line: a thin dark plate at z 0
bpy.ops.mesh.primitive_plane_add(size=L * 3, location=(0, 0, -0.002))
g = bpy.context.active_object; m = bpy.data.materials.new('g'); m.diffuse_color = (0.25, 0.27, 0.3, 1); g.data.materials.append(m)
VIEWS = {'side': Vector((1, 0, 0.08)), 'front34': Vector((0.8, -0.9, 0.35))}
ctr = Vector((0, 0, H * 0.45))
for act in bpy.data.actions:
    if ONLY and act.name not in ONLY: continue
    rig.animation_data.action = act
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    frames = [round(f0 + (f1 - f0) * k / 5) for k in range(6)]
    files = []
    for vn, d in VIEWS.items():
        d = d.normalized(); cam.location = ctr + d * L * 4
        cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
        for f in frames:
            sc.frame_set(f + 1); sc.frame_set(f); bpy.context.view_layer.update()
            sc.render.filepath = os.path.join(OUT, '_m_%s_%d.png' % (vn, f))
            if f == frames[0]: bpy.ops.render.render()          # (the first picture after a new move came out stale)
            bpy.ops.render.render(write_still=True); files.append(sc.render.filepath)
    print('SHEET', act.name, '|'.join(files))
