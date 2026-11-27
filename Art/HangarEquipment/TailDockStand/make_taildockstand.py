# Tail dock stand: a mobile stair tower with an 8.5 m top deck for tail-strike, fin and rudder work on an A320 (to-do 364
# A), our own model, no brand. Looked at: mobile maintenance towers and tail docks in public-domain US Air Force / Navy
# photos on Wikimedia Commons (yellow square towers with switchback stairs, guard rails, outriggers, castors). Sizes from
# published narrow-body tail stands: top deck 8-10 m (A320 fin tip 11.8 m, rudder hinge line from ~6 m), tower 2.0 x
# 2.4 m, stairs about 48 deg with 7 risers per flight and landings, 1.1 m rails, four outriggers with screw feet.
# Mechanics push it out of the aircraft's path before a tow (castors with brakes, outriggers folded) and back after.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_lib import *

reset(); materials('TDS')
R = 0.125; CT = 2 * R + 0.07; ZB = CT + 0.003; T = 0.12
BX, BY = 1.0, 1.2
Z0 = ZB + T + 0.006; ZD = 8.5; NF = 6; H = (ZD - Z0) / NF; NR = 7
PX, PY = 0.95, 1.15
f = B()
# base frame, castor cross members, floor plate
f.box((-BX, -BY, ZB), (BX, -BY + T, ZB + T), 'Y'); f.box((-BX, BY - T, ZB), (BX, BY, ZB + T), 'Y')
for s in (-1, 1): f.box((s * BX - (T if s > 0 else 0), -BY + T, ZB), (s * BX + (0 if s > 0 else T), BY - T, ZB + T), 'Y')
for y in (-0.98, 0.98): f.box((-BX + T, y - 0.06, ZB), (BX - T, y + 0.06, ZB + T - 0.02), 'Y')
f.box((-BX + T, -0.04, ZB), (BX - T, 0.04, ZB + T - 0.02), 'Y')
f.box((-0.88, -1.08, ZB + T), (0.88, 1.08, Z0), 'PL', 0.002)
# corner posts and ties at the landing levels; diagonal bracing on the long sides
for sx in (-1, 1):
    for sy in (-1, 1): f.box((sx * PX - 0.05, sy * PY - 0.05, ZB + T), (sx * PX + 0.05, sy * PY + 0.05, ZD + 1.1), 'Y', 0.004)
LV = [Z0 + H * k for k in range(NF + 1)]
for k in range(1, NF + 1):
    z = LV[k]
    for sx in (-1, 1): f.box((sx * PX - 0.03, -PY + 0.05, z - 0.10), (sx * PX + 0.03, PY - 0.05, z), 'Y', 0.003)
    for sy in (-1, 1):
        if k == NF or (k % 2 == 1) == (sy > 0): f.box((-PX + 0.05, sy * PY - 0.03, z - 0.10), (PX - 0.05, sy * PY + 0.03, z), 'Y', 0.003)
for k in range(NF):
    for sx in (-1, 1):
        a, b = (-PY + 0.05, LV[k] + 0.02 if k else ZB + T + 0.05), (PY - 0.05, LV[k + 1] - 0.10)
        if k % 2: a, b = (PY - 0.05, a[1]), (-PY + 0.05, b[1])
        f.bar((sx * PX, a[0], a[1]), (sx * PX, b[0], b[1]), 0.05, 0.03, 'Y', up=(1, 0, 0))
