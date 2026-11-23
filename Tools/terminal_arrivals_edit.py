# Terminal part 2b (owner approved 2026-11-23): gate 13's swing lobby, the arrivals' ways down at gates 13-16 and their closed corridor
# in the terminal's layout: run by Tools/terminal_edit.py, last (after terminal_concourse_edit.py), so every other node keeps its
# index. The models: Art/TerminalEast/make_arrivals.py (Tools/import_east.py); the numbers: Art/TerminalEast/arr_geom.py. The
# escalators, the stairs' steps, the lifts' cars, the switching signs and which doors start locked: SwingGate.cpp.
# Doors (part 5 locks and unlocks them by these names, ASwingGate / ABabylonModel::LockDoor):
#   D-F2-SWING-B1, -B2   gate 13 to the non-Schengen walkway          D-F2-SWING-C, -F   the swing lobby: Schengen walkway, gate 13
#   D-F2-VEST<g>-GATE    the jetways' vestibule to the gate side       D-F2-VEST<g>-ARR   ... to the way down (never both)
#   D-F2-VIEW-<g>-JW<n>  the jetway doors in the view bay's glass      D-F1-ARR<g>-1WAY, D-F1-ARR-PC-1WAY   the one-way doors
#   D-F1-RAMP-<g>B-3     the gate stair hall's own door onto the apron D-F1-RAMP-<g>B-1   the ground lobby's emergency exit
import math as _m3
_sp3 = _gu.spec_from_file_location('arr_geom', os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'arr_geom.py')); _A = _gu.module_from_spec(_sp3); _sp3.loader.exec_module(_A)
_APL = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'arr_place.json')))
_apar = {c: i for i, n in enumerate(N) for c in n.get('children', [])}
def _apos(i):
    x = y = z = 0.0
    while i is not None: t = N[i].get('translation', [0, 0, 0]); x += t[0]; y += t[1]; z += t[2]; i = _apar.get(i)
    return -x, y, z
def _aout(i): N[_apar[i]]['children'].remove(i)
def _akids(room):
    """the room's own group (the one holding its structure and doors)"""
    g = max((i for i, n in enumerate(N) if n['name'] == room and i in _apar), key=lambda i: len(N[i].get('children', [])))
    return g, {N[c]['name']: c for c in N[g]['children']}
def _aput(p, name, asset, x=0.0, y=0.0, z=0.0): return _cadd(p, {'name': name, 'asset': asset, 'mesh': 90000 + len(N), **({'translation': [-x, y, z]} if x or y or z else {})})
def _alamp(p, name, x, y, z):
    N.append({'name': name, 'translation': [-x, y, z], 'children': [len(N) + 1]}); N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186}); N[byname[p]]['children'].append(len(N) - 2)
_TURN = [0.0, -_m3.sqrt(0.5), 0.0, _m3.sqrt(0.5)]
def _aslide(p, name, x, y, z, w, h, turn=False):
    """the terminal's own sliding glass doors (the main entrance's), w clear and h high"""
    d = copy_tree(byname['D-F1-ENTR-C-1'], skip=('sign',)); N[d]['name'] = name; N[d]['translation'] = [-x, y, z]
    if turn: N[d]['rotation'] = list(_TURN)
    q = w / 2; lf = h - 0.04
    T = [(q + .05, h / 2, 0, .1, h, .32), (-q - .05, h / 2, 0, .1, h, .32), (0, h + .07, 0, w + .3, .14, .32), (w / 4, lf / 2, 0, q - .02, lf, .04), (q - .03, lf / 2, 0, .05, lf, .06), (w / 4, 1.05, 0, q - .1, .06, .05),
         (-w / 4, lf / 2, 0, q - .02, lf, .04), (-q + .03, lf / 2, 0, .05, lf, .06), (-w / 4, 1.05, 0, q - .1, .06, .05), (0, h - .11, .14, w + .1, .22, .2)]
    assert len(N[d]['children']) == len(T)
    for c, t in zip(N[d]['children'], T): N[c]['translation'] = [round(v, 4) for v in t[:3]]; N[c]['scale'] = [round(v, 4) for v in t[3:]]
    N[byname[p]]['children'].append(d)
