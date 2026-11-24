# The airport taxi bus (to-do 239, part A: the model): the cabin cleaning van's own body, wheels and cab, copied (Art/CleaningVan
# stays as it is), made into an 8-passenger airport taxi bus, our own shape, no brand. Same published L3H2 sizes as the van:
# length 5.998, width 2.050, height 2.522, wheelbase 4.035, tracks 1.810 / 1.790, tyres 225/75 R16, side door 1.25 x 1.75.
# Changes from the van: no beacon, stickers, hi-vis marking, plates, cleaning kit or shelves; a lit taxi sign on the roof;
# seats built as the coach's (Tools/make_coach_seat.py: rounded quad blocks with a waterfall cushion, bolstered backrest
# reclined 15 deg, back shell, headrest on posts, belts) for the driver + 8: front passenger, rows 1 and 2 two seats on the
# left with the aisle on the right at the sliding door, row 3 three across; side windows along the passenger room; a grab
# bar at the sliding door's step; a taximeter on the dash; the luggage space at the rear doors; a plain made-up livery.
# Axes: x forward, y left, z up; metres; origin on the ground under the middle. Driver front left.
# The body is one lofted skin (skin(a, b) = the space between two offsets of it); every panel, door, pane and strip is cut
# out of that same skin, so a door always follows the body.
#   blender -b --python Art/TaxiBus/make_taxibus.py [-- --pic]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools')]
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ALL, IKS
ART = os.path.join(ROOT, 'Art', 'TaxiBus'); TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures')
K.reset()
COACH = os.path.join(ROOT, 'Art', 'CoachSeat')                     # the coach seat's own fabric, shell and floor pictures
L, W, H, WB, TD = 5.998, 2.050, 2.522, 4.035, 0.744
FX, RX, TRF, TRR, RW = 2.051, -1.984, 1.810, 1.790, TD / 2
FLOOR = 0.575                                                   # the floor's top, cab and load space

