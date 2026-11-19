# The jetways' walls and the service doorway (owner, 2026-11-19), on the export in place (names and mesh numbers kept),
# after the whole pipeline (realorder -> finish -> details -> newgates -> monitor -> THIS -> find_zfight -> import):
#  - the cab's and the rotunda's inside walls wear the tunnel's metal plate exactly as the tunnel does: the plate
#    (jwWall) was already on them, but stretched over each big face, so they looked plain. Their faces get UVs in
#    metres - the same scale and direction as the tunnel's own walls (measured from them here), round walls unrolled
#    along their arc, flat ones along their face - so the plates run on seamlessly. Not the console, not the doors, not
#    the floor or the ceiling (walls only: faces standing upright);
#  - the console (his marked pictures): the cabinet's stepped top replaced by one wedge that meets the sloped desk plate
#    all round (flush, nothing jagged, no faces on top of each other); the desk plate in the cabinet's own grey (jwPanel)
#    - the buttons, joystick, screen and hood untouched;
#  - the window behind the console: the cab's side wall was a solid slab with the window's glass on its outside, so
#    from inside it was dark metal. The opening is cut through the slab where the glass is, its four sides lined;
#  - the cab front's carpet reaches the walls and the round part of the cab (the floor slab's bare metal showed there);
#  - the cab doors' windows and the front window facing the aircraft: their glass was there, but boxed in by the dark
#    frame part (its faces covered the glass both sides). The frame's faces are opened over the glass, a 2-3 cm border
#    kept, so they are real windows (the jetway's own glass);
#  - the cab's round walls in their own plate (jwCabWall: the same plate, a little darker and more matte) - right under
#    the cab's lamp the plate read almost white, like the console (owner's marked picture); now the grey of the wall
#    behind the console;
#  - the service doorway: the sill strip outside the cab-end section ran straight across the doorway (its long faces
#    span the whole section, so the doorway cut in jetway_part5.py missed them) - a 36 cm high bar you couldn't get
#    over. Its faces are cut at the doorway's edges now and the part across the opening (above the floor) removed; the
#    skirt below the floor stays.
#   python Tools/jetway_walls.py [glb]      (default F:/ADP-Export/jetways.glb, rewritten in place)
import json, struct, sys, math
import numpy as np

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/ADP-Export/jetways.glb'
b = open(PATH, 'rb').read()
jl = struct.unpack('<I', b[12:16])[0]; g = json.loads(b[20:20 + jl])
BIN = bytearray(b[20 + jl + 8: 20 + jl + 8 + struct.unpack('<I', b[20 + jl:24 + jl])[0]])
N = g['nodes']; par = {}
for i, nd in enumerate(N):
    for c in nd.get('children', []): par[c] = i
idx = {nd.get('name'): i for i, nd in enumerate(N)}
MAT = {m.get('name'): i for i, m in enumerate(g['materials'])}

def acc_read(ai):
    a = g['accessors'][ai]; bv = g['bufferViews'][a['bufferView']]
    off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]
    stride = bv.get('byteStride', 0)
    if stride and stride != nc * np.dtype(dt).itemsize:
        raw = np.frombuffer(BIN, np.uint8, a['count'] * stride, off).reshape(a['count'], stride)
        return np.frombuffer(raw[:, :nc * np.dtype(dt).itemsize].tobytes(), dt).reshape(a['count'], nc).copy()
    return np.frombuffer(BIN, dt, a['count'] * nc, off).reshape(a['count'], nc).copy()

def acc_new(arr, ctype, atype, target=None):
    global BIN
    while len(BIN) % 4: BIN.append(0)
    off = len(BIN); BIN += arr.tobytes()
    bv = {'buffer': 0, 'byteOffset': off, 'byteLength': arr.nbytes}
    if target: bv['target'] = target
    g['bufferViews'].append(bv)
    acc = {'bufferView': len(g['bufferViews']) - 1, 'componentType': ctype, 'count': int(arr.shape[0]), 'type': atype}
    if atype == 'VEC3' and ctype == 5126: acc['min'] = arr.min(0).tolist(); acc['max'] = arr.max(0).tolist()
    g['accessors'].append(acc)
    return len(g['accessors']) - 1

TYPE = {1: 'SCALAR', 2: 'VEC2', 3: 'VEC3', 4: 'VEC4'}

