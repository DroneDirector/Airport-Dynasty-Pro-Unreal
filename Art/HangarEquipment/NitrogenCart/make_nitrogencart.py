# Nitrogen servicing cart for A320 tyres, struts and accumulators (to-do 364 A, batch 2), our own model, no brands.
# Looked at: "US Navy 070430-N-4420S-029 ... operates a nitrogen cart" (Wikimedia Commons, public domain): a low steel
# trailer on 4 pneumatic wheels with a steerable tow bar, high-pressure bottles lying in the frame under straps, a raised
# panel with regulator, gauges and valves, steel pigtails, a hose and a charging chuck. Sizes: 3 bottles 230 mm x 0.9 m
# (about 35 l, 200 bar), wheels 4.00-8 (410 mm), frame 0.68 m wide, 2.0 m long without the tow bar, panel at 1.0-1.2 m.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_detail import *

reset(); detail_materials('N2C')
R = 0.205; ZF = 0.40; T = 0.06                 # wheel radius, frame underside, rail depth
XR = 0.34; YR0, YR1 = -1.06, 0.20              # rail half width, rail ends
YF, YB = 0.72, -0.72                           # front turntable, rear axle
f = D()
for s in (-1, 1):
    f.sheet((s * XR - 0.04, YR0, ZF), (s * XR + 0.04, YR1, ZF + T), 'Y', 0.005)                     # side rails 80x60
    for y in (YR0 + 0.02, -0.30, YR1 - 0.04):
        f.weld((s * (XR - 0.042), y - 0.02, ZF + T), (s * (XR - 0.042), y + 0.02, ZF + T), 'Y')
for y in (YR0 + 0.02, -0.30, YR1 - 0.04): f.sheet((-XR + 0.04, y - 0.02, ZF + 0.005), (XR - 0.04, y + 0.02, ZF + T - 0.005), 'Y', 0.004)
# narrow nose to the turntable (front wheels swing clear of it +-35 deg)
for s in (-1, 1):
    f.bar((s * (XR - 0.02), YR1 - 0.02, ZF + 0.03), (s * 0.15, YF - 0.05, ZF + 0.03), 0.06, 0.05, 'Y', 0.004, up=(0, 0, 1))
    f.weld((s * (XR - 0.04), YR1 - 0.04, ZF + T + 0.002), (s * (XR - 0.08), YR1 + 0.01, ZF + T + 0.002), 'Y')
f.sheet((-0.2, YF - 0.2, ZF + 0.004), (0.2, YF + 0.15, ZF + 0.012), 'Y', 0.003)                        # turntable plate
for k in range(6):
    a = 2 * math.pi * k / 6; f.bolt((0.12 * math.cos(a), YF + 0.12 * math.sin(a), ZF + 0.012), (0, 0, 1), 0.008)
f.label((0, YF - 0.11, ZF + 0.0125), (0, 0, 1), 0.12, 0.06, 'LW')
# bottle saddles (rubber lined) and the bottles' strap posts
XB = (-0.235, 0.0, 0.235); RB = 0.115; ZB = ZF + T + 0.03 + RB
for y in (-0.62, -0.05):
    f.sheet((-XR - 0.02, y - 0.03, ZF + T - 0.002), (XR + 0.02, y + 0.03, ZF + T + 0.03), 'Y', 0.004)
    for x in XB: f.box((x - 0.07, y - 0.035, ZF + T + 0.03), (x + 0.07, y + 0.035, ZB - RB * 0.75), 'RB', 0.004)
# rear cabinet (regulator panel facing -y, the operator stands behind the cart)
CY0, CY1, CZ = YR0 - 0.005, YR0 + 0.23, 1.26
f.sheet((-XR - 0.02, CY0, ZF + T - 0.002), (XR + 0.02, CY1, CZ), 'LG', 0.014)
f.sheet((-XR - 0.03, CY0 - 0.008, CZ - 0.42), (XR + 0.03, CY0 + 0.01, CZ + 0.008), 'G', 0.006)        # panel face, darker
for x in (-XR - 0.01, XR + 0.01):
    for z in (CZ - 0.405, CZ - 0.01): f.bolt((x, CY0 - 0.008, z), (0, -1, 0), 0.007)
for z in (ZF + 0.25, ZF + 0.55):                                                                       # louvres both sides
    for s in (-1, 1):
        for k in range(5):
            f.box((s * (XR + 0.02) - 0.006, CY0 + 0.05, z + 0.03 * k), (s * (XR + 0.02) + 0.004, CY1 - 0.05, z + 0.03 * k + 0.012), 'LG', 0.003)