# #region ---- materials
P = K.pmat
M_BODY = P('TB_Livery_Body_Paint', (0.86, 0.85, 0.80), 0.1, 0.32); M_MAIN = P('TB_Livery_Band_Paint', (0.95, 0.66, 0.03), 0.1, 0.32)   # made-up livery: ivory, amber band
M_BUMP = P('TB_BumperPlastic', (0.045, 0.045, 0.05), 0, 0.62); M_PLAST = P('TB_BlackPlastic', (0.035, 0.035, 0.04), 0, 0.55); M_DASH = P('TB_DashPlastic', (0.10, 0.105, 0.115), 0, 0.7)
M_TRIM = P('TB_GreyTrimPlastic', (0.33, 0.34, 0.36), 0, 0.75); M_LIN = P('TB_HeadLining', (0.62, 0.62, 0.60), 0, 0.9); M_LOAD = P('TB_RoomLining_Paint', (0.60, 0.61, 0.62), 0.05, 0.55)
M_GLASS = P('TB_Glass', (0.2, 0.3, 0.35), 0, 0.05); M_SEAL = P('TB_SealRubber', (0.02, 0.02, 0.02), 0, 0.85); M_TYRE = P('TB_Tyre', (0.03, 0.03, 0.03), 0, 0.9)
M_STEEL = P('TB_Steel', (0.5, 0.51, 0.53), 1, 0.4); M_DARK = P('TB_DarkSteel', (0.09, 0.09, 0.1), 0.6, 0.55); M_RIM = P('TB_RimSteel', (0.62, 0.63, 0.65), 0.9, 0.35)
M_FLOOR = P('TB_FloorMat', (0.05, 0.05, 0.05), 0, 0.9)
M_PLY = P('TB_LoadFloorPlate', (0.30, 0.29, 0.27), 0, 0.8); M_AMB = P('TB_AmberLens', (1.0, 0.5, 0.04), 0, 0.2, 1.0); M_RED = P('TB_RedLens', (0.85, 0.04, 0.03), 0, 0.2, 0.6)
M_WHT = P('TB_WhiteLens', (0.95, 0.95, 0.92), 0, 0.1, 1.0); M_CHROME = P('TB_Reflector', (0.8, 0.8, 0.82), 1, 0.12); M_MIR = P('TB_MirrorGlass', (0.8, 0.85, 0.9), 1, 0.02)
M_SCR = P('TB_Screen', (0.03, 0.10, 0.14), 0, 0.1, 0.4)
M_BELT = P('TB_BeltWebbing', (0.02, 0.02, 0.022), 0, 0.8); M_ORG = P('TB_BuckleRed', (0.7, 0.04, 0.02), 0, 0.45)
for m, t in ((M_STEEL, None), (M_DARK, (.25, .25, .27)), (M_RIM, (1.05, 1.06, 1.08))): K.photo(m, TEX, 'Metal027', tile=0.6, tint=t)
K.photo(M_TYRE, TEX, 'Rubber004', tile=0.5, tint=(.35, .35, .35), metal=0.0); K.photo(M_FLOOR, TEX, 'Rubber001', tile=0.5, tint=(.4, .4, .4), metal=0.0)
for m, t in ((M_BUMP, (.40, .40, .42)), (M_PLAST, (.32, .32, .34)), (M_DASH, (.75, .77, .8))): K.photo(m, TEX, 'Plastic006', tile=0.4, tint=t, metal=0.0)
def cmat(name, img, metal, rough, tile=0.25, emit=None, strength=0.0):
    """A material on the coach seat's own pictures (Art/CoachSeat/<img>.png + <img>_N.png), 1 picture per tile metres."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / tile, 1 / tile, 1)
    nt.links.new(tc.outputs['UV'], mp.inputs['Vector'])
    ti = nt.nodes.new('ShaderNodeTexImage'); ti.image = bpy.data.images.load(os.path.join(COACH, img + '.png'), check_existing=True)
    nt.links.new(mp.outputs[0], ti.inputs[0]); nt.links.new(ti.outputs['Color'], b.inputs['Base Color'])
    tn = nt.nodes.new('ShaderNodeTexImage'); tn.image = bpy.data.images.load(os.path.join(COACH, img + '_N.png'), check_existing=True); tn.image.colorspace_settings.name = 'Non-Color'
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6
    nt.links.new(mp.outputs[0], tn.inputs[0]); nt.links.new(tn.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
    b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit: b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    return m
M_MOQ = cmat('TB_SeatMoquette', 'T_Coach_Moquette', 0, 0.92); M_MOQC = cmat('TB_SeatMoquetteCentre', 'T_Coach_MoquetteCentre', 0, 0.90)
M_SHELLS = cmat('TB_SeatShell', 'T_Coach_Shell', 0, 0.45); M_ALU = cmat('TB_SeatCastAlu', 'T_Coach_CastAlu', 1, 0.38)
M_CSTEEL = cmat('TB_SeatSteel', 'T_Coach_Steel', 1, 0.30); M_CBLACK = cmat('TB_SeatBlackPlastic', 'T_Coach_BlackPlastic', 0, 0.50)
M_VINYL = cmat('TB_FloorVinyl', 'T_Coach_FloorVinyl', 0, 0.70, 0.5); M_CRUB = cmat('TB_SeatRubber', 'T_Coach_Rubber', 0, 0.85)
M_SIGN = P('TB_TaxiSign_Lens', (1.0, 0.80, 0.22), 0, 0.25, 3.0); M_SIGNT = P('TB_TaxiSign_Letters', (0.02, 0.02, 0.025), 0, 0.5)
M_METER = P('TB_Taximeter_Digits', (1.0, 0.10, 0.04), 0, 0.1, 2.0)
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
WP = [rnd([(-1.80, 1.30), (-1.03, 1.30), (-1.03, 1.98), (-1.80, 1.98)], .07), rnd([(-2.66, 1.30), (-1.90, 1.30), (-1.90, 1.98), (-2.66, 1.98)], .07)]   # the passenger room's side windows
for w in WP:                                                                # (sill 1.30: clear of the slide rail's groove and lip on the right)
    for s in (-1, 1): boole(shell, rawprism('c', w, 'y', s * .3, s * 1.3), 'DIFFERENCE', False)
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
for i, w in enumerate(WP): pane('RoomWindowL%d' % i, w, 'y', .3, 1.3); pane('RoomWindowR%d' % i, w, 'y', -1.3, -.3)
pane('SlideDoor_Window', WS, 'y', -1.3, -.3); pane('CrewWindowL', WS, 'y', .3, 1.3); pane('RearDoorL_Window', WR[1], 'x', -3.2, -2.7); pane('RearDoorR_Window', WR[-1], 'x', -3.2, -2.7)
# #endregion

# #region ---- skin fittings: arch lips, rub strips, lamps (no stickers, marking or plates on the taxi)
def arc(x, r, a0, a1, n=28): return [(x + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), RW + r * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]
for s, sn in ((1, 'L'), (-1, 'R')):
    a, b = (s * .80, s * 1.3)
    for k, x in (('F', FX), ('R', RX)): patch('ArchLip_%s%s' % (k, sn), arc(x, .476, 4, 176) + arc(x, .429, 176, 4), 'y', a, b, M_BUMP, -.010, .028)
    def strip(n, x0, x1, z0=.625, z1=.735): patch(n, rnd([(x0, z0), (x1, z0), (x1, z1), (x0, z1)], .02, 3), 'y', a, b, M_BUMP, -.012, .006, smooth=30)
    strip('DoorDriver_Rub' if s > 0 else 'DoorPassenger_Rub', .487, 1.53)
    if s > 0: strip('RubL_Mid', -1.47, .455)
    else: strip('SlideDoor_Rub', -.868, .357); strip('RubR_Mid', -1.47, -.903)
    strip('Rub%s_Rear' % sn, -2.80, -2.50)
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
for k in range(9):                                                          # the grille: slats over a black field
    pass
GR = rnd([(-.50, .66), (.50, .66), (.60, .88), (-.60, .88)], .05)
patch('Grille_Field', GR, 'x', 2.5, 3.2, M_PLAST, -.004, .02)
for i in range(5):
    z = .692 + .038 * i; hw = .47 + .10 * (z - .66) / .22; patch('Grille_Slat%d' % i, [(-hw, z), (hw, z), (hw, z + .016), (-hw, z + .016)], 'x', 2.5, 3.2, M_DARK, -.016, .002)
patch('ChargeFlap', rnd([(-.09, .925), (.09, .925), (.09, 1.0), (-.09, 1.0)], .02, 3), 'x', 2.6, 3.2, M_PLAST, -.004, .01)
patch('LowerGrille', rnd([(-.55, .36), (.55, .36), (.60, .47), (-.60, .47)], .03), 'x', 2.5, 3.2, M_PLAST, -.004, .02)
for s, sn in ((1, 'L'), (-1, 'R')): patch('FogBezel_' + sn, rnd([(s * .66, .40), (s * .80, .40), (s * .80, .50), (s * .66, .50)], .03, 3), 'x', 2.2, 3.2, M_PLAST, -.006, .02)
box('RearBumper', (-2.955, 0, .46), (.155, 1.78, .13), M_BUMP, .02, 3); box('RearBumper_Step', (-2.975, 0, .528), (.11, 1.45, .014), M_FLOOR, .004)
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

# #region ---- floor, steps, firewall, room trim, liner (no bulkhead: cab and passenger room are one)
fl = box('Floor', (-.615, 0, FLOOR - .03), (4.63, 1.968, .06), M_PLY, 0)
for c in ([(1.01, .86, .5), (1.06, .30, .5)], [(1.01, -.86, .5), (1.06, .30, .5)], [(-.255, -.86, .5), (1.18, .30, .5)]): cut(fl, box('c', *c, M_PLY))
for s in (-1, 1): cut(fl, box('c', (RX, s * .86, .6), (.90, .31, .5), M_PLY))
for n, x0, x1, y0, y1 in (('StepDriver', .485, 1.535, .715, .972), ('StepPassenger', .485, 1.535, -.972, -.715), ('StepSlide', -.84, .33, -.972, -.715)):
    xm, ym = (x0 + x1) / 2, (y0 + y1) / 2; s = 1 if ym > 0 else -1
    box(n, (xm, ym, .40), (x1 - x0, abs(y1 - y0), .03), M_FLOOR, .006); box(n + '_Riser', (xm, s * .709, .48), (x1 - x0 + .01, .014, .16), M_TRIM, .002, 1)
    for e, xx in ((0, x0), (1, x1)): box(n + '_End%d' % e, (xx, ym, .48), (.014, abs(y1 - y0), .16), M_TRIM, .002, 1)
box('CabMat', (.50, 0, FLOOR + .003), (2.35, 1.40, .012), M_FLOOR, .004)
box('RoomFloor', (-1.4675, 0, FLOOR + .003), (1.575, 1.395, .014), M_VINYL, .004)          # the passenger room: vinyl, x -2.255 .. -0.68
patch('FirewallSkin', [(-1.2, .50), (1.2, .50), (1.2, 1.335), (-1.2, 1.335)], 'x', 1.685, 1.700, M_PLAST, .036, 3.0) if False else None
fw = rawprism('Firewall', [(-1.2, .54), (1.2, .54), (1.2, 1.31), (-1.2, 1.31)], 'x', 1.722, 1.742); boole(fw, LF(.036), 'INTERSECT'); boole(fw, rawprism('c', [(-1.2, .50), (-.63, .50), (-.63, .86), (-1.2, .86)], 'x', 1.4, 2), 'DIFFERENCE', False); boole(fw, rawprism('c', [(1.2, .50), (.63, .50), (.63, .86), (1.2, .86)], 'x', 1.4, 2), 'DIFFERENCE', False); K.finish(fw, M_PLAST, 32)
cw = rawprism('Cowl', [(-1.2, 1.300), (1.2, 1.300), (1.2, 1.318), (-1.2, 1.318)], 'x', 1.715, 2.6); boole(cw, LF(.036), 'INTERSECT'); K.finish(cw, M_PLAST, 32)
for s, sn in ((1, 'L'), (-1, 'R')):                                         # the passenger room's side trim, stepped over the rear arch
    patch('RoomTrim' + sn, [(-.955, .62), (-1.50, .62), (-1.50, .86), (-2.50, .86), (-2.50, .62), (-2.78, .62), (-2.78, 1.22), (-.955, 1.22)], 'y', s * .5, s * 1.2, M_TRIM, .037, .058)
patch('CabLiner', [(-2.78, 2.20), (.98, 2.20), (.98, 2.6), (-2.78, 2.6)], 'y', -1.2, 1.2, M_LIN, .036, .052)
for i, (x, y) in enumerate(((-.45, 0), (-1.75, 0), (.45, 0))):              # ceiling lamps: the cab and the passenger room
    z = 2.466 if x < 0 else 2.395; box('CeilingLamp%d_Base' % i, (x, y, z), (.30, .10, .05), M_PLAST, .006); box('CeilingLamp%d_Lens' % i, (x, y, z - .026), (.28, .085, .012), M_WHT, .004)
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
# coach-built seats (Art/CoachSeat, Tools/make_coach_seat.py): built in the coach seat's own frame (x across, -y forward, z up from
# the floor; cushion front CF, back CB; backrest reclined 15 degrees about the cushion's back), then turned to face +x and set in place
CF, CB, RECLINE = -.50, -.04, math.radians(15)
def cushion_deform(co, u, v, w):
    top = K.ss(-1.0, 1.0, w)
    co.z += top * (.007 * (1 - u * u) + .014 * K.ss(.55, .95, abs(u)) - .006 * K.ss(.3, 1.0, v))      # crown, bolsters, rear dip
    fr = K.ss(-.55, -1.0, v); co.z -= fr * fr * .022 * (.4 + .6 * top); co.y -= fr * .006 * (1 - top)  # the waterfall front
    return co
def back_deform(co, u, v, w):
    front = K.ss(.6, -1.0, v)
    co.y -= front * (.032 * K.ss(.55, .95, abs(u)) + .032 * math.exp(-((co.z - .20) / .13) ** 2))        # bolsters, lumbar
    co.y += front * (.022 * K.ss(.35, 1.0, w) + .006 * K.ss(.9, 1.0, w))                                # thinner towards the top
    cz = K.ss(.5, .38, abs(u)) * K.ss(.12, .18, co.z) * K.ss(.70, .64, co.z); co.y += front * .004 * cz # the centre panel sits in
    return co
def shell_deform(co, u, v, w): co.y += .012 * (1 - u * u) * K.ss(1.0, -.2, v) * (1 - w * w); return co
def head_deform(co, u, v, w):
    wing = K.ss(.42, 1.0, abs(u)); co.y -= wing * .04; co.x -= math.copysign(wing * .012, u); co.y -= .008 * (1 - u * u) * K.ss(.5, -1.0, v); return co
def taxi_seat(n, X, Y, fz, inner, belt3=True):
    """one seat; (X, Y) the cushion's back centre in the van, fz the floor top under it, inner = +1/-1 the side (van y) of the buckle."""
    S, B = [], []
    S.append(box(n + '_Cushion', (0, (CF + CB) / 2, .412), (.41, CB - CF, .076), M_MOQ, .028, 4, 10, 1, cushion_deform, 60))
    S.append(box(n + '_CushionInset', (0, CF - .001, .396), (.30, .008, .028), M_SHELLS, .006, 3, 2))
    S.append(box(n + '_CushionPan', (0, (CF + CB) / 2 + .01, .366), (.42, .44, .016), M_SHELLS, .006, 2, 2))
    for i, x in enumerate((-.19, .19)):
        S.append(box(n + '_SideMember%d' % i, (x, -.25, .352), (.026, .33, .034), M_CSTEEL, .005, 2, 2))
        for j, y in enumerate((-.38, -.12)):
            S.append(cyl(n + '_Leg%d%d' % (i, j), (x, y, .17), .013, .33, 'z', M_CSTEEL, 20))
            S.append(box(n + '_Foot%d%d' % (i, j), (x, y, .004), (.06, .06, .008), M_ALU, .002, 2))
            S.append(cyl(n + '_Bolt%d%d' % (i, j), (x + .02, y + .02, .0095), .006, .004, 'z', M_CSTEEL, 12, .001))
    for j, y in enumerate((-.38, -.12)): S.append(cyl(n + '_CrossTube%d' % j, (0, y, .32), .016, .40, 'x', M_CSTEEL, 24))
    ix = inner                                                                   # local x = van y
    S.append(box(n + '_BuckleStem', (ix * .175, CB - .005, .44), (.024, .004, .05), M_BELT, .001, 1))
    S.append(box(n + '_Buckle', (ix * .175, CB - .02, .463), (.028, .012, .05), M_CBLACK, .004, 2))
    S.append(box(n + '_BuckleRed', (ix * .175, CB - .02, .4895), (.016, .008, .004), M_ORG, .001, 1))
    if belt3:
        S.append(box(n + '_BeltAnchor', (-ix * .175, CB - .02, .463), (.028, .012, .05), M_CSTEEL, .004, 2))
        S.append(box(n + '_BeltStrap', (-ix * .175, CB - .005, .44), (.024, .004, .05), M_BELT, .001, 1))
    br = box(n + '_Backrest', (0, .045, .41), (.40, .09, .78), M_MOQ, .03, 4, 14, 1, back_deform, 60); br.data.materials.append(M_MOQC)
    for p in br.data.polygons:
        if p.normal.y < -.5 and abs(p.center.x) < .118 and .115 < p.center.z < .69: p.material_index = 1
    B.append(br)
    for s in (-1, 1): B.append(box(n + '_BolsterInset%d' % (s > 0), (s * .2005, .045, .43), (.004, .045, .40), M_SHELLS, .004, 2, 2))
    B.append(box(n + '_BackShell', (0, .103, .395), (.418, .028, .82), M_SHELLS, .014, 3, 10, 1, shell_deform, 60))
    B.append(box(n + '_Headrest', (0, .03, .88), (.31, .10, .15), M_MOQ, .035, 4, 8, 1, head_deform, 60))
    for s in (-1, 1): B.append(cyl(n + '_HeadCap%d' % (s > 0), (s * .141, -.004, .88), .042, .006, 'x', M_SHELLS, 32, .002))
    for j, dx in enumerate((-.055, .055)):
        B.append(cyl(n + '_HeadPost%d' % j, (dx, .05, .80), .0065, .04, 'z', M_CSTEEL, 16)); B.append(cyl(n + '_PostSleeve%d' % j, (dx, .05, .795), .011, .012, 'z', M_CBLACK, 16, .002))
    if belt3:                                                                    # the shoulder belt: a guide on the outer top corner, the web down the side
        ox = -ix
        B.append(box(n + '_BeltGuide', (ox * .185, .045, .815), (.06, .05, .03), M_CBLACK, .006, 2))
        B.append(box(n + '_BeltWeb', (ox * .2070, .045, .45), (.003, .040, .73), M_BELT, .001, 1))
        B.append(box(n + '_BeltTongue', (ox * .2105, .045, .30), (.004, .030, .050), M_CSTEEL, .001, 1))
    BF = Matrix.Translation((0, CB + .01, .46)) @ Matrix.Rotation(-RECLINE, 4, 'X')
    for o in B: o.data.transform(BF)
    T = Matrix.Translation((X, Y, fz)) @ Matrix.Rotation(math.radians(90), 4, 'Z')
    for o in S + B: o.data.transform(T); o.data.update()
    P_ = lambda v: tuple(T @ Vector(v))
    ik('Seat_' + n, P_((0, -.25, .47))); ik('Buckle_' + n, P_((ix * .175, CB - .02, .50)))
    if belt3: ik('BeltTongue_' + n, tuple(T @ BF @ Vector((-ix * .2105, .045, .30))))