def trs(nd):
    M = np.eye(4)
    if 'matrix' in nd: return np.array(nd['matrix'], float).reshape(4, 4).T
    s = nd.get('scale', [1, 1, 1]); x, y, z, w = nd.get('rotation', [0, 0, 0, 1]); t = nd.get('translation', [0, 0, 0])
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M[:3, :3] = R * np.array(s)[None, :]; M[:3, 3] = t
    return M

def world(i):
    M = np.eye(4)
    while i is not None:
        M = trs(N[i]) @ M; i = par.get(i)
    return M

def descendants(i):
    out = []; todo = [i]
    while todo:
        k = todo.pop(); out.append(k); todo += N[k].get('children', [])
    return out

def prim_arrays(p):
    """All of a primitive's vertex attributes (float arrays) and its triangles."""
    A = {k: acc_read(v).astype(np.float64) for k, v in p['attributes'].items()}
    I = acc_read(p['indices']).reshape(-1, 3).astype(np.int64)
    return A, I

def prim_write(p, A, I):
    """A primitive's attributes and triangles written anew (its own accessors)."""
    for k, arr in A.items():
        old = g['accessors'][p['attributes'][k]]
        p['attributes'][k] = acc_new(arr.astype(np.float32), 5126, TYPE[arr.shape[1]], 34962)
    p['indices'] = acc_new(I.reshape(-1, 1).astype(np.uint32), 5125, 'SCALAR', 34963)

# ------------------------------------------------------------------------------------------- the tunnel's own UVs
# how many texture repeats per metre, and which way, on the tunnel walls' plate (jwWall faces standing upright)
def measure_tunnel():
    J = 'J-1'; si = idx[f'{J}/TUNNEL-2']; samples_u = []; samples_v = []
    for d in descendants(si):
        if 'mesh' not in N[d]: continue
        M = np.linalg.inv(world(si)) @ world(d)
        for p in g['meshes'][N[d]['mesh']]['primitives']:
            if g['materials'][p.get('material', 0)]['name'] != 'jwWall' or 'TEXCOORD_0' not in p['attributes'] or 'indices' not in p: continue
            A, I = prim_arrays(p)
            P = (np.c_[A['POSITION'], np.ones(len(A['POSITION']))] @ M.T)[:, :3]; UV = A['TEXCOORD_0']
            for t in I[:400]:
                a, b2, c = P[t]; ua, ub, uc = UV[t]
                n = np.cross(b2 - a, c - a); ln = np.linalg.norm(n)
                if ln < 1e-6 or abs(n[1] / ln) > 0.3: continue
                # solve the UV gradient over the face: d(uv) = G . d(pos) along two edges
                E = np.array([b2 - a, c - a]); D = np.array([ub - ua, uc - ua])
                hz = np.array([0.0, 0.0, 1.0]) if abs(n[2] / ln) < 0.7 else np.array([1.0, 0.0, 0.0])
                # (in-plane basis: horizontal along the wall, and up)
                h = np.cross([0, 1, 0], n / ln); h /= max(np.linalg.norm(h), 1e-9); up = np.array([0, 1.0, 0])
                B = np.array([[e @ h, e @ up] for e in E])
                if abs(np.linalg.det(B)) < 1e-8: continue
                G = np.linalg.solve(B, D)   # rows: d/dh, d/dup ; cols: u, v
                samples_u.append(G[:, 0]); samples_v.append(G[:, 1])
    su = np.median(np.array(samples_u), 0) if samples_u else np.array([1.0, 0.0])
    sv = np.median(np.array(samples_v), 0) if samples_v else np.array([0.0, -1.0])
    return su, sv

SU, SV = measure_tunnel()
print('tunnel plate UV per metre: u =', np.round(SU, 3), '(along, up)  v =', np.round(SV, 3))
# the same per-metre rates for our walls: u from the horizontal distance along the wall, v from the height
U_ALONG, U_UP = SU; V_ALONG, V_UP = SV

