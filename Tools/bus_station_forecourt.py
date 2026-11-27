"""The forecourt's BUS STATION and TAXI RANK ground (docs/Bus-Station-Plan.md, step 2 - the layout in data).

Changes Data/Sites/chablais/roads.json (and the Content copy) and Data/holomap-buildings.json in place; safe to run
again (it takes out everything it made first, marked "src": "bus_station", and its own lane width keys):
    python Tools/bus_station_forecourt.py

Where it goes (AIRPORT-FORECOURT, right side = the terminal side; the road's distance s = 1859.92 - plan x, exactly as
Tools/forecourt_stands.py measures it):
  the run        x 180 .. 520 (s 1339.92 .. 1679.92), 340 m of frontage, tapered over 20 m at each end
  the lanes      the two Parking lanes go: lane 5 (3.1 m) is closed over the run, lane 7 (3.5 m) opens out to 9.7 m as
                 the BAY APRON, the Bus lane (lane 6, 3.5 m) stays as the through lane. A new ISLAND PLATFORM (lane 8,
                 "Apron" = paved, kerb 0.18 m) 8 m wide sits between the apron and the terminal pavement, which keeps
                 10.4 m there - so the ground reaches the terminal's landside wall (y 0) and no new sheet is laid.
  the bays       12 sawtooth bays A1-A12, each 3.6 m wide and 14 m long on a 25 deg sawtooth: 9.18 m of apron, the bays
                 3.6 / sin 25 = 8.52 m apart along the road, numbers painted on the deck (RoadNetwork.cpp "bays")
  the taxi rank  6 boxes 2.5 x 6.0 m with a TAXI legend in the kerb lane at x 99 .. 140, a feeder lane marked up to
                 them and a head-of-rank bar at their front end. NO taxi is drawn or parked ("park": false - the
                 owner: "don't draw the taxis, just a taxi spot"). The two taxishelter props stay where they are.
  crossings      yellow, 3 m, like the three at the main doors: three onto the platform over the station run and one
                 more at x 127.5 toward the doors
  lights         a lamp on the platform at every bay's dividing line and the existing kerb row split around them, all
                 through AStreetLights (its own distance limit, no flares)
  the pad        "levels" BUS-STATION, the TOWN-HUB margin 8 / blend 35, so the deck is flat
"""
import json, math, os, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
F = os.path.join(ROOT, 'Data', 'Sites', 'chablais', 'roads.json')
TAG = 'bus_station'
S_OF_X = 1859.92                      # s = S_OF_X - x along the forecourt's straight (Tools/forecourt_stands.py ex())
X0, X1 = 180.0, 520.0                 # the station's run
TAPER = 20.0
S0, S1 = S_OF_X - X1, S_OF_X - X0     # 1339.92 .. 1679.92
T0, T1 = S0 - TAPER, S1 + TAPER
APRON, PLATFORM, WALK = 9.7, 8.0, 10.4    # the bay apron, the island platform, the pavement left to the terminal wall
BAYS, BAY_W, BAY_D, ANG = 12, 3.6, 14.0, 25.0
TH = math.radians(ANG)
PITCH = BAY_W / math.sin(TH)                                  # 8.52 m along the road between two bays
NOSE = BAY_D * math.cos(TH)                                   # 12.69 m along the road from a bay's tail to its nose
FRONT = BAY_D * math.sin(TH) + BAY_W * math.cos(TH)           # 9.18 m of apron the sawtooth needs
BAY_FROM = round((S0 + S1) / 2 - ((BAYS - 1) * PITCH + NOSE) / 2, 2)   # centred in the run (and so under the vault)
TAXI_FROM, TAXI_N, TAXI_LEN, TAXI_GAP, TAXI_BOX = 1714.0, 6, 6.0, 1.0, 2.5   # x 145.9 .. 104.9, clear of the crossing at 1758.6
TAXI_END = TAXI_FROM + TAXI_N * (TAXI_LEN + TAXI_GAP) - TAXI_GAP
LANE5, LANE7, PLAT, SIDEWALK = 5, 7, 8, 9     # the lanes after the platform is put in at 8

R = json.load(open(F, encoding='utf-8'))
road = {r['id']: r for r in R['roads']}['AIRPORT-FORECOURT']
right = road['lanes']['right']

