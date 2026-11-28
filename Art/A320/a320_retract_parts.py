# To-do 380 step A (owner 2026-11-28: the landing gear retracts and extends, the gear-bay doors open and close). Run by
# make_a320_parts.py at its end (globals o, redo, split, HERE); not on its own. Makes, out of his Meshy gear pieces and new
# parts of ours (his shapes untouched: pieces are only lifted out; new doors lie on his skin):
#   NoseLegUpper      - the upper nose strut and its links, + our upper strut and trunnion up to the hinge; folds FORWARD
#   MainLegUpper_L/R  - the main strut, trunnion arm and its fittings; folds INWARDS (NoseLeg/NoseWheels, MainLeg/Wheels ride on them)
#   SideBraceUp/Lo_L/R- his side stay cut at its knee: upper link hinged at the wing, lower link hinged on the leg
#   NoseDoor_L/R      - the forward nose doors, hinged at their outer edges, open downwards (closed with the gear up AND down)
#   MainBayDoor_L/R   - the belly doors over the wheels, hinged at their keel edge, open downwards (closed up and down)
#   MainDoor_L/R      - the leg doors: closed flush in the wing when retracted, ride on MainLegUpper (made there, shown down)
#   NoseBay, MainBay_L/R - dark bays: walls from the hole edge in his skin up to a lid (under the wing's upper skin)
# The skin is cut once per bay along the hole outline (level set: edges split where the line crosses, faces inside go).
# Each part's origin is its hinge. Prints GEAR lines (Blender and glb frames) and the overlap check of the retracted gear.
import math
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
from mathutils import geometry as mgeo

def Vb(*a): return Vector(a)
def glb(v): return (round(v[0], 3), round(v[2], 3), round(-v[1], 3))       # Blender (x, y fwd, z up) -> glb (x, y up, z aft)
def vts(ob): return np.array([v.co[:] for v in ob.data.vertices]) + np.array(ob.location[:])
M_BAY = bpy.data.materials.new('A320_GearBay'); M_BAY.use_nodes = True
_b = M_BAY.node_tree.nodes['Principled BSDF']; _b.inputs['Base Color'].default_value = (0.045, 0.048, 0.05, 1); _b.inputs['Roughness'].default_value = 0.8
M_GEAR = bpy.data.objects['NoseLeg'].material_slots[0].material

# ---------------------------------------------------------------- 1. the folding pieces out of his mesh
def ns_test(lo, hi):     # the nose strut pieces above the steering collar (the ones aft of the strut stay: fixed fittings)
    c = (lo + hi) / 2
    return abs(lo[0]) < 0.75 and abs(hi[0]) < 0.75 and lo[1] > -1.4 and hi[1] < 1.3 and lo[2] > 0.3 and hi[2] < 2.55 and hi[2] - lo[2] > 0.25 and c[1] > -0.35
def zm(s, brace):
    def t(lo, hi):
        c = (lo + hi) / 2
        if s > 0 and not (lo[0] > 1.9 and hi[0] < 5.4): return False
        if s < 0 and not (hi[0] < -1.9 and lo[0] > -5.4): return False
        if not (lo[1] > -14.3 and hi[1] < -11.3 and c[1] < -12.0 and lo[2] > 0.3 and hi[2] < 3.45 and hi[2] - lo[2] > 0.3): return False
        if math.hypot(c[0] - s * 5.45, c[2] - 1.97) < 1.45 and lo[1] > -12.0: return False      # (the engine's exhaust)
        if abs(c[0]) > 4.55: return False                                                         # wing fittings outboard of the hinge: fixed
        if brace and (lo[2] < 2.2 or min(abs(lo[0]), abs(hi[0])) < 2.6): return False                                                  # (a skin piece on the belly, not a stay)
        return (hi[0] - lo[0] > 0.7) == brace
    return t
nose_up = redo(ns_test, 'NoseLegUpper')
SB = {}; MU = {}
for s, sd in ((-1, '_L'), (1, '_R')):
    SB[sd] = redo(zm(s, True), 'SideBrace' + sd); MU[sd] = redo(zm(s, False), 'MainLegUpper' + sd)

# his skin (what is left of the A320 part), for shooting the doors and bays onto it and taking its UVs
src = bmesh.new(); src.from_mesh(o.data); src.faces.ensure_lookup_table()
UVL = src.loops.layers.uv.active
BVH0 = BVHTree.FromBMesh(src)
def skin(x, y, z0=1.6):          # first surface going up from z0 at (x, y): (point, face index)
    p = Vb(x, y, z0)
    h = BVH0.ray_cast(p, Vb(0, 0, 1), 8.0)
    return (h[0], h[2]) if h[0] is not None else (None, None)