def remap_walls(node_i, frame_i, pred, mats=('jwWall',)):
    """Faces in mats for which pred(centroid, normal) (frame_i's frame): their own vertices, UVs in metres."""
    if 'mesh' not in N[node_i]: return 0
    M = np.linalg.inv(world(frame_i)) @ world(node_i); done = 0
    for p in g['meshes'][N[node_i]['mesh']]['primitives']:
        if g['materials'][p.get('material', 0)]['name'] not in mats or 'indices' not in p or 'TEXCOORD_0' not in p['attributes']: continue
        A, I = prim_arrays(p)
        P = (np.c_[A['POSITION'], np.ones(len(A['POSITION']))] @ M.T)[:, :3]
        a, b2, c = P[I[:, 0]], P[I[:, 1]], P[I[:, 2]]
        n = np.cross(b2 - a, c - a); ln = np.linalg.norm(n, axis=1); n = n / np.maximum(ln, 1e-9)[:, None]
        if 'NORMAL' in A:
            Nv = A['NORMAL'] @ M[:3, :3].T; avg = Nv[I[:, 0]] + Nv[I[:, 1]] + Nv[I[:, 2]]
            n = np.where((np.sum(n * avg, 1) < 0)[:, None], -n, n)
        m = pred((a + b2 + c) / 3, n) & (ln > 1e-10) & (np.abs(n[:, 1]) < 0.3)
        if not m.any(): continue
        # the chosen faces get vertices of their own (so the faces round them keep theirs)
        sel = I[m]; base = len(A['POSITION'])
        for k in A: A[k] = np.vstack([A[k], A[k][sel.reshape(-1)]])
        newI = np.arange(base, base + sel.size).reshape(-1, 3)
        Pn = P[sel.reshape(-1)]
        # each face: round (unrolled along its arc about the section's middle) or flat (along its own face)
        for f in range(len(newI)):
            vs = Pn[f * 3:f * 3 + 3]; nf = n[np.where(m)[0][f]]
            r = np.hypot(vs[:, 0], vs[:, 2])
            if pred.round and np.all(r > 0.6) and np.all(np.abs(r - r.mean()) < 0.15):
                ang = np.unwrap(np.arctan2(vs[:, 2], vs[:, 0])); along = ang * pred.radius
            else:
                h = np.cross([0, 1, 0], nf); h /= max(np.linalg.norm(h), 1e-9); along = vs @ h
            up = vs[:, 1]
            A['TEXCOORD_0'][newI[f]] = np.c_[along * U_ALONG + up * U_UP, along * V_ALONG + up * V_UP]
        I = np.vstack([I[~m], newI]) if (~m).any() else newI
        prim_write(p, A, I); done += int(m.sum())
    return done


# ------------------------------------------------------------------------------------------------ small helpers
def prim_of(node_i, mat):
    for p in g['meshes'][N[node_i]['mesh']]['primitives']:
        if g['materials'][p.get('material', 0)]['name'] == mat and 'indices' in p: return p
    return None

def reassign_faces(node_i, frame_i, from_mat, to_mat, pred):
    """Faces of from_mat with pred(centroid) (frame_i's frame) moved to to_mat (a primitive sharing the vertices)."""
    p = prim_of(node_i, from_mat)
    if p is None: return 0
    M = np.linalg.inv(world(frame_i)) @ world(node_i)
    A, I = prim_arrays(p); P = (np.c_[A['POSITION'], np.ones(len(A['POSITION']))] @ M.T)[:, :3]
    m = pred((P[I[:, 0]] + P[I[:, 1]] + P[I[:, 2]]) / 3)
    if not m.any(): return 0
    keep = I[~m] if (~m).any() else I[:1]
    p['indices'] = acc_new(keep.reshape(-1, 1).astype(np.uint32), 5125, 'SCALAR', 34963)
    g['meshes'][N[node_i]['mesh']]['primitives'].append({'attributes': dict(p['attributes']), 'indices': acc_new(I[m].reshape(-1, 1).astype(np.uint32), 5125, 'SCALAR', 34963), 'material': MAT[to_mat], 'mode': 4})
    return int(m.sum())

def drop_faces(node_i, frame_i, mat, pred):
    p = prim_of(node_i, mat)
    if p is None: return 0
    M = np.linalg.inv(world(frame_i)) @ world(node_i)
    A, I = prim_arrays(p); P = (np.c_[A['POSITION'], np.ones(len(A['POSITION']))] @ M.T)[:, :3]
    m = pred(P[I[:, 0]], P[I[:, 1]], P[I[:, 2]])
    if not m.any(): return 0
    keep = I[~m] if (~m).any() else I[:1]
    p['indices'] = acc_new(keep.reshape(-1, 1).astype(np.uint32), 5125, 'SCALAR', 34963)
    return int(m.sum())

