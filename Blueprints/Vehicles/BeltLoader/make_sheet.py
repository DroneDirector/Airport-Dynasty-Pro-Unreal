# The electric belt loader's blueprint sheet: the Blender wireframe views (Tools/make_belt_loader.py -- --wire) with
# dimension lines in whole millimetres drawn onto the orthographic views (metres -> pixels from the projections the build
# writes), moving parts, belt line, IK points, lamps and sounds. Reads Art/BeltLoader/beltloader-parts.json.
#   python -I Blueprints/Vehicles/BeltLoader/make_sheet.py
import json, os, math
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt, matplotlib.image as mpimg
D = os.path.dirname(os.path.abspath(__file__)) + '/'
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(D.rstrip('/'))))
info = json.load(open(os.path.join(ROOT, 'Art', 'BeltLoader', 'beltloader-parts.json')))
BG, INK, ACC, DIM, IKC = '#071426', '#dbe6f3', '#f4b740', '#8fd0ff', '#4cd18a'
mm = lambda v: '%d' % round(v * 1000)
S, V, BM = info['size_m'], info['wire_views'], info['boom']
PV = BM['pivot']; A320 = -BM['a320_bulk_hold_deg']; MAXD = -BM['range_deg'][1]
def raised(p, deg):
    a = math.radians(deg); x, z = p[0] - PV[0], p[2] - PV[2]
    return (PV[0] + x * math.cos(a) - z * math.sin(a), p[1], PV[2] + x * math.sin(a) + z * math.cos(a))
fig = plt.figure(figsize=(30, 40), dpi=100, facecolor=BG)
fig.text(0.02, 0.990, 'AIRPORT DYNASTY PRO · ELECTRIC BELT LOADER (self-propelled baggage conveyor, 80 V) · BLUEPRINT v1 · WIREFRAME', color=INK, fontsize=28, fontweight='bold', va='top')
fig.text(0.02, 0.977, 'Our own design (no brand) drawn to the TLD NBL-E / Mallaghan / Charlatte CBL class · every line is an edge of the real Blender mesh · sizes in whole millimetres · x forward (boom top end), y left, z up',
         color=DIM, fontsize=15, va='top')

def view(name, title, rect):
    a = fig.add_axes(rect); im = mpimg.imread(D + 'wire/' + name + '.png'); a.imshow(im); a.axis('off')
    a.set_title(title, color=ACC, fontsize=15, loc='left', fontweight='bold'); a.set_xlim(0, im.shape[1]); a.set_ylim(im.shape[0], 0)
    return a
def P(vn, p):
    v = V[vn]; O, X, Y, Z = v['O'], v['X'], v['Y'], v['Z']
    return (O[0] + p[0] * (X[0] - O[0]) + p[1] * (Y[0] - O[0]) + p[2] * (Z[0] - O[0]), O[1] + p[0] * (X[1] - O[1]) + p[1] * (Y[1] - O[1]) + p[2] * (Z[1] - O[1]))
def dim(a, vn, p0, p1, off, label=None, col=DIM, fs=12):
    (x0, y0), (x1, y1) = P(vn, p0), P(vn, p1)
    dx, dy = x1 - x0, y1 - y0; L = (dx * dx + dy * dy) ** 0.5 or 1; nx, ny = -dy / L * off, dx / L * off
    a.plot([x0, x0 + nx], [y0, y0 + ny], color=col, lw=0.8, alpha=0.7); a.plot([x1, x1 + nx], [y1, y1 + ny], color=col, lw=0.8, alpha=0.7)
    a.annotate('', xy=(x1 + nx, y1 + ny), xytext=(x0 + nx, y0 + ny), arrowprops=dict(arrowstyle='<->', color=col, lw=1.2))
    d = sum((p1[i] - p0[i]) ** 2 for i in range(3)) ** 0.5
    a.text((x0 + x1) / 2 + nx * 1.25, (y0 + y1) / 2 + ny * 1.25, label or mm(d), color=col, fontsize=fs, ha='center', va='center', fontweight='bold',
           bbox=dict(facecolor=BG, edgecolor='none', alpha=0.75, pad=1.5))
def mark(a, vn, p, label, col=IKC, side=1, fs=11):
    px, py = P(vn, p); a.plot([px], [py], 'o', color=col, ms=6)
    a.text(px + side * 14, py, label, color=col, fontsize=fs, va='center', ha='left' if side > 0 else 'right', bbox=dict(facecolor=BG, edgecolor='none', alpha=0.7, pad=1))

