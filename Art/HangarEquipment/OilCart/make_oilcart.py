# Oil / fluids cart (to-do 364 A, batch 2), our own model, no brands. No usable free photo found: built from published
# sizes of hangar fluid dispensing carts: a welded steel cart on four braked swivel castors, a bunded drip tray that
# holds two 60 l drums (390 mm across, 580 mm high) behind a welded restraint rail, a lever drum pump in the rear drum's
# bung, its outlet hose to the swivel of a hand-cranked hose reel on welded stands, the reel's hose to a dispensing
# nozzle that sits in a holster on the push handle, a screw cap on the front drum's bung.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_joints import *

reset(); detail_materials('OIL')
R = 0.10; CT = 2 * R + 0.07; Z0 = CT + 0.004; ZR = Z0 + 0.05      # castor plate top, rails bottom / top (50x50 tube)
ZB = ZR + 0.002; TOP = ZB + 0.58                                    # drum bottom (1 mm into the tray floor), drum lid
DY = (-0.08, 0.33)                                                  # drum centres (x = 0)
H = ZR + 0.45                                                       # restraint rail height
f = D()
# chassis: side rails, end and middle cross members, rear tread deck, castor pads
for s in (-1, 1):
    f.box((s * 0.30, -0.65, Z0), (s * 0.35, 0.60, ZR), 'Y', 0.004)
    f.label((s * 0.3506, 0.25, Z0 + 0.025), (s, 0, 0), 0.16, 0.035, 'LW')
for y0, y1 in ((-0.65, -0.60), (-0.025, 0.025), (0.55, 0.60)):
    f.box((-0.301, y0, Z0 + 0.002), (0.301, y1, ZR - 0.002), 'Y', 0.004)
f.box((-0.35, -0.65, ZR - 0.001), (0.35, -0.312, ZR + 0.005), 'AL', 0.002)                # rear deck, tread plate
for k in range(9):
    for j in range(4):
        x, y = -0.30 + 0.075 * k, -0.62 + 0.08 * j
        f.box((x - 0.012, y - 0.003, ZR + 0.004), (x + 0.012, y + 0.003, ZR + 0.0065), 'AL', 0.001)
# bunded drip tray: floor, walls with a folded lip, drain plug, label
f.box((-0.345, -0.31, ZR - 0.004), (0.345, 0.565, ZR + 0.003), 'G', 0.002)
for lo, hi in (((-0.345, -0.31, ZR), (-0.341, 0.565, ZR + 0.09)), ((0.341, -0.31, ZR), (0.345, 0.565, ZR + 0.09)),
               ((-0.345, -0.31, ZR), (0.345, -0.306, ZR + 0.09)), ((-0.345, 0.561, ZR), (0.345, 0.565, ZR + 0.09))):
    f.box(lo, hi, 'G', 0.001)
for s in (-1, 1):
    f.box((s * 0.345 - (0.012 if s > 0 else 0), -0.31, ZR + 0.087), (s * 0.345 + (0.0 if s > 0 else 0.012), 0.565, ZR + 0.092), 'G', 0.001)
    f.box((-0.345, (0.565 - 0.012) if s > 0 else -0.31, ZR + 0.087), (0.345, 0.565 if s > 0 else -0.298, ZR + 0.092), 'G', 0.001)
