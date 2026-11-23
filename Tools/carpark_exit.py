"""The car park's entry and exit at Chablais (owner's OK 2026-11-23: docs/Blueprints/Terminal-Changes/v2/TCv2-2f, OPTION 1 -
a give-way junction at the back onto the road to the town, x 120, 2 entry + 2 exit barrier lanes), to-do 217. Switzerland
only: it changes Data/Sites/chablais/roads.json in place and is safe to run again (it starts from the plain car park every
time); Data/roads.json and Tools/make_roads.py stay the plain car park.

As few spaces lost as possible (the drawing: 224, 3 982 -> 3 758): the lanes' plaza is only as wide as the lanes (9 columns,
not 28), two of the three footways run through the near ten rows only (where the passengers' cars stand), the third through
all 22. Everything keeps the car park's own 2.5 m columns, so the passengers' spawn spaces stay where they were.

    python Tools/carpark_exit.py
"""
import json, math, os, shutil
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
F = os.path.join(ROOT, 'Data', 'Sites', 'chablais', 'roads.json')
W = 2.5
X0, X1, Y0, Y1 = -93.0, 367.0, -247.0, -54.2
COLS = (X0 + 3.0, X1 - 3.0)                       # the rows' ends in the plain car park
END = (-87.5, 361.5)                              # the end lanes' inner edges (5.5 m)
MID = (115.0, 122.5)                              # the middle lane
PLAZA = (107.5, 130.0)                            # the barrier lanes' columns in the last double row
WALKS = ((47.5, 50.0, False), (72.5, 75.0, False), (100.0, 102.5, True))   # the footways' columns; True: all 22 rows
ROAD_N = -250.75                                  # the town road's north edge (its shoulder's)


def plain_rows():
    rows, y = [], round(-46.75 - 3.5 - 1.0 - 3.0 - 0.2, 2) - 6.5
    while y - 10.0 > Y0 + 6.0:
        rows += [dict(y=round(y, 2), d=5.0, dir=-1), dict(y=round(y - 10.0, 2), d=4.9, dir=1)]
        y -= 16.5
    return rows


def rect(x0, y0, x1, y1, color='white'):
    return dict(pts=[[round(a, 3) for a in p] for p in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))], color=color)


def arrow(x, y, up):
    """A lane arrow 4 m long, its point toward up (+1 north, -1 south)."""
    s = up
    return [rect(x - 0.08, min(y, y + s * 2.4), x + 0.08, max(y, y + s * 2.4)),
            dict(pts=[[x - 0.4, round(y + s * 2.4, 2)], [x + 0.4, round(y + s * 2.4, 2)], [x, round(y + s * 4.0, 2)]], color='white')]


