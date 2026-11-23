# The terminal's one continuous wave roof (owner, 2026-11-23: "when you smooth it out, make it look exactly like the good
# wave roof parts"): its shape in numbers, in the terminal plan's metres (x east, z north, y up) - used by
# Art/TerminalAnnex/make_wave_roof.py (the models) and Tools/terminal_edit.py (where everything goes).
#
# The good part is the Grand Hall's own roof (Airline-Manager-Pro-Babylon, plan.ts waveH / waveSouth): a 30 m cosine wave,
# 3.4 m up and down, falling 6 mm per m2 to both sides of z 16, its front edge a bow (z -16 in the middle, -7 at the ends)
# over each three waves. The roof used to be 8 copies of that 102 m piece, set end to end and partly squeezed (82 %) or
# stretched (105 %): each ends 6 m past a valley, rising, so two copies met in an upward point, and their bows in an
# inward notch, at x -161.5, -78, 24, 126, 233.3, 340.6 and 447.9.
# Now: the same bow every 90 m (three waves exactly), so wave, bow, skylights and downlights all carry on in their own
# rhythm and meet in the valleys, where everything is level. Over 36 m each side of a bow's middle the shape is the good
# part's to the millimetre (the Grand Hall, x 36..114, is not changed at all); in the 9 m to the valley the bow runs out
# in one smooth curve instead of a notch. At the two real ends of the building the roof ends as the good part always did:
# 6 m past the valley, the edge coming in to z -7.
import math

CX = 75.0                # the Grand Hall's bow
BAY = 90.0               # three waves
BOWS = [CX + BAY * k for k in range(-3, 6)]              # -195 .. 525
X0, X1 = BOWS[0] - 51.0, BOWS[-1] + 51.0                 # -246 .. 576: the roof's ends (before: -245.1 .. 555.2)
Z1, THICK = 34.0, 0.7


def s5(t):
    t = min(1.0, max(0.0, t)); return t * t * t * (10 - 15 * t + 6 * t * t)


def droop(u, end):
    """the good part's 34 cm fall towards a free end; inside the roof only its first 1.4 cm (u 36..39, so the Grand Hall's
    glass fits as before), gone again before the valley"""
    t = abs(u) - 36.0
    if t <= 0: return 0.0
    d = 0.0015 * t * t
    return d if end else d * (1.0 - s5((t - 3.0) / 6.0))


def bow(u, end):
    """the front edge (z) at u metres from a bow's middle"""
    a = abs(u)
    if a <= 36.0 or end: return -7.0 - 9.0 * max(0.0, 1.0 - (a / 51.0) ** 2)
    # from 36 to the valley at 45: value, slope and curvature carried on, level at 45 (a smooth run-out, symmetric)
    z36, m, k, L = -7.0 - 9.0 * (1.0 - (36.0 / 51.0) ** 2), 18.0 * 36.0 / 2601.0, 18.0 / 2601.0, 9.0
    t = a - 36.0
    # slope q(t) = m + k t + c2 t2 + c3 t3 with q(L) = 0 and q''(L) = 0 (the next bow is its mirror image)
    # 2 c2 + 6 c3 L = 0  ->  c2 = -3 c3 L ;  m + k L - 3 c3 L^3 + c3 L^3 = 0
    c3 = (m + k * L) / (2 * L ** 3); c2 = -3 * c3 * L
    return z36 + m * t + k * t * t / 2 + c2 * t ** 3 / 3 + c3 * t ** 4 / 4


def at(x):
    """(u, end) of a place along the roof: metres from its bow's middle; end: on the free side of the last bow"""
    k = min(range(len(BOWS)), key=lambda i: abs(x - BOWS[i])); u = x - BOWS[k]
    return u, (k == 0 and u < 0) or (k == len(BOWS) - 1 and u > 0)


def h_u(u, z, end=False): return 18.4 + 3.4 * math.cos(2 * math.pi * u / 30.0) - 0.006 * (z - 16.0) ** 2 - droop(u, end)
def height(x, z): u, e = at(x); return h_u(u, z, e)                       # the underside
def south(x): u, e = at(x); return bow(u, e)


def downlights():
    """the good part's own: every 6 m from x 28, z -12..30, 2 m in from the edge, not under the skylights"""
    out = []
    x = 28.0 + 6.0 * math.ceil((X0 + 3.99 - 28.0) / 6.0)
    while x < X1 - 3.0:
        z = -12.0
        while z < Z1 - 2.0:
            c = (x - 45.0 + 15.0) % 30.0 - 15.0                           # metres from the nearest crest (45 + 30 k)
            if z >= south(x) + 2.0 and not (abs(c) < 3.0 and 3.0 < z < 29.0): out.append((x, height(x, z) - 0.03, z))
            z += 6.0
        x += 6.0
    return out


def skylights(): return [b + d for b in BOWS for d in (-30.0, 0.0, 30.0)]


# the kerb's tree pillars: the good part's own two (C-7 and C-8, 19 m each side of a bow's middle, z -9) under every bow, so
# every head meets the roof exactly as in the good part
PILLARS = [(('C-7', 'C-8')[s > 0], b + s) for b in BOWS for s in (-19.0, 19.0)]


if __name__ == '__main__':
    def old(x, z): return 18.4 + 3.4 * math.cos(2 * math.pi * (x - 75) / 30) - 0.006 * (z - 16) ** 2 - 0.0015 * max(0, abs(x - 75) - 36) ** 2
    worst = max(abs(height(36 + .25 * i, z) - old(36 + .25 * i, z)) for i in range(313) for z in range(-16, 35))
    print('Grand Hall x 36..114: worst change %.6f m' % worst, '| x 24..126: %.3f m' % max(abs(height(24 + .25 * i, z) - old(24 + .25 * i, z)) for i in range(409) for z in (0, 34)))
    e = 1e-4
    js = [(abs(height(x + e, z) - 2 * height(x, z) + height(x - e, z)) / e, abs(south(x + e) - 2 * south(x) + south(x - e)) / e) for x in [X0 + 1 + .5 * i for i in range(1640)] for z in (0,)]
    print('largest crease anywhere (slope jump): roof %.5f, edge %.5f' % (max(j[0] for j in js), max(j[1] for j in js)))
    for b in (30.0, 120.0): print('valley x', b, 'edge z %.2f' % south(b), 'underside at the edge %.2f' % height(b, south(b)))
    print('ends', X0, X1, 'edge %.2f' % south(X0), 'downlights', len(downlights()), 'skylights', len(skylights()))