# ---- the lanes: take out what this script made last time, then lay it again ----
right[:] = [l for l in right if l.get('src') != TAG]
def keys(lane, add):
    """the lane's width keys with ours over the run replaced"""
    w = lane['w'] if isinstance(lane['w'], list) else [[0.0, lane['w']], [2604.0, lane['w']]]
    w = [k for k in w if not (T0 - 0.01 <= k[0] <= T1 + 0.01)] + [[round(a, 2), b] for a, b in add]
    lane['w'] = sorted(w, key=lambda k: k[0])

keys(right[5], [(T0, 3.1), (S0, 0.05), (S1, 0.05), (T1, 3.1)])          # the inner Parking lane closed over the run
keys(right[7], [(T0, 3.5), (S0, APRON), (S1, APRON), (T1, 3.5)])        # the kerb lane opened out as the bay apron
keys(right[8], [(T0, 13.1), (S0, WALK), (S1, WALK), (T1, 13.1)])        # the pavement gives the platform its width
# "Sidewalk", not "Apron": a platform is a raised pavement with a kerb - people stand and walk on it (the paving
# material, and the walking ground the passengers get), while an Apron/Median zone is ground nobody is sent over
right.insert(PLAT, {'type': 'Sidewalk', 'w': [[0.0, 0.05], [round(T0, 2), 0.05], [round(S0, 2), PLATFORM],
                                           [round(S1, 2), PLATFORM], [round(T1, 2), 0.05], [2604.0, 0.05]],
                    'kerb': 18, 'src': TAG,
                    'note': "the bus station's island platform, 8 m, paved, 0.18 m kerb (docs/Bus-Station-Plan.md)"})

def edge(k, s):
    """how far out from the path lane edge k is at s (the same sum RoadNetwork.cpp Edge() makes)"""
    o = 0.0
    for lane in right[:k]:
        w = lane['w']
        if not isinstance(w, list):
            o += w; continue
        if s <= w[0][0]: o += w[0][1]
        elif s >= w[-1][0]: o += w[-1][1]
        else:
            for i in range(len(w) - 1):
                if w[i][0] <= s <= w[i + 1][0]:
                    t = (s - w[i][0]) / max(w[i + 1][0] - w[i][0], 1e-6)
                    o += w[i][1] + (w[i + 1][1] - w[i][1]) * (t * t * (3 - 2 * t)); break   # (the engine eases, flat at the keys)
    return o

E7, E8, E9 = edge(7, BAY_FROM), edge(8, BAY_FROM), edge(9, BAY_FROM)
# ---- the markings ----
road['paint'] = [p for p in road['paint'] if p.get('src') != TAG]
# The kerb lane's old boxes go: the station's 12 bays are the coaches' stands now (the plan: "the route steps in routes
# end at these") and the taxi rank needs the frontage by the doors. Out go the old east taxi rank and the five bays by
# the old coach station (both inside the run) and all seven BUSES ONLY coach stands between x 20 and x 550 - the "bays"
# marking gives 12 stands back in their place (RoadNetwork.cpp BusStands).
def gone(p):
    if p.get('kind') != 'boxes' or p.get('lane') != LANE7: return False
    f = p.get('from', 0.0); e = f + p['n'] * (p['len'] + p['gap'])
    if 'BUSES' in (p.get('text') or [''])[0]: return 1300.0 < f < 1860.0
    return p.get('src') == 'forecourt_stands' and e > T0 and f < T1
road['paint'] = [p for p in road['paint'] if not gone(p)]
# the 12 car bays in the inner Parking lane stood at x 154 .. 235, under the station: they move west of the doors,
# clear of the three crossings (lane 5 keeps its 3.1 m from s 1699.92 to 1939.87)
for p in road['paint']:
    if p.get('kind') == 'boxes' and p.get('lane') == LANE5 and abs(p.get('from', 0.0) - 1624.42) < 0.1: p['from'] = 1817.0