def upper(x, y, z):              # next surface above z
    h = BVH0.ray_cast(Vb(x, y, z + 0.02), Vb(0, 0, 1), 8.0)
    return h[0].z if h[0] is not None else 99.0
def uv_at(P, fi):
    f = src.faces[fi]; ls = f.loops
    a, b, c = ls[0], ls[1], ls[2]
    w = mgeo.barycentric_transform(P, a.vert.co, b.vert.co, c.vert.co, Vb(1, 0, 0), Vb(0, 1, 0), Vb(0, 0, 1))
    uv = a[UVL].uv * w[0] + b[UVL].uv * w[1] + c[UVL].uv * w[2]
    return uv, f.material_index

def new_obj(name, verts, faces, mats, uvs=None, fmat=None):
    me = bpy.data.meshes.new(name); me.from_pydata([tuple(v) for v in verts], [], faces); me.update()
    for m in mats: me.materials.append(m)
    if fmat is not None:
        for p, mi in zip(me.polygons, fmat): p.material_index = mi
    if uvs is not None:
        ul = me.uv_layers.new(name='UVMap')
        for p in me.polygons:
            for li, vi in zip(p.loop_indices, p.vertices): ul.data[li].uv = uvs[vi]
    else:
        ul = me.uv_layers.new(name='UVMap')
        for p in me.polygons:
            for li, vi in zip(p.loop_indices, p.vertices): ul.data[li].uv = (verts[vi][0] * 0.5, verts[vi][1] * 0.5 + verts[vi][2] * 0.5)
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    for p in me.polygons: p.use_smooth = False
    return ob

# ---------------------------------------------------------------- 2. holes, bays, doors (plan rectangles, Blender x, y)
NOSE_Y0 = -1.20
HOLES = {'NoseBay': (-0.80, 0.80, NOSE_Y0, 3.05, 2.0)}                 # x0, x1, y0, y1, lowest skin z to shoot from
for s, sd in ((-1, '_L'), (1, '_R')):
    xa, xb = sorted((s * 0.75, s * 4.45)); HOLES['MainBay' + sd] = (xa, xb, -13.80, -11.35, 1.8)
LIDMAX = {'NoseBay': 3.45}
def lid_z(name, x, y):
    if name == 'NoseBay': return LIDMAX[name]
    P, _ = skin(x, y, 1.8)
    z0 = P.z if P else 2.0
    return min(4.45, upper(x, y, z0 + 0.08) - 0.04)
def samples(a, b, step=0.05):
    n = max(2, int(math.ceil(abs(b - a) / step)) + 1); return [a + (b - a) * i / (n - 1) for i in range(n)]

def cut_hole(name):
    x0, x1, y0, y1, zs = HOLES[name]
    bm = bmesh.new(); bm.from_mesh(o.data)
    def cap(v): return lid_z(name, v.co.x, v.co.y) if (x0 - 0.1 < v.co.x < x1 + 0.1 and y0 - 0.1 < v.co.y < y1 + 0.1) else -9
    def f(v):                    # negative inside the hole (below the lid)
        q = max(x0 - v.co.x, v.co.x - x1, y0 - v.co.y, v.co.y - y1)
        if q < 0.15 and v.co.z > cap(v): return 0.5
        return q
    val = {v: f(v) for v in bm.verts if x0 - 1.5 < v.co.x < x1 + 1.5 and y0 - 1.5 < v.co.y < y1 + 1.5}
    cross = [e for e in bm.edges if e.verts[0] in val and e.verts[1] in val and (val[e.verts[0]] < 0) != (val[e.verts[1]] < 0)]
    new = set()
    for e in cross:
        a, b = e.verts; fa, fb = val[a], val[b]
        _, nv = bmesh.utils.edge_split(e, a, fa / (fa - fb)); new.add(nv); val[nv] = 0.0
    for fc in list(set(fc for v in new for fc in v.link_faces)):
        on = [l.vert for l in fc.loops if l.vert in new]
        if len(on) == 2 and not any(e for e in on[0].link_edges if e.other_vert(on[0]) is on[1]):
            try: bmesh.utils.face_split(fc, on[0], on[1])
            except Exception: pass
    gone = []
    for fc in bm.faces:
        c = fc.calc_center_median()
        if x0 < c.x < x1 and y0 < c.y < y1 and c.z < lid_z(name, c.x, c.y) and all(l.vert in val for l in fc.loops): gone.append(fc)
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    bm.to_mesh(o.data); bm.free(); o.data.update()
    print('HOLE %s: %d edges split, %d faces out' % (name, len(cross), len(gone)))

