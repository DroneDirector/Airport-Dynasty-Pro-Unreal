# Fuselage / door work stand, adjustable 2.5 - 4.0 m deck (to-do 364 A), our own model, no brand. Looked at: yellow
# wheeled maintenance stands at a C-130 (Wikimedia Commons "374 AW MXS, AMXS perform engine maintenance operations",
# public domain) and the scissor lift photos for rails and gates. Sizes from published adjustable docking platforms for
# narrow-bodies: A320 passenger door sill ~3.4 m, cargo door sill ~2.1 m, wing-root fuselage 3-4 m -> deck 2.5..4.0 m,
# deck 1.6 x 1.2 m, rails 1.1 m, four telescopic legs (120 / 100 mm), electro-hydraulic with a hand pump back-up.
# Self-levelling stair: the stair is hinged at the back of the deck and its foot rolls on the floor; each tread turns on
# its stringer pins so it stays level (tread turn = minus the stair's turn). Modelled at the lowest deck, 2.50 m.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_lib import *

reset(); materials('DWS')
HD = 2.50; STROKE = 1.50
R = 0.125; CT = 2 * R + 0.07; ZB = CT + 0.003; T = 0.10
BX, BY = 0.90, 0.75
DX, DY = 0.80, 0.60
f = B()
f.box((-BX, -BY, ZB), (BX, -BY + T, ZB + T), 'Y'); f.box((-BX, BY - T, ZB), (BX, BY, ZB + T), 'Y')
for s in (-1, 1): f.box((s * BX - (T if s > 0 else 0), -BY + T, ZB), (s * BX + (0 if s > 0 else T), BY - T, ZB + T), 'Y')
for y in (-DY + 0.06, DY - 0.06): f.box((-BX + T, y - 0.05, ZB), (BX - T, y + 0.05, ZB + 0.08), 'Y')
ZOT = 2.25
for sx in (-1, 1):
    for sy in (-1, 1):
        x, y = sx * (DX - 0.06), sy * (DY - 0.06)
        f.box((x - 0.06, y - 0.06, ZB + 0.08), (x + 0.06, y + 0.06, ZOT), 'Y', 0.004)       # outer telescopic legs
        f.box((x - 0.07, y - 0.07, ZOT - 0.04), (x + 0.07, y + 0.07, ZOT), 'G', 0.003)        # guide collar
    f.bar((sx * (DX - 0.06), -DY + 0.12, ZB + 0.12), (sx * (DX - 0.06), DY - 0.12, 1.9), 0.05, 0.04, 'Y', up=(1, 0, 0))
    f.box((sx * (DX - 0.06) - 0.025, -DY + 0.12, 1.15), (sx * (DX - 0.06) + 0.025, DY - 0.12, 1.2), 'Y')
f.box((-0.67, -0.03, ZB + 0.08), (0.67, 0.03, ZB + 0.14), 'Y')
# hydraulic power pack + hand pump on the base, right side
f.box((0.30, -0.40, ZB + 0.08), (0.66, -0.12, ZB + 0.40), 'G', 0.01)
f.lathe([(0.05, 0.0), (0.05, 0.25), (0.0, 0.27)], 'G', (0.40, -0.26, ZB + 0.40), 'z', 20)
for y in (-0.33, -0.19): f.box((0.66, y - 0.006, ZB + 0.30), (0.72, y + 0.006, ZB + 0.38), 'ST', 0.002)
f.lathe([(0.02, 0.0), (0.02, 0.03)], 'ST', (0.66, -0.18 + 0.05, ZB + 0.16), 'x', 14)
# side push handles, front tow bar lugs, front rubber bumper
for sx in (-1, 1):
    for y in (-0.35, 0.35): f.tube((sx * (BX + 0.02), y, ZB + 0.04), (sx * (BX + 0.02), y, 1.05), 0.019, 'Y')
    f.tube((sx * (BX + 0.02), -0.35, 1.05), (sx * (BX + 0.02), 0.35, 1.05), 0.019, 'Y')
    f.tube((sx * (BX + 0.02), -0.22, 1.05), (sx * (BX + 0.02), 0.22, 1.05), 0.023, 'BK')
    f.box((sx * (BX + 0.02) - 0.03, -0.37, ZB + 0.01), (sx * (BX + 0.02) + 0.03, 0.37, ZB + 0.04), 'Y')
    f.box((sx * 0.07 - 0.006, BY, ZB + 0.01), (sx * 0.07 + 0.006, BY + 0.07, ZB + 0.08), 'ST', 0.002)
    for sy in (-1, 1):                                                                            # stabiliser sleeves
        f.box((sx * (BX + 0.005), sy * (BY - 0.15) - 0.04, ZB - 0.02), (sx * (BX + 0.085), sy * (BY - 0.15) + 0.04, ZB + 0.12), 'Y', 0.004)