paint = [
    {'kind': 'bays', 'side': 'right', 'lane': LANE7, 'n': BAYS, 'bw': BAY_W, 'depth': BAY_D, 'angle': ANG,
     'inset': 0.25, 'w': 0.2, 'color': 'yellow', 'prefix': 'A', 'first': 1,
     'from': BAY_FROM, 'to': round(S1, 2), 'src': TAG},
    # the platform's tactile edge strip, inside its kerb all along the station
    {'kind': 'line', 'side': 'right', 'edge': PLAT, 'off': 0.45, 'w': 0.5, 'from': round(S0, 2), 'to': round(S1, 2), 'src': TAG},
    # the taxi rank: the boxes, the feeder lane up to them, the rank's kerb-side line, the head of the rank
    {'kind': 'boxes', 'side': 'right', 'lane': LANE7, 'n': TAXI_N, 'len': TAXI_LEN, 'gap': TAXI_GAP, 'across': TAXI_BOX,
     'w': 0.15, 'color': 'yellow', 'text': ['TAXI'], 'park': False, 'from': round(TAXI_FROM, 2), 'src': TAG},
    {'kind': 'dash', 'side': 'right', 'edge': 7, 'off': -0.25, 'w': 0.12, 'dash': 2.0, 'gap': 2.0,
     'from': round(TAXI_FROM - 34.0, 2), 'to': round(TAXI_FROM, 2), 'src': TAG},
    {'kind': 'line', 'side': 'right', 'edge': 7, 'off': -0.25, 'w': 0.15,
     'from': round(TAXI_FROM, 2), 'to': round(TAXI_END + 1.5, 2), 'src': TAG},
    {'kind': 'bar', 'side': 'right', 'at': round(TAXI_END + 1.5, 2), 'o0': 21.9, 'o1': 25.1, 'w': 0.3,
     'color': 'yellow', 'src': TAG},
]
road['paint'] += paint

# ---- the crossings: onto the island platform over the station, and one more toward the main doors ----
road['crossings'] = [c for c in road['crossings'] if c.get('src') != TAG]
cross = []
for s in (S_OF_X - 440.0, S_OF_X - 240.0, S_OF_X - 186.0):
    cross.append({'at': round(s, 2), 'w': 3.0, 'o0': 8.0, 'o1': round(edge(PLAT, s), 2), 'color': 'yellow', 'src': TAG})
    cross.append({'at': round(s, 2), 'w': 3.0, 'o0': -4.5, 'o1': 4.5, 'color': 'yellow', 'src': TAG})
for s in (S_OF_X - 127.5,):
    cross.append({'at': round(s, 2), 'w': 3.0, 'o0': 8.0, 'o1': 25.25, 'color': 'yellow', 'src': TAG})
    cross.append({'at': round(s, 2), 'w': 3.0, 'o0': -4.5, 'o1': 4.5, 'color': 'yellow', 'src': TAG})
road['crossings'] += cross

# ---- the lights: a lamp at every bay's dividing line, the existing kerb row split round them ----
road['lights'] = [l for l in road['lights'] if l.get('src') != TAG]
KEEP0, KEEP1 = round(BAY_FROM - 24.0, 2), round(BAY_FROM + (BAYS - 1) * PITCH + NOSE + 12.0, 2)
old = [l for l in road['lights'] if l.get('side') == 'right' and l.get('edge') == 8 and l.get('off') == 0.6]
for l in old:
    f, t, sp = l.get('from', 0.0), l.get('to', 2604.0), l.get('spacing', 30.0)
    l['to'] = KEEP0
    n = math.ceil((KEEP1 - f) / sp)
    road['lights'].append({'side': 'right', 'edge': 8, 'off': 0.6, 'spacing': sp, 'from': round(f + n * sp, 2),
                           'to': t, 'src': TAG})
road['lights'].append({'side': 'right', 'edge': PLAT, 'off': 1.6, 'spacing': round(PITCH, 3),
                       'from': round(BAY_FROM + NOSE - BAY_W * math.sin(TH), 2),
                       'to': round(BAY_FROM + (BAYS - 1) * PITCH + NOSE, 2), 'src': TAG,
                       'note': 'a lamp over every bay, on the platform at the bays\' dividing lines'})

# ---- the level pad, like TOWN-HUB's ----
R['levels'] = [l for l in R.get('levels', []) if l.get('id') != 'BUS-STATION'] + [
    {'id': 'BUS-STATION', 'x0': X0 - 12.0, 'y0': -34.0, 'x1': X1 + 12.0, 'y1': -1.0, 'margin': 8.0, 'blend': 35.0,
     'note': "the bus station's level pad (Tools/make_land_corridor.py): the deck, the bays and the platform flat, "
             "the TOWN-HUB margin and blend"}]

