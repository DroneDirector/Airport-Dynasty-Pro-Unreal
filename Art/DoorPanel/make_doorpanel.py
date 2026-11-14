# The door button panel (owner, 2026-11-14: his reference photo - our own model of it): a 12 x 16 cm brushed stainless
# plate on the wall, two domed screws, a green OPEN button (white up-arrow) and a red CLOSE button (white down-arrow), each
# in a chrome bezel, the words drawn on the plate's face (T_DoorPanel_Face.png); beside it a small sign with the E key
# (T_DoorPanel_Key.png). In the game it goes inside the fire station by every bay door and inside every hangar by its
# big door. The buttons are their own objects (BtnOpen, BtnClose) so a finger can press them in.
# Metres; the wall is the plane y = 0, the panel stands out along +y; origin on the wall at the floor; the plate's middle
# 1.35 m up.
# Blender 3.3:  blender -b --python Art/DoorPanel/make_doorpanel.py   ->  DoorPanel.blend, doorpanel.glb
import bpy, bmesh, math, os, sys
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
sys.path.insert(0, os.path.join(HERE, '..', 'FireTruck'))
from truck_kit import mat, cyl, finish, coll   # noqa: E402

PW, PH, PT, ZC = 0.12, 0.16, 0.006, 1.35          # the plate: width, height, thickness, its middle's height


def tex_mat(name, png, rough=0.35, metal=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    t = m.node_tree.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(HERE, png))
    m.node_tree.links.new(t.outputs['Color'], b.inputs['Base Color'])
    b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    return m


FACE = tex_mat('DoorPanelFace', 'T_DoorPanel_Face.png', 0.32, 0.85)
KEY = tex_mat('DoorPanelKey', 'T_DoorPanel_Key.png', 0.45, 0.0)
CHROME = mat('DoorPanelChrome', (0.78, 0.78, 0.8), 1.0, 0.15)
GREEN = mat('DoorPanelGreen', (0.05, 0.62, 0.12), 0.0, 0.25)
RED = mat('DoorPanelRed', (0.8, 0.06, 0.05), 0.0, 0.25)
WHITE = mat('DoorPanelWhite', (0.9, 0.9, 0.9), 0.0, 0.35)
SCREW = mat('DoorPanelScrew', (0.6, 0.6, 0.62), 1.0, 0.3)


def plate(name, x0, x1, z0, z1, depth, material):
    """A plate on the wall whose front (facing +y) shows its whole texture; the edges take the texture's rim."""
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector((x1 - x0, depth, z1 - z0)), verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector(((x0 + x1) / 2, depth / 2, (z0 + z1) / 2)), verts=bm.verts)
    uv = bm.loops.layers.uv.new()
    for f in bm.faces:
        front = f.normal.y > 0.9
        for l in f.loops:
            p = l.vert.co
            # (u runs right to left in Blender's x: the game's model loader mirrors x, so it reads right way round there)
            l[uv].uv = ((x1 - p.x) / (x1 - x0), (p.z - z0) / (z1 - z0)) if front else (0.01, 0.01)
    bm.normal_update()
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); coll.objects.link(ob)
    ob.data.materials.append(material)
    return ob


def at(u, v, y):
    """A point on the plate's face from the texture's u (left to right) and v (top down)."""
    return (PW / 2 - u * PW, y, ZC + PH / 2 - v * PH)   # (mirrored in x, as the texture: see plate())


def tri(name, cu, cv, size, up, y):
    """A flat white arrow on a button's top."""
    bm = bmesh.new()
    c = Vector(at(cu, cv, y)); s = size
    pts = [(0, s * 0.62), (-s * 0.62, -s * 0.38), (s * 0.62, -s * 0.38)] if up else [(0, -s * 0.62), (-s * 0.62, s * 0.38), (s * 0.62, s * 0.38)]
    vs = [bm.verts.new(c + Vector((x, 0, z))) for x, z in pts]
    vb = [bm.verts.new(c + Vector((x, -0.0006, z))) for x, z in pts]
    bm.faces.new(vs); bm.faces.new(list(reversed(vb)))
    for i in range(3): bm.faces.new([vs[i], vb[i], vb[(i + 1) % 3], vs[(i + 1) % 3]])
    bm.normal_update()
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); coll.objects.link(ob); ob.data.materials.append(WHITE)
    return ob


# the plate
plate('DoorPanelPlate', -PW / 2, PW / 2, ZC - PH / 2, ZC + PH / 2, PT, FACE)
# its two screws, top and bottom, domed
for nm, v in (('DoorPanelScrewTop', 0.058), ('DoorPanelScrewBottom', 0.91)):
    s = cyl(nm, 0.0042, 0.003, at(0.5, v, PT + 0.0026), SCREW, 'y', 20, 0.0012)
# the buttons: a chrome bezel standing proud of the plate, the coloured cap in it, its arrow on top
for nm, v, colour, up in (('Open', 0.376, GREEN, True), ('Close', 0.685, RED, False)):
    cyl('Btn%sBezel' % nm, 0.0188, 0.006, at(0.345, v, PT + 0.0041), CHROME, 'y', 48, 0.002)
    cyl('Btn%s' % nm, 0.0152, 0.008, at(0.345, v, PT + 0.0065), colour, 'y', 48, 0.0025)
    tri('Btn%sArrow' % nm, 0.345, v, 0.012, up, PT + 0.0126)
# the E sign beside it, level with the green button
plate('DoorPanelKeySign', -PW / 2 - 0.06, -PW / 2 - 0.02, ZC + PH / 2 - 0.376 * PH - 0.02, ZC + PH / 2 - 0.376 * PH + 0.02, 0.004, KEY)

for ob in bpy.data.objects:
    if ob.type == 'MESH':
        assert len(ob.data.materials) and all(m is not None for m in ob.data.materials), ob.name
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'DoorPanel.blend'))
bpy.ops.object.select_all(action='SELECT')
out = os.path.join(HERE, 'doorpanel.glb')
bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_apply=True, export_format='GLB')
import runpy
_tool = os.path.join(HERE, '..', '..', 'Tools', 'glb_strip_mesh_names.py')
sys.argv = [_tool, out]
runpy.run_path(_tool, run_name='__main__')
print('DOORPANEL DONE')
