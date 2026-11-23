# The terminal's east side, part 1 (the shell, owner 2026-11-23, v2 approved) in the terminal's layout: run by
# Tools/terminal_edit.py, last (after terminal_wave.py), so every other node keeps its index. The models:
# Art/TerminalEast/make_terminal_east.py (Tools/import_east.py); where the copies go: Art/TerminalEast/place.json.
import json as _ej
X_EAST = (155.0, 552.0)
_E = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'place.json')))
def _eput(p, n): N.append(n); N[byname[p]]['children'].append(len(N) - 1); return len(N) - 1
def _easset(p, name, asset, x=0.0, y=0.0, z=0.0):
    n = {'name': name, 'asset': asset, 'mesh': 90000 + len(N)}
    if x or y or z: n['translation'] = [-x, y, z]
    return _eput(p, n)
for _as in _E['solid']: _easset('F1', 'F1-EAST/' + _as[5:].upper().replace('_', '-'), _as)
for _nm, _as in (('GLASS-FRONT-W', 'east_glass_front_w'), ('GLASS-FRONT-E', 'east_glass_front_e'), ('GLASS-SKIN', 'east_glass_skin'),
                 ('GLASS-INNER', 'east_glass_inner'), ('PROMENADE-WALL', 'east_prom_wall'), ('PAVING', 'east_pave'), ('F2-PLUG', 'east_f2_plug'), ('F2-RAIL', 'east_f2_rail'), ('F2-RAIL-GLASS', 'east_f2_rail_glass')):
    _easset('F1', 'F1-EAST/' + _nm, _as)
for _k, (_as, _x, _y, _z) in enumerate(_E['cols']): _easset('F1', 'F1-EAST/COL-%03d' % (_k + 1), _as, _x, _y, _z)
# the four entrances: the vestibule, the main entrance's sliding doors (both skins, without its sign), its mat, three lamps
# and the kerb canopy of Entrance West/East (11 m wide for the 10 m ones)
for _k, (_x, _w) in enumerate(_E['entrances']):
    _id = 'F1-ENTR-EAST%d' % (_k + 1)
    _easset('F1', _id + '/SOLID', 'east_vest%d_solid' % _w, _x); _easset('F1', _id + '/GLASS', 'east_vest%d_glass' % _w, _x)
    for _j, _z in ((1, -4.0), (2, 0.0)):
        _d = copy_tree(byname['D-F1-ENTR-C-%d' % _j], skip=('sign',)); N[_d]['name'] = 'D-%s-%d' % (_id, _j); N[_d]['translation'] = [-_x, 0.0, _z]
        N[byname['F1']]['children'].append(_d)
    _m = copy_tree(byname['F1-ENTR-E/MAT-001']); N[_m]['name'] = _id + '/MAT-001'; _t = N[_m]['translation']; N[_m]['translation'] = [-_x, _t[1], _t[2]]
    if _w < 12:
        for _c in N[_m]['children']: _s = N[_c]['scale']; N[_c]['scale'] = [_s[0] - 2.0, _s[1], _s[2]]
    N[byname['F1']]['children'].append(_m)
    lamps('F1', _id, [_x - 4.0, _x, _x + 4.0] if _w >= 12 else [_x - 3.0, _x, _x + 3.0], [-2.0], 3.33)
    _c2 = copy_tree(byname['KERB/CANOPY-003']); N[_c2]['name'] = 'KERB/CANOPY-EAST%d' % (_k + 1); N[_c2]['translation'] = [-_x, 3.8, -6.0]
    if _w < 12:
        for _c in N[_c2]['children']:
            _s = N[_c].get('scale', [1, 1, 1]); _t = N[_c].get('translation', [0, 0, 0])
            if _s[0] > 5: N[_c]['scale'] = [_s[0] - 2.0, _s[1], _s[2]]
            elif abs(_t[0]) > 4: N[_c]['translation'] = [5.1 if _t[0] > 0 else -5.1, _t[1], _t[2]]
    N[parent[byname['KERB/CANOPY-003']]]['children'].append(_c2)
    log.append('new: entrance %s (x %.0f, %.0f m)' % (_id, _x, _w))
# the lamps, the terminal's own (1.2 x 0.6 m, by the lamp system: every other one lights, 9 m reach, no shadows)
_n = 0
for _nm, (_p, _y, _pl) in _E['lamps'].items():
    for _k, (_x, _z) in enumerate(_pl):
        N.append({'name': '%s/LAMP-%03d' % (_nm, _k + 1), 'translation': [-_x, _y, _z], 'children': [len(N) + 1]})
        N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186}); N[byname[_p]]['children'].append(len(N) - 2); _n += 1
# what stood in the way: Entrance East's end pane and its frame (rebuilt without the middle post, x 107 z -2: the link
# gallery's way in), and the old paving stub by the staff door (x 156..170, z 40..52: floor now)
_i = byname['F1-ENTR-E']; _w = [c for c in N[_i]['children'] if N[c]['name'] in ('w#459', 'F1-ENTR-E/STRUCTURE-mullion')]
assert len(_w) == 2, 'Entrance East'
for _c in _w: drop(_c, 'the link gallery opens into it')
drop(byname['SITE/SIDE-PATH'], 'the East Hall floor')
log.append('new: the east side shell: %d columns, 4 entrances, %d lamps' % (len(_E['cols']), _n))
# the wave roof's downlights in the row at z 0 stood half in, half out of the new front's glass: moved 60 cm out, under the
# roof's edge, on the same wave (none in the Grand Hall's own x 36..114: unchanged)
_n = 0
for _i in [k for k, n in enumerate(N) if n['name'].startswith('ROOF/DOWNLIGHT-')]:
    _t = N[_i].get('translation')
    if _t and abs(_t[2]) < 0.2 and X_EAST[0] < -_t[0] < X_EAST[1]: N[_i]['translation'] = [_t[0], round(WR.height(-_t[0], -0.6) - 0.03, 4), -0.6]; _n += 1
log.append('moved: %d downlights 60 cm out of the new front' % _n)
