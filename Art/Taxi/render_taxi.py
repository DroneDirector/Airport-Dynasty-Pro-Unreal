# Renders the taxi's check views (side, and front three-quarter from a little above) in a blueprint look:
# shaded navy body with white outlines. Run: python3 render_taxi.py [scale]  -> /tmp/taxi_side.png, /tmp/taxi_34.png
import bpy, math, os, sys, addon_utils
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
SCALE = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
addon_utils.enable('cycles')
bpy.ops.wm.open_mainfile(filepath=os.path.join(HERE, 'Taxi.blend'))
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 24
sc.cycles.use_denoising = True
sc.render.film_transparent = False
sc.view_settings.view_transform = 'Standard'

# #region ── Light and background ──
w = bpy.data.worlds.new('Blueprint'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.012, 0.035, 0.085, 1)
w.node_tree.nodes['Background'].inputs['Strength'].default_value = 1.6
for name, loc, energy, size in (('Key', (3, 4, 6), 900, 5), ('Fill', (-4, -3, 4), 350, 6), ('Front', (7, 1, 2), 300, 4)):
    ld = bpy.data.lights.new(name, 'AREA'); ld.energy = energy; ld.size = size
    lo = bpy.data.objects.new(name, ld); sc.collection.objects.link(lo); lo.location = loc
    lo.rotation_euler = (Vector((0, 0, 0.7)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
# #endregion

# #region ── Outlines (Freestyle) ──
sc.render.use_freestyle = True
sc.render.line_thickness_mode = 'ABSOLUTE'
vl = sc.view_layers[0]
fs = vl.freestyle_settings
fs.crease_angle = math.radians(138)
ls = fs.linesets[0] if fs.linesets else fs.linesets.new('Lines')
ls.select_by_visibility = True; ls.visibility = 'VISIBLE'
ls.select_silhouette = True; ls.select_border = True; ls.select_crease = True
ls.select_material_boundary = True; ls.select_external_contour = True
st = ls.linestyle or bpy.data.linestyles.new('Blueprint'); ls.linestyle = st
st.color = (0.86, 0.93, 1.0); st.thickness = 1.5 * SCALE
# #endregion


def cam(name, loc, target, ortho=None, lens=50):
    cd = bpy.data.cameras.new(name)
    if ortho: cd.type = 'ORTHO'; cd.ortho_scale = ortho
    else: cd.lens = lens
    co = bpy.data.objects.new(name, cd); sc.collection.objects.link(co); co.location = loc
    co.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    return co


def shot(co, w, h, path):
    sc.camera = co
    sc.render.resolution_x = int(w * SCALE); sc.render.resolution_y = int(h * SCALE)
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print('rendered', path, flush=True)


shot(cam('Side', (0, 14, 0.80), (0, 0, 0.80), ortho=5.2), 2400, 900, '/tmp/taxi_side.png')
shot(cam('ThreeQuarter', (6.4, 5.6, 3.1), (0.15, 0, 0.55), lens=55), 2400, 1500, '/tmp/taxi_34.png')
shot(cam('RearThreeQuarter', (-6.4, 5.4, 2.9), (-0.15, 0, 0.55), lens=55), 2400, 1500, '/tmp/taxi_r34.png')
