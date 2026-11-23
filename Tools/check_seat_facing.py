# Which way do people sit? (to-do 233, owner 2026-11-23: passengers on the blue chairs the wrong way round.)
# Reads the seat's own model and the layout, places every seat with the game's own formulas (BabylonModel.cpp Place,
# ReplaceSeats and SeatSpots; People.cpp SitPose; Seats.cpp Sit) and prints, per kind of seat: the side its backrest is
# on (from the geometry), the way a sitter faces, OK or FLIPPED - now, and for the AI people before the fix.
#   python Tools/check_seat_facing.py
import json, math, os, struct, re
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---- vectors and quaternions (Unreal's: child then parent) ----
def qrot(q, v):
    x, y, z, w = q; tx, ty, tz = 2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0])
    return (v[0] + w * tx + y * tz - z * ty, v[1] + w * ty + z * tx - x * tz, v[2] + w * tz + x * ty - y * tx)
def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)
def point(t, p): q, o, s = t; r = qrot(q, (p[0] * s[0], p[1] * s[1], p[2] * s[2])); return (r[0] + o[0], r[1] + o[1], r[2] + o[2])
def compose(c, p): return (qmul(p[0], c[0]), point(p, c[1]), tuple(c[2][i] * p[2][i] for i in range(3)))
def yawq(deg): a = math.radians(deg) / 2; return (0, 0, math.sin(a), math.cos(a))
ONE = ((0, 0, 0, 1), (0, 0, 0), (1, 1, 1))
def flat(v): n = math.hypot(v[0], v[1]) or 1; return (v[0] / n, v[1] / n)
def dot(a, b): return a[0] * b[0] + a[1] * b[1]
def deg(v): return (math.degrees(math.atan2(v[1], v[0])) + 360.0 + 1e-6) % 360

# ---- 1: the seat in its own space, as imported: glTF (x, y, z) -> Unreal (x, z, y) x 100 ----
g = open(os.path.join(REPO, 'Art/GateSeat/gateseat.glb'), 'rb').read(); n = struct.unpack_from('<I', g, 12)[0]; J = json.loads(g[20:20 + n]); B0 = 28 + n
pts = []
for m in J['meshes']:
    if m['name'] != 'SeatModule': continue
    for p in m['primitives']:
        if J['materials'][p['material']]['name'] != 'Shell': continue
        a = J['accessors'][p['attributes']['POSITION']]; bv = J['bufferViews'][a['bufferView']]
        o = B0 + bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        for i in range(a['count']): x, y, z = struct.unpack_from('<3f', g, o + i * 12); pts.append((x * 100, z * 100, y * 100))
hi = [p for p in pts if p[2] > 60]; pan = [p for p in pts if p[2] < 46]
back_y = sum(p[1] for p in hi) / len(hi); pan_y = sum(p[1] for p in pan) / len(pan)
BACK = -1 if back_y < pan_y else 1     # the side of its own Y the backrest is on
print('1. the seat itself (Unreal axes): backrest (over 60 cm) at Y %.1f, pan at Y %.1f -> backrest on its %sY' % (back_y, pan_y, '-' if BACK < 0 else '+'))
# the imported asset's own bounds (min then max): the import kept that side
u = open(os.path.join(REPO, 'Content/Babylon/GateSeat/gateseat/StaticMeshes/SeatModule.uasset'), 'rb').read(); ub = None
for o in range(0, min(len(u) - 48, 40000)):
    v = struct.unpack_from('<6d', u, o)
    if abs(v[0] + 31.1) < .7 and abs(v[3] - 31.1) < .7 and abs(v[4] - v[1] - 68.3) < 1 and 25 < v[2] < 35: ub = v; break
if ub: print('   the imported asset: Y %.1f .. %.1f -> backrest (the side reaching further) on its %sY' % (ub[1], ub[4], '-' if -ub[1] > ub[4] else '+')); assert (-1 if -ub[1] > ub[4] else 1) == BACK
else: print('   the imported asset: its bounds not found (not checked)')

