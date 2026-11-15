# The road network's data (owner, 2026-11-14; HANDOVER 3b, to-do 55): every road from the owner's map plans
# (Art/Maps/Airport-Road-Town-Overview-v11.png, Town-Plan-v7.png, Airport-Only-v11.png) written to Data/roads.json,
# which the game builds with MetaRoad Free when it starts (Source/.../RoadNetwork.cpp). Never placed by hand: run this
# again and the whole network is rebuilt the same.
#   python Tools/make_roads.py      (writes Data/ and Content/Data roads.json, then the land along the roads; then Tools/make_land_nanite.sh)
#
# A road: its path in airport plan metres (x along the runway, y across it, as Data/airfield.json), and its lanes either
# side of the path, inside out, each {type, w}: Driving, Bus, Parking, Shoulder, Sidewalk (raised, kerb cm), Island
# (raised grass), Apron (a roundabout's low concrete ring), Median. w is metres, or [[m along the road, m], ...] where the
# plan widens or narrows (eased in smoothly). Right-hand traffic: the right lanes run along the path, the left against it.
# On a road: "paint" (line / dash / boxes / teeth on lane edges), "crossings" (zebras), "lights" (street-light rows on lane
# edges), "signs", "disc" (a roundabout's grass middle); "lots" (car parks) beside the roads.
#
# Positions are read off the owner's blueprint with Tools/blueprint_grid.py (the airport's plan grid over the drawing).
# The drawing puts the terminal's landside front at y -120 and 37 m further west than the game's terminal (front at y 0,
# x -80..242; Data/terminal-plan.json): the airport-area roads are moved by that much so the forecourt meets the real
# terminal, the move easing out east of x 1000 (gone by x 1900) so the roads further out stay where the drawing has them.
import json, math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
roads, lots = [], []


def road(id, pts, left, right, **kw):
    import copy
    # (its own copy of the lanes: the town's streets share one lane set, and one street's pavement openings were
    # written into all of them - 2026-11-15)
    r = {'id': id, 'pts': [list(p) for p in pts], 'lanes': {'left': copy.deepcopy(left), 'right': copy.deepcopy(right)}}
    r.update(kw)
    roads.append(r)
    return r


def lane(t, w, **kw):
    d = {'type': t, 'w': w}
    d.update(kw)
    return d


def moved(x, y):
    """A drawing position (plan m) to the game's: the terminal's move, easing out east of x 1000."""
    w = 1.0 if x <= 1000 else max(0.0, 1.0 - (x - 1000) / 900.0)
    w = w * w * (3 - 2 * w)
    return (round(x + 37.0 * w, 2), round(y + 120.0 * w, 2))


def smooth(P, step=2.0):
    """The game's path (RoadNetwork.cpp SmoothPath, centripetal Catmull-Rom), to know how far along it each point is."""
    n = len(P)
    def at(i):
        if i < 0: return (2 * P[0][0] - P[1][0], 2 * P[0][1] - P[1][1])
        if i >= n: return (2 * P[-1][0] - P[-2][0], 2 * P[-1][1] - P[-2][1])
        return P[i]
    out = []
    for i in range(n - 1):
        p0, p1, p2, p3 = at(i - 1), at(i), at(i + 1), at(i + 2)
        kn = lambda t, a, b: t + max(math.sqrt(math.dist(a, b)), 1e-4)
        t0 = 0.0; t1 = kn(t0, p0, p1); t2 = kn(t1, p1, p2); t3 = kn(t2, p2, p3)
        K = max(2, math.ceil(math.dist(p1, p2) / step))
        for k in range(K):
            t = t1 + (t2 - t1) * k / K
            L = lambda a, b, ta, tb: tuple(a[j] * (tb - t) / (tb - ta) + b[j] * (t - ta) / (tb - ta) for j in range(2))
            a1, a2, a3 = L(p0, p1, t0, t1), L(p1, p2, t1, t2), L(p2, p3, t2, t3)
            b1, b2 = L(a1, a2, t0, t2), L(a2, a3, t1, t3)
            out.append(L(b1, b2, t1, t2))
    out.append(tuple(P[-1]))
    return out


class Along:
    """How far along a road (m) the point nearest a plan x is (for the roads running along x)."""
    def __init__(self, pts):
        self.d = smooth(pts); self.s = [0.0]
        for i in range(1, len(self.d)): self.s.append(self.s[-1] + math.dist(self.d[i], self.d[i - 1]))
        self.len = self.s[-1]
    def x(self, x):
        best = min(range(len(self.d)), key=lambda i: abs(self.d[i][0] - x))
        return round(self.s[best], 2)


def keys(*pairs):
    """Width keys [[m along, m], ...], sorted, from (m along, m) pairs."""
    return [[round(a, 2), w] for a, w in sorted(pairs)]

# ---------------------------------------------------------------------------------------------------------------------
# The test road (only built with -RoadTest): two 3.5 m lanes, 2.5 m pavements, west of the fire station
TWO_LANE_TOWN = dict(left=[lane('Driving', 3.5), lane('Sidewalk', 2.5, kerb=15)],
                     right=[lane('Driving', 3.5), lane('Sidewalk', 2.5, kerb=15)])
road('TEST-PART1', [(-1262, 70), (-1285, 110), (-1315, 150), (-1330, 200), (-1332, 260)], test=True,
     lights=[dict(side='right', edge=2, off=-0.45, spacing=35), dict(side='left', edge=2, off=-0.45, spacing=35)],
     crossings=[dict(at=30, w=3.0)], signs=[dict(at=120, km='2', dir=-1)], **TWO_LANE_TOWN)

