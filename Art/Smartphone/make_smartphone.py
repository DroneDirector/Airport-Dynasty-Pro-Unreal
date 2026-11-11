# Your smartphone, lying on the desks in your office and Operations (owner, 2026-11-11), as the game's own (game/src/
# screens/phone3d.ts, phoneMesh): 72 x 152 mm, a titanium frame with rounded edges, a matte grey back with a camera
# island (three lenses in a row and a flash), the volume and power keys on the right edge, black glass on the front and
# the screen behind it (its own material, PhoneScreen, UVs over the whole picture: off in the game until you pick it
# up). High detail (Nanite in the game). It lies screen up with its middle at the origin, its top (the cameras) at +y,
# the glass's face 8.6 mm up. Blender 3.3:
#   blender -b --python Art/Smartphone/make_smartphone.py
# writes Art/Smartphone/Smartphone.blend and Art/Smartphone/smartphone.glb.
import bpy, bmesh, math, os
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
coll = bpy.context.scene.collection
W, H, R, T = 0.072, 0.152, 0.009, 0.0086


def mat(name, color, metal=0.0, rough=0.5, emit=None, strength=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    return m


M = {
    'frame': mat('PhoneFrame', (0.47, 0.455, 0.42), 0.92, 0.28),
    'back': mat('PhoneBack', (0.24, 0.23, 0.215), 0.15, 0.62),
    'glass': mat('PhoneGlass', (0.006, 0.007, 0.008), 0.0, 0.06),
    'screen': mat('PhoneScreen', (0.004, 0.005, 0.006), 0.0, 0.05),
    'lens': mat('PhoneLens', (0.01, 0.012, 0.016), 0.1, 0.05),
    'flash': mat('PhoneFlash', (0.9, 0.85, 0.72), 0.0, 0.3),
    'emblem': mat('PhoneEmblem', (0.34, 0.33, 0.31), 0.6, 0.35),
}


def rounded(w, h, r, seg=10):
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(seg + 1):
            a = math.radians(a0 + 90 * i / seg); pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def slab(name, w, h, r, z0, z1, material, bevel=0.0, seg=10):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    bot = [bm.verts.new((x, y, z0)) for x, y in rounded(w, h, r, seg)]
    top = [bm.verts.new((x, y, z1)) for x, y in rounded(w, h, r, seg)]
    bm.faces.new(list(reversed(bot))); bm.faces.new(top)
    n = len(bot)
    for i in range(n):
        bm.faces.new((bot[i], bot[(i + 1) % n], top[(i + 1) % n], top[i]))
    bm.normal_update(); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); coll.objects.link(ob); ob.data.materials.append(material)
    if bevel:
        b = ob.modifiers.new('bevel', 'BEVEL'); b.width = bevel; b.segments = 4; b.limit_method = 'ANGLE'
    ob.data.use_auto_smooth = True; ob.data.auto_smooth_angle = math.radians(35)
    for p in ob.data.polygons: p.use_smooth = True
    return ob


def disc(name, r, z0, z1, at, material, verts=32):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=z1 - z0, location=(at[0], at[1], (z0 + z1) / 2))
    ob = bpy.context.active_object; ob.name = name; ob.data.materials.append(material)
    bpy.ops.object.shade_smooth(); return ob


def box(name, size, at, material):
    bpy.ops.mesh.primitive_cube_add(size=1, location=at); ob = bpy.context.active_object; ob.name = name; ob.scale = size
    bpy.ops.object.transform_apply(scale=True); ob.data.materials.append(material)
    b = ob.modifiers.new('bevel', 'BEVEL'); b.width = min(size) * 0.3; b.segments = 3
    return ob


# the frame (titanium, rounded edges), the back (matte) and the front glass (black, with the screen behind it)
slab('Frame', W, H, R, 0.0008, T - 0.0006, M['frame'], 0.0014)
slab('Back', W - 0.0016, H - 0.0016, R * 0.9, 0.0, 0.0009, M['back'], 0.0004)
slab('Glass', W - 0.0008, H - 0.0008, R * 0.95, T - 0.0007, T, M['glass'], 0.0003)
# the screen: its own surface just under the glass's face, the picture's corners rounded as the glass's
me = bpy.data.meshes.new('Screen'); bm = bmesh.new(); uv = bm.loops.layers.uv.new('UVMap')
sw, sh = W - 0.005, H - 0.005
f = bm.faces.new([bm.verts.new((x, y, T + 0.00005)) for x, y in rounded(sw, sh, R * 0.8)])
for lp in f.loops:
    lp[uv].uv = ((lp.vert.co.x + sw / 2) / sw, (lp.vert.co.y + sh / 2) / sh)
bm.normal_update(); bm.to_mesh(me); bm.free()
scr = bpy.data.objects.new('Screen', me); coll.objects.link(scr); scr.data.materials.append(M['screen'])
# the front camera's hole in the glass
disc('FrontCamera', 0.0022, T, T + 0.0001, (0, H / 2 - 0.0095), M['lens'], 24)
# the camera island on the back: three lenses in a row, their rings, and the flash
iy = H / 2 - 0.022
slab('CameraIsland', W * 0.74, 0.022, 0.011, -0.0016, 0.0, M['frame'], 0.0006).location.y = iy
for i in range(3):
    lx = -W * 0.22 + i * W * 0.17
    disc('LensRing%d' % i, 0.0062, -0.0028, -0.0016, (lx, iy), M['frame'], 40)
    disc('Lens%d' % i, 0.0046, -0.0030, -0.0027, (lx, iy), M['lens'], 40)
disc('Flash', 0.0018, -0.0020, -0.0016, (W * 0.29, iy), M['flash'], 20)
# a small engraved emblem on the back (no maker's name)
disc('Emblem', 0.005, -0.0002, 0.0, (0, -0.005), M['emblem'], 4)
# the keys on the right edge: volume up, volume down, power
for i, (y, l) in enumerate(((0.03, 0.016), (0.012, 0.016), (-0.016, 0.012))):
    box('Key%d' % i, (0.0012, l, 0.0028), (W / 2 + 0.0004, y, T / 2), M['frame'])

for ob in bpy.data.objects:
    if ob.type == 'MESH':
        assert len(ob.data.materials) and all(m is not None for m in ob.data.materials), ob.name
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Smartphone.blend'))
bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'smartphone.glb'), use_selection=True, export_apply=True, export_format='GLB')
# the meshes' own names out (Unreal then names them <file>_mesh_<n>, placed by number as the Babylon models are)
import runpy, sys as _sys
_tool = os.path.join(HERE, '..', '..', 'Tools', 'glb_strip_mesh_names.py')
_sys.argv = [_tool, os.path.join(HERE, 'smartphone.glb')]
runpy.run_path(_tool, run_name='__main__')
print('PHONE DONE')
