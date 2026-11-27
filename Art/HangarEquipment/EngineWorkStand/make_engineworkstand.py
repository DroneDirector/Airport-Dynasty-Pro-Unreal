# Engine work stand for A320 nacelle work (to-do 364 A), our own model. Looked at: a yellow wheeled maintenance stand at a
# C-130 nacelle (Wikimedia Commons, "374 AW MXS, AMXS perform engine maintenance operations in Sri Lanka", US Air Force,
# public domain). Sizes: A320 CFM56 / LEAP nacelle bottom ~0.6 m, centre ~1.6 m, top ~2.6 m above the floor, so the deck
# is at 1.75 m (a 1.8 m mechanic reaches the fan cowl latches and the top of the nacelle). Deck 1.2 x 1.0 m, rails 1.1 m
# with knee rail and toe boards, a 61 deg step ladder with 6 treads at 0.25 m, 4 braked swivel castors (200 mm),
# side push handles, a folding tow bar, a fold-down front rail and a deck extension that slides 0.45 m toward the cowl.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_lib import *

reset(); materials('EWS')
HD = 1.75                       # deck top
BX, BY0, BY1 = 0.75, -0.62, 0.75   # base half width, rear, front
R = 0.10; CT = 2 * R + 0.07     # castor plate top
ZB = CT + 0.003                 # base frame bottom (gap over the castor plates)
T = 0.08                        # base tube

f = B()
# base: a ring of 80 mm tube plus two cross members
f.box((-BX, BY0, ZB), (BX, BY0 + T, ZB + T), 'Y'); f.box((-BX, BY1 - T, ZB), (BX, BY1, ZB + T), 'Y')
for s in (-1, 1): f.box((s * BX - (T if s > 0 else 0), BY0 + T, ZB), (s * BX + (0 if s > 0 else T), BY1 - T, ZB + T), 'Y')
for y in (-0.475, 0.475): f.box((-BX + T, y - 0.025, ZB), (BX - T, y + 0.025, ZB + 0.06), 'Y')
f.box((-0.42, BY1, ZB + 0.01), (0.42, BY1 + 0.05, ZB + 0.07), 'RB')      # rubber bumper at the front
# four posts 50 mm square to the deck rim, side diagonals and mid ties
for sx in (-1, 1):
    for y in (-0.475, 0.475): f.box((sx * 0.575 - 0.025, y - 0.025, ZB + 0.06 if abs(y) < 0.5 else ZB + T), (sx * 0.575 + 0.025, y + 0.025, HD - 0.056), 'Y')
    f.bar((sx * 0.575, -0.45, ZB + 0.08), (sx * 0.575, 0.45, HD - 0.08), 0.04, 0.03, 'Y', up=(1, 0, 0))
    f.box((sx * 0.575 - 0.02, -0.45, 0.98), (sx * 0.575 + 0.02, 0.45, 1.02), 'Y')
# deck rim (front rim set low: the extension slides over it) and the tread plate
for sx in (-1, 1): f.box((sx * 0.575 - 0.025, -0.5, HD - 0.056), (sx * 0.575 + 0.025, 0.5, HD - 0.006), 'Y')
f.box((-0.55, -0.5, HD - 0.056), (0.55, -0.45, HD - 0.006), 'Y')
f.box((-0.55, 0.45, HD - 0.13), (0.55, 0.5, HD - 0.075), 'Y')
f.box((-0.6, -0.5, HD - 0.006), (0.6, 0.5, HD), 'PL', 0.002)
f.box((-0.5, -0.43, HD - 0.07), (-0.46, 0.45, HD - 0.03), 'ST'); f.box((0.46, -0.43, HD - 0.07), (0.5, 0.45, HD - 0.03), 'ST')  # slide guides
# deck rails: sides and the back beside the ladder opening; toe boards
RT, RK = HD + 1.1, HD + 0.55
for sx in (-1, 1):
    for y in (-0.47, 0.0, 0.45): f.tube((sx * 0.58, y, HD), (sx * 0.58, y, RT), 0.021, 'Y')
    f.tube((sx * 0.58, -0.47, RT), (sx * 0.58, 0.45, RT), 0.021, 'Y'); f.tube((sx * 0.58, -0.47, RK), (sx * 0.58, 0.45, RK), 0.018, 'Y')
    f.box((sx * 0.595 - 0.003, -0.5, HD), (sx * 0.595 + 0.003, 0.5, HD + 0.15), 'WS', 0.001)
    f.tube((sx * 0.42, -0.47, HD), (sx * 0.42, -0.47, RT), 0.021, 'Y')
    f.tube((sx * 0.42, -0.47, RT), (sx * 0.58, -0.47, RT), 0.021, 'Y'); f.tube((sx * 0.42, -0.47, RK), (sx * 0.58, -0.47, RK), 0.018, 'Y')
    # hinge lugs for the ladder and for the front rail
    f.box((sx * 0.36 - 0.006, -0.61, HD - 0.075), (sx * 0.36 + 0.006, -0.5, HD - 0.03), 'ST', 0.002)
    f.box((sx * 0.62, 0.48, HD - 0.05), (sx * 0.632, 0.54, HD + 0.03), 'ST', 0.002)
