# Renders the kiosk model (SM_CheckinKiosk.blend) from roughly the owner's four photo angles, in daylight on a light
# floor, for comparing it with the photos side by side (Art/Kiosk/compare_<n>.png). Not shipped; a modelling aid.
#   blender -b Art/Kiosk/SM_CheckinKiosk.blend --python Art/Kiosk/compare_renders.py -- <outdir>
import bpy, math, os, sys
from mathutils import Vector
OUT = sys.argv[sys.argv.index('--') + 1]
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.use_ssr = True; sc.eevee.use_gtao = True; sc.eevee.use_soft_shadows = True
sc.render.resolution_x, sc.render.resolution_y = 900, 1200
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.72, 0.76, 0.82, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = 1.2
sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN')); sc.collection.objects.link(sun)
sun.data.energy = 2.5; sun.rotation_euler = (math.radians(40), 0, math.radians(30))
bpy.ops.mesh.primitive_plane_add(size=20); g = bpy.context.object
gm = bpy.data.materials.new('Floor'); gm.use_nodes = True; b = gm.node_tree.nodes['Principled BSDF']
b.inputs['Base Color'].default_value = (0.55, 0.53, 0.50, 1); b.inputs['Roughness'].default_value = 0.2; g.data.materials.append(gm)
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
# (the photos' angles, read off them: the front a little from the right, the side from the right, three-quarter, the back)
for name, pos, look, lens in (('front', (0.45, -3.3, 1.25), (0, 0, 0.84), 42), ('front_left', (1.5, -2.6, 1.3), (0, 0, 0.84), 42),
                              ('side', (3.1, -0.9, 1.3), (0, 0, 0.84), 42), ('back', (0.05, 3.3, 1.2), (0, 0, 0.84), 42),
                              # the close-ups: the deck from the front left and the front, its open side from the right, its wall
                              ('close_front_left', (0.35, -0.95, 1.22), (0.05, -0.12, 1.02), 50), ('close_front', (0.05, -0.95, 1.2), (-0.05, -0.12, 1.02), 50),
                              ('close_side_right', (-0.95, -0.55, 1.12), (0.0, -0.12, 1.05), 50), ('close_front_right', (-0.45, -0.85, 1.25), (0.08, -0.12, 1.02), 50)):
    cam.location = pos; cam.data.lens = lens
    cam.rotation_euler = (Vector(look) - Vector(pos)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, 'render_%s.png' % name); bpy.ops.render.render(write_still=True)
