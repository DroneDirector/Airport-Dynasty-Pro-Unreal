# Scissor lift, 19 ft slab class (to-do 364 A), our own model, no brand. Looked at: "Hy-Brid Lifts PS-1930 scissor lift
# with worker working while elevated" and a slab scissor at Hyderabad airport (Wikimedia Commons). Published sizes for the
# class: 0.76-0.81 m wide, 1.83 m long, platform 0.74 x 1.64 m with a 0.9 m roll-out extension, platform 5.8 m high
# (7.8 m working), stowed about 2.1 m with the rails up, 305 mm solid non-marking wheels, 4 scissor stages.
# The lift: stage i's centre pin sits at Z0 + (i + 0.5) * h, h = L * sin(theta), L = 1.55 m pin to pin; outer arms (A)
# turn +theta about x, inner arms (B) -theta. Stowed theta 4.26 deg (h 0.115), full 57.7 deg (h 1.31): platform 0 .. +4.78 m.
# Push mode: the brake release lever frees the drive wheels; the rear ladder's grips are the push handles.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_lib import *

reset(); materials('SCL')
L, Z0, H0 = 1.55, 0.44, 0.115
TH0 = math.asin(H0 / L); ZTOP = Z0 + 4 * H0     # top pins 0.90
PU = ZTOP + 0.07; DK = PU + 0.08               # platform underside 0.97, deck top 1.05
WR, WW = 0.155, 0.10

c = B()
c.box((-0.25, -0.915, 0.10), (0.25, 0.915, 0.38), 'Y', 0.012)                      # central tray
for s in (-1, 1):
    c.box((s * 0.255 if s > 0 else -0.38, -0.47, 0.12), (0.38 if s > 0 else -0.255, 0.47, 0.38), 'Y', 0.01)   # battery / pump trays
    for y in (-1, 1): c.box((s * 0.25 if s > 0 else -0.38, y * 0.85 if y > 0 else -0.915, 0.12), (0.38 if s > 0 else -0.25, 0.915 if y > 0 else -0.85, 0.38), 'Y', 0.01)
    c.box((s * 0.365 - 0.006, -L / 2 * math.cos(TH0) - 0.04, 0.38), (s * 0.365 + 0.006, -L / 2 * math.cos(TH0) + 0.04, 0.47), 'ST', 0.002)   # A0 pin lugs
    c.box((s * 0.256 - 0.004, 0.55, 0.19), (s * 0.256 + 0.004, 0.77, 0.30), 'G', 0.001)                   # front knuckle guards
for y in (0.55, 0.65):
    c.box((-0.24, y, 0.38), (0.24, y + 0.03, 0.39), 'ST', 0.002)                                           # B0 slide track
c.box((-0.24, 0.4, 0.38), (0.24, 0.66, 0.383), 'ST', 0.001)
for sx in (-1, 1): c.box((sx * 0.03 - 0.005, -0.50, 0.38), (sx * 0.03 + 0.005, -0.40, 0.46), 'ST', 0.002)  # cylinder lugs
# rear entry ladder; its side tubes and top grips are the push handles
for sx in (-1, 1):
    c.tube((sx * 0.2, -0.955, 0.16), (sx * 0.2, -0.955, 0.88), 0.017, 'Y')
    c.tube((sx * 0.2, -0.955, 0.2), (sx * 0.2, -0.915, 0.2), 0.012, 'Y')
    c.tube((sx * 0.2, -0.955, 0.7), (sx * 0.2, -0.915, 0.7), 0.012, 'Y')
    c.tube((sx * 0.2, -0.955, 0.88), (sx * 0.2, -0.915, 0.88), 0.017, 'Y')
    c.tube((sx * 0.2, -0.97, 0.80), (sx * 0.2, -0.97, 0.875), 0.021, 'BK')
