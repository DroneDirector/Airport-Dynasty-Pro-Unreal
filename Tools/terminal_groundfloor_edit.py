# Terminal east, part 3, step 1: the East Wing's ground-floor rooms (their floor finishes, skirtings, door linings) in the terminal's
# layout: run by Tools/terminal_edit.py, last (after terminal_arrivals_edit.py), so every other node keeps its index. The models:
# Art/TerminalEast/make_groundfloor.py (Tools/import_east.py); which pieces: Art/TerminalEast/ground_place.json. They are made in
# the plan's metres, so they go in as they are (no translation), under F1.
_GP = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalEast', 'ground_place.json')))
for _as in _GP['big']:
    N.append({'name': 'F1-EAST-GROUND/' + _as[7:].upper().replace('_', '-'), 'asset': _as, 'mesh': 90000 + len(N)})
    N[byname['F1']].setdefault('children', []).append(len(N) - 1)
log.append('new: the ground-floor rooms\' finishes, skirtings and door linings: %d models' % len(_GP['big']))