f.label((0, CY0 - 0.009, CZ - 0.05), (0, -1, 0), 0.16, 0.08, 'LN')
f.label((-0.2, CY0 - 0.009, CZ - 0.37), (0, -1, 0), 0.1, 0.05, 'LW')
f.label((0.2, CY0 - 0.009, CZ - 0.37), (0, -1, 0), 0.1, 0.05, 'LI')
# manifold on the cabinet's front and the pigtails from each bottle valve
f.tube((-0.3, CY1 + 0.03, CZ - 0.12), (0.3, CY1 + 0.03, CZ - 0.12), 0.014, 'BR', 6, weld=False)
for x in (-0.3, 0.3): f.box((x - 0.012, CY1, CZ - 0.135), (x + 0.012, CY1 + 0.04, CZ - 0.105), 'ST', 0.002)
BY0, BY1 = -0.84, 0.08                                     # bottle base and neck
for x in XB:
    f.fitting((x + 0.002, CY1 + 0.046, CZ - 0.12), (0, 1, 0), 0.006)
    f.hose([(x, CY1 + 0.07, CZ - 0.12), (x + 0.04, CY1 + 0.2, ZB + RB + 0.05), (x + 0.07, -0.3, ZB + RB + 0.03),
            (x + 0.07, BY1 - 0.08, ZB + RB + 0.03), (x + 0.085, BY1 + 0.035, ZB + 0.05), (x + 0.052, BY1 + 0.035, ZB)], 0.005, 'CR', 10, 5)
# strap posts and straps with turnbuckle bolts
for y in (-0.62, -0.05):
    for s in (-1, 1): f.box((s * (XR + 0.005) - 0.015, y - 0.02, ZF + T + 0.03), (s * (XR + 0.005) + 0.015, y + 0.02, ZB + RB + 0.02), 'Y', 0.004)
    f.box((-XR - 0.02, y - 0.022, ZB + RB + 0.004), (XR + 0.02, y + 0.022, ZB + RB + 0.012), 'ST', 0.002)
    for s in (-1, 1): f.bolt((s * (XR + 0.005), y, ZB + RB + 0.012), (0, 0, 1), 0.009)
# push handles at the back corners and the rear fenders
for s in (-1, 1):
    f.bent_tube([(s * (XR + 0.04), CY0 + 0.06, ZF + 0.5), (s * (XR + 0.04), CY0 - 0.06, ZF + 0.5), (s * (XR + 0.04), CY0 - 0.06, ZF + 0.72),
                 (s * (XR + 0.04), CY0 + 0.06, ZF + 0.72)], 0.017, 0.035, 'Y')                 # ONE tube bent twice
    f.tube((s * (XR + 0.04), CY0 - 0.06, ZF + 0.55), (s * (XR + 0.04), CY0 - 0.06, ZF + 0.67), 0.021, 'BK', 14, weld=False)
    f.band((s * 0.46, YB, R), R + 0.057, R + 0.063, 0.14, 20, 160, 'Y', 20)   # fender: one pressed strip over the rear wheel
    f.bar((s * 0.42, YB - 0.1, ZF + 0.025), (s * 0.42, YB - 0.1, R + 0.262), 0.03, 0.006, 'Y', 0.002, up=(0, 1, 0))
    f.bar((s * XR, YB - 0.1, ZF + 0.03), (s * 0.43, YB - 0.1, ZF + 0.03), 0.04, 0.006, 'Y', 0.002)
cart_frame(f, 0.46, YF, YB, ZF, R)
part('Frame', f, (0, 0, 0), None, 'root: the cart, rolls on its wheels')
cart_gear('Frame', 0.46, YF, YB, ZF, R)

# the bottles (one mesh, drawn three times): grey body, black shoulder (nitrogen), neck ring, each with its own valve
def bottle(g):
    L = BY1 - BY0
    g.lathe([(0.0, 0.0), (0.07, 0.0), (0.1, 0.012), (RB, 0.05), (RB, L - 0.16)], 'G', (0, 0, 0), 'y', 32)
    g.lathe([(RB, L - 0.16), (RB, L - 0.12), (0.095, L - 0.07), (0.05, L - 0.035), (0.03, L - 0.03), (0.03, L), (0.0, L)], 'BK', (0, 0, 0), 'y', 32)
    g.lathe([(0.034, L - 0.032), (0.04, L - 0.03), (0.04, L - 0.012), (0.034, L - 0.01)], 'ST', (0, 0, 0), 'y', 16)
    g.lathe([(0.0, L), (0.022, L), (0.022, L + 0.07), (0.0, L + 0.07)], 'BR', (0, 0, 0), 'y', 8)                    # valve body
    g.tube((0.0, L + 0.035, 0), (0.045, L + 0.035, 0), 0.008, 'BR', 8, weld=False)                                   # outlet
    g.label((0, L - 0.3, RB + 0.0002), (0, 0, 1), 0.12, 0.07, 'LN')