for sx in (-1, 1): f.box((sx * 0.5 - 0.3, BY + 0.0, ZB + 0.02), (sx * 0.5 + 0.3, BY + 0.05, ZB + 0.09), 'RB')
part('Base', f, (0, 0, 0), None, 'root: rolls on its castors; stabilisers down before anyone climbs')

for tag, x, y in (('FL', -0.72, 0.45), ('FR', 0.72, 0.45), ('RL', -0.72, -0.45), ('RR', 0.72, -0.45)):
    castor(tag, x, y, 'Base', R, 0.06, CT)
st = B(); st.tube((0, 0, -0.02), (0, 0, 0.30), 0.022, 'ST'); st.lathe([(0.0, -0.02), (0.06, -0.02), (0.06, 0.0), (0.0, 0.0)], 'RB', (0, 0, 0), 'z', 18)
st.tube((0, 0, 0.30), (0, 0, 0.33), 0.03, 'Y'); st.tube((0, 0, 0.33), (0.0, 0.12, 0.33), 0.01, 'ST'); st.tube((0, 0.12, 0.33), (0, 0.12, 0.40), 0.014, 'BK')
SM = st.mesh('DWS_Stabiliser', (0, 0, 0))
for sx, sy, tag in ((-1, 1, 'FL'), (1, 1, 'FR'), (-1, -1, 'RL'), (1, -1, 'RR')):
    p = (sx * (BX + 0.045), sy * (BY - 0.15), 0.04)
    part('Stabiliser_' + tag, SM, p, 'Base', 'screw foot: crank turns, foot slides z -0.04..0 (down = on the floor)')
    ik('Stabiliser_' + tag, (p[0], p[1] + 0.12, 0.44), 'hand', 'Stabiliser_' + tag)
    ctrl('Stabiliser ' + tag, 'Stabiliser_' + tag, 'Stabiliser_' + tag, 'crank down before climbing, up before moving')

# deck lift: the inner legs, deck frame, tread plate, rails, front bumper; rises z 0..1.5 m
dl = B(); ZU = HD - 0.12
for sx in (-1, 1):
    for sy in (-1, 1):
        x, y = sx * (DX - 0.06), sy * (DY - 0.06)
        dl.box((x - 0.05, y - 0.05, 0.50), (x + 0.05, y + 0.05, ZU), 'G', 0.003)
