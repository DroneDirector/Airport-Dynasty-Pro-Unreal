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
# (380 A2) Not solved yet: his wing is 0.5-0.6 m deep over the main leg and the folded strut is 0.63-0.75 m wide, so the
# folded main leg still pokes through the lower skin and the bay lid (about 1260-1370 touching pairs per side); the nose
# folds clean (0 pairs). The right gear is the left one mirrored; the doors are fitted to his skin (2D fit, within 12 mm).
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
        if not (lo[1] > -14.3 and hi[1] < -11.3 and c[1] < -11.9 and lo[2] > 0.3 and hi[2] < 3.45 and hi[2] - lo[2] > 0.3): return False
        if math.hypot(c[0] - s * 5.45, c[2] - 1.97) < 1.45 and lo[1] > -12.0: return False      # (the engine's exhaust)
        if abs(c[0]) > 4.55: return False                                                         # wing fittings outboard of the hinge: fixed
        if brace and (lo[2] < 2.2 or min(abs(lo[0]), abs(hi[0])) < 2.6): return False                                                  # (a skin piece on the belly, not a stay)
        return (hi[0] - lo[0] > 0.7) == brace
    return t
nose_up = redo(ns_test, 'NoseLegUpper')
SB = {}; MU = {}
SB['_L'] = redo(zm(-1, True), 'SideBrace_L'); MU['_L'] = redo(zm(-1, False), 'MainLegUpper_L')
# (380 A2) the gear is symmetric: the right gear and the small pieces round it are the left ones mirrored (his left side kept)
def islands_of(me):
    b = bmesh.new(); b.from_mesh(me); b.faces.ensure_lookup_table(); sn = set(); out = []
    for f in b.faces:
        if f.index in sn: continue
        st = [f]; comp = []; sn.add(f.index)
        while st:
            g = st.pop(); comp.append(g.index)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in sn: sn.add(h.index); st.append(h)
        vs = np.array([v.co[:] for i in comp for v in b.faces[i].verts]); out.append((comp, vs.min(0), vs.max(0)))
    b.free(); return out
def mirror_mesh(me_src, keep=None):            # a mirrored copy (x -> -x), winding and split normals mirrored too
    me_src.calc_normals_split()
    VK = lambda c: (round(c[0], 4), round(c[1], 4), round(c[2], 4))
    nrm = {(p.index,) + VK(me_src.vertices[me_src.loops[li].vertex_index].co): me_src.loops[li].normal.copy() for p in me_src.polygons for li in p.loop_indices}
    me = me_src.copy(); bm = bmesh.new(); bm.from_mesh(me); bm.faces.ensure_lookup_table()
    if keep is not None: bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context='FACES')
    bm.faces.ensure_lookup_table(); fidx = [f.index for f in bm.faces]
    bm.transform(Matrix.Scale(-1, 4, (1, 0, 0))); bmesh.ops.reverse_faces(bm, faces=bm.faces[:]); bm.to_mesh(me); bm.free()
    old = sorted(keep) if keep is not None else list(range(len(me_src.polygons)))
    me.use_auto_smooth = True; cl = []
    for p in me.polygons:
        po = old[p.index]
        for li in p.loop_indices:
            c = me.vertices[me.loops[li].vertex_index].co; n = nrm.get((po,) + VK((-c.x, c.y, c.z)), Vector((0, 0, 1))); cl.append((-n.x, n.y, n.z))
    me.normals_split_custom_set(cl); return me
for nm in ('MainWheels_R', 'MainLeg_R'):
    if nm in bpy.data.objects: bpy.data.objects.remove(bpy.data.objects[nm])
for t_ in (zm(1, True), zm(1, False)):                   # his right gear pieces out, as on the left ...
    r_ = redo(t_, '_right')
    if r_ is not None: bpy.data.objects.remove(r_)
