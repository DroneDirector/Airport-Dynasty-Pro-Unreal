# The roof of the roofed passenger stairs, run inside make_passengerstairs.py (its names and points): smoked polycarbonate on slim hoops. The upper roof stands on the
# upper flight's side plates and runs inside the lower one (4 cm clear), which stands on the lower side panels; over the platform a wider hood on its side walls, with a
# visor in front (over the door's top, behind the bumper's plane). Profile: upright sides, then a quarter superellipse. h = height over the nosing line f, offset d = outwards.
M_POLY = P('PS_SmokedRoof_Glass', (.085, .08, .075), 0, .08)
NUP = lambda x: min(NU(x), PZ)                                                  # the upper flight's nosing line, level over the platform
def prof(a, hs, b, n, h0):
    D = [(a, h0 + (hs - h0) * i / 60) for i in range(60)] + [(a * math.copysign(abs(math.cos(t)) ** (2 / n), math.cos(t)), hs + b * max(0, math.sin(t)) ** (2 / n)) for t in [math.pi * i / 1200 for i in range(1201)]] + [(-a, hs - (hs - h0) * i / 60) for i in range(1, 61)]
    S_ = [0]
    for p, q in zip(D, D[1:]): S_.append(S_[-1] + math.hypot(q[0] - p[0], q[1] - p[1]))
    return D, S_
class Arch:
    def __init__(self, a, hs, b, n, f): self.a, self.hs, self.b, self.n, self.f = a, hs, b, n, f
    def hy(self, y): return self.hs + self.b * max(0, 1 - abs(y / self.a) ** self.n) ** (1 / self.n)
    def pts(self, h0, d=0.0, N=44, ymax=None):
        """The outline from (+a, h0) over the top to (-a, h0), moved out by d: evenly spaced."""
        D, S_ = prof(self.a, self.hs, self.b, self.n, h0); L = S_[-1]; R_ = []; j = 0; s0 = 0
        if ymax: i0 = next(i for i, p in enumerate(D) if p[0] <= ymax); s0 = S_[i0]
        for i in range(N + 1):
            s = s0 + (L - 2 * s0) * i / N
            while j < len(S_) - 2 and S_[j + 1] < s: j += 1
            t = (s - S_[j]) / max(S_[j + 1] - S_[j], 1e-9); p, q = D[j], D[j + 1]; e0, e1 = D[max(j - 6, 0)], D[min(j + 7, len(D) - 1)]; dy, dh = e1[0] - e0[0], e1[1] - e0[1]; l = math.hypot(dy, dh)
            R_.append((p[0] + (q[0] - p[0]) * t + dh / l * d, p[1] + (q[1] - p[1]) * t - dy / l * d))
        return R_
    def at(self, y, h, b, k): x = b - (h - .9) * k; return (x, y, self.f(x) + h)      # on the hoop that stands at b and leans back by k