# ---------------------------------------------------------------------------------------------------------------------
# THE AIRPORT AREA (owner's Airport-Only-v11 + the forecourt detail on the Overview-v11)
#
# The through road: from the 3 km mark (two lanes) along the terminal's forecourt to the small roundabout at the west end
# ("turn back to town"). Its path runs north-east to south-west, so its right side is the terminal's. From its middle
# outward (the forecourt detail, about 20 px to the metre): through lane 3.5, edge 1.0, grass island 3.5 with lamps,
# spare lane 3.6, taxi lane 3.55, taxi stands 3.1, bus lane 3.5, bus stands 3.5, the pavement outside the main hall
# 5.25 (to the terminal's front at y 0: the path at y -30.5); the other way: through lane 3.5, edge 1.0, the pavement
# along the car park 3.0. The terminal-side lanes widen in over 120 m where the band starts (x 938 / -683 on the
# drawing's band, full from 820 to -560); the stands and the terminal's pavement along the terminal (x -80..242) with
# 40 m lay-by tapers; the car park's pavement along the car park (x -93..372) with 30 m tapers.
# The terminal's pavement is 21.5 m, not the drawing's 5.25: the terminal model's entrance canopies stand on columns about
# 10 m out and its benches 12-14 m out (Data/babylon-terminal.json KERB/), and they must stand on the pavement, not in
# the lanes - everything else as drawn, 10.75 m further out
TP = 21.5   # (the model's benches stand 17-19 m out from the terminal line: 2.5 m clear of them to the kerb)
Y0 = -(30.5 - 5.25 + TP)
FC_PTS = [(1856, -116), (1314, -144), (1078, -153 + (Y0 + 30.5) * 0.5), (901, Y0 - 120), (500, Y0 - 120), (0, Y0 - 120), (-500, Y0 - 120), (-781, Y0 - 120)]   # (ends 1 m inside the roundabout's outer edge)
FC = [moved(*p) for p in FC_PTS]
A = Along(FC)
z = 0.05                      # (a lane narrowed away: MetaRoad keeps a sliver, laid flat)
s_ne0, s_ne1, s_sw1, s_sw0 = A.x(938), A.x(820), A.x(-560), A.x(-683)   # the band: starts widening, full, narrowing, gone
s_t0a, s_t0, s_t1, s_t1a = A.x(282), A.x(242), A.x(-80), A.x(-120)      # the terminal's stretch (40 m tapers)
s_c0a, s_c0, s_c1, s_c1a = A.x(402), A.x(372), A.x(-93), A.x(-123)      # the car park's stretch (30 m tapers)
band = lambda w: keys((0, z), (s_ne0, z), (s_ne1, w), (s_sw1, w), (s_sw0, z), (A.len, z))
term = lambda w: keys((0, z), (s_t0a, z), (s_t0, w), (s_t1, w), (s_t1a, z), (A.len, z))
park = lambda w: keys((0, z), (s_c0a, z), (s_c0, w), (s_c1, w), (s_c1a, z), (A.len, z))
# the terminal's pavement runs on past the stands to x 300 at the east end (the terminal model's rental area: its cars
# 7 m out and its row of birches 18.5 m out, x 164-286, Data/babylon-terminal.json RENTAL/) and 40 m past the west end;
# where the stands narrow away it widens by as much, so its outer edge stays on the building's line (y 0)
s_w0a, s_w0, s_w1, s_w1a = A.x(340), A.x(300), A.x(-120), A.x(-160)
STANDS = 3.1 + 3.5
walk = lambda w: keys((0, z), (s_w0a, z), (s_w0, w + STANDS), (s_t0a, w + STANDS), (s_t0, w), (s_t1, w), (s_t1a, w + STANDS), (s_w1, w + STANDS), (s_w1a, z), (A.len, z))
FC_RIGHT = [lane('Driving', 3.5), lane('Shoulder', 1.0), lane('Island', band(3.5), kerb=15), lane('Driving', band(3.6)),
            lane('Driving', band(3.55)), lane('Parking', term(3.1)), lane('Bus', band(3.5)), lane('Parking', term(3.5)),
            lane('Sidewalk', walk(TP), kerb=15)]
FC_LEFT = [lane('Driving', 3.5, invert=True), lane('Shoulder', 1.0), lane('Sidewalk', park(3.0), kerb=15)]
x_ent = (50, 75, 101)          # the terminal's three entrances (Data/terminal-plan.json: F1-ENTR-W/C/E)
fc_paint = [
    # the through road's double yellow middle, dashed out on the two-lane ends
    dict(kind='line', side='right', edge=0, off=0.1, w=0.1, color='yellow', from_=s_ne0, to=s_sw0),
    dict(kind='line', side='left', edge=0, off=0.1, w=0.1, color='yellow', from_=s_ne0, to=s_sw0),
    dict(kind='dash', side='right', edge=0, off=0.0, w=0.12, color='yellow', dash=3, gap=9, to=s_ne0),
    dict(kind='dash', side='right', edge=0, off=0.0, w=0.12, color='yellow', dash=3, gap=9, from_=s_sw0),
    # the through lanes' white edge lines (in their 1 m edges)
    dict(kind='line', side='right', edge=1, off=0.12, w=0.15),
    dict(kind='line', side='left', edge=1, off=0.12, w=0.15),
    # the one-way lanes: dashed spare | taxi lane and taxi | bus lane, solid along the stands
    dict(kind='dash', side='right', edge=4, w=0.12, dash=3, gap=6, from_=s_ne1, to=s_sw1),
    dict(kind='dash', side='right', edge=6, w=0.12, dash=3, gap=6, from_=s_ne1, to=s_sw1),
    dict(kind='line', side='right', edge=5, w=0.12, from_=s_t0, to=s_t1),
    dict(kind='line', side='right', edge=7, w=0.12, from_=s_t0, to=s_t1),
    # the bus lane's outer edge where there are no stands
    dict(kind='line', side='right', edge=7, off=-0.25, w=0.15, from_=s_ne1, to=s_t0a),
    dict(kind='line', side='right', edge=7, off=-0.25, w=0.15, from_=s_t1a, to=s_sw1),
    # the stands: five buses in line (yellow, 13 m each) by the main entrance, taxis (white, 6 m) toward the east end
    dict(kind='boxes', side='right', lane=7, n=5, len=13.0, gap=2.0, color='yellow', from_=A.x(112.5)),
    dict(kind='boxes', side='right', lane=5, n=12, len=6.0, gap=0.8, from_=A.x(236)),
    # give way into the roundabout (shark teeth, on the lane going in)
    dict(kind='teeth', at=A.len - 0.8, dir=1, o0=0.15, o1=3.4),
]
for p in fc_paint:
    if 'from_' in p: p['from'] = p.pop('from_')
fc_cross = []
for x in x_ent:
    s = A.x(x)
    fc_cross.append(dict(at=s, w=3.0, o0=3.5 + 1.0 + 3.5, o1=30.5 - 5.25))   # (the lanes' side of the pavement: unchanged)   # the terminal's pavement to the island
    fc_cross.append(dict(at=s, w=3.0, o0=-4.5, o1=3.5 + 1.0))                  # the island to the car park's pavement
fc_lights = [
    dict(side='right', edge=8, off=0.6, spacing=30.5, **{'from': s_t0, 'to': s_t1}),               # the terminal's pavement, by its kerb (clear of the canopies and benches)
    dict(side='right', edge=7, off=0.9, spacing=30.5, **{'from': s_ne1, 'to': s_t0a}),             # the band's edge beyond it
    dict(side='right', edge=7, off=0.9, spacing=30.5, **{'from': s_t1a, 'to': s_sw1}),
    dict(side='right', edge=2, off=1.75, spacing=30.5, toward='out', **{'from': s_ne1, 'to': s_sw1}),   # the island
    dict(side='left', edge=3, off=-0.45, spacing=30.5, **{'from': s_c0, 'to': s_c1}),              # the car park's pavement
    dict(side='left', edge=2, off=0.9, spacing=30.5, **{'from': s_ne1, 'to': s_c0a}),
    dict(side='left', edge=2, off=0.9, spacing=30.5, **{'from': s_c1a, 'to': s_sw1}),
    dict(side='right', edge=2, off=0.9, spacing=45, **{'from': 15, 'to': s_ne1}),                   # the approach: one side
]
# Drainage (owner, 2026-11-14: as real roads, at real distances): a gully grating in the gutter by every kerb every 22 m
# (the usual spacing on kerbed streets, 20-25 m), the grating 0.31 m from the kerb's face - by the terminal's pavement,
# both sides of the island, by the car park's pavement; round manhole covers far apart on the pavements (every 90-110 m).
# The country stretches have no kerbs: they drain to the verge, as real ones do.
fc_props = [
    dict(mesh='drain', side='right', edge=8, off=-0.31, spacing=22, out=1, **{'from': s_t0, 'to': s_t1}),
    dict(mesh='drain', side='right', edge=3, off=0.31, spacing=22, out=-1, **{'from': s_ne1, 'to': s_sw1}),
    dict(mesh='drain', side='right', edge=2, off=-0.31, spacing=22, out=1, **{'from': s_ne1 + 11, 'to': s_sw1}),
    dict(mesh='drain', side='left', edge=2, off=-0.31, spacing=22, out=1, **{'from': s_c0, 'to': s_c1}),
    dict(mesh='manhole', side='right', edge=8, off=7.5, spacing=95, **{'from': s_t0 + 20, 'to': s_t1}),
    dict(mesh='manhole', side='left', edge=2, off=1.5, spacing=110, **{'from': s_c0 + 35, 'to': s_c1}),
]
road('AIRPORT-FORECOURT', FC, FC_LEFT, FC_RIGHT, paint=fc_paint, crossings=fc_cross, lights=fc_lights, props=fc_props, step=6.0)   # (a point every 6 m: MetaRoad's spline curves smoothly between; 2 m took 25 s to build)

