# Preview renders of the street light (Eevee, daylight): the whole light and a close view of its head.
#   blender -b Art/StreetLight/StreetLight.blend --python Art/StreetLight/render_preview.py -- <outdir>
import bpy, math, os, sys
from mathutils import Vector
OUT = sys.argv[sys.argv.index('--') + 1]
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE'; sc.render.resolution_x, sc.render.resolution_y = 1080, 1350
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.65, 0.8, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.8
sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN')); sc.collection.objects.link(sun)
sun.data.energy = 4.0; sun.rotation_euler = (math.radians(50), 0, math.radians(-40))
bpy.ops.mesh.primitive_plane_add(size=40); g = bpy.context.object
gm = bpy.data.materials.new('G'); gm.use_nodes = True; gm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.25, 0.32, 0.12, 1); g.data.materials.append(gm)
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
def shot(name, pos, look, lens):
    cam.location = pos; cam.data.lens = lens
    d = Vector(look) - Vector(pos); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name); bpy.ops.render.render(write_still=True)
shot('sl_full.png', (6.5, -6.0, 2.5), (0, 0.6, 4.4), 28)
shot('sl_head.png', (1.3, 0.3, 7.6), (0, 1.35, 8.1), 50)
shot('sl_under.png', (0.9, 2.6, 6.9), (0, 1.5, 8.1), 45)
shot('sl_foot.png', (0.8, -1.2, 0.9), (0, 0, 0.6), 45)