for nm in ('MainWheels', 'MainLeg', 'MainLegUpper', 'SideBrace'):
    sl = bpy.data.objects.get(nm + '_L')
    if sl is None: continue
    ob = bpy.data.objects.new(nm + '_R', mirror_mesh(sl.data)); ob.data.name = nm + '_R'; bpy.context.scene.collection.objects.link(ob)
    ob.location = (-sl.location.x, sl.location.y, sl.location.z)
    if nm == 'MainLegUpper': MU['_R'] = ob
    if nm == 'SideBrace': SB['_R'] = ob

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
# (380 A2) height maps of his skin over each main gear hole: lower skin (outside) and the bay lid (under the upper skin).
# His lower wing skin has a gap round the top of the leg (his gear well): there the first hit is the upper skin - the lower
# skin is taken from the straight line along the row (the wing's underside), the hit becomes the upper skin.
ST = 0.04
def build_map(x0, x1):
    xs = np.arange(x0, x1 + 1e-6, ST); ys = np.arange(-14.0, -11.1 + 1e-6, ST)
    LO = np.full((len(ys), len(xs)), 2.5); UP = np.full_like(LO, 99.0)
    for j, yy in enumerate(ys):
        for i, xx in enumerate(xs):
            Pk, _ = skin(xx, yy, 0.5)
            if Pk is not None: LO[j, i] = Pk.z; UP[j, i] = upper(xx, yy, Pk.z + 0.06)
    wing = np.abs(xs) > 2.45; xw = xs[wing]; nfix = 0
    for j in range(len(ys)):
        z = LO[j, wing]; ok = np.ones(len(z), bool)
        for thr in (0.3, 0.12, 0.06, 0.04):
            cf = np.polyfit(xw[ok], z[ok], 1); fit = np.polyval(cf, xw); ok2 = np.abs(z - fit) < thr
            if ok2.sum() < 6: break
            ok = ok2
        bad = (z - fit) > 0.12; nfix += int(bad.sum())
        col = np.where(wing)[0][bad]; UP[j, col] = np.minimum(UP[j, col], LO[j, col]); LO[j, col] = fit[bad]
    TOP = np.full_like(LO, 99.0)                 # his outer top skin (a ray down from above): the lid never reaches it
    for j, yy in enumerate(ys):
        for i, xx in enumerate(xs):
            h = BVH0.ray_cast(Vb(xx, yy, 12.0), Vb(0, 0, -1), 20.0)
            if h[0] is not None: TOP[j, i] = h[0].z
    LID = np.maximum(np.minimum(np.minimum(4.45, UP - 0.04), TOP - 0.04), LO + 0.08)
    print('MAP %.1f..%.1f: %d points of the wing underside missing in his skin (taken from the line); %d points with under 12 cm between his lower and upper skin' % (x0, x1, nfix, int((TOP - LO < 0.12).sum())))
    return xs, ys, LO, LID
MAPS = {'_L': build_map(-4.6, -0.6), '_R': build_map(0.6, 4.6)}
def map_at(sd, x, y):
    xs, ys, LO, LID = MAPS[sd]
    i = int(np.clip(round((x - xs[0]) / ST), 0, len(xs) - 1)); j = int(np.clip(round((y - ys[0]) / ST), 0, len(ys) - 1))
    return float(LO[j, i]), float(LID[j, i])
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

# ---------------------------------------------------------------- 2. holes, bays, doors (plan outlines, Blender x, y)
# (380 A3) The hole in his skin follows a clean outline (a rectangle with rounded corners, in the plane of his skin), cut
# 1.5 cm inside it; that edge is hidden under a dark rim that lies on his skin (1.5 mm out) from 3 cm
# inside to 4 cm outside the outline and turns up into the bay as its wall. The doors are 5 cm bigger than the hole all
# round (3.5 mm out from his skin, rolled edges), so closed they cover the rim and read as flush panels with a seam.
NOSE_Y0 = -0.78
HOLES = {'NoseBay': (-0.80, 0.80, NOSE_Y0, 3.05, 2.0)}                 # x0, x1, y0, y1, lowest skin z to shoot from
for s, sd in ((-1, '_L'), (1, '_R')):
    xa, xb = sorted((s * 0.75, s * 4.45)); HOLES['MainBay' + sd] = (xa, xb, -13.80, -11.35, 1.8)
LIDMAX = {'NoseBay': 3.95}
RC = 0.08                        # corner radius of the outlines
RIM_IN, RIM_OUT, RIM_OFF = 0.03, 0.04, 0.0015
DOOR_OUT, DOOR_OFF, DOOR_EDGE = 0.05, 0.0035, 0.001
def lid_z(name, x, y):
    if name == 'NoseBay': return LIDMAX[name]
    return map_at(name[-2:], x, y)[1]
def samples(a, b, step=0.05):
    n = max(2, int(math.ceil(abs(b - a) / step)) + 1); return [a + (b - a) * i / (n - 1) for i in range(n)]
