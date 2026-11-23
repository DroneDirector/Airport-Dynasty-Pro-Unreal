# The blueprint sheets of the apron bus and the taxi bus (4K PNG, large text), made from the wireframe views of the real
# Blender meshes (Blueprints/Vehicles/<ApronBus|TaxiBus>/wire/*.png, made by Tools/make_apron_bus.py / make_taxi_bus.py
# --wire):  1 side elevation with the main dimensions,  2 the views,  3 the interior,  4 close-ups and the specification.
#   python3 Tools/make_bus_sheets.py [apron] [taxi]
import os, sys, json
from PIL import Image, ImageDraw, ImageFont
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'; REG = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
F = lambda p, s: ImageFont.truetype(p, s)
BG = (8, 20, 38); INK = (232, 240, 255); SUB = (150, 175, 210); ACC = (255, 196, 70); DIMC = (150, 205, 255); GRN = (110, 230, 140)
W, H = 3840, 2160

# #region ── Helpers ──
def canvas(title, sub):
    im = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(im)
    d.text((50, 30), title, font=F(BOLD, 70), fill=INK); d.text((52, 122), sub, font=F(REG, 40), fill=SUB)
    return im, d
def tbox(d, xy, t, f, fill=INK, anchor='mm'):
    b = d.textbbox(xy, t, font=f, anchor=anchor); d.rectangle((b[0] - 10, b[1] - 6, b[2] + 10, b[3] + 6), fill=BG)
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
def paste(im, d, wire, name, box, label):
    x, y, w, h = box
    r = Image.open(os.path.join(wire, name + '.png')).convert('RGB')
    k = min(w / r.width, h / r.height); r = r.resize((int(r.width * k), int(r.height * k)), Image.LANCZOS)
    im.paste(r, (x, y + 58)); d.text((x, y), label, font=F(BOLD, 44), fill=ACC)
    return k, (x, y + 58), r.size
def lines(d, x, y, rows, step=58):
    for i, (t, col, b) in enumerate(rows):
        d.text((x, y + i * step), t, font=F(BOLD if b else REG, 42 if b else 38), fill=col)
# #endregion

