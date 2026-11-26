# The maintenance van (to-do 311): a copy of the cabin cleaning van (Art/CleaningVan, left as it is; owner: copy means copy).
# Changed: MAINTENANCE on both sides and the right rear door, navy on white panels like the name panels, drawn mirrored
# (the loader mirrors Blender x); its amber roof beacon (BeaconLens) is the van's own, through the lamp system (GSE.cpp).
# The cleaning van's text follows. The cabin cleaning van (to-do 212): a long-wheelbase, high-roof electric panel CREW van, our own shape, no brand.
# Published sizes of the European L3H2 class: length 5.998, width 2.050 (no mirrors), height 2.522, wheelbase 4.035, tracks
# 1.810 / 1.790, tyres 225/75 R16 (dia 0.744), sill 0.56, side door 1.25 x 1.75, rear opening 1.56 x 1.79.
# Shape seen in photos (named on docs/Blueprints/Apron/CleaningVan.png). 1 + 1 seats in front, a row of three behind them
# (sliding door on the RIGHT), a bulkhead, the load space with shelves, caddies, vacuums, bag rolls, full bags and the cart.
# Axes: x forward, y left, z up; metres; origin on the ground under the middle. Driver front left.
# The body is one lofted skin (skin(a, b) = the space between two offsets of it); every panel, door, pane and strip is cut
# out of that same skin, so a door always follows the body. Names as the baggage tractor / belt loader (GSE.cpp) use them.
#   blender -b --python Art/MaintenanceVan/make_maintenancevan.py [-- --pic]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ALL, IKS
ART = os.path.join(ROOT, 'Art', 'MaintenanceVan'); TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures')
K.reset()
import make_cleaningkit as CK
L, W, H, WB, TD = 5.998, 2.050, 2.522, 4.035, 0.744
FX, RX, TRF, TRR, RW = 2.051, -1.984, 1.810, 1.790, TD / 2
FLOOR = 0.575                                                   # the floor's top, cab and load space

# #region ---- materials
P = K.pmat
M_BODY = P('MV_Livery_Body_Paint', (0.96, 0.74, 0.04), 0.1, 0.35); M_MAIN = P('MV_Livery_Main_Paint', (0.04, 0.10, 0.30), 0.1, 0.35)
M_NAME = P('MV_NamePanel_Paint', (0.9, 0.9, 0.9), 0, 0.4); M_WORD = P('MV_MaintLetter_Paint', (0.006, 0.014, 0.05), 0, 0.45)
M_BUMP = P('MV_BumperPlastic', (0.045, 0.045, 0.05), 0, 0.62); M_PLAST = P('MV_BlackPlastic', (0.035, 0.035, 0.04), 0, 0.55); M_DASH = P('MV_DashPlastic', (0.10, 0.105, 0.115), 0, 0.7)
M_TRIM = P('MV_GreyTrimPlastic', (0.33, 0.34, 0.36), 0, 0.75); M_LIN = P('MV_HeadLining', (0.62, 0.62, 0.60), 0, 0.9); M_LOAD = P('MV_LoadLining_Paint', (0.62, 0.64, 0.66), 0.05, 0.5)
M_GLASS = P('MV_Glass', (0.2, 0.3, 0.35), 0, 0.05); M_SEAL = P('MV_SealRubber', (0.02, 0.02, 0.02), 0, 0.85); M_TYRE = P('MV_Tyre', (0.03, 0.03, 0.03), 0, 0.9)
M_STEEL = P('MV_Steel', (0.5, 0.51, 0.53), 1, 0.4); M_DARK = P('MV_DarkSteel', (0.09, 0.09, 0.1), 0.6, 0.55); M_RIM = P('MV_RimSteel', (0.62, 0.63, 0.65), 0.9, 0.35)
M_SEAT = P('MV_SeatFabric', (0.06, 0.065, 0.075), 0, 0.85); M_SEAT2 = P('MV_SeatBolster', (0.03, 0.06, 0.14), 0, 0.85); M_FLOOR = P('MV_FloorMat', (0.05, 0.05, 0.05), 0, 0.9)
M_PLY = P('MV_LoadFloorPlate', (0.30, 0.29, 0.27), 0, 0.8); M_AMB = P('MV_AmberLens', (1.0, 0.5, 0.04), 0, 0.2, 1.0); M_RED = P('MV_RedLens', (0.85, 0.04, 0.03), 0, 0.2, 0.6)
M_WHT = P('MV_WhiteLens', (0.95, 0.95, 0.92), 0, 0.1, 1.0); M_CHROME = P('MV_Reflector', (0.8, 0.8, 0.82), 1, 0.12); M_MIR = P('MV_MirrorGlass', (0.8, 0.85, 0.9), 1, 0.02)
M_SCR = P('MV_Screen', (0.03, 0.10, 0.14), 0, 0.1, 0.4); M_HVR = P('MV_HiVisRedReflector', (0.75, 0.03, 0.02), 0, 0.35); M_HVY = P('MV_HiVisYellowReflector', (0.95, 0.85, 0.05), 0, 0.35)
M_HVW = P('MV_HiVisWhiteReflector', (0.88, 0.9, 0.9), 0, 0.3); M_BELT = P('MV_BeltWebbing', (0.02, 0.02, 0.022), 0, 0.8); M_ORG = P('MV_BuckleRed', (0.7, 0.04, 0.02), 0, 0.45)
for m, t in ((M_STEEL, None), (M_DARK, (.25, .25, .27)), (M_RIM, (1.05, 1.06, 1.08))): K.photo(m, TEX, 'Metal027', tile=0.6, tint=t)
K.photo(M_TYRE, TEX, 'Rubber004', tile=0.5, tint=(.35, .35, .35), metal=0.0); K.photo(M_FLOOR, TEX, 'Rubber001', tile=0.5, tint=(.4, .4, .4), metal=0.0)
for m, t in ((M_BUMP, (.40, .40, .42)), (M_PLAST, (.32, .32, .34)), (M_DASH, (.75, .77, .8))): K.photo(m, TEX, 'Plastic006', tile=0.4, tint=t, metal=0.0)
SLOTS = [M_BODY, M_MAIN, M_BUMP, M_LIN, M_LOAD, M_SEAL]
# #endregion

# #region ---- the skin: outlines at each height, put together into a closed block, offset inwards by d
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
XF = [(.26, 2.90), (.34, 2.965), (.46, 2.999), (.60, 2.992), (.74, 2.975), (.86, 2.955), (.96, 2.915), (1.04, 2.83), (1.12, 2.68), (1.20, 2.49), (1.27, 2.315), (1.33, 2.215), (1.40, 2.135), (1.60, 1.945), (1.90, 1.665), (2.10, 1.475), (2.20, 1.36), (2.28, 1.23), (2.35, 1.06), (2.41, .84), (2.46, .56), (2.49, .30), (2.51, .05), (2.522, -.25), (2.60, -.9)]
XR = [(.26, -2.94), (.40, -2.990), (.60, -2.992), (1.2, -2.985), (2.0, -2.965), (2.3, -2.952), (2.40, -2.92), (2.46, -2.86), (2.49, -2.79), (2.51, -2.70), (2.522, -2.55), (2.60, -2.2)]
WH = [(.26, .935), (.36, .985), (.52, 1.013), (.74, 1.025), (1.10, 1.025), (1.50, 1.013), (1.90, .988), (2.20, .960), (2.32, .935), (2.40, .895), (2.45, .845), (2.49, .765), (2.51, .67), (2.522, .50), (2.60, .2)]
LN = [(.26, .95), (.9, 1.0), (1.2, .95), (1.4, .80), (2.1, .72), (2.4, .75), (2.6, .7)]
ZS = [.26, .30, .34, .40, .46, .52, .60, .66, .74, .80, .86, .94, 1.02, 1.07, 1.12, 1.16, 1.20, 1.235, 1.27, 1.30, 1.33, 1.365, 1.40, 1.46, 1.53, 1.60, 1.68, 1.76, 1.84, 1.92, 2.0, 2.06, 2.12, 2.16, 2.20, 2.24, 2.28, 2.315, 2.35, 2.38, 2.41, 2.435, 2.46, 2.475, 2.49, 2.50, 2.51, 2.517, 2.522]
def outline(z, d):
    zz = z + d * K.ss(1.9, 2.5, z)
    xf, xr, w, ln = ip(XF, zz) - d, ip(XR, zz) + d, ip(WH, zz) - d, ip(LN, zz)
    pts = []; n = 3.4
    for k in range(9):
        a = math.pi / 2 * k / 8; pts.append((xr + .16 * (1 - math.sin(a) ** .34), w * (math.cos(a) ** .34 if k < 8 else 0)))
    rear = [(xr + .16 * (1 - math.cos(math.pi / 2 * k / 8) ** (1 / 3)), w * math.sin(math.pi / 2 * k / 8) ** (1 / 3)) for k in range(9)]
    xs0, xs1 = xr + .16, xf - ln
    side = [(xs0 + (xs1 - xs0) * k / 20, w) for k in range(1, 20)]
    nose = [(xf - ln + ln * math.sin(math.pi / 2 * k / 20) ** (2 / n), w * max(math.cos(math.pi / 2 * k / 20), 0) ** (2 / n)) for k in range(21)]
    half = rear + side + nose                                                # rear middle .. front middle, left side
    return half + [(x, -y) for x, y in half[-2:0:-1]]
