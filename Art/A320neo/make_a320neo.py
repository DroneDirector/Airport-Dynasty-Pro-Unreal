# A320neo - step 1: the FUSELAGE SHELL.
# Blender 3.3, headless, real metres.  Built to Art/A320neo/BUILD-PLAN.md and the numbers in
# Art/A320/blueprint/a320neo-data.json; panel lines, skin joints and rivet rows sit on the real frame
# stations FR1-FR87 and stringer lines read off the structural wireframe sheets
# (docs/Pictures/a320neo-blueprint/wire-side.png, wire-plan.png, wire-front.png, wire-iso.png).
#
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/A320neo/make_a320neo.py
#       [-- --render]       also writes 3 x 3840x2160 renders to docs/Pictures/a320neo/
#
# Axes: origin at the nose tip on the centreline, +x aft, +y starboard, +z up, ground at z = 0
# (MRW, forward CG - the left column of the clearance table on sheet 1).
# Naming: A320N_<Group>_<Part>[_L|_R][_<n>].  Groups used here: Fus, Cockpit, Door, Tail, Light.
# Wing, engines, gear and interior are NOT built here (steps 2-6).
import os, sys, math, json, bisect
import bpy, bmesh
from mathutils import Vector, Quaternion

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
ART = HERE
D = json.load(open(os.path.join(ROOT, 'Art', 'A320', 'blueprint', 'a320neo-data.json')))
O, DR, T = D['overall'], D['doors'], D['tail']
L = O['length']
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []

# ---------------------------------------------------------------------- monotone cubic (pchip)
class Pchip:
    def __init__(s, pts):
        p = sorted(pts); s.x = [a for a, _ in p]; s.y = [b for _, b in p]; n = len(p)
        s.h = [s.x[i + 1] - s.x[i] for i in range(n - 1)]
        d = [(s.y[i + 1] - s.y[i]) / s.h[i] for i in range(n - 1)]
        m = [0.0] * n; m[0] = d[0]; m[n - 1] = d[-1]
        for i in range(1, n - 1):
            if d[i - 1] * d[i] <= 0:
                m[i] = 0.0
            else:
                w1 = 2 * s.h[i] + s.h[i - 1]; w2 = s.h[i] + 2 * s.h[i - 1]
                m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
        s.m = m
    def __call__(s, q):
        if q <= s.x[0]: return s.y[0]
        if q >= s.x[-1]: return s.y[-1]
        i = bisect.bisect_right(s.x, q) - 1; h = s.h[i]; t = (q - s.x[i]) / h
        t2 = t * t; t3 = t2 * t
        return ((2 * t3 - 3 * t2 + 1) * s.y[i] + (t3 - 2 * t2 + t) * h * s.m[i] +
                (-2 * t3 + 3 * t2) * s.y[i + 1] + (t3 - t2) * h * s.m[i + 1])

# fuselage side profile and half width (Airbus AC fig 2-2-0, same curves as Tools/make_a320neo_blueprint.py)
ZTOP = Pchip([(0, 3.09), (.2, 3.57), (.66, 3.92), (1.34, 4.38), (2.04, 4.92), (2.92, 5.39), (3.83, 5.67),
              (4.7, 5.83), (5.61, 5.91), (6, 5.92), (25, 5.92), (28.68, 5.96), (30.2, 5.94), (33.0, 5.70),
              (35.38, 5.58), (36.50, 5.43), (37.57, 5.30)])
ZBOT = Pchip([(0, 3.09), (.07, 2.85), (.28, 2.6), (.64, 2.39), (1.11, 2.21), (1.79, 2.02), (3.3, 1.81),
              (4.75, 1.78), (23.86, 1.80), (25.37, 1.88), (26.62, 2.0), (28.24, 2.27), (30, 2.66),
              (32.18, 3.14), (35, 3.93), (36.30, 4.30), (37.00, 4.58), (37.57, 4.74)])
HW = Pchip([(0, 0), (.06, .22), (.31, .5), (.75, .84), (1.12, 1.05), (2.58, 1.58), (3.72, 1.83), (4.69, 1.96),
            (5.46, 1.98), (27.96, 1.98), (28.7, 1.9), (30.31, 1.66), (32.06, 1.34), (35, .85), (36.00, .60),
            (36.80, .40), (37.57, .28)])

SE = 2.14          # superellipse exponent of the barrel (3.95 wide x 4.14 high)
TAU = 2 * math.pi
# the skin grid seam sits at 202.5 deg (lower port, between the door 1L/4L surround and the aft
# equipment door) - the one band of the barrel with no cut-out in it.
T1 = math.radians(202.5)
T0 = T1 - TAU

def wrapt(t):
    return T0 + (t - T0) % TAU

def in_band(t, a, b):
    return (wrapt(t) - a) % TAU <= (b - a) % TAU

# The three traced profiles meet in one point at x=0 with finite slopes, and their mid-line sags
# 3 cm between x 0 and 0.2 - read literally the nose ends in a cone point with a lobe under it,
# which is the lump the owner saw low on the radome. Forward of TIP_X the section is blended onto
# a round (quarter-ellipse) cap on a straight mid-line, so the tip is a dome. The blend is zero
# with zero slope at TIP_X, leaves the whole fuselage aft of x 0.22 exactly as traced, and puts
# the tip point at z 3.095 against the traced 3.090.
TIP_X = .22
TIP_HH = (ZTOP(TIP_X) - ZBOT(TIP_X)) * .5
TIP_HW = HW(TIP_X)
TIP_CZ = (ZTOP(TIP_X) + ZBOT(TIP_X)) * .5
TIP_CZ_SLOPE = ((ZTOP(TIP_X + .01) + ZBOT(TIP_X + .01))
                - (ZTOP(TIP_X - .01) + ZBOT(TIP_X - .01))) * .5 / .02

def ring(x):
    x = min(max(x, 0.0), L)
    zt, zb, hw = ZTOP(x), ZBOT(x), max(HW(x), 0.004)
    cz, hh = (zt + zb) * .5, (zt - zb) * .5
    if x < TIP_X:
        u = 1.0 - x / TIP_X
        w = u * u
        k = math.sqrt(max(1.0 - u * u, 0.0))
        hh = hh * (1 - w) + TIP_HH * k * w
        hw = hw * (1 - w) + TIP_HW * k * w
        cz = cz * (1 - w) + (TIP_CZ + TIP_CZ_SLOPE * (x - TIP_X)) * w
    return cz, max(hh, .004), max(hw, .004)

def _p(v, e):
    return math.copysign(abs(v) ** e, v) if v else 0.0

def surf(x, t):
    cz, hh, hw = ring(x); e = 2.0 / SE
    ct, st = math.cos(t), math.sin(t)
    y = hw * _p(ct, e); z = cz + hh * _p(st, e)
    if st < 0:                              # slightly flattened keel. The flattening is faded
        s = min(1.0, -st / .35)             # out towards the widest line, where it used to step
        w = s * s * (3 - 2 * s)             # the slope and leave a crease down the radome.
        if x > 1.4:                         # only on the nose: aft of x 2.6 the keel is as before,
            r = min(1.0, (x - 1.4) / 1.2)   # so nothing on the rest of the fuselage moves
            w = max(w, r * r * (3 - 2 * r))
        z = cz + hh * _p(st, e) * (1 - .045 * ct * ct * w)
    return Vector((x, y, z))

def nrm(x, t):
    du = surf(min(x + .02, L), t) - surf(max(x - .02, 0), t)
    dv = surf(x, t + .02) - surf(x, t - .02)
    n = dv.cross(du)
    rad = Vector((0, math.cos(t), math.sin(t)))
    if n.length < 1e-9: return rad.normalized()
    n.normalize()
    if n.dot(rad) < 0: n = -n
    return n

def soff(x, t, d):
    return surf(x, t) + nrm(x, t) * d

def t_of_z(x, z, side=1):
    cz, hh, hw = ring(x)
    s = max(-.995, min(.995, (z - cz) / hh))
    a = math.asin(_p(s, SE * .5))
    return a if side > 0 else math.pi - a

def z_top_at(x, y):
    cz, hh, hw = ring(x)
    c = max(-1.0, min(1.0, abs(y) / hw))
    t = math.acos(_p(c, SE * .5)) if c < 1 else 0.0
    return surf(x, t).z

def y_side_at(x, z, side=1):
    return surf(x, t_of_z(x, z, side)).y

# ---------------------------------------------------------------------- frame stations FR1-FR87
DATUM = 2.5425
STA_CM = {1: 350, 24: 950, 35: 1537, 47: 2136, 64: 3043, 70: 3366, 77: 3655, 87: 4011}
AMM_AFT = {64: 26695, 65: 27279, 66: 27860, 67: 28351, 68: 28842, 69: 29362, 70: 29921, 71: 30321,
           72: 30772, 73: 31172, 74: 31572, 75: 31885, 76: 32398, 77: 32811}
AMM_TAIL = {77: 32811, 78: 32923, 79: 33242, 80: 33561, 81: 33931, 82: 34301, 83: 34756, 84: 35211,
            85: 35666, 86: 36121, 87: 36379}
SECTIONS = [(1, 24), (24, 35), (35, 47), (47, 64), (64, 70), (70, 77), (77, 87)]

def _frames():
    mm = {n: c * 10.0 for n, c in STA_CM.items()}
    for src, a, b in ((AMM_AFT, 64, 77), (AMM_TAIL, 77, 87)):
        k = (mm[b] - mm[a]) / (src[b] - src[a])
        for n, v in src.items():
            if n not in mm: mm[n] = mm[a] + (v - src[a]) * k
    for a, b in SECTIONS[:4]:
        for n in range(a + 1, b): mm[n] = mm[a] + (mm[b] - mm[a]) * (n - a) / (b - a)
    return {n: mm[n] / 1000.0 - DATUM for n in sorted(mm)}

FX = _frames()
SECTION_JOINT_FR = [24, 35, 47, 64, 70, 77]
LAP_T_DEG = [64, 36, -6, -42, -74]     # stringer lines that carry a visible longitudinal lap joint
X_NOSE = FX[1]                         # radome joint plane = FR1 (0.958 m aft of the nose tip)
SEAM_HW = .002                         # half the 4 mm radome seam channel: radome ends here, skin starts
X_RADOME_END = X_NOSE - SEAM_HW
X_SKIN0 = X_NOSE + SEAM_HW
X_TAILEND = 37.30                     # skin ends here, the APU exhaust closes it

# ---------------------------------------------------------------------- Blender helpers
MATS = {}
def mat(name, base, rough=.45, metal=0.0, alpha=1.0):
    if name in MATS: return MATS[name]
    m = bpy.data.materials.new('A320N_' + name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (base[0], base[1], base[2], 1)
    b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if alpha < 1:
        b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND'
        b.inputs['Transmission'].default_value = .9; b.inputs['IOR'].default_value = 1.52
    MATS[name] = m; return m

MT = {}
def mats():
    MT.update(dict(
        paint=mat('Paint', (.88, .89, .91), .33),
        grey=mat('PaintGrey', (.58, .60, .63), .40),
        alu=mat('Alu', (.72, .73, .75), .30, 1.0),
        dark=mat('DarkGrey', (.10, .10, .11), .62),
        bay=mat('BayGreen', (.46, .50, .36), .70),
        rubber=mat('Rubber', (.055, .055, .06), .80),
        glass=mat('Glass', (.62, .70, .76), .07, 0.0, .18),
        lens=mat('LensRed', (.60, .06, .05), .12, 0.0, .40),
        steel=mat('Steel', (.56, .57, .59), .26, 1.0)))

OBJS = []
def add(name, verts, faces, material, smooth=True, bevel=0.0):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], faces)
    me.validate(verbose=False)
    ob = bpy.data.objects.new(name, me)
    ob.data.materials.append(material)
    bpy.context.collection.objects.link(ob)
    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    loose = [v for v in bm.verts if not v.link_faces]
    if loose: bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    if smooth:
        me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(38)
        for p in me.polygons: p.use_smooth = True
    if bevel > 0:
        md = ob.modifiers.new('bev', 'BEVEL'); md.width = bevel; md.segments = 2
        md.limit_method = 'ANGLE'; md.angle_limit = math.radians(42)
    OBJS.append(ob); return ob

def grid_faces(nx, nt, base=0, wrap_t=False, skip=None):
    f = []
    for i in range(nx - 1):
        for j in range(nt if wrap_t else nt - 1):
            j2 = (j + 1) % nt
            if skip and skip(i, j): continue
            f.append([base + i * nt + j, base + (i + 1) * nt + j,
                      base + (i + 1) * nt + j2, base + i * nt + j2])
    return f

def bridge(a, b, flip=False, closed=True):
    n = len(a); f = []
    rng = range(n) if closed else range(n - 1)
    for k in rng:
        k2 = (k + 1) % n
        q = [a[k], a[k2], b[k2], b[k]]
        f.append(q[::-1] if flip else q)
    return f

def fan(loop, centre_idx, flip=False):
    n = len(loop); f = []
    for k in range(n):
        tri = [loop[k], loop[(k + 1) % n], centre_idx]
        f.append(tri[::-1] if flip else tri)
    return f

