# Your tablet, always with you (owner, 2026-11-18, to-do 117; his photo docs/Pictures/Tablet/tablet-reference-photo.webp),
# made as the smartphone is (Art/Smartphone/make_smartphone.py): an 11-inch tablet, 254 x 151 x 7 mm, a dark graphite
# aluminium body with rounded corners and edges, an even black bezel all round (about 9 mm), the front camera in the
# middle of the top bezel, black glass on the front with the screen behind it (its own material, TabletScreen, UVs over
# the whole picture - the game draws its UI on it), on the back a small camera with a flash and a quiet emblem (no maker's
# name), the power key on the top edge, the volume keys on the right, speaker grilles on both short edges and a USB-C
# port. It lies screen up, its middle at the origin, landscape (long side along x), its top edge (the front camera) at
# +y, the glass's face 7 mm up. Blender 3.3:
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Tablet/make_tablet.py
# writes Art/Tablet/Tablet.blend and Art/Tablet/tablet.glb.
import bpy, bmesh, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
coll = bpy.context.scene.collection
W, H, R, T = 0.254, 0.151, 0.0095, 0.0070        # body; corner radius; thickness
BZ = 0.0090                                       # the bezel (sides and top/bottom alike, as in his photo)


def mat(name, color, metal=0.0, rough=0.5, emit=None, strength=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    b.inputs['Specular'].default_value = 0.5 if metal > 0.5 else 0.22
    if name == 'TabletGlass': b.inputs['Specular'].default_value = 0.06; b.inputs['Roughness'].default_value = 0.22   # (owner: the front black - the bezel shows black, a faint sheen only)
    return m


M = {
    # (owner, 2026-11-18: black, glossy but only a little - about 10 gloss: a satin black, not a mirror)
    'frame': mat('TabletFrame', (0.005, 0.005, 0.0055), 0.10, 0.46),
    'back': mat('TabletBack', (0.004, 0.004, 0.0045), 0.0, 0.50),   # (black, not grey: no metal, a soft sheen)
    'glass': mat('TabletGlass', (0.003, 0.003, 0.004), 0.0, 0.10),
    'screen': mat('TabletScreen', (0.004, 0.005, 0.006), 0.0, 0.05),
    'lens': mat('TabletLens', (0.01, 0.012, 0.016), 0.1, 0.05),
    'flash': mat('TabletFlash', (0.9, 0.85, 0.72), 0.0, 0.3),
    'grille': mat('TabletGrille', (0.02, 0.02, 0.022), 0.4, 0.6),
    'emblem': mat('TabletEmblem', (0.16, 0.165, 0.17), 0.85, 0.25),   # (the aeroplane: a slightly shinier inlay, so it shows softly)
}


def rounded(w, h, r, seg=12):
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(seg + 1):
            a = math.radians(a0 + 90 * i / seg); pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def slab(name, w, h, r, z0, z1, material, bevel=0.0, seg=12):
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


def disc(name, r, z0, z1, at, material, verts=32, axis='Z'):
    rot = (0, math.radians(90), 0) if axis == 'X' else (math.radians(90), 0, 0) if axis == 'Y' else (0, 0, 0)
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=z1 - z0, location=at, rotation=rot)
    ob = bpy.context.active_object; ob.name = name; ob.data.materials.append(material)
    bpy.ops.object.shade_smooth(); return ob


def box(name, size, at, material, bev=0.3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=at); ob = bpy.context.active_object; ob.name = name; ob.scale = size
    bpy.ops.object.transform_apply(scale=True); ob.data.materials.append(material)
    b = ob.modifiers.new('bevel', 'BEVEL'); b.width = min(size) * bev; b.segments = 3
    return ob


# the body: the aluminium frame with softly rounded edges, the back plate, the front glass (black), the screen behind it
slab('Frame', W, H, R, 0.0006, T - 0.0005, M['frame'], 0.0018)
slab('Back', W - 0.0012, H - 0.0012, R * 0.95, 0.0, 0.0008, M['back'], 0.0004)
slab('Glass', W - 0.0010, H - 0.0010, R * 0.96, T - 0.0006, T, M['glass'], 0.0003)
me = bpy.data.meshes.new('Screen'); bm = bmesh.new(); uv = bm.loops.layers.uv.new('UVMap')
sw, sh = W - 2 * BZ, H - 2 * BZ
f = bm.faces.new([bm.verts.new((x, y, T + 0.0004)) for x, y in rounded(sw, sh, 0.0040)])
for lp in f.loops:
    lp[uv].uv = ((lp.vert.co.x + sw / 2) / sw, (lp.vert.co.y + sh / 2) / sh)
