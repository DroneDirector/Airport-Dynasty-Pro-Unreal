# Modelled details, pivots and IK points for the crate fetcher (called by make_cratefetcher.py; game metres: x forward, y left,
# z up). Built from bevelled primitives (Art/CargoForklift/forklift_details.py) and lathes (Art/Lorry/lorry_details.py):
# 6 round wheels (twin front, single rear, 25 in rims), rear steering knuckles, boom lift cylinder and two carriage tilt
# cylinders (barrel and rod separate), head / tail / work lamps (housing, chrome reflector, ribbed lens), amber beacon,
# mirrors on arms, two steps with studs, grab handle on the flat rear pillar, roof guard, a dark cab lining placeholder
# (the cab interior comes later), steering wheel, joystick, two levers and three pedals.
import bpy, math
import numpy as np
from mathutils import Vector, Matrix
import forklift_details as FD
import lorry_details as LD

Part, lamp, material, W = FD.Part, FD.lamp, FD.material, FD.W


def tyre(P, c, s, y0, w, R, rb):
    pr = [(rb, y0 + 0.03), (rb + 0.012, y0 + 0.012), (rb + 0.11, y0), (R - 0.13, y0 + 0.004), (R - 0.05, y0 + 0.022), (R - 0.006, y0 + 0.05), (R, y0 + 0.075)]
    g = np.linspace(y0 + 0.075, y0 + w - 0.075, 7)
    for k in (1, 3, 5): pr += [(R, g[k] - 0.012), (R - 0.02, g[k] - 0.009), (R - 0.02, g[k] + 0.009), (R, g[k] + 0.012)]
    pr += [(R, y0 + w - 0.075), (R - 0.006, y0 + w - 0.05), (R - 0.05, y0 + w - 0.022), (R - 0.13, y0 + w - 0.004), (rb + 0.11, y0 + w),
           (rb + 0.012, y0 + w - 0.012), (rb, y0 + w - 0.03), (rb, y0 + 0.03)]
    LD.lathe(P, pr, c, s, 0)


def wheel(sc, name, c, s, width, twin, R, M):
    """round off-road wheel: 25 in rim (bead r 0.32), lathed tyre(s) without tread blocks, 10 nuts; c = centre on the outer face"""
    rb = 0.32; P = Part(name, [M['rubber'], M['rim'], M['chrome']])
    tw = [(0.0, (width - 0.08) / 2), ((width + 0.08) / 2, (width - 0.08) / 2)] if twin else [(0.0, width)]
    for y0, w in tw: tyre(P, c, s, y0, w, R, rb)
    back = width; dish = 0.09
    LD.lathe(P, [(0.0, dish + 0.03), (0.10, dish + 0.03), (0.11, dish), (0.21, dish), (0.25, dish + 0.02), (rb - 0.03, 0.04), (rb, 0.015),
                 (rb + 0.012, 0.008), (rb + 0.012, 0.028), (rb - 0.012, 0.05), (rb - 0.012, back - 0.05), (rb + 0.008, back - 0.015),
                 (rb + 0.008, back), (rb - 0.03, back), (rb - 0.03, dish + 0.045), (0.0, dish + 0.045)], c, s, 1)
    LD.lathe(P, [(0.0, dish - 0.05), (0.06, dish - 0.05), (0.085, dish - 0.03), (0.095, dish - 0.005), (0.095, dish + 0.01), (0.0, dish + 0.01)], c, s, 2, 32)
    for i in range(10):
        a = 2 * math.pi * i / 10; x, z = c[0] + 0.17 * math.cos(a), c[2] + 0.17 * math.sin(a)
        P.cyl((x, c[1] - s * (dish - 0.004), z), (x, c[1] - s * (dish - 0.032), z), 0.018, 2, 6, 0.002)
    return P.done(sc)