def tube(V, F, pts, radii, ns=10, cap=True):
    """a swept round tube through the given points"""
    loops = []
    for i, p in enumerate(pts):
        a = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        if a.length < 1e-9: a = Vector((1, 0, 0))
        a.normalize()
        up = Vector((0, 0, 1)) if abs(a.z) < .9 else Vector((1, 0, 0))
        e1 = a.cross(up).normalized(); e2 = a.cross(e1).normalized()
        b = len(V)
        for q in range(ns):
            ang = 2 * math.pi * q / ns
            V.append(p + e1 * (radii[i] * math.cos(ang)) + e2 * (radii[i] * math.sin(ang)))
        loops.append(list(range(b, b + ns)))
    for a, b in zip(loops, loops[1:]): F += bridge(a, b)
    if cap:
        cb = len(V); V.append(pts[0]); F += fan(loops[0], cb, True)
        cb = len(V); V.append(pts[-1]); F += fan(loops[-1], cb)
    return loops

def place_hinged(ob, pivot, axis, angle_deg):
    """put the object origin on its real hinge axis and swing it open about that axis"""
    p = Vector(pivot)
    for v in ob.data.vertices: v.co -= p
    ob.location = p
    ob.rotation_mode = 'QUATERNION'
    ob.rotation_quaternion = Quaternion(Vector(axis).normalized(), math.radians(angle_deg))

# ---------------------------------------------------------------------- cut-outs
EXCL = []          # (x0, x1, t0, t1) areas where no frame line or lap joint may be drawn
def excl(x0, x1, t0, t1, mx=.02, mt=.02):
    EXCL.append((x0 - mx, x1 + mx, wrapt(min(t0, t1)) - mt, wrapt(min(t0, t1)) + abs(t1 - t0) + mt))

def blocked(x, t):
    for x0, x1, a, b in EXCL:
        if x0 < x < x1 and in_band(t, a, b): return True
    return False

def segments(pts, key):
    """split a polyline at every point that falls inside an excluded area"""
    out, cur = [], []
    for p in pts:
        if blocked(*key(p)):
            if len(cur) > 1: out.append(cur)
            cur = []
        else:
            cur.append(p)
    if len(cur) > 1: out.append(cur)
    return out

CUTS = []
def cut(name, xc, w, t0, t1, r=.09, pad=.10, kind='door', sh=0.0):
    t0, t1 = min(t0, t1), max(t0, t1)
    tcc = wrapt((t0 + t1) * .5); half = (t1 - t0) * .5
    t0, t1 = tcc - half, tcc + half
    hh = ring(xc)[1]
    if sh: SHEAR[(round(xc, 4), round(tcc, 4))] = (sh, half)
    spread = abs(sh) * w * .5
    CUTS.append(dict(name=name, xc=xc, tc=tcc, ax=w * .5, at=half,
                     x0=xc - w * .5, x1=xc + w * .5, t0=t0 - spread, t1=t1, r=r,
                     px=pad, pt=pad / hh, S=hh, kind=kind))
    return CUTS[-1]

def cut_xz(name, xc, w, zsill, h, side, r=.09, pad=.10, kind='door'):
    return cut(name, xc, w, t_of_z(xc, zsill, side), t_of_z(xc, zsill + h, side), r, pad, kind)

SHEAR = {}     # (xc, tc) -> (k, at): the lower edge of the loop rakes, the upper edge stays put

def rr_points(xc, tc, ax, at, r, S, n=320):
    """dense rounded-rectangle polyline in (x, t); r is the corner radius in metres"""
    r = min(r, ax * .92); rt = min(r / S, at * .92)
    seg = max(3, n // 8); pts = []
    def arc(cx, ct, a0, a1):
        for k in range(seg + 1):
            a = a0 + (a1 - a0) * k / seg
            pts.append((cx + r * math.cos(a), ct + rt * math.sin(a)))
    arc(xc - ax + r, tc - at + rt, math.pi, math.pi * 1.5)
    arc(xc + ax - r, tc - at + rt, math.pi * 1.5, math.pi * 2.0)
    arc(xc + ax - r, tc + at - rt, 0.0, math.pi * .5)
    arc(xc - ax + r, tc + at - rt, math.pi * .5, math.pi)
    sk = SHEAR.get((round(xc, 4), round(tc, 4)))
    if sk:
        k, at0 = sk
        pts = [(x_, t_ + k * (x_ - xc) * min(max((tc + at0 - t_) / (2 * at0), 0.0), 1.0))
               for (x_, t_) in pts]
    out = []
    for p in pts:
        if not out or abs(p[0] - out[-1][0]) + abs(p[1] - out[-1][1]) > 1e-9: out.append(p)
    return out

def resample(pts, fracs, S):
    d = [0.0]
    for k in range(1, len(pts) + 1):
        a, b = pts[k - 1], pts[k % len(pts)]
        d.append(d[-1] + math.hypot(b[0] - a[0], (b[1] - a[1]) * S))
    tot = d[-1]; out = []
    for f in fracs:
        s = (f % 1.0) * tot
        i = min(max(bisect.bisect_right(d, s) - 1, 0), len(pts) - 1)
        seg = d[i + 1] - d[i]; u = (s - d[i]) / seg if seg > 1e-12 else 0.0
        a, b = pts[i], pts[(i + 1) % len(pts)]
        out.append((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u))
    return out

def shrink(loop, xc, tc, k):
    return [(xc + (p[0] - xc) * k, tc + (p[1] - tc) * k) for p in loop]

# ---------------------------------------------------------------------- the cut-out list
WIN_X = DR['windows']['x_centers']
WIN_Z = DR['windows'].get('center_z_MRW_fwd', 4.55)
WIN_W, WIN_H = 0.255, 0.385          # skin cut-out; clear aperture 0.19 x 0.34
EXIT_X = DR['overwing_exits']['x_centers']
WIN_IN_EXIT = [14.51, 15.37]         # two of the 38 sit in the overwing hatches

P1 = DR['passenger']['1L_1R']; P4 = DR['passenger']['4L_4R']; OW = DR['overwing_exits']
def g(d, *keys, **kw):
    for k in keys:
        if k in d: return d[k]
    return kw.get('dflt')

DOORS = []
def door_def(**k): DOORS.append(k); return k

for sd, sg in (('L', -1), ('R', 1)):
    door_def(id='Pax1' + sd, side=sg, kind='pax', thick=.145, hinge='fwd', angle=170,
             bustle=True, window=True,
             cut=cut_xz('Pax1' + sd, P1['x_center'], P1['width'],
                        g(P1, 'sill_ground_MRW_fwd', 'sill_ground', dflt=3.38), P1['height'], sg, r=.13))
    door_def(id='Pax4' + sd, side=sg, kind='pax', thick=.145, hinge='fwd', angle=170,
             bustle=True, window=True,
             cut=cut_xz('Pax4' + sd, P4['x_center'], P4['width'],
                        g(P4, 'sill_ground_MRW_fwd', 'sill_ground', dflt=3.60), P4['height'], sg, r=.13))
    for n, xe in enumerate(EXIT_X):
        door_def(id='Exit%d%s' % (n + 1, sd), side=sg, kind='exit', thick=.115, hinge='top', angle=0,
                 bustle=False, window=True,
                 cut=cut_xz('Exit%d%s' % (n + 1, sd), xe, OW['width'],
                            g(OW, 'sill_ground_MRW_fwd', 'sill_ground', dflt=3.87), OW['height'],
                            sg, r=.10, pad=.085))
for nm, key, th, ang, rr_, pd in (('CargoFwd', 'fwd', .125, 58, .15, .10),
                                  ('CargoAft', 'aft', .125, 58, .15, .10),
                                  ('Bulk', 'bulk', .105, 62, .11, .09)):
    c_ = DR['cargo'][key]
    door_def(id=nm, side=1, kind='cargo', thick=th, hinge='top', angle=ang, bustle=False, window=False,
             cut=cut_xz(nm, c_['x_center'], c_['width'],
                        g(c_, 'sill_ground_MRW_fwd', 'sill_ground', dflt=2.0), c_['height'], 1,
                        r=rr_, pad=pd))
# service doors: forward avionics hatch on the keel, aft equipment door lower left
door_def(id='SvcAvionics', side=0, kind='service', thick=.085, hinge='fwd', angle=96, bustle=False,
         window=False, cut=cut('SvcAvionics', 4.00, .62, -math.pi / 2 - .155, -math.pi / 2 + .155,
                               r=.08, pad=.085, kind='service'))
door_def(id='SvcAftEquip', side=-1, kind='service', thick=.085, hinge='fwd', angle=98, bustle=False,
         window=False, cut=cut_xz('SvcAftEquip', 24.50, .70, 2.20, .60, -1, r=.08, pad=.085,
                                  kind='service'))

WINCUTS = []
for sd, sg in (('L', -1), ('R', 1)):
    n = 0
    for xw in WIN_X:
        n += 1
        if xw in WIN_IN_EXIT: continue
        tz = t_of_z(xw, WIN_Z, sg); dt = (WIN_H * .5) / ring(xw)[1]
        WINCUTS.append((cut('Win%s%02d' % (sd, n), xw, WIN_W, tz - dt, tz + dt,
                            r=.075, pad=.085, kind='window'), sd, n))

# ---------------------------------------------------------------------- cockpit windows
# doors.cockpit_windows (band x 1.13-2.65, glazing z 4.16-4.95, CP1 lower corner x 1.13 z 4.17)
# is a SIDE-VIEW SILHOUETTE of the three windows, not a patch of skin: at x 1.13 the crown is
# only z 4.235, because that end of the band lies on the FRONT FACE of the nose.  So the windows
# are not drawn in the side view at all.  Each one is drawn as a closed outline on its own flat
# drawing board in space - the windscreen on a board raked 32 deg back from vertical, windows 2
# and 3 on boards facing straight outboard - and every point of that outline is then shrunk onto
# the nose skin along a ray from a point inside the cockpit (a shrinkwrap on the analytic skin,
# along its local normal).  Frame and pane therefore follow the real skin everywhere, and the
# silhouette they end up with lands inside the published band.
# Only the PORT side is modelled; starboard is an exact mirror across y = 0.
CKW = CKWIN = DR['cockpit_windows']
CKZ0, CKZ1 = CKW['z_ground_range']
CKX0, CKX1 = CKW['x_range']
CKFRAME = .058        # width of the raised frame round each pane, metres
CKPROUD = .012        # the frame lip stands this far proud of the skin

def _ccw(pts):
    a = sum((pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1])
            for i in range(len(pts)))
    return pts if a > 0 else pts[::-1]

def off_poly(pts, d):
    """offset a closed polygon in its own plane by d (positive = outward)"""
    p = _ccw(list(pts)); n = len(p); out = []
    for i in range(n):
        a, b, c = p[i - 1], p[i], p[(i + 1) % n]
        e1 = (b[0] - a[0], b[1] - a[1]); e2 = (c[0] - b[0], c[1] - b[1])
        l1 = math.hypot(*e1) or 1e-9; l2 = math.hypot(*e2) or 1e-9
        n1 = (e1[1] / l1, -e1[0] / l1); n2 = (e2[1] / l2, -e2[0] / l2)   # outward for CCW
        bx = n1[0] + n2[0]; by = n1[1] + n2[1]
        bl = math.hypot(bx, by)
        if bl < 1e-6: bx, by, bl = n1[0], n1[1], 1.0
        cosh = max(.35, (bx * n1[0] + by * n1[1]) / bl)
        out.append((b[0] + d * bx / bl / cosh, b[1] + d * by / bl / cosh))
    return out

def round_poly(pts, r, seg=9):
    """round every corner of a closed polygon with a quadratic fillet of radius ~r"""
    p = _ccw(list(pts)); n = len(p); out = []
    for i in range(n):
        a, b, c = p[i - 1], p[i], p[(i + 1) % n]
        v1 = (a[0] - b[0], a[1] - b[1]); v2 = (c[0] - b[0], c[1] - b[1])
        l1 = math.hypot(*v1) or 1e-9; l2 = math.hypot(*v2) or 1e-9
        d = min(r, l1 * .45, l2 * .45)
        s = (b[0] + v1[0] / l1 * d, b[1] + v1[1] / l1 * d)
        e = (b[0] + v2[0] / l2 * d, b[1] + v2[1] / l2 * d)
        for k in range(seg + 1):
            u = k / seg; w = (1 - u)
            out.append((w * w * s[0] + 2 * w * u * b[0] + u * u * e[0],
                        w * w * s[1] + 2 * w * u * b[1] + u * u * e[1]))
    return out

def xz_to_xt(pts, side):
    """wrap a side-view outline onto the skin; z is clamped just clear of the crown and keel"""
    out = []
    for (xx, zz) in pts:
        xx = min(max(xx, X_NOSE * .4), L - .02)
        zz = min(max(zz, ZBOT(xx) + .06), ZTOP(xx) - .090)
        out.append((xx, t_of_z(xx, zz, side)))
    return out

