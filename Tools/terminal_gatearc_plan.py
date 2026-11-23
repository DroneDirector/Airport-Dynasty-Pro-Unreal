# The gates' curved windows in Data/terminal-plan.json: every gate room in Art/GateArc/gatearc_geom.py's list reaches to z 85
# (the bay: passengers, staff, seats, waiting spots, sounds and lamps in it belong to the gate), and a jetway door of the plan's
# own kind where a jetway stands at the gate's own door and the plan had none yet (gates 9 and 11, copies of gate 1).
#   python Tools/terminal_gatearc_plan.py     (after Tools/terminal_east_plan.py; run again any time)
import json, os, shutil, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(ROOT, 'Art', 'GateArc'))
import gatearc_geom as G
F = os.path.join(ROOT, 'Data', 'terminal-plan.json'); raw = open(F, encoding='utf-8', newline='').read(); P = json.loads(raw)
JL = json.load(open(os.path.join(ROOT, 'Data', 'babylon-jetways.json'))); rooms = {r['id']: r for r in P['rooms']}; n = d = 0
for g in G.GATES:
    r = rooms['F2-GATE-%d' % g]; n += r['z1'] != 85; r['z1'] = 85
    j = G.has_corridor(JL, r['x0'])
    if j and not any(q['a'] == r['id'] and q['b'] == j for q in P['doors']):
        P['doors'].append({'id': 'D-%s-JW' % r['id'], 'f': 2, 'a': r['id'], 'b': j, 'type': 'jetway', 'w': 1.8, 'x': round(r['x0'] + G.JX, 3), 'z': 82, 'along': 'x'}); d += 1
nl = '\r\n' if '\r\n' in raw else '\n'
open(F, 'w', encoding='utf-8', newline='').write(json.dumps(P, indent=1 if '\n ' in raw else None, ensure_ascii=raw.isascii()).replace('\n', nl))
C = os.path.join(ROOT, 'Content', 'Data', 'terminal-plan.json')
if os.path.exists(C): shutil.copy(F, C)
print('plan: %d gate rooms to z 85, %d jetway doors added' % (n, d))
