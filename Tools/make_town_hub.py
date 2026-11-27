"""The town coach and taxi hub at Chablais (to-do 373 step A, owner 2026-11-27) and the coaches' ONE-WAY LOOP to it and back.
Changes Data/Sites/chablais/roads.json (and the Content copy) in place; safe to run again (it removes what it made first).

The hub: the open meadow on the Rhone plain at the south edge of Villeneuve, between the Route du Simplon (CHABLAIS-LINK)
and the Grand Canal, behind the side street that runs along it (seen on the 10 cm SWISSIMAGE photo: grass, no buildings).
  TOWN-HUB-SPUR    the link's roundabout -> the side street (the real street, traced from the photo)
  TOWN-HUB-STREET  the side street along the meadow, its north end back onto the link
  TOWN-HUB         the hub itself: a one-way aisle, IN at its south end, OUT at its north end; coach bays (13 m) along a
                   5.5 m platform on the right with the shelter's place, taxi bays (6.2 m) on the left, kerbs, street
                   lights both sides, bay words painted; the ground under it levelled ("levels": make_land_corridor.py)
  TERMINAL-RETURN  the way back into the forecourt from its town side: from the road behind the car park (AIRPORT-BACK-
                   TO-TOWN) up to the forecourt east of the terminal, so a coach comes back facing the way the parked
                   coaches face and pulls straight into its bay - the loop never runs a road backwards and has no U-turn
"routes": the two legs of the loop as data (ACoaches::SendOnRoute): each step a lane of a road between two plan points.
    python Tools/make_town_hub.py
"""
import json, math
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
FILES = [ROOT / 'Data' / 'Sites' / 'chablais' / 'roads.json', ROOT / 'Content' / 'Data' / 'Sites' / 'chablais' / 'roads.json']
R = json.loads(FILES[0].read_text(encoding='utf-8'))
MINE = ('TOWN-HUB-SPUR', 'TOWN-HUB-STREET', 'TOWN-HUB', 'TERMINAL-RETURN')
R['roads'] = [r for r in R['roads'] if r['id'] not in MINE]
L = {r['id']: r for r in R['roads']}
link, back, fore, rb = L['CHABLAIS-LINK'], L['AIRPORT-BACK-TO-TOWN'], L['AIRPORT-FORECOURT'], L['AIRPORT-ROUNDABOUT']

def length(p): return sum(math.dist(p[i], p[i + 1]) for i in range(len(p) - 1))
def nearest(pts, q): return min(range(len(pts)), key=lambda i: math.dist(pts[i], q))
def plain(rid, pts, lights_side='right'):
    """a two-lane town road like the link (lanes, centre/edge lines), street lights one side"""
    r = {k: json.loads(json.dumps(v)) for k, v in link.items() if k not in ('pts', 'note', 'lights', 'paint')}
    r['id'] = rid; r['pts'] = [[round(x, 2), round(y, 2)] for x, y in pts]; n = length(pts)
    r['paint'] = [dict(p, **{'from': 0.0, 'to': round(n, 1)}) for p in link.get('paint', []) if p.get('kind') == 'line']
    r['lights'] = [{'side': lights_side, 'edge': 2, 'off': 1.0, 'spacing': 35.0, 'from': 6.0, 'to': round(n - 6, 1)}]
    return r

lp = link['pts']
ia, ib = nearest(lp, (2163.0, -1535.0)), nearest(lp, (2328.0, -1598.0))
LA, LB = lp[ia], lp[ib]
print('link joins: spur at %s (%.1f m off the drawn spot), street end at %s (%.1f m)' % (LA, math.dist(LA, (2163, -1535)), LB, math.dist(LB, (2328, -1598))))
T = (2142.0, -1626.0)                                   # the side street's T-junction with the spur
spur = plain('TOWN-HUB-SPUR', [LA, ((LA[0] + T[0]) / 2, (LA[1] + T[1]) / 2), T])
street = plain('TOWN-HUB-STREET', [(2036.0, -1628.0), (2090.0, -1626.6), T, (2190.0, -1634.0), (2240.0, -1643.0),
                                   (2280.0, -1648.0), (2302.0, -1642.0), (2316.0, -1627.0), LB])