# The roundabout at the west end ("turn back to town", drawing centre (-801, -150)): 21 m to its outer edge, a 7 m ring
# (one lane, wide enough for a bus), a low concrete apron for long vehicles' back wheels, a raised grass middle; round
# anticlockwise (right-hand traffic). The ring's path at 17.25 m: its outer half to the right, its inner to the left.
RB = moved(-801, Y0 - 120)
RS = 17.25
ring = [(RB[0] + RS * math.cos(2 * math.pi * k / 24), RB[1] + RS * math.sin(2 * math.pi * k / 24)) for k in range(24)]
road('AIRPORT-ROUNDABOUT', [(round(x, 2), round(y, 2)) for x, y in ring], closed=True, lift=0.055,
     left=[lane('Driving', 3.25), lane('Apron', 1.5, kerb=5), lane('Island', 6.5, kerb=15)],
     right=[lane('Driving', 3.75)],
     disc=dict(r=RS - 3.25 - 1.5 - 6.5 + 0.25, kerb=0.15),
     lights=[dict(side='right', edge=1, off=1.6, spacing=27.0)])

# The road on west past the roundabout: closed (kept for later expansion), to the drawing's barrier at x -1130
TWO = dict(left=[lane('Driving', 3.5, invert=True), lane('Shoulder', 0.75)], right=[lane('Driving', 3.5), lane('Shoulder', 0.75)])
two_paint = lambda dash=True: [dict(kind='dash', side='right', edge=0, off=0.0, w=0.12, color='yellow', dash=3, gap=9),
                               dict(kind='line', side='right', edge=1, off=0.1, w=0.15), dict(kind='line', side='left', edge=1, off=0.1, w=0.15)]
road('AIRPORT-WEST-CLOSED', [(RB[0] - 20.0, RB[1]), (moved(-1130, -150.5)[0], RB[1])], paint=two_paint(), **TWO)

# The road back to town (two lanes): from the roundabout's south side round the loop and east past the car park's
# south edge (drawing: the loop, then straight at y about -375, then curving away south-east toward the big roundabout).
# Lights on one side for its first kilometre (the approach); give way into the roundabout on the lane going in.
# (east of x 1139 it runs on as traced off the overview to the big roundabout: TOWN, below)
TRACES = json.loads((ROOT / 'Data' / 'road-traces.json').read_text(encoding='utf-8'))   # Tools/trace_road.py, the overview


def tidy(pts, every=5, half=10, keep_off=1.5):
    """A traced line made smooth and true to the drawing: at each point a curve (a parabola in x and in y along the line)
    fitted through the 21 points round it (168 m) - it follows the drawn bends but not the tracer's swerves round the
    drawing's labels and sign markers; points more than `keep_off` m off their fitted curve are dropped and the fit made
    again without them, then thinned to every `every`-th point (the game's own curve runs smoothly between)."""
    import numpy as np
    P = np.array(pts, float); n = len(P)
    keep = np.ones(n, bool)
    for _ in range(3):
        fit = P.copy()
        for i in range(n):
            lo, hi = max(0, i - half), min(n, i + half + 1)
            idx = np.arange(lo, hi)[keep[lo:hi]]
            if len(idx) < 5: continue
            t = idx - i
            for k in (0, 1): fit[i, k] = np.polyval(np.polyfit(t, P[idx, k], 2), 0)
        off = np.hypot(*(P - fit).T)
        keep = off <= keep_off; keep[0] = keep[-1] = True
    out = [tuple(fit[i]) for i in range(n)]
    out[0], out[-1] = tuple(pts[0]), tuple(pts[-1])
    sel = out[::every]
    if sel[-1] != out[-1]: sel.append(out[-1])
    return [(round(x, 2), round(y, 2)) for x, y in sel]


def fair_smooth(pts, period=60.0, every=6.0, hold=True):
    """A line through measured middle points made smooth (owner, 2026-11-15: "the roads are very jagged"): resampled
    every 2 m along it, then a Whittaker smoother (third differences) that takes out every ripple shorter than about
    `period` m - the measuring's noise - and keeps the plan's bends and corners (25 m radius and more). Its ends held."""
    import numpy as np
    from scipy import sparse
    from scipy.sparse.linalg import spsolve
    P = np.array(pts, float)
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(P, axis=0).T))]
    keep = np.r_[True, np.diff(d) > 1e-6]; P, d = P[keep], d[keep]
    h = 2.0; s = np.linspace(0, d[-1], max(8, int(d[-1] / h) + 1)); h = s[1] - s[0]
    X = np.c_[np.interp(s, d, P[:, 0]), np.interp(s, d, P[:, 1])]
    n = len(s); lam = (period / (2 * math.pi * h)) ** 6
    D3 = sparse.diags([-1, 3, -3, 1], [0, 1, 2, 3], shape=(n - 3, n))
    w = np.ones(n)
    if hold: w[:2] = w[-2:] = 1e4
    A = sparse.diags(w) + lam * (D3.T @ D3)
    Z = np.c_[spsolve(A.tocsc(), w * X[:, 0]), spsolve(A.tocsc(), w * X[:, 1])]
    zd = np.r_[0, np.cumsum(np.hypot(*np.diff(Z, axis=0).T))]
    m = max(2, int(round(zd[-1] / every)))
    out = [(float(np.interp(x, zd, Z[:, 0])), float(np.interp(x, zd, Z[:, 1]))) for x in np.linspace(0, zd[-1], m + 1)]
    if hold: out[0], out[-1] = tuple(pts[0]), tuple(pts[-1])
    return [(round(x, 2), round(y, 2)) for x, y in out]


def fair_lsq(pts, knot=22.0, every=6.0):
    """A line through measured middle points as a least-squares cubic spline with a knot every `knot` m along it: it
    follows every bend the plan draws (its tightest corners are 25 m and more) but cannot ripple over a few metres as
    the measured points do (lamp dots, drives). Its ends kept where they are (they meet other streets)."""
    import numpy as np
    from scipy.interpolate import make_lsq_spline
    P = np.array(pts, float)
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(P, axis=0).T))]
    keep = np.r_[True, np.diff(d) > 1e-6]; P, d = P[keep], d[keep]
    n_in = max(0, int(d[-1] / knot) - 1)
    inner = list(np.linspace(0, d[-1], n_in + 2)[1:-1])
    t = np.r_[[d[0]] * 4, inner, [d[-1]] * 4]
    w = np.ones(len(d)); w[0] = w[-1] = 200.0
    sx = make_lsq_spline(d, P[:, 0], t, k=3, w=w); sy = make_lsq_spline(d, P[:, 1], t, k=3, w=w)
    n = max(2, int(round(d[-1] / every)))
    ss = np.linspace(0, d[-1], n + 1)
    out = [(float(sx(x)), float(sy(x))) for x in ss]
    out[0], out[-1] = tuple(pts[0]), tuple(pts[-1])
    return [(round(x, 2), round(y, 2)) for x, y in out]


