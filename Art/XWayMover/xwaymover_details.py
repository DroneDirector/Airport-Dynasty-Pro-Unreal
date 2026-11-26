# Modelled parts, pivots and IK points of the X-Way mover (called by make_xwaymover.py; game metres: x forward = the deck
# end, y left, z up). Bevelled primitives (Art/CargoForklift/forklift_details.py) and lathes (Art/Lorry/lorry_details.py):
# 4 round wheels (solid industrial tyres, 15 in rims) each on its own steering yoke (360 deg all-wheel / crab steering),
# the lifting deck plate with 12 rollers (each its own part), the stand platform (tread plate, studs), handrail, control
# column with console, small steering wheel, two levers, emergency stop, horn and key, dead-man pedal, rear step, battery
# hatches with pull handles, head / tail lamps (housing, chrome reflector, ribbed lens), amber beacon.
# Layout choice: Meshy put the column at the far rear with the console facing the deck (the operator would look backwards);
# here the column stands at the front of the stand platform and the operator faces the deck, as on the real pallet movers.
import bpy, math
import numpy as np
from mathutils import Vector, Matrix
import forklift_details as FD
import lorry_details as LD

Part, lamp, material, W = FD.Part, FD.lamp, FD.material, FD.W


def wheel(sc, name, c, s, width, R, M):
    """solid industrial tyre on a 15 in steel rim; c = centre of the outer face, s = +1 left / -1 right"""
    rb = 0.53 * R; P = Part(name, [M['rubber'], M['rim'], M['chrome']])
    pr = [(rb, 0.02), (rb + 0.01, 0.008), (rb + 0.06, 0.0), (R - 0.05, 0.003), (R - 0.018, 0.016), (R - 0.004, 0.036), (R, 0.055)]
    pr += [(R, width - 0.055), (R - 0.004, width - 0.036), (R - 0.018, width - 0.016), (R - 0.05, width - 0.003), (rb + 0.06, width),
           (rb + 0.01, width - 0.008), (rb, width - 0.02), (rb, 0.02)]
    LD.lathe(P, pr, c, s, 0)
    dish = 0.05
    LD.lathe(P, [(0.0, dish + 0.02), (0.07, dish + 0.02), (0.08, dish), (0.12, dish), (0.14, dish + 0.012), (rb - 0.012, 0.02), (rb, 0.008),
                 (rb + 0.008, 0.004), (rb + 0.008, 0.018), (rb - 0.006, 0.03), (rb - 0.006, width - 0.03), (rb + 0.006, width - 0.01),
                 (rb + 0.006, width), (rb - 0.02, width), (rb - 0.02, dish + 0.03), (0.0, dish + 0.03)], c, s, 1)
    LD.lathe(P, [(0.0, dish - 0.03), (0.05, dish - 0.03), (0.065, dish - 0.015), (0.07, dish + 0.004), (0.0, dish + 0.004)], c, s, 2, 32)
    for i in range(8):
        a = 2 * math.pi * i / 8; x, z = c[0] + 0.10 * math.cos(a), c[2] + 0.062 * math.sin(a)
        P.cyl((x, c[1] - s * (dish - 0.002), z), (x, c[1] - s * (dish - 0.02), z), 0.009, 2, 6, 0.001)
    return P.done(sc)


