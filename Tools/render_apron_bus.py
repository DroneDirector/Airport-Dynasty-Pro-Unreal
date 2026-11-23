# High-detail renders of the textured apron bus (owner 2026-11-23: three pictures, two outside and one inside): the
# built model (Art/ApronBus/ApronBus.blend, from Tools/make_apron_bus.py) on photo-scanned apron asphalt (Poly Haven
# asphalt_04, CC0) under a daylight sky; Cycles on the CPU with denoising. Writes Art/ApronBus/Renders/*.png.
#   python3 Tools/render_apron_bus.py [--only=name,name] [--samples=96] [--res=2560x1440]
import bpy, math, os, sys
from mathutils import Vector
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'Art', 'ApronBus'); OUT = os.path.join(ART, 'Renders'); os.makedirs(OUT, exist_ok=True)
arg = lambda k, d: next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--' + k + '=')), d)
SAMPLES = int(arg('samples', '96')); RES = tuple(int(v) for v in arg('res', '2560x1440').split('x'))
ONLY = arg('only', '') and arg('only', '').split(',')
bpy.ops.wm.open_mainfile(filepath=os.path.join(ART, 'ApronBus.blend'))
sc = bpy.context.scene

# #region ── Glass that is glass, the ground, the sky, the sun ──
g = bpy.data.materials.get('AB_Glass')            # thin glass: see-through with reflections, no lens-like bending
if g:                                              # (the panes are modelled as thin slabs; real refraction through them
    nt = g.node_tree; b = nt.nodes['Principled BSDF']   #  would bend the view like a lens - real bus glass is 5-6 mm)
    for k in ('Transmission Weight', 'Transmission'):
        if k in b.inputs: b.inputs[k].default_value = 0.0
    b.inputs['Base Color'].default_value = (0.02, 0.03, 0.035, 1); b.inputs['Roughness'].default_value = 0.03
    b.inputs['Alpha'].default_value = 0.18
    if hasattr(g, 'blend_method'): g.blend_method = 'BLEND'
bpy.ops.mesh.primitive_plane_add(size=1600, location=(0, 0, 0)); gr = bpy.context.active_object; gr.name = 'Apron'
m = bpy.data.materials.new('Asphalt'); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (400, 400, 1)
nt.links.new(tc.outputs['UV'], mp.inputs['Vector'])
A = os.path.join(ROOT, 'Art', 'Ground', 'Scans', 'asphalt_04')
def tex(f, noncolor=False):
    n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(os.path.join(A, f))
    if noncolor: n.image.colorspace_settings.name = 'Non-Color'
    nt.links.new(mp.outputs[0], n.inputs[0]); return n
col = tex('asphalt_04_diff_4k.jpg'); rough = tex('asphalt_04_rough_4k.jpg', True)
dark = nt.nodes.new('ShaderNodeMixRGB'); dark.blend_type = 'MULTIPLY'; dark.inputs[0].default_value = 1.0; dark.inputs[2].default_value = (0.62, 0.64, 0.68, 1)
nt.links.new(col.outputs[0], dark.inputs[1]); nt.links.new(dark.outputs[0], b.inputs['Base Color']); nt.links.new(rough.outputs[0], b.inputs['Roughness'])
gr.data.materials.append(m)
# apron markings: a yellow lead-in line and the stand's red restraint line, painted on the asphalt (thin boxes)
def mark(name, c, s, colr):
    bpy.ops.mesh.primitive_cube_add(location=c); o = bpy.context.active_object; o.name = name; o.scale = (s[0] / 2, s[1] / 2, s[2] / 2)
    mm = bpy.data.materials.new(name); mm.use_nodes = True; mm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*colr, 1)
    mm.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.6; o.data.materials.append(mm)
mark('LeadIn', (0, -4.2, 0.002), (60, 0.15, 0.004), (0.85, 0.62, 0.02))
mark('BusBayLine', (0, 2.2, 0.002), (18, 0.12, 0.004), (0.9, 0.9, 0.88))
mark('BusBayLine2', (0, -2.2, 0.002), (18, 0.12, 0.004), (0.9, 0.9, 0.88))
w = bpy.data.worlds.new('Sky'); w.use_nodes = True; sc.world = w; nt = w.node_tree; bg = nt.nodes['Background']
sky = nt.nodes.new('ShaderNodeTexSky')
for t in ('NISHITA', 'MULTIPLE_SCATTERING', 'SINGLE_SCATTERING', 'PREETHAM', 'HOSEK_WILKIE'):
    try: sky.sky_type = t; break
    except Exception: pass
for k, v in (('sun_elevation', math.radians(32)), ('sun_rotation', math.radians(150)), ('altitude', 100.0)):
    if hasattr(sky, k): setattr(sky, k, v)
nt.links.new(sky.outputs[0], bg.inputs['Color']); bg.inputs['Strength'].default_value = 0.15
sd = bpy.data.lights.new('Sun', 'SUN'); sd.energy = 3.2; sd.angle = math.radians(1.5)
so = bpy.data.objects.new('Sun', sd); sc.collection.objects.link(so); so.rotation_euler = (math.radians(58), 0, math.radians(150))
# inside: the LED strips glow; one soft fill along the ceiling so the inside reads (as the bus's own lights do)
ld = bpy.data.lights.new('CabinFill', 'AREA'); ld.shape = 'RECTANGLE'; ld.size = 11.0; ld.size_y = 1.6; ld.energy = 900
lo = bpy.data.objects.new('CabinFill', ld); sc.collection.objects.link(lo); lo.location = (-0.3, 0, 2.66)
# #endregion

# #region ── Render settings and the three cameras ──
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
sc.cycles.max_bounces = 8; sc.cycles.transmission_bounces = 8; sc.cycles.transparent_max_bounces = 8
sc.render.resolution_x, sc.render.resolution_y = RES; sc.render.resolution_percentage = 100
sc.render.use_freestyle = False
sc.view_settings.view_transform = 'Standard'      # (true paint colours; AgX washes strong yellows out)
sc.view_settings.look = 'None'
sc.view_settings.exposure = -0.9
cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
VIEWS = [
    ('1-outside-front-right', (13.5, -10.5, 2.6), (1.0, 0.0, 1.45), 35, ()),
    ('2-outside-rear-left', (-14.0, 10.0, 3.4), (-0.6, 0.0, 1.35), 35, ()),
    ('3-inside', (-5.9, -0.45, 1.72), (3.0, 0.25, 1.25), 16, ()),
    ('4-front-closeup', (13.5, -3.2, 2.0), (6.6, -0.2, 1.6), 50, ()),
]
for name, loc, at, lens, hide in VIEWS:
    if ONLY and name not in ONLY: continue
    cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cam_d.lens = lens
    cam_d.clip_start = 0.05
    lo.hide_render = not name.startswith('3')
    sc.render.filepath = os.path.join(OUT, f'ApronBus-{name}.png'); bpy.ops.render.render(write_still=True); print('rendered', name, flush=True)
# #endregion