FX, RX, XF, XR = S['front_axle_x'], S['rear_axle_x'], 2.40, -3.30
IK = info['ik']
# ---- the big dimensioned side view (rest)
vn = '3-side-left'; a = view(vn, 'SIDE (left: half-cab + door, step) - boom at rest - dimensions in mm', [0.01, 0.735, 0.62, 0.23])
dim(a, vn, (S['x_max'], 0, 0), (S['x_min'], 0, 0), 110, 'overall %s' % mm(S['length']))
dim(a, vn, (XF, 0, 0), (XR, 0, 0), 70, 'chassis %s' % mm(S['chassis_length']))
dim(a, vn, (FX, 0, 0.0), (RX, 0, 0.0), 30, 'wheelbase %s' % mm(S['wheelbase']))
dim(a, vn, (3.80, 0, 2.15), (-3.80, 0, 2.15), -10, 'boom %s (belt drums %s apart, belt %s wide)' % (mm(S['boom_length']), mm(S['belt_length_centres']), mm(S['belt_width'])))
dim(a, vn, (-1.05, 0, 0), (-1.05, 0, S['height_roof']), -40, 'roof %s' % mm(S['height_roof']))
dim(a, vn, (-0.62, 0, 0), (-0.62, 0, S['height_beacon']), 40, 'beacon %s' % mm(S['height_beacon']), fs=11)
dim(a, vn, (-3.65, 0, 0), (-3.65, 0, S['belt_top_rest']), 40, 'belt %s' % mm(S['belt_top_rest']))
dim(a, vn, (FX, 0, 0), (FX, 0, S['tyre'][0]), -10, 'tyre %s' % mm(S['tyre'][0]), fs=10)
mark(a, vn, IK['IK_Boom_Hinge'], 'boom hinge (pivot)', ACC, side=-1)
mark(a, vn, IK['IK_Ram_Front_Base'], 'front ram base', ACC, side=1); mark(a, vn, IK['IK_Ram_Rear_Base'], 'rear ram base', ACC, side=1)
mark(a, vn, IK['IK_Belt_Bottom'], 'IK_Belt_Bottom', side=1); mark(a, vn, IK['IK_Belt_Top'], 'IK_Belt_Top', side=-1)
mark(a, vn, IK['IK_Controls_Bottom'], 'controls (bottom)', side=1); mark(a, vn, IK['IK_Controls_Top'], 'controls (top)', side=-1)
mark(a, vn, IK['IK_Bumper_Contact'], 'bumper contact', side=1)
mark(a, vn, IK['IK_Seat_Driver'], 'seat %s' % mm(S['seat_cushion_top']), side=1); mark(a, vn, IK['IK_Step'], 'step', side=1)
# ---- front / rear / top
vn = '5-front'; a = view(vn, 'FRONT (boom top end, bumper pad)', [0.635, 0.845, 0.17, 0.12])
dim(a, vn, (0, 1.08, 0.0), (0, -1.08, 0.0), 40, 'body %s' % mm(S['width_body']), fs=11)
dim(a, vn, (0, S['width_mirrors'] / 2, 1.55), (0, -S['width_mirrors'] / 2, 1.55), -40, 'mirrors %s' % mm(S['width_mirrors']), fs=11)
dim(a, vn, (0, 0.80, 0.05), (0, -0.80, 0.05), 85, 'track %s' % mm(S['track']), fs=11)
view('6-rear', 'REAR (boom bottom end, lamps, e-stop)', [0.815, 0.845, 0.17, 0.12])
vn = '7-top'; a = view(vn, 'TOP (front to the right)', [0.635, 0.735, 0.355, 0.105])
dim(a, vn, (XR, -1.08, 0), (XF, -1.08, 0), -30, 'chassis %s' % mm(S['chassis_length']), fs=11)
# ---- the raised view at A320 bulk hold height
vn = '15-side-raised-A320'; a = view(vn, 'RAISED to an A320 bulk hold sill (boom -%.2f deg)' % A320, [0.01, 0.525, 0.49, 0.20])
top = raised(BM['sill_point_rest'], A320)
dim(a, vn, (top[0] + 0.25, 0, 0), (top[0] + 0.25, 0, top[2]), -40, 'sill %s' % mm(top[2]))
mark(a, vn, raised(IK['IK_Stand_Top'], A320), 'IK_Stand_Top (in the hold door)', side=-1)
vn = '16-side-raised-max'; a = view(vn, 'FULLY RAISED (boom -%.1f deg)' % MAXD, [0.50, 0.525, 0.49, 0.20])
top = raised(BM['sill_point_rest'], MAXD)
dim(a, vn, (top[0] + 0.25, 0, 0), (top[0] + 0.25, 0, top[2]), -40, 'max sill %s' % mm(top[2]))
view('1-three-quarter-front', 'THREE-QUARTER FRONT', [0.01, 0.335, 0.32, 0.18])
view('2-three-quarter-rear', 'THREE-QUARTER REAR', [0.335, 0.335, 0.32, 0.18])
view('13-rams-and-hinge', 'RAMS + BOOM HINGE (chassis taken off)', [0.66, 0.335, 0.33, 0.18])
view('8-cab-interior', 'HALF-CAB INTERIOR (shell + door off)', [0.01, 0.20, 0.24, 0.13])
view('9-seat-and-controls', 'SEAT, WHEEL, BOOM JOYSTICK', [0.255, 0.20, 0.24, 0.13])
view('10-top-end', 'TOP END: bumper pad, controls, work lamp', [0.50, 0.20, 0.24, 0.13])
view('11-bottom-end', 'BOTTOM END: loading end, controls', [0.745, 0.20, 0.24, 0.13])