FZC, FZR = FLOOR, FLOOR + .010                                                  # cab floor / room floor tops
taxi_seat('SeatDriver', .49, .42, FZC, -1, False); taxi_seat('SeatPassenger', .49, -.46, FZC, 1, False)
box('SeatDriver_Lever', (.80, .645, FZC + .352), (.20, .07, .03), M_PLAST, .01)
SEATS = [('SeatR1A', -.30, .64, FZC, -1), ('SeatR1B', -.30, .20, FZC, 1), ('SeatR2A', -1.09, .64, FZR, -1), ('SeatR2B', -1.09, .20, FZR, 1),
         ('SeatR3A', -1.88, .44, FZR, -1), ('SeatR3B', -1.88, 0, FZR, 1), ('SeatR3C', -1.88, -.44, FZR, 1)]     # 1 + 1 + 2 + 2 + 3: driver and 8
for n, X, Y, fz, inn in SEATS: taxi_seat(n, X, Y, fz, inn)
for n, x, y, z0 in (('D', .50, .865, 1.93), ('P', .50, -.865, 1.93)):   # belts on the pillars
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
# roof: the lit taxi sign (own small box on two rubber pads, glowing lenses front and back), aerial, cooling vent
TSX = -.40
for i, y in enumerate((-.22, .22)): box('TaxiSign_Pad%d' % i, (TSX, y, 2.527), (.14, .10, .03), M_SEAL, .006)
box('TaxiSign_Housing', (TSX, 0, 2.625), (.17, .64, .17), M_PLAST, .02, 3)
for e, n in ((1, 'F'), (-1, 'B')): box('TaxiSign_%s_Lens' % n, (TSX + e * .0835, 0, 2.630), (.008, .56, .11), M_SIGN, .003, 2)
ik('TaxiSign', (TSX, 0, 2.63))
box('RoofVent', (-1.75, 0, 2.538), (.42, .42, .05), M_PLAST, .02, 3); tube('Aerial', [(.55, -.45, 2.44), (.45, -.45, 2.62), (.36, -.45, 2.78)], .004, M_PLAST); cyl('Aerial_Base', (.55, -.45, 2.448), .02, .03, 'z', M_PLAST, 20)
# the taximeter on the dashboard top, facing the driver and the passengers
box('Taximeter_Foot', (1.52, .05, 1.305), (.06, .10, .05), M_PLAST, .008)
box('Taximeter_Body', (1.50, .05, 1.37), (.10, .19, .075), M_DARK, .012, 3)
box('Taximeter_Screen', (1.4485, .05, 1.385), (.005, .15, .03), M_METER, .001, 1)
for i, y in enumerate((-.01, .05, .11)): box('Taximeter_Btn%d' % i, (1.447, y, 1.345), (.008, .03, .012), M_ORG if i == 1 else M_PLAST, .002)
ik('Taximeter_Button', (1.44, .05, 1.345))
# #endregion

