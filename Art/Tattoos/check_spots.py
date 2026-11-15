# Shows the tattoo places on the MetaHuman's body and head in Blender: each place a colour with a checker of its own
# design coordinates (a design laid there would follow that checker), rendered front, back, sides and close-ups of a
# hand (nails must stay clear) and the neck. Writes Art/Tattoos/Placed/Spots/check_<view>.png
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Tattoos/check_spots.py
import math, os, sys
import numpy as np
try:
    import bpy
except ImportError:
    bpy = None
HERE = os.path.dirname(os.path.abspath(__file__)); SP = os.path.join(HERE, 'Placed', 'Spots'); CK = os.path.join(HERE, 'Placed', 'SpotChecks')
COL = {'arm_l': (1, .3, .2), 'arm_r': (.2, .5, 1), 'leg_l': (1, .8, .1), 'leg_r': (.2, .9, .3), 'fingers_l': (1, 0, 1),
       'fingers_r': (0, 1, 1), 'neck_throat': (1, .5, 0), 'neck_l': (.6, .2, 1), 'neck_back': (.1, .8, .8), 'neck_r': (.9, .1, .4)}


def debug_tex(part, names):
    # (the pattern picture is made outside Blender by make_debug(): Blender's Python has no OpenCV)
    return bpy.data.images.load(os.path.join(CK, 'debug_%s.png' % part))


def make_debug():
    import cv2
    for part, names in (('body', ['arm_l', 'arm_r', 'leg_l', 'leg_r', 'fingers_l', 'fingers_r']), ('face', ['neck_throat', 'neck_l', 'neck_back', 'neck_r'])):
        img = np.full((1024, 1024, 3), 0.55)
        for n in names:
            m = cv2.imread(os.path.join(SP, 'T_TatSpot_%s.png' % n), cv2.IMREAD_UNCHANGED).astype(float) / 65535
            a = m[..., 3] > 0.5
            # (cv2 reads blue-green-red: B = sin, G = cos, R = along)
            u = (np.arctan2(m[..., 0] * 2 - 1, m[..., 1] * 2 - 1) / (2 * math.pi) + 0.5) % 1.0; v = m[..., 2]
            chk = ((np.floor(u * 10) + np.floor(v * 10)) % 2)[..., None]
            c = np.array(COL[n]); img[a] = (c * (0.55 + 0.45 * chk))[a]
        cv2.imwrite(os.path.join(CK, 'debug_%s.png' % part), np.round(img[..., ::-1] * 255).astype(np.uint8))


if bpy is None:
    make_debug(); sys.exit(0)
bpy.ops.wm.read_factory_settings(use_empty=True)
for part, names in (('body', ['arm_l', 'arm_r', 'leg_l', 'leg_r', 'fingers_l', 'fingers_r']), ('face', ['neck_throat', 'neck_l', 'neck_back', 'neck_r'])):
    bpy.ops.import_scene.fbx(filepath='F:/ADP-Export/tattoo/%s.fbx' % part)
    meshes = sorted([o for o in bpy.context.selected_objects if o.type == 'MESH'], key=lambda o: -len(o.data.vertices))
    for o in meshes[1:]: bpy.data.objects.remove(o)
    ob = meshes[0]
    uvl = ob.data.uv_layers[0]
    uv = np.empty(len(uvl.data) * 2); uvl.data.foreach_get('uv', uv); uv = uv.reshape(-1, 2); uv -= np.floor(uv.min(0)); uvl.data.foreach_set('uv', uv.ravel())
    mat = bpy.data.materials.new('m_' + part); mat.use_nodes = True
    t = mat.node_tree.nodes.new('ShaderNodeTexImage'); t.image = debug_tex(part, names)
    mat.node_tree.links.new(t.outputs['Color'], mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
    ob.data.materials.clear(); ob.data.materials.append(mat)
for o in bpy.data.objects:
    if o.type == 'ARMATURE': o.hide_render = True
sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE_NEXT'; sc.render.resolution_x, sc.render.resolution_y = 900, 1200
w = bpy.data.worlds.new('w'); w.color = (0.9, 0.9, 0.9); sc.world = w
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); sc.collection.objects.link(sun); sun.rotation_euler = (0.6, 0.2, 0.4); sun.data.energy = 3
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
from mathutils import Vector
views = {'front': ((0, -4.2, 1.0), (0, 0, 0.95), 50), 'back': ((0, 4.2, 1.0), (0, 0, 0.95), 50), 'left': ((4.2, 0, 1.0), (0, 0, 0.95), 50),
         'hand_l': ((0.95, -0.55, 1.05), (0.52, -0.2, 1.02), 70), 'neck': ((0.45, -0.75, 1.62), (0, 0, 1.55), 80), 'neck_back': ((-0.35, 0.8, 1.6), (0, 0, 1.55), 80)}
for n, (loc, tgt, lens) in views.items():
    cam.location = Vector(loc); cam.data.lens = lens
    cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(CK, 'check_%s.png' % n); bpy.ops.render.render(write_still=True)
print('SPOTS CHECK done')
