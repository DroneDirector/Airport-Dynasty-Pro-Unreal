# The terminal's east side: the automatic overlap check (owner's rule: nothing cuts through anything). The live layout
# (Data/terminal-layout.json: the glb's pieces, Art/TerminalAnnex and Art/TerminalEast models, each where its node puts it)
# is laid out in the plan's metres, and the new parts (east_*, the new entrances' copies, their lamps) are checked against
# everything else and against each other:
#   1 flicker   faces on one plane (2 mm), looking the same way, over each other (> 4 cm2)
#   2 cuts      triangles of the new parts cutting through triangles of existing parts (joints where a new part only meets
#               a wall, floor or the roof it ends at are listed apart with how deep they reach in)
#   3 routes    the departure bag trains, the staff door, the cart lane's roller door, kerb lamps, gratings, zebras, coach
#               stands, set-down bays, pillars, benches (Data/roads.json, GSE.cpp) against the new parts' boxes
#   F:/Tools/Blender-4.2/blender.exe -b --python Tools/check_east.py        -> Saved/east-check.json and the summary
import bpy, json, math, os, struct, sys, glob
import numpy as np
from mathutils.bvhtree import BVHTree
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = lambda n: json.load(open(os.path.join(ROOT, 'Data', n), encoding='utf-8'))
L = json.load(open(os.environ['EAST_LAYOUT'])) if 'EAST_LAYOUT' in os.environ else D('terminal-layout.json'); N = L['nodes']
raw = open('F:/ADP-Export/terminal.glb', 'rb').read(); jn = struct.unpack('<I', raw[12:16])[0]; GJ = json.loads(raw[20:20 + jn]); GB = raw[20 + jn + 8:]
def acc(i):
    a = GJ['accessors'][i]; bv = GJ['bufferViews'][a['bufferView']]; o = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    dt = {5126: '<f4', 5123: '<u2', 5125: '<u4', 5121: 'u1'}[a['componentType']]; nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[a['type']]
    return np.frombuffer(GB, dt, a['count'] * nc, o).reshape(a['count'], nc)
CG = {}
def glb_mesh(m):
    if m not in CG:
        V, T = [], []; b = 0
        for p in GJ['meshes'][m]['primitives']:
            P = acc(p['attributes']['POSITION']).astype(float); I = acc(p['indices']).astype(int).reshape(-1, 3); V.append(P); T.append(I + b); b += len(P)
        CG[m] = (np.vstack(V), np.vstack(T))
    return CG[m]
CA = {}
def asset_mesh(name):
    """an annex / east model, in glTF axes (x = -plan x, y up, z = plan z), from its .fbx"""
    if name not in CA:
        f = next((q for q in (os.path.join(ROOT, 'Art', d, name + '.fbx') for d in ('TerminalEast', 'TerminalAnnex')) if os.path.exists(q)), None)
        if not f: CA[name] = None; return None
        before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=f); V, T = [], []; b = 0
        for o in set(bpy.data.objects) - before:
            if o.type != 'MESH': continue
            o.data.calc_loop_triangles(); M = np.array(o.matrix_world)
            P = np.array([v.co[:] for v in o.data.vertices]); P = P @ M[:3, :3].T + M[:3, 3]
            V.append(np.stack([P[:, 0], P[:, 2], -P[:, 1]], 1)); T.append(np.array([t.vertices[:] for t in o.data.loop_triangles], int) + b); b += len(P)
            bpy.data.objects.remove(o)
        CA[name] = (np.vstack(V), np.vstack(T))
    return CA[name]