# #region ── The two buses: sizes for the dimension lines, view names, texts ──
CFG = {
 'apron': dict(
    dir='ApronBus', file='ApronBus', name='APRON BUS',
    sub='Electric low-floor airside bus  ·  our own design (e.COBUS 3000 class)  ·  every line is an edge of the real Blender mesh  ·  millimetres',
    side_res=(2400, 900), ortho=15.6, cz=1.55, k=1.45, OX=180, OY=200,
    L2=6.96, FX=3.76, RX=-3.34, Htot=3.10, body=2.86, floor=0.33,
    extra=[('door', 1.95, 0.95, 1.75, '1,900 clear')],
    notes=['3 + 3 double doors (1,900 clear, 2,150 high) on both sides, plus a front right door; the driver front left behind a glazed partition',
           'Floor 330 above the apron, kneels 80; 2,400 headroom; 9 seats on the wheel housings, about 100 standing, a wheelchair / pram space',
           'Batteries and the A/C on the roof (3,100 over them); 275/70 R22.5 tyres, twin at the rear; up to 110 passengers, 20.4 t gross'],
    views=[('1-three-quarter-front', 'THREE-QUARTER FRONT (right side, the kerb doors)'), ('2-three-quarter-rear', 'THREE-QUARTER REAR (left side)'),
           ('5-front', 'FRONT'), ('6-rear', 'REAR'), ('7-top', 'TOP (front to the right)')],
    top_in=('8-inside-from-above', 'THE INSIDE FROM ABOVE (roof, batteries and lining off; front to the right)'),
    inside=[('9-cutaway', 'CUTAWAY'), ('10-drivers-cab', "DRIVER'S CAB"), ('11-standing-area-and-seats', 'STANDING AREA, POLES, SEATS')],
    close=[('12-door-closeup', 'PASSENGER DOOR: GLAZED LEAVES, RUBBER EDGES'), ('13-wheels-and-arch', 'REAR TWIN WHEELS AND ARCH')],
    spec1=[('INSIDE', ACC, True), ('· 28 grab poles (stainless / yellow), stop buttons', INK, False), ('· 2 ceiling rails with 38 hanging straps', INK, False),
           ('· 9 seats (3 on each rear and front-right housing)', INK, False), ('· 2 luggage racks by the middle doors', INK, False),
           ('· wheelchair backrest at the front right door', INK, False), ('· 2 double-sided flight information screens', INK, False),
           ('· LED light strips, A/C ducts, roof hatches', INK, False), ('· cab: partition + door, dash, door switches,', INK, False),
           ('  wheel, pedals, suspension seat, extinguisher', INK, False)],
    spec2=[('IK POINTS + PIVOTS (112)', GRN, True), ('driver: seat, back, hands, pedals, door controls;', INK, False),
           ('every pole hold, strap loop, seat, rack, 4 standing', INK, False), ('spots; pivots: 14 door leaves, wipers, cab door,', INK, False),
           ('front steering', INK, False), ('', INK, False), ('LIGHTS + SOUNDS', ACC, True), ('head, DRL, indicators, tail/brake, reverse, markers,', INK, False),
           ('door warning lamps, step lights, LED strips,', INK, False), ('destination display, beacon;  drive whine, air', INK, False),
           ('brakes + kneeling, door chime, A/C, reverse beeper', INK, False)]),
 'taxi': dict(
    dir='TaxiBus', file='TaxiBus', name='TAXI BUS',
    sub='Minibus taxi for 8 passengers + driver, waits at the landside kerb  ·  our own design (long high-roof van class)  ·  every line is an edge of the real mesh  ·  millimetres',
    side_res=(2400, 1000), ortho=6.6, cz=1.25, k=1.40, OX=240, OY=190,
    L2=2.965, FX=1.965, RX=-1.70, Htot=2.59, body=2.40, floor=0.58,
    extra=[('slide', (-0.38 + 0.82) / 2, 0.60, 1.00, '1,200')],
    notes=['Sliding door on the kerb (right) side with an entry step at 360; two hinged front doors; two rear doors with a folding wheelchair ramp',
           'Seats: driver + front passenger, 2 + 2 in rows 2 and 3 with the aisle on the kerb side, a 3-place rear bench; luggage area behind it',
           'Cabin floor 580; 235/65 R16C tyres; the lit TAXI sign on the roof (2,590 over it); taximeter and card terminal on the dashboard'],
    views=[('1-three-quarter-front', 'THREE-QUARTER FRONT (right side, the sliding door)'), ('2-three-quarter-rear', 'THREE-QUARTER REAR (left side)'),
           ('5-front', 'FRONT'), ('6-rear', 'REAR'), ('7-top', 'TOP (front to the right)')],
    top_in=('8-interior-from-above', 'THE INTERIOR FROM ABOVE (roof and body shell off; front to the right)'),
    inside=[('9-cutaway', 'CUTAWAY'), ('10-drivers-place', "DRIVER'S PLACE AND TAXIMETER"), ('11-passenger-seats', 'PASSENGER SEATS, BELTS, HEADRESTS')],
    close=[('12-sliding-door-and-step', 'SLIDING DOOR, TRACKS, ENTRY STEP'), ('13-rear-doors-and-ramp', 'REAR DOORS AND WHEELCHAIR RAMP')],
    spec1=[('INSIDE', ACC, True), ('· 9 seats, each with bolsters, headrest on posts,', INK, False), ('  three-point belt and buckle', INK, False),
           ('· dashboard: cluster, centre screen, 4 vents,', INK, False), ('  taximeter (lit), card terminal on its arm,', INK, False),
           ('  gear selector, park brake, wheel, pedals', INK, False), ('· headliner, ceiling + dome lights, air ducts', INK, False),
           ('  with vents, roof grab handles, entry grab pole', INK, False), ('· floor mat, wheelchair tracks, tie-down rings,', INK, False),
           ('  folding ramp against the rear doors', INK, False)],
    spec2=[('IK POINTS + PIVOTS (70)', GRN, True), ('every seat: seat, back, head, buckle; driver hands,', INK, False),
           ('pedals, taximeter, gear, card terminal; grab handles,', INK, False), ('door handles, entry step, luggage + wheelchair spots;', INK, False),
           ('pivots: front, sliding and rear doors, wipers, ramp, steering', INK, False), ('', INK, False), ('LIGHTS + SOUNDS', ACC, True),
           ('head, fog, indicators + repeaters, tail/brake, reverse,', INK, False), ('third brake, markers, TAXI sign, meter, cabin lights;', INK, False),
           ('drive, sliding door run + latch, doors, indicator,', INK, False), ('meter + card beeps, reverse beeper, horn', INK, False)]),
}
def mm(v): return f'{round(v * 1000):,}'
# #endregion