# flights: lane A (x<0) goes +y, lane B (x>0) goes -y; 7 risers, treads of tread plate, rails both sides
YS = 0.6; PEND = []
def ikl(*a): PEND.append(a)
def flight(k):
    sx = -1 if k % 2 == 0 else 1; d = 1 if k % 2 == 0 else -1
    x0, x1 = (-0.88, -0.07) if sx < 0 else (0.07, 0.88)
    y0, y1, z0, z1 = -d * YS, d * YS, LV[k], LV[k + 1]
    for x in (x0, x1):
        f.bar((x, y0, z0 - 0.10 if k else z0), (x, y1, z1 - 0.10), 0.012, 0.20, 'Y', up=(0, 1, 0))
        pts = []
        for t in (0.12, 0.88):
            yy, zz = y0 + (y1 - y0) * t, z0 + (z1 - z0) * t
            f.tube((x + (0.022 if x == x0 else -0.022) * -1 * -1, yy, zz), (x, yy, zz + 0.95), 0.019, 'Y'); pts.append((yy, zz))
        xr = x
        f.tube((xr, pts[0][0], pts[0][1] + 0.95), (xr, pts[1][0], pts[1][1] + 0.95), 0.021, 'Y')
        f.tube((xr, pts[0][0], pts[0][1] + 0.50), (xr, pts[1][0], pts[1][1] + 0.50), 0.018, 'Y')
    for i in range(1, NR):
        t = i / NR; yy, zz = y0 + (y1 - y0) * t, z0 + (z1 - z0) * t
        f.box((x0 + 0.004, yy - 0.10, zz - 0.035), (x1 - 0.004, yy + 0.10, zz), 'PL', 0.003)
        ikl('Step_F%d_%d' % (k + 1, i), ((x0 + x1) / 2, yy, zz), 'foot', 'Frame')
    m = 0.5; ikl('Rail_F%d' % (k + 1), (x1 if sx < 0 else x0, y0 + (y1 - y0) * m, z0 + (z1 - z0) * m + 0.95), 'hand', 'Frame')
for k in range(NF): flight(k)
# landings (alternate ends), their beam at the stair edge and the outer guard rails
RT, RM = 1.1, 0.55
for k in range(1, NF):
    sy = 1 if k % 2 else -1; z = LV[k]
    f.box((-0.90, min(sy * YS, sy * (PY - 0.03)), z - 0.006), (0.90, max(sy * YS, sy * (PY - 0.03)), z), 'PL', 0.002)
    f.box((-0.90, sy * YS - 0.03, z - 0.10), (0.90, sy * YS + 0.03, z - 0.006), 'Y', 0.003)
    for h in (RT, RM):
        r = 0.021 if h == RT else 0.018
        f.tube((-PX + 0.05, sy * PY, z + h), (PX - 0.05, sy * PY, z + h), r, 'Y')
        for sx in (-1, 1): f.tube((sx * PX, sy * (YS - 0.0), z + h), (sx * PX, sy * (PY - 0.05), z + h), r, 'Y')
    for sx in (-1, 1): f.tube((sx * PX, sy * YS, z), (sx * PX, sy * YS, z + RT), 0.021, 'Y')
    for sx in (-1, 1): f.box((sx * 0.5 - 0.3, sy * (PY - 0.03) - 0.003, z), (sx * 0.5 + 0.3, sy * (PY - 0.03) + 0.003, z + 0.15), 'WS', 0.001)
    ikl('Landing_%d' % k, (0, sy * 0.88, z), 'stand', 'Frame')
# top deck with the stair hole over lane B (y -0.6..0.6), its rails, perimeter rails
for a, b in (((-PX - 0.05, -PY - 0.05), (0.05, PY + 0.05)), ((0.05, -PY - 0.05), (PX + 0.05, -YS)),
             ((0.90, -YS), (PX + 0.05, YS)), ((0.05, YS), (PX + 0.05, PY + 0.05))):
    f.box((a[0], a[1], ZD - 0.006), (b[0], b[1], ZD), 'PL', 0.002)
f.box((0.02, -YS, ZD - 0.10), (0.05, YS, ZD - 0.006), 'Y', 0.003); f.box((0.05, YS - 0.03, ZD - 0.10), (0.90, YS + 0.03, ZD - 0.006), 'Y', 0.003)
f.box((0.88, -YS, ZD - 0.10), (0.90, YS, ZD - 0.006), 'Y', 0.003)
for h in (RT, RM):
    r = 0.021 if h == RT else 0.018
    for sx in (-1, 1): f.tube((sx * 0.97, -PY + 0.05, ZD + h), (sx * 0.97, PY - 0.05, ZD + h), r, 'Y')
    f.tube((-PX + 0.05, -PY - 0.02, ZD + h), (PX - 0.05, -PY - 0.02, ZD + h), r, 'Y')
    f.tube((0.05, -YS, ZD + h), (0.05, YS, ZD + h), r, 'Y'); f.tube((0.05, YS, ZD + h), (0.86, YS, ZD + h), r, 'Y')
