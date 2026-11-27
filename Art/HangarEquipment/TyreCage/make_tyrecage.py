# Tyre inflation safety cage (to-do 364 A, batch 2), our own model, no brands. No usable free photo found: built from
# published sizes of aircraft tyre inflation cages: a welded steel cage that holds a mounted wheel up to the A320 main
# wheel (46x17.0R20, 1.17 m across, 0.43 m wide) standing upright; square-tube corner posts and rails, 32 mm round bars
# at about 120 mm, a tread-plate floor with a ramp to roll the wheel in, a hinged front safety gate with a latch, feet
# bolted to the floor. The tyre is filled from OUTSIDE the cage: a panel with a regulator, line and tyre gauges, an
# inflate and a deflate valve, and a long hose to a clip-on chuck that hangs on a hook when not in use.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_joints import *

reset(); detail_materials('TCG')
XI, YI, XO, YO = 0.72, 0.38, 0.78, 0.44          # inner / outer faces of the posts and rails
ZB, ZT = 0.072, 1.5                               # floor top, underside of the top rails
f = D()
# feet with anchor bolts, bottom rails, tread-plate floor, ramp
for sx in (-1, 1):
    for sy in (-1, 1):
        f.box((sx * 0.75 - 0.07, sy * 0.41 - 0.07, 0.0), (sx * 0.75 + 0.07, sy * 0.41 + 0.07, 0.008), 'Y', 0.002)
        for b in ((sx * 0.75 + sx * 0.05, sy * 0.41 - sy * 0.04), (sx * 0.75 - sx * 0.04, sy * 0.41 + sy * 0.05)):
            f.bolt((b[0], b[1], 0.008), (0, 0, 1), 0.009)
for s in (-1, 1):
    f.box((s * XI, -YO, 0.007), (s * XO, YO, 0.067), 'Y', 0.004)                       # side rails
f.box((-XI - 0.005, -YO, 0.007), (XI + 0.005, -YI, 0.067), 'Y', 0.004)                  # back rail
f.box((-XI - 0.005, YI, 0.007), (XI + 0.005, YO, 0.067), 'Y', 0.004)                    # front sill under the floor
f.box((-0.005, -YI + 0.002, 0.007), (0.005, YI - 0.002, 0.066), 'Y', 0.002)            # floor stiffener
f.box((-XO, -YO, 0.066), (XO, YO, ZB), 'PL', 0.002)
f.bar((0, YO - 0.004, ZB - 0.003), (0, YO + 0.17, 0.004), 1.4, 0.006, 'PL', 0.002)     # ramp, welded to the floor edge
f.weld((-0.69, YO + 0.002, ZB + 0.001), (0.69, YO + 0.002, ZB + 0.001), 'ST', 0.003)
# corner posts, top rails, middle rails
for sx in (-1, 1):
    for sy in (-1, 1):
        f.box((sx * XI, sy * YI, 0.02), (sx * XO, sy * YO, ZT + 0.06), 'Y', 0.004)
        f.box((sx * XI - 0.002, sy * YI - 0.002, ZT + 0.06), (sx * XO + 0.002, sy * YO + 0.002, ZT + 0.066), 'Y', 0.001)   # cap plate
for s in (-1, 1):
    f.box((s * XI, -YI, ZT), (s * XO, YI, ZT + 0.06), 'Y', 0.004)
    f.box((-XI, s * YI, ZT), (XI, s * YO, ZT + 0.06), 'Y', 0.004)
    f.box((s * (XI + 0.01), -YI, 0.76), (s * (XO - 0.01), YI, 0.8), 'Y', 0.003)
f.box((-XI, -YO + 0.01, 0.76), (XI, -YI - 0.01, 0.8), 'Y', 0.003)
for p0, p1 in (((-XI, -YI, 0.78), (XI, -YI, 0.78)), ((-XI, YI, ZT), (XI, YI, ZT))): f.weld(p0, p1, 'Y', 0.002)
# bars: back wall, side walls, roof; ends 5.5 mm into the rails so the weld bead sits on the joint
for k in range(11):
    x = -0.6 + 0.12 * k
    f.tube((x, -0.41, ZB - 0.0055), (x, -0.41, ZT + 0.0055), 0.016, 'Y', 14)
for s in (-1, 1):
    for k in range(6):
        y = -0.29 + 0.116 * k
        f.tube((s * 0.75, y, ZB - 0.0055), (s * 0.75, y, ZT + 0.0055), 0.016, 'Y', 14)
for k in range(9):
    x = -0.6 + 0.15 * k
    f.tube((x, -YI - 0.0055, ZT + 0.03), (x, YI + 0.0055, ZT + 0.03), 0.016, 'Y', 14)