def fair(pts, rms=0.9, every=8.0):
    """A traced line made into one smooth curve (owner, 2026-11-15: the traced streets were jagged - the tracer swerves
    round the plan's drawn houses, drives and labels): resampled every 2 m, a smoothing spline fitted through it that
    may stand off the trace by `rms` m on average (the drawing's line is about that true), points on it every `every` m
    (the game's own curve runs smoothly between)."""
    import numpy as np
    from scipy.interpolate import splprep, splev
    P = np.array(pts, float)
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(P, axis=0).T))]
    keep = np.r_[True, np.diff(d) > 1e-6]; P, d = P[keep], d[keep]
    t = np.linspace(0, d[-1], max(8, int(d[-1] / 2.0)))
    Q = np.c_[np.interp(t, d, P[:, 0]), np.interp(t, d, P[:, 1])]
    w = np.ones(len(Q)); w[0] = w[-1] = 30.0     # (its ends stay where they are: they meet other streets)
    tck, _ = splprep([Q[:, 0], Q[:, 1]], w=w, s=len(Q) * rms * rms, k=3)
    fine = np.array(splev(np.linspace(0, 1, len(Q) * 4), tck)).T
    fd = np.r_[0, np.cumsum(np.hypot(*np.diff(fine, axis=0).T))]
    n = max(2, int(round(fd[-1] / every)))
    out = [(float(np.interp(x, fd, fine[:, 0])), float(np.interp(x, fd, fine[:, 1]))) for x in np.linspace(0, fd[-1], n + 1)]
    out[0], out[-1] = tuple(pts[0]), tuple(pts[-1])
    return [(round(x, 2), round(y, 2)) for x, y in out]


BIG_RB = (2217.2, -1169.5)   # the roundabout where the roads meet (the overview's 'roundabout', under its sign marker)
def to_ring(pts, r=20.0):
    """A traced road cut where it meets the big roundabout: it ends (or starts) 1 m inside the ring's outer edge."""
    return [p for p in pts if math.dist(p, BIG_RB) > r] 
BACK_EAST = to_ring(tidy(TRACES['back']))
BT_PTS = [(RB[0], RB[1] - 20.0)] + [moved(*p) for p in [(-798, -222), (-786, -251), (-765, -288), (-742, -317), (-710, -341),
          (-676, -358), (-597, -375), (-282, -375), (35, -375), (298, -375), (545, -380), (781, -421), (948, -476)]] +          [moved(*p) for p in BACK_EAST] + [moved(*(BIG_RB[0] + (BACK_EAST[-1][0] - BIG_RB[0]) * 20.0 / math.dist(BACK_EAST[-1], BIG_RB), BIG_RB[1] + (BACK_EAST[-1][1] - BIG_RB[1]) * 20.0 / math.dist(BACK_EAST[-1], BIG_RB)))]
road('AIRPORT-BACK-TO-TOWN', BT_PTS, paint=two_paint() + [dict(kind='teeth', at=0.8, dir=-1, o0=-3.4, o1=-0.15)],
     lights=[dict(side='right', edge=2, off=0.9, spacing=45, **{'from': 30, 'to': 1000})], step=4.0, **TWO)

# The large car park across the through road (drawing x -130..330 -> -93..367; from the pavement to the road back to
# town): aisles 6.5 m along it, double rows of 2.5 x 5 m spaces back to back between them
cp = dict(x0=-93.0, x1=367.0, y0=-247.0, y1=round(Y0 - 3.5 - 1.0 - 3.0 - 0.2, 2), lift=0.03, rows=[])
y = cp['y1'] - 6.5
while y - 10.0 > cp['y0'] + 6.0:
    cp['rows'].append(dict(x0=cp['x0'] + 3.0, x1=cp['x1'] - 3.0, y=round(y, 2), d=5.0, dir=-1, w=2.5))
    cp['rows'].append(dict(x0=cp['x0'] + 3.0, x1=cp['x1'] - 3.0, y=round(y - 10.0, 2), d=4.9, dir=1, w=2.5))
    y -= 16.5
lots.append(cp)
# the rental cars' bays on the terminal's pavement (its cars stand in groups 4 m apart, nose in, 4-10 m out): white
# lines on the pavement round each, no paving of their own (the pavement is it), at the pavement's height
rental = dict(x0=174.0, x1=282.0, y0=-10.5, y1=-3.5, paved=False, lift=0.226, rows=[])
for g0, n in ((174, 2), (190, 3), (210, 3), (230, 3), (250, 3), (270, 3)):
    rental['rows'].append(dict(x0=g0, x1=g0 + 4.0 * n, y=-4.0, d=6.0, dir=-1, w=4.0))
# its two shuttle buses (RENTAL/SHUTTLE-1, -2: side-on, 14 m out): a yellow bay each
rental['boxes'] = [dict(x0=x - 6.5, y0=-15.6, x1=x + 6.5, y1=-12.4, color='yellow') for x in (202.0, 242.0)]
rental['y0'] = -16.0
lots.append(rental)

# ---------------------------------------------------------------------------------------------------------------------
# THE ROAD TO TOWN (owner's overview: 'two lanes, 90 km/h, no pavements, flat sand all the way', 7.8 km), traced off the
# drawing: from the 3 km mark (the forecourt road's end) round to the big roundabout, and from it south and east past
# the 5, 6 and 7 km marks to the edge of the town. Two 3.5 m lanes, 0.75 m paved edges outside white edge lines, a
# dashed yellow middle; no kerbs, no lights from 1 km out (owner: as in this part of the world).
def ring_point(p, r=20.0):
    d = math.dist(p, BIG_RB); return (round(BIG_RB[0] + (p[0] - BIG_RB[0]) * r / d, 2), round(BIG_RB[1] + (p[1] - BIG_RB[1]) * r / d, 2))
main = to_ring(tidy(TRACES['main']))
main = [moved(*FC_PTS[0])] + [moved(*p) for p in main[1:]] + [ring_point(main[-1])]
road('TOWN-ROAD-NORTH', main, step=6.0, paint=two_paint(), **TWO)
south = to_ring(tidy(TRACES['south']))
south = [ring_point(south[0])] + south
road('TOWN-ROAD-SOUTH', south, step=6.0, paint=two_paint() + [dict(kind='teeth', at=0.8, dir=-1, o0=-3.4, o1=-0.15)], **TWO)
# give way into the big roundabout on the other two roads' lanes going in
roads[-2]['paint'].append(dict(kind='teeth', at=round(Along(main).len - 0.8, 2), dir=1, o0=0.15, o1=3.4))
# the big roundabout: as the airport's (a 7 m ring, a low apron for long vehicles, a raised grass middle), lights round it
ring2 = [(BIG_RB[0] + RS * math.cos(2 * math.pi * k / 24), BIG_RB[1] + RS * math.sin(2 * math.pi * k / 24)) for k in range(24)]
road('TOWN-ROUNDABOUT', [(round(x, 2), round(y, 2)) for x, y in ring2], closed=True, lift=0.055,
     left=[lane('Driving', 3.25), lane('Apron', 1.5, kerb=5), lane('Island', 6.5, kerb=15)], right=[lane('Driving', 3.75)],
     disc=dict(r=RS - 3.25 - 1.5 - 6.5 + 0.25, kerb=0.15), lights=[dict(side='right', edge=1, off=1.6, spacing=27.0)])

