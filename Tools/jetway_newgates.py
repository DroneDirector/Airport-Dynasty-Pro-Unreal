# The jetways at the new gates (owner, 2026-11-17, to-do 85; blueprint docs/Blueprints/16-Gates-*.png):
#   gates 9 and 11 (A320 family, like today's 1-4): one bridge each, J-1 moved along the pier (its stand is J-1's, 81.71 m
#   pitch, so it docks exactly as J-1 does);
#   gate 13 (A380): three bridges - M1L, M2L and U1L (the upper deck);  gates 14-16 (B777): two each - L1 and L2.
# Every new bridge is a copy of J-1 sharing J-1's meshes (instanced, Nanite as J-1), with its own copies of the per-jetway
# lamp materials (LED, markers, beacon, drive lamp) so the game lights each one on its own. The fixed corridor runs
# straight out from the gate room's glass (one, two or three 12 m modules, repeated - never stretched); the moving part
# (rotunda sleeve, tunnels, drive unit, stair, cab sleeve) is swung about the rotunda's axis to face its door; the cab
# is turned back so it stays square to the fuselage. An empty J-x/CAB-TARGET@<type>.<door> node marks where the cab docks
# (its height = the door's sill) - JetwayRigs aims the bridge there. Upper-deck bridge: longer lift columns.
# Run after Tools/jetway_details.py (pipeline: realorder -> finish -> details -> newgates -> find_zfight -> import).
#   python Tools/jetway_newgates.py [glb]
import json, struct, sys, math, re, copy
import numpy as np

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/ADP-Export/jetways.glb'
b = open(PATH, 'rb').read()
jl = struct.unpack('<I', b[12:16])[0]; g = json.loads(b[20:20 + jl])
BIN = b[20 + jl + 8: 20 + jl + 8 + struct.unpack('<I', b[20 + jl:24 + jl])[0]]
N = g['nodes']; par = {}
for i, n in enumerate(N):
    for c in n.get('children', []): par[c] = i
if any(n.get('name', '').startswith('J-9') for n in N): sys.exit('newgates: already done on this file (run the pipeline from jetway_realorder.py)')

def local(n):
    if 'matrix' in n: return np.array(n['matrix'], float).reshape(4, 4).T
    t = n.get('translation', [0, 0, 0]); x, y, z, w = n.get('rotation', [0, 0, 0, 1]); s = n.get('scale', [1, 1, 1])
    R = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)], [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)], [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
    M = np.eye(4); M[:3, :3] = R * np.array(s, float); M[:3, 3] = t; return M
def world(i):
    M = local(N[i])
    while i in par: i = par[i]; M = local(N[i]) @ M
    return M
def set_trs(n, M):
    for k in ('matrix', 'translation', 'rotation', 'scale'): n.pop(k, None)
    t = M[:3, 3]; A = M[:3, :3]; s = np.linalg.norm(A, axis=0); R = A / s
    if np.linalg.det(R) < 0: s[0] = -s[0]; R[:, 0] = -R[:, 0]
    tr = R.trace()
    if tr > 0: S = math.sqrt(tr + 1) * 2; w = 0.25 * S; x = (R[2, 1] - R[1, 2]) / S; y = (R[0, 2] - R[2, 0]) / S; z = (R[1, 0] - R[0, 1]) / S
    elif R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]: S = math.sqrt(1 + R[0, 0] - R[1, 1] - R[2, 2]) * 2; w = (R[2, 1] - R[1, 2]) / S; x = 0.25 * S; y = (R[0, 1] + R[1, 0]) / S; z = (R[0, 2] + R[2, 0]) / S
    elif R[1, 1] > R[2, 2]: S = math.sqrt(1 + R[1, 1] - R[0, 0] - R[2, 2]) * 2; w = (R[0, 2] - R[2, 0]) / S; x = (R[0, 1] + R[1, 0]) / S; y = 0.25 * S; z = (R[1, 2] + R[2, 1]) / S
    else: S = math.sqrt(1 + R[2, 2] - R[0, 0] - R[1, 1]) * 2; w = (R[1, 0] - R[0, 1]) / S; x = (R[0, 2] + R[2, 0]) / S; y = (R[1, 2] + R[2, 1]) / S; z = 0.25 * S
    if np.abs(t).max() > 1e-9: n['translation'] = [float(v) for v in t]
    if abs(w - 1) > 1e-9: n['rotation'] = [float(x), float(y), float(z), float(w)]
    if np.abs(s - 1).max() > 1e-7: n['scale'] = [float(v) for v in s]
