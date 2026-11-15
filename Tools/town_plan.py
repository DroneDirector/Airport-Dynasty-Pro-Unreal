# The owner's town plan (Art/Maps/Town-Plan-v7.png, north up, 0.1728 m a pixel) placed on the game's plan, and a tracer
# for its streets (owner, 2026-11-15: "build the rest of the roads"). The plan's frame is the town box on the overview
# drawing (Art/Maps/Airport-Road-Town-Overview-v11.png, 1738 x 1438 m, measured: Tools/blueprint_grid.py's overview
# calibration), so a town pixel goes to the overview's pixels and from there to plan metres - the road from the airport
# traced off the overview meets the town's roundabout (9) where the town plan has it.
#   python Tools/town_plan.py <name> <start u> <start v> <toward u> <toward v> <stop u> <stop v>   (town plan pixels)
# writes the traced street (plan metres) into Data/road-traces.json under 'town-<name>'.
import json, math, os, sys
os.environ['ADP_MAP'] = 'overview'
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blueprint_grid as B
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'Maps', 'Town-Plan-v7.png')
# the town plan's frame (its border line's middle, town pixels) and the overview's town box (overview pixels)
FRAME = (237.0, 171.5, 10301.0, 8485.5)
BOX = (9926.0, 4723.0, 12047.0, 6478.0)
KX = (BOX[2] - BOX[0]) / (FRAME[2] - FRAME[0]); KY = (BOX[3] - BOX[1]) / (FRAME[3] - FRAME[1])
MPX = KX / B.PXM          # metres a town pixel (about 0.1728)

def to_plan(u, v):
    return B.to_plan(BOX[0] + (u - FRAME[0]) * KX, BOX[1] + (v - FRAME[1]) * KY)

def to_town(x, y):
    ix, iy = B.to_img(x, y)
    return FRAME[0] + (ix - BOX[0]) / KX, FRAME[1] + (iy - BOX[1]) / KY

_IM = None
def image():
    global _IM
    if _IM is None: _IM = Image.open(SRC).convert('RGB')
    return _IM

def is_road(c):
    # the town's asphalt (74, 78, 84), its yellow middle line, white dashes and edge lines
    return (abs(c[0] - 74) <= 9 and abs(c[1] - 78) <= 9 and abs(c[2] - 84) <= 9) or (c[0] > 220 and c[1] > 190 and c[2] < 140) or (c[0] > 225 and c[1] > 225 and c[2] > 225)

def trace(start, toward, stop, step_m=6.0, half_m=9.0, max_steps=3000):
    im = image(); W, H = im.size; px = im.load()
    x, y = start; dx, dy = toward[0] - x, toward[1] - y; n = math.hypot(dx, dy); dx, dy = dx / n, dy / n
    step = step_m / MPX; half = int(half_m / MPX); pts = [to_plan(x, y)]; lost = 0
    for _ in range(max_steps):
        nx, ny = x + dx * step, y + dy * step
        px_, py_ = -dy, dx; hits = []
        for t in range(-half, half + 1, 2):
            qx, qy = int(round(nx + px_ * t)), int(round(ny + py_ * t))
            if 0 <= qx < W and 0 <= qy < H and is_road(px[qx, qy]): hits.append(t)
        if len(hits) >= 6:
            runs, cur = [], [hits[0]]
            for h in hits[1:]:
                if h - cur[-1] <= 4: cur.append(h)
                else: runs.append(cur); cur = [h]
            runs.append(cur)
            run = min(runs, key=lambda r: abs(sum(r) / len(r)))
            off = sum(run) / len(run)
            nx, ny = nx + px_ * off, ny + py_ * off; lost = 0
        else:
            lost += 1
            if lost > 6: print('# lost the street at town px', int(nx), int(ny)); break
        ndx, ndy = nx - x, ny - y; m = math.hypot(ndx, ndy)
        if m > 0: dx, dy = 0.6 * dx + 0.4 * ndx / m, 0.6 * dy + 0.4 * ndy / m; mm = math.hypot(dx, dy); dx, dy = dx / mm, dy / mm
        x, y = nx, ny
        if lost == 0: pts.append(tuple(round(v, 2) for v in to_plan(x, y)))
        if math.hypot(x - stop[0], y - stop[1]) < step * 1.2: break
    return pts

