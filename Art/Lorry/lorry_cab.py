# The lorry's cab: the Meshy cab cleaned (called by make_lorry.py after lorry_details.build; game metres: x forward, y left, z up).
# The owner wants the EXACT Meshy cab, only its imperfections cleaned (2026-11-26): the Meshy Tractor, doors and texture
# stay; only repair boxes (door + side window band, front arch lip) get torn holes filled and a Taubin smooth (max 3.5 cm,
# UVs untouched); outside them no vertex moves. Window faces inside the Meshy window lines are opened and get curved
# see-through glass, rubber surrounds and door shut seals; a hidden lining (9 cm inside the Meshy skin, never outside it)
# closes the walls. Every existing part keeps its name and pivot (Door_L/R, Door_*_Glass, Glass_Windscreen, Mirror_*,
# lamps, steps, grab handles ...). The placeholder CabLining becomes the full interior (CabInterior + moving parts
# SteeringWheel, Needle_Speed/RPM, GearLever, ParkBrake, Pedal_Accel/Brake, SunVisor_L/R).
# Left-hand drive (driver on +y).
import bpy, bmesh, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
import forklift_details as FD

Part, material, W = FD.Part, FD.material, FD.W
XB, XF, HY, WALL = 0.88, 3.90, 1.37, 0.09          # cab back, front face, half width (door skin), wall depth
WC, RA = (2.326, 0.60), 0.78                        # front wheel centre (x, z), arch radius
ZB = 1.38                                           # cab bottom; floor inside = ZB + WALL
WIN = dict(a0=1.47, a1=2.56, b0=2.20, b1=2.92)      # door window opening (x, z)
DOOR = dict(a0=1.37, a1=2.62, b0=1.39, b1=3.24)     # door outline (hinge at the front edge, pivot kept)
WS = dict(a0=-1.08, a1=1.08, b0=0.03, b1=0.84)      # windscreen opening (y, metres up the raked face)
P0 = Vector((3.889, 0, 2.0)); SL = Vector((-0.276, 0, 0.83)).normalized(); NO = Vector((SL.z, 0, -SL.x))   # windscreen frame (Meshy rake)
RC = 0.36                                           # plan radius of the front corners (Meshy: y 1.39 at x 3.48 -> 0.93 at 3.88)


def outer_pts():
    """side outline traced from the Meshy cab: raked screen, round brow, roof rising into the high roof / deflector"""
    p = [(XB, ZB), (WC[0], ZB)]
    p += [(WC[0] + RA * math.cos(a), WC[1] + RA * math.sin(a)) for a in np.linspace(math.pi / 2, 0, 13)[1:]]
    return p + [(WC[0] + RA, 0.42), (XF - 0.08, 0.42), (XF - 0.03, 0.50), (XF, 1.00), (XF + 0.02, 1.80), (XF, 2.0), (3.62, 2.86),
                (3.585, 3.02), (3.50, 3.20), (3.36, 3.33), (3.0, 3.42), (2.55, 3.47), (2.25, 3.53), (1.85, 3.72), (1.45, 3.87),
                (1.10, 3.92), (XB, 3.88)]


def plan_pts(n=8):
    """plan outline: straight sides, front corners rounded RC (closed loop in x, y)"""
    p = [(XB - 0.3, -HY), (XF - RC, -HY)]
    p += [(XF - RC + RC * math.sin(a), -HY + RC - RC * math.cos(a)) for a in np.linspace(0, math.pi / 2, n + 1)[1:]]
    p += [(XF - RC + RC * math.cos(a), HY - RC + RC * math.sin(a)) for a in np.linspace(0, math.pi / 2, n + 1)]
    return p + [(XB - 0.3, HY)]


def inner_pts(w=WALL):
    return [(XB + w, ZB + w), (XF - w, ZB + w), (XF - w, 1.95), (XF - 0.12 - w, 2.88), (XF - 0.20 - 0.8 * w, 3.12 - w),
            (XF - 0.38 - 0.5 * w, 3.32 - w), (2.6, 3.55 - w), (1.5, 3.80 - w), (1.05, 3.90 - w), (XB + w, 3.86 - w)]


def rrect(a0, a1, b0, b1, r, n=6):
    out = []
    for (ca, cb, t0) in ((a1 - r, b0 + r, -90), (a1 - r, b1 - r, 0), (a0 + r, b1 - r, 90), (a0 + r, b0 + r, 180)):
        for k in range(n + 1):
            t = math.radians(t0 + 90 * k / n); out.append((ca + r * math.cos(t), cb + r * math.sin(t)))
    return out


