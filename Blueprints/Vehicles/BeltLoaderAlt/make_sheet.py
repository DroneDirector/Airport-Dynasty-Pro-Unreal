# The belt loader's blueprint sheets (4K PNG, large text) from the wireframe views of the real Blender mesh (wire/*.png,
# made by Tools/make_belt_loader_alt.py --wire) and the lift rig (Art/BeltLoaderAlt/beltloader-rig.json):
#   1 side elevation set to the A320's forward hold, with the main dimensions
#   2 the views: three-quarter front and rear, front, rear, top
#   3 close-ups: operator station, lift arms and cylinders, boom front and controls, rear loading end, underside
#   4 adjusting to the aircraft: the lift at its lowest, A320 and highest settings, the presets, the specs
#   python3 Blueprints/Vehicles/BeltLoaderAlt/make_sheet.py
import json, math, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
WIRE = os.path.join(HERE, 'wire')
RIG = json.load(open(os.path.join(ROOT, 'Art', 'BeltLoaderAlt', 'beltloader-rig.json')))
BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'; REG = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
F = lambda p, s: ImageFont.truetype(p, s)
BG = (8, 20, 38); INK = (232, 240, 255); SUB = (150, 175, 210); ACC = (255, 196, 70); DIMC = (150, 205, 255); GRN = (110, 230, 140)
W, H = 3840, 2160


# #region ── Helpers ──
def canvas(title, sub):
    im = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(im)
    d.text((50, 30), title, font=F(BOLD, 70), fill=INK); d.text((52, 122), sub, font=F(REG, 40), fill=SUB)
    return im, d


def tbox(d, xy, t, f, fill=INK, anchor='mm', bg=BG):
    b = d.textbbox(xy, t, font=f, anchor=anchor); d.rectangle((b[0] - 10, b[1] - 6, b[2] + 10, b[3] + 6), fill=bg)
    d.text(xy, t, font=f, fill=fill, anchor=anchor)


def dim_h(d, x0, x1, y, t, col=DIMC, size=44):
    d.line((x0, y, x1, y), fill=col, width=4)
    for x, s in ((x0, 1), (x1, -1)):
        d.polygon([(x, y), (x + s * 24, y - 10), (x + s * 24, y + 10)], fill=col); d.line((x, y - 22, x, y + 22), fill=col, width=3)
    tbox(d, ((x0 + x1) / 2, y), t, F(BOLD, size), col)


def dim_v(d, x, y0, y1, t, col=DIMC, size=42, side='r'):
    d.line((x, y0, x, y1), fill=col, width=4)
    for y, s in ((y0, 1), (y1, -1)):
        d.polygon([(x, y), (x - 10, y + s * 24), (x + 10, y + s * 24)], fill=col); d.line((x - 22, y, x + 22, y), fill=col, width=3)
    tbox(d, (x + (18 if side == 'r' else -18), (y0 + y1) / 2), t, F(BOLD, size), col, 'lm' if side == 'r' else 'rm')


def paste(im, d, name, box, label):
    x, y, w, h = box
    r = Image.open(os.path.join(WIRE, name + '.png')).convert('RGB')
    k = min(w / r.width, h / r.height); r = r.resize((int(r.width * k), int(r.height * k)), Image.LANCZOS)
    im.paste(r, (x, y + 58)); d.text((x, y), label, font=F(BOLD, 44), fill=ACC)
    return k, (x, y + 58)


def mm(v): return f'{round(v * 1000):,}'
# #endregion

# #region ── Pose numbers (the same sums as the model) ──
BL, BD, S, AR, AF = RIG['boom_length'], RIG['belt_to_chord'], RIG['pin_from_rear'], RIG['rear_arm'], RIG['front_arm']
PR, PF = RIG['rear_arm_pivot'], RIG['front_arm_pivot']