def mat(n):
    t = n.get('translation', [0, 0, 0]); x, y, z, w = n.get('rotation', [0, 0, 0, 1]); s = n.get('scale', [1, 1, 1])
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M = np.eye(4); M[:3, :3] = R * np.array(s); M[:3, 3] = t; return M
REG = (90.0, 600.0, -32.0, 54.0)                      # the region looked at (plan x, z)
NEW = ('F1-EAST/', 'F1-ENTR-EAST', 'D-F1-ENTR-EAST', 'KERB/CANOPY-EAST', 'F1-LINK', 'F1-EAST-', 'F1-CUSTOMS-E', 'F1-CLAIM-E', 'F1-PC-', 'F1-CREW-CENTRE', 'F1-CANTEEN', 'F1-GROUND-OFF',
       'F1-LOCKERS', 'F1-STAFFSEC', 'F1-POLICE', 'F1-COACH-STATION', 'F2-CONC', 'D-F2-ENTR', 'F2-WALK/CEILING/GABLE-240')
SKIP = ('SITE/GRASS', 'SITE/ROAD', 'SITE/KERB', 'SITE/CARPARK', 'SITE/PARKING', 'SITE/CAR-', 'SITE/TREE', 'SITE/APRON', 'STAND-', 'RENTAL')
OBJ = []                                              # (name, new?, V plan, T)
def walk(i, M, name, new):
    n = N[i]
    if n['name'].startswith(SKIP): return
    W = M @ mat(n); nm = name if n['name'].startswith(('box', 'cyl', 'scr', 'sign', 'w#', 'arm', 'ball', 'wq')) else n['name']; new = new or n['name'].startswith(NEW)
    if 'mesh' in n or 'asset' in n:
        g = asset_mesh(n['asset']) if 'asset' in n else glb_mesh(n['mesh']) if n['mesh'] < len(GJ['meshes']) else None
        if g is not None:
            V = g[0] @ W[:3, :3].T + W[:3, 3]; V = np.stack([-V[:, 0], V[:, 1], V[:, 2]], 1)          # the plan's x, y, z
            T = g[1]; c = V[T]; lo, hi = c.min(1), c.max(1)
            k = (hi[:, 0] >= REG[0]) & (lo[:, 0] <= REG[1]) & (hi[:, 2] >= REG[2]) & (lo[:, 2] <= REG[3])
            if k.any(): OBJ.append((nm + ((' [' + n['name'] + ']') if nm != n['name'] else '') + (' {' + n['asset'] + '}' if 'asset' in n else ''), new, V, T[k]))
    for c in n.get('children', []): walk(c, W, nm, new)
for r in L['roots']: walk(r, np.eye(4), '', False)
news = [o for o in OBJ if o[1]]; olds = [o for o in OBJ if not o[1]]
print('CHECK objects in the region: %d new, %d existing' % (len(news), len(olds)))

# ---- 1 flicker: axis-aligned triangles, per plane ----
def planes(objs):
    out = {}
    for k, (nm, new, V, T) in enumerate(objs):
        c = V[T]; n = np.cross(c[:, 1] - c[:, 0], c[:, 2] - c[:, 0]); a = np.linalg.norm(n, axis=1); ok = a > 2e-6; c, n, a = c[ok], n[ok], a[ok]; n = n / a[:, None]
        for ax in range(3):
            m = np.abs(n[:, ax]) > 0.9999
            for tri, s in zip(c[m], np.sign(n[m, ax])): out.setdefault((ax, int(round(tri[0][ax] * 500))), []).append((k, tri, s))
    return out
def clip(P, Q):
    """area of the overlap of two triangles (2D)"""
    def side(a, b, p): return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
    if side(*Q) < 0: Q = Q[::-1]
    out = [tuple(p) for p in P]
    for i in range(3):
        a, b = Q[i], Q[(i + 1) % 3]; inp, out = out, []
        for j in range(len(inp)):
            p, q = inp[j], inp[(j + 1) % len(inp)]; sp, sq = side(a, b, p), side(a, b, q)
            if sp >= 0: out.append(p)
            if sp * sq < 0: t = sp / (sp - sq); out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
        if len(out) < 3: return 0.0
    return abs(sum(out[i][0] * out[(i + 1) % len(out)][1] - out[(i + 1) % len(out)][0] * out[i][1] for i in range(len(out)))) / 2
