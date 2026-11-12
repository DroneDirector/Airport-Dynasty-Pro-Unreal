# The whole terminal as a holographic map picture (owner, 2026-11-12): both floors' rooms from the plan, the four
# jetways out to their stands with an aircraft at each, in the game's holographic map colours (SHoloMap.cpp: cyan lines
# on deep blue) over a blueprint grid. Our own drawing from Data/terminal-plan.json and Data/airfield.json.
#   python Tools/make_holo_terminal_map.py   ->   docs/terminal-holomap.png
import json, math
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
plan = json.load(open(ROOT / 'Data' / 'terminal-plan.json'))
field = json.load(open(ROOT / 'Data' / 'airfield.json'))
# the window: plan x -110..330, z -40..150 (the terminal, the kerb, the stands); 6 px a metre
X0, X1, Z0, Z1, S = -110, 330, -40, 168, 6
W, H = int((X1 - X0) * S), int((Z1 - Z0) * S)
def P(x, z): return (int((x - X0) * S), int((Z1 - z) * S))   # the apron (big z) at the top, as the map shows it

BG = (3, 13, 26); GRID = (10, 34, 60); WALL = (140, 214, 255); GLASS = (120, 230, 255); DIM = (70, 120, 170)
LABEL = (200, 235, 255); AIR = (255, 191, 102); ROUTE = (255, 200, 120)
img = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(img)
# the blueprint grid, 10 m and 50 m
for x in range(X0 - X0 % 10, X1 + 1, 10):
    c = (16, 52, 88) if x % 50 == 0 else GRID; d.line([P(x, Z0), P(x, Z1)], fill=c, width=1)
for z in range(Z0 - Z0 % 10, Z1 + 1, 10):
    c = (16, 52, 88) if z % 50 == 0 else GRID; d.line([P(X0, z), P(X1, z)], fill=c, width=1)
try: font = ImageFont.truetype('arial.ttf', 11); big = ImageFont.truetype('arialbd.ttf', 16)
except Exception: font = big = ImageFont.load_default()

glow = Image.new('RGB', (W, H), (0, 0, 0)); gd = ImageDraw.Draw(glow)
def room_box(r, col, width):
    a, b = P(r['x0'], r['z1']), P(r['x1'], r['z0'])
    d.rectangle([a, b], outline=col, width=width); gd.rectangle([a, b], outline=col, width=width + 2)
    if r.get('glass'):   # the glass side a brighter line: n s e w
        g = r['glass']
        if 'n' in g: d.line([P(r['x0'], r['z1']), P(r['x1'], r['z1'])], fill=GLASS, width=width + 1)
        if 's' in g: d.line([P(r['x0'], r['z0']), P(r['x1'], r['z0'])], fill=GLASS, width=width + 1)
        if 'e' in g: d.line([P(r['x1'], r['z0']), P(r['x1'], r['z1'])], fill=GLASS, width=width + 1)
        if 'w' in g: d.line([P(r['x0'], r['z0']), P(r['x0'], r['z1'])], fill=GLASS, width=width + 1)
# the ground floor faint, the upper floor (the gates) bright on top
for r in plan['rooms']:
    if r.get('f', 1) == 1: room_box(r, DIM, 1)
for r in plan['rooms']:
    if r.get('f', 1) == 2: room_box(r, WALL, 2)
for r in plan['rooms']:
    if r.get('f', 1) == 2 and (r['x1'] - r['x0']) * (r['z1'] - r['z0']) > 120:
        cx, cz = (r['x0'] + r['x1']) / 2, (r['z0'] + r['z1']) / 2
        name = r['id'].replace('F2-', '').replace('-', ' ')
        d.text((P(cx, cz)[0] - len(name) * 3, P(cx, cz)[1] - 14), name, fill=LABEL, font=font)
# the jetways: from each gate's jetway door straight out, a 5-degree slope not seen from above, to the aircraft's door
stands = {s['id']: s for s in field['stands']}
jet = [dd for dd in plan['doors'] if dd.get('type') == 'jetway']
for n, dd in enumerate(sorted(jet, key=lambda q: q['x']), 1):
    st = stands.get('STAND-%d' % n)
    if not st: continue
    nose = st['nose']; L = st['len']; span = st['span']; x = st['x']
    door_z = nose + 0.18 * L                      # the front door a little behind the nose (the body runs away from the terminal)
    a, b = P(dd['x'], dd['z']), P(x - 2.6, door_z)
    d.line([a, b], fill=WALL, width=4); gd.line([a, b], fill=WALL, width=8)
    d.ellipse([b[0] - 6, b[1] - 6, b[0] + 6, b[1] + 6], outline=WALL, width=2)
    # the aircraft: fuselage, wings swept back, tailplane, fin, pointing at the terminal (dir -1: nose towards -z)
    tail = nose + L
    fus = [P(x, nose), P(x + 2.2, nose + 4), P(x + 2.2, tail - 6), P(x + 1.2, tail), P(x - 1.2, tail), P(x - 2.2, tail - 6), P(x - 2.2, nose + 4)]
    wing_root, wing_tip = nose + L * 0.42, nose + L * 0.72
    wings = [P(x - 2.2, wing_root), P(x - span / 2, wing_tip), P(x - span / 2, wing_tip + 2.5), P(x - 2.2, wing_root + 7)]
    wingr = [(2 * P(x, 0)[0] - px, py) for px, py in wings]
    tailp = [P(x - 1.2, tail - 8), P(x - span * 0.2, tail - 2), P(x - span * 0.2, tail - 0.5), P(x - 1.2, tail - 4)]
    tailr = [(2 * P(x, 0)[0] - px, py) for px, py in tailp]
    for poly in (fus, wings, wingr, tailp, tailr):
        d.polygon(poly, outline=AIR, fill=(60, 44, 20)); gd.polygon(poly, outline=AIR, fill=(0, 0, 0))
    d.text((P(x, tail + 12)[0] - 22, P(x, tail + 12)[1]), 'STAND %d' % n, fill=AIR, font=font)