def solve(f, r):
    th = math.asin((f - r) / BL); u = (math.cos(th), math.sin(th)); n = (-math.sin(th), math.cos(th))
    bz = r + S * u[1] - BD * n[1]; dz = bz - PR[2]; bx = PR[0] + math.sqrt(AR ** 2 - dz ** 2)
    Rx, Rz = bx - S * u[0] + BD * n[0], bz - S * u[1] + BD * n[1]
    dx, dzz = bx - PF[0], bz - PF[2]; b = 2 * (dx * u[0] + dzz * u[1]); c = dx * dx + dzz * dzz - AF ** 2
    t = (-b + math.sqrt(b * b - 4 * c)) / 2
    return dict(th=th, R=(Rx, Rz), F=(Rx + BL * u[0], Rz + BL * u[1]), B=(bx, bz), T=(bx + t * u[0], bz + t * u[1]), u=u, n=n)


P = solve(RIG['pose_saved']['front'], RIG['pose_saved']['rear'])
# #endregion

# #region ── Sheet 1: side elevation with dimensions ──
im, d = canvas('BAGGAGE BELT LOADER  ·  SIDE ELEVATION, SET TO THE A320 FORWARD HOLD',
               'Electric, self-propelled  ·  our own design (TLD NBL-E class)  ·  every line is an edge of the real Blender mesh  ·  sizes in millimetres')
r = Image.open(os.path.join(WIRE, '3-side-left.png')).convert('RGB')
k = 1.30; r = r.resize((int(r.width * k), int(r.height * k)), Image.LANCZOS)
OX, OY = 330, 175; im.paste(r, (OX, OY))
ppm = 2400 / 9.6 * k; CX, CZ = 1.3, 1.6
px = lambda x: OX + r.width / 2 - (x - CX) * ppm          # (seen from the left: the front is on the left)
pz = lambda z: OY + r.height / 2 - (z - CZ) * ppm
g = pz(0)
d.line((px(6.0), g, px(-3.4), g), fill=SUB, width=3)
# the A320 forward hold sill
zs = RIG['pose_saved']['front']
for x in range(int(px(6.2)), int(px(3.6)), 30): d.line((x, pz(zs), x + 16, pz(zs)), fill=GRN, width=4)
tbox(d, (OX + 30, pz(zs) - 120), 'A320 forward\nhold sill\n~2,110-2,180', F(BOLD, 38), GRN, 'lm')
dim_v(d, px(P['F'][0]) - 30, pz(P['F'][1]), g, mm(P['F'][1]) + ' front', size=40, side='l')
dim_v(d, px(P['R'][0]) + 30, pz(P['R'][1]), g, mm(P['R'][1]) + ' rear', size=40)
dim_h(d, px(1.5), px(-1.5), g + 70, 'wheelbase 3,000')
dim_h(d, px(2.35), px(-2.35), g + 160, 'chassis 4,700')
xf, xr = P['F'][0] + 0.17, P['R'][0] - 0.08
dim_h(d, px(xf), px(xr), g + 250, f'overall {mm(xf - xr)} (boom 7,800 along the belt)')
dim_v(d, px(-3.35), pz(2.07), g, '2,070 roof guard', size=38)
ang = math.degrees(P['th']); tbox(d, (px(1.6), pz(1.85)), f'boom {ang:.1f}°', F(BOLD, 44), ACC)
tbox(d, (px(1.5) - 40, g + 70), 'tyres 225/70 R15 (696)', F(BOLD, 36), SUB, 'rm')
notes = ['Front belt height 840-4,190, rear 850-1,300 (two lift arms, each with its own cylinder)',
         'Belt 600 wide, boom 700 (780 with the left guide), 1,200 kg on the belt, 18-30 m/min',
         'Driver on the left under a roof guard; the boom on the right; handrails 1,000 above the belt at the front']
for i, t in enumerate(notes): d.text((60, H - 170 + i * 52), t, font=F(REG, 40), fill=INK)
im.save(os.path.join(HERE, 'BeltLoader-Blueprint-1-Side.png'))
# #endregion

