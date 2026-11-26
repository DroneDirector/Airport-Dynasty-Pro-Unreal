# Modelled details and pivots for the lorry (called by make_lorry.py, game metres: x forward, y left, z up).
# Melted Meshy pieces are cut away and built again from bevelled primitives (Art/CargoForklift/forklift_details.py):
# fifth wheel (horseshoe plate, slot to the rear, release handle) + kingpin, steering knuckles, mirrors on arms,
# head lamps + indicators (housing, chrome reflector, ribbed lens), trailer tail lamps (red/amber/white) and amber side
# markers, landing legs (housings, sliding legs with feet, cross shaft, gearbox, crank), cab steps with anti-slip studs,
# grab handles on the flat pillar behind each door, mudflaps, fuel-tank caps, a dark cab lining (the interior comes next).
import bpy, bmesh, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
import forklift_details as FD

Part, lamp, material, delete_faces, W = FD.Part, FD.lamp, FD.material, FD.delete_faces, FD.W


def tree(o): return FD.tree_world(o)


def pivot(o, p, rot=None):
    p = Vector(p); Wm = Matrix.Translation(p) @ (rot.to_4x4() if rot else Matrix.Identity(4))
    o.data.transform(Wm.inverted()); o.matrix_world = Wm


def box_cut(o, lo, hi):
    lo, hi = np.array(lo), np.array(hi)
    return delete_faces(o, lambda v: np.all((v > lo) & (v < hi), axis=1), True)


def hide_behind(o, cover, dist):
    """drop faces of o parallel to the cover part's surface and within dist of it at any corner or the centre (would fight)"""
    t = tree(cover); mw = o.matrix_world; bm = bmesh.new(); bm.from_mesh(o.data); gone = []
    for f in bm.faces:
        n = (mw.to_3x3() @ f.normal).normalized()
        for p in [f.calc_center_median()] + [q.co for q in f.verts]:
            h = t.find_nearest(mw @ p, dist)
            if h[0] is not None and abs(h[1].dot(n)) > 0.95: gone.append(f); break
    bmesh.ops.delete(bm, geom=gone, context='FACES'); bm.to_mesh(o.data); bm.free(); o.data.update(); return len(gone)

def cut_cyl(o, X, Z, R, ylo, yhi, s):
    """drop faces of o that reach into a wheel volume: any corner, edge middle or face centre inside (axis along y)"""
    bm = bmesh.new(); bm.from_mesh(o.data)
    def inside(p): return math.hypot(p.x - X, p.z - Z) < R and ylo < s * p.y < yhi
    gone = []
    for f in bm.faces:
        pts = [q.co for q in f.verts]
        if any(inside(p) for p in pts) or inside(f.calc_center_median()) or any(inside((pts[i] + pts[i - 1]) / 2) for i in range(len(pts))): gone.append(f)
    bmesh.ops.delete(bm, geom=gone, context='FACES'); bm.to_mesh(o.data); bm.free(); o.data.update(); return len(gone)


def lathe(P, prof, c, s, mi, n=96):
    """Closed ring surface about the y axis through c: prof = [(radius, depth inwards from the outer face)], s = +1 left side."""
    t = bmesh.new(); rings = []
    for r, dy in prof:
        rings.append([t.verts.new((c[0] + r * math.cos(2 * math.pi * i / n), c[1] - s * dy, c[2] + r * math.sin(2 * math.pi * i / n))) for i in range(n)])
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            try: t.faces.new((a[i], a[j], b[j], b[i]))
            except ValueError: pass
    bmesh.ops.remove_doubles(t, verts=t.verts, dist=1e-6); bmesh.ops.recalc_face_normals(t, faces=t.faces)
    P._merge(t, mi)


def tyre(P, c, s, y0, w, R=0.60):
    """One tyre (rubber, 4 tread grooves) from depth y0 to y0+w, bead r 0.295 (sits on the rim barrel)."""
    pr = [(0.295, y0 + 0.03), (0.30, y0 + 0.012), (0.40, y0), (0.52, y0 + 0.004), (0.575, y0 + 0.018), (R - 0.004, y0 + 0.035), (R, y0 + 0.05)]
    g = np.linspace(y0 + 0.05, y0 + w - 0.05, 9)
    for k in range(1, 8): pr += [(R, g[k] - 0.008), (R - 0.014, g[k] - 0.006), (R - 0.014, g[k] + 0.006), (R, g[k] + 0.008)] if k % 2 else []
    pr += [(R, y0 + w - 0.05), (R - 0.004, y0 + w - 0.035), (0.575, y0 + w - 0.018), (0.52, y0 + w - 0.004), (0.40, y0 + w), (0.30, y0 + w - 0.012),
           (0.295, y0 + w - 0.03), (0.295, y0 + 0.03)]
    lathe(P, pr, c, s, 0)


