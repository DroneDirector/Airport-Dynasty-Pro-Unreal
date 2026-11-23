# To-do 213 detailed renders from F:/ADP-Export/tc_after.blend (build_scene.py -> build_new.py), several shots in one load:
#   TC_W=960 TC_H=540 TC_SAMPLES=16 TC_OUT=<dir> blender -b F:/ADP-Export/tc_after.blend --python render_shot.py -- D1 D3a ...
# Writes <name>_raw.png + <name>_labels.json; annotate.py adds the labels and the caption bar.
import bpy, sys, os, json
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
OUT = os.environ.get('TC_OUT') or 'F:/ADP-Export/tc_raw'
os.makedirs(OUT, exist_ok=True)
def P(x, z, h): return Vector((x - 200.0, z, h))
N, E = 'NEW', 'EXISTING'
OUTL = [(N, 'East Hall', 194, -4, 4), (N, 'Glass front up to the curved roof, x 156-551', 345, 0, 9), (N, 'Double-skin glass front (offices keep their windows)', 132, -4, 9), (N, 'Taxi rank east', 198, -20, 3), (N, 'Taxi rank west', -40, -20, 3),
        (N, 'Coach + bus station', 510, -2, 8), (N, 'Parking pay machines', 79, -57, 2.8), (N, '5 coach bays', 511, -23, 3), (N, 'Promenade with shops (behind the glass)', 420, 0, 3)]
EXL = [(E, 'Grand Hall', 78, 0, 10), (E, 'Arrivals wing', -40, -4, 8), (E, 'Curved roof + tree pillars (new smooth roof, already in the game)', 280, -16, 20), (E, 'Coach stands', 100, -23, 3)]
GATES = [(E, 'Gate %d' % g, x, 75, 12.2) for g, x in ((13, 272.4), (14, 352.2), (15, 424.5), (16, 496.8))]
GF = [(N, 'East Hall: check-in, cafe, car rental, meeting point', 192, 26, 4.5), (N, 'Customs', 232, 28, 5.5), (N, 'Claim hall east, 2 wide-body belts', 270, 36, 4), (N, 'Arrivals duty free', 291, 14, 4.5), (N, 'Lost + found', 254, 12, 3.6),
      (N, 'Passport control IN: 6 booths, 8 e-gates', 318, 34, 5.5), (N, 'Queue hall', 335, 26, 2), (N, 'Crew centre', 388, 16, 5.5), (N, 'Canteen', 420, 16, 5.5), (N, 'Handling + airlines', 453, 16, 5.5), (N, 'Lockers', 388, 39, 5.5), (N, 'Staff security', 420, 39, 5.5), (N, 'Police + border guard', 453, 39, 5.5),
      (N, 'Coach + bus station', 510, 28, 3), (N, 'Promenade with shops', 310, 3, 3)]
F2 = [(N, 'Glass passport wall x 240', 240, 60, 11.4), (N, 'Passport control OUT: queue, 4 booths, 8 e-gates', 250, 36, 9.8), (N, 'Duty free', 274, 38, 9.9), (N, 'Transfer centre', 323, 38, 9.9), (N, 'Food court, cafe, bar', 370, 38, 9.9), (N, 'Prayer room, toilets', 412, 38, 9.9), (N, 'Premium lounge', 448, 38, 9.9), (N, 'Seating', 496, 38, 8), (N, 'Shops, toilets', 536, 38, 9.9),
      (N, 'Swing lobby, gate 13', 246, 75, 10.9), (N, 'Arrivals way down (each gate)', 372, 76, 9.6), (E, 'Gate walkway', 430, 62, 8)]
