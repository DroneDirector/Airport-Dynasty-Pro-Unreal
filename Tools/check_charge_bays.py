"""Data check: the charging bays - no parked bus overlaps another, each bay its own charger, and a bus standing in the next
bay does not count as standing in this one (GSECharge.cpp InBay: 4.2 m aside, 16 m along)."""
import json, math, os
A = json.load(open(os.path.join(os.path.dirname(__file__), '..', 'Data', 'vehicle-area.json'), encoding='utf-8'))
B = [p for p in A['places'] if p.get('charge')]; bad = 0
BUSES = os.path.join(os.path.dirname(__file__), '..', 'Data', 'apron-buses.json')
home = json.load(open(BUSES, encoding='utf-8')).get('buses', [])
for p in B:
    n = [c for c in A['chargers'] if c.get('bay') == p['name']]
    if len(n) != 1: bad += 1; print('bay %s has %d chargers' % (p['name'], len(n)))
    a = math.radians(p['heading']); f = (math.cos(a), math.sin(a))
    for q in B + [dict(x=h['x'], y=h['y'], name=h['name']) for h in home]:
        if q is p: continue
        dx, dy = q['x'] - p['x'], q['y'] - p['y']; l, s = dx * f[0] + dy * f[1], -dx * f[1] + dy * f[0]
        if abs(l) < 16 and abs(s) < 4.2:
            own = math.hypot(dx, dy) < 2
            if not own: bad += 1
            print('bay %s: %s %s (%.1f m along, %.1f m aside)' % (p['name'], q['name'], 'has its place there' if own else 'OVERLAPS', l, s))
print('%d bays, %d findings' % (len(B), bad))