flick = {}
PA = planes(OBJ)
for (ax, q), lst in PA.items():
    cand = lst + PA.get((ax, q + 1), [])
    L1 = [t for t in lst if OBJ[t[0]][1]]
    if not L1: continue
    u, v = [i for i in range(3) if i != ax]
    C = np.array([[t[1][:, u].min(), t[1][:, u].max(), t[1][:, v].min(), t[1][:, v].max()] for t in cand]); S = np.array([t[2] for t in cand])
    for t in L1:
        b = (t[1][:, u].min(), t[1][:, u].max(), t[1][:, v].min(), t[1][:, v].max())
        hit = np.where((C[:, 0] < b[1] - 0.005) & (C[:, 1] > b[0] + 0.005) & (C[:, 2] < b[3] - 0.005) & (C[:, 3] > b[2] + 0.005) & (S == t[2]))[0]
        for h in hit:
            o = cand[h]
            if o[1] is t[1]: continue
            A = clip(t[1][:, [u, v]].tolist(), o[1][:, [u, v]].tolist())
            if A > 4e-4:
                key = (OBJ[t[0]][0], OBJ[o[0]][0]); e = flick.setdefault(key, [0.0, []]); e[0] += A
                if len(e[1]) < 400: e[1].append([round(float(v_), 3) for v_ in t[1].mean(0)] + ['xyz'[ax]])
# ---- 2 cuts: the new parts against the existing ones ----
VN = []; TN = []; b = 0; who = []
for k, (nm, new, V, T) in enumerate(news):
    VN.append(V); TN.append(T + b); b += len(V); who += [k] * len(T)
VN = np.vstack(VN); TN = np.vstack(TN)
bt = BVHTree.FromPolygons([tuple(v) for v in VN], [tuple(int(i) for i in t) for t in TN], epsilon=0.0)
cuts = {}
for nm, new, V, T in olds:
    o = BVHTree.FromPolygons([tuple(v) for v in V], [tuple(int(i) for i in t) for t in T], epsilon=0.0)
    for a, c in bt.overlap(o):
        A, B = VN[TN[a]], V[T[c]]; lo = np.maximum(A.min(0), B.min(0)); hi = np.minimum(A.max(0), B.max(0)); dep = float((hi - lo).max())
        # (how far each reaches past the other's plane: the smaller of the two, a real cut reaches through)
        def past(P, Q):
            n = np.cross(Q[1] - Q[0], Q[2] - Q[0]); n = n / max(1e-12, np.linalg.norm(n)); d = (P - Q[0]) @ n; return float(min(max(d.max(), 0), max(-d.min(), 0)))
        p = min(past(A, B), past(B, A))
        if p < 0.002: continue
        e = cuts.setdefault((news[who[a]][0], nm), [0, 0.0, []]); e[0] += 1; e[1] = max(e[1], p)
        if len(e[2]) < 3: e[2].append([round(float(v_), 2) for v_ in (lo + hi) / 2])
# ---- 3 routes ----
ROADS = D('roads.json'); fc = next(r for r in ROADS['roads'] if r['id'] == 'AIRPORT-FORECOURT')
S_OF = lambda x: 1860.32 - x                          # (how far along the forecourt road a plan x is: A.x(242) = 1618.32)
zone = []                                             # (name, x0, x1, y0, y1, z0, z1)
TR = [(134.4, 44), (134.4, 53.5), (137.9, 57), (265, 57), (269, 61), (269, 92)]
for (xa, za), (xb, zb) in zip(TR, TR[1:]): zone.append(('departure bag trains', min(xa, xb) - 1.6, max(xa, xb) + 1.6, 0.05, 3.4, min(za, zb) - 1.6, max(za, zb) + 1.6))
zone += [('staff door x 156 z 46', 156.2, 159.0, 0.05, 2.4, 45.1, 46.9), ('cart lane roller door', 127.9, 135.7, 0.05, 4.1, 48, 56)]
def row(p, name, r, h, size=0.3):
    off = {'right': 1}.get(p['side'], -1); e = p.get('edge')
    edges = [0, 3.5, 4.5, 8.0, 11.6, 15.15, 18.25, 21.75, 25.25, 46.75]
    z = -46.75 + edges[e] + p.get('off', 0); s = p['from']
    while s <= p['to'] + 1e-6:
        x = S_OF(s)
        if REG[0] < x < REG[1]: zone.append((name, x - r, x + r, 0.05, h, z - r, z + r))
        s += p['spacing']
