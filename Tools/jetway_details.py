# The jetways' details (owner, 2026-11-17, to-do 84b and 84a fix 13), on the export after Tools/jetway_finish.py
# (in place; existing names and mesh numbers kept, new parts added as their own nodes under the part they ride with):
#  - the stepped sill under each tunnel section's windows, both sides (glazing bead, drip ledge, a recessed channel,
#    the deep side beam with its lower lip) - reference ref-sill-beam-under-windows;
#  - under each section: two steel stringers, cross members every metre, a perforated-look cable tray on hangers with
#    its cables, junction boxes; under the rotunda: two ring beams and eight radial beams - ref-underside-details;
#  - the pre-conditioned air unit hung under the cab-end section (dark grey cabinet, a louvred grille face in three
#    panels, a filter grille on its end, service panels with labels and bolts, two hanger cradles) - ref-air-unit-1/2;
#  - its two yellow hoses (fix 13): one up into the metal tube (the pipe on the lift beam), one hanging in a loose loop
#    on its hook; the old four (two under the rotunda, two on the lift beam) gone. They ride with the cab-end section.
# Everything inside the slim envelope of its section (Tools/jetway_realorder.py), so the sections still slide clear.
#   python Tools/jetway_details.py [glb]
import json, struct, sys, math
import numpy as np

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/ADP-Export/jetways.glb'
b = open(PATH, 'rb').read()
jl = struct.unpack('<I', b[12:16])[0]; g = json.loads(b[20:20 + jl])
BIN = bytearray(b[20 + jl + 8: 20 + jl + 8 + struct.unpack('<I', b[20 + jl:24 + jl])[0]])
N = g['nodes']; par = {}
for i, nd in enumerate(N):
    for c in nd.get('children', []): par[c] = i
MAT = {m.get('name'): i for i, m in enumerate(g['materials'])}

HW = [1.32, 1.51, 1.70]; H = [2.40, 2.69, 2.98]; FY = [0.024, 0.012, 0.0]; L = [10.5, 10.1, 9.7]; STEP_Y = 0.29

def acc_read(ai):
    a = g['accessors'][ai]; bv = g['bufferViews'][a['bufferView']]
    off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[a['type']]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16}[a['componentType']]
    return np.frombuffer(BIN, dt, a['count'] * nc, off).reshape(a['count'], nc).copy()

def acc_new(arr, ctype, atype, target=None):
    global BIN
    while len(BIN) % 4: BIN.append(0)
    off = len(BIN); BIN += arr.tobytes()
    bv = {'buffer': 0, 'byteOffset': off, 'byteLength': arr.nbytes}
    if target: bv['target'] = target
    g['bufferViews'].append(bv)
    a = {'bufferView': len(g['bufferViews']) - 1, 'componentType': ctype, 'count': int(arr.shape[0]), 'type': atype}
    if atype == 'VEC3' and ctype == 5126: a['min'] = [float(v) for v in arr.min(0)]; a['max'] = [float(v) for v in arr.max(0)]
    g['accessors'].append(a)
    return len(g['accessors']) - 1

def trs(nd):
    M = np.eye(4); s = nd.get('scale', [1, 1, 1]); x, y, z, w = nd.get('rotation', [0, 0, 0, 1]); t = nd.get('translation', [0, 0, 0])
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M[:3, :3] = R * np.array(s)[None, :]; M[:3, 3] = t
    return M

def rel(anc, d):
    M = np.eye(4); i = d
    while i != anc: M = trs(N[i]) @ M; i = par[i]
    return M

