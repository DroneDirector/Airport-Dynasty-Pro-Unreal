# Tripod aircraft jacks (to-do 364 A), our own models in two sizes. Looked at: US Navy photos on Wikimedia Commons (public
# domain) "use an aircraft jack to lift an EA-6B Prowler" (091211-N-2367S-213) and "removes a ring lock from an aircraft
# jack" (130807-N-XZ031-072): a hydraulic cylinder on a round base, three tube legs from a collar near its top to round
# foot pads, horizontal struts from the base to the legs, a threaded ram with a ring lock nut, a screw extension with a
# cup pad on top, a hand pump on the base. Sizes from published tripod-jack data for A320-class aircraft:
#   wing jack: closed 2.05 m, ram stroke 0.95 m, screw 0.50 m -> 3.50 m reach, feet on a 1.05 m radius
#   nose jack: closed 1.05 m, ram stroke 0.45 m, screw 0.15 m -> 1.65 m reach, feet on a 0.62 m radius
# The castors are spring castors: they carry the jack when it is pushed and give way when it takes the aircraft's load.
import math
from hq_lib import *

SIZES = {
    'WingJack': dict(closed=2.05, stroke=0.95, screw=0.50, rf=1.05, rc=0.09, rr=0.065, rs=0.045, base=0.30, collar=1.66,
                     leg=0.038, cr=0.075, handle=0.95, push=1.0),
    'NoseJack': dict(closed=1.05, stroke=0.45, screw=0.15, rf=0.62, rc=0.07, rr=0.05, rs=0.035, base=0.22, collar=0.80,
                     leg=0.032, cr=0.05, handle=0.70, push=0.62),
}


