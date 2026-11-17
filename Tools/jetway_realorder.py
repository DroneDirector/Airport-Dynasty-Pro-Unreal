# The jetways' tunnel in the real-world order (owner, 2026-11-17): the three sections telescope smallest at the rotunda
# to biggest at the cab (where the drive unit is), each only a little bigger than the one inside it, so they never cut
# into each other. Done on the jetways' own export (F:/ADP-Export/jetways.glb, the model the game imports as "Jetways"),
# keeping every node, name and mesh number (the game finds the parts by name: TUNNEL-1..3, CAB, DRIVE-UNIT ...), so the
# animation, sounds, lights and docking keep working. Never stretched blindly: per section the walls, glass, posts and
# X-bracing move out (their thickness kept), the roof and ceiling move up (the roof slimmer), the floors step a little
# down towards the cab; what touches them follows (the joint seals, the drive unit's columns, legs and wheels, the stair
# landing and its stair). New: at each joint a frame on the smaller section's front (it slides along the bigger one's
# inside, hides the gap) with a floor plate onto the bigger floor, and a lip on the bigger section's back end outside.
# The bigger sections' handrails go (the smaller one slides where they were); hung exit signs move to the smallest.
#   python Tools/jetway_realorder.py [in.glb] [out.glb]       (default: the backup -> F:/ADP-Export/jetways.glb)
import json, struct, sys, math
import numpy as np

SRC = sys.argv[1] if len(sys.argv) > 1 else 'F:/ADP-Backup/jetway-before-realorder-2026-11-17/jetways.glb'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'F:/ADP-Export/jetways.glb'

# ---- the sections: old measures (his generator) and new ones. Section frame (glTF): x across, y up, z along (0 at the
#      rotunda end, l at the cab end)
IW = 2.5; T = 0.14
L = [10.5, 10.1, 9.7]
G_OLD = [0.22, 0.11, 0.0]
HW_OLD = [IW / 2 + T / 2 + g / 2 for g in G_OLD]          # wall centre line: 1.43 / 1.375 / 1.32
H_OLD = [2.4 + g for g in G_OLD]                           # ceiling: 2.62 / 2.51 / 2.40
FY_OLD = [0.012 * (0.22 - g) / 0.11 for g in G_OLD]        # floor: 0 / 0.012 / 0.024
ENV = 0.11                                                 # how far the slimmed outside reaches past the wall line
STEP_X, STEP_Y = 0.19, 0.26                                # each section this much wider (each side) and taller
HW_NEW = [HW_OLD[2] + i * STEP_X for i in range(3)]        # 1.32 / 1.51 / 1.70
H_NEW = [H_OLD[2] + i * STEP_Y for i in range(3)]          # 2.40 / 2.63 / 2.86
FY_NEW = [0.024, 0.012, 0.0]                               # the smaller section's floor rides on the bigger one's
ROOF_K = 0.35                                              # the roof build-up above the ceiling, slimmer
DRIVE_DX = (HW_NEW[2] + ENV) - (HW_OLD[2] + 0.18)          # the columns move out with the cab-end section's side

# ------------------------------------------------------------------------------------------------ glb in/out
b = open(SRC, 'rb').read()
jl = struct.unpack('<I', b[12:16])[0]
g = json.loads(b[20:20 + jl])
bin_off = 20 + jl
bl = struct.unpack('<I', b[bin_off:bin_off + 4])[0]
BIN = bytearray(b[bin_off + 8: bin_off + 8 + bl])
N = g['nodes']
par = {}
for i, nd in enumerate(N):
    for c in nd.get('children', []): par[c] = i
idx = {nd.get('name'): i for i, nd in enumerate(N)}

