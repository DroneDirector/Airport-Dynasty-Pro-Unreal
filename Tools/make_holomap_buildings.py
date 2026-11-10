# The holographic map's other buildings (the jetways, the car rental centre), traced from the Babylon model itself
# (its glTF export): every upright face taller than a metre becomes a wall line on the map (glass kept as glass), merged
# on a 10 cm grid; each building's floors found from where its walls stand (gaps of 2.5 m between their feet). The
# control tower is drawn by the game from its own plan (TowerADC).
# Output: Data/holomap-buildings.json: [{id, label, floors: [{label, y0, y1, segs: [[x0, z0, x1, z1, glass], ...]}]}]
# in plan metres (x, z: the terminal plan's, as the map draws it).
#   python Tools/make_holomap_buildings.py [F:/ADP-Export/terminal.glb]
import json, struct, sys, os
import numpy as np

SRC = sys.argv[1] if len(sys.argv) > 1 else 'F:/ADP-Export/terminal.glb'
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Data', 'holomap-buildings.json')
BUILDINGS = [('J-1', 'JETWAY 1'), ('J-2', 'JETWAY 2'), ('J-3', 'JETWAY 3'), ('J-4', 'JETWAY 4'), ('RENTAL', 'CAR RENTAL')]
SKIP = ('FIRE-EXTINGUISHER', 'SIGNS', '/LED', 'TRIM', 'LIGHT', 'LAMP', 'CAR-', 'PLANT', 'KIOSK', 'COUNTER', 'SEATS', 'BIN')

b = open(SRC, 'rb').read()
jl = struct.unpack('<I', b[12:16])[0]
g = json.loads(b[20:20 + jl])
bin0 = 20 + jl + 8
N = g['nodes']

def node_matrix(nd):
    if 'matrix' in nd: return np.array(nd['matrix']).reshape(4, 4).T
    t = np.array(nd.get('translation', [0, 0, 0])); x, y, z, w = nd.get('rotation', [0, 0, 0, 1]); s = np.array(nd.get('scale', [1, 1, 1]))
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M = np.eye(4); M[:3, :3] = R * s; M[:3, 3] = t; return M

W, NAME = {}, {}
def walk(i, P, path):
    M = P @ node_matrix(N[i]); W[i] = M; NAME[i] = N[i].get('name', '')
    for c in N[i].get('children', []): walk(c, M, path)
for r in g['scenes'][g.get('scene', 0)]['nodes']: walk(r, np.eye(4), '')

def accessor(i):
    a = g['accessors'][i]; v = g['bufferViews'][a['bufferView']]
    comp = {5126: ('f', 4), 5125: ('I', 4), 5123: ('H', 2), 5121: ('B', 1)}[a['componentType']]
    n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    off = bin0 + v.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = v.get('byteStride', 0) or comp[1] * n
    raw = np.frombuffer(b, dtype=np.dtype(comp[0]).newbyteorder('<'), count=a['count'] * stride // comp[1], offset=off) if stride == comp[1] * n else None
    if raw is None:
        out = np.zeros((a['count'], n));
        for k in range(a['count']): out[k] = struct.unpack_from('<' + comp[0] * n, b, off + k * stride)
        return out
    return raw.reshape(a['count'], n)

result = []
for gid, label in BUILDINGS:
    walls = []   # (base y, top y, x0, z0, x1, z1, glass)
    for i, nd in W.items():
        nm = NAME[i]
        if not nm.startswith(gid + '/') or 'mesh' not in N[i] or any(s in nm for s in SKIP): continue
        for p in g['meshes'][N[i]['mesh']]['primitives']:
            mname = g['materials'][p['material']]['name'].lower() if 'material' in p else ''
            glass = 'glass' in mname
            P = accessor(p['attributes']['POSITION']).astype(np.float64)
            P = (np.c_[P, np.ones(len(P))] @ W[i].T)[:, :3]
            idx = accessor(p['indices']).astype(np.int64).reshape(-1, 3) if 'indices' in p else np.arange(len(P)).reshape(-1, 3)
            T = P[idx]                                      # triangles, 3 x xyz (glTF: x flipped from the plan)
            nrm = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
            ln = np.linalg.norm(nrm, axis=1) + 1e-12
            upright = np.abs(nrm[:, 1]) / ln < 0.15
            for t in T[upright]:
                y0, y1 = t[:, 1].min(), t[:, 1].max()
                if y1 - y0 < 0.3: continue
                xz = np.c_[-t[:, 0], t[:, 2]]               # plan x, z
                # the triangle's footprint: its two farthest points
                d = [(np.linalg.norm(xz[a] - xz[c]), a, c) for a, c in ((0, 1), (1, 2), (0, 2))]
                L, a, c = max(d)
                if L < 0.08: continue
                walls.append((y0, y1, xz[a][0], xz[a][1], xz[c][0], xz[c][1], glass))
    if not walls: continue
    walls = np.array(walls, dtype=object)
    # floors: the walls' feet, split where there's a gap of 2.5 m
    feet = sorted(set(round(float(w[0]), 1) for w in walls))
    groups, cur = [], [feet[0]]
    for f in feet[1:]:
        if f - cur[-1] > 2.5: groups.append(cur); cur = [f]
        else: cur.append(f)
    groups.append(cur)
    floors = []
    for k, gr in enumerate(groups):
        lo, hi = gr[0] - 0.2, (groups[k + 1][0] - 0.2) if k + 1 < len(groups) else 99.0
        segs = set()
        for w in walls:
            if not (lo <= w[0] < hi) or w[1] - w[0] < 1.0: continue
            q = lambda v: round(float(v) * 10) / 10
            s = (q(w[2]), q(w[3]), q(w[4]), q(w[5]))
            if s[:2] == s[2:]: continue
            if s[:2] > s[2:]: s = s[2:] + s[:2]
            segs.add(s + (1 if w[6] else 0,))
        if not segs: continue
        name = 'ON BOARD LEVEL' if gid.startswith('J-') else ('GROUND FLOOR' if k == 0 else 'FLOOR %d' % (k + 1))
        floors.append({'label': name, 'y0': lo, 'y1': hi, 'segs': [list(s) for s in sorted(segs)]})
    if gid.startswith('J-') and floors:   # a jetway is one floor (owner): all its walls together
        allsegs = sorted({tuple(s) for f in floors for s in f['segs']})
        floors = [{'label': 'ON BOARD LEVEL', 'y0': -1.0, 'y1': 99.0, 'segs': [list(s) for s in allsegs]}]
    result.append({'id': gid, 'label': label, 'floors': floors})
    print(gid, [(f['label'], round(f['y0'], 1), len(f['segs'])) for f in floors])

os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(result, open(OUT, 'w'), separators=(',', ':'))
print('written', OUT, os.path.getsize(OUT) // 1024, 'KB')