# side push handles (the mechanics push the stand out of the aircraft's path and back)
for sx in (-1, 1):
    f.tube((sx * (BX + 0.02), -0.3, ZB + 0.04), (sx * (BX + 0.02), -0.3, 1.05), 0.019, 'Y')
    f.tube((sx * (BX + 0.02), 0.3, ZB + 0.04), (sx * (BX + 0.02), 0.3, 1.05), 0.019, 'Y')
    f.tube((sx * (BX + 0.02), -0.3, 1.05), (sx * (BX + 0.02), 0.3, 1.05), 0.019, 'Y')
    f.tube((sx * (BX + 0.02), -0.2, 1.05), (sx * (BX + 0.02), 0.2, 1.05), 0.023, 'BK')       # grip sleeve
    f.box((sx * (BX + 0.02) - 0.03, -0.32, ZB + 0.01), (sx * (BX + 0.02) + 0.03, 0.32, ZB + 0.04), 'Y')
# tow bar lugs at the front middle
for sx in (-1, 1): f.box((sx * 0.06 - 0.006, BY1 + 0.05, ZB + 0.01), (sx * 0.06 + 0.006, BY1 + 0.11, ZB + 0.07), 'ST', 0.002)
part('Frame', f, (0, 0, 0), None, 'root: the stand, rolls on its castors')

for tag, x, y in (('FL', -0.62, 0.475), ('FR', 0.62, 0.475), ('RL', -0.62, -0.475), ('RR', 0.62, -0.475)):
    castor(tag, x, y, 'Frame', R, 0.05, CT)

# tow bar: lies forward to tow, stands up when parked (pivot on its lugs)
tb = B(); P = (0, BY1 + 0.08, ZB + 0.04)
tb.tube((-0.062, P[1], P[2]), (0.062, P[1], P[2]), 0.012, 'CR')
tb.tube((0, P[1], P[2]), (0, P[1], P[2] + 0.95), 0.022, 'Y')
tb.lathe([(0.0, 0.95), (0.06, 0.95), (0.06, 0.975), (0.0, 0.975)], 'Y', (0, P[1], P[2]), 'z', 20)
tb.tube((-0.11, P[1], P[2] + 0.92), (0.11, P[1], P[2] + 0.92), 0.02, 'BK')
part('TowBar', tb, P, 'Frame', 'hinge about x: 0 = up (parked) .. 85 deg forward = towing')
ik('TowBar_L', (-0.08, P[1], P[2] + 0.92), 'push', 'TowBar'); ik('TowBar_R', (0.08, P[1], P[2] + 0.92), 'push', 'TowBar')

# step ladder, hinged at the back of the deck, folds up for moving
HT = (0, -0.58, HD - 0.05); FT = -1.46
tana = (HT[2]) / (HT[1] - FT)
def lad_y(z): return HT[1] - (HT[2] - z) / tana
ld = B()
for sx in (-1, 1):
    ld.bar((sx * 0.32, HT[1], HT[2] + 0.02), (sx * 0.32, FT, 0.035), 0.012, 0.09, 'Y', up=(0, 1, 0))
    ld.box((sx * 0.32 - 0.03, FT - 0.06, 0.0), (sx * 0.32 + 0.03, FT + 0.05, 0.02), 'RB')
    # handrail 0.9 m above the treads
    yb, yt = lad_y(0.25), lad_y(HT[2] - 0.12)
    ld.tube((sx * 0.335, yb, 0.25), (sx * 0.335, yb, 0.25 + 0.9), 0.019, 'Y')
    ld.tube((sx * 0.335, yt, HT[2] - 0.12), (sx * 0.335, yt, HT[2] - 0.12 + 0.9), 0.019, 'Y')
    ld.tube((sx * 0.335, yb, 1.15), (sx * 0.335, yt, HT[2] + 0.78), 0.021, 'Y')
