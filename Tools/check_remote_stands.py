# Proves the remote stands (to-do 277) from Data/airfield.json alone: the clearances of the plan, the grass between the pads
# and the taxiways, no paving over other paving, the triangles covering their outlines, and all the paint on paving.
#   python Tools/check_remote_stands.py [airfield.json]     (ends with 0 only when every line says ok)
import os, sys, json
from shapely.geometry import Polygon, Point, LineString, box
from shapely.ops import unary_union
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d = json.load(open(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'Data', 'airfield.json'), encoding='utf-8'))
R = d['remote']; bad = []
def say(ok, what, v=''):
    print('ok  ' if ok else 'FAIL', what, v)
    if not ok: bad.append(what)
pairs = lambda a: list(zip(a[::2], a[1::2]))
P = {p['id']: Polygon(pairs(p['outer'])) for p in R['polys']}
# #region ── The stands ──
st = {s['id']: s for s in d['stands'] if s.get('remote')}
say(sorted(st) == ['STAND-R%d' % k for k in range(8)], 'eight remote stands R0-R7')
say(all(s['dir'] == 1 and not s['jetway'] and s['built'] in (False, True) for s in st.values()), 'nose north, no jetway')
say(all(s.get('built', True) for s in d['stands'] if not s.get('remote')), 'every other stand in use as before', sum(1 for s in d['stands'] if not s.get('remote')))
plane = {k: box(s['x'] - s['span'] / 2, s['nose'] - s['len'], s['x'] + s['span'] / 2, s['nose']) for k, s in st.items()}
# (the body and the wings, for what stands beside a nose)
shape = {k: unary_union([box(s['x'] - 3.5, s['nose'] - s['len'], s['x'] + 3.5, s['nose']), box(s['x'] - s['span'] / 2, s['nose'] - 0.75 * s['len'], s['x'] + s['span'] / 2, s['nose'] - 0.25 * s['len'])]) for k, s in st.items()}
ks = sorted(st, key=lambda k: st[k]['x'])
for a, b in zip(ks, ks[1:]):
    need = 7.5 if 'E' in (st[a]['code'], st[b]['code']) else 4.5; g = plane[a].distance(plane[b])
    say(g >= need - 0.01, 'between %s and %s at least %.1f m' % (a[6:], b[6:], need), '%.2f' % g)
half = 35.8 / 2
for k, s in st.items():
    if not s.get('flex'): continue
    l, r = s['flex']; say(r - l - 2 * half >= 4.5 - 0.01, '%s: its two code C apart' % k[6:], '%.2f' % (r - l - 2 * half))
    for o in ks:
        if o != k: g = min(abs(st[o]['x'] - f) - half - st[o]['span'] / 2 for f in (l, r)); need = 7.5 if st[o]['code'] == 'E' else 4.5; say(g >= need - 0.01, '%s as two code C to %s' % (k[6:], o[6:]), '%.2f' % g) if g < 20 else None
T = next(t for t in d['taxiways'] if t['id'] == 'T'); TY = T['pts'][0][1]
A = next(t for t in d['taxiways'] if t['id'] == 'A'); AY = A['pts'][0][1]
for k, s in st.items(): say(s['nose'] - s['len'] - TY >= 40 - 0.01, '%s: tail to T at least 40 m' % k[6:], '%.2f' % (s['nose'] - s['len'] - TY))
for k, s in st.items(): say(s['nose'] + 2.1 < 335, '%s: its red line south of the bus road' % k[6:], '%.2f' % s['nose'])
say(AY - 342.5 >= 43.5 - 0.01, 'bus road to taxiway A at least 43.5 m', '%.2f' % (AY - 342.5))
for x, c in zip(R['linkX'], (43.5, 43.5, 51.0)):
    for k in ks:
        g = abs(st[k]['x'] - x) - st[k]['span'] / 2
        if g < 90: say(g >= c - 0.01, 'link at %g to %s wing at least %.1f m' % (x, k[6:], c), '%.2f' % g)
# #endregion
# #region ── Paving ──
links = [box(x - R['linkHalf'], 238, x + R['linkHalf'], 366) for x in R['linkX']]
aprons = [box(a['x0'], a['y0'], a['x1'], a['y1']) for a in d['aprons']]
ids = list(P)
for p in R['polys']:
    g = P[p['id']]; say(g.is_valid and g.exterior.is_ccw, '%s: a clean outline' % p['id'])
    pts = pairs(p['pts']) or pairs(p['outer']); t = p['tri']; tris = [Polygon([pts[i] for i in t[k:k + 3]]) for k in range(0, len(t), 3)]
    inner = Polygon(pts) if not p['id'].startswith('RAMP') else g
    ar = sum(q.area for q in tris); un = unary_union([q.buffer(0) for q in tris]).area
    say(abs(ar - inner.area) < 0.05 + 0.001 * inner.area and abs(un - ar) < 0.05 + 0.001 * ar, '%s: the triangles cover it once' % p['id'], '%.1f of %.1f m2' % (ar, inner.area))
    if p['ring']: say(len(pts) * 2 == len(p['outer']) and inner.buffer(-0.01).within(g), '%s: shoulder outside the concrete' % p['id'])