def T(dx, dy, dz): M = np.eye(4); M[:3, 3] = (dx, dy, dz); return M
def Ry(a, px, pz):
    c, s = math.cos(a), math.sin(a); M = np.eye(4); M[0, 0] = c; M[0, 2] = s; M[2, 0] = -s; M[2, 2] = c
    return T(px, 0, pz) @ M @ T(-px, 0, -pz)
def Sy(s, y0): M = np.eye(4); M[1, 1] = s; return T(0, y0, 0) @ M @ T(0, -y0, 0)

idx = {n.get('name'): i for i, n in enumerate(N) if n.get('name')}
J1 = idx['J-1']; J1W = world(J1); TOP = par.get(J1)
ROT0 = world(idx['J-1/ROTUNDA-SLEEVE'])[:3, 3]; CAB0 = world(idx['J-1/CAB'])[:3, 3]
LEGS_Y0 = 1.5                         # the lift legs' feet (J-1/LIFT-LEG, 1.5 .. 5.72 m; the sleeves 2.22 .. 7.14 m)
MODULE = 12.0                         # the fixed corridor's module, glass (z 82) to the rotunda's edge
J1_SILL = 3.43                        # J-1's export is docked to an A320's L1 (sill 3.43 m, cab floor 3.30 m)
CAB_FROM_SIDE = 5.65 - 3.95 / 2       # the cab's middle from the fuselage's side, as J-1 at its A320

# (plan x = -glTF x; plan z = glTF z) - stands from Data/airfield.json, doors from the makers' planning documents
# (approximate: A380 M1L 10.3 m behind the nose, M2L 22.5, U1L 14.5 at sill 8.0; B777 L1 6.6, L2 18.2, sill 4.95)
def wide(gate, cx, half, letter, door, nose_to_door, sill, rot_dx, modules, kind):
    return dict(id='J-%d%s' % (gate, letter), rot_x=cx + rot_dx, modules=modules, cab_x=cx + half + CAB_FROM_SIDE,
                cab_z=95.0 + nose_to_door + 0.46, sill=sill, tag='%s.%s' % (kind, door))
SPECS = []
# (gates 9 and 11: the gate rooms are gate 1's copied 80.55 m per module west, their jetway doorway in the glass with
#  them; the stands are 81.71 m apart, so the bridge leaves its doorway and swings a few degrees to its stand)
for gate, sx, door_x in ((9, -225.42, -50.6 - 2 * 80.55), (11, -143.71, -50.6 - 80.55)):
    SPECS.append(dict(id='J-%d' % gate, rot_x=door_x, modules=1, cab_x=sx + 5.65, cab_z=CAB0[2], sill=J1_SILL, tag='A320.L1'))
SPECS += [wide(13, 289.4, 3.57, 'A', 'M1L', 10.3, 5.2, 20.0, 1, 'A380'),
          wide(13, 289.4, 3.57, 'B', 'M2L', 22.5, 5.2, 32.0, 3, 'A380'),
          wide(13, 289.4, 2.96, 'C', 'U1L', 14.5, 8.0, 25.0, 2, 'A380')]
for gate, cx in ((14, 369.2), (15, 441.5), (16, 513.8)):
    SPECS += [wide(gate, cx, 3.10, 'A', 'L1', 6.6, 4.95, 20.0, 1, 'B777'), wide(gate, cx, 3.10, 'B', 'L2', 18.2, 4.95, 30.0, 2, 'B777')]