def bay(name):                    # walls from the hole edge on his skin up to the lid, and the lid; normals into the bay
    x0, x1, y0, y1, zs = HOLES[name]
    xs, ys = samples(x0, x1), samples(y0, y1)
    ring = [(x, y0) for x in xs[:-1]] + [(x1, y) for y in ys[:-1]] + [(x, y1) for x in xs[:0:-1]] + [(x0, y) for y in ys[:0:-1]]
    V = []; F = []
    for x, y in ring:
        P, _ = skin(x, y, zs); zb = (P.z if P else 2.2) + 0.002
        V += [(x, y, zb), (x, y, lid_z(name, x, y))]
    n = len(ring)
    for i in range(n):
        j = (i + 1) % n; F.append((2 * i, 2 * j, 2 * j + 1, 2 * i + 1))
    base = len(V); nx = len(xs)
    for y in ys:
        for x in xs: V.append((x, y, lid_z(name, x, y)))
    for r in range(len(ys) - 1):
        for c in range(nx - 1):
            a = base + r * nx + c; F.append((a, a + 1, a + nx + 1, a + nx))
    ob = new_obj(name, V, F, [M_BAY])
    bm = bmesh.new(); bm.from_mesh(ob.data); cen = Vb((x0 + x1) / 2, (y0 + y1) / 2, 2.6)
    for fc in bm.faces:
        fc.normal_update(); c = fc.calc_center_median()
        if fc.normal.z > 0.7 or (abs(fc.normal.z) < 0.7 and fc.normal.dot(Vb(cen.x - c.x, cen.y - c.y, 0)) < 0): fc.normal_flip()
    bm.to_mesh(ob.data); bm.free()
    return ob

def door(name, x0, x1, y0, y1, zs, t=0.03, step=0.05):     # a panel on his skin: outer face with his texture, inner face and edges dark
    xs, ys = samples(x0, x1, step), samples(y0, y1, step); nx, ny = len(xs), len(ys)
    V = []; UV = []; mats = {}; fm = []
    MAT = []
    def mslot(m):
        if m not in mats: mats[m] = len(MAT); MAT.append(m)
        return mats[m]
    out_mi = []
    for y in ys:
        for x in xs:
            P, fi = skin(x, y, zs)
            if P is None: P, fi = Vb(x, y, 2.2), None
            V.append(P[:])
            if fi is not None:
                uv, mi = uv_at(P, fi); UV.append(uv[:]); out_mi.append(mi)
            else: UV.append((0, 0)); out_mi.append(0)
    zz = np.array([v[2] for v in V]).reshape(ny, nx)
    # his skin has gaps and other pieces lie on it: along each line across the door (fore-aft) the skin is smooth - fit a
    # quadratic, drop what is off it by more than 2 cm, fit again; the dropped points go onto the fit
    w = np.array(ys) - np.mean(ys); A = np.stack([np.ones_like(w), w, w * w], 1); fixed = 0
    for c in range(nx):
        col = zz[:, c].copy(); ok = np.abs(col - np.median(col)) < 0.25
        for _ in range(6):
            cf = np.linalg.lstsq(A[ok], col[ok], rcond=None)[0]; fit = A @ cf; ok2 = np.abs(col - fit) < 0.02
            if (ok2 == ok).all() or ok2.sum() < 4: break
            ok = ok2
        fixed += int((~ok).sum()); zz[:, c] = np.where(ok, col, fit)
    for _ in range(3):           # and across: a line that is off its neighbours by more than 5 cm goes onto their median
        pad = np.pad(zz, ((0, 0), (2, 2)), mode='edge'); med = np.median(np.stack([pad[:, j:j + nx] for j in range(5)]), 0)
        bad = np.abs(zz - med) > 0.05; fixed += int(bad.sum()); zz = np.where(bad, med, zz)
    for k in range(len(V)):
        r, c = divmod(k, nx)
        if abs(V[k][2] - zz[r, c]) > 1e-6:
            P = Vb(V[k][0], V[k][1], float(zz[r, c])); hit = BVH0.find_nearest(P)
            V[k] = P[:]
            if hit[0] is not None: uv, mi = uv_at(hit[0], hit[2]); UV[k] = uv[:]; out_mi[k] = mi
    print('DOOR %s: %d points off his skin (other pieces or misses) set to their neighbours' % (name, fixed))
    bump = max(float(np.abs(np.diff(zz, axis=0)).max()), float(np.abs(np.diff(zz, axis=1)).max()))
    for v in list(V): V.append((v[0], v[1], v[2] + t)); UV.append((v[0] * 0.5, v[1] * 0.5))
    F = []; N = nx * ny
    bm_slot = mslot(o.material_slots[max(set(out_mi), key=out_mi.count)].material); dark = mslot(M_BAY)
    for r in range(ny - 1):
        for c in range(nx - 1):
            a = r * nx + c
            F.append((a, a + nx, a + nx + 1, a + 1)); fm.append(bm_slot)                    # outer (faces down)
            F.append((N + a, N + a + 1, N + a + nx + 1, N + a + nx)); fm.append(dark)       # inner (faces up, into the bay)
    edge = [r * nx for r in range(ny)][::-1] + [c for c in range(nx)] + [r * nx + nx - 1 for r in range(ny)] + [(ny - 1) * nx + c for c in range(nx)][::-1]
    loop = []; [loop.append(i) for i in edge if not loop or loop[-1] != i]
    if loop[0] == loop[-1]: loop.pop()
    for i in range(len(loop)):
        a, b = loop[i], loop[(i + 1) % len(loop)]
        F.append((a, b, N + b, N + a)); fm.append(dark)
    ob = new_obj(name, V, F, MAT, UV, fm)
    bm = bmesh.new(); bm.from_mesh(ob.data); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(ob.data); bm.free()
    for p in ob.data.polygons: p.use_smooth = True
    print('DOOR %s %d x %d verts, steepest step between neighbours %.3f m' % (name, nx, ny, bump))
    return ob

