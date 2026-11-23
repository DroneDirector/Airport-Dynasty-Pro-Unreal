# The gates' curved windows: the automatic overlap check (Tools/check_east.py's, on the apron side): the live layout laid out in
# the plan's metres with the jetways (F:/ADP-Export/jetways.glb by Data/babylon-jetways.json) and the curved models, the gate's
# new lamps and seats, against everything else.
#   1 flicker, 2 cuts (as check_east.py; pieces cut by Data/terminal-cuts.json are cut here too)
#   3 routes    every jetway's whole swing and rotunda, tug and GPU boxes, light masts, the aircraft's noses, the departure bag
#               trains, the roller door, columns - the smallest distance to the curve, per gate, worked out from the numbers
#   F:/Tools/Blender-4.2/blender.exe -b --python Tools/check_gate_arc.py     (GATEARC_LAYOUT: another layout)
import json, math, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(ROOT, 'Art', 'GateArc'))
import gatearc_geom as G
src = open(os.path.join(ROOT, 'Tools', 'check_east.py'), encoding='utf-8').read()
src = src[:src.index('# ---- 3 routes')].replace("for d in ('TerminalEast', 'TerminalAnnex')", "for d in ('TerminalEast', 'TerminalAnnex', 'GateArc')")
src = re.sub(r'REG = .*', "REG = (-245.0, 560.0, 66.0, 100.0)", src); src = re.sub(r"NEW = \((.|\n)*?\)\n", "NEW = ('F2-GATE-@',)\n", src, count=1)
if 'GATEARC_LAYOUT' in os.environ: os.environ['EAST_LAYOUT'] = os.environ['GATEARC_LAYOUT']
src = src.replace("new = new or n['name'].startswith(NEW)", "new = new or bool(re.match(r'F2-GATE-\d+/(ARC-|LAMP-0(2[5-9]|30)|SEATS-00[45])', n['name']))")
src = src.replace("V = g[0] @ W[:3, :3].T + W[:3, 3]; V = np.stack([-V[:, 0], V[:, 1], V[:, 2]], 1)", "V = g[0] @ W[:3, :3].T + W[:3, 3]; V = np.stack([-V[:, 0], V[:, 1], V[:, 2]], 1); CUTS(i, n, V, g)")
src = src.replace("T = g[1]; c = V[T]", "T = CUT.pop('T', g[1]); c = V[T]")
src = src.replace("news = [o for o in OBJ if o[1]]", "JETS(); news = [o for o in OBJ if o[1]]")
# (the glb's four old jetways stand in the layout but not in the game: the game has jetways.glb)
src = src.replace("    if n['name'].startswith(SKIP): return", "    if n['name'].startswith(SKIP) or re.match(r'J-[1-4]$', n['name']) or '/SIGN-001' in n['name']: return")
CUT = {}; CB = None


def CUTS(i, n, V, g):
    """the triangles Tools/terminal_cut.py takes out of this piece (the first copy's boxes, moved to this copy)"""
    global CB
    import numpy as np
    if CB is None:
        cj = json.load(open(os.path.join(ROOT, 'Data', 'terminal-cuts.json'))); CB = {}
        nodes = L['nodes']
        for c in cj:
            if 'same_as' not in c: CB[nodes[c['node']]['mesh']] = (c['boxes'], 0.0)
        for c in cj:
            if 'same_as' in c: CB[nodes[c['node']]['mesh']] = (next(q for q in cj if q['node'] == c['same_as'])['boxes'], {0: 0}.get(0))
    if n.get('mesh') in CB and 'asset' not in n:
        bx = CB[n['mesh']][0]; m = V[g[1]].mean(1); keep = np.ones(len(m), bool)
        if n['name'].endswith('/STRUCTURE-plaster') and n['name'].startswith('F2-GATE'):
            keep &= ~((m[:, 1] > 8.2) & (m[:, 2] > 81.8) & (m[:, 2] < 81.99) & (np.abs(m[:, 0] - V[:, 0].min()) > 5) & (np.abs(m[:, 0] - V[:, 0].max()) > 5))
        else:
            for b in bx: keep &= ~((m[:, 0] >= b[0]) & (m[:, 0] <= b[3]) & (m[:, 1] >= b[1]) & (m[:, 1] <= b[4]) & (m[:, 2] >= b[2]) & (m[:, 2] <= b[5]))
        CUT['T'] = g[1][keep]


