import bpy, bmesh, json, sys, numpy as np
import os
S = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Art', 'Roof')
D = json.load(open(S + '/roof_positions.json')); REF = json.load(open(S + '/roof_uvs_arc.json')); out = {}
for name, P in D.items():
    P = np.array(P) / 100.0; T = P.reshape(-1, 3, 3)
    n = -np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); n /= np.linalg.norm(n, axis=1)[:, None] + 1e-12
    up = np.where(n[:, 2] > 0.25)[0]
    key = {}; verts = []
    def vid(p):
        k = tuple(np.round(p, 3))
        if k not in key: key[k] = len(verts); verts.append(p)
        return key[k]
    faces = [[vid(T[t, 0]), vid(T[t, 1]), vid(T[t, 2])] for t in up]
    me = bpy.data.meshes.new(name); me.from_pydata([tuple(v) for v in verts], [], faces); me.update()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob; ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.unwrap(method='CONFORMAL', margin=0.0)
    bpy.ops.uv.select_all(action='SELECT'); bpy.ops.uv.minimize_stretch(iterations=300)
    bpy.ops.object.mode_set(mode='OBJECT')
    uvl = me.uv_layers.active.data
    U = np.array([[uvl[p.loop_start + k].uv[:] for k in range(3)] for p in me.polygons])   # per up tri, corners in order
    # metric scale, turned so the seams run as the arc-length layout's (down the slope): least squares similarity
    R = np.array(REF[name]).reshape(-1, 3, 2)[up]
    # each separate piece of the roof on its own: its turn from the arc-length layout (seams down the slope), its size
    # from its area (the texture's area equal to the roof's), its place from that layout
    F = np.array(faces); par = list(range(len(verts)))
    def root(i):
        while par[i] != i: par[i] = par[par[i]]; i = par[i]
        return i
    for f in F:
        for k in (1, 2): par[root(f[k])] = root(f[0])
    isl = np.array([root(f[0]) for f in F])
    M = np.zeros_like(U)
    def tri_area2(X):
        if X.shape[-1] == 2: return np.abs(np.cross(X[:, 1] - X[:, 0], X[:, 2] - X[:, 0]))
        return np.linalg.norm(np.cross(X[:, 1] - X[:, 0], X[:, 2] - X[:, 0]), axis=1)
    for g in np.unique(isl):
        sel = isl == g
        a = U[sel].reshape(-1, 2); b = R[sel].reshape(-1, 2)
        ac = a - a.mean(0); bc = b - b.mean(0)
        Za = ac[:, 0] + 1j * ac[:, 1]; Zb = bc[:, 0] + 1j * bc[:, 1]
        s = (np.conj(Za) @ Zb) / max(abs(np.conj(Za) @ Za), 1e-12)
        rot = s / max(abs(s), 1e-12)
        k = np.sqrt(tri_area2(T[up][sel]).sum() / max(tri_area2(U[sel]).sum(), 1e-12))
        m = rot * k * Za
        M[sel] = (np.stack([m.real, m.imag], 1) + b.mean(0)).reshape(-1, 3, 2)
    print('ROOF', name, 'pieces', len(np.unique(isl)))
    # the true size: the whole roof's area in the texture equal to its area on the roof
    full = np.array(REF[name]).reshape(-1, 3, 2).copy(); full[up] = M
    E3 = []; EU = []
    for i, j in ((0, 1), (1, 2), (0, 2)):
        E3.append(np.linalg.norm(T[up, i] - T[up, j], axis=1)); EU.append(np.linalg.norm(full[up, i] - full[up, j], axis=1))
    r = np.concatenate(EU) / np.concatenate(E3); ok = np.concatenate(E3) > 0.2
    print('ROOF', name, 'conformal stretch pct 1/5/50/95/99:', np.percentile(r[ok], [1, 5, 50, 95, 99]).round(3), 'over 10%%: %.1f%%' % (100 * (np.abs(r[ok] - 1) > 0.1).mean()))
    out[name] = full.reshape(-1, 2).round(4).tolist()
json.dump(out, open(S + '/roof_uvs.json', 'w'))