for sy in (-1, 1): dl.box((-DX, sy * DY - (0.06 if sy > 0 else 0), ZU), (DX, sy * DY + (0 if sy > 0 else 0.06), HD - 0.006), 'Y')
for sx in (-1, 1): dl.box((sx * DX - (0.06 if sx > 0 else 0), -DY + 0.06, ZU), (sx * DX + (0 if sx > 0 else 0.06), DY - 0.06, HD - 0.006), 'Y')
dl.box((-0.70, -0.03, ZU + 0.02), (0.70, 0.03, HD - 0.006), 'Y')
dl.box((-DX, -DY, HD - 0.006), (DX, DY, HD), 'PL', 0.002)
for sx in (-1, 1): dl.box((sx * 0.62 - 0.02, -0.40, ZU - 0.05), (sx * 0.62 + 0.02, DY - 0.07, ZU), 'ST', 0.003)   # extension guides
RT, RM = HD + 1.1, HD + 0.55
for sx in (-1, 1):
    x = sx * (DX - 0.02)
    for y in (-DY + 0.02, 0.0, DY - 0.05): dl.tube((x, y, HD), (x, y, RT), 0.021, 'Y')
    dl.tube((x, -DY + 0.02, RT), (x, DY - 0.05, RT), 0.021, 'Y'); dl.tube((x, -DY + 0.02, RM), (x, DY - 0.05, RM), 0.018, 'Y')
    dl.box((sx * (DX - 0.003) - 0.003, -DY, HD), (sx * (DX - 0.003) + 0.003, DY, HD + 0.15), 'WS', 0.001)
    dl.tube((sx * 0.52, -DY + 0.02, HD), (sx * 0.52, -DY + 0.02, RT), 0.021, 'Y')
    dl.tube((sx * 0.52, -DY + 0.02, RT), (x, -DY + 0.02, RT), 0.021, 'Y'); dl.tube((sx * 0.52, -DY + 0.02, RM), (x, -DY + 0.02, RM), 0.018, 'Y')
    dl.box((sx * 0.49 - 0.006, -DY - 0.07, HD - 0.11), (sx * 0.49 + 0.006, -DY, HD - 0.02), 'ST', 0.002)        # stair hinge lugs
    dl.box((sx * (DX + 0.01), DY - 0.06, HD - 0.04), (sx * (DX + 0.022), DY + 0.03, HD + 0.06), 'ST', 0.002)    # front gate lugs
# deck control box (up / down) on the right rail, rear
dl.box((DX - 0.16, -0.42, RT - 0.30), (DX - 0.045, -0.22, RT - 0.05), 'G', 0.008)
dl.box((DX - 0.06, -0.38, RT - 0.06), (DX - 0.045, -0.26, RT + 0.034), 'ST', 0.002)
part('DeckLift', dl, (0, 0, HD), 'Base', 'lifts z 0..+%.1f m (deck %.1f..%.1f m)' % (STROKE, HD, HD + STROKE))

bt = B()
for k, (nm, col, y) in enumerate((('Button_Up', 'G', -0.36), ('Button_Down', 'Y', -0.28))):
    b = B(); P = (DX - 0.10, y, RT - 0.048)
    b.lathe([(0.018, 0.0), (0.018, 0.012), (0.0, 0.014)], 'G' if k else 'RD', P, 'z', 16)
    part(nm, b, P, 'DeckLift', 'pushes down z 0..-0.006 m: hold = deck %s' % ('up' if k == 0 else 'down'))
    ik(nm, (P[0], P[1], P[2] + 0.014), 'hand', nm)
    ctrl('Deck ' + ('up' if k == 0 else 'down') + ' button', nm, nm, 'hold to move the deck %s (electro-hydraulic)' % ('up' if k == 0 else 'down'))

# deck extension with the fuselage bumper (slides +y), front gate (swings up about y on its left lug)
ex = B(); P = (0, DY - 0.07, HD - 0.009)
ex.box((-0.58, -0.40, HD - 0.015), (0.58, DY - 0.07, HD - 0.009), 'PL', 0.002)
ex.box((-0.58, DY - 0.09, HD - 0.07), (0.58, DY - 0.07, HD - 0.015), 'Y', 0.002)
ex.tube((-0.15, DY - 0.08, HD - 0.045), (0.15, DY - 0.08, HD - 0.045), 0.012, 'BK')
part('Deck_Extension', ex, P, 'DeckLift', 'slides along +y 0..0.40 m to the fuselage')
ik('Extension_Pull', (0, DY - 0.08, HD - 0.045), 'hand', 'Deck_Extension')
gt = B(); P = (-(DX + 0.03), DY + 0.0, HD + 0.01)
gt.tube((-(DX + 0.03), DY, HD + 0.01), (-(DX + 0.03), DY, RT), 0.021, 'Y')
gt.tube((-(DX + 0.03), DY, RT), (DX + 0.03, DY, RT), 0.021, 'Y'); gt.tube((-(DX + 0.03), DY, RM), (DX + 0.03, DY, RM), 0.018, 'Y')
gt.tube((DX + 0.03, DY, RM - 0.05), (DX + 0.03, DY, RT), 0.021, 'Y')
gt.lathe([(0.03, 0.0), (0.03, 0.08), (0.0, 0.08)], 'RB', (0, DY, RT), 'y', 18)
part('FrontGate', gt, P, 'DeckLift', 'swings about x on the left lug, up 0..85 deg to open the front')
ik('FrontGate', (0.5, DY, RT), 'hand', 'FrontGate')
ctrl('Front gate', 'FrontGate', 'FrontGate', 'lift to open the front at the door, lower to close')
ctrl('Deck extension', 'Deck_Extension', 'Extension_Pull', 'pull out 0..0.40 m until the bumper meets the skin')

