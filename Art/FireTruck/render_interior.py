# Grey renders of the fire truck's cab interior: from above with the roof taken off, and from the driver's seat.
# blender -b <truck .blend> --python Art/FireTruck/render_interior.py -- <outdir>
import bpy, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = argv[0]; os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.use_gtao = True; sc.eevee.use_soft_shadows = True; sc.eevee.taa_render_samples = 64
sc.render.resolution_x, sc.render.resolution_y = 1600, 900
sc.view_settings.view_transform = 'Filmic'
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.65, 0.8, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 1.2
bpy.ops.object.light_add(type='SUN', rotation=(math.radians(35), 0, math.radians(30))); bpy.context.active_object.data.energy = 4
clay = bpy.data.materials.new('Clay'); clay.use_nodes = True
clay.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.62, 0.62, 0.6, 1)
dark = bpy.data.materials.new('ClayDark'); dark.use_nodes = True
dark.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.25, 0.26, 0.28, 1)
REAL = 'real' in argv[1:]          # with 'real': the materials as made (textured), not the grey clay
for ob in (bpy.data.objects if not REAL else []):
    if ob.type == 'MESH':
        for i, m in enumerate(ob.data.materials):
            if m and m.name != 'TruckGlass' and not m.name.startswith('TruckCluster'):
                ob.data.materials[i] = dark if m.name in ('TruckSeat', 'TruckDash', 'TruckScreen', 'TruckFloor') else clay
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam


def shot(name, at, look, lens):
    cam.location = at; cam.data.lens = lens
    cam.rotation_euler = (Vector(look) - Vector(at)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True)


# from the driver's seat, looking out through the windscreen
shot('interior_driver', (0.0, 4.0, 2.45), (0.0, 7.0, 1.75), 18)
# what the driver sees of the gauges: from the eyes, over the wheel
shot('interior_pedals', (0.25, 4.25, 1.75), (0.0, 4.95, 1.15), 28)
shot('interior_cluster', (0.0, 4.05, 2.45), (0.0, 5.18, 2.02), 30)
# from outside, looking in through the windscreen at the dash
shot('exterior_window', (0.9, 8.6, 2.75), (0.0, 5.2, 1.85), 38)
# over the driver's shoulder at the wheel, the stalks and the gauges
shot('interior_cockpit', (0.32, 3.95, 2.35), (-0.02, 5.0, 1.72), 24)
# roof, headliner and everything on the roof hidden: looking down into the cab from above the left side
for ob in bpy.data.objects:
    if ob.type == 'MESH' and (ob.name.startswith(('CabRoof', 'Headliner', 'LightBar', 'Light_BlueBar', 'RoofTurret', 'RoofNozzle', 'GlassHeader', 'Glass_Cab', 'DoorL', 'Glass_DoorL', 'DoorHandleL', 'MirrorArm', 'Mirror'))):
        ob.hide_render = True
shot('interior_above', (-3.6, 2.2, 5.6), (0.0, 4.1, 1.4), 24)
print('INTERIOR DONE')
