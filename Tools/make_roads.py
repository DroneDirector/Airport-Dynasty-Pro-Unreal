# The road network's data (owner, 2026-11-14; HANDOVER 3b, to-do 55): every road from the owner's map plans
# (Art/Maps/Airport-Road-Town-Overview-v11.png, Town-Plan-v7.png, Airport-Only-v11.png) written to Data/roads.json,
# which the game builds with MetaRoad Free when it starts (Source/.../RoadNetwork.cpp). Never placed by hand: run this
# again and the whole network is rebuilt the same.
#   python Tools/make_roads.py      (then copy Data/roads.json to Content/Data)
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
    r = {'id': id, 'pts': [list(p) for p in pts], 'lanes': {'left': left, 'right': right}}
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
BT_PTS = [(RB[0], RB[1] - 20.0)] + [moved(*p) for p in [(-798, -222), (-786, -251), (-765, -288), (-742, -317), (-710, -341),
          (-676, -358), (-597, -375), (-282, -375), (35, -375), (298, -375), (545, -380), (781, -421), (948, -476), (1139, -576)]]
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

out = ROOT / 'Data' / 'roads.json'
out.write_text(json.dumps({'source': 'Tools/make_roads.py', 'roads': roads, 'lots': lots}, indent=1, ensure_ascii=False), encoding='utf-8')
print('%s: %d roads, %d car parks; forecourt %.0f m (band %.0f-%.0f, terminal %.0f-%.0f)' % (out, len(roads), len(lots), A.len, s_ne0, s_sw0, s_t0, s_t1))