for sx in (-1, 1):
    for y in (-0.4, 0.4): f.tube((sx * 0.97, y, ZD), (sx * 0.97, y, ZD + RT), 0.021, 'Y')
f.tube((0.0, -PY - 0.02, ZD), (0.0, -PY - 0.02, ZD + RT), 0.021, 'Y')
for p in ((0.05, -YS + 0.02), (0.05, 0.0), (0.05, YS), (0.86, YS)): f.tube((p[0], p[1], ZD), (p[0], p[1], ZD + RT), 0.021, 'Y')
f.tube((0.92, -0.72, ZD), (0.92, -0.72, ZD + RT), 0.021, 'Y')                                      # gate stop post
for sx in (-1, 1): f.box((sx * 0.5 - 0.45, -PY - 0.05, ZD), (sx * 0.5 + 0.45, -PY - 0.044, ZD + 0.15), 'WS', 0.001)
for sx in (-1, 1): f.box((sx * PX + (0.05 if sx > 0 else -0.056), -PY + 0.05, ZD), (sx * PX + (0.056 if sx > 0 else -0.05), PY - 0.05, ZD + 0.15), 'WS', 0.001)
# push handles on the long sides, outrigger hinge lugs, tow bar lugs (+y)
for sx in (-1, 1):
    for y in (-0.4, 0.4):
        f.box((sx * 1.0 - 0.035, y - 0.035, ZB + T), (sx * 1.0 + 0.035, y + 0.035, ZB + T + 0.01), 'Y', 0.002)
        f.tube((sx * 1.0, y, ZB + T + 0.01), (sx * 1.0, y, 1.05), 0.019, 'Y')
    f.tube((sx * 1.0, -0.4, 1.05), (sx * 1.0, 0.4, 1.05), 0.019, 'Y'); f.tube((sx * 1.0, -0.25, 1.05), (sx * 1.0, 0.25, 1.05), 0.023, 'BK')
    for sy in (-1, 1):
        for z in (ZB + 0.005, ZB + 0.103):
            f.box((sx * 1.0 - (0.0 if sx > 0 else 0.105), sy * 1.17 - 0.035, z), (sx * 1.0 + (0.105 if sx > 0 else 0.0), sy * 1.17 + 0.035, z + 0.012), 'ST', 0.002)
for sx in (-1, 1): f.box((sx * 0.07 - 0.006, BY, ZB + 0.01), (sx * 0.07 + 0.006, BY + 0.07, ZB + 0.08), 'ST', 0.002)
part('Frame', f, (0, 0, 0), None, 'root: the tower rolls on its castors with the outriggers folded; outriggers down before climbing')
for a in PEND: ik(*a)

for tag, x, y in (('FL', -0.80, 0.98), ('FR', 0.80, 0.98), ('RL', -0.80, -0.98), ('RR', 0.80, -0.98)):
    castor(tag, x, y, 'Frame', R, 0.06, CT)

# outriggers: arms swing out about z on the corner lugs; each screw foot winds down to the floor
def arm_mesh(d, nm):
    ar = B()
    ar.tube((0, 0, -0.08), (0, 0, -0.002), 0.03, 'Y', weld=False); ar.box((-0.04, -d * 1.0 if d > 0 else 0.03, -0.08), (0.04, -0.03 if d > 0 else 1.0, -0.01), 'Y', 0.004)
    ar.tube((0, 0, -0.11), (0, 0, 0.012), 0.012, 'CR', weld=False)
    ar.box((-0.04, -d * 1.04 if d > 0 else 0.96, -0.10), (0.04, -0.96 if d > 0 else 1.04, 0.0), 'Y', 0.004)
    return ar.mesh(nm, (0, 0, 0))