def loft(name, d, top=0.0, bot=0.0, mi=0):
    zs = [z - d for z in ZS if z > 2.4] ; zs = [z for z in ZS if z <= 2.4 and z < zs[0] - .012] + zs
    zs = [zs[0] - bot] + zs[1:]
    bm = bmesh.new(); R = []
    for z in zs: R.append([bm.verts.new((x, y, z)) for x, y in outline(z, d)])
    n = len(R[0])
    for A, B in zip(R, R[1:]):
        for k in range(n): bm.faces.new((A[k], A[(k + 1) % n], B[(k + 1) % n], B[k]))
    bm.faces.new(R[0][::-1]); t = R[-1]                                      # the roof: strips across, crowned
    c = [bm.verts.new((v.co.x * 1.0, 0, v.co.z + .004)) if False else None for v in t]
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
def ik(n, loc, **kw): return K.ik(n, loc, **kw)
def piv(n, loc): return K.ik(n, loc, prefix='PIV_')
# #endregion

# #region ---- the body: skin 40 mm, everything cut from it
SH = 0.040
body = loft('Body', 0.0); inner = loft('inner', SH, bot=.5, mi=3)
for p in inner.data.polygons:
    if p.center.x < -.93: p.material_index = 4
    if p.center.z < .56: p.material_index = 2
shell = boole(body, inner, 'DIFFERENCE', False)
_b = bmesh.new(); _b.from_mesh(shell.data); bmesh.ops.bisect_plane(_b, geom=_b.verts[:] + _b.edges[:] + _b.faces[:], plane_co=(2.28, 0, .94), plane_no=(1, 0, -.6), dist=1e-5); _b.to_mesh(shell.data); _b.free()
for p in shell.data.polygons:                                               # livery band and the black bumper, on the skin's own grid lines
    c = p.center
    if p.material_index == 0 and p.normal.z < .9:
        if .86 < c.z < 1.02 and c.x - 2.28 - .6 * (c.z - .94) < 0: p.material_index = 1
        if c.z < .60 and (c.x > 2.35 or c.x < -2.35): p.material_index = 2
        if c.z < .40: p.material_index = 2
CABZ = 2.06
DOORF = rnd([(.47, .40), (1.545, .40), (1.56, .70), (1.64, .90), (1.70, 1.28), (.985, CABZ), (.47, CABZ)], [.05, .05, .10, .10, .12, .10, .07])
SLIDE = rnd([(-.885, .40), (.375, .40), (.375, 2.12), (-.885, 2.12)], .07)
DW = .781
REARO = rnd([(-DW, .54), (DW, .54), (DW, 2.33), (-DW, 2.33)], .06)
HOOD = rnd([(2.30, -.74), (2.80, -.66), (2.80, .66), (2.30, .74)], .08)
GAP = .004
def opening(name, pts, axis, a, b):
    """Cuts the opening out of the body and returns the panel (the same skin inside the outline, GAP smaller all round)."""
    pan = copy(shell0, name); boole(pan, rawprism('c', grow(pts, -GAP), axis, a, b), 'INTERSECT', False)
    boole(shell, rawprism('c', pts, axis, a, b), 'DIFFERENCE', False); return pan
shell0 = copy(shell, 'shell0')
doorD = opening('DoorDriver', DOORF, 'y', .3, 1.3); doorP = opening('DoorPassenger', DOORF, 'y', -1.3, -.3)
slide = opening('SlideDoor', SLIDE, 'y', -1.3, -.3)
rL = opening('RearDoorL', [p for p in rnd([(.0025, .54), (DW, .54), (DW, 2.33), (.0025, 2.33)], [.002, .06, .06, .002])], 'x', -3.2, -2.80)
rR = opening('RearDoorR', [p for p in rnd([(-.0025, .54), (-DW, .54), (-DW, 2.33), (-.0025, 2.33)], [.002, .06, .06, .002])], 'x', -3.2, -2.80)
hood = opening('Hood', HOOD, 'z', .85, 1.36)
boole(shell, rawprism('c', REARO, 'x', -3.2, -2.80), 'DIFFERENCE', False)   # (no sliver between the two leaves)
rm(shell0)
HL0 = rnd([(.50, .90), (.80, .84), (.905, .90), (.93, 1.06), (.86, 1.10), (.62, 1.02)], [.03, .04, .05, .05, .04, .05])
for s in (-1, 1): boole(shell, rawprism('c', [(s * y, z) for y, z in HL0], 'x', 2.0, 3.1), 'DIFFERENCE', False)
for s in (-1, 1):                                                           # wheel arches
    for k, x in (('F', FX), ('R', RX)):
        c = cyl('c', (x, s * .95, RW), .432, .55, 'y', M_BODY, 64); ALL.remove(c); boole(shell, c, 'DIFFERENCE', False)
WSC = rnd([(-.825, 1.385), (.825, 1.385), (.745, 2.13), (-.745, 2.13)], [.06, .06, .09, .09])
boole(shell, rawprism('c', WSC, 'x', 1.0, 2.6), 'DIFFERENCE', False)
WD = rnd([(.56, 1.20), (1.575, 1.20), (1.605, 1.30), (.965, 1.98), (.56, 1.98)], [.05, .05, .06, .06, .05])    # front door pane
WS = rnd([(-.76, 1.26), (.26, 1.26), (.26, 1.98), (-.76, 1.98)], .07)        # sliding door's pane, and the one opposite
WR = {1: rnd([(.11, 1.30), (.70, 1.30), (.70, 2.06), (.11, 2.06)], .06)}; WR[-1] = [(-y, z) for y, z in WR[1]]
for o, s in ((doorD, 1), (doorP, -1)): boole(o, rawprism('c', WD, 'y', s * .3, s * 1.3), 'DIFFERENCE', False)
boole(slide, rawprism('c', WS, 'y', -1.3, -.3), 'DIFFERENCE', False); boole(shell, rawprism('c', WS, 'y', .3, 1.3), 'DIFFERENCE', False)
for o, s in ((rL, 1), (rR, -1)): boole(o, rawprism('c', WR[s], 'x', -3.2, -2.7), 'DIFFERENCE', False)
RAILZ = 1.235
boole(shell, rawprism('c', [(-2.36, RAILZ - .022), (-.80, RAILZ - .022), (-.80, RAILZ + .022), (-2.36, RAILZ + .022)], 'y', -1.3, -.995), 'DIFFERENCE', False)   # the slide rail's groove
for o in (shell, doorD, doorP, slide, rL, rR, hood):                        # clean: unused slots off, edges softened by normals
    used = sorted({p.material_index for p in o.data.polygons}); ms = [o.data.materials[i] for i in used]; ix = [used.index(p.material_index) for p in o.data.polygons]
    o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, i in zip(o.data.polygons, ix): p.material_index = i; p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32); ALL.append(o)
# #endregion

# #region ---- panes and their seals
def pane(n, pts, axis, a, b):
    patch(n + '_Glass', grow(pts, .006), axis, a, b, M_GLASS, .012, .018)
    patch(n + '_Seal', grow(pts, .014), axis, a, b, M_SEAL, -.003, .026, hole=grow(pts, -.016))
pane('Windscreen', WSC, 'x', 1.0, 2.6); pane('DoorDriver_Window', WD, 'y', .3, 1.3); pane('DoorPassenger_Window', WD, 'y', -1.3, -.3)
pane('SlideDoor_Window', WS, 'y', -1.3, -.3); pane('CrewWindowL', WS, 'y', .3, 1.3); pane('RearDoorL_Window', WR[1], 'x', -3.2, -2.7); pane('RearDoorR_Window', WR[-1], 'x', -3.2, -2.7)
# #endregion