def cut_loop(name, loop_xt, dep, pad=.085, kind='cockpit', liploop=None):
    xs = [p[0] for p in loop_xt]; ts = [p[1] for p in loop_xt]
    xc = (min(xs) + max(xs)) * .5; tc = (min(ts) + max(ts)) * .5
    S = ring(xc)[1]
    CUTS.append(dict(name=name, xc=xc, tc=tc, ax=(max(xs) - min(xs)) * .5,
                     at=(max(ts) - min(ts)) * .5, x0=min(xs), x1=max(xs), t0=min(ts), t1=max(ts),
                     r=.05, px=pad, pt=pad / S, S=S, kind=kind, loop=loop_xt, liploop=liploop,
                     dep=dep))
    return CUTS[-1]

# ---- shrinkwrap of a drawn outline onto the skin ---------------------------------------------
def _f_imp(P):
    """< 0 inside the skin, 0 on it: the superellipse of the ring at that station"""
    cz, hh, hw = ring(P[0])
    return abs(P[1] / hw) ** SE + abs((P[2] - cz) / hh) ** SE - 1.0

def _xt_of_point(P):
    """exact inverse of surf(): the (x, t) of a point that lies on the skin"""
    cz, hh, hw = ring(P[0])
    return (P[0], math.atan2(_p((P[2] - cz) / hh, SE * .5), _p(P[1] / hw, SE * .5)))

def skin_hit(C, d):
    """the point where the ray C + s*d (s > 0, C inside) leaves the skin"""
    n = math.sqrt(sum(q * q for q in d)) or 1e-9
    d = [q / n for q in d]
    lo, hi = 0.0, .05
    while hi < 8.0:
        if _f_imp([C[i] + hi * d[i] for i in range(3)]) > 0: break
        lo = hi; hi *= 1.6
    for _ in range(64):
        mid = (lo + hi) * .5
        if _f_imp([C[i] + mid * d[i] for i in range(3)]) < 0: lo = mid
        else: hi = mid
    s = (lo + hi) * .5
    return [C[i] + s * d[i] for i in range(3)]

def board(C, nvec, R, hint):
    """a flat drawing board R m in front of C: origin, u axis (along hint), v axis (up)"""
    nv = Vector(nvec).normalized(); h = Vector(hint)
    eu = (h - nv * h.dot(nv)).normalized()
    ev = nv.cross(eu)
    if ev.z < 0: ev = -ev
    return ([C[i] + R * nv[i] for i in range(3)], eu, ev)

def wrap_loop(w, pts_uv, side):
    """shrinkwrap a board outline onto the skin -> a closed loop in (x, t).
    side -1 = port (as drawn), +1 = starboard (mirrored across y = 0)"""
    C = w['C']; P0, eu, ev = w['board']; out = []
    for (u, v) in pts_uv:
        P = [P0[i] + eu[i] * u + ev[i] * v for i in range(3)]
        x, t = _xt_of_point(skin_hit(C, [P[i] - C[i] for i in range(3)]))
        out.append((x, math.pi - t) if side > 0 else (x, t))
    return out

def densify(pts, step=.012):
    out = []
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / step))
        for k in range(n):
            out.append((a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n))
    return out

