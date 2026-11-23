# The Grand Hall's check-in, every desk and every gap between two desks by numbers (owner, 2026-11-23, to-do 225): the
# layout's desks, chairs, banners, barriers and bag drops with the spawned weigh belts and hatches (DepartureBelts' places
# and parts): what stands inside another part, what lies on another part's face (flicker), what floats, how wide the gaps
# are; and the passengers' ways at the desks (the queue, the way out) clear of everything.
#   python Tools/check_checkin.py [layout.json] [-fights]     (-fights: Tools/find_zfight.py on the whole scene as well)
import json, os, re, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terminal_where as TW, find_zfight as ZF
LAY = next((a for a in sys.argv[1:] if a.endswith('.json')), TW.D('terminal-layout.json'))
SRC = open(os.path.join(TW.ROOT, 'Source', 'AirportDynastyPro', 'Private', 'DepartureBelts.cpp'), encoding='utf-8').read()
for s in ('ISLX[] = {47.5f, 74.5f}', 'BELT_DX = 0.78f', 'TEXT("SM_WeighBelt"), FVector(BX, 0, 15.95f)', 'TEXT("SM_DeskHatch"), FVector(BX, 0, 18.1f)'):
    assert s in SRC, 'DepartureBelts moved: put its places in here (%s)' % s
DX = [x + 1.05 + k * 2.08 for i, x in enumerate((47.5, 74.5)) for k in range(12) if k not in ((6, 7) if i == 0 else (1, 2))]
P = []   # (owner, piece, box)
for k, path, b in TW.boxes(LAY):
    m = re.search(r'(F1-CHECKIN/[A-Z]+-[0-9A-Z-]+|F1-HALL/OFFICE-CHAIR-\d+|F1-HALL/QUEUE-POST-\d+|F1-BAGDROP/POINT-\d+)/(.*)$', path)
    if m: P.append((m.group(1).split('/', 1)[1], m.group(2) + '@%d' % k, b))
def part(name):
    js, bn = ZF.load_glb(os.path.join(TW.ROOT, 'Saved', 'ZFight', 'DepBaggage', name + '.glb'))
    return ZF.parts(js, bn)
BELTS = []
for d, x in enumerate(DX, 1):
    for nm, z0 in (('SM_WeighBelt', 15.95), ('SM_DeskHatch', 18.1)):
        for n, t in part(nm):
            # (glb: x east, y up, -z the Blender y; the game: Blender y towards the airside)
            w = np.stack([x + 0.78 + t[..., 0], t[..., 1], z0 - t[..., 2]], axis=-1)
            f = w.reshape(-1, 3); lo, hi = f.min(0), f.max(0)
            BELTS.append(('%s %d' % ('BELT' if 'Weigh' in nm else 'HATCH', d), n, (lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]), w))
P += [(o, n, b) for o, n, b, _ in BELTS]
def grp(o): return 'B' + o.split()[1] if o.startswith(('BELT', 'HATCH')) else o
bad = 0
def core(o, n, b): return o.startswith('DESK') and b[2] > 16.85 and 0.4 < b[1] < 0.5      # (the steel cores stand inside the belts' side panels, by design)
for i in range(len(P)):
    oa, na, a = P[i]
    for j in range(i + 1, len(P)):
        ob, nb, b = P[j]
        if grp(oa) == grp(ob) or oa[:5] == ob[:5] == 'QUEUE' or oa[:6] == ob[:6] == 'BANNER': continue
        ov = [min(a[k + 3], b[k + 3]) - max(a[k], b[k]) for k in range(3)]
        if min(ov) > 0.0005:
            if core(*P[i]) != core(*P[j]): continue
            bad += 1; print('INSIDE  %-18s %-18s in %-18s %-18s %s' % (oa, na, ob, nb, ' x '.join('%.3f' % v for v in ov)))
        elif min(ov) > -0.0008 and sorted(ov)[1] > 0.02: print('TOUCH   %-18s %-18s at %-18s %-18s' % (oa, na, ob, nb))