for p in fc['lights']:
    if p['side'] == 'right' and p['edge'] == 8: row(p, 'kerb lamp', 0.35, 9.0)
for p in fc['props']:
    if p['side'] == 'right' and p['edge'] == 8: row(p, p['mesh'], 0.5, 0.3)
for c in fc['crossings']: zone.append(('zebra', S_OF(c['at']) - 1.5, S_OF(c['at']) + 1.5, 0.05, 3.0, -46.75 + c['o0'], -46.75 + c['o1']))
for p in fc['paint']:
    if p['kind'] == 'boxes':
        z0 = -46.75 + (21.75 if p['lane'] == 7 else 15.15); s = p['from']
        for k in range(p['n']): zone.append(('coach stand' if p['lane'] == 7 else 'set-down bay', S_OF(s + p['len']), S_OF(s), 0.05, 4.2, z0, z0 + (3.5 if p['lane'] == 7 else 3.1))); s += p['len'] + p['gap']
hitz = {}
for k, (nm, new, V, T) in enumerate(news):
    c = V[T]; lo, hi = c.min(1), c.max(1)
    for z in zone:
        m = (lo[:, 0] < z[2]) & (hi[:, 0] > z[1]) & (lo[:, 1] < z[4]) & (hi[:, 1] > z[3]) & (lo[:, 2] < z[6]) & (hi[:, 2] > z[5])
        # (big triangles' boxes reach further than they do: the triangle cut to the zone's height and box exactly, by sampling)
        for t in np.where(m)[0]:
            g = np.random.default_rng(0).dirichlet((1, 1, 1), 4000) @ c[t]
            if ((g[:, 0] > z[1]) & (g[:, 0] < z[2]) & (g[:, 1] > z[3]) & (g[:, 1] < z[4]) & (g[:, 2] > z[5]) & (g[:, 2] < z[6])).any(): hitz.setdefault((z[0], nm), []).append([round(float(q), 1) for q in c[t].mean(0)])
json.dump({'flicker': [[a, b, round(v[0], 4), v[1]] for (a, b), v in flick.items()], 'cuts': [[a, b, v[0], round(v[1], 3), v[2]] for (a, b), v in cuts.items()], 'routes': [[a, b, v[:5]] for (a, b), v in hitz.items()]}, open(os.path.join(ROOT, 'Saved', 'east-check.json'), 'w'), indent=1)
print('CHECK 1 flicker: %d pairs' % len(flick))
for (a, b), v in sorted(flick.items(), key=lambda q: -q[1][0])[:40]: print('   %-34s %-44s %.4f m2 %s' % (a[:34], b[:44], v[0], v[1][:3]))
print('CHECK 2 cuts: %d pairs' % len(cuts))
for (a, b), v in sorted(cuts.items(), key=lambda q: -q[1][1])[:200]: print('   %-34s %-44s %4d tris, %.3f m deep %s' % (a[:34], b[:44], v[0], v[1], v[2][:2]))
print('CHECK 3 routes (%d places): %s' % (len(zone), 'CLEAR' if not hitz else ''))
for (a, b), v in hitz.items(): print('   %s <- %s %s' % (a, b, v[:3]))
