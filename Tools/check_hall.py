# The Grand Hall's automatic overlap check (to-do 234/213, West Wing part 3): Tools/check_east.py's three checks (flicker,
# cuts, routes) run on the Grand Hall (plan x 35..115, z -1..31). The new parts are the hall's self-service fittings
# (F1-HALLFIT...: service desk, trolley bays, e-gates and their fences); the routes are what sheet 2b keeps clear: the three
# entrance doors' ways in, 4 m in front of (and behind) the check-in islands, the six columns, the four lanes through each
# security door's e-gates, the service desk's front, the trolley bays' fronts and the kiosk rows' fronts.
#   F:/Tools/Blender-4.2/blender.exe -b --python Tools/check_hall.py        -> Saved/hall-check.json and the summary
#   (HALL_NEW='F1-HALL' checks the hall's own contents too, for a before/after; HALL_OUT names the json)
import os, re
_p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'check_east.py'); _s = open(_p, encoding='utf-8').read()
_s = re.sub(r"REG = \(90\.0, 600\.0, -32\.0, 54\.0\)", "REG = (35.0, 115.0, -1.0, 31.0)", _s)
_a = _s.index('NEW = ('); _b = _s.index('SKIP = (')
_s = _s[:_a] + "NEW = tuple(os.environ.get('HALL_NEW', 'F1-HALLFIT').split(','))\n" + _s[_b:]
_a = _s.index("ROADS = D('roads.json')"); _b = _s.index('hitz = {}')
_z = '''zone = []
def leg(nm, xa, za, xb, zb, r=0.6):
    zone.append((nm, min(xa, xb) - r, max(xa, xb) + r, 0.05, 2.0, min(za, zb) - r, max(za, zb) + r))
for dx in (50.0, 75.0, 101.0): leg('entrance %.0f in' % dx, dx, 0.3, dx, 11.9, 1.0)
leg('4 m south of the islands', 47.5, 11.97, 99.5, 15.97, 0.0); leg('4 m north of the islands', 47.5, 20.76, 99.5, 24.76, 0.0)
for cx in (46.0, 75.0, 104.0):
    for cz in (6.0, 22.0): leg('column %.0f,%.0f' % (cx, cz), cx, cz, cx, cz, 1.0)
for dx in (62.0, 78.0):
    for lx in (-2.1, -0.7, 0.7, 2.1): leg('security %.0f lane %+.1f' % (dx, lx), dx + lx, 24.8, dx + lx, 29.9, 0.3)
leg('service desk front', 107.6, 24.8, 107.6, 29.6, 0.4)
for bx in (45.5, 111.3): leg('trolley bay %.0f front' % bx, bx - 2.5, 2.2, bx + 2.5, 2.2, 0.4)
for kx in (61.2, 88.2): leg('kiosk row %.1f front' % kx, kx - 3.75, 6.6, kx + 3.75, 6.6, 0.5)
'''
_s = _s[:_a] + _z + _s[_b:]
_s = _s.replace("'east-check.json'", "os.environ.get('HALL_OUT', 'hall-check.json')")
exec(compile(_s, _p, 'exec'))
