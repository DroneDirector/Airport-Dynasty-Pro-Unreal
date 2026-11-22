# Turns the cargo terminal's renders into 4K sheets (3840 x 2160) with titles, room names and dimensions in large type.
#   python3 finish_cargo_sheets.py   ->  /tmp/cargo_views/Cargo-6..15-*.png  (1-5 are the plans, make_cargo_blueprints.py)
import json, os, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cargo_layout as CL

V = os.environ.get('CARGO_VIEWS') or os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'Blueprints', 'Cargo')
VIEWS = json.load(open(os.path.join(V, 'views.json')))
BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'; REG = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
F = lambda p, s: ImageFont.truetype(p, s)
BG = (7, 26, 51); INK = (234, 242, 255); DIMC = (158, 197, 240); SUB = (127, 155, 189); ACC = (255, 209, 102)
W, H = 3840, 2160


def canvas(title, sub):
    im = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(im)
    d.text((60, 40), title, font=F(BOLD, 76), fill=INK)
    d.text((62, 140), sub, font=F(REG, 40), fill=SUB)
    return im, d


def text_box(d, xy, t, font, fill=INK, bg=BG, anchor='mm'):
    b = d.textbbox(xy, t, font=font, anchor=anchor)
    d.rectangle((b[0] - 12, b[1] - 8, b[2] + 12, b[3] + 8), fill=bg)
    d.text(xy, t, font=font, fill=fill, anchor=anchor)


def arrow_h(d, x0, x1, y, t, col=DIMC, size=46):
    d.line((x0, y, x1, y), fill=col, width=4)
    for x, s in ((x0, 1), (x1, -1)):
        d.polygon([(x, y), (x + s * 26, y - 11), (x + s * 26, y + 11)], fill=col)
        d.line((x, y - 24, x, y + 24), fill=col, width=3)
    text_box(d, ((x0 + x1) / 2, y), t, F(BOLD, size), fill=col)


def arrow_v(d, x, y0, y1, t, col=DIMC, size=42, side='r'):
    d.line((x, y0, x, y1), fill=col, width=4)
    for y, s in ((y0, 1), (y1, -1)):
        d.polygon([(x, y), (x - 11, y + s * 26), (x + 11, y + s * 26)], fill=col)
        d.line((x - 24, y, x + 24, y), fill=col, width=3)
    f = F(BOLD, size)
    if side == 'r': text_box(d, (x + 18, (y0 + y1) / 2), t, f, fill=col, anchor='lm')
    else: text_box(d, (x - 18, (y0 + y1) / 2), t, f, fill=col, anchor='rm')


# #region ── Elevations: two halves stacked (front = airside, back = landside) ──
def elevation(kind, title, sub, order, mirrored):
    im, d = canvas(title, sub)
    y = 260
    for half, label in order:
        v = VIEWS[f'{kind}_{half}']; r = Image.open(os.path.join(V, f'{kind}_{half}.png')).convert('RGB')
        im.paste(r, (0, y)); h = r.height
        ppm = W / v['scale']; xc = v['loc'][0]; zc = v['loc'][2]
        px = (lambda X: W / 2 - (X - xc) * ppm) if mirrored else (lambda X: W / 2 + (X - xc) * ppm)
        pz = lambda Z: y + h / 2 - (Z - zc) * ppm
        d.text((40, y + 20), label, font=F(BOLD, 44), fill=ACC)
        # heights at the zones that are in this half
        for X, Z, t in ((15, 14, '14 m'), (82, 16, '16 m'), (210, 24, '24 m'), (315, 16, '16 m'), (375, 12, '12 m'), (435, 12, '12 m')):
            xa, xb = sorted((v['loc'][0] - v['scale'] / 2, v['loc'][0] + v['scale'] / 2))
            if xa + 6 < X < xb - 6:
                arrow_v(d, px(X), pz(Z), pz(0), t, size=40)
        if kind == 'front':
            X = 105 if half == 'w' else 300
            arrow_v(d, px(X), pz(9.0), pz(0), '9 m canopy', col=(199, 166, 255), size=34)
        im.paste(Image.new('RGB', (W, 4), (40, 70, 110)), (0, y + h + 6))
        y += h + 70
    # zone lengths along the bottom
    nm = dict(off='offices', exp='export', etv='high-bay ULD store', imp='import', spc='special', bcp='border control')
    seg = [(a, b, f'{nm[zk]} {b - a:.0f}') for zk, (a, b), _, _ in CL.ZONES]
    yy = y + 40
    sx = (W - 3600) / 2; k = 3600 / CL.L
    X = (lambda a: sx + (CL.L - a) * k) if mirrored else (lambda a: sx + a * k)
    for a, b, t in seg:
        x0, x1 = sorted((X(a), X(b)))
        arrow_h(d, x0 + 4, x1 - 4, yy, t, size=36 if b - a < 60 else 42)
    arrow_h(d, X(0) if not mirrored else X(CL.L), X(CL.L) if not mirrored else X(0), yy + 110, f'{CL.L:.0f} m overall', col=INK, size=48)
    return im


