# Aircraft wheel and brake dolly (to-do 364 A, batch 2), our own model, no brands. No usable free photo found: built
# from published sizes of wheel / brake dollies for airliner main wheels (A320 main wheel 46x17.0R20: 1.17 m across,
# 0.43 m wide, about 200 kg with the brake): a yellow U-shaped chassis open at the front on four 150 mm braked castors, a
# rear mast with a lifting carriage raised by a hydraulic hand pump (foot pedal, release valve), two L-shaped fork arms
# with rollers that slide under the tyre and are spread to the tyre's size by a crank and a lead screw. The dolly is
# pushed in along the axle so the arms pass either side of the tyre's contact patch; the tyre then rolls on the rollers.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_joints import *

reset(); detail_materials('WDL')
R = 0.075; CT = 2 * R + 0.07; Z0 = CT + 0.004; ZR = Z0 + 0.06          # castor plate top, rails bottom / top
f = D()
for s in (-1, 1):
    f.box((s * 0.42, -0.56, Z0), (s * 0.48, 0.56, ZR), 'Y', 0.004)                     # side rails, 60x60 tube
    f.box((s * 0.45 - 0.033, 0.56 - 0.001, Z0 - 0.003), (s * 0.45 + 0.033, 0.566, ZR + 0.003), 'Y', 0.002)   # end plate
    gusset(f, (s * 0.42, -0.47, Z0 + 0.02), (-s, 0, 0), (0, 1, 0), 0.08, 0.008, 'Y')    # rear corner gussets between the tubes
    f.label((s * 0.4806, 0.1, Z0 + 0.03), (s, 0, 0), 0.16, 0.035, 'LW')
f.box((-0.425, -0.56, Z0), (0.425, -0.47, ZR), 'Y', 0.004)                              # rear cross member
for s in (-1, 1):
    f.weld((s * 0.4195, -0.555, ZR - 0.002), (s * 0.4195, -0.475, ZR - 0.002), 'Y', 0.004)
# mast: two channels welded into the cross member, tie on top, diagonal braces to the rails
for s in (-1, 1):
    f.box((s * 0.135, -0.53, ZR - 0.005), (s * 0.185, -0.47, 1.255), 'Y', 0.003)
    for y in (-0.533, -0.467): f.weld((s * 0.135, y, ZR + 0.002), (s * 0.185, y, ZR + 0.002), 'Y', 0.004)
    f.bar((s * 0.17, -0.5, 0.85), (s * 0.44, -0.5, ZR - 0.004), 0.035, 0.03, 'Y', 0.003, up=(0, 1, 0))
    gusset(f, (s * 0.185, -0.5, 1.25), (s, 0, 0), (0, 0, -1), 0.024, 0.008, 'Y')
f.box((-0.21, -0.535, 1.25), (0.21, -0.465, 1.28), 'Y', 0.003)
f.label((0.0, -0.5355, 1.265), (0, -1, 0), 0.2, 0.022, 'LW')
# push handle: tubes back from the mast, ball corners, cross grip with rubber
for s in (-1, 1):
    f.tube((s * 0.16, -0.5245, 1.0), (s * 0.16, -0.72, 1.0), 0.017, 'Y')
    f.lz([(0.0, -0.019), (0.013, -0.017), (0.019, 0.0), (0.013, 0.017), (0.0, 0.019)], 'Y', frame((s * 0.16, -0.72, 1.0), (1, 0, 0)), 16)
f.tube((-0.16, -0.72, 1.0), (0.16, -0.72, 1.0), 0.017, 'Y', 14, weld=False)
f.tube((-0.13, -0.72, 1.0), (0.13, -0.72, 1.0), 0.021, 'BK', 16, weld=False)
# hydraulic cylinder on its base plate on the cross member
f.box((-0.06, -0.556, ZR - 0.001), (0.06, -0.464, ZR + 0.011), 'Y', 0.002)
for x in (-0.045, 0.045):
    for y in (-0.545, -0.475): f.bolt((x, y, ZR + 0.011), (0, 0, 1), 0.006)