# #region ---- door grab bar and the luggage space at the back doors
tube('GrabBar_SlideDoor', [(-.985, -.905, 1.00), (-.985, -.905, 1.60)], .016, M_CSTEEL, bez=False)
for i, z in enumerate((1.04, 1.56)): box('GrabBar_SlideDoor_Foot%d' % i, (-.985, -.94, z), (.04, .09, .04), M_CBLACK, .006)
ik('Grab_SlideDoor', (-.985, -.905, 1.30))
box('LuggageFloor', (-2.579, 0, FLOOR + .0015), (.636, 1.38, .016), M_FLOOR, .003)
for s, n in ((1, 'L'), (-1, 'R')): box('LuggageRail_' + n, (-2.58, s * .66, FLOOR + .024), (.58, .025, .03), M_ALU, .004)
for i, (x, y) in enumerate(((-2.40, .45), (-2.40, -.45), (-2.78, .45), (-2.78, -.45))):
    cyl('LuggageEye_%d' % i, (x, y, FLOOR + .0115), .022, .006, 'z', M_CSTEEL, 20, .001)
    tube('LuggageEye_%d_Ring' % i, [(x - .018, y, FLOOR + .013), (x - .012, y, FLOOR + .032), (x + .012, y, FLOOR + .032), (x + .018, y, FLOOR + .013)], .004, M_CSTEEL)
    ik('Luggage_%d' % i, (x, y, FLOOR + .012))
