# The baggage cart's blueprint sheet: the Blender wireframe views (Tools/make_baggage_cart.py -- --wire) with the sizes
# (whole millimetres), moving parts, IK points, reflectors and sounds. Reads Art/BaggageCart/baggagecart-parts.json.
#   C:/Python311/python.exe -I Blueprints/Vehicles/BaggageCart/make_sheet.py
import json, os
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt, matplotlib.image as mpimg
D = os.path.dirname(os.path.abspath(__file__)) + '/'
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(D.rstrip('/'))))
info = json.load(open(os.path.join(ROOT, 'Art', 'BaggageCart', 'baggagecart-parts.json')))
BG, INK, ACC, DIM, IK = '#071426', '#dbe6f3', '#f4b740', '#8fb4e6', '#4cd18a'
mm = lambda v: '%d' % round(v * 1000)
fig = plt.figure(figsize=(30, 34), dpi=100, facecolor=BG)
fig.text(0.02, 0.988, 'AIRPORT DYNASTY PRO · COVERED BAGGAGE CART · BLUEPRINT v2 (open for loading) · WIREFRAME', color=INK, fontsize=28, fontweight='bold', va='top')
fig.text(0.02, 0.973, 'Every line is an edge of the real Blender mesh · sizes in whole millimetres · nothing floats (mesh-contact check) · x forward (drawbar), y left, z up · origin on the ground under the middle', color=DIM, fontsize=15, va='top')
views = [('1-three-quarter-front', 'THREE-QUARTER FRONT (right curtain, bulkhead, drawbar down)', [0.01, 0.70, 0.49, 0.26]),
         ('2-three-quarter-rear', 'THREE-QUARTER REAR (left curtain, net gate, coupling jaw)', [0.50, 0.70, 0.49, 0.26]),
         ('3-side-left', 'SIDE (left)', [0.01, 0.53, 0.49, 0.17]),
         ('4-side-right', 'SIDE (right)', [0.50, 0.53, 0.49, 0.17]),
         ('5-front', 'FRONT', [0.01, 0.36, 0.24, 0.17]),
         ('6-rear', 'REAR', [0.25, 0.36, 0.24, 0.17]),
         ('7-top', 'TOP (front to the right)', [0.50, 0.36, 0.49, 0.17]),
         ('8-drawbar-turntable', 'DRAWBAR, TURNTABLE, DEAD-MAN BRAKE', [0.01, 0.19, 0.24, 0.17]),
         ('9-rear-coupler', 'REAR COUPLING JAW + LIFT PIN', [0.25, 0.19, 0.24, 0.17]),
         ('12-open-for-loading', 'OPEN FOR LOADING (both curtains up, net gate open)', [0.50, 0.19, 0.24, 0.17]),
         ('11-underside', 'UNDERSIDE', [0.75, 0.19, 0.24, 0.17])]
for f, title, r in views:
    a = fig.add_axes(r); a.imshow(mpimg.imread(D + 'wire/' + f + '.png')); a.axis('off')
    a.set_title(title, color=ACC, fontsize=15, loc='left', fontweight='bold')
s = info['size_m']; db = info['drawbar']; fa = info['front_axle']; cr = info['coupler_rear']; cu = info['curtains']
n = fig.add_axes([0.01, 0.005, 0.98, 0.175], facecolor=BG); n.axis('off')
rows = [('OUTSIDE', ACC),
        ('Length %s with the drawbar down (body %s) · width %s over the roof beads (deck %s) · height %s at the roof crown · floor %s above ground · wheelbase %s · track %s · tyres 4.00-8 (415 x 105) · load space %s x %s x %s'
         % (mm(s['length']), mm(s['body_length']), mm(s['width']), mm(s['deck_width']), mm(s['height']), mm(s['deck_height']), mm(s['wheelbase']), mm(s['track']), *[mm(v) for v in s['load_space']]), INK),
        ('Galvanised chassis + 10 mm chequer floor · four 60 x 60 corner posts (no middle posts), 80-deep top rails, crowned aluminium roof on bows · white ribbed front bulkhead · hinged net gate at the rear · navy PVC roll-up curtains both sides (hem, 4 straps + buckles to deck hooks, pull strap)', INK),
        ('Towing eye 40 mm at %s high (x %s) · rear coupling jaw + lift pin at %s high (x %s) - carts chain eye-in-jaw; the next eye is checked to fit round the pin' % (mm(info['tow_eye']['height_m']), mm(info['tow_eye']['centre'][0]), mm(cr['height_m']), mm(cr['pin_axis'][0])), INK),
        ('MOVING PARTS (pivot in Blender metres, axis)', ACC),
        ('Drawbar*: (%.3f, %.3f, %.3f) about y, raise = negative deg up to -82 (dead-man: raised = brake on) · FrontAxle_* + front wheels: (%.3f, %.3f, %.3f) about z, +-80 deg (checked clear) · CouplerRear_Pin*: lift +z 100 mm · wheels roll about y through IK_Wheel_*'
         % (*db['pivot'], *fa['pivot']), INK),
        ('Curtain_L / Curtain_R: scale z about z = %.3f (sheet) with hem/straps/buckles/pull strap moving up · CurtainRoll_L / _R: scale round the tube axis (y +-%.3f, z %.3f) 1.0 -> 1.4 as it opens · NetGate*: about z, negative deg to open (0 .. -180)'
         % (cu['L']['sheet_pivot_z'], abs(cu['L']['roll_axis_point'][1]), cu['L']['roll_axis_point'][2]), INK),
        ('OPENINGS: each side %d x %d clear with the curtain rolled up (roll + hem stay above %d); rear %d x %d clear with the net gate swung open to -180 deg about its hinge on the left rear post (%.3f, %.3f) about z - all checked: nothing in the openings' % (*info['load_openings_mm']['side_each'], round(info['load_openings_mm']['side_each'][1] + 540), *info['load_openings_mm']['rear'], info['net_gate']['pivot'][0], info['net_gate']['pivot'][1]), INK),
        ('IK POINTS (%d): TowEye, Drawbar_Handle, Coupler_Rear (where the next eye goes), Coupler_Pin, Curtain_L/R_Strap_0..3 (buckles: unhook/hook), Curtain_L/R_Handle + HandL/HandR (closed) and HandleUp + HandLUp/HandRUp (rolled up), Stand_Curtain_L/R, NetGate_Handle, NetGate_Latch, NetGate_Hinge, Stand_Rear, Stand_Load_L/R, Load_Min / Load_Max (floor load box), Stand_Drawbar, Stand_Coupler, wheel centres, pivots' % len(info['ik']), IK),
        ('REFLECTORS + SOUNDS', ACC),
        ('Reflectors set into the channel webs: white front x2, amber sides x2 each, red rear x2 (no lamps: carts are unlit, as real ones) · Sounds (Freesound CC0): rolling rattle loop, drawbar clank, coupling pin clunk, curtain rustle (Art/Sounds/BaggageCart)', INK)]
y = 0.98
for t, c in rows:
    n.text(0.0, y, t, color=c, fontsize=14 if c == ACC else 12.5, va='top', fontweight='bold' if c == ACC else 'normal', wrap=True); y -= (0.088 if len(t) < 260 else 0.11) if c != ACC else 0.08
fig.savefig(D + 'BaggageCart-Blueprint-v2.png', facecolor=BG)
print('ok')