f.lz([(0.0, ZR + 0.01), (0.04, ZR + 0.01), (0.04, 0.84), (0.046, 0.845), (0.046, 0.865), (0.03, 0.87), (0.0, 0.87)], 'G', frame((0, -0.51, 0), (0, 0, 1)), 28)
f.fitting((0.04, -0.51, 0.33), (1, 0, 0), 0.007)
# hand pump on a plate over the back of the cross member, supported by a gusset
f.box((0.23, -0.65, ZR - 0.001), (0.37, -0.5, ZR + 0.007), 'Y', 0.002)
gusset(f, (0.3, -0.56, ZR - 0.001), (0, -1, 0), (0, 0, -1), 0.05, 0.008, 'Y')
for x in (0.24, 0.36): f.bolt((x, -0.49 - 0.02, ZR + 0.007), (0, 0, 1), 0.006)
f.sheet((0.25, -0.64, ZR + 0.006), (0.35, -0.565, 0.37), 'RD', 0.006)
f.lz([(0.0, 0.0), (0.03, 0.0), (0.03, 0.1), (0.032, 0.103), (0.032, 0.11), (0.0, 0.112)], 'RD', frame((0.3, -0.6, 0.365), (0, 0, 1)), 20)  # reservoir
f.lz([(0.0, 0.0), (0.012, 0.0), (0.012, 0.018), (0.0, 0.02)], 'BK', frame((0.3, -0.6, 0.476), (0, 0, 1)), 6)                            # filler plug
f.label((0.3, -0.6406, 0.32), (0, -1, 0), 0.07, 0.035, 'LI')
for s in (-1, 1):                                                                      # pedal hinge lugs
    f.box((0.3 + s * 0.04 - 0.004, -0.67, 0.31), (0.3 + s * 0.04 + 0.004, -0.635, 0.36), 'RD', 0.001)
pin(f, (0.3, -0.655, 0.335), (1, 0, 0), 0.006, 0.1)
f.fitting((0.25, -0.6, 0.33), (-1, 0, 0), 0.007)
f.hose([(0.23, -0.6, 0.33), (0.18, -0.585, 0.315), (0.1, -0.552, 0.312), (0.06, -0.522, 0.325), (0.075, -0.51, 0.33)], 0.007, 'HS', 12, 6)
part('Frame', f, (0, 0, 0), None, 'root: the chassis and mast, pushed on its castors')
for tag, x, y in (('FL', -0.45, 0.48), ('FR', 0.45, 0.48), ('RL', -0.45, -0.48), ('RR', 0.45, -0.48)):
    castor2(tag, x, y, 'Frame', R, 0.045, CT + 0.005)

# lifting carriage: beam, two uprights, crosshead on the ram, guide rollers in the mast, lead screw on brackets
c = D()
c.box((-0.34, -0.465, 0.26), (0.34, -0.36, 0.34), 'Y', 0.004)
for s in (-1, 1):
    c.box((s * 0.08, -0.465, 0.335), (s * 0.12, -0.425, 0.945), 'Y', 0.003)
    gusset(c, (s * 0.08, -0.445, 0.34), (-s, 0, 0), (0, 0, 1), 0.05, 0.008, 'Y')
    gusset(c, (s * 0.12, -0.445, 0.34), (s, 0, 0), (0, 0, 1), 0.05, 0.008, 'Y')
    for z in (0.4, 0.88):
        c.tube((s * 0.1155, -0.5, z), (s * 0.1155, -0.455, z), 0.02, 'ST', 16, weld=False)
        c.bolt((s * 0.1155, -0.455, z), (0, 1, 0), 0.006)
    c.box((s * 0.34 - 0.005, -0.4, 0.215), (s * 0.34 + 0.005, -0.36, 0.262), 'Y', 0.001)   # screw brackets
c.box((-0.125, -0.535, 0.94), (0.125, -0.42, 0.97), 'Y', 0.003)
c.tube((0, -0.51, 0.8), (0, -0.51, 0.941), 0.02, 'CR', 16, weld=False)                  # ram
c.tube((-0.365, -0.38, 0.235), (0.365, -0.38, 0.235), 0.012, 'ST', 12, weld=False)       # lead screw
for k in range(28):
    x = -0.33 + 0.0245 * k
    c.lz([(0.0115, -0.003), (0.0135, 0.0), (0.0115, 0.003)], 'ST', frame((x, -0.38, 0.235), (1, 0, 0)), 12, caps=False)
part('Carriage', c, (0, -0.445, 0.3), 'Frame', 'slides up z 0..0.25 m on the mast (hydraulic ram), lifts the wheel')
cr = D(); CP = (0.36, -0.38, 0.235)
cr.lz([(0.0, 0.0), (0.02, 0.0), (0.02, 0.025), (0.0, 0.025)], 'Y', frame(CP, (1, 0, 0)), 16)
cr.bar((0.375, -0.38, 0.23), (0.375, -0.38, 0.335), 0.025, 0.012, 'Y', 0.003, up=(1, 0, 0))
cr.tube((0.37, -0.38, 0.335), (0.425, -0.38, 0.335), 0.006, 'CR', 10, weld=False)
cr.tube((0.385, -0.38, 0.335), (0.43, -0.38, 0.335), 0.014, 'BK', 14, weld=False)
part('SpreadCrank', cr, CP, 'Carriage', 'turns about x (lead screw): both fork arms move in / out together, 4 mm per turn')
ik('SpreadCrank', (0.41, -0.38, 0.335), 'hand', 'SpreadCrank')
ctrl('Spread crank', 'SpreadCrank', 'SpreadCrank', 'set the arms to the tyre: rollers just under the tyre either side of its footprint')

