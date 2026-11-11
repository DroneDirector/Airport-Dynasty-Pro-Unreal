# The wash hose set on the fire station's outside wall (owner, 2026-11-11; our own design): a galvanised wall plate with
# a saddle bracket holding the coiled green rubber hose, a brass tap with its lever below it feeding the hose, and the
# spray gun (its own object, WashHoseGun) hanging on its hook. In the game the coil is shown while the hose is stowed;
# when taken, a simulated cable runs from the tap's outlet (the empty HoseOutlet) to the gun in your hand.
# Metres; the wall is the plane y = 0, the set stands out along +y; origin on the wall at the plate's foot.
# Blender 3.3:  blender -b --python Art/WashHose/make_washhose.py   ->  WashHose.blend, washhose.glb
import bpy, bmesh, math, os, sys
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
sys.path.insert(0, os.path.join(HERE, '..', 'FireTruck'))
from truck_kit import mat, M, box, cyl, finish, new_obj, coll   # noqa: E402
sys.path.insert(0, os.path.join(HERE, '..', 'FireTruck'))

GALV = mat('HoseGalvanised', (0.55, 0.57, 0.58), 0.9, 0.45)
HOSE = mat('HoseRubber', (0.03, 0.16, 0.06), 0.0, 0.55)
BRASS = mat('HoseBrass', (0.75, 0.55, 0.22), 1.0, 0.3)
GUNBODY = mat('HoseGunBody', (0.9, 0.35, 0.03), 0.0, 0.45)
GUNBLACK = mat('HoseGunBlack', (0.02, 0.02, 0.022), 0.0, 0.5)


def torus(name, R, r, at, rot, material, seg=48, minor=12):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=seg, minor_segments=minor, location=at, rotation=rot)
    ob = bpy.context.active_object; ob.name = name; finish(ob, material)
    return ob


# the wall plate and its bolts
box('WallPlate', -0.2, 0.0, 0.85, 0.2, 0.012, 1.45, GALV, 0.006)
for x in (-0.16, 0.16):
    for z in (0.89, 1.41):
        cyl('Bolt', 0.012, 0.012, (x, 0.018, z), M['satin'], 'y', 6)
# the saddle bracket: an arm out from the plate, curving up into a half-round saddle the coil hangs on
box('BracketArm', -0.03, 0.012, 1.25, 0.03, 0.06, 1.33, GALV, 0.008)
# the saddle: a half-round shelf standing out from the wall, the hose's loops resting over it; a lip at its front end
cyl('Saddle', 0.12, 0.34, (0, 0.2, 1.36), GALV, 'y', 48, 0.01)
box('SaddleLip', -0.1, 0.37, 1.44, 0.1, 0.385, 1.51, GALV, 0.005)
# the coiled hose: loops hanging over the saddle, side by side along it, sagging into an oval under their weight
for k in range(7):
    lp = torus('HoseLoop', 0.23, 0.013, (0.0, 0.07 + k * 0.042, 1.215), (math.pi / 2, 0, math.radians(-3 + k)), HOSE, 64, 10)
    lp.scale = (1.0, 1.25, 1.0)                                   # (its local y is up once turned: the sag)
# the tap: a pipe out of the wall, the valve body, its lever, the hose fitting
cyl('TapPipe', 0.016, 0.1, (0, 0.05, 0.7), BRASS, 'y', 16)
box('TapBody', -0.035, 0.09, 0.66, 0.035, 0.15, 0.74, BRASS, 0.012)
box('TapLever', -0.012, 0.12, 0.74, 0.012, 0.135, 0.85, M['alu'], 0.004).rotation_euler = (0, 0, 0)
cyl('TapOutlet', 0.018, 0.06, (0, 0.12, 0.63), BRASS, 'z', 16)
cyl('HoseFitting', 0.022, 0.04, (0, 0.12, 0.59), M['satin'], 'z', 16)
# the hose from the tap up into the coil
bm = bmesh.new()
from truck_kit import strut   # noqa: E402
strut('HoseFeed', (0, 0.12, 0.57), (0.03, 0.07, 0.93), 0.013, HOSE)
outlet = bpy.data.objects.new('HoseOutlet', None); coll.objects.link(outlet); outlet.location = (0, 0.12, 0.57)
# the spray gun on its hook (its own object: it goes into the hand)
box('GunHook', 0.22, 0.012, 1.1, 0.25, 0.09, 1.12, GALV, 0.004)
gun_parts = [box('t', -0.015, -0.11, -0.02, 0.015, 0.06, 0.02, GUNBODY, 0.008),   # the barrel body
             box('t', -0.014, -0.04, -0.15, 0.014, 0.0, -0.01, GUNBODY, 0.008),   # the grip
             box('t', -0.006, 0.0, -0.09, 0.006, 0.02, -0.03, GUNBLACK, 0.003),   # the trigger
             cyl('t', 0.022, 0.05, (0, 0.085, 0), GUNBLACK, 'y', 20, 0.004),      # the nozzle
             cyl('t', 0.014, 0.04, (0, -0.02, -0.17), M['satin'], 'z', 16)]       # the hose connector
bpy.ops.object.select_all(action='DESELECT')
for q in gun_parts:
    bpy.context.view_layer.objects.active = q
    for m in list(q.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    q.select_set(True)
bpy.context.view_layer.objects.active = gun_parts[0]; bpy.ops.object.join()
gun = bpy.context.active_object; gun.name = 'WashHoseGun'
gun.location = (0.235, 0.06, 1.0); gun.rotation_euler = (math.radians(-90), 0, 0)

for ob in bpy.data.objects:
    if ob.type == 'MESH':
        assert len(ob.data.materials) and all(m is not None for m in ob.data.materials), ob.name
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'WashHose.blend'))
bpy.ops.object.select_all(action='SELECT')
out = os.path.join(HERE, 'washhose.glb')
bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_apply=True, export_format='GLB')
import runpy
_tool = os.path.join(HERE, '..', '..', 'Tools', 'glb_strip_mesh_names.py')
sys.argv = [_tool, out]
runpy.run_path(_tool, run_name='__main__')
print('WASHHOSE DONE')
