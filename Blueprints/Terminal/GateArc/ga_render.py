# Gate arc, step 3: the pictures. Environment: GA_W / GA_H / GA_SAMPLES / GA_OUT / GA_DEV (GPU or CPU)
#   blender -b F:/ADP-Export/ga_scene.blend --python ga_render.py -- apron raised raised:before inside top
import bpy, sys, os
from mathutils import Vector
E = os.environ.get
OUT = E('GA_OUT') or 'F:/ADP-Export/ga_raw'
os.makedirs(OUT, exist_ok=True)


def P(x, z, h): return Vector((x - 200.0, z, h))


# name: (from, to, lens, exposure, options)
SH = {'apron': (P(-258, 106.5, 1.65), P(-217, 98.0, 5.4), 21, -0.4, ''),
      'raised': (P(-262, 129, 15.0), P(-216, 99, 3.6), 25, -0.4, ''),
      'front': (P(-232, 113, 20.0), P(-216.5, 84, 5.2), 24, -0.4, ''),
      'close': (P(-243.5, 103, 1.65), P(-214.5, 86.5, 5.6), 21, -0.4, ''),
      'inside': (P(-230.0, 78.5, 8.15), P(-218.0, 83.3, 6.75), 17, 0.1, 'in'),
      'inside2': (P(-207.3, 78.4, 7.65), P(-220.0, 86.0, 7.75), 17, 0.1, 'in'),
      'aisle': (P(-223.5, 78.5, 7.65), P(-223.5, 84.9, 7.25), 17, 0.1, 'in'),
      'through': (P(-223.48, 68.8, 7.65), P(-223.48, 84.9, 7.15), 20, 0.1, 'in opt'),
      'seats': (P(-216.5, 77.0, 60.0), P(-216.5, 77.0, 0.0), 0, 0.2, 'ortho cut in'),
      'seatsopt': (P(-216.5, 77.0, 60.0), P(-216.5, 77.0, 0.0), 0, 0.2, 'ortho cut in opt'),
      'top': (P(-215.0, 98.0, 120.0), P(-215.0, 98.0, 0.0), 0, -0.3, 'ortho')}
sc = bpy.context.scene; cam = sc.camera
for l in bpy.data.lights:                                  # (a saved sun reads 100 times too strong: set here)
    if l.type == 'SUN': l.energy = 3.4
if E('GA_DEV', 'GPU') == 'GPU':
    p = bpy.context.preferences.addons['cycles'].preferences; p.compute_device_type = 'OPTIX'; p.get_devices()
    for d in p.devices: d.use = d.type == 'OPTIX'
    sc.cycles.device = 'GPU'
else: sc.cycles.device = 'CPU'
sc.cycles.samples = int(E('GA_SAMPLES', '192')); sc.cycles.use_adaptive_sampling = True; sc.cycles.adaptive_threshold = 0.012
sc.cycles.use_denoising = True; sc.cycles.denoiser = 'OPENIMAGEDENOISE'
sc.render.resolution_x, sc.render.resolution_y = int(E('GA_W', '3840')), int(E('GA_H', '2160')); sc.render.resolution_percentage = 100
for a in sys.argv[sys.argv.index('--') + 1:]:
    n, _, mode = a.partition(':'); frm, to, lens, ex, opt = SH[n]; before = mode == 'before'
    bpy.data.collections['OLD9'].hide_render = not before; bpy.data.collections['ARC'].hide_render = before
    cam.data.type = 'ORTHO' if 'ortho' in opt else 'PERSP'; cam.data.ortho_scale = 42.0 if 'cut' in opt else 135.0
    W_ = opt.split(); bpy.data.collections['ROWS_NOW'].hide_render = 'opt' in W_; bpy.data.collections['ROWS_OPT'].hide_render = 'opt' not in W_; hid = []
    if 'cut' in W_:                                        # the ceiling and the roof off
        for o in bpy.data.objects:
            if o.type != 'MESH' or o.hide_render: continue
            bb = [o.matrix_world @ Vector(c) for c in o.bound_box]; hh = [v.z for v in bb]; yy = [v.y for v in bb]
            if max(hh) > 9.3 and max(yy) > 50 and min(yy) < 90 and (min(hh) > 10.5 or o.name.split('/')[-1].startswith(('STRUCTURE-white', 'STRUCTURE-plaster', 'SOLID')) or o.name.startswith(('F2-WALK/CEIL', 'F1-PIER'))): o.hide_render = True; hid.append(o)
    if lens: cam.data.lens = lens
    cam.location = frm; cam.rotation_euler = (0, 0, 0) if 'ortho' in opt else (to - frm).to_track_quat('-Z', 'Y').to_euler(); sc.view_settings.exposure = ex
    ext = []
    if 'in' in opt.split():
        for x in (-229, -221, -213, -206):
            for z in (71.5, 77.5):
                L = bpy.data.lights.new('in', 'AREA'); L.size = 5; L.energy = 240; L.color = (1, 0.95, 0.88)
                o = bpy.data.objects.new('in', L); sc.collection.objects.link(o); o.location = P(x, z, 11.2); o.visible_camera = o.visible_glossy = False; ext.append(o)
    bpy.context.view_layer.update()
    sc.render.filepath = os.path.join(OUT, n + ('_' + mode if mode else '') + '.png'); bpy.ops.render.render(write_still=True); print('RENDERED', n, mode, flush=True)
    for o in ext: bpy.data.objects.remove(o)
    for o in hid: o.hide_render = False