def sdf(name, x, y):             # signed plan distance to the outline (negative inside)
    x0, x1, y0, y1, _ = HOLES[name]; cx, cy, hx, hy = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
    qx, qy = abs(x - cx) - (hx - RC), abs(y - cy) - (hy - RC)
    return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - RC
def ring(name, d, step=0.04, na=6):   # the outline moved out by d: same point count for every d (rows of one grid)
    x0, x1, y0, y1, _ = HOLES[name]; cx, cy, ex, ey = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2 - RC, (y1 - y0) / 2 - RC
    r = RC + d; P = []
    for (ax, ay), (bx, by), a0 in (((-ex, -ey), (ex, -ey), -90), ((ex, -ey), (ex, ey), 0), ((ex, ey), (-ex, ey), 90), ((-ex, ey), (-ex, -ey), 180)):
        n = max(1, int(math.ceil(math.hypot(bx - ax, by - ay) / step)))
        nx_, ny_ = math.cos(math.radians(a0)), math.sin(math.radians(a0))
        for i in range(n):
            t = i / n; P.append((cx + ax + (bx - ax) * t + r * nx_, cy + ay + (by - ay) * t + r * ny_))
        for i in range(na):
            a = math.radians(a0 + 90.0 * i / na); P.append((cx + bx + r * math.cos(a), cy + by + r * math.sin(a)))
    return P
def skin_z(name, x, y):          # his outer skin under (x, y): the cleaned map for the main bays, a ray for the nose
    if name == 'NoseBay':
        P_, _ = skin(x, y, HOLES[name][4]); return P_.z if P_ is not None else None
    xs, ys, LO, LID = MAPS[name[-2:]]
    fx = float(np.clip((x - xs[0]) / ST, 0, len(xs) - 1.001)); fy = float(np.clip((y - ys[0]) / ST, 0, len(ys) - 1.001))
    i, j = int(fx), int(fy); u, w = fx - i, fy - j
    return float(LO[j, i] * (1 - u) * (1 - w) + LO[j, i + 1] * u * (1 - w) + LO[j + 1, i] * (1 - u) * w + LO[j + 1, i + 1] * u * w)
def smooth_row(z):               # pieces lying on his skin make bumps: along the ring, points off the running median go onto it
    z = np.array([np.nan if q is None else q for q in z], float); n = len(z); out = z.copy()
    for i in range(n):
        win = z[[(i + k) % n for k in range(-5, 6)]]; med = np.nanmedian(win)
        if np.isnan(z[i]) or abs(z[i] - med) > 0.01: out[i] = med
    return out

CUT_AT = -0.015                  # the hole's edge in his skin: 1.5 cm inside the outline, in the middle of the rim (hidden)
def cut_hole(name):              # his big triangles reach up to 0.4 m past the outline, so whole faces can't go: the faces
    bm = bmesh.new(); bm.from_mesh(o.data)            # crossing the line are made triangles, split along the line, and
    x0, x1, y0, y1, _ = HOLES[name]                   # the inside goes; the line lies under the rim, so no edge is seen
    def near(c): return x0 - 0.6 < c.x < x1 + 0.6 and y0 - 0.6 < c.y < y1 + 0.6
    def low(v): return v.co.z < lid_z(name, v.co.x, v.co.y)
    val = {v: sdf(name, v.co.x, v.co.y) - CUT_AT for v in bm.verts if near(v.co) and low(v)}
    cross = [f for f in bm.faces if all(l.vert in val for l in f.loops) and min(val[l.vert] for l in f.loops) < 0 < max(val[l.vert] for l in f.loops)]
    bmesh.ops.triangulate(bm, faces=cross)
    edges = [e for e in bm.edges if e.verts[0] in val and e.verts[1] in val and (val[e.verts[0]] < 0) != (val[e.verts[1]] < 0)]
    new = set()
    for e in edges:
        a, b = e.verts; fa, fb = val[a], val[b]
        _, nv = bmesh.utils.edge_split(e, a, fa / (fa - fb)); new.add(nv); val[nv] = 0.0
    fail = 0
    for fc in list(set(fc for v in new for fc in v.link_faces)):
        on = [l.vert for l in fc.loops if l.vert in new]
        if len(on) == 2 and not any(e for e in on[0].link_edges if e.other_vert(on[0]) is on[1]):
            try: bmesh.utils.face_split(fc, on[0], on[1])
            except Exception: fail += 1
    gone = []
    for fc in bm.faces:
        if all(l.vert in val for l in fc.loops) and sum(val[l.vert] for l in fc.loops) / len(fc.loops) < 0: gone.append(fc)
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    left = sum(1 for fc in bm.faces if all(l.vert in val for l in fc.loops) and min(val[l.vert] for l in fc.loops) < -0.003)
    bm.to_mesh(o.data); bm.free(); o.data.update()
    print('HOLE %s: %d faces split along the line (%d failed), %d faces out; %d faces still reaching past the line into the hole (shards)' % (name, len(edges), fail, len(gone), left))