def acc_read(ai):
    a = g['accessors'][ai]; bv = g['bufferViews'][a['bufferView']]
    off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    ncomp = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]
    stride = bv.get('byteStride', 0); cnt = a['count']; isz = np.dtype(dt).itemsize * ncomp
    if stride and stride != isz:
        raw = np.frombuffer(BIN, np.uint8, cnt * stride, off).reshape(cnt, stride)[:, :isz].copy()
        return raw.view(dt).reshape(cnt, ncomp)
    return np.frombuffer(BIN, dt, cnt * ncomp, off).reshape(cnt, ncomp).copy()

def acc_new(arr, ctype, atype, target=None):
    """Append data as a new bufferView + accessor; returns the accessor index."""
    global BIN
    while len(BIN) % 4: BIN.append(0)
    raw = arr.tobytes(); off = len(BIN); BIN += raw
    bv = {'buffer': 0, 'byteOffset': off, 'byteLength': len(raw)}
    if target: bv['target'] = target
    g['bufferViews'].append(bv)
    a = {'bufferView': len(g['bufferViews']) - 1, 'componentType': ctype, 'count': int(arr.shape[0]), 'type': atype}
    if atype == 'VEC3' and ctype == 5126:
        a['min'] = [float(v) for v in arr.min(0)]; a['max'] = [float(v) for v in arr.max(0)]
    g['accessors'].append(a)
    return len(g['accessors']) - 1

def trs(nd):
    M = np.eye(4)
    if 'matrix' in nd: return np.array(nd['matrix'], float).reshape(4, 4).T
    s = nd.get('scale', [1, 1, 1]); r = nd.get('rotation', [0, 0, 0, 1]); t = nd.get('translation', [0, 0, 0])
    x, y, z, w = r
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M[:3, :3] = R * np.array(s)[None, :]; M[:3, 3] = t
    return M

def rel(anc, d):
    """Node d's frame expressed in ancestor anc's frame."""
    M = np.eye(4); i = d
    while i != anc:
        M = trs(N[i]) @ M; i = par[i]
    return M

def descendants(i):
    out = []; todo = [i]
    while todo:
        k = todo.pop(); out.append(k); todo += N[k].get('children', [])
    return out

def apply(P, M): return (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]

def set_positions(mesh_i, fn):
    """fn(positions in the node frame) -> new positions; written as new accessors, min/max updated."""
    for p in g['meshes'][mesh_i]['primitives']:
        P = acc_read(p['attributes']['POSITION']).astype(np.float64)
        p['attributes']['POSITION'] = acc_new(fn(P).astype(np.float32), 5126, 'VEC3', 34962)

def drop_tris(mesh_i, keep_fn):
    """keep_fn(positions per triangle, 3xN) -> bool mask of the triangles to keep."""
    for p in g['meshes'][mesh_i]['primitives']:
        if 'indices' not in p: continue
        P = acc_read(p['attributes']['POSITION'])
        I = acc_read(p['indices']).reshape(-1, 3).astype(np.uint32)
        k = keep_fn(P[I[:, 0]], P[I[:, 1]], P[I[:, 2]])
        I2 = I[k].reshape(-1)
        if len(I2) == 0: I2 = I[:1].reshape(-1)   # (a primitive must keep one triangle: a degenerate one)
        p['indices'] = acc_new(I2.astype(np.uint32).reshape(-1, 1), 5125, 'SCALAR', 34963)

# ------------------------------------------------------------------------------------------------ the mapping
def map_section(S, k, centre=False, squash_inside=False):
    """Section k's point (x across, y up, z along) -> its new place."""
    S = S.copy()
    x, y = S[:, 0], S[:, 1]
    # up: the floor's step, the walls and glass stretched to the new ceiling, the roof above it slimmer
    yf = y - FY_OLD[k]
    # (owner, 2026-11-17, photo ref-section-steps-up-and-down: each bigger section reaches as far down under the smaller
    #  one as it reaches up over it - the floor structure deeper underneath by the same step; the walking floor stays)
    deep = (0.22 + k * STEP_Y) / 0.22
    yn = np.where(yf < 0, yf * deep, yn_up(yf, k))
    S[:, 1] = yn + FY_NEW[k]
    if centre: return S
    return map_across(S, x, k, squash_inside)