# #region ── Sheet 2: the views ──
im, d = canvas('BAGGAGE BELT LOADER  ·  THE VIEWS', 'Three-quarter front and rear, front, rear and top  ·  set to the A320 forward hold  ·  millimetres')
paste(im, d, '1-three-quarter-front', (40, 170, 1860, 1163), 'THREE-QUARTER FRONT (left side, the driver)')
paste(im, d, '2-three-quarter-rear', (1940, 170, 1860, 1163), 'THREE-QUARTER REAR (right side, the lift)')
k, o = paste(im, d, '5-front', (40, 1370, 700, 700), 'FRONT')
pp = 1400 / 4.0 * k
cxp = o[0] + 700 * k / 1400 * 700
dim_h(d, o[0] + 350 - 1.05 * pp, o[0] + 350 + 1.05 * pp, o[1] + 700 - (0.25 - 0.0) * pp - (1.75 - 2.0) * pp - 0.5 * pp, '2,100', size=38)
k2, o2 = paste(im, d, '6-rear', (780, 1370, 700, 700), 'REAR')
k3, o3 = paste(im, d, '7-top', (1520, 1370, 2280, 700), 'TOP (front to the right)')
im.save(os.path.join(HERE, 'BeltLoader-Blueprint-2-Views.png'))
# #endregion

# #region ── Sheet 3: close-ups ──
im, d = canvas('BAGGAGE BELT LOADER  ·  CLOSE-UPS', 'The operator station, the lift, the boom front, the loading end and the underside  ·  nothing floats: every part sits on, hangs on or is bolted to the part under it')
cells = [('8-operator-station', 'OPERATOR STATION (roof guard off)'), ('9-lift-arms-and-cylinders', 'LIFT ARMS AND CYLINDERS'),
         ('10-boom-front-and-controls', 'BOOM FRONT: BUMPER, RAILS, CONTROLS'), ('11-rear-loading-end-and-hitch', 'LOADING END, FLARES, HITCH'),
         ('12-underside', 'UNDERSIDE')]
for i, (n, t) in enumerate(cells):
    c, rr = i % 3, i // 3
    paste(im, d, n, (40 + c * 1265, 175 + rr * 1000, 1230, 769), t)
tx, ty = 40 + 2 * 1265, 175 + 1000
for i, t in enumerate(['WHAT THE CLOSE-UPS SHOW', '· suspension seat, joystick + belt', '  switches, dash display, 2 pedals,',
                       '  park brake, protection grid', '· rear arm pinned to the boom,', '  front arm rolling in its track,',
                       '  2 cylinders with chrome rods', '· rubber nose bumper, anti-pinch', '  edge, work light, strobe,',
                       '  front control box, E-stop', '· rear flares, drive drum + motor,', '  pin hitch, battery, drive axle']):
    d.text((tx, ty + i * 62), t, font=F(BOLD if i == 0 else REG, 44 if i == 0 else 40), fill=ACC if i == 0 else INK)
im.save(os.path.join(HERE, 'BeltLoader-Blueprint-3-Closeups.png'))
# #endregion

# #region ── Sheet 4: adjusting to the aircraft ──
im, d = canvas('BAGGAGE BELT LOADER  ·  ADJUSTING TO THE AIRCRAFT', 'The front of the belt goes to the door sill, the rear stays low at the carts  ·  '
               'the game sets it from each aircraft\'s door sill  ·  millimetres')