for nm in HOLES: cut_hole(nm)
BAYS = {nm: bay(nm) for nm in HOLES}
SEAM = 0.006
DOORS = {}
for s, sd in ((-1, '_L'), (1, '_R')):
    xa, xb = sorted((s * SEAM / 2, s * (0.80 - SEAM))); DOORS['NoseDoor' + sd] = door('NoseDoor' + sd, xa, xb, 0.45, 3.05 - SEAM, 2.0)
    xa, xb = sorted((s * (0.75 + SEAM), s * (2.55 - SEAM / 2))); DOORS['MainBayDoor' + sd] = door('MainBayDoor' + sd, xa, xb, -13.80 + SEAM, -11.35 - SEAM, 1.8)
    xa, xb = sorted((s * (2.55 + SEAM / 2), s * (4.45 - SEAM))); DOORS['MainDoor' + sd] = door('MainDoor' + sd, xa, xb, -13.80 + SEAM, -11.35 - SEAM, 1.8)

# ---------------------------------------------------------------- 3. hinges, the overlap check, the angles
def about(ob_list, piv, axis, deg):
    R = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-piv)
    return R
def tris(ob, M=Matrix()):
    bm = bmesh.new(); bm.from_mesh(ob.data); bm.transform(Matrix.Translation(ob.location)); bm.transform(M); return bm
def bvh_of(obs, M=Matrix()):
    bm = bmesh.new()
    for ob in obs:
        b = tris(ob, M); me = bpy.data.meshes.new('_t'); b.to_mesh(me); b.free(); bm.from_mesh(me); bpy.data.meshes.remove(me)
    return BVHTree.FromBMesh(bm)
STATIC = [o] + list(BAYS.values()) + [DOORS[k] for k in DOORS if not k.startswith('MainDoor')]
BV_STATIC = bvh_of(STATIC)

# nose: hinge on the strut's column, high enough that the folded wheels sit in the bay above the doors
col = vts(nose_up); top = col[(col[:, 2] > 1.95) & (np.abs(col[:, 0]) < 0.2)]
NY = float(top[:, 1].mean()) if len(top) else 0.0
nose_parts = [nose_up, bpy.data.objects['NoseLeg'], bpy.data.objects['NoseWheels']]
best = None
for pz in (2.80, 2.90, 3.00):
    for ang in (90.0, 95.0, 100.0):
        piv = Vb(0.0, NY, pz); hits = len(BV_STATIC.overlap(bvh_of(nose_parts, about(None, piv, 'X', ang))))
        print('CHECK nose hinge z %.2f, %.0f deg: %d touching triangle pairs' % (pz, ang, hits))
        if best is None or hits < best[0]: best = (hits, pz, ang)