def even_n(pts, n=56):
    """resample a closed 2-D outline to exactly n points, evenly by arc length, so two
    rings of different size still bridge one-to-one"""
    L = [0.0]
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        L.append(L[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    tot = L[-1]; out = []; j = 0
    for k in range(n):
        d = tot * k / n
        while L[j + 1] < d: j += 1
        a, b = pts[j], pts[(j + 1) % len(pts)]
        f = (d - L[j]) / max(1e-9, L[j + 1] - L[j])
        out.append((a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f))
    return out

def _loop_order(loop, S):
    """counter-clockwise in (x, t*S), starting at the lower-left corner - the order
    rr_points() uses, so the skin bridge and every frame ring pair up the same way"""
    a = sum(loop[i][0] * loop[(i + 1) % len(loop)][1] - loop[(i + 1) % len(loop)][0] * loop[i][1]
            for i in range(len(loop)))
    rev = a < 0
    l2 = loop[::-1] if rev else loop
    k = min(range(len(l2)), key=lambda i: l2[i][0] + l2[i][1] * S)
    return (rev, k)

def _apply_order(loop, order):
    rev, k = order
    l2 = loop[::-1] if rev else loop
    return l2[k:] + l2[:k]

# ---- the three windows are drawn in the SIDE VIEW, as real metres (x, z) -----------------------
# Measured off two licensed photographs (SOURCES.md P1, P2): the three windows are ONE band under
# ONE straight, level top line; the posts between them are hairlines, ~0.05 m of skin; the sill
# rises steadily aft; the panes are 0.66 / 0.58 / 0.51 m tall.  Each corner below is a point the
# glass edge really has to pass through, and ck_solve() finds the ray through the drawing board
# that lands exactly there on the nose skin, so the band is right whatever the skin does under it.
#
# WHERE THE BAND SITS.  The published band (doors.cockpit_windows, x 1.13-2.65, z 4.16-4.95) will
# not fit this fuselage: at x 1.13 the crown of the nose is z 4.235, so a 4.95 top line is 0.72 m
# above the skin and nothing taller than 0.07 m fits between the crown and the measured sill.  A
# level top line at z 4.95 first touches the skin at x 2.095 (the crown), so the whole band is set
# 1.02 m further aft, keeping every measured height, pane size, post width and sill slope.  Noted
# in SOURCES.md; the x_range in the data file is the one number that is not met.
CKTOP = 4.892                     # top edge of the GLASS; the frame outer edge lands on z 4.95
CKX0, CKZ0 = 2.208, 4.228         # forward bottom corner of the forward pane
CKSLOPE = .15132                  # sill rise per metre aft (z 4.17 -> 4.40 over the 1.52 m band)
def cksill(x):
    return CKZ0 + CKSLOPE * (x - CKX0)

def ck_x_at(z, yabs):
    """the station x at which the port skin at height z stands |y| out from the centreline -
    used for the windscreen's inboard edge, which runs just beside the crown of the nose"""
    def yy(x):
        cz, hh, hw = ring(x)
        s = abs((z - cz) / hh)
        return hw * max(0.0, 1.0 - s ** SE) ** (1.0 / SE) if s < 1.0 else 0.0
    lo, hi = .5, 12.0
    for _ in range(60):
        m = (lo + hi) * .5
        if yy(m) < yabs: lo = m
        else: hi = m
    return (lo + hi) * .5

CKWINS = [
    # W1 windscreen: inboard edge beside the crown (|y| 0.11 at the top line), outboard edge
    # vertical on the post, sill rising aft - the tallest of the three at 0.664 m.
    dict(nm='W1', C=(2.35, 0.0, 4.00), n=(0.0, -.62, .78), R=1.00, hint=(1, 0, 0), rr=.040,
         pad=.030, wiper=True,
         quad=[(CKX0, CKZ0), (2.562, cksill(2.562)), (2.562, CKTOP), (None, CKTOP)]),
    # W2 sliding (DV / escape) window: near-rectangular, its forward edge raked.
    dict(nm='W2', C=(2.90, 0.0, 4.30), n=(0.0, -.94, .34), R=1.10, hint=(1, 0, 0), rr=.040,
         pad=.046,
         quad=[(2.778, cksill(2.778)), (3.082, cksill(3.082)), (3.082, CKTOP), (2.728, CKTOP)]),
    # W3 fixed rear window: a trapezoid, its aft edge sloping forward so it is narrower at the foot.
    dict(nm='W3', C=(3.40, 0.0, 4.30), n=(0.0, -.95, .31), R=1.20, hint=(1, 0, 0), rr=.040,
         pad=.046,
         quad=[(3.248, cksill(3.248)), (3.482, cksill(3.482)), (3.612, CKTOP), (3.248, CKTOP)]),
]

def ck_meas(w, u, v):
    """(x, z) of the skin point the board point (u, v) projects to, port side"""
    P0, eu, ev = w['board']; C = w['C']
    P = [P0[i] + eu[i] * u + ev[i] * v for i in range(3)]
    q = skin_hit(C, [P[i] - C[i] for i in range(3)])
    return (q[0], q[2])

def ck_solve(w, tgt, guess):
    """Newton on the board coordinates until the projected point sits on the wanted (x, z)"""
    u, v = guess
    for _ in range(60):
        f = ck_meas(w, u, v); r = (f[0] - tgt[0], f[1] - tgt[1])
        if abs(r[0]) < 1e-5 and abs(r[1]) < 1e-5: break
        h = 5e-4
        fu = ck_meas(w, u + h, v); fv = ck_meas(w, u, v + h)
        J = ((fu[0] - f[0]) / h, (fv[0] - f[0]) / h, (fu[1] - f[1]) / h, (fv[1] - f[1]) / h)
        det = J[0] * J[3] - J[1] * J[2]
        if abs(det) < 1e-9: break
        du = (-r[0] * J[3] + r[1] * J[1]) / det
        dv = (-r[1] * J[0] + r[0] * J[2]) / det
        u += max(-.25, min(.25, du)); v += max(-.25, min(.25, dv))
    return (u, v)

CKCUTS = []
for w in CKWINS:
    w['board'] = board(w['C'], w['n'], w['R'], w['hint'])
    q = list(w['quad'])
    for i, p in enumerate(q):
        if p[0] is None: q[i] = (ck_x_at(p[1], .110), p[1])     # inboard edge, beside the crown
    w['tgt'] = even_n(densify(_ccw(round_poly(q, w['rr'])), .010), 72)
    w['base'] = []; g = (0.0, 0.0); err = 0.0
    for p in w['tgt']:
        g = ck_solve(w, p, g); w['base'].append(g)
        got = ck_meas(w, *g); err = max(err, abs(got[0] - p[0]), abs(got[1] - p[1]))
    if err > .003:
        raise SystemExit('cockpit %s: drawn outline misses the skin by %.1f mm' % (w['nm'], err * 1e3))
    # how many metres on the skin one metre on the drawing board is worth, so the frame band and
    # every lip offset below can be given in real metres and come out the same width all round
    P3 = [surf(*p) for p in wrap_loop(w, w['base'], -1)]
    pw = sum((P3[i] - P3[i - 1]).length for i in range(len(P3)))
    pu = sum(math.hypot(w['base'][i][0] - w['base'][i - 1][0], w['base'][i][1] - w['base'][i - 1][1])
             for i in range(len(w['base'])))
    w['scale'] = pw / max(pu, 1e-9)
    w['order'] = {}
    for sd, sg in (('L', -1), ('R', 1)):
        raw = wrap_loop(w, w['base'], sg)
        S = ring(sum(p[0] for p in raw) / len(raw))[1]
        w['order'][sg] = o = _loop_order(raw, S)
        hole = _apply_order(raw, o)
        lip = _apply_order(wrap_loop(w, off_poly(w['base'], -.008 / w['scale']), sg), o)
        c = cut_loop('Ck%s%s' % (w['nm'], sd), hole, .005, pad=w['pad'], liploop=lip)
        if sd == 'L':
            CKCUTS.append((c, w))
            P3 = [surf(x_, t_) for (x_, t_) in hole]
            w['bbox'] = (min(p.x for p in P3), max(p.x for p in P3), min(p.y for p in P3),
                         max(p.y for p in P3), min(p.z for p in P3), max(p.z for p in P3))
            print('A320NEO-STEP1 cockpit %s port glazing x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f'
                  % ((w['nm'],) + w['bbox']))
_CKALL = [c for c in CUTS if c['kind'] == 'cockpit']
for _a, _b in [(_CKALL[i], _CKALL[j]) for i in range(len(_CKALL)) for j in range(len(_CKALL))
               if i < j]:
    if (_a['x0'] - _a['px'] < _b['x1'] + _b['px'] and _b['x0'] - _b['px'] < _a['x1'] + _a['px']
            and _a['t0'] - _a['pt'] < _b['t1'] + _b['pt'] and _b['t0'] - _b['pt'] < _a['t1'] + _a['pt']):
        raise SystemExit('cockpit window cut-outs overlap: %s %s' % (_a['name'], _b['name']))

# ---------------------------------------------------------------------- the skin
def build_skin():
    # station spacing is set by the shading, not by the frames: no gap over 0.12 m anywhere and
    # none over 0.05 m forward of x 6.0, so the curved nose reads as one round form, not as bands
    x0 = round(X_SKIN0, 5)
    xs = set([x0, X_TAILEND])
    x = x0
    while x < 3.0: xs.add(round(x, 5)); x += .034
    while x < 6.0: xs.add(round(x, 5)); x += .048
    while x < 27.5: xs.add(round(x, 5)); x += .115
    while x <= X_TAILEND - .02: xs.add(round(x, 5)); x += .118
    for n, fx in FX.items():
        if x0 + .03 < fx < X_TAILEND - .03: xs.add(round(fx, 5))
    nt0 = 288
    ts = set(round(T0 + TAU * k / nt0, 6) for k in range(nt0))
    for c in CUTS:
        for f in (1.0, 2.2):
            for v in (c['x0'] - c['px'] * f, c['x1'] + c['px'] * f):
                xs.add(round(min(max(v, X_SKIN0 + .01), X_TAILEND - .01), 5))
            for v in (c['t0'] - c['pt'] * f, c['t1'] + c['pt'] * f):
                ts.add(round(v, 6))
    XSL = sorted(xs); TSL = sorted(ts)
    nx, ntt = len(XSL), len(TSL)
    V = [surf(xx, tt) for xx in XSL for tt in TSL]
    idx = lambda i, j: i * ntt + j
    rects = [(c['x0'] - c['px'], c['x1'] + c['px'], c['t0'] - c['pt'], c['t1'] + c['pt']) for c in CUTS]

    def skip(i, j):
        xm = (XSL[i] + XSL[i + 1]) * .5
        tm = (TSL[j] + (TSL[j + 1] if j < ntt - 1 else T1)) * .5
        for a, b, c_, d_ in rects:
            if a < xm < b and c_ < tm < d_: return True
        return False
    F = grid_faces(nx, ntt, 0, wrap_t=True, skip=skip)

    holes = {}
    for c in CUTS:
        i0 = XSL.index(round(c['x0'] - c['px'], 5)); i1 = XSL.index(round(c['x1'] + c['px'], 5))
        j0 = TSL.index(round(c['t0'] - c['pt'], 6)); j1 = TSL.index(round(c['t1'] + c['pt'], 6))
        bnd = []
        for i in range(i0, i1): bnd.append(idx(i, j0))
        for j in range(j0, j1): bnd.append(idx(i1, j))
        for i in range(i1, i0, -1): bnd.append(idx(i, j1))
        for j in range(j1, j0, -1): bnd.append(idx(i0, j))
        S = c['S']
        bxt = [(XSL[k // ntt], TSL[k % ntt]) for k in bnd]
        d = [0.0]
        for k in range(1, len(bxt) + 1):
            a, b = bxt[k - 1], bxt[k % len(bxt)]
            d.append(d[-1] + math.hypot(b[0] - a[0], (b[1] - a[1]) * S))
        fr = [v / d[-1] for v in d[:-1]]
        inner = resample(c.get('loop') or rr_points(c['xc'], c['tc'], c['ax'], c['at'], c['r'], S), fr, S)
        base = len(V)
        for (xx, tt) in inner: V.append(surf(xx, tt))
        iloop = list(range(base, base + len(inner)))
        F += bridge(bnd, iloop)
        dep = c.get('dep') or {'window': .052, 'cockpit': .055}.get(c['kind'], .085)
        lip = (resample(c['liploop'], fr, S) if c.get('liploop')
               else shrink(inner, c['xc'], c['tc'], 0.975))
        b2 = len(V)
        for (xx, tt) in lip: V.append(soff(xx, tt, -dep))
        F += bridge(iloop, list(range(b2, b2 + len(lip))))
        holes[c['name']] = dict(cut=c, depth=dep)
        excl(c['x0'] - c['px'], c['x1'] + c['px'], c['t0'] - c['pt'], c['t1'] + c['pt'], .03, .03)
    add('A320N_Fus_Skin', V, F, MT['paint'])
    return holes

def loop_mesh(V, F, pts_xt, d):
    b = len(V)
    for (x_, t_) in pts_xt: V.append(soff(x_, t_, d))
    return list(range(b, b + len(pts_xt)))

# ---------------------------------------------------------------------- windows
def build_windows(holes):
    for sd in ('L', 'R'):
        Vf, Ff, Vp, Fp = [], [], [], []
        for c, s2, n in WINCUTS:
            if s2 != sd: continue
            h = holes[c['name']]; S = c['S']; dp = h['depth']
            fr = [k / 72.0 for k in range(72)]
            out_xt = resample(rr_points(c['xc'], c['tc'], c['ax'] * .982, c['at'] * .982, c['r'], S), fr, S)
            in_xt = resample(rr_points(c['xc'], c['tc'], .095, .17 / S, .055, S), fr, S)
            a1 = loop_mesh(Vf, Ff, out_xt, -dp + .004)
            a2 = loop_mesh(Vf, Ff, out_xt, -dp - .016)
            a3 = loop_mesh(Vf, Ff, in_xt, -dp - .016)
            a4 = loop_mesh(Vf, Ff, in_xt, -dp + .010)
            Ff += bridge(a1, a2) + bridge(a2, a3) + bridge(a3, a4) + bridge(a4, a1)
            pane_xt = shrink(in_xt, c['xc'], c['tc'], 1.055)  # rim bites well into the frame wall
            q1 = loop_mesh(Vp, Fp, pane_xt, -dp - .006)      # one solid pane, joined to the frame
            q2 = loop_mesh(Vp, Fp, pane_xt, -dp - .030)
            Fp += bridge(q1, q2)
            cb = len(Vp); Vp.append(soff(c['xc'], c['tc'], -dp - .006)); Fp += fan(q1, cb, True)
            cb = len(Vp); Vp.append(soff(c['xc'], c['tc'], -dp - .030)); Fp += fan(q2, cb)
        add('A320N_Fus_WindowFrame_' + sd, Vf, Ff, MT['grey'])
        add('A320N_Fus_WindowPane_' + sd, Vp, Fp, MT['glass'])

def slab_on_surface(V, F, xs, tfun, dout, din, nt):
    """a solid slab following the skin over the stations xs; tfun(x, u in 0..1) gives the ring
    angle, so a band can keep a constant width in metres where the section radius changes"""
    nx = len(xs) - 1
    m0 = len(V)
    for xx in xs:
        for q in range(nt + 1): V.append(soff(xx, tfun(xx, q / nt), dout))
    m1 = len(V)
    for xx in xs:
        for q in range(nt + 1): V.append(soff(xx, tfun(xx, q / nt), din))
    F += grid_faces(nx + 1, nt + 1, m0)
    F += [f[::-1] for f in grid_faces(nx + 1, nt + 1, m1)]
    for k in range(nx):
        F.append([m0 + k * (nt + 1), m0 + (k + 1) * (nt + 1), m1 + (k + 1) * (nt + 1), m1 + k * (nt + 1)])
        F.append([m0 + k * (nt + 1) + nt, m1 + k * (nt + 1) + nt,
                  m1 + (k + 1) * (nt + 1) + nt, m0 + (k + 1) * (nt + 1) + nt])
    for q in range(nt):
        F.append([m0 + q, m1 + q, m1 + q + 1, m0 + q + 1])
        F.append([m0 + nx * (nt + 1) + q, m0 + nx * (nt + 1) + q + 1,
                  m1 + nx * (nt + 1) + q + 1, m1 + nx * (nt + 1) + q])


def box_on_surface(V, F, xa, xb, ta, tb, dout, din, nx=10, nt=12):
    """a solid slab following the skin between two stations and two ring angles"""
    xs = [xa + (xb - xa) * k / nx for k in range(nx + 1)]
    slab_on_surface(V, F, xs, lambda xx, u: ta + (tb - ta) * u, dout, din, nt)


def mirror_y(ob, newname):
    """exact mirror of a port part across y = 0 (what a mirror modifier + apply does)"""
    me = ob.data.copy(); me.name = newname
    bm = bmesh.new(); bm.from_mesh(me)
    for v in bm.verts: v.co.y = -v.co.y
    bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    nb = bpy.data.objects.new(newname, me)
    nb.matrix_world = ob.matrix_world.copy()
    bpy.context.collection.objects.link(nb)
    for md in ob.modifiers:
        if md.type == 'BEVEL':
            m2 = nb.modifiers.new('bev', 'BEVEL'); m2.width = md.width; m2.segments = md.segments
            m2.limit_method = md.limit_method; m2.angle_limit = md.angle_limit
    OBJS.append(nb); return nb

def box_in_xz(V, F, xa, xb, zlo, zhi, side, dout, din, nx=8, nz=4):
    """a solid slab on the skin bounded in the side view by x and z"""
    m0 = len(V)
    for d in (dout, din):
        for k in range(nx + 1):
            xx = xa + (xb - xa) * k / nx
            for q in range(nz + 1):
                zz = zlo + (zhi - zlo) * q / nz
                V.append(soff(xx, t_of_z(xx, zz, side), d))
    m1 = m0 + (nx + 1) * (nz + 1)
    F += grid_faces(nx + 1, nz + 1, m0)
    F += [f[::-1] for f in grid_faces(nx + 1, nz + 1, m1)]
    for k in range(nx):
        F.append([m0 + k * (nz + 1), m0 + (k + 1) * (nz + 1), m1 + (k + 1) * (nz + 1), m1 + k * (nz + 1)])
        F.append([m0 + k * (nz + 1) + nz, m1 + k * (nz + 1) + nz,
                  m1 + (k + 1) * (nz + 1) + nz, m0 + (k + 1) * (nz + 1) + nz])
    for q in range(nz):
        F.append([m0 + q, m1 + q, m1 + q + 1, m0 + q + 1])
        F.append([m0 + nx * (nz + 1) + q, m0 + nx * (nz + 1) + q + 1,
                  m1 + nx * (nz + 1) + q + 1, m1 + nx * (nz + 1) + q])

def ck_ring(x, t, r, d, n=12):
    """n points of a circle of radius r round (x, t) on the skin, d proud of it"""
    S = ring(x)[1]
    return [soff(x + r * math.cos(TAU * k / n), t + r * math.sin(TAU * k / n) / S, d)
            for k in range(n)]

def ck_rivets(V, F, w, loop):
    """a rivet row along the top edge and down both posts (never along the lower sill)"""
    for i in range(0, len(loop), 2):
        x_, t_ = loop[i]
        if surf(x_, t_).z < cksill(x_) + .055: continue
        b = len(V)
        V += ck_ring(x_, t_, .0052, .0008, 10)
        V += ck_ring(x_, t_, .0030, .0030, 10)
        V.append(soff(x_, t_, .0036))
        a = list(range(b, b + 10)); c2 = list(range(b + 10, b + 20))
        F += bridge(a, c2) + fan(c2, b + 20)

CKWIPER_LEN = .460           # parked arm, pivot to blade tip, along the lower sill
def ck_wiper_path(w, n=11):
    """the parked wiper path: horizontal along the sill, from the inboard bottom corner aft"""
    x0 = w['quad'][0][0] + .030
    out = []
    for k in range(n):
        x_ = x0 + CKWIPER_LEN * k / (n - 1)
        out.append((x_, t_of_z(x_, cksill(x_) - CKFRAME - .042, -1)))
    return out

def ck_sweep(V, F, path, hw0, hw1, d0, d1a, d1b):
    """a swept square section along a (x, t) path: half width hw0 -> hw1, from d0 up to d1"""
    rings = []
    for k, (x_, t_) in enumerate(path):
        f = k / (len(path) - 1.0); S = ring(x_)[1]
        hw = (hw0 + (hw1 - hw0) * f) / S; d1 = d1a + (d1b - d1a) * f
        b = len(V)
        V += [soff(x_, t_ - hw, d0), soff(x_, t_ + hw, d0),
              soff(x_, t_ + hw, d1), soff(x_, t_ - hw, d1)]
        rings.append([b, b + 1, b + 2, b + 3])
    for a, b in zip(rings, rings[1:]): F += bridge(a, b)
    F.append(rings[0][::-1]); F.append(list(rings[-1]))

def ck_wiper_boss(V, F, w):
    """the wiper pivot boss, built into the window frame so nothing stands loose on the skin"""
    x_, t_ = ck_wiper_path(w)[0]
    b = len(V)
    V += ck_ring(x_, t_, .030, .000, 14)
    V += ck_ring(x_, t_, .026, .020, 14)
    V.append(soff(x_, t_, .020))
    a = list(range(b, b + 14)); c2 = list(range(b + 14, b + 28))
    F += bridge(a, c2) + fan(c2, b + 28)

def build_cockpit_windows(holes):
    """PORT side only, then mirrored across y = 0 - the two sides are identical by construction.
    Each window: a narrow 58 mm frame band standing 12 mm proud of the skin, corners rounded on
    0.04 m, a rivet row along the top edge and down both posts, and the pane seated under the
    frame shelf (3 mm inside the skin, so no glass stands out).  Window 1 also carries a wiper."""
    port = []
    for c, w in CKCUTS:
        S = c['S']; nm = w['nm']
        def P(d, w=w):
            """the window outline offset by d REAL metres on the skin, as a closed (x, t) loop"""
            return _apply_order(wrap_loop(w, off_poly(w['base'], d / w['scale']), -1),
                                w['order'][-1])
        V, F = [], []
        a1 = loop_mesh(V, F, P(CKFRAME), .000)            # sits flat on the skin
        a2 = loop_mesh(V, F, P(CKFRAME - .006), CKPROUD)  # bevelled outer edge of the lip
        a3 = loop_mesh(V, F, P(.012), CKPROUD)            # flat top of the lip
        a4 = loop_mesh(V, F, P(-.005), CKPROUD - .007)    # bevel down to the glass
        a5 = loop_mesh(V, F, P(-.005), -.036)             # inner wall, down past the pane
        a6 = loop_mesh(V, F, P(.016), -.036)              # the shelf the pane beds on
        a7 = loop_mesh(V, F, P(.016), -.056)
        a8 = loop_mesh(V, F, P(CKFRAME), -.056)           # flange behind the skin
        F += (bridge(a1, a2) + bridge(a2, a3) + bridge(a3, a4) + bridge(a4, a5)
              + bridge(a5, a6) + bridge(a6, a7) + bridge(a7, a8) + bridge(a8, a1))
        if nm == 'W2':
            # the sliding window's handle: one solid with the frame (its base sits inside the
            # lip, so nothing floats and no two faces lie on each other)
            uc, vc = ck_solve(w, (3.000, cksill(3.000) + .085), w['base'][0])
            hu, hv = .038 / w['scale'], .019 / w['scale']
            rings = []
            for sc, hgt in ((1.00, CKPROUD - .006), (1.00, CKPROUD + .020), (.78, CKPROUD + .032)):
                hq = _ccw(round_poly([(uc - hu * sc, vc - hv * sc), (uc + hu * sc, vc - hv * sc),
                                      (uc + hu * sc, vc + hv * sc), (uc - hu * sc, vc + hv * sc)],
                                     .009 / w['scale']))
                rings.append(loop_mesh(V, F, wrap_loop(w, even_n(hq, 56), -1), hgt))
            F += bridge(rings[0], rings[1]) + bridge(rings[1], rings[2])
            for r, flip in ((rings[0], False), (rings[2], True)):
                cb = len(V)
                V.append(sum((V[i] for i in r), Vector((0, 0, 0))) / len(r))
                F += fan(r, cb, flip)
        ck_rivets(V, F, w, P(CKFRAME + .019))
        if w.get('wiper'):
            ck_wiper_boss(V, F, w)
        port.append(add('A320N_Cockpit_WindowFrame_%s_L' % nm, V, F, MT['grey'], bevel=.0025))
        V, F = [], []                               # the pane: 22 mm thick, under the frame shelf
        pane = P(.006)                      # the rim beds under the frame shelf
        xs = [q[0] for q in pane]; ts = [q[1] for q in pane]
        pxc = sum(xs) / len(xs); ptc = sum(ts) / len(ts)
        o = disc_on_surface(V, F, pane, pxc, ptc, -.020)   # follows the nose contour, so the
        i = disc_on_surface(V, F, pane, pxc, ptc, -.042, flip=True)   # glass never breaks the skin
        F += bridge(o, i, flip=True)
        port.append(add('A320N_Cockpit_WindowPane_%s_L' % nm, V, F, MT['glass']))
        if w.get('wiper'):
            # one wiper per windscreen, parked horizontal along the sill.  Its root sits 4 mm
            # down inside the pivot boss (part of the frame), so the arm is really carried.
            pth = ck_wiper_path(w)
            V, F = [], []
            ck_sweep(V, F, pth, .016, .009, .016, .044, .030)          # the arm
            ck_sweep(V, F, pth[4:], .007, .007, .003, .018, .018)      # the blade under it
            port.append(add('A320N_Cockpit_WiperArm_%s_L' % nm, V, F, MT['grey'], bevel=.002))
    # the sliding window's rails and handle are part of its own frame solid (above), so
    # nothing stands loose on the skin and no two faces lie on each other.
    for ob in port:
        mirror_y(ob, ob.name[:-1] + 'R')

# ---------------------------------------------------------------------- doors
def disc_on_surface(V, F, loop_xt, xc, tc, d, flip=False):
    rings = [1.0, .74, .46, .20]
    idxs = []
    for s in rings:
        pts = shrink(loop_xt, xc, tc, s)
        idxs.append(loop_mesh(V, F, pts, d))
    cb = len(V); V.append(soff(xc, tc, d))
    for a, b in zip(idxs, idxs[1:]): F += bridge(a, b, flip)
    F += fan(idxs[-1], cb, not flip)
    return idxs[0]

def build_doors(holes):
    for dd in DOORS:
        c = dd['cut']; h = holes[c['name']]; S = c['S']; did = dd['id']; dp = h['depth']
        fr = [k / 64.0 for k in range(64)]
        hole_xt = resample(rr_points(c['xc'], c['tc'], c['ax'], c['at'], c['r'], S), fr, S)
        # a leaf that stays shut (the overwing exits) sits INSIDE the recess: its outline
        # clears the turned-in lip of the skin and its face is 6 mm under the skin, so the
        # hatch is flush to look at and no two faces lie on each other.
        shut = dd['angle'] == 0
        lsh = .045 if shut else .006
        leaf_xt = resample(rr_points(c['xc'], c['tc'], c['ax'] - lsh, c['at'] - lsh / S, c['r'], S), fr, S)
        lz = -.006 if shut else 0.0
        th = dd['thick']

        # frame channel inside the recess
        V, F = [], []
        f1 = loop_mesh(V, F, hole_xt, -dp + .002)
        f2 = loop_mesh(V, F, hole_xt, -dp - .055)
        f3 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .962), -dp - .055)
        f4 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .962), -dp + .002)
        F += bridge(f1, f2) + bridge(f2, f3) + bridge(f3, f4) + bridge(f4, f1)
        add('A320N_Door_%s_Frame' % did, V, F, MT['alu'])

        # sill plate across the bottom of the opening (a shut hatch fills the opening itself,
        # so it gets no sill - nothing to stand on and nothing for the leaf to clash with)
        if not shut:
            V, F = [], []
            tb = c['t0'] + .022
            box_on_surface(V, F, c['x0'] + .02, c['x1'] - .02, tb, tb + .034, -dp + .004, -dp - .050, 14, 3)
            add('A320N_Door_%s_Sill' % did, V, F, MT['steel'], bevel=.003)

        # the bay behind the opening so nothing is see-through
        V, F = [], []
        back = {'cargo': .95, 'service': .30}.get(dd['kind'], .62)
        w1 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .96), -dp - .055)
        w2 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .96), -dp - .055 - back)
        F += bridge(w1, w2)
        cb = len(V); V.append(soff(c['xc'], c['tc'], -dp - .055 - back)); F += fan(w2, cb)
        add('A320N_Door_%s_Bay' % did, V, F, MT['bay'])
        # inner lining ring (frame return) and the floor of the bay - no view into nothing
        V, F = [], []
        r1 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .962), -dp - .052)
        r2 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .900), -dp - .052)
        r3 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .900), -dp - .052 - back * .92)
        r4 = loop_mesh(V, F, shrink(hole_xt, c['xc'], c['tc'], .962), -dp - .052 - back * .92)
        F += bridge(r1, r2) + bridge(r2, r3) + bridge(r3, r4)
        add('A320N_Door_%s_Lining' % did, V, F, MT['grey'])


        # leaf: outer skin following the fuselage, edge band, inner skin
        V, F = [], []
        o_loop = disc_on_surface(V, F, leaf_xt, c['xc'], c['tc'], lz)
        i_loop = disc_on_surface(V, F, leaf_xt, c['xc'], c['tc'], lz - th, flip=True)
        F += bridge(o_loop, i_loop, flip=True)
        leaf = add('A320N_Door_%s_Leaf' % did, V, F, MT['paint'], bevel=.006)

        # rubber seal round the back edge of the leaf
        V, F = [], []
        sect = []
        for k in range(8):
            a = k / 8.0 * math.tau
            sect.append(loop_mesh(V, F, shrink(leaf_xt, c['xc'], c['tc'],
                                               .990 + .012 * math.cos(a)),
                                  lz - th - .004 + .014 * math.sin(a)))
        for k in range(8):
            F += bridge(sect[k], sect[(k + 1) % 8])
        seal = add('A320N_Door_%s_Seal' % did, V, F, MT['rubber'])

        # handle on the outer face: recessed grip with a lever in it
        hx = c['xc'] - (c['ax'] - .15) if dd['hinge'] == 'fwd' else c['xc']
        ht = c['tc'] + c['at'] * (-.52 if dd['hinge'] == 'top' else -.06)
        V, F = [], []
        hw_, hh_ = .115, .035 / S
        g1 = loop_mesh(V, F, rr_points(hx, ht, hw_, hh_, .022, S, 80), lz + .004)
        g2 = loop_mesh(V, F, rr_points(hx, ht, hw_ * .80, hh_ * .70, .018, S, 80), lz - .040)
        F += bridge(g1, g2)
        cb = len(V); V.append(soff(hx, ht, lz - .040)); F += fan(g2, cb)
        l1 = loop_mesh(V, F, rr_points(hx, ht, hw_ * .70, hh_ * .40, .014, S, 80), lz - .012)
        l2 = loop_mesh(V, F, rr_points(hx, ht, hw_ * .70, hh_ * .40, .014, S, 80), lz - .030)
        F += bridge(l1, l2)
        cb = len(V); V.append(soff(hx, ht, lz - .030)); F += fan(l2, cb)
        cb = len(V); V.append(soff(hx, ht, lz - .012)); F += fan(l1, cb, True)
        handle = add('A320N_Door_%s_Handle' % did, V, F, MT['steel'], bevel=.003)

        swing = [leaf, seal, handle]

        if dd['bustle']:      # slide bustle on the inside of the passenger doors
            V, F = [], []
            btc = c['tc'] - c['at'] * .40
            bxt = resample(rr_points(c['xc'], btc, c['ax'] * .80, c['at'] * .46, .09, S),
                           [k / 48.0 for k in range(48)], S)
            b1 = loop_mesh(V, F, bxt, lz - th - .004)
            b2 = loop_mesh(V, F, shrink(bxt, c['xc'], btc, .90), lz - th - .215)
            F += bridge(b1, b2, True)
            cb = len(V); V.append(soff(c['xc'], btc, lz - th - .215)); F += fan(b2, cb, True)
            cb = len(V); V.append(soff(c['xc'], btc, lz - th - .004)); F += fan(b1, cb)
            swing.append(add('A320N_Door_%s_Bustle' % did, V, F, MT['grey'], bevel=.005))

        if dd['window']:      # a window in the passenger doors and the overwing hatches
            wx = c['xc']; wt = c['tc'] + (c['at'] * .42 if dd['kind'] == 'pax' else 0.0)
            ww, wh = (.105, .17 / S) if dd['kind'] == 'pax' else (.095, .15 / S)
            V, F = [], []
            o1 = loop_mesh(V, F, rr_points(wx, wt, ww + .028, wh + .028 / S, .05, S, 120), lz + .006)
            o2 = loop_mesh(V, F, rr_points(wx, wt, ww, wh, .04, S, 120), lz - .026)
            F += bridge(o1, o2)
            swing.append(add('A320N_Door_%s_WindowFrame' % did, V, F, MT['grey'], bevel=.003))
            V, F = [], []
            p = loop_mesh(V, F, rr_points(wx, wt, ww + .010, wh + .010 / S, .04, S, 120), lz - .026)
            cb = len(V); V.append(soff(wx, wt, lz - .026)); F += fan(p, cb)
            swing.append(add('A320N_Door_%s_WindowPane' % did, V, F, MT['glass']))

        # hinge: a bracket standing off the skin, a pin, and a gooseneck arm onto the leaf.
        # the arm's collar is coaxial with the pin, so the open door stays joined to the fuselage.
        stand = .215 if dd['kind'] == 'pax' else (th * .55 + .085)
        if dd['hinge'] == 'fwd':
            px = c['x0'] - .018
            ax = (surf(px, c['t1']) - surf(px, c['t0'])).normalized()
            if dd['side'] == 0: ax = Vector((0, 1, 0))
            pivot = soff(px, c['tc'], stand)
            fs = (.17, .83)
            pins = [(soff(px, c['t0'] + (c['t1'] - c['t0']) * f, stand),
                     soff(px + .012, c['t0'] + (c['t1'] - c['t0']) * f, -.020),
                     soff(c['x0'] + .075, c['t0'] + (c['t1'] - c['t0']) * f, -th * .55)) for f in fs]
        else:
            pt = c['t1'] + .012
            ax = Vector((1, 0, 0))
            pivot = soff(c['xc'], pt, stand)
            fs = (.16, .84)
            pins = [(soff(c['x0'] + (c['x1'] - c['x0']) * f, pt, stand),
                     soff(c['x0'] + (c['x1'] - c['x0']) * f, pt + .006, -.020),
                     soff(c['x0'] + (c['x1'] - c['x0']) * f, c['t1'] - .030, -th * .55)) for f in fs]
        for k, (pp, root, leafpt) in enumerate(pins):
            V, F = [], []                                     # fixed bracket: skin -> pin
            tube(V, F, [root, root + (pp - root) * .55, pp], [.044, .036, .034], 12)
            tube(V, F, [pp - ax * .070, pp + ax * .070], [.021, .021], 12)
            add('A320N_Door_%s_HingeBracket_%d' % (did, k + 1), V, F, MT['steel'])
            V, F = [], []                                     # gooseneck, swings with the leaf
            tube(V, F, [pp - ax * .052, pp + ax * .052], [.033, .033], 12)
            mid = (pp + leafpt) * .5 + (pp - root).normalized() * .055
            tube(V, F, [pp, mid, leafpt + (pp - leafpt).normalized() * .02, leafpt],
                 [.030, .028, .026, .034], 12)
            swing.append(add('A320N_Door_%s_HingeArm_%d' % (did, k + 1), V, F, MT['steel']))

        ang = dd['angle']
        if dd['hinge'] == 'fwd':
            ang = dd['angle'] * (1 if dd['side'] >= 0 else -1)
            if dd['side'] == 0: ang = -dd['angle']
        else:
            ang = dd['angle'] * (-1 if dd['side'] >= 0 else 1)
        for ob in swing:
            place_hinged(ob, pivot, ax, ang)

