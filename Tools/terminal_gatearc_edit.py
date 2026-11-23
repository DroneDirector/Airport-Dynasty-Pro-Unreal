# The gates' curved windows (owner approved 2026-11-23, docs/Blueprints/Gate-Arc) in the terminal's layout: run by
# Tools/terminal_edit.py, last (after terminal_east_edit.py), so every other node keeps its index. One generator for every
# gate in Art/GateArc/gatearc_geom.py's list (a new gate: its number there, nothing else).  Per gate room: the flat glass and
# its mullions go (and the dead jetway door where no jetway stands: gates 13-16), the curved models come (five, instanced), the
# three rows get their aisle (4 seats of each to its ends), 24 seats along the curve, the lamps carried on into the bay.
# The old wall panel over the door is cut out of the room's plaster piece: Data/terminal-cuts.json (Tools/terminal_cut.py).
import math, importlib.util as _gu
_sp = _gu.spec_from_file_location('gatearc_geom', os.path.join(TW.ROOT, 'Art', 'GateArc', 'gatearc_geom.py')); _GG = _gu.module_from_spec(_sp); _sp.loader.exec_module(_GG)
_JL = json.load(open(TW.D('babylon-jetways.json'))); _PR = {r['id']: r for r in json.load(open(TW.D('terminal-plan.json')))['rooms']}
_par = {c: i for i, n in enumerate(N) for c in n.get('children', [])}
def _gq(a): return [0.0, math.sin(a / 2.0), 0.0, math.cos(a / 2.0)]        # a turn about the height, as the layout turns
def _gadd(p, n):
    N.append(n); N[p].setdefault('children', []).append(len(N) - 1); return len(N) - 1
for _g in _GG.GATES:
    _id = 'F2-GATE-%d' % _g
    _grp = next(i for i, n in enumerate(N) if n['name'] == _id and any(N[c]['name'].startswith(_id + '/STRUCTURE-') for c in n.get('children', [])))
    _k = _grp; _tx = 0.0
    while _k is not None: _tx += N[_k].get('translation', [0, 0, 0])[0]; _k = _par.get(_k)
    _x0 = _PR[_id]['x0']; _lx = _x0 + _tx                                   # (the room's corner in the plan, and in its own group)
    _jw = _GG.has_corridor(_JL, _x0); _v = 'j' if _jw else 'f'
    _out = [c for c in N[_grp]['children'] if N[c]['name'].startswith('w#') or N[c]['name'] == _id + '/STRUCTURE-mullion' or (not _jw and N[c]['name'] == 'D-%s-JW' % _id)]
    assert len(_out) == (3 if _jw else 4), (_id, [N[c]['name'] for c in _out])
    N[_grp]['children'] = [c for c in N[_grp]['children'] if c not in _out]
    for _nm, _as in (('FLOOR', 'gatearc_floor_' + _v), ('ROOF', 'gatearc_roof'), ('FRAME', 'gatearc_frame_' + _v), ('GLASS', 'gatearc_glass_' + _v)):
        _gadd(_grp, {'name': '%s/ARC-%s' % (_id, _nm), 'asset': _as, 'mesh': 90000 + len(N), 'translation': [-_lx, 0.0, 0.0]})
    # the three rows: 12 seats, the 2.42 m aisle, 25 seats (37 as before)
    _rows = [c for c in N[_grp]['children'] if N[c]['name'].startswith(_id + '/SEATS-')]; assert len(_rows) == 3, _id
    for _r in _rows:
        _kids = N[_r]['children']; assert len(_kids) == 3 + 74
        _t = N[_r]['translation']; _at = 0
        for _sec, (_first, _n) in enumerate(_GG.ROWS):
            _mid = _lx + _GG.ROW0 + _GG.PITCH * (_first + (_n - 1) / 2.0); _take = _kids[3 + 2 * _at: 3 + 2 * (_at + _n)]; _at += _n; _new = []
            for _b, _s in enumerate(_kids[:3]):                             # (the old beam and legs: hidden in the game, kept the row's length)
                _c = dict(N[_s]); _L = _n * _GG.PITCH
                if _b == 0: _c['scale'] = [_L, _c['scale'][1], _c['scale'][2]]
                else: _c['translation'] = [(_L / 2 - 0.2) * (1 if _b == 1 else -1), _c['translation'][1], _c['translation'][2]]
                N.append(_c); _new.append(len(N) - 1)
            for _q in range(_n):
                for _c in _take[2 * _q: 2 * _q + 2]:
                    _u = N[_c]['translation']; N[_c]['translation'] = [-_GG.PITCH * (_q - (_n - 1) / 2.0), _u[1], _u[2]]
            if _sec == 0: _row0 = _new + _take; N[_r]['translation'] = [-_mid, _t[1], _t[2]]
            else: _gadd(_grp, {'name': N[_r]['name'] + 'E', 'translation': [-_mid, _t[1], _t[2]], 'children': _new + _take})
        N[_r]['children'] = _row0
    # 24 seats along the curve, facing the apron (the game sets the seats on the row's own circle: "arc")
    _pan, _back = N[_rows[0]]['children'][3], N[_rows[0]]['children'][4]; _Rs = _GG.R + _GG.SOFF
    for _s, _sec in enumerate((_GG.SEC_W, _GG.SEC_E)):
        _tm = (_sec[0] + _sec[-1]) / 2.0; _x, _z = _GG.pt(_tm, _GG.SOFF); _ks = []
        for _t in _sec:
            _a = (_t - _tm) / _GG.R; _px, _pz = -_Rs * math.sin(_a), _Rs * (math.cos(_a) - 1.0)
            for _src, _dz in ((_pan, 0.0), (_back, -0.24)):
                _c = dict(N[_src]); _c['translation'] = [_px - _dz * math.sin(_a), N[_src]['translation'][1], _pz + _dz * math.cos(_a)]; _c['rotation'] = _gq(-_a)
                N.append(_c); _ks.append(len(N) - 1)
        _gadd(_grp, {'name': '%s/SEATS-%03d' % (_id, 4 + _s), 'translation': [-(_lx + _x), 6, _z], 'rotation': _gq(-_tm / _GG.R), 'arc': round(_Rs, 4), 'children': _ks})
    # the room's own lamps carried on into the bay, along the curve
    for _q, _t in enumerate(_GG.LAMPS):
        _x, _z = _GG.pt(_t, _GG.LAMP_OFF)
        N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186})
        _gadd(_grp, {'name': '%s/LAMP-%03d' % (_id, 25 + _q), 'translation': [-(_lx + _x), _GG.LAMP_Y, _z], 'rotation': _gq(-_t / _GG.R), 'children': [len(N) - 1]})
    log.append('curved window: %s (x %.2f..%.2f, %s)' % (_id, _x0, _x0 + _GG.X1, 'jetway ' + _jw if _jw else 'no jetway: full curve, its dead door out'))
