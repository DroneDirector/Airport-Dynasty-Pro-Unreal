# The apron bus's wheel housings (owner 2026-11-25: the wheel came through the housing, inside and outside): closed shells, wide
# enough for the wheel steered 30 degrees and the body kneeling, over wells in the floor; the steps before them moved in with them.
# Used by Art/ApronBus/fix_housings.py (the finished bus) and Tools/make_apron_bus.py (a full rebuild). Metres, x forward, y left.
import vehicle_kit as K
FX, RX, FLOOR, TOP, T = 3.76, -3.34, 0.33, 1.08, 0.025
X = {'F': (3.012, 4.509), 'R': (-4.149, -2.532)}
IN = {'F': 0.75, 'R': 0.715}      # the housing's inner face
STEP_IN = 0.665                   # (clear of the poles at y 0.62)
OUT = 1.452                       # (ends in the skirt)


def wells(*floors):
    for f in floors:
        for nm in 'FR':
            for s in (1, -1):
                a, b = X[nm][0] + T, X[nm][1] - T; y0 = IN[nm] + T
                K.cut(f, K.box('WellCut', ((a + b) / 2, s * (y0 + 1.6) / 2, 0.25), (b - a, 1.6 - y0, 0.4), f.data.materials[0]))


def housing(nm, s, m):
    x0, x1 = X[nm]; y0 = IN[nm]
    h = K.box(f'WheelHousing_{nm}{"L" if s > 0 else "R"}', ((x0 + x1) / 2, s * (y0 + OUT) / 2, (FLOOR + TOP) / 2), (x1 - x0, OUT - y0, TOP - FLOOR), m, bevel=0.02, segs=2)
    K.cut(h, K.box('HousingCut', ((x0 + x1) / 2, s * (y0 + T + 1.7) / 2, (TOP - T) / 2), (x1 - x0 - 2 * T, 1.7 - y0 - T, TOP - T), m))
    return h


def step(nm, s, m):
    x0, x1 = X[nm]; ax = FX if nm == 'F' else RX
    return K.box(f'HousingStep_{ax:+.0f}{s:+d}', ((x0 + x1) / 2, s * (STEP_IN + IN[nm] + 0.004) / 2, FLOOR + 0.1175), (x1 - x0, IN[nm] + 0.004 - STEP_IN, 0.237), m, bevel=0.02)


def flap(sd, m):      # (the rear mud flap stood in the cabin, behind the housing)
    s = 1 if sd == 'L' else -1
    return K.box(f'MudFlap_{sd}R', (-4.10, s * 1.30, 0.40), (0.012, 0.30, 0.34), m)