def wheel(sc, name, c, s, yout, yin, kind, M):
    """Modelled truck wheel 22.5 in rim, 1.20 m tyre, perfectly round (96 sides). kind: single (steer), twin (drive), wide (trailer).
    c = axle centre on the outer face plane (x, yout, z); depths run inwards (towards the lorry's middle)."""
    P = Part(name, [M['rubber'], M['rim'], M['black'], M['chrome']]); w = abs(yout - yin)
    tw = {'single': [(0.0, 0.315)], 'twin': [(0.0, 0.255), (0.275, w - 0.275)], 'wide': [(0.0, min(w, 0.385))]}[kind]
    for y0, tw_ in tw: tyre(P, c, s, y0, tw_)
    back = tw[-1][0] + tw[-1][1]
    dish = 0.075 if kind != 'single' else 0.11                                    # disc set inwards from the outer face
    lathe(P, [(0.0, dish + 0.02), (0.085, dish + 0.02), (0.09, dish - 0.005), (0.175, dish - 0.005), (0.21, dish + 0.01), (0.262, 0.035),
              (0.287, 0.012), (0.302, 0.006), (0.302, 0.022), (0.285, 0.045), (0.285, back - 0.04), (0.30, back - 0.012), (0.30, back),
              (0.27, back), (0.27, dish + 0.03), (0.0, dish + 0.03)], c, s, 1)
    lathe(P, [(0.0, dish - 0.045), (0.05, dish - 0.045), (0.07, dish - 0.03), (0.08, dish - 0.008), (0.08, dish + 0.01), (0.0, dish + 0.01)], c, s, 2, 32)
    for i in range(10):                                                           # wheel nuts on the 335 mm circle
        a = 2 * math.pi * i / 10; x, z = c[0] + 0.1675 * math.cos(a), c[2] + 0.1675 * math.sin(a)
        P.cyl((x, c[1] - s * (dish - 0.004), z), (x, c[1] - s * (dish - 0.03), z), 0.016, 3, 6, 0.002)
    for i in range(10):                                                           # hand holes in the disc (dark insets)
        a = 2 * math.pi * (i + 0.5) / 10; x, z = c[0] + 0.235 * math.cos(a), c[2] + 0.235 * math.sin(a)
        P.cyl((x, c[1] - s * (dish + 0.005), z), (x, c[1] - s * (dish + 0.0), z), 0.028, 2, 16, 0.0)
    return P.done(sc)