def F_side(s): return Matrix(((1, 0, 0, 0), (0, 0, s, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
def FS(s): return Matrix.Translation((0, s * HY, 0)) @ F_side(s)        # side frame on the door skin plane
F_WS = Matrix(((0, SL.x, NO.x, P0.x), (1, 0, 0, 0), (0, SL.z, NO.z, P0.z), (0, 0, 0, 1)))


def obj(name, bm, mats=()):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for m in mats: me.materials.append(m)
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o


def prism_bm(pts, c0, c1, F, bm=None):
    bm = bm or bmesh.new()
    A = [bm.verts.new(F @ Vector((a, b, c0))) for a, b in pts]; B = [bm.verts.new(F @ Vector((a, b, c1))) for a, b in pts]
    bm.faces.new(A[::-1]); bm.faces.new(B)
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm.faces.new((A[i], A[j], B[j], B[i]))
    return bm


def ring_bm(outer, inner, c0, c1, F, bev=0.0):
    """closed tube of a rounded-rect frame (seal): outer/inner loops equal length"""
    bm = bmesh.new(); L = []
    for loop, c in ((outer, c1), (inner, c1), (inner, c0), (outer, c0)):
        L.append([bm.verts.new(F @ Vector((a, b, c))) for a, b in loop])
    n = len(outer)
    for k in range(4):
        P, Q = L[k], L[(k + 1) % 4]
        for i in range(n):
            j = (i + 1) % n; bm.faces.new((P[i], P[j], Q[j], Q[i]))
    if bev: bmesh.ops.bevel(bm, geom=[e for e in bm.edges if e.calc_face_angle(0) > 1.0], offset=bev, segments=2, profile=0.5, affect='EDGES', clamp_overlap=True)
    return bm


def solid(pts, hy, rk, rs):
    """the side outline extruded across +-hy; kinks rounded rk, every edge round the side faces rolled rs"""
    bm = bmesh.new(); n = len(pts)
    A = [bm.verts.new((x, hy, z)) for x, z in pts]; B = [bm.verts.new((x, -hy, z)) for x, z in pts]
    bm.faces.new(A); bm.faces.new(B[::-1])
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((A[i], A[j], B[j], B[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    kink = []
    for i in range(n):
        p0, p1, p2 = (np.array(pts[k]) for k in (i - 1, i, (i + 1) % n))
        d1, d2 = p1 - p0, p2 - p1
        if math.degrees(math.acos(np.clip(d1 @ d2 / np.linalg.norm(d1) / np.linalg.norm(d2), -1, 1))) > 20:
            kink.append(bm.edges.get((A[i], B[i])))
    bmesh.ops.bevel(bm, geom=kink, offset=rk, segments=3, profile=0.5, affect='EDGES', clamp_overlap=True)
    side = {e for f in bm.faces if abs(f.normal.y) > 0.99 for e in f.edges}
    bmesh.ops.bevel(bm, geom=list(side), offset=rs, segments=4, profile=0.5, affect='EDGES', clamp_overlap=True)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4], quad_method='BEAUTY', ngon_method='EAR_CLIP')
    return bm


def apply_bool(o, cutter, op):
    m = o.modifiers.new('b', 'BOOLEAN'); m.operation = op; m.solver = 'EXACT'; m.object = cutter
    dg = bpy.context.evaluated_depsgraph_get(); me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
    o.modifiers.clear(); old = o.data; o.data = me; bpy.data.meshes.remove(old)


def tree_of(o):
    bm = bmesh.new(); bm.from_mesh(o.data); bmesh.ops.transform(bm, matrix=o.matrix_world, verts=bm.verts)
    t = BVHTree.FromBMesh(bm); bm.free(); return t


def join_into(target, parts):
    for o in bpy.context.scene.objects: o.select_set(False)
    for o in parts: o.select_set(True)
    target.select_set(True); bpy.context.view_layer.objects.active = target; bpy.ops.object.join()


def swap_mesh(o, src):
    """o keeps its name, pivot and matrix; its mesh becomes src's (world coordinates)"""
    me = src.data; me.transform(o.matrix_world.inverted()); old = o.data; o.data = me; me.name = o.name
    bpy.data.objects.remove(src, do_unlink=True); bpy.data.meshes.remove(old)


def sweep(P, path, prof, up, mi, caps=True):
    """profile (u along up, v along tangent x up) swept along the 3D path"""
    t = bmesh.new(); rings = []; path = [Vector(p) for p in path]; up = Vector(up)
    for i, p in enumerate(path):
        tg = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized(); bn = tg.cross(up).normalized(); u = bn.cross(tg)
        rings.append([t.verts.new(p + u * a + bn * b) for a, b in prof])
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(len(prof)):
            j = (i + 1) % len(prof); t.faces.new((r0[i], r0[j], r1[j], r1[i]))
    if caps: t.faces.new(rings[0][::-1]); t.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(t, faces=t.faces); P._merge(t, mi)


def circle(c, r, n=10): return [(c[0] + r * math.cos(2 * math.pi * k / n), c[1] + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def fender(P, axles, zt, yi, yo, mi_plate, mi_lip, end=60):
    """mudguard over one or more axles: plate 6 mm, rolled outer lip; arcs down over the first/last tyre"""
    R = zt - 0.60; ax = sorted(axles, reverse=True); path = []
    path += [(ax[0] + R * math.cos(math.radians(a)), 0, 0.60 + R * math.sin(math.radians(a))) for a in np.linspace(90 - end, 90, 9)]
    path += [(ax[-1] + R * math.cos(math.radians(a)), 0, 0.60 + R * math.sin(math.radians(a))) for a in np.linspace(90, 90 + end, 9)]
    s = 1 if yo > 0 else -1
    for yy, pr, mi in ((0, [(yi, 0), (yo, 0), (yo, 0.006), (yi, 0.006)], mi_plate),):
        sweep(P, [(x, 0, z) for x, _, z in path], [(a, b) for a, b in pr], (0, 1, 0), mi)
    sweep(P, [(x, 0, z) for x, _, z in path], circle((yo, 0.0), 0.011), (0, 1, 0), mi_lip)


def cab_zone(c): return (c.x > 0.80 and c.z > 1.30) or (c.x > 2.75 and c.z > 0.30)


def meshy_target(body, PARTS):
    """the Meshy cab surface as it came (body + its split doors, door glass, windscreen) - the shape to follow"""
    bm = bmesh.new()
    for o in [body] + [PARTS[k] for k in ('Door_L', 'Door_R', 'Door_L_Glass', 'Door_R_Glass', 'Glass_Windscreen') if k in PARTS]:
        t = bmesh.new(); t.from_mesh(o.data); bmesh.ops.transform(t, matrix=o.matrix_world, verts=t.verts)
        bmesh.ops.delete(t, geom=[f for f in t.faces if not cab_zone(f.calc_center_median())], context='FACES')
        me = bpy.data.meshes.new('_t'); t.to_mesh(me); t.free(); bm.from_mesh(me); bpy.data.meshes.remove(me)
    T = BVHTree.FromBMesh(bm); bm.free(); return T


def lap(V, E, n, fixed, iters, lam=0.5, data=None, w=None):
    """umbrella smoothing of a per-vertex field (positions or offsets); fixed rows kept, data rows pulled back by w"""
    deg = np.bincount(E.ravel(), minlength=n).astype(float)[:, None]; deg[deg == 0] = 1
    for _ in range(iters):
        acc = np.zeros_like(V); np.add.at(acc, E[:, 0], V[E[:, 1]]); np.add.at(acc, E[:, 1], V[E[:, 0]])
        Vn = V + lam * (acc / deg - V)
        if data is not None: Vn = Vn * (1 - w) + data * w
        Vn[fixed] = V[fixed]; V = Vn
    return V


def meshy_skin(T, voxel=0.035):
    """the clean cage: side outline x rounded plan, voxel-remeshed to an even quad skin, then laid onto the Meshy cab
    by rays along each normal; the offsets are smoothed over ~25 cm so the curves stay and the crumples go. The door
    sides, back wall, floor and wheel arch stay true (pinned) so doors, shuts and seals keep their planes."""
    S = obj('_S', solid(outer_pts(), HY, 0.05, 0.05)); Pl = obj('_P', prism_bm(plan_pts(), -0.5, 4.6, Matrix.Identity(4)))
    apply_bool(S, Pl, 'INTERSECT'); bpy.data.objects.remove(Pl, do_unlink=True)
    m = S.modifiers.new('r', 'REMESH'); m.mode = 'VOXEL'; m.voxel_size = voxel
    dg = bpy.context.evaluated_depsgraph_get(); me = bpy.data.meshes.new_from_object(S.evaluated_get(dg)); S.modifiers.clear(); S.data = me
    me = S.data; n = len(me.vertices)
    V = np.array([v.co for v in me.vertices]); N = np.array([v.normal for v in me.vertices])
    E = np.array([e.vertices[:] for e in me.edges])
    pin = ((np.abs(N[:, 1]) > 0.93) & (V[:, 0] < XF - RC - 0.02) & (V[:, 2] < 3.30)) | (N[:, 2] < -0.9) | (N[:, 0] < -0.93) \
        | ((V[:, 2] < ZB + 0.06) & (V[:, 0] < XF - 0.45))
    d = np.zeros(n); hit = np.zeros(n, bool)
    for i in np.where(~pin)[0]:
        p, nn = Vector(V[i]), Vector(N[i])
        h = T.ray_cast(p + nn * 0.30, -nn, 0.60)
        if h[0] is not None and abs(h[1].dot(nn)) > 0.3: d[i] = 0.30 - h[3]; hit[i] = True
    d = np.clip(d, -0.28, 0.10)
    D = lap(d[:, None], E, n, pin, 220, 0.5, d[:, None], hit[:, None] * 0.04)[:, 0]
    V = V + N * D[:, None]
    V = lap(V, E, n, pin, 4, 0.5)
    me.vertices.foreach_set('co', V.ravel()); me.update()
    Nn = np.array([v.normal for v in me.vertices]); Nn[pin & (np.abs(N[:, 1]) > 0.93)] = N[pin & (np.abs(N[:, 1]) > 0.93)]
    Vi = lap(V - Nn * WALL, E, n, pin, 6, 0.5)
    print('SKIN verts %d pinned %d hit %d offset mean %.3f min %.3f max %.3f' % (n, pin.sum(), hit.sum(), D[~pin].mean(), D.min(), D.max()))
    I = obj('_I', bmesh.new()); I.data = me.copy(); I.data.vertices.foreach_set('co', Vi.ravel()); I.data.update()
    return S, I


def fit_inside(I, T):
    """the lining never shows outside: each lining vertex is checked by a ray from the cab's middle out through it to
    the Meshy surface and pulled in so it sits a wall depth (9 cm) inside it; only ever pulled in, the floor stays"""
    me = I.data; V = np.array([v.co for v in me.vertices]); moved = 0
    for i, v in enumerate(V):
        if v[2] < ZB + 0.15 and abs(v[1]) < HY - 0.15 and XB + 0.12 < v[0] < 3.5: continue   # the cab floor itself
        c = Vector((min(max(v[0], 1.15), 3.45), 0.0, min(max(v[2], 1.95), 3.0))); dv = Vector(v) - c; L = dv.length
        if L < 0.05: continue
        dv /= L; h = T.ray_cast(c, dv, 3.0)
        if h[0] is None:
            q = T.find_nearest(Vector(v))[0]; lim = (q - c).length - WALL if q is not None else L
        else: lim = h[3] - WALL
        if L > lim: V[i] = np.array(c + dv * max(lim, 0.05)); moved += 1
    me.vertices.foreach_set('co', V.ravel()); me.update(); return moved


def hollow(O, I):
    """outer skin + inward-facing inner skin = one closed wall (the cut openings then get their reveals)"""
    bm = bmesh.new(); bm.from_mesh(O.data); t = bmesh.new(); t.from_mesh(I.data)
    bmesh.ops.reverse_faces(t, faces=t.faces); me = bpy.data.meshes.new('_t'); t.to_mesh(me); t.free(); bm.from_mesh(me); bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new('_H'); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new('_H', me); bpy.context.scene.collection.objects.link(o); return o


def to_frame(F, a, b, c): return F @ Vector((a, b, c))


def on_skin(tO, F, a, b):
    """point of the outer skin seen along the frame normal at (a, b)"""
    nrm = (F.to_3x3() @ Vector((0, 0, 1))).normalized(); h = tO.ray_cast(to_frame(F, a, b, 0.4), -nrm, 1.0)
    return (h[0], h[1]) if h[0] is not None else (to_frame(F, a, b, 0), nrm)


def skin_glass(O, F, rect, sc, inset=0.012, depth=0.020, thick=0.006):
    """glass that follows the skin's curve: the skin under the opening, sunk 'depth', 6 mm thick, trimmed to the opening"""
    a0, a1, b0, b1, r = rect; Fi = F.inverted(); bm = bmesh.new(); bm.from_mesh(O.data)
    kill = []
    for f in bm.faces:
        q = Fi @ f.calc_center_median(); fn = (Fi.to_3x3() @ f.normal)
        if not (a0 - 0.08 < q.x < a1 + 0.08 and b0 - 0.08 < q.y < b1 + 0.08 and fn.z > 0.3): kill.append(f)
    bmesh.ops.delete(bm, geom=kill, context='FACES')
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for v in bm.verts: v.co -= v.normal * depth
    ex = bmesh.ops.extrude_face_region(bm, geom=bm.faces[:]); nv = [e for e in ex['geom'] if isinstance(e, bmesh.types.BMVert)]
    for v in nv: v.co -= v.normal * thick
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    g = obj('_G', bm, [bpy.data.materials['M_Lorry_Glass']])
    c = obj('_c', prism_bm(rrect(a0 + inset, a1 - inset, b0 + inset, b1 - inset, max(r - inset, 0.01)), -0.5, 0.5, F))
    apply_bool(g, c, 'INTERSECT'); bpy.data.objects.remove(c, do_unlink=True)
    assert len(g.data.vertices) > 0, 'glass trimmed to nothing'
    g.data.materials.clear(); g.data.materials.append(bpy.data.materials['M_Lorry_Glass'])
    g.data.polygons.foreach_set('material_index', [0] * len(g.data.polygons))
    for p in g.data.polygons: p.use_smooth = True
    return g


def skin_loop(tO, F, rect, n=6):
    a0, a1, b0, b1, r = rect
    return [on_skin(tO, F, a, b) for a, b in rrect(a0, a1, b0, b1, r, n)]


def skin_seal(P, tO, F, rect, mi, w=0.028, d0=-0.004, d1=-0.044, n=6, c=None):
    """rubber seal swept round an opening on the curved skin: a band 'w' wide just inside the edge, from d0 to d1 deep"""
    a0, a1, b0, b1, r = rect; c = w / 2 - 0.006 if c is None else c
    pts = skin_loop(tO, F, (a0 + c, a1 - c, b0 + c, b1 - c, max(r - c, 0.01)), n)
    t = bmesh.new(); rings = []; m = len(pts)
    prof = [(d0, -w / 2), (d0 + 0.002, w / 2), (d1, w / 2 - 0.004), (d1, -w / 2 + 0.004)]
    for i, (p, nrm) in enumerate(pts):
        tg = (pts[(i + 1) % m][0] - pts[i - 1][0]).normalized(); bn = tg.cross(nrm).normalized(); u = bn.cross(tg)
        rings.append([t.verts.new(p + u * a + bn * b) for a, b in prof])
    for i in range(m):
        r0, r1 = rings[i], rings[(i + 1) % m]
        for k in range(4): t.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
    bmesh.ops.recalc_face_normals(t, faces=t.faces); P._merge(t, mi)


def in_rr(rect, x, y):
    a0, a1, b0, b1, r = rect
    if not (a0 <= x <= a1 and b0 <= y <= b1): return False
    dx = max(a0 + r - x, 0, x - a1 + r); dy = max(b0 + r - y, 0, y - b1 + r); return dx * dx + dy * dy <= r * r


REPAIR_BODY = [((0.95, 1.12, 1.42), (3.30, 1.60, 3.30)), ((1.45, 0.95, 0.85), (3.25, 1.60, 1.48))]   # side + window band; front arch lip (|y|)
REPAIR_DOOR = [((1.30, 1.10, 1.35), (2.70, 1.60, 3.30))]


def zone_weight(V, zones, fade=0.08):
    """1 inside a repair box (y as |y|), fading to 0 over 'fade' outside it; 0 elsewhere (those vertices never move)"""
    P = V.copy(); P[:, 1] = np.abs(P[:, 1]); w = np.zeros(len(V))
    for lo, hi in zones:
        lo, hi = np.array(lo), np.array(hi); out = np.maximum(np.maximum(lo - P, P - hi), 0).max(1)
        w = np.maximum(w, np.clip(1 - out / fade, 0, 1))
    return w * w * (3 - 2 * w)


def repair(o, zones, cap=0.035):
    """local clean-up of a Meshy part in place: torn holes (up to 10 edges) filled, then Taubin smoothing (no shrink) of
    the vertices in the repair boxes only; open edges (the door gaps, Meshy's own lines) stay put. UVs untouched, so the
    Meshy texture stays."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    bnd = [e for e in bm.edges if e.is_boundary]; nf = len(bm.faces)
    bmesh.ops.holes_fill(bm, edges=bnd, sides=10); filled = len(bm.faces) - nf
    bm.to_mesh(o.data); bm.free(); me = o.data; me.update()
    M = np.array(o.matrix_world); V = np.array([v.co for v in me.vertices]); Vw = V @ M[:3, :3].T + M[:3, 3]
    w = zone_weight(Vw, zones); E = np.array([e.vertices[:] for e in me.edges])
    bm = bmesh.new(); bm.from_mesh(me); fix = np.array([v.is_boundary or not v.link_faces for v in bm.verts]); bm.free()
    fix |= w <= 0
    S = V.copy()
    for _ in range(25):
        S = lap(S, E, len(V), fix, 1, 0.55); S = lap(S, E, len(V), fix, 1, -0.58)
    dV = (S - V) * w[:, None]; L = np.linalg.norm(dV, axis=1); k = np.minimum(1, cap / np.maximum(L, 1e-9))
    V = V + dV * k[:, None]; me.vertices.foreach_set('co', V.ravel()); me.update()
    return int((w > 0).sum()), float((L * k).max()), filled


def open_window(o, F, rect, grow=0.0):
    """faces of a Meshy part inside a window outline (frame coords, facing out) go: the window is glass, never paint"""
    a0, a1, b0, b1, r = rect; Fi = F.inverted() @ o.matrix_world; R = Fi.to_3x3(); poly = rrect(a0 - grow, a1 + grow, b0 - grow, b1 + grow, r + grow)
    bm = bmesh.new(); bm.from_mesh(o.data); kill = []
    for f in bm.faces:
        q = Fi @ f.calc_center_median()
        if -0.25 < q.z < 0.25 and abs((R @ f.normal).z) > 0.2 and in_rr((a0 - grow, a1 + grow, b0 - grow, b1 + grow, r + grow), q.x, q.y): kill.append(f)
    bmesh.ops.delete(bm, geom=kill, context='FACES'); bm.to_mesh(o.data); bm.free(); o.data.update(); return len(kill)


def door_inside(D, s, yi):
    """door card on the door's inner face: armrest, pull handle, window switches, speaker, pocket, inner release, map light"""
    D.box((1.42, s * (yi - 0.012), 1.46), (2.58, s * (yi + 0.004), 2.16), 3, 0.02)                         # card panel
    D.box((1.55, s * (yi - 0.075), 2.00), (2.30, s * (yi - 0.010), 2.075), 3, 0.022)                       # armrest
    D.box((1.57, s * (yi - 0.078), 2.06), (2.28, s * (yi - 0.030), 2.082), 4, 0.010)                       # soft armrest pad
    D.box((2.08, s * (yi - 0.065), 2.075), (2.20, s * (yi - 0.020), 2.090), 4, 0.004)                      # window switch block
    for xx in (2.095, 2.15): D.box((xx, s * (yi - 0.055), 2.090), (xx + 0.04, s * (yi - 0.030), 2.098), 2, 0.002)
    D._merge(sweep_tmp([(1.62, s * (yi - 0.008), 2.16), (1.64, s * (yi - 0.050), 2.17), (1.80, s * (yi - 0.050), 2.17), (1.82, s * (yi - 0.008), 2.16)], 0.013), 3)
    D.cyl((2.30, s * (yi - 0.010), 1.68), (2.30, s * (yi - 0.022), 1.68), 0.095, 3, 32, 0.004)            # speaker ring
    D.cyl((2.30, s * (yi - 0.020), 1.68), (2.30, s * (yi - 0.026), 1.68), 0.08, 2, 32, 0.0)
    for rr in (0.025, 0.045, 0.065): D.torus((0, 0, 0), rr, 0.003, 4, 32, 6, M=Matrix.Translation((2.30, s * (yi - 0.026), 1.68)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    D.box((1.58, s * (yi - 0.065), 1.50), (2.10, s * (yi - 0.008), 1.56), 3, 0.012)                       # pocket lip
    D.box((1.58, s * (yi - 0.030), 1.52), (2.10, s * (yi - 0.008), 1.78), 3, 0.012)                       # pocket back
    D.box((1.47, s * (yi - 0.020), 2.30), (1.53, s * (yi + 0.003), 2.40), 4, 0.008); D.box((1.475, s * (yi - 0.035), 2.335), (1.525, s * (yi - 0.015), 2.365), 1, 0.006)
    D.box((1.42, s * (yi - 0.020), 2.16), (2.58, s * (yi + 0.004), 2.20), 4, 0.008)                       # window sill trim


def shut_seals(E, s):
    """door shut sealed: ring A frames the aperture inside the lining, ring B is the lip just inside the closed door,
    overlapping its edge - the gap round the door shows rubber, never light"""
    E._merge(ring_bm(rrect(DOOR['a0'] - 0.030, DOOR['a1'] + 0.030, ZB - 0.01, DOOR['b1'] + 0.030, 0.09),
                     rrect(DOOR['a0'] - 0.004, DOOR['a1'] + 0.004, ZB + 0.005, DOOR['b1'] + 0.004, 0.065),
                     HY - WALL - 0.03, HY - 0.012, F_side(s), 0.003), 1)
    E._merge(ring_bm(rrect(DOOR['a0'] - 0.010, DOOR['a1'] + 0.010, ZB, DOOR['b1'] + 0.010, 0.07),
                     rrect(DOOR['a0'] + 0.035, DOOR['a1'] - 0.035, ZB + WALL + 0.01, DOOR['b1'] - 0.035, 0.03),
                     HY - WALL - 0.03, HY - WALL - 0.010, F_side(s), 0.002), 1)


def lining(sc, I, holes, mats):
    """the inner wall (wall depth 9 cm behind the Meshy skin): the smooth inner skin, facing in, with the window and door
    openings taken out under their rubber surrounds; floor = mat, roof = headliner, rest = trim"""
    bm = bmesh.new(); bm.from_mesh(I.data); bmesh.ops.reverse_faces(bm, faces=bm.faces); kill = set()
    for F, (a0, a1, b0, b1, r) in holes:
        Fi = F.inverted()
        for f in bm.faces:
            q = Fi @ f.calc_center_median()
            if -0.6 < q.z < 0.3 and in_rr((a0, a1, b0, b1, r), q.x, q.y): kill.add(f)
    for s in (1, -1):
        Fi = F_side(s).inverted(); dr = (DOOR['a0'] - 0.01, DOOR['a1'] + 0.01, ZB - 0.2, DOOR['b1'] + 0.01, 0.07)
        for f in bm.faces:
            q = Fi @ f.calc_center_median()
            if HY - 0.3 < q.z < HY + 0.05 and in_rr(dr, q.x, q.y): kill.add(f)
    kill |= {f for f in bm.faces if f.calc_center_median().z < ZB + WALL - 0.03}   # nothing below the cab floor (the Meshy lower cab is solid)
    bmesh.ops.delete(bm, geom=list(kill), context='FACES')
    for f in bm.faces:
        c = f.calc_center_median(); f.smooth = True
        f.material_index = 2 if f.normal.z > 0.8 and c.z < ZB + WALL + 0.05 else 1 if f.normal.z < -0.5 and c.z > 2.9 else 0
    o = obj('CabLining', bm, mats); return o

def build(sc, body, trailer, PARTS):
    INFO, IKN, mats = [], {}, {}
    def mat(name, col, rough, metal=0.0, emit=None):
        if name not in mats: mats[name] = material(name, col, rough, metal, emit)
        return mats[name]
    paint = mat('M_Lorry_CabPaint', (0.035, 0.19, 0.52), 0.28)
    p = paint.node_tree.nodes['Principled BSDF']; p.inputs['Clearcoat'].default_value = 1.0; p.inputs['Clearcoat Roughness'].default_value = 0.05
    trim = mat('M_Lorry_Trim', (0.10, 0.10, 0.105), 0.65); head = mat('M_Lorry_Headliner', (0.42, 0.41, 0.39), 0.9)
    blk = bpy.data.materials['M_Lorry_BlackPlastic']; rub = bpy.data.materials['M_Lorry_Rubber']; chrome = bpy.data.materials['M_Lorry_Chrome']
    seal = mat('M_Lorry_Seal', (0.008, 0.008, 0.009), 0.55); glass = bpy.data.materials['M_Lorry_Glass']
    fab = mat('M_Lorry_SeatFabric', (0.045, 0.047, 0.055), 0.95); dash = mat('M_Lorry_Dash', (0.025, 0.025, 0.028), 0.55)
    dial = mat('M_Lorry_DialFace', (0.01, 0.01, 0.012), 0.3); white = mat('M_Lorry_DialMarks', (0.9, 0.9, 0.88), 0.4, 0.0, (0.9, 0.9, 0.85))
    needle = mat('M_Lorry_Needle', (0.9, 0.12, 0.02), 0.4, 0.0, (1.0, 0.2, 0.02)); screen = mat('M_Lorry_Screen', (0.004, 0.006, 0.01), 0.08, 0.0, (0.02, 0.05, 0.09))
    knob = mat('M_Lorry_ParkKnob', (0.75, 0.45, 0.02), 0.4); matm = mat('M_Lorry_FloorMat', (0.02, 0.02, 0.022), 0.85); bed = mat('M_Lorry_Mattress', (0.09, 0.08, 0.07), 0.9)
    # tyres: black rubber, little sheen (they rendered grey); glass: see-through (transmission), still alpha for the game
    r = rub.node_tree.nodes['Principled BSDF']; r.inputs['Base Color'].default_value = (0.010, 0.010, 0.011, 1); r.inputs['Roughness'].default_value = 0.78; r.inputs['Specular'].default_value = 0.25
    g = glass.node_tree.nodes['Principled BSDF']; g.inputs['Transmission'].default_value = 1.0; g.inputs['IOR'].default_value = 1.52
    g.inputs['Roughness'].default_value = 0.0; g.inputs['Base Color'].default_value = (0.80, 0.86, 0.84, 1)
    glass.use_backface_culling = False; glass.shadow_method = 'HASHED'

    # ------------------------------------------------ 1. the Meshy cab kept: only its bad spots repaired, in place
    if 'CabLining' in PARTS: bpy.data.objects.remove(PARTS.pop('CabLining'), do_unlink=True)   # the placeholder box
    TM = meshy_target(body, PARTS)                 # the Meshy cab as it came (the smooth inner lining follows it)
    for o, zone in ((body, REPAIR_BODY), (PARTS['Door_L'], REPAIR_DOOR), (PARTS['Door_R'], REPAIR_DOOR)):
        INFO.append('Repair %s: %d vertices smoothed (max move %.3f m), %d torn holes filled' % ((o.name,) + repair(o, zone)))
    # ------------------------------------------------ 2. windows opened along Meshy's lines; see-through glass on the cab's curve
    O, I = meshy_skin(TM); tO = tree_of(O); INFO.append('Cab lining: %d vertices pulled inside the Meshy skin' % fit_inside(I, TM))
    E = Part('_Ext', [blk, seal, chrome, paint, trim])
    rws = (WS['a0'], WS['a1'], WS['b0'], WS['b1'], 0.08); rwi = (WIN['a0'], WIN['a1'], WIN['b0'], WIN['b1'], 0.07)
    n = open_window(body, F_WS, rws, 0.015)
    for k in ('Glass_Windscreen',): PARTS[k].matrix_world = Matrix.Identity(4); swap_mesh(PARTS[k], skin_glass(O, F_WS, rws, sc, -0.02, 0.03))
    skin_seal(E, tO, F_WS, rws, 1, w=0.055, d0=0.006, d1=-0.115, c=0.0)
    INFO.append('Windscreen: %d painted Meshy faces opened, glass follows the curve 3 cm in, rubber surround to the lining' % n)
    for s, side in ((1, 'L'), (-1, 'R')):
        d = PARTS['Door_' + side]; n = open_window(d, FS(s), rwi, 0.015)
        swap_mesh(PARTS['Door_%s_Glass' % side], obj('_G', prism_bm(rrect(rwi[0] - 0.02, rwi[1] + 0.02, rwi[2] - 0.02, rwi[3] + 0.02, rwi[4] + 0.02), -0.036, -0.030, FS(s)), [bpy.data.materials['M_Lorry_Glass']]))   # the door side is flat: a flat pane 3 cm in
        D = Part('_DS', [seal, chrome, blk, trim, dash]); skin_seal(D, tO, FS(s), rwi, 0, w=0.05, d0=0.006, d1=-0.10, c=0.0)
        door_inside(D, s, yi=HY - WALL)
        join_into(d, [D.done(sc)]); INFO.append('Door_%s: %d Meshy faces opened for the window, door card inside' % (side, n))
        shut_seals(E, s)
    # ------------------------------------------------ 3. the lining: the inner wall 9 cm inside, open at doors and windows
    lining(sc, I, [(F_WS, rws), (FS(1), rwi), (FS(-1), rwi)], (trim, head, matm))
    # fenders over the drive tandem (tractor) - top kept under the trailer's swing
    xs = [PARTS['Wheel_%sL' % a].matrix_world.translation.x for a in ('D1', 'D2')]
    for s in (1, -1):
        fender(E, xs, 1.240, s * 0.86, s * 1.29, 0, 0)
        for xx in xs: E.cyl((xx, s * 0.45, 1.00), (xx, s * 0.87, 1.235), 0.015, 0, 10)
    ext = E.done(sc); join_into(body, [ext])
    # trailer fenders: under the box floor (ray up), over the three axles
    tt = tree_of(trailer); xs = [PARTS['Wheel_%sL' % a].matrix_world.translation.x for a in ('T1', 'T2', 'T3')]
    zs = [h[0].z for x in (xs[0], xs[2]) for yy in (0.9, 1.25, -0.9, -1.25) for h in [tt.ray_cast(Vector((x, yy, 1.21)), Vector((0, 0, 1)), 1.0)] if h[0] is not None]
    zt = min(1.27, (min(zs) - 0.012) if zs else 1.27)
    if zt > 1.225:
        T = Part('_TF', [blk])
        for s in (1, -1):
            fender(T, xs, zt, s * 0.86, s * 1.27, 0, 0)
            for xx in xs: T.cyl((xx, s * 0.95, zt + 0.004), (xx, s * 0.95, zt + 0.004 + max(0.005, (min(zs) if zs else zt) - zt - 0.004)), 0.012, 0, 8)
        join_into(trailer, [T.done(sc)])
        INFO.append('Trailer fenders top z %.3f (box underside %.3f)' % (zt, min(zs) if zs else -1))
    else: INFO.append('Trailer fenders skipped: box underside %.3f too low' % min(zs))

    # ------------------------------------------------ 4. interior (static) + moving controls
    if 'CabLining' in PARTS: bpy.data.objects.remove(PARTS.pop('CabLining'), do_unlink=True)
    zf = ZB + WALL; yd = 0.62
    C = Part('CabInterior', [trim, dash, fab, blk, chrome, matm, screen, bed, white, dial, head])
    C.box((3.36, -1.27, zf - 0.01), (3.78, 1.27, 1.90), 1, 0.05)                       # dash body against the front wall
    C.box((3.24, -1.27, 1.86), (3.78, 1.27, 1.99), 1, 0.045)                          # rounded dash top pad
    C.box((3.16, 0.28, 1.96), (3.40, 0.96, 2.22), 1, 0.05)                            # instrument binnacle (hood)
    for yy, nm in ((0.46, 'Speed'), (0.78, 'RPM')):
        C.cyl((3.168, yy, 2.08), (3.18, yy, 2.08), 0.085, 9, 40, 0.0)
        C.torus((0, 0, 0), 0.087, 0.006, 4, 40, 8, M=Matrix.Translation((3.165, yy, 2.08)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
        for k in range(11):                                                             # tick marks
            a = math.radians(225 - 27 * k); C.box((3.163, yy + 0.07 * math.cos(a) - 0.002, 2.08 + 0.07 * math.sin(a) - 0.002), (3.167, yy + 0.07 * math.cos(a) + 0.002, 2.08 + 0.07 * math.sin(a) + 0.002), 8, 0.0)
        N = Part('Needle_' + nm, [needle, chrome]); N.box((3.158, yy - 0.003, 2.08), (3.162, yy + 0.003, 2.145), 0, 0.001); N.cyl((3.157, yy, 2.08), (3.163, yy, 2.08), 0.009, 1, 16, 0.001)
        PARTS['Needle_' + nm] = N.done(sc, (3.16, yy, 2.08)); INFO.append('Needle_%s pivot 3.160 %.3f 2.080, turns about x (0 = straight up, -135..+135 deg)' % (nm, yy))
    C.box((3.30, -0.15, 1.94), (3.34, 0.22, 2.08), 6, 0.006)                           # centre screen
    for k in range(6): C.box((3.335, -0.13 + k * 0.06, 1.89), (3.355, -0.09 + k * 0.06, 1.915), 3, 0.004)   # switch row
    for yy in (-1.05, -0.55, 1.12):                                                     # vents
        C.box((3.235, yy - 0.09, 1.905), (3.25, yy + 0.09, 1.975), 3, 0.006)
        for k in range(4): C.box((3.228, yy - 0.085, 1.912 + k * 0.016), (3.236, yy + 0.085, 1.918 + k * 0.016), 4, 0.001)
    C.box((3.20, -1.05, 1.60), (3.37, -0.35, 1.84), 1, 0.03)                           # glovebox bulge, passenger
    # steering column + shroud (wheel tilted 30 deg from horizontal)
    nW = Vector((-0.5, 0, 0.866)); cW = Vector((3.00, yd, 2.02))
    C.cyl(cW - nW * 0.07, cW - nW * 0.62, 0.035, 3, 16); C.cyl(cW - nW * 0.12, cW - nW * 0.40, 0.065, 3, 20, 0.01, r2=0.08)
    S = Part('SteeringWheel', [blk, chrome, dash]); Mw = Matrix.Translation(cW) @ nW.to_track_quat('Z', 'X').to_matrix().to_4x4()
    S.torus((0, 0, 0), 0.24, 0.019, 0, 56, 12, M=Mw); S.cyl((0, 0, -0.04), (0, 0, 0.02), 0.07, 2, 28, 0.01, M=Mw)
    for a in (0, 180, 270):
        ra = math.radians(a); S._merge(sweep_tmp([(0.06 * math.cos(ra), 0.06 * math.sin(ra), -0.005), (0.23 * math.cos(ra), 0.23 * math.sin(ra), 0.0)], 0.014), 0, Mw)
    for k, (bx, by) in enumerate(((0.10, 0.03), (0.10, -0.01), (-0.10, 0.03), (-0.10, -0.01))):
        S.box((bx - 0.012, by - 0.009, 0.004), (bx + 0.012, by + 0.009, 0.012), 1 if k % 2 else 2, 0.002, M=Mw)
    S.cyl((0, 0, 0.018), (0, 0, 0.024), 0.035, 1, 24, 0.002, M=Mw)
    PARTS['SteeringWheel'] = S.done(sc, cW, nW.to_track_quat('Z', 'X').to_matrix()); INFO.append('SteeringWheel centre %.3f %.3f %.3f, turns about its local z (column axis %s)' % (*cW, tuple(round(q, 3) for q in nW)))
    for s_, nm in ((1, 'L'), (-1, 'R')): IKN['Hand_Wheel_' + nm] = tuple(cW + Mw.to_3x3() @ Vector((0, s_ * 0.24, 0)))
    # pedals hanging from the dash
    for nm, yy, w in (('Brake', yd + 0.08, 0.09), ('Accel', yd - 0.13, 0.06)):
        Pd = Part('Pedal_' + nm, [blk, chrome]); top = (3.40, yy, 1.86); pad = (3.33, yy, 1.62)
        Pd.cyl(top, pad, 0.012, 1, 10); Pd.box((pad[0] - 0.012, yy - w / 2, pad[2] - 0.09), (pad[0] + 0.012, yy + w / 2, pad[2] + 0.04), 0, 0.006)
        PARTS['Pedal_' + nm] = Pd.done(sc, top); IKN['Pedal_' + nm] = (pad[0] - 0.015, yy, pad[2] - 0.02); INFO.append('Pedal_%s pivot %.3f %.3f %.3f about y' % (nm, *top))
    # seats (driver +y, passenger -y): suspended base, cushion with bolsters, raked back with bolsters, headrest, armrest
    for yc, nm in ((yd, 'Driver'), (-yd, 'Passenger')):
        C.box((2.32, yc - 0.20, zf - 0.005), (2.68, yc + 0.20, zf + 0.20), 3, 0.02)        # air-suspension base
        for k in range(3): C.torus((0, 0, 0), 0.13, 0.024, 3, 32, 8, M=Matrix.Translation((2.50, yc, zf + 0.22 + k * 0.035)))   # bellows
        C.box((2.26, yc - 0.22, zf + 0.29), (2.74, yc + 0.22, zf + 0.315), 3, 0.01)        # seat pan
        C.box((2.22, yc - 0.19, zf + 0.30), (2.73, yc + 0.19, zf + 0.405), 2, 0.05)        # cushion
        C.cyl((2.725, yc - 0.19, zf + 0.37), (2.725, yc + 0.19, zf + 0.37), 0.042, 2, 20, 0.0)   # front roll
        for sy in (1, -1):                                                                 # side bolsters, rounded
            C._merge(sweep_tmp([(2.23, yc + sy * 0.215, zf + 0.37), (2.45, yc + sy * 0.225, zf + 0.425), (2.70, yc + sy * 0.215, zf + 0.39)], 0.048, 16), 2)
            C.box((2.25, yc + sy * 0.12 - 0.002, zf + 0.404), (2.70, yc + sy * 0.12 + 0.002, zf + 0.409), 3, 0.0)   # seams
        C.box((2.30, yc - 0.12, zf + 0.404), (2.304, yc + 0.12, zf + 0.409), 3, 0.0); C.box((2.55, yc - 0.12, zf + 0.404), (2.554, yc + 0.12, zf + 0.409), 3, 0.0)
        Mb = Matrix.Translation((2.26, yc, zf + 0.42)) @ Matrix.Rotation(math.radians(-14), 4, 'Y')
        C.box((-0.12, -0.19, 0.0), (0.0, 0.19, 0.78), 2, 0.05, M=Mb)                       # back cushion
        C.box((-0.17, -0.24, 0.02), (-0.11, 0.24, 0.80), 3, 0.035, M=Mb)                   # hard back shell
        C.cyl((0.0, -0.17, 0.20), (0.0, 0.17, 0.20), 0.03, 2, 16, 0.0, M=Mb)               # lumbar roll
        for sy in (1, -1):
            C._merge(sweep_tmp([(-0.06, sy * 0.215, 0.04), (0.01, sy * 0.225, 0.30), (-0.01, sy * 0.215, 0.58), (-0.06, sy * 0.19, 0.74)], 0.05, 16), 2, Mb)
            C.box((0.0, sy * 0.12 - 0.002, 0.05), (0.004, sy * 0.12 + 0.002, 0.74), 3, 0.0, M=Mb)
        C.box((0.0, -0.12, 0.42), (0.004, 0.12, 0.424), 3, 0.0, M=Mb)
        C.box((-0.13, -0.15, 0.85), (0.02, 0.15, 1.05), 2, 0.06, M=Mb); C.box((0.018, -0.10, 0.93), (0.022, 0.10, 0.934), 3, 0.0, M=Mb)   # headrest + seam
        for yy in (-0.08, 0.08): C.cyl((-0.06, yy, 0.77), (-0.06, yy, 0.86), 0.008, 4, 8, 0.0, M=Mb)
        ya = yc - 0.29 if yc > 0 else yc + 0.29                                            # fold-down armrest, inner side
        C._merge(sweep_tmp([(2.30, ya, zf + 0.62), (2.60, ya, zf + 0.615)], 0.038, 16), 2); C.cyl((2.27, ya, zf + 0.62), (2.27, ya + (0.03 if yc > 0 else -0.03), zf + 0.62), 0.02, 3, 12)
        IKN['Seat_' + nm] = (2.47, yc, zf + 0.42)
        C.box((2.80, yc - 0.33, zf - 0.004), (3.32, yc + 0.33, zf + 0.008), 5, 0.004)        # floor mat with ribs
        for k in range(7): C.box((2.84 + k * 0.065, yc - 0.30, zf + 0.008), (2.86 + k * 0.065, yc + 0.30, zf + 0.013), 5, 0.002)
    # console between the seats with gear selector; park brake knob on the dash
    C.box((2.62, 0.17, zf - 0.005), (3.30, 0.33, zf + 0.33), 1, 0.03)
    Gl = Part('GearLever', [blk, chrome]); gb = (3.05, 0.25, zf + 0.33)
    Gl.cyl(gb, (3.03, 0.25, zf + 0.50), 0.010, 1, 12); Gl.ball((3.03, 0.25, zf + 0.53), 0.032, 0, 16, sz=1.3)
    PARTS['GearLever'] = Gl.done(sc, gb); IKN['GearLever'] = (3.03, 0.25, zf + 0.54); INFO.append('GearLever pivot %.3f %.3f %.3f about y' % gb)
    C.box((3.02, 0.20, zf + 0.33), (3.08, 0.30, zf + 0.338), 3, 0.003)
    Pb = Part('ParkBrake', [knob, blk]); pb = (3.26, 0.14, 1.93)
    Pb.cyl(pb, (3.22, 0.14, 1.93), 0.008, 1, 10); Pb.box((3.19, 0.12, 1.915), (3.22, 0.16, 1.95), 0, 0.008)
    PARTS['ParkBrake'] = Pb.done(sc, pb); IKN['ParkBrake'] = (3.20, 0.14, 1.933); INFO.append('ParkBrake pivot %.3f %.3f %.3f, pulls along -x' % pb)
    # bunk behind the seats (storage base + mattress), back wall trim panel, headliner console, sun visors
    C.box((XB + WALL - 0.01, -1.27, zf - 0.005), (1.72, 1.27, zf + 0.46), 0, 0.03)
    C.box((XB + WALL + 0.01, -1.25, zf + 0.46), (1.70, 1.25, zf + 0.60), 7, 0.05)
    for yy in (-0.6, 0.6): C.box((1.715, yy - 0.25, zf + 0.12), (1.725, yy + 0.25, zf + 0.30), 3, 0.004)   # locker lids
    C.box((3.20, -1.0, 3.00), (3.50, 1.0, 3.14), 10, 0.04)                             # overhead shelf front edge
    for s, side in ((1, 'L'), (-1, 'R')):
        V_ = Part('SunVisor_' + side, [head, blk]); hz = 2.93
        V_.box((3.36, s * 0.18 if s > 0 else -0.98, hz - 0.02), (3.60, 0.98 if s > 0 else -0.18, hz), 0, 0.008)
        V_.cyl((3.60, s * 0.20, hz - 0.01), (3.60, s * 0.96, hz - 0.01), 0.008, 1, 10)
        PARTS['SunVisor_' + side] = V_.done(sc, (3.60, s * 0.58, hz - 0.01)); INFO.append('SunVisor_%s hinge (y axis) 3.600 %.3f %.3f, folds down' % (side, s * 0.58, hz - 0.01))
    C.cyl((1.76, -1.24, 3.06), (1.76, 1.24, 3.06), 0.012, 4, 10)                         # bunk curtain rail
    for sy in (1, -1):                                                                 # curtain drawn back to both walls, in folds
        for k in range(7): C.cyl((1.76 + 0.022 * (k % 2), sy * (1.20 - k * 0.045), zf + 0.62), (1.76 + 0.022 * (k % 2), sy * (1.20 - k * 0.045), 3.05), 0.026, 2, 10, 0.0)
    for k in range(5): C.box((2.80 + k * 0.10, -0.03, 3.40), (2.86 + k * 0.10, 0.03, 3.42), 4, 0.004)   # roof console lamps
    Mc = Matrix.Translation((3.24, 0.05, 1.78)) @ Matrix.Rotation(math.radians(-22), 4, 'Z') @ Matrix.Rotation(math.radians(-18), 4, 'Y')
    C.box((-0.04, -0.20, -0.20), (0.06, 0.20, 0.16), 1, 0.03, M=Mc)                     # centre stack turned to the driver
    C.box((-0.045, -0.15, 0.02), (-0.035, 0.15, 0.13), 6, 0.004, M=Mc)                  # its screen
    for k in range(8): C.box((-0.05, -0.16 + k * 0.042, -0.06), (-0.035, -0.13 + k * 0.042, -0.035), 3 if k % 3 else 4, 0.003, M=Mc)   # switch bank
    for k in range(3): C.cyl((-0.035, -0.10 + k * 0.10, -0.14), (-0.06, -0.10 + k * 0.10, -0.14), 0.022, 4, 20, 0.003, M=Mc)        # climate knobs
    for yy in (0.24, 1.00):                                                            # round vents either side of the binnacle
        C.torus((0, 0, 0), 0.045, 0.01, 4, 24, 8, M=Matrix.Translation((3.215, yy, 1.94)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
        for k in range(4): C.box((3.212, yy - 0.04, 1.915 + k * 0.016), (3.22, yy + 0.04, 1.92 + k * 0.016), 3, 0.001)
    IKN['Bunk'] = (1.30, 0.0, zf + 0.60)
    PARTS['CabInterior'] = C.done(sc)
    for o in (O, I): bpy.data.objects.remove(o, do_unlink=True)
    # sidewall detail on every tyre is in lorry_details.tyre (rim protector, two raised rings)
    IKS = []
    for k, p in IKN.items():
        e = bpy.data.objects.new('IK_' + k, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
        e.location = Vector(p); sc.collection.objects.link(e); IKS.append(e); INFO.append('IK_%s %.3f %.3f %.3f' % (k, *p))
    return INFO, IKS


def sweep_tmp(path, r, n=12):
    """round bar along a 3D polyline (handles, spokes) as a bmesh"""
    t = bmesh.new(); rings = []; path = [Vector(p) for p in path]
    for i, p in enumerate(path):
        tg = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        a = tg.cross(Vector((0, 0, 1)) if abs(tg.z) < 0.9 else Vector((1, 0, 0))).normalized(); b = tg.cross(a)
        rings.append([t.verts.new(p + (a * math.cos(2 * math.pi * k / n) + b * math.sin(2 * math.pi * k / n)) * r) for k in range(n)])
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n; t.faces.new((r0[i], r0[j], r1[j], r1[i]))
    t.faces.new(rings[0][::-1]); t.faces.new(rings[-1]); bmesh.ops.recalc_face_normals(t, faces=t.faces); return t