# ---- the hub's aisle, a U: in at the west mouth on the street, south down the west leg, east along the bottom, north up
# the east leg, out at the east mouth. Coach bays (13 m) all round the OUTSIDE (the right, along a 5.5 m platform), taxi
# bays (6.2 m) all round the INSIDE (the left); the widths open after the mouths and close before them
XW, XE_, YB, RT = 2060.0, 2155.0, -1805.0, 22.0         # the legs' x, the bottom's y, the turns' radius (a coach's easy turn)
def arc(cx, cy, r, a0, a1, n=8): return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]
pts = [(XW, -1628.4)] + [(XW, y) for y in range(-1636, int(YB + RT) - 1, -6)] + arc(XW + RT, YB + RT, RT, 180.0, 270.0)
pts += [(x, YB) for x in range(int(XW + RT) + 6, int(XE_ - RT) - 2, 6)] + arc(XE_ - RT, YB + RT, RT, 270.0, 360.0)
pts += [(XE_, y) for y in range(int(YB + RT) + 6, -1640, 6)] + [(XE_, -1636.0), (XE_ + 1.0, -1630.6)]
pts2 = []
for p in pts:
    if not pts2 or math.dist(p, pts2[-1]) > 2.5: pts2.append(p)
hub_len = length(pts2)
def d_of(q):
    i = nearest(pts2, q); return length(pts2[:i + 1])
d_in, d_out = d_of((XW, -1650.0)), d_of((XE_, -1650.0))
legs = [(d_in + 1.0, d_of((XW, YB + RT))), (d_of((XW + RT, YB)), d_of((XE_ - RT, YB))), (d_of((XE_, YB + RT)), d_out - 1.0)]
def sched(w, t=6.0): return [[0, 0.05], [round(d_in - t, 2), 0.05], [round(d_in, 2), w], [round(d_out, 2), w], [round(d_out + t, 2), 0.05], [round(hub_len, 2), 0.05]]
hub = {'id': 'TOWN-HUB', 'pts': [[round(x, 2), round(y, 2)] for x, y in pts2], 'step': 2.0,
       'note': 'to-do 373: the town coach + taxi hub, ONE WAY (in at the west mouth, out at the east); coach bays outside, taxis inside; Tools/make_town_hub.py',
       'lanes': {'right': [{'type': 'Driving', 'w': 4.0}, {'type': 'Parking', 'w': sched(3.6)}, {'type': 'Sidewalk', 'w': sched(5.5), 'kerb': 15}],
                 'left': [{'type': 'Driving', 'w': 3.75}, {'type': 'Parking', 'w': sched(2.7)}, {'type': 'Sidewalk', 'w': sched(3.5), 'kerb': 15}]}}
CB, CG, TB, TG = 13.0, 2.0, 6.2, 0.8
hub['paint'] = [
    {'kind': 'line', 'side': 'right', 'edge': 1, 'off': 0.1, 'w': 0.15, 'from': 0.0, 'to': round(hub_len, 1)},
    {'kind': 'line', 'side': 'left', 'edge': 1, 'off': 0.1, 'w': 0.15, 'from': 0.0, 'to': round(hub_len, 1)},
    {'kind': 'dash', 'side': 'right', 'edge': 0, 'off': 0.0, 'w': 0.12, 'dash': 3, 'gap': 6, 'from': 0.0, 'to': round(hub_len, 1)}]
