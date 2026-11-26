# Clean-up and modelled details for the cargo forklift (called by make_cargo_forklift.py, game metres: x forward, y left,
# z up). The Meshy shell keeps its body; what came out melted is cut away and modelled again as clean, bevelled parts:
#   - spikes (stray far vertices) and floating crumbs removed; flat panels snapped flat (lumps up to 8 mm)
#   - overhead guard roof: box-tube rim, five flat bars, beacon, two rear work lamps
#   - head lamps (housing, chrome reflector, ribbed lens) on the front posts, rear-view mirrors on arms
#   - seat (pan, cushion, backrest, hip bolsters, lap belt with retractor and buckle), steering wheel with spinner knob,
#     dash pod with two gauges and warning lamps, three valve levers on a valve block, three pedals, floor mat,
#     step with anti-slip studs, grab handle on the left front post, lift chains in the mast
# Everything checked against the body for overlaps (printed); IK points come back from the real new parts.
import bpy, bmesh, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree


def V(o):
    a = np.empty(len(o.data.vertices) * 3, np.float64); o.data.vertices.foreach_get('co', a); return a.reshape(-1, 3)


def W(o):
    M = np.array(o.matrix_world); return V(o) @ M[:3, :3].T + M[:3, 3]


def tree_world(o):
    bm = bmesh.new(); bm.from_mesh(o.data); bmesh.ops.transform(bm, matrix=o.matrix_world, verts=bm.verts)
    t = BVHTree.FromBMesh(bm); bm.free(); return t


def delete_faces(o, inside, any_=False):
    """drop the faces of o whose every corner (any_: any corner) passes inside(xyz array) -> bool array"""
    v = V(o); keep_in = inside(v)
    bm = bmesh.new(); bm.from_mesh(o.data)
    test = any if any_ else all
    gone = [f for f in bm.faces if test(keep_in[q.index] for q in f.verts)]
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    loose = [q for q in bm.verts if not q.link_faces]; bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bm.to_mesh(o.data); bm.free(); o.data.update(); return len(gone)


def drop_crumbs(o, max_faces=100):
    """loose islands of at most max_faces faces (Meshy crumbs, doubled scraps) go"""
    bm = bmesh.new(); bm.from_mesh(o.data); bm.faces.ensure_lookup_table()
    seen = np.zeros(len(bm.faces), bool); gone = []
    for f in bm.faces:
        if seen[f.index]: continue
        st = [f]; seen[f.index] = True; isl = []
        while st:
            g = st.pop(); isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if not seen[h.index]: seen[h.index] = True; st.append(h)
        if len(isl) <= max_faces: gone += isl
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    bmesh.ops.delete(bm, geom=[q for q in bm.verts if not q.link_faces], context='VERTS')
    bm.to_mesh(o.data); bm.free(); o.data.update(); return len(gone)


def planar_snap(o, cell=0.25, tol=0.008, passes=2):
    """flat panels: in each 25 cm cell, vertices facing +-x, +-y or +z within tol of the cell's main plane go onto it"""
    moved = 0
    for _ in range(passes):
        me = o.data; me.update(); n = len(me.vertices); co = V(o)
        nr = np.empty(n * 3); me.vertices.foreach_get('normal', nr); nr = nr.reshape(-1, 3)
        for k in range(3):
            for sg in (1, -1):
                if k == 2 and sg < 0: continue
                sel = np.nonzero(nr[:, k] * sg > 0.94)[0]
                if len(sel) < 20: continue
                o1, o2 = [a for a in range(3) if a != k]
                key = np.floor(co[sel, o1] / cell).astype(np.int64) * 100000 + np.floor(co[sel, o2] / cell).astype(np.int64)
                order = np.argsort(key, kind='stable'); b = np.nonzero(np.diff(key[order]))[0] + 1
                for grp in np.split(order, b):
                    if len(grp) < 15: continue
                    idx = sel[grp]; c = co[idx, k]
                    h, e = np.histogram(c, bins=max(1, int((c.max() - c.min()) / 0.003) + 1))
                    pk = (e[h.argmax()] + e[h.argmax() + 1]) / 2
                    m = np.abs(c - pk) < tol
                    if m.sum() < 0.5 * len(c): continue
                    co[idx[m], k] = np.median(c[m]); moved += int(m.sum())
        me.vertices.foreach_set('co', co.ravel()); me.update()
    return moved


# ---------------------------------------------------------------- materials (no textures: clean paint, rubber, chrome, glass)
def lin(c): return tuple(((x / 255.0 + 0.055) / 1.055) ** 2.4 if x > 10 else x / 255.0 / 12.92 for x in c)