# ---------------------------------------------------------------------------------------------------------------------
# THE TOWN (owner's Town-Plan-v7: north up, two-lane streets with 2.5 m pavements - kerbed, the town is built-up -
# zebras, traffic lights and street lights later), placed by Tools/town_plan.py (its frame = the overview's town box),
# its streets traced off it. Part 1 (2026-11-15): the road from the airport on to the town's roundabout (9), the
# roundabout, and Main Street east from it to the town's east end.
import town_plan as TP_
R9 = tuple(round(v, 2) for v in TP_.to_plan(768, 4365))      # roundabout 9's middle (the plan's circle)
def ring9(p, r=RS + 0.0):
    d = math.dist(p, R9); return (round(R9[0] + (p[0] - R9[0]) * r / d, 2), round(R9[1] + (p[1] - R9[1]) * r / d, 2))
# the road from the airport: on from where the overview's drawing ends to the roundabout's west side
sr = next(r for r in roads if r['id'] == 'TOWN-ROAD-SOUTH')
end = tuple(sr['pts'][-1]); far = ring9(end, RS + 25.0)
sr['pts'] += [list(far), list(ring9(end))]
sr['paint'].append(dict(kind='teeth', at=round(Along([tuple(p) for p in sr['pts']]).len - 0.8, 2), dir=1, o0=0.15, o1=3.4))
ring9_pts = [(R9[0] + RS * math.cos(2 * math.pi * k / 24), R9[1] + RS * math.sin(2 * math.pi * k / 24)) for k in range(24)]
road('TOWN-RB9', [(round(x, 2), round(y, 2)) for x, y in ring9_pts], closed=True, lift=0.055,
     left=[lane('Driving', 3.25), lane('Apron', 1.5, kerb=5), lane('Island', 6.5, kerb=15)], right=[lane('Driving', 3.75)],
     disc=dict(r=RS - 3.25 - 1.5 - 6.5 + 0.25, kerb=0.15), lights=[dict(side='right', edge=1, off=1.6, spacing=27.0)])
# Main Street: from the roundabout's east side to the town's east end (1.46 km), two lanes, pavements both sides; at the
# east end it bends south and runs on as the east loop (one street on the plan) back to Main Street
def town_tidy(name, pts=None):
    # the traced line onto the plan's asphalt middle (Tools/town_plan.py centre: the ridge of its distance to the
    # edges), smoothed, and again: the second pass starts from a line already on the street
    p = pts or TRACES['town-' + name]
    p = fair(p, rms=1.5)
    # (its ends free: the rough trace's ends lie in the junctions' wide asphalt, and the street's ends are made
    # where it meets the next one - meet / bulb)
    for reach in (16.0, 9.0, 6.0, 4.0):   # (the rough trace wanders up to ~20 m: no two drawn streets run that close)
        p = fair_smooth(TP_.centre(p, reach=reach), hold=False)
    return p


# (Main Street and its east loop are one drawn line: smoothed as one, then parted where the trace of Main Street ended)
_me = TRACES['town-main'] + [p for p in TRACES['town-east-loop'] if math.dist(p, TRACES['town-main'][-1]) > 3.0]
_me = town_tidy(None, _me)
_k = min(range(len(_me)), key=lambda i: math.dist(_me[i], TRACES['town-main'][-1]))
EAST_LOOP_PTS = _me[_k:]
ms = [p for p in _me[:_k + 1] if math.dist(p, R9) > RS + 6.0]
ms = [ring9(ms[0])] + ms
TOWN2 = dict(left=[lane('Driving', 3.5, invert=True), lane('Sidewalk', 2.5, kerb=15)], right=[lane('Driving', 3.5), lane('Sidewalk', 2.5, kerb=15)])
ONEWAY = dict(left=[lane('Driving', 2.25), lane('Sidewalk', 2.5, kerb=15)], right=[lane('Driving', 2.25), lane('Sidewalk', 2.5, kerb=15)])
BULB = dict(left=[lane('Driving', 4.99)], right=[lane('Driving', 4.5), lane('Sidewalk', 2.5, kerb=15)])
BR = 5.0                     # a turning circle's path radius: its carriageway out to 9.5 m, its pavement to 12 m

# Parts 2-4 (2026-11-15, owner: "build the road until it's fully completed"): every other street on the plan, traced off
# it (Tools/town_plan.py STREETS / trace_via), joined where they meet: a street runs on to the middle of the street it
# meets, at its own angle as drawn, a centimetre lower (its "lift": the part under the other street never shows); the
# pavements of both open across the corner (narrowed away, 3 m tapers). Give-way teeth
# on every street coming out, Sign B (the airport's km) 16 m before them, zebras over each side street's mouth, street
# lights every 32 m on both pavements (staggered), none in the mouths. Turning circles: a paved disc, a pavement round it.
TSTREET = {}                 # road id -> dict(d (its path, fine), s (m along each point))
CUTS = {}                    # road id -> [(side 'right'/'left', s, half)]


def host_info(pts):
    d = smooth(pts); s = [0.0]
    for i in range(1, len(d)): s.append(s[-1] + math.dist(d[i], d[i - 1]))
    return dict(d=d, s=s)


ENDCUTS = {}                 # road id -> [(s, half)]: its own pavements opened at its ends (both sides)


def meet(pts, at_start, host_id):
    """A street's end on the street it meets (host), the way the plan draws it - at its own angle, not bent square: its
    middle runs on to the host's middle line (the part over the host lies under it: a centimetre lower), its own
    pavements open where it crosses the host's carriageway and pavement, and the host's pavement opens across its whole
    mouth (wider when it comes in at a slant). Returns the new points, the host's point there, and the length of the
    end that is opened (m)."""
    H = TSTREET[host_id]; P = list(pts) if at_start else list(reversed(pts))
    i = min(range(len(H['d'])), key=lambda k: math.dist(H['d'][k], P[0]))
    c = H['d'][i]
    while len(P) > 2 and math.dist(P[0], c) < 12.0: P.pop(0)
    a, b = H['d'][max(0, i - 2)], H['d'][min(len(H['d']) - 1, i + 2)]
    tx, ty = b[0] - a[0], b[1] - a[1]; tl = math.hypot(tx, ty); tx, ty = tx / tl, ty / tl
    rx, ry = ty, -tx                                   # the host's right (plan: y is flipped in the game)
    ux, uy = P[0][0] - c[0], P[0][1] - c[1]; ul = math.hypot(ux, uy); ux, uy = ux / ul, uy / ul
    sg = 1.0 if ux * rx + uy * ry > 0 else -1.0
    cos = max(0.5, abs(ux * rx + uy * ry))             # (square = 1; a slant of 60 degrees at most)
    sin = math.sqrt(max(0.0, 1 - cos * cos))
    # its own line carried straight on (as the plan draws it) to the host's middle line: no hook at the end
    j = min(3, len(P) - 1)                             # (its way from ~18 m of its own line: not a last point's lean)
    vx, vy = P[0][0] - P[j][0], P[0][1] - P[j][1]; vl = math.hypot(vx, vy); vx, vy = vx / vl, vy / vl
    nd = (P[0][0] - c[0]) * rx + (P[0][1] - c[1]) * ry
    rate = vx * rx + vy * ry
    E = (P[0][0] - vx * nd / rate, P[0][1] - vy * nd / rate) if abs(rate) > 0.3 else c
    P = [(round(E[0], 2), round(E[1], 2))] + P
    CUTS.setdefault(host_id, []).append(('right' if sg > 0 else 'left', H['s'][i], 6.0 / cos + 3.5 * sin / cos + 0.5))
    corner_fills(E, (-vx, -vy), c, (tx, ty), (rx * sg, ry * sg))
    return (P if at_start else list(reversed(P))), c, 6.0 / cos + 0.5