# ---------------------------------------------------------------------- radome, tail, fairings
def radome_xs(turn_deg=1.6, dxmax=.028, dxmin=.0006):
    """ring stations along the radome. The tip turns through nearly 90 deg in the first
    0.15 m, so even spacing facets it: step on so no ring-to-ring step turns the surface
    by more than turn_deg."""
    def tanv(xx, tt):
        v = surf(xx + .0015, tt) - surf(max(xx - .0015, 0.0), tt)
        return v.normalized()
    lim = math.radians(turn_deg)
    probes = (0.0, math.pi * .5, -math.pi * .5, math.pi)
    xs = [.0015]
    while xs[-1] < X_RADOME_END - dxmin:
        x = xs[-1]; d = dxmax
        while d > dxmin:
            if max(tanv(x + d, p).angle(tanv(x, p), 0.0) for p in probes) <= lim: break
            d *= .62
        xs.append(min(x + max(d, dxmin), X_RADOME_END))
    xs[-1] = X_RADOME_END
    return xs


def build_radome():
    V, F = [], []
    nt = 288
    ts = [T0 + TAU * q / nt for q in range(nt)]
    xs = radome_xs()
    for xx in xs:
        for tt in ts: V.append(surf(xx, tt))
    F += grid_faces(len(xs), nt, 0, wrap_t=True)
    cb = len(V); V.append(Vector((0, 0, surf(0, 0).z)))
    F += fan(list(range(0, nt)), cb, True)
    b = len(V)                                  # FR1 bulkhead closing the back of the radome
    for tt in ts: V.append(soff(X_RADOME_END, tt, -.004))
    cb = len(V); V.append(Vector((X_RADOME_END, 0, ring(X_RADOME_END)[0])))
    F += fan(list(range(b, b + nt)), cb)
    add('A320N_Fus_Radome', V, F, MT['grey'])

    # The radome joint. The real A320 latches are inside the radome and nothing of them shows;
    # outside there is only a hairline seam, so this is a 4 mm channel recessed 1.5 mm between
    # the aft edge of the radome and the forward edge of the skin - nothing stands proud.
    V, F = [], []
    r0 = loop_mesh(V, F, [(X_RADOME_END, t) for t in ts], 0.0)
    r1 = loop_mesh(V, F, [(X_RADOME_END, t) for t in ts], -.0015)
    r2 = loop_mesh(V, F, [(X_SKIN0, t) for t in ts], -.0015)
    r3 = loop_mesh(V, F, [(X_SKIN0, t) for t in ts], 0.0)
    F += bridge(r0, r1) + bridge(r1, r2) + bridge(r2, r3)
    add('A320N_Fus_RadomeSeam', V, F, MT['alu'])

    # lightning diverter strips: crown and 35 deg either side of it, 8 mm wide, 3 mm proud,
    # from near the tip back to the seam (ref photo a320ref/nose-ezy.jpg)
    sxs = [v for v in xs if .105 <= v <= X_RADOME_END - .014]
    for nm, tc in (('Crown', math.pi * .5), ('R', math.radians(55)), ('L', math.radians(125))):
        def tf(xx, u, tc=tc):
            arc = (surf(xx, tc + .004) - surf(xx, tc - .004)).length / .008
            dt = .004 / max(arc, .05)
            return tc - dt + 2 * dt * u
        V, F = [], []
        slab_on_surface(V, F, sxs, tf, .003, -.004, 4)
        add('A320N_Fus_DiverterStrip_' + nm, V, F, MT['grey'], bevel=.0010)