# #region ---- skin fittings: arch lips, rub strips, lamps, hi-vis marking, name panels
def arc(x, r, a0, a1, n=28): return [(x + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), RW + r * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]
for s, sn in ((1, 'L'), (-1, 'R')):
    a, b = (s * .80, s * 1.3)
    for k, x in (('F', FX), ('R', RX)): patch('ArchLip_%s%s' % (k, sn), arc(x, .476, 4, 176) + arc(x, .429, 176, 4), 'y', a, b, M_BUMP, -.010, .028)
    def strip(n, x0, x1, z0=.625, z1=.735): patch(n, rnd([(x0, z0), (x1, z0), (x1, z1), (x0, z1)], .02, 3), 'y', a, b, M_BUMP, -.012, .006, smooth=30)
    strip('DoorDriver_Rub' if s > 0 else 'DoorPassenger_Rub', .487, 1.53)
    if s > 0: strip('RubL_Mid', -1.47, .455)
    else: strip('SlideDoor_Rub', -.868, .357); strip('RubR_Mid', -1.47, -.903)
    strip('Rub%s_Rear' % sn, -2.80, -2.50)
    # reflective marking: white-red blocks along the sill line (apron high visibility), above the black strip
    xs = [(-2.80, -2.50), (-1.45, -1.18), (-1.17, -.905)] + ([(-.87, -.46), (-.45, -.04), (-.03, .36)]) + [(.49, .84), (.85, 1.19), (1.20, 1.54)]
    for i, (x0, x1) in enumerate(xs):
        own = 'HiVis%s_%d' % (sn, i)
        if x0 > .48: own = ('DoorDriver' if s > 0 else 'DoorPassenger') + '_HiVis%d' % i
        elif x0 > -.88 and s < 0: own = 'SlideDoor_HiVis%d' % i
        patch(own, [(x0, .755), (x1, .755), (x1, .835), (x0, .835)], 'y', a, b, M_HVR if i % 2 else M_HVW, -.0025, .006)
    patch('NamePanel_' + sn, rnd([(-2.62, 1.46), (-1.06, 1.46), (-1.06, 1.86), (-2.62, 1.86)], .03, 3), 'y', a, b, M_NAME, -.003, .006)
    for i, x in enumerate((.95, -1.30, -2.64)):                              # side markers in the skin's strip
        n = 'SideMarker_%s%d' % (sn, i)
        if x > 0: continue
        patch(n + '_Lens', rnd([(x - .05, .655), (x + .05, .655), (x + .05, .705), (x - .05, .705)], .012, 3), 'y', a, b, M_AMB, -.016, .004)
    # headlamp cluster: a dark housing in the skin, reflector bowls behind the lens
    HL = rnd([(.50, .90), (.80, .84), (.905, .90), (.93, 1.06), (.86, 1.10), (.62, 1.02)], [.03, .04, .05, .05, .04, .05]); HL = [(s * y, z) for y, z in HL]
    patch('Headlamp_%s_Bezel' % sn, grow(HL, .012), 'x', 2.0, 3.1, M_PLAST, -.004, .05, hole=grow(HL, -.012))
    patch('Headlamp_%s_Bowl' % sn, grow(HL, -.008), 'x', 2.0, 3.1, M_CHROME, .022, .045)
    for nm, y0, y1, mt in (('LowBeam', .55, .72, M_WHT), ('HighBeam', .725, .80, M_WHT), ('IndicatorF', .805, .925, M_AMB)):
        q = [p for p in grow(HL, -.010)]
        o = patch('%s_%s_Lens' % (nm, sn), q, 'x', 2.0, 3.1, mt, .001, .006)
        c = rawprism('c', [(s * y0, .5), (s * y1, .5), (s * y1, 1.5), (s * y0, 1.5)], 'x', 1.5, 3.4); boole(o, c, 'INTERSECT', False); K.finish(o, mt, 32); ALL.remove(o)
        assert len(o.data.polygons), nm
    # rear lamps: upright, in the corner pillar
    for nm, z0, z1, mt in (('TailBrake', 1.16, 1.50, M_RED), ('IndicatorR', 1.04, 1.155, M_AMB), ('Reverse', .925, 1.035, M_WHT)):
        patch('%s_%s_Lens' % (nm, sn), [(s * .845, z0), (s * .999, z0), (s * .985, z1), (s * .845, z1)], 'x', -3.2, -2.86, mt, -.008, .006)
    patch('RearLamp_%s_Housing' % sn, rnd([(s * .835, .912), (s * 1.012, .912), (s * 1.0, 1.513), (s * .835, 1.513)], .02, 3), 'x', -3.2, -2.845, M_PLAST, -.0035, .02)
    # rear chevrons, red and yellow, 45 degrees, on each leaf's lower panel
    y0, y1, z0, z1 = .02, .76, .60, 1.18; n = 0; wd = .165
    for k in range(-6, 9):
        c0 = k * wd * 1.4142; pts = [(y0, c0 + z0), (y0, c0 + z0 + wd * 1.4142), (y1, c0 + z0 + wd * 1.4142 - (y1 - y0)), (y1, c0 + z0 - (y1 - y0))]
        cl = [(y, z) for y, z in pts]                                        # clipped to the panel
        def clip(poly, f):
            out = []
            for i in range(len(poly)):
                p, q = poly[i - 1], poly[i]; fp, fq = f(p), f(q)
                if (fp >= 0) != (fq >= 0): t = fp / (fp - fq); out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
                if fq >= 0: out.append(q)
            return out
        cl = clip(clip(cl, lambda p: p[1] - z0), lambda p: z1 - p[1])
        cl = [p for i, p in enumerate(cl) if (Vector(p) - Vector(cl[i - 1])).length > 1e-4]
        if len(cl) < 3 or abs(sum(cl[i - 1][0] * cl[i][1] - cl[i][0] * cl[i - 1][1] for i in range(len(cl)))) < .004: continue
        patch('RearDoor%s_Chev%d' % (sn, n), [(s * y, z) for y, z in cl], 'x', -3.2, -2.86, M_HVR if k % 2 else M_HVY, -.0022, .006); n += 1
patch('RearDoorL_NamePanel', rnd([(.06, 2.115), (.70, 2.115), (.70, 2.265), (.06, 2.265)], .02, 3), 'x', -3.2, -2.86, M_NAME, -.003, .006)
for k in range(9):                                                          # the grille: slats over a black field
    pass
GR = rnd([(-.50, .66), (.50, .66), (.60, .88), (-.60, .88)], .05)
patch('Grille_Field', GR, 'x', 2.5, 3.2, M_PLAST, -.004, .02)
for i in range(5):
    z = .692 + .038 * i; hw = .47 + .10 * (z - .66) / .22; patch('Grille_Slat%d' % i, [(-hw, z), (hw, z), (hw, z + .016), (-hw, z + .016)], 'x', 2.5, 3.2, M_DARK, -.016, .002)
patch('ChargeFlap', rnd([(-.09, .925), (.09, .925), (.09, 1.0), (-.09, 1.0)], .02, 3), 'x', 2.6, 3.2, M_PLAST, -.004, .01)
patch('LowerGrille', rnd([(-.55, .36), (.55, .36), (.60, .47), (-.60, .47)], .03), 'x', 2.5, 3.2, M_PLAST, -.004, .02)
for s, sn in ((1, 'L'), (-1, 'R')): patch('FogBezel_' + sn, rnd([(s * .66, .40), (s * .80, .40), (s * .80, .50), (s * .66, .50)], .03, 3), 'x', 2.2, 3.2, M_PLAST, -.006, .02)
patch('PlateFront', [(-.26, .50), (.26, .50), (.26, .61), (-.26, .61)], 'x', 2.7, 3.2, M_NAME, -.005, .006)
box('RearBumper', (-2.955, 0, .46), (.155, 1.78, .13), M_BUMP, .02, 3); box('RearBumper_Step', (-2.975, 0, .528), (.11, 1.45, .014), M_FLOOR, .004)
box('PlateRear', (-3.034, 0, .455), (.006, .52, .11), M_NAME, .0015, 1)
for s, sn in ((1, 'L'), (-1, 'R')): box('Reflector_%s' % sn, (-3.034, s * .70, .455), (.008, .14, .05), M_RED, .003)
# #endregion

# #region ---- wheels (tyre, steel rim with holes, hub, cap, nuts), spindles, arch liners, underside
TP = [(.200, -.098), (.232, -.112), (.275, -.1165), (.318, -.112), (.348, -.100), (.362, -.086), (.3695, -.066), (.372, -.030), (.372, .030), (.3695, .066), (.362, .086), (.348, .100), (.318, .112), (.275, .1165), (.232, .112), (.200, .098), (.196, 0)]
RP = [(.214, -.104), (.208, -.098), (.196, -.092), (.186, -.050), (.182, 0), (.186, .050), (.196, .092), (.208, .098), (.214, .104), (.204, .106), (.190, .098), (.176, .050), (.174, 0), (.176, -.050), (.190, -.098), (.204, -.106)]
def wheel(key, x, y, s):
    c = (x, y, RW); lathe('Tyre_' + key, TP, c, M_TYRE, 72, 40); lathe('Rim_' + key, RP, c, M_RIM, 72, 40)
    for j, yy in enumerate((-.045, -.015, .015, .045)): pass
    d = lathe('WheelDisc_' + key, [(.0, s * .030), (.058, s * .030), (.075, s * .050), (.100, s * .062), (.130, s * .050), (.150, s * .030), (.179, s * .022), (.179, s * .010), (.148, s * .016), (.128, s * .036), (.100, s * .048), (.072, s * .034), (.058, s * .014), (.0, s * .014)], c, M_RIM, 72, 40)
    for j in range(8):
        a = math.pi / 4 * j + .2; h = cyl('c', (x + .142 * math.cos(a), y + s * .03, RW + .142 * math.sin(a)), .017, .12, 'y', M_RIM, 20); cut(d, h)
    cyl('Hub_' + key, (x, y - s * .010, RW), .062, .075, 'y', M_DARK, 32, .004); cyl('HubCap_' + key, (x, y + s * .048, RW), .036, .040, 'y', M_PLAST, 32, .008)
    for j in range(5):
        a = 2 * math.pi * j / 5 + .3; cyl('Nut_%s_%d' % (key, j), (x + .056 * math.cos(a), y + s * .038, RW + .056 * math.sin(a)), .0105, .024, 'y', M_STEEL, 6)