# the measures (owner, 2026-11-12): an arrow with its length in metres beside it, holographic style, for every
# upper-floor room (width and depth) and along the building's outer edges
ARROW = (255, 230, 150)
def arrow(a, b, text, offset=(0, 0), col=ARROW):
    ax, ay = a[0] + offset[0], a[1] + offset[1]; bx, by = b[0] + offset[0], b[1] + offset[1]
    d.line([(ax, ay), (bx, by)], fill=col, width=1)
    ang = math.atan2(by - ay, bx - ax)
    for (px, py, sgn) in ((ax, ay, 1), (bx, by, -1)):
        for side in (-1, 1):
            d.line([(px, py), (px + sgn * 7 * math.cos(ang + side * 0.5), py + sgn * 7 * math.sin(ang + side * 0.5))], fill=col, width=1)
    tw = len(text) * 5.5
    mx, my = (ax + bx) / 2, (ay + by) / 2
    if abs(bx - ax) > abs(by - ay): d.text((mx - tw / 2, my - 14), text, fill=col, font=font)
    else: d.text((mx + 5, my + 6), text, fill=col, font=font)
def measure_room(r, inner=True):
    w, dp = r['x1'] - r['x0'], r['z1'] - r['z0']
    if inner:
        if w * S > 70: arrow(P(r['x0'] + 1, r['z1'] - 2.2), P(r['x1'] - 1, r['z1'] - 2.2), '%.0f m' % w)
        if dp * S > 50 and w >= 10: arrow(P(r['x0'] + 2.2, r['z1'] - 1), P(r['x0'] + 2.2, r['z0'] + 1), '%.0f m' % dp)
for r in plan['rooms']:
    if r.get('f', 1) == 2: measure_room(r)
# the outside: the building's extent on the upper floor (its outer edges), measured a little outside the walls
f2 = [r for r in plan['rooms'] if r.get('f', 1) == 2]
bx0, bx1 = min(r['x0'] for r in f2), max(r['x1'] for r in f2)
bz0, bz1 = min(r['z0'] for r in f2), max(r['z1'] for r in f2)
arrow(P(bx0, bz1 + 4), P(bx1, bz1 + 4), 'UPPER FLOOR  %.0f m' % (bx1 - bx0))
arrow(P(bx1 + 4, bz1), P(bx1 + 4, bz0), '%.0f m' % (bz1 - bz0))
# each gate block's length along the front, and the walk's
for r in f2:
    if r['id'].startswith('F2-GATE-') and r['id'].count('-') == 2:
        arrow(P(r['x0'], r['z1'] + 2), P(r['x1'], r['z1'] + 2), '%.0f m' % (r['x1'] - r['x0']))
# a stand's dimensions once (they are all alike): the aircraft's length and span, and the jetway's reach
st1 = stands['STAND-1']
arrow(P(st1['x'] + st1['span'] / 2 + 3, st1['nose']), P(st1['x'] + st1['span'] / 2 + 3, st1['nose'] + st1['len']), 'AIRCRAFT %.1f m' % st1['len'])
arrow(P(st1['x'] - st1['span'] / 2, st1['nose'] + st1['len'] + 3), P(st1['x'] + st1['span'] / 2, st1['nose'] + st1['len'] + 3), 'SPAN %.0f m' % st1['span'])
j1 = sorted(jet, key=lambda q: q['x'])[0]
jl = math.hypot(st1['x'] - 2.6 - j1['x'], st1['nose'] + 0.18 * st1['len'] - j1['z'])
arrow(P(j1['x'] - 3, j1['z']), P(st1['x'] - 5.6, st1['nose'] + 0.18 * st1['len']), 'JETWAY %.0f m' % jl)
# the glow laid under the lines
glow = glow.filter(ImageFilter.GaussianBlur(6))
img = Image.blend(img, Image.composite(glow, img, glow.convert('L').point(lambda v: min(255, v * 2))), 0.5)
d = ImageDraw.Draw(img)
for r in plan['rooms']:
    if r.get('f', 1) == 2: room_box(r, WALL, 2)
for n, dd in enumerate(sorted(jet, key=lambda q: q['x']), 1):
    st = stands.get('STAND-%d' % n)
    if st: d.line([P(dd['x'], dd['z']), P(st['x'] - 2.6, st['nose'] + 0.18 * st['len'])], fill=WALL, width=3)
d.text((14, H - 44), 'MÝRDALSSANDUR TERMINAL  ·  HOLOGRAPHIC MAP', fill=LABEL, font=big)
d.text((14, H - 22), 'upper floor bright, ground floor faint; jetways J-1 to J-4 to stands 1-4 with an aircraft at each', fill=DIM, font=font)
out = ROOT / 'docs' / 'terminal-holomap.png'
img.save(out); print('wrote', out, img.size)