bm_ = cached('N2Bottle', bottle)
for i, x in enumerate(XB):
    part('Bottle_%d' % (i + 1), bm_, (x, BY0, ZB), 'Frame', 'fixed: strapped in the saddles')
    valve('BottleValve_%d' % (i + 1), (x, BY1 + 0.07, ZB), (0, 1, 0), 'Frame', 'wheel', 'BK', 'open / close the bottle (turn left to open)', 0.028)

# panel controls (facing -y, the operator behind the cart)
PY = CY0 - 0.008; N = (0, -1, 0)
gauge('Supply', (-0.17, PY, CZ - 0.14), N, 0.055, 'Frame', 'bottle pressure 0..300 bar')
gauge('Outlet', (0.17, PY, CZ - 0.14), N, 0.055, 'Frame', 'charging pressure 0..250 bar')
valve('Regulator', (0.0, PY, CZ - 0.16), N, 'Frame', 'wheel', 'BK', 'set the charging pressure (turn right = more)', 0.045)
valve('Valve_Supply', (-0.2, PY, CZ - 0.31), N, 'Frame', 'T', 'KB', 'bottles to the regulator')
valve('Valve_Outlet', (0.2, PY, CZ - 0.31), N, 'Frame', 'T', 'KG', 'regulator to the hose')
valve('Valve_Vent', (0.0, PY, CZ - 0.3), N, 'Frame', 'lever', 'KR', 'bleeds the hose before the chuck comes off', 0.025)

# lid over the cabinet, hinged at its front edge
ld = D(); P = (0, CY1 - 0.01, CZ + 0.012)
ld.sheet((-XR - 0.03, CY0 - 0.02, CZ + 0.009), (XR + 0.03, CY1, CZ + 0.024), 'LG', 0.008)
ld.sheet((-XR - 0.03, CY0 - 0.022, CZ - 0.03), (XR + 0.03, CY0 - 0.012, CZ + 0.02), 'LG', 0.003)
ld.tube((-0.08, CY0 - 0.045, CZ - 0.005), (0.08, CY0 - 0.045, CZ - 0.005), 0.009, 'CR', 12, weld=False)
for x in (-0.08, 0.08): ld.box((x - 0.006, CY0 - 0.05, CZ - 0.01), (x + 0.006, CY0 - 0.02, CZ + 0.0), 'CR', 0.002)
ld.tube((-0.3, CY1 - 0.01, CZ + 0.012), (0.3, CY1 - 0.01, CZ + 0.012), 0.008, 'CR', 12, weld=False)
part('Lid', ld, P, 'Frame', 'hinge about x: 0 = shut .. 100 deg open (rear edge up)')
ik('Lid', (0, CY0 - 0.045, CZ - 0.005), 'hand', 'Lid'); ctrl('Lid', 'Lid', 'Lid', 'lift open / shut over the regulator')

# hose reel on the cabinet's right side (axis x), crank, hose to the charging chuck on its hook
RC = (XR + 0.085, CY0 + 0.115, ZF + 0.5); RR = 0.15
f2 = D()
for s in (-1, 1): f2.box((XR + 0.015, RC[1] + s * 0.06 - 0.008, RC[2] - 0.02), (RC[0] + 0.07, RC[1] + s * 0.06 + 0.008, RC[2] + 0.02), 'ST', 0.003)
rl = D()
for h in (-0.055, 0.055):
    rl.lathe([(0.02, h - 0.003), (RR, h - 0.003), (RR, h + 0.003), (0.02, h + 0.003)], 'Y', (0, 0, 0), 'x', 32)
rl.lathe([(0.05, -0.052), (0.05, 0.052)], 'Y', (0, 0, 0), 'x', 24)
for k in range(6):                                                         # wound hose, 6 turns side by side
    h = -0.045 + 0.018 * k
    rl.lathe([(0.05, h - 0.008), (0.1, h - 0.009), (0.106, h), (0.1, h + 0.009), (0.05, h + 0.008)], 'HS', (0, 0, 0), 'x', 32)
