# The electric baggage tow tractor's blueprint sheet: the Blender wireframe views (Tools/make_baggage_tractor.py -- --wire)
# with dimension lines in whole millimetres drawn onto the orthographic views (metres -> pixels from the projections the
# build writes), moving parts, IK points, lamps, hitches and sounds. Reads Art/BaggageTractor/baggagetractor-parts.json.
#   python -I Blueprints/Vehicles/BaggageTractor/make_sheet.py
import json, os
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt, matplotlib.image as mpimg
D = os.path.dirname(os.path.abspath(__file__)) + '/'
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(D.rstrip('/'))))
info = json.load(open(os.path.join(ROOT, 'Art', 'BaggageTractor', 'baggagetractor-parts.json')))
BG, INK, ACC, DIM, IKC = '#071426', '#dbe6f3', '#f4b740', '#8fd0ff', '#4cd18a'
mm = lambda v: '%d' % round(v * 1000)
S, V = info['size_m'], info['wire_views']
fig = plt.figure(figsize=(30, 40), dpi=100, facecolor=BG)
fig.text(0.02, 0.990, 'AIRPORT DYNASTY PRO · ELECTRIC BAGGAGE TOW TRACTOR (closed cab, 80 V, 25 kN class) · BLUEPRINT v2 · WIREFRAME (interior joints fixed)', color=INK, fontsize=28, fontweight='bold', va='top')
fig.text(0.02, 0.977, 'Our own design (no brand) drawn to the compact European class (T135 / Comet 3E / JST-25E sizes) · every line is an edge of the real Blender mesh · sizes in whole millimetres · x forward, y left, z up',
         color=DIM, fontsize=15, va='top')

def view(name, title, rect):
    a = fig.add_axes(rect); im = mpimg.imread(D + 'wire/' + name + '.png'); a.imshow(im); a.axis('off')
    a.set_title(title, color=ACC, fontsize=15, loc='left', fontweight='bold'); a.set_xlim(0, im.shape[1]); a.set_ylim(im.shape[0], 0)
    return a
def P(vn, p):
    v = V[vn]; O, X, Y, Z = v['O'], v['X'], v['Y'], v['Z']
    return (O[0] + p[0] * (X[0] - O[0]) + p[1] * (Y[0] - O[0]) + p[2] * (Z[0] - O[0]), O[1] + p[0] * (X[1] - O[1]) + p[1] * (Y[1] - O[1]) + p[2] * (Z[1] - O[1]))
def dim(a, vn, p0, p1, off, label=None, col=DIM, fs=12):
    """A dimension line between two points (metres), offset 'off' pixels sideways, with extension lines and the size."""
    (x0, y0), (x1, y1) = P(vn, p0), P(vn, p1)
    dx, dy = x1 - x0, y1 - y0; L = (dx * dx + dy * dy) ** 0.5 or 1; nx, ny = -dy / L * off, dx / L * off
    a.plot([x0, x0 + nx], [y0, y0 + ny], color=col, lw=0.8, alpha=0.7); a.plot([x1, x1 + nx], [y1, y1 + ny], color=col, lw=0.8, alpha=0.7)
    a.annotate('', xy=(x1 + nx, y1 + ny), xytext=(x0 + nx, y0 + ny), arrowprops=dict(arrowstyle='<->', color=col, lw=1.2))
    d = sum((p1[i] - p0[i]) ** 2 for i in range(3)) ** 0.5
    a.text((x0 + x1) / 2 + nx * 1.25, (y0 + y1) / 2 + ny * 1.25, label or mm(d), color=col, fontsize=fs, ha='center', va='center', fontweight='bold',
           bbox=dict(facecolor=BG, edgecolor='none', alpha=0.75, pad=1.5))
def hmark(a, vn, x, z, label, col=IKC, side=1):
    px, py = P(vn, (x, 0, z)); a.plot([px - 30, px + 30], [py, py], color=col, lw=1.5)
    a.text(px + side * 38, py, label, color=col, fontsize=11, va='center', ha='left' if side > 0 else 'right', bbox=dict(facecolor=BG, edgecolor='none', alpha=0.7, pad=1))

