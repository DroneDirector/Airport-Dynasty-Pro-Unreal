# The West Wing's automatic overlap check (to-do 234): Tools/check_east.py's three checks (flicker, cuts, routes) run on the
# claim hall and the arrivals hall (plan x -81..1, z -5..53). The new parts are the fit-out (F1-WESTFIT...) and the wing itself
# (F1-ARRIVALS/...); the routes are the arriving passengers' ways (escalators' foot, each belt's front, through customs, the
# meeting rail to the kerb doors) and the cafe's, the store's and the office's doors.
#   F:/Tools/Blender-4.2/blender.exe -b --python Tools/check_west.py        -> Saved/west-check.json and the summary
#   (WEST_NEW='F1-ARRIVALS/' checks only the wing as it was, for a before/after)
import os, re
_p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'check_east.py'); _s = open(_p, encoding='utf-8').read()
_s = re.sub(r"REG = \(90\.0, 600\.0, -32\.0, 54\.0\)", "REG = (-81.0, 1.0, -5.0, 53.0)", _s)
_a = _s.index('NEW = ('); _b = _s.index('SKIP = (')
_s = _s[:_a] + "NEW = tuple(os.environ.get('WEST_NEW', 'F1-WESTFIT,D-F1-WESTFIT,F1-ARRIVALS/').split(','))\n" + _s[_b:]
_a = _s.index("ROADS = D('roads.json')"); _b = _s.index('hitz = {}')
_z = '''zone = []
def leg(nm, xa, za, xb, zb, r=0.6):
    zone.append((nm, min(xa, xb) - r, max(xa, xb) + r, 0.05, 2.0, min(za, zb) - r, max(za, zb) + r))
for bx in (-60.8, -48.15, -35.5, -22.85, -10.2):
    leg('escalator foot to belt %.1f' % bx, -77.18, 35.86, -77.18, 28.0); leg('to belt %.1f' % bx, -77.18, 28.0, bx - 2.6, 28.0, 0.4)
    leg('belt %.1f to customs' % bx, bx - 2.6, 18.5, -36.5, 18.5, 0.4)
leg('green channel', -36.5, 18.5, -36.5, 8.0); leg('to the kerb', -36.5, 8.0, -36.5, -3.8)
leg('kerb doors', -46.0, -3.8, -34.0, -3.2, 0.0)
leg('store door x 0 z 40..41.4', -1.4, 40.0, -0.2, 41.4, 0.0); leg('office door x -12', -12.6, 6.6, -11.4, 7.9, 0.0)
leg('office door x -70', -70.6, 6.6, -69.4, 7.9, 0.0)
leg('aisle north of the belts', -66.0, 43.3, -5.0, 43.3, 1.6)
'''
_s = _s[:_a] + _z + _s[_b:]
_s = _s.replace("'east-check.json'", "os.environ.get('WEST_OUT', 'west-check.json')")
exec(compile(_s, _p, 'exec'))