# ---- 2: every seat placed ----
L = json.load(open(os.path.join(REPO, 'Data/terminal-layout.json'))); N = L['nodes']; W = {}
def local(nd):
    t = nd.get('translation', (0, 0, 0)); r = nd.get('rotation', (0, 0, 0, 1)); s = nd.get('scale', (1, 1, 1))
    return ((-r[0], -r[2], -r[1], r[3]), (t[0] * 100, t[2] * 100, t[1] * 100), (s[0], s[2], s[1]))
st = [(r, ONE) for r in L['roots']]
while st:
    i, p = st.pop(); W[i] = compose(local(N[i]), p)
    for k in N[i].get('children', ()): st.append((k, W[i]))
ROOT = (yawq(180), (0, 0, 0), (1, 1, 1))   # (Place: the model's half turn; the terminal's actor unturned)
def world(i): return compose(W[i], ROOT)
inst = []; spots = []; mirrored = 0
for i in W:
    nm = N[i].get('name', '').upper(); last = nm.split('/')[-1]
    if not (last.startswith('SEATS-') and 'children' in N[i]): continue
    kids = N[i]['children']; ks = [(k, W[k][1][2] - W[i][1][2]) for k in kids]
    backs = [k for k, z in ks if abs(z - 72) <= 3]; pans = [k for k, z in ks if abs(z - 44) <= 3]
    num = sum(1 for k in kids if abs(N[k].get('translation', (0, 0, 0))[1] - .72) <= .01)
    if not num: continue
    if min(W[i][2]) < 0: mirrored += 1
    arc = N[i].get('arc', 0); row = world(i)
    kind = 'curved window' if arc > 1 else re.sub(r'-\d+[A-Z]?$', '', nm.split('/')[0])
    for q in range(num):
        bx = -num * .62 / 2 + .62 * (q + .5)
        if arc > 1: A = bx / arc; at = (yawq(math.degrees(A)), (-arc * math.sin(A) * 100, arc * (math.cos(A) - 1) * 100, 0), (1, 1, 1))
        else: at = ((0, 0, 0, 1), (-bx * 100, 0, 0), (1, 1, 1))
        inst.append(compose(at, row))
    # (SeatSpots: the row's places about its middle, as its seats stand)
    mid = sum(N[k]['translation'][0] for k in pans) / len(pans) * 100 if pans else 0
    dx, dy, _ = qrot(row[0], (-mid if abs(mid) > 20 else 0, 0, 0))
    for b in backs:
        bw = world(b)[1]; pn = min(pans, key=lambda k: math.dist(world(k)[1][:2], bw[:2]), default=None)
        if pn is None: continue
        pw = world(pn)[1]; spots.append((kind, nm, (pw[0] + dx, pw[1] + dy, row[1][2] + 46), flat((pw[0] - bw[0], pw[1] - bw[1]))))