for s, sn in ((1, 'L'), (-1, 'R')):
    wheel('F' + sn, FX, s * TRF / 2, s); wheel('R' + sn, RX, s * TRR / 2, s)
    piv('Wheel_F' + sn, (FX, s * TRF / 2, RW)); piv('Wheel_R' + sn, (RX, s * TRR / 2, RW)); piv('Steer_F' + sn, (FX, s * .775, RW))
    box('Spindle_F' + sn, (FX, s * .790, RW), (.11, .06, .20), M_DARK, .012); cyl('Spindle_F%s_Axle' % sn, (FX, s * .835, RW), .030, .09, 'y', M_STEEL, 24)
    cyl('Disc_F' + sn, (FX, s * .868, RW), .150, .020, 'y', M_STEEL, 48, .002); box('SteerArm_F' + sn, (FX - .085, s * .765, RW - .03), (.13, .03, .025), M_DARK, .006)
    cyl('Strut_F' + sn, (FX, s * .735, .62), .032, .40, 'z', M_DARK, 24); box('WishArm_F' + sn, (FX, s * .56, .290), (.10, .50, .035), M_DARK, .01)
    for kk, x in (('F', FX), ('R', RX)):
        bm = bmesh.new(); A = []; yo, yi = s * .985, s * (.640 if kk == 'F' else .705)
        n = 30
        for r in (.436, .448):
            A.append([[bm.verts.new((x + r * math.cos(math.radians(-8 + 196 * k / n)), y, RW + r * math.sin(math.radians(-8 + 196 * k / n)))) for k in range(n + 1)] for y in (yi, yo)])
        for k in range(n):
            for q, (p0, p1) in enumerate(((A[0][0], A[0][1]), (A[1][0], A[1][1]))): bm.faces.new((p0[k], p0[k + 1], p1[k + 1], p1[k]))
            for e in (0, 1): bm.faces.new((A[0][e][k], A[0][e][k + 1], A[1][e][k + 1], A[1][e][k]))
        for k in (0, n): bm.faces.new((A[0][0][k], A[0][1][k], A[1][1][k], A[1][0][k]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); K.finish(K.link('ArchLiner_%s%s' % (kk, sn), bm), M_PLAST, 40)
        w = cyl('ArchWall_%s%s' % (kk, sn), (x, yi - s * .006, RW), .448, .014, 'y', M_PLAST, 64); c = box('c', (x, yi, .05), (1.2, .2, .56), M_PLAST); cut(w, c)
cyl('RearAxle', (RX, 0, RW), .045, 1.60, 'y', M_DARK, 32); box('SubFrameF', (FX, 0, .34), (.42, 1.10, .10), M_DARK, .02); box('EMotor', (FX + .06, 0, .56), (.46, .56, .36), M_DARK, .05)
for s, sn in ((1, 'L'), (-1, 'R')):
    box('Frame_' + sn, (-.45, s * .60, .455), (4.70, .075, .11), M_DARK, .008, 1); box('LeafSpring_' + sn, (RX, s * .60, .375), (1.35, .065, .05), M_DARK, .012); box('RearHanger_' + sn, (RX - .60, s * .60, .41), (.06, .06, .05), M_DARK, .004); box('RearHangerF_' + sn, (RX + .60, s * .60, .41), (.06, .06, .05), M_DARK, .004)
box('BatteryBox', (-.45, 0, .37), (2.30, 1.14, .20), M_DARK, .03); box('FrontCross', (1.60, 0, .455), (.10, 1.16, .10), M_DARK, .008)
box('SillL', (-.10, .945, .315), (2.90, .10, .07), M_BUMP, .015); box('SillR', (-.10, -.945, .315), (2.90, .10, .07), M_BUMP, .015)
# #endregion

# #region ---- floor, steps, firewall, bulkhead, ribs
fl = box('Floor', (-.615, 0, FLOOR - .03), (4.63, 1.968, .06), M_PLY, 0)
for c in ([(1.01, .86, .5), (1.06, .30, .5)], [(1.01, -.86, .5), (1.06, .30, .5)], [(-.255, -.86, .5), (1.18, .30, .5)]): cut(fl, box('c', *c, M_PLY))
for s in (-1, 1): cut(fl, box('c', (RX, s * .86, .6), (.90, .31, .5), M_PLY))
for n, x0, x1, y0, y1 in (('StepDriver', .485, 1.535, .715, .972), ('StepPassenger', .485, 1.535, -.972, -.715), ('StepSlide', -.84, .33, -.972, -.715)):
    xm, ym = (x0 + x1) / 2, (y0 + y1) / 2; s = 1 if ym > 0 else -1
    box(n, (xm, ym, .40), (x1 - x0, abs(y1 - y0), .03), M_FLOOR, .006); box(n + '_Riser', (xm, s * .709, .48), (x1 - x0 + .01, .014, .16), M_TRIM, .002, 1)
    for e, xx in ((0, x0), (1, x1)): box(n + '_End%d' % e, (xx, ym, .48), (.014, abs(y1 - y0), .16), M_TRIM, .002, 1)
box('CabMat', (.50, 0, FLOOR + .003), (2.35, 1.40, .012), M_FLOOR, .004); box('LoadFloor', (-1.93, 0, FLOOR + .003), (1.95, 1.395, .014), M_PLY, .004)
patch('FirewallSkin', [(-1.2, .50), (1.2, .50), (1.2, 1.335), (-1.2, 1.335)], 'x', 1.685, 1.700, M_PLAST, .036, 3.0) if False else None
fw = rawprism('Firewall', [(-1.2, .54), (1.2, .54), (1.2, 1.31), (-1.2, 1.31)], 'x', 1.722, 1.742); boole(fw, LF(.036), 'INTERSECT'); boole(fw, rawprism('c', [(-1.2, .50), (-.63, .50), (-.63, .86), (-1.2, .86)], 'x', 1.4, 2), 'DIFFERENCE', False); boole(fw, rawprism('c', [(1.2, .50), (.63, .50), (.63, .86), (1.2, .86)], 'x', 1.4, 2), 'DIFFERENCE', False); K.finish(fw, M_PLAST, 32)
cw = rawprism('Cowl', [(-1.2, 1.300), (1.2, 1.300), (1.2, 1.318), (-1.2, 1.318)], 'x', 1.715, 2.6); boole(cw, LF(.036), 'INTERSECT'); K.finish(cw, M_PLAST, 32)
bh = rawprism('Bulkhead', [(-1.2, FLOOR - .01), (1.2, FLOOR - .01), (1.2, 2.6), (-1.2, 2.6)], 'x', -.945, -.925); boole(bh, LF(.036), 'INTERSECT'); BW = rnd([(-.50, 1.45), (.50, 1.45), (.50, 1.95), (-.50, 1.95)], .05); boole(bh, rawprism('c', BW, 'x', -1.5, -.5), 'DIFFERENCE', False); K.finish(bh, M_TRIM, 32)
gl = rawprism('Bulkhead_Glass', grow(BW, .01), 'x', -.938, -.932); K.finish(gl, M_GLASS, 30)
bs = rawprism('Bulkhead_Seal', grow(BW, .02), 'x', -.950, -.920); boole(bs, rawprism('c', grow(BW, -.012), 'x', -1.5, -.5), 'DIFFERENCE', False); K.finish(bs, M_SEAL, 30)
for i, x in enumerate((-1.45, -2.05, -2.62)):                               # ribs of the load space: sides and roof, cut from the same skin
    o = patch('Rib%d' % i, [(x - .035, FLOOR + .02), (x + .035, FLOOR + .02), (x + .035, 2.6), (x - .035, 2.6)], 'y', -1.2, 1.2, M_LOAD, .038, .068)
for s, sn in ((1, 'L'), (-1, 'R')):                                         # plywood lining, low, between the arch and the back
    patch('Ply%s0' % sn, [(-1.50, .62), (-.955, .62), (-.955, 1.16), (-1.50, 1.16)], 'y', s * .7, s * 1.2, M_PLY, .062, .074)
patch('CabLiner', [(-.915, 2.20), (.98, 2.20), (.98, 2.6), (-.915, 2.6)], 'y', -1.2, 1.2, M_LIN, .036, .052)
for i, (x, y) in enumerate(((-1.75, 0), (-2.45, 0), (.45, 0))):
    z = 2.466 if x < 0 else 2.395; box('RoofLight%d_Base' % i, (x, y, z), (.30, .10, .05), M_PLAST, .006); box('RoofLight%d_Glass' % i, (x, y, z - .026), (.28, .085, .012), M_NAME, .004)
# #endregion

# #region ---- doors: trims, handles, hinges, the slide door's arm and rail, rear door gear
for s, n in ((1, 'DoorDriver'), (-1, 'DoorPassenger')):
    a, b = s * .3, s * 1.3
    tr = [p for p in grow(DOORF, -.028)]
    patch(n + '_Trim', tr, 'y', a, b, M_DASH, .038, .062, hole=grow(WD, .012)); clipz = 0
    box(n + '_Armrest', (.93, s * .915, 1.02), (.55, .055, .06), M_DASH, .02, 3); box(n + '_Pocket', (1.00, s * .925, .66), (.72, .040, .18), M_DASH, .015)
    box(n + '_HandleIn_Cup', (1.28, s * .936, 1.10), (.13, .018, .06), M_PLAST, .008); box(n + '_HandleIn', (1.27, s * .926, 1.105), (.085, .012, .022), M_CHROME, .004)
    box(n + '_Switches', (1.16, s * .915, 1.053), (.13, .045, .014), M_PLAST, .005)
    for i in range(3): box(n + '_Sw%d' % i, (1.12 + .04 * i, s * .915, 1.061), (.024, .028, .009), M_DARK, .003)
    cyl(n + '_Speaker', (1.36, s * .937, .78), .075, .014, 'y', M_PLAST, 36, .003)
    patch(n + '_HandleCup', rnd([(.53, 1.045), (.67, 1.045), (.67, 1.115), (.53, 1.115)], .025, 4), 'y', a, b, M_PLAST, -.002, .008)
    box(n + '_Handle', (.605, s * 1.044, 1.082), (.15, .018, .030), M_PLAST, .008, 3); box(n + '_Handle_P0', (.54, s * 1.032, 1.082), (.02, .028, .022), M_PLAST, .004); box(n + '_Handle_P1', (.665, s * 1.032, 1.082), (.02, .028, .022), M_PLAST, .004)
    for i, z in enumerate((.78, 1.10)): box(n + '_Hinge%d' % i, (1.555 + .07 * i, s * .955, z), (.07, .030, .07), M_DARK, .004)
    # the mirror: arm, shell, two glasses
    box(n + '_MirrorFoot', (1.595, s * 1.010, 1.29), (.09, .06, .10), M_PLAST, .018, 3); box(n + '_MirrorArm', (1.59, s * 1.075, 1.30), (.06, .11, .07), M_PLAST, .02, 3); box(n + '_MirrorShell', (1.60, s * 1.180, 1.42), (.115, .20, .36), M_PLAST, .04, 4)
    box(n + '_MirrorGlass', (1.5425, s * 1.180, 1.475), (.004, .168, .20), M_MIR, .001, 1); box(n + '_MirrorGlass2', (1.5425, s * 1.180, 1.315), (.004, .168, .09), M_MIR, .001, 1)
    side = 'L' if s > 0 else 'R'
    piv(n, (1.705, s * 1.015, 1.0)); ik('Handle_%s_Out' % n, (.605, s * 1.053, 1.082)); ik('Handle_%s_In' % n, (1.27, s * .915, 1.105)); ik('Step_%s' % n, (1.0, s * .86, .415)); ik('Stand_%s' % n, (.80, s * 1.65, 0))
    tube('GrabHandle_' + side, [(1.22, s * .80, 1.82), (1.19, s * .75, 1.85), (1.06, s * .75, 1.965), (1.03, s * .80, 2.0)], .012, M_PLAST)
    for e, p in enumerate(((1.22, s * .825, 1.82), (1.03, s * .835, 2.0))): box('GrabHandle_%s_F%d' % (side, e), p, (.05, .09, .05), M_PLAST, .012)
    ik('Grab_' + side, (1.125, s * .75, 1.91))
patch('SlideDoor_Trim', grow(SLIDE, -.028), 'y', -1.3, -.3, M_TRIM, .038, .058, hole=grow(WS, .012))
box('SlideDoor_PullIn', (.20, -.930, 1.12), (.035, .035, .24), M_PLAST, .012, 3)
for e in (-.10, .10): box('SlideDoor_PullIn_F%d' % (e > 0), (.20, -.948, 1.12 + e), (.03, .05, .03), M_PLAST, .006)
patch('SlideDoor_HandleCup', rnd([(.19, 1.045), (.33, 1.045), (.33, 1.115), (.19, 1.115)], .025, 4), 'y', -1.3, -.3, M_PLAST, -.002, .008)
box('SlideDoor_Handle', (.255, -1.044, 1.082), (.15, .018, .030), M_PLAST, .008, 3); box('SlideDoor_Handle_P0', (.19, -1.032, 1.082), (.02, .028, .022), M_PLAST, .004); box('SlideDoor_Handle_P1', (.32, -1.032, 1.082), (.02, .028, .022), M_PLAST, .004)
box('SlideDoor_Arm', (-.845, -.995, RAILZ), (.10, .030, .020), M_STEEL, .003); cyl('SlideDoor_Roller', (-.815, -.994, RAILZ), .013, .018, 'z', M_PLAST, 20)
box('SlideRail', (-1.60, -.982, RAILZ), (1.46, .012, .042), M_STEEL, .002, 1); box('SlideRail_Lip', (-1.60, -1.0175, RAILZ + .0185), (1.46, .010, .006), M_STEEL, .001, 1)
box('SlideRail_Low', (-.26, -.945, .365), (1.20, .05, .03), M_STEEL, .004)
piv('SlideDoor', (-.255, -.985, RAILZ)); ik('Handle_SlideDoor_Out', (.255, -1.053, 1.082)); ik('Handle_SlideDoor_In', (.20, -.905, 1.12)); ik('Step_SlideDoor', (-.10, -.84, .415)); ik('Stand_SlideDoor', (.05, -1.70, 0))
for s, sn in ((1, 'L'), (-1, 'R')):
    n = 'RearDoor' + sn
    full = rnd([(s * .03, .575), (s * (DW - .03), .575), (s * (DW - .03), 2.295), (s * .03, 2.295)], .04)
    patch(n + '_Trim', full, 'x', -3.2, -2.78, M_TRIM, .038, .056, hole=grow(WR[s], .012))
    for i, z in enumerate((.74, 1.28, 2.18)): box(n + '_Hinge%d' % i, (-2.990, s * (DW - .03), z), (.028, .14, .06), M_DARK, .006); cyl('RearPin_%s%d' % (sn, i), (-2.992, s * (DW + .045), z), .012, .075, 'z', M_STEEL, 20)
    piv(n, (-2.992, s * (DW + .045), 1.40)); ik('Stand_' + n, (-3.75, s * .55, 0))
    box(n + '_PullIn', (-2.895, s * .10, 1.15), (.03, .03, .20), M_PLAST, .01)
    for e in (-.08, .08): box(n + '_PullIn_F%d' % (e > 0), (-2.916, s * .10, 1.15 + e), (.04, .028, .03), M_PLAST, .006)
box('RearDoorR_HandleCup', (-2.990, -.085, 1.225), (.014, .16, .07), M_PLAST, .004); box('RearDoorR_Handle', (-3.004, -.085, 1.225), (.020, .13, .028), M_PLAST, .006); ik('Handle_RearDoorR_Out', (-3.016, -.085, 1.225)); ik('Handle_RearDoorL_Edge', (-2.97, .02, 1.22))
box('WorkLamp_Base', (-2.945, 0, 2.395), (.05, .20, .055), M_PLAST, .008); box('WorkLamp_Lens', (-2.968, 0, 2.393), (.014, .17, .036), M_WHT, .004)
box('StopLamp_Housing', (-2.885, 0, 2.442), (.05, .28, .032), M_PLAST, .004); box('TailBrake_C_Lens', (-2.909, 0, 2.442), (.010, .25, .020), M_RED, .002)
# #endregion

# #region ---- cab: dashboard, wheel, pedals, seats, belts
da = rawprism('Dashboard', rnd([(1.745, .86), (1.38, .90), (1.28, 1.00), (1.26, 1.12), (1.30, 1.245), (1.46, 1.30), (1.745, 1.315)], [.02, .06, .05, .05, .05, .06, .02], 4), 'y', -1.2, 1.2); boole(da, LF(.0395), 'INTERSECT')
c = rawprism('c', [(1.10, .5), (1.9, .5), (1.9, 1.6), (1.10, 1.6)], 'y', -.895, .895); boole(da, c, 'INTERSECT', False); K.finish(da, M_DASH, 32)
box('Dash_Binnacle', (1.36, .42, 1.295), (.24, .40, .12), M_DASH, .04, 4); box('Dash_Cluster', (1.2395, .42, 1.295), (.010, .32, .085), M_SCR, .004); ik('Cluster', (1.23, .42, 1.295))
box('Dash_Centre', (1.265, 0, 1.085), (.06, .40, .33), M_PLAST, .015, 3); box('Dash_Screen', (1.2325, 0, 1.175), (.010, .235, .135), M_SCR, .003)
for i, y in enumerate((-.085, 0, .085)): cyl('Dash_Knob%d' % i, (1.232, y, 1.04), .024, .03, 'x', M_DARK, 28, .004)
for i in range(6): box('Dash_Btn%d' % i, (1.2335, -.125 + .05 * i, .975), (.010, .036, .022), M_ORG if i == 3 else M_DARK, .003)
for i, y in enumerate((-.80, -.165, .165, .80)):
    box('Dash_Vent%d' % i, (1.285, y, 1.175), (.07, .10 if abs(y) < .5 else .12, .10), M_PLAST, .012)
    for k in range(4): box('Dash_Vent%d_S%d' % (i, k), (1.2485, y, 1.142 + .022 * k), (.010, .08, .007), M_DARK, .001, 1)
box('Dash_Glove', (1.262, -.50, 1.02), (.03, .42, .17), M_DASH, .012); box('Dash_GloveCatch', (1.2455, -.50, 1.085), (.010, .10, .022), M_PLAST, .004)
box('Dash_Shelf', (1.45, -.50, 1.296), (.22, .36, .03), M_PLAST, .01)
box('Console', (1.245, 0, .80), (.22, .30, .26), M_DASH, .03, 3); box('GearSelect_Base', (1.135, .07, .89), (.02, .07, .07), M_PLAST, .008); cyl('GearSelect', (1.115, .07, .89), .026, .04, 'x', M_CHROME, 32, .006)
ik('GearSelect', (1.09, .07, .89))
for e in (-1, 1): cyl('CupHolder%d' % (e > 0), (1.22, e * .075, .925), .042, .02, 'z', M_PLAST, 32, .003)
# steering wheel: the axis leans 45 degrees, as the belt loader's and the tractor's (GSE turns it about (-0.7071, 0, 0.7071))
SC = Vector((1.085, .42, 1.175)); AXW = Vector((-.7071, 0, .7071))
def tilt(o): o.data.transform(Matrix.Translation(SC) @ AXW.to_track_quat('Z', 'Y').to_matrix().to_4x4()); o.data.update(); return o
tilt(torus('SteeringWheel_Rim', (0, 0, 0), .195, .0155, 'z', M_PLAST, 64, 16)); tilt(cyl('SteeringWheel_Boss', (0, 0, -.012), .070, .045, 'z', M_PLAST, 40, .012)); tilt(cyl('SteeringWheel_Cap', (0, 0, .012), .038, .012, 'z', M_DASH, 32, .004))
for i, a in enumerate((90, 210, 330)):
    q = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0)); tilt(tube('SteeringWheel_Spoke%d' % i, [tuple(q * .05 + Vector((0, 0, -.015))), tuple(q * .12 + Vector((0, 0, -.008))), tuple(q * .193)], .013, M_PLAST))