def build_apu_and_tail():
    """blunt tail cone closing on the APU exhaust: tail.apu gives exhaust_x 37.57,
    exhaust_z 4.76-5.28 and the tail bottom at z 4.74 - so the cone ends full and high,
    it does not taper to a point."""
    ez0, ez1 = T['apu']['exhaust_z']; ex = T['apu']['exhaust_x']
    ez = (ez0 + ez1) * .5; rn = (ez1 - ez0) * .5
    V, F = [], []
    nt = 112
    ts = [2 * math.pi * q / nt for q in range(nt)]
    loops = []
    for xx in (X_TAILEND, 37.42, ex):
        b = len(V)
        for tt in ts: V.append(surf(xx, tt))
        loops.append(list(range(b, b + nt)))
    for a, b in zip(loops, loops[1:]): F += bridge(a, b)
    b = len(V)                                      # the rim between the skin and the nozzle
    for tt in ts: V.append(Vector((ex, rn * math.cos(tt), ez + rn * math.sin(tt))))
    F += bridge(loops[-1], list(range(b, b + nt)))
    add('A320N_Tail_TailCone', V, F, MT['paint'])
    V, F = [], []                                   # nozzle: lip rolled in, then the duct
    b = len(V)
    for tt in ts: V.append(Vector((ex, rn * math.cos(tt), ez + rn * math.sin(tt))))
    b2 = len(V)
    for tt in ts: V.append(Vector((ex - .040, rn * .93 * math.cos(tt), ez + rn * .93 * math.sin(tt))))
    b3 = len(V)
    for tt in ts: V.append(Vector((ex - .70, rn * .82 * math.cos(tt), ez + rn * .82 * math.sin(tt))))
    F += bridge(list(range(b, b + nt)), list(range(b2, b2 + nt)))
    F += bridge(list(range(b2, b2 + nt)), list(range(b3, b3 + nt)))
    cb = len(V); V.append(Vector((ex - .70, 0, ez))); F += fan(list(range(b3, b3 + nt)), cb)
    add('A320N_Tail_APUExhaust', V, F, MT['steel'])
    # APU access doors FR80-FR84, one each side, hinged at their lower edge and open
    xa, xb = FX[80], FX[84]
    for sd, sg in (('L', -1), ('R', 1)):
        xc = (xa + xb) * .5
        za = ring(xc)[0] + .22
        c = cut_xz('APUDoor' + sd, xc, xb - xa - .12, za, .58, sg, r=.08, pad=.07, kind='service')
        CUTS.remove(c)                                 # not a hole in the skin: a hinged access panel
        S = c['S']
        leaf_xt = resample(rr_points(c['xc'], c['tc'], c['ax'], c['at'], c['r'], S),
                           [k / 48.0 for k in range(48)], S)
        V, F = [], []
        o = disc_on_surface(V, F, leaf_xt, c['xc'], c['tc'], .004)
        i = disc_on_surface(V, F, leaf_xt, c['xc'], c['tc'], -.055, flip=True)
        F += bridge(o, i, flip=True)
        ob = add('A320N_Tail_APUDoor_' + sd, V, F, MT['paint'], bevel=.005)
        V, F = [], []
        box_on_surface(V, F, c['x0'], c['x1'], c['t0'] - .013, c['t0'] + .013, .0028, -.013, 10, 2)
        add('A320N_Tail_APUDoorHinge_' + sd, V, F, MT['steel'])
        place_hinged(ob, soff(c['xc'], c['t0'], .030), Vector((1, 0, 0)), 72 * (-1 if sg > 0 else 1))

# ---------------------------------------------------------------- fin, fin root fairing, tailplane
FIN = T['fin']; HTP = T['htp']
FIN_LE_R, FIN_LE_T = FIN['LE_root_xz'], FIN['LE_tip_xz']
FIN_TE_R, FIN_TE_T = FIN['TE_root_xz'], FIN['TE_tip_xz']
FIN_TIPZ = FIN['tip_z']; FIN_TOP = FIN['fin_top_ground_MRW_fwd']

def fin_le(z):
    if z <= FIN_LE_T[1]:
        u = max(0.0, (z - FIN_LE_R[1]) / (FIN_LE_T[1] - FIN_LE_R[1]))
        return FIN_LE_R[0] + (FIN_LE_T[0] - FIN_LE_R[0]) * u
    u = min(1.0, (z - FIN_LE_T[1]) / max(FIN_TIPZ - FIN_LE_T[1], 1e-6))
    return FIN_LE_T[0] + (FIN['tip_x_range'][0] - FIN_LE_T[0]) * u

def fin_te(z):
    u = max(0.0, min(1.0, (z - FIN_TE_R[1]) / (FIN_TE_T[1] - FIN_TE_R[1])))
    return FIN_TE_R[0] + (FIN_TE_T[0] - FIN_TE_R[0]) * u

def naca(u, th):
    u = max(0.0, min(1.0, u))
    return 5 * th * (.2969 * math.sqrt(u) - .126 * u - .3516 * u * u
                     + .2843 * u ** 3 - .1015 * u ** 4)

def fin_halft(x, z=None):
    """fin half thickness at station x on the root section"""
    z = FIN_LE_R[1] if z is None else z
    xle, xte = fin_le(z), fin_te(z)
    if not (xle < x < xte): return 0.0
    ch = xte - xle
    return max(naca((x - xle) / ch, .115 * ch * 1.18), .004)

def build_fin():
    """the whole fin from tail.fin: LE (28.68, 5.96) -> (34.78, 11.58), 42.6 deg sweep,
    TE (35.38, 5.65) -> (36.74, 11.78), tip chord 35.18-36.74, top of cap at 11.98"""
    nz, nc = 26, 34
    V, F = [], []
    zs = [5.80 + (FIN_TIPZ - 5.80) * (k / nz) for k in range(nz + 1)]
    for k, z in enumerate(zs):
        xle, xte = fin_le(z), fin_te(z)
        ch = xte - xle
        f = k / nz
        th = .115 * ch * (1.0 + .18 * math.exp(-f * 6.0))
        prof = []
        for q in range(nc):
            u = (1 - math.cos(math.pi * q / (nc - 1))) * .5
            prof.append((xle + ch * u, max(naca(u, th), .004)))
        for sg in (1, -1):
            for (xx, yt) in (prof if sg > 0 else prof[::-1]):
                y = sg * yt
                if k == 0:
                    zz = z_top_at(xx, y) - .010
                else:
                    zz = max(z, z_top_at(xx, y) + .004)
                V.append(Vector((xx, y, zz)))
    per = nc * 2
    for k in range(nz):
        F += bridge([k * per + q for q in range(per)], [(k + 1) * per + q for q in range(per)])
    tip = [nz * per + q for q in range(per)]
    xle, xte = fin_le(FIN_TIPZ), fin_te(FIN_TIPZ)
    cx = xle + (xte - xle) * .45
    caps = []
    for f, hgt in ((.70, .55), (.38, .85), (.0, 1.0)):
        b = len(V)
        for q in range(per):
            vx = V[tip[q]]
            V.append(Vector((cx + (vx.x - cx) * (.52 + .48 * f), vx.y * f,
                             FIN_TIPZ + (FIN_TOP - FIN_TIPZ) * hgt)))
        caps.append(list(range(b, b + per)))
    F += bridge(tip, caps[0]) + bridge(caps[0], caps[1]) + bridge(caps[1], caps[2])
    add('A320N_Tail_Fin', V, F, MT['paint'], bevel=.004)

def build_fin_root_fairing():
    """fillet from the crown into the fin side, along tail.fin.root_fairing_line
    (30.2, 5.96) -> (37.57, 5.28).  Radius fades to nothing at both ends: no step."""
    (fx0, _), (fx1, _) = FIN['root_fairing_line']
    xa, xb = FIN_LE_R[0] - .80, 37.00
    def rad(xx):
        if xx <= fx0:
            u = max(0.0, (xx - xa) / (fx0 - xa))
            return .005 + .45 * (u * u * (3 - 2 * u))
        u = min(1.0, (xx - fx0) / (xb - fx0))
        return .005 + .45 * (1 - u * u * (3 - 2 * u)) ** .80
    nx, na = 70, 12
    for sd, sg in (('L', -1), ('R', 1)):
        V, F = [], []
        for inner in (0, 1):
            for i in range(nx + 1):
                xx = xa + (xb - xa) * i / nx
                r0 = rad(xx)
                r = r0 + (.008 if inner else 0.0)
                yf = fin_halft(xx)
                zc = z_top_at(xx, min(yf + r0, 1.80) * sg)
                for j in range(na + 1):
                    a = math.pi * .5 * j / na
                    V.append(Vector((xx, sg * (yf + r0 - r * math.sin(a)),
                                     zc + r0 - r * math.cos(a))))
        m = (nx + 1) * (na + 1)
        F += grid_faces(nx + 1, na + 1, 0)
        F += [f[::-1] for f in grid_faces(nx + 1, na + 1, m)]
        for i in range(nx):
            F.append([i * (na + 1), (i + 1) * (na + 1), m + (i + 1) * (na + 1), m + i * (na + 1)])
            F.append([i * (na + 1) + na, m + i * (na + 1) + na,
                      m + (i + 1) * (na + 1) + na, (i + 1) * (na + 1) + na])
        for j in range(na):
            F.append([j, m + j, m + j + 1, j + 1])
            F.append([nx * (na + 1) + j, nx * (na + 1) + j + 1,
                      m + nx * (na + 1) + j + 1, m + nx * (na + 1) + j])
        add('A320N_Tail_FinRootFairing_' + sd, V, F, MT['paint'], bevel=.005)

# ---- tailplane -----------------------------------------------------------------------------
HTP_DIH = math.radians(HTP.get('dihedral_deg_approx', 6))
HTP_TIPY = HTP['span'] * .5
HTP_Z0 = HTP['tip_ground']['MRW_fwd'] - HTP_TIPY * math.tan(HTP_DIH)
HTP_JOIN = 1.90          # where the root fairing hands over to the tailplane itself

