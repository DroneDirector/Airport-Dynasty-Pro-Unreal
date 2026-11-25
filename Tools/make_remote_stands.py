# The remote stands R0-R7 (to-do 277 step A): writes the stands and the "remote" block of Data/airfield.json from the approved
# plan (Tools/plan_remote_stands_fit.py, Tools/render_remote_stands.py, HANDOVER item 3; picture Blueprints/Airfield/
# RemoteStands-Full.png). Plan metres, y north. Everything drawn is worked out here (outlines with fillets, triangles, paint,
# lights, signs), so Tools/check_remote_stands.py proves the very shapes the game draws (AirfieldRemote.cpp).
#   python Tools/make_remote_stands.py [out.json]     (stands stay "built": false; step B switches them on)
import os, sys, json, math
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC = os.path.join(ROOT, 'Data', 'airfield.json')
OUT = sys.argv[1] if len(sys.argv) > 1 else SRC
d = json.load(open(SRC, encoding='utf-8'))
# #region ── Numbers of the plan ──
TY, TE, Y0, RY0, RY1, SH = 218.0, 238.0, 244.0, 335.0, 342.5, 3.0     # taxilane T, its paved edge, the pads' south edge, the bus road, the paved shoulder
LINK = {'P1': (-130.0, 43.5), 'P2': (150.0, 43.5), 'P3': (362.0, 51.0)}   # centre line, clearance (code E; P3: A380)
LH = 20.0                                                              # a link's half width with its shoulder
R = lambda i, x, c, fl=None: dict(id='STAND-R%d' % i, x=x, nose=318.0 if c == 'C' else 331.9, dir=1, span=36 if c == 'C' else 65,
        len=46.0 if c == 'C' else 73.9, code=c, jetway=False, remote=True, built=False, **({'flex': fl} if fl else {}))
STANDS = [R(0, -191.5, 'C'), R(1, -68.5, 'C'), R(2, -28.0, 'C'), R(3, 35.75, 'E', [15.5, 56.0]), R(4, 231.75, 'E', [211.5, 252.0]),
          R(5, 292.5, 'C'), R(6, 445.5, 'E'), R(7, 518.0, 'E')]
HW = lambda s: 16.0 if s['code'] == 'C' else 29.0 if s.get('flex') else 26.0   # a throat's half width (two lead-ins on the flexible ones)
PADS = [('PAD-R0', -219.5, -163.5, 1, 1), ('PAD-R1-R3', -96.5, 84.0, 1, 1), ('PAD-R4-R5', 183.5, 320.5, 1, 1), ('PAD-R6-R7', 403.0, 560.5, 1, 0)]
DEPOT = (-298.0, 244.0, -258.0, 304.0); LEG = (-248.5, -241.5)         # the bus depot, the bus road's way over T (x -245)
WEST = (-290.0, 82.0, -265.0, 238.0)                                   # the apron at the terminal's west end, 25 m wider
MASTS = [(-216.0, 331.0), (-48.25, 331.0), (77.0, 331.0), (269.5, 331.0), (481.75, 331.0)]
# #endregion
# #region ── Outlines ──
def arc(cx, cy, r, a0, a1, n=10): return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]
def pad(x0, x1, rl, rr, throats, s):
    """A pad's outline, counter-clockwise, s metres in from its edge (its mouths on T and its side on the bus road stay put)."""
    f, t = 3.5, 3.0; cut = math.degrees(math.asin(f / (f + s))); tc = math.degrees(math.asin(t / (t + s)))
    P = arc(x0 - f, RY0 - f, f + s, cut, 0) if rl else [(x0 + s, RY0)]
    P += arc(x0 + 12, Y0 + 12, 12 - s, 180, 270)
    for c, hw in throats:
        P += arc(c - hw - t, TE + t, t + s, 90, -tc, 16) + arc(c + hw + t, TE + t, t + s, 180 + tc, 90, 16)
    P += arc(x1 - 12, Y0 + 12, 12 - s, 270, 360)
    P += arc(x1 + f, RY0 - f, f + s, 180, 180 - cut) if rr else [(x1 - s, RY0)]
    return P