tube('SteerColumn', [tuple(SC - AXW * .035), tuple(SC - AXW * .30)], .032, M_PLAST, bez=False); tilt(box('SteerShroud', (0, 0, -.10), (.12, .13, .11), M_PLAST, .025, 3))
for s, n in ((1, 'Ind'), (-1, 'Wip')): tilt(tube('Stalk' + n, [(0, s * .05, -.075), (0, s * .13, -.055), (0, s * .165, -.05)], .008, M_PLAST, bez=False))
piv('SteeringWheel', tuple(SC)); RQ = AXW.to_track_quat('Z', 'Y').to_matrix()
for s, n in ((1, 'L'), (-1, 'R')): ik('Hand_Wheel_' + n, tuple(SC + Vector((0, s * .195, 0))))
for n, y, w, zz in (('Accel', .30, .055, .70), ('Brake', .43, .085, .71)):
    turn(box('Pedal_%s_Pad' % n, (1.475, y, zz), (.018, w, .10), M_SEAL, .004), (1.475, y, zz), 'y', -25); tube('Pedal_%s_Arm' % n, [(1.478, y, zz), (1.53, y, zz + .10), (1.60, y, .93)], .010, M_DARK); ik('Foot_' + n, (1.462, y, zz)); piv('Pedal_' + n, (1.60, y, .93))