elevation('front', 'CARGO TERMINAL  -  AIRSIDE ELEVATION (FRONT)',
          'Seen from the cargo apron  ·  21 ULD doors under the 12 m canopy (amber)  ·  cold-room and animal doors at the east end  ·  green: escape doors  ·  heights',
          [('w', 'WEST HALF'), ('e', 'EAST HALF')], False).save(os.path.join(V, 'Cargo-8-Airside-Elevation.png'))
elevation('back', 'CARGO TERMINAL  -  LANDSIDE ELEVATION (BACK)',
          'Seen from the truck court  ·  24 truck docks + 8 cool-chain and BCP docks (amber), 2 drive-in doors, green escape doors  ·  east end on the left',
          [('e', 'EAST HALF'), ('w', 'WEST HALF')], True).save(os.path.join(V, 'Cargo-9-Landside-Elevation.png'))
# #endregion

# #region ── End elevations ──
im, d = canvas('CARGO TERMINAL  -  END ELEVATIONS (SIDES)', 'West end (offices) and east end (special cargo)  ·  70 m deep, canopy 12 m, docks 6 m')
y = 250
for nm, label, flip in (('west', 'WEST END  -  airside on the right', True), ('east', 'EAST END  -  airside on the left', False)):
    v = VIEWS[nm]; r = Image.open(os.path.join(V, nm + '.png')).convert('RGB'); im.paste(r, (0, y)); h = r.height
    ppm = W / v['scale']; yc = v['loc'][1]; zc = v['loc'][2]
    py = (lambda Y: W / 2 - (Y - yc) * ppm) if flip else (lambda Y: W / 2 + (Y - yc) * ppm)
    pz = lambda Z: y + h / 2 - (Z - zc) * ppm
    d.text((40, y + 20), label, font=F(BOLD, 44), fill=ACC)
    x0, x1 = sorted((py(0), py(70)))
    arrow_h(d, x0, x1, pz(0) + 60, '70 m', size=44)
    hz = 14 if nm == 'west' else 24
    arrow_v(d, py(35) + (0 if flip else 0), pz(hz), pz(0), f'{hz} m' + (' offices' if nm == 'west' else ' high bay behind'), size=40)
    if nm == 'east':
        arrow_v(d, py(55), pz(12), pz(0), '12 m border control post', size=36, side='r')
    y += h + 50
im.save(os.path.join(V, 'Cargo-10-End-Elevations.png'))
# #endregion

# #region ── Top view ──
v = VIEWS['top']; r = Image.open(os.path.join(V, 'top.png')).convert('RGB')
im = r.copy(); d = ImageDraw.Draw(im)
ppm = W / v['scale']; cx, cy = v['loc'][0], v['loc'][1]
px = lambda X: W / 2 + (X - cx) * ppm; py = lambda Y: H / 2 - (Y - cy) * ppm
d.rectangle((0, 0, W, 210), fill=BG)
d.text((60, 30), 'CARGO TERMINAL  -  TOP VIEW', font=F(BOLD, 76), fill=INK)
d.text((62, 128), 'Apron with stands C1-C4 at the bottom, the building, the truck court and the moved fence at the top  ·  metres',
       font=F(REG, 40), fill=SUB)
arrow_h(d, px(0), px(CL.L), py(-170) + 40, f'{CL.L:.0f} m', size=48)
arrow_v(d, px(CL.L + 5) + 40, py(70), py(0), '70 m', size=44)
arrow_v(d, px(CL.L + 5) + 40, py(210), py(70), '140 m truck court', size=40)
arrow_v(d, px(-35) - 30, py(0), py(-165), '165 m apron', size=40, side='l')
for i, sx in enumerate((60.0, 147.5, 235.0, 322.5)):
    text_box(d, (px(sx), py(-150)), f'C{i + 1}', F(BOLD, 54), fill=ACC)