def add_quads(node_i, frame_i, mat, quads):
    """Flat quads (4 corners each, in frame_i's frame, clockwise seen from the front) added to node_i's mat
    primitive: their own vertices, flat normals, UVs in metres (the plate's per-metre rates)."""
    p = prim_of(node_i, mat)
    if p is None: return 0
    Minv = np.linalg.inv(np.linalg.inv(world(frame_i)) @ world(node_i))
    A, I = prim_arrays(p); base = len(A['POSITION']); newI = []
    add = {k: [] for k in A}
    for qi, q in enumerate(quads):
        # (the corners as listed go round clockwise seen from the front: the faces are wound the other way, facing out)
        q = np.array(q, float); n = np.cross(q[2] - q[0], q[1] - q[0]); n /= max(np.linalg.norm(n), 1e-9)
        h = np.cross([0, 1, 0], n); h = h / np.linalg.norm(h) if np.linalg.norm(h) > 1e-6 else np.array([1.0, 0, 0])
        for v in q:
            lv = (Minv @ np.r_[v, 1])[:3]; ln = Minv[:3, :3] @ n; ln /= max(np.linalg.norm(ln), 1e-9)
            along, up = v @ h, (v[1] if abs(n[1]) < 0.7 else v[2])
            for k in A:
                if k == 'POSITION': add[k].append(lv)
                elif k == 'NORMAL': add[k].append(ln)
                elif k == 'TEXCOORD_0': add[k].append([along * U_ALONG + up * U_UP, along * V_ALONG + up * V_UP])
                else: add[k].append(A[k][0])
        b0 = base + qi * 4
        newI += [[b0, b0 + 2, b0 + 1], [b0, b0 + 3, b0 + 2]]
    for k in A: A[k] = np.vstack([A[k], np.array(add[k], float)])
    prim_write(p, A, np.vstack([I, np.array(newI)]))
    return len(quads)

class Pred:
    def __init__(self, fn, round_=False, radius=1.6): self.fn = fn; self.round = round_; self.radius = radius
    def __call__(self, c, n): return self.fn(c, n)

# ------------------------------------------------------------------------------------------------ the doorway cut
def clip_poly(poly, axis, val, keep_greater):
    """A convex polygon (list of (pos, attrs) rows: one float array per vertex) clipped to one side of a plane."""
    out = []
    for i in range(len(poly)):
        cur, nxt = poly[i], poly[(i + 1) % len(poly)]
        dc, dn = cur[0][axis] - val, nxt[0][axis] - val
        if not keep_greater: dc, dn = -dc, -dn
        if dc >= 0: out.append(cur)
        if (dc >= 0) != (dn >= 0):
            t = dc / (dc - dn); out.append((cur[0] + (nxt[0] - cur[0]) * t, cur[1] + (nxt[1] - cur[1]) * t))
    return out

def cut_box(node_i, frame_i, box):
    """Faces of node_i minus the box ((x0,x1),(y0,y1),(z0,z1) in frame_i's frame): split at its sides, the inside gone."""
    if 'mesh' not in N[node_i]: return 0
    M = np.linalg.inv(world(frame_i)) @ world(node_i); Minv = np.linalg.inv(M); cut = 0
    for p in g['meshes'][N[node_i]['mesh']]['primitives']:
        if 'indices' not in p: continue
        A, I = prim_arrays(p)
        P = (np.c_[A['POSITION'], np.ones(len(A['POSITION']))] @ M.T)[:, :3]
        keys = [k for k in A if k != 'POSITION']; widths = [A[k].shape[1] for k in keys]
        attrs = np.hstack([A[k] for k in keys]) if keys else np.zeros((len(P), 0))
        lo = np.array([box[0][0], box[1][0], box[2][0]]); hi = np.array([box[0][1], box[1][1], box[2][1]])
        tmin = np.minimum(np.minimum(P[I[:, 0]], P[I[:, 1]]), P[I[:, 2]]); tmax = np.maximum(np.maximum(P[I[:, 0]], P[I[:, 1]]), P[I[:, 2]])
        touch = np.all(tmax > lo, 1) & np.all(tmin < hi, 1)
        if not touch.any(): continue
        newP, newA, newI = [], [], []
        for t in np.where(touch)[0]:
            pieces = []; rest = [(P[v].copy(), attrs[v].copy()) for v in I[t]]
            for axis in range(3):
                for val, side in ((lo[axis], False), (hi[axis], True)):
                    outside = clip_poly(rest, axis, val, side)   # the part beyond this side: kept
                    if len(outside) >= 3: pieces.append(outside)
                    rest = clip_poly(rest, axis, val, not side)
                    if len(rest) < 3: rest = []; break
                if not rest: break
            for poly in pieces:
                b0 = len(P) + len(newP)
                for pos, at in poly: newP.append(pos); newA.append(at)
                for k in range(1, len(poly) - 1): newI.append([b0, b0 + k, b0 + k + 1])
            cut += 1
        if newP:
            P2 = np.vstack([P, np.array(newP)]); at2 = np.vstack([attrs, np.array(newA)]) if keys else np.zeros((len(P2), 0))
        else:
            P2, at2 = P, attrs
        I2 = np.vstack([I[~touch]] + ([np.array(newI)] if newI else []))
        A2 = {'POSITION': (np.c_[P2, np.ones(len(P2))] @ Minv.T)[:, :3]}
        o = 0
        for k, w in zip(keys, widths):
            v = at2[:, o:o + w]; o += w
            if k == 'NORMAL': v = v / np.maximum(np.linalg.norm(v, axis=1), 1e-9)[:, None]
            A2[k] = v
        prim_write(p, A2, I2)
    return cut