turn(box('FootRest', (1.50, .575, .66), (.02, .075, .20), M_SEAL, .005), (1.50, .575, .66), 'y', -35); ik('Foot_Rest', (1.485, .575, .66)); box('FootRest_B', (1.56, .575, .61), (.11, .05, .08), M_PLAST, .01)
box('CabMirror_Arm', (1.490, 0, 2.0), (.070, .03, .06), M_PLAST, .008); box('CabMirror', (1.450, 0, 1.972), (.03, .24, .075), M_PLAST, .012, 3); box('CabMirror_MirrorGlass', (1.434, 0, 1.972), (.004, .215, .055), M_MIR, .001, 1)
for s, n in ((1, 'L'), (-1, 'R')):
    box('SunVisor_' + n, (1.10, s * .36, 2.176), (.19, .44, .022), M_LIN, .008, 3); cyl('SunVisor_%s_Rod' % n, (1.190, s * .36, 2.180), .008, .46, 'y', M_PLAST, 16)
    for e in (.16, .56): box('SunVisor_%s_F%d' % (n, e > .3), (1.192, s * e, 2.205), (.04, .03, .07), M_PLAST, .006)
def seat(n, x, y, w, base=True, depth=.50, back=.66, zc=.94):
    """cushion top about zc + 0.07 at the front; backrest leaning 14 degrees; head restraint on two posts."""
    cu = box(n + '_Cushion', (x, y, zc), (depth, w, .15), M_SEAT, .05, 4, cuts=4)
    for v in cu.data.vertices:
        if v.co.z > zc: v.co.z += .022 * abs((v.co.y - y) / (w / 2)) ** 3 - .012 * K.ss(0, 1, 1 - abs((v.co.x - x) / (depth / 2)))
    for s in (-1, 1): box(n + '_Bolster%d' % (s > 0), (x, y + s * (w / 2 - .035), zc + .055), (depth - .08, .06, .06), M_SEAT2, .025, 3)
    bx = x - depth / 2 - .03; bk = box(n + '_Back', (bx, y, zc + .05 + back / 2), (.13, w, back), M_SEAT, .05, 4, cuts=4); parts = [bk]
    for s in (-1, 1): parts.append(box(n + '_BackBolster%d' % (s > 0), (bx + .055, y + s * (w / 2 - .04), zc + .05 + back * .48), (.07, .07, back * .70), M_SEAT2, .028, 3))
    parts.append(box(n + '_BackShell', (bx - .055, y, zc + .04 + back / 2), (.05, w - .04, back - .04), M_PLAST, .02, 3))
    for s in (-1, 1): parts.append(cyl(n + '_Post%d' % (s > 0), (bx, y + s * .055, zc + .05 + back + .035), .007, .14, 'z', M_STEEL, 14))
    parts.append(box(n + '_Head', (bx + .005, y, zc + .05 + back + .145), (.11, .27, .18), M_SEAT, .045, 4))
    for o in parts: turn(o, (bx, y, zc), 'y', -12)
    box(n + '_Pan', (x, y, zc - .085), (depth - .04, w - .05, .05), M_PLAST, .015)
    if base: box(n + '_Box', (x - .02, y, (zc - .10 + FLOOR) / 2), (depth - .10, w - .10, zc - .10 - FLOOR + .01), M_PLAST, .02)
    for s in (-1, 1):
        if abs(s) : break
    b = 1 if y <= 0 else -1                                                  # the buckle: on the inner side
    box(n + '_BuckleStem', (x - depth / 2 + .04, y + b * (w / 2 + .012), zc + .03), (.03, .014, .15), M_PLAST, .004); box(n + '_Buckle', (x - depth / 2 + .04, y + b * (w / 2 + .012), zc + .13), (.045, .024, .06), M_PLAST, .006); box(n + '_BuckleRed', (x - depth / 2 + .04, y + b * (w / 2 + .012), zc + .161), (.026, .014, .008), M_ORG, .002)
    ik('Seat_' + n, (x - depth / 2 + .14, y, zc + .08)); ik('Buckle_' + n, (x - depth / 2 + .04, y + b * (w / 2 + .012), zc + .17))
seat('SeatDriver', .74, .42, .50); seat('SeatPassenger', .74, -.46, .48)
box('SeatDriver_Lever', (.80, .672, .87), (.22, .05, .05), M_PLAST, .01)
for i, y in enumerate((-.53, 0, .53)): seat('SeatRow%d' % i, -.36, y, .46, base=False)
box('RowBench', (-.36, 0, .725), (.36, 1.44, .06), M_DARK, .008)
for i, y in enumerate((-.60, 0, .60)):
    box('RowLeg%d' % i, (-.36, y, .645), (.30, .06, .15), M_DARK, .008)
    box('RowPan%d' % i, (-.36, (-.53, 0, .53)[i], .79), (.30, .30, .10), M_DARK, .008)
for n, x, y, z0 in (('D', .50, .865, 1.93), ('P', .50, -.865, 1.93), ('R0', -.83, -.80, 1.90), ('R2', -.83, .80, 1.90)):   # belts on the pillars
    s = 1 if y > 0 else -1; xx = .42 if n in 'DP' else -.895
    box('Belt%s_Top' % n, (xx, s * .895, z0), (.05, .05, .07), M_PLAST, .01); box('Belt%s_Web' % n, (xx, s * .885, (z0 + .75) / 2), (.046, .006, z0 - .75), M_BELT, .001, 1); box('Belt%s_Reel' % n, (xx, s * .905, .74), (.10, .09, .14), M_PLAST, .015); box('Belt%s_Tongue' % n, (xx, s * .878, 1.15), (.05, .014, .06), M_PLAST, .004)
    ik('BeltTongue_' + n, (xx, s * .87, 1.15))
