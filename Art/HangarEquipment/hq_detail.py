# Finer pieces for the hangar carts (to-do 364 A, batch 2), on top of hq_lib.py: rounded sheet metal, hex bolts, weld
# beads, swept hoses with crimped fittings, tyres with tread on rims with wheel nuts, gauges with dial, glass and their own
# needle part, valves / switches / knobs as their own parts, plain labels (own pictures from make_detail_textures.py),
# and a towed cart's running gear (turntable front axle with the tow bar, rear axle welded to the frame, parking brake).
# Same conventions as hq_lib: z up, metres, +y = the side that faces the aircraft (the front, tow bar end).
from hq_lib import *
import hq_lib

DT = os.path.join(HERE, 'Textures')
FIT = set()          # material keys whose faces get their picture fitted face by face (labels, dials)


def detail_materials(prefix):
    materials(prefix)
    mat('BR', prefix + '_Brass', (0.78, 0.55, 0.24), 0.3, 1.0, 'Metal027')
    mat('AL', prefix + '_Aluminium', (0.80, 0.81, 0.83), 0.33, 1.0, 'Metal011', True)
    mat('HS', prefix + '_HoseRubber', (0.022, 0.022, 0.024), 0.55, 0.0, 'Rubber004', True)
    mat('TY', prefix + '_TyreRubber', (0.028, 0.028, 0.03), 0.9, 0.0, 'Rubber004', True)
    mat('KG', prefix + '_KnobGreen', (0.02, 0.32, 0.06), 0.35, 0.0, 'Plastic006')
    mat('KR', prefix + '_KnobRed', (0.6, 0.02, 0.015), 0.35, 0.0, 'Plastic006')
    mat('KB', prefix + '_KnobBlue', (0.02, 0.10, 0.45), 0.35, 0.0, 'Plastic006')
    mat('WH', prefix + '_PaintWhite', (0.78, 0.79, 0.78), 0.45, 0.0, 'Paint004')
    mat('BL', prefix + '_PaintBlue', (0.035, 0.12, 0.32), 0.45, 0.0, 'Paint004')
    for k, f in (('GF', 'HQ_GaugeFace.png'), ('LW', 'HQ_Label_Warning.png'), ('LV', 'HQ_Label_Volt.png'),
                 ('LI', 'HQ_Label_Info.png'), ('LN', 'HQ_Label_Gas.png')):
        mat(k, prefix + '_' + f[3:-4], (1, 1, 1), 0.35, 0.0, 'Plastic006', img=os.path.join(DT, f)); FIT.add(k)
    g = mat('GL', prefix + '_GaugeGlass', (0.9, 0.92, 0.95), 0.03, 0.0)
    g.node_tree.nodes['Principled BSDF'].inputs['Alpha'].default_value = 0.14
    g.blend_method = 'BLEND'; g.shadow_method = 'NONE'


def frame(p, n, up=(0, 0, 1)):
    """Matrix: z along n, origin at p (x level where it can be)."""
    z = Vector(n).normalized(); u = Vector(up)
    if abs(u.dot(z)) > 0.95: u = Vector((0, 1, 0))
    x = u.cross(z).normalized(); y = z.cross(x)
    M = Matrix.Identity(4)
    for i in range(3): M[i][0], M[i][1], M[i][2], M[i][3] = x[i], y[i], z[i], p[i]
    return M