if __name__ == '__main__':
    name = sys.argv[1]; a = [float(v) for v in sys.argv[2:8]]
    p = trace((a[0], a[1]), (a[2], a[3]), (a[4], a[5]))
    f = os.path.join(ROOT, 'Data', 'road-traces.json'); T = json.load(open(f, encoding='utf-8'))
    T['town-' + name] = p; json.dump(T, open(f, 'w', encoding='utf-8'), indent=1)
    print('town-%s: %d points, %.0f m' % (name, len(p), sum(math.dist(p[i], p[i + 1]) for i in range(len(p) - 1))))


# ---- the rest of the town's streets (owner, 2026-11-15: "build the road until it's fully completed") ----
# Each street: rough waypoints read off the plan (town px), its two ends ('host' road it meets, 'bulb' = a turning circle
# at that end, None = open). trace_via() follows the drawn asphalt through them, kept on its middle.
STREETS = {
    'loop-n': [(2200, 4180), (2200, 3600), (2240, 3000), (2400, 2400), (2640, 2000), (3080, 1680), (3800, 1432), (4600, 1368), (5340, 1500), (6000, 1860), (6380, 2320), (6460, 2800), (6300, 3600), (6180, 4160)],
    'loop-s': [(2160, 4300), (2060, 5000), (2160, 6000), (2360, 6480), (2600, 6800), (3080, 7120), (3800, 7300), (4600, 7340), (5320, 7240), (6000, 6880), (6360, 6480), (6540, 5600), (6500, 4800), (6280, 4160)],
    'midbogi': [(3940, 1420), (3980, 2320), (4100, 2680), (4600, 2760), (5000, 2760), (5200, 2580), (5320, 1600), (5340, 1512)],
    'nw-a': [(2200, 3140), (1800, 2800), (1340, 2460)],
    'nw-b': [(2640, 1992), (2920, 2200), (3120, 2600), (3200, 2940)],
    'ne-a': [(6420, 3040), (6700, 2960), (7100, 2680), (7380, 2360)],
    'torg': [(3740, 4240), (3800, 3520), (3940, 3340), (4600, 3312), (5400, 3280), (5540, 3420), (5640, 4160)],
    'skola': [(3500, 4380), (3400, 5320), (3560, 5580), (4240, 5620)],
    'skola-oneway': [(4240, 5620), (4900, 5580), (5480, 5530)],
    'sw-a': [(2580, 6760), (2080, 7080), (1560, 7320)],
    'sw-b': [(2580, 6760), (2880, 6400), (3088, 5888)],
    'sudur-u': [(3840, 7300), (3880, 6240), (4040, 6000), (4600, 5940), (5080, 6000), (5260, 6240), (5340, 7160), (5340, 7240)],
    'east-loop': [(9004, 4324), (9020, 4600), (8940, 5480), (8620, 5880), (8100, 6060), (7740, 5800), (7380, 5000), (7420, 4120)],
    'east-s': [(8100, 6060), (8140, 6400), (8220, 7000), (8272, 7320)],
    'house': [(8500, 4040), (8600, 3400), (8700, 3000), (8660, 2720), (8592, 2360)],
}


