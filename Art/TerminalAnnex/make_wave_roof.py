# The terminal's one continuous wave roof (owner, 2026-11-23; the shape and why: Tools/wave_roof.py), in the terminal's own
# materials (slots named after /Game/Terminal/Materials) and in pieces the game instances:
#   wave_soffit_m, wave_top_m, wave_fascia_m, wave_edge_m: underside, top, white edges, the line of light of one bow of the roof
#     (90 m, three waves, valley to valley): 7 copies side by side
#   ..._w / ..._e: the west and the east end (a bow and the 6 m past its valley), their free edge rounded off like the front
#   (underside and top each a piece of their own, as before; Tools/import_annex.py gives them their full Nanite backup mesh and a
#    two-sided distance field - without it Lumen took the space under the sheets as solid: dark and blotchy under the roof)
# (the kerb's tree pillars: copies of the good part's own two, Tools/terminal_wave.py)
# Underside: MI_T_timber, laid as it was (x/8, z/8). Top: MI_T_roofZinc, laid in metres along the roof itself (an
# as-rigid-as-possible unwrap of one bow with its two ends joined, so the seams run on from bow to bow), the photo's 6 m a
# whole number of times per bow. Front edge: the 84 cm white round and its light line; the two ends the same round, the back
# edge (z 34, open before) closed with the edge's white. Smooth normals from the shape itself: nothing shows at a joint.
# Blender axes for the game's import: (x, y, z) = (-plan x, -plan z, plan y); each bow's middle at x 0.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_wave_roof.py
#   then Tools/import_annex.py <names>, Tools/terminal_edit.py (the names are printed: WAVE names)
import bpy, json, math, os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import wave_roof as W
bpy.ops.wm.read_factory_settings(use_empty=True)
DX, NZ = 0.75, 56                      # the grid: 0.75 m along (the crests and valleys on it), 56 rows front to back
TIMBER_U = 68.0 / 6.0 / 90.0           # the planks' picture is 1.333 m long: 68 of them per bow (67.5 at its true size, +0.7 %)
TILE_H = 6.0                           # the zinc photo along the roof (Tools/set_roof.py)


def dh(u, z, end):
    e = 1e-3
    return (W.h_u(u + e, z, end) - W.h_u(u - e, z, end)) / (2 * e), -0.012 * (z - 16.0)


def grid(u0, u1, w, e):
    n = int(round((u1 - u0) / DX)); us = [u0 + (u1 - u0) * i / n for i in range(n + 1)]
    def ends(u): return (w and u < 0) or (e and u > 0)
    P = []
    for u in us:
        zs = W.bow(u, ends(u))
        P.append([(u, W.h_u(u, zs + (W.Z1 - zs) * j / NZ, ends(u)), zs + (W.Z1 - zs) * j / NZ) for j in range(NZ + 1)])
    return us, np.array(P), ends