ld.tube((-0.37, HT[1], HT[2]), (0.37, HT[1], HT[2]), 0.012, 'CR')
STEPS = [0.25 * k for k in range(1, 7)]
for z in STEPS:
    y = lad_y(z)
    ld.box((-0.314, y - 0.08, z - 0.03), (0.314, y + 0.08, z), 'PL', 0.002)
part('Ladder', ld, HT, 'Frame', 'hinge about x at the deck: 0 = on the floor .. 75 deg folded up')
for k, z in enumerate(STEPS): ik('Step_%d' % (k + 1), (0, lad_y(z), z), 'foot', 'Ladder')
ik('LadderRail_L', (-0.335, lad_y(0.9), 1.83), 'hand', 'Ladder'); ik('LadderRail_R', (0.335, lad_y(0.9), 1.83), 'hand', 'Ladder')

# front rail: drops forward onto the cowl side when the deck extension is out
fr = B(); P = (0, 0.515, HD + 0.012)
for sx in (-1, 1):
    fr.tube((sx * 0.64, 0.515, HD + 0.012), (sx * 0.64, 0.515, RT), 0.021, 'Y')
fr.tube((-0.64, 0.515, RT), (0.64, 0.515, RT), 0.021, 'Y'); fr.tube((-0.64, 0.515, RK), (0.64, 0.515, RK), 0.018, 'Y')
fr.tube((-0.66, 0.515, HD + 0.012), (0.66, 0.515, HD + 0.012), 0.01, 'CR')
part('Rail_Front', fr, P, 'Frame', 'hinge about x: 0 = up .. 90 deg down forward')
ik('RailFront_Grip', (0, 0.515, RT), 'hand', 'Rail_Front')

# deck extension: slides out toward the cowl
ex = B(); P = (0, 0.48, HD - 0.009)
ex.box((-0.455, -0.43, HD - 0.015), (0.455, 0.48, HD - 0.009), 'PL', 0.002)
ex.box((-0.455, 0.46, HD - 0.07), (0.455, 0.48, HD - 0.015), 'Y', 0.002)
ex.tube((-0.15, 0.49, HD - 0.045), (0.15, 0.49, HD - 0.045), 0.012, 'BK')
part('Deck_Extension', ex, P, 'Frame', 'slides along +y 0..0.45 m')
ik('Extension_Pull', (0, 0.49, HD - 0.045), 'hand', 'Deck_Extension')

lt = B(); P = (0.40, -0.52, HD - 0.04)
lt.tube((0.40, -0.52, HD - 0.04), (0.385, -0.52, HD - 0.04), 0.008, 'CR')
lt.box((0.40, -0.535, HD - 0.06), (0.44, -0.505, HD - 0.02), 'RD', 0.004)
part('LadderLatch', lt, P, 'Frame', 'turns about x 0..60 deg: up = ladder free to fold')
ik('LadderLatch', (0.43, -0.535, HD - 0.04), 'hand', 'LadderLatch')
ctrl('Ladder latch', 'LadderLatch', 'LadderLatch', 'lift to free the ladder, then fold it up for moving')
ctrl('Ladder fold', 'Ladder', 'LadderRail_R', 'fold up 75 deg for moving / down to climb')
ctrl('Front rail', 'Rail_Front', 'RailFront_Grip', 'drop forward onto the cowl side / lift back up')
ctrl('Deck extension', 'Deck_Extension', 'Extension_Pull', 'pull out toward the cowl 0..0.45 m')
ctrl('Tow bar', 'TowBar', 'TowBar_R', 'lower forward to tow, lift up to park')
ik('Deck', (0, 0, HD), 'stand', 'Frame')
ik('DeckRail_L', (-0.58, 0.1, RT), 'hand', 'Frame'); ik('DeckRail_R', (0.58, 0.1, RT), 'hand', 'Frame')
ik('Push_L', (-(BX + 0.02), 0, 1.05), 'push', 'Frame'); ik('Push_R', (BX + 0.02, 0, 1.05), 'push', 'Frame')
finish('EngineWorkStand', 'engine work stand (A320 nacelle)',
       {'deck_height_m': HD, 'deck_m': [1.2, 1.0], 'rail_m': 1.1, 'ladder_deg': round(math.degrees(math.atan(tana)), 1),
        'castors_mm': 200, 'extension_m': 0.45},
       {'x_min': -0.79, 'x_max': 0.79, 'y_min': FT - 0.06, 'y_max': BY1 + 0.05, 'towbar_down_reach_y': BY1 + 1.0,
        'note': 'floor size with the ladder down; ladder folded: y_min -0.65'},
       'Wikimedia Commons: 374 AW MXS, AMXS perform engine maintenance operations in Sri Lanka (public domain); A320 nacelle heights')