for z in (0.32, 0.58): c.box((-0.2, -0.98, z - 0.02), (0.2, -0.93, z), 'PL', 0.002)
c.box((-0.3, -0.917, 0.16), (0.3, -0.915, 0.3), 'BK', 0.0005)                         # rear rubber strip
c.box((-0.3, 0.915, 0.16), (0.3, 0.917, 0.3), 'BK', 0.0005)
part('Chassis', c, (0, 0, 0), None, 'root: rolls on its wheels (drive, or pushed with the brakes released)')

# wheels: front steer (knuckle about z), rear fixed; one wheel mesh
wh = B()
wh.lathe([(0.0, -WW / 2 + 0.006), (0.095, -WW / 2 + 0.006), (0.095, -WW / 2), (WR - 0.015, -WW / 2), (WR, -WW / 2 + 0.015), (WR, WW / 2 - 0.015),
          (WR - 0.015, WW / 2), (0.095, WW / 2), (0.095, WW / 2 - 0.006), (0.0, WW / 2 - 0.006)], 'RB', (0, 0, 0), 'x', 32)
wh.lathe([(0.0, -WW / 2 - 0.002), (0.092, -WW / 2 - 0.002), (0.092, WW / 2 + 0.002), (0.0, WW / 2 + 0.002)], 'LG', (0, 0, 0), 'x', 24)
WM = wh.mesh('SCL_Wheel', (0, 0, 0))
kn = B(); kn.box((0.258, -0.05, WR - 0.05), (0.268, 0.05, WR + 0.05), 'G', 0.002); kn.tube((0.268, 0, WR), (0.27, 0, WR), 0.03, 'ST')
KR = kn.mesh('SCL_Knuckle_R', (0.32, 0.66, 0))
kl = B(); kl.box((-0.268, -0.05, WR - 0.05), (-0.258, 0.05, WR + 0.05), 'G', 0.002); kl.tube((-0.27, 0, WR), (-0.268, 0, WR), 0.03, 'ST')
KL = kl.mesh('SCL_Knuckle_L', (-0.32, 0.66, 0))
for tag, sx, y in (('FL', -1, 0.66), ('FR', 1, 0.66), ('RL', -1, -0.66), ('RR', 1, -0.66)):
    if y > 0:
        part('Steer_' + tag, KL if sx < 0 else KR, (sx * 0.32, y, 0), 'Chassis', 'steers about z -35..+35 deg')
        part('Wheel_' + tag, WM, (sx * 0.322, y, WR), 'Steer_' + tag, 'spins about x, r %.3f' % WR)
    else:
        part('Wheel_' + tag, WM, (sx * 0.322, y, WR), 'Chassis', 'spins about x, r %.3f (drive, braked)' % WR)
# Steer knuckle geometry was made at x 0: move its mesh to the wheel side by the pivot
# rear wheel hub plates on the chassis side (fixed axle)
# pothole guards: drop about y at their outer hinge when the platform rises
pg = B(); pg.box((0.262, -0.40, 0.085), (0.375, 0.40, 0.105), 'G', 0.004)
PG = pg.mesh('SCL_PotholeGuard', (0.375, 0, 0.095))
part('PotholeGuard_R', PG, (0.375, 0, 0.095), 'Chassis', 'turns about y 0..-60 deg (down) above 0.5 m lift')
pgl = B(); pgl.box((-0.375, -0.40, 0.085), (-0.262, 0.40, 0.105), 'G', 0.004)
part('PotholeGuard_L', pgl, (-0.375, 0, 0.095), 'Chassis', 'turns about y 0..+60 deg (down) above 0.5 m lift')
# brake release lever (push mode), rear right
bl = B(); P = (0.31, -0.92, 0.34)
bl.tube(P, (0.31, -0.98, 0.34), 0.008, 'ST'); bl.box((0.29, -0.985, 0.33), (0.33, -0.975, 0.38), 'RD', 0.003)
part('BrakeRelease', bl, P, 'Chassis', 'turns about y 0..90 deg: up = brakes released (push mode)')