ARM = {1: arm_mesh(1, 'TDS_OutriggerArm_F'), -1: arm_mesh(-1, 'TDS_OutriggerArm_R')}
ft = B(); ft.lathe([(0.0, 0.0), (0.085, 0.0), (0.085, 0.02), (0.03, 0.035), (0.0, 0.035)], 'RB', (0, 0, 0), 'z', 22)
ft.tube((0, 0, 0.035), (0, 0, ZB + 0.30 - 0.06), 0.02, 'ST'); ft.tube((0, 0, ZB + 0.24), (0.12, 0, ZB + 0.24), 0.01, 'ST')
ft.tube((0.12, 0, ZB + 0.24), (0.12, 0, ZB + 0.31), 0.014, 'BK')
FTM = ft.mesh('TDS_OutriggerFoot', (0, 0, 0))
for sx, sy, tag in ((-1, 1, 'FL'), (1, 1, 'FR'), (-1, -1, 'RL'), (1, -1, 'RR')):
    P = (sx * 1.07, sy * 1.17, ZB + 0.10)
    part('Outrigger_' + tag, ARM[sy], P, 'Frame', 'swings about z 0..135 deg out to the diagonal (stowed along the side)')
    fp = (sx * 1.07, sy * 0.17, 0.06)
    part('OutriggerFoot_' + tag, FTM, fp, 'Outrigger_' + tag, 'screw foot: crank turns, foot slides z -0.06..0 (down = on the floor)')
    ik('Outrigger_' + tag, (sx * 1.07, sy * 0.60, ZB + 0.0), 'hand', 'Outrigger_' + tag)
    ik('OutriggerCrank_' + tag, (sx * 1.07 + 0.12, sy * 0.17, ZB + 0.33), 'hand', 'OutriggerFoot_' + tag)
    ctrl('Outrigger ' + tag, 'Outrigger_' + tag, 'Outrigger_' + tag, 'swing out before climbing, fold in before moving')
    ctrl('Outrigger foot ' + tag, 'OutriggerFoot_' + tag, 'OutriggerCrank_' + tag, 'crank down onto the floor / up before moving')

# deck extension (slides +y toward the fin), with its own rails and the rubber bumper
ex = B(); ZE = ZD + 0.004; P = (0, 0.65, ZE)
ex.box((-0.88, 0.65, ZE), (0.88, 1.25, ZE + 0.03), 'PL', 0.003)
for sx in (-1, 1):
    for y in (0.70, 1.23): ex.tube((sx * 0.86, y, ZE + 0.03), (sx * 0.86, y, ZD + RT), 0.021, 'Y')
    ex.tube((sx * 0.86, 0.70, ZD + RT), (sx * 0.86, 1.23, ZD + RT), 0.021, 'Y'); ex.tube((sx * 0.86, 0.70, ZD + RM), (sx * 0.86, 1.23, ZD + RM), 0.018, 'Y')
ex.tube((-0.86, 1.23, ZD + RT), (0.86, 1.23, ZD + RT), 0.021, 'Y'); ex.tube((-0.86, 1.23, ZD + RM), (0.86, 1.23, ZD + RM), 0.018, 'Y')
ex.box((-0.6, 1.255, ZE + 0.002), (0.6, 1.31, ZE + 0.05), 'RB', 0.008)
ex.tube((-0.15, 1.23, ZD + RT), (0.15, 1.23, ZD + RT), 0.027, 'BK', weld=False)
part('Deck_Extension', ex, P, 'Frame', 'slides along +y 0..0.55 m toward the fin')
ik('Extension_Pull', (0, 1.23, ZD + RT), 'hand', 'Deck_Extension')
ctrl('Deck extension', 'Deck_Extension', 'Extension_Pull', 'push out to the fin until the bumper touches, pull back before moving')