# gussets: front opening corners, back corners in the side walls
for s in (-1, 1):
    gusset(f, (s * XI, 0.41, ZT), (0, 0, -1), (-s, 0, 0), 0.13, 0.008, 'Y')
    gusset(f, (s * XI, 0.41, ZB), (0, 0, 1), (-s, 0, 0), 0.09, 0.008, 'Y')
    gusset(f, (s * 0.75, -YI, ZT), (0, 0, -1), (0, 1, 0), 0.07, 0.008, 'Y')
    gusset(f, (s * 0.75, -YI, ZB), (0, 0, 1), (0, 1, 0), 0.07, 0.008, 'Y')
f.label((0.0, -YO - 0.0006, ZT + 0.03), (0, -1, 0), 0.24, 0.045, 'LW')
f.label((-XO - 0.0006, 0.0, ZT + 0.03), (-1, 0, 0), 0.24, 0.045, 'LW')
# gate hinges on the left front post: lugs welded to the post, a vertical clevis pin through the gate's knuckles
HX, HY = -0.75, 0.472
for zk in (0.4, 1.2):
    for dz in (-0.035, 0.035):
        f.box((HX - 0.025, YO - 0.003, zk + dz - 0.004), (HX + 0.025, HY + 0.025, zk + dz + 0.004), 'Y', 0.002)
        f.weld((HX - 0.025, YO + 0.002, zk + dz + 0.006 * (1 if dz > 0 else -1)), (HX + 0.025, YO + 0.002, zk + dz + 0.006 * (1 if dz > 0 else -1)), 'Y', 0.003)
    pin(f, (HX, HY, zk), (0, 0, 1), 0.009, 0.1)
# latch keeper on the right front post
f.box((0.74, YO - 0.003, 0.97), (0.77, HY + 0.03, 0.978), 'Y', 0.002)
f.box((0.74, YO - 0.003, 1.032), (0.77, HY + 0.03, 1.04), 'Y', 0.002)
f.tube((0.755, HY + 0.022, 0.96), (0.755, HY + 0.022, 1.05), 0.006, 'CR', 10, weld=False)
# inflation panel on stand-off brackets at the right front post (outside the cage), hook for the chuck, supply inlet
PX = 0.806
for z in (0.98, 1.29):
    f.bar((XO - 0.005, 0.41, z), (PX - 0.005, 0.41, z), 0.04, 0.04, 'Y', 0.003)
    f.weld((XO + 0.002, 0.388, z + 0.022), (XO + 0.002, 0.432, z + 0.022), 'Y', 0.003)
f.sheet((PX - 0.006, 0.22, 0.94), (PX, 0.5, 1.32), 'G', 0.004)
for y in (0.235, 0.485):
    for z in (0.955, 1.305): f.bolt((PX, y, z), (1, 0, 0), 0.006)
f.label((PX + 0.0005, 0.36, 1.3), (1, 0, 0), 0.12, 0.022, 'LI')
f.lz([(0.012, -0.004), (0.012, 0.03), (0.009, 0.035), (0.009, 0.06), (0.0, 0.06)], 'BR', frame((0.803, 0.46, 0.942), (0, 0, -1)), 6)   # supply inlet
f.fitting((0.803, 0.3, 0.94 - 0.03), (0, 0, -1), 0.007)                                 # hose outlet under the panel
f.lz([(0.009, -0.002), (0.009, 0.03), (0.0, 0.03)], 'BR', frame((0.803, 0.3, 0.942), (0, 0, -1)), 6)
f.bar((XO - 0.005, 0.41, 0.72), (0.9, 0.41, 0.72), 0.02, 0.02, 'Y', 0.003)            # chuck hook
f.tube((0.895, 0.41, 0.711), (0.895, 0.41, 0.77), 0.01, 'Y', 10, weld=False)
cap(f, (0.895, 0.41, 0.77), (0, 0, 1), 0.01, 'BK')
f.weld((XO + 0.002, 0.395, 0.733), (XO + 0.002, 0.425, 0.733), 'Y', 0.003)
CH = (0.86, 0.41, 0.72)                                                                  # the chuck's ring sits on the hook
f.hose([(0.803, 0.3, 0.88), (0.815, 0.28, 0.6), (0.86, 0.32, 0.35), (0.92, 0.4, 0.42), (0.875, 0.41, 0.51), (CH[0], CH[1], CH[2] - 0.165)],
       0.0065, 'HS', 12, 6)
part('Frame', f, (0, 0, 0), None, 'root: the cage, bolted to the hangar floor')

# safety gate: square-tube frame with bars, two hinge knuckles round the pins, latch handle on the right
g = D(); GY0, GY1 = HY - 0.018, HY + 0.018
g.box((-0.735, GY0, 0.12), (-0.695, GY1, 1.45), 'Y', 0.003)
g.box((0.695, GY0, 0.12), (0.735, GY1, 1.45), 'Y', 0.003)
g.box((-0.7, GY0, 0.12), (0.7, GY1, 0.16), 'Y', 0.003)
g.box((-0.7, GY0, 1.41), (0.7, GY1, 1.45), 'Y', 0.003)
for k in range(10):
    x = -0.585 + 0.13 * k
    g.tube((x, HY, 0.16 - 0.0055), (x, HY, 1.41 + 0.0055), 0.016, 'Y', 14)