# self-levelling stair (hinged at the deck back), its feet roll on the floor
HT = (0, -DY - 0.035, HD - 0.07); L = 4.6
ang = math.asin((HT[2] - 0.05) / L); run = L * math.cos(ang)
def sp(t): return Vector((0, HT[1] - run * t, HT[2] - (HT[2] - 0.05) * t))
sd = Vector((0, -math.cos(ang), -math.sin(ang)))
sr = B()
for sx in (-1, 1):
    sr.bar((sx * 0.47, HT[1], HT[2]), (sx * 0.47, HT[1] - run, 0.05), 0.012, 0.20, 'Y', up=(0, 1, 0))
    for t in (0.06, 0.5, 0.94):
        p = sp(t); sr.tube((sx * 0.485, p.y, p.z + 0.08), (sx * 0.485, p.y, p.z + 0.98), 0.019, 'Y')
    a, b = sp(0.06), sp(0.94)
    sr.tube((sx * 0.485, a.y, a.z + 0.98), (sx * 0.485, b.y, b.z + 0.98), 0.021, 'Y')
    sr.tube((sx * 0.485, a.y, a.z + 0.55), (sx * 0.485, b.y, b.z + 0.55), 0.018, 'Y')
sr.tube((-0.50, HT[1], HT[2]), (0.50, HT[1], HT[2]), 0.012, 'CR')
sr.tube((-0.518, HT[1] - run, 0.05), (0.518, HT[1] - run, 0.05), 0.012, 'CR')
part('Stair', sr, HT, 'DeckLift', 'hinge about x at the deck: %.1f deg (deck 2.5 m) .. %.1f deg (4.0 m); foot rolls on the floor'
     % (math.degrees(ang), math.degrees(math.asin((HT[2] + STROKE - 0.05) / L))))
tr = B(); tr.box((-0.455, -0.13, -0.035), (0.455, 0.13, 0.0), 'PL', 0.003); tr.tube((-0.468, 0, -0.02), (0.468, 0, -0.02), 0.01, 'CR')
TM = tr.mesh('DWS_Tread', (0, 0, 0))
NT = 12
for k in range(NT):
    p = sp((NT - k) / (NT + 1))
    part('Tread_%02d' % (k + 1), TM, (0, p.y, p.z + 0.05), 'Stair', 'turns about x on its pins: always minus the stair turn (stays level)')
    ik('Step_%02d' % (k + 1), (0, p.y, p.z + 0.05), 'foot', 'Tread_%02d' % (k + 1))
rl = B(); rl.lathe([(0.0, -0.025), (0.05, -0.025), (0.05, 0.025), (0.0, 0.025)], 'RB', (0, 0, 0), 'x', 20)
RLM = rl.mesh('DWS_StairRoller', (0, 0, 0))
for sx, tag in ((-1, 'L'), (1, 'R')):
    part('StairRoller_' + tag, RLM, (sx * 0.535, HT[1] - run, 0.05), 'Stair', 'spins about x as the stair foot rolls')
p = sp(0.5); ik('StairRail_L', (-0.485, p.y, p.z + 0.98), 'hand', 'Stair'); ik('StairRail_R', (0.485, p.y, p.z + 0.98), 'hand', 'Stair')