def build(sc, body, trailer, PARTS, g):
    SX, SY, SZ, G, WHEEL = g['SX'], g['SY'], g['SZ'], g['G'], g['WHEEL']
    INFO, IKN = [], {}
    M = dict(black=material('M_Lorry_BlackSteel', (0.02, 0.02, 0.022), 0.45, 0.5), rubber=material('M_Lorry_Rubber', (0.012, 0.012, 0.013), 0.88),
             plastic=material('M_Lorry_BlackPlastic', (0.025, 0.025, 0.027), 0.5), chrome=material('M_Lorry_Chrome', (0.9, 0.9, 0.9), 0.08, 1.0),
             steel=material('M_Lorry_Steel', (0.45, 0.46, 0.47), 0.35, 1.0), lens=material('M_Lorry_LensClear', (0.85, 0.87, 0.9), 0.04),
             amber=material('M_Lorry_LensAmber', (1.0, 0.33, 0.0), 0.08, 0.0, (1.0, 0.3, 0.0)), red=material('M_Lorry_LensRed', (0.7, 0.02, 0.01), 0.1),
             mirror=material('M_Lorry_MirrorSilver', (0.92, 0.93, 0.95), 0.02, 1.0), rim=material('M_Lorry_RimSilver', (0.62, 0.63, 0.64), 0.3, 0.9), lining=material('M_Lorry_CabLining', (0.03, 0.03, 0.032), 0.7))
    gx = lambda rx: G((rx, 0, 0))[0]; gz = lambda rz: G((0, 0, rz))[2]
    HWg = g['HW'] * SY; ZTg = gz(g['ZT']); KPx = gx(g['KP']); XRg = gx(g['XR']); THWg = g['THW'] * SY
    tb, tt = tree(body), tree(trailer)
    def ray(t, p, d, dist=3.0):
        h = t.ray_cast(Vector(p), Vector(d), dist); return h[0]
    new = {}

    # ------------------------------------------------ wheels: pivots on the axle; front knuckles (kingpin vertical)
    # Meshy's tyres were lumpy (outline spread 25-69 mm) and the trailer's three melted together: all 12 are modelled again,
    # round, on the fitted axle lines (trailer axles evenly spaced), standing on the ground; the Meshy tyre volume sets each width.
    ax = {k[:-1]: G((cx, 0, cz))[0] for k, (cx, cz, r, sg) in WHEEL.items()}
    ax['T2'] = (ax['T1'] + ax['T3']) / 2
    for k in list(WHEEL):
        o = PARTS['Wheel_' + k]; v = W(o); s = 1 if v[:, 1].mean() > 0 else -1
        yout = s * min(abs(v[:, 1]).max(), 1.25); yin = s * max(abs(v[:, 1]).min(), abs(yout) - 0.52);   # outer tyre face inside 2.55 m
        bpy.data.objects.remove(o, do_unlink=True)
        side = 'L' if s > 0 else 'R'; a = k[:-1]; kind = 'single' if a == 'F' else 'twin' if a[0] == 'D' else 'wide'
        o = wheel(sc, 'Wheel_%s%s' % (a, side), (ax[a], yout, 0.60), s, yout, yin, kind, M); del PARTS['Wheel_' + k]
        PARTS['Wheel_%s%s' % (a, side)] = new['Wheel_%s%s' % (a, side)] = o; WHEEL[k] = (WHEEL[k][0], WHEEL[k][1], 0.60 / SZ, WHEEL[k][3])
        for tg in (body, trailer):    # Meshy tyre remnants (inner half, old outer bulge) inside the new wheel's volume
            n = cut_cyl(tg, ax[a], 0.60, 0.606, abs(yin) - 0.02, 1.42, s)
            if n: print('DETAIL wheel %s%s remnant faces cut %d' % (a, side, n))
        yc = (yout + yin) / 2; pivot(o, (ax[a], yc, 0.60)); INFO.append('Wheel_%s%s %s axle %.3f %.3f 0.600 dia 1.200 width %.3f' % (a, side, kind, ax[a], yc, abs(yout - yin)))
    for side, s in (('L', 1), ('R', -1)):
        w = PARTS['Wheel_F' + side]; wy = abs(W(w)[:, 1]).min(); c = w.matrix_world.translation
        kx, ky = c.x, s * (wy - 0.16)
        P = Part('SteerKnuckle_' + side, [M['black']])
        P.cyl((kx, ky, c.z - 0.16), (kx, ky, c.z + 0.16), 0.055, 0, 20, 0.006)            # kingpin boss
        P.cyl((kx, ky, c.z), (kx, s * (wy - 0.015), c.z), 0.05, 0, 20, 0.004)               # stub axle, 15 mm short of the hub
        P.box((kx - 0.03, ky - 0.03, c.z - 0.03), (kx - 0.30, ky + 0.03, c.z + 0.02), 0, 0.006)   # steering arm (trailing)
        new['SteerKnuckle_' + side] = o = P.done(sc, (kx, ky, c.z)); INFO.append('SteerKnuckle_%s kingpin %.3f %.3f %.3f (z axis)' % (side, kx, ky, c.z))

    # ------------------------------------------------ fifth wheel (tractor) + kingpin (trailer)
    zt = ZTg - 0.004; R, th, sw = 0.42, 0.07, 0.036
    bm = bmesh.new(); pts = []
    for i in range(41):
        a = math.radians(14 + i * (332 / 40)); pts.append((KPx - R * math.cos(a), R * math.sin(a)))       # opening to the rear (-x)
    for i in range(13):
        a = math.radians(-90 + i * 15); pts.append((KPx + sw * math.cos(a) * -1 + 0.0, -sw * math.sin(a)))
    vs = [bm.verts.new((px, py, zt - th)) for px, py in pts]; f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f]); bmesh.ops.translate(bm, vec=(0, 0, th), verts=[q for q in ext['geom'] if isinstance(q, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bmesh.ops.triangulate(bm, faces=bm.faces)
    me = bpy.data.meshes.new('FifthWheel'); bm.to_mesh(me); bm.free(); me.materials.append(M['black'])
    fw = bpy.data.objects.new('FifthWheel', me); sc.collection.objects.link(fw)
    P = Part('FifthWheelMount', [M['black']])
    for s in (1, -1): P.box((KPx - 0.25, s * 0.30 - 0.06, zt - th - 0.09), (KPx + 0.25, s * 0.30 + 0.06, zt - th - 0.002), 0, 0.01)
    new['FifthWheelMount'] = P.done(sc); new['FifthWheel'] = fw
    P = Part('FifthWheelHandle', [M['black'], M['rubber']])
    P.cyl((KPx + 0.05, 0.40, zt - th / 2), (KPx + 0.05, 0.62, zt - th / 2), 0.012, 0, 12); P.cyl((KPx + 0.05, 0.62, zt - th / 2 - 0.04), (KPx + 0.05, 0.62, zt - th / 2 + 0.04), 0.018, 1, 14)
    new['FifthWheelHandle'] = P.done(sc, (KPx + 0.05, 0.40, zt - th / 2)); IKN['FifthWheelHandle'] = (KPx + 0.05, 0.62, zt - th / 2)
    P = Part('Kingpin', [M['steel']]); P.cyl((KPx, 0, ZTg + 0.010), (KPx, 0, ZTg - 0.064), 0.026, 0, 20); P.cyl((KPx, 0, ZTg - 0.041), (KPx, 0, ZTg - 0.056), 0.034, 0, 20)
    new['Kingpin'] = P.done(sc)
    box_cut(body, (KPx - 0.07, -0.07, ZTg - 0.1), (KPx + 0.07, 0.07, ZTg + 0.06))
    print('DETAIL fifth wheel zone faces cut', box_cut(body, (KPx - 0.46, -0.46, zt - th - 0.1), (KPx + 0.46, 0.46, ZTg - 0.001)))

    # ------------------------------------------------ landing legs (trailer), 2.3 m behind the kingpin
    lx = KPx - 2.3; zleg = 0.95
    for s in (1, -1): print('DETAIL leg zone faces cut', box_cut(trailer, (lx - 0.10, s * 0.95 - 0.10, 0.3), (lx + 0.10, s * 0.95 + 0.10, ZTg - 0.002)) if s > 0 else
                            box_cut(trailer, (lx - 0.10, -1.05, 0.3), (lx + 0.10, -0.85, ZTg - 0.002)))
    P = Part('LandingGear', [M['black']])
    for s in (1, -1):
        P.box((lx - 0.065, s * 0.95 - 0.065, zleg), (lx + 0.065, s * 0.95 + 0.065, ZTg - 0.003), 0, 0.006)
        P.box((lx - 0.01, s * 0.95 - 0.01, zleg + 0.2), (lx + 0.30, s * 0.95 + 0.01, ZTg - 0.15), 0, 0.004)   # brace
    P.cyl((lx, 0.95, zleg + 0.35), (lx, -0.95, zleg + 0.35), 0.022, 0, 14)                                    # cross shaft
    P.box((lx - 0.08, -1.08, zleg + 0.25), (lx + 0.08, -1.015, zleg + 0.45), 0, 0.008)                         # gearbox (right)
    new['LandingGear'] = P.done(sc)
    for side, s in (('L', 1), ('R', -1)):
        P = Part('LandingLeg_' + side, [M['black'], M['rubber']])
        P.box((lx - 0.05, s * 0.95 - 0.05, 0.53), (lx + 0.05, s * 0.95 + 0.05, zleg + 0.25), 0, 0.005)
        P.box((lx - 0.14, s * 0.95 - 0.10, 0.48), (lx + 0.14, s * 0.95 + 0.10, 0.53), 0, 0.008)
        new['LandingLeg_' + side] = P.done(sc, (lx, s * 0.95, zleg)); INFO.append('LandingLeg_%s slides along z, top %.3f %.3f %.3f, foot %.2f above ground (raised)' % (side, lx, s * 0.95, zleg, 0.48))
    P = Part('LandingCrank', [M['black'], M['plastic']])
    P.cyl((lx, -1.08, zleg + 0.35), (lx, -1.14, zleg + 0.35), 0.012, 0, 12); P.box((lx - 0.012, -1.15, zleg + 0.35 - 0.30), (lx + 0.012, -1.13, zleg + 0.36), 0, 0.004)
    P.cyl((lx, -1.15, zleg + 0.06), (lx, -1.27, zleg + 0.06), 0.016, 1, 14)
    new['LandingCrank'] = P.done(sc, (lx, -1.08, zleg + 0.35)); IKN['LandingCrank'] = (lx, -1.24, zleg + 0.06)
    INFO.append('LandingCrank turns about y at %.3f -1.080 %.3f' % (lx, zleg + 0.35))

    # ------------------------------------------------ mirrors on arms (pivot: arm root, folds about z)
    xm = gx(-0.875); zm = gz(0.045)
    print('DETAIL mirror faces cut', delete_faces(body, lambda v: (np.abs(v[:, 1]) > 0.178 * SY) & (v[:, 0] > gx(-0.84)) & (v[:, 2] > gz(-0.035)) & (v[:, 2] < gz(0.115)), True))
    for side, s in (('L', 1), ('R', -1)):
        h = ray(tb, (xm, s * 3.0, zm + 0.3), (0, -s, 0)); yr = abs(h.y) - 0.01 if h is not None and abs(h.y) > HWg - 0.25 else HWg - 0.02
        P = Part('Mirror_' + side, [M['black'], M['mirror']]); ye = s * (HWg + 0.15)
        P.cyl((xm, s * yr, zm + 0.30), (xm - 0.03, ye, zm + 0.30), 0.016, 0, 14); P.cyl((xm, s * yr, zm - 0.15), (xm - 0.03, ye, zm - 0.15), 0.014, 0, 14)
        P.box((xm - 0.09, ye - 0.10, zm - 0.22), (xm - 0.03, ye + 0.10, zm + 0.36), 0, 0.02)
        P.box((xm - 0.095, ye - 0.085, zm - 0.20), (xm - 0.088, ye + 0.085, zm + 0.12), 1, 0.003)
        P.box((xm - 0.095, ye - 0.085, zm + 0.14), (xm - 0.088, ye + 0.085, zm + 0.34), 1, 0.003)
        new['Mirror_' + side] = P.done(sc, (xm, s * yr, zm)); INFO.append('Mirror_%s arm root %.3f %.3f %.3f, folds about z' % (side, xm, s * yr, zm))

    # ------------------------------------------------ lamps: head + indicators (front), tail clusters + side markers (trailer)
    zl = gz(-0.09)
    for side, s in (('L', 1), ('R', -1)):
        for nm, yy, w, h_, ml in (('Light_Head_', 0.125 * SY, 0.30, 0.15, 'lens'), ('Light_Indicator_F', 0.158 * SY, 0.09, 0.10, 'amber')):
            hit = ray(tb, (20.0, s * yy, zl), (-1, 0, 0), 30.0); fx = hit.x + 0.004 if hit is not None else gx(-0.95)
            box_cut(body, (fx - 0.08, s * yy - w / 2 - 0.005, zl - h_ / 2 - 0.005) if s > 0 else (fx - 0.08, -yy - w / 2 - 0.005, zl - h_ / 2 - 0.005),
                    (fx + 0.02, s * yy + w / 2 + 0.005, zl + h_ / 2 + 0.005) if s > 0 else (fx + 0.02, -yy + w / 2 + 0.005, zl + h_ / 2 + 0.005))
            P = Part(nm + side, [M['black'], M['chrome'], M[ml]]); lamp(P, (fx, s * yy, zl), 1, w, h_, 0.075, 0, 1, 2)
            new[nm + side] = P.done(sc, (fx, s * yy, zl))
    zr = ZTg - 0.22
    for side, s in (('L', 1), ('R', -1)):
        P = Part('Light_Tail_' + side, [M['black'], M['chrome'], M['red'], M['amber'], M['lens']])
        P.box((XRg + 0.02, s * 0.95 - 0.02, zr + 0.06), (XRg + 0.06, s * 0.95 + 0.02, ZTg - 0.002), 0, 0.004)   # hanger
        for k, (dy, ml) in enumerate(((0.12, 3), (0.0, 2), (-0.12, 4))):
            lamp(P, (XRg + 0.0, s * 0.95 + s * dy, zr), -1, 0.11, 0.11, 0.06, 0, 1, ml)
        new['Light_Tail_' + side] = P.done(sc, (XRg, s * 0.95, zr))
    P = Part('Light_SideMarkers', [M['black'], M['amber']])
    for s in (1, -1):
        for xx in np.arange(KPx - 1.0, XRg + 0.5, -3.0):
            P.box((xx - 0.05, s * THWg - s * 0.004, ZTg + 0.03), (xx + 0.05, s * (THWg + 0.018), ZTg + 0.09), 0, 0.004)
            P.box((xx - 0.04, s * (THWg + 0.016), ZTg + 0.037), (xx + 0.04, s * (THWg + 0.028), ZTg + 0.083), 1, 0.003)
    new['Light_SideMarkers'] = P.done(sc)

    # ------------------------------------------------ grab handles (flat pillar behind each door), steps, fuel caps
    xg = gx(float(g['XREAR'](0.0))) - 0.07
    for side, s in (('L', 1), ('R', -1)):
        hs = [ray(tb, (xg, s * 3.0, zz), (0, -s, 0)) for zz in (gz(-0.06), gz(0.06))]
        ys = max(abs(h.y) for h in hs if h is not None) if any(hs) else HWg
        P = Part('GrabHandle_' + side, [M['chrome']]); z0, z1 = gz(-0.06), gz(0.06)
        P.cyl((xg, s * (ys + 0.06), z0), (xg, s * (ys + 0.06), z1), 0.016, 0, 16)
        for zz in (z0, z1): P.cyl((xg, s * (ys - 0.005), zz), (xg, s * (ys + 0.075), zz), 0.014, 0, 14)
        new['GrabHandle_' + side] = P.done(sc); IKN['Grab_%s_Low' % side] = (xg, s * (ys + 0.06), z0 + 0.1); IKN['Grab_%s_High' % side] = (xg, s * (ys + 0.06), z1 - 0.1)
        x0, x1 = gx(-0.62), gx(-0.692); zc = gz(-0.088)   # behind the front tyre (ends 1.73 m), below the door rear half
        P = Part('Steps_' + side, [M['black']]); outer = HWg - 0.02
        for k, zz in enumerate((0.50, 0.92)):
            hh = ray(tb, ((x0 + x1) / 2, s * 3.0, zz + 0.02), (0, -s, 0)); inner = max(abs(hh.y) + 0.012 if hh is not None else 0, outer - 0.24)
            if inner > outer - 0.16: outer = inner + 0.22
            P.box((x0 + 0.03, s * inner, zz - 0.035), (x1 - 0.03, s * outer, zz), 0, 0.006)
            for xx in np.arange(x0 + 0.06, x1 - 0.05, 0.05):
                for yy in np.arange(inner + 0.03, outer - 0.02, 0.05): P.box((xx - 0.008, s * yy - 0.008, zz), (xx + 0.008, s * yy + 0.008, zz + 0.006), 0, 0.002)
            IKN['Step_%s_%d' % (side, k + 1)] = ((x0 + x1) / 2, s * (inner + outer) / 2, zz + 0.006)
        for xx in (x0, x1 - 0.02):
            P.box((xx, s * (outer - 0.012), 0.44), (xx + 0.02 if xx == x0 else xx + 0.02, s * outer, zc - 0.005), 0, 0.004)
        new['Steps_' + side] = P.done(sc)
        h = None
        for xx in (gx(-0.62), gx(-0.56), gx(-0.50)):
            h = ray(tb, (xx, s * (HWg - 0.18), 3.0), (0, 0, -1), 3.0)
            if h is not None and 0.8 < h.z < 1.35: break
            h = None
        if h is not None:
            box_cut(body, (h.x - 0.07, h.y - 0.07, h.z + 0.003), (h.x + 0.07, h.y + 0.07, h.z + 0.08))
            P = Part('FuelCap_' + side, [M['chrome']]); P.cyl((h.x, h.y, h.z - 0.018), (h.x, h.y, h.z + 0.041), 0.055, 0, 24, 0.003)
            new['FuelCap_' + side] = P.done(sc, (h.x - 0.055, h.y, h.z + 0.013)); INFO.append('FuelCap_%s hinge %.3f %.3f %.3f (y axis)' % (side, h.x - 0.055, h.y, h.z + 0.013))
        else: print('DETAIL no fuel tank top found on side', side)

    # ------------------------------------------------ mudflaps behind the last tractor and trailer axles
    for unit, key, tgt in (('Tractor', 'D2', body), ('Trailer', 'T3', trailer)):
        for side, s in (('L', 1), ('R', -1)):
            w = PARTS['Wheel_%s%s' % (key, side)]; c = w.matrix_world.translation; r = 0.60
            fx = c.x - r - 0.16; v = W(w); yo, yi = abs(v[:, 1]).max(), abs(v[:, 1]).min()
            box_cut(tgt, (fx - 0.03, min(s * yo, s * yi) - 0.03, 0.15), (fx + 0.03, max(s * yo, s * yi) + 0.03, 1.0))
            P = Part('Mudflap_%s_%s' % (unit, side), [M['rubber'], M['black']])
            P.box((fx - 0.006, min(s * yo, s * yi), 0.24), (fx + 0.006, max(s * yo, s * yi), 0.98), 0, 0.004)
            P.box((fx - 0.015, min(s * yo, s * yi) - 0.01, 0.95), (fx + 0.015, max(s * yo, s * yi) + 0.01, 0.99), 1, 0.003)
            new['Mudflap_%s_%s' % (unit, side)] = P.done(sc)

    # ------------------------------------------------ dark lining inside the cab (glass is see-through; the interior builder replaces it)
    lo = np.array(G((-0.62, 0.15, -0.07))); hi = np.array(G((-0.88, -0.15, 0.16)))
    lo, hi = np.minimum(lo, hi), np.maximum(lo, hi)
    for it in range(12):
        P = Part('CabLining', [M['lining']]); P.box(lo, hi, 0, 0.02); o = P.done(sc)
        d = [tb.find_nearest(Vector(q))[3] for q in W(o)]
        if min(d) > 0.04: break
        bpy.data.objects.remove(o, do_unlink=True); c = (lo + hi) / 2; lo, hi = c + (lo - c) * 0.95, c + (hi - c) * 0.95
    new['CabLining'] = o; INFO.append('CabLining box %s..%s (placeholder for the cab interior)' % (lo.round(2), hi.round(2)))

    # ------------------------------------------------ pivots: doors, glass, trailer
    for side, s in (('L', 1), ('R', -1)):
        PARTS['Door_%s_Glass' % side].data.transform(Matrix.Translation((0, -s * 0.004, 0)))      # glass 4 mm inboard of the door skin (no fight)
        PARTS['RearDoor_' + side].data.transform(Matrix.Translation((-0.003, 0, 0)))             # rear doors 3 mm proud of the frame
        n = [hide_behind(PARTS['Door_' + side], PARTS['Door_%s_Glass' % side], 0.003), hide_behind(body, PARTS['Door_%s_Glass' % side], 0.003),
             hide_behind(trailer, PARTS['RearDoor_' + side], 0.0025), hide_behind(body, new['FuelCap_' + side], 0.002) if 'FuelCap_' + side in new else 0]
        print('DETAIL faces hidden behind door glass / rear door / fuel cap', side, n)
        d = PARTS['Door_' + side]; v = W(d); hx = v[:, 0].max(); hy = s * abs(v[:, 1]).max()
        for o in (d, PARTS['Door_%s_Glass' % side]): pivot(o, (hx, hy, v[:, 2].min()))
        INFO.append('Door_%s hinge (z axis) %.3f %.3f %.3f, opens outwards; Door_%s_Glass shares it' % (side, hx, hy, v[:, 2].min(), side))
        hz = v[:, 2].min() + 0.45 * (v[:, 2].max() - v[:, 2].min()); IKN['DoorHandle_' + side] = (v[:, 0].min() + 0.08, hy + s * 0.02, hz)
        rd = PARTS['RearDoor_' + side]; v = W(rd); hy = s * abs(v[:, 1]).max()
        pivot(rd, (v[:, 0].min(), hy, v[:, 2].min())); INFO.append('RearDoor_%s hinge (z axis) %.3f %.3f %.3f, opens 270 deg' % (side, v[:, 0].min(), hy, v[:, 2].min()))
        IKN['RearDoorHandle_' + side] = (v[:, 0].min() - 0.02, s * 0.15, 1.6)
    print('DETAIL faces hidden behind windscreen / kingpin', hide_behind(body, PARTS['Glass_Windscreen'], 0.003), hide_behind(body, new['Kingpin'], 0.002))
    # ------------------------------------------------ loading: lock bars, hold-back hooks, load edge, cargo floor slots
    tt = tree(trailer); xr = XRg + 0.6
    hf = ray(tt, (xr, 0.0, 3.0), (0, 0, -1), 2.5); zf = min(W(PARTS['RearDoor_' + q])[:, 2].min() for q in 'LR')   # floor top = the rear doors' sill (the box has no inside yet; the ray finds only the underside)
    hw = ray(tt, (xr, 0.0, zf + 1.0), (0, 1, 0), 2.0); iw = 2 * (hw.y if hw is not None and 1.0 < hw.y < 1.3 else THWg - 0.05)
    hx = ray(tt, (xr, 0.0, zf + 1.0), (1, 0, 0), 16.0); xf = hx.x if hx is not None and hx.x > xr + 10 else gx(-0.533) - 0.06
    xb = XRg + 0.02                                                     # inside face of the closed rear doors (sill end)
    INFO.append('Cargo floor top z %.3f, inside %.3f long x %.3f wide (x %.3f..%.3f)' % (zf, xf - xb, iw, xb, xf))
    IKN['LoadEdge'] = (XRg, 0.0, zf); IKN['FloorFront'] = (xf, 0.0, zf)
    for side, s in (('L', 1), ('R', -1)):
        rd = PARTS['RearDoor_' + side]; v = W(rd); xo = v[:, 0].min(); z0, z1 = v[:, 2].min(), v[:, 2].max()
        yb = s * 0.42; xa = xo - 0.035                                   # bar axis, 35 mm proud of the door skin
        P = Part('LockBar_' + side, [M['steel'], M['black']])
        P.cyl((xa, yb, z0 + 0.04), (xa, yb, z1 - 0.04), 0.017, 0, 16)
        for zz in (z0 + 0.10, z1 - 0.10, z0 + 0.55, z1 - 0.70):           # keepers on the door
            P.box((xo - 0.004, yb - 0.035, zz - 0.03), (xa - 0.014, yb + 0.035, zz + 0.03), 1, 0.004)
        zh = z0 + 0.55 + 0.08                                            # cam handle, folded flat towards the doors' middle
        ya, yz = sorted((yb + s * 0.02, yb - s * 0.36)); P.box((xa - 0.03, ya, zh - 0.025), (xa - 0.006, yz, zh + 0.025), 0, 0.005)
        new['LockBar_' + side] = P.done(sc, (xa, yb, z0))
        IKN['LockHandle_' + side] = (xa - 0.01, yb - s * 0.32, zh)
        INFO.append('LockBar_%s axis (z) %.3f %.3f from %.3f, turns about its own axis to unlock; rides with RearDoor_%s' % (side, xa, yb, z0, side))
        hy = abs(v[:, 1]).max() - abs(v[:, 1]).min(); xk = XRg + hy - 0.10; yk = s * (THWg + 0.02); zk = zf + 1.0
        P = Part('DoorHoldBack_' + side, [M['black']])
        P.box((xk - 0.04, yk - s * 0.035, zk - 0.05), (xk + 0.04, yk + s * 0.004, zk + 0.05), 0, 0.004)
        P.box((xk - 0.012, yk, zk - 0.012), (xk + 0.012, yk + s * 0.08, zk + 0.012), 0, 0.003)
        P.box((xk - 0.04, yk + s * 0.068, zk - 0.012), (xk + 0.012, yk + s * 0.08, zk + 0.012), 0, 0.003)
        new['DoorHoldBack_' + side] = P.done(sc); IKN['DoorHoldBack_' + side] = (xk - 0.02, yk + s * 0.075, zk)
    # pallet slots, slot 01 at the front, rows towards the doors (code fills back to front)
    n = 0
    for tag in ('', 'IND_'):                                           # EUR 1.2 x 0.8: 0.8 across; industrial 1.2 x 1.0: 1.2 across
        wa, la = (0.8, 1.2) if not tag else (1.2, 1.0)
        across = int(iw // wa); rows = int((xf - xb - 0.02) // la); k = 0
        for r_ in range(rows):
            for c_ in range(across):
                k += 1; x = xf - 0.01 - la * (r_ + 0.5); y = ((across - 1) / 2 - c_) * (iw / across)
                IKN['Slot_%s%02d' % (tag, k)] = (x, y, zf)
        INFO.append('Slots %s pallets %.1f x %.1f: %d (%d rows x %d across), Slot_%s01 front left' % (tag or 'EUR', la, wa, k, rows, across, tag))
    pivot(trailer, (KPx, 0, ZTg)); INFO.append('Trailer kingpin (articulates about z) %.3f 0 %.3f; fifth wheel top %.3f' % (KPx, ZTg, zt))
    PARTS.update(new)
    tb, tt = tree(body), tree(trailer)
    # overlaps: new parts against the shells (vertices closer than 3 mm)
    for k, o in new.items():
        if k in ('CabLining',): continue
        vv = W(o); n1 = sum(1 for q in vv[::3] if (tb.find_nearest(Vector(q))[3] or 1) < 0.003); n2 = sum(1 for q in vv[::3] if (tt.find_nearest(Vector(q))[3] or 1) < 0.003)
        if n1 + n2 and k.startswith('Wheel_'):
            c = o.matrix_world.translation; bad = np.array([q for q in vv[::3] if min(tb.find_nearest(Vector(q))[3] or 1, tt.find_nearest(Vector(q))[3] or 1) < 0.003])
            print('OVERLAP-AT %s radius %.3f..%.3f |y| %.3f..%.3f' % (k, *(lambda r: (r.min(), r.max()))(np.hypot(bad[:, 0] - c.x, bad[:, 2] - c.z)), abs(bad[:, 1]).min(), abs(bad[:, 1]).max()))
        if n1 + n2: print('OVERLAP %s touches tractor %d trailer %d (sampled vertices)' % (k, n1, n2))
    IKS = []
    for k, p in IKN.items():
        e = bpy.data.objects.new('IK_' + k, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
        e.location = Vector(p); sc.collection.objects.link(e); IKS.append(e); INFO.append('IK_%s %.3f %.3f %.3f' % (k, *p))
    return INFO, IKS
