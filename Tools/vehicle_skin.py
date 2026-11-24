# A vehicle's lofted skin (the cleaning van's, Art/CleaningVan/make_cleaningvan.py, made a module for the next vehicles of to-do 212).
# setup(...) gives it the outline tables; everything else - patch, pane, opening cuts - works on that skin. Runs inside Blender.
import bpy, bmesh, math
from mathutils import Vector
import vehicle_kit as K
from vehicle_kit import ALL
XF = XR = WH = LN = ZS = None; SLOTS = []; TOPZ = 2.4; N = 3.4; RR = .16; M_GLASS = M_SEAL = None
def setup(xf, xr, wh, ln, zs, topz, slots, glass, seal, n=3.4, rr=.16):
    """Tables (z, value): nose x, tail x, half width, nose length; zs = the rings; above topz the rings move in with the offset."""
    global XF, XR, WH, LN, ZS, SLOTS, TOPZ, N, RR, M_GLASS, M_SEAL
    XF, XR, WH, LN, ZS, SLOTS, TOPZ, N, RR, M_GLASS, M_SEAL = xf, xr, wh, ln, zs, slots, topz, n, rr, glass, seal; LOFTS.clear()
def ip(tab, z):
    if z <= tab[0][0]: return tab[0][1]
    for i in range(len(tab) - 1):
        (z0, v0), (z1, v1) = tab[i], tab[i + 1]
        if z <= z1:
            h = z1 - z0; t = (z - z0) / h
            def sl(k):
                if k <= 0 or k >= len(tab) - 1: return (tab[min(k + 1, len(tab) - 1)][1] - tab[max(k - 1, 0)][1]) / (tab[min(k + 1, len(tab) - 1)][0] - tab[max(k - 1, 0)][0])
                a = (tab[k][1] - tab[k - 1][1]) / (tab[k][0] - tab[k - 1][0]); b = (tab[k + 1][1] - tab[k][1]) / (tab[k + 1][0] - tab[k][0])
                return 0.0 if a * b <= 0 else 2 * a * b / (a + b)
            m0, m1 = sl(i), sl(i + 1)
            return (2 * t ** 3 - 3 * t * t + 1) * v0 + (t ** 3 - 2 * t * t + t) * h * m0 + (-2 * t ** 3 + 3 * t * t) * v1 + (t ** 3 - t * t) * h * m1
    return tab[-1][1]
def outline(z, d):
    zz = z + d * K.ss(TOPZ - .5, TOPZ + .1, z)
    xf, xr, w, ln = ip(XF, zz) - d, ip(XR, zz) + d, ip(WH, zz) - d, ip(LN, zz)
    pts = []; n = N
    for k in range(9):
        a = math.pi / 2 * k / 8; pts.append((xr + RR * (1 - math.sin(a) ** .34), w * (math.cos(a) ** .34 if k < 8 else 0)))
    rear = [(xr + RR * (1 - math.cos(math.pi / 2 * k / 8) ** (1 / 3)), w * math.sin(math.pi / 2 * k / 8) ** (1 / 3)) for k in range(9)]
    xs0, xs1 = xr + RR, xf - ln
    side = [(xs0 + (xs1 - xs0) * k / 20, w) for k in range(1, 20)]
    nose = [(xf - ln + ln * math.sin(math.pi / 2 * k / 20) ** (2 / n), w * max(math.cos(math.pi / 2 * k / 20), 0) ** (2 / n)) for k in range(21)]
    half = rear + side + nose                                                # rear middle .. front middle, left side
    return half + [(x, -y) for x, y in half[-2:0:-1]]
def loft(name, d, top=0.0, bot=0.0, mi=0):
    zs = [z - d for z in ZS if z > TOPZ] ; zs = [z for z in ZS if z <= TOPZ and z < zs[0] - .012] + zs
    zs = [zs[0] - bot] + zs[1:]
    bm = bmesh.new(); R = []
    for z in zs: R.append([bm.verts.new((x, y, z)) for x, y in outline(z, d)])
    n = len(R[0])
    for A, B in zip(R, R[1:]):
        for k in range(n): bm.faces.new((A[k], A[(k + 1) % n], B[(k + 1) % n], B[k]))
    bm.faces.new(R[0][::-1]); t = R[-1]                                      # the roof: strips across, crowned
    h = n // 2
    for k in range(h):
        q = [t[k], t[k + 1], t[(n - k - 1) % n], t[(n - k) % n]]; u = []
        for v in q:
            if v not in u: u.append(v)
        bm.faces.new(u)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = K.link(name, bm); o.data.materials.clear()
    for m in SLOTS: o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True; p.material_index = mi
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32); return o
def boole(o, c, op, keep=True):
    md = o.modifiers.new('b', 'BOOLEAN'); md.operation = op; md.object = c; md.solver = 'EXACT'; K.apply_mods(o)
    if not keep: rm(c)
    return o
def rm(o):
    if o in ALL: ALL.remove(o)
    bpy.data.objects.remove(o, do_unlink=True)
def copy(o, name):
    c = o.copy(); c.data = o.data.copy(); c.name = name; c.data.name = name; bpy.context.collection.objects.link(c); return c