# ---- geometry builders: each returns (P, N, UV, I) lists to merge into one mesh per material
class Geo:
    def __init__(self): self.P, self.N, self.U, self.I = [], [], [], []
    def quad(self, a, b2, c, d, n, uv=None):
        o = len(self.P); self.P += [a, b2, c, d]; self.N += [n] * 4
        self.U += uv or [(0, 0), (np.linalg.norm(np.subtract(b2, a)), 0), (np.linalg.norm(np.subtract(b2, a)), np.linalg.norm(np.subtract(c, b2))), (0, np.linalg.norm(np.subtract(c, b2)))]
        if np.dot(np.cross(np.subtract(b2, a), np.subtract(c, a)), n) >= 0: self.I += [o, o + 1, o + 2, o, o + 2, o + 3]
        else: self.I += [o, o + 2, o + 1, o, o + 3, o + 2]
    def box(self, x0, x1, y0, y1, z0, z1):
        x0, x1 = min(x0, x1), max(x0, x1); y0, y1 = min(y0, y1), max(y0, y1); z0, z1 = min(z0, z1), max(z0, z1)
        v = lambda x, y, z: (x, y, z)
        self.quad(v(x1, y0, z0), v(x1, y1, z0), v(x1, y1, z1), v(x1, y0, z1), (1, 0, 0))
        self.quad(v(x0, y0, z1), v(x0, y1, z1), v(x0, y1, z0), v(x0, y0, z0), (-1, 0, 0))
        self.quad(v(x0, y1, z0), v(x0, y1, z1), v(x1, y1, z1), v(x1, y1, z0), (0, 1, 0))
        self.quad(v(x0, y0, z1), v(x0, y0, z0), v(x1, y0, z0), v(x1, y0, z1), (0, -1, 0))
        self.quad(v(x0, y0, z1), v(x1, y0, z1), v(x1, y1, z1), v(x0, y1, z1), (0, 0, 1))
        self.quad(v(x1, y0, z0), v(x0, y0, z0), v(x0, y1, z0), v(x1, y1, z0), (0, 0, -1))
    def beam(self, p0, p1, w, h, up=(0, 1, 0)):
        p0 = np.array(p0, float); p1 = np.array(p1, float); d = p1 - p0; Ln = np.linalg.norm(d); d /= Ln
        u = np.array(up, float); u = u - d * np.dot(u, d)
        if np.linalg.norm(u) < 1e-6: u = np.array([1.0, 0, 0]) - d * d[0]
        u /= np.linalg.norm(u); s = np.cross(d, u)
        cs = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
        r0 = [p0 + s * a * w / 2 + u * c * h / 2 for a, c in cs]; r1 = [p + d * Ln for p in r0]
        for f in range(4):
            a0, a1 = r0[f], r0[(f + 1) % 4]; b0, b1 = r1[f], r1[(f + 1) % 4]
            n = (a0 + a1) / 2 - p0; n -= d * np.dot(n, d); n /= np.linalg.norm(n)
            self.quad(tuple(a0), tuple(a1), tuple(b1), tuple(b0), tuple(n))
    def tube(self, pts, r, sides=12, rings=None):
        """A tube along pts (a smooth path); rings=(period, depth) for a corrugated hose."""
        pts = [np.array(p, float) for p in pts]; s_acc = [0.0]
        for i in range(1, len(pts)): s_acc.append(s_acc[-1] + np.linalg.norm(pts[i] - pts[i - 1]))
        frames = []; prev_n = None
        for i, p in enumerate(pts):
            t = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]; t /= np.linalg.norm(t)
            if prev_n is None:
                a = np.array([0, 1.0, 0]) if abs(t[1]) < 0.9 else np.array([1.0, 0, 0])
                n = np.cross(t, a); n /= np.linalg.norm(n)
            else:
                n = prev_n - t * np.dot(prev_n, t); n /= np.linalg.norm(n)
            prev_n = n; frames.append((p, t, n, np.cross(t, n)))
        base = len(self.P)
        for i, (p, t, n, bn) in enumerate(frames):
            rr = r * (1 + (rings[1] * math.sin(2 * math.pi * s_acc[i] / rings[0]) if rings else 0))
            for k in range(sides + 1):
                a = 2 * math.pi * k / sides; dirv = n * math.cos(a) + bn * math.sin(a)
                self.P.append(tuple(p + dirv * rr)); self.N.append(tuple(dirv)); self.U.append((s_acc[i], k / sides))
        for i in range(len(frames) - 1):
            for k in range(sides):
                a0 = base + i * (sides + 1) + k; a1 = a0 + 1; b0 = a0 + sides + 1; b1 = b0 + 1
                self.I += [a0, b0, a1, a1, b0, b1]
    def cylinder(self, c, axis, r, h, sides=20):
        c = np.array(c, float); ax = np.array(axis, float); ax /= np.linalg.norm(ax)
        self.tube([c, c + ax * h * 0.5, c + ax * h], r, sides)
        for end, sgn in ((c, -1), (c + ax * h, 1)):
            a = np.array([0, 1.0, 0]) if abs(ax[1]) < 0.9 else np.array([1.0, 0, 0]); n = np.cross(ax, a); n /= np.linalg.norm(n); bn = np.cross(ax, n)
            o = len(self.P); self.P.append(tuple(end)); self.N.append(tuple(ax * sgn)); self.U.append((0.5, 0.5))
            for k in range(sides + 1):
                aa = 2 * math.pi * k / sides; self.P.append(tuple(end + (n * math.cos(aa) + bn * math.sin(aa)) * r)); self.N.append(tuple(ax * sgn)); self.U.append((0.5 + 0.5 * math.cos(aa), 0.5 + 0.5 * math.sin(aa)))
            for k in range(sides): self.I += ([o, o + 1 + k, o + 2 + k] if sgn > 0 else [o, o + 2 + k, o + 1 + k])