# scissor stages
oa = B()
for s in (-1, 1):
    oa.box((s * 0.33 if s > 0 else -0.36, -0.815, -0.05), (0.36 if s > 0 else -0.33, 0.815, 0.05), 'LG', 0.012)
for y in (-L / 2, 0.0, L / 2): oa.tube((-0.37, y, 0), (0.37, y, 0), 0.018 if y else 0.02, 'ST', 14)
OA = oa.mesh('SCL_Arms_Outer', (0, 0, 0))
ia = B()
for s in (-1, 1):
    ia.box((s * 0.275 if s > 0 else -0.305, -0.815, -0.05), (0.305 if s > 0 else -0.275, 0.815, 0.05), 'LG', 0.012)
for y in (-0.6, 0.6): ia.tube((-0.28, y, 0), (0.28, y, 0), 0.025, 'LG', 14)
IA = ia.mesh('SCL_Arms_Inner', (0, 0, 0))
for i in range(4):
    zc = Z0 + (i + 0.5) * H0
    part('Scissor_%d_A' % (i + 1), OA, (0, 0, zc), 'Chassis', 'stage %d outer arms: centre pin z = %.2f + %.1f*h, turn about x +theta (4.26..57.7 deg)' % (i + 1, Z0, i + 0.5), rot_x=TH0)
    part('Scissor_%d_B' % (i + 1), IA, (0, 0, zc), 'Chassis', 'stage %d inner arms: centre pin as A, turn about x -theta' % (i + 1), rot_x=-TH0)

# lift cylinder and its ram (ram slides along the cylinder's axis)
cy = B(); P = (0, -0.45, 0.43)
cy.tube((-0.033, -0.45, 0.43), (0.033, -0.45, 0.43), 0.02, 'ST')
cy.tube((0, -0.45, 0.43), (0, 0.25, 0.43), 0.045, 'G', 20)
part('LiftCylinder', cy, P, 'Chassis', 'tilts about x with the stages')
rm = B(); rm.tube((0, 0.10, 0.43), (0, 0.45, 0.43), 0.028, 'CR', 16); rm.lathe([(0.0, -0.02), (0.035, -0.02), (0.035, 0.02), (0.0, 0.02)], 'G', (0, 0.47, 0.43), 'x', 16)
part('LiftRam', rm, (0, 0.25, 0.43), 'LiftCylinder', 'slides out along the cylinder 0..0.9 m')

# platform: lifts with the stages, carries rails, gate, extension deck, control box
pf = B()
for s in (-1, 1): pf.box((s * 0.34 if s > 0 else -0.385, -0.82, PU), (0.385 if s > 0 else -0.34, 0.82, PU + 0.075), 'Y', 0.008)
for y in (-0.82, -0.3, 0.3, 0.76): pf.box((-0.34, y, PU + 0.015), (0.34, y + 0.06, PU + 0.075), 'Y', 0.006)
pf.box((-0.385, -0.82, PU + 0.075), (0.385, 0.82, DK), 'PL', 0.003)
for s in (-1, 1):
    pf.box((s * 0.312 - 0.005, -L / 2 * math.cos(TH0) - 0.04, ZTOP - 0.03), (s * 0.312 + 0.005, -L / 2 * math.cos(TH0) + 0.04, PU), 'ST', 0.002)
    pf.box((s * 0.37 - 0.006, 0.55, PU - 0.06), (s * 0.37 + 0.006, 0.82, PU), 'ST', 0.002)                # A4 slide tracks
