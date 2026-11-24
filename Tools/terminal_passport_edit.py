# Terminal east, part 5: the ways through passport control and the transfer lane's way out as hinged glass doors, in the
# terminal's layout: run by Tools/terminal_edit.py, last (after terminal_escape_edit.py), so every other node keeps its index.
# Where: Art/TerminalEast/make_concourse.py's HINGED (conc_place.json "hinged"): [id, plan x, y, z, the wall's axis, clear width,
# leaf, gap at each jamb]. Each is the terminal's own hinged door ('D-...' with 'hinge' nodes, as part 3's and part 4's) under F2,
# shut: a glass leaf (east_c_leaf_glass, low, between the booths / e-gates; east_c_tleaf_glass, full height, the lane's pair),
# a steel rail at its foot and head and a pull handle on each face. Over 1.4 m: two leaves, 1 cm apart. Which way they open and
# for whom (boarding pass, passport check, the swing gate's side) is not here: to-do 212 / 214.
import math as _m7
_HP = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'conc_place.json')))
_LV = {'low': ('east_c_leaf_glass', 0.10, 1.52, 1.0), 'tall': ('east_c_tleaf_glass', 0.04, 2.18, 1.05)}   # asset, rails' middles, handle
_nh = 0
for _id, _x, _y, _z, _ax, _w, _kind, _gap in _HP['hinged']:
    _as, _rb, _rt, _hy = _LV[_kind]; _two = _w > 1.4
    _lw = round((_w - 2 * _gap - 0.01) / 2 if _two else _w - 2 * _gap, 4); _h = round(_w / 2 - _gap, 4)
    N.append({'name': _id, 'translation': [-_x, _y, _z], 'children': []}); _d = len(N) - 1; N[byname['F2']]['children'].append(_d)
    if _ax == 'x': N[_d]['rotation'] = [0.0, -_m7.sqrt(0.5), 0.0, _m7.sqrt(0.5)]
    for _sx in ((1, -1) if _two else (1,)):
        N.append({'name': 'hinge#' + _id, 'translation': [_sx * _h, 0, 0], 'children': [len(N) + 1 + _i for _i in range(5)]}); N[_d]['children'].append(len(N) - 1)
        N.append({'name': 'F2-CONC/LEAF/' + _id[3:], 'asset': _as, 'mesh': 90000 + len(N), 'translation': [-_sx * _lw / 2, 0, 0], 'scale': [_lw, 1, 1]})
        for _ry in (_rb, _rt): N.append({'name': 'box#rail', 'translation': [-_sx * _lw / 2, _ry, 0], 'scale': [_lw, 0.04, 0.04], 'mesh': 184})
        for _s in (-1, 1): N.append({'name': 'box#handle', 'translation': [-_sx * (_lw - 0.08), _hy, _s * 0.027], 'scale': [0.025, 0.25, 0.03], 'mesh': 184})
        _nh += 1
log.append('new: passport control\'s and the transfer lane\'s hinged glass doors: %d doors, %d leaves, shut' % (len(_HP['hinged']), _nh))