def sheet(name, A, rows, d0, d1, h0, m, N=44, ymax=None, smooth=40):
    """A bent sheet between hoops (b, k), from offset d0 to d1: closed."""
    bm = bmesh.new(); V = [[[bm.verts.new(A.at(y, h, b, k)) for y, h in A.pts(h0, d, N, ymax)] for b, k in rows] for d in (d0, d1)]; n = len(rows)
    for s_, G_ in enumerate(V):
        for i in range(n - 1):
            for j in range(N): q = (G_[i][j], G_[i][j + 1], G_[i + 1][j + 1], G_[i + 1][j]); bm.faces.new(q if s_ else q[::-1])
    I, O = V
    for i in range(n - 1):
        for j in (0, N): bm.faces.new((I[i][j], O[i][j], O[i + 1][j], I[i + 1][j]))
    for i in (0, n - 1):
        for j in range(N): bm.faces.new((I[i][j], I[i][j + 1], O[i][j + 1], O[i][j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); return K.finish(K.link(name, bm), m, smooth)
def lie(o, p, u, up): u = Vector(u).normalized(); w_ = Vector(up).cross(u).normalized(); v = u.cross(w_); o.data.transform(Matrix(((u.x, w_.x, v.x, p[0]), (u.y, w_.y, v.y, p[1]), (u.z, w_.z, v.z, p[2]), (0, 0, 0, 1)))); o.data.update(); return o
def hoop(n, A, b, k, h0, hs0=None, cover=True, ymax=None, w=.018):
    tube(n, [A.at(y, h, b, k) for y, h in A.pts(h0, 0, 44, ymax)], .015, M_BODY, bez=False, bres=2)
    if not cover: return
    sheet(n + '_Cover', A, [(b - w, k), (b, k), (b + w, k)], .0242, .0267, hs0, M_ALU, 44, ymax)
    for i, ((y, h), q) in enumerate(zip(A.pts(hs0 + .04, .0270, 14, ymax), A.pts(hs0 + .04, .0330, 14, ymax))):      # its screws
        p0, p1 = Vector(A.at(y, h, b, k)), Vector(A.at(q[0], q[1], b, k)); lie(cyl('%s_Screw%02d' % (n, i), (0, 0, 0), .0065, .004, 'x', M_STEEL, 12, .001), p0, p1 - p0, (1, 0, 0))
def foot(n, A, b, k, s, face, ha, hb, top):
    """The hoop's flat foot: strap outside the hand rail, bolted to the side plate."""
    yc = A.a - (.004 if A is AL else 0); X = lambda h: b - (h - .9) * k
    Q = lambda w, h: (X(h) + w, A.f(X(h) + w) + h)
    prism(n + '_Strap', [Q(-.020, ha), Q(.020, ha), Q(.020, top), Q(-.020, top)], s * (yc - .006), s * (yc + .006), M_BODY, .002, 1)
    prism(n + '_Plate', [Q(-.037, ha - .012), Q(.037, ha - .012), Q(.037, hb), Q(-.037, hb)], s * (face - .0015), s * (yc - .0055), M_BODY, .002, 1)
    for e in (-1, 1):
        for i, h in enumerate((ha + .018, hb - .022)): x, z = Q(e * .0285, h); cyl('%s_Bolt%d%d' % (n, e > 0, i), (x, s * (yc - .0045), z), .0065, .006, 'y', M_STEEL, 16, .0012)
def leds(pre, lens, A, rows, ys, gap=.0165):
    for i, ((b0, k0), (b1, k1)) in enumerate(zip(rows, rows[1:])):
        for y in ys:
            h = A.hy(y); p, q = Vector(A.at(y, h, b0, k0)), Vector(A.at(y, h, b1, k1)); u = (q - p).normalized(); l = (q - p).length - 2 * gap
            if l < .12: continue
            c = (p + q) / 2; t = '%d%s' % (i, 'C' if y == 0 else 'L' if y > 0 else 'R')
            lie(box(pre + t, (0, 0, 0), (l, .046, .024), M_ALU, .004, 2), c, u, (0, 0, 1))
            for e in (-1, 1): lie(box('%s%s_End%d' % (pre, t, e > 0), (e * (l / 2 + .0020), 0, 0), (.010, .030, .016), M_ALU, .002, 1), c, u, (0, 0, 1))      # welded to the hoops
            lie(box(lens + t, (0, 0, -.0130), (l - .05, .030, .006), M_STEP, .002, 2), c, u, (0, 0, 1))
def gutter(n, f, xs, s, y0, y1, h):
    prism(n + '_Floor', [(x, f(x) + h + .003) for x in xs] + [(x, f(x) + h + .007) for x in xs[::-1]], s * y0, s * (y1 - .002), M_ALU); prism(n + '_Lip', [(x, f(x) + h) for x in xs] + [(x, f(x) + h + .040) for x in xs[::-1]], s * (y1 - .004), s * y1, M_ALU)
    for e, x in ((0, xs[0] + .004), (1, xs[-1] - .004)): prism('%s_Cap%d' % (n, e), [(x - .003, f(x - .003) + h + .001), (x + .003, f(x + .003) + h + .001), (x + .003, f(x + .003) + h + .036), (x - .003, f(x - .003) + h + .036)], s * (y0 + .001), s * (y1 - .001), M_ALU)
AU = Arch(.685, 1.55, .705, 3.6, NUP); AL = Arch(.770, 1.58, .795, 3.6, NL); AH = Arch(.968, 1.55, .705, 3.6, NUP); KP = T / (1 + T * T)      # KP = square to the slope
HU = [(-2.78, .12), (-2.26, .35), (-1.74, KP), (-1.22, KP), (-.70, KP), (-.18, KP), (.09, .28), (XP, 0)]
HL = [(-2.62, .21), (-2.12, .40), (-1.59, KP), (-1.15, KP), (-.70, KP), (-.22, KP), (.12, .34), (.45, .20)]
HH = [(XP + .352, 0), (XP + .740, 0), (XP + 1.118, 0)]; XBK = XP + .300; XV = XP + 1.50
for i, (b, k) in enumerate(HU): hoop('UpRoofHoop%d' % i, AU, b, k, 1.03, 1.14); [foot('UpRoofFoot%d_%s' % (i, sn), AU, b, k, s, .665, .56, .80, 1.06) for s, sn, f in SIDES]
for i, (b, k) in enumerate(HL): hoop('LowRoofHoop%d' % i, AL, b, k, 1.03, 1.14); [foot('LowRoofFoot%d_%s' % (i, sn), AL, b, k, s, .740, .62, .84, 1.06) for s, sn, f in SIDES]
for i, (b, k) in enumerate(HH): hoop('UpRoofHood%d' % i, AH, b, k, 1.125, 1.23); [foot('UpRoofHoodFoot%d_%s' % (i, sn), AH, b, k, s, .950, .735, .895, 1.16) for s, sn, f in SIDES]
sheet('UpRoofPanel_Glass', AU, HU[:-1] + [(XP - .001, 0)], .0175, .0215, 1.10, M_POLY); sheet('UpRoofPanelTop_Glass', AU, [(XP + .001, 0), (XBK - .006, 0)], .0175, .0215, 1.17, M_POLY);      # (higher: the lower hand rail's end runs past below it)
sheet('LowRoofPanel_Glass', AL, HL, .0175, .0215, 1.10, M_POLY); sheet('UpRoofHoodPanel_Glass', AH, [(XBK - .008, 0), (XP + 1.127, 0)], .0175, .0215, 1.19, M_POLY)
sheet('UpRoofFlange', AU, [(XBK - .022, 0), (XBK + .004, 0)], .0242, .0270, 1.14, M_ALU)
# the wall between the narrow roof and the hood, on top of the platform's back walls
Ia, Oa = AU.pts(.907, .0250), AH.pts(.907, .0150); Oa = [(math.copysign(min(abs(y), .949), y) if h < 1.185 else y, h) for y, h in Oa]; bm = bmesh.new(); V_ = [[[bm.verts.new((x, y, PZ + h)) for y, h in Q_] for Q_ in (Ia, Oa)] for x in (XBK - .006, XBK + .006)]
for j in range(len(Ia) - 1):
    for x in (0, 1): bm.faces.new((V_[x][0][j], V_[x][0][j + 1], V_[x][1][j + 1], V_[x][1][j]))
    for c in (0, 1): bm.faces.new((V_[0][c][j], V_[0][c][j + 1], V_[1][c][j + 1], V_[1][c][j]))
for j in (0, len(Ia) - 1): bm.faces.new((V_[0][0][j], V_[0][1][j], V_[1][1][j], V_[1][0][j]))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); K.finish(K.link('UpRoofBulk', bm), M_BODY, 40)
sheet('UpRoofBulkTrim', AH, [(XBK - .014, 0), (XBK + .010, 0)], .0242, .0270, 1.23, M_ALU)
# visor: level sheet over the door's top, a bow in front, three bars, rubber edge
sheet('UpRoofVisor_Glass', AH, [(XP + 1.130, 0), (XV - .010, 0)], .0175, .0215, 1.19, M_POLY, 24, .80); hoop('UpRoofVisorBow', AH, XP + 1.455, 0, 1.2, 1.2, False, .80)
sheet('UpRoofVisorSeal', AH, [(XV - .014, 0), (XV, 0)], .0110, .0280, 1.19, M_SEAL, 24, .80)
for i, y in enumerate((-.70, 0, .70)): p = AH.at(y, AH.hy(y), XP + 1.133, 0); cyl('UpRoofVisorBar%d' % i, (XP + 1.2865, p[1], p[2]), .011, .314, 'x', M_BODY, 24)
leds('UpRoofLed', 'StepLight_Up_Roof', AU, HU + [(XBK - .010, 0)], (.28, -.28)); leds('LowRoofLed', 'StepLight_Low_Roof', AL, HL, (.28, -.28)); leds('UpRoofHoodLed', 'StepLight_Up_Hood', AH, HH, (.34, -.34))
for s, sn, f in SIDES:
    gutter('UpRoofGutter_' + sn, NUP, [HU[0][0] - .04, XP, XP + .020], s, .697, .722, 1.084); gutter('LowRoofGutter_' + sn, NL, [HL[0][0] - .05, HL[-1][0] - .02], s, .782, .807, 1.084); gutter('UpRoofHoodGutter_' + sn, NUP, [XBK - .004, XP + 1.145], s, .980, .9965, 1.175)
# rubber lips between the two roofs: on the lower one's top end inwards, on the upper one's foot outwards
b, k = HL[-1]; sheet('LowRoofLip', AL, [(b - .003, k), (b + .003, k)], -.0400, -.0120, 1.16, M_SEAL); b, k = HU[0]; sheet('UpRoofLip', AU, [(b + .009, k), (b + .015, k)], .0258, .0450, 1.16, M_SEAL)
def roof_check():
    """Clear height over every nosing and over the platform, |y| <= 0.60."""
    BV = lambda pre: [BVHTree.FromPolygons([o.matrix_world @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons]) for o in ALL if o.name.startswith(pre)]
    U_, L_ = BV(('UpRoof', 'StepLight_Up_R', 'StepLight_Up_H')), BV(('LowRoof', 'StepLight_Low_R'))
    def up(TT, p):
        h = [t.ray_cast(Vector(p) + Vector((0, 0, .05)), Vector((0, 0, 1)), 6) for t in TT]; h = [x[3] + .05 for x in h if x[0] is not None]; return round(min(h), 3) if h else None
    YS = [i * .06 for i in range(-10, 11)]; m_ = []
    for j in range(1, 11): m_ += [(up(U_, (X0 + G * j - .005, y, NU(X0 + G * j))), 'up%d' % j, round(y, 2)) for y in YS]
    for x in (XP, XP + .3, XP + .6, XP + .9, XP + 1.2, XP + 1.45): m_ += [(up(U_, (x, y, PZ)), 'plat%.2f' % (x - XP), round(y, 2)) for y in YS]
    for j in range(0, 11): m_ += [(up(L_, (X0 + G * j - .005, y, NL(X0 + G * j))), 'low%d' % j, round(y, 2)) for y in YS]
    print(TAG, 'HEADROOM min', sorted(x for x in m_ if x[0])[:3], 'open sky', sorted({x[1] for x in m_ if not x[0]}))
    for f in (0, 5, 10):                                                        # the two roofs' real gap
        u_ = [Vector(v.co) + DIR * f for n in ('UpRoofGutter_L_Lip', 'UpRoofHoop3_Cover', 'UpRoofPanel_Glass', 'UpRoofLip') for v in bpy.data.objects[n].data.vertices]
        print(TAG, 'ROOF GAP f=%d %.1f mm' % (f, 1000 * min(d for v in u_ for t in L_ for d in [t.find_nearest(v, .2)[3]] if d is not None)))