# #endregion

# #region ---- points for the code
ik('Driver_Eye', (.60, .42, 1.72)); ik('Tow', (-3.05, 0, .46))
for i, x in enumerate((.25, -.55, -1.34)): ik('Aisle_Row%d' % (i + 1), (x, -.45, FLOOR + .01))
# #endregion

K.uv_cube()
for o in list(LOFTS.values()): rm(o)
if '--nochk' not in sys.argv:
    K.checks('TAXI', ground_ok=('Tyre',), overhang_skip_prefix=('',))
    for nm, pv, ax, dg, sk in (('DoorDriver', (1.705, 1.015, 1), 'z', (-15, -40, -68), ('Tyre_FL',)), ('DoorPassenger', (1.705, -1.015, 1), 'z', (15, 40, 68), ()), ('RearDoorL', (-2.992, DW + .045, 1.4), 'z', (-30, -90), ()), ('RearDoorR', (-2.992, -DW - .045, 1.4), 'z', (30, 90), ())):
        h = K.sweep(nm, pv, ax, dg, sk); print('TAXI SWEEP', nm, len(h), sorted(h)[:10])
    bv = [(o.name, K.bvh(o)) for o in ALL if not o.name.startswith('SlideDoor')]; hits = set()
    for sh in ((-.04, -.125, 0), (-.6, -.125, 0), (-1.30, -.125, 0)):
        for o in ALL:
            if o.name.startswith('SlideDoor'):
                from mathutils.bvhtree import BVHTree
                t = BVHTree.FromPolygons([v.co + Vector(sh) for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
                for n, b in bv:
                    if t.overlap(b): hits.add((o.name, n, sh[0]))
    print('TAXI SLIDE', len(hits), sorted(hits)[:12])
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
GLB = os.path.join(ART, 'taxibus.glb'); ZF = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, os.path.join(ART, 'TaxiBus.blend'), GLB, ZF, 'TAXI', 'TaxiBus')
GROUPS = ['DoorDriver', 'DoorPassenger', 'SlideDoor', 'RearDoorL', 'RearDoorR', 'SteeringWheel']
KEEP = ('Tyre_', 'Rim_', 'WheelDisc_', 'Hub_', 'HubCap_', 'Nut_', 'Spindle_', 'SteerArm_', 'Disc_F', 'Wiper', 'Pedal_', 'Stock_', 'Body', 'Hood')
def groupof(o):
    n = o.name
    if n.startswith(KEEP) or n.endswith('_Lens'): return None
    g = next((g for g in GROUPS if n.startswith(g)), None)
    if g: return g + ('_Glass' if n.endswith('_Glass') else '_Parts' if n != g else None or '')
    if n.endswith('_Glass') : return 'GlassFixed'
    if n.startswith(('Seat', 'Row', 'Belt')): return 'Seats'
    if n.startswith(('Dash', 'Console', 'CabM', 'SunV', 'Grab', 'Steer', 'Stalk', 'Foot', 'Cup', 'Gear', 'Firewall', 'Cowl', 'Taximeter')): return 'CabFittings'
    if n.startswith(('Arch', 'Rub', 'HiVis', 'Headlamp', 'RearLamp', 'Grille', 'Charge', 'Lower', 'Fog', 'Plate', 'Rear', 'Reflector', 'Sill', 'Slide', 'Work', 'Stop', 'TaxiSign', 'Roof', 'Aerial')): return 'OuterFittings'
    if n.startswith(('Frame', 'Leaf', 'Battery', 'Front', 'EMotor', 'Sub', 'Strut', 'Wish')): return 'Underside'
    return 'InnerFittings'
def join(name, parts):
    for x in bpy.context.selected_objects: x.select_set(False)
    for o in parts: o.select_set(True); K.ALL.remove(o)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.name = name; o.data.name = name; o.select_set(False)
    used = sorted({p.material_index for p in o.data.polygons}); ms = [o.data.materials[i] for i in used]
    mp = {u: i for i, u in enumerate(used)}; ix = [mp[p.material_index] for p in o.data.polygons]
    o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, i in zip(o.data.polygons, ix): p.material_index = i
    K.ALL.append(o); return o
G = {}
for o in ALL:
    g = groupof(o)
    if g and o.name != g: G.setdefault(g, []).append(o)
for g, l in G.items(): join(g + ('_All' if g in GROUPS else ''), l)
for n, o in {o.name: o for o in ALL}.items():
    a = (len(o.data.polygons), len(o.data.materials)); assert o.data.materials and all(o.data.materials), n
    assert {p.material_index for p in o.data.polygons} == set(range(len(o.data.materials))), 'empty slot ' + n
last2 = K.save_export(ROOT, os.path.join(ART, 'TaxiBus.blend'), GLB, ZF, 'TAXI joined', 'TaxiBusGame')
ue = lambda v: [round(v[0] * 100, 1), round(-v[1] * 100, 1), round(v[2] * 100, 1)]
tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL)
json.dump({'note': 'points in the model root, Unreal cm (Blender (x,y,z) m = (x,-y,z) * 100)', 'size_m': [L, W, H], 'wheelbase': WB, 'tyre_radius_cm': RW * 100, 'points_cm': {e.name: ue(e.location) for e in IKS},
           'parts': sorted(o.name for o in ALL), 'materials': sorted({m.name for o in ALL for m in o.data.materials}), 'tris': tris, 'flicker': last2}, open(os.path.join(ART, 'taxibus-parts.json'), 'w'), indent=1)
print('DONE TAXI: %d parts, %d points, %d tris, %s' % (len(ALL), len(IKS), tris, last2))
for o in ALL:
    a, b = K.bbox(o)
    if o.name in ('Body', 'DoorDriver', 'SlideDoor', 'RearDoorL'): print('TAXI BOX', o.name, [round(v, 3) for v in a], [round(v, 3) for v in b])

