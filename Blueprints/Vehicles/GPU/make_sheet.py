# The ground power unit's blueprint sheet: the Blender wireframe views (Tools/make_gpu.py -- --wire) with the sizes
# (whole millimetres), moving parts, IK points, lights and sounds. Reads Art/GPU/gpu-parts.json (written by the build).
#   python -I Blueprints/Vehicles/GPU/make_sheet.py
import json, os
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt, matplotlib.image as mpimg
D = os.path.dirname(os.path.abspath(__file__)) + '/'
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(D.rstrip('/'))))
info = json.load(open(os.path.join(ROOT, 'Art', 'GPU', 'gpu-parts.json')))
BG, INK, ACC, DIM, IK = '#071426', '#dbe6f3', '#f4b740', '#8fb4e6', '#4cd18a'
mm = lambda v: '%d' % round(v * 1000)
fig = plt.figure(figsize=(30, 34), dpi=100, facecolor=BG)
fig.text(0.02, 0.988, 'AIRPORT DYNASTY PRO · GROUND POWER UNIT (towable diesel, 90 kVA 400 Hz, A320 class) · BLUEPRINT v1 · WIREFRAME', color=INK, fontsize=28, fontweight='bold', va='top')
fig.text(0.02, 0.973, 'Every line is an edge of the real Blender mesh · sizes in whole millimetres · nothing floats (mesh-contact check) · x forward (drawbar), y left, z up', color=DIM, fontsize=15, va='top')
views = [('1-three-quarter-front', 'THREE-QUARTER FRONT (right side: cable tray, plug, gland)', [0.01, 0.70, 0.49, 0.26]),
         ('2-three-quarter-rear', 'THREE-QUARTER REAR (left side, control-panel door)', [0.50, 0.70, 0.49, 0.26]),
         ('3-side-left', 'SIDE (left: engine door, chocks)', [0.01, 0.53, 0.49, 0.17]),
         ('4-side-right', 'SIDE (right: generator door, cable coils)', [0.50, 0.53, 0.49, 0.17]),
         ('5-front', 'FRONT', [0.01, 0.36, 0.24, 0.17]),
         ('6-rear', 'REAR', [0.25, 0.36, 0.24, 0.17]),
         ('7-top', 'TOP (front to the right)', [0.50, 0.36, 0.49, 0.17]),
         ('8-panel-and-doors-open', 'CONTROL PANEL (door taken off)', [0.01, 0.19, 0.24, 0.17]),
         ('9-plug-and-cable', 'PLUG IN ITS DUMMY SOCKET + CABLE GLAND', [0.25, 0.19, 0.24, 0.17]),
         ('10-engine-bay', 'ENGINE BAY (left door off)', [0.50, 0.19, 0.24, 0.17]),
         ('11-generator-bay', 'GENERATOR BAY (right door off)', [0.75, 0.19, 0.24, 0.17])]
for f, title, r in views:
    a = fig.add_axes(r); a.imshow(mpimg.imread(D + 'wire/' + f + '.png')); a.axis('off')
    a.set_title(title, color=ACC, fontsize=15, loc='left', fontweight='bold')
s = info['size_m']; dr = info['doors']
n = fig.add_axes([0.01, 0.005, 0.98, 0.175], facecolor=BG); n.axis('off')
rows = [('OUTSIDE', ACC),
        ('Length %s with the drawbar down (body/bed plate 2200) · width %s (trays 1732) · height %s to the exhaust cap (roof 1640, beacon 1805) · wheelbase 1400 · track 1400 · tyres 560 x 165' % (mm(s['length']), mm(s['width']), mm(s['height'])), INK),
        ('White moulded canopy with rounded edges on a galvanised chassis tub; bed plate = floor of both cable trays (tray walls 220 high); fifth-wheel turntable steering +-40 deg (checked clear); drawbar 1000 from hinge to eye, 76 mm lunette, parking support', INK),
        ('MOVING PARTS (pivot in Blender metres, axis)', ACC),
        ('  '.join('%s: (%.3f, %.3f, %.3f) about %s' % (k, *v['pivot'], v['axis']) for k, v in dr.items()) + '   ·   Drawbar*: (%.3f, %.3f, %.3f) about y   ·   FrontAxle_* + front wheels: (%.3f, %.3f) about z' % (*info['drawbar']['pivot'], info['front_axle']['pivot'][0], info['front_axle']['pivot'][1]), INK),
        ('Cable: CableGland at (%.3f, %.3f, %.3f) · CablePlug* parked nose-first (-x) at (%.3f, %.3f, %.3f) · CableCoil_0..4 (hide when paid out) · CableRun_Gland, CableRun_Plug · about %s m' % (*info['cable_gland'], *info['plug']['nose'], info['cable_length_m']), INK),
        ('IK POINTS (%d): door handles (engine, generator, panel), panel Start / Stop / Contactor ON / OFF / E-stop, Stand_Panel, plug grip 1 + 2, Stand_Plug, drawbar handle, tow eye, parking support + foot, fuel filler, cable exit, wheel centres, pivots' % len(info['ik']), IK),
        ('LIGHTS + SOUNDS', ACC),
        ('Amber roof beacon (flashing like the jetway) · tail/brake + indicator each side set into the rear bumper · red rear, amber side and white front reflectors - every one a housing set into flat plate', INK),
        ('Sounds (Freesound CC0): engine start, idle loop, stop, access door open + close, plug connect, plug disconnect (Art/Sounds/GPU)', INK)]
y = 0.98
for t, c in rows:
    n.text(0.0, y, t, color=c, fontsize=14 if c == ACC else 12.5, va='top', fontweight='bold' if c == ACC else 'normal', wrap=True); y -= 0.105 if c != ACC else 0.09
fig.savefig(D + 'GPU-Blueprint-v1.png', facecolor=BG)
print('ok')