# tow bar (front), hand pump lever and lowering valve (base, right)
tb = B(); P = (0, BY + 0.04, ZB + 0.045)
tb.tube((-0.072, P[1], P[2]), (0.072, P[1], P[2]), 0.012, 'CR'); tb.tube(P, (0, P[1], P[2] + 0.95), 0.022, 'Y')
tb.tube((-0.11, P[1], P[2] + 0.92), (0.11, P[1], P[2] + 0.92), 0.02, 'BK')
part('TowBar', tb, P, 'Base', 'hinge about x: 0 = up (parked) .. 85 deg forward = towing')
ik('TowBar_L', (-0.08, P[1], P[2] + 0.92), 'push', 'TowBar'); ik('TowBar_R', (0.08, P[1], P[2] + 0.92), 'push', 'TowBar')
ctrl('Tow bar', 'TowBar', 'TowBar_R', 'lower forward to tow, lift up to park')
pl = B(); P = (0.69, -0.26, ZB + 0.35)
pl.tube((0.69, -0.334, ZB + 0.35), (0.69, -0.186, ZB + 0.35), 0.009, 'CR')
hd = Vector((-0.35, 0, 0.937)); e = Vector(P) + hd * 0.75
pl.tube(P, e, 0.016, 'ST'); pl.tube(e - hd * 0.16, e, 0.021, 'BK')
part('PumpLever', pl, P, 'Base', 'pumps about y -15..+35 deg: hand back-up to raise the deck')
ik('PumpLever', tuple(e - hd * 0.08), 'hand', 'PumpLever')
lv = B(); P = (0.686, -0.13, ZB + 0.16)
lv.lathe([(0.008, 0.0), (0.008, 0.03), (0.025, 0.03), (0.025, 0.05), (0.0, 0.05)], 'BK', P, 'x', 16)
part('LowerValve', lv, P, 'Base', 'turns about x 0..360 deg: open = deck comes down slowly')
ik('LowerValve', (0.74, -0.13, ZB + 0.16), 'hand', 'LowerValve')
ctrl('Hand pump lever (deck up)', 'PumpLever', 'PumpLever', 'pump: each stroke raises the deck (back-up)')
ctrl('Lowering valve (deck down)', 'LowerValve', 'LowerValve', 'turn open = deck comes down, close = holds')
ctrl('Stair', 'Stair', 'StairRail_R', 'follows the deck by itself; its foot rolls')

ik('Deck', (0, 0, HD), 'stand', 'DeckLift')
ik('DeckRail_L', (-(DX - 0.02), 0.2, RT), 'hand', 'DeckLift'); ik('DeckRail_R', (DX - 0.02, 0.2, RT), 'hand', 'DeckLift')
ik('Push_L', (-(BX + 0.02), 0, 1.05), 'push', 'Base'); ik('Push_R', (BX + 0.02, 0, 1.05), 'push', 'Base')
finish('DoorWorkStand', 'fuselage / door work stand (adjustable)',
       {'deck_min_m': HD, 'deck_max_m': HD + STROKE, 'deck_m': [2 * DX, 2 * DY], 'rail_m': 1.1, 'stair_length_m': L,
        'stair_deg_low': round(math.degrees(ang), 1), 'treads': NT, 'castors_mm': 250, 'extension_m': 0.40},
       {'x_min': -(BX + 0.09), 'x_max': BX + 0.09, 'y_min': round(HT[1] - run - 0.06, 2), 'y_max': BY + 0.07,
        'stair_foot_y_at_4m': round(HT[1] - L * math.cos(math.asin((HT[2] + STROKE - 0.05) / L)) - 0.06, 2),
        'note': 'the stair foot moves with the deck height; tow bar down reaches y %.2f' % (BY + 1.0)},
       'Wikimedia Commons: 374 AW MXS, AMXS perform engine maintenance operations in Sri Lanka (public domain); A320 door sill heights; published docking-platform sizes')