# ------------------------------------------------------------------------------------------------ every jetway
stat = {'cab': 0, 'rotunda': 0, 'sill': 0, 'doors': 0}
done_mesh = set()   # (J-9 .. share J-1's meshes: once each)
jets = sorted({nm.split('/')[0] for nm in idx if nm and nm.startswith('J-') and '/' in nm})
for J in jets:
    # the cab: its round wall and its front, inside, upright; never the console or the doors
    cab = idx.get(f'{J}/CAB')
    if cab is not None:
        def cab_fn(c, n):
            r = np.hypot(c[:, 0], c[:, 2]); toward = -(c[:, 0] * n[:, 0] + c[:, 2] * n[:, 2]) / np.maximum(r, 1e-6)
            round_in = (r < 1.75) & (toward > 0.3)
            front = (c[:, 0] < -1.35) & (c[:, 0] > -3.15) & (np.abs(c[:, 2]) < 1.85)
            cen = np.array([-2.25, 0, 0]); tow2 = ((cen[0] - c[:, 0]) * n[:, 0] + (cen[2] - c[:, 2]) * n[:, 2])
            return (round_in | (front & (tow2 > 0.05))) & (c[:, 1] > 0.03) & (c[:, 1] < 2.58)
        for part in (f'{J}/CAB', f'{J}/CAB-FRONT', f'{J}/CAB-SLEEVE'):
            pi = idx.get(part)
            if pi is None: continue
            for d in descendants(pi):
                nm = N[d].get('name', '')
                if 'CONSOLE' in nm or 'DOOR' in nm or 'mesh' not in N[d] or N[d]['mesh'] in done_mesh: continue
                k = remap_walls(d, cab, Pred(cab_fn, True, 1.6))
                if k: done_mesh.add(N[d]['mesh']); stat['cab'] += k
    # the rotunda: inside its drum and its turning sleeve, upright
    rs = idx.get(f'{J}/ROTUNDA-SLEEVE')
    if rs is not None:
        def rot_fn(c, n):
            r = np.hypot(c[:, 0], c[:, 2]); toward = -(c[:, 0] * n[:, 0] + c[:, 2] * n[:, 2]) / np.maximum(r, 1e-6)
            return (r < 1.85) & (toward > 0.3) & (c[:, 1] > 0.03) & (c[:, 1] < 2.7)
        for part in (f'{J}/ROTUNDA', f'{J}/ROTUNDA-SLEEVE', f'{J}/ROTUNDA-DRUM'):
            pi = idx.get(part)
            if pi is None: continue
            for d in descendants(pi):
                nm = N[d].get('name', '')
                if 'DOOR' in nm or 'mesh' not in N[d] or N[d]['mesh'] in done_mesh: continue
                k = remap_walls(d, rs, Pred(rot_fn, True, 1.7))
                if k: done_mesh.add(N[d]['mesh']); stat['rotunda'] += k
    # the service doorway: the sill's faces across it cut away (above the floor; the skirt below stays)
    sv = idx.get(f'{J}/SERVICE-DOOR')
    if sv is not None and sv in par:
        s3 = par[sv]; t = N[sv]['translation']
        sg = 1.0 if t[0] > 0 else -1.0; X = abs(t[0]) + 0.005; FYc = t[1] - 0.005; D0 = t[2] - 0.005; D1 = D0 + 1.0; DH = 2.10 + FYc
        xr = (X - 0.12, X + 0.40) if sg > 0 else (-(X + 0.40), -(X - 0.12))
        box = (xr, (FYc + 0.005, DH), (D0 + 0.005, D1 - 0.005))
        for d in descendants(s3):
            nm = N[d].get('name', '')
            if not nm.endswith('#sill') or 'mesh' not in N[d] or ('sill', N[d]['mesh']) in done_mesh: continue
            k = cut_box(d, s3, box)
            if k:
                done_mesh.add(('sill', N[d]['mesh'])); stat['sill'] += k
                # the threshold: a plate at floor height across the doorway, from inside the wall out to the sill's face, so
                # coming in from the stair landing you step up onto something (the cut left the skirt open on top)
                if N[d]['name'].endswith(':jwTruss#sill'):
                    xa_, xb_ = (X - 0.15, X + 0.115) if sg > 0 else (-(X + 0.115), -(X - 0.15))
                    yt = FYc + 0.004
                    q = [(xa_, yt, D0), (xb_, yt, D0), (xb_, yt, D1), (xa_, yt, D1)]
                    qa = np.array(q); n_ = np.cross(qa[2] - qa[0], qa[1] - qa[0])
                    if n_[1] < 0: q = q[::-1]
                    stat['thresholds'] = stat.get('thresholds', 0) + add_quads(d, s3, 'jwTruss', [q])
        stat['doors'] += 1