# J-1's nodes: which stay with the corridor (fixed: the corridor, its legs, signs, lamps, the rotunda's drum and
# machinery), which swing with the bridge, and the cab (swung, then turned back square)
sub = []; st = [J1]
while st: k = st.pop(); sub.append(k); st += N[k].get('children', [])
W0 = {k: world(k) for k in sub}
def bbox_z(k):
    hi = -1e9; s = [k]
    while s:
        q = s.pop(); s += N[q].get('children', [])
        if 'mesh' in N[q]:
            for p in g['meshes'][N[q]['mesh']]['primitives']:
                a = g['accessors'][p['attributes']['POSITION']]; mx = W0[q] @ np.array([*a['max'], 1.0]); mn = W0[q] @ np.array([*a['min'], 1.0])
                hi = max(hi, mx[2], mn[2])
    return hi
FIXED_TOP = ('J-1/CORRIDOR', 'J-1/CORRIDOR-SIGNS', 'J-1/FIRE-EXTINGUISHER', 'J-1/GATE-TRIM', 'J-1/ROTUNDA', 'J-1/ROTUNDA-DRUM', 'J-1/ROTUNDA-MACHINERY')
ROT_TOP = ('J-1/ROTUNDA', 'J-1/ROTUNDA-DRUM', 'J-1/ROTUNDA-MACHINERY')   # (fixed, but out at the corridor's end)
REPEAT_TOP = ('J-1/CORRIDOR', 'J-1/GATE-TRIM')       # repeated per extra module (the gate's own trim at the glass kept once below)
kind = {}
for k in N[J1]['children']:
    nm = N[k].get('name', '')
    if nm.split(':')[0] in ROT_TOP: c = 'rot'
    elif nm.split(':')[0] in FIXED_TOP or (nm.startswith('jwp#') and bbox_z(k) < 95.0): c = 'fixed'
    else: c = 'move'
    s = [k]
    while s: q = s.pop(); kind[q] = c; s += N[q].get('children', [])
cab = idx['J-1/CAB']; s = [cab]
while s:
    q = s.pop()
    if N[q].get('name', '').startswith('J-1/CAB-SLEEVE'): continue
    kind[q] = 'cab'; s += N[q].get('children', [])
legs = set()
for k in sub:
    if N[k].get('name', '') == 'J-1/LIFT-LEG':
        s = [k]
        while s: q = s.pop(); legs.add(q); s += N[q].get('children', [])

# the per-jetway lamp materials: each new jetway gets its own copies (meshes using them copied with the new material)
mat_ix = {m.get('name'): i for i, m in enumerate(g['materials'])}
OWN = [m for m in mat_ix if m.startswith('J-1/')]
rn = lambda s, Id: re.sub(r'J-1(?=/|$|:)', Id, s)