def zinc_uv():
    """one bow unwrapped with its two ends joined: U across the slope (metres), V along the roof; V(+90 m) = V + whole photos"""
    us, P, _ = grid(-45.0, 45.0, False, False); n, m = len(us) - 1, NZ
    sx = np.cumsum([0] + [math.hypot(DX, W.h_u(us[i + 1], 16) - W.h_u(us[i], 16)) for i in range(n)])
    Lv = round(sx[-1] / TILE_H) * TILE_H
    zz = np.linspace(-17, 35, 1041); az = np.concatenate([[0], np.cumsum(np.hypot(np.diff(zz), np.diff(0.006 * (zz - 16) ** 2)))])
    V0 = np.repeat((sx / sx[-1] * Lv)[:, None], m + 1, axis=1); U0 = np.interp(P[:, :, 2], zz, az)
    nv = n * (m + 1); tris = []
    for i in range(n):
        for j in range(m):
            a, b, c, d = (i, j), (i + 1, j), (i, j + 1), (i + 1, j + 1)
            tris += [(a, b, c), (c, b, d)] if (i + j) % 2 else [(a, b, d), (a, d, c)]
    T3 = np.array([[(q[0] % n) * (m + 1) + q[1] for q in t] for t in tris])
    WR = np.array([[1.0 if q[0] == n else 0.0 for q in t] for t in tris])          # which corners lie one bow on
    L2 = []
    for t in tris:
        p = [P[q[0], q[1]] for q in t]; e1 = p[1] - p[0]; e2 = p[2] - p[0]; l1 = np.linalg.norm(e1); ex = e1 / l1; x2 = e2 @ ex
        L2.append([[0, 0], [l1, 0], [x2, np.linalg.norm(e2 - x2 * ex)]])
    L2 = np.array(L2); cot = np.zeros((len(T3), 3))
    for k in range(3):
        o = L2[:, k]; a = L2[:, (k + 1) % 3] - o; b = L2[:, (k + 2) % 3] - o
        cot[:, k] = (a * b).sum(1) / np.maximum(1e-9, np.abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]))
    uv = np.stack([U0[:n].ravel(), V0[:n].ravel()], 1)
    def corner(uv):
        q = uv[T3].copy(); q[:, :, 1] += WR * Lv; return q
    ED = [(T3[:, (k + 1) % 3], T3[:, (k + 2) % 3], cot[:, k] * 0.5, (k + 1) % 3, (k + 2) % 3) for k in range(3)]
    def acc(i, v): return np.stack([np.bincount(i, v[:, 0], nv), np.bincount(i, v[:, 1], nv)], 1)
    def lap(y):
        out = np.zeros_like(y)
        for i, j, w, _, _ in ED:
            d = w[:, None] * (y[i] - y[j]); out += acc(i, d) - acc(j, d)
        return out
    diag = sum(np.bincount(i, w, nv) + np.bincount(j, w, nv) for i, j, w, _, _ in ED)
    q = corner(uv); s = np.sign(np.median(np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0]))); L2[:, :, 1] *= s
    for it in range(40):
        q = corner(uv); S = np.zeros((len(T3), 2, 2))
        for i, j, w, a, b in ED: S += w[:, None, None] * np.einsum('ni,nj->nij', L2[:, a] - L2[:, b], q[:, a] - q[:, b])
        th = np.arctan2(S[:, 0, 1] - S[:, 1, 0], S[:, 0, 0] + S[:, 1, 1]); c, sn = np.cos(th), np.sin(th)
        R = np.stack([np.stack([c, sn], 1), np.stack([-sn, c], 1)], 1)
        rhs = np.zeros((nv, 2))
        for i, j, w, a, b in ED:
            d = np.einsum('ni,nij->nj', L2[:, a] - L2[:, b], R); d[:, 1] -= (WR[:, a] - WR[:, b]) * Lv
            d = w[:, None] * d; rhs += acc(i, d) - acc(j, d)
        y = uv.copy(); r = rhs - lap(y); z = r / diag[:, None]; p = z.copy(); rz = (r * z).sum(0)
        for _ in range(300):
            if np.abs(r).max() < 1e-5: break
            Ap = lap(p); al = rz / np.maximum(1e-30, (p * Ap).sum(0)); y = y + al * p; r = r - al * Ap
            z = r / diag[:, None]; rz2 = (r * z).sum(0); p = z + (rz2 / np.maximum(1e-30, rz)) * p; rz = rz2
        uv = y - (y.mean(0) - uv.mean(0))
    q = corner(uv)
    ra = np.concatenate([np.linalg.norm(q[:, a] - q[:, b], axis=1) / np.linalg.norm(L2[:, a] - L2[:, b], axis=1) for _, _, _, a, b in ED])
    print('WAVE zinc: one bow is %.2f m along the roof, the photo %d times (%.2f %%); stretch 1/5/50/95/99 %%: %s'
          % (sx[-1], Lv / TILE_H, 100 * (Lv / sx[-1] - 1), np.percentile(ra, [1, 5, 50, 95, 99]).round(3)))
    return uv.reshape(n, m + 1, 2), Lv, n


ZUV, LV, ZN = zinc_uv()