# ------------------------------------------------------------------------------------------------ console, window, carpet
for J in jets:
    con = idx.get(f'{J}/CAB-CONSOLE'); cab = idx.get(f'{J}/CAB'); cf = idx.get(f'{J}/CAB-FRONT')
    if con is not None:
        pan = idx.get(f'{J}/CAB-CONSOLE:jwPanel#new'); alu = idx.get(f'{J}/CAB-CONSOLE:jwAluB#new')
        if pan is not None and 'mesh' in N[pan] and ('con', N[pan]['mesh']) not in done_mesh:
            done_mesh.add(('con', N[pan]['mesh']))
            # the stepped boxes on the cabinet's top (and its flat top face under them) gone ...
            # (the cabinet and the vents are two nodes of the same name: every one of them)
            for ch in N[con].get('children', []):
                if N[ch].get('name', '') == f'{J}/CAB-CONSOLE:jwPanel#new' and 'mesh' in N[ch]:
                    stat['console_top'] = stat.get('console_top', 0) + drop_faces(ch, con, 'jwPanel', lambda a, b2, c: np.minimum(np.minimum(a[:, 1], b2[:, 1]), c[:, 1]) > 0.899)
            # ... and one wedge from the cabinet's top (0.90) up to the desk plate's underside, flush with the plate (the
            # plate: 0.012 thick on the line y 0.92 at z -0.34 .. 1.06 at z 0.34; its underside about 6 mm under that)
            yu = lambda z: 0.92 + 0.14 * (z + 0.34) / 0.68 - 0.004
            x0, x1, z0, z1, yb = -0.40, 0.40, -0.30, 0.30, 0.90
            stat['console_wedge'] = stat.get('console_wedge', 0) + add_quads(pan, con, 'jwPanel', [
                [(x0, yb, z1), (x0, yb, z0), (x0, yu(z0), z0), (x0, yu(z1), z1)],     # left side
                [(x1, yb, z0), (x1, yb, z1), (x1, yu(z1), z1), (x1, yu(z0), z0)],     # right side
                [(x1, yb, z1), (x0, yb, z1), (x0, yu(z1), z1), (x1, yu(z1), z1)],     # back
                [(x0, yb, z0), (x1, yb, z0), (x1, yu(z0), z0), (x0, yu(z0), z0)],     # front
                [(x0, yu(z0) - 0.001, z0), (x1, yu(z0) - 0.001, z0), (x1, yu(z1) - 0.001, z1), (x0, yu(z1) - 0.001, z1)]])   # its top, just under the plate
        if alu is not None and 'mesh' in N[alu] and ('desk', N[alu]['mesh']) not in done_mesh:
            done_mesh.add(('desk', N[alu]['mesh']))
            # the sloped desk plate in the cabinet's grey (the hood over the screen keeps its metal)
            stat['desk'] = stat.get('desk', 0) + reassign_faces(alu, con, 'jwAluB', 'jwPanel', lambda c: (c[:, 1] > 0.89) & (c[:, 1] < 1.08) & (np.abs(c[:, 0]) < 0.43) & (np.abs(c[:, 2]) < 0.35))
    if cf is not None and cab is not None:
        ca = idx.get(f'{J}/CAB-FRONT:jwAluB'); gl = idx.get(f'{J}/CAB-FRONT:glassOuter'); cp = idx.get(f'{J}/CAB-FRONT:jwCarpet')
        if ca is not None and gl is not None and 'mesh' in N[ca] and 'mesh' in N[gl] and ('win', N[ca]['mesh']) not in done_mesh:
            done_mesh.add(('win', N[ca]['mesh']))
            # the side windows' glass (thin across z, on the slab's outside): the opening through the slab behind each
            pg = prim_of(gl, 'glassOuter'); Mg = np.linalg.inv(world(cab)) @ world(gl)
            Ag, Ig = prim_arrays(pg); Pg = (np.c_[Ag['POSITION'], np.ones(len(Ag['POSITION']))] @ Mg.T)[:, :3]
            # (each box apart: vertices grouped by the faces that join them)
            parent = list(range(len(Pg)))
            def root(a):
                while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
                return a
            for t in Ig:
                for v in t[1:]: parent[root(int(v))] = root(int(t[0]))
            # (and by position: a box's corners are often split per face)
            key = {}
            for i_, v in enumerate(np.round(Pg, 4)):
                kk = tuple(v)
                if kk in key: parent[root(i_)] = root(key[kk])
                else: key[kk] = i_
            lab = np.array([root(i_) for i_ in range(len(Pg))])
            for L_ in np.unique(lab):
                B = Pg[lab == L_]; lo, hi = B.min(0), B.max(0)
                ext = hi - lo; ax = int(np.argmin(ext))   # (the glass's thin direction: z = a side window, x = the front facing the aircraft)
                if ext[ax] > 0.06 or ax == 1 or min(e for i_, e in enumerate(ext) if i_ != ax) < 0.3: continue
                mid = (lo + hi) / 2
                # the slab is on the cab's inside of the glass (towards the cab's middle in that direction)
                sgn = 1.0 if mid[ax] > 0 else -1.0
                gin = lo[ax] if sgn > 0 else hi[ax]; gw = gin - sgn * 0.10
                rng = (min(gin, gw) - 0.02, max(gin, gw) + 0.005)
                box = [(lo[0], hi[0]), (lo[1], hi[1]), (lo[2], hi[2])]; box[ax] = rng
                k = cut_box(ca, cab, tuple(box))
                if not k: continue
                a0, a1 = (gw, gin) if sgn > 0 else (gin, gw)
                def P3(u, y, w):   # (u: along the window, y: up, w: through it)
                    v = [0.0, y, 0.0]; v[ax] = w; v[2 if ax == 0 else 0] = u; return tuple(v)
                ua, ub = (lo[2], hi[2]) if ax == 0 else (lo[0], hi[0]); ya, yb2 = lo[1], hi[1]
                rev = [
                    [P3(ua, ya, a0), P3(ub, ya, a0), P3(ub, ya, a1), P3(ua, ya, a1)],
                    [P3(ua, yb2, a1), P3(ub, yb2, a1), P3(ub, yb2, a0), P3(ua, yb2, a0)],
                    [P3(ua, ya, a1), P3(ua, yb2, a1), P3(ua, yb2, a0), P3(ua, ya, a0)],
                    [P3(ub, ya, a0), P3(ub, yb2, a0), P3(ub, yb2, a1), P3(ub, ya, a1)]]
                # (each reveal faces into the opening: flip any whose normal points away from the window's middle)
                fixed = []
                for q in rev:
                    qa = np.array(q); n = np.cross(qa[2] - qa[0], qa[1] - qa[0]); c = qa.mean(0)
                    tow = mid.copy(); tow[ax] = c[ax]
                    fixed.append(q if n @ (tow - c) > 0 else q[::-1])
                rev = fixed
                stat['window'] = stat.get('window', 0) + k; stat['reveals'] = stat.get('reveals', 0) + add_quads(ca, cab, 'jwAluB', rev)
        if cp is not None and 'mesh' in N[cp] and ('carpet', N[cp]['mesh']) not in done_mesh:
            done_mesh.add(('carpet', N[cp]['mesh']))
            # the cab front's carpet out to the side walls (their inside at |z| 1.75) and back over the round cab's edge
            p = prim_of(cp, 'jwCarpet'); Mc = np.linalg.inv(world(cab)) @ world(cp)
            A, I = prim_arrays(p); P = (np.c_[A['POSITION'], np.ones(len(A['POSITION']))] @ Mc.T)[:, :3]
            P[P[:, 2] > 1.69, 2] = 1.748; P[P[:, 2] < -1.69, 2] = -1.748; P[P[:, 0] < 1.40, 0] = 1.25
            A['POSITION'] = (np.c_[P, np.ones(len(P))] @ np.linalg.inv(Mc).T)[:, :3]
            prim_write(p, A, I); stat['carpet'] = stat.get('carpet', 0) + 1