def add_node(parent_i, name, mat_name, geo):
    if not geo.I: return
    P = np.array(geo.P, np.float32); Nn = np.array(geo.N, np.float32); U = np.array(geo.U, np.float32); I = np.array(geo.I, np.uint32)
    prim = {'attributes': {'POSITION': acc_new(P, 5126, 'VEC3', 34962), 'NORMAL': acc_new(Nn, 5126, 'VEC3', 34962), 'TEXCOORD_0': acc_new(U, 5126, 'VEC2', 34962)},
            'indices': acc_new(I.reshape(-1, 1), 5125, 'SCALAR', 34963), 'material': MAT[mat_name], 'mode': 4}
    g['meshes'].append({'primitives': [prim]}); N.append({'name': name, 'mesh': len(g['meshes']) - 1})
    N[parent_i].setdefault('children', []).append(len(N) - 1); par[len(N) - 1] = parent_i

def smooth(ctrl, n=10):
    """Catmull-Rom through control points."""
    P = [np.array(c, float) for c in ctrl]; P = [P[0]] + P + [P[-1]]; out = []
    for i in range(1, len(P) - 2):
        for k in range(n):
            t = k / n; p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(P[-2]); return out

def acc_bounds(node_i, frame_i):
    M = np.linalg.inv(rel(par_root(node_i), frame_i)) @ rel(par_root(node_i), node_i)
    P = np.vstack([acc_read(p['attributes']['POSITION']) for p in g['meshes'][N[node_i]['mesh']]['primitives']]).astype(float)
    W = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]
    return W.min(0), W.max(0)

def par_root(i):
    while N[i].get('name', '').count('/') or (i in par and not N[i].get('name', '').startswith('J-')):
        if N[i].get('name', '') in JROOTS: return i
        i = par[i]
    return i

