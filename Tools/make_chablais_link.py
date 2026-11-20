"""The road from the end of the airport's road behind the terminal to the town (Rennaz / Villeneuve), along REAL roads:
swisstopo's swissTLM3D road centre lines (open data, "source: swisstopo"), the shortest way over roads of 4 m and wider,
turned into the game's plan metres and added to Data/Sites/chablais/roads.json as CHABLAIS-LINK (lanes and markings as
the airport road's). Also draws docs/Switzerland/Chablais-link-road.png: the road over the 10 cm aerial photo.
  python -I Tools/make_chablais_link.py <tlm_roads.json (cut by the caller)> [town_lat town_lon]"""
import json, math, sys
from pathlib import Path
import numpy as np, networkx as nx
from pyproj import Transformer
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SD = ROOT / 'Data' / 'Sites' / 'chablais'
TLM = json.load(open(sys.argv[1]))
TOWN = (float(sys.argv[2]), float(sys.argv[3])) if len(sys.argv) > 3 else (46.3885, 6.9245)
AF = json.loads((ROOT / 'Data' / 'airfield.json').read_text(encoding='utf-8'))
S = json.loads((SD / 'site.json').read_text(encoding='utf-8'))['airport']
MX = (AF['runway']['x0'] + AF['runway']['x1']) / 2; MY = AF['runway']['y']; H = math.radians(S['hdg'])
to_ll = Transformer.from_crs(2056, 4326, always_xy=True); to_lv = Transformer.from_crs(4326, 2056, always_xy=True)

def plan_to_ll(x, y):
    dx, dy = x - MX, y - MY
    e = dx * math.sin(H) + dy * math.sin(H - math.pi / 2); n = dx * math.cos(H) + dy * math.cos(H - math.pi / 2)
    return S['lon'] + e / (111320 * math.cos(math.radians(S['lat']))), S['lat'] + n / 111320

def ll_to_plan(lo, la):
    e = (lo - S['lon']) * 111320 * math.cos(math.radians(S['lat'])); n = (la - S['lat']) * 111320
    dx = e * math.sin(H) + n * math.cos(H); dy = e * math.sin(H - math.pi / 2) + n * math.cos(H - math.pi / 2)
    return dx + MX, dy + MY

R = json.loads((SD / 'roads.json').read_text(encoding='utf-8'))
back = [r for r in R['roads'] if r['id'] == 'AIRPORT-BACK-TO-TOWN'][0]
start_plan = back['pts'][-1]
start = to_lv.transform(*plan_to_ll(*start_plan))
goal = to_lv.transform(TOWN[1], TOWN[0])

# the graph: road pieces of 4 m and wider (and the roads into a roundabout), nodes at their points (snapped to 0.5 m)
OK = {'4m Strasse', '6m Strasse', '8m Strasse', '10m Strasse', 'Verbindung'}
G = nx.Graph(); key = lambda p: (round(p[0] * 2) / 2, round(p[1] * 2) / 2)
for r in TLM:
    if r['kind'] not in OK: continue
    P = r['pts']
    for a, b in zip(P[:-1], P[1:]): G.add_edge(key(a), key(b), w=math.dist(a, b), name=r['name'], kind=r['kind'])
nodes = np.array(list(G.nodes))
near = lambda p: tuple(nodes[np.argmin(np.hypot(nodes[:, 0] - p[0], nodes[:, 1] - p[1]))])
s0, g0 = near(start), near(goal)
print('start %.0f m from a real road, goal %.0f m' % (math.dist(s0, start), math.dist(g0, goal)))
path = nx.shortest_path(G, s0, g0, weight='w')
names = []
for a, b in zip(path[:-1], path[1:]):
    n = G[a][b]['name']
    if n and (not names or names[-1] != n): names.append(n)
L = sum(G[a][b]['w'] for a, b in zip(path[:-1], path[1:]))
print('route %.0f m over %d points: %s' % (L, len(path), ' > '.join(names)))

# into plan metres; the first point is the airport road's end (joined with a short straight piece)
pts = [list(start_plan)] + [list(ll_to_plan(*to_ll.transform(*p))) for p in path]
# thinned: no two points closer than 6 m (MetaRoad builds pieces between them)
thin = [pts[0]]
for p in pts[1:]:
    if math.dist(p, thin[-1]) >= 6.0: thin.append(p)