rf, rr = info['rams']['RamFront'], info['rams']['RamRear']; dr = info['doors']['DoorDriver']; st = info['steering']; bu = info['bumper']; be = info['belt']
n = fig.add_axes([0.01, 0.005, 0.98, 0.19], facecolor=BG); n.axis('off')
rows = [('OUTSIDE', ACC),
        ('Length %s overall (chassis %s, boom %s) · width %s body, %s over mirrors · roof %s, beacon %s · wheelbase %s · track %s · tyres %s x %s (6.50-10 class, no modelled tread) · cab floor %s · seat %s · deck %s · clearance %s' % (
            mm(S['length']), mm(S['chassis_length']), mm(S['boom_length']), mm(S['width_body']), mm(S['width_mirrors']), mm(S['height_roof']), mm(S['height_beacon']), mm(S['wheelbase']), mm(S['track']),
            mm(S['tyre'][0]), mm(S['tyre'][1]), mm(S['floor']), mm(S['seat_cushion_top']), mm(S['deck']), mm(S['ground_clearance'])), INK),
        ('Belt %s wide, top run %s above ground at rest; sill range %s - %s (A320 bulk 2100 at -%.2f deg) · GSE-yellow chassis + boom, white half-cab, black rubber, chequer-plate decks · 80 V battery under the right deck hatch' % (
            mm(S['belt_width']), mm(S['belt_top_rest']), mm(S['sill_min']), mm(S['sill_max']), A320), INK),
        ('MOVING PARTS (Blender metres, origin on the ground under the middle)', ACC),
        ('Boom* + BeltSurface + BumperPad* + WorkLamp*: pivot (%.2f, %.2f, %.2f) about y, raises NEGATIVE 0..-%.1f · RamFront* base (%.2f, 0, %.2f) top (%.2f, 0, %.2f) · RamRear* base (%.2f, 0, %.2f) top (%.2f, 0, %.2f): barrel turns about its base, rod follows the boom eye' % (
            *PV, MAXD, rf['base_pivot'][0], rf['base_pivot'][2], rf['top_pivot_boom_local'][0], rf['top_pivot_boom_local'][2], rr['base_pivot'][0], rr['base_pivot'][2], rr['top_pivot_boom_local'][0], rr['top_pivot_boom_local'][2]), INK),
        ('BumperPad*: pivot (%.2f, 0, %.3f) boom-local about y, %s deg, keeps upright (turn + boom angle) · DoorDriver*: pivot (%.3f, %.3f, %.3f) about z, opens negative to -95 · front wheels steer +-%d about z (kingpins x %.2f, y +-%.2f) · BeltSurface: bags pinned along IK_Belt_Bottom -> IK_Belt_Top, UV scrolls' % (
            bu['pivot_boom_local'][0], bu['pivot_boom_local'][2], bu['range_deg'], *dr['pivot'], st['max_deg'], st['front_wheels']['FL']['pivot'][0], st['front_wheels']['FL']['pivot'][1]), INK),
        ('IK POINTS (%d): seat + back, both hands on the wheel, brake + accelerator, direction stalk, key, boom joystick, step, grab handles outside + inside, door handles + hinge, belt bottom + top, controls bottom + top, stand load (ground, bottom end), stand top (in the hold door), bumper contact + pivot, handrails, ram eyes, boom hinge, charge socket, e-stop, beacon, work lamp, wheel centres, kingpins' % len(IK), IKC),
        ('LIGHTS + SOUNDS', ACC),
        ('Low + high beams, front indicators, side markers (6: front / middle / rear), tail/brake, rear indicators, reversing lamps, red reflectors - every one a housing set into flat panel · amber roof beacon (jetway flash) · work lamp at the boom top · cab lamp', INK),
        ('Sounds (Freesound CC0, Art/Sounds/BeltLoader): electric drive motor loop (pitched with speed), belt rollers loop, hydraulic boom up / down, reverse beeper, horn, door open + close', INK)]
y = 0.98
for t, c in rows:
    n.text(0.0, y, t, color=c, fontsize=14 if c == ACC else 12.5, va='top', fontweight='bold' if c == ACC else 'normal', wrap=True); y -= 0.10 if c != ACC else 0.085
fig.savefig(D + 'BeltLoader-Blueprint-v1.png', facecolor=BG)
print('ok')