def front(t): return flat(qrot(t[0], (0, -BACK * t[2][1], 0)))
grid = {}
for t in inst: grid.setdefault((int(t[1][0] // 100), int(t[1][1] // 100)), []).append(t)
K = {}
for kind, nm, at, f in spots:
    gx, gy = int(at[0] // 100), int(at[1] // 100)
    c = [t for dx in (-1, 0, 1) for dy in (-1, 0, 1) for t in grid.get((gx + dx, gy + dy), ()) if math.dist(t[1][:2], at[:2]) < 80]
    r = K.setdefault(kind, dict(n=0, none=0, lay=0, flip=0, side=0, ok=0, near=0, floor=0, far=0., ex=None)); r['n'] += 1
    same = [t for t in c if abs(t[1][2] + 46 - at[2]) < 30]
    if not same: r['none'] += 1; continue
    t = min(same, key=lambda t: math.dist(t[1][:2], at[:2])); fr = front(t)
    t2 = min(c, key=lambda t: math.dist(t[1][:2], at[:2]))        # (SeatSpots in cc891058: by the plan only, any floor)
    if dot(front(t2), fr) < .7: r['near'] += 1
    if abs(t2[1][2] + 46 - at[2]) >= 30: r['floor'] += 1
    r['far'] = max(r['far'], math.dist(t[1][:2], at[:2]))
    if dot(f, fr) < .7: r['lay'] += 1
    # the AI before cc891058: SitPose turned the body ONCE, to its frame's turn BEFORE the frame was put at the seat - a
    # fresh frame, unturned: the body (its own forward is +Y) faced the world's +Y on every seat
    d = dot((0, 1), fr); r['flip' if d < -.7 else 'side' if d < .7 else 'ok'] += 1
    if r['ex'] is None or (d < -.7 and r['exd'] >= -.7): r['ex'] = (nm, at, fr, f); r['exd'] = d
print('2. mirrored rows (negative scale): %d; seats placed %d, places to sit %d' % (mirrored, len(inst), len(spots)))
print('3./4./5. per kind (world yaw, degrees; the player and the AI now both turn to the SITTER column)')
print('%-17s %5s %8s %7s %-9s | %-36s | %s' % ('kind', 'seats', 'backrest', 'sitter', 'now', 'AI before cc891058 (all faced 90)', 'example'))
bad = 0
for k in sorted(K):
    r = K[k]
    if not r['ex']: print('%-17s %5d  no seat found' % (k, r['n'])); continue
    nm, at, fr, f = r['ex']
    now = 'OK' if not (r['lay'] or r['near'] or r['none']) else 'FLIPPED layout %d, nearest %d, none %d' % (r['lay'], r['near'], r['none'])
    bad += r['lay'] + r['near']
    print('%-17s %5d %8.1f %7.1f %-9s | %4d FLIPPED, %3d sideways, %3d right | %s at %.0f, %.0f, %.0f (module %.0f cm from the point at most; %d matched another floor\'s)' % (k, r['n'], deg((-fr[0], -fr[1])), deg(f), now, r['flip'], r['side'], r['ok'], nm, *at, r['far'], r['floor']))
t = {x: sum(r[x] for r in K.values()) for x in ('n', 'flip', 'side', 'ok')}
print('all rows: %d seats; before: %d FLIPPED, %d sideways, %d right by luck' % (t['n'], t['flip'], t['side'], t['ok']))

# ---- chairs, armchairs, sofas: SeatSpots' rule on their boxes (unit boxes scaled) ----
C = {}
for i in W:
    last = N[i].get('name', '').upper().split('/')[-1]
    if not any(x in last for x in ('CHAIR', 'STOOL', 'BENCH', 'SOFA')) or not N[i].get('children'): continue
    ws = [W[k] for k in N[i]['children'] if 'mesh' in N[k]]
    if not ws: continue
    fl = min(w[1][2] - 50 * abs(w[2][2]) for w in ws); top = []; lo = [1e9, 1e9]; h = [-1e9, -1e9]
    for w in ws:
        for a in (-1, 1):
            for b in (-1, 1):
                c = point(w, (a * 50, b * 50, 0))
                for d in (0, 1): lo[d] = min(lo[d], c[d]); h[d] = max(h[d], c[d])
        zc, hz = w[1][2], 50 * abs(w[2][2])
        if zc + hz > fl + 46 + 18 and zc - hz > fl + 46 - 15: top.append(w[1])
    r = C.setdefault(re.sub(r'[-_\d]+', '', last), [0, 0, 0, 0]); r[0] += 1
    if not top or 'STOOL' in last: r[1] += 1; continue
    b = (sum(q[0] for q in top) / len(top), sum(q[1] for q in top) / len(top)); f = flat(((lo[0] + h[0]) / 2 - b[0], (lo[1] + h[1]) / 2 - b[1]))
    s = min(ws, key=lambda w: abs(w[1][2] - fl - 40) - 20 * w[2][0] * w[2][1])[1]   # (the cushion: low and wide)
    v = (s[0] - b[0], s[1] - b[1]); r[2 if math.hypot(*v) > 3 and dot(f, flat(v)) > .9 else 3] += 1
print('other seats: ' + '; '.join('%s %d (no backrest %d, OK %d, off %d)' % (k, *v) for k, v in sorted(C.items())))
print('RESULT: %s' % ('every row seat is faced away from its backrest' if not bad else '%d seats FLIPPED' % bad))