pf.tube((-0.318, -L / 2 * math.cos(TH0), ZTOP), (0.318, -L / 2 * math.cos(TH0), ZTOP), 0.018, 'ST')
RT, RM = DK + 1.1, DK + 0.55
for s in (-1, 1):
    x = s * 0.395
    for y in (-0.80, -0.4, 0.0): pf.tube((x, y, PU + 0.02), (x, y, RT), 0.021, 'Y')
    pf.tube((x, -0.80, RT), (x, 0.0, RT), 0.021, 'Y'); pf.tube((x, -0.80, RM), (x, 0.0, RM), 0.018, 'Y')
    pf.tube((s * 0.395, -0.80, RT), (s * 0.12, -0.80, RT), 0.021, 'Y'); pf.tube((s * 0.395, -0.80, RM), (s * 0.12, -0.80, RM), 0.018, 'Y')
    pf.tube((s * 0.12, -0.80, DK), (s * 0.12, -0.80, RT), 0.021, 'Y')
    pf.box((x - 0.003, -0.82, DK), (x + 0.003, 0.0, DK + 0.12), 'WS', 0.001)
part('Platform', pf, (0, 0, ZTOP), 'Chassis', 'lifts z 0..+4.78 m (deck 1.05 -> 5.83 m)')

gt = B(); P = (-0.09, -0.80, DK + 0.1)
gt.tube((-0.09, -0.80, DK + 0.1), (-0.09, -0.80, RT - 0.02), 0.015, 'Y')
gt.tube((-0.09, -0.80, RT - 0.05), (0.09, -0.80, RT - 0.05), 0.017, 'Y'); gt.tube((-0.09, -0.80, RM), (0.09, -0.80, RM), 0.015, 'Y')
gt.tube((0.09, -0.80, RM), (0.09, -0.80, RT - 0.05), 0.015, 'Y')
part('Gate', gt, P, 'Platform', 'self-closing, swings about z 0..100 deg inward')

ex = B(); P = (0, -0.05, DK + 0.005)
ex.box((-0.34, -0.05, DK + 0.005), (0.34, 0.84, DK + 0.017), 'PL', 0.002)
for s in (-1, 1):
    x = s * 0.33
    for y in (-0.03, 0.40, 0.82): ex.tube((x, y, DK + 0.017), (x, y, RT), 0.021, 'Y')
    ex.tube((x, -0.03, RT), (x, 0.82, RT), 0.021, 'Y'); ex.tube((x, -0.03, RM), (x, 0.82, RM), 0.018, 'Y')
ex.tube((-0.33, 0.82, RT), (0.33, 0.82, RT), 0.021, 'Y'); ex.tube((-0.33, 0.82, RM), (0.33, 0.82, RM), 0.018, 'Y')
ex.box((-0.34, 0.835, DK + 0.017), (0.34, 0.84, DK + 0.13), 'Y', 0.001)
part('Platform_Extension', ex, P, 'Platform', 'slides along +y 0..0.90 m')

cb = B(); P = (-0.26, -0.55, RT)
cb.box((-0.36, -0.66, RT - 0.26), (-0.25, -0.44, RT - 0.03), 'G', 0.008)
cb.box((-0.372, -0.62, RT - 0.04), (-0.356, -0.48, RT + 0.034), 'ST', 0.002); cb.box((-0.43, -0.62, RT + 0.024), (-0.356, -0.48, RT + 0.034), 'ST', 0.002)                 # hook over the top rail
part('ControlBox', cb, P, 'Platform', 'hangs on the rail; lifts off')
js = B(); P2 = (-0.305, -0.50, RT - 0.028)
js.lathe([(0.025, 0.0), (0.02, 0.02), (0.008, 0.03), (0.008, 0.11), (0.018, 0.13), (0.018, 0.2), (0.0, 0.21)], 'BK', P2, 'z', 16)
part('Joystick', js, P2, 'ControlBox', 'tilts about x and y +-20 deg (drive, steer, lift)')
es = B(); P3 = (-0.305, -0.62, RT - 0.028)
es.lathe([(0.012, 0.0), (0.012, 0.012), (0.03, 0.012), (0.03, 0.03), (0.0, 0.034)], 'RD', P3, 'z', 20)
part('EStop', es, P3, 'ControlBox', 'pushes down z 0..-0.01 m (stop), turns to release')
sw = B(); P4 = (-0.305, -0.565, RT - 0.028)
sw.lathe([(0.012, 0.0), (0.012, 0.01), (0.0, 0.01)], 'BK', P4, 'z', 12); sw.box((-0.309, -0.585, RT - 0.02), (-0.301, -0.545, RT + 0.005), 'BK', 0.002)
part('LiftDriveSwitch', sw, P4, 'ControlBox', 'turns about z -45 / +45 deg: lift or drive')
la = B(); P5 = (0.108, -0.80, RM + 0.1)
la.box((0.108, -0.812, RM + 0.08), (0.15, -0.788, RM + 0.12), 'BK', 0.004)
part('GateLatch', la, P5, 'Gate', 'turns about y 0..40 deg: up = unlatched')