for i, a in enumerate(ids):
    for b in ids[i + 1:]:
        o = P[a].intersection(P[b]).area
        if o > 0.02: say(False, '%s over %s' % (a, b), '%.2f m2' % o)
    for n, q in zip([a_['id'] for a_ in d['aprons']], aprons):
        o = P[a].intersection(q).area
        if o > 0.02: say(False, '%s over %s' % (a, n), '%.2f m2' % o)
    for x, q in zip(R['linkX'], links):
        o = P[a].intersection(q.buffer(-0.55)).area
        if o > 0.02: say(False, '%s over the link at %g' % (a, x), '%.2f m2' % o)
say(not any('over' in b for b in bad), 'no paving over other paving (the ramps rest 0.5 m on the links)')
bodies = {n: P[n].intersection(box(-1e4, 244.0, 1e4, 331.4)) for n in P if n.startswith('PAD-')}
for n, g in bodies.items():
    for x, q in zip(R['linkX'], links):
        v = g.distance(q)
        if v < 60: say(v >= 12.0, '%s: grass to the link at %g at least 12 m' % (n, x), '%.2f' % v)
    say(g.bounds[1] >= 244 - 0.01, '%s: 6 m of grass to the taxilane apron beside the throats' % n, '%.2f' % (g.bounds[1] - 238))
say(P['BUS-DEPOT'].distance(P['PAD-R0']) > 20, 'grass between the depot and R0', '%.2f' % P['BUS-DEPOT'].distance(P['PAD-R0']))
solid = unary_union([g.buffer(0.01) for g in P.values()] + [box(a['x0'], a['y0'], a['x1'], a['y1']) for a in d['aprons']] + [box(x - 20, 230, x + 20, 390) for x in R['linkX']])
for p in R['polys']:
    for r in p['runs']:
        L = LineString(pairs(r)); U = L.parallel_offset(0.6, 'right') if L.length > 2 else L
        v = U.intersection(solid).length if L.length > 2 else 0
        if v > 1.5: say(False, '%s: a grass edge runs along paving' % p['id'], '%.1f m' % v)
# #endregion
# #region ── Paint, lights, masts, signs ──
off = [(m, round(pts[0], 1), round(pts[1], 1)) for m, w, u, pts in R['paint'] if not LineString(pairs(pts)).buffer(w / 2, cap_style=2).within(solid.buffer(0.03))]
say(not off, 'all paint on paving', off[:4])
conc = unary_union([Polygon(pairs(p['pts'])) for p in R['polys'] if p['ring']] + aprons + [P['BUS-DEPOT'], P['APRON-WEST']])
red = [pairs(pts) for m, w, u, pts in R['paint'] if m == 'paintred']
say(all(LineString(q).within(conc) for q in red), 'the red lines on the concrete, not on the shoulder', len(red))
say(all(not solid.contains(Point(q[:2])) for q in R['lights']), 'every blue light beside the paving', len(R['lights']))
say(all(not solid.contains(Point(q[1:])) for q in R['signs']), 'every sign on the grass', len(R['signs']))
for m in R['masts']:
    g = min(shape[k].distance(Point(m)) for k in ks); say(g >= 6 and m[1] + 1.5 < 335, 'mast at %g clear of the aircraft and the road' % m[0], '%.2f' % g)
    say(min(abs(m[0] - x) for x in R['linkX']) >= 43.5, 'mast at %g clear of the links' % m[0])
low = [(s, y) for s, x, y in R['signs'] if any(abs(x - l) < 43.5 for l in R['linkX'])]; say(not low, 'every sign clear of the links', low)
tower = [box(l['x0'], l['y0'], l['x1'], l['y1']) for l in d['lots']]
say(all(g.distance(q) > 5 for g in P.values() for q in tower), 'clear of every lot')
for r in d['roads']:
    L = LineString(r['pts']).buffer(r.get('w', 7) / 2)
    for n, g in P.items():
        o = L.intersection(g).area
        if o > 0.5: print('note road %s crosses %s (%.0f m2): drawn as lines on it' % (r['id'], n, o))
# #endregion
print('FAILED: %d' % len(bad) if bad else 'ALL OK'); sys.exit(1 if bad else 0)