# self-closing gate at the stair head (swings about z on its hinge at x 0.08), with its latch
gt = B(); P = (0.08, -0.72, ZD + 0.05)
gt.tube((0.08, -0.72, ZD + 0.05), (0.08, -0.72, ZD + RT), 0.019, 'Y')
gt.tube((0.08, -0.72, ZD + RT), (0.87, -0.72, ZD + RT), 0.019, 'Y'); gt.tube((0.08, -0.72, ZD + RM), (0.87, -0.72, ZD + RM), 0.016, 'Y')
gt.tube((0.87, -0.72, ZD + RM - 0.05), (0.87, -0.72, ZD + RT), 0.019, 'Y')
gt.tube((0.053, -0.72, ZD + 0.12), (0.061, -0.72, ZD + 0.12), 0.012, 'CR', weld=False); gt.tube((0.053, -0.72, ZD + 1.0), (0.061, -0.72, ZD + 1.0), 0.012, 'CR', weld=False)
part('TopGate', gt, P, 'Frame', 'swings about z 0..90 deg toward -y, springs shut')
lt = B(); P = (0.87, -0.72, ZD + RT - 0.06)
lt.box((0.885, -0.73, ZD + RT - 0.08), (0.895, -0.71, ZD + RT - 0.04), 'ST', 0.002)
lt.tube((0.895, -0.72, ZD + RT - 0.06), (0.896, -0.72, ZD + RT - 0.06), 0.01, 'ST', weld=False)
part('GateLatch', lt, P, 'TopGate', 'flips about y 0..70 deg up to free the gate')
ik('GateLatch', (0.89, -0.72, ZD + RT - 0.06), 'hand', 'GateLatch'); ik('TopGate', (0.6, -0.72, ZD + RT), 'hand', 'TopGate')
ctrl('Top gate latch', 'GateLatch', 'GateLatch', 'flip up to open the gate')
ctrl('Top gate', 'TopGate', 'TopGate', 'push open, it springs shut behind')

# tow bar (+y end)
tb = B(); P = (0, BY + 0.04, ZB + 0.045)
tb.tube((-0.072, P[1], P[2]), (0.072, P[1], P[2]), 0.012, 'CR', weld=False); tb.tube(P, (0, P[1], P[2] + 0.95), 0.022, 'Y')
tb.tube((-0.11, P[1], P[2] + 0.92), (0.11, P[1], P[2] + 0.92), 0.02, 'BK')
part('TowBar', tb, P, 'Frame', 'hinge about x: 0 = up (parked) .. 85 deg forward = towing')
ik('TowBar_L', (-0.08, P[1], P[2] + 0.92), 'push', 'TowBar'); ik('TowBar_R', (0.08, P[1], P[2] + 0.92), 'push', 'TowBar')
ctrl('Tow bar', 'TowBar', 'TowBar_R', 'lower forward to tow, lift up to park')

ik('Deck', (-0.45, 0, ZD), 'stand', 'Frame'); ik('Floor', (-0.45, -0.9, Z0), 'stand', 'Frame')
ik('DeckRail_L', (-0.97, 0.2, ZD + RT), 'hand', 'Frame'); ik('DeckRail_R', (0.97, 0.2, ZD + RT), 'hand', 'Frame')
ik('Push_L', (-1.0, 0, 1.05), 'push', 'Frame'); ik('Push_R', (1.0, 0, 1.05), 'push', 'Frame')
finish('TailDockStand', 'tail dock stand (mobile stair tower)',
       {'top_deck_m': ZD, 'rail_top_m': ZD + RT, 'tower_m': [2 * BX, 2 * BY], 'flights': NF, 'risers_per_flight': NR,
        'riser_m': round(H / NR, 3), 'going_m': round(2 * YS / NR, 3), 'stair_deg': round(math.degrees(math.atan2(H, 2 * YS)), 1),
        'extension_m': 0.55, 'castors_mm': 250},
       {'x_min': -1.11, 'x_max': 1.11, 'y_min': -1.21, 'y_max': 1.31,
        'outriggers_out': {'x_min': -1.85, 'x_max': 1.85, 'y_min': -1.95, 'y_max': 1.95},
        'note': 'rolling = folded outriggers; working = outriggers out (bigger box); tow bar down reaches y %.2f' % (BY + 1.0)},
       'Wikimedia Commons public-domain USAF / US Navy photos of mobile maintenance towers and tail docks; published narrow-body tail stand sizes')