def breakdown(tag, moving, M):
    for st in STATIC:
        b = bvh_of([st]); n = 0; txt = []
        for mv in moving:
            pairs = b.overlap(bvh_of([mv], M))
            if pairs:
                n += len(pairs); txt.append('%s %d' % (mv.name, len(pairs)))
        if n: print('TOUCH %s: %s with %s' % (tag, st.name, ', '.join(txt)))
breakdown('nose', nose_parts, about(None, Vb(0.0, NY, best[1]), 'X', best[2]))
NOSE = dict(piv=Vb(0.0, NY, best[1]), axis=Vb(1, 0, 0), deg=best[2], hits=best[0])
# our upper strut and trunnion from his strut top to the hinge
def tube(p0, p1, r, n=16):
    d = (p1 - p0); L = d.length; d.normalize(); q = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    V = []; F = []
    for k, p in enumerate((p0, p1)):
        for i in range(n):
            a = 2 * math.pi * i / n; V.append(tuple(p + q.to_3x3() @ Vb(r * math.cos(a), r * math.sin(a), 0)))
    for i in range(n): F.append((i, (i + 1) % n, n + (i + 1) % n, n + i))
    F.append(tuple(range(n))[::-1]); F.append(tuple(range(n, 2 * n)))
    return V, F
def add_tubes(target, specs):
    for p0, p1, r in specs:
        V, F = tube(p0, p1, r); t = new_obj('_tube', V, F, [M_GEAR])
        for p in t.data.polygons: p.use_smooth = True
        for x in bpy.context.selected_objects: x.select_set(False)
        t.select_set(True); target.select_set(True); bpy.context.view_layer.objects.active = target; bpy.ops.object.join()
P = NOSE['piv']; ztop = float(top[:, 2].max()) - 0.05 if len(top) else 2.2
add_tubes(nose_up, [(Vb(0, NY, ztop), Vb(0, NY, P.z + 0.08), 0.075), (Vb(-0.38, NY, P.z), Vb(0.38, NY, P.z), 0.06)])

MAIN = {}
for s, sd in ((-1, '_L'), (1, '_R')):
    mu = MU[sd]; v = vts(mu); hi = v[v[:, 2] > 2.85]; ml = vts(bpy.data.objects['MainLeg' + sd])
    px = float(hi[:, 0].mean()) if len(hi) else s * 4.3; py = float((ml[:, 1].min() + ml[:, 1].max()) / 2)
    parts = [mu, bpy.data.objects['MainLeg' + sd], bpy.data.objects['MainWheels' + sd], DOORS['MainDoor' + sd]]
    best = None
    for pz in (3.00, 3.10, 3.20, 3.30):
        for ang in (80.0, 85.0, 90.0, 95.0):
            piv = Vb(px, py, pz); M = about(None, piv, 'Y', s * ang)
            mv = bvh_of(parts[:3], M); bvd = bvh_of([parts[3]])                 # the leg door: built in its retracted place
            hits = len(BV_STATIC.overlap(mv)) + len(mv.overlap(bvd))
            print('CHECK main%s hinge z %.2f, %.0f deg: %d touching triangle pairs' % (sd, pz, ang, hits))
            if best is None or hits < best[0]: best = (hits, pz, ang)
    breakdown('main' + sd, parts[:3], about(None, Vb(px, py, best[1]), 'Y', s * best[2]))
    MAIN[sd] = dict(piv=Vb(px, py, best[1]), axis=Vb(0, 1, 0), deg=s * best[2], hits=best[0])
    P = MAIN[sd]['piv']
    add_tubes(mu, [(Vb(P.x, py - 0.7, P.z), Vb(P.x, py + 0.7, P.z), 0.07)])
    # the leg door rides on the leg: from its flush (retracted) place down to where it is with the gear down
    d = DOORS['MainDoor' + sd]; d.data.transform(about(None, P, 'Y', -MAIN[sd]['deg']))
    # the side stay: cut at its knee; upper link about its wing end, lower link about its leg end
    sb = SB[sd]
    if sb is not None:
        v = vts(sb); order = np.argsort(np.abs(v[:, 0]))
        B = Vector(v[order[:30]].mean(0).tolist()); A = Vector(v[order[-30:]].mean(0).tolist()); J = (A + B) / 2
        lo_ = sb.copy(); lo_.data = sb.data.copy(); bpy.context.scene.collection.objects.link(lo_)
        for ob, keep_inner in ((sb, True), (lo_, False)):
            bm = bmesh.new(); bm.from_mesh(ob.data); n = (A - B).normalized()
            r = bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=J, plane_no=n, clear_outer=keep_inner, clear_inner=not keep_inner)
            edges = [e for e in bm.edges if e.is_boundary]
            if edges: bmesh.ops.holes_fill(bm, edges=edges, sides=0)
            bm.to_mesh(ob.data); bm.free()
        sb.name = sb.data.name = 'SideBraceUp' + sd; lo_.name = lo_.data.name = 'SideBraceLo' + sd
        Rm = about(None, P, 'Y', MAIN[sd]['deg']); A2 = Rm @ A
        L1, L2 = (J - B).length, (A - J).length; dx, dz = A2.x - B.x, A2.z - B.z; dd = math.hypot(dx, dz)
        a = (L1 * L1 - L2 * L2 + dd * dd) / (2 * dd); h = math.sqrt(max(0.0, L1 * L1 - a * a))
        mx, mz = B.x + a * dx / dd, B.z + a * dz / dd
        Js = [Vb(mx + h * dz / dd, J.y, mz - h * dx / dd), Vb(mx - h * dz / dd, J.y, mz + h * dx / dd)]
        J2 = max(Js, key=lambda q: q.z)
        def ang2(u, w): return math.degrees(math.atan2(u.z * w.x - u.x * w.z, u.x * w.x + u.z * w.z))   # signed, about +y
        up_deg = ang2(J - B, J2 - B); lo_world = ang2(J - A, J2 - A2)
        MAIN[sd].update(B=B, A=A, up_deg=up_deg, lo_deg=lo_world - MAIN[sd]['deg'], reach=dd <= L1 + L2)