JROOTS = {nd.get('name') for nd in N if nd.get('name', '').startswith('J-') and '/' not in nd.get('name', '')}
stat = {'sills': 0, 'unders': 0, 'pca': 0, 'hoses_new': 0, 'hoses_old_gone': 0}
for J in sorted(JROOTS):
    ji = next(i for i, nd in enumerate(N) if nd.get('name') == J)
    sec = [next(i for i, nd in enumerate(N) if nd.get('name') == f'{J}/TUNNEL-{k + 1}') for k in range(3)]
    for k in range(3):
        X = HW[k]; fy = FY[k]; bot = fy - 0.22 - k * STEP_Y; z0, z1 = 0.02, L[k] - 0.02
        # ---- the sill, both sides
        gm = Geo(); gd = Geo()
        for s in (-1, 1):
            o = lambda a: s * (X + a)
            gm.box(o(0.028), o(0.06), fy + 0.44, fy + 0.49, z0, z1)           # glazing bead
            gm.box(o(0.06), o(0.105), fy + 0.40, fy + 0.44, z0, z1)           # drip ledge
            gd.box(o(0.068), o(0.075), fy + 0.12, fy + 0.40, z0, z1)          # the recessed channel
            gm.box(o(0.075), o(0.10), bot, fy + 0.40, z0, z1)                 # the deep side beam
            gm.box(o(0.075), o(0.108), bot, bot + 0.035, z0, z1)              # its lower lip
        add_node(sec[k], f'{J}/TUNNEL-{k + 1}:jwTruss#sill', 'jwTruss', gm); add_node(sec[k], f'{J}/TUNNEL-{k + 1}:jwDark#sill', 'jwDark', gd); stat['sills'] += 1
        # ---- under the floor: stringers, cross members, the cable tray on hangers with its cables, junction boxes
        gs = Geo(); gt = Geo(); gc = Geo(); gj = Geo()
        for sx in (-1, 1):
            gs.box(sx * 0.80, sx * 0.90, bot - 0.12, bot, z0, z1)
            gs.box(sx * 0.80, sx * 0.84, bot - 0.12, bot - 0.10, z0, z1)      # (the channel's lower flange)
        zz = 0.5
        while zz < L[k] - 0.3:
            gs.box(-0.95, 0.95, bot - 0.06, bot, zz - 0.025, zz + 0.025); zz += 1.0
        tx0, tx1 = 0.30, 0.60; ty = bot - 0.24
        gt.box(tx0, tx1, ty, ty + 0.006, z0 + 0.1, z1 - 0.1)
        for xx in (tx0, tx1 - 0.006): gt.box(xx, xx + 0.006, ty, ty + 0.07, z0 + 0.1, z1 - 0.1)
        zz = 0.6
        while zz < L[k] - 0.4:
            for xx in (tx0 - 0.012, tx1): gt.box(xx, xx + 0.012, ty, bot, zz - 0.01, zz + 0.01)
            gt.box(tx0 - 0.012, tx1 + 0.012, ty - 0.012, ty, zz - 0.02, zz + 0.02); zz += 1.5
        for xc, rc in ((0.36, 0.018), (0.42, 0.014), (0.47, 0.02), (0.53, 0.016)):
            gc.tube([(xc, ty + 0.008 + rc, z0 + 0.12), (xc, ty + 0.008 + rc, (z0 + z1) / 2), (xc, ty + 0.008 + rc, z1 - 0.12)], rc, 8)
        for zc in (L[k] * 0.3, L[k] * 0.72):
            gj.box(-0.62, -0.36, bot - 0.30, bot - 0.06, zc - 0.12, zc + 0.12)
            gs.box(-0.66, -0.32, bot - 0.06, bot - 0.04, zc - 0.03, zc + 0.03)
        add_node(sec[k], f'{J}/TUNNEL-{k + 1}:jwTruss#underframe', 'jwTruss', gs)
        add_node(sec[k], f'{J}/TUNNEL-{k + 1}:jwAluB#cabletray', 'jwAluB', gt)
        add_node(sec[k], f'{J}/TUNNEL-{k + 1}:jwRubber#cables', 'jwRubber', gc)
        add_node(sec[k], f'{J}/TUNNEL-{k + 1}:jwDark#junction', 'jwDark', gj); stat['unders'] += 1
    # ---- under the rotunda: two ring beams and eight radial beams
    ro = next((i for i, nd in enumerate(N) if nd.get('name') == f'{J}/ROTUNDA'), None)
    if ro is not None:
        gr = Geo(); yb = -0.22
        for rr in (0.55, 1.65):
            for k in range(16):
                a0, a1 = 2 * math.pi * k / 16, 2 * math.pi * (k + 1) / 16
                gr.beam((rr * math.cos(a0), yb - 0.09, rr * math.sin(a0)), (rr * math.cos(a1), yb - 0.09, rr * math.sin(a1)), 0.10, 0.18)
        for k in range(8):
            a = 2 * math.pi * k / 8 + math.pi / 8
            gr.beam((0.3 * math.cos(a), yb - 0.11, 0.3 * math.sin(a)), (1.8 * math.cos(a), yb - 0.11, 1.8 * math.sin(a)), 0.12, 0.22)
        add_node(ro, f'{J}/ROTUNDA:jwTruss#underframe', 'jwTruss', gr)
    # ---- the air unit under the cab-end section
    k = 2; s3 = sec[2]; ub, ut = -1.85, -1.05; ux = 0.75; uz0, uz1 = 3.0, 6.2
    gb = Geo(); gg = Geo(); gh = Geo(); gl = Geo(); gbolt = Geo()
    gb.box(-ux, ux, ub, ut, uz0, uz1)
    # the grille face (+x): three panels of louvres in a frame
    for (pz0, pz1) in ((3.1, 4.1), (4.15, 5.15), (5.2, 6.1)):
        gg.box(ux, ux + 0.02, ub + 0.06, ub + 0.08, pz0, pz1); gg.box(ux, ux + 0.02, ut - 0.08, ut - 0.06, pz0, pz1)
        gg.box(ux, ux + 0.02, ub + 0.06, ut - 0.06, pz0, pz0 + 0.02); gg.box(ux, ux + 0.02, ub + 0.06, ut - 0.06, pz1 - 0.02, pz1)
        yy = ub + 0.09
        while yy < ut - 0.09:
            gg.beam((ux + 0.012, yy, pz0 + 0.02), (ux + 0.012, yy, pz1 - 0.02), 0.022, 0.004, (0.6, 0.8, 0)); yy += 0.025
    # the filter grille on its rotunda-side end: a fine grid in a frame
    gg.box(-ux + 0.08, ux - 0.08, ub + 0.06, ut - 0.06, uz0 - 0.015, uz0)
    for xx in np.arange(-ux + 0.1, ux - 0.1, 0.04): gg.box(xx, xx + 0.006, ub + 0.08, ut - 0.08, uz0 - 0.03, uz0 - 0.015)
    for yy in np.arange(ub + 0.1, ut - 0.1, 0.04): gg.box(-ux + 0.1, ux - 0.1, yy, yy + 0.006, uz0 - 0.03, uz0 - 0.015)
    # the service side (-x): two panels with raised edges, their labels, bolts round them
    for (pz0, pz1) in ((3.2, 4.5), (4.7, 6.0)):
        for (a0, a1, b0, b1) in ((pz0, pz1, ub + 0.08, ub + 0.1), (pz0, pz1, ut - 0.1, ut - 0.08), (pz0, pz0 + 0.02, ub + 0.08, ut - 0.08), (pz1 - 0.02, pz1, ub + 0.08, ut - 0.08)):
            gg.box(-ux - 0.012, -ux, b0, b1, a0, a1)
        gl.box(-ux - 0.004, -ux, ut - 0.3, ut - 0.18, pz0 + 0.15, pz0 + 0.45)
        for zb in np.linspace(pz0 + 0.06, pz1 - 0.06, 6):
            for yb2 in (ub + 0.13, ut - 0.13): gbolt.cylinder((-ux, yb2, zb), (-1, 0, 0), 0.008, 0.008, 8)
    # two hanger cradles: posts from the section's underside, a beam under the unit, bolted
    bot3 = FY[2] - 0.22 - 2 * STEP_Y
    for zc in (3.3, 5.9):
        for sx in (-1, 1):
            gh.box(sx * 0.80, sx * 0.87, ub - 0.08, bot3, zc - 0.035, zc + 0.035)
            gh.box(sx * 0.76, sx * 0.91, bot3 - 0.02, bot3, zc - 0.07, zc + 0.07)
            for dz in (-0.045, 0.045): gbolt.cylinder((sx * 0.835, bot3 - 0.02, zc + dz), (0, -1, 0), 0.01, 0.012, 8)
        gh.box(-0.91, 0.91, ub - 0.08, ub, zc - 0.04, zc + 0.04)
    # the hose outlets under it
    for xo in (-0.35, 0.35): gh.cylinder((xo, ub, 5.7), (0, -1, 0), 0.17, 0.10, 20)
    add_node(s3, f'{J}/TUNNEL-3:jwDark#pca', 'jwDark', gb)
    add_node(s3, f'{J}/TUNNEL-3:jwAluB#pca-grille', 'jwAluB', gg)
    add_node(s3, f'{J}/TUNNEL-3:jwTruss#pca-hangers', 'jwTruss', gh)
    add_node(s3, f'{J}/TUNNEL-3:jwPanel#pca-labels', 'jwPanel', gl)
    add_node(s3, f'{J}/TUNNEL-3:jwAluB#pca-bolts', 'jwAluB', gbolt); stat['pca'] += 1
    # ---- the hoses: the old ones go; one into the metal tube on the lift beam, one in a loose loop on its hook
    pipe = None
    for i, nd in enumerate(N):
        nm = nd.get('name', '')
        if nm == f'{J}/PCA-HOSE' and 'mesh' in nd: del nd['mesh']; stat['hoses_old_gone'] += 1
        if nm == f'{J}/PCA-PIPES:jwDark' and 'mesh' in nd:
            M = np.linalg.inv(rel(ji, s3)) @ rel(ji, i)
            P = np.vstack([acc_read(p['attributes']['POSITION']) for p in g['meshes'][nd['mesh']]['primitives']]).astype(float)
            W = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]
            if W[:, 2].mean() > 0 and (pipe is None or W[:, 2].mean() < pipe[2]):
                pipe = np.array([(W[:, 0].min() + W[:, 0].max()) / 2, W[:, 1].min(), (W[:, 2].min() + W[:, 2].max()) / 2])
    if pipe is None: pipe = np.array([-0.75, -1.85, 9.37])
    a0 = np.array([-0.35, ub - 0.10, 5.7])
    hose_a = smooth([a0, a0 + (0, -0.35, 0.25), (pipe[0] * 0.6 - 0.1, -2.75, (5.7 + pipe[2]) / 2), (pipe[0], pipe[1] - 0.55, pipe[2] - 0.35), (pipe[0], pipe[1] - 0.12, pipe[2]), (pipe[0], pipe[1] + 0.05, pipe[2])], 8)
    b0 = np.array([0.35, ub - 0.10, 5.7])
    # (the loose one hangs in a loop and ends on its hook at the unit's cab-side end: the grille face stays clear)
    hose_b = smooth([b0, b0 + (0.02, -0.4, 0.05), (0.42, -2.95, 6.05), (0.5, -2.75, 6.6), (0.52, -2.05, 6.62), (0.5, -1.66, 6.36)], 8)
    # each hose is its own pivot node at its unit end, with an empty END child at its other fixed end: the game swings it
    # a little about the line through both ends (JetwayRigs: HOSE-A / HOSE-B; Settings > Graphics > Animations)
    for nmh, pts in (('HOSE-A', hose_a), ('HOSE-B', hose_b)):
        o = np.array(pts[0], float); e = np.array(pts[-1], float)
        N.append({'name': f'{J}/{nmh}', 'translation': [float(v) for v in o]}); N[s3].setdefault('children', []).append(len(N) - 1); par[len(N) - 1] = s3; pv = len(N) - 1
        N.append({'name': f'{J}/{nmh}/END', 'translation': [float(v) for v in (e - o)]}); N[pv]['children'] = [len(N) - 1]; par[len(N) - 1] = pv
        gh_ = Geo(); gh_.tube([np.array(p, float) - o for p in pts], 0.14, 14, (0.06, 0.05))
        add_node(pv, f'{J}/{nmh}:jwHose', 'jwHose', gh_)
    hook = Geo(); hook.box(0.42, 0.58, -1.62, -1.50, uz1, uz1 + 0.12); hook.box(0.42, 0.58, -1.62, -1.40, uz1 + 0.10, uz1 + 0.14)
    add_node(s3, f'{J}/TUNNEL-3:jwAluB#hose-hook', 'jwAluB', hook); stat['hoses_new'] += 2