bay_d = []; nt = 0
for a, b in legs:
    n = int((b - a - 2.0 + CG) // (CB + CG)); m = int((b - a - 2.0 + TG) // (TB + TG))
    if n > 0:
        hub['paint'].append({'kind': 'boxes', 'side': 'right', 'lane': 1, 'n': n, 'len': CB, 'gap': CG, 'w': 0.2, 'color': 'yellow', 'text': ['BUS'], 'park': False, 'from': round(a + 1.0, 2)})
        bay_d += [a + 1.0 + i * (CB + CG) + CB - 0.8 for i in range(n)]
    if m > 0:
        hub['paint'].append({'kind': 'boxes', 'side': 'left', 'lane': 1, 'n': m, 'len': TB, 'gap': TG, 'w': 0.15, 'color': 'yellow', 'text': ['TAXI'], 'park': False, 'from': round(a + 1.0, 2)})
        nt += m
nc = len(bay_d)
hub['lights'] = [{'side': 'right', 'edge': 3, 'off': -0.6, 'spacing': 24.0, 'from': round(d_in + 4, 1), 'to': round(d_out - 2, 1)},
                 {'side': 'left', 'edge': 3, 'off': -0.6, 'spacing': 24.0, 'from': round(d_in + 16, 1), 'to': round(d_out - 2, 1)}]
# the coach bays' stop points (plan m): the front of each box, on the bay lane's middle (for SendOnRoute and step B)
def on(d, side_off):
    s = 0.0
    for a, b in zip(pts2[:-1], pts2[1:]):
        l = math.dist(a, b)
        if s + l >= d:
            t = (d - s) / l; fx, fy = (b[0] - a[0]) / l, (b[1] - a[1]) / l
            # (the right of the way in the plan's right-handed frame: (fy, -fx))
            return [round(a[0] + (b[0] - a[0]) * t + fy * side_off, 2), round(a[1] + (b[1] - a[1]) * t - fx * side_off, 2)]
        s += l
    return [round(pts2[-1][0], 2), round(pts2[-1][1], 2)]
hub['bays'] = [on(d, 4.0 + 1.8) for d in bay_d]
hub['shelter'] = {'at': on((legs[0][0] + legs[0][1]) / 2, 4.0 + 3.6 + 3.2), 'len': 30.0, 'note': 'the shelter\'s place on the west platform (its model: a later step)'}
XS, Y = XW, -1650.0

# ---- the way back into the forecourt from its town side (the forecourt's own point at x 1340.6 is its junction)
bi = nearest(back['pts'], (1174.0, -464.0)); FJ = fore['pts'][1]
ret = plain('TERMINAL-RETURN', [back['pts'][bi], (1215.0, -420.0), (1262.0, -350.0), (1300.0, -260.0), (1326.0, -170.0), (1336.0, -110.0), FJ], 'left')

# ---- the roundabout: right-hand traffic runs it anticlockwise seen from above. The plan frame is right-handed like
# east/north (make_chablais_link.py), so anticlockwise in plan is anticlockwise on the ground; neither side is inverted,
# both lanes run the points' way: the outer lane (right 0) is the one to enter and leave by
rp = rb['pts']; area = sum(rp[i][0] * rp[(i + 1) % len(rp)][1] - rp[(i + 1) % len(rp)][0] * rp[i][1] for i in range(len(rp))) / 2
assert area > 0 and not any(l.get('invert') for l in rb['lanes']['right']), 'the roundabout does not run anticlockwise'
rb_side, rb_lane = 'right', 0
print('roundabout: points run anticlockwise (area %.0f m2); the loop takes its outer lane, right 0' % area)

fl = fore['pts']; d_fore = [0.0]
for a, b in zip(fl[:-1], fl[1:]): d_fore.append(d_fore[-1] + math.dist(a, b))
def fore_at(d):
    for i in range(len(fl) - 1):
        if d_fore[i + 1] >= d:
            t = (d - d_fore[i]) / (d_fore[i + 1] - d_fore[i]); return [round(fl[i][0] + (fl[i + 1][0] - fl[i][0]) * t, 2), round(fl[i][1] + (fl[i + 1][1] - fl[i][1]) * t, 2)]
    return fl[-1]
def pt(a): return a if isinstance(a, str) else [round(a[0], 2), round(a[1], 2)]   # 'coach': where the coach stands, 'bay': its bay
def step(road, side, lane, a, b, join=8.0): return {'road': road, 'side': side, 'lane': lane, 'from': pt(a), 'to': pt(b), 'join': join}
R['routes'] = {
    'note': 'to-do 373: the coaches\' one-way loop (owner): out of the terminal bays FORWARD, round the roundabout, behind the car park, the link, the hub; back by the link, behind the car park, TERMINAL-RETURN into the forecourt from its town side, straight into the bay. Each step: a lane of a road from the lane point nearest "from" to the one nearest "to" ("coach": where the coach stands; "bay": its bay - the TOWN-HUB "bays" or its terminal stand; "bay-N": N m before it along the lane), in its driving direction (closed roads wrap); "join": the metres each side of a change rounded into one curve. ACoaches::SendOnRoute.',
    'coach-to-town': [
        step('AIRPORT-FORECOURT', 'right', 6, 'coach', fore_at(2500.0), 10.0),
        step('AIRPORT-FORECOURT', 'right', 0, fore_at(2560.0), fl[-1], 12.0),
        step('AIRPORT-ROUNDABOUT', rb_side, rb_lane, fl[-1], back['pts'][0], 10.0),
        step('AIRPORT-BACK-TO-TOWN', 'right', 0, back['pts'][0], back['pts'][-1]),
        step('CHABLAIS-LINK', 'right', 0, lp[0], LA, 10.0),
        step('TOWN-HUB-SPUR', 'right', 0, LA, T, 10.0),
        step('TOWN-HUB-STREET', 'left', 0, T, (2052.0, -1628.5), 10.0),
        step('TOWN-HUB', 'right', 0, pts2[0], 'bay-24', 8.0),
        step('TOWN-HUB', 'right', 1, 'bay-12', 'bay', 6.0)],
    'coach-to-terminal': [
        step('TOWN-HUB', 'right', 0, 'coach', pts2[-1], 10.0),
        step('TOWN-HUB-STREET', 'right', 0, (2190.0, -1636.5), LB, 10.0),
        step('CHABLAIS-LINK', 'left', 0, LB, lp[0]),
        step('AIRPORT-BACK-TO-TOWN', 'left', 0, back['pts'][-1], back['pts'][bi], 10.0),
        step('TERMINAL-RETURN', 'right', 0, back['pts'][bi], FJ, 12.0),
        step('AIRPORT-FORECOURT', 'right', 0, FJ, fore_at(980.0), 10.0),
        step('AIRPORT-FORECOURT', 'right', 6, fore_at(1060.0), 'bay-24', 10.0),
        step('AIRPORT-FORECOURT', 'right', 7, 'bay-12', 'bay', 6.0)]}
R['levels'] = [l for l in R.get('levels', []) if l.get('id') != 'TOWN-HUB'] + [
    {'id': 'TOWN-HUB', 'x0': 2040.0, 'y0': -1822.0, 'x1': 2172.0, 'y1': -1636.0, 'margin': 8.0, 'blend': 35.0,
     'note': 'the hub\'s level pad (Tools/make_land_corridor.py): a town-size margin and blend, not the airfield\'s 120 / 450 m (that would flatten half of Villeneuve)'}]
R['roads'] += [spur, street, hub, ret]
for f in FILES: f.write_text(json.dumps(R, ensure_ascii=False), encoding='utf-8')
km = lambda m: '%.2f km' % (m / 1000)
print('hub: %d coach bays, %d taxi bays, aisle %.0f m' % (nc, nt, hub_len))
plan_to_ll = None   # (make_chablais_link.py has the converter but runs when imported)
def run(rid, a, b):
    P = L[rid]['pts'] if rid in L else hub['pts']; i, j = nearest(P, a), nearest(P, b)
    if j < i and L.get(rid, {}).get('closed'): return length(P[i:] + P[:j + 1])
    return length(P[min(i, j):max(i, j) + 1])
L.update({r['id']: r for r in (spur, street, hub, ret)})
to_town = (run('AIRPORT-FORECOURT', fore_at(1770.0), fl[-1]) + 15 + run('AIRPORT-BACK-TO-TOWN', back['pts'][0], back['pts'][-1])
           + run('CHABLAIS-LINK', lp[0], LA) + run('TOWN-HUB-SPUR', LA, T) + run('TOWN-HUB-STREET', T, (XW, -1628.4)) + bay_d[len(bay_d) // 2])
back_len = (d_out + 6 + run('TOWN-HUB-STREET', (XE_, -1630.6), LB) + run('CHABLAIS-LINK', LB, lp[0]) + run('AIRPORT-BACK-TO-TOWN', back['pts'][-1], back['pts'][bi])
            + length(ret['pts']) + run('AIRPORT-FORECOURT', FJ, fore_at(1770.0)))
c = ((XW + XE_) / 2, (YB - 1636.0) / 2)
print('hub middle plan (%.0f, %.0f) = %s; by road from the terminal stands %.2f km, back %.2f km' % (c[0], c[1], ('%.5f N %.5f E' % plan_to_ll(*c)[::-1]) if plan_to_ll else '?', to_town / 1000, back_len / 1000))
