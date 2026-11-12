# Swatches of the fabric library (Tools/make_fabrics.py) rendered in Blender as a draped, folded piece of cloth under
# a soft studio light, one picture each, so the weave, the sheen and the folds can be judged at close range.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/render_fabrics.py -- [out_dir] [name ...]
import bpy, sys, os, math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FAB = ROOT / 'Art' / 'Fabrics'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = Path(argv[0]) if argv else ROOT / 'Art' / 'Fabrics' / 'swatches'
NAMES = argv[1:] or sorted(p.name for p in FAB.iterdir() if (p / 'albedo.png').exists())
OUT.mkdir(parents=True, exist_ok=True)
# how many tiles across the 30 cm cloth: a tile is ~4 cm for the fine weaves, ~8-10 for the coarse ones
TILES = {'cotton_jersey': 7.5, 'satin': 7.5, 'wool_suiting': 7.5, 'nylon_shell': 6, 'hivis_polyester': 6, 'fleece': 5, 'denim': 3.75, 'canvas': 3.75, 'leather': 3.75, 'knit': 3}

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'
sc.cycles.samples = 96
sc.cycles.use_denoising = True
sc.render.resolution_x = sc.render.resolution_y = 768
sc.render.film_transparent = False
try:
    sc.cycles.device = 'CPU'
except Exception:
    pass

# the cloth: a 30 cm square, subdivided, folded by a few smooth waves (as cloth lying loosely on a table)
bpy.ops.mesh.primitive_grid_add(x_subdivisions=160, y_subdivisions=160, size=0.3)
cloth = bpy.context.object
cloth.name = 'Swatch'
for v in cloth.data.vertices:
    x, y = v.co.x, v.co.y
    v.co.z = 0.012 * math.sin(x * 38 + 0.6) * math.cos(y * 24) + 0.02 * math.sin(y * 18 + x * 9) + 0.006 * math.cos(x * 70 + y * 55)
for p in cloth.data.polygons: p.use_smooth = True
bpy.ops.object.shade_smooth()
# a table under it
bpy.ops.mesh.primitive_plane_add(size=2, location=(0, 0, -0.03))
table = bpy.context.object
tm = bpy.data.materials.new('Table'); tm.use_nodes = True
tm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.35, 0.35, 0.36, 1)
tm.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.7
table.data.materials.append(tm)
# the light: a big soft area light from the upper left, a weaker fill, a grey world
bpy.ops.object.light_add(type='AREA', location=(-0.5, -0.4, 0.7))
key = bpy.context.object; key.data.energy = 60; key.data.size = 0.8
key.rotation_euler = (math.radians(35), math.radians(-30), 0)
bpy.ops.object.light_add(type='AREA', location=(0.6, 0.3, 0.5))
fill = bpy.context.object; fill.data.energy = 15; fill.data.size = 1.0
fill.rotation_euler = (math.radians(-30), math.radians(40), 0)
sc.world = bpy.data.worlds.new('W'); sc.world.use_nodes = True
sc.world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.2, 0.2, 0.22, 1)
sc.world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.6
# the camera: close, looking down at an angle, so the weave shows and the folds catch the light
bpy.ops.object.camera_add(location=(0.0, -0.22, 0.26), rotation=(math.radians(42), 0, 0))
cam = bpy.context.object; cam.data.lens = 60
sc.camera = cam

def material(name):
    d = FAB / name
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; bsdf = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping')
    mp.inputs['Scale'].default_value = (TILES.get(name, 5), TILES.get(name, 5), 1)
    nt.links.new(tc.outputs['UV'], mp.inputs['Vector'])
    def tex(file, colorspace):
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(str(d / file))
        t.image.colorspace_settings.name = colorspace
        nt.links.new(mp.outputs['Vector'], t.inputs['Vector'])
        return t
    alb = tex('albedo.png', 'sRGB'); nrm = tex('normal.png', 'Non-Color'); rgh = tex('roughness.png', 'Non-Color')
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 1.0
    nt.links.new(nrm.outputs['Color'], nm.inputs['Color'])
    nt.links.new(alb.outputs['Color'], bsdf.inputs['Base Color'])
    nt.links.new(rgh.outputs['Color'], bsdf.inputs['Roughness'])
    nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    bsdf.inputs['Sheen'].default_value = 0.6 if name in ('cotton_jersey', 'wool_suiting', 'fleece', 'knit', 'canvas') else 0.1
    if name == 'satin':
        bsdf.inputs['Anisotropic'].default_value = 0.8
    if name == 'leather':
        bsdf.inputs['Specular'].default_value = 0.5
    return m

for name in NAMES:
    cloth.data.materials.clear()
    cloth.data.materials.append(material(name))
    sc.render.filepath = str(OUT / (name + '.png'))
    bpy.ops.render.render(write_still=True)
    print('rendered', name)
