# Surface-true UVs for the roof tops (metres along the roof itself): v runs down the slope (the seams), u across.
import json, numpy as np
from scipy.interpolate import griddata
import os, sys
HERE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Art', 'Roof')
D = json.load(open(os.path.join(HERE, 'roof_positions.json'))); out = {}
for name, P in D.items():
    P = np.array(P) / 100.0; T = P.reshape(-1, 3, 3)
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); area = np.linalg.norm(n, axis=1); n = -n / np.maximum(area[:, None], 1e-12)   # (Unreal winds them the other way)
    up = n[:, 2] > 0.25
    V = T[up].reshape(-1, 3)
    # seam direction: the horizontal axis the roof falls along most
    gx = np.abs(n[up, 0]).mean(); gy = np.abs(n[up, 1]).mean(); A = 0 if gx >= gy else 1; B = 1 - A
    st = 0.2
    xa = np.arange(V[:, A].min() - 1, V[:, A].max() + 1, st); xb = np.arange(V[:, B].min() - 1, V[:, B].max() + 1, st)
    GA, GB = np.meshgrid(xa, xb, indexing='ij')
    H = griddata(V[:, [A, B]], V[:, 2], (GA, GB), method='linear')
    Hn = griddata(V[:, [A, B]], V[:, 2], (GA, GB), method='nearest'); H = np.where(np.isnan(H), Hn, H)
    dA = np.diff(H, axis=0) / st; dB = np.diff(H, axis=1) / st
    SA = np.vstack([np.zeros((1, H.shape[1])), np.cumsum(np.sqrt(1 + dA ** 2) * st, axis=0)])
    SB = np.hstack([np.zeros((H.shape[0], 1)), np.cumsum(np.sqrt(1 + dB ** 2) * st, axis=1)])
    ia = np.clip((P[:, A] - xa[0]) / st, 0, len(xa) - 1.001); ib = np.clip((P[:, B] - xb[0]) / st, 0, len(xb) - 1.001)
    def bil(G):
        a0 = ia.astype(int); b0 = ib.astype(int); fa = ia - a0; fb = ib - b0
        return G[a0, b0] * (1 - fa) * (1 - fb) + G[a0 + 1, b0] * fa * (1 - fb) + G[a0, b0 + 1] * (1 - fa) * fb + G[a0 + 1, b0 + 1] * fa * fb
    u = bil(SB); v = bil(SA)
    UV = np.stack([u, v], 1)
    # the sides, edges and lip: one plain spot of the zinc (between seams)
    flat = np.repeat(~up, 3); UV[flat] = [0.25, 0.75]
    # how true: each up triangle's edges, length on the roof vs in the texture
    E3 = []; EU = []
    for a, b in ((0, 1), (1, 2), (0, 2)):
        E3.append(np.linalg.norm(T[up, a] - T[up, b], axis=1)); UT = UV.reshape(-1, 3, 2)[up]; EU.append(np.linalg.norm(UT[:, a] - UT[:, b], axis=1))
    r = np.concatenate(EU) / np.maximum(np.concatenate(E3), 1e-9); ok = np.concatenate(E3) > 0.2
    print(name, 'seams along', 'XY'[A], 'up tris', up.sum(), 'of', len(T), 'size on roof %.1f x %.1f m' % (np.ptp(u[np.repeat(up,3)]), np.ptp(v[np.repeat(up,3)])),
          'stretch pct 1/50/99: %s' % np.percentile(r[ok], [1, 50, 99]).round(3))
    out[name] = UV.round(4).tolist()
json.dump(out, open(os.path.join(HERE, 'roof_uvs_arc.json'), 'w'))