def zuv(i, j):                                                # grid column i counted from u -45 (any number: the bows repeat)
    k, r = divmod(i, ZN); u, v = ZUV[r][j]; return (u, v + k * LV)


class Mesh:
    def __init__(self, name):
        self.name, self.v, self.f, self.m, self.uv, self.n, self.mats = name, [], [], [], [], [], []

    def tri(self, pts, nrm, uvs, mat):
        if mat not in self.mats: self.mats.append(mat)
        b = [(-x, -z, y) for (x, y, z) in pts]; nb = [(-x, -z, y) for (x, y, z) in nrm]
        g = np.cross(np.array(b[1]) - b[0], np.array(b[2]) - b[0])
        if np.linalg.norm(g) < 1e-9: return
        o = [0, 1, 2] if g @ np.sum(nb, axis=0) >= 0 else [0, 2, 1]
        k = len(self.v); self.v += [b[i] for i in o]; self.f.append((k, k + 1, k + 2)); self.m.append(self.mats.index(mat))
        self.uv += [uvs[i] for i in o]; self.n += [nb[i] for i in o]

    def quad(self, p, n, t, mat):                             # p in rows: a b / c d
        self.tri((p[0], p[1], p[2]), (n[0], n[1], n[2]), (t[0], t[1], t[2]), mat)
        self.tri((p[2], p[1], p[3]), (n[2], n[1], n[3]), (t[2], t[1], t[3]), mat)

    def make(self):
        key = {}; vs = []; fs = []
        for f in self.f:
            q = []
            for i in f:
                k = tuple(round(float(c), 6) for c in self.v[i]) + tuple(round(float(c), 4) for c in self.n[i])
                if k not in key:
                    key[k] = len(vs); vs.append(self.v[i])
                q.append(key[k])
            fs.append(q)
        me = bpy.data.meshes.new(self.name); me.from_pydata(vs, [], fs); me.update()
        for m in self.mats: me.materials.append(bpy.data.materials.get(m) or bpy.data.materials.new(m))
        me.polygons.foreach_set('material_index', self.m); me.polygons.foreach_set('use_smooth', [True] * len(fs))
        u = me.uv_layers.new(name='UVMap'); u.data.foreach_set('uv', [float(c) for t in self.uv for c in t])
        me.normals_split_custom_set([tuple(float(c) for c in n) for n in self.n])
        o = bpy.data.objects.new(self.name, me); bpy.context.scene.collection.objects.link(o)
        print('WAVE', self.name, len(fs), 'triangles', self.mats); return o


def unit(v): return np.array(v, float) / np.linalg.norm(v)


def tube(M, path, radius, tess, mat, cap0=False, cap1=False, per=1.0, flat=(False, False)):
    P = np.array(path, float); n = len(P); T = np.gradient(P, axis=0); T /= np.linalg.norm(T, axis=1)[:, None]
    # (a joint with the next bow, in the valley: the path is level and straight along the building there - a one-sided slope
    #  tilted the last ring and left a 2 cm slit in the white edge)
    if flat[0]: T[0] = (1.0, 0.0, 0.0)
    if flat[1]: T[-1] = (1.0, 0.0, 0.0)
    # the cross sections square to the path, one side always up, so two pieces' ends are the same circle
    R = []; s = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))])
    for i in range(n):
        up = np.array([0, 1.0, 0]); a = unit(up - (up @ T[i]) * T[i]); b = np.cross(T[i], a)
        R.append([math.cos(2 * math.pi * k / tess) * a + math.sin(2 * math.pi * k / tess) * b for k in range(tess + 1)])
    for i in range(n - 1):
        for k in range(tess):
            p = [P[i] + radius * R[i][k], P[i + 1] + radius * R[i + 1][k], P[i] + radius * R[i][k + 1], P[i + 1] + radius * R[i + 1][k + 1]]
            M.quad(p, [R[i][k], R[i + 1][k], R[i][k + 1], R[i + 1][k + 1]],
                   [(k / tess, s[i] * per), (k / tess, s[i + 1] * per), ((k + 1) / tess, s[i] * per), ((k + 1) / tess, s[i + 1] * per)], mat)
    for on, i, d in ((cap0, 0, -1), (cap1, n - 1, 1)):
        if on:
            for k in range(tess): M.tri((P[i], P[i] + radius * R[i][k], P[i] + radius * R[i][k + 1]), (d * T[i],) * 3, ((.5, .5),) * 3, mat)