bm.normal_update(); bm.to_mesh(me); bm.free()
scr = bpy.data.objects.new('Screen', me); coll.objects.link(scr); scr.data.materials.append(M['screen'])
# the front camera in the middle of the top bezel (as in his photo), a light sensor beside it
disc('FrontCamera', 0.0016, T, T + 0.0001, (0, H / 2 - BZ / 2, T + 0.00005), M['lens'], 24)
disc('LightSensor', 0.0007, T, T + 0.0001, (0.006, H / 2 - BZ / 2, T + 0.00005), M['lens'], 12)
# the back: one camera in a slim ring, its flash, and a quiet emblem
disc('BackLensRing', 0.0058, -0.0010, 0.0, (-W / 2 + 0.020, H / 2 - 0.020, -0.0005), M['frame'], 40)
disc('BackLens', 0.0040, -0.0012, -0.0008, (-W / 2 + 0.020, H / 2 - 0.020, -0.0010), M['lens'], 40)
disc('Flash', 0.0016, -0.0004, 0.0, (-W / 2 + 0.034, H / 2 - 0.020, -0.0002), M['flash'], 20)
# the logo on the back: a small passenger aeroplane seen from above (owner, 2026-11-18), a thin flat inlay 24 mm long, built
# from simple flat pieces (each convex, so they close cleanly): the fuselage, two swept wings, two tail planes, the fin's
# root as a slim strip
def prism(bm, pts, z0, z1):
    bot = [bm.verts.new((x, y, z0)) for x, y in pts]; top = [bm.verts.new((x, y, z1)) for x, y in pts]
    bm.faces.new(list(reversed(bot))); bm.faces.new(top); n = len(pts)
    for i in range(n): bm.faces.new((bot[i], bot[(i + 1) % n], top[(i + 1) % n], top[i]))
def airliner(name, at, length, z0, z1, material):
    k = length / 22.8   # (metres per unit: nose to tail is 22.8 units)
    P = lambda pts: [(at[0] + x * k, at[1] + y * k) for x, y in pts]
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    fus = [(0, 11.4), (0.75, 10.7), (1.1, 9.3), (1.1, -8.5), (0.7, -11.0), (0, -11.4), (-0.7, -11.0), (-1.1, -8.5), (-1.1, 9.3), (-0.75, 10.7)]
    prism(bm, P(fus), z0, z1)
    for sx in (1, -1):
        prism(bm, P([(sx * 1.0, 2.8), (sx * 11.0, -2.3), (sx * 11.0, -3.9), (sx * 1.0, -1.4)][::sx]), z0, z1)          # a wing
        prism(bm, P([(sx * 1.0, -7.7), (sx * 4.4, -9.9), (sx * 4.4, -10.9), (sx * 1.0, -9.9)][::sx]), z0, z1)          # a tail plane
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
    bm.normal_update(); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); coll.objects.link(ob); ob.data.materials.append(material)
    return ob
airliner('Logo', (0.0, 0.0), 0.024, -0.00012, 0.00002, M['emblem'])
# the keys: power on the top edge (left), volume up/down on the right edge
box('PowerKey', (0.014, 0.0012, 0.0024), (-W / 2 + 0.040, H / 2 + 0.0004, T / 2), M['frame'])
box('VolumeUp', (0.0012, 0.012, 0.0024), (W / 2 + 0.0004, H / 2 - 0.030, T / 2), M['frame'])
box('VolumeDown', (0.0012, 0.012, 0.0024), (W / 2 + 0.0004, H / 2 - 0.046, T / 2), M['frame'])
# speaker grilles: two rows of small holes on each short edge; the USB-C port in the middle of the right short edge
for sx in (-1, 1):
    for row in (-1, 1):
        for i in range(9):
            y = (-0.04 if row < 0 else 0.03) + i * 0.0018
            disc('Grille_%d_%d_%d' % (sx, row, i), 0.0004, 0.0, 0.0012, (sx * (W / 2 + 0.0001), y, T / 2), M['grille'], 8, 'X')
box('UsbC', (0.0012, 0.0086, 0.0026), (W / 2 + 0.0002, 0.0, T / 2), M['grille'], 0.45)

for ob in bpy.data.objects:
    if ob.type == 'MESH':
        assert len(ob.data.materials) and all(m is not None for m in ob.data.materials), ob.name
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Tablet.blend'))
bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'tablet.glb'), use_selection=True, export_apply=True, export_format='GLB')
import runpy, sys as _sys
_tool = os.path.join(HERE, '..', '..', 'Tools', 'glb_strip_mesh_names.py')
_sys.argv = [_tool, os.path.join(HERE, 'tablet.glb')]
runpy.run_path(_tool, run_name='__main__')
print('TABLET DONE')
