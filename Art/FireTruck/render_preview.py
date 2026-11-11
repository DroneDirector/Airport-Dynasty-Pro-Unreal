# Preview renders of Art/FireTruck/FireTruck.blend (front 3/4, rear 3/4, side, wheel close-up, cab inside).
# blender -b Art/FireTruck/FireTruck.blend --python Art/FireTruck/render_preview.py -- <outdir>
import bpy, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = argv[0] if argv else os.path.join(os.path.dirname(bpy.data.filepath), 'preview')
CLAY = 'clay' in argv[1:]
os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE'
sc.eevee.use_ssr = True; sc.eevee.use_gtao = True; sc.eevee.use_soft_shadows = True
sc.eevee.taa_render_samples = 64
sc.render.resolution_x, sc.render.resolution_y = 1600, 900
sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'

w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
bg = w.node_tree.nodes['Background']; bg.inputs[0].default_value = (0.55, 0.65, 0.8, 1); bg.inputs[1].default_value = 0.9
bpy.ops.mesh.primitive_plane_add(size=80, location=(0, 0, 0))
g = bpy.context.active_object; gm = bpy.data.materials.new('Ground'); gm.use_nodes = True
gm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.32, 0.32, 0.3, 1)
gm.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.8
g.data.materials.append(gm)
bpy.ops.object.light_add(type='SUN', rotation=(math.radians(50), 0, math.radians(35)))
bpy.context.active_object.data.energy = 4.5; bpy.context.active_object.data.angle = math.radians(3)

if CLAY:   # the shape alone: one grey clay for everything, the glass kept see-through
    clay = bpy.data.materials.new('Clay'); clay.use_nodes = True
    cb = clay.node_tree.nodes['Principled BSDF']; cb.inputs['Base Color'].default_value = (0.62, 0.62, 0.6, 1); cb.inputs['Roughness'].default_value = 0.6
    for ob in bpy.data.objects:
        if ob.type == 'MESH' and ob.name != g.name:
            for i, m in enumerate(ob.data.materials):
                if m and m.name != 'TruckGlass': ob.data.materials[i] = clay
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam

def shot(name, at, look, lens=35):
    cam.location = at; cam.data.lens = lens
    d = Vector(look) - Vector(at); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True)

SHEET = 'sheet' in argv[1:]
if SHEET: sc.render.resolution_x, sc.render.resolution_y = 960, 540
shot('front34', (9.5, 13.5, 2.6), (0, 1.0, 1.6))
shot('front', (0.0, 18.0, 1.8), (0, 0, 1.7), 40)
shot('side', (17.0, 0.0, 2.0), (0, 0, 1.7), 28)
shot('rear34', (-9.0, -14.0, 3.2), (0, -1.0, 1.6))
shot('rear', (0.0, -18.0, 2.0), (0, 0, 1.7), 40)
shot('top', (0.01, 0.0, 22.0), (0, 0, 0), 35)
if not SHEET:
    shot('wheel', (3.6, 5.5, 1.0), (1.3, 3.55, 0.72), 50)
    shot('cab', (0.0, 2.55, 2.55), (0, 5.8, 1.7), 20)
print('PREVIEW DONE')