def snap(x, y, dx, dy, half_m=10.0, max_off=6.0, max_w=9.5):
    """The drawn street's middle across (dx, dy) at a town pixel (None if no clean street there). The drawing's asphalt
    is about 6-7 m wide: a wider run is a junction or a turning circle (its middle pulls toward the side street) and is
    skipped, as is a run further than `max_off` m off - the march keeps its line there."""
    im = image(); W, H = im.size; px = im.load(); half = int(half_m / MPX); nx, ny = -dy, dx; hits = []
    for t in range(-half, half + 1):
        qx, qy = int(round(x + nx * t)), int(round(y + ny * t))
        if 0 <= qx < W and 0 <= qy < H and is_road(px[qx, qy]): hits.append(t)
    if len(hits) < 10: return None
    runs, cur = [], [hits[0]]
    for h in hits[1:]:
        if h - cur[-1] <= 4: cur.append(h)
        else: runs.append(cur); cur = [h]
    runs.append(cur)
    run = min(runs, key=lambda r: abs((r[0] + r[-1]) / 2))
    o = (run[0] + run[-1]) / 2; w = (run[-1] - run[0]) * MPX
    if w < 4.0 or w > max_w or abs(o) * MPX > max_off: return None
    return x + nx * o, y + ny * o


def trace_via(wps, step_m=4.0):
    """A street through its waypoints, each step put on the drawn asphalt's middle (plan m)."""
    step = step_m / MPX
    # each waypoint onto the drawn street's middle first (square to its leg)
    W2 = []
    for i, (wx, wy) in enumerate(wps):
        a_, b_ = wps[max(0, i - 1)], wps[min(len(wps) - 1, i + 1)]
        lx, ly = b_[0] - a_[0], b_[1] - a_[1]; L = math.hypot(lx, ly)
        s_ = snap(wx, wy, lx / L, ly / L, max_off=9.0, max_w=40.0 if i in (0, len(wps) - 1) else 9.5)
        W2.append(s_ if s_ else (wx, wy))
    wps = W2
    out = [wps[0]]; x, y = wps[0]
    for (ax, ay), (bx, by) in zip(wps[:-1], wps[1:]):
        lx, ly = bx - ax, by - ay; L = math.hypot(lx, ly); ux, uy = lx / L, ly / L
        dx, dy = ux, uy
        while True:
            # toward the leg's end, following the street's own turn
            tx, ty = bx - x, by - y; tl = math.hypot(tx, ty)
            if tl < step * 1.5 or (x - ax) * ux + (y - ay) * uy > L - step * 0.5: break
            dx, dy = 0.5 * dx + 0.5 * tx / tl, 0.5 * dy + 0.5 * ty / tl; m = math.hypot(dx, dy); dx, dy = dx / m, dy / m
            nx_, ny_ = x + dx * step, y + dy * step
            s = snap(nx_, ny_, dx, dy)
            if s: ndx, ndy = s[0] - x, s[1] - y; m = math.hypot(ndx, ndy); dx, dy = ndx / m, ndy / m; nx_, ny_ = s
            x, y = nx_, ny_; out.append((x, y))
        x, y = bx, by
        s = snap(x, y, ux, uy)
        if s: x, y = s
        out.append((x, y))
    return [tuple(round(v, 2) for v in to_plan(u, v_)) for u, v_ in out]


def bulb_centre(u, v, r=130):
    """A turning circle's middle (town px): the middle of the asphalt round the rough point, twice."""
    im = image(); px = im.load()
    for _ in range(3):
        sx = sy = n = 0
        for qy in range(int(v - r), int(v + r), 3):
            for qx in range(int(u - r), int(u + r), 3):
                if (qx - u) ** 2 + (qy - v) ** 2 <= r * r and is_road(px[qx, qy]): sx += qx; sy += qy; n += 1
        if n: u, v = sx / n, sy / n
    return u, v


def trace_all():
    f = os.path.join(ROOT, 'Data', 'road-traces.json'); T = json.load(open(f, encoding='utf-8'))
    for name, wps in STREETS.items():
        p = trace_via(wps); T['town-' + name] = p
        print('town-%s: %d points, %.0f m' % (name, len(p), sum(math.dist(p[i], p[i + 1]) for i in range(len(p) - 1))))
    json.dump(T, open(f, 'w', encoding='utf-8'), indent=1)