stat = {'jetways': 0, 'nodes': 0, 'meshes': 0, 'modules': 0}
for sp in SPECS:
    Id = sp['id']
    own = {}
    for m in OWN:
        nm = copy.deepcopy(g['materials'][mat_ix[m]]); nm['name'] = rn(m, Id); g['materials'].append(nm); own[mat_ix[m]] = len(g['materials']) - 1
    mesh_map = {}
    def mesh_of(mi):
        if mi in mesh_map: return mesh_map[mi]
        prims = g['meshes'][mi]['primitives']
        if any(p.get('material') in own for p in prims):
            nm = copy.deepcopy(g['meshes'][mi])
            for p in nm['primitives']:
                if p.get('material') in own: p['material'] = own[p['material']]
            g['meshes'].append(nm); mesh_map[mi] = len(g['meshes']) - 1; stat['meshes'] += 1
        else: mesh_map[mi] = mi
        return mesh_map[mi]
    # where: the corridor along plan x at the rotunda's x; the rotunda 12 m per extra module further out
    gx = -sp['rot_x']; dz_mod = MODULE * (sp['modules'] - 1)
    Rn = np.array([gx, ROT0[1], ROT0[2] + dz_mod]); Cn = np.array([-sp['cab_x'], CAB0[1], sp['cab_z']])
    v0 = CAB0 - ROT0; v1 = Cn - Rn
    a = math.atan2(v1[0], v1[2]) - math.atan2(v0[0], v0[2])
    FIX = T(gx - J1W[0, 3], 0, 0)
    MOVE = T(*(Rn - ROT0)) @ Ry(a, ROT0[0], ROT0[2])
    CABM = MOVE @ Ry(-a, CAB0[0], CAB0[2])
    H = (sp['sill'] - J1_SILL)
    LEG = Sy((4.22 + max(0.0, H - 2.0)) / 4.22, LEGS_Y0) if H > 3.0 else np.eye(4)
    def place(k, extra=np.eye(4)):
        c = kind.get(k, 'fixed')
        if k == J1: return FIX @ W0[k]
        if c == 'fixed': return extra @ FIX @ W0[k]
        if c == 'rot': return T(0, 0, dz_mod) @ FIX @ W0[k]
        if c == 'cab': return CABM @ W0[k]
        return MOVE @ (LEG if k in legs else np.eye(4)) @ W0[k]
    def clone(k, parent_new, parent_W, extra=np.eye(4), only=None):
        n = copy.deepcopy({kk: vv for kk, vv in N[k].items() if kk != 'children'})
        n['name'] = rn(n.get('name', ''), Id) if n.get('name') else n.get('name')
        if 'mesh' in n: n['mesh'] = mesh_of(n['mesh'])
        Wn = place(k, extra); set_trs(n, np.linalg.inv(parent_W) @ Wn)
        N.append(n); me = len(N) - 1; stat['nodes'] += 1
        if parent_new is not None: N[parent_new].setdefault('children', []).append(me); par[me] = parent_new
        for c in N[k].get('children', []):
            if only is None or c in only: clone(c, me, Wn, extra)
        return me
    TOPW = world(TOP) if TOP is not None else np.eye(4)
    root = clone(J1, TOP, TOPW, only=set(N[J1]['children']))   # (clone hangs it under the terminal's root)
    if TOP is None: g['scenes'][0]['nodes'].append(root)
    RW = place(J1)
    # the extra corridor modules: the corridor and its legs again, 12 m further out each
    for m in range(1, sp['modules']):
        for k in N[J1]['children']:
            nm = N[k].get('name', '')
            if nm.split(':')[0] in REPEAT_TOP and nm != 'J-1/GATE-TRIM:jwTruss' or (nm.startswith('jwp#') and kind[k] == 'fixed'):
                cnode = clone(k, root, RW, extra=T(0, 0, MODULE * m)); N[cnode]['name'] = (N[cnode].get('name') or '') + '#m%d' % m
        stat['modules'] += 1
    # where the cab docks (its floor at the door's sill less 0.13 m, as J-1's), with the door it serves
    tgt = {'name': '%s/CAB-TARGET@%s' % (Id, sp['tag'])}
    set_trs(tgt, np.linalg.inv(RW) @ T(Cn[0], CAB0[1] + H, Cn[2])); N.append(tgt); N[root]['children'].append(len(N) - 1); par[len(N) - 1] = root
    stat['jetways'] += 1
    print('  %-6s rotunda x %7.2f z %6.2f  cab x %7.2f z %6.2f  reach %.1f m  turn %+.1f deg  sill %.2f  modules %d' % (
        Id, sp['rot_x'], Rn[2], sp['cab_x'], sp['cab_z'], float(np.linalg.norm((v1[0], v1[2]))), math.degrees(a), sp['sill'], sp['modules']))
# (J-1 itself gets its target too, so every jetway reads the same way)
tg1 = {'name': 'J-1/CAB-TARGET@A320.L1'}; set_trs(tg1, np.linalg.inv(J1W) @ T(*CAB0)); N.append(tg1); N[J1]['children'].append(len(N) - 1)

js = json.dumps(g, separators=(',', ':')).encode()
while len(js) % 4: js += b' '
out = bytearray(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(BIN)))
out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(BIN), 0x004E4942) + BIN
open(PATH, 'wb').write(out)
print('NEWGATES', stat)