FILLS = []                   # the corners' curves: [{pts (plan m), lift}] - asphalt the game lays on our ground


def corner_fills(E, a, c, t, out, half=3.5, R=6.0, n=10):
    """The two curved corners where a street (its end E, running away along a) meets another (its middle point c, its
    way t, `out` toward the street): each corner's asphalt between the two carriageway edges and a curve of radius R
    touching both (owner, 2026-11-15: "make the curves meet each other when the roads connect")."""
    ns = (-a[1], a[0])
    for k in (1.0, -1.0):
        # the street's edge: E + ns*k*half + a*u ; the host's edge: c + out*half + t*w
        p1 = (E[0] + ns[0] * k * half, E[1] + ns[1] * k * half); p2 = (c[0] + out[0] * half, c[1] + out[1] * half)
        det = a[0] * (-t[1]) - a[1] * (-t[0])
        if abs(det) < 1e-3: continue
        dx, dy = p2[0] - p1[0], p2[1] - p1[1]
        u = (dx * (-t[1]) - dy * (-t[0])) / det
        Q = (p1[0] + a[0] * u, p1[1] + a[1] * u)
        d2 = t if t[0] * ns[0] * k + t[1] * ns[1] * k > 0 else (-t[0], -t[1])     # along the host, away from the street
        cosphi = max(-0.99, min(0.99, a[0] * d2[0] + a[1] * d2[1])); phi = math.acos(cosphi)
        T = R / math.tan(phi / 2)
        if T > 14.0: continue
        T1 = (Q[0] + a[0] * T, Q[1] + a[1] * T); T2 = (Q[0] + d2[0] * T, Q[1] + d2[1] * T)
        bis = (a[0] + d2[0], a[1] + d2[1]); bl = math.hypot(*bis); bis = (bis[0] / bl, bis[1] / bl)
        C = (Q[0] + bis[0] * R / math.sin(phi / 2), Q[1] + bis[1] * R / math.sin(phi / 2))
        a1 = math.atan2(T1[1] - C[1], T1[0] - C[0]); a2 = math.atan2(T2[1] - C[1], T2[0] - C[0])
        da = (a2 - a1 + math.pi) % (2 * math.pi) - math.pi
        arc = [(C[0] + R * math.cos(a1 + da * j / n), C[1] + R * math.sin(a1 + da * j / n)) for j in range(n + 1)]
        # (from the corner point, a little inside both carriageways so no seam shows)
        Qi = (Q[0] - a[0] * 0.3 - d2[0] * 0.3, Q[1] - a[1] * 0.3 - d2[1] * 0.3)
        FILLS.append(dict(pts=[[round(x, 2), round(y, 2)] for x, y in [Qi] + arc], lift=0.035))


BULBS = []                   # [(middle, [angles of the streets coming in])]


def bulb(pts, at_start, rough):
    """A street's end on a turning circle (its middle found on the plan, round the rough town px): the street runs on
    to its middle (under the circle's paving), its pavements open over the circle."""
    C = TP_.to_plan(*TP_.bulb_centre(*rough))
    P = list(pts) if at_start else list(reversed(pts))
    while len(P) > 2 and math.dist(P[0], C) < 16.0: P.pop(0)
    ux, uy = P[0][0] - C[0], P[0][1] - C[1]; ul = math.hypot(ux, uy); ux, uy = ux / ul, uy / ul
    # the street carried straight on into the circle (as drawn: a street may come in off its middle), to the point of
    # its line nearest the middle
    j = min(3, len(P) - 1)
    vx, vy = P[0][0] - P[j][0], P[0][1] - P[j][1]; vl = math.hypot(vx, vy); vx, vy = vx / vl, vy / vl
    t = (C[0] - P[0][0]) * vx + (C[1] - P[0][1]) * vy
    E = (P[0][0] + vx * t, P[0][1] + vy * t)
    if math.dist(E, C) > 4.5: E = C
    P = [(round(E[0], 2), round(E[1], 2))] + P
    for B in BULBS:
        if math.dist(B[0], C) < 4.0: B[1].append(math.atan2(uy, ux)); break
    else: BULBS.append((C, [math.atan2(uy, ux)]))
    return P if at_start else list(reversed(P))


road_ms = road('TOWN-MAIN-STREET', ms, step=4.0, lift=0.05,
               paint=two_paint() + [dict(kind='teeth', at=0.8, dir=-1, o0=-3.4, o1=-0.15)], **TOWN2)
TSTREET['TOWN-MAIN-STREET'] = host_info(ms)
# the east loop: Main Street bending south at the town's east end and round back to it (its own road, a centimetre
# under Main Street where it comes back)
el = EAST_LOOP_PTS
el, c_el, e_el = meet(el, False, 'TOWN-MAIN-STREET')
L_el = Along(el).len
road('TOWN-EAST-LOOP', el, step=4.0, lift=0.045, paint=[dict(p, to=round(L_el - e_el - 2.0, 2)) for p in two_paint()] +
     [dict(kind='teeth', at=round(L_el - 3.5 - 0.6, 2), dir=1, o0=0.15, o1=3.4)],
     crossings=[dict(at=round(L_el - e_el - 2.0, 2), w=3.0)], signs=[dict(at=round(L_el - 20.0, 2), km='8', dir=1)], **TOWN2)
ENDCUTS['TOWN-EAST-LOOP'] = [(L_el, e_el)]
TSTREET['TOWN-EAST-LOOP'] = host_info(el)

