# The game icon's neon cube (a die with rounded edges) rendered in Blender: glossy faces with depth in their colour
# (cyan top, magenta left, violet right), bright bevels, seen straight down its corner so its outline is the old flat
# icon's hexagon. Writes Airline-Manager-Pro-Babylon/tools/icon-cube.png (transparent); make-icons.py lays the
# background, the glow and the white airliner over it.
# Run: python3 make_cube_icon.py [size]
import bpy, bmesh, math, os, sys, addon_utils
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'Airline-Manager-Pro-Babylon', 'tools', 'icon-cube.png')
SIZE = int(sys.argv[1]) if len(sys.argv) > 1 else 2048
addon_utils.enable('cycles')
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = int(sys.argv[2]) if len(sys.argv) > 2 else 96
sc.cycles.use_denoising = True; sc.render.film_transparent = True
sc.view_settings.view_transform = 'Standard'
sc.render.resolution_x = sc.render.resolution_y = SIZE


# #region ── Face materials: a colour that deepens across the face, a clear glossy coat, a little glow of its own ──
def face_mat(name, light, dark, axis, flip=False, glow=0.35):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Roughness'].default_value = 0.22; b.inputs['Metallic'].default_value = 0.0
    b.inputs['Coat Weight'].default_value = 0.55; b.inputs['Coat Roughness'].default_value = 0.10
    tc = nt.nodes.new('ShaderNodeTexCoord'); sx = nt.nodes.new('ShaderNodeSeparateXYZ')
    mp = nt.nodes.new('ShaderNodeMapRange'); ramp = nt.nodes.new('ShaderNodeValToRGB')
    nt.links.new(tc.outputs['Object'], sx.inputs[0]); nt.links.new(sx.outputs[axis], mp.inputs['Value'])
    mp.inputs['From Min'].default_value = 0.5 if flip else -0.5; mp.inputs['From Max'].default_value = -0.5 if flip else 0.5
    nt.links.new(mp.outputs['Result'], ramp.inputs[0])
    ramp.color_ramp.elements[0].color = (*dark, 1); ramp.color_ramp.elements[1].color = (*light, 1)
    nt.links.new(ramp.outputs['Color'], b.inputs['Base Color'])
    nt.links.new(ramp.outputs['Color'], b.inputs['Emission Color'])
    b.inputs['Emission Strength'].default_value = glow
    return m

s = lambda h: tuple(((int(h[i:i + 2], 16) / 255) ** 2.2) for i in (0, 2, 4))
TOP = face_mat('Top', s('45E6FF'), s('0A86C4'), 'Y', glow=0.45)          # cyan, lighter toward the back corner
LEFT = face_mat('Left', s('FF5FC8'), s('B0127A'), 'Z', glow=0.40)        # magenta, lighter at the top
RIGHT = face_mat('Right', s('A070FF'), s('4A1AB0'), 'Z', glow=0.40)      # violet, lighter at the top
# #endregion


# #region ── The die: a cube with rounded edges, each face its own material ──
bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.075, segments=8, profile=0.5, affect='EDGES')
me = bpy.data.meshes.new('Die'); bm.to_mesh(me); bm.free()
for m in (TOP, LEFT, RIGHT): me.materials.append(m)
for p in me.polygons:
    n = p.normal; k = max((n.z, 0), (n.x, 2), (-n.y, 1))
    p.material_index = k[1]
me.shade_smooth()
die = bpy.data.objects.new('Die', me); sc.collection.objects.link(die)
# #endregion

# #region ── Light: a soft key from the upper left, a cool fill, a hot rim from behind ──
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.05, 0.02, 0.12, 1)
w.node_tree.nodes['Background'].inputs['Strength'].default_value = 1.0


def area(name, loc, energy, size, col):
    l = bpy.data.lights.new(name, 'AREA'); l.energy = energy; l.size = size; l.color = col
    o = bpy.data.objects.new(name, l); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()

area('Key', (-4.5, -3.0, 3.2), 420, 2.0, (1.0, 0.95, 1.0))
area('Fill', (5.0, 1.0, 1.0), 260, 4.0, (0.7, 0.8, 1.0))
area('Rim', (-3.0, 4.0, 1.5), 300, 2.0, (1.0, 0.5, 0.9))
# #endregion

# #region ── Camera straight down the corner (an isometric view) ──
cd = bpy.data.cameras.new('Cam'); cd.type = 'ORTHO'
cd.ortho_scale = 0.8165 / 0.39 * 1.0      # the hexagon's corners at 39 % of the picture from its middle, as the old icon
cam = bpy.data.objects.new('Cam', cd); sc.collection.objects.link(cam); sc.camera = cam
cam.location = (6, -6, 6)
cam.rotation_euler = (Vector((0, 0, 0)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
# #endregion

sc.render.filepath = os.path.abspath(OUT)
bpy.ops.render.render(write_still=True)
print('rendered', sc.render.filepath, flush=True)