def htp_le(y):
    (xr, yr), (xt, yt) = HTP['LE_root_xy'], HTP['LE_tip_xy']
    return xr + (xt - xr) * (abs(y) - yr) / (yt - yr)

def htp_te(y):
    (xr, yr), (xt, yt) = HTP['TE_root_xy'], HTP['TE_tip_xy']
    return xr + (xt - xr) * (abs(y) - yr) / (yt - yr)

def htp_section(y, nc=30):
    """closed section loop of the tailplane at |y| (over the top, back under the bottom)"""
    xle, xte = htp_le(y), htp_te(y)
    ch = xte - xle
    zc = HTP_Z0 + abs(y) * math.tan(HTP_DIH)
    th = .098 * ch
    pts = []
    for sgn in (1, -1):
        for q in range(nc):
            u = (1 - math.cos(math.pi * q / (nc - 1))) * .5
            if sgn < 0: u = 1 - u
            pts.append((xle + ch * u, zc + sgn * max(naca(u, th), .0035)))
    return pts, zc, ch

def build_htp():
    nc, ny = 30, 22
    for sd, sg in (('L', -1), ('R', 1)):
        V, F, loops = [], [], []
        for k in range(ny + 1):
            y = HTP_JOIN + (HTP_TIPY - HTP_JOIN) * k / ny
            pts, zc, ch = htp_section(y, nc)
            b = len(V)
            for (xx, zz) in pts: V.append(Vector((xx, sg * y, zz)))
            loops.append(list(range(b, b + len(pts))))
        for a, b in zip(loops, loops[1:]): F += bridge(a, b)
        pts, zc, ch = htp_section(HTP_TIPY, nc)
        cx = htp_le(HTP_TIPY) + ch * .42
        for f, out in ((.62, .030), (.0, .055)):
            b = len(V)
            for (xx, zz) in pts:
                V.append(Vector((cx + (xx - cx) * (1 - (1 - f) * .20), sg * (HTP_TIPY + out),
                                 zc + (zz - zc) * f)))
            loops.append(list(range(b, b + len(pts))))
            F += bridge(loops[-2], loops[-1])
        add('A320N_Tail_HTP_' + sd, V, F, MT['paint'], bevel=.004)

def build_htp_fillets():
    """curved root fairing each side: an ellipse at the fuselage skin lofted into the
    tailplane's own section at y = HTP_JOIN, so the root grows out of a fairing"""
    nc, K = 30, 18
    y0 = .70
    for sd, sg in (('L', -1), ('R', 1)):
        V, F, loops = [], [], []
        for k in range(K + 1):
            v = k / K
            g = v * v * (3 - 2 * v)
            y = y0 + (HTP_JOIN - y0) * v
            pts, zc, ch = htp_section(max(y, HTP_JOIN * .55), nc)
            cx = (pts[0][0] + pts[nc - 1][0]) * .5
            ex = abs(pts[nc - 1][0] - pts[0][0]) * .5 + .42 * (1 - g)
            ez = .34 * (1 - g) ** .9
            b = len(V); n = len(pts)
            for q in range(n):
                a = 2 * math.pi * q / n
                exx = cx - ex * math.cos(a)
                ezz = zc + ez * math.sin(a)
                V.append(Vector((pts[q][0] * g + exx * (1 - g), sg * y,
                                 pts[q][1] * g + ezz * (1 - g))))
            loops.append(list(range(b, b + n)))
        for a, b in zip(loops, loops[1:]): F += bridge(a, b)
        cb = len(V)
        xs = sum(V[i].x for i in loops[0]) / len(loops[0])
        zs = sum(V[i].z for i in loops[0]) / len(loops[0])
        V.append(Vector((xs, sg * y0, zs)))
        F += fan(loops[0], cb, flip=True)
        add('A320N_Tail_HTPRootFairing_' + sd, V, F, MT['paint'], bevel=.005)

def build_belly_fairing():
    """belly / wing-root fairing - one shell over the keel with a 5 mm panel step at its edge"""
    xa, xb = 10.40, 21.60
    tc0 = -math.pi / 2; ab = 1.30
    excl(xa, xb, tc0 - ab, tc0 + ab, .04, .04)
    nx, nt = 60, 46
    def off(xx, tt):
        A = 0.205 * max(0.0, 1 - ((xx - 16.3) / ((xb - xa) * .5)) ** 2) ** 0.80
        w = max(math.cos(math.pi * .5 * (tt - tc0) / ab), 0.0) ** 1.15
        fade = min(1.0, (xx - xa) / 1.10) * min(1.0, (xb - xx) / 2.20)
        fade = max(fade, 0.0) ** 1.4
        return .0016 + (.0036 + A * w) * fade
    V, F = [], []
    for i in range(nx + 1):
        xx = xa + (xb - xa) * i / nx
        for j in range(nt + 1):
            tt = tc0 - ab + 2 * ab * j / nt
            V.append(soff(xx, tt, off(xx, tt)))
    F += grid_faces(nx + 1, nt + 1, 0)
    m = len(V)
    for i in range(nx + 1):
        xx = xa + (xb - xa) * i / nx
        for j in range(nt + 1):
            tt = tc0 - ab + 2 * ab * j / nt
            V.append(soff(xx, tt, -.004))
    F += [f[::-1] for f in grid_faces(nx + 1, nt + 1, m)]
    for i in range(nx):
        F.append([i * (nt + 1), (i + 1) * (nt + 1), m + (i + 1) * (nt + 1), m + i * (nt + 1)])
        F.append([i * (nt + 1) + nt, m + i * (nt + 1) + nt,
                  m + (i + 1) * (nt + 1) + nt, (i + 1) * (nt + 1) + nt])
    for j in range(nt):
        F.append([j, m + j, m + j + 1, j + 1])
        F.append([nx * (nt + 1) + j, nx * (nt + 1) + j + 1,
                  m + nx * (nt + 1) + j + 1, m + nx * (nt + 1) + j])
    add('A320N_Fus_BellyFairing', V, F, MT['paint'], bevel=.004)

# ---------------------------------------------------------------------- joints, rivets, panels
def fastener(V, F, x, t, d, rr=.0042, ns=10):
    """a flush screw head: a shallow disc sunk into the panel with a slot across it"""
    c0 = soff(x, t, d); nv = nrm(x, t)
    e1 = Vector((1, 0, 0)).cross(nv)
    if e1.length < .2: e1 = Vector((0, 1, 0)).cross(nv)
    e1.normalize(); e2 = nv.cross(e1).normalized()
    b = len(V)
    for q in range(ns):
        a = 2 * math.pi * q / ns
        V.append(c0 + nv * .0003 + e1 * (rr * math.cos(a)) + e2 * (rr * math.sin(a)))
    b2 = len(V)
    for q in range(ns):
        a = 2 * math.pi * q / ns
        V.append(c0 - nv * .0006 + e1 * (rr * .80 * math.cos(a)) + e2 * (rr * .80 * math.sin(a)))
    F += bridge(list(range(b, b + ns)), list(range(b2, b2 + ns)))
    cb = len(V); V.append(c0 - nv * .0009); F += fan(list(range(b2, b2 + ns)), cb)

def dome(V, F, x, t, d, rr, rh, ns=8):
    c0 = soff(x, t, d); nv = nrm(x, t)
    e1 = Vector((1, 0, 0)).cross(nv)
    if e1.length < .2: e1 = Vector((0, 1, 0)).cross(nv)
    e1.normalize(); e2 = nv.cross(e1).normalized()
    b = len(V)
    for q in range(ns):
        a = 2 * math.pi * q / ns
        V.append(c0 + e1 * (rr * math.cos(a)) + e2 * (rr * math.sin(a)))
    b2 = len(V)
    for q in range(ns):
        a = 2 * math.pi * q / ns
        V.append(c0 + nv * (rh * .72) + e1 * (rr * .62 * math.cos(a)) + e2 * (rr * .62 * math.sin(a)))
    F += bridge(list(range(b, b + ns)), list(range(b2, b2 + ns)))
    cb = len(V); V.append(c0 + nv * rh)
    F += fan(list(range(b2, b2 + ns)), cb)

def strap(V, F, pts, half_t, S, rise=.0028, rivet_off=(-.6, .6), pitch=.060):
    """a raised lap/butt strap along a (x,t) polyline with its rivet rows merged in"""
    n = len(pts)
    ta, tb, ba, bb = [], [], [], []
    for (xx, tt) in pts:
        ta.append(len(V)); V.append(soff(xx, tt - half_t, rise))
        tb.append(len(V)); V.append(soff(xx, tt + half_t, rise))
        ba.append(len(V)); V.append(soff(xx, tt - half_t, -.0030))
        bb.append(len(V)); V.append(soff(xx, tt + half_t, -.0030))
    for k in range(n - 1):
        F.append([ta[k], ta[k + 1], tb[k + 1], tb[k]])
        F.append([ba[k], bb[k], bb[k + 1], ba[k + 1]])
        F.append([ta[k], ba[k], ba[k + 1], ta[k + 1]])
        F.append([tb[k], tb[k + 1], bb[k + 1], bb[k]])
    F.append([ta[0], tb[0], bb[0], ba[0]])
    F.append([ta[-1], ba[-1], bb[-1], tb[-1]])
    d = [0.0]
    for k in range(1, n):
        d.append(d[-1] + math.hypot(pts[k][0] - pts[k - 1][0], (pts[k][1] - pts[k - 1][1]) * S))
    s = pitch * .5
    while s < d[-1]:
        i = min(max(bisect.bisect_right(d, s) - 1, 0), n - 2)
        seg = d[i + 1] - d[i]; u = (s - d[i]) / seg if seg > 1e-12 else 0
        xx = pts[i][0] + (pts[i + 1][0] - pts[i][0]) * u
        tt = pts[i][1] + (pts[i + 1][1] - pts[i][1]) * u
        for f in rivet_off:
            dome(V, F, xx, tt + half_t * f, rise - .0002, .0024, .0007)
        s += pitch

def build_joints():
    build_section_joints()
    # longitudinal lap joints on the stringer lines, both sides
    for sd, sg in (('L', -1), ('R', 1)):
        for n, deg in enumerate(LAP_T_DEG):
            t = math.radians(deg)
            tt = t if sg > 0 else math.pi - t
            pts = []
            x = 2.6
            while x <= 32.5: pts.append((x, tt)); x += .25
            S = ring(12)[1]
            excl(2.6, 32.5, tt - .040, tt + .040, .0, .004)   # no frame line inside the strap
            for si, seg in enumerate(segments(pts, lambda q: q)):
                V, F = [], []
                strap(V, F, seg, .030 / S, S)
                add('A320N_Fus_LapJoint_%s_%d_%d' % (sd, n + 1, si + 1), V, F, MT['paint'])

def build_section_joints():
    # circumferential butt joints at the six barrel-section ends
    for fr in SECTION_JOINT_FR:
        xx = FX[fr]
        if not (X_NOSE + .2 < xx < X_TAILEND - .2): continue
        excl(xx - .042, xx + .042, T0, T1 - 1e-6, .004, .0)
        S = ring(xx)[1]
        nt = 150
        allts = [T0 + TAU * q / nt for q in range(nt + 1)]
        for si, ts in enumerate(segments(allts, lambda t_: (xx, t_))):
         V, F = [], []
         ta, tb, ba, bb = [], [], [], []
         for t_ in ts:
            ta.append(len(V)); V.append(soff(xx - .034, t_, .0028))
            tb.append(len(V)); V.append(soff(xx + .034, t_, .0028))
            ba.append(len(V)); V.append(soff(xx - .034, t_, -.0030))
            bb.append(len(V)); V.append(soff(xx + .034, t_, -.0030))
         for k in range(len(ts) - 1):
            F.append([ta[k], tb[k], tb[k + 1], ta[k + 1]])
            F.append([ba[k], ba[k + 1], bb[k + 1], bb[k]])
            F.append([ta[k], ta[k + 1], ba[k + 1], ba[k]])
            F.append([tb[k], bb[k], bb[k + 1], tb[k + 1]])
         F.append([ta[0], ba[0], bb[0], tb[0]])
         F.append([ta[-1], tb[-1], bb[-1], ba[-1]])
         step = max(1, int(round(.055 / (TAU * S / nt))))
         for rowdx in (-.019, .019):
            q = 0
            while q < len(ts):
                dome(V, F, xx + rowdx, ts[q], .0026, .0024, .0007)
                q += step
         add('A320N_Fus_SectionJoint_FR%d_%d' % (fr, si + 1), V, F, MT['paint'])

def build_frame_lines():
    """the faint skin line over every barrel frame FR1-FR87"""
    V, F = [], []
    nt = 120
    for n, xx in FX.items():
        if not (X_NOSE + .12 < xx < X_TAILEND - .12): continue
        if n in SECTION_JOINT_FR: continue
        allts = [T0 + TAU * q / nt for q in range(nt + 1)]
        for ts in segments(allts, lambda t_: (xx, t_)):
         a1 = loop_mesh(V, F, [(xx - .013, t) for t in ts], .0024)
         a2 = loop_mesh(V, F, [(xx + .013, t) for t in ts], .0024)
         b1 = loop_mesh(V, F, [(xx - .013, t) for t in ts], -.0030)
         b2 = loop_mesh(V, F, [(xx + .013, t) for t in ts], -.0030)
         for k in range(len(ts) - 1):
            F.append([a1[k], a2[k], a2[k + 1], a1[k + 1]])
            F.append([b1[k], b1[k + 1], b2[k + 1], b2[k]])
            F.append([a1[k], a1[k + 1], b1[k + 1], b1[k]])
            F.append([a2[k], b2[k], b2[k + 1], a2[k + 1]])
         F.append([a1[0], b1[0], b2[0], a2[0]])
         F.append([a1[-1], a2[-1], b2[-1], b1[-1]])
    add('A320N_Fus_FrameLines', V, F, MT['paint'])