FX, RX, XF, XR = S['front_axle_x'], S['rear_axle_x'], 1.52, -1.46
# ---- the big dimensioned side view
a = view('3-side-left', 'SIDE (left: driver door, kick step) - dimensions in mm', [0.01, 0.725, 0.62, 0.24])
dim(a, '3-side-left', (S['x_max'], 0, 0), (S['x_min'], 0, 0), 95, 'overall %s (with both hitches)' % mm(S['length']))
dim(a, '3-side-left', (XF, 0, 0), (XR, 0, 0), 55, 'body %s' % mm(S['body_length']))
dim(a, '3-side-left', (FX, 0, 0.0), (RX, 0, 0.0), 20, 'wheelbase %s' % mm(S['wheelbase']))
dim(a, '3-side-left', (XF, 0, 2.0), (FX, 0, 2.0), -20, 'front overhang %s' % mm(XF - FX))
dim(a, '3-side-left', (RX, 0, 2.0), (XR, 0, 2.0), -20, 'rear overhang %s' % mm(RX - XR))
dim(a, '3-side-left', (XR - 0.30, 0, 0), (XR - 0.30, 0, S['height_roof']), 60, 'roof %s' % mm(S['height_roof']))
dim(a, '3-side-left', (-0.35, 0, 0), (-0.35, 0, S['height_beacon']), 0, 'beacon %s' % mm(S['height_beacon']), fs=11)
dim(a, '3-side-left', (XF + 0.35, 0, 0), (XF + 0.35, 0, S['deck']), -40, 'belt / deck %s' % mm(S['deck']))
dim(a, '3-side-left', (FX, 0, 0), (FX, 0, S['front_tyre'][0]), -10, 'tyre %s' % mm(S['front_tyre'][0]), fs=10)
dim(a, '3-side-left', (RX, 0, 0), (RX, 0, S['rear_tyre'][0]), -10, 'tyre %s' % mm(S['rear_tyre'][0]), fs=10)
for z, t in ((0.28, 'eye 280'), (0.40, 'eye 400 (GPU, main)'), (0.49, 'eye 490')): hmark(a, '3-side-left', -1.62, z, t, side=-1)
hmark(a, '3-side-left', 1.68, 0.40, 'front pin eye 400', side=1)
hmark(a, '3-side-left', -0.09, S['floor'], 'floor %s (inside)' % mm(S['floor']), col=ACC)
hmark(a, '3-side-left', -0.09, S['seat_cushion_top'], 'seat %s' % mm(S['seat_cushion_top']), col=ACC)
hmark(a, '3-side-left', -0.01, 0.315, 'step 315', col=ACC)
hmark(a, '3-side-left', 0.0, S['ground_clearance'], 'clearance %s' % mm(S['ground_clearance']), col=ACC)
# ---- front / rear / top with sizes
a = view('5-front', 'FRONT', [0.635, 0.845, 0.18, 0.12])
dim(a, '5-front', (0, 0.70, 0.0), (0, -0.70, 0.0), 40, 'body %s' % mm(S['width_body']), fs=11)
dim(a, '5-front', (0, S['width_mirrors'] / 2, 1.42), (0, -S['width_mirrors'] / 2, 1.42), -40, 'mirrors %s' % mm(S['width_mirrors']), fs=11)
dim(a, '5-front', (0, 0.56, 0.05), (0, -0.56, 0.05), 85, 'track %s' % mm(S['track']), fs=11)
a = view('6-rear', 'REAR (3-height pin hitch, lamps)', [0.815, 0.845, 0.18, 0.12])
a = view('7-top', 'TOP (front to the right)', [0.635, 0.722, 0.36, 0.115])
dim(a, '7-top', (XR, -0.70, 0), (XF, -0.70, 0), -40, 'body %s' % mm(S['body_length']), fs=11)
dim(a, '7-top', (XF + 0.1, 0.70, 0), (XF + 0.1, -0.70, 0), -40, mm(S['width_body']), fs=11)
view('1-three-quarter-front', 'THREE-QUARTER FRONT', [0.01, 0.49, 0.49, 0.225])
view('2-three-quarter-rear', 'THREE-QUARTER REAR (battery lid, mesh guard)', [0.50, 0.49, 0.49, 0.225])
view('4-side-right', 'SIDE (right: fixed glass, controller vent)', [0.01, 0.335, 0.49, 0.15])
view('8-cab-interior', 'CAB INTERIOR (cab and door taken off)', [0.50, 0.335, 0.49, 0.15])
view('9-seat-and-controls', 'SEAT, WHEEL, PEDESTAL, PEDALS', [0.01, 0.20, 0.24, 0.13])
view('10-battery-bay', 'BATTERY BAY (lid off)', [0.255, 0.20, 0.24, 0.13])
view('11-rear-hitch', 'REAR PIN HITCH', [0.50, 0.20, 0.24, 0.13])
view('13-mirror-and-hinges', 'MIRROR ARM + DOOR HINGES', [0.745, 0.20, 0.24, 0.13])

