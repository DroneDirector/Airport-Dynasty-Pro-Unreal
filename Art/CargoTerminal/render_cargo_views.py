# Renders the cargo terminal's views in a blueprint look (navy shading, white outlines): top, the airside (front) and
# landside (back) elevations in two halves each, both end elevations, three angled views, and three cutaways (roof,
# upper walls and upper office floors left out) that show the rooms and equipment inside. finish_cargo_sheets.py
# then puts titles and dimensions on them as 4K sheets.   python3 render_cargo_views.py [scale] [only]
import bpy, math, os, sys, addon_utils, json
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join('/tmp', 'cargo_views'); os.makedirs(OUT, exist_ok=True)
S = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
ONLY = sys.argv[2].split(',') if len(sys.argv) > 2 else None
addon_utils.enable('cycles')
bpy.ops.wm.open_mainfile(filepath=os.path.join(HERE, 'CargoTerminal.blend'))
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 10; sc.cycles.use_denoising = True
sc.view_settings.view_transform = 'Standard'
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.010, 0.030, 0.075, 1)
w.node_tree.nodes['Background'].inputs['Strength'].default_value = 2.2
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 2.6; sun.angle = math.radians(8)
so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(-35))
sc.render.use_freestyle = True; sc.render.line_thickness_mode = 'ABSOLUTE'
fs = sc.view_layers[0].freestyle_settings; fs.crease_angle = math.radians(140)
ls = fs.linesets[0] if fs.linesets else fs.linesets.new('L')
ls.select_silhouette = ls.select_border = ls.select_crease = ls.select_external_contour = True
ls.select_material_boundary = True
st = ls.linestyle or bpy.data.linestyles.new('BP'); ls.linestyle = st
st.color = (0.88, 0.94, 1.0); st.thickness = 1.8 * S

VIEWS = {}


def ortho(name, loc, target, scale, w, h, up=(0, 0, 1)):
    cd = bpy.data.cameras.new(name); cd.type = 'ORTHO'; cd.ortho_scale = scale; cd.clip_end = 5000
    co = bpy.data.objects.new(name, cd); sc.collection.objects.link(co); co.location = loc
    d = Vector(target) - Vector(loc)
    co.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler() if up == (0, 0, 1) else co.rotation_euler
    VIEWS[name] = dict(type='ortho', loc=list(loc), target=list(target), scale=scale, w=w, h=h)
    return co


def persp(name, loc, target, lens, w=3840, h=2160):
    cd = bpy.data.cameras.new(name); cd.lens = lens; cd.clip_end = 5000
    co = bpy.data.objects.new(name, cd); sc.collection.objects.link(co); co.location = loc
    co.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    VIEWS[name] = dict(type='persp', w=w, h=h)
    return co


CUT = ('Roofs', 'OuterWallsHigh', 'FireWallsHigh', 'HighBayGlazing', 'OfficeWindowsUpper', 'Office_F1_Slab', 'Office_F1_Walls',
       'Office_F2_Slab', 'Office_F2_Walls', 'Office_StairCore', 'ColdCeilings', 'Skylights', 'SmokeVents', 'SolarPanels',
       'RefrigerationPlant', 'SignText', 'AirsideCanopy', 'LandsideCanopy', 'OfficeEntranceCanopy')


def shot(co, path):
    v = VIEWS[co.name]
    for n in CUT:
        o = bpy.data.objects.get(n)
        if o: o.hide_render = co.name.startswith('cut')
    sc.camera = co; sc.render.resolution_x = int(v['w'] * S); sc.render.resolution_y = int(v['h'] * S)
    sc.render.filepath = path; bpy.ops.render.render(write_still=True); print('rendered', path, flush=True)


cams = []
# top: straight down, north of the drawing = the airside at the bottom? no: landside at the top, apron at the bottom
top = ortho('top', (195, 22, 500), (195, 22, 0), 711, 3840, 2160)
top.rotation_euler = (0, 0, 0)
cams.append(top)
# elevations: two halves each, 35 m tall bands
for nm, y, rot in (('front', -700, 1), ('back', 770, -1)):
    for half, (xa, xb) in (('w', (-8, 200)), ('e', (190, 398))):
        xc = (xa + xb) / 2; span = xb - xa
        h = int(round(3840 * 35 / span))
        c = ortho(f'{nm}_{half}', (xc, y, 14), (xc, 35, 14), span, 3840, h)
        cams.append(c)
# end elevations (west end seen from the west, east end from the east), 150 m wide bands
for nm, x in (('west', -700), ('east', 1090)):
    c = ortho(nm, (x, 25, 14), (195, 25, 14), 150, 3840, int(round(3840 * 38 / 150)))
    cams.append(c)
cams.append(persp('airside_above', (-170, -330, 170), (205, 25, 4), 40))
cams.append(persp('landside_above', (560, 330, 110), (190, 40, 6), 38))
cams.append(persp('apron_low', (430, -150, 22), (170, 20, 10), 30))
# cutaways: straight down over the building (a 3D floor plan), and from above at the west and the east end
c = ortho('cut_top', (195, 34, 300), (195, 34, 0), 400, 3840, 1000); c.rotation_euler = (0, 0, 0); cams.append(c)
cams.append(persp('cut_west', (-75, -150, 135), (150, 42, 0), 30))
cams.append(persp('cut_east', (480, -125, 115), (300, 42, 0), 30))
json.dump(VIEWS, open(os.path.join(OUT, 'views.json'), 'w'), indent=1)
for c in cams:
    if ONLY and c.name not in ONLY: continue
    shot(c, os.path.join(OUT, c.name + '.png'))
