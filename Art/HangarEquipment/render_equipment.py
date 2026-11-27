import os
# One simple render of a hangar equipment model (to-do 364 A): blender -b <Name>.blend --python render_equipment.py -- <out.png> [az]
import bpy, sys, math
from mathutils import Vector
out = os.path.abspath(sys.argv[sys.argv.index('--') + 1]); az = math.radians(float(sys.argv[sys.argv.index('--') + 2]) if len(sys.argv) > sys.argv.index('--') + 2 else 35.0)
sc = bpy.context.scene
ms = [o for o in sc.objects if o.type == 'MESH']
pts = [o.matrix_world @ Vector(c) for o in ms for c in o.bound_box]
lo = Vector([min(p[i] for p in pts) for i in range(3)]); hi = Vector([max(p[i] for p in pts) for i in range(3)])
c = (lo + hi) / 2; r = (hi - lo).length / 2
bpy.ops.mesh.primitive_plane_add(size=60, location=(0, 0, -0.001))
fm = bpy.data.materials.new('Floor'); fm.use_nodes = True
fm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.25, 0.25, 0.26, 1)
fm.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.6
bpy.context.object.data.materials.append(fm)
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
WIDE = os.environ.get('HQ_RES') == '3840x2160'     # 16:9 picture (UHD); else 4:3
cam.data.lens = 50; d = r / ((10.125 if WIDE else 13.5) / 50) * 1.05   # fit the bounding sphere to the picture's height
el = math.radians(18)
cam.location = c + Vector((math.sin(az) * math.cos(el), math.cos(az) * math.cos(el), math.sin(el))) * d
cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN')); sc.collection.objects.link(sun)
sun.data.energy = 3.5; sun.rotation_euler = (math.radians(50), 0, math.radians(30))
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.35, 0.37, 0.4, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.8
sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.use_gtao = True; sc.eevee.use_soft_shadows = True
sc.render.resolution_x, sc.render.resolution_y = (3840, 2160) if WIDE else (3840, 2880) if os.environ.get('HQ_4K') else (1280, 960); sc.render.filepath = out
sc.view_settings.view_transform = 'Filmic'
bpy.ops.render.render(write_still=True)
print('RENDERED', out)