def build(sc, body, g):
    SX, SY, SZ, G, WHEEL, ZG = g['SX'], g['SY'], g['SZ'], g['G'], g['WHEEL'], g['ZG']
    INFO, IKN, new = [], {}, {}
    M = dict(black=material('M_XWayMover_BlackSteel', (0.02, 0.02, 0.022), 0.45, 0.5), rubber=material('M_XWayMover_Rubber', (0.008, 0.008, 0.009), 0.9),
             blue=material('M_XWayMover_BluePaint', (0.012, 0.03, 0.11), 0.36, 0.0), chrome=material('M_XWayMover_Chrome', (0.9, 0.9, 0.9), 0.08, 1.0),
             rim=material('M_XWayMover_RimGrey', (0.45, 0.46, 0.47), 0.35, 0.7), lens=material('M_XWayMover_LensClear', (0.85, 0.87, 0.9), 0.04),
             amber=material('M_XWayMover_LensAmber', (1.0, 0.33, 0.0), 0.08, 0.0, (1.0, 0.3, 0.0)), red=material('M_XWayMover_LensRed', (0.7, 0.02, 0.01), 0.1),
             steel=material('M_XWayMover_RollerSteel', (0.62, 0.63, 0.64), 0.28, 1.0), plastic=material('M_XWayMover_BlackPlastic', (0.025, 0.025, 0.027), 0.5),
             tread=material('M_XWayMover_TreadPlate', (0.30, 0.31, 0.32), 0.42, 0.85), grey=material('M_XWayMover_HatchGrey', (0.50, 0.51, 0.52), 0.40, 0.6),
             screen=material('M_XWayMover_Screen', (0.01, 0.012, 0.015), 0.05, 0.0), estop=material('M_XWayMover_EStopRed', (0.55, 0.02, 0.01), 0.3),
             yellow=material('M_XWayMover_SignalYellow', (0.8, 0.6, 0.02), 0.35))
    bv = W(body); BX0, BX1 = bv[:, 0].min(), bv[:, 0].max(); BY = np.abs(bv[:, 1]).max()
    tb = LD.tree(body)
    def ray(p, d, dist=3.0):
        h = tb.ray_cast(Vector(p), Vector(d), dist); return h[0]

    # ------------------------------------------------ wheels on 360 deg steering yokes
    for key, (cx, cz, r, y0, y1) in WHEEL.items():
        X = G((cx, 0, cz))[0]; R = r * SZ; yout = min(y1 * SY, BY - 0.02); width = min(0.22, yout - y0 * SY); yin = yout - width
        for side, s in (('L', 1), ('R', -1)):
            nm = key + side
            n = LD.cut_cyl(body, X, R, R + 0.012, yin - 0.03, 2.0, s)
            o = wheel(sc, 'Wheel_' + nm, (X, s * yout, R), s, width, R, M)
            yc = s * (yin + yout) / 2; LD.pivot(o, (X, yc, R)); new[o.name] = o
            INFO.append('Wheel_%s axle %.3f %.3f %.3f dia %.3f width %.3f (Meshy remnants cut %d)' % (nm, X, yc, R, 2 * R, width, n))
            P = Part('SteerYoke_' + nm, [M['black']])                     # fork over the tyre: kingpin, crown, inner leg to the hub
            P.cyl((X, yc, 2 * R + 0.03), (X, yc, 2 * R + 0.10), 0.05, 0, 24, 0.004)
            P.box((X - 0.08, yc - s * (width / 2 + 0.05), 2 * R + 0.012), (X + 0.08, yc + s * 0.0, 2 * R + 0.04), 0, 0.006) if s > 0 else \
                P.box((X - 0.08, yc, 2 * R + 0.012), (X + 0.08, yc + width / 2 + 0.05, 2 * R + 0.04), 0, 0.006)
            yl = s * (yin - 0.035)
            P.box((X - 0.05, yl - 0.012, R), (X + 0.05, yl + 0.012, 2 * R + 0.04), 0, 0.006)
            P.cyl((X, yl, R), (X, s * (yin + 0.004), R), 0.04, 0, 24, 0.003)
            new[P.name] = P.done(sc, (X, yc, 2 * R + 0.03))
            INFO.append('SteerYoke_%s kingpin %.3f %.3f %.3f (z axis, 360 deg)' % (nm, X, yc, 2 * R + 0.03))

    # ------------------------------------------------ the lifting deck: plate, side rails, roller brackets, 12 rollers
    zc = (g['DECK_CUT'] - ZG) * SZ; xs0 = G((g['STAND_X'], 0, 0))[0]
    dx0, dx1 = xs0 + 0.02, BX1 + 0.01; hw = 1.25; z0, z1 = zc + 0.012, zc + 0.085
    P = Part('Deck', [M['black'], M['blue'], M['yellow']])
    P.box((dx0, -hw, z0), (dx1, hw, z1), 0, 0.012)
    for s in (1, -1):
        P.box((dx0, s * hw - (0.07 if s > 0 else 0), z1 - 0.002), (dx1, s * hw + (0 if s > 0 else 0.07), z1 + 0.10), 1, 0.01)   # side rails
        P.box((dx1 - 0.08, s * 0.05 if s > 0 else -hw + 0.07, z1 - 0.002), (dx1 - 0.004, hw - 0.07 if s > 0 else -0.05, z1 + 0.06), 2, 0.008)  # end stop
    P.box((dx0 + 0.02, -0.06, z1 - 0.002), (dx1 - 0.10, 0.06, z1 + 0.045), 0, 0.008)                   # centre guide
    rows = [G((xr, 0, 0))[0] for xr in (-0.885, -0.66, -0.45, 0.083, 0.29, 0.50)]
    rr, za = 0.055, z1 + 0.012 + 0.055; k = 0
    for xr in rows:
        for s in (1, -1):
            ya, yb = s * 0.10, s * (hw - 0.10)
            for yy, o_ in ((ya, -s), (yb, s)):                                                          # brackets carry the axle stubs
                yb0 = yy + o_ * 0.004
                P.box((xr - 0.05, yb0 - 0.012, z1 - 0.002), (xr + 0.05, yb0 + 0.012, za + 0.03), 0, 0.006)
            k += 1; Q = Part('Roller_%d' % k, [M['steel'], M['black']])
            a, b = Vector((xr, ya + s * 0.02, za)), Vector((xr, yb - s * 0.02, za))
            Q.cyl(a, b, rr, 0, 32, 0.004)
            Q.cyl(a - Vector((0, s * 0.008, 0)), a + Vector((0, s * 0.004, 0)), 0.02, 1, 16, 0.002); Q.cyl(b - Vector((0, s * 0.004, 0)), b + Vector((0, s * 0.008, 0)), 0.02, 1, 16, 0.002)
            new[Q.name] = Q.done(sc, (xr, (ya + yb) / 2, za)); INFO.append('Roller_%d axis %.3f %.3f %.3f, spins about y, r %.3f, rides with Deck' % (k, xr, (ya + yb) / 2, za, rr))
    new['Deck'] = P.done(sc, ((dx0 + dx1) / 2, 0, z0))
    INFO.append('Deck plate x %.3f..%.3f, |y| < %.2f, bottom %.3f, top %.3f, roller tops %.3f; lifts along z (pivot = bottom centre, about 0.10 m)' % (dx0, dx1, hw, z0, z1, za + rr))

    # ------------------------------------------------ stand platform, column, console, handrail, step
    zf0 = (-0.215 - ZG) * SZ + 0.004; zf = zf0 + 0.035; xr0 = BX0
    # Meshy's stand base (tapered slab under the floor) and the deck end wall facing the operator were crumpled: cut and rebuilt
    nb = FD.delete_faces(body, lambda v: v[:, 0] < xs0 - 0.003, True)
    import bmesh
    zb0, zb1 = 0.345, zf0 + 0.012
    ol = [(xr0, 0.20), (xr0 + 0.09, 0.74), (xr0 + 0.19, 0.93), (xs0 - 0.05, 1.24), (xs0 + 0.002, 1.24)]
    ol = ol + [(x_, -y_) for x_, y_ in reversed(ol)]
    t = bmesh.new(); lo_ = [t.verts.new((x_, y_, zb0)) for x_, y_ in ol]; hi_ = [t.verts.new((x_, y_, zb1)) for x_, y_ in ol]
    t.faces.new(list(reversed(lo_))); t.faces.new(hi_)
    for i in range(len(ol)): j = (i + 1) % len(ol); t.faces.new((lo_[i], lo_[j], hi_[j], hi_[i]))
    bmesh.ops.recalc_face_normals(t, faces=t.faces)
    P = Part('StandBase', [M['black'], M['blue']]); P._merge(t, 0)
    P.box((xs0 - 0.02, -1.245, zb0), (xs0 + 0.004, 1.245, 0.829), 1, 0.004)
    new['StandBase'] = P.done(sc); INFO.append('StandBase (static): rebuilt tapered slab x %.3f..%.3f z %.3f..%.3f and deck end wall at x %.3f (Meshy faces cut %d)' % (xr0, xs0, zb0, zb1, xs0, nb))
    P = Part('StandFloor', [M['tread'], M['black']])
    P.box((xr0 + 0.01, -0.92, zf0), (xs0 - 0.01, 0.92, zf), 0, 0.006)
    for i in range(int((xs0 - xr0 - 0.08) / 0.07)):
        for j in range(25): P.box((xr0 + 0.05 + i * 0.07, -0.86 + j * 0.0716 - 0.012, zf - 0.001), (xr0 + 0.05 + i * 0.07 + 0.03, -0.86 + j * 0.0716 + 0.012, zf + 0.004), 0, 0.001)
    new['StandFloor'] = P.done(sc); INFO.append('StandFloor x %.3f..%.3f |y| < 0.92, top %.3f' % (xr0, xs0, zf))
    xc = xs0 - 0.14; P = Part('ControlColumn', [M['blue'], M['black'], M['screen']])
    P.box((xc - 0.08, -0.12, zf - 0.01), (xc + 0.08, 0.12, zf + 1.02), 0, 0.012)
    hz = zf + 1.02; cm = Matrix.Translation((xc, 0, hz)) @ Matrix.Rotation(math.radians(-32), 4, 'Y')   # console top slopes towards the operator (-x)
    P.box((-0.16, -0.30, 0.0), (0.10, 0.30, 0.10), 1, 0.012, cm)
    P.box((-0.12, 0.05, 0.1005), (0.06, 0.26, 0.103), 2, 0.002, cm)                                      # display
    new['ControlColumn'] = P.done(sc)
    top = lambda u, v_, w=0.103: cm @ Vector((u, v_, w))
    sw = top(-0.04, -0.12, 0.103); ax = (cm.to_3x3() @ Vector((0, 0, 1))).normalized(); Rm = cm.to_3x3()
    P = Part('SteeringWheel', [M['plastic'], M['black']]); R4 = Matrix.Translation(sw + ax * 0.06) @ Rm.to_4x4()
    P.torus((0, 0, 0), 0.13, 0.014, 0, 40, 10, R4)
    for kk in range(3): P.box((-0.006, -0.01, -0.01), (0.12, 0.01, 0.003), 0, 0.003, R4 @ Matrix.Rotation(2 * math.pi * kk / 3, 4, 'Z'))
    P.cyl((0, 0, -0.06), (0, 0, 0.012), 0.03, 1, 20, 0.003, M=R4); P.ball((0.10, 0.06, 0.035), 0.018, 0, 12, R4)
    hub = sw + ax * 0.06; new['SteeringWheel'] = P.done(sc, tuple(hub), Rm)
    INFO.append('SteeringWheel hub %.3f %.3f %.3f, turns about its local z (%.2f %.2f %.2f), rim r 0.130' % (*hub, *ax))
    for side, s in (('L', 1), ('R', -1)): IKN['Hand_' + side] = tuple(hub + Rm @ Vector((0, s * 0.13, 0)))
    IKN['Hand_Knob'] = tuple(hub + Rm @ Vector((0.10, 0.06, 0.05)))
    for nm, (u, v_) in (('Lever_DeckLift', (-0.06, 0.18)), ('Lever_Rollers', (0.03, 0.18))):
        b0 = top(u, v_ + 0.02, 0.10); P = Part(nm, [M['plastic'], M['black']]); P.ball(b0, 0.02, 1, 12, sz=0.5)
        P.cyl(b0, b0 + ax * 0.11, 0.007, 1, 12, 0.002); P.ball(b0 + ax * 0.125, 0.018, 0, 14)
        new[nm] = P.done(sc, tuple(b0)); IKN['Hand_' + nm] = tuple(b0 + ax * 0.13); INFO.append('%s base %.3f %.3f %.3f, tips about y' % (nm, *b0))
    for nm, (u, v_), r_, h_, mi in (('EStop', (-0.10, -0.25), 0.03, 0.035, 'estop'), ('HornButton', (0.05, -0.25), 0.016, 0.012, 'black'), ('KeySwitch', (0.05, 0.0), 0.014, 0.01, 'chrome')):
        b0 = top(u, v_, 0.10); P = Part(nm, [M[mi], M['black']]); P.cyl(b0 + ax * 0.003, b0 + ax * h_, r_, 0, 24, 0.004)
        P.cyl(b0 - ax * 0.002, b0 + ax * 0.006, r_ * 1.35, 1, 24, 0.002)
        new[nm] = P.done(sc, tuple(b0)); IKN['Hand_' + nm] = tuple(b0 + ax * h_); INFO.append('%s at %.3f %.3f %.3f, presses along the console normal' % (nm, *b0))
    st = Vector((xc - 0.48, 0, zf)); IKN['Stand_Operator'] = tuple(st); IKN['Foot_L'] = (st.x, 0.15, zf); IKN['Foot_R'] = (st.x, -0.15, zf)
    hp = Vector((st.x + 0.12, -0.15, zf)); P = Part('Pedal_DeadMan', [M['black'], M['plastic']])
    P.box((hp.x - 0.16, hp.y - 0.09, hp.z + 0.022), (hp.x + 0.02, hp.y + 0.09, hp.z + 0.048), 1, 0.006, Matrix.Translation(hp) @ Matrix.Rotation(math.radians(-6), 4, 'Y') @ Matrix.Translation(-hp))
    new['Pedal_DeadMan'] = P.done(sc, tuple(hp)); IKN['Foot_DeadMan'] = (hp.x - 0.07, hp.y, hp.z + 0.04); INFO.append('Pedal_DeadMan hinge %.3f %.3f %.3f, tips about y' % tuple(hp))
    P = Part('Handrail', [M['yellow'], M['black']]); yr = 0.86; zt = zf + 1.0
    for s in (1, -1):
        for xx in (xc - 0.02, xr0 + 0.12):
            P.cyl((xx, s * yr, zf - 0.002), (xx, s * yr, zt), 0.02, 0, 16, 0.003)
            P.box((xx - 0.04, s * yr - 0.04, zf - 0.003), (xx + 0.04, s * yr + 0.04, zf + 0.009), 1, 0.003)
        P.cyl((xc - 0.02, s * yr, zt), (xr0 + 0.12, s * yr, zt), 0.02, 0, 16, 0.003); P.cyl((xc - 0.02, s * yr, zf + 0.55), (xr0 + 0.12, s * yr, zf + 0.55), 0.016, 0, 16, 0.003)
        P.cyl((xc - 0.02, s * yr, zt - 0.25), (xc - 0.02, s * 0.12, zt - 0.25), 0.016, 0, 16, 0.003)                  # front bar into the column
    new['Handrail'] = P.done(sc); INFO.append('Handrail: posts at x %.3f / %.3f, |y| %.2f, top %.3f; open at the rear for getting on' % (xc - 0.02, xr0 + 0.12, yr, zt))
    for s, nm in ((1, 'L'), (-1, 'R')): IKN['Grab_' + nm] = (xr0 + 0.12, s * yr, zf + 0.85)
    P = Part('Step', [M['black'], M['tread']]); zs = zf / 2
    P.box((xr0 - 0.22, -0.40, zs - 0.03), (xr0 - 0.005, 0.40, zs), 0, 0.006)
    for s in (1, -1): P.box((xr0 - 0.06, s * 0.40 - (0.02 if s > 0 else 0), zs - 0.03), (xr0 - 0.005, s * 0.40 + (0 if s > 0 else 0.02), zf0 - 0.06), 0, 0.004)
    for i in range(5):
        for j in range(10): P.cyl((xr0 - 0.19 + i * 0.04, -0.34 + j * 0.076, zs), (xr0 - 0.19 + i * 0.04, -0.34 + j * 0.076, zs + 0.005), 0.007, 1, 8, 0.0)
    new['Step'] = P.done(sc); IKN['Step_1'] = (xr0 - 0.11, 0, zs + 0.005); IKN['Step_2'] = (xr0 + 0.15, 0, zf)
    INFO.append('Step at the rear x %.3f..%.3f, top %.3f (stand floor %.3f)' % (xr0 - 0.22, xr0, zs, zf))

    # ------------------------------------------------ battery hatches (Meshy's grey panels cut, flat ones modelled), hinge at the bottom
    for side, (rx0, rx1, rz0, rz1, ry) in g['HATCH'].items():
        s = 1 if side == 'L' else -1
        a, b = G((rx0, 0, rz0)), G((rx1, 0, rz1)); hx0, hx1 = sorted((a[0], b[0])); hz0, hz1 = sorted((a[2], b[2]))
        ys = ry * SY
        n = LD.box_cut(body, np.array([hx0, -9 if s < 0 else ys - 0.05, hz0]), np.array([hx1, 9 if s > 0 else -ys + 0.05, hz1]))
        P = Part('Hatch_' + side, [M['grey'], M['black']])
        P.box((hx0 - 0.015, min(s * (ys + 0.004), s * (ys + 0.026)), hz0 - 0.015), (hx1 + 0.015, max(s * (ys + 0.004), s * (ys + 0.026)), hz1 + 0.015), 0, 0.008)
        for zz in np.linspace(hz0 + 0.06, hz1 - 0.06, 4):                                               # stamped ribs
            P.box((hx0 + 0.06, min(s * (ys + 0.025), s * (ys + 0.031)), zz - 0.006), (hx1 - 0.06, max(s * (ys + 0.025), s * (ys + 0.031)), zz + 0.006), 0, 0.002)
        new[P.name] = P.done(sc, ((hx0 + hx1) / 2, s * (ys + 0.004), hz0 - 0.015))
        xm = (hx0 + hx1) / 2; hz = hz1 - 0.05; P = Part('HatchHandle_' + side, [M['black']])
        P.box((xm - 0.08, min(s * (ys + 0.028), s * (ys + 0.052)), hz - 0.012), (xm + 0.08, max(s * (ys + 0.028), s * (ys + 0.052)), hz + 0.012), 0, 0.005)
        new[P.name] = P.done(sc, (xm, s * (ys + 0.031), hz)); IKN['HatchHandle_' + side] = (xm, s * (ys + 0.06), hz)
        INFO.append('Hatch_%s x %.3f..%.3f z %.3f..%.3f, hinge along x at the bottom edge (y %.3f), folds out about x; HatchHandle_%s rides with it (Meshy faces cut %d)' % (side, hx0, hx1, hz0, hz1, s * (ys + 0.004), side, n))

    # ------------------------------------------------ lamps set into the end faces, beacon on the column
    P = Part('Lamps', [M['black'], M['chrome'], M['lens'], M['amber'], M['red']])
    spots = []
    for end, sx, c, lo, hi in g['LAMPS']:
        p = G(tuple(c)); spots.append((1 if end == 'front' else -1, p[1], p[2]))
    if not spots: spots = [(1, 0.95, 0.55), (1, -0.95, 0.55), (-1, 0.95, 0.25), (-1, -0.95, 0.25)]
    for s_, yy, zz in spots:
        xf = BX1 if s_ > 0 else BX0
        print('DETAIL lamp cut', LD.box_cut(body, np.array([xf - 0.06 if s_ > 0 else xf - 0.01, yy - 0.13, zz - 0.09]), np.array([xf + 0.01 if s_ > 0 else xf + 0.06, yy + 0.13, zz + 0.09])))
        lamp(P, (xf + 0.004 * s_, yy + 0.04 * np.sign(yy), zz), s_, 0.14, 0.10, 0.05, 0, 1, 2 if s_ > 0 else 4)
        lamp(P, (xf + 0.004 * s_, yy - 0.08 * np.sign(yy), zz), s_, 0.08, 0.10, 0.05, 0, 1, 3)
        if s_ > 0: IKN['HeadLamp_' + ('L' if yy > 0 else 'R')] = (xf, yy, zz)
    new['Lamps'] = P.done(sc); INFO.append('Lamps: %d head/tail + indicator pairs in the end faces (one part, static)' % len(spots))
    P = Part('Beacon', [M['black'], M['amber']]);     bc = Vector((xc + 0.04, -0.22, zf + 1.02 + 0.16))
    P.cyl(bc - Vector((0, 0, 0.11)), bc, 0.02, 0, 12, 0.002)
    P.cyl(bc, bc + Vector((0, 0, 0.03)), 0.06, 0, 32, 0.004); P.cyl(bc + Vector((0, 0, 0.03)), bc + Vector((0, 0, 0.13)), 0.05, 1, 32, 0.005, r2=0.042)
    P.ball(bc + Vector((0, 0, 0.13)), 0.042, 1, 24, sz=0.5); new['Beacon'] = P.done(sc, tuple(bc)); INFO.append('Beacon (amber) on a mast at the console %.3f %.3f %.3f' % tuple(bc))

    # side bands: Meshy's frame band (z 0.64..0.78) carries embossed lettering - covered by clean modelled steel panels
    P = Part('SideBands', [M['black']])
    for sg in (1, -1):
        y0, y1 = sorted((sg * 1.232, sg * 1.258))
        P.box((-1.598, y0, 0.706), (1.888, y1, 0.829), 0, 0.004)
    new['SideBands'] = P.done(sc); INFO.append('SideBands (static) x -1.598..1.888, z 0.706..0.829, outer face |y| 1.258')
    # ------------------------------------------------ overlap check: new parts against the shell (sampled vertices closer than 3 mm)
    tb = LD.tree(body)
    for k, o in new.items():
        if k in ('Lamps',) or k.startswith(('Wheel_', 'Hatch')): continue
        q = np.array([q for q in W(o)[::3] if (tb.find_nearest(Vector(q))[3] or 1) < 0.003])
        if len(q): print('OVERLAP %s touches the body (%d sampled vertices) around %s' % (k, len(q), q.mean(0).round(3)))
    IKS = []
    for k, p in IKN.items():
        e = bpy.data.objects.new('IK_' + k, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
        e.location = Vector(p); sc.collection.objects.link(e); IKS.append(e); INFO.append('IK_%s %.3f %.3f %.3f' % (k, *p))
    return INFO, IKS