for zk in (0.4, 1.2):
    g.lz([(0.0095, -0.0295), (0.019, -0.0295), (0.019, 0.0295), (0.0095, 0.0295)], 'Y', frame((HX, HY, zk), (0, 0, 1)), 18)
    g.weld((HX + 0.015, GY0 - 0.001, zk - 0.03), (HX + 0.015, GY0 - 0.001, zk + 0.03), 'Y', 0.003)
for zz in (0.16, 1.41):
    for s in (-1, 1): gusset(g, (s * 0.695, HY, zz), (-s, 0, 0), (0, 0, 1 if zz < 1 else -1), 0.07, 0.008, 'Y', weld=False)
g.label((0.0, GY1 + 0.0006, 1.43), (0, 1, 0), 0.3, 0.03, 'LW')
g.box((0.735, HY - 0.008, 0.995), (0.79, HY + 0.008, 1.007), 'ST', 0.002)              # latch tongue into the keeper
g.tube((0.715, GY1 - 0.004, 1.015), (0.715, HY + 0.12, 1.015), 0.0085, 'ST', 10, weld=False)
g.tube((0.715, HY + 0.1, 1.015), (0.715, HY + 0.2, 1.015), 0.013, 'BK', 14, weld=False)
cap(g, (0.715, HY + 0.2, 1.015), (0, 1, 0), 0.013, 'BK')
part('Gate', g, (HX, HY, 0.4), 'Frame', 'swings about z on the left hinge pins: 0 = shut and latched .. 100 deg open (wheel in / out)')
ik('Gate_Handle', (0.715, HY + 0.16, 1.015), 'hand', 'Gate')
ctrl('Safety gate', 'Gate', 'Gate_Handle', 'lift the latch and swing open to roll the wheel in; shut and latch before filling')

# clip-on chuck: ring on the hook, brass body, clip lever, hose ferrule at the bottom
c = D(); I = Matrix.Identity(4)
c.lz([(0.0155 + 0.003 * math.cos(2 * math.pi * j / 10), 0.003 * math.sin(2 * math.pi * j / 10)) for j in range(11)], 'ST',
     frame((0, 0, 0), (1, 0, 0)), 20, caps=False)
c.tube((0, 0, -0.0175), (0, 0, -0.035), 0.0025, 'ST', 8, weld=False)
c.lz([(0.0, -0.035), (0.009, -0.035), (0.009, -0.12), (0.006, -0.13), (0.0, -0.13)], 'BR', I, 14)
c.fbox(I, (-0.004, 0.008, -0.11), (0.004, 0.016, -0.04), 'CR', 0.002)                  # clip lever
c.fitting((0, 0, -0.165), (0, 0, -1), 0.0065)
c.tube((0, 0, -0.13), (0, 0, -0.165), 0.006, 'CR', 12, weld=False)
part('Chuck', c, CH, 'Frame', 'off the hook by hand, clipped onto the tyre valve (hose follows); back on the hook after')
ik('Chuck', (CH[0], CH[1], CH[2] - 0.08), 'hand', 'Chuck')
ctrl('Tyre chuck', 'Chuck', 'Chuck', 'clip onto the wheel valve through the bars, then step back to the panel')

N = (1, 0, 0)
gauge('Line', (PX, 0.27, 1.2), N, 0.04, 'Frame', 'air line pressure (0..25 bar)')
gauge('Tyre', (PX, 0.43, 1.2), N, 0.04, 'Frame', 'tyre pressure read through the hose (0..25 bar)')
valve('Regulator', (PX, 0.35, 1.2), N, 'Frame', 'knob', 'BK', 'sets the fill pressure, never above the tyre limit')
valve('InflateValve', (PX, 0.29, 1.04), N, 'Frame', 'lever', 'KG', 'open = air flows to the tyre')
valve('DeflateValve', (PX, 0.43, 1.04), N, 'Frame', 'T', 'KR', 'bleeds the tyre down')
ik('Operator', (1.35, 0.35, 0), 'stand', 'Frame')
finish('TyreCage', 'tyre inflation safety cage', {'inner_m': [1.44, 0.76, 1.43], 'tyre_max': 'A320 main 46x17.0R20, 1.17 m',
       'air': 'up to 25 bar via regulator, remote clip-on chuck', 'bars_mm': 32},
       {'x_min': -0.82, 'x_max': 1.0, 'y_min': -0.47, 'y_max': 1.25, 'note': 'bolted to the floor; the gate swings out over +y'},
       'No usable free photo found; published sizes of aircraft tyre inflation cages and A320 tyres')
