# Proves Data/vehicle-area.json (to-do 277 E1): no place off the paving, none over another, nothing a vehicle sweeps on its
# way in or out touches another place's vehicle or leaves the paving. The biggest vehicle each place takes, with its real
# turning circle (ServiceVehicles.h) and the bus's mirrors.
import json, math, os
from shapely.geometry import Polygon, box
from shapely.ops import unary_union
from shapely.affinity import rotate, translate
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = json.load(open(os.path.join(R, 'Data', 'vehicle-area.json'), encoding='utf-8'))
A = json.load(open(os.path.join(R, 'Data', 'airfield.json'), encoding='utf-8'))['remote']
pairs = lambda o: o if isinstance(o[0], (list, tuple)) else list(zip(o[::2], o[1::2]))
polys = [p['outer'] if isinstance(p, dict) else p for p in A['polys']]
pave = unary_union([Polygon(pairs(p)).buffer(0.05) for p in polys] + [box(-248.5, 200, -241.5, 312)])
S = {'bus': (6.99, 0.46, 1.51, 11.2), 'any': (5.10, 0, 1.23, 6.8), 'short': (3.0, 0, 1.03, 6.2)}


def body(x, y, h, f):
    L, m, W, _ = S[f]
    return translate(rotate(box(-L, -W, L + m, W), h, origin=(0, 0)), x, y)


def arc(cx, cy, r, a0, a1, n=30):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


LANE, P, bad = D['lane'], D['places'], 0
st = [body(p['x'], p['y'], p['heading'], p['for']) for p in P]
mk = [translate(rotate(box(-p['len'] / 2, -p['wid'] / 2, p['len'] / 2, p['wid'] / 2), p['heading'], origin=(0, 0)), p['x'], p['y']) for p in P]
for i, p in enumerate(P):
    x, y, h, f = p['x'], p['y'], p['heading'], p['for']
    s = 1 if h > 0 else -1
    r = S[f][3] + 0.4
    if not mk[i].within(pave): bad += 1; print(p['name'], 'place off the paving')
    if not st[i].within(mk[i]): bad += 1; print(p['name'], 'vehicle longer than its place')
    for j in range(i):
        if mk[i].intersects(mk[j]): bad += 1; print(p['name'], 'over', P[j]['name'])
    pin = [(x + r + 5, LANE), (x + r, LANE)] + arc(x + r, LANE + s * r, r, -90 * s, -180 * s) + [(x, LANE + s * r), (x, y)]
    out = [(x, y), (x, LANE - s * r)] + arc(x + r, LANE - s * r, r, 180, 180 - 90 * s) + [(x + r + 2, LANE)]
    gap = 9.9
    for nm, path in (('in', pin), ('out', out)):
        a = 180.0
        for (x0, y0), (x1, y1) in zip(path, path[1:]):
            d = math.hypot(x1 - x0, y1 - y0)
            if d < 1e-6: continue
            n = max(1, int(d / 0.4))
            a = math.degrees(math.atan2(y1 - y0, x1 - x0))
            if nm == 'in' and path.index((x1, y1)) == len(path) - 1 and (x0, y0) == (x, LANE + s * r): a = h  # backing
            for k in range(n + 1):
                cx, cy = x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n
                b = body(cx, cy, a, f)
                if not b.within(pave): bad += 1; print(p['name'], nm, 'off the paving at %.1f %.1f' % (cx, cy)); break
                gap = min([gap] + [b.distance(q) for j, q in enumerate(st) if j != i])
    print('place %s (%s): least gap to another vehicle %.2f m' % (p['name'], f, gap))
    if gap < 0.6: bad += 1; print(p['name'], 'too tight')
print('%d faults' % bad)
