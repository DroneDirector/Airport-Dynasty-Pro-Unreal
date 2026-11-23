# The wave roof in the terminal's layout (owner, 2026-11-23): run by Tools/terminal_edit.py, last. The eight end-to-end roof
# copies (the Grand Hall's own, ROOF-ARRIVALS, ROOF-W1/2, ROOF-E1..4) go out and the one continuous roof goes in
# (Tools/wave_roof.py its shape, Art/TerminalAnnex/make_wave_roof.py its models): 9 bows, their light lines, the skylights and
# downlights in the good part's own rhythm; the kerb's tree pillars: the good part's own two (C-7, C-8) under every bow, so each
# head meets the roof as it does there (the old outer ones stood in other places of the wave, some outside its new edge).
import re
import wave_roof as WR
_roof = byname['ROOF']
for _c in list(N[_roof]['children']):
    if N[_c]['name'].startswith(('ROOF/WAVE-', 'ROOF/FASCIA', 'ROOF/EDGE-LIGHT', 'ROOF/SKYLIGHT', 'ROOF/DOWNLIGHT')): N[_roof]['children'].remove(_c)
for _c in list(N[0]['children']):
    if N[_c]['name'] in ('ROOF-ARRIVALS', 'ROOF-W1', 'ROOF-W2', 'ROOF-E1', 'ROOF-E2', 'ROOF-E3', 'ROOF-E4'): N[0]['children'].remove(_c)


def _wput(p, n): N.append(n); N[p]['children'].append(len(N) - 1); return len(N) - 1


for _k, _b in enumerate(WR.BOWS):
    _s = 'w' if _k == 0 else 'e' if _k == len(WR.BOWS) - 1 else 'm'
    for _nm, _as in (('ROOF/WAVE-SOFFIT-%02d', 'wave_soffit_'), ('ROOF/WAVE-TOP-%02d', 'wave_top_'), ('ROOF/FASCIA-%02d', 'wave_fascia_'), ('ROOF/EDGE-LIGHT-%02d', 'wave_edge_')):
        _wput(_roof, {'name': _nm % (_k + 1), 'asset': _as + _s, 'mesh': 90000 + len(N), 'translation': [-_b, 0.0, 0.0]})
for _k, _x in enumerate(WR.skylights()):
    _wput(_roof, {'name': 'ROOF/SKYLIGHT-%02d' % (_k + 1), 'mesh': 1000, 'translation': [-(_x - 45.0), 0.0, 0.0]})
for _k, (_x, _y, _z) in enumerate(WR.downlights()):
    _d = _wput(_roof, {'name': 'ROOF/DOWNLIGHT-%03d' % (_k + 1), 'translation': [-_x, round(_y, 4), _z], 'children': []})
    _wput(_d, {'name': 'cyl1-18#downlight', 'scale': [0.36, 0.04, 0.36], 'mesh': 1003})
_kerb = parent[byname['C-7']]
for _c in list(N[_kerb]['children']):
    if re.match(r'^C-[78]-(W\d|E\d|ARRIVALS)$', N[_c]['name']): N[_kerb]['children'].remove(_c)
_n = 0
for _k, (_src, _x) in enumerate(WR.PILLARS):
    if _x in (56.0, 94.0): continue                          # (the Grand Hall's own two stand as they are)
    _p = copy_tree(byname[_src]); N[_p]['name'] = '%s-B%d' % (_src, _k // 2 + 1); N[_p]['translation'] = [-_x, 0.0, -9.0]
    N[_kerb]['children'].append(_p); _n += 1
log.append('new: the one wave roof x %.0f..%.0f (%d bows, %d skylights, %d downlights), %d kerb pillars besides the two at the hall'
           % (WR.X0, WR.X1, len(WR.BOWS), len(WR.skylights()), len(WR.downlights()), _n))
