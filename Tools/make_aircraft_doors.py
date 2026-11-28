"""Writes Data/aircraft-doors.json: every door of the game's A320 (the FlightGear A320-200 model, Art/A320/A320.glb, as
Art/A320/make_a320_fg.py builds it) measured on the model itself - no guessed numbers.
Frame: the model's own cm - x its right, y aft, z up, the root at the nose wheel on the ground (glTF x, z, y * 100).
Per door, from the door's own part (DoorL1/R1/L4/R4, Cargo1/2/3, their .Inner faces and .Placards, DoorHingeL1/R1/L2/R2):
  frame     the closed door's bounds
  skin      [x, y]: the middle of the sill on the outer skin; sill: its height; turn: degrees the skin there is turned
            from straight out (toward the tail; the cater truck, stairs and jetways square up to it)
  out       [x, y]: the skin's outward direction in plan (unit)
  hinge     the hinge line's middle (passenger doors: the hinge arm's forward pivot; cargo 1/2: the top edge; bulk: top edge)
  handle    out: the outside handle (hand IK to open and to close from outside), in: the inside handle (from the cabin/hold)
            passenger doors: the middle of the handle placard on the door (the decal island nearest the door's middle);
            cargo doors: the model has no handle part - the locking handle sits in the middle of the lower edge (Airbus
            layout), so 15 cm up the skin from the lower edge's middle
  outside   where a person stands outside the door, on the stair, jetway, catering or belt-loader top: 0.6 m out, sill height
  inside    where a person stands inside the open door: 0.6 m in (cabin floor / hold floor at the sill)
  dock      where the jetway's or stair's bumper, the cater box's or loader's front stops: 5 cm off the skin at the sill
box: the model's bounds when measured; the game moves the points with the model's real size (GSEStairs.cpp, AircraftDoors.cpp).
The order of "doors" is kept: 0 L1, 1 L4 (the stairs' front and rear door), then R1, R4, Cargo1, Cargo2, Cargo3.
Run: python Tools/make_aircraft_doors.py  (then python Tools/make_aircraft_workpoints.py: the cargo door work points follow)"""
import json, math, os, struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GLB = os.path.join(ROOT, 'Art', 'A320', 'A320.glb')


def load(p):
    b = open(p, 'rb').read(); n = struct.unpack('<I', b[12:16])[0]; g = json.loads(b[20:20 + n])
    off = 20 + n; bl = struct.unpack('<I', b[off:off + 4])[0]
    return g, b[off + 8:off + 8 + bl]


def acc(g, bn, i):
    a = g['accessors'][i]; v = g['bufferViews'][a['bufferView']]
    nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    dt = np.dtype({5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']])
    st = v.get('byteStride', 0); o = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    if st and st != nc * dt.itemsize:
        raw = np.frombuffer(bn, np.uint8, a['count'] * st, o).reshape(-1, st)[:, :nc * dt.itemsize]
        return np.frombuffer(raw.tobytes(), dt).reshape(-1, nc)
    return np.frombuffer(bn, dt, a['count'] * nc, o).reshape(-1, nc)


def local(nd):
    if 'matrix' in nd: return np.array(nd['matrix']).reshape(4, 4).T
    x, y, z, w = nd.get('rotation', [0, 0, 0, 1])
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]) * np.array(nd.get('scale', [1, 1, 1]))
    M = np.eye(4); M[:3, :3] = R; M[:3, 3] = nd.get('translation', [0, 0, 0]); return M


G, BN = load(GLB)
PAR = {c: i for i, nd in enumerate(G['nodes']) for c in nd.get('children', [])}
def world(i): return world(PAR[i]) @ local(G['nodes'][i]) if i in PAR else local(G['nodes'][i])
G2M = np.array([[1, 0, 0], [0, 0, 1], [0, 1, 0]]) * 100.0
IDX = {nd.get('name'): i for i, nd in enumerate(G['nodes'])}


def prims(name):
    """[(points in model cm, triangles)] of a part"""
    i = IDX[name]; W = world(i); out = []
    for pr in G['meshes'][G['nodes'][i]['mesh']]['primitives']:
        P = acc(G, BN, pr['attributes']['POSITION']).astype(float)
        P = (G2M @ ((W[:3, :3] @ P.T).T + W[:3, 3]).T).T
        out.append((P, acc(G, BN, pr['indices']).reshape(-1, 3)))
    return out


def pts(name): return np.vstack([p for p, _ in prims(name)])


def islands(name):
    """the separate pieces of a part (the decals of a placard sheet), as point sets"""
    res = []
    for P, I in prims(name):
        key = [tuple(p) for p in P.round(1)]; par = {}
        def f(a):
            while par.setdefault(a, a) != a: a = par[a]
            return a
        for t in I:
            a = f(key[t[0]]); par[f(key[t[1]])] = a; par[f(key[t[2]])] = a
        grp = {}
        for k in set(key): grp.setdefault(f(k), []).append(k)
        res += [np.array(v) for v in grp.values()]
    return res


def r(v, n=1): return [round(float(x), n) for x in v]


OPENS = {"pax": "lift the handle, the door swings out and forward on its arm (A320Parts.cpp, 5 s)",
         "cargo": "unlock at the handle, the door swings up about its top edge (10 s); from outside only",
         "bulk": "unlock at the handle, the door slides in and swings up inside (3 s)"}