# ------------------------------------------------------------------------------------------------ windows in the dark frames
def glass_boxes(gl_node, frame_i):
    pg = prim_of(gl_node, 'glassOuter')
    if pg is None: return []
    Mg = np.linalg.inv(world(frame_i)) @ world(gl_node)
    Ag, Ig = prim_arrays(pg); Pg = (np.c_[Ag['POSITION'], np.ones(len(Ag['POSITION']))] @ Mg.T)[:, :3]
    parent = list(range(len(Pg)))
    def root(a):
        while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
        return a
    for t in Ig:
        for v in t[1:]: parent[root(int(v))] = root(int(t[0]))
    key = {}
    for i_, v in enumerate(np.round(Pg, 4)):
        kk = tuple(v)
        if kk in key: parent[root(i_)] = root(key[kk])
        else: key[kk] = i_
    lab = np.array([root(i_) for i_ in range(len(Pg))])
    return [(Pg[lab == L_].min(0), Pg[lab == L_].max(0)) for L_ in np.unique(lab)]

for J in jets:
    cab = idx.get(f'{J}/CAB')
    if cab is None: continue
    # (every node of the cab front and of both door leaves - the leaves are two nodes of the same names)
    holders = [i for i, nd in enumerate(N) if nd.get('name') in (f'{J}/CAB-FRONT', f'{J}/CAB-DOOR')]
    for h in holders:
        kids = N[h].get('children', [])
        gls = [k for k in kids if N[k].get('name', '').endswith(':glassOuter') and 'mesh' in N[k]]
        # (the dark frame and the wall's inside face behind it - every solid part over the glass)
        dks = [k for k in kids if (N[k].get('name', '').endswith(':jwDark') or N[k].get('name', '').endswith(':jwWall')) and 'mesh' in N[k]]
        for dk in dks:
            if ('frame', N[dk]['mesh']) in done_mesh: continue
            done_mesh.add(('frame', N[dk]['mesh']))
            for gl in gls:
                for lo, hi in glass_boxes(gl, cab):
                    ext = hi - lo; ax = int(np.argmin(ext))
                    if ext[ax] > 0.10 or min(e for i_, e in enumerate(ext) if i_ != ax) < 0.15: continue
                    box = [(lo[0] + 0.002, hi[0] - 0.002), (lo[1] + 0.002, hi[1] - 0.002), (lo[2] + 0.002, hi[2] - 0.002)]
                    box[ax] = (lo[ax] - 0.15, hi[ax] + 0.15)   # (the wall's inside face sits a few cm inside the glass)
                    k = cut_box(dk, cab, tuple(box))
                    stat['frames_opened'] = stat.get('frames_opened', 0) + (1 if k else 0)