def service_panel(name, xc, tc, w, h, material, screws=True):
    S = ring(xc)[1]
    excl(xc - w * .5, xc + w * .5, tc - (h * .5) / S, tc + (h * .5) / S, .035, .035)
    V, F = [], []
    out = resample(rr_points(xc, tc, w * .5, (h * .5) / S, min(w, h) * .18, S),
                   [k / 56.0 for k in range(56)], S)
    o1 = loop_mesh(V, F, out, .0035)
    o2 = loop_mesh(V, F, out, -.0010)
    F += bridge(o1, o2)
    ins = shrink(out, xc, tc, .86)
    i1 = loop_mesh(V, F, ins, .0028)
    F += bridge(o1, i1)
    cb = len(V); V.append(soff(xc, tc, .0028)); F += fan(i1, cb)
    if screws:
        for k in range(0, 56, 7):
            x_, t_ = out[k]
            fastener(V, F, xc + (x_ - xc) * .93, tc + (t_ - tc) * .93, .0028)
    return add(name, V, F, material, bevel=.002)

def build_panels():
    service_panel('A320N_Fus_PanelGroundPower', 3.60, t_of_z(3.60, 2.35, 1), .52, .34, MT['grey'])
    service_panel('A320N_Fus_PanelExtPowerAft', 4.60, t_of_z(4.60, 2.30, 1), .34, .26, MT['grey'])
    service_panel('A320N_Fus_PanelPotableWater', 27.00, t_of_z(27.00, 2.42, 1), .40, .34, MT['grey'])
    service_panel('A320N_Fus_PanelWaterDrain', 27.80, t_of_z(27.80, 2.30, 1), .26, .22, MT['grey'])
    service_panel('A320N_Fus_PanelHydraulic', 19.20, t_of_z(19.20, 2.05, -1), .42, .30, MT['grey'])
    service_panel('A320N_Fus_PanelOxygen', 5.90, t_of_z(5.90, 2.25, 1), .30, .24, MT['grey'])
    for sd, sg in (('L', -1), ('R', 1)):
        for n, xx in enumerate((2.85, 3.05, 3.25)):
            service_panel('A320N_Fus_StaticPort_%s_%d' % (sd, n + 1), xx, t_of_z(xx, 3.35, sg),
                          .085, .085, MT['steel'], screws=False)

def probe(name, x, t, length, dia, droop=12, material=None):
    excl(x - dia * 1.5, x + dia * 1.5, t - .030, t + .030, .035, .025)
    V, F = [], []
    base = surf(x, t); nv = nrm(x, t)
    axis = (nv * math.sin(math.radians(droop)) + Vector((-1, 0, 0)) * math.cos(math.radians(droop))).normalized()
    mast = base + nv * .055
    tube(V, F, [base - nv * .004, mast], [dia * 1.5, dia * 1.2], 12)
    tube(V, F, [mast - nv * .01, mast + axis * length * .85, mast + axis * length],
         [dia, dia * .9, dia * .45], 12)
    return add(name, V, F, material or MT['steel'])

def build_probes():
    for sd, sg, cnt in (('L', -1, 3), ('R', 1, 1)):
        for n in range(cnt):
            xx = 2.62 + n * .10
            probe('A320N_Fus_PitotProbe_%s_%d' % (sd, n + 1), xx, t_of_z(xx, 3.10 + n * .20, sg),
                  .215, .013, 8)
    for sd, sg, cnt in (('L', -1, 2), ('R', 1, 1)):
        for n in range(cnt):
            xx = 3.45 + n * .22
            t = t_of_z(xx, 3.55, sg)
            excl(xx - .085, xx + .065, t - .035, t + .035, .035, .030)
            base = surf(xx, t); nv = nrm(xx, t)
            V, F = [], []
            tube(V, F, [base - nv * .004, base + nv * .030, base + nv * .038],
                 [.030, .028, .022], 12)
            c = base + nv * .024
            b = len(V)
            for do in (.005, -.005):
                for (dx, dz) in ((-.075, .012), (.055, .018), (.055, -.018), (-.075, -.012)):
                    V.append(c + Vector((dx, 0, dz)) + nv * do)
            F += [[b, b + 1, b + 2, b + 3], [b + 4, b + 7, b + 6, b + 5],
                  [b, b + 4, b + 5, b + 1], [b + 1, b + 5, b + 6, b + 2],
                  [b + 2, b + 6, b + 7, b + 3], [b + 3, b + 7, b + 4, b]]
            add('A320N_Fus_AoAVane_%s_%d' % (sd, n + 1), V, F, MT['steel'], bevel=.002)
    for sd, sg in (('L', -1), ('R', 1)):
        probe('A320N_Fus_TATProbe_' + sd, 4.25, t_of_z(4.25, 2.95, sg), .105, .016, -28)

def blade_antenna(name, x, t, length=.34, height=.22, thick=.022, material=None):
    excl(x - length * .58, x + length * .58, t - .035, t + .035, .035, .030)
    V, F = [], []
    base = surf(x, t); nv = nrm(x, t)
    fwd = Vector((-1, 0, 0)); side = nv.cross(fwd)
    if side.length < .2: side = nv.cross(Vector((0, 0, 1)))
    side.normalize()
    prof = [(-.50, .00), (-.42, .55), (-.10, .95), (.26, 1.0), (.50, .40), (.50, .00)]
    n = len(prof)
    for s_ in (1, -1):
        for (u, v) in prof:
            V.append(base + fwd * (u * length) + nv * (v * height + .003)
                     + side * (thick * .5 * s_ * (1 - .45 * v)))
    b = len(V)
    for s_ in (1, -1):
        for (u, v) in prof:
            V.append(base + fwd * (u * length * 1.08) + nv * (-.005) + side * (thick * .9 * s_))
    F += bridge(list(range(0, n)), list(range(n, 2 * n)), closed=False)
    F += bridge(list(range(b, b + n)), list(range(b + n, b + 2 * n)), closed=False)
    F += bridge(list(range(0, n)), list(range(b, b + n)), True, closed=False)
    F += bridge(list(range(n, 2 * n)), list(range(b + n, b + 2 * n)), closed=False)
    F.append(list(range(0, n))[::-1]); F.append(list(range(n, 2 * n)))
    F.append(list(range(b, b + n))); F.append(list(range(b + n, b + 2 * n))[::-1])
    F.append([0, n, b + n, b]); F.append([n - 1, b + n - 1, b + 2 * n - 1, 2 * n - 1])
    return add(name, V, F, material or MT['paint'], bevel=.003)

def build_antennas():
    top = math.pi * .5; keel = -math.pi * .5
    blade_antenna('A320N_Fus_AntennaVHF1', 6.95, top, .42, .28)
    blade_antenna('A320N_Fus_AntennaVHF2', 9.60, keel, .42, .28)
    blade_antenna('A320N_Fus_AntennaATC1', 5.55, keel, .22, .14)
    blade_antenna('A320N_Fus_AntennaATC2', 12.10, top, .22, .14)
    blade_antenna('A320N_Fus_AntennaDME1', 24.20, keel, .20, .13)
    blade_antenna('A320N_Fus_AntennaDME2', 10.90, top, .20, .13)
    service_panel('A320N_Fus_AntennaGPS1', 7.90, top + .09, .22, .16, MT['grey'])
    service_panel('A320N_Fus_AntennaGPS2', 7.90, top - .09, .22, .16, MT['grey'])
    service_panel('A320N_Fus_AntennaSatcom', 20.40, top, .46, .30, MT['grey'])
    for sd, sg in (('L', -1), ('R', 1)):
        service_panel('A320N_Fus_AntennaRadioAlt_' + sd, 15.60, keel + sg * .22, .20, .16, MT['grey'])

def build_lights():
    """the two anti-collision beacons: housing, reflector, lens, screws round the glass"""
    for nm, xx, tt in (('Top', 17.50, math.pi * .5), ('Bottom', 9.80, -math.pi * .5)):
        base = surf(xx, tt); nv = nrm(xx, tt)
        excl(xx - .09, xx + .09, tt - .060, tt + .060, .035, .030)
        V, F = [], []
        tube(V, F, [base - nv * .004, base + nv * .020, base + nv * .034], [.085, .082, .066], 20)
        e1 = Vector((1, 0, 0)); e2 = nv.cross(e1).normalized()
        for q in range(0, 20, 3):
            a = 2 * math.pi * q / 20
            c0 = base + nv * .021 + e1 * (.074 * math.cos(a)) + e2 * (.074 * math.sin(a))
            b = len(V); nsr = 6; rr = .0040
            for z_ in range(nsr):
                aa = 2 * math.pi * z_ / nsr
                V.append(c0 + e1 * (rr * math.cos(aa)) + e2 * (rr * math.sin(aa)))
            b2 = len(V)
            for z_ in range(nsr):
                aa = 2 * math.pi * z_ / nsr
                V.append(c0 + nv * .0014 + e1 * (rr * .5 * math.cos(aa)) + e2 * (rr * .5 * math.sin(aa)))
            F += bridge(list(range(b, b + nsr)), list(range(b2, b2 + nsr)))
            cb = len(V); V.append(c0 + nv * .0016); F += fan(list(range(b2, b2 + nsr)), cb)
        add('A320N_Light_Beacon%s_Housing' % nm, V, F, MT['grey'])
        V, F = [], []
        tube(V, F, [base + nv * .032, base + nv * .062, base + nv * .085], [.062, .050, .026], 20)
        add('A320N_Light_Beacon%s_Lens' % nm, V, F, MT['lens'])

# ---------------------------------------------------------------------- run
def renders():
    import mathutils
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 3840, 2160
    w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (.05, .055, .065, 1)
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = 4.0; sun.rotation_euler = (math.radians(55), 0, math.radians(35))
    sc.collection.objects.link(sun)
    fill = bpy.data.objects.new('Fill', bpy.data.lights.new('Fill', 'AREA'))
    fill.data.energy = 120000; fill.data.size = 30; fill.location = (18, -40, 22)
    fill.rotation_euler = (math.radians(62), 0, math.radians(25)); sc.collection.objects.link(fill)
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('C'))
    sc.collection.objects.link(cam); sc.camera = cam
    out = os.path.join(ROOT, 'docs', 'Pictures', 'a320neo'); os.makedirs(out, exist_ok=True)
    for name, loc, look, lens in [
            ('A320neo-shell-side.png', (18.0, -72.0, 7.5), (18.6, 0, 4.4), 62),
            ('A320neo-shell-doors.png', (2.0, -10.5, 6.6), (5.4, 0, 4.2), 45),
            ('A320neo-shell-tail.png', (30.0, -16.0, 10.0), (32.0, 0, 5.8), 55),
            ('A320neo-shell-nose.png', (-7.8, -7.4, 5.8), (2.2, 0, 4.55), 55),
            # straight-on front: both windscreens, symmetric about the centreline
            ('A320neo-shell-front.png', (-26.0, 0.0, 4.60), (2.0, 0.0, 4.50), 180),
            # straight abeam, close up on the nose: windows 2 and 3 must show plainly
            ('A320neo-shell-nose-side.png', (1.95, -16.0, 4.47), (1.95, 0.0, 4.40), 135)]:
        cam.location = loc; cam.data.lens = lens
        d = mathutils.Vector(look) - mathutils.Vector(loc)
        cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = os.path.join(out, name)
        bpy.ops.render.render(write_still=True)
        print('rendered', name)

def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'; sc.unit_settings.scale_length = 1.0
    mats()
    build_apu_and_tail()          # first: it removes its own access-panel cuts from CUTS
    holes = build_skin()
    build_windows(holes)
    build_cockpit_windows(holes)
    build_doors(holes)
    build_radome()
    build_fin()
    build_fin_root_fairing()
    build_htp()
    build_htp_fillets()
    build_belly_fairing()
    build_panels()
    build_probes()
    build_antennas()
    build_lights()
    build_joints()
    build_frame_lines()
    bpy.context.view_layer.update()
    tris = 0
    for ob in OBJS:
        tris += sum(len(p.vertices) - 2 for p in ob.data.polygons)
        if not ob.data.materials or ob.data.materials[0] is None:
            raise SystemExit('empty material slot on ' + ob.name)
    print('A320NEO-STEP1 objects=%d tris=%d' % (len(OBJS), tris))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ART, 'A320neo.blend'))
    bpy.ops.export_scene.gltf(filepath=os.path.join(ART, 'A320neo.glb'), export_format='GLB',
                              export_apply=True, use_selection=False, export_yup=False)
    print('A320NEO-STEP1 saved')
    if '--render' in ARGS: renders()

main()