def bay(name):                    # the rim on his skin, the walls up to the lid, the lid; normals out of the skin / into the bay
    D = (RIM_OUT, 0.02, 0.0, -RIM_IN)
    rows = []
    for k, d in enumerate(D):
        pts = ring(name, d); z = smooth_row([skin_z(name, x, y) for x, y in pts])
        off = 0.001 if k == 0 else RIM_OFF
        rows.append([(x, y, zz - off) for (x, y), zz in zip(pts, z)])
    inner = rows[-1]
    rows.append([(x, y, z + 0.03) for x, y, z in inner])                         # the rolled lip into the bay
    rows.append([(x, y, lid_z(name, x, y)) for x, y, z in inner])
    n = len(inner); V = [v for r in rows for v in r]; F = []
    for r in range(len(rows) - 1):
        for i in range(n):
            j = (i + 1) % n; F.append((r * n + i, r * n + j, (r + 1) * n + j, (r + 1) * n + i))
    x0, x1, y0, y1, zs = HOLES[name]
    xs, ys = samples(x0 + RIM_IN, x1 - RIM_IN), samples(y0 + RIM_IN, y1 - RIM_IN)
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
    for p in ob.data.polygons: p.use_smooth = True
    print('RIM %s: %d points round, %.2f m inside to %.2f m outside the outline, %.1f mm off his skin' % (name, n, RIM_IN, RIM_OUT, 1000 * RIM_OFF))
    return ob

def edge_samples(a, b, step=0.05, bev=0.01):    # door grid lines: the edge, a bevel line 1 cm in, then even steps
    return [a] + samples(a + bev, b - bev, step) + [b]
