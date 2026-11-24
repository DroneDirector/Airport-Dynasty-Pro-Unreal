# West Wing part 1 (to-do 234): the claim hall and the arrivals hall fitted out (Art/TerminalAnnex/make_west_fitout.py; which
# pieces where: Art/TerminalAnnex/westfit_place.json). Run by Tools/terminal_edit.py, last, so every other node keeps its index.
# The two big models are in the plan's metres (no translation); the instanced pieces (the ground floor's WC and office
# furniture, the tax free's bays, its island and pay point, the ATM, the SIM kiosk) where the json puts them; the accessible
# WC's and the lost and found store's doors as the terminal's own hinged doors (a 'D-...' node with its 'hinge' node: closed,
# they open as you use them), in their oak linings; the rooms' own lamps under their ceilings (3.2 m).
import math as _mw
_WF = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalAnnex', 'westfit_place.json')))
def _wadd(n): N.append(n); N[byname['F1']].setdefault('children', []).append(len(N) - 1); return len(N) - 1
for _as in _WF['big']: _wadd({'name': 'F1-WESTFIT/' + _as[6:].upper().replace('_', '-'), 'asset': _as, 'mesh': 90000 + len(N)})
for _nm, _as, _x, _y, _z, _r in _WF['place']:
    _n = {'name': 'F1-WESTFIT/' + _nm, 'asset': _as, 'mesh': 90000 + len(N), 'translation': [-_x, _y, _z]}
    if _r: _n['rotation'] = [0.0, -_mw.sin(_mw.radians(_r) / 2), 0.0, _mw.cos(_mw.radians(_r) / 2)]
    _wadd(_n)
for _id, _x, _z, _ax, _w, _mesh in _WF['doors']:
    _cw = _w - 0.08; _lw = _cw - 0.006; _h = _cw / 2 - 0.003
    _d = _wadd({'name': _id, 'translation': [-_x, 0.02, _z], 'children': []})
    if _ax == 'x': N[_d]['rotation'] = [0.0, -_mw.sqrt(0.5), 0.0, _mw.sqrt(0.5)]
    N.append({'name': 'hinge#' + _id, 'translation': [_h, 0, 0], 'children': [len(N) + 1, len(N) + 2]}); N[_d]['children'].append(len(N) - 1)
    N.append({'name': 'box#leaf', 'translation': [-_lw / 2, 1.09, 0], 'scale': [_lw, 2.18, 0.05], 'mesh': _mesh})
    N.append({'name': 'box#handle', 'translation': [-(_lw - 0.1), 1.0, 0], 'scale': [0.03, 0.03, 0.16], 'mesh': 184})
for _k, (_x, _y, _z) in enumerate(_WF['lamps']):
    N.append({'name': 'F1-WESTFIT/LAMP-%03d' % (_k + 1), 'translation': [-_x, _y, _z], 'children': [len(N) + 1]})
    N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186}); N[byname['F1']]['children'].append(len(N) - 2)
log.append('new: the West Wing fitted out (claim hall, arrivals hall): %d models, %d places, %d hinged doors, %d lamps' % (len(_WF['models']), len(_WF['place']), len(_WF['doors']), len(_WF['lamps'])))