# id: (file name, camera, look at, lens, options, labels)
SHOTS = {
    'D1': ('D1-Aerial-Forecourt', P(515, -135, 74), P(305, 12, 2), 24, '', EXL + OUTL + [(N, 'Flat roof z 34-52 over the new halls', 330, 43, 12.6), (E, 'Gates 13-16 + jetways', 390, 82, 10), (E, 'Gates 1-8 + jetways', 60, 84, 10)]),
    'D1b': ('D1b-Aerial-Forecourt-BEFORE', P(515, -135, 74), P(305, 12, 2), 24, 'before', EXL + [(E, 'Open ground under the roof (no hall yet)', 350, 20, 2), (E, 'Gates 13-16 + jetways', 390, 82, 10)]),
    'D3a': ('D3a-Cutaway-East-Floor2', P(345, -105, 112), P(385, 46, 0), 24, 'cut', [l for l in F2] + GATES + [(N, 'East Hall (open to the roof)', 192, 20, 4)]),
    'D3b': ('D3b-Cutaway-East-Ground-Floor', P(335, -100, 112), P(365, 30, 0), 24, 'cut nof2', GF + GATES),
    'D3c': ('D3c-Cutaway-Concourse-Close', P(222, -30, 48), P(292, 48, 4), 24, 'cut', F2[:4] + F2[9:] + GATES[:1] + [(N, 'East Hall below', 205, 14, 4)]),
    'D2': ('D2-East-End-Forecourt', P(132, -50, 7.5), P(222, 6, 7.5), 22, '', OUTL[:5] + [EXL[0], EXL[2]]),
    'D2b': ('D2b-East-End-Forecourt-BEFORE', P(132, -50, 7.5), P(222, 6, 7.5), 22, 'before', [EXL[0], (E, 'Open ground today', 260, 10, 2)]),
    'D4': ('D4-Floor2-Passport-Wall', P(221, 63.5, 7.8), P(243, 55.5, 8.4), 18, 'e-0.45', [(N, 'Glass passport wall', 240, 63, 10.2), (N, 'Way to passport control', 235.4, 52, 9.6), (N, 'Non-Schengen side', 252, 60, 8.2)]),
    'D4b': ('D4b-Floor2-Passport-Wall-BEFORE', P(221, 63.5, 7.8), P(243, 55.5, 8.4), 18, 'before e-0.45', []),
    'D4c': ('D4c-Floor2-Passport-Control', P(232.5, 51, 8.0), P(252, 36, 7.4), 18, 'in e-0.1', [(N, 'Queue + EES kiosks', 238, 30, 8), (N, '4 booths', 252, 30, 8.5), (N, '8 e-gates', 252, 43, 8)]),
    'D5': ('D5-Concourse-Walking-In', P(263, 57.8, 8.1), P(276.5, 36, 7.6), 16, 'in e-0.15 L0.5', [(N, 'Duty free', 274, 50, 9.4), (N, 'Way into the concourse', 264, 52, 10)]),
    'D5c': ('D5c-Concourse-Inside', P(396, 51, 8.2), P(362, 30, 7.2), 18, 'in e0.3', [(N, 'Food court', 370, 38, 8), (N, 'Window gallery to the forecourt', 352, 22, 7.6)]),
    'D5d': ('D5d-Concourse-Window-Gallery', P(322.6, 20.9, 8.3), P(420, 22.2, 7.2), 18, 'in e0.5', [(N, 'Window gallery over the promenade', 340, 22, 8)]),
    'D6': ('D6-Claim-Hall-Arrivals-Duty-Free', P(247, 44, 3.9), P(288, 17, 1.6), 18, 'in e0.95', [(N, 'Belts 6 + 7', 268, 34, 3.6), (N, 'Arrivals duty free', 288, 19, 4), (N, 'Passport control IN', 318, 30, 4)]),
    'D7': ('D7-Grand-Hall', P(60, 10.5, 5.8), P(110, 20, 0.6), 24, 'in e0.4', [(N, '12 kiosks', 61, 3.5, 1.9), (N, 'Bag drop', 107, 17, 2.2), (N, 'Boarding-pass e-gates', 78, 29, 1.7), (N, 'Service desk', 112, 25, 2.4), (E, 'Grand Hall, check-in islands', 72, 18, 3.4)]),
    'D7b': ('D7b-Grand-Hall-BEFORE', P(60, 10.5, 5.8), P(110, 20, 0.6), 24, 'before in e0.4', []),
    'D8': ('D8-Taxi-Rank', P(217, -31, 3.6), P(193, -16.5, 2.6), 22, '', [(N, 'Taxi rank', 200, -20, 2.9), (N, 'East Hall entrance', 194, -4, 4), (N, 'Double-skin front', 140, -4, 9)]),
    'D9': ('D9-Double-Skin-Forecourt', P(124, -20.2, 2.6), P(134, 0, 8.2), 16, '', [(N, 'Double-skin glass front', 124, -4, 8), (E, 'Baggage block + office windows', 137, 0, 8)]),
    'D9b': ('D9b-Double-Skin-Forecourt-BEFORE', P(124, -20.2, 2.6), P(134, 0, 8.2), 16, 'before', []),
    'D10': ('D10-Double-Skin-From-Office', P(136.4, 4.6, 7.8), P(134.6, -12, 7.2), 17, 'in e-0.1 L0.05', [(N, 'Outer glass skin (z -4)', 134, -4, 9.4)]),
    'D11': ('D11-East-Hall', P(226.6, 9.5, 5.0), P(174, 22, 2.4), 17, 'in e0.45', [(N, 'Check-in, bag drop', 174, 27, 3.6), (N, 'Meeting point', 212, 20, 4), (N, 'Cafe', 165, 8, 3)]),
}
sc = bpy.context.scene; cam = sc.camera
p = bpy.context.preferences.addons['cycles'].preferences; p.compute_device_type = 'OPTIX'; p.get_devices()
for d in p.devices: d.use = (d.type == 'OPTIX')
sc.cycles.device = 'GPU'; print('DEVICE', [(d.name, d.type) for d in p.devices if d.use])
sc.cycles.samples = int(os.environ.get('TC_SAMPLES', '160')); sc.cycles.use_adaptive_sampling = True; sc.cycles.adaptive_threshold = 0.015; sc.cycles.use_denoising = True; sc.cycles.denoiser = 'OPENIMAGEDENOISE'
sc.render.use_persistent_data = False
sc.render.resolution_x, sc.render.resolution_y = int(os.environ.get('TC_W', '3840')), int(os.environ.get('TC_H', '2160'))
COL = bpy.data.collections
if 'door_e_closed' not in bpy.data.objects:                       # (gate 4 does not swing: the wall to the lobby at x 237 stays shut)
    me = bpy.data.meshes.new('door_e_closed'); x_, w_ = 237.05, 0.1
    me.from_pydata([P(x, z, h) for h in (6.0, 9.2) for x, z in ((x_ - w_, 70), (x_ + w_, 70), (x_ + w_, 77), (x_ - w_, 77))], [], [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)])
    me.materials.append(bpy.data.materials.get('N2_plaster')); COL['NEW_f2'].objects.link(bpy.data.objects.new('door_e_closed', me))
