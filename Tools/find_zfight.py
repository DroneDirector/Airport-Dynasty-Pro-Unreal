# Finds surfaces that fight for the same place (z-fighting: two parts' faces lying on top of each other, so the picture
# flickers between them), in an exported model (.glb): every part's faces are sampled, and a sample that lies within
# 0.8 mm of another part's face, facing the same way, is a fight. Prints the pairs of parts (node names) with how much
# of the smaller one fights, and writes them to the given .json for the game to separate (by a hair, along the face).
#   python Tools/find_zfight.py F:/ADP-Export/firetruck1.glb Data/firetruck-zfight.json
import json, struct, sys
import numpy as np

def load_glb(path):
    b = open(path, 'rb').read()
    _, _, length = struct.unpack_from('<III', b, 0)
    off = 12; js = None; bin_ = None
    while off < length:
        clen, ctype = struct.unpack_from('<II', b, off); off += 8
        chunk = b[off:off + clen]; off += clen
        if ctype == 0x4E4F534A: js = json.loads(chunk)
        elif ctype == 0x004E4942: bin_ = chunk
    return js, bin_

def accessor(js, bin_, i):
    a = js['accessors'][i]; bv = js['bufferViews'][a['bufferView']]
    comp = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]
    n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', 0) or n * np.dtype(comp).itemsize
    raw = np.frombuffer(bin_, dtype=np.uint8, count=stride * (a['count'] - 1) + n * np.dtype(comp).itemsize, offset=start)
    out = np.lib.stride_tricks.as_strided(raw.view(comp), shape=(a['count'], n), strides=(stride, np.dtype(comp).itemsize))
    return np.array(out, dtype=np.float64 if comp == np.float32 else np.int64)

def mat_of(node):
    from math import isclose
    T = np.eye(4)
    if 'matrix' in node: return np.array(node['matrix']).reshape(4, 4).T
    t = node.get('translation', [0, 0, 0]); r = node.get('rotation', [0, 0, 0, 1]); s = node.get('scale', [1, 1, 1])
    x, y, z, w = r
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    T[:3, :3] = R * np.array(s); T[:3, 3] = t
    return T

def parts(js, bin_):
    out = []
    def walk(i, P):
        n = js['nodes'][i]; W = P @ mat_of(n)
        if 'mesh' in n:
            for pr in js['meshes'][n['mesh']]['primitives']:
                if pr.get('mode', 4) != 4: continue
                v = accessor(js, bin_, pr['attributes']['POSITION'])
                idx = accessor(js, bin_, pr['indices']).reshape(-1, 3) if 'indices' in pr else np.arange(len(v)).reshape(-1, 3)
                vw = (W @ np.c_[v, np.ones(len(v))].T).T[:, :3]
                out.append((n.get('name', '?'), vw[idx]))
        for c in n.get('children', []): walk(c, W)
    for r in js['scenes'][js.get('scene', 0)]['nodes']: walk(r, np.eye(4))
    return out

def point_tri_dist(p, tri):
    # distance from points p (N,3) to triangles tri (M,3,3), all pairs -> (N,M); plus the triangles' normals
    a, b, c = tri[:, 0], tri[:, 1], tri[:, 2]
    n = np.cross(b - a, c - a); nl = np.linalg.norm(n, axis=1) + 1e-12; n = n / nl[:, None]
    d = np.einsum('mk,nmk->nm', n, p[:, None, :] - a[None])
    q = p[:, None, :] - d[..., None] * n[None]
    # inside test by barycentric signs
    def side(u, v):
        return np.einsum('mk,nmk->nm', n, np.cross((v - u)[None], q - u[None]))
    inside = (side(a, b) >= -1e-9) & (side(b, c) >= -1e-9) & (side(c, a) >= -1e-9)
    return np.where(inside, np.abs(d), np.inf), n

def main(glb, out_json):
    js, bin_ = load_glb(glb)
    P = parts(js, bin_)
    boxes = [(t.reshape(-1, 3).min(0) - 0.002, t.reshape(-1, 3).max(0) + 0.002) for _, t in P]
    rng = np.random.default_rng(1)
    hits = {}
    dirs = {}
    areas = {n: float(np.linalg.norm(np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0]), axis=1).sum()) / 2 for n, t in P}
    for i, (na, ta) in enumerate(P):
        # samples on part A, area-weighted, with A's face normals
        ea, eb = ta[:, 1] - ta[:, 0], ta[:, 2] - ta[:, 0]
        cr = np.cross(ea, eb); area = np.linalg.norm(cr, axis=1)
        if area.sum() <= 0: continue
        k = min(300, max(20, len(ta) * 2))
        f = rng.choice(len(ta), size=k, p=area / area.sum())
        u, v = rng.random(k), rng.random(k); m = u + v > 1; u[m], v[m] = 1 - u[m], 1 - v[m]
        pts = ta[f, 0] + ea[f] * u[:, None] + eb[f] * v[:, None]
        nA = cr[f] / (area[f][:, None] + 1e-12)
        for j, (nb, tb) in enumerate(P):
            if j == i or nb == na: continue
            lo, hi = boxes[j]
            inb = np.all((pts >= lo) & (pts <= hi), axis=1)
            if not inb.any(): continue
            d, nB = point_tri_dist(pts[inb], tb)
            close = d < 0.0008
            par = np.abs(nA[inb] @ nB.T) > 0.98
            fight = (close & par).any(axis=1)
            frac = fight.sum() / k
            if frac > 0.02:
                key = tuple(sorted((na, nb)))
                hits[key] = max(hits.get(key, 0), frac)
                nrm = nA[inb][fight].mean(0)
                if np.linalg.norm(nrm) > 1e-6: dirs.setdefault((na, nb), []).append(nrm / np.linalg.norm(nrm))
    rows = sorted(hits.items(), key=lambda kv: -kv[1])
    for (a, b), fr in rows: print('%-34s %-34s %4.0f %%' % (a, b, fr * 100))
    # which part moves: the smaller one of the pair (the stripe on the door, the lens on the band, the cushion on its
    # base), 1.5 mm out along the faces they share; a part in several fights takes the largest move on each axis
    move = {}
    for (a, b), fr in rows:
        mover = a if areas.get(a, 0) <= areas.get(b, 0) else b
        other = b if mover == a else a
        ds = dirs.get((mover, other)) or [-d for d in dirs.get((other, mover), [])]
        if not ds: continue
        d = np.mean(ds, axis=0); d = d / (np.linalg.norm(d) + 1e-12) * 0.0015
        cur = move.get(mover, np.zeros(3))
        move[mover] = np.where(np.abs(d) > np.abs(cur), d, cur)
    json.dump({'pairs': [{'a': a, 'b': b, 'share': round(fr, 3)} for (a, b), fr in rows],
               'nudge': {k: [round(float(x), 5) for x in v] for k, v in move.items()}}, open(out_json, 'w'), indent=1)
    print('%d parts move by a hair' % len(move))
    print('%d pairs fight' % len(rows))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