for X, t in ((15, 'offices'), (82, 'export'), (210, 'high-bay ULD store'), (315, 'import'), (375, 'special'), (435, 'BCP')):
    text_box(d, (px(X), py(35)), t, F(BOLD, 40 if X != 210 else 46), fill=INK, bg=(20, 45, 80))
im.save(os.path.join(V, 'Cargo-6-Top-View.png'))
# #endregion

# #region ── Angled views ──
for nm, out, title, sub in (('airside_above', 'Cargo-11-Airside-From-Above.png', 'CARGO TERMINAL  -  AIRSIDE, FROM ABOVE',
                             'The apron side: canopy over the ULD doors, ULDs waiting in the staging lanes, the high-bay store with solar panels'),
                            ('landside_above', 'Cargo-12-Landside-Odd-Angle.png', 'CARGO TERMINAL  -  LANDSIDE, FROM AN ANGLE',
                             'The truck court: lorries at the docks, the lorry park, the staff car park, the security gate in the moved fence'),
                            ('apron_low', 'Cargo-13-Apron-Low-Angle.png', 'CARGO TERMINAL  -  FROM THE APRON, LOW',
                             'As a freighter crew sees it from stand C4: the airside face, 465 m long'),
                            ('cut_west', 'Cargo-14-Cutaway-West.png', 'CARGO TERMINAL  -  CUTAWAY FROM THE WEST',
                             'Roof and upper walls left out: office ground floor, export with screening, racks and build-up, the ETV store'),
                            ('cut_east', 'Cargo-15-Cutaway-East.png', 'CARGO TERMINAL  -  CUTAWAY FROM THE EAST',
                             'Roof off: the border control post (horses, animals, animal products), special cargo, dangerous goods  ·  amber and green: doors')):
    r = Image.open(os.path.join(V, nm + '.png')).convert('RGB'); im = r.copy(); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W, 210), fill=BG)
    d.text((60, 30), title, font=F(BOLD, 76), fill=INK); d.text((62, 128), sub, font=F(REG, 40), fill=SUB)
    im.save(os.path.join(V, out))
# #endregion
# #region ── Cutaway from straight above: a 3D floor plan with every room named or numbered ──
v = VIEWS['cut_top']; r = Image.open(os.path.join(V, 'cut_top.png')).convert('RGB')
im, d = canvas('CARGO TERMINAL  -  CUTAWAY PLAN (ROOF OFF)', 'Every room and working area seen from above  ·  numbers as on the floor plan  ·  apron at the bottom, trucks at the top')
TOP = 230; im.paste(r, (0, TOP)); ppm = W / v['scale']
px = lambda X: W / 2 + (X - v['loc'][0]) * ppm; py = lambda Y: TOP + r.height / 2 - (Y - v['loc'][1]) * ppm
for rr in CL.ROOMS:
    name, x0, y0, x1, y1, zk, kind, doors, short = rr
    if kind == 'aisle': continue
    c = (px((x0 + x1) / 2), py((y0 + y1) / 2))
    if short: text_box(d, c, short.replace('\n', ' ') if x1 - x0 > 70 else short, F(BOLD, 34), bg=(7, 26, 51))
    else:
        n = str(CL.NUM[id(rr)]); small = x1 - x0 < 9; f = F(BOLD, 24 if small else 32); rad = 19 if small else 28
        d.ellipse((c[0] - rad, c[1] - rad, c[0] + rad, c[1] + rad), fill=BG, outline=ACC, width=4); d.text(c, n, font=f, fill=INK, anchor='mm')
yb = TOP + r.height + 60
for a, b in [(a, b) for _, (a, b), _, _ in CL.ZONES]: arrow_h(d, px(a) + 3, px(b) - 3, yb, f'{b - a:.0f} m', size=40)
arrow_h(d, px(0), px(CL.L), yb + 90, f'{CL.L:.0f} m', col=INK, size=44)
y0l = yb + 170
for i, rr in enumerate(CL.NUMBERED):
    col, row = i // 13, i % 13
    d.text((40 + col * 950, y0l + row * 52), f'{i + 1:>2}', font=F(BOLD, 30), fill=ACC)
    d.text((100 + col * 950, y0l + row * 52), rr[0], font=F(REG, 30), fill=INK)
im.save(os.path.join(V, 'Cargo-7-Cutaway-Plan.png'))
# #endregion
print('sheets done')