for s, n in ((1, 'L'), (-1, 'R')): patch('BPillarTrim_' + n, [(.385, .62), (.455, .62), (.455, 2.15), (.385, 2.15)], 'y', s * .5, s * 1.2, M_TRIM, .037, .050)
patch('CrewTrimL', [(-.915, .62), (.375, .62), (.375, 1.22), (-.915, 1.22)], 'y', .5, 1.2, M_TRIM, .037, .058)
# wipers on the windscreen (laid on the glass: the skin's own place there)
def surf(y, z, o=0.0): return Vector((ip(XF, z) - ip(LN, z) + ip(LN, z) * max(1 - (abs(y) / ip(WH, z)) ** 3.4, 0) ** (1 / 3.4) + o, y, z))
for s, n in ((1, 'L'), (-1, 'R')):
    py = .62 if s > 0 else -.10; pv = surf(py, 1.362, .004); piv('Wiper_' + n, tuple(pv)); cyl('Wiper%s_Hub' % n, tuple(pv), .022, .06, 'z', M_PLAST, 20, .004)
    tube('Wiper%s_Arm' % n, [tuple(surf(py, 1.375, .018)), tuple(surf(py - .14, 1.40, .026)), tuple(surf(py - .30, 1.425, .016))], .008, M_PLAST)
    tube('Wiper%s_Blade' % n, [tuple(surf(py - .02, 1.408, .002))] + [tuple(surf(py - .02 - .08 * k, 1.408 + .0035 * k, .002)) for k in range(1, 8)], .010, M_SEAL)
# roof: beacon, aerial, cooling vent
cyl('Beacon_Base', (.25, 0, 2.532), .085, .05, 'z', M_PLAST, 40, .006); cl = CK.lathez('BeaconLens', [(0, 2.545), (.074, 2.545), (.078, 2.56), (.076, 2.625), (.066, 2.655), (.04, 2.672), (0, 2.675)], (.25, 0, 0), M_AMB, 48); cyl('Beacon_Cap', (.25, 0, 2.674), .03, .012, 'z', M_PLAST, 24, .003); ik('Beacon', (.25, 0, 2.61))
box('RoofVent', (-1.75, 0, 2.538), (.42, .42, .05), M_PLAST, .02, 3); tube('Aerial', [(.55, -.45, 2.44), (.45, -.45, 2.62), (.36, -.45, 2.78)], .004, M_PLAST); cyl('Aerial_Base', (.55, -.45, 2.448), .02, .03, 'z', M_PLAST, 20)
# #endregion

# #region ---- load space: shelves on the left, bag cage and cart on the right
SX0, SX1 = -2.80, -1.00; SY0, SY1 = .47, .855
for i, x in enumerate((SX0 + .02, -1.90, SX1 - .02)):
    for j, y in enumerate((SY0 + .018, SY1 - .018)):
        z0 = FLOOR if j == 0 else (.55 if i == 1 else FLOOR); box('Rack_U%d%d' % (i, j), (x, y, (z0 + 2.12) / 2), (.036, .036, 2.12 - z0), M_STEEL, .003)
SHZ = (.86, 1.24, 1.62, 2.0)
for i, z in enumerate(SHZ):
    box('Rack_Shelf%d' % i, ((SX0 + SX1) / 2, (SY0 + SY1) / 2, z), (SX1 - SX0, SY1 - SY0, .020), M_STEEL, .003); box('Rack_Lip%d' % i, ((SX0 + SX1) / 2, SY0 - .004, z + .035), (SX1 - SX0, .010, .075), M_STEEL, .003)
    for e, x in enumerate((SX0 + .004, SX1 - .004)): box('Rack_End%d%d' % (i, e), (x, (SY0 + SY1) / 2, z + .035), (.010, SY1 - SY0, .075), M_STEEL, .003)
CG = (-2.02, -1.00, -.865, -.40)                                             # the full bags' cage x0 x1 y0 y1
for i, (x, y) in enumerate(((CG[0], CG[3]), (CG[1], CG[3]), ((CG[0] + CG[1]) / 2, CG[3]))): tube('Cage_P%d' % i, [(x, y, FLOOR), (x, y, 1.50)], .016, M_STEEL, bez=False)
for i, z in enumerate((.66, .90, 1.14, 1.38, 1.495)): tube('Cage_R%d' % i, [(CG[0], CG[3], z), (CG[1], CG[3], z)], .011, M_STEEL, bez=False); tube('Cage_E%d' % i, [(CG[0], CG[3], z), (CG[0], -.952, z)], .011, M_STEEL, bez=False)
tube('CargoRail', [(-2.78, -.895, 1.02), (-2.10, -.895, 1.02)], .014, M_STEEL, bez=False)
for e, x in enumerate((-2.74, -2.14)): box('CargoRail_F%d' % e, (x, -.935, 1.02), (.04, .09, .04), M_STEEL, .004)
box('CartStrap', (-2.45, -.49, .70), (.05, .012, .02), M_BELT, .001, 1) if False else None
box('VacDock', (-.985, -.18, 1.68), (.07, .50, .05), M_PLAST, .01); box('VacDock_Board', (-.953, -.18, 1.56), (.022, .60, .50), M_PLY, .004)
for i, y in enumerate((-.32, -.04)): box('VacDock_H%d' % i, (-1.045, y, 1.68), (.06, .06, .05), M_PLAST, .01)
KM = CK.mats(); KIT = {}
for n, o in CK.make_all(KM).items():
    ALL.remove(o); KIT[n] = o; o.data.name = 'Kit_' + n; o.hide_render = True; o.hide_set(True)
STOCK = []; STOCKP = {}
def stock(n, loc, rz=0.0, ry=0.0):
    i = len([1 for k in STOCKP if k.startswith('Stock_' + n + '_')]); nm = 'Stock_%s_%d' % (n, i); o = bpy.data.objects.new(nm, KIT[n].data); bpy.context.collection.objects.link(o)
    o.location = loc; o.rotation_euler = (0, math.radians(ry), math.radians(rz)); STOCK.append(o); STOCKP[nm] = [list(loc), rz, ry]
YM = (SY0 + SY1) / 2
for i, x in enumerate((-2.56, -2.02, -1.44)): stock('Caddy', (x, YM, SHZ[1] + .0115 + .278))
for i, x in enumerate((-2.68, -2.50, -2.32, -2.14)): stock('BagRoll', (x, YM, SHZ[2] + .0115), 180)
for i, x in enumerate((-1.72, -1.54)): stock('BagRoll', (x, YM, SHZ[2] + .0115), 180)
for i, (x, y) in enumerate(((-1.28, -.655), (-1.755, -.66))): stock('BagFull', (x, y, FLOOR + .012 + .556), 40 * i)
stock('CleanCart', (-2.44, -.62, FLOOR + .0105), 0)
for i, y in enumerate((-.32, -.04)): stock('Vacuum', (-1.135, y, 1.78), 180, 34.6)   # (its wand upright along the bulkhead)
for i, (x, z) in enumerate(((-2.55, SHZ[0]), (-1.95, SHZ[0]), (-1.35, SHZ[0]), (-2.5, SHZ[3] - 0), (-1.35, SHZ[3]))):     # tubs: blankets, head-rest covers, cloths
    hh = .24 if z < 1.9 else .20
    t = box('Tub%d' % i, (x, YM, z + .0115 + hh / 2), (.46, .30, hh), KM['grey'], .02, 2); cut(t, box('c', (x, YM, z + .03 + hh / 2), (.43, .27, hh), KM['grey'], .015))
    for k in range(3): box('Tub%d_Fold%d' % (i, k), (x, YM, z + .022 + .05 * k + .025), (.40, .24, .051), KM[('cloth1', 'white', 'cloth2')[i % 3]], .014, 2)
# #endregion

# #region ---- the markings: MAINTENANCE, raised navy letters on a white panel (drawn mirrored: the loader mirrors x)
def word(name, body, size, c, axis, s, target, d0=-.0008, d1=.0016):
    """Letters centred at c (the face's two other coordinates), laid onto target by rays along -s*axis; mirrored as seen."""
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = body; cu.size = size; cu.extrude = .5; cu.align_x = 'CENTER'; cu.align_y = 'CENTER'; cu.resolution_u = 4
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    for x in bpy.context.selected_objects: x.select_set(False)
    bpy.context.view_layer.objects.active = o; o.select_set(True); bpy.ops.object.convert(target='MESH'); o = bpy.context.view_layer.objects.active; o.select_set(False)
    tb = K.bvh(target); i = 'xyz'.index(axis); n = Vector((0, 0, 0)); n[i] = s; miss = 0
    for v in o.data.vertices:
        u, h, w = v.co.x, v.co.y, (v.co.z + .5)          # (w 0 .. 1: back .. front)
        p = Vector((0, 0, 0)); p[2] = c[1] + h
        if axis == 'y': p[0] = c[0] + s * u           # (as seen from outside the letters run to the left: mirrored)
        else: p[1] = c[0] - s * u
        p[i] = s * 9.0; hit = tb.ray_cast(p, -n, 20.0)
        if hit[0] is None: miss += 1; continue
        v.co = hit[0] + n * (d0 + (d1 - d0) * w)
    assert not miss, '%s: %d letters off the panel' % (name, miss)
    bm = bmesh.new(); bm.from_mesh(o.data); bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bm.to_mesh(o.data); bm.free(); o.data.update()
    return K.finish(o, M_WORD, 20)
for s, sn in ((1, 'L'), (-1, 'R')):
    pn = patch('MaintPanel_' + sn, rnd([(-2.62, 1.24), (-1.06, 1.24), (-1.06, 1.40), (-2.62, 1.40)], .03, 3), 'y', s * .80, s * 1.3, M_NAME, -.003, .006)
    word('MaintWord_' + sn, 'MAINTENANCE', .15, (-1.84, 1.32), 'y', s, pn)