def door(name, kind, hinge_part=None):
    V = pts(name); lo, hi = V.min(0), V.max(0)
    side = 1.0 if (lo[0] + hi[0]) > 0 else -1.0
    yc = (lo[1] + hi[1]) / 2; sill = lo[2]
    low = V[V[:, 2] < sill + 15.0]                          # (the lower edge)
    sx = (low[:, 0] * side).max() * side                   # (its outermost point: the skin at the sill)
    # turn: the skin's lean in plan along the sill - its outermost points fore and aft of the middle
    mid = low
    fr, af = mid[mid[:, 1] < yc], mid[mid[:, 1] >= yc]
    turn = 0.0
    if len(fr) and len(af):
        xf, xa = (fr[:, 0] * side).max(), (af[:, 0] * side).max()
        yf, ya = fr[(fr[:, 0] * side).argmax(), 1], af[(af[:, 0] * side).argmax(), 1]
        turn = math.degrees(math.atan2(xf - xa, max(ya - yf, 1.0)))
        if abs(turn) < 1.0: turn = 0.0
    a = math.radians(turn); o = np.array([side * math.cos(a), math.sin(a)])
    skin = np.array([sx, yc])
    # handles
    if kind == 'pax':
        # (the hinges are forward: the handle placard is the small decal aft of the door's middle, the larger of them)
        isl = [I for I in islands(name + '.Placards') if len(I) <= 6 and I[:, 1].mean() > yc]
        best = max(isl, key=lambda I: np.prod((I.max(0) - I.min(0))[1:]))
        h = (best.min(0) + best.max(0)) / 2; src = 'the handle placard on the door (%s.Placards)' % name
    else:
        band = V   # (the panel has its corners and edges only: x at a height is the same along it)
        band = band[band[:, 2].argsort()]
        z = sill + 15.0; x = np.interp(z, band[:, 2], band[:, 0])
        h = np.array([x, yc, z]); src = 'the lower edge, 15 cm up (no handle part on the model)'
    hout = h + np.array([side * 1.0, 0, 0])
    In = pts(name + '.Inner') if name + '.Inner' in IDX else V
    near = In[(np.abs(In[:, 1] - h[1]) < 12) & (np.abs(In[:, 2] - h[2]) < 12)]
    xi = (near[:, 0] * side).min() * side if len(near) else h[0] - side * 10.0
    if kind != 'pax' or abs(h[0] - xi) < 3.0 or abs(h[0] - xi) > 25.0: xi = h[0] - side * 10.0   # (cargo doors have no inside handle face in the model; a face too thin or a frame part: 10 cm in)
    hin = np.array([xi - side * 1.0, h[1], h[2]])
    if hinge_part:
        HV = pts(hinge_part); hv = HV[HV[:, 1] < HV[:, 1].min() + 3.0]; hg = hv.mean(0)
    else:
        top = V[V[:, 2] > hi[2] - 10.0]; hg = np.array([top[:, 0].mean(), yc, top[:, 2].mean()])
    def at(d, z): p = skin + o * d; return r([p[0], p[1], z])
    return {"name": name.replace('Door', '') if kind == 'pax' else name, "part": name,
            "skin": r(skin), "sill": round(float(sill), 1), "turn": round(turn, 1), "out": r(o, 4),
            "frame": {"min": r(lo), "max": r(hi)}, "hinge": r(hg),
            "handle": {"out": r(hout), "in": r(hin), "from": src, "opens": OPENS[kind if name != 'Cargo3' else 'bulk']},
            "outside": at(60.0, sill), "inside": at(-60.0 if kind == 'pax' else -80.0, sill), "dock": at(5.0, sill)}


D = [door('DoorL1', 'pax', 'DoorHingeL1'), door('DoorL4', 'pax', 'DoorHingeL2'),
     door('DoorR1', 'pax', 'DoorHingeR1'), door('DoorR4', 'pax', 'DoorHingeR2'),
     door('Cargo1', 'cargo'), door('Cargo2', 'cargo'), door('Cargo3', 'cargo')]
F = pts('Fuselage'); ALL = np.vstack([pts(nd['name']) for nd in G['nodes'] if 'mesh' in nd and 'name' in nd])   # (every part: as the game measures it)
note = __doc__.split('\n')
BOXLO = r(ALL.min(0)); BOXLO[2] = 0.0   # (the tyres stand on the ground at the root)
out = {"note": " ".join(l.strip() for l in note if l.strip()),
       "source": "measured on Art/A320/A320.glb by Tools/make_aircraft_doors.py",
       "A320": {"box": {"min": BOXLO, "max": r(ALL.max(0))}, "nose": round(float(F.min(0)[1]), 1),
                "doors": D}}
for d in ('Data', 'Content/Data'):
    if os.path.isdir(os.path.join(ROOT, d)):
        with open(os.path.join(ROOT, d, 'aircraft-doors.json'), 'w', encoding='utf-8') as f: json.dump(out, f, indent=1)
for x in D:
    print('%-7s skin %s sill %.1f turn %.1f handle %s in %s hinge %s' % (x['name'], x['skin'], x['sill'], x['turn'], x['handle']['out'], x['handle']['in'], x['hinge']))
print('box', out['A320']['box'], 'nose', out['A320']['nose'])
