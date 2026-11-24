# Terminal east, part 4: the closed escape stairs' numbers (sheet 9c, Blueprints/Terminal/Changes/v2/s9_escape.py), shared by
# make_escape_stairs.py (the stair boxes), make_terminal_east.py (the slab's openings, the balustrade's and the promenade wall's
# gaps, the ceilings and lids round the boxes) and make_groundfloor.py (the rooms' finishes and skirtings round them). Plan metres.
# The 11 boxes x 246 + 30 k, 2.6 m wide (x +-1.3), concrete walls 20 cm: inside 2.2 m, the stair 2.04 m, 2.0 m between the rails.
# Sheet 9c draws them z 7..21.5 on the ground floor and z 14..21.5 at Floor 2; built here from the promenade wall's face (z 6.13)
# so the foot's door is the promenade wall's own doorway (STAIR_DOORS) and no 0.9 m strip of hall is left between them; where
# there is no promenade wall (the coach station, x 486 516 546) the box stands free with its door to the hall's front.
STAIRS = [246.0 + 30.0 * k for k in range(11)]
HW, WT = 1.3, 0.2                     # half its width outside, its walls
Z0, ZT, Z1 = 6.13, 13.8, 21.5         # its south face, where it steps up to its tall part (Floor 2: 14..21.5), its north face
LOW, HIGH = 5.44, 8.7                 # its roof: the low part level with the staff rooms' lids, the tall part 2.7 m over Floor 2
DOOR_W, DOOR_H, SIDE_W = 1.8, 2.25, 1.2


def rect(x, pad=0.0):
    return (x - HW - pad, x + HW + pad, Z0 - pad, Z1 + pad)


def promenade_doors():                # the doorways in the promenade wall (z 6, x 228..470)
    return [x for x in STAIRS if 228.0 < x < 470.0]


def cut(x0, x1, z0, z1, pad=0.01):
    """The rectangle x0..x1, z0..z1 less the stair boxes' footprints (pad off them): a list of rectangles."""
    hs = [(max(x0, a), min(x1, b), max(z0, c), min(z1, d)) for a, b, c, d in (rect(x, pad) for x in STAIRS)]
    hs = [h for h in hs if h[0] < h[1] and h[2] < h[3]]
    if not hs: return [(x0, x1, z0, z1)]
    xs = sorted({x0, x1} | {v for h in hs for v in h[:2]}); zs = sorted({z0, z1} | {v for h in hs for v in h[2:]})
    out = []
    for a, b in zip(xs, xs[1:]):
        for c, d in zip(zs, zs[1:]):
            if not any(h[0] <= a and b <= h[1] and h[2] <= c and d <= h[3] for h in hs): out.append((a, b, c, d))
    return out


def gaps(x0, x1, pad=0.02):
    """The span x0..x1 less the boxes' widths: for the balustrade and the slab's edge (z 20)."""
    sp = [(x0, x1)]
    for x in STAIRS:
        a, b = x - HW - pad, x + HW + pad
        sp = [s for p, q in sp for s in ((p, min(q, a)), (max(p, b), q)) if s[1] - s[0] > 1e-3]
    return sp