for o in bpy.data.objects:
    if o.type == 'FONT' and o.data.body == 'SWING GATES': o.data.body = 'SWING GATE 13'     # (owner 2026-11-23: only gate 13 swings)
for l in bpy.data.lights:                                      # (the saved file's sun read 260 for 2.6: set and checked here)
    if l.type == 'SUN': l.energy = float(os.environ.get('TC_SUN', '3.0')); print('SUN', l.name, l.energy)
def blo(o): b = [o.matrix_world @ Vector(c) for c in o.bound_box]; return min(v.x for v in b) + 200, max(v.x for v in b) + 200, min(v.y for v in b), max(v.y for v in b), min(v.z for v in b), max(v.z for v in b)
for sid in sys.argv[sys.argv.index('--') + 1:]:
    name, frm, to, lens, opt, labels = SHOTS[sid]; before = 'before' in opt
    cam.data.lens = lens; cam.data.clip_start = 0.05; cam.location = frm; cam.rotation_euler = (to - frm).to_track_quat('-Z', 'Y').to_euler()
    vis = lambda c, v: c in COL and setattr(COL[c], 'hide_render', not v)
    vis('NEW', not before); vis('GOLD', not before and 'nogold' not in opt)
    for k in ('ground', 'f2', 'roof', 'out'):
        h = before or ('cut' in opt and k == 'roof') or ('nof2' in opt and k == 'f2'); vis('NEW_' + k, not h); vis('GOLD_' + k, not h)
    for k in ('body', 'roof', 'lid'):
        s_ = not (k != 'body' and 'cut' in opt); vis('J_real_' + k, s_); vis('J_before_' + k, s_ and before); vis('J_after_' + k, s_ and not before)
    g = bpy.data.materials['X2_GOLD'].node_tree.nodes['Principled BSDF']; g.inputs['Emission Strength'].default_value = 0.55 if 'in' in opt.split() else 1.15; g.inputs['Emission'].default_value = (1, 0.46, 0.03, 1)
    sc.view_settings.exposure = next((float(w[1:]) for w in opt.split() if w[0] == 'e' and w[1:2] in '-0123456789'), 0.0)
    extra = []
    if 'in' in opt.split():                                    # soft light inside (the game's own lamps are real lights there)
        for k in range(5):
            d = (to - frm); q = frm + d * (0.1 + 0.45 * k); L = bpy.data.lights.new('in', 'AREA'); L.size = 9; L.energy = 900 * next((float(w_[1:]) for w_ in opt.split() if w_[0] == 'L'), 1.0); L.color = (1, 0.95, 0.88)
            o = bpy.data.objects.new('in', L); sc.collection.objects.link(o); o.location = (q.x, q.y, min(frm.z + 2.4, 10.4)); extra.append(o); o.visible_camera = False; o.visible_glossy = False
    bpy.context.view_layer.update()
    out = []
    for (kind, t, x, z, h) in labels:
        v = world_to_camera_view(sc, cam, P(x, z, h))
        if 0.01 < v.x < 0.99 and 0.02 < v.y < 0.98 and v.z > 0: out.append(dict(kind=kind, text=t, x=v.x, y=1 - v.y, d=v.z))
    json.dump(out, open(os.path.join(OUT, name + '_labels.json'), 'w'), indent=1)
    sc.render.filepath = os.path.join(OUT, name + '_raw.png'); bpy.ops.render.render(write_still=True); print('RENDERED', name, flush=True)
    for o in extra: bpy.data.objects.remove(o)