# ---- the streets' true middles (owner, 2026-11-15: "the roads are very jagged ... look at the blueprints super well") --
# The march above wobbles where the plan's lamps, drives and labels break the asphalt. Here: the asphalt as a clean mask
# (its colour, its white and yellow lines; small gaps - lamp dots, dashes - closed), and its distance to the nearest edge
# (a ridge along every street's middle). A street's line is moved, every 2 m, square across onto that ridge.
_DT = None
RIDGE = (5.0, 7.5)
DS = 2                                      # (the mask at half the plan's resolution: 0.35 m a pixel)
def ridge():
    global _DT
    if _DT is None:
        import numpy as np
        from scipy import ndimage
        a = np.asarray(image())[::DS, ::DS].astype(np.int16)
        r, g, b = a[..., 0], a[..., 1], a[..., 2]
        near = lambda c, t: (np.abs(r - c[0]) <= t) & (np.abs(g - c[1]) <= t) & (np.abs(b - c[2]) <= t)
        # the whole street as drawn, kerb to kerb: asphalt, its pavements both sides, their lines and the lamps' glow
        # on them (the band is even along every street: its middle is the road's middle)
        m = near((74, 78, 84), 12) | near((196, 191, 180), 16)
        m |= (r > 220) & (g > 190) & (b < 140)                      # the yellow middle line
        m |= (r > 225) & (g > 225) & (b > 225)                      # white dashes and edge lines
        m |= (r > 88) & (r - b >= 18) & (r - b <= 70) & (r >= g) & (g >= b)   # the lamps' warm glow
        m |= near((26, 28, 30), 8)                                  # the street names' dark labels on the street
        m = ndimage.binary_closing(m, structure=np.ones((9, 9), bool))   # (lamp posts, gaps: ~3 m)
        m = ndimage.binary_opening(m, structure=np.ones((5, 5), bool))   # (stray specks off the street)
        _DT = ndimage.distance_transform_edt(m).astype(np.float32) * DS * MPX   # metres to the asphalt's edge
    return _DT


def centre(pts, every=2.0, reach=6.0, keep=None):
    """A street's line (plan m) moved onto the asphalt's middle (the ridge), every `every` m. Points where the ridge is
    not clear (a junction's middle, a turning circle) keep their place."""
    import numpy as np
    D = ridge(); H, W = D.shape
    P = np.array(pts, float); d = np.r_[0, np.cumsum(np.hypot(*np.diff(P, axis=0).T))]
    out = []
    for s in np.arange(0, d[-1] + 1e-6, every):
        x, y = np.interp(s, d, P[:, 0]), np.interp(s, d, P[:, 1])
        i = min(max(int(np.searchsorted(d, s)), 1), len(P) - 1)
        tx, ty = P[i] - P[i - 1]; tl = math.hypot(tx, ty) or 1; nx, ny = ty / tl, -tx / tl
        best, bo = -1, 0.0
        for o in np.arange(-reach, reach + 1e-6, 0.2):
            u, v = to_town(x + nx * o, y + ny * o); iu, iv = int(u / DS), int(v / DS)
            if 0 <= iu < W and 0 <= iv < H and D[iv, iu] > best + 1e-4: best, bo = D[iv, iu], o
        # a clean street: the ridge 5-7.5 m from the edges (kerb to kerb 10-15 m); anything else (a junction, a turning
        # circle) keeps the line
        # (a junction's wider asphalt too, up to 10 m from its edges: its ridge still runs near the through street's
        # middle - dropped, the curve cut across the east loop's dip by 6 m)
        if RIDGE[0] <= best <= 10.0: out.append((x + nx * bo, y + ny * bo))
        # (else dropped - a junction's or turning circle's middle, a gap in the drawing: the smoothing bridges it, and
        # a street's ends are made where it meets the next street - Tools/make_roads.py meet / bulb)
    return [tuple(round(float(v), 2) for v in p) for p in out]
