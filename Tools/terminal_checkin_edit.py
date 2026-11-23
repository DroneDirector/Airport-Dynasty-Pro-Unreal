# The Grand Hall's check-in (owner, 2026-11-23, to-do 225: "between every single check-in stand there are no glitches in
# between them", "this blue tape in front everywhere is not natural"): run by Tools/terminal_edit.py, last, so every other
# node keeps its index. Checked by Tools/check_checkin.py.
#  1 the old scale boxes go (0.6 m wide, they stood 0.22 m into the next desk with their fronts on its front: the flicker);
#    the desks are 1.31 m wide instead of 1.7, so the weigh belt (DepartureBelts: 0.78 m east of the desk's middle) runs
#    in the gap between two desks, clear of both, and not through the desk
#  2 the screens stand on a foot (they hung 6 cm over the top), the number signs' posts end in the signs, and the numbers
#    read right from both sides (the passengers saw them mirrored)
#  3 steel cores inside the belts' side panels: the walking map (which leaves the moving belts out) keeps the gaps shut
#  4 the queue tape ran at waist height along all the desks, 2.4 m before them, across every lane and over the ends of its
#    posts: gone. Instead a short barrier line (three posts, two belts) between two airlines' desks, running out from the
#    desks: every lane stays open from the hall, and the passengers' way out along the next lane (PaxFlow) too
_CI_BOX = {'white': 116, 'steel': 119, 'black': 122, 'navy': 115}; _CI_POST = 125
_ci_n = [0, 0, 0]
def _ci_add(p, n):
    N.append(n); N[p].setdefault('children', []).append(len(N) - 1); return len(N) - 1
def _ci_box(p, mat, x, y, z, w, h, d): _ci_add(p, {'name': 'box#checkin', 'translation': [x, y, z], 'scale': [w, h, d], 'mesh': _CI_BOX[mat]})
_ci_hall = next(i for i, n in enumerate(N) if n['name'] == 'F1-HALL')
_ci_desks = [c for c in N[_ci_hall]['children'] if N[c]['name'].startswith('F1-CHECKIN/DESK-')]
assert len(_ci_desks) == 20
for _d in _ci_desks:
    # (a desk is turned half round: its own +x is east in the plan, its +z towards the passengers)
    _kids = [k for k in N[_d]['children']]
    assert abs(N[_kids[0]]['scale'][0] - 1.7) < 1e-6 and abs(N[_kids[1]]['scale'][0] - 1.74) < 1e-6, N[_d]['name']
    for _k, _w in ((_kids[0], 1.31), (_kids[1], 1.34)):
        _n = N[_k]; _t = _n['translation']; _s = _n['scale']
        # (the west edge: 0.85 m west of the desk's middle, as it was; the east edge 2 cm clear of the belt's trim)
        _n['translation'] = [(-0.85 + 0.46) / 2 if _w < 1.32 else (-0.87 + 0.47) / 2, _t[1], _t[2]]; _n['scale'] = [_w, _s[1], _s[2]]
    _n = N[_kids[0]]; _n['translation'][1] = 0.53; _n['scale'][1] = 1.06                                  # (its top 1.5 cm up inside the worktop, not on its underside)
    for _k in list(_kids):
        _n = N[_k]
        if _n.get('scale', [0])[0] == 0.6: N[_d]['children'].remove(_k); _ci_n[0] += 1                       # the old scale
        elif _n.get('scale', [0])[0] == 0.5 and _n['mesh'] == 122: _mon = _n['translation']                   # the screen
        elif _n.get('scale', [0])[0] == 0.04: _n['translation'] = [_n['translation'][0], 1.275, 0]; _n['scale'] = [0.04, 0.38, 0.014]   # the sign's post
        elif _n['name'].startswith('dn'):
            # the number: one sheet, so the passengers read it mirrored - one for each side of a thin board
            _t = _n['translation']; _n['translation'] = [_t[0], _t[1], -0.011]; _sign = (_n, _t)
    _n, _t = _sign; _ci_add(_d, dict(_n, translation=[_t[0], _t[1], 0.011], rotation=[0, 1, 0, 0])); _ci_box(_d, 'black', _t[0], _t[1], 0, 0.496, 0.296, 0.018)
    _ci_box(_d, 'black', _mon[0], 1.101, _mon[2], 0.22, 0.016, 0.15)        # the screen's foot
    _ci_box(_d, 'black', _mon[0], 1.135, _mon[2], 0.06, 0.07, 0.018)        # and its neck
    for _sx in (-1, 1): _ci_box(_d, 'steel', 0.78 + _sx * 0.28, 0.58, -1.075, 0.010, 0.26, 1.15)   # the belt panels' cores
for _c in list(N[_ci_hall]['children']):
    if N[_c]['name'].startswith('F1-HALL/QUEUE-POST-'): N[_ci_hall]['children'].remove(_c); _ci_n[1] += 1
# the barrier lines (plan x): the west end, 4 | 5, 7 | 8, the east end of island 1; the same of island 2 (11 | walk-through,
# 13 | 14, 16 | 17, the east end): 0.52 m off the queue beside them and off the way out
for _i, _x in enumerate((48.81, 57.13, 66.49, 72.73, 75.81, 85.17, 91.41, 99.73)):
    for _j, _z in enumerate((14.2, 12.6, 11.0)):
        _p = _ci_add(_ci_hall, {'name': 'F1-HALL/QUEUE-POST-%03d' % (_i * 3 + _j + 1), 'translation': [-_x, 0, _z], 'children': []})
        _ci_add(_p, {'name': 'cyl1-18#checkin', 'translation': [0, 0.485, 0], 'scale': [0.08, 0.95, 0.08], 'mesh': _CI_POST})
        _ci_add(_p, {'name': 'cyl1-18#checkin', 'translation': [0, 0.02, 0], 'scale': [0.32, 0.03, 0.32], 'mesh': _CI_POST})
        if _j < 2: _ci_box(_p, 'navy', 0, 0.91, -0.8, 0.004, 0.05, 1.6)
        _ci_n[2] += 1
log.append('check-in: %d old scale parts and %d queue posts out, 20 desks narrowed, %d lane posts' % tuple(_ci_n))