for o in sorted({p[0] for p in P}):
    lo = min(p[2][1] for p in P if p[0] == o)
    if lo > 0.035 and 'BANNER' not in o: bad += 1; print('FLOATS  %s starts %.3f over the floor' % (o, lo))
body = {int(o[-2:]): b for o, n, b in P if o.startswith('DESK') and b[4] - b[1] > 0.9}
for d in range(1, 21):
    bx = [p[2] for p in P if p[0] == 'BELT %d' % d]; lo = min(b[0] for b in bx); hi = max(b[3] for b in bx); nx = body.get(d + 1)
    nxg = '%.1f cm' % ((nx[0] - hi) * 100) if nx and nx[0] - hi < 1.2 else 'open'
    print('gap %2d: desk %.3f..%.3f  belt %.3f..%.3f (%.1f cm clear of its desk)  next desk: %s' % (d, body[d][0], body[d][3], lo, hi, (lo - body[d][3]) * 100, nxg))
# the passengers: the queue south of each belt, the step aside and the way out beside it (PaxFlow WayToSecurity), 0.3 m round
for d, x in enumerate(DX, 1):
    qx = x + 0.78; S = 1 if (62.35 if qx < 73 else 78.95) > qx else -1
    way = [(qx, z) for z in np.arange(15.35, 9.0, -0.05)] + [(qx + S * 1.04 * t, 15.35 - 0.95 * t) for t in np.arange(0, 1, 0.05)] + [(qx + S * 1.04, z) for z in np.arange(14.4, 9.8, -0.05)]
    for o, n, b in P:
        if b[1] > 1.8 or o.startswith('BELT'): continue
        c = min(np.hypot(max(b[0] - a, a - b[3], 0), max(b[2] - z, z - b[5], 0)) for a, z in way)
        if c < 0.3: bad += 1; print('WAY     desk %d: %s %s %.2f m off' % (d, o, n, c))
print('%d pieces, %d faults' % (len(P), bad))
if '-fights' in sys.argv:
    buf = b''; J = {'asset': {'version': '2.0'}, 'scene': 0, 'scenes': [{'nodes': []}], 'nodes': [], 'meshes': [], 'accessors': [], 'bufferViews': []}
    def put(name, tri):
        global buf
        v = tri.reshape(-1, 3).astype(np.float32).tobytes()
        J['bufferViews'].append({'buffer': 0, 'byteOffset': len(buf), 'byteLength': len(v)}); buf += v
        J['accessors'].append({'bufferView': len(J['bufferViews']) - 1, 'componentType': 5126, 'count': tri.size // 3, 'type': 'VEC3'})
        J['meshes'].append({'primitives': [{'attributes': {'POSITION': len(J['accessors']) - 1}}]})
        J['nodes'].append({'name': name, 'mesh': len(J['meshes']) - 1}); J['scenes'][0]['nodes'].append(len(J['nodes']) - 1)
    Q = [(0, 1, 3), (0, 3, 2), (4, 6, 7), (4, 7, 5), (0, 4, 5), (0, 5, 1), (2, 3, 7), (2, 7, 6), (0, 2, 6), (0, 6, 4), (1, 5, 7), (1, 7, 3)]
    for o, n, b in P:
        if o.startswith(('BELT', 'HATCH')): continue
        c = np.array([[b[3 if i & 4 else 0], b[4 if i & 2 else 1], b[5 if i & 1 else 2]] for i in range(8)])
        put(o + '|' + n, np.array([[c[i] for i in q] for q in Q]))
    for o, n, b, w in BELTS: put(o + '|' + n, w)
    J['buffers'] = [{'byteLength': len(buf)}]
    js = json.dumps(J).encode(); js += b' ' * (-len(js) % 4); buf += b'\0' * (-len(buf) % 4)
    f = os.path.join(TW.ROOT, 'Saved', 'ZFight', 'checkin.glb')
    with open(f, 'wb') as fh:
        fh.write(struct.pack('<4sIIII', b'glTF', 2, 20 + len(js) + 8 + len(buf), len(js), 0x4E4F534A) + js + struct.pack('<II', len(buf), 0x004E4942) + buf)
    ZF.main(f, f[:-4] + '.json')
sys.exit(1 if bad else 0)