_no = 0
for _g, _a in _A.GATE.items():
    _t13 = '13' if _g == 13 else ''
    # what the new parts replace, and what stood in their way
    _grp, _k = _akids('F2-VIEW-%d' % _g); _p = 'F2-VIEW-%d/' % _g
    for _nm in (_p + 'STRUCTURE-carpetBlue', _p + 'STRUCTURE-concrete', _p + 'STRUCTURE-mullion', _p + 'SEATS-003', _p + 'PLANT-001', _p + 'PLANT-002', 'w#1669'): _aout(_k[_nm]); _no += 1
    _s2 = N[_k[_p + 'SEATS-002']]; _s2['translation'] = [_s2['translation'][0], _s2['translation'][1], 73.3]      # (clear of the lift's shaft)
    for _nm, _c in _k.items():
        if '/LAMP-' in _nm:
            _x, _y, _z = _apos(_c); _u = _x - _a
            if abs(_u - 48.87) < 0.1 and abs(_z - 75) < 0.1: _aout(_c); _no += 1
    _grp, _k = _akids('F2-GATE-%d' % _g); _p = 'F2-GATE-%d/' % _g
    for _nm in [_p + 'STRUCTURE-plaster'] + ([_p + 'STRUCTURE-carpetBlue', _p + 'STRUCTURE-concrete', _p + 'STRUCTURE-white', _p + 'PLANT-001'] if _t13 else []): _aout(_k[_nm]); _no += 1
    _r = 'F1-RAMP-%dB' % _g; _grp, _k = _akids(_r); _p = _r + '/'
    for _nm in [_p + 'STRUCTURE-offwhite', _p + 'STRUCTURE-mullion', 'w#881', _p + 'SEATS-001', _p + 'SEATS-002', _p + 'SEATS-003', _p + 'GATE-DESK-001', _p + 'OFFICE-CHAIR-001', _p + 'BIN-001', 'M-F1-G8']: _aout(_k[_nm]); _no += 1
    for _nm, _c in _k.items():
        if '/LAMP-' in _nm:
            _x, _y, _z = _apos(_c); _u = _x - _a
            if abs(_u - 34) < 0.1 or (36 < _u < 51 and _z > 79) or (abs(_u - 50) < 0.1 and abs(_z - 75) < 0.1) or (_t13 and abs(_u - 38) < 0.1 and _z < 71): _aout(_c); _no += 1
    if _t13:                                           # its stair turned along the walkway wall, foot west (2.8 x 11 m as before)
        _s = N[_k['S-G4']]; _s['rotation'] = [0.0, _m3.sqrt(0.5), 0.0, _m3.sqrt(0.5)]; _s['translation'] = [-278.85, 0.0, -129.05]
    for _as in ('core', 'core_glass', 'csigns', 'gfront', 'gfront_glass'): _aput('F2', 'F2-ARR%d/%s' % (_g, _as.upper().replace('_', '-')), 'east_a_' + _as, _a)
    for _as in ('slab' + _t13, 'plaster' + _t13, 'front' + _t13, 'front%s_glass' % _t13, 'side' + _t13) + (('white13',) if _t13 else ()): _aput('F2', 'F2-ARR%d/%s' % (_g, _as.upper().replace('_', '-')), 'east_a_' + _as, _a)
    # the lift's shaft: the gate lifts' own (its car and doors: the game's)
    _l = copy_tree(byname['E-G4'], skip=('lift-car', 'lf#', 'ld#')); N[_l]['name'] = 'E-ARR%d' % _g; N[_l]['translation'] = [-(_a + _A.LU), 0.0, _A.LZ]; N[_l]['rotation'] = list(_TURN)
    N[_l]['children'] = [c for c in N[_l]['children'] if N[c].get('mesh') != 560]; N[byname['F1']]['children'].append(_l)
    # the doors
    for _j, _u in enumerate(_A.JW[_g]): _aslide('F2', 'D-F2-VIEW-%d-JW%d' % (_g, _j + 1), _a + _u, 6.01, 82.0, 1.8, 2.25)
    _aslide('F2', 'D-F2-VEST%d-GATE' % _g, _a + _A.GATE_U, 6.01, _A.VS, 5.0, 3.0); _aslide('F2', 'D-F2-VEST%d-ARR' % _g, _a + _A.CE, 6.01, _A.ARR_Z, 5.0, 3.0, True)
    _aslide('F1', 'D-F1-ARR%d-1WAY' % _g, _a + _A.OUT_U, 0.022, 68.0, 3.0, 3.0); _aslide('F1', 'D-F1-RAMP-%dB-3' % _g, _a + _A.STAIR_DOOR, 0.0, 82.0, 1.8, 2.25)
    for _j, _z in enumerate((72.0, 77.0)): _alamp('F1', '%s-HALL/LAMP-%03d' % (_r, _j + 1), _a + 32.6, 5.33, _z)
