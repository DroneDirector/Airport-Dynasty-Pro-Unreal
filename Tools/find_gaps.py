# Finds loose pieces in a model .glb: every separate piece of mesh (an island: triangles joined by shared corners) must touch,
# cross or sit within 1 mm of another piece, and the whole model must hang together as ONE group. Moving parts are judged
# in the pose they are saved in (closed / stowed). Prints two lists:
#   parts  - named parts that touch no other part at all (the owner's rule: no floating part)
#   loose  - pieces (inside any part) that are not joined to the main group, with where they are and how big
#   python Tools/find_gaps.py Art/HangarEquipment/HangarGPU/hangargpu.glb [out.json] [tol_m=0.001]
import json, sys
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from find_zfight import load_glb, parts

def islands(tris):
    v = tris.reshape(-1, 3); key = np.round(v / 1e-5).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True); inv = inv.reshape(-1, 3)
    nv = inv.max() + 1; nt = len(inv)
    rows = np.repeat(np.arange(nt), 3); g = coo_matrix((np.ones(3 * nt), (rows, nt + inv.ravel())), shape=(nt + nv, nt + nv))
    _, lab = connected_components(g, directed=False)
    lab = lab[:nt]; return [tris[lab == u] for u in np.unique(lab)]

def seg_dist(p, a, b):
    ab = b - a; t = np.clip(np.einsum('ptk,ptk->pt', p - a, ab) / np.maximum(np.einsum('ptk,ptk->pt', ab, ab), 1e-18), 0, 1)
    return np.linalg.norm(p - (a + ab * t[..., None]), axis=-1)

def pt_tri(P, T):
    """smallest distance from points P (n,3) to triangles T (m,3,3)"""
    best = np.inf
    a, b, c = T[:, 0], T[:, 1], T[:, 2]; n = np.cross(b - a, c - a); ln = np.linalg.norm(n, axis=1); ok = ln > 1e-14
    n = n / np.maximum(ln, 1e-14)[:, None]
    step = max(1, int(2e6 // max(1, len(T))))
    for i in range(0, len(P), step):
        p = P[i:i + step][:, None, :]
        d = np.einsum('ptk,tk->pt', p - a[None], n); q = p - d[..., None] * n[None]
        inside = ok[None] & (np.einsum('ptk,tk->pt', np.cross(b - a, q - a[None]), n) >= 0) & \
                 (np.einsum('ptk,tk->pt', np.cross(c - b, q - b[None]), n) >= 0) & (np.einsum('ptk,tk->pt', np.cross(a - c, q - c[None]), n) >= 0)
        e = np.minimum(np.minimum(seg_dist(p, a[None], b[None]), seg_dist(p, b[None], c[None])), seg_dist(p, c[None], a[None]))
        best = min(best, float(np.where(inside, np.abs(d), e).min()))
    return best

def edges_cross(E0, E1, T):
    """does any edge (E0->E1) pass through any triangle T (Moller-Trumbore)"""
    a, b, c = T[:, 0], T[:, 1], T[:, 2]; e1 = b - a; e2 = c - a
    step = max(1, int(2e6 // max(1, len(T))))
    for i in range(0, len(E0), step):
        o = E0[i:i + step][:, None]; dv = (E1[i:i + step] - E0[i:i + step])[:, None]
        h = np.cross(dv, e2[None]); det = np.einsum('etk,tk->et', h, e1)
        good = np.abs(det) > 1e-14; f = 1.0 / np.where(good, det, 1.0)
        s = o - a[None]; u = f * np.einsum('etk,etk->et', s, h); qv = np.cross(s, e1[None])
        v = f * np.einsum('etk,etk->et', np.broadcast_to(dv, qv.shape), qv); t = f * np.einsum('etk,tk->et', qv, e2)
        if np.any(good & (u >= 0) & (v >= 0) & (u + v <= 1) & (t >= 0) & (t <= 1)): return True
    return False

def near(tA, tB, tol):
    lo = np.maximum(tA.reshape(-1, 3).min(0), tB.reshape(-1, 3).min(0)) - tol
    hi = np.minimum(tA.reshape(-1, 3).max(0), tB.reshape(-1, 3).max(0)) + tol
    def clip_t(t): m = np.all((t.max(1) >= lo) & (t.min(1) <= hi), axis=1); return t[m]
    A, B = clip_t(tA), clip_t(tB)
    if not len(A) or not len(B): return False
    for X, Y in ((A, B), (B, A)):
        P = np.unique(X.reshape(-1, 3), axis=0); P = P[np.all((P >= lo) & (P <= hi), axis=1)]
        if len(P) and pt_tri(P, Y) <= tol: return True
    for X, Y in ((A, B), (B, A)):
        E0 = np.concatenate([X[:, 0], X[:, 1], X[:, 2]]); E1 = np.concatenate([X[:, 1], X[:, 2], X[:, 0]])
        if edges_cross(E0, E1, Y): return True
    return False

def main(glb, out_json=None, tol=0.001):
    js, bin_ = load_glb(glb)
    pieces = []                                                    # (part, tris)
    for name, tris in parts(js, bin_):
        for isl in islands(tris): pieces.append((name, isl))
    n = len(pieces)
    lo = np.array([p[1].reshape(-1, 3).min(0) for p in pieces]); hi = np.array([p[1].reshape(-1, 3).max(0) for p in pieces])
    cand = np.all((lo[:, None] <= hi[None] + tol) & (hi[:, None] >= lo[None] - tol), axis=2)
    I, J = np.nonzero(np.triu(cand, 1)); link = []
    for i, j in zip(I, J):
        if near(pieces[i][1], pieces[j][1], tol): link.append((i, j))
    g = coo_matrix((np.ones(len(link)), ([i for i, _ in link], [j for _, j in link])), shape=(n, n))
    k, lab = connected_components(g, directed=False)
    size = np.bincount(lab, weights=[len(p[1]) for p in pieces]); main_g = int(size.argmax())
    names = sorted({p[0] for p in pieces})
    touch = {nm: False for nm in names}
    for i, j in link:
        if pieces[i][0] != pieces[j][0]: touch[pieces[i][0]] = touch[pieces[j][0]] = True
    lone = [nm for nm in names if not touch[nm]] if len(names) > 1 else []
    loose = [{'part': pieces[i][0], 'at': [round(float(x), 3) for x in (lo[i] + hi[i]) / 2],
              'size': [round(float(x), 3) for x in hi[i] - lo[i]], 'tris': len(pieces[i][1])}
             for i in range(n) if lab[i] != main_g]
    print(f'{n} pieces in {len(names)} parts, {len(link)} contacts, {k} groups')
    print(f'parts touching no other part: {len(lone)}', *lone)
    print(f'loose pieces (not joined to the main group): {len(loose)}')
    for L in loose[:60]: print('  ', L['part'], 'at', L['at'], 'size', L['size'], L['tris'], 'tris')
    if out_json: json.dump({'tol_m': tol, 'lone_parts': lone, 'loose': loose}, open(out_json, 'w'), indent=1)
    return len(lone) + len(loose)

if __name__ == '__main__':
    bad = main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None, float(sys.argv[3]) if len(sys.argv) > 3 else 0.001)
    sys.exit(1 if bad else 0)