# ------------------------------------------------------------------------------------------------ part 4 (fixes 8, 11)
def set_positions(mesh_i, fn):
    for p in g['meshes'][mesh_i]['primitives']:
        P = acc_read(p['attributes']['POSITION']).astype(np.float64)
        p['attributes']['POSITION'] = acc_new(fn(P).astype(np.float32), 5126, 'VEC3', 34962)

def smooth_normals(mesh_i, mats):
    """Round parts drawn smooth: each corner's normal the average over the faces meeting at that point."""
    for p in g['meshes'][mesh_i]['primitives']:
        if g['materials'][p.get('material', 0)]['name'] not in mats or 'NORMAL' not in p['attributes']: continue
        P = acc_read(p['attributes']['POSITION']).astype(float); Nn = acc_read(p['attributes']['NORMAL']).astype(float)
        key = [tuple(v) for v in np.round(P, 4)]; acc = {}
        for k2, n in zip(key, Nn): acc[k2] = acc.get(k2, 0) + n
        out = np.array([acc[k2] / max(np.linalg.norm(acc[k2]), 1e-9) for k2 in key], np.float32)
        # (where the faces meet at a hard edge - the cap's rim against its flat side - keep the original)
        hard = np.sum(out * Nn, 1) < 0.6
        out[hard] = Nn[hard]
        p['attributes']['NORMAL'] = acc_new(out, 5126, 'VEC3', 34962)