ik('Deck', (0, -0.3, DK), 'stand', 'Platform')
ik('Rail_L', (-0.395, -0.2, RT), 'hand', 'Platform'); ik('Rail_R', (0.395, -0.2, RT), 'hand', 'Platform')
ik('Joystick', (-0.305, -0.50, RT + 0.15), 'hand', 'Joystick'); ik('EStop', (-0.305, -0.62, RT + 0.004), 'hand', 'EStop')
ik('LiftDriveSwitch', (-0.305, -0.565, RT + 0.005), 'hand', 'LiftDriveSwitch'); ik('GateLatch', (0.12, -0.80, RM + 0.12), 'hand', 'GateLatch')
ik('Gate', (0.0, -0.80, RT - 0.05), 'hand', 'Gate'); ik('Extension_Rail', (0.0, 0.82, RT), 'hand', 'Platform_Extension')
ik('Step_1', (0, -0.955, 0.32), 'foot', 'Chassis'); ik('Step_2', (0, -0.955, 0.58), 'foot', 'Chassis')
ik('Push_L', (-0.2, -0.97, 0.84), 'push', 'Chassis'); ik('Push_R', (0.2, -0.97, 0.84), 'push', 'Chassis')
ik('BrakeRelease', (0.31, -0.98, 0.37), 'hand', 'BrakeRelease')
ctrl('Platform up / down, drive, steer', 'Joystick', 'Joystick', 'hold the trigger, push forward = up / drive forward; left-right = steer')
ctrl('Lift or drive selector', 'LiftDriveSwitch', 'LiftDriveSwitch', 'chooses what the joystick does')
ctrl('Emergency stop', 'EStop', 'EStop', 'press = everything stops; turn to release')
ctrl('Gate latch', 'GateLatch', 'GateLatch', 'lift to open the gate')
ctrl('Gate', 'Gate', 'Gate', 'swing in to step on, self-closing')
ctrl('Extension deck', 'Platform_Extension', 'Extension_Rail', 'pull out / push in 0.9 m')
ctrl('Brake release (push mode)', 'BrakeRelease', 'BrakeRelease', 'up = drive wheels free to push')
finish('ScissorLift', 'scissor lift (19 ft slab class)',
       {'width_m': 0.76, 'length_m': 1.83, 'deck_stowed_m': round(DK, 2), 'deck_max_m': round(DK + 4.78, 2), 'rail_m': 1.1,
        'extension_m': 0.9, 'wheel_mm': 310, 'stages': 4, 'pin_to_pin_m': L, 'Z0': Z0, 'h_stowed': H0, 'h_full': 1.31},
       {'x_min': -0.40, 'x_max': 0.40, 'y_min': -0.99, 'y_max': 0.92, 'extension_out_y_max': 1.74},
       'Wikimedia Commons: Hy-Brid Lifts PS-1930 scissor lift with worker working while elevated; JCB S1930E at Hyderabad airport; published 19 ft class data')