def rawprism(name, pts, axis, a, b, mi=0):
    """pts pushed along an axis: 'y' pts = (x, z); 'x' pts = (y, z); 'z' pts = (x, y)."""
    f = {'y': lambda p, t: (p[0], t, p[1]), 'x': lambda p, t: (t, p[0], p[1]), 'z': lambda p, t: (p[0], p[1], t)}[axis]
    bm = bmesh.new(); A = [bm.verts.new(f(p, a)) for p in pts]; B = [bm.verts.new(f(p, b)) for p in pts]; n = len(pts)
    bm.faces.new(A[::-1]); bm.faces.new(B)
    for i in range(n): bm.faces.new((A[i], A[(i + 1) % n], B[(i + 1) % n], B[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = K.link(name, bm)
    for m in SLOTS: o.data.materials.append(m)
    for p in o.data.polygons: p.material_index = mi; p.use_smooth = True
    return o
def rnd(pts, r, seg=5):
    """A closed outline with its corners rounded by r (one radius, or one per corner)."""
    out = []; n = len(pts)
    for i, p in enumerate(pts):
        rr = r[i] if isinstance(r, (list, tuple)) else r
        p, a, b = Vector(p), Vector(pts[i - 1]), Vector(pts[(i + 1) % n])
        if rr <= 0: out.append(tuple(p)); continue
        u, v = (a - p), (b - p); rr = min(rr, u.length * .48, v.length * .48); u = p + u.normalized() * rr; v = p + v.normalized() * rr
        for k in range(seg + 1):
            t = k / seg; out.append(tuple(u * (1 - t) ** 2 + p * 2 * t * (1 - t) + v * t * t))
    return out
def grow(pts, g):
    """The outline moved outwards by g (inwards when negative)."""
    pts = [p for i, p in enumerate(pts) if (Vector(p) - Vector(pts[i - 1])).length > 1e-5]
    n = len(pts); ar = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n)); s = 1 if ar > 0 else -1; out = []
    X = [s * ((pts[i][0] - pts[i - 1][0]) * (pts[(i + 1) % n][1] - pts[i][1]) - (pts[i][1] - pts[i - 1][1]) * (pts[(i + 1) % n][0] - pts[i][0])) for i in range(n)]
    if g < 0 and min(X) > -1e-7:                                             # inwards and convex: cut by every edge moved in (no crossings at tight corners)
        poly = [tuple(p) for p in pts]
        for i in range(n):
            a, b = Vector(pts[i]), Vector(pts[(i + 1) % n]); e = (b - a).normalized(); nn = Vector((-e.y, e.x)) * s; o = a - nn * g; res = []
            for j in range(len(poly)):
                p, q = Vector(poly[j - 1]), Vector(poly[j]); fp, fq = (p - o).dot(nn), (q - o).dot(nn)
                if (fp >= 0) != (fq >= 0): res.append(tuple(p + (q - p) * (fp / (fp - fq))))
                if fq >= 0: res.append(tuple(q))
            poly = res
        out = []
        for p in poly:
            if not out or (Vector(p) - Vector(out[-1])).length > 2e-4: out.append(p)
        return out
    for i in range(n):
        p, a, b = Vector(pts[i]), Vector(pts[i - 1]), Vector(pts[(i + 1) % n]); e0, e1 = (p - a), (b - p)
        if e0.length < 1e-6 or e1.length < 1e-6: out.append(tuple(p)); continue
        e0.normalize(); e1.normalize(); n0, n1 = Vector((e0.y, -e0.x)) * s, Vector((e1.y, -e1.x)) * s; m = n0 + n1
        m = m / max(m.length_squared / 2, .35); out.append(tuple(p + m * g))
    return out
LOFTS = {}
def LF(d, bot=0.0):
    if (d, bot) not in LOFTS: LOFTS[(d, bot)] = loft('loft_%s' % len(LOFTS), d, bot=bot)
    return LOFTS[(d, bot)]
def patch(name, pts, axis, a, b, m, d0, d1, hole=None, smooth=32):
    """The skin between offsets d0 (outer) and d1 (inner) inside an outline: panes, strips, seals, trims."""
    o = rawprism(name, pts, axis, a, b)
    if hole: boole(o, rawprism('h', hole, axis, a - 1, b + 1), 'DIFFERENCE', False)
    boole(o, LF(d0), 'INTERSECT'); boole(o, LF(d1, .5), 'DIFFERENCE')
    assert len(o.data.polygons), 'empty patch ' + name
    return K.finish(o, m, smooth)
def pane(n, pts, axis, a, b):
    patch(n + '_Glass', grow(pts, .006), axis, a, b, M_GLASS, .012, .018)
    patch(n + '_Seal', grow(pts, .014), axis, a, b, M_SEAL, -.003, .026, hole=grow(pts, -.016))
def surf(y, z, o=0.0):
    """A place on the nose (y across, z up), o in front of the glass."""
    return Vector((ip(XF, z) - ip(LN, z) + ip(LN, z) * max(1 - (abs(y) / ip(WH, z)) ** N, 0) ** (1 / N) + o, y, z))
def drop_lofts():
    for o in list(LOFTS.values()): rm(o)
    LOFTS.clear()