def JETS():
    import numpy as np, struct
    JL = json.load(open(os.path.join(ROOT, 'Data', 'babylon-jetways.json'))); JN = JL['nodes']
    r = open('F:/ADP-Export/jetways.glb', 'rb').read(); jn = struct.unpack('<I', r[12:16])[0]; J = json.loads(r[20:20 + jn]); B = r[28 + jn:]
    def ac(i):
        a = J['accessors'][i]; bv = J['bufferViews'][a['bufferView']]; o = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[a['type']]; return np.frombuffer(B, {5126: '<f4', 5123: '<u2', 5125: '<u4', 5121: 'u1'}[a['componentType']], a['count'] * nc, o).reshape(a['count'], nc)
    def w(i, M, nm):
        n = JN[i]; W = M @ mat(n); nm = n['name'] if '/' in n['name'] or n['name'].startswith('J-') else nm
        if 'mesh' in n and n['mesh'] < len(J['meshes']):
            for p in J['meshes'][n['mesh']]['primitives']:
                V = ac(p['attributes']['POSITION']).astype(float) @ W[:3, :3].T + W[:3, 3]; V = np.stack([-V[:, 0], V[:, 1], V[:, 2]], 1); T = ac(p['indices']).astype(int).reshape(-1, 3)
                c = V[T]; k = c.min(1)[:, 2] < 88
                if k.any(): OBJ.append(('JET ' + nm, False, V, T[k]))
        for c in n.get('children', []): w(c, W, nm)
    for q in JL['roots']: w(q, np.eye(4), '')


exec(compile(src, 'check_east.py', 'exec'))
out = [[a, b, v[0], round(v[1], 3), v[2]] for (a, b), v in cuts.items()]
print('CHECK 1 flicker: %d pairs' % len(flick))
for (a, b), v in sorted(flick.items(), key=lambda q: -q[1][0])[:40]: print('   %-34s %-44s %.4f m2 %s' % (a[:34], b[:44], v[0], v[1][:2]))
print('CHECK 2 cuts: %d pairs' % len(out))
gen = {}
for a, b, n, d, p in out:
    key = (re.sub(r'\d+', '#', a), re.sub(r'\d+', '#', b)); e = gen.setdefault(key, [0, 0.0, p]); e[0] += 1; e[1] = max(e[1], d)
for (a, b), v in sorted(gen.items(), key=lambda q: -q[1][1]): print('   %-32s %-40s x%d, %.3f m %s' % (a[:32], b[:40], v[0], v[1], v[2][:1]))
# ---- 3 the routes, by numbers
A = json.load(open(os.path.join(ROOT, 'Data', 'airfield.json'))); P = {r['id']: r for r in D('terminal-plan.json')['rooms']}; JL = D('babylon-jetways.json')
def near(x0, x, z):
    o = (x - x0 - G.H, z - G.CZ); a = math.atan2(*o)
    return math.hypot(*o) - (G.R + G.E1) if abs(a) <= G.HALF else min(math.hypot(x - x0 - q, z - G.ZG) for q in (0.0, G.X1))
