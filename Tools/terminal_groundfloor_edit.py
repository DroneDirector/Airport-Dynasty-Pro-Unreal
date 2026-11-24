# Terminal east, part 3, step 1: the East Wing's ground-floor rooms (their floor finishes, skirtings, door linings) in the terminal's
# layout: run by Tools/terminal_edit.py, last (after terminal_arrivals_edit.py), so every other node keeps its index. The models:
# Art/TerminalEast/make_groundfloor.py (Tools/import_east.py); which pieces: Art/TerminalEast/ground_place.json. They are made in
# the plan's metres, so they go in as they are (no translation), under F1.
_GP = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'ground_place.json')))
for _as in _GP['big']:
    N.append({'name': 'F1-EAST-GROUND/' + _as[7:].upper().replace('_', '-'), 'asset': _as, 'mesh': 90000 + len(N)})
    N[byname['F1']].setdefault('children', []).append(len(N) - 1)
log.append('new: the ground-floor rooms\' finishes, skirtings and door linings: %d models' % len(_GP['big']))
# Terminal east, part 3, step 2: the rooms fitted out (Art/TerminalEast/make_groundfloor_fit.py; which pieces where:
# ground_fit_place.json): the doorway signs (a model in the plan's metres, as it is), the furniture and the lamps' housings (instanced
# pieces), and the 19 door leaves as the terminal's own hinged doors ('D-...' with 'hinge' nodes, as D-F1-ACADEMY-1: closed, they open
# as you use them), in the doorways' oak linings (no frame of their own: the lining is it). The leaf: 4 mm over the threshold, 1 cm
# under the lining's head, 3 mm off each jamb; WC and family room pale, the police and staff security steel, the offices oak with a
# vision panel.
import math as _m5
_FP = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'ground_fit_place.json')))
def _gadd(n): N.append(n); N[byname['F1']].setdefault('children', []).append(len(N) - 1); return len(N) - 1
for _as in _FP['big']: _gadd({'name': 'F1-EAST-FIT/' + _as[7:].upper().replace('_', '-'), 'asset': _as, 'mesh': 90000 + len(N)})
for _nm, _as, _x, _y, _z, _r in _FP['place']:
    _n = {'name': 'F1-EAST-FIT/' + _nm, 'asset': _as, 'mesh': 90000 + len(N), 'translation': [-_x, _y, _z]}
    if _r: _n['rotation'] = [0.0, -_m5.sin(_m5.radians(_r) / 2), 0.0, _m5.cos(_m5.radians(_r) / 2)]
    _gadd(_n)
_LEAF = {'F1-EAST-WC': 521, 'F1-EAST-FAMILY': 521, 'F1-STAFFSEC': 119, 'F1-POLICE': 119}
_PANE = ('F1-CUSTOMS-E-OFFICE', 'F1-PC-OFFICE', 'F1-PC-SECOND', 'F1-CREW-CENTRE', 'F1-CANTEEN', 'F1-GROUND-OFF')
for _id, _room, _x, _z, _ax, _w in _FP['doors']:
    _cw = _w - 0.08; _two = _w > 1.4; _lw = (_cw / 2 if _two else _cw) - 0.006; _h = _cw / 2 - 0.003
    _d = _gadd({'name': _id, 'translation': [-_x, 0.02, _z], 'children': []})
    if _ax == 'x': N[_d]['rotation'] = [0.0, -_m5.sqrt(0.5), 0.0, _m5.sqrt(0.5)]
    for _sx in ((1, -1) if _two else (1,)):
        _k = [len(N) + 1, len(N) + 2] + ([len(N) + 3] if _room in _PANE else [])
        N.append({'name': 'hinge#' + _id, 'translation': [_sx * _h, 0, 0], 'children': _k}); N[_d]['children'].append(len(N) - 1)
        N.append({'name': 'box#leaf', 'translation': [-_sx * _lw / 2, 1.09, 0], 'scale': [_lw, 2.18, 0.05], 'mesh': _LEAF.get(_room, 554)})
        N.append({'name': 'box#handle', 'translation': [-_sx * (_lw - 0.1), 1.0, 0], 'scale': [0.03, 0.03, 0.16], 'mesh': 184})
        if _room in _PANE: N.append({'name': 'box#pane', 'translation': [-_sx * _lw / 2, 1.5, 0], 'scale': [min(0.42, _lw - 0.4), 0.6, 0.06], 'mesh': 152})
log.append('new: the ground-floor rooms fitted out: %d models (%d places, lamp housings in), %d hinged doors' % (len(_FP['models']), len(_FP['place']), len(_FP['doors'])))