def build(name):
    S = SIZES[name]; reset(); materials('TJ' + name[0])
    rc, rr, rs, zb, zc = S['rc'], S['rr'], S['rs'], S['base'], S['collar']
    ztop = S['closed']; zpad = ztop - 0.05                 # pad cup 5 cm, screw top under it
    zcyl = zc + 0.18                                       # cylinder top
    ANG = [math.radians(a) for a in (-90, 30, 150)]        # legs; the pump side (+y) is open
    rleg0 = rc + 0.03
    def leg_r(z): return rleg0 + (zc - z) / (zc - 0.03) * (S['rf'] - rleg0)
    f = B()
    f.lathe([(0.0, zb - 0.05), (rc + 0.07, zb - 0.05), (rc + 0.07, zb), (0.0, zb)], 'G', (0, 0, 0), 'z', 32)     # base plate
    f.lathe([(rc, zb), (rc, zcyl), (rc - 0.012, zcyl), (rc - 0.012, zcyl - 0.01)], 'G', (0, 0, 0), 'z', 32)   # cylinder
    f.lathe([(rc + 0.025, zc - 0.05), (rc + 0.025, zc + 0.05), (rc, zc + 0.05)], 'Y', (0, 0, 0), 'z', 32)  # leg collar
    for a in ANG:
        c, s = math.cos(a), math.sin(a)
        top = Vector((rleg0 * c, rleg0 * s, zc)); foot = Vector((S['rf'] * c, S['rf'] * s, 0.05))
        f.tube(top, foot, S['leg'], 'Y', 16)
        f.lathe([(0.0, 0.0), (0.085, 0.0), (0.085, 0.025), (0.05, 0.05), (0.0, 0.05)], 'G', (foot.x, foot.y, 0), 'z', 20)
        zs = zb - 0.025; rs_ = leg_r(zs)
        f.tube((c * (rc + 0.05), s * (rc + 0.05), zs), (c * rs_, s * rs_, zs), 0.028, 'Y', 12)                    # base strut
        # push handle grips on two legs at hip height
    for a in ANG[1:]:
        c, s = math.cos(a), math.sin(a); z = S['push']; r = leg_r(z)
        f.tube((c * r, s * r, z), (c * (r + 0.14), s * (r + 0.14), z), 0.018, 'Y', 12)
        f.tube((c * (r + 0.14) - s * 0.12, s * (r + 0.14) + c * 0.12, z), (c * (r + 0.14) + s * 0.12, s * (r + 0.14) - c * 0.12, z), 0.022, 'BK', 14)
    # hand pump on the base, +y side: body, reservoir, handle socket lugs, release valve boss
    f.box((-0.07, rc + 0.04, zb), (0.07, rc + 0.20, zb + 0.14), 'RD', 0.006)
    f.lathe([(0.045, 0.0), (0.045, 0.22), (0.0, 0.24)], 'G', (0, rc + 0.12, zb + 0.14), 'z', 20)
    for sx in (-1, 1): f.box((sx * 0.035 - 0.006, rc + 0.15, zb + 0.14), (sx * 0.035 + 0.006, rc + 0.21, zb + 0.20), 'ST', 0.002)
    f.lathe([(0.018, 0.0), (0.018, 0.03)], 'ST', (0.07, rc + 0.09, zb + 0.07), 'x', 14)
    f.tube((0, rc + 0.05, zb + 0.03), (0, 0.0, zb + 0.03), 0.008, 'ST')   # pressure line into the cylinder foot
    part('Frame', f, (0, 0, 0), None, 'root: the jack, pushed on its spring castors')

    for k, a in enumerate(ANG):
        c, s = math.cos(a), math.sin(a); zs = zb - 0.025; r = leg_r(zs) - 0.20
        castor(str(k + 1), c * r, s * r, 'Frame', S['cr'], 0.04, zs - 0.028 - 0.003)

    # ram: slides up out of the cylinder; carries the lock nut and the screw extension
    ram = B(); hr = S['stroke'] + 0.1
    ram.lathe([(0.0, zcyl - hr), (rr, zcyl - hr), (rr, zcyl + 0.08), (rs + 0.012, zcyl + 0.08), (rs + 0.012, zcyl + 0.075)], 'CR', (0, 0, 0), 'z', 28)
    part('Ram', ram, (0, 0, zcyl), 'Frame', 'slides up z 0..%.2f m' % S['stroke'])
    ln = B(); zl = zcyl + 0.004
    ln.lathe([(rr + 0.002, zl), (rc + 0.03, zl), (rc + 0.03, zl + 0.06), (rr + 0.002, zl + 0.06)], 'G', (0, 0, 0), 'z', 8)
    for a in (0, math.pi):
        ln.tube((math.cos(a) * (rc + 0.025), math.sin(a) * (rc + 0.025), zl + 0.03), (math.cos(a) * (rc + 0.16), math.sin(a) * (rc + 0.16), zl + 0.03), 0.012, 'ST', 10)
    part('LockNut', ln, (0, 0, zl), 'Ram', 'turns about z (runs down the ram thread onto the cylinder): 0..%.2f m down' % S['stroke'])
    sc = B(); zs0 = zcyl + 0.08
    sc.lathe([(0.0, zs0 - S['screw'] + 0.02), (rs, zs0 - S['screw'] + 0.02), (rs, zpad), (0.0, zpad)], 'ST', (0, 0, 0), 'z', 24)
    sc.lathe([(0.0, zpad), (rs + 0.02, zpad), (rs + 0.04, ztop - 0.012), (rs + 0.04, ztop), (rs + 0.025, ztop), (0.0, ztop - 0.015)], 'G', (0, 0, 0), 'z', 24)
    part('Extension_Screw', sc, (0, 0, zs0), 'Ram', 'screws up z 0..%.2f m (with the lift pad)' % S['screw'])
    # pump handle (hinged on its lugs, about x) and the release valve knob (turns about x)
    ph = B(); P = (0, rc + 0.18, zb + 0.18)
    ph.tube((-0.038, P[1], P[2]), (0.038, P[1], P[2]), 0.008, 'CR')
    hd = Vector((0, 0.5, 0.866)); e = Vector(P) + hd * S['handle']
    ph.tube(P, e, 0.016, 'ST'); ph.tube(e - hd * 0.18, e, 0.021, 'BK')
    part('PumpHandle', ph, P, 'Frame', 'pumps about x: -15..+35 deg')
    rv = B(); P2 = (0.088, rc + 0.09, zb + 0.07)
    rv.lathe([(0.008, 0.0), (0.008, 0.03), (0.025, 0.03), (0.025, 0.05), (0.0, 0.05)], 'BK', P2, 'x', 16)
    part('ReleaseValve', rv, P2, 'Frame', 'turns about x 0..360 deg (open = ram goes down)')

    ik('LiftPad', (0, 0, ztop), 'contact', 'Extension_Screw')
    ik('PumpHandle_Grip', tuple(e - hd * 0.09), 'hand', 'PumpHandle')
    ik('ReleaseValve', (P2[0] + 0.05, P2[1], P2[2]), 'hand', 'ReleaseValve')
    ik('LockNut_A', (rc + 0.14, 0, zl + 0.03), 'hand', 'LockNut'); ik('LockNut_B', (-(rc + 0.14), 0, zl + 0.03), 'hand', 'LockNut')
    for k, a in enumerate(ANG[1:]):
        c, s = math.cos(a), math.sin(a); r = leg_r(S['push']) + 0.14
        ik('Push_%d' % (k + 1), (c * r, s * r, S['push']), 'push', 'Frame')
    ctrl('Pump handle (raise)', 'PumpHandle', 'PumpHandle_Grip', 'pump up and down: each stroke raises the ram')
    ctrl('Release valve (lower)', 'ReleaseValve', 'ReleaseValve', 'turn open = the ram goes down slowly, close = holds')
    ctrl('Lock nut', 'LockNut', 'LockNut_A', 'spin down onto the cylinder = load locked mechanically')
    ctrl('Extension screw', 'Extension_Screw', 'LiftPad', 'wind up to the jacking point before pumping')
    rf = S['rf'] + 0.085
    finish(name, 'tripod aircraft jack (%s)' % ('wing' if name == 'WingJack' else 'nose'),
           {'closed_m': S['closed'], 'ram_stroke_m': S['stroke'], 'screw_m': S['screw'],
            'max_reach_m': round(S['closed'] + S['stroke'] + S['screw'], 2), 'feet_radius_m': S['rf']},
           {'shape': 'tripod', 'radius_m': round(rf, 3), 'x_min': round(-rf * math.cos(math.radians(30)), 3),
            'x_max': round(rf * math.cos(math.radians(30)), 3), 'y_min': round(-rf, 3), 'y_max': round(max(rf * 0.5, rc + 0.21), 3),
            'note': 'three foot pads on the radius; the open side (+y, pump) faces the mechanic'},
           'Wikimedia Commons US Navy 091211-N-2367S-213 and 130807-N-XZ031-072 (public domain); published tripod-jack sizes')
