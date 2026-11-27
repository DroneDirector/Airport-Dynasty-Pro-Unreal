# The forecourt's taxi ranks and the coach station's bays (to-do 234, West Wing part 2; design sheet
# Blueprints/Terminal/Changes/v2/s2_ground.py, SHEET 2e THE FORECOURT), all in the kerb lane (AIRPORT-FORECOURT, right
# side, lane 7: z -25..-21.5, the boxes 0.25 m in from its edges like the seven coach stands'):
#   - TAXI RANK WEST: 10 cabs in x -75..-5, its shelter + sign on the pavement, x -46..-34, z -20.5..-18
#   - TAXI RANK EAST: 11 cabs in x 158..238, its shelter + sign, x 192..204, z -20.5..-18
#   - 5 coach bays by the coach station, 13 x 3.3 m, 3 m apart, x 472..549 ("like your 7": yellow, 20 cm lines, BUSES ONLY)
# The pavement and the kerb lane east of x 241 (sheet 2d/2e) are already there: make_roads.py runs both on to x 560
# (2026-11-23). Not here: the sheet's pay machines (they wait for the car park's exit decision).
# The taxi boxes and the new coach bays are painted only ("park": false): no airport coach is parked in them (the seven
# stands keep the seven coaches, RoadNetwork.cpp BusStands). The shelters are Art/TaxiShelter (road prop "taxishelter",
# solid to people: "block").
# Used by Tools/make_roads.py (Data/roads.json); run on its own it writes the same into Data/roads.json and
# Data/Sites/chablais/roads.json in place (safe to run again: it replaces its own entries, marked "src"):
#   python Tools/forecourt_stands.py
import json, os

TAG = 'forecourt_stands'
CAB_LEN, CAB_GAP = 6.2, 0.8           # (a saloon taxi 4.8-5.0 m long, room to pull in and out of a row)


def extras(ex):
    """(paint, props) for the forecourt road; ex(x): how far along the road (m) plan x is (exactly)."""
    paint, props = [], []
    # the ranks run with the road (its distance grows toward the west): a rank's first box at its east end
    for x_east, n in ((-5.0, 10),):
        paint.append({'kind': 'boxes', 'side': 'right', 'lane': 7, 'n': n, 'len': CAB_LEN, 'gap': CAB_GAP, 'w': 0.15, 'color': 'yellow',
                      'text': ['TAXI'], 'park': False, 'from': ex(x_east), 'src': TAG})
    # TAXI RANK EAST (x 158..238) and the five old coach bays (x 472..549) were REMOVED on 2026-11-27: the bus station
    # now stands over x 180..520 (Tools/bus_station_forecourt.py, docs/Bus-Station-Plan.md) with its own 12 sawtooth
    # bays A1-A12 and 6 taxi spots. Both old rows sat inside that run and painted straight over the new bays, so
    # running this script put the old forecourt back. Do not restore them. The west rank and both shelters stand on.
    # the shelters: 12 x 2.5 m, 1 m in from the kerb (edge 8), their middles at x -40 and 198; one each ("spacing" past its end)
    for xc in (-40.0, 198.0):
        props.append({'mesh': 'taxishelter', 'side': 'right', 'edge': 8, 'off': 2.25, 'spacing': 1000, 'out': 1, 'block': True,
                      'from': ex(xc), 'to': ex(xc) + 1.0, 'src': TAG})
    return paint, props


def ex_from_json(road):
    """ex() from the road's own points (the forecourt's straight, y -46.75: between them the distance is linear)."""
    import math
    P = road['pts']; s = [0.0]
    for i in range(1, len(P)): s.append(s[-1] + math.dist(P[i], P[i - 1]))
    def ex(x):
        for i in range(1, len(P)):
            xa, xb = P[i - 1][0], P[i][0]
            if min(xa, xb) <= x <= max(xa, xb) and abs(xb - xa) > 1e-6: return round(s[i - 1] + (s[i] - s[i - 1]) * (x - xa) / (xb - xa), 2)
        raise ValueError('x %.1f is off the road' % x)
    return ex


if __name__ == '__main__':
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for f, ind in ((os.path.join(ROOT, 'Data', 'roads.json'), 1), (os.path.join(ROOT, 'Data', 'Sites', 'chablais', 'roads.json'), None)):
        R = json.load(open(f, encoding='utf-8'))
        fc = next(r for r in R['roads'] if r['id'] == 'AIRPORT-FORECOURT')
        paint, props = extras(ex_from_json(fc))
        fc['paint'] = [p for p in fc['paint'] if p.get('src') != TAG] + paint
        fc['props'] = [p for p in fc.get('props', []) if p.get('src') != TAG] + props
        with open(f, 'w', encoding='utf-8', newline='\n') as o:
            json.dump(R, o, indent=ind, ensure_ascii=False)
        print('%s: %d paint, %d props (%s)' % (os.path.relpath(f, ROOT), len(fc['paint']), len(fc['props']),
              ', '.join('%s from %.2f' % (p.get('text', [p.get('mesh')])[0], p['from']) for p in paint + props)))
