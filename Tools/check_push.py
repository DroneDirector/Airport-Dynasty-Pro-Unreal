"""Data check: every stand's push ends facing the way the aircraft first taxis, with its connector still ahead, on the
taxilane (to-do 279-284 round, item 9). The same rules as SimTraffic.cpp TaxiOutDir, PushRadius. No game needed."""
import json, os
D = json.load(open(os.path.join(os.path.dirname(__file__), '..', 'Data', 'airfield.json'), encoding='utf-8'))
WB, PX = 12.64, {True: -130.0, False: 362.0}
twys = [t for v in D.values() if isinstance(v, list) for t in v if isinstance(t, dict) and t.get('pts')]
ends = {t['id']: sorted((t['pts'][0][0], t['pts'][-1][0])) for t in twys if t.get('id') in ('T', 'K')}
bad = 0
for s in D['stands']:
    for x1 in (True, False):
        cargo = bool(s.get('cargo'))
        link = (-530.0 if x1 else 620.0) if cargo else PX[x1]
        way = (-1 if x1 else 1) if cargo else (1 if link >= s['x'] else -1)
        lo, hi = ends['K' if cargo else 'T']
        R = max(22.0, min(40.0, (s['x'] - (lo if way > 0 else hi)) * way - 3.0))
        main = s['x'] - way * R
        ahead = (link - (main + way * WB)) * way
        why = []
        if ahead < 25: why.append('its link only %.1f m ahead of the nose' % ahead)
        if not lo <= main <= hi: why.append('main gear %.1f m past the taxilane end' % max(lo - main, main - hi))
        if why: bad += 1; print('%-10s runway to %s: %s' % (s['id'], 'x1' if x1 else 'x0', '; '.join(why)))
print('%d stands, %d findings' % (len(D['stands']), bad))