stat4 = {'canopy': 0, 'caps': 0}
for i, nd in enumerate(N):
    nm = nd.get('name', '')
    if 'mesh' not in nd: continue
    # fix 11: the front seal's side bands half as far in towards the doorway (its outside size kept; the top band as is)
    if nm.endswith('/CANOPY') or nm.endswith('/CANOPY:jwFolds') or (nm.startswith('J-') and 'CANOPY' in nm and 'LINING' not in nm):
        def fc(P):
            P = P.copy()
            kk = np.clip(np.round((np.abs(P[:, 0]) - 0.05) / 0.11), 0, 4); grow = kk * 0.08
            zout = (3.4 + grow) / 2; ytop = 2.4 + 0.4 + grow - 0.09
            side = (np.abs(P[:, 2]) > zout - 0.5 - grow / 2 - 0.01) & (P[:, 1] < ytop)
            P[:, 2] = np.where(side, np.sign(P[:, 2]) * (zout - (zout - np.abs(P[:, 2])) * 0.5), P[:, 2])
            return P
        set_positions(nd['mesh'], fc); stat4['canopy'] += 1
    # fix 8: the wheels' centre caps (and the stair wheels', the camera dome) smooth and round
    if ('WHEEL' in nm) or nm.endswith(':jwDome'):
        smooth_normals(nd['mesh'], ('jwHubCap', 'jwDome', 'jwRim')); stat4['caps'] += 1
print('PART4', stat4)

import os
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'jetway_part5.py'), encoding='utf-8').read())
while len(BIN) % 4: BIN.append(0)
g['buffers'][0]['byteLength'] = len(BIN)
js = json.dumps(g, separators=(',', ':')).encode()
while len(js) % 4: js += b' '
out = bytearray(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(BIN)))
out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(BIN), 0x004E4942) + BIN
open(PATH, 'wb').write(out)
print('DETAILS', sorted(JROOTS), stat)