def door(name, x0, x1, y0, y1, zs, t=0.025, step=0.05):  # a panel on his skin: outer face with his texture (3.5 mm out, edges rolled to 1 mm), inner face and edges dark
    xs, ys = edge_samples(x0, x1, step), edge_samples(y0, y1, step); nx, ny = len(xs), len(ys)
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
    # his skin has gaps and other pieces lie on it: fit a smooth surface, drop what is off it, fit again; the dropped
    # points go onto the fit
    gx, gy = np.meshgrid(np.array(xs), np.array(ys))
    u = (gx - np.mean(xs)) / (np.ptp(xs) / 2 + 1e-9); w = (gy - np.mean(ys)) / (np.ptp(ys) / 2 + 1e-9)
    A = np.stack([(u ** i * w ** j).ravel() for i in range(6) for j in range(4)], 1); zr = zz.ravel().copy()
    ok = np.abs(zr - np.median(zr)) < 0.6
    for thr in (0.3, 0.15, 0.08, 0.04, 0.02, 0.012, 0.012):
        cf = np.linalg.lstsq(A[ok], zr[ok], rcond=None)[0]; fit = A @ cf; ok2 = np.abs(zr - fit) < thr
        if ok2.sum() < A.shape[1] * 3: break
        ok = ok2
    fixed = int((~ok).sum()); FLUSH = (float(np.abs(zr - fit)[ok].max()), float(np.sqrt(np.mean((zr - fit)[ok] ** 2))), float(ok.mean()))
    zz = np.where(ok, zr, fit).reshape(ny, nx)
    for k in range(len(V)):
        r, c = divmod(k, nx)
        if abs(V[k][2] - zz[r, c]) > 1e-6:
            P = Vb(V[k][0], V[k][1], float(zz[r, c])); hit = BVH0.find_nearest(P)
            V[k] = P[:]
            if hit[0] is not None: uv, mi = uv_at(hit[0], hit[2]); UV[k] = uv[:]; out_mi[k] = mi
    print('FLUSH %s: %.0f%% of its points on his skin (max %.1f mm, rms %.1f mm off the fitted surface); %d points (his skin hidden by other pieces or missed) set onto it' % (name, 100 * FLUSH[2], 1000 * FLUSH[0], 1000 * FLUSH[1], fixed))
    cP, cfi = skin(0.0, -6.0, 0.5)              # one clean spot of his paint for the whole outer face: his texture
    cuv, cmi = uv_at(cP, cfi)                    # has the old doors painted in stair-steps, which must not show
    UV = [cuv[:] for _ in UV]; out_mi = [cmi for _ in out_mi]
    def is_edge(k):
        r, c = divmod(k, nx); return r in (0, ny - 1) or c in (0, nx - 1)
    for k in range(len(V)):                     # out from his skin: 3.5 mm, the outermost line 1 mm (the rolled edge)
        V[k] = (V[k][0], V[k][1], V[k][2] - (DOOR_EDGE if is_edge(k) else DOOR_OFF))
    zz = np.array([v[2] for v in V]).reshape(ny, nx)
    bump = max(float(np.abs(np.diff(zz[1:-1, 1:-1], axis=0)).max()), float(np.abs(np.diff(zz[1:-1, 1:-1], axis=1)).max()))
    for k, v in enumerate(list(V)):
        V.append((v[0], v[1], v[2] + (t - 0.004 if is_edge(k) else t))); UV.append((v[0] * 0.5, v[1] * 0.5))
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
    xa, xb = sorted((s * SEAM / 2, s * (0.80 + DOOR_OUT))); DOORS['NoseDoor' + sd] = door('NoseDoor' + sd, xa, xb, 0.45, 3.05 + DOOR_OUT, 2.0)
    xa, xb = sorted((s * (0.75 - DOOR_OUT), s * (2.55 - SEAM / 2))); DOORS['MainBayDoor' + sd] = door('MainBayDoor' + sd, xa, xb, -13.80 - DOOR_OUT, -11.35 + DOOR_OUT, 1.8)
    xa, xb = sorted((s * (2.55 + SEAM / 2), s * (4.45 + DOOR_OUT))); DOORS['MainDoor' + sd] = door('MainDoor' + sd, xa, xb, -13.80 - DOOR_OUT, -11.35 + DOOR_OUT, 1.8)

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
# (380 A2) the slot aft of the nose doors (where the strut comes out) is closed by a panel on the strut: built flush in
# its place, it rides on NoseLegUpper (with the gear down it stands inside the bay behind the strut)
AFT = door('_NoseAft', -0.80 - DOOR_OUT, 0.80 + DOOR_OUT, NOSE_Y0 - DOOR_OUT, 0.45 - SEAM, 2.0)
BV_AFT = bvh_of([AFT])
best = None
for pz in (2.80, 2.85, 2.90, 2.95, 3.00, 3.05):
    for ang in (85.0, 87.5, 90.0, 92.5, 95.0, 97.5, 100.0):
        piv = Vb(0.0, NY, pz); mv = bvh_of(nose_parts, about(None, piv, 'X', ang)); hits = len(BV_STATIC.overlap(mv)) + len(mv.overlap(BV_AFT))
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
AFT.data.transform(about(None, P, 'X', -NOSE['deg']))
print('EXT nose gear down: %d touching triangle pairs' % (len(BV_STATIC.overlap(bvh_of(nose_parts))) + len(BV_STATIC.overlap(bvh_of([AFT])))))
for x in bpy.context.selected_objects: x.select_set(False)
AFT.select_set(True); nose_up.select_set(True); bpy.context.view_layer.objects.active = nose_up; bpy.ops.object.join()

MAIN = {}
# (380 A3) where a retracted main gear piece may lie: above the closed leg/bay door's inner face (inside the hole) or 1.2 cm
# above his lower skin (outside it), and 2 cm under his upper wing skin / under the bay lid in the belly fairing
def map_idx(sd, Q):
    xs, ys, LO, LID = MAPS[sd]
    ii = np.clip(np.round((Q[:, 0] - xs[0]) / ST).astype(int), 0, len(xs) - 1); jj = np.clip(np.round((Q[:, 1] - ys[0]) / ST).astype(int), 0, len(ys) - 1)
    inmap = (Q[:, 0] >= xs.min() - 0.02) & (Q[:, 0] <= xs.max() + 0.02) & (Q[:, 1] >= ys[0] - 0.02) & (Q[:, 1] <= ys[-1] + 0.02)
    return ii, jj, inmap
def map_lo(sd, Q):
    ii, jj, _ = map_idx(sd, Q); return MAPS[sd][2][jj, ii]