def build():
    R = json.load(open(F, encoding='utf-8'))
    rows = plain_rows(); n_plain = sum(int((COLS[1] - COLS[0]) / W) for _ in rows)
    lot = dict(x0=X0, x1=X1, y0=Y0, y1=Y1, lift=0.03, rows=[], polys=[], props=[])
    P, Q, n = lot['polys'], lot['props'], 0
    # ---- the spaces: the plain car park's columns less the lanes, the footways and the plaza
    for i, r in enumerate(rows):
        gaps = [(COLS[0], END[0]), MID, (END[1], COLS[1])] + [w[:2] for w in WALKS if w[2] or r['y'] > -140.0]
        if i >= len(rows) - 2: gaps.append(PLAZA)
        segs, x = [], COLS[0]
        last = COLS[0] + W * int((COLS[1] - COLS[0]) / W)
        while x < last - 1e-6:
            free = not any(x < g1 - 1e-6 and x + W > g0 + 1e-6 for g0, g1 in gaps)
            if free: segs.append([x, x + W]) if not segs or abs(segs[-1][1] - x) > 1e-6 else segs[-1].__setitem__(1, x + W); n += 1
            x += W
        # (the near rows first in the file as before: PaxFlow and the walking map read them by their y)
        lot['rows'] += [dict(x0=round(a, 2), x1=round(b, 2), y=r['y'], d=r['d'], dir=r['dir'], w=W) for a, b in segs]
    # ---- the footways (Swiss: yellow): an edge line along each side between the cars, yellow zebra bars over each aisle
    ys = [r['y'] for r in rows]
    for a, b, full in WALKS:
        k = 22 if full else 10
        for i in range(0, k, 2):
            top, bot = ys[i], ys[i + 1]
            P += [rect(a + 0.1, bot, a + 0.22, top, 'yellow'), rect(b - 0.22, bot, b - 0.1, top, 'yellow')]
            hi = Y1 - 0.25 if i == 0 else ys[i - 1]         # (the aisle in front of this double row)
            m = int((hi - top) / 1.0); s = top + ((hi - top) - (m - 0.5)) / 2
            P += [rect(a, s + j, b, s + j + 0.5, 'yellow') for j in range(m)]
    # ---- the middle lane: its broken centre line and an arrow each way at the plaza's end
    mx = (MID[0] + MID[1]) / 2
    y = -225.0
    while y < -62.0: P.append(rect(mx - 0.06, y, mx + 0.06, y + 1.5)); y += 4.5
    P += arrow(MID[0] + 1.9, -205.0, -1) + arrow(MID[1] - 1.9, -209.0, 1)
    # ---- the barrier lanes: 2 out (x 109 - 116.3), 2 in (x 120.2 - 127.5)
    for k in range(11):
        yk = -240.5 + k
        if k in (0, 10): continue
        Q.append(dict(mesh='cpkerb', x=112.65, y=yk)); Q.append(dict(mesh='cpkerb', x=108.75, y=yk))
    for k in range(11): Q.append(dict(mesh='cpisland', x=117.75, y=-240.5 + k))
    for k in range(6):
        if k in (0, 5): continue
        Q += [dict(mesh='cpkerb', x=x_, y=-235.5 + k) for x_ in (119.95, 123.85, 127.75)]
    for x_, a, b in ((112.65, -240.0, -231.0), (108.75, -240.0, -231.0), (119.95, -235.0, -231.0), (123.85, -235.0, -231.0), (127.75, -235.0, -231.0)):
        Q += [dict(mesh='cpkerbend', x=x_, y=a - 0.25), dict(mesh='cpkerbend', x=x_, y=b + 0.25)]
    # (out: the driver sits on the left - the east - of a car driving south; in: on the west)
    for x_ in (112.65, 116.6): Q += [dict(mesh='cpbarrier', x=x_, y=-236.5, yaw=180, up=0.15), dict(mesh='cpticket', x=x_, y=-234.0, yaw=0, up=0.15)]
    for x_ in (119.95, 123.85): Q += [dict(mesh='cpbarrier', x=x_, y=-231.6, yaw=0, up=0.15), dict(mesh='cpticket', x=x_, y=-234.2, yaw=180, up=0.15)]
    for x_ in (110.75, 114.55): P += arrow(x_, -228.5, -1) + arrow(x_, -241.5, -1)
    for x_ in (121.95, 125.75): P += arrow(x_, -245.5, 1) + arrow(x_, -228.6, 1)[:0]
    # (the lanes shut off from the rows on either side: nobody drives round the barriers)
    for x_, yaw in ((107.95, 0), (129.55, 180)):
        for k in range(10): Q.append(dict(mesh='cprail', x=x_, y=-227.2 - 2 * k, yaw=yaw))
    # ---- the mouth onto the town road: its tarmac, the rounded corners, white give-way teeth, the line between in and out
    mouth = dict(x0=PLAZA[0], x1=PLAZA[1], y0=ROAD_N - 0.05, y1=Y0, lift=0.03, rows=[], polys=[], props=[], note='the car park\'s mouth (Tools/carpark_exit.py)')
    MP, MQ = mouth['polys'], mouth['props']
    x = 109.2
    while x + 0.6 <= 116.2: MP.append(dict(pts=[[round(x, 2), -250.45], [round(x + 0.6, 2), -250.45], [round(x + 0.3, 2), -249.55]], color='white')); x += 0.9
    MP += [rect(118.2, -250.4, 118.4, Y0), rect(107.6, -250.4, 107.72, Y0), rect(129.78, -250.4, 129.9, Y0)]
    P += [rect(118.2, Y0, 118.4, -241.2), rect(112.55, -246.5, 112.75, -241.3), rect(123.75, -246.5, 123.95, -236.4)]
    # (the give-way sign faces the cars coming out - north; the P sign the cars on the road, from the west and from the east)
    MQ += [dict(mesh='cpgiveway', x=107.0, y=-249.6, yaw=0), dict(mesh='cpsign', x=130.7, y=-249.4, yaw=-90), dict(mesh='cpsign', x=106.2, y=-248.6, yaw=90)]
    # ---- ten pay machines on the car park's pavement by the footways' heads, their faces to the pavement, clear of the lamps
    for x_ in (45.2, 46.1, 51.6, 52.5, 71.0, 76.8, 77.7, 103.6, 104.5, 105.4): Q.append(dict(mesh='cppay', x=x_, y=-53.95, yaw=0, on='pavement'))
    R['lots'] = [lot] + [l for l in R['lots'][1:] if 'mouth' not in l.get('note', '')] + [mouth]
    fills = [f for f in R.get('fills', []) if f.get('note') != 'car park']
    for cx, sg in ((PLAZA[0], -1), (PLAZA[1], 1)):
        r = 3.5
        arc = [[round(cx + sg * (r - r * math.sin(t)), 3), round(ROAD_N + (r - r * math.cos(t)), 3)] for t in [math.pi / 2 * k / 10 for k in range(11)]]
        fills.append(dict(pts=[[cx - sg * 0.05, ROAD_N - 0.05]] + arc, lift=0.03, note='car park'))
    R['fills'] = fills
    # ---- the town road (Swiss): its centre line white, its edge line on the car park's side left open over the mouth
    bt = next(r for r in R['roads'] if r['id'] == 'AIRPORT-BACK-TO-TOWN')
    import bisect
    L, pts = [0.0], bt['pts']
    for a, b in zip(pts, pts[1:]): L.append(L[-1] + math.dist(a, b))
    k = next(i for i in range(len(pts) - 1) if pts[i][0] <= 118 <= pts[i + 1][0] and abs(pts[i][1] + 255) < 1e-6)
    at = lambda x_: L[k] + (x_ - pts[k][0])
    paint = []
    for p in bt['paint']:
        if p.get('kind') == 'dash' and p.get('off') == 0.0: p['color'] = 'white'
        if p.get('note') == 'mouth': continue
        if p.get('kind') == 'line' and p.get('side') == 'left' and p.get('edge') == 1:
            whole = bt.setdefault('edgeTo', p['to'])
            paint += [dict(p, **{'from': 0.0, 'to': round(at(PLAZA[0] - 3.5), 2)}), dict(p, **{'from': round(at(PLAZA[1] + 3.5), 2), 'to': whole}, note='mouth')]
            p = None
        if p: paint.append(p)
    bt['paint'] = paint
    # ---- Swiss paint: the zebra crossings yellow
    for rd in R['roads']:
        for c in rd.get('crossings', []): c['color'] = 'yellow'
    s = json.dumps(R, ensure_ascii=False)
    open(F, 'w', encoding='utf-8', newline='\n').write(s)
    C = os.path.join(ROOT, 'Content', 'Data', 'Sites', 'chablais')
    if os.path.isdir(C): shutil.copyfile(F, os.path.join(C, 'roads.json'))
    print('car park: %d spaces (%d before, %d less), %d fittings, %d markings; mouth at road metre %.1f' % (n, n_plain, n_plain - n, len(Q) + len(MQ), len(P) + len(MP), at(118.75)))


build()