# name (Tools/town_plan.py STREETS), host at its start ('bulb:u,v' = a turning circle round that town px), host at its
# end, lift, one-way, road id
SIDE = [
    ('loop-n', 'TOWN-MAIN-STREET', 'TOWN-MAIN-STREET', 0.04, False, 'TOWN-NORDURBOGI'),
    ('loop-s', 'TOWN-MAIN-STREET', 'TOWN-MAIN-STREET', 0.04, False, 'TOWN-SUDURBOGI'),
    ('torg', 'TOWN-MAIN-STREET', 'TOWN-MAIN-STREET', 0.03, False, 'TOWN-TORGVEGUR'),
    ('house', 'TOWN-MAIN-STREET', 'bulb:8592,2360', 0.03, False, 'TOWN-VILLA-ROAD'),
    ('skola', 'TOWN-MAIN-STREET', 'bulb:4240,5620', 0.03, False, 'TOWN-SKOLAVEGUR'),
    ('skola-oneway', 'bulb:4240,5620', 'bulb:5480,5530', 0.03, True, 'TOWN-SKOLAVEGUR-ONEWAY'),
    ('east-s', 'TOWN-EAST-LOOP', 'bulb:8272,7320', 0.03, False, 'TOWN-EAST-CULDESAC'),
    ('midbogi', 'TOWN-NORDURBOGI', 'TOWN-NORDURBOGI', 0.03, False, 'TOWN-MIDBOGI'),
    ('nw-a', 'TOWN-NORDURBOGI', 'bulb:1340,2460', 0.03, False, 'TOWN-NW-CULDESAC-1'),
    ('nw-b', 'TOWN-NORDURBOGI', 'bulb:3200,2940', 0.03, False, 'TOWN-NW-CULDESAC-2'),
    ('ne-a', 'TOWN-NORDURBOGI', 'bulb:7380,2360', 0.03, False, 'TOWN-NE-CULDESAC'),
    ('sudur-u', 'TOWN-SUDURBOGI', 'TOWN-SUDURBOGI', 0.03, False, 'TOWN-SUDURBOGI-INNER'),
    ('sw-a', 'TOWN-SUDURBOGI', 'bulb:1560,7320', 0.03, False, 'TOWN-SW-CULDESAC-1'),
    ('sw-b', 'TOWN-SUDURBOGI', 'bulb:3088,5888', 0.03, False, 'TOWN-SW-CULDESAC-2'),
]


def km_at(c):   # the airport's km from that corner (the plan's Sign B markers: 7 km west of the town square, 8 km east)
    return '7' if TP_.to_town(*c)[0] < 5600 else '8'


VILLA_END = None
for name, h0, h1, lf, oneway, rid in SIDE:
    P = town_tidy(name)
    signs, cross, ends, cuts = [], [], [], []
    e0 = e1 = 0.0
    for at_start, h in ((True, h0), (False, h1)):
        if h.startswith('bulb:'):
            P = bulb(P, at_start, tuple(float(v) for v in h[5:].split(',')))
            if at_start: e0 = 12.5
            else: e1 = 12.5
        else:
            P, c, e = meet(P, at_start, h); ends.append((at_start, c, h, e))
            if at_start: e0 = e
            else: e1 = e
    L = Along(P).len
    base = [dict(kind='line', side='right', edge=1, off=0.1, w=0.15), dict(kind='line', side='left', edge=1, off=0.1, w=0.15)] if oneway else two_paint()
    paint = [dict(p, **{'from': round(e0 + 1.0, 2), 'to': round(L - e1 - 1.0, 2)}) for p in base]
    for at_start, c, h, e in ends:
        tooth = e - 2.5 + 0.6             # (just out of the host's carriageway)
        if at_start:
            paint.append(dict(kind='teeth', at=round(tooth, 2), dir=-1, o0=-3.4, o1=-0.15))
            if h == 'TOWN-MAIN-STREET': signs.append(dict(at=round(tooth + 16.0, 2), km=km_at(c), dir=-1))
            cross.append(dict(at=round(e + 2.0, 2), w=3.0))
        else:
            paint.append(dict(kind='teeth', at=round(L - tooth, 2), dir=1, o0=0.15, o1=3.4))
            if h == 'TOWN-MAIN-STREET': signs.append(dict(at=round(L - tooth - 16.0, 2), km=km_at(c), dir=1))
            cross.append(dict(at=round(L - e - 2.0, 2), w=3.0))
    road(rid, P, step=4.0, lift=lf, paint=paint, crossings=cross, signs=signs, **(ONEWAY if oneway else TOWN2))
    ENDCUTS[rid] = [(0.0, e0), (L, e1)]
    TSTREET[rid] = host_info(P)
    if name == 'house': VILLA_END = P[-1]


# THE TOWN'S BUILDINGS made so far (owner, 2026-11-15: "place it into the world there as a town"), placed square to
# the town plan (north up): each model's own frame (its Blender x east, y north) in plan metres - Data/town-buildings.json,
# read by the game (TownBuildings.cpp) - and what they need from the roads: the dealership's forecourt and car lot down
# to Main Street (the pavement dropped at its entrance), the villa's drive from its turning circle to the garage.
_n = (TP_.to_plan(5000, 3000)[0] - TP_.to_plan(5000, 4000)[0], TP_.to_plan(5000, 3000)[1] - TP_.to_plan(5000, 4000)[1])
_l = math.hypot(*_n); NORTH = (_n[0] / _l, _n[1] / _l); EAST = (NORTH[1], -NORTH[0])
def frame_pt(o, u, v, bx, by): return (round(o[0] + u[0] * bx + v[0] * by, 2), round(o[1] + u[1] * bx + v[1] * by, 2))
BUILDINGS = {}
# the dealership (marker 3): its glass front (Blender y 0) faces north to Main Street 33 m away, its east end (x 0; the
# building runs west to x 66) 23 m short of the west loop. (The model's front faces its own -y: turned round.)
D_O = TP_.to_plan(1960, 4560); D_U = (-EAST[0], -EAST[1]); D_V = (-NORTH[0], -NORTH[1])
BUILDINGS['Dealership'] = dict(o=D_O, u=D_U, v=D_V, size=[-5.2, -3.2, 66.2, 30.1],
                               spot=dict(at=frame_pt(D_O, D_U, D_V, 24.0, -26.0), look=frame_pt(D_O, D_U, D_V, 30.0, 6.0)))
# its forecourt and car lot: paved from the front to Main Street's pavement, two rows of spaces facing the glass and the street
H = TSTREET['TOWN-MAIN-STREET']
c = min(H['d'], key=lambda p: math.dist(p, frame_pt(D_O, D_U, D_V, 30.0, -31.0)))
gap = math.dist(c, frame_pt(D_O, D_U, D_V, 30.0, 0.0)) - 6.0     # from the front to the pavement's back
lots.append(dict(frame=dict(o=list(D_O), u=list(D_U), v=list(D_V)), x0=-5.0, x1=66.0, y0=round(-gap, 2), y1=0.0, lift=0.03,
                 rows=[dict(x0=0.0, x1=60.0, y=-3.5, d=5.0, dir=-1, w=2.5), dict(x0=0.0, x1=60.0, y=round(-gap + 1.0, 2), d=5.0, dir=1, w=2.5)]))
# its entrance: the street's pavement dropped where the lot's aisle meets it (as the side streets' mouths)
i = min(range(len(H['d'])), key=lambda k: math.dist(H['d'][k], c))
side = 'right' if (frame_pt(D_O, D_U, D_V, 30.0, 0.0)[0] - c[0]) * (H['d'][min(i + 1, len(H['d']) - 1)][1] - H['d'][max(i - 1, 0)][1]) - \
       (frame_pt(D_O, D_U, D_V, 30.0, 0.0)[1] - c[1]) * (H['d'][min(i + 1, len(H['d']) - 1)][0] - H['d'][max(i - 1, 0)][0]) > 0 else 'left'