def rrect(x0, y0, x1, y1, rs, s=0.0):
    """A rectangle with rounded corners (sw, se, ne, nw; 0: square), counter-clockwise."""
    P = []
    for (cx, cy, sx, sy, a), r in zip(((x0, y0, 1, 1, 180), (x1, y0, -1, 1, 270), (x1, y1, -1, -1, 0), (x0, y1, 1, -1, 90)), rs):
        P += arc(cx + sx * r, cy + sy * r, r - s, a, a + 90) if r > s else [(cx + sx * s, cy + sy * s)]
    return P
def ears(P):
    """Triangles of a simple counter-clockwise polygon."""
    cr = lambda a, b, c: (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    I, T = list(range(len(P))), []
    while len(I) > 3:
        best = None
        for k in range(len(I)):
            a, b, c = P[I[k - 1]], P[I[k]], P[I[(k + 1) % len(I)]]; A = cr(a, b, c)
            if A < -1e-9: continue
            if A > 1e-9 and any(cr(a, b, P[q]) > -1e-9 and cr(b, c, P[q]) > -1e-9 and cr(c, a, P[q]) > -1e-9 for q in I if q not in (I[k - 1], I[k], I[(k + 1) % len(I)])): continue
            L = math.dist(a, c)
            if A <= 1e-9: best = (0, k, False); break
            if best is None or L < best[0]: best = (L, k, True)
        if best is None: raise SystemExit('the outline crosses itself')
        _, k, keep = best
        if keep: T.append((I[k - 1], I[k], I[(k + 1) % len(I)]))
        I.pop(k)
    T.append(tuple(I)); return T
polys = []
def add(i, mat, outer, pts=None, tri=None, ring=False): polys.append(dict(id=i, mat=mat, outer=outer, pts=pts or outer, tri=tri or ears(pts or outer), ring=ring))
for n, x0, x1, rl, rr in PADS:
    th = [(s['x'], HW(s)) for s in STANDS if x0 < s['x'] < x1]
    add(n, 'apron', pad(x0, x1, rl, rr, th, 0.0), pad(x0, x1, rl, rr, th, SH), ring=True)
add('BUS-DEPOT', 'apron', rrect(*DEPOT, (8, 8, 8, 8)))
add('APRON-WEST', 'apron', rrect(*WEST, (12, 0, 0, 12)))
# the bus road: level pieces 12 m short of each link's shoulder, then a ramp down onto it with flared corners
XW = (LEG[0], LINK['P1'][0] - 32)
L = [(LEG[1] + 6, TE)] + arc(LEG[1] + 6, TE + 6, 6, 270, 180)[1:] + arc(LEG[1] + 7, RY0 - 7, 7, 180, 90) + [(XW[1], RY0), (XW[1], RY1)] \
    + arc(LEG[0] + 14, RY1 - 14, 14, 90, 180) + arc(LEG[0] - 6, TE + 6, 6, 0, -90)
add('BUS-ROAD-W', 'asphalt', L)
add('DEPOT-WAY', 'asphalt', arc(DEPOT[2] + 4, 266, 4, 180, 90, 6) + arc(LEG[0] - 4, 266, 4, 90, 0, 6) + arc(LEG[0] - 4, 281, 4, 0, -90, 6) + arc(DEPOT[2] + 4, 281, 4, 270, 180, 6))
cs = [v[0] for v in LINK.values()]
for k in range(2): add('BUS-ROAD-%d' % (k + 1), 'asphalt', rrect(cs[k] + 32, RY0, cs[k + 1] - 32, RY1, (0, 0, 0, 0)))
add('BUS-ROAD-E', 'asphalt', rrect(cs[2] + 32, RY0, 560.5, RY1, (0, 0, 5, 0)))
for n, (c, _) in LINK.items():
    for s in (-1, 1):
        u = [32 - 0.5 * k for k in range(26)]; f = lambda q: 0.0 if q >= 25 else 5 - math.sqrt(max(0.0, 25 - (25 - max(q, 20)) ** 2))
        a, b = [(c + s * q, RY0 - f(q)) for q in u], [(c + s * q, RY1 + f(q)) for q in u]; N = len(u)
        o = a + b[::-1]; o = o if s < 0 else o[::-1]
        add('RAMP-%s%s' % (n, 'W' if s < 0 else 'E'), 'asphalt', o, a + b, [t for k in range(N - 1) for t in ((k, k + 1, N + k + 1), (k, N + k + 1, N + k))])
def inside(P, q):
    r = False
    for (ax, ay), (bx, by) in zip(P, P[1:] + P[:1]):
        if (ay > q[1]) != (by > q[1]) and q[0] < (bx - ax) * (q[1] - ay) / (by - ay) + ax: r = not r
    return r
AP = next(a for a in d['aprons'] if a['id'] == 'APRON-T')
def paved(q, me=None):
    if AP['x0'] <= q[0] <= AP['x1'] and AP['y0'] <= q[1] <= AP['y1'] + 0.01: return True
    if any(abs(q[0] - c) < LH + 0.2 and 230 < q[1] < 390 for c in cs): return True
    return any(p['id'] != me and inside(p['outer'], q) for p in polys)
# the runs of edge that meet the grass (right of the way round is outside), straight legs in steps of at most 2.5 m
lights = []
for p in polys:
    P, runs, run = p['outer'], [], []
    for a, b in zip(P, P[1:] + P[:1]):
        D = math.dist(a, b)
        if D < 1e-6: continue
        nx, ny = (b[1] - a[1]) / D, -(b[0] - a[0]) / D; n = max(1, math.ceil(D / 2.5))
        for k in range(n):
            q0, q1 = [(a[0] + (b[0] - a[0]) * t / n, a[1] + (b[1] - a[1]) * t / n) for t in (k, k + 1)]; m = ((q0[0] + q1[0]) / 2 + nx * 0.5, (q0[1] + q1[1]) / 2 + ny * 0.5)
            if m[1] < 82.1 or any(paved((m[0] + (q1[0] - q0[0]) * t, m[1] + (q1[1] - q0[1]) * t), p['id']) for t in (-0.45, 0, 0.45)):
                if len(run) > 1: runs.append(run)
                run = []; continue
            if not run: run = [q0]
            run.append(q1)
    if len(run) > 1:
        if runs and math.dist(run[-1], runs[0][0]) < 1e-6: runs[0] = run[:-1] + runs[0]
        else: runs.append(run)
    p['runs'] = runs
    if not p['id'].startswith('PAD-'): continue
    # the blue edge lights: 75 cm out, 20 m apart on the straights, closer round the curves and throats
    for r in runs:
        left = 1.0
        for k in range(len(r) - 1):
            a, b = r[k], r[k + 1]; D = math.dist(a, b); nx, ny = (b[1] - a[1]) / D, -(b[0] - a[0]) / D; left -= D
            if left <= 0:
                q = (b[0] + nx * 0.75, b[1] + ny * 0.75); step = 20.0 if D > 2.0 else 9.0
                if not paved(q) and all(math.dist(q, o[:2]) > 6 for o in lights): lights.append((q[0], q[1], (b[0] - a[0]) / D, (b[1] - a[1]) / D)); left = step
                else: left = 1.0
# #endregion
# #region ── Paint, texts, signs ──
paint, texts = [], []
def ln(mat, w, up, *pts): paint.append((mat, w, up, [c for p in pts for c in p]))
def boxl(mat, w, up, x0, y0, x1, y1): ln(mat, w, up, (x0, y0), (x1, y0)); ln(mat, w, up, (x0, y1), (x1, y1)); ln(mat, w, up, (x0, y0 + w / 2), (x0, y1 - w / 2)); ln(mat, w, up, (x1, y0 + w / 2), (x1, y1 - w / 2))
def tx(t, x, y, h, yaw, c='yellow'): texts.append((t, x, y, h, yaw, c))
def dash(mat, w, up, a, b, on, off):
    D = math.dist(a, b); t = 0.0
    while t < D - 0.2:
        e = min(D, t + on); ln(mat, w, up, *[(a[0] + (b[0] - a[0]) * v / D, a[1] + (b[1] - a[1]) * v / D) for v in (t, e)]); t += on + off
NARROW = (('E190', 3.6), ('B737', 4.4), ('A319 A320 A321', 5.1)); WIDE = (('B767', 5.6), ('A330 A350 B787', 6.5), ('B777', 7.6))
def lead(x, nose, types, w, y0=TY, r=30.0):
    ln('paintyellow', w, 0.008, (x, y0), (x, nose))
    for s in (-1, 1): ln('paintyellow', w, 0.008, *[(x + s * (r - r * math.cos(math.radians(90 * k / 20))), y0 + r - r * math.sin(math.radians(90 * k / 20))) for k in range(21)])
    for k, (t, g) in enumerate(types):
        h = 3.0 if k == len(types) - 1 else 1.5; y = nose - g
        ln('paintyellow', 0.3, 0.0095, (x - h, y), (x + h, y)); tx(t, x + h + 0.4 + 0.16 * len(t), y, 0.45, -90)
def edge(s, k): pd = next(q for q in PADS if q[1] < s['x'] < q[2]); return (pd[2] - s['x'] if k > 0 else s['x'] - pd[1]) - SH - 0.69
for s in STANDS:
    x, n, sp, tail, big = s['x'], s['nose'], s['span'], s['nose'] - s['len'], s['code'] == 'E'; num = s['id'][6:]
    lead(x, n, WIDE if big else NARROW, 0.3)
    if s.get('flex'):   # the two code C places on it, their lines dashed-in from the pad's own throat
        for k, fx in enumerate(s['flex']): lead(fx, 318.0, NARROW, 0.15); tx(num + 'LR'[k], fx - 3.2, 251.5, 1.5, -90)
    # each side to the middle of the gap to a neighbour on the same pad
    room = {k: min([1e9] + [(abs(o['x'] - x) + sp / 2 - o['span'] / 2) / 2 for o in STANDS if (o['x'] - x) * k > 0]) for k in (-1, 1)}
    e = {k: min(sp / 2 + 3, room[k] - 0.35) for k in (-1, 1)}
    boxl('paintred', 0.25, 0.0075, x - e[-1], tail - 3, x + e[1], n + 2)                       # the red restraint line
    w = 2.1 * len(num) + 1.2; ln('paintblack', 4.2, 0.0085, (x - w / 2, tail - 9), (x + w / 2, tail - 9)); boxl('paintyellow', 0.15, 0.0095, x - w / 2 - 0.1, tail - 11.2, x + w / 2 + 0.1, tail - 6.8)
    tx(num, x, tail - 9, 3.0, -90)
    for k in (-1, 1):
        pw = max((q for q in (4.0, 3.0) if e[k] + 0.3 + q <= min(room[k], edge(s, k))), default=0)
        if pw: gx = x + k * (e[k] + 0.3 + pw / 2); boxl('paintwhite', 0.15, 0.0075, gx - pw / 2, tail - 2, gx + pw / 2, tail + 10)   # ground equipment, outside the red line where there is room
# the bus road: edge lines, dashed middle, STOP clear of each link, ladder crossings
m = (RY0 + RY1) / 2
xs = [LEG[1] + 7] + [c + s * LH for c in cs for s in (-1, 1)] + [555.0]
for a, b in zip(xs[::2], xs[1::2]):
    ln('paintwhite', 0.15, 0.008, (a, RY0 + 0.4), (b, RY0 + 0.4)); ln('paintwhite', 0.15, 0.008, (a, RY1 - 0.4), (b, RY1 - 0.4)); dash('paintwhite', 0.12, 0.008, (a, m), (b, m), 3, 6)
for n, (c, clear) in LINK.items():
    for s in (-1, 1):
        sx = c + s * (clear + 0.5); ya, yb = (RY0 + 0.6, m) if s < 0 else (m, RY1 - 0.6)   # (the lane driving towards the link: right-hand traffic)
        ln('paintwhite', 0.5, 0.009, (sx, ya), (sx, yb)); tx('STOP', sx + s * 3.0, (ya + yb) / 2, 1.8, 0 if s < 0 else 180, 'white')
    for k in range(-6, 7): ln('paintwhite', 1.0, 0.0115, (c + 2 * k * 0.95, RY0 + 0.3), (c + 2 * k * 0.95, RY1 - 0.3))
for x in (LEG[0] + 0.4, LEG[1] - 0.4): ln('paintwhite', 0.15, 0.008, (x, 244.0), (x, 328.0)); dash('paintwhite', 0.15, 0.0095, (x, 178.5), (x, 238.0), 2, 2)
dash('paintwhite', 0.12, 0.008, (-245, 245), (-245, 328), 3, 6)
ln('paintwhite', 0.5, 0.009, (-245, 178), (LEG[1] - 0.6, 178)); tx('STOP', -243.4, 175, 1.2, -90, 'white')
ln('paintwhite', 0.5, 0.009, (LEG[0] + 0.6, 258.5), (-245, 258.5)); tx('STOP', -246.6, 261.5, 1.2, 90, 'white')
# the depot's 6 charging bays (14 x 4 m) and the bus gate's 2 bays
for k in range(6):
    bx = -292.0 + k * 5.6; boxl('paintwhite', 0.15, 0.0075, bx - 2, 286, bx + 2, 300); tx(str(k + 1), bx, 283.8, 1.2, -90, 'white')
tx('BUS DEPOT', -276, 250, 1.6, -90, 'white')
for k, bx in enumerate((-282.0, -273.0)):
    boxl('paintwhite', 0.15, 0.0075, bx - 2, 88, bx + 2, 104); tx('BUS %d' % (k + 1), bx, 106.5, 1.2, 90, 'white')
# the stand signs: yellow figures on black boards on legs, on the grass behind the bus road, facing the pilot
signs = [(s['id'][6:] + (' L-R' if s.get('flex') else ''), s['x'], 345.0) for s in STANDS]
# #endregion
# #region ── Write ──
r2 = lambda v: round(v + 0.0, 3)
fl = lambda P: [r2(c) for p in P for c in p]
rem = dict(note='GENERATED by Tools/make_remote_stands.py (proved by Tools/check_remote_stands.py): the remote stands R0-R7, the bus road, the depot and the bus bays as drawn (AirfieldRemote.cpp). polys: outer = outline counter-clockwise, pts + tri = the surface, ring = paved shoulder between outer and pts, runs = the edges that meet the grass; paint: material, width, lift, points',
    built=bool((d.get('remote') or {}).get('built', False)), linkX=cs, linkHalf=LH, rampTo=30.0, polys=[dict(id=p['id'], mat=p['mat'], ring=p['ring'], outer=fl(p['outer']), pts=fl(p['pts']) if p['pts'] is not p['outer'] else [], tri=[i for t in p['tri'] for i in t], runs=[fl(q) for q in p['runs']]) for p in polys],
    paint=[[a, b, c, [r2(v) for v in q]] for a, b, c, q in paint], texts=[[t, r2(x), r2(y), h, yaw, c] for t, x, y, h, yaw, c in texts],
    lights=[[r2(v) for v in q] for q in lights], signs=[[t, x, y] for t, x, y in signs], masts=[list(q) for q in MASTS])
d['stands'] = [s for s in d['stands'] if not s.get('remote')] + STANDS; d.pop('remote', None)
d['standsNote'] = d['standsNote'].split('; 2026-11-25:')[0] + '; 2026-11-25: the remote stands R0-R7 (to-do 277) with "remote": true, nose north, tail to taxilane T; "built": false = drawn only, every reader of the stands skips it'
txt = json.dumps(d, indent=1, ensure_ascii=True)
rt = '{\n' + ',\n'.join('  %s: %s' % (json.dumps(k), json.dumps(v, separators=(',', ':')) if not isinstance(v, list) or k in ('linkX',) else '[\n   ' + ',\n   '.join(json.dumps(q, separators=(',', ':')) for q in v) + '\n  ]') for k, v in rem.items()) + '\n }'
txt = txt.rstrip()[:-1].rstrip() + ',\n "remote": ' + rt + '\n}'
json.loads(txt); open(OUT, 'w', encoding='utf-8', newline='\n').write(txt + '\n')
print('polys', len(polys), 'paint', len(paint), 'texts', len(texts), 'lights', len(lights), 'bytes', len(txt))
# #endregion