f.bolt((0.25, 0.48, ZR + 0.003), (0, 0, 1), 0.012, 'BR')                                   # drain plug
f.label((0, 0.5656, ZR + 0.05), (0, 1, 0), 0.14, 0.05, 'LI')
# two 60 l drums: rolled chimes, two rolling hoops, a painted band, bungs
for k, y in enumerate(DY):
    M = frame((0, y, 0), (0, 0, 1))
    prof = [(0.0, ZB), (0.186, ZB), (0.195, ZB + 0.01), (0.195, ZB + 0.18), (0.199, ZB + 0.19), (0.195, ZB + 0.2),
            (0.195, ZB + 0.38), (0.199, ZB + 0.39), (0.195, ZB + 0.4), (0.195, TOP - 0.004), (0.198, TOP + 0.008),
            (0.192, TOP + 0.013), (0.188, TOP + 0.004), (0.186, TOP), (0.0, TOP)]
    f.lz(prof, ('BL', 'OR')[k], M, 40)
    f.lz([(0.1957, ZB + 0.24), (0.1957, ZB + 0.34)], 'WH', M, 40, caps=False)
    f.lz([(0.03, TOP - 0.002), (0.03, TOP + 0.006), (0.0, TOP + 0.006)], 'CR', frame((0.09, y, 0), (0, 0, 1)), 16)    # main bung
    f.lz([(0.016, TOP - 0.002), (0.016, TOP + 0.01), (0.0, TOP + 0.01)], 'RD', frame((-0.1, y + 0.05, 0), (0, 0, 1)), 6)  # vent plug
# restraint rail: four posts welded into the tray floor, rails around the drums, capped post tops
PX, PY = 0.215, (-0.29, 0.545)
for sx in (-1, 1):
    for py in PY:
        f.tube((sx * PX, py, ZR - 0.0025), (sx * PX, py, H + 0.02), 0.016, 'Y', 14)
        cap(f, (sx * PX, py, H + 0.02), (0, 0, 1), 0.016, 'Y')
    f.tube((sx * PX, PY[0] + 0.0105, H), (sx * PX, PY[1] - 0.0105, H), 0.014, 'Y', 12, weld=False)
    for py in PY:
        f.weld((sx * PX - 0.012, py - math.copysign(0.0105, py) + 0.0, H + 0.012), (sx * PX + 0.012, py - math.copysign(0.0105, py), H + 0.012), 'Y', 0.0025)
for py in PY:
    f.tube((-PX + 0.0105, py, H), (PX - 0.0105, py, H), 0.014, 'Y', 12, weld=False)
# push handle: raked uprights welded to the side rails, a cross bar with grips, gussets, capped tops
for s in (-1, 1):
    f.tube((s * 0.325, -0.625, ZR - 0.0055), (s * 0.325, -0.70, 1.0), 0.016, 'Y', 14)
    cap(f, (s * 0.325, -0.70, 1.0), Vector((0, -0.075, 0.676)).normalized(), 0.016, 'Y')
    gusset(f, (s * 0.325, -0.605, ZR), (0, 1, 0), (0, 0, 1), 0.07, 0.006, 'Y')
    f.tube((s * 0.31, -0.697, 0.98), (s * 0.20, -0.697, 0.98), 0.0185, 'BK', 14, weld=False)
f.tube((-0.3145, -0.697, 0.98), (0.3145, -0.697, 0.98), 0.014, 'Y', 12, weld=False)
# hose reel stands: plates welded to the deck, gussets, bearing collars, the swivel housing on the left
RC = (0.0, -0.48, ZR + 0.20)
for s in (-1, 1):
    f.box((s * 0.126, -0.515, ZR + 0.003), (s * 0.134, -0.445, RC[2] + 0.035), 'Y', 0.002)
    for gy in (-1, 1):
        gusset(f, (s * 0.13, -0.48 + gy * 0.035, ZR + 0.005), (0, gy, 0), (0, 0, 1), 0.05, 0.006, 'Y')
    f.lz([(0.028, 0.0), (0.028, 0.016), (0.0, 0.016)], 'ST', frame((s * 0.1325, RC[1], RC[2]), (s, 0, 0)), 18)