def yn_up(yf, k):
    return (np.where(yf <= 1.2, yf,
         np.where(yf <= H_OLD[k], 1.2 + (yf - 1.2) * (H_NEW[k] - 1.2) / (H_OLD[k] - 1.2),
                  H_NEW[k] + (yf - H_OLD[k]) * ROOF_K)))

def map_across(S, x, k, squash_inside):
    a = np.abs(x); s = np.sign(x)
    dx = HW_NEW[k] - HW_OLD[k]
    an = np.where(a <= 0.9, a, a + dx)
    # the outside (posts, X-bracing, frames, roof edge) pulled in towards the wall: its thickness kept where it can be
    out = a > HW_OLD[k] + 0.03
    an = np.where(out, np.minimum(HW_NEW[k] + ENV, np.maximum(HW_NEW[k] + 0.03, an - 0.07)), an)
    if squash_inside:
        # (the bigger sections: things on the inside face of the wall flattened against it - the smaller section's
        #  frame slides past them)
        zone = (a > HW_OLD[k] - 0.3) & (a < HW_OLD[k] - 0.07)
        an = np.where(zone, HW_NEW[k] - 0.066 + (a - (HW_OLD[k] - 0.3)) * 0.02, an)
    S[:, 0] = s * an
    return S

stats = {'sections': 0, 'meshes': 0, 'dropped_rails': 0, 'joints': 0}

# ------------------------------------------------------------------------------------------------ new parts
MAT = {m.get('name'): i for i, m in enumerate(g['materials'])}