pr = patch('RearDoorR_MaintPanel', rnd([(-.70, 2.115), (-.06, 2.115), (-.06, 2.265), (-.70, 2.265)], .02, 3), 'x', -3.2, -2.86, M_NAME, -.003, .006)
word('RearDoorR_MaintWord', 'MAINTENANCE', .072, (-.38, 2.19), 'x', -1, pr)
# #endregion

# #region ---- points for the code
for s, n in ((1, 'L'), (-1, 'R')): ik('Name_' + n, (-1.84, s * (ip(WH, 1.66) + .004), 1.66))
ik('Name_Rear', (-2.966, .38, 2.19)); ik('Driver_Eye', (.60, .42, 1.72)); ik('Cart_Out', (-3.9, -.62, 0)); ik('Tow', (-3.05, 0, .46))
for i, y in enumerate((-.53, 0, .53)): ik('Aisle_Row%d' % i, (.12, y, FLOOR + .01))
# #endregion

K.uv_cube()
for o in KIT.values():
    bpy.context.view_layer.objects.active = o
for o in list(LOFTS.values()): rm(o)
if '--nochk' not in sys.argv:
    K.checks('VAN', ground_ok=('Tyre',), overhang_skip_prefix=('',))
    for nm, pv, ax, dg, sk in (('DoorDriver', (1.705, 1.015, 1), 'z', (-15, -40, -68), ('Tyre_FL',)), ('DoorPassenger', (1.705, -1.015, 1), 'z', (15, 40, 68), ()), ('RearDoorL', (-2.992, DW + .045, 1.4), 'z', (-30, -90), ()), ('RearDoorR', (-2.992, -DW - .045, 1.4), 'z', (30, 90), ())):
        h = K.sweep(nm, pv, ax, dg, sk); print('VAN SWEEP', nm, len(h), sorted(h)[:10])
    bv = [(o.name, K.bvh(o)) for o in ALL if not o.name.startswith('SlideDoor')]; hits = set()
    for sh in ((-.04, -.125, 0), (-.6, -.125, 0), (-1.30, -.125, 0)):
        for o in ALL:
            if o.name.startswith('SlideDoor'):
                from mathutils.bvhtree import BVHTree
                t = BVHTree.FromPolygons([v.co + Vector(sh) for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
                for n, b in bv:
                    if t.overlap(b): hits.add((o.name, n, sh[0]))
    print('VAN SLIDE', len(hits), sorted(hits)[:12])
ALL.extend(STOCK)
if '--pic' in sys.argv:
    OUT = sys.argv[sys.argv.index('--pic') + 1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.use_gtao = True; sc.eevee.taa_render_samples = 12
    w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (.6, .68, .8, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = .9; sc.world = w
    K.glass(M_GLASS, .25); M_GLASS.blend_method = 'BLEND'
    sd = bpy.data.lights.new('S', 'SUN'); sd.energy = 4; so = bpy.data.objects.new('S', sd); sc.collection.objects.link(so); so.rotation_euler = (.8, .1, .6)
    cd = bpy.data.cameras.new('C'); bc = bpy.data.objects.new('C', cd); sc.collection.objects.link(bc); sc.camera = bc; sc.render.resolution_x, sc.render.resolution_y = 900, 600
    def shot(n, loc, at, lens=40):
        bc.location = loc; bc.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens; cd.clip_start = .02; sc.render.filepath = os.path.join(OUT, n + '.png'); bpy.ops.render.render(write_still=True)
    shot('a1', (7.2, -5.6, 2.3), (0, 0, 1.15)); shot('a2', (-7.0, 5.4, 2.2), (0, 0, 1.2))
    def mv(pre, pv, deg=0, sh=(0, 0, 0)):
        for o in ALL:
            if o.name.startswith(pre): o.matrix_world = Matrix.Translation(Vector(pv) + Vector(sh)) @ Matrix.Rotation(math.radians(deg), 4, 'Z') @ Matrix.Translation(-Vector(pv)) @ o.matrix_world
    mv('DoorDriver', (1.705, 1.015, 1), -68); mv('DoorPassenger', (1.705, -1.015, 1), 68); mv('SlideDoor', (0, 0, 0), 0, (-1.30, -.125, 0)); mv('RearDoorL', (-2.992, DW + .045, 1), -100); mv('RearDoorR', (-2.992, -DW - .045, 1), 100)
    bpy.context.view_layer.update()
    shot('a3', (-6.0, -2.6, 2.2), (-1.6, 0, 1.2), 32); shot('a4', (2.6, -4.6, 1.9), (0.1, 0, 1.2), 32); shot('b1', (.42, .42, 1.78), (1.6, .10, 1.10), 18); shot('b2', (2.2, 4.0, 1.7), (.6, 0, 1.1), 32)
    for o in ALL:
        if o.name == 'Body': o.hide_render = True
    shot('b3', (1.6, 4.6, 3.6), (-.6, 0, .9), 30); shot('b4', (-2.2, -4.2, 2.6), (-2.0, 0, 1.1), 34)
    sys.exit(0)
# the flicker rounds, then (few components) what belongs together becomes one mesh per moving group, then a last check
for m in KM.values(): pass
for o in STOCK: assert all(o.data.materials)
GLB = os.path.join(ART, 'maintenancevan.glb'); ZF = os.path.join(ART, 'zfight-check.json')
for n, o in KIT.items(): o.hide_set(False); bpy.data.objects.remove(o)
last = K.save_export(ROOT, os.path.join(ART, 'MaintenanceVan.blend'), GLB, ZF, 'VAN', 'MaintenanceVan')
GROUPS = ['DoorDriver', 'DoorPassenger', 'SlideDoor', 'RearDoorL', 'RearDoorR', 'SteeringWheel']
KEEP = ('Tyre_', 'Rim_', 'WheelDisc_', 'Hub_', 'HubCap_', 'Nut_', 'Spindle_', 'SteerArm_', 'Disc_F', 'Wiper', 'Pedal_', 'Stock_', 'Body', 'Hood')
def groupof(o):
    n = o.name
    if n.startswith(KEEP) or n.endswith('_Lens') or n == 'BeaconLens' or 'NamePanel' in n: return None
    g = next((g for g in GROUPS if n.startswith(g)), None)
    if g: return g + ('_Glass' if n.endswith('_Glass') else '_Parts' if n != g else None or '')
    if n.endswith('_Glass') : return 'GlassFixed'
    if n.startswith(('Seat', 'Row', 'Belt')): return 'Seats'
    if n.startswith(('Dash', 'Console', 'CabM', 'SunV', 'Grab', 'Steer', 'Stalk', 'Foot', 'Cup', 'Gear', 'Firewall', 'Cowl')): return 'CabFittings'
    if n.startswith(('Rack', 'Cage', 'Cargo', 'Vac', 'Tub', 'Rib', 'Ply', 'RoofLight')): return 'LoadFittings'
    if n.startswith(('Arch', 'Rub', 'HiVis', 'Headlamp', 'RearLamp', 'Grille', 'Charge', 'Lower', 'Fog', 'Plate', 'Rear', 'Reflector', 'Sill', 'Slide', 'Work', 'Stop', 'Beacon', 'Roof', 'Aerial', 'Maint')): return 'OuterFittings'
    if n.startswith(('Frame', 'Leaf', 'Battery', 'Front', 'EMotor', 'Sub', 'Strut', 'Wish')): return 'Underside'
    return 'InnerFittings'
G = {}
for o in ALL:
    g = groupof(o)
    if g and o.name != g: G.setdefault(g, []).append(o)
for g, l in G.items(): CK.join(g + ('_All' if g in GROUPS else ''), l)
for n, o in {o.name: o for o in ALL}.items():
    a = (len(o.data.polygons), len(o.data.materials)); assert o.data.materials and all(o.data.materials), n
    assert {p.material_index for p in o.data.polygons} == set(range(len(o.data.materials))), 'empty slot ' + n
last2 = K.save_export(ROOT, os.path.join(ART, 'MaintenanceVan.blend'), GLB, ZF, 'VAN joined', 'MaintenanceVanGame')
ue = lambda v: [round(v[0] * 100, 1), round(-v[1] * 100, 1), round(v[2] * 100, 1)]
tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL)
json.dump({'note': 'points in the model root, Unreal cm (Blender (x,y,z) m = (x,-y,z) * 100)', 'size_m': [L, W, H], 'wheelbase': WB, 'tyre_radius_cm': RW * 100, 'points_cm': {e.name: ue(e.location) for e in IKS},
           'parts': sorted(o.name for o in ALL), 'materials': sorted({m.name for o in ALL for m in o.data.materials}), 'stock': STOCKP, 'tris': tris, 'flicker': last2}, open(os.path.join(ART, 'maintenancevan-parts.json'), 'w'), indent=1)
print('DONE VAN: %d parts, %d points, %d tris, %s' % (len(ALL), len(IKS), tris, last2))
for o in ALL:
    a, b = K.bbox(o)
    if o.name in ('Body', 'DoorDriver', 'SlideDoor', 'RearDoorL'): print('VAN BOX', o.name, [round(v, 3) for v in a], [round(v, 3) for v in b])