def material(name, col, rough, metal=0.0, emit=None):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name); m.use_nodes = True
    p = m.node_tree.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*col, 1.0); p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    if emit:
        p.inputs['Emission'].default_value = (*emit, 1.0); p.inputs['Emission Strength'].default_value = 0.6
    return m


def paint_yellow(tex_path):
    """the body's own yellow from the cleaned colour picture (so the new guard matches the shell)"""
    im = bpy.data.images.load(tex_path, check_existing=True)
    px = np.array(im.pixels[:]).reshape(-1, 4)[::7, :3]
    y = px[(px[:, 0] > 0.45) & (px[:, 1] > 0.30) & (px[:, 2] < 0.5 * px[:, 0])]
    c = np.median(y, 0) if len(y) > 100 else np.array([0.85, 0.62, 0.08])
    return lin(tuple(c * 255))


# ---------------------------------------------------------------- a part built from bevelled primitives
class Part:
    def __init__(s, name, mats):
        s.name, s.mats, s.bm = name, mats, bmesh.new()

    def _merge(s, t, mi, M=None):
        if M is not None: bmesh.ops.transform(t, matrix=M, verts=t.verts)
        for f in t.faces: f.material_index = mi; f.smooth = True
        me = bpy.data.meshes.new('_t'); t.to_mesh(me); t.free(); s.bm.from_mesh(me); bpy.data.meshes.remove(me)

    def box(s, lo, hi, mi, bev=0.004, M=None):
        t = bmesh.new(); bmesh.ops.create_cube(t, size=1.0)
        lo, hi = np.array(lo, float), np.array(hi, float); c, d = (lo + hi) / 2, hi - lo
        for v in t.verts: v.co = Vector(c + np.array(v.co) * d)
        if bev > 0:
            bmesh.ops.bevel(t, geom=list(t.edges), offset=min(bev, d.min() * 0.4), offset_type='OFFSET', segments=2,
                            profile=0.5, affect='EDGES', clamp_overlap=True)
        s._merge(t, mi, M)

    def cyl(s, p0, p1, r, mi, seg=24, bev=0.0015, r2=None, M=None):
        p0, p1 = Vector(p0), Vector(p1); d = p1 - p0
        t = bmesh.new()
        bmesh.ops.create_cone(t, cap_ends=True, cap_tris=False, segments=seg, radius1=r, radius2=r if r2 is None else r2, depth=d.length)
        if bev > 0:
            rim = [e for e in t.edges if len(e.link_faces) == 2 and abs(e.link_faces[0].normal.dot(e.link_faces[1].normal)) < 0.5]
            bmesh.ops.bevel(t, geom=rim, offset=min(bev, r * 0.3), offset_type='OFFSET', segments=2, profile=0.5,
                            affect='EDGES', clamp_overlap=True)
        R = d.normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
        bmesh.ops.transform(t, matrix=Matrix.Translation((p0 + p1) / 2) @ R, verts=t.verts); s._merge(t, mi, M)

    def ball(s, c, r, mi, seg=20, M=None, sz=1.0):
        t = bmesh.new(); bmesh.ops.create_uvsphere(t, u_segments=seg, v_segments=seg // 2, radius=r)
        for v in t.verts: v.co.z *= sz
        bmesh.ops.translate(t, vec=Vector(c), verts=t.verts); s._merge(t, mi, M)

    def torus(s, c, R, r, mi, n1=40, n2=12, M=None):
        t = bmesh.new(); ring = []
        for i in range(n1):
            a = 2 * math.pi * i / n1; row = []
            for j in range(n2):
                b = 2 * math.pi * j / n2; q = R + r * math.cos(b)
                row.append(t.verts.new((c[0] + q * math.cos(a), c[1] + q * math.sin(a), c[2] + r * math.sin(b))))
            ring.append(row)
        for i in range(n1):
            for j in range(n2):
                t.faces.new((ring[i][j], ring[(i + 1) % n1][j], ring[(i + 1) % n1][(j + 1) % n2], ring[i][(j + 1) % n2]))
        bmesh.ops.recalc_face_normals(t, faces=t.faces); s._merge(t, mi, M)

    def done(s, sc, pivot=None, rot=None):
        bmesh.ops.recalc_face_normals(s.bm, faces=s.bm.faces)
        me = bpy.data.meshes.new(s.name); s.bm.to_mesh(me); s.bm.free()
        for m in s.mats: me.materials.append(m)
        me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(35)
        o = bpy.data.objects.new(s.name, me); sc.collection.objects.link(o)
        if pivot is not None:
            Wm = Matrix.Translation(Vector(pivot)) @ (rot.to_4x4() if rot else Matrix.Identity(4))
            me.transform(Wm.inverted()); o.matrix_world = Wm
        return o


def lamp(P, centre, axis, w, h, depth, mh, mr, ml, round_=False):
    """housing (black), chrome reflector bowl, ribbed lens on the face; axis = the way it shines (unit, +-x)"""
    cx, cy, cz = centre; s = axis
    P.box((cx - (depth if s > 0 else 0), cy - w / 2, cz - h / 2), (cx + (0 if s > 0 else depth), cy + w / 2, cz + h / 2), mh, 0.006)
    fx = cx - 0.001 * s                                               # the face: bezel ring, reflector, lens ribs
    P.box((min(fx, fx + 0.004 * s), cy - w / 2 + 0.006, cz - h / 2 + 0.006), (max(fx, fx + 0.004 * s), cy + w / 2 - 0.006, cz + h / 2 - 0.006), mr, 0.002)
    P.cyl((fx + 0.003 * s, cy, cz), (fx + 0.007 * s, cy, cz), min(w, h) * 0.36, ml, 28, 0.001)
    for k in range(-2, 3):                                            # vertical prism ribs on the lens
        yy = cy + k * min(w, h) * 0.12
        P.box((min(fx + 0.006 * s, fx + 0.009 * s), yy - 0.002, cz - min(w, h) * 0.26), (max(fx + 0.006 * s, fx + 0.009 * s), yy + 0.002, cz + min(w, h) * 0.26), ml, 0.0008)


# ================================================================== the work
def setup(tex_base):
    out, info, ik = {}, [], {}
    yel = paint_yellow(tex_base)
    M = dict(paint=material('M_Forklift_Paint', yel, 0.38), black=material('M_Forklift_BlackSteel', (0.018, 0.018, 0.02), 0.45, 0.4),
             rubber=material('M_Forklift_Rubber', (0.012, 0.012, 0.013), 0.88), vinyl=material('M_Forklift_Vinyl', (0.02, 0.02, 0.022), 0.55),
             chrome=material('M_Forklift_Chrome', (0.9, 0.9, 0.9), 0.1, 1.0), lens=material('M_Forklift_LensClear', (0.85, 0.87, 0.9), 0.04),
             amber=material('M_Forklift_LensAmber', (1.0, 0.33, 0.0), 0.08, 0.0, (1.0, 0.3, 0.0)),
             red=material('M_Forklift_LensRed', (0.7, 0.02, 0.01), 0.1), mirror=material('M_Forklift_MirrorGlass', (0.92, 0.93, 0.95), 0.02, 1.0),
             belt=material('M_Forklift_Belt', lin((235, 90, 20)), 0.7), white=material('M_Forklift_Marking', (0.85, 0.85, 0.82), 0.4),
             green=material('M_Forklift_LensGreen', (0.02, 0.5, 0.05), 0.1))
    return out, info, ik, M


def clean_shell(body, PARTS):
    """before the pivots: spikes, crumbs, the melted roof / lamps / mirrors cut out, flat panels snapped"""
    n = delete_faces(body, lambda v: ~((v[:, 0] > -1.6) & (v[:, 0] < 1.25) & (np.abs(v[:, 1]) < 0.76) & (v[:, 2] > 0.1) & (v[:, 2] < 2.6)), True)
    print('DETAIL spikes: body faces outside the machine dropped', n)
    for side in ('L', 'R'):
        k = PARTS['SteerKnuckle_' + side]; w = PARTS['Wheel_R' + side]
        c = W(w).mean(0)
        n = delete_faces(k, lambda v, c=c: np.linalg.norm(v - c, axis=1) > 0.4, True)
        print('DETAIL spikes: knuckle', side, 'faces dropped', n)
    print('DETAIL crumbs dropped (faces)', drop_crumbs(body, 100))
    # the melted roof (with its beacon), head lamps and mirrors
    print('DETAIL roof faces cut', delete_faces(body, lambda v: (v[:, 2] > 2.03) & (v[:, 0] > -0.50) & (v[:, 0] < 0.65) & (np.abs(v[:, 1]) < 0.52)))
    print('DETAIL lamp faces cut', delete_faces(body, lambda v: (v[:, 2] > 1.70) & (v[:, 2] < 1.96) & (np.abs(v[:, 1]) > 0.36) & (np.abs(v[:, 1]) < 0.53)
                                                & (v[:, 0] > 0.595 + (1.96 - v[:, 2]) * 0.40) & (v[:, 0] < 0.85)))
    print('DETAIL mirror faces cut', delete_faces(body, lambda v: (v[:, 2] > 1.65) & (v[:, 2] < 1.97) & (np.abs(v[:, 1]) > 0.495) & (v[:, 0] > 0.40) & (v[:, 0] < 0.85)))
    print('DETAIL flat-panel vertices snapped', planar_snap(body))


def refine(sc, body, PARTS, levers, tex_base):
    out, info, ik, M = setup(tex_base)
    bt = BVHTree.FromObject(body, bpy.context.evaluated_depsgraph_get())

    def down(x, y, z0=2.6, lo=0.0, hi=9.0):
        h = bt.ray_cast(Vector((x, y, z0)), Vector((0, 0, -1)), 3.0)
        return h[0].z if h[0] is not None and lo <= h[0].z <= hi else None

    def across(x, y, z, d, dist=1.0):
        h = bt.ray_cast(Vector((x, y, z)), Vector(d), dist); return h[0]

    # ------------------------------------------------ overhead guard roof (rim over the post tops, five bars)
    G = Part('OverheadGuard', [M['paint'], M['black']])
    X0, X1, YI, YO, Z0, Z1 = -0.46, 0.585, 0.405, 0.478, 2.022, 2.092
    for s in (1, -1):
        G.box((X0, min(s * YI, s * YO), Z0), (X1, max(s * YI, s * YO), Z1), 0, 0.007)
    G.box((X1 - 0.07, -YI - 0.002, Z0 + 0.004), (X1, YI + 0.002, Z1 - 0.004), 0, 0.007)
    G.box((X0, -YI - 0.002, Z0 + 0.004), (X0 + 0.07, YI + 0.002, Z1 - 0.004), 0, 0.007)
    for yy in (-0.27, -0.135, 0.0, 0.135, 0.27):
        G.box((X0 + 0.068, yy - 0.007, Z0 + 0.012), (X1 - 0.068, yy + 0.007, Z1 - 0.006), 0, 0.003)
    G.box((X0 + 0.01, -0.07, Z1 - 0.002), (X0 + 0.07, 0.07, Z1 + 0.006), 1, 0.002)       # the beacon's foot plate
    out['OverheadGuard'] = G.done(sc)
    bx = X0 + 0.04
    B = Part('BeaconBase', [M['black'], M['chrome']])
    B.cyl((bx, 0, Z1 + 0.004), (bx, 0, Z1 + 0.032), 0.058, 0, 32, 0.004)
    B.cyl((bx, 0, Z1 + 0.034), (bx, 0, Z1 + 0.10), 0.012, 1, 16, 0.001)                  # the turning mirror's post
    B.box((bx - 0.003, -0.03, Z1 + 0.045), (bx + 0.003, 0.03, Z1 + 0.09), 1, 0.001)
    out['BeaconBase'] = B.done(sc)
    L = Part('Light_Beacon', [M['amber']])
    L.cyl((bx, 0, Z1 + 0.030), (bx, 0, Z1 + 0.105), 0.050, 0, 32, 0.002)
    L.ball((bx, 0, Z1 + 0.105), 0.050, 0, 32, sz=0.45)
    out['Light_Beacon'] = L.done(sc, (bx, 0, Z1 + 0.03))
    for s, nm in ((1, 'L'), (-1, 'R')):                                                  # rear work lamps on the rim
        H = Part('WorkLampHousing_' + nm, [M['black'], M['chrome'], M['lens']])
        H.box((X0 + 0.005, s * 0.36 - 0.022, Z1 - 0.002), (X0 + 0.055, s * 0.36 + 0.022, Z1 + 0.012), 0, 0.002)   # bracket
        lamp(H, (X0 - 0.002, s * 0.36, Z1 + 0.05), -1, 0.10, 0.08, 0.055, 0, 1, 2)
        out['Light_Work_R' + nm] = H.done(sc, (X0, s * 0.36, Z1 + 0.05))
    # ------------------------------------------------ head lamps on the front posts, mirrors on arms
    for s, nm in ((1, 'L'), (-1, 'R')):
        h = across(1.2, s * 0.445, 1.86, (-1, 0, 0))
        px = h.x if h is not None and 0.45 < h.x < 0.75 else 0.585
        H = Part('Light_Head_' + nm, [M['black'], M['chrome'], M['lens']])
        H.box((px - 0.006, s * 0.445 - 0.03, 1.85), (px + 0.025, s * 0.445 + 0.03, 1.87), 0, 0.002)   # bracket, into the post
        lamp(H, (px + 0.085, s * 0.445, 1.86), 1, 0.10, 0.09, 0.065, 0, 1, 2)
        out['Light_Head_' + nm] = H.done(sc, (px + 0.085, s * 0.445, 1.86))
        h = across(0.565, s * 1.0, 1.84, (0, -s, 0))
        py = abs(h.y) if h is not None and 0.40 < abs(h.y) < 0.56 else 0.475
        Mi = Part('Mirror_' + nm, [M['black'], M['mirror']])
        Mi.cyl((0.565, s * (py - 0.006), 1.84), (0.565, s * (py + 0.10), 1.84), 0.009, 0, 16)
        Mi.cyl((0.565, s * (py - 0.004), 1.84), (0.565, s * (py + 0.012), 1.84), 0.022, 0, 20)   # clamp on the post
        Mi.ball((0.565, s * (py + 0.10), 1.84), 0.013, 0, 16)
        cy = s * (py + 0.165)
        Mi.box((0.57, cy - 0.065, 1.75), (0.605, cy + 0.065, 1.94), 0, 0.012)
        Mi.box((0.566, cy - 0.057, 1.758), (0.571, cy + 0.057, 1.932), 1, 0.004)              # the glass, facing back
        out['Mirror_' + nm] = Mi.done(sc, (0.565, s * py, 1.84))
        print('DETAIL post at z 1.86 front x %.3f, outer |y| %.3f' % (px, py))
    # ------------------------------------------------ seat
    seat = PARTS['Seat']; sp = Vector(seat.matrix_world.translation)
    S = Part('Seat', [M['vinyl'], M['black'], M['belt'], M['chrome']])
    zb = sp.z
    S.box((-0.30, -0.235, zb - 0.004), (0.085, 0.235, zb + 0.045), 1, 0.006)                      # steel pan on its rails
    S.box((-0.255, -0.232, zb + 0.040), (0.105, 0.232, zb + 0.160), 0, 0.03)                       # cushion
    S.box((0.035, -0.225, zb + 0.15), (0.11, 0.225, zb + 0.175), 0, 0.02)                          # front roll
    for s in (1, -1):
        S.box((-0.25, s * 0.205 - 0.03, zb + 0.15), (0.0, s * 0.205 + 0.03, zb + 0.20), 0, 0.022)   # hip bolsters
    tilt = Matrix.Translation((-0.31, 0, zb + 0.13)) @ Matrix.Rotation(math.radians(-5), 4, 'Y') @ Matrix.Translation((0.31, 0, -(zb + 0.13)))
    S.box((-0.33, -0.235, zb + 0.13), (-0.245, 0.235, zb + 0.50), 0, 0.03, tilt)                  # backrest
    S.box((-0.345, -0.225, zb + 0.14), (-0.327, 0.225, zb + 0.48), 1, 0.005, tilt)                 # its steel back shell
    S.box((-0.26, -0.20, zb + 0.30), (-0.235, 0.20, zb + 0.36), 0, 0.012, tilt)                    # lumbar roll
    # lap belt: retractor on the right, strap over the bolster, tongue resting on the cushion; buckle on the left
    S.box((-0.215, -0.262, zb + 0.06), (-0.165, -0.233, zb + 0.13), 1, 0.006)
    S.box((-0.205, -0.240, zb + 0.13), (-0.175, -0.236, zb + 0.20), 2, 0.001)
    S.box((-0.205, -0.236, zb + 0.199), (-0.175, -0.172, zb + 0.203), 2, 0.001)
    S.box((-0.205, -0.176, zb + 0.159), (-0.175, -0.172, zb + 0.203), 2, 0.001)
    S.box((-0.203, -0.176, zb + 0.159), (-0.177, -0.06, zb + 0.163), 2, 0.001)
    S.box((-0.20, -0.062, zb + 0.161), (-0.18, -0.02, zb + 0.166), 3, 0.001)
    S.box((-0.215, 0.233, zb + 0.06), (-0.17, 0.262, zb + 0.10), 1, 0.004)
    S.box((-0.205, 0.236, zb + 0.10), (-0.18, 0.240, zb + 0.20), 2, 0.001)
    S.box((-0.21, 0.205, zb + 0.198), (-0.175, 0.245, zb + 0.235), 1, 0.006)                         # the buckle
    S.box((-0.205, 0.215, zb + 0.235), (-0.18, 0.235, zb + 0.238), 3, 0.001)
    new = S.done(sc); me = new.data; me.transform(seat.matrix_world.inverted()); old = seat.data
    seat.data = me; bpy.data.objects.remove(new, do_unlink=True); bpy.data.meshes.remove(old); me.name = 'Seat'
    ik['Seat_Driver'] = (-0.08, 0.0, zb + 0.160); ik['Back_Driver'] = (-0.25, 0.0, zb + 0.45)
    # ------------------------------------------------ steering wheel (local: z the column axis, ring in xy)
    sw = PARTS['SteeringWheel']; R0, r0 = 0.150, 0.0155
    T = Part('SteeringWheel', [M['vinyl'], M['black'], M['chrome']])
    T.torus((0, 0, 0), R0, r0, 0, 64, 14)
    T.cyl((0, 0, -0.05), (0, 0, 0.028), 0.042, 1, 32, 0.004)
    T.cyl((0, 0, 0.026), (0, 0, 0.034), 0.03, 2, 32, 0.002)
    for a in (90, 210, 330):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        T.cyl((0.035 * ca, 0.035 * sa, 0.0), (R0 * ca, R0 * sa, 0.0), 0.009, 1, 12, 0.001)
    ka = math.radians(115); kp = (R0 * math.cos(ka), R0 * math.sin(ka), 0.0)
    T.cyl(kp, (kp[0], kp[1], 0.045), 0.006, 2, 12, 0.001)
    T.cyl((kp[0], kp[1], 0.03), (kp[0], kp[1], 0.072), 0.016, 0, 24, 0.006)                    # the spinner knob
    me = T.done(sc); old = sw.data; sw.data = me.data; bpy.data.objects.remove(me, do_unlink=True); bpy.data.meshes.remove(old)
    sw.data.name = 'SteeringWheel'
    swm = sw.matrix_world; ax = (swm.to_3x3() @ Vector((0, 0, 1))).normalized(); yl = (swm.to_3x3() @ Vector((0, 1, 0))).normalized()
    hub = swm.translation
    ik['Hand_L'] = tuple(hub + yl * R0); ik['Hand_R'] = tuple(hub - yl * R0); ik['Hand_Knob'] = tuple(swm @ Vector((kp[0], kp[1], 0.055)))
    # dash pod under the wheel, on the column (gauges face the driver along the column axis)
    D = Part('Dash', [M['black'], M['chrome'], M['white'], M['red'], M['amber'], M['green'], M['lens']])
    D.box((-0.06, -0.11, -0.115), (0.06, 0.11, -0.068), 0, 0.01)
    for yy in (-0.06, 0.06):
        D.torus((0.0, yy, -0.068), 0.031, 0.0035, 1, 40, 8)
        D.cyl((0.0, yy, -0.070), (0.0, yy, -0.067), 0.029, 0, 32, 0.0)
        for i in range(9):
            a = math.radians(-135 + i * 33.75); ca, sa = math.cos(a), math.sin(a)
            D.box((0.022 * ca - 0.0015, yy + 0.022 * sa - 0.0015, -0.0675), (0.022 * ca + 0.0015, yy + 0.022 * sa + 0.0015, -0.0662), 2, 0.0)
        D.box((-0.002, yy - 0.0012, -0.0665), (0.019, yy + 0.0012, -0.0655), 3, 0.0)
        D.cyl((0.0, yy, -0.0675), (0.0, yy, -0.064), 0.004, 2, 12, 0.0)
    for i, mi in enumerate((3, 4, 5, 4)):
        D.cyl((-0.04, -0.03 + i * 0.02, -0.069), (-0.04, -0.03 + i * 0.02, -0.0655), 0.0055, mi, 16, 0.0005)
    out['Dash'] = D.done(sc); out['Dash'].data.transform(swm); out['Dash'].data.update()
    # ------------------------------------------------ valve levers (three parts, each with its pivot)
    lv0 = [o for o in levers]
    lo = np.min([W(o).min(0) for o in lv0], 0); hi = np.max([W(o).max(0) for o in lv0], 0)
    for o in lv0: bpy.data.objects.remove(o, do_unlink=True)
    # crumbs of the melted lever bank that went back into the body
    print('DETAIL old lever crumbs cut', delete_faces(body, lambda v: (v[:, 0] > lo[0] - 0.01) & (v[:, 0] < hi[0] + 0.01) & (v[:, 1] > lo[1] - 0.01)
                                                       & (v[:, 1] < hi[1] + 0.01) & (v[:, 2] > lo[2] + 0.012) & (v[:, 2] < hi[2] + 0.02)))
    zb = lo[2]; xc = (lo[0] + hi[0]) / 2
    VB = Part('ValveBlock', [M['black'], M['rubber']])
    VB.box((xc - 0.036, 0.058, zb - 0.03), (xc + 0.036, 0.222, zb + 0.016), 0, 0.005)
    out['ValveBlock'] = VB.done(sc)
    newlev = []
    for i, yy in enumerate((0.085, 0.140, 0.195)):
        P = Part('Lever_%d' % (i + 1), [M['chrome'], M['rubber'], M['vinyl'], M['black']])
        p0 = Vector((xc, yy, zb + 0.016)); top = Vector((xc - 0.018, yy, zb + 0.165))
        P.cyl(p0 - Vector((0, 0, 0.003)), (xc, yy, zb + 0.045), 0.016, 1, 16, 0.002, r2=0.008)                 # rubber boot
        P.cyl(p0 + Vector((0, 0, 0.005)), top, 0.0065, 0, 16, 0.001)
        P.ball(top + Vector((0, 0, 0.012)), 0.017, 2, 20, sz=1.15)
        o = P.done(sc, p0); newlev.append(o); out[o.name] = o
        ik['Hand_Lever_%d' % (i + 1)] = tuple(top + Vector((0, 0, 0.012)))
    # ------------------------------------------------ floor, pedals, mat
    hits = [down(x, y, 1.15, 0.75, 1.05) for x in np.arange(0.06, 0.50, 0.04) for y in np.arange(-0.28, 0.30, 0.04)]
    hits = np.array([h for h in hits if h is not None]); floor = float(np.median(hits)) if len(hits) > 10 else 0.886
    print('DETAIL floor z %.3f from %d rays (spread %.3f)' % (floor, len(hits), hits.std() if len(hits) else -1))
    Fm = Part('FloorMat', [M['rubber']])
    Fm.box((0.06, -0.27, floor - 0.002), (0.46, 0.30, floor + 0.006), 0, 0.003)
    for k in range(13):
        xx = 0.08 + k * 0.03
        Fm.box((xx - 0.005, -0.25, floor + 0.005), (xx + 0.005, 0.28, floor + 0.009), 0, 0.0015)
    out['FloorMat'] = Fm.done(sc)
    for nm, yy, wd in (('Inching', 0.215, 0.075), ('Brake', 0.115, 0.085), ('Accel', -0.13, 0.07)):
        hx = across(0.30, yy, floor + 0.10, (1, 0, 0), 0.6)
        piv = Vector(((hx.x - 0.01) if hx is not None and 0.5 < hx.x < 0.8 else 0.62, yy, floor + 0.03))
        P = Part('Pedal_' + nm, [M['black'], M['rubber']])
        pad = Matrix.Translation((piv.x - 0.09, yy, floor + 0.11)) @ Matrix.Rotation(math.radians(-35), 4, 'Y')
        P.cyl(piv, (piv.x - 0.085, yy, floor + 0.105), 0.009, 0, 12, 0.001)
        P.box((-0.06, -wd / 2, -0.006), (0.06, wd / 2, 0.006), 0, 0.003, pad)
        for k in range(5):
            P.box((-0.05 + k * 0.025 - 0.004, -wd / 2 + 0.006, 0.005), (-0.05 + k * 0.025 + 0.004, wd / 2 - 0.006, 0.010), 1, 0.0015, pad)
        o = P.done(sc, piv); out[o.name] = o
        ik['Foot_' + nm] = tuple(pad @ Vector((0, 0, 0.012)))
    # ------------------------------------------------ step with anti-slip studs (driver's left, +y)
    sh = [(x, y, down(x, y, 0.85, 0.55, 0.75)) for x in np.arange(0.02, 0.28, 0.02) for y in np.arange(0.50, 0.66, 0.02)]
    sh = np.array([q for q in sh if q[2] is not None])
    if len(sh) > 6:
        zt = float(np.median(sh[:, 2])); good = sh[np.abs(sh[:, 2] - zt) < 0.02]
        x0, x1, y0, y1 = good[:, 0].min() - 0.01, good[:, 0].max() + 0.01, good[:, 1].min() - 0.01, good[:, 1].max() + 0.01
    else:
        zt, x0, x1, y0, y1 = 0.645, 0.01, 0.28, 0.52, 0.64
    St = Part('Step_L', [M['black'], M['paint']])
    St.box((x0, y0, zt - 0.004), (x1, y1, zt + 0.008), 0, 0.003)
    for xx in np.arange(x0 + 0.015, x1 - 0.01, 0.022):
        for yy in np.arange(y0 + 0.015, y1 - 0.01, 0.022):
            St.box((xx - 0.004, yy - 0.004, zt + 0.007), (xx + 0.004, yy + 0.004, zt + 0.0105), 0, 0.0015)
    out['Step_L'] = St.done(sc)
    ik['Step'] = ((x0 + x1) / 2, (y0 + y1) / 2, zt + 0.0105)
    print('DETAIL step top %.3f x %.3f..%.3f y %.3f..%.3f' % (zt, x0, x1, y0, y1))
    # ------------------------------------------------ grab handle on the flat outer side of the left front post
    pts = []
    for z in (1.25, 1.60):
        xp = 0.79 - (z - 0.99) * 0.26
        h = across(xp, 1.0, z, (0, -1, 0))
        if h is not None and 0.40 < h.y < 0.56: pts.append(Vector((xp, h.y, z)))
    if len(pts) == 2:
        GH = Part('GrabHandle_L', [M['black'], M['rubber']])
        a, b = [p + Vector((0, 0.055, 0)) for p in pts]
        for p, q in zip(pts, (a, b)):
            GH.cyl(p - Vector((0, 0.006, 0)), q, 0.011, 0, 16, 0.001)
            GH.box((p.x - 0.025, p.y - 0.004, p.z - 0.03), (p.x + 0.025, p.y + 0.004, p.z + 0.03), 0, 0.002)
        GH.cyl(a, b, 0.0145, 1, 20, 0.002)
        for q in (a, b): GH.ball(q, 0.0145, 1, 16)
        out['GrabHandle_L'] = GH.done(sc); ik['Grab_Outside'] = tuple((a + b) / 2)
    else:
        print('DETAIL grab handle: the post was not found where expected, none made')
    # ------------------------------------------------ lift chains between the inner mast and the carriage
    # above the carriage (rests at the bottom) the leaf chains and the lift hoses hang in front of the inner mast, each
    # straight, 4 mm clear of the mast's most forward point on its line, from the carriage top to the mast head
    mast = PARTS['MastInner']; mi_t, ca_t = tree_world(mast), tree_world(PARTS['Carriage'])
    ch = Part('MastChains', [M['black'], M['rubber'], M['chrome']]); made = []
    ztop = W(mast)[:, 2].max() - 0.06
    # the rails' inner faces (rays out from the middle at mid height); chains 3 cm inside them, hoses 7 cm inside
    rails = {}
    for sg in (1, -1):
        hs = [mi_t.ray_cast(Vector((xm, 0, z)), Vector((0, sg, 0)), 0.6)[0] for xm in (1.43, 1.45, 1.47) for z in (1.4, 1.7, 2.0)]
        hs = [abs(h.y) for h in hs if h is not None]
        rails[sg] = min(hs) if hs else None
    print('DETAIL inner mast rails inner faces |y|', rails)
    jobs = []
    for sg in (1, -1):
        if rails[sg]: jobs += [(sg * (rails[sg] - 0.03), 'chain'), (sg * (rails[sg] - 0.07), 'hose')]
    for yy, kind in jobs:
        fx = [mi_t.ray_cast(Vector((2.6, yy, z)), Vector((-1, 0, 0)), 1.5)[0] for z in np.arange(1.1, ztop, 0.04)]
        fx = [h.x for h in fx if h is not None and 1.35 < h.x < 1.60] or [1.432]
        hw = 0.0075 if kind == 'chain' else 0.011
        x = max(fx) + 0.004 + hw
        cb = ca_t.ray_cast(Vector((x, yy, ztop)), Vector((0, 0, -1)), 2.0)[0]
        z0 = (cb.z if cb is not None and cb.z < 1.3 else W(PARTS['Carriage'])[:, 2].max()) - 0.004   # on the carriage's top bar
        if kind == 'chain':
            n = int((ztop - z0) / 0.025)
            for i in range(n):
                za = z0 + i * 0.025; off = 0.0085 if i % 2 else 0.0055
                for sgn in (1, -1):
                    ch.box((x - 0.0075, yy + sgn * off - 0.0015, za - 0.003), (x + 0.0075, yy + sgn * off + 0.0015, za + 0.028), 0, 0.001)
                ch.cyl((x, yy - 0.0105, za), (x, yy + 0.0105, za), 0.0028, 2, 8, 0.0)
            ch.box((x - 0.012, yy - 0.016, ztop - 0.002), (x + 0.012, yy + 0.016, ztop + 0.03), 0, 0.003)   # anchor at the head
        else:
            ch.cyl((x, yy, z0 + 0.03), (x, yy, ztop), 0.011, 1, 16, 0.002)
            for zz in (z0, ztop - 0.005):
                ch.cyl((x, yy, zz), (x, yy, zz + 0.035), 0.0135, 2, 6, 0.001)                         # crimped fittings
        made.append('%s y %.3f x %.3f z %.3f..%.3f' % (kind, yy, x, z0, ztop))
    print('DETAIL mast:', made)
    if made:
        o = ch.done(sc); out['MastChains'] = o
        # (joined into the inner mast after the overlap check: it lifts with it)
    else:
        ch.bm.free()
    # ------------------------------------------------ overlaps with the body (mounted parts touch on purpose)
    bt_w = tree_world(body)
    for k, o in out.items():
        if o is None: continue
        ov = len(bt_w.overlap(tree_world(o)))
        print('OVERLAP %-18s with body: %d triangle pairs' % (k, ov))
    for a in newlev:
        for b in newlev:
            if a.name < b.name: print('OVERLAP %s/%s %d' % (a.name, b.name, len(tree_world(a).overlap(tree_world(b)))))
    out = {k: v for k, v in out.items() if v is not None}
    if 'MastChains' in out:
        print('OVERLAP MastChains with carriage', len(ca_t.overlap(tree_world(out['MastChains']))), 'mast', len(mi_t.overlap(tree_world(out['MastChains']))))
        for x in bpy.context.selected_objects: x.select_set(False)
        out['MastChains'].select_set(True); mast.select_set(True); bpy.context.view_layer.objects.active = mast
        bpy.ops.object.join(); out.pop('MastChains')
    print('DETAIL parts', sorted(out))
    return out, newlev, ik