def sdf_np(name, X, Y):
    x0, x1, y0, y1, _ = HOLES[name]; cx, cy, hx, hy = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
    qx, qy = np.abs(X - cx) - (hx - RC), np.abs(Y - cy) - (hy - RC)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - RC
def band_np(sd, Q):
    ii, jj, inmap = map_idx(sd, Q); LO, LID = MAPS[sd][2], MAPS[sd][3]
    inside = sdf_np('MainBay' + sd, Q[:, 0], Q[:, 1]) < 0
    return LO[jj, ii] + np.where(inside, 0.03, 0.012), LID[jj, ii] + 0.02, inmap
def fold_np(W, px_, pz_, deg_):
    a = math.radians(deg_); c, sn = math.cos(a), math.sin(a); dx, dz = W[:, 0] - px_, W[:, 2] - pz_
    return np.stack([px_ + dx * c + dz * sn, W[:, 1], pz_ - dx * sn + dz * c], 1)
def stow_clamp(ob, M, sd):       # slims a piece locally: each vertex that would leave the wing when retracted is moved, in the
    mw = np.array(ob.matrix_world); A = np.array(M) @ mw; Ai = np.linalg.inv(A)    # retracted frame, straight up or down into the band
    co = np.array([v.co[:] for v in ob.data.vertices]); Q = co @ A[:3, :3].T + A[:3, 3]
    lo, hi, inmap = band_np(sd, Q); z = np.clip(Q[:, 2], lo, np.maximum(lo, hi)); mv = np.abs(z - Q[:, 2]) * inmap
    Q[:, 2] = np.where(inmap, z, Q[:, 2]); new = Q @ Ai[:3, :3].T + Ai[:3, 3]
    down = co @ mw[:3, :3].T + mw[:3, 3]; seen = down[:, 2] < map_lo(sd, down) - 0.02
    for i in np.nonzero(mv > 1e-5)[0]: ob.data.vertices[int(i)].co = new[i].tolist()
    ob.data.update()
    print('STOW %-15s %4d of %4d verts moved into the wing (max %.3f m); of those seen with the gear down %d (max %.3f m); %d outside the map' % (
        ob.name, int((mv > 1e-5).sum()), len(co), float(mv.max()), int(((mv > 1e-5) & seen).sum()), float((mv * seen).max()), int((~inmap).sum())))