# ------------------------------------------------------------------------------------------------ the cab's round walls
if 'jwCabWall' not in MAT and 'jwWall' in MAT:
    import copy
    m = copy.deepcopy(g['materials'][MAT['jwWall']]); m['name'] = 'jwCabWall'
    pbr = m.setdefault('pbrMetallicRoughness', {})
    pbr['baseColorFactor'] = [c * (0.62 if i_ < 3 else 1.0) for i_, c in enumerate(pbr.get('baseColorFactor', [1, 1, 1, 1]))]
    pbr['metallicFactor'] = 0.10; pbr['roughnessFactor'] = 0.55
    g['materials'].append(m); MAT['jwCabWall'] = len(g['materials']) - 1
for J in jets:
    for i, nd in enumerate(N):
        if nd.get('name') == f'{J}/CAB:jwWall' and 'mesh' in nd and ('cabwall', nd['mesh']) not in done_mesh:
            done_mesh.add(('cabwall', nd['mesh']))
            for p in g['meshes'][nd['mesh']]['primitives']:
                if g['materials'][p.get('material', 0)]['name'] == 'jwWall': p['material'] = MAT['jwCabWall']; stat['cab_round'] = stat.get('cab_round', 0) + 1

while len(BIN) % 4: BIN.append(0)
g['buffers'][0]['byteLength'] = len(BIN)
js = json.dumps(g, separators=(',', ':')).encode()
while len(js) % 4: js += b' '
out = bytearray(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(BIN)))
out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(BIN), 0x004E4942) + BIN
open(PATH, 'wb').write(out)
print('WALLS', stat)