# ---------------------------------------------------------------- 4. origins at the hinges, the GEAR lines
def origin(ob, p):
    ob.data.transform(Matrix.Translation(Vector(ob.location) - p)); ob.location = p
def hinge_line(name, ob, p, axis, deg, note):
    origin(ob, p)
    ga = glb(axis); print('GEAR %-15s hinge glb %s axis glb %s retract %+.1f deg | blender %s | %s' % (name, glb(p), ga, deg, tuple(round(c, 3) for c in p), note))
hinge_line('NoseLegUpper', nose_up, NOSE['piv'], NOSE['axis'], NOSE['deg'], 'forward; NoseLeg+NoseWheels ride on it; check %d' % NOSE['hits'])
for s, sd in ((-1, '_L'), (1, '_R')):
    m = MAIN[sd]
    hinge_line('MainLegUpper' + sd, MU[sd], m['piv'], m['axis'], m['deg'], 'inwards; MainLeg+MainWheels+MainDoor ride on it; check %d' % m['hits'])
    hinge_line('MainDoor' + sd, DOORS['MainDoor' + sd], m['piv'], m['axis'], m['deg'], 'leg door, child of MainLegUpper (same hinge)')
    if 'B' in m:
        hinge_line('SideBraceUp' + sd, bpy.data.objects['SideBraceUp' + sd], m['B'], m['axis'], m['up_deg'], 'about its wing end')
        hinge_line('SideBraceLo' + sd, bpy.data.objects['SideBraceLo' + sd], m['A'], m['axis'], m['lo_deg'], 'child of MainLegUpper, about its leg end, relative%s' % ('' if m['reach'] else ' (links cannot reach: approx)'))
    for nm, edge in (('NoseDoor', 0.80), ('MainBayDoor', 0.75)):
        x0, x1, y0, y1, zs = HOLES['NoseBay' if nm == 'NoseDoor' else 'MainBay' + sd]
        hx = s * (edge - SEAM)
        P0, _ = skin(hx, (y0 + y1) / 2, zs); hp = Vb(hx, (y0 + y1) / 2, (P0.z if P0 else 2.2) + 0.015)
        deg = (-s * 90.0) if nm == 'NoseDoor' else (s * 90.0)
        hinge_line(nm + sd, DOORS[nm + sd], hp, Vb(0, 1, 0), deg, 'open angle (hinge at its %s edge, opens downwards; closed with the gear up and down)' % ('outer' if nm == 'NoseDoor' else 'keel'))
for nm, ob in BAYS.items(): print('BAY %s fixed, dark' % nm)
src.free()