def box_geo(boxes):
    """Boxes (x0,x1,y0,y1,z0,z1) -> positions, normals, uvs, indices (UVs in metres)."""
    P, Nn, U, I = [], [], [], []
    for (x0, x1, y0, y1, z0, z1) in boxes:
        faces = [((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
                 ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
                 ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
                 ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
                 ((0, 0, 1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
                 ((0, 0, -1), [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)])]
        for n, q in faces:
            b0 = len(P)
            for v in q:
                P.append(v); Nn.append(n)
                U.append((v[2] if n[0] else v[0], v[1]) if n[1] == 0 else (v[0], v[2]))
            I += [b0, b0 + 2, b0 + 1, b0, b0 + 3, b0 + 2]
    return (np.array(P, np.float32), np.array(Nn, np.float32), np.array(U, np.float32), np.array(I, np.uint32))

def add_mesh_node(parent_i, name, mat_name, boxes):
    P, Nn, U, I = box_geo(boxes)
    prim = {'attributes': {'POSITION': acc_new(P, 5126, 'VEC3', 34962), 'NORMAL': acc_new(Nn, 5126, 'VEC3', 34962),
                           'TEXCOORD_0': acc_new(U, 5126, 'VEC2', 34962)},
            'indices': acc_new(I.reshape(-1, 1), 5125, 'SCALAR', 34963), 'material': MAT[mat_name], 'mode': 4}
    g['meshes'].append({'primitives': [prim]})
    N.append({'name': name, 'mesh': len(g['meshes']) - 1})
    N[parent_i].setdefault('children', []).append(len(N) - 1)
    par[len(N) - 1] = parent_i

# ------------------------------------------------------------------------------------------------ each jetway
jets = sorted({nm.split('/')[0] for nm in idx if nm and nm.startswith('J-') and '/' in nm})
for J in jets:
    ji = idx[J]
    sec = [idx[f'{J}/TUNNEL-{k + 1}'] for k in range(3)]
    # the stair landing's own frame node (its mesh nodes are named <J>/STAIR-LANDING:<material>)
    land_root = next((par[i] for nm, i in idx.items() if nm and nm.startswith(f'{J}/STAIR-LANDING:')), None)
    exitpics = [c for c in N[sec[2]].get('children', []) if N[c].get('translation') and 'mesh' in N[c]
                and abs(N[c]['translation'][0]) < 0.1 and 1.8 < N[c]['translation'][2] < 2.8 and 1.7 < N[c]['translation'][1] < 2.4]
    for k, si in enumerate(sec):
        skip = set(descendants(land_root)) if (k == 2 and land_root is not None) else set()
        if k == 2: skip |= set(exitpics)
        for d in descendants(si):
            if d == si or d in skip or 'mesh' not in N[d]: continue
            nm = N[d].get('name', '')
            if k > 0 and nm.endswith(':jwChrome'):
                del N[d]['mesh']; stats['dropped_rails'] += 1; continue
            M = rel(si, d); Mi = np.linalg.inv(M)
            centre = any(nm.endswith(s) for s in (':jwDark', ':jwDome')) or nm.endswith(f':{J}/LED')
            # the smaller sections' joint pieces at their back end (the old order's) go: the new joint is made below
            if k > 0:
                drop_tris(N[d]['mesh'], lambda a, b2, c, M=M: ~((apply(a.astype(float), M)[:, 2] < -0.03) |
                                                              (apply(b2.astype(float), M)[:, 2] < -0.03) |
                                                              (apply(c.astype(float), M)[:, 2] < -0.03)))
            # the hung exit signs of the cab-end section (in the way of the sections sliding inside it) go
            if k == 2:
                def hung(a, b2, c, M=M):
                    out = np.ones(len(a), bool)
                    for v in (a, b2, c):
                        S = apply(v.astype(float), M)
                        out &= (np.abs(S[:, 0]) < 0.45) & (S[:, 2] > 1.8) & (S[:, 2] < 2.8) & (S[:, 1] > 1.7) & (S[:, 1] < H_OLD[2] + 0.45)
                    return ~out
                drop_tris(N[d]['mesh'], hung)
            set_positions(N[d]['mesh'], lambda P, M=M, Mi=Mi, k=k, centre=centre: apply(map_section(apply(P, M), k, centre, squash_inside=k > 0), Mi))
            stats['meshes'] += 1
        # the hung exit-sign pictures on the cab-end section: moved to the smallest section (its front part)
        if k == 2:
            for c in exitpics:
                t = N[c].get('translation')
                if True:
                    N[si]['children'].remove(c); N[sec[0]]['children'].append(c); par[c] = sec[0]
                    N[c]['translation'] = [t[0], H_NEW[0] - 0.3 + FY_NEW[0], L[0] - 2.3 + (t[2] - 2.3)]
                    # its hanger: two thin rods from the ceiling
            add_mesh_node(sec[0], f'{J}/TUNNEL-1:jwAluB#exit-hanger', 'jwAluB',
                          [(-0.29, -0.27, H_NEW[0] - 0.15 + FY_NEW[0], H_NEW[0] + FY_NEW[0], L[0] - 2.31, L[0] - 2.29),
                           (0.27, 0.29, H_NEW[0] - 0.15 + FY_NEW[0], H_NEW[0] + FY_NEW[0], L[0] - 2.31, L[0] - 2.29)])
        stats['sections'] += 1
    # the stair landing on the cab-end section: out with its side, down with its floor; the stair with it
    if land_root is not None:
        t = N[land_root].get('translation', [0, 0, 0]); sgn = 1 if t[0] >= 0 else -1
        dxl = (HW_NEW[2] - HW_OLD[2]); dyl = FY_NEW[2] - FY_OLD[2]
        N[land_root]['translation'] = [t[0] + sgn * dxl, t[1] + dyl, t[2]]
        st = idx.get(f'{J}/SERVICE-STAIR')
        if st is not None:
            Ms = rel(ji, sec[2]); v = Ms[:3, :3] @ np.array([sgn * dxl, dyl, 0.0])
            ts = N[st].get('translation', [0, 0, 0]); N[st]['translation'] = [ts[0] + v[0], ts[1] + v[1], ts[2] + v[2]]
    # the joint seals: the rotunda's follows the smallest section's back end, the cab's the biggest one's front end
    for bel, k, end in ((f'{J}/BELLOWS-ROTUNDA', 0, 'back'), (f'{J}/BELLOWS-ROTUNDA-LINING', 0, 'back'),
                        (f'{J}/BELLOWS-CAB', 2, 'front'), (f'{J}/BELLOWS-CAB-LINING', 2, 'front')):
        bi = idx.get(bel)
        if bi is None or 'mesh' not in N[bi]: continue
        M = np.linalg.inv(rel(ji, sec[k])) @ rel(ji, bi); Mi = np.linalg.inv(M)
        def fb(P, M=M, Mi=Mi, k=k, end=end):
            S = apply(P, M); z = S[:, 2]
            if end == 'back': w = np.clip((z - z.min()) / max(1e-6, 0.0 - z.min()), 0, 1)
            else: w = np.clip((z.max() - z) / max(1e-6, z.max() - L[k]), 0, 1)
            Sm = map_section(S, k)
            return apply(S + (Sm - S) * w[:, None], Mi)
        set_positions(N[bi]['mesh'], fb)
    # the drive unit: columns, legs, wheels, yoke, bogie frame and lift beam out with the cab-end section's sides
    for root in (f'{J}/DRIVE-UNIT', f'{J}/LIFT-SLEEVES'):
        ri = idx.get(root)
        if ri is None: continue
        for d in descendants(ri):
            if d == ri: continue
            nm = N[d].get('name', ''); t = N[d].get('translation')
            moved = False
            if t and abs(abs(t[0]) - 1.78) < 0.02 and ('LIFT-SLEEVE' in nm or 'LIFT-LEG' in nm):
                N[d]['translation'] = [t[0] + math.copysign(DRIVE_DX, t[0]), t[1], t[2]]; moved = True
            elif t and abs(abs(t[0]) - 1.55) < 0.02 and abs(abs(t[2]) - 0.66) < 0.02:   # the wheels
                N[d]['translation'] = [t[0] + math.copysign(DRIVE_DX, t[0]), t[1], t[2]]; moved = True
            if moved: continue
            if 'mesh' in N[d] and ('LIFT-BEAM' in nm or 'PCA-PIPES' in nm):
                M = rel(ri, d); Mi = np.linalg.inv(M)
                def fl(P, M=M, Mi=Mi, beam='LIFT-BEAM' in nm):
                    S = apply(P, M); S[:, 1] -= 2 * STEP_Y
                    if beam:
                        a = np.abs(S[:, 0]); S[:, 0] = np.where(a > 0.6, np.sign(S[:, 0]) * (a + DRIVE_DX), S[:, 0])
                    return apply(S, Mi)
                set_positions(N[d]['mesh'], fl); continue
            if 'mesh' in N[d] and any(s in nm for s in ('DRIVE-YOKE', 'BOGIE:')):
                M = rel(ri, d); Mi = np.linalg.inv(M)
                def fd(P, M=M, Mi=Mi):
                    S = apply(P, M); a = np.abs(S[:, 0]); S[:, 0] = np.where(a > 0.6, np.sign(S[:, 0]) * (a + DRIVE_DX), S[:, 0])
                    return apply(S, Mi)
                set_positions(N[d]['mesh'], fd)
    # the lift cables (loose under the jetway, in its own frame): their ends follow the columns
    for c in N[ji].get('children', []):
        if N[c].get('name', '').endswith('/LIFT-CABLE') and 'mesh' in N[c] and idx.get(f'{J}/LIFT-SLEEVES') is not None:
            ri = idx[f'{J}/LIFT-SLEEVES']; M = np.linalg.inv(rel(ji, ri)) @ rel(ji, c); Mi = np.linalg.inv(M)
            def fc(P, M=M, Mi=Mi):
                S = apply(P, M); a = np.abs(S[:, 0]); S[:, 0] = np.where(a > 0.6, np.sign(S[:, 0]) * (a + DRIVE_DX), S[:, 0])
                return apply(S, Mi)
            set_positions(N[c]['mesh'], fc)
    # ---- the new joints: the smaller section's front frame (slides along the bigger one's inside: hides the gap),
    #      its floor plate onto the bigger floor, and the bigger section's back lip outside
    for k in (0, 1):
        s, bg = k, k + 1
        zf = L[s]                                            # the smaller section's front end (its own frame)
        Xo = HW_NEW[bg] - 0.075; Xi = HW_NEW[s] - 0.07
        rr = (H_NEW[bg] - 1.2) / (H_OLD[bg] - 1.2)   # (the bigger one's ceiling lights hang 0.06 m below it, stretched by rr)
        y0 = FY_NEW[bg] + 0.03; ytop = H_NEW[bg] + FY_NEW[bg] - 0.06 * rr - 0.02; yin = H_NEW[s] + FY_NEW[s]
        add_mesh_node(sec[s], f'{J}/TUNNEL-{s + 1}:jwWall#joint-frame', 'jwWall',
                      [(Xi, Xo, y0, ytop, zf, zf + 0.04), (-Xo, -Xi, y0, ytop, zf, zf + 0.04), (-Xo, Xo, yin, ytop, zf, zf + 0.04)])
        # the floor plate: from the smaller floor down onto the bigger one, its yellow and black leading edge
        fs = FY_NEW[s] + 0.022; fbg = FY_NEW[bg] + 0.022
        add_mesh_node(sec[s], f'{J}/TUNNEL-{s + 1}:jwStud#joint-plate', 'jwStud',
                      [(-Xi + 0.02, Xi - 0.02, fbg, fs + 0.004, zf - 0.05, zf + 0.30)])
        add_mesh_node(sec[s], f'{J}/TUNNEL-{s + 1}:jwHaz#joint-edge', 'jwHaz',
                      [(-Xi + 0.02, Xi - 0.02, fbg, fbg + 0.006, zf + 0.30, zf + 0.36)])
        # the bigger section's back lip: from its outside in to just clear of the smaller section's outside
        xs = HW_NEW[s] + ENV + 0.01; xb = HW_NEW[bg] + ENV
        yb0 = FY_NEW[bg] - 0.22 - bg * STEP_Y; ys0 = FY_NEW[s] - 0.22 - s * STEP_Y - 0.01; yb1 = H_NEW[bg] + FY_NEW[bg] + 0.17; ys1 = H_NEW[s] + FY_NEW[s] + 0.17 + 0.01
        add_mesh_node(sec[bg], f'{J}/TUNNEL-{bg + 1}:jwAluB#joint-lip', 'jwAluB',
                      [(xs, xb, yb0, yb1, -0.03, 0.0), (-xb, -xs, yb0, yb1, -0.03, 0.0), (-xb, xb, ys1, yb1, -0.03, 0.0),
                       (-xb, xb, yb0, ys0, -0.03, 0.0)])
        stats['joints'] += 1

# ------------------------------------------------------------------------------------------------ write
while len(BIN) % 4: BIN.append(0)
g['buffers'][0]['byteLength'] = len(BIN)
js = json.dumps(g, separators=(',', ':')).encode()
while len(js) % 4: js += b' '
out = bytearray()
out += struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(BIN))
out += struct.pack('<II', len(js), 0x4E4F534A) + js
out += struct.pack('<II', len(BIN), 0x004E4942) + BIN
open(OUT, 'wb').write(out)
print('JETWAYS', jets, stats, 'new HW', [round(v, 3) for v in HW_NEW], 'H', [round(v, 3) for v in H_NEW], 'drive dx', round(DRIVE_DX, 3), '->', OUT)