dr = info['doors']['DoorDriver']; lid = info['battery_lid']; st = info['steering']; h = info['hitches']
n = fig.add_axes([0.01, 0.005, 0.98, 0.19], facecolor=BG); n.axis('off')
rows = [('OUTSIDE', ACC),
        ('Length %s overall (body %s) · width %s body, %s over mirrors · roof %s, beacon %s · wheelbase %s · track %s · front tyres %s x %s (6.00-9 class), rear %s x %s (7.00-12) - no modelled tread · floor %s · seat %s · deck %s · clearance %s' % (
            mm(S['length']), mm(S['body_length']), mm(S['width_body']), mm(S['width_mirrors']), mm(S['height_roof']), mm(S['height_beacon']), mm(S['wheelbase']), mm(S['track']),
            mm(S['front_tyre'][0]), mm(S['front_tyre'][1]), mm(S['rear_tyre'][0]), mm(S['rear_tyre'][1]), mm(S['floor']), mm(S['seat_cushion_top']), mm(S['deck']), mm(S['ground_clearance'])), INK),
        ('GSE-yellow pressed-steel body (like the pushback tug), white cab + roof (like the GPU), black rubber bumper band, yellow/black chevron panels at the rear · 80 V battery under the chequer-plate deck lid', INK),
        ('MOVING PARTS (Blender metres, origin on the ground under the wheelbase middle)', ACC),
        ('DoorDriver*: pivot (%.3f, %.3f, %.3f) about z, opens negative to -95 · BatteryLid*: pivot (%.3f, %.3f, %.3f) about y, lifts negative to -100 · front wheels steer +-%d about z through the kingpins (%.2f, +-%.2f) · SteeringWheel* about the column axis · HitchPinRear* / HitchPinFront*: pulled straight up (+z)' % (
            *dr['pivot'], *lid['pivot'], st['max_deg'], st['front_wheels']['FL']['pivot'][0], st['front_wheels']['FL']['pivot'][1]), INK),
        ('HITCHES: rear multi-height pin jaw, eye centres 280 / 400 / 490 (slots 60 high) - the GPU towing eye (400) sits in the middle slot · front pin jaw at 400 · each pin has a ring grip for the crew (IK_HitchPin_Rear / _Front)', INK),
        ('IK POINTS (%d): seat + back, both hands on the wheel, brake + accelerator, direction stalk, key, park brake, step, grab handles outside + inside, door handles outside + inside, door hinge, hitch pins rear + front, hitch eyes (rear low/main/high, front), battery lid handle + hinge, charge socket, e-stop, beacon, wheel centres, kingpins, stand points (door, rear hitch, front hitch, battery)' % len(info['ik']), IKC),
        ('LIGHTS + SOUNDS', ACC),
        ('Low + high beams, front indicators, side markers (4), tail/brake, rear indicators, reversing lamps, red reflectors - every one a housing set into flat panel, lens 2 mm in · amber roof beacon flashing like the jetway · cab lamp', INK),
        ('Sounds (Freesound CC0, Art/Sounds/BaggageTractor): electric motor whine loop (pitched with speed), start (contactor + ready chime), stop, reverse beeper, horn, door open + close, hitch pin in, pin out', INK)]
y = 0.98
for t, c in rows:
    n.text(0.0, y, t, color=c, fontsize=14 if c == ACC else 12.5, va='top', fontweight='bold' if c == ACC else 'normal', wrap=True); y -= 0.10 if c != ACC else 0.085
fig.savefig(D + 'BaggageTractor-Blueprint-v2.png', facecolor=BG)
print('ok')