def ball(M, c, r, mat, n=16):
    def pt(i, j): return np.array([math.sin(math.pi * i / n) * math.cos(math.pi * j / n), math.cos(math.pi * i / n), math.sin(math.pi * i / n) * math.sin(math.pi * j / n)])
    for i in range(n):
        for j in range(2 * n):
            q = [pt(i, j), pt(i + 1, j), pt(i, j + 1), pt(i + 1, j + 1)]
            M.quad([c + r * k for k in q], q, [(.5, .5)] * 4, mat)


def roof(name, w, e):
    u0, u1 = (-51.0 if w else -45.0), (51.0 if e else 45.0)
    us, P, ends = grid(u0, u1, w, e); i0 = int(round((u0 + 45.0) / DX)); M = Mesh('wave_soffit_' + name); T = Mesh('wave_top_' + name); B = Mesh('wave_fascia_' + name); n = len(us) - 1
    def nrm(i, j, s):
        a, b = dh(P[i][j][0], P[i][j][2], ends(us[i])); return unit((-a * s, s, -b * s))
    up = np.array([0, W.THICK, 0])
    for i in range(n):
        for j in range(NZ):
            c = [(i, j), (i + 1, j), (i, j + 1), (i + 1, j + 1)]
            M.quad([P[q] for q in c], [nrm(*q, -1) for q in c], [((P[q][0] + 45.0) * TIMBER_U, P[q][2] / 8.0) for q in c], 'MI_T_timber')
            T.quad([P[q] + up for q in c], [nrm(*q, 1) for q in c], [zuv(i0 + q[0], q[1]) for q in c], 'MI_T_roofZinc')
        a, b = P[i][NZ], P[i + 1][NZ]                                                        # the back edge, closed
        B.quad([a, b, a + up, b + up], [(0, 0, 1)] * 4, [(a[0] / 4, 0), (b[0] / 4, 0), (a[0] / 4, .2), (b[0] / 4, .2)], 'MI_T_whiteGloss')
    k = 2                                                                                    # (the edge finer than the grid)
    fine = [u0 + (u1 - u0) * i / (n * k) for i in range(n * k + 1)]
    tube(B, [(u, W.h_u(u, W.bow(u, ends(u)), ends(u)) + 0.35, W.bow(u, ends(u))) for u in fine], 0.42, 20, 'MI_T_whiteGloss', per=0.25, flat=(not w, not e))
    for on, i in ((w, 0), (e, n)):
        if not on: continue                                                                  # the free end: the same round, its corner a ball
        tube(B, [P[i][j] + (0, 0.35, 0) for j in range(NZ + 1)], 0.42, 20, 'MI_T_whiteGloss', cap1=True, per=0.25)
        ball(B, P[i][0] + (0, 0.35, 0), 0.42, 'MI_T_whiteGloss')
    o = [M.make(), T.make(), B.make()]
    G = Mesh('wave_edge_' + name)
    tube(G, [(u, W.h_u(u, W.bow(u, ends(u)), ends(u)) - 0.05, W.bow(u, ends(u)) + 0.15) for u in fine], 0.06, 8, 'MI_T_glow_24', cap0=w, cap1=e, flat=(not w, not e))
    return o + [G.make()]


OBS = roof('m', False, False) + roof('w', True, False) + roof('e', False, True)

names = []
for o in OBS:
    assert all(s.material for s in o.material_slots), o.name
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='OFF', path_mode='STRIP')
    bpy.ops.export_scene.gltf(filepath=os.path.join(ROOT, 'Saved', o.name + '_check.glb'), use_selection=True)      # (for Tools/find_zfight.py)
    names.append(o.name)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'WaveRoof.blend'))
print('WAVE names', ' '.join(names))