for s, sd in ((-1, '_L'), (1, '_R')):
    mu = MU[sd]; v = vts(mu); hi = v[v[:, 2] > 2.85]; ml = vts(bpy.data.objects['MainLeg' + sd])
    px = float(hi[:, 0].mean()) if len(hi) else s * 4.3; py = float((ml[:, 1].min() + ml[:, 1].max()) / 2)
    if sd == '_R': py = MAIN['_L']['piv'].y
    parts = [mu, bpy.data.objects['MainLeg' + sd], bpy.data.objects['MainWheels' + sd], DOORS['MainDoor' + sd]]
    if sd == '_L':
        # (380 A3) his wing over the leg is only 0.25-0.45 m deep (lower skin z 2.9-3.15, upper 3.2-3.6) and the strut with
        # its fittings is wider than that; the fold is searched so the parts seen with the gear down need the least
        # slimming to lie between the closed leg door and the upper skin (the wheels in the belly fairing, untouched),
        # then stow_clamp slims what is left of the strut and its stays in the retracted frame (see below)
        Wv = [vts(p_) for p_ in parts[:3]]; Wr = np.concatenate(Wv)
        xs_, ys_, LO_, LID_ = MAPS[sd]
        vis = np.concatenate([Wv[0][:, 2] < map_lo(sd, Wv[0]) - 0.02, Wv[1][:, 2] < map_lo(sd, Wv[1]) - 0.02, np.ones(len(Wv[2]), bool)])
        wgt = np.where(vis, 1.0, 0.15); wgt[len(Wv[0]) + len(Wv[1]):] = 6.0
        def score(px_, pz_, ang_):
            Q = fold_np(Wr, px_, pz_, s * ang_); lo, hi, inmap = band_np(sd, Q)
            d = np.maximum(0, lo - Q[:, 2]) + np.maximum(0, Q[:, 2] - hi) + (~inmap) * 1.0
            return float((d * wgt).sum()), float(d[vis].max()), int((d > 0.005).sum())
        cand = []
        for px_ in np.arange(px - 0.2, px + 0.201, 0.05):
            for pz_ in np.arange(2.95, 3.451, 0.025):
                for ang_ in range(70, 101):
                    r = score(px_, pz_, ang_); cand.append((r[0], r, float(px_), float(pz_), float(ang_)))
        cand.sort(key=lambda q: q[0])
        for q in cand[:4]: print('OPT main_L hinge x %.3f z %.3f %3.0f deg: slimming %.1f (weighted m), deepest on a part seen gear-down %.3f m, %d verts to move' % (q[2], q[3], q[4], q[0], q[1][1], q[1][2]))
        q = cand[0]; best = (0, q[3], q[4], q[2]); LBEST = best
    else:                                    # the right gear: the left one mirrored
        b0 = LBEST; piv = Vb(-b0[3], py, b0[1]); M = about(None, piv, 'Y', s * b0[2])
        mv = bvh_of(parts[:3], M); best = (len(BV_STATIC.overlap(mv)) + len(mv.overlap(bvh_of([parts[3]]))), b0[1], b0[2], -b0[3])
        print('CHECK main_R (mirrored) hinge x %.3f z %.3f, %.0f deg: %d touching triangle pairs' % (best[3], best[1], best[2], best[0]))
    px = best[3]
    # with the gear down: the leg, wheels and the leg door (in its gear-down place) against the skin, bays and closed doors
    Md = about(None, Vb(px, py, best[1]), 'Y', -s * best[2])
    print('EXT main%s gear down: %d touching triangle pairs' % (sd, len(BV_STATIC.overlap(bvh_of(parts[:3]))) + len(BV_STATIC.overlap(bvh_of([parts[3]], Md)))))
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
        MAIN[sd].update(B=B, A=A, J=J, up_deg=up_deg, lo_deg=lo_world - MAIN[sd]['deg'], reach=dd <= L1 + L2)
    # (380 A3) slim what would still leave the wing at full retraction (the strut top, fittings and stays; wheels if needed)
    bpy.context.view_layer.update()
    Mr = about(None, P, 'Y', MAIN[sd]['deg'])
    for nm in ('MainLegUpper', 'MainLeg', 'MainWheels'): stow_clamp(bpy.data.objects[nm + sd], Mr, sd)
    if 'B' in MAIN[sd]:
        m_ = MAIN[sd]
        stow_clamp(bpy.data.objects['SideBraceUp' + sd], about(None, m_['B'], 'Y', m_['up_deg']), sd)
        stow_clamp(bpy.data.objects['SideBraceLo' + sd], Mr @ about(None, m_['A'], 'Y', m_['lo_deg']), sd)
    MAIN[sd]['hits'] = len(BV_STATIC.overlap(bvh_of(parts[:3], Mr)))
    print('CHECK main%s after slimming: %d touching triangle pairs with the skin, bays and closed doors (retracted)' % (sd, MAIN[sd]['hits']))
    breakdown('main' + sd + ' slimmed', parts[:3], Mr)

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
        print('KNEE %s glb %s (gear down; SideBraceUp%s turns about its wing end %s, SideBraceLo%s about its leg end %s)' % (sd, glb(m['J']), sd, glb(m['B']), sd, glb(m['A'])))
        hinge_line('SideBraceLo' + sd, bpy.data.objects['SideBraceLo' + sd], m['A'], m['axis'], m['lo_deg'], 'child of MainLegUpper, about its leg end, relative%s' % ('' if m['reach'] else ' (links cannot reach: approx)'))
    for nm, edge in (('NoseDoor', 0.80 + DOOR_OUT), ('MainBayDoor', 0.75 - DOOR_OUT)):
        x0, x1, y0, y1, zs = HOLES['NoseBay' if nm == 'NoseDoor' else 'MainBay' + sd]
        hx = s * edge
        P0, _ = skin(hx, (y0 + y1) / 2, zs); hp = Vb(hx, (y0 + y1) / 2, (P0.z if P0 else 2.2) + 0.015)
        deg = (-s * 90.0) if nm == 'NoseDoor' else (s * 90.0)
        hinge_line(nm + sd, DOORS[nm + sd], hp, Vb(0, 1, 0), deg, 'open angle (hinge at its %s edge, opens downwards; closed with the gear up and down)' % ('outer' if nm == 'NoseDoor' else 'keel'))
for nm, ob in BAYS.items(): print('BAY %s fixed, dark' % nm)
src.free()