CUTS.setdefault('TOWN-MAIN-STREET', []).append((side, H['s'][i], 4.5))
# the villa (marker 8): its glass to the north, its garage door (the model's x 3.5, y -9.1; doors south) where the plan
# draws the garage on the lot, and its drive as the plan draws it - from the villa road's turning circle up across the
# lot to the garage, its last 8 m straight in to the doors
GD = TP_.to_plan(8790, 2045)
V_O = (round(GD[0] - EAST[0] * 3.5 + NORTH[0] * 9.2, 2), round(GD[1] - EAST[1] * 3.5 + NORTH[1] * 9.2, 2))
BUILDINGS['Villa'] = dict(o=V_O, u=EAST, v=NORTH, size=[-3.0, -18.6, 29.6, 18.0],
                          spot=dict(at=frame_pt(V_O, EAST, NORTH, -6.0, -32.0), look=frame_pt(V_O, EAST, NORTH, 10.0, 0.0)))
VB = next(B for B in BULBS if math.dist(B[0], TP_.to_plan(*TP_.bulb_centre(8592, 2360))) < 4.0)[0]
d0 = frame_pt(V_O, EAST, NORTH, 3.5, -9.2); d1 = frame_pt(V_O, EAST, NORTH, 3.5, -17.0)
dr = [d0, d1, ((d1[0] + VB[0]) / 2, (d1[1] + VB[1]) / 2), VB]
dr = bulb(list(reversed(dr)), True, (8592, 2360))
L_dr = Along(dr).len
road('TOWN-VILLA-DRIVE', dr, step=4.0, lift=0.03, left=[lane('Driving', 3.25, invert=True)], right=[lane('Driving', 3.25)],
     lights=[dict(side='right', edge=1, off=0.9, spacing=12, **{'from': 14.0, 'to': round(L_dr - 4.0, 2)})])
(ROOT / 'Data' / 'town-buildings.json').write_text(json.dumps({'source': 'Tools/make_roads.py', 'buildings': BUILDINGS}, indent=1), encoding='utf-8')

# the turning circles: a paved disc (its path a 5 m circle, the lane inward filling it), a pavement round it, open
# where a street comes in (the ring starts opposite its first street, so no opening straddles its start)
for n, (C, angs) in enumerate(BULBS):
    a0 = angs[0] + math.pi
    ring_b = [(round(C[0] + BR * math.cos(a0 + 2 * math.pi * k / 16), 2), round(C[1] + BR * math.sin(a0 + 2 * math.pi * k / 16), 2)) for k in range(16)]
    rid = 'TOWN-TURN-%d' % n
    road(rid, ring_b, closed=True, lift=0.04, **BULB)
    CUTS[rid] = [('right', BR * ((a - a0) % (2 * math.pi)), 4.4) for a in angs]
    TSTREET[rid] = dict(L=2 * math.pi * BR)


def sidewalk_keys(L, cuts, w=2.5):
    """A pavement's width along a street, narrowed away at each opening (3 m tapers either side; an opening at an end
    runs right to the end - no full-width sliver left there)."""
    ks = []
    for s_, half in sorted(cuts):
        a, b = s_ - half, s_ + half
        if a > 0.5: ks += [(max(0.0, a - 3.0), w), (a, z)]
        else: ks += [(0.0, z)]
        if b < L - 0.5: ks += [(b, z), (min(L, b + 3.0), w)]
        else: ks += [(L, z)]
    if not ks or ks[0][0] > 0.0: ks.insert(0, (0.0, w))
    if max(k[0] for k in ks) < L: ks.append((L, w))
    out = []
    for k in sorted(ks):
        if out and k[0] - out[-1][0] < 0.01: out[-1] = [out[-1][0], min(out[-1][1], k[1])]; continue
        out.append([round(k[0], 2), k[1]])
    return out


# the pavements opened at the mouths, and the street lights (32 m apart, the two sides staggered, none in a mouth)
for r in roads:
    if not r['id'].startswith('TOWN-') or r['id'] in ('TOWN-ROAD-NORTH', 'TOWN-ROAD-SOUTH', 'TOWN-ROUNDABOUT', 'TOWN-RB9'): continue
    info = TSTREET.get(r['id'], {})
    L = info.get('L') or Along([tuple(p) for p in r['pts']]).len
    cuts = CUTS.get(r['id'], [])
    for side in ('right', 'left'):
        mine = [(s, h) for sd, s, h in cuts if sd == side] + [(s, h) for s, h in ENDCUTS.get(r['id'], []) if h > 0]
        lanes = r['lanes'][side]
        if not lanes or lanes[-1]['type'] != 'Sidewalk': continue
        if mine: lanes[-1] = dict(lanes[-1], w=sidewalk_keys(L, mine))
        if r.get('closed'): continue
        segs, lo = [], 4.0
        for s, h in sorted(mine) + [(L + 4.0, 0.0)]:
            hi = s - h - 4.0
            if hi > lo: segs.append((lo, hi))
            lo = s + h + 4.0
        first = 8.0 if side == 'right' else 24.0
        for a, b in segs:
            st = first + 32.0 * math.ceil((a - first) / 32.0)
            if st <= b: r.setdefault('lights', []).append(dict(side=side, edge=2, off=-0.45, spacing=32, **{'from': round(st, 2), 'to': round(b, 2)}))
    if r.get('closed'):   # a turning circle: a few lights round its pavement, clear of the openings
        r['lights'] = []
        for k in range(4):
            s = L * (k + 0.5) / 4
            if all(min(abs(s - c[1]), L - abs(s - c[1])) > c[2] + 3.0 for c in cuts):
                r['lights'].append(dict(side='right', edge=2, off=-0.45, spacing=1000, **{'from': round(s, 2), 'to': round(s + 1, 2)}))
# Sign A (Main Street, about 135 m before the roundabout, for the cars driving west out of town) and Sign D (the road
# out of town, past the roundabout: the airport 7 km)
road_ms.setdefault('signs', []).append(dict(at=135.0, km='7', dir=-1))
sr.setdefault('signs', []).append(dict(at=round(Along([tuple(p) for p in sr['pts']]).len - 60.0, 2), km='7', dir=-1))
# zebras over Main Street by the shops, the town square and the park (the plan's)
for u in (3300, 4500, 6700, 7700):
    H = TSTREET['TOWN-MAIN-STREET']; c = TP_.to_plan(u, 4250)
    i = min(range(len(H['d'])), key=lambda k: math.dist(H['d'][k], c))
    road_ms.setdefault('crossings', []).append(dict(at=round(H['s'][i], 2), w=3.0))

out = ROOT / 'Data' / 'roads.json'
out.write_text(json.dumps({'source': 'Tools/make_roads.py', 'roads': roads, 'lots': lots, 'fills': FILLS}, indent=1, ensure_ascii=False), encoding='utf-8')
print('%s: %d roads, %d car parks; forecourt %.0f m (band %.0f-%.0f, terminal %.0f-%.0f)' % (out, len(roads), len(lots), A.len, s_ne0, s_sw0, s_t0, s_t1))
# our own land along the roads follows them (owner, 2026-11-14: extend a road and the ground under it comes with it):
# the band rebuilt from the roads just written (Tools/make_land_corridor.py; needs the terrain tiles on F:)
import subprocess, sys as _sys
import shutil as _sh
_sh.copyfile(out, ROOT / 'Content' / 'Data' / 'roads.json')
_sh.copyfile(ROOT / 'Data' / 'town-buildings.json', ROOT / 'Content' / 'Data' / 'town-buildings.json')   # (the game reads Content/Data first: it was copied by hand, 2026-11-15)
if '--no-land' not in _sys.argv:
    subprocess.run([_sys.executable, str(ROOT / 'Tools' / 'make_land_corridor.py')], check=False)