jets = [(n['name'], -n['translation'][0]) for n in JL['nodes'] if re.match(r'J-\d+[A-Z]?$', n['name'])]
import numpy as np
ROTS = {}
for nm, new, V, T in []: pass
for g in G.GATES:
    x0 = P['F2-GATE-%d' % g]['x0']; line = []
    st = [s for s in A['stands'] if x0 - 45 < s['x'] < x0 + 75]
    if st: s = min(st, key=lambda s: near(x0, s['x'], s.get('nose', s.get('nz', 120)))); line.append('nearest nose %s %.1f' % (s.get('id'), near(x0, s['x'], s.get('nose', s.get('nz', 120)))))
    for nm, jx in jets:
        if x0 - 20 < jx < x0 + 80: line.append('%s rotunda %.1f' % (nm, near(x0, jx, 95.84) - 1.92))
    masts = [-43.0, -2.14, 38.71, 79.57, 120.43, 161.29, 202.14, 243.0]; line.append('mast %.1f' % min(near(x0, m, 90.0) for m in masts))
    if g == 13: line.append('bag-train door x 267.4-271.4: head 4.0, the soffit 5.40; trains 3.4 high: %.2f clear' % (G.SOFFIT - 3.4))
    print('ROUTES gate %d: %s' % (g, '; '.join(line)))
# ---- 4 one floor, one ceiling, one roof, one soffit at every point of every gate room and its bay (a 25 cm grid; nothing missing, nothing twice)
FL = []                                              # every level face: (y, up?, three corners x z), structure parts only
for nm, new, V, T in OBJ:
    if not re.search(r'STRUCTURE|ARC-(FLOOR|ROOF)|BAGBLOCK|RAMP', nm) or 'JET' in nm: continue
    c = V[T]; n = np.cross(c[:, 1] - c[:, 0], c[:, 2] - c[:, 0]); ln = np.linalg.norm(n, axis=1); ok = (ln > 1e-7) & (np.abs(n[:, 1]) > 0.999 * ln)
    for t, s in zip(c[ok], np.sign(n[ok, 1])): FL.append((t[0, 1], t[:, [0, 2]], s))
FY = np.array([f[0] for f in FL]); FT = np.array([f[1] for f in FL]); FS = np.array([f[2] for f in FL])
def count(px, pz, y0, y1):
    k = (FY > y0) & (FY < y1); t = FT[k] + 0.0; out = np.zeros(len(px), int)
    sg = None
    for q in t:
        a, b, c = q; d = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        if abs(d) < 1e-9: continue
        u = ((px - a[0]) * (c[1] - a[1]) - (pz - a[1]) * (c[0] - a[0])) / d; v = ((b[0] - a[0]) * (pz - a[1]) - (b[1] - a[1]) * (px - a[0])) / d
        out += (u >= 0) & (v >= 0) & (u + v <= 1)
    return out
for g in G.GATES:
    x0 = P['F2-GATE-%d' % g]['x0']; jw = G.has_corridor(JL, x0); X, Z = np.meshgrid(np.arange(0.31, 31.7, 0.25), np.arange(68.13, 85.0, 0.25)); X, Z = X.ravel() + 0.0031, Z.ravel() + 0.0017
    edge = np.array([G.zat(x) for x in X]); ins = Z < edge - 0.12; st = (X > 28.2) & (X < 31.2) & (Z > 69.4) & (Z < 80.6)          # (the stair's opening)
    cor = (np.abs(X - G.JX) < 1.75) & (Z > 81.95) if jw else np.zeros(len(X), bool); bay = Z > 82.1
    bad = {}
    for what, y0, y1, w, also in (('floor', 6.0, 6.02, ins & ~st & ~cor, None), ('ceiling', 11.35, 11.365, ins, None), ('roof', 11.99, 12.003, ins, None), ('soffit', 5.39, 5.41, ins & bay, None)):
        n = count(X[w] + x0, Z[w], y0, y1); bad[what] = (int((n == 0).sum()), int((n > 1).sum()), int(w.sum()), [[round(float(X[w][i]), 2), round(float(Z[w][i]), 2), int(n[i])] for i in np.where(n != 1)[0][:3]])
    print('FLOORS gate %d: %s' % (g, '; '.join('%s %d missing %d twice of %d %s' % (k, v[0], v[1], v[2], v[3] or '') for k, v in bad.items())))