f.lz([(0.024, 0.0), (0.024, 0.042), (0.0, 0.042)], 'BR', frame((-0.148, RC[1], RC[2]), (-1, 0, 0)), 6)
# lever drum pump in the rear drum's bung: adapter, barrel, head, clevis for the lever, outlet hose to the reel swivel
PB = (0.09, DY[0])
f.lz([(0.024, TOP + 0.004), (0.024, TOP + 0.03), (0.0, TOP + 0.03)], 'ST', Matrix.Translation((PB[0], PB[1], 0)), 6)
f.tube((PB[0], PB[1], TOP + 0.028), (PB[0], PB[1], TOP + 0.292), 0.02, 'RD', 16, weld=False)
f.box((PB[0] - 0.035, PB[1] - 0.035, TOP + 0.29), (PB[0] + 0.035, PB[1] + 0.035, TOP + 0.36), 'RD', 0.006)
for s in (-1, 1):
    f.box((PB[0] + s * 0.018, PB[1] - 0.075, TOP + 0.315), (PB[0] + s * 0.024, PB[1] - 0.032, TOP + 0.36), 'RD', 0.001)
LP = (PB[0], PB[1] - 0.06, TOP + 0.34)
pin(f, LP, (1, 0, 0), 0.006, 0.07)
f.fitting((PB[0] - 0.05, PB[1], TOP + 0.2), (-1, 0, 0), 0.011)
f.tube((PB[0] - 0.05, PB[1], TOP + 0.2), (PB[0] - 0.018, PB[1], TOP + 0.2), 0.008, 'BR', 10, weld=False)
f.hose([(PB[0] - 0.095, PB[1], TOP + 0.2), (-0.08, -0.2, TOP + 0.13), (-0.2, -0.33, TOP - 0.08),
        (-0.205, -0.43, RC[2] + 0.08), (-0.169, RC[1], RC[2] + 0.02)], 0.011, 'HS')
# nozzle holster: a cup on a bracket welded to the left upright
NX, NY = -0.395, -0.67
f.box((-0.366, NY - 0.012, 0.75), (-0.336, NY + 0.012, 0.79), 'Y', 0.002)
f.lz([(0.025, 0.0), (0.025, 0.08), (0.019, 0.08), (0.019, 0.012), (0.0, 0.012)], 'BK', frame((NX, NY, 0.74), (0, 0, 1)), 18)
part('Frame', f, (0, 0, 0), None, 'root: the cart, pushed on its castors')
for tag, x, y in (('FL', -0.325, 0.48), ('FR', 0.325, 0.48), ('RL', -0.325, -0.55), ('RR', 0.325, -0.55)):
    castor2(tag, x, y, 'Frame', R, 0.05, CT + 0.005)
ik('Push_L', (-0.25, -0.697, 0.98), 'push', 'Frame'); ik('Push_R', (0.25, -0.697, 0.98), 'push', 'Frame')
ik('Operator', (0, -1.15, 0), 'stand', 'Frame')

# hose reel: axle through the collars, drum, flanges, the hose wound on, crank with a turning knob
hr = D()
hr.tube((-0.17, RC[1], RC[2]), (0.172, RC[1], RC[2]), 0.015, 'CR', 14, weld=False)
hr.tube((-0.1, RC[1], RC[2]), (0.1, RC[1], RC[2]), 0.07, 'RD', 28, weld=False)
for s in (-1, 1):
    hr.lz([(0.016, 0.0), (0.16, 0.0), (0.16, 0.005), (0.016, 0.005)], 'RD', frame((s * 0.095, RC[1], RC[2]), (s, 0, 0)), 32)
coil(hr, RC, 'x', 0.082, 0.012, 7, 'HS')
hr.box((0.168, RC[1] - 0.012, RC[2] - 0.012), (0.18, RC[1] + 0.012, RC[2] + 0.1), 'RD', 0.003)
hr.tube((0.179, RC[1], RC[2] + 0.09), (0.25, RC[1], RC[2] + 0.09), 0.013, 'BK', 12, weld=False)
part('HoseReel', hr, RC, 'Frame', 'turns about x: the crank winds the 6 m hose back in')
ik('ReelCrank', (0.235, RC[1], RC[2] + 0.09), 'hand', 'HoseReel')
ctrl('Reel crank', 'HoseReel', 'ReelCrank', 'wind the hose back onto the reel after use')

