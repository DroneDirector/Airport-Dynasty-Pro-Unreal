# West Wing part 3 (to-do 234/213, sheet 2b): the Grand Hall's own self-service fittings (Art/TerminalAnnex/make_hall_fitout.py;
# which pieces where: Art/TerminalAnnex/hallfit_place.json). Run by Tools/terminal_edit.py, last, so every other node keeps its
# index. The big model (service desk, trolley bays, the e-gate banks' fences) is in the plan's metres (no translation); the
# instanced pieces (ten e-gates, eight trolleys, the desk's three staff chairs) where the json puts them. The self check-in
# kiosks are the owner's own model, placed by Data/terminal-props.json (Source/.../TerminalProps.cpp), not here.
import math as _mh
_HF = _ej.load(open(os.path.join(TW.ROOT, 'Art', 'TerminalAnnex', 'hallfit_place.json')))
def _hadd(n): N.append(n); N[byname['F1']].setdefault('children', []).append(len(N) - 1); return len(N) - 1
for _as in _HF['big']: _hadd({'name': 'F1-HALLFIT/' + _as[6:].upper().replace('_', '-'), 'asset': _as, 'mesh': 90000 + len(N)})
for _nm, _as, _x, _y, _z, _r in _HF['place']:
    _n = {'name': 'F1-HALLFIT/' + _nm, 'asset': _as, 'mesh': 90000 + len(N), 'translation': [-_x, _y, _z]}
    if _r: _n['rotation'] = [0.0, -_mh.sin(_mh.radians(_r) / 2), 0.0, _mh.cos(_mh.radians(_r) / 2)]
    _hadd(_n)
log.append('new: the Grand Hall self-service fittings (service desk, e-gates, trolley bays): %d models, %d places' % (len(_HF['models']), len(_HF['place'])))