XA = 0.25
for tag, s in (('L', -1), ('R', 1)):
    a = D(); x = s * XA
    a.box((x - 0.04, -0.469, 0.252), (x + 0.04, -0.353, 0.348), 'Y', 0.004)              # sleeve round the beam
    a.box((x - 0.02, -0.395, 0.215), (x + 0.02, -0.365, 0.256), 'Y', 0.002)              # nut lug on the screw
    a.box((x - 0.012, -0.356, 0.095), (x + 0.012, -0.29, 0.30), 'Y', 0.003)              # drop plate
    gusset(a, (x, -0.29, 0.15), (0, 1, 0), (0, 0, 1), 0.08, 0.008, 'Y')
    a.box((x - 0.03, -0.3, 0.09), (x + 0.03, 0.45, 0.15), 'Y', 0.004)                    # arm
    a.box((x - 0.032, 0.45 - 0.001, 0.088), (x + 0.032, 0.456, 0.152), 'Y', 0.002)
    for y in (-0.275, 0.415):                                                            # roller brackets
        a.box((s * 0.135 - 0.0, y - 0.005, 0.09), (s * 0.222, y + 0.005, 0.19), 'Y', 0.002)
        a.weld((s * 0.221, y - 0.006, 0.15), (s * 0.221, y + 0.006, 0.15), 'Y', 0.003)
    a.label((x + s * 0.0306, 0.1, 0.12), (s, 0, 0), 0.14, 0.03, 'LW')
    part('ForkArm_' + tag, a, (x, -0.4, 0.3), 'Carriage', 'slides along x on the beam (lead screw): %s0.15 .. %s0.35 m from the centre' % ('-' if s < 0 else '', '-' if s < 0 else ''))
    ro = D(); rc = (s * 0.182, 0.07, 0.14)
    ro.lathe([(0.0, -0.3335), (0.03, -0.3335), (0.035, -0.3285), (0.035, 0.3285), (0.03, 0.3335), (0.0, 0.3335)], 'RB', rc, 'y', 28)
    pin(ro, rc, (0, 1, 0), 0.008, 0.71)
    part('Roller_' + tag, ro, rc, 'ForkArm_' + tag, 'spins about y: the tyre turns on it (to line up the wheel with the axle)')
ik('Push_L', (-0.1, -0.72, 1.0), 'push', 'Frame'); ik('Push_R', (0.1, -0.72, 1.0), 'push', 'Frame')

pp = D(); PV = (0.3, -0.655, 0.335)
pp.lz([(0.0, -0.035), (0.0105, -0.035), (0.0105, 0.035), (0.0, 0.035)], 'RD', frame(PV, (1, 0, 0)), 16)
pp.bar((0.3, -0.655, 0.335), (0.3, -0.84, 0.30), 0.03, 0.014, 'RD', 0.003)
pp.box((0.25, -0.87, 0.29), (0.35, -0.8, 0.305), 'RD', 0.003)
for k in range(5): pp.box((0.255, -0.865 + 0.013 * k, 0.304), (0.345, -0.86 + 0.013 * k, 0.308), 'BK', 0.001)
part('PumpPedal', pp, PV, 'Frame', 'tips about x 0..25 deg: each stroke raises the carriage about 8 mm')
ik('PumpPedal', (0.3, -0.835, 0.31), 'foot', 'PumpPedal')
ctrl('Lift pedal', 'PumpPedal', 'PumpPedal', 'pump to lift the wheel off the floor', 'foot')
valve('ReleaseValve', (0.35, -0.6025, 0.33), (1, 0, 0), 'Frame', 'knob', 'BK', 'open: the carriage comes down slowly')
ik('Operator', (0, -1.15, 0), 'stand', 'Frame')
finish('WheelDolly', 'aircraft wheel and brake dolly', {'capacity_kg': 450, 'tyres': 'up to 46x17.0R20 (A320 main)', 'lift_m': 0.25,
       'arm_spread_m': [0.3, 0.7], 'castors_mm': 150},
       {'x_min': -0.52, 'x_max': 0.52, 'y_min': -0.9, 'y_max': 0.58, 'note': 'pushed in along the axle, arms either side of the tyre'},
       'No usable free photo found; published sizes of airliner wheel and brake dollies and A320 tyres')