if thin[-1] != pts[-1]: thin.append(pts[-1])
link = {k: v for k, v in back.items() if k not in ('pts', 'crossings', 'props', 'lights')}
link['id'] = 'CHABLAIS-LINK'; link['pts'] = [[round(x, 2), round(y, 2)] for x, y in thin]
link['note'] = 'swissTLM3D real roads: ' + ' > '.join(names)
# street lights one side all along (owner: posts both sides at the airport, one side on the approach)
link['lights'] = [{'side': 'right', 'edge': 0, 'off': 0.6, 'spacing': 35.0, 'from': 0.0, 'to': round(L, 1)}]
link['paint'] = [p for p in back.get('paint', []) if p.get('kind') == 'line']
for p in link['paint']: p['from'] = 0.0; p['to'] = round(L, 1)
R['roads'] = [r for r in R['roads'] if r['id'] != 'CHABLAIS-LINK'] + [link]
for d in (SD, ROOT / 'Content' / 'Data' / 'Sites' / 'chablais'):
    (d / 'roads.json').write_text(json.dumps(R, ensure_ascii=False), encoding='utf-8')
print('CHABLAIS-LINK: %d points' % len(thin))

# the picture: the road over the aerial photo (the site's photo tiles), the airport's roads, the town's spot
T = json.loads((Path(json.loads((SD / 'site.json').read_text(encoding='utf-8'))['imageryDev']) / 'tiles.json').read_text())
D = Path(json.loads((SD / 'site.json').read_text(encoding='utf-8'))['imageryDev'])
allp = [plan_to_ll(*p) for r in R['roads'] for p in r['pts']] + [(TOWN[1], TOWN[0])]
lo0, lo1 = min(p[0] for p in allp) - 0.004, max(p[0] for p in allp) + 0.004; la0, la1 = min(p[1] for p in allp) - 0.003, max(p[1] for p in allp) + 0.003
L7 = T['maxLevel']; z = T['webLevel0'] + L7; z0 = T['webLevel0']; TS = 32   # (the finest level, each 256 px tile shrunk to TS px)
def web(lo, la): return (lo + 180) / 360 * (1 << z), (1 - math.asinh(math.tan(math.radians(la))) / math.pi) / 2 * (1 << z)
wx0, wy0 = web(T['west'] + 1e-9, T['north'] - 1e-9); X0 = int(wx0 / (1 << L7)); Y0 = int(wy0 / (1 << L7))
ax, ay = web(lo0, la1); bx, by = web(lo1, la0)
W = int((bx - ax) * TS); Hh = int((by - ay) * TS)
img = Image.new('RGB', (W, Hh), (40, 40, 40))
for tx in range(int(ax), int(bx) + 1):
    for ty in range(int(ay), int(by) + 1):
        f = D / str(L7) / str(tx - X0 * (1 << L7)) / ('%d.jpg' % (ty - Y0 * (1 << L7)))
        if f.exists(): img.paste(Image.open(f).resize((TS, TS), Image.BILINEAR), (int((tx - ax) * TS), int((ty - ay) * TS)))
dr = ImageDraw.Draw(img)
def P(lo, la): x, y = web(lo, la); return ((x - ax) * TS, (y - ay) * TS)
for r in R['roads']:
    c = (255, 150, 0) if r['id'] == 'CHABLAIS-LINK' else (80, 230, 255)
    dr.line([P(*plan_to_ll(*p)) for p in r['pts']], fill=c, width=6 if r['id'] == 'CHABLAIS-LINK' else 4)
b = AF['boundary']
dr.polygon([P(*plan_to_ll(x, y)) for x, y in ((b['x0'], b['y0']), (b['x1'], b['y0']), (b['x1'], b['y1']), (b['x0'], b['y1']))], outline=(255, 80, 200), width=4)
tp = P(TOWN[1], TOWN[0]); dr.ellipse([tp[0] - 14, tp[1] - 14, tp[0] + 14, tp[1] + 14], outline=(255, 220, 0), width=5)
f = ImageFont.truetype('arial.ttf', 30)
dr.text((tp[0] + 20, tp[1] - 18), 'your town', fill=(255, 220, 0), font=f, stroke_width=3, stroke_fill=(0, 0, 0))
dr.text((20, 20), 'Orange: the new road to the town, on the real roads (swissTLM3D): ' + ' > '.join(names) + '  (%.1f km)' % (L / 1000), fill=(255, 255, 255), font=f, stroke_width=3, stroke_fill=(0, 0, 0))
dr.text((20, 60), 'Cyan: the airport roads (forecourt, roundabout, the road behind the terminal). Pink: the airport fence. Photo: swisstopo SWISSIMAGE', fill=(255, 255, 255), font=f, stroke_width=3, stroke_fill=(0, 0, 0))
out = ROOT / 'docs' / 'Switzerland' / 'Chablais-link-road.png'; img.save(out); print('picture', img.size, out)
