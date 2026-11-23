# Terminal part 2a (owner approved 2026-11-23): the Floor 2 concourse's rooms and the passport wall in the terminal's layout: run
# by Tools/terminal_edit.py, last (after terminal_gatearc_edit.py), so every other node keeps its index. The models:
# Art/TerminalEast/make_concourse.py (Tools/import_east.py); where they go: Art/TerminalEast/conc_place.json. The old faces of the
# walkway's wall x 146..242 go by Data/terminal-cuts.json ("touch", nodes 10381 / 10382, Tools/terminal_cut.py).
import math as _m2
_CP = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'conc_place.json')))
def _cadd(p, n): N.append(n); N[byname[p]].setdefault('children', []).append(len(N) - 1); return len(N) - 1
for _as in _CP['big']: _cadd('F2', {'name': 'F2-CONC/' + _as[7:].upper().replace('_', '-'), 'asset': _as, 'mesh': 90000 + len(N)})
for _nm, _as, _x, _y, _z, _r, _s in _CP['place']:
    _n = {'name': 'F2-CONC/' + _nm, 'asset': _as, 'mesh': 90000 + len(N), 'translation': [-_x, _y, _z]}
    if _r: _n['rotation'] = [0.0, -_m2.sin(_m2.radians(_r) / 2), 0.0, _m2.cos(_m2.radians(_r) / 2)]
    if _s and _s != [1, 1, 1]: _n['scale'] = _s
    _cadd('F2', _n)
# the passport wall's two automatic doors (the main entrance's own, without its sign; "-ENTR-": passengers go through by themselves -
# until part 5 the passengers of gates 13-16 do) and the vault's gable over the wall, a copy of the walkway's own end gable
for _id, _x, _z in _CP['doors']:
    _d = copy_tree(byname['D-F1-ENTR-C-1'], skip=('sign',)); N[_d]['name'] = _id; N[_d]['translation'] = [-_x, 6.01, _z]
    N[_d]['rotation'] = [0.0, -0.7071067811865475, 0.0, 0.7071067811865476]; N[byname['F2']]['children'].append(_d)
N.append({'name': 'F2-WALK/CEILING/GABLE-240', 'mesh': N[byname['F2-WALK/CEILING/GABLE-E']]['mesh'], 'translation': [2.0, 0, 0]}); N[byname['F2-WALK/CEILING']]['children'].append(len(N) - 1)
# the enclosed rooms' own lamps under their ceilings; the concourse's lamps over those rooms' tops go
_n = 0
for _k, _nd in enumerate(N):
    if _nd['name'].startswith('F2-CONCOURSE/LAMP-') and any(a < -_nd['translation'][0] < b and c < _nd['translation'][2] < d for a, b, c, d in _CP['lids']):
        N[byname['F2']]['children'].remove(_k); _n += 1
for _nm, _pl in _CP['lamps'].items():
    for _k, (_x, _z) in enumerate(_pl):
        N.append({'name': '%s/LAMP-%03d' % (_nm, _k + 1), 'translation': [-_x, 9.57, _z], 'children': [len(N) + 1]})
        N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186}); N[byname['F2']]['children'].append(len(N) - 2)
log.append('new: the concourse: %d models, %d lamps in its rooms, %d lamps over them out' % (len(_CP['big']) + len(_CP['place']), sum(len(v) for v in _CP['lamps'].values()), _n))