json.dump(R, open(F, 'w', encoding='utf-8'), ensure_ascii=False)
c = os.path.join(ROOT, 'Content', 'Data', 'Sites', 'chablais', 'roads.json')
if os.path.isfile(c): shutil.copyfile(F, c)
print('forecourt: lanes 5/7/8 retapered, platform %.1f m at lane %d, apron %.1f m (the sawtooth needs %.2f m), '
      'pavement %.1f m; x %.0f .. %.0f' % (PLATFORM, PLAT, APRON, FRONT, WALK, X0, X1))
print('bays A1-A%d: s %.2f .. %.2f (x %.1f .. %.1f), %.2f m apart, nose %.2f m along the road; apron edges %.2f .. %.2f m out'
      % (BAYS, BAY_FROM, BAY_FROM + (BAYS - 1) * PITCH + NOSE, S_OF_X - BAY_FROM,
         S_OF_X - (BAY_FROM + (BAYS - 1) * PITCH + NOSE), PITCH, NOSE, E7, E8))
print('taxi rank: %d boxes %.1f x %.1f m, x %.1f .. %.1f, head of rank at x %.1f (no taxi drawn)'
      % (TAXI_N, TAXI_BOX, TAXI_LEN, S_OF_X - TAXI_FROM, S_OF_X - TAXI_END, S_OF_X - TAXI_END - 1.5))
print('%d crossings, %d light rows, %d paint entries' % (len(cross), len(road['lights']), len(paint)))

# ---- the holomap: the changed ground on the hologram (Data/holomap-buildings.json) ----
HP = os.path.join(ROOT, 'Data', 'holomap-buildings.json')
H = json.load(open(HP, encoding='utf-8'))
segs = []
def line(a, b, glass=0): segs.append([round(a[0], 2), round(a[1], 2), round(b[0], 2), round(b[1], 2), glass])
def at(s, o): return (S_OF_X - s, -46.75 + o)          # the plan point of (s along the road, metres out from the path)
for o in (edge(LANE7, (S0 + S1) / 2), edge(PLAT, (S0 + S1) / 2), edge(SIDEWALK, (S0 + S1) / 2)):
    line(at(S0, o), at(S1, o))                          # the carriageway edge, the platform's kerb, the pavement's back
line(at(S0, edge(LANE7, S0)), at(S0, edge(SIDEWALK, S0)))
line(at(S1, edge(LANE7, S1)), at(S1, edge(SIDEWALK, S1)))
for b in range(BAYS):                                   # every bay: its two sides
    s = BAY_FROM + b * PITCH; oi = edge(LANE7, s) + 0.25
    c = [(0.0, oi), (NOSE, oi + BAY_D * math.sin(TH)), (NOSE - BAY_W * math.sin(TH), oi + FRONT), (-BAY_W * math.sin(TH), oi + BAY_W * math.cos(TH))]
    for i in range(4):
        line(at(s + c[i][0], c[i][1]), at(s + c[(i + 1) % 4][0], c[(i + 1) % 4][1]))
for t in range(TAXI_N):                                 # the taxi spots
    s = TAXI_FROM + t * (TAXI_LEN + TAXI_GAP); m = (21.75 + 25.25) / 2
    for i, (a, b) in enumerate((((0.0, m - TAXI_BOX / 2), (TAXI_LEN, m - TAXI_BOX / 2)), ((TAXI_LEN, m - TAXI_BOX / 2), (TAXI_LEN, m + TAXI_BOX / 2)),
                                ((TAXI_LEN, m + TAXI_BOX / 2), (0.0, m + TAXI_BOX / 2)), ((0.0, m + TAXI_BOX / 2), (0.0, m - TAXI_BOX / 2)))):
        line(at(s + a[0], a[1]), at(s + b[0], b[1]))
line(at(TAXI_END + 1.5, 21.9), at(TAXI_END + 1.5, 25.1), 1)      # the head of the rank
H = [x for x in H if x.get('id') != 'BUS-STATION'] + [
    {'id': 'BUS-STATION', 'label': 'BUS STATION', 'floors': [{'label': 'FORECOURT', 'y0': -0.3, 'y1': 99.0, 'segs': segs}]}]
json.dump(H, open(HP, 'w', encoding='utf-8'), separators=(',', ':'))
ch = os.path.join(ROOT, 'Content', 'Data', 'holomap-buildings.json')
if os.path.isfile(ch): shutil.copyfile(HP, ch)
print('holomap BUS STATION: %d lines' % len(segs))
