# Terminal east, part 4: the closed escape stairs (sheet 9c) in the terminal's layout: run by Tools/terminal_edit.py, last (after
# terminal_groundfloor_edit.py), so every other node keeps its index. The models: Art/TerminalEast/make_escape_stairs.py (one box and
# one steel stair, made round x 0, Tools/import_east.py); where: Art/TerminalEast/esc_place.json. Each of the 11 stairs is the same
# two models moved along x (instanced), under F1; its three doors are the terminal's own hinged doors as part 3's (D-F1-...), shut:
# a steel leaf (two over 1.4 m), a handle, a narrow vision panel and a kick plate. -HEAD: Floor 2's gallery into the box; -HALL: the
# foot to its hall; -OUT: the foot's way out through the promenade wall (alarmed; no alarm logic here).
import math as _m6
_EP = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'esc_place.json')))
def _eadd(n): N.append(n); N[byname['F1']].setdefault('children', []).append(len(N) - 1); return len(N) - 1
for _nm, _as, _x, _y, _z in _EP['place']:
    _eadd({'name': 'F1-EAST-ESC/' + _nm, 'asset': _as, 'mesh': 90000 + len(N), 'translation': [-_x, _y, _z]})
for _id, _room, _x, _y, _z, _ax, _w in _EP['doors']:
    _cw = _w - 0.08; _two = _w > 1.4; _lw = (_cw / 2 if _two else _cw) - 0.006; _h = _cw / 2 - 0.003
    _d = _eadd({'name': _id, 'translation': [-_x, _y + 0.01, _z], 'children': []})
    if _ax == 'x': N[_d]['rotation'] = [0.0, -_m6.sqrt(0.5), 0.0, _m6.sqrt(0.5)]
    for _sx in ((1, -1) if _two else (1,)):
        N.append({'name': 'hinge#' + _id, 'translation': [_sx * _h, 0, 0], 'children': [len(N) + 1 + _i for _i in range(4)]})
        N[_d]['children'].append(len(N) - 1)
        N.append({'name': 'box#leaf', 'translation': [-_sx * _lw / 2, 1.09, 0], 'scale': [_lw, 2.18, 0.05], 'mesh': 119})
        N.append({'name': 'box#handle', 'translation': [-_sx * (_lw - 0.1), 1.0, 0], 'scale': [0.03, 0.03, 0.16], 'mesh': 184})
        N.append({'name': 'box#pane', 'translation': [-_sx * (_lw - 0.3), 1.45, 0], 'scale': [0.2, 0.7, 0.06], 'mesh': 152})
        N.append({'name': 'box#kick', 'translation': [-_sx * _lw / 2, 0.15, 0], 'scale': [_lw - 0.04, 0.25, 0.056], 'mesh': 184})
log.append('new: the escape stairs: %d boxes (%d places), %d hinged fire doors' % (len(_EP['stairs']), len(_EP['place']), len(_EP['doors'])))