# #region ── The four sheets ──
def make(key):
    c = CFG[key]; HERE = os.path.join(ROOT, 'Blueprints', 'Vehicles', c['dir']); WIRE = os.path.join(HERE, 'wire')
    # 1 side elevation (the right side seen from outside: the front is to the right)
    im, d = canvas(f'{c["name"]}  ·  SIDE ELEVATION (RIGHT SIDE)', c['sub'])
    r = Image.open(os.path.join(WIRE, '3-side-right.png')).convert('RGB'); k = c['k']
    r = r.resize((int(r.width * k), int(r.height * k)), Image.LANCZOS); OX, OY = c['OX'], c['OY']; im.paste(r, (OX, OY))
    ppm = c['side_res'][0] / c['ortho'] * k
    px = lambda x: OX + r.width / 2 + x * ppm; pz = lambda z: OY + r.height / 2 - (z - c['cz']) * ppm
    g = pz(0); L2 = c['L2']
    d.line((px(-L2 - 0.4), g, px(L2 + 0.4), g), fill=SUB, width=3)
    dim_h(d, px(c['RX']), px(c['FX']), g + 70, f'wheelbase {mm(c["FX"] - c["RX"])}')
    dim_h(d, px(-L2), px(c['RX']), g + 160, f'{mm(c["RX"] + L2)}', size=40); dim_h(d, px(c['FX']), px(L2), g + 160, f'{mm(L2 - c["FX"])}', size=40)
    dim_h(d, px(-L2), px(L2), g + 250, f'overall length {mm(2 * L2)}')
    dim_v(d, px(L2) + 70, pz(c['Htot']), g, mm(c['Htot']))
    dim_v(d, px(L2) + 260, pz(c['body']), g, mm(c['body']) + ' body', size=38) if False else None
    dim_v(d, px(-L2) - 60, pz(c['floor']), g, f'floor {mm(c["floor"])}', size=40, side='l')
    for _, x, half, zz, t in c['extra']:
        dim_h(d, px(x - half), px(x + half), pz(zz), t, col=ACC, size=36)
    for i, t in enumerate(c['notes']): d.text((60, H - 200 + i * 56), t, font=F(REG, 38), fill=INK)
    im.save(os.path.join(HERE, f'{c["file"]}-Blueprint-1-Side.png'))
    # 2 the views
    im, d = canvas(f'{c["name"]}  ·  THE VIEWS', 'Three-quarter front and rear, front, rear and top  ·  every line is an edge of the real mesh')
    v = c['views']
    paste(im, d, WIRE, v[0][0], (40, 170, 1860, 1163), v[0][1]); paste(im, d, WIRE, v[1][0], (1940, 170, 1860, 1163), v[1][1])
    paste(im, d, WIRE, v[2][0], (40, 1380, 700, 700), v[2][1]); paste(im, d, WIRE, v[3][0], (780, 1380, 700, 700), v[3][1])
    paste(im, d, WIRE, v[4][0], (1540, 1380, 2260, 700), v[4][1])
    im.save(os.path.join(HERE, f'{c["file"]}-Blueprint-2-Views.png'))
    # 3 the interior
    im, d = canvas(f'{c["name"]}  ·  THE INTERIOR', 'Every seat, pole, handle and fitting is a modelled part with its IK points  ·  nothing floats')
    k3, o3, sz3 = paste(im, d, WIRE, c['top_in'][0], (40, 170, 3760, 1000), c['top_in'][1])
    for i, (n, t) in enumerate(c['inside']):
        paste(im, d, WIRE, n, (40 + i * 1265, 1310, 1230, 770), t)
    im.save(os.path.join(HERE, f'{c["file"]}-Blueprint-3-Interior.png'))
    # 4 close-ups and the specification
    im, d = canvas(f'{c["name"]}  ·  CLOSE-UPS AND SPECIFICATION', 'For the game: IK points for the people, pivots for everything that moves, the lights and the sounds')
    for i, (n, t) in enumerate(c['close']):
        paste(im, d, WIRE, n, (40 + i * 1900, 170, 1860, 1163), t)
    lines(d, 60, 1440, c['spec1']); lines(d, 1960, 1440, c['spec2'], step=54)
    im.save(os.path.join(HERE, f'{c["file"]}-Blueprint-4-Closeups.png'))
    print('sheets', key)
for key in (sys.argv[1:] or ['apron', 'taxi']): make(key)
# #endregion
