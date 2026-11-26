# The cargo terminal's site data (run again after the layout changes; it replaces its own entries, nothing else):
#  - Data/Sites/chablais/roads.json: CARGO-ACCESS, the truck court's gate out to the public road - the real Route de
#    Beau Cret (swissTLM3D, a 4 m road that runs right past the court) west to the main Route de la Porte du Scex
#  - Data/holomap-buildings.json: CARGO TERMINAL - the shed's outline and fire walls, the truck court's fence with its
#    gate, the barrier's boom and the access road's first stretch (to where it meets Beau Cret)
# Plan metres throughout (x along the airport, y south). The layout's numbers: Art/CargoTerminal/cargo_layout.py.
import json, math, os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Art', 'CargoTerminal'))
import cargo_layout as CL
X0, Y0 = 200.0, 1231.0                 # the model's origin in plan (Airfield.h Cargo; yaw 180: model x -> plan x, y -> y)
L, D, YARD = CL.L, CL.D, CL.YARD_D       # the shed's length and depth, the truck court's depth
FENCE = Y0 + D + YARD                  # 1441
GATE = (X0 + 150.0, X0 + 176.0)        # the gate's opening in the fence
BOOM = (X0 + 161.5, X0 + 174.0)
HALF = 4.25                            # half the road: a 3.5 m lane and a 0.75 m shoulder each side

# the gate straight out, a bend onto Beau Cret, then its swissTLM3D line to the main road
ROAD = [(363.0, FENCE), (363.0, 1462.0), (356.0, 1478.0), (338.0, 1490.0), (300.0, 1501.0), (250.0, 1510.0),
        (14.3, 1554.6), (-75.1, 1588.6), (-286.2, 1615.6), (-524.6, 1613.8)]


def length(pts):
    return sum(math.dist(pts[i], pts[i + 1]) for i in range(len(pts) - 1))


def edges(pts, off):
    """The polyline moved off metres to its left (mitred at the corners)."""
    out = []
    for i, p in enumerate(pts):
        a = pts[max(0, i - 1)]; b = pts[min(len(pts) - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]; n = math.hypot(dx, dy)
        out.append((round(p[0] - dy / n * off, 2), round(p[1] + dx / n * off, 2)))
    return out


def road():
    p = os.path.join(ROOT, 'Data', 'Sites', 'chablais', 'roads.json')
    j = json.load(open(p, encoding='utf-8'))
    n = round(length(ROAD), 1)
    r = {'id': 'CARGO-ACCESS', 'pts': [[round(x, 2), round(y, 2)] for x, y in ROAD],
         'lanes': {'left': [{'type': 'Driving', 'w': 3.5, 'invert': True}, {'type': 'Shoulder', 'w': 0.75}],
                   'right': [{'type': 'Driving', 'w': 3.5}, {'type': 'Shoulder', 'w': 0.75}]},
         'paint': [{'kind': 'line', 'side': s, 'edge': 1, 'off': 0.1, 'w': 0.15, 'from': 0.0, 'to': n} for s in ('right', 'left')],
         'step': 4.0,
         'note': "the cargo terminal's truck court gate out to the public road: swissTLM3D Route de Beau Cret to the Route de la Porte du Scex"}
    j['roads'] = [x for x in j['roads'] if x.get('id') != 'CARGO-ACCESS'] + [r]
    json.dump(j, open(p, 'w', encoding='utf-8'))
    c = os.path.join(ROOT, 'Content', 'Data', 'Sites', 'chablais', 'roads.json')
    if os.path.isdir(os.path.dirname(c)): shutil.copyfile(p, c)
    print('road CARGO-ACCESS: %d points, %.0f m' % (len(ROAD), n))


def holomap():
    p = os.path.join(ROOT, 'Data', 'holomap-buildings.json')
    j = json.load(open(p, encoding='utf-8'))
    segs = []
    def line(a, b, glass=0): segs.append([round(a[0], 2), round(a[1], 2), round(b[0], 2), round(b[1], 2), glass])
    def ring(x0, y0, x1, y1):
        for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))): line(a, b)
    ring(X0, Y0, X0 + L, Y0 + D)                                     # the shed
    for fx, _h in CL.FIRE_WALLS:                                     # its fire walls
        line((X0 + fx, Y0), (X0 + fx, Y0 + D))
    line((X0, Y0 + D), (X0, FENCE)); line((X0 + L, Y0 + D), (X0 + L, FENCE))   # the court's fence: sides
    line((X0, FENCE), (GATE[0], FENCE)); line((GATE[1], FENCE), (X0 + L, FENCE))  # and front, the gate open
    line((BOOM[0], FENCE - 0.85), (BOOM[1], FENCE - 0.85), 1)       # the barrier's boom
    stub = ROAD[:7]                                                  # the road to Beau Cret (the rest would zoom the map out)
    for off in (HALF, -HALF):
        e = edges(stub, off)
        for i in range(len(e) - 1): line(e[i], e[i + 1])
    b = {'id': 'CARGO', 'label': 'CARGO TERMINAL', 'floors': [{'label': 'GROUND FLOOR', 'y0': -0.3, 'y1': 99.0, 'segs': segs}]}
    j = [x for x in j if x.get('id') != 'CARGO'] + [b]
    json.dump(j, open(p, 'w', encoding='utf-8'), separators=(',', ':'))
    c = os.path.join(ROOT, 'Content', 'Data', 'holomap-buildings.json')
    if os.path.isfile(c): shutil.copyfile(p, c)
    print('holomap CARGO TERMINAL: %d lines' % len(segs))


road(); holomap()