# pump lever: boss on the clevis pin, bar back toward the operator, grip
pl = D()
pl.tube((LP[0] - 0.0155, LP[1], LP[2]), (LP[0] + 0.0155, LP[1], LP[2]), 0.012, 'CR', 14, weld=False)
pl.tube((LP[0], LP[1] - 0.005, LP[2]), (LP[0], LP[1] - 0.33, LP[2] + 0.06), 0.0105, 'CR', 12, weld=False)
pl.tube((LP[0], LP[1] - 0.24, LP[2] + 0.044), (LP[0], LP[1] - 0.345, LP[2] + 0.063), 0.016, 'BK', 14, weld=False)
part('PumpLever', pl, LP, 'Frame', 'rocks about x 0..30 deg: each stroke pumps about 0.3 l into the hose')
ik('PumpLever', (LP[0], LP[1] - 0.3, LP[2] + 0.055), 'hand', 'PumpLever')
ctrl('Pump lever', 'PumpLever', 'PumpLever', 'pump oil from the drum to the nozzle')

# drum cap: screw cap on the front drum's bung (ribbed)
dc = D(); CP = (0.09, DY[1], TOP + 0.0045)
dc.lz([(0.026, 0.0), (0.026, 0.016), (0.022, 0.019), (0.0, 0.019)], 'BK', frame(CP, (0, 0, 1)), 12)
dc.box((CP[0] - 0.004, CP[1] - 0.02, CP[2] + 0.018), (CP[0] + 0.004, CP[1] + 0.02, CP[2] + 0.03), 'BK', 0.002)
part('DrumCap', dc, CP, 'Frame', 'unscrews about z and lifts off: open to top up or check the level')
ik('DrumCap', (CP[0], CP[1], CP[2] + 0.03), 'hand', 'DrumCap')
ctrl('Drum cap', 'DrumCap', 'DrumCap', 'open the front drum')

# nozzle: the gun with spout, grip and trigger, its hose back to the reel
nz = D()
nz.tube((NX, NY, 0.749), (NX, NY, 0.866), 0.008, 'CR', 12, weld=False)
nz.box((NX - 0.016, NY - 0.135, 0.86), (NX + 0.016, NY + 0.012, 0.895), 'BK', 0.006)
nz.box((NX - 0.014, NY - 0.135, 0.79), (NX + 0.014, NY - 0.105, 0.862), 'BK', 0.005)
nz.box((NX - 0.005, NY - 0.09, 0.835), (NX + 0.005, NY - 0.07, 0.862), 'RD', 0.002)
nz.hose([(NX, NY - 0.13, 0.878), (-0.37, -0.85, 0.80), (-0.2, -0.74, 0.66), (-0.05, -0.56, 0.64), (0.0, RC[1], 0.627)], 0.011, 'HS')
part('Nozzle', nz, (NX, NY - 0.12, 0.83), 'Frame', 'lifted out of the holster by its grip; the hose pays off the reel')
ik('Nozzle', (NX, NY - 0.12, 0.83), 'hand', 'Nozzle')
ctrl('Nozzle', 'Nozzle', 'Nozzle', 'take the nozzle to the engine filler and squeeze the trigger')
finish('OilCart', 'oil / fluids dispensing cart',
       {'drums': '2 x 60 l (390 x 580 mm)', 'tray_l': 50, 'hose': '6 m on a hand-crank reel', 'pump': 'lever drum pump, 0.3 l per stroke',
        'deck_m': [0.70, 1.25], 'castors_mm': 200},
       {'x_min': -0.43, 'x_max': 0.36, 'y_min': -0.75, 'y_max': 0.62, 'note': 'pushed by the handle at -y; operator stands behind it'},
       'No usable free photo found; published sizes of hangar oil dispensing carts and 60 l steel drums')