rl.tube((0.058, 0, 0), (0.058, 0.12, 0.0), 0.008, 'Y', 10, weld=False)
rl.tube((0.058, 0.12, 0.0), (0.1, 0.12, 0.0), 0.006, 'CR', 10, weld=False)
rl.tube((0.075, 0.12, 0.0), (0.11, 0.12, 0.0), 0.012, 'BK', 12, weld=False)
rl.tube((-0.07, 0, 0), (0.06, 0, 0), 0.012, 'CR', 12, weld=False)
part('HoseReel', rl.mesh('HoseReel', (0, 0, 0)), RC, 'Frame', 'spins about x: pay out / wind in 10 m of hose; the crank turns with it')
ik('ReelCrank', (RC[0] + 0.1, RC[1] + 0.12, RC[2]), 'hand', 'HoseReel'); ctrl('Hose reel', 'HoseReel', 'ReelCrank', 'wind the hose in after charging')
HK = (XR + 0.035, CY0 + 0.05, ZF + 0.24)
ch = D()                                                                   # charging chuck: brass body, clip lever, check gauge
M = frame(HK, (0, 0, 1))
ch.lz([(0.0, -0.06), (0.014, -0.06), (0.014, 0.02), (0.01, 0.03), (0.0, 0.03)], 'BR', M, 16)
ch.lz([(0.0, -0.075), (0.008, -0.075), (0.008, -0.06), (0.0, -0.06)], 'CR', M, 12)
ch.box((HK[0] + 0.012, HK[1] - 0.006, HK[2] - 0.03), (HK[0] + 0.022, HK[1] + 0.006, HK[2] + 0.04), 'CR', 0.002)
ch.fitting((HK[0], HK[1], HK[2] + 0.03), (0, 0, 1), 0.007)
part('Chuck', ch, HK, 'Frame', 'off its hook by hand onto the tyre or strut valve; clip lever holds it')
ik('Chuck', (HK[0], HK[1], HK[2] - 0.02), 'hand', 'Chuck'); ctrl('Charging chuck', 'Chuck', 'Chuck', 'take off the hook, push onto the valve, clip')
f2.box((XR + 0.015, HK[1] - 0.012, HK[2] - 0.01), (HK[0] + 0.004, HK[1] + 0.012, HK[2] + 0.002), 'ST', 0.002)    # hook
f2.hose([(RC[0], RC[1] - 0.02, RC[2] - 0.105), (RC[0] + 0.01, RC[1] - 0.06, RC[2] - 0.2), (HK[0] + 0.02, HK[1] + 0.03, HK[2] + 0.2),
         (HK[0], HK[1], HK[2] + 0.075)], 0.008, 'HS', 12, 6)
part('ReelBracket', f2, (0, 0, 0), 'Frame', 'fixed: reel bracket, hook and the hanging hose')
# lower cabinet door (bottle-change tools, spare seals), hinged on its left edge
dr = D(); P = (-XR + 0.01, CY0 - 0.004, ZF + 0.12)
dr.sheet((-XR + 0.01, CY0 - 0.012, ZF + 0.12), (XR - 0.01, CY0 - 0.002, CZ - 0.45), 'LG', 0.005)
for z in (ZF + 0.18, CZ - 0.51): dr.tube((-XR + 0.006, CY0 - 0.006, z), (-XR + 0.006, CY0 - 0.006, z + 0.06), 0.007, 'CR', 10, weld=False)
dr.box((XR - 0.07, CY0 - 0.03, ZF + 0.4), (XR - 0.05, CY0 - 0.0095, ZF + 0.52), 'BK', 0.004)
part('Door_Cabinet', dr, P, 'Frame', 'hinge about z: 0 = shut .. 110 deg open')
ik('Door_Cabinet', (XR - 0.06, CY0 - 0.035, ZF + 0.46), 'hand', 'Door_Cabinet'); ctrl('Cabinet door', 'Door_Cabinet', 'Door_Cabinet', 'open / shut the lower cabinet')
park_brake('Frame', (-XR - 0.05, YB + 0.32, ZF + 0.03), -1)
ik('Push_L', (-(XR + 0.04), CY0 - 0.06, ZF + 0.61), 'push', 'Frame'); ik('Push_R', (XR + 0.04, CY0 - 0.06, ZF + 0.61), 'push', 'Frame')
ik('Operator', (0, CY0 - 0.45, 0), 'stand', 'Frame')
finish('NitrogenCart', 'nitrogen servicing cart (tyres, struts, accumulators)',
       {'bottles': '3 x 230 mm x 0.92 m, about 35 l, 200 bar', 'wheels': '4.00-8 pneumatic, 410 mm', 'panel_height_m': [0.84, 1.26],
        'hose_m': 10, 'steer_deg': 35},
       {'x_min': -0.52, 'x_max': 0.56, 'y_min': YR0 - 0.1, 'y_max': YF + 0.25, 'towbar_down_reach_y': YF + 1.2,
        'note': 'tow bar up; operator stands behind the panel (y about -1.5)'},
       'Wikimedia Commons: US Navy 070430-N-4420S-029 nitrogen cart (public domain); 4.00-8 wheel and 50 l bottle sizes')