# the swing lobby, the corridor
for _as in _APL['once']: _aput('F2' if 'swing' in _as else 'F1', 'F2-ARR/' + _as[7:].upper().replace('_', '-'), _as)
for _j, (_c, _x0, _x1) in enumerate(_A.DB): _aslide('F2', 'D-F2-SWING-B%d' % (_j + 1), _c, 6.01, 68.0, 6.0, 3.0)
_aslide('F2', 'D-F2-SWING-C', _A.DC, 6.01, 67.97, 2.4, 2.7); _aslide('F2', 'D-F2-SWING-F', 255.42, 6.01, _A.DF, 6.0, 3.0, True)
_aslide('F1', 'D-F1-ARR-PC-1WAY', _A.PC_X, 0.015, 52.0, 5.0, 3.0)
for _j, (_x, _z) in enumerate(((241.0, 72.0), (251.4, 72.0), (241.0, 79.5), (251.4, 79.5))): _aput('F1', 'F2-SWING/COL-%d' % (_j + 1), 'east_col_g', _x, 0.0, _z)
for _j, (_x, _z) in enumerate([(x, z) for x in (240.2, 246.2, 252.2) for z in (71.0, 75.0, 79.0)]): _alamp('F2', 'F2-SWING/LAMP-%03d' % (_j + 1), _x, 11.33, _z)
_xs = [296.5 + 4.1 * k for k in range(56)]
for _j, (_x, _z) in enumerate([(x, 65.65) for x in _xs] + [(343.0, z) for z in (54.5, 58.0, 61.5)]): _alamp('F1', 'F1-ARR-CORR/LAMP-%03d' % (_j + 1), _x, 3.97, _z)
# the baggage hall where the corridor runs: the old sorting belts' row z 63.5 and the lamps over the corridor's ceiling
for _i in list(N[byname['F1-BAGHALL']]['children']):
    _nm = N[_i]['name']
    if _nm.endswith('@+322.2') and any(('SORTING-BELT-%03d@' % q) in _nm for q in (8, 9, 10, 11)): N[byname['F1-BAGHALL']]['children'].remove(_i); _no += 1
    elif '/LAMP-' in _nm:
        _q = _i
        while not N[_q].get('mesh') and N[_q].get('children'): _q = N[_q]['children'][0]
        _x, _y, _z = _apos(_q)
        if (_A.CX0 - 1.3 < _x < _A.CX1 + 1.3 and _z > _A.CZ0 - 0.3) or (_A.KX0 - 1.4 < _x < _A.KX1 + 1.4 and _z < _A.CZ0): N[byname['F1-BAGHALL']]['children'].remove(_i); _no += 1
log.append('new: the swing lobby, the arrivals ways down at gates 13-16 and their corridor; %d old parts out' % _no)