class D(B):
    def lz(self, prof, m, M, n=24, rot=0.0, caps=True):
        """Profile [(r, h)] turned about local z of M."""
        o = bmesh.new(); rings = []
        for r, h in prof:
            rings.append([o.verts.new(M @ Vector((r * math.cos(rot + 2 * math.pi * k / n), r * math.sin(rot + 2 * math.pi * k / n), h))) for k in range(n)])
        for i in range(len(rings) - 1):
            for k in range(n): o.faces.new((rings[i][k], rings[i][(k + 1) % n], rings[i + 1][(k + 1) % n], rings[i + 1][k]))
        if caps and prof[0][0] > 1e-4: o.faces.new(list(reversed(rings[0])))
        if caps and prof[-1][0] > 1e-4: o.faces.new(rings[-1])
        self._take(o, MATS[m])

    def fbox(self, M, lo, hi, m, bevel=0.0, seg=2):
        o = bmesh.new(); bmesh.ops.create_cube(o, size=1.0)
        lo, hi = Vector(lo), Vector(hi)
        for v in o.verts:
            v.co = M @ Vector(((lo.x + hi.x) / 2 + v.co.x * (hi.x - lo.x), (lo.y + hi.y) / 2 + v.co.y * (hi.y - lo.y), (lo.z + hi.z) / 2 + v.co.z * (hi.z - lo.z)))
        if bevel > 0:
            bv = min(bevel, min(hi[i] - lo[i] for i in range(3)) * 0.3)
            bmesh.ops.bevel(o, geom=list(o.edges), offset=bv, segments=seg, profile=0.5, affect='EDGES', clamp_overlap=True)
        self._take(o, MATS[m])

    def sheet(self, lo, hi, m, r=0.012):
        """Folded sheet-metal box: rounded edges (3 segments) like a pressed and folded panel."""
        self.fbox(Matrix.Identity(4), lo, hi, m, r, 3)

    def bolt(self, p, n, d=0.013, m='CR'):
        """Hex bolt head with its washer at p, standing out along n."""
        M = frame(p, n)
        self.lz([(d * 1.05, -0.0006), (d * 1.05, 0.0015), (0, 0.0015)], m, M, 14)
        self.lz([(d * 0.82, 0.0010), (d * 0.82, 0.0015 + d * 0.6), (d * 0.6, 0.0015 + d * 0.7), (0, 0.0015 + d * 0.7)], m, M, 6, math.pi / 6)

    def weld(self, p0, p1, m='Y', r=0.0035):
        """A fillet weld bead along a joint (a slightly rippled round bead)."""
        self.tube(p0, p1, r, m, 6, weld=False)

    def hose(self, pts, r, m='HS', n=12, seg=6):
        """A hose swept along a smooth curve through pts (Catmull-Rom), round, capped."""
        P = [Vector(p) for p in pts]; P = [P[0] * 2 - P[1]] + P + [P[-1] * 2 - P[-2]]
        C = []
        for i in range(1, len(P) - 2):
            p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
            for s in range(seg):
                t = s / seg
                C.append(0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
        C.append(P[-2])
        o = bmesh.new(); rings = []; up = None
        for i, c in enumerate(C):
            t = (C[min(i + 1, len(C) - 1)] - C[max(i - 1, 0)]).normalized()
            if up is None: up = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            x = up.cross(t).normalized(); y = t.cross(x); up = y
            rings.append([o.verts.new(c + (x * math.cos(2 * math.pi * k / n) + y * math.sin(2 * math.pi * k / n)) * r) for k in range(n)])
        for i in range(len(rings) - 1):
            for k in range(n): o.faces.new((rings[i][k], rings[i][(k + 1) % n], rings[i + 1][(k + 1) % n], rings[i + 1][k]))
        o.faces.new(list(reversed(rings[0]))); o.faces.new(rings[-1])
        self._take(o, MATS[m])

    def bent_tube(self, pts, r, R, m='G', n=16, seg=10):
        """ONE continuous tube along the polyline pts with real pipe bends (bend radius R at every inner corner),
        round, capped: push handles, rails, frames bent from one tube instead of straight pieces meeting at a corner."""
        P = [Vector(p) for p in pts]; C = [P[0]]
        for i in range(1, len(P) - 1):
            a = (P[i - 1] - P[i]).normalized(); b = (P[i + 1] - P[i]).normalized()
            ang = math.acos(max(-1.0, min(1.0, a.dot(b))))
            d = R / math.tan(ang / 2)
            s0, s1 = P[i] + a * d, P[i] + b * d
            bis = (a + b).normalized(); ctr = P[i] + bis * (R / math.sin(ang / 2))
            v0, v1 = s0 - ctr, s1 - ctr; th = math.acos(max(-1.0, min(1.0, v0.normalized().dot(v1.normalized()))))
            C.append(s0)
            for k in range(1, seg):
                f = k / seg
                C.append(ctr + (v0 * math.sin((1 - f) * th) + v1 * math.sin(f * th)) / math.sin(th))
            C.append(s1)
        C.append(P[-1])
        o = bmesh.new(); rings = []; up = None
        for i, c in enumerate(C):
            t = (C[min(i + 1, len(C) - 1)] - C[max(i - 1, 0)]).normalized()
            if up is None: up = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            x = up.cross(t).normalized(); y = t.cross(x); up = y
            rings.append([o.verts.new(c + (x * math.cos(2 * math.pi * k / n) + y * math.sin(2 * math.pi * k / n)) * r) for k in range(n)])
        for i in range(len(rings) - 1):
            for k in range(n): o.faces.new((rings[i][k], rings[i][(k + 1) % n], rings[i + 1][(k + 1) % n], rings[i + 1][k]))
        o.faces.new(list(reversed(rings[0]))); o.faces.new(rings[-1])
        self._take(o, MATS[m])

    def fitting(self, p, n, r, m='BR'):
        """Hose end at p, hose leaving along n: crimped steel ferrule, then a brass hex nut toward -n."""
        M = frame(p, n)
        self.lz([(r * 1.35, 0.0), (r * 1.35, 0.035), (r * 1.25, 0.04), (r * 1.02, 0.045)], 'CR', M, 16)
        self.lz([(r * 0.9, -0.03), (r * 1.6, -0.03), (r * 1.6, 0.0), (r * 0.9, 0.0)], m, M, 6)

    def band(self, c, R0, R1, w, a0, a1, m, n=16):
        """A curved strip about the x axis through c (a fender or guard): radii R0..R1, width w in x, from a0 to a1 deg (0 = +y, 90 = up)."""
        o = bmesh.new(); rings = []
        for i in range(n + 1):
            a = math.radians(a0 + (a1 - a0) * i / n); cy, cz = math.cos(a), math.sin(a)
            rings.append([o.verts.new((c[0] + sx * w / 2, c[1] + rr * cy, c[2] + rr * cz)) for sx, rr in ((-1, R0), (1, R0), (1, R1), (-1, R1))])
        for i in range(n):
            for k in range(4): o.faces.new((rings[i][k], rings[i][(k + 1) % 4], rings[i + 1][(k + 1) % 4], rings[i + 1][k]))
        o.faces.new(list(reversed(rings[0]))); o.faces.new(rings[-1])
        bmesh.ops.recalc_face_normals(o, faces=o.faces)
        self._take(o, MATS[m])

    def label(self, c, n, w, h, m):
        """A plain label plate (0.8 mm) set into the surface at c facing n."""
        self.fbox(frame(c, n), (-w / 2, -h / 2, -0.0005), (w / 2, h / 2, 0.0008), m)

    def tyre(self, R, W, rim, lugs=True, nuts=5, rim_m='LG', hub=True):
        """Wheel about x at the origin: tyre with ribs (and lugs for pneumatic cart tyres), pressed rim, hub, nuts, valve."""
        sw = W / 2
        prof = [(rim + 0.004, -sw + 0.01), (rim + 0.012, -sw), (R - 0.03, -sw - 0.006), (R - 0.008, -sw + 0.004), (R, -sw + 0.016)]
        g = [(-sw * 0.3, 0), (sw * 0.3, 0)] if (not lugs and sw > 0.03) else []
        x = -sw + 0.016
        for gy, gr in g:
            prof += [(R, gy - 0.003), (R - 0.006, gy - 0.002), (R - 0.006, gy + 0.002), (R, gy + 0.003)]
        prof += [(R, sw - 0.016), (R - 0.008, sw - 0.004), (R - 0.03, sw + 0.006), (rim + 0.012, sw), (rim + 0.004, sw - 0.01)]
        self.lathe([(r, h) for r, h in prof], 'TY', (0, 0, 0), 'x', 40)
        if lugs:                                     # chevron lugs, two rows, offset
            for k in range(24):
                a = 2 * math.pi * k / 24
                for s in (-1, 1):
                    a2 = a + s * 0.06 + (0.13 if s > 0 else 0)
                    M = Matrix.Rotation(a2, 4, 'X') @ Matrix.Translation((s * sw * 0.45, 0, R - 0.002))
                    self.fbox(M @ Matrix.Rotation(s * 0.45, 4, 'Z'), (-sw * 0.42, -0.012, 0), (sw * 0.42, 0.012, 0.008), 'TY', 0.002)
        d = min(0.03, sw - 0.006)                    # the rim's centre disc
        # rim: two pressed halves with a bead seat, a dished centre and the hub
        rp = [(rim + 0.01, -sw + 0.004), (rim, -sw + 0.012), (rim, sw - 0.012), (rim + 0.01, sw - 0.004), (rim - 0.004, sw - 0.004),
              (rim - 0.012, sw - 0.02), (rim * 0.55, d - 0.005), (rim * 0.35, d), (0.0, d)]
        self.lathe(rp, rim_m, (0, 0, 0), 'x', 32)
        hb = (d - 0.001, -sw, rim * 0.2) if hub else (sw + 0.009, -sw - 0.009, 0.0095)   # hub barrel to the axle nut / between the fork legs
        self.lathe([(rim * 0.35, hb[0]), (rim * 0.35, hb[1]), (hb[2], hb[1]), (hb[2], hb[0])], rim_m, (0, 0, 0), 'x', 24)
        if hub: self.lathe([(0.0, 0.05), (0.022, 0.05), (0.022, 0.032), (0.0, 0.032)], 'CR', (0, 0, 0), 'x', 16)       # hub cap
        for k in range(nuts):
            a = 2 * math.pi * k / nuts + 0.3; rr = rim * 0.24 + 0.012
            self.bolt((d + 0.0002, rr * math.cos(a), rr * math.sin(a)), (1, 0, 0), 0.0075)
        a = 1.2
        if hub: self.tube((rim * 0.5 * 0 + 0.01, (rim - 0.01) * math.cos(a), (rim - 0.01) * math.sin(a)),
                           (0.034, (rim - 0.026) * math.cos(a), (rim - 0.026) * math.sin(a)), 0.003, 'CR', 8, weld=False)   # valve

    def gauge_body(self, r):
        """Gauge housing about z at the origin, face at z=0.012: steel case, chrome bezel, dial, glass over the needle."""
        I = Matrix.Identity(4)
        self.lz([(0.0, -0.03), (r * 0.95, -0.03), (r, -0.025), (r, 0.006), (0.0, 0.006)], 'BK', I, 32)
        self.lz([(r * 0.9, 0.006), (r * 1.06, 0.006), (r * 1.08, 0.012), (r * 1.06, 0.02), (r * 0.9, 0.02), (r * 0.9, 0.006)], 'CR', I, 32)
        self.lz([(0.0, 0.009), (r * 0.9, 0.009), (r * 0.9, 0.0095), (0.0, 0.0095)], 'GF', I, 32)
        self.lz([(0.0, 0.018), (r * 0.9, 0.018), (r * 0.9, 0.0186), (0.0, 0.0186)], 'GL', I, 32)
        self.lz([(0.007, -0.05), (0.007, -0.03), (0.0, -0.03)], 'BR', I, 6)                             # stem nut


_CACHE = {}


def cached(key, build):
    if key not in _CACHE:
        g = D(); build(g); _CACHE[key] = g.mesh(key, (0, 0, 0))
    return _CACHE[key]


def gauge(tag, p, n, r, parent, does):
    """A gauge at p facing n: Gauge_<tag> (fixed) and Needle_<tag> (turns about the face normal, 0..270 deg)."""
    me = cached('GaugeBody_%d' % round(r * 1000), lambda g: g.gauge_body(r))
    M = frame(p, n)
    q = M.to_quaternion().to_euler()
    o = part('Gauge_' + tag, me, tuple(p), parent, 'fixed: ' + does)
    o.rotation_euler = q
    nd = cached('Needle_%d' % round(r * 1000), lambda g: (g.fbox(Matrix.Identity(4), (-0.0015, -r * 0.15, 0.011), (0.0015, r * 0.82, 0.0125), 'KR'),
                                                         g.lz([(0.0, 0.008), (0.006, 0.008), (0.006, 0.014), (0.0, 0.014)], 'BK', Matrix.Identity(4), 12)))
    o2 = part('Needle_' + tag, nd, tuple(p), 'Gauge_' + tag, 'turns about its local z (out of the dial) 0..270 deg clockwise from -135: ' + does)
    o2.location = (0, 0, 0)
    return o


def cart_frame(g, x, yf, yr, zf, R=0.205, W=0.1):
    """Into the frame's geometry g: the rear axle tube on welded brackets, axle nuts, the turntable ring."""
    g.tube((-x + W / 2 + 0.01, yr, R), (x - W / 2 - 0.01, yr, R), 0.03, 'G', 18, weld=False)        # rear axle
    for s in (-1, 1):
        g.box((s * (x - 0.16) - 0.04, yr - 0.045, R + 0.02), (s * (x - 0.16) + 0.04, yr + 0.045, zf + 0.004), 'G', 0.004)   # axle brackets
        g.weld((s * (x - 0.16) - 0.045, yr - 0.032, R + 0.02), (s * (x - 0.16) + 0.045, yr - 0.032, R + 0.02), 'G', 0.005)
        g.weld((s * (x - 0.16) - 0.045, yr + 0.032, R + 0.02), (s * (x - 0.16) + 0.045, yr + 0.032, R + 0.02), 'G', 0.005)
        g.tube((s * (x - W / 2 - 0.012), yr, R), (s * (x - W / 2 + 0.004), yr, R), 0.022, 'CR', 6, weld=False)   # axle nut
    g.lz([(0.155, zf - 0.008), (0.19, zf - 0.008), (0.19, zf + 0.002), (0.155, zf + 0.002)], 'ST', frame((0, yf, 0), (0, 0, 1)), 32)   # turntable ring on the frame


def cart_gear(root, x, yf, yr, zf, R=0.205, W=0.1, rim=0.1, tow_len=1.1, steer=35):
    """Running gear of a towed cart: rear axle tube welded to the frame (wheels Wheel_RL/RR), a turntable front axle
    FrontAxle (steers about z) with Wheel_FL/FR, and the TowBar hinged on it (about x: 0 = up, parked .. 80 deg down).
    zf: underside of the frame. Call cart_frame(f, ...) first to weld the rear axle and turntable ring into the frame."""
    wm = cached('CartWheel_%d' % round(R * 1000), lambda g: g.tyre(R, W, rim, True, 5))
    for tag, s in (('RL', -1), ('RR', 1)):
        part('Wheel_' + tag, wm, (s * x, yr, R), root, 'spin about x, rolls r=%.3f m' % R, rot_z=math.pi if s < 0 else 0)
    # front axle beam with king pin turntable
    fa = D()
    fa.box((-x + W / 2 + 0.03, yf - 0.04, R - 0.03), (x - W / 2 - 0.03, yf + 0.04, R + 0.05), 'G', 0.006)
    for s in (-1, 1):
        fa.tube((s * (x - W / 2 - 0.05), yf, R), (s * (x - W / 2 + 0.003), yf, R), rim * 0.2 + 0.003, 'CR', 16, weld=False)   # stub axle into the hub
        fa.lz([(0.02, -0.004), (0.026, 0.0), (0.02, 0.004)], 'G', frame((s * (x - W / 2 - 0.03), yf, R), (s, 0, 0)), 12, caps=False)   # weld ring
    fa.lz([(0.0, zf - 0.025), (0.16, zf - 0.025), (0.16, zf - 0.005), (0.0, zf - 0.005)], 'ST', frame((0, yf, 0), (0, 0, 1)), 32)
    fa.box((-0.05, yf - 0.06, R + 0.05), (0.05, yf + 0.06, zf - 0.025), 'G', 0.005)
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        fa.bolt((0.135 * math.cos(a), yf + 0.135 * math.sin(a), zf - 0.025), (0, 0, -1), 0.008)
    for s in (-1, 1): fa.box((s * 0.07 - 0.008, yf + 0.04, R - 0.02), (s * 0.07 + 0.008, yf + 0.12, R + 0.04), 'ST', 0.002)
    part('FrontAxle', fa, (0, yf, 0), root, 'steers about z +-%d deg (turntable), led by the tow bar' % steer)
    for tag, s in (('FL', -1), ('FR', 1)):
        part('Wheel_' + tag, wm, (s * x, yf, R), 'FrontAxle', 'spin about x, rolls r=%.3f m' % R, rot_z=math.pi if s < 0 else 0)
    tb = D(); P = (0, yf + 0.09, R + 0.01)
    tb.tube((-0.082, P[1], P[2]), (0.082, P[1], P[2]), 0.013, 'CR')
    tb.lz([(0.0, -0.035), (0.03, -0.035), (0.03, 0.035), (0.0, 0.035)], 'Y', frame(P, (1, 0, 0)), 20)
    tb.tube((-0.03, P[1], P[2]), (-0.02, P[1], P[2] + tow_len - 0.08), 0.022, 'Y')
    tb.tube((0.03, P[1], P[2]), (0.02, P[1], P[2] + tow_len - 0.08), 0.022, 'Y')
    tb.box((-0.03, P[1] - 0.02, P[2] + 0.25), (0.03, P[1] + 0.02, P[2] + 0.3), 'Y', 0.004)
    top = P[2] + tow_len
    tb.lz([(0.034, -0.012), (0.052, -0.012), (0.056, 0.0), (0.052, 0.012), (0.034, 0.012), (0.03, 0.0), (0.034, -0.012)], 'ST', frame((0, P[1], top - 0.03), (0, 1, 0)), 24, caps=False)
    tb.box((-0.025, P[1] - 0.012, top - 0.12), (0.025, P[1] + 0.012, top - 0.085), 'ST', 0.003)
    for s in (-1, 1): tb.weld((s * 0.02, P[1] - 0.023, top - 0.1), (s * 0.02, P[1] + 0.023, top - 0.1), 'Y')
    tb.label((0, P[1] - 0.0203, P[2] + 0.275), (0, -1, 0), 0.05, 0.025, 'LW')
    part('TowBar', tb, P, 'FrontAxle', 'hinge about x: 0 = up (parked) .. 80 deg forward = on the tug hitch; steers FrontAxle')
    ik('TowBar_Eye', (0, P[1], top - 0.07), 'hand', 'TowBar'); ik('TowBar_Grip', (0, P[1], P[2] + 0.6), 'push', 'TowBar')
    ctrl('Tow bar', 'TowBar', 'TowBar_Eye', 'lower onto the tug hitch / lift up to park; steer by hand to push')


def park_brake(root, p, side):
    """Hand parking brake lever on the frame side (pivot at p, tips about x 0..50 deg back = on)."""
    pb = D(); s = side
    pb.box((p[0] - 0.006, p[1] - 0.02, p[2] - 0.02), (p[0] + 0.006, p[1] + 0.02, p[2] + 0.02), 'ST', 0.003)
    pb.tube(p, (p[0], p[1] - 0.05, p[2] + 0.32), 0.012, 'RD', 12, weld=False)
    pb.tube((p[0], p[1] - 0.05, p[2] + 0.27), (p[0], p[1] - 0.05, p[2] + 0.36), 0.017, 'BK', 14, weld=False)
    part('ParkBrake', pb, p, root, 'lever about x: 0 = off (forward) .. 50 deg back = rear wheels braked')
    ik('ParkBrake', (p[0], p[1] - 0.05, p[2] + 0.32), 'hand', 'ParkBrake')
    ctrl('Parking brake', 'ParkBrake', 'ParkBrake', 'pull back = braked (before unhooking), forward = free to tow')


def valve(tag, p, n, parent, kind, col, does, r=0.03):
    """A control on a panel at p facing n: kind 'T' (T-handle valve, turns about n 0..270 deg), 'wheel' (hand wheel),
    'lever' (quarter-turn ball valve lever), 'button' (push 6 mm), 'toggle' (flips +-30 deg), 'knob' (rotary)."""
    v = D(); M = frame((0, 0, 0), (0, 0, 1))
    if kind in ('T', 'wheel', 'lever'):
        v.lz([(0.009, -0.003), (0.009, 0.04), (0.0, 0.04)], 'CR', M, 12)
        v.lz([(0.016, -0.004), (0.016, 0.012), (0.0, 0.012)], 'BR', M, 6)
    if kind == 'T':
        v.tube((-r, 0, 0.045), (r, 0, 0.045), 0.008, col, 12, weld=False)
        v.lz([(0.0, 0.035), (0.012, 0.035), (0.012, 0.055), (0.0, 0.055)], col, M, 12)
    elif kind == 'wheel':
        for k in range(3):
            a = 2 * math.pi * k / 3; v.tube((0, 0, 0.042), (r * math.cos(a), r * math.sin(a), 0.042), 0.004, col, 8, weld=False)
        v.lz([(r - 0.006, 0.036), (r + 0.006, 0.036), (r + 0.006, 0.048), (r - 0.006, 0.048), (r - 0.006, 0.036)], col, M, 28)
    elif kind == 'lever':
        v.fbox(M, (-0.007, -0.01, 0.038), (0.007, r * 3, 0.046), col, 0.003)
    if kind in ('button', 'mushroom', 'knob'):
        v.lz([(0.024, -0.002), (0.024, 0.003), (0.021, 0.005), (0.0, 0.005)], 'CR', M, 24)
    if kind == 'button':
        v.lz([(0.0, 0.004), (0.016, 0.004), (0.017, 0.012), (0.014, 0.02), (0.0, 0.02)], col, M, 20)
    elif kind == 'mushroom':
        v.lz([(0.0, 0.004), (0.012, 0.004), (0.012, 0.025), (0.03, 0.028), (0.03, 0.036), (0.022, 0.044), (0.0, 0.046)], col, M, 24)
    elif kind == 'toggle':
        v.lz([(0.012, -0.002), (0.012, 0.006), (0.0, 0.006)], 'CR', M, 6)
        v.tube((0, 0, -0.004), (0, 0, 0.026), 0.0035, 'CR', 10, weld=False)
        v.lz([(0.0, 0.022), (0.006, 0.022), (0.006, 0.03), (0.0, 0.032)], col, M, 10)
    elif kind == 'knob':
        v.lz([(0.0, 0.004), (0.02, 0.004), (0.018, 0.026), (0.0, 0.028)], col, M, 18)
        v.fbox(M, (-0.003, 0.0, 0.0275), (0.003, 0.016, 0.0285), 'WH')
    me = v.mesh('%s_%s' % (kind.capitalize(), tag), (0, 0, 0))
    o = part(tag, me, tuple(p), parent, {'T': 'turns about the panel normal 0..270 deg (open)', 'wheel': 'turns about the panel normal 0..3 turns',
                                          'lever': 'quarter turn about the panel normal 0..90 deg', 'button': 'pushes in 6 mm',
                                          'mushroom': 'pushes in 8 mm and latches (emergency stop), twist to release',
                                          'toggle': 'flips about its local x -30..+30 deg', 'knob': 'turns about the panel normal 0..270 deg'}[kind] + ': ' + does)
    o.rotation_euler = frame(p, n).to_quaternion().to_euler()
    tip = Vector(p) + Vector(n).normalized() * 0.045
    ik(tag, tuple(tip), 'hand', tag); bpy.data.objects['IK_' + tag].location = (0, 0, 0.045)
    ctrl(tag.replace('_', ' '), tag, tag, does)
    return o


_mesh_B = B.mesh


def _mesh_fit(self, name, pivot):
    """B.mesh, then label and dial faces get their picture fitted to the face (mirrored in u: the game mirrors x)."""
    me = _mesh_B(self, name, pivot)
    fit = [i for i, m in enumerate(self.ms) if any(MATS.get(k) is m for k in FIT)]
    if not fit: return me
    bm = bmesh.new(); bm.from_mesh(me); uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        if f.material_index not in fit or f.calc_area() < 2e-5: continue
        n = f.normal; t = Vector((0, 0, 1)) if abs(n.z) < 0.9 else Vector((0, 1, 0))
        u = t.cross(n).normalized(); v = n.cross(u)
        cs = [(l.vert.co.dot(u), l.vert.co.dot(v)) for l in f.loops]
        u0, u1 = min(c[0] for c in cs), max(c[0] for c in cs); v0, v1 = min(c[1] for c in cs), max(c[1] for c in cs)
        for l, c in zip(f.loops, cs): l[uv].uv = (1 - (c[0] - u0) / max(u1 - u0, 1e-6), (c[1] - v0) / max(v1 - v0, 1e-6))
    bm.to_mesh(me); bm.free()
    return me


B.mesh = _mesh_fit


def castor2(tag, x, y, root, r=0.10, w=0.05, top=None):
    """hq_lib's braked swivel castor with a real wheel: rubber tyre with rib grooves on a pressed hub."""
    castor(tag, x, y, root, r, w, top)
    h = top if top is not None else 2 * r + 0.07; off = r * 0.45
    def fork(g):                                     # plate, closed swivel bearing into the plate, legs welded to it, axle with nuts
        g.box((-w / 2 - 0.03, -0.07, h - 0.012), (w / 2 + 0.03, 0.07, h), 'ST', 0.003)
        for sx in (-1, 1):
            for sy in (-1, 1): g.bolt((sx * (w / 2 + 0.018), sy * 0.055, h - 0.012), (0, 0, -1), 0.005)
        g.lathe([(0.0, h - 0.042), (0.035, h - 0.042), (0.037, h - 0.03), (0.035, h - 0.009), (0.0, h - 0.009)], 'ST', (0, 0, 0), 'z', 20)
        for s in (-1, 1):
            g.bar((s * (w / 2 + 0.012), 0, h - 0.032), (s * (w / 2 + 0.012), -off, r), 0.05, 0.01, 'ST', 0.002, up=(1, 0, 0))
        g.tube((-w / 2 - 0.02, -off, r), (w / 2 + 0.02, -off, r), 0.011, 'CR', 10, weld=False)
    OBJ['Castor_' + tag].data = cached('CastorFork_%d_%d_%d' % (round(r * 1000), round(w * 1000), round(h * 1000)), fork)
    OBJ['Wheel_' + tag].data = cached('CastorTyre_%d_%d' % (round(r * 1000), round(w * 1000)),
                                      lambda g: g.tyre(r, w, r * 0.55, False, 0, 'G', hub=False))
    def brake(g):                                    # pedal on a boss that turns on the axle end, outside the fork leg
        g.tube((w / 2 + 0.014, 0, 0), (w / 2 + 0.04, 0, 0), 0.013, 'ST', 12, weld=False)
        g.bar((w / 2 + 0.03, 0.0, 0.0), (w / 2 + 0.03, 0.09, 0.0), 0.012, 0.035, 'ST', 0.002)
        g.box((w / 2 + 0.012, 0.07, -0.006), (w / 2 + 0.07, 0.115, 0.012), 'RD', 0.003)
        for k in range(4): g.box((w / 2 + 0.02 + 0.012 * k, 0.072, 0.011), (w / 2 + 0.026 + 0.012 * k, 0.113, 0.0135), 'BK', 0.001)
    OBJ['Brake_' + tag].data = cached('CastorBrake_%d' % round(w * 1000), brake)


def coil(g, c, axis, R, r, turns, m='HS'):
    """A hose or cable hanging coiled on a hook: turns loops of radius R side by side along axis (x or y)."""
    for k in range(turns):
        cc = list(c); i = 0 if axis == 'x' else 1; cc[i] += (k - (turns - 1) / 2) * r * 2.1
        prof = [(R - r + r * (1 - math.cos(2 * math.pi * j / 10)), r * math.sin(2 * math.pi * j / 10)) for j in range(11)]
        g.lathe(prof, m, tuple(cc), axis, 36)


def louvres(g, side_x, y0, y1, z0, rows, m='WH'):
    """Pressed louvre slots on a side wall at x = side_x (sign gives the outward side)."""
    s = 1 if side_x > 0 else -1
    for k in range(rows):
        z = z0 + 0.03 * k
        g.box((side_x - s * 0.006, y0, z), (side_x + s * 0.006, y1, z + 0.012), m, 0.004)
        g.box((side_x - s * 0.004, y0 + 0.004, z - 0.008), (side_x + s * 0.0045, y1 - 0.004, z - 0.002), 'BK', 0.001)