def cylinder(sc, name, a, b, rb, rr, M, frac=0.58):
    """hydraulic cylinder from pin a (barrel end) to pin b (rod end): Name_Barrel pivots at a, Name_Rod at b (both turn about y)"""
    a, b = Vector(a), Vector(b); d = (b - a).normalized(); L = (b - a).length; e = Vector((0, 1, 0))
    P = Part(name + '_Barrel', [M['paint'], M['black']])
    P.cyl(a + d * (rb * 1.1), a + d * (L * frac), rb, 0, 32, 0.006)
    P.cyl(a + d * (L * frac - 0.02), a + d * (L * frac + 0.012), rb * 1.08, 1, 32, 0.004)          # gland nut
    P.cyl(a - e * rb * 0.9, a + e * rb * 0.9, rb * 0.75, 1, 24, 0.004)                                 # eye on the pin
    ob = P.done(sc, tuple(a))
    P = Part(name + '_Rod', [M['chrome'], M['black']])
    P.cyl(a + d * (L * frac - 0.30), b - d * (rr * 1.2), rr, 0, 32, 0.003)
    P.cyl(b - e * rr * 1.1, b + e * rr * 1.1, rr * 1.15, 1, 24, 0.004)
    orod = P.done(sc, tuple(b))
    return ob, orod