ppm = 215; OX, GZ = 820, 1430
sx = lambda x: OX + x * ppm; sz = lambda z: GZ - z * ppm
d.line((sx(-3.6), GZ, sx(6.0), GZ), fill=SUB, width=4)
for x in (1.5, -1.5): d.ellipse((sx(x) - 0.348 * ppm, sz(0.696), sx(x) + 0.348 * ppm, GZ), outline=SUB, width=4)
d.rectangle((sx(-2.35), sz(0.78), sx(2.35), sz(0.42)), outline=SUB, width=4)
d.rectangle((sx(-1.05), sz(2.05), sx(0.30), sz(0.78)), outline=(90, 110, 140), width=3)
for (nm, (f, r)), col in zip([('lowest (regional jets)', RIG['presets']['lowest (regional jets)']),
                              ('A320 forward hold', RIG['presets']['A320 forward hold']),
                              ('highest (wide-body bulk doors)', RIG['presets']['highest (wide-body bulk doors)'])],
                             ((150, 200, 255), GRN, ACC)):
    q = solve(f, r)
    d.line((sx(q['R'][0]), sz(q['R'][1]), sx(q['F'][0]), sz(q['F'][1])), fill=col, width=12)
    d.line((sx(PR[0]), sz(PR[2]), sx(q['B'][0]), sz(q['B'][1])), fill=col, width=7)
    d.line((sx(PF[0]), sz(PF[2]), sx(q['T'][0]), sz(q['T'][1])), fill=col, width=7)
    for p_ in (q['B'], q['T']): d.ellipse((sx(p_[0]) - 12, sz(p_[1]) - 12, sx(p_[0]) + 12, sz(p_[1]) + 12), fill=col)
    tbox(d, (sx(q['F'][0]) + 30, sz(q['F'][1])), {'lowest (regional jets)': '840 lowest', 'A320 forward hold': '2,100 A320 forward hold'}.get(nm, '4,190 highest'), F(BOLD, 40), col, 'lm')
for p_ in (PR, PF): d.ellipse((sx(p_[0]) - 14, sz(p_[2]) - 14, sx(p_[0]) + 14, sz(p_[2]) + 14), fill=INK)
tbox(d, (sx(PR[0]), sz(PR[2]) + 50), 'rear arm pivot', F(REG, 34), INK); tbox(d, (sx(PF[0]) + 40, sz(PF[2]) + 50), 'front arm pivot', F(REG, 34), INK)
# the presets table and the specs
ty = 1560
d.text((60, ty), 'PRESETS  (front / rear belt-top height, boom angle)', font=F(BOLD, 44), fill=ACC)
for i, (nm, (f, r)) in enumerate(RIG['presets'].items()):
    q = solve(f, r)
    d.text((60, ty + 64 + i * 56), f'{nm:32s}', font=F(REG, 40), fill=INK)
    d.text((900, ty + 64 + i * 56), f'{mm(f)} / {mm(r)}   {math.degrees(q["th"]):.1f}°', font=F(BOLD, 40), fill=INK)
spec = [('HOW THE GAME SETS IT', ACC, True), ('front = the door\'s sill height from the aircraft data,', INK, False),
        ('rear = the carts\' height (950); the lift solves both', INK, False), ('(beltloader-rig.json); the arms turn on their pivots,', INK, False),
        ('the boom turns on its rear pin, the rods slide', INK, False), ('', INK, False),
        ('IK POINTS (30): seat + back, hands on the wheel,', GRN, True), ('pedals, joystick, park brake, step, grab handle,', GRN, False),
        ('the front control box, standing on the belt;', GRN, False), ('pivots: arms, boom pin, roller, cylinders, drums', GRN, False), ('', INK, False),
        ('LIGHTS: head, indicators, tail/brake, reversing,', ACC, True), ('side markers, work light under the nose, amber', INK, False),
        ('beacon + strobe;  SOUNDS (Sonniss / CC0): drive,', INK, False), ('hydraulic pump, belt running, reverse beeper, horn', INK, False)]
for i, (t, col, b) in enumerate(spec):
    d.text((2480, 210 + i * 58), t, font=F(BOLD if b else REG, 40), fill=col)
im.save(os.path.join(HERE, 'BeltLoader-Blueprint-4-Adjusting.png'))
# #endregion
print('sheets done')
