# The timetables that hung without a wall behind them (glass, shutters, room openings) go onto the nearest solid wall
# (owner, 2026-11-25, to-do 262). Run at the end of terminal_edit.py. Plan x = -layout x; each copy is found by its name and
# by the plan x it stands at; south wall boards face the walkway (+z), north wall boards are turned about y.
_N = L['nodes']
_par = {c: i for i, n in enumerate(_N) for c in n.get('children', [])}
def _boardpos(i):
    p = list(_N[i].get('translation', [0, 0, 0])); j = _par.get(i)
    while j is not None:
        n = _N[j]
        assert 'rotation' not in n and 'scale' not in n, ('board under a turned group', n.get('name'))
        t = n.get('translation', [0, 0, 0]); p = [a + b for a, b in zip(p, t)]; j = _par.get(j)
    return p
for _name, _was, _now, _side in (
        ('M-F2-04', -201.1, -201.1, 'N'), ('M-F2-04', 282.2, 288.05, 'S'), ('M-F2-05', -116.1, -116.1, 'N'),
        ('M-F2-05', 367.2, 367.2, 'N'), ('M-F2-06', 95.0, 90.0, 'N'), ('M-F2-07', 512.2, 512.2, 'N')):
    _hit = [i for i, n in enumerate(_N) if n.get('name') == _name and abs(_boardpos(i)[0] + _was) < 0.3]
    if len(_hit) != 1:
        _done = [i for i, n in enumerate(_N) if n.get('name') == _name and abs(_boardpos(i)[0] + _now) < 0.02]
        assert len(_done) == 1, (_name, _was, _hit)
        continue
    _i = _hit[0]; _p = _boardpos(_i); _t = list(_N[_i]['translation'])
    _z = 67.75 if _side == 'N' else 52.25
    _t[0] = round(_t[0] + (-_now - _p[0]), 4); _t[2] = round(_t[2] + (_z - _p[2]), 4)
    _N[_i]['translation'] = _t
    if _side == 'N': _N[_i]['rotation'] = [0, 1, 0, 0]
    log.append('moved: %s from x %.1f to %s wall x %.2f' % (_name, _was, _side, _now))