def build(sc, body, PARTS, g):
    SX, SY, SZ, G, WHEEL = g['SX'], g['SY'], g['SZ'], g['G'], g['WHEEL']
    INFO, IKN, new = [], {}, {}
    paint = (0.16, 0.165, 0.17)
    M = dict(black=material('M_CrateFetcher_BlackSteel', (0.02, 0.02, 0.022), 0.45, 0.5), rubber=material('M_CrateFetcher_Rubber', (0.012, 0.012, 0.013), 0.88),
             paint=material('M_CrateFetcher_Paint', paint, 0.38, 0.0), chrome=material('M_CrateFetcher_Chrome', (0.9, 0.9, 0.9), 0.08, 1.0),
             rim=material('M_CrateFetcher_RimGrey', (0.45, 0.46, 0.47), 0.35, 0.7), lens=material('M_CrateFetcher_LensClear', (0.85, 0.87, 0.9), 0.04),
             amber=material('M_CrateFetcher_LensAmber', (1.0, 0.33, 0.0), 0.08, 0.0, (1.0, 0.3, 0.0)), red=material('M_CrateFetcher_LensRed', (0.7, 0.02, 0.01), 0.1),
             mirror=material('M_CrateFetcher_MirrorSilver', (0.92, 0.93, 0.95), 0.02, 1.0), lining=material('M_CrateFetcher_CabLining', (0.03, 0.03, 0.032), 0.7),
             plastic=material('M_CrateFetcher_BlackPlastic', (0.025, 0.025, 0.027), 0.5), tread=material('M_CrateFetcher_Tread', (0.05, 0.05, 0.05), 0.8, 0.3))
    tb = LD.tree(body)
    def ray(p, d, dist=6.0):
        h = tb.ray_cast(Vector(p), Vector(d), dist); return h[0]

    # ------------------------------------------------ wheels (all modelled again, round, standing on the ground), rear knuckles
    for key, (cx, cz, r, y0, y1) in WHEEL.items():
        X = G((cx, 0, cz))[0]; R = r * SZ; yin, yout = (0.50 if key == 'F' else y0 * SY), min(y1 * SY, 1.415); width = yout - yin   # Meshy's front twins reach in to |y| 0.50
        for side, s in (('L', 1), ('R', -1)):
            n = LD.cut_cyl(body, X, R, R + 0.01, yin - 0.02, 1.6, s)
            o = wheel(sc, 'Wheel_%s%s' % (key, side), (X, s * yout, R), s, width, key == 'F', R, M)
            LD.pivot(o, (X, s * (yin + yout) / 2, R)); new[o.name] = o
            INFO.append('Wheel_%s%s %s axle %.3f %.3f %.3f dia %.3f width %.3f (Meshy remnants cut %d)' % (key, side, 'twin' if key == 'F' else 'single', X, s * (yin + yout) / 2, R, 2 * R, width, n))
            if key == 'R':
                ky = s * (yin - 0.17); P = Part('SteerKnuckle_' + side, [M['black']])
                P.cyl((X, ky, R - 0.20), (X, ky, R + 0.20), 0.065, 0, 24, 0.006)
                P.cyl((X, ky, R), (X, s * (yin + 0.02), R), 0.09, 0, 32, 0.006)
                P.box((X - 0.05, ky - 0.04, R + 0.12), (X + 0.30, ky + 0.04, R + 0.19), 0, 0.01)
                new[P.name] = P.done(sc, (X, ky, R)); INFO.append('SteerKnuckle_%s kingpin %.3f %.3f %.3f (z axis)' % (side, X, ky, R))
    XF, XR = G((WHEEL['F'][0], 0, 0))[0], G((WHEEL['R'][0], 0, 0))[0]

    # ------------------------------------------------ boom lift cylinder, carriage tilt cylinders
    a, b = G((g['LC0'][0], 0, g['LC0'][1])), G((g['LC1'][0], 0, g['LC1'][1]))
    ob, orod = cylinder(sc, 'LiftCyl', a, b, 0.095, 0.058, M); new[ob.name] = ob; new[orod.name] = orod
    INFO.append('LiftCyl_Barrel pin %.3f %.3f %.3f, LiftCyl_Rod pin %.3f %.3f %.3f (both about y; the rod pin rides on BoomBase)' % (*a, *b))
    car = W(PARTS['Carriage']); xc0, zc0, zc1 = car[:, 0].min(), car[:, 2].min(), car[:, 2].max()
    for side, s in (('L', 1), ('R', -1)):
        a = (xc0 - 0.34, s * 0.62, zc0 + 0.25); b = (xc0 - 0.05, s * 0.62, zc1 - 0.30)
        ob, orod = cylinder(sc, 'TiltCyl_' + side, a, b, 0.055, 0.032, M, 0.55); new[ob.name] = ob; new[orod.name] = orod
        INFO.append('TiltCyl_%s_Barrel pin %.3f %.3f %.3f (rides on MastInner), _Rod pin %.3f %.3f %.3f (on Carriage)' % (side, *a, *b))

    # ------------------------------------------------ cab box (game), lining placeholder, door
    c0, c1 = G((0.17, -0.125, -0.34)), G((-0.17, -0.37, -0.05))
    cx0, cx1 = min(c0[0], c1[0]), max(c0[0], c1[0]); cy0, cy1 = min(c0[1], c1[1]), max(c0[1], c1[1]); cz0, cz1 = c0[2], c1[2]
    xc, yc = (cx0 + cx1) / 2, (cy0 + cy1) / 2
    h = ray((xc, yc, cz1 - 0.15), (0, 0, -1), 2.0); zfl = h.z if h is not None and h.z > cz0 - 0.4 else cz0
    INFO.append('Cab outside x %.3f..%.3f y %.3f..%.3f roof %.3f, floor %.3f' % (cx0, cx1, cy0, cy1, cz1, zfl))
    P = Part('CabLining', [M['lining']])
    P.box((cx0 + 0.10, cy0 + 0.08, zfl + 0.002), (cx1 - 0.12, cy1 - 0.08, zfl + 0.03), 0, 0.01)            # floor mat
    P.box((cx0 + 0.07, cy0 + 0.08, zfl + 0.03), (cx0 + 0.10, cy1 - 0.08, cz1 - 0.12), 0, 0.01)              # back wall
    P.box((cx0 + 0.20, cy0 + 0.08, zfl + 0.03), (cx0 + 0.60, cy0 + 0.22, zfl + 0.62), 0, 0.02)              # right console
    new['CabLining'] = P.done(sc); INFO.append('CabLining floor, back wall, right console (placeholder for the cab interior)')
    sp = Vector((cx0 + 0.42, yc, zfl + 0.48)); IKN['Seat_Driver'] = tuple(sp); IKN['Back_Driver'] = (cx0 + 0.16, yc, zfl + 0.80)
    # steering wheel on a column leaning back 25 deg
    hub = Vector((sp.x + 0.50, yc, zfl + 0.88)); Rm = Matrix.Rotation(math.radians(-65), 3, 'Y'); ax_ = Rm @ Vector((0, 0, 1))
    P = Part('SteeringWheel', [M['plastic'], M['black']]); R4 = Matrix.Translation(hub) @ Rm.to_4x4()
    P.torus((0, 0, 0), 0.17, 0.016, 0, 48, 12, R4)
    for k in range(3):
        a_ = 2 * math.pi * k / 3 + math.pi / 2; P.box((-0.008, -0.012, -0.012), (0.155, 0.012, 0.004), 0, 0.004, R4 @ Matrix.Rotation(a_, 4, 'Z'))
    P.cyl((0, 0, -0.03), (0, 0, 0.012), 0.045, 1, 24, 0.004, M=R4)
    P.ball((0.13, 0.08, 0.045), 0.02, 0, 12, R4)
    new['SteeringWheel'] = P.done(sc, tuple(hub), Rm)
    P = Part('SteeringColumn', [M['black']]); P.cyl(hub - ax_ * 0.04, hub - ax_ * 0.62, 0.035, 0, 20, 0.004)
    new['SteeringColumn'] = P.done(sc)
    INFO.append('SteeringWheel hub %.3f %.3f %.3f, turns about its local z (%.2f %.2f %.2f), rim r 0.170' % (*hub, *ax_))
    for side, s in (('L', 1), ('R', -1)): IKN['Hand_' + side] = tuple(hub + Rm @ Vector((0, s * 0.17, 0)))
    IKN['Hand_Knob'] = tuple(hub + Rm @ Vector((0.13, 0.08, 0.06)))
    # joystick and two levers on the right console, pedals on the floor
    jb = Vector((cx0 + 0.52, cy0 + 0.15, zfl + 0.62))
    for nm, base, ln, knob in (('Joystick', jb, 0.17, 0.030), ('Lever_1', jb + Vector((-0.17, -0.02, 0)), 0.20, 0.020), ('Lever_2', jb + Vector((-0.24, -0.02, 0)), 0.20, 0.020)):
        P = Part(nm, [M['plastic'], M['black']]); P.ball(base, 0.03, 1, 12, sz=0.5)
        P.cyl(base, base + Vector((0, 0, ln)), 0.009 if nm != 'Joystick' else 0.014, 1, 12, 0.002)
        P.ball(base + Vector((0, 0, ln + knob * 0.6)), knob, 0, 16, sz=1.4 if nm == 'Joystick' else 1.0)
        new[nm] = P.done(sc, tuple(base)); IKN['Hand_' + nm] = tuple(base + Vector((0, 0, ln + knob)))
        INFO.append('%s base %.3f %.3f %.3f, tips about x and y' % (nm, *base) if nm == 'Joystick' else '%s base %.3f %.3f %.3f, tips about y' % (nm, *base))
    for nm, dy in (('Pedal_Inching', 0.20), ('Pedal_Brake', 0.07), ('Pedal_Accel', -0.15)):
        hp = Vector((cx1 - 0.20, yc + dy, zfl + 0.04)); P = Part(nm, [M['black']])
        P.box((hp.x - 0.03, hp.y - 0.04, hp.z), (hp.x - 0.01, hp.y + 0.04, hp.z + 0.20), 0, 0.004, Matrix.Translation(hp) @ Matrix.Rotation(math.radians(-35), 4, 'Y') @ Matrix.Translation(-hp))
        new[nm] = P.done(sc, tuple(hp)); IKN['Foot_' + nm[6:]] = (hp.x - 0.11, hp.y, hp.z + 0.16)
        INFO.append('%s hinge %.3f %.3f %.3f, tips about y' % (nm, *hp))

    # ------------------------------------------------ door pivot, handle, glass
    d = PARTS['Door']; dg = PARTS['Door_Glass']; dg.data.transform(Matrix.Translation((0, -0.004, 0)))
    gc = PARTS['Glass_Cab']
    print('DETAIL fights cleared', LD.hide_behind(gc, d, 0.004), LD.hide_behind(gc, dg, 0.004), LD.hide_behind(gc, PARTS['MastOuter'], 0.004),
          LD.hide_behind(PARTS['Carriage'], PARTS['Fork_R'], 0.003), LD.hide_behind(PARTS['Carriage'], PARTS['Fork_L'], 0.003),
          LD.hide_behind(PARTS['BoomTele'], PARTS['BoomBase'], 0.004), LD.hide_behind(d, dg, 0.007), LD.hide_behind(body, d, 0.004))
    print('DETAIL faces hidden behind door glass', LD.hide_behind(d, dg, 0.003), LD.hide_behind(body, dg, 0.003), LD.hide_behind(body, PARTS['Glass_Cab'], 0.003))
    v = W(d); hx, hy = v[:, 0].max(), v[:, 1].max(); dz0, dz1 = v[:, 2].min(), v[:, 2].max()
    for o in (d, dg): LD.pivot(o, (hx, hy, dz0))
    INFO.append('Door hinge (z axis) %.3f %.3f %.3f at its front edge, opens outwards; Door_Glass shares it' % (hx, hy, dz0))
    hz = dz0 + 0.45 * (dz1 - dz0); xh = v[:, 0].min() + 0.09
    P = Part('DoorHandle', [M['black']]); P.box((xh - 0.07, hy + 0.012, hz - 0.015), (xh + 0.07, hy + 0.035, hz + 0.015), 0, 0.006)
    P.box((xh - 0.07, hy, hz - 0.012), (xh - 0.05, hy + 0.014, hz + 0.012), 0, 0.003); P.box((xh + 0.05, hy, hz - 0.012), (xh + 0.07, hy + 0.014, hz + 0.012), 0, 0.003)
    new['DoorHandle'] = P.done(sc, (xh, hy, hz)); IKN['DoorHandle_Out'] = (xh, hy + 0.03, hz); IKN['DoorHandle_In'] = (xh + 0.04, hy - 0.08, hz)
    INFO.append('DoorHandle rides with Door, pulls about x at %.3f %.3f %.3f' % (xh, hy, hz))

    # ------------------------------------------------ steps (left, under the door), grab handle on the flat rear pillar
    xs = (v[:, 0].min() + hx) / 2
    hit = ray((xs, 3.0, 0.95), (0, -1, 0), 3.0); ys = hit.y if hit is not None else cy1 - 0.1
    P = Part('Steps', [M['black'], M['tread']])
    for k, zz in enumerate((0.52, 0.98)):
        P.box((xs - 0.24, ys - 0.01, zz - 0.04), (xs + 0.24, ys + 0.27, zz), 0, 0.006)
        for i in range(8):
            for j in range(3): P.cyl((xs - 0.21 + i * 0.06, ys + 0.04 + j * 0.08, zz), (xs - 0.21 + i * 0.06, ys + 0.04 + j * 0.08, zz + 0.006), 0.008, 1, 8, 0.0)
        IKN['Step_%d' % (k + 1)] = (xs, ys + 0.14, zz + 0.006)
    for xx in (xs - 0.26, xs + 0.24): P.box((xx, ys - 0.01, 0.48), (xx + 0.02, ys + 0.27, 1.10), 0, 0.004)
    new['Steps'] = P.done(sc); INFO.append('Steps at x %.3f, hung from the frame side y %.3f' % (xs, ys))
    xg = v[:, 0].min() - 0.10; hit = ray((xg, 3.0, 2.0), (0, -1, 0), 3.0); yg = hit.y if hit is not None else cy1
    P = Part('GrabHandle', [M['paint'], M['black']])
    P.cyl((xg, yg + 0.065, dz0 + 0.12), (xg, yg + 0.065, dz1 - 0.15), 0.016, 0, 16)
    for zz in (dz0 + 0.12, dz1 - 0.15): P.cyl((xg, yg - 0.005, zz), (xg, yg + 0.065, zz), 0.018, 1, 16, 0.002)
    new['GrabHandle'] = P.done(sc); IKN['Grab_Low'] = (xg, yg + 0.065, dz0 + 0.25); IKN['Grab_High'] = (xg, yg + 0.065, dz1 - 0.28)

    # ------------------------------------------------ lamps: Meshy's melted lamps cut, modelled ones set into the found faces
    # front lamp blocks sit at raw x -0.545 (face) and rear ones at raw x 0.668 (fender ends), |y| 0.255..0.35, z -0.35..-0.285
    P = Part('Lamps', [M['black'], M['chrome'], M['lens'], M['amber'], M['red']])
    for (x0, x1, xf, s_) in ((-0.99, -0.47, -0.545, 1), (0.62, 0.99, 0.668, -1)):
        lo, hi = G((x0, 0.255, -0.35)), G((x1, 0.35, -0.285)); lo, hi = np.minimum(lo, hi), np.maximum(lo, hi)
        dep = abs(G((xf, 0, 0))[0] - G((x1 if s_ > 0 else x0, 0, 0))[0]) + 0.03
        for sg in (1, -1):
            l_, h_ = lo * [1, sg, 1], hi * [1, sg, 1]
            print('DETAIL melted lamp block cut', LD.box_cut(body, np.minimum(l_, h_), np.maximum(l_, h_)))
            c = G((xf, sg * 0.3025, -0.3175)); w = 0.34
            lamp(P, (c[0], c[1], c[2] + 0.05), s_, w, 0.16, dep, 0, 1, 2 if s_ > 0 else 4)
            lamp(P, (c[0], c[1], c[2] - 0.085), s_, w, 0.10, dep, 0, 1, 3)
            if s_ > 0: IKN['HeadLamp_' + ('L' if sg > 0 else 'R')] = (c[0], c[1], c[2] + 0.05)
            print('DETAIL lamp', 'front' if s_ > 0 else 'rear', np.round(c, 3), 'housing depth %.2f' % dep)
    for yy in (cy0 + 0.12, cy1 - 0.12):                                                        # work lamps on the roof's front edge
        P.box((cx1 - 0.12, yy - 0.03, cz1 + 0.002), (cx1 - 0.06, yy + 0.03, cz1 + 0.07), 0, 0.006)
        lamp(P, (cx1 - 0.02, yy, cz1 + 0.12), 1, 0.16, 0.11, 0.09, 0, 1, 2)
    new['Lamps'] = P.done(sc); INFO.append('Lamps: head + indicator pairs front, tail + indicator pairs rear, 2 work lamps on the roof (one part, static)')
    P = Part('Beacon', [M['black'], M['amber']]); bc = Vector((cx0 + 0.25, yc, cz1 + 0.002))
    P.cyl(bc, bc + Vector((0, 0, 0.04)), 0.075, 0, 32, 0.004); P.cyl(bc + Vector((0, 0, 0.04)), bc + Vector((0, 0, 0.16)), 0.065, 1, 32, 0.006, r2=0.055)
    P.ball(bc + Vector((0, 0, 0.16)), 0.055, 1, 24, sz=0.5); new['Beacon'] = P.done(sc, tuple(bc)); INFO.append('Beacon (amber) on the roof %.3f %.3f %.3f' % tuple(bc))
    # roof guard: frame on four posts 6 cm above the roof, 5 bars
    P = Part('RoofGuard', [M['black']]); zg = cz1 + 0.07
    for xx in (cx0 + 0.08, cx1 - 0.20):
        for yy in (cy0 + 0.06, cy1 - 0.06): P.box((xx - 0.02, yy - 0.02, cz1 + 0.001), (xx + 0.02, yy + 0.02, zg), 0, 0.004)
    for yy in (cy0 + 0.06, cy1 - 0.06): P.box((cx0 + 0.05, yy - 0.025, zg), (cx1 - 0.17, yy + 0.025, zg + 0.04), 0, 0.006)
    for k in range(5): xx = cx0 + 0.10 + k * (cx1 - cx0 - 0.32) / 4; P.box((xx - 0.015, cy0 + 0.04, zg + 0.04), (xx + 0.015, cy1 - 0.04, zg + 0.065), 0, 0.004)
    new['RoofGuard'] = P.done(sc)
    for side, s in (('L', 1), ('R', -1)):                                                       # mirrors on arms from the front pillars
        yr = cy1 if s > 0 else cy0; xm = cx1 - 0.06; zm = cz1 - 0.30
        P = Part('Mirror_' + side, [M['black'], M['mirror']])
        P.cyl((xm, yr, zm), (xm, yr + s * 0.28, zm + 0.10), 0.014, 0, 12)
        P.box((xm - 0.05, yr + s * 0.26 - 0.09, zm + 0.02), (xm, yr + s * 0.26 + 0.09, zm + 0.30), 0, 0.012)
        P.box((xm + 0.0, yr + s * 0.26 - 0.08, zm + 0.03), (xm + 0.004, yr + s * 0.26 + 0.08, zm + 0.29), 1, 0.002)
        new['Mirror_' + side] = P.done(sc, (xm, yr, zm)); INFO.append('Mirror_%s arm root %.3f %.3f %.3f, folds about z' % (side, xm, yr, zm))

    # ------------------------------------------------ pivots of the Meshy parts
    def bx(o): v = W(o); return v.min(0), v.max(0)
    lo, hi = bx(PARTS['MastOuter']); LD.pivot(PARTS['MastOuter'], ((lo[0] + hi[0]) / 2, 0, lo[2])); INFO.append('MastOuter tilt pin %.3f 0 %.3f (y axis)' % ((lo[0] + hi[0]) / 2, lo[2]))
    lo, hi = bx(PARTS['MastInner']); LD.pivot(PARTS['MastInner'], ((lo[0] + hi[0]) / 2, 0, lo[2])); INFO.append('MastInner foot %.3f 0 %.3f, lifts along z; the boom head is pinned to its top %.3f' % ((lo[0] + hi[0]) / 2, lo[2], hi[2]))
    lo, hi = bx(PARTS['Carriage']); LD.pivot(PARTS['Carriage'], (lo[0], 0, lo[2])); INFO.append('Carriage tilt pin back-bottom %.3f 0 %.3f (y axis), lifts with MastInner' % (lo[0], lo[2]))
    for side in 'LR':
        o = PARTS['Fork_' + side]; lo, hi = bx(o); p = (lo[0], (lo[1] + hi[1]) / 2, hi[2]); LD.pivot(o, p)
        INFO.append('Fork_%s hook %.3f %.3f %.3f, rides with Carriage, slides along y; tine %.2f m' % (side, *p, hi[0] - lo[0]))
    bp = G((0.35, 0, -0.15)); LD.pivot(PARTS['BoomBase'], bp); INFO.append('BoomBase luff pin %.3f %.3f %.3f (y axis)' % tuple(bp))
    A, B = G((g['BA'][0], 0, g['BA'][1])), G((g['BB'][0], 0, g['BB'][1])); dv = (B - A) / np.linalg.norm(B - A); tp = A + 0.62 * (B - A)
    LD.pivot(PARTS['BoomTele'], tp); INFO.append('BoomTele foot %.3f %.3f %.3f, slides along (%.3f 0 %.3f) out of BoomBase; head pin %s on the mast top' % (*tp, dv[0], dv[2], G((-0.15, 0, 0.645)).round(3)))
    IKN['Hand_DoorGlass'] = (hx - 0.3, hy + 0.01, dz1 - 0.3)

    # ------------------------------------------------ overlap check: new parts against the shell (sampled vertices closer than 3 mm)
    PARTS.update(new); tb = LD.tree(body)
    for k, o in new.items():
        if k in ('CabLining', 'Lamps', 'GrabHandle') or k.startswith(('Wheel_', 'SteerKnuckle')): continue
        n = sum(1 for q in W(o)[::3] if (tb.find_nearest(Vector(q))[3] or 1) < 0.003)
        if n: q = np.array([q for q in W(o)[::3] if (tb.find_nearest(Vector(q))[3] or 1) < 0.003]); print('OVERLAP %s touches the body (%d sampled vertices) around %s' % (k, n, q.mean(0).round(3)))
    IKS = []
    for k, p in IKN.items():
        e = bpy.data.objects.new('IK_' + k, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
        e.location = Vector(p); sc.collection.objects.link(e); IKS.append(e); INFO.append('IK_%s %.3f %.3f %.3f' % (k, *p))
    return INFO, IKS
