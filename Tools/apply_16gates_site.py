# The 16 gates (owner, 2026-11-17), the airfield side: the stands, apron, taxilane, the control tower's place and road,
# and the car rental's parking, in Data/airfield.json, Data/roads.json and Data/terminal-plan.json (runs again safely:
# it starts from the files' state before the 16 gates, kept as *-pre16.json).
#   west: stands 9-12 exactly like today's (A320 family, 81.71 m pitch; 9 and 11 at jetways, nose 120; 10 and 12 walk-out,
#         nose 122)
#   east: stand 13 A380 (code F), 14-16 B777 (code E), noses at y 95 so their tails (<= y 169) stay clear of taxilane T
#         (y 218) by more than the 50.5 m ICAO code F taxilane-to-object distance; wingtips 7.5 m apart (code E/F);
#         today's remote A380 stand 9 and its apron (x 250..392, y 40..82: now under the new pier) go - gate 13 takes over
#   apron x -265..570; taxilane T x -255..560
#   control tower: 145 m further west (x -350): its lot's east edge (x -325) 60 m outside the apron - more than the
#         50.5 m code F taxilane/apron object clearance - and 109 m from the pier's new end; the runway's transitional
#         surface (1:7 from 150 m off the centre line, y 646) is 53 m high over it, the tower 22 m: clear
#   tower road: from the stands' tail-of-stand service road at the apron's west end, leaving the apron at a right angle,
#         then outside it (never across a stand or the taxilane) to the lot
#   car rental: its parking moved with the building (+242 m east, -231.3 m south: x 398..554, beside the main car park,
#         outside the airport boundary y -170)
import json, os, shutil
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = lambda n: os.path.join(ROOT, 'Data', n)
def load(n):
    pre = D(n.replace('.json', '-pre16.json'))
    if not os.path.exists(pre): shutil.copy(D(n), pre)
    return json.load(open(pre))
STEP = 81.71
RENTAL_DX, RENTAL_DZ = 242.0, -231.3
TOWER = (-350.0, 120.0)

# ---- airfield.json ----
A = load('airfield.json')
A['stands'] = [s for s in A['stands'] if s['id'] != 'STAND-9']
new = []
for k, (jet, walk) in ((2, (9, 10)), (1, (11, 12))):
    new.append({'id': 'STAND-%d' % jet, 'x': round(-62 - k * STEP, 2), 'nose': 120.0, 'dir': -1, 'span': 36, 'len': 44.5, 'code': 'C', 'jetway': True})
    new.append({'id': 'STAND-%d' % walk, 'x': round(-21.14 - k * STEP, 2), 'nose': 122.0, 'dir': -1, 'span': 36, 'len': 44.5, 'code': 'C', 'jetway': False})
WB = [(13, 'A380', 289.4, 80, 72.7, 'F'), (14, 'B777', 369.2, 65, 73.9, 'E'), (15, 'B777', 441.5, 65, 73.9, 'E'), (16, 'B777', 513.8, 65, 73.9, 'E')]
for g, kind, x, span, ln, code in WB:
    new.append({'id': 'STAND-%d' % g, 'x': x, 'nose': 95.0, 'dir': -1, 'span': span, 'len': ln, 'code': code, 'jetway': True, 'type': kind})
A['stands'] = new[:4] + A['stands'][:8] + new[4:] + A['stands'][8:]
A['standsNote'] = A.get('standsNote', '') + '; 2026-11-17: 16 gates - stands 9-12 west (A320 like today), 13 A380 and 14-16 B777 east (noses y 95), the old remote A380 stand 9 gone'
for ap in A['aprons']:
    if ap['id'] == 'APRON-T': ap['x0'], ap['x1'] = -265, 570
A['aprons'] = [ap for ap in A['aprons'] if ap['id'] != 'APRON-9']
for t in A['taxiways']:
    if t['id'] == 'T': t['pts'] = [[-255, 218], [560, 218]]
for l in A['lots']:
    if l['id'] == 'LOT-TOWER':
        l['x0'], l['x1'] = TOWER[0] - 27, TOWER[0] + 25
        l['note'] = 'tarmac round the control tower with 8 parking spaces (moved west with the tower for the 16 gates, 2026-11-17)'
for r in A['roads']:
    if r['id'] == 'ROAD-TOWER':
        r['pts'] = [[-250, 171], [-268, 171], [-292, 171], [-312, 158], [TOWER[0] + 25, 136]]
        r['note'] = 'the control tower: from the west end of the stands\' tail-of-stand service road, off the apron at a right angle at its edge (x -265), then outside the apron to the tower lot (16 gates, 2026-11-17)'
json.dump(A, open(D('airfield.json'), 'w'), indent=1)

# ---- roads.json: the car rental's parking ----
R = load('roads.json')
for l in R['lots']:
    if l.get('x0') == 174.0 and l.get('y0') == -16.0:
        l['x0'] += RENTAL_DX; l['x1'] += RENTAL_DX; l['y0'] += RENTAL_DZ; l['y1'] += RENTAL_DZ
        for row in l.get('rows', []):
            row['x0'] += RENTAL_DX; row['x1'] += RENTAL_DX; row['y'] += RENTAL_DZ
json.dump(R, open(D('roads.json'), 'w'), indent=1)

# ---- terminal-plan.json: the new rooms and stands ----
P = load('terminal-plan.json')
MOD = 80.55
for k, (jet, walk) in ((2, (9, 10)), (1, (11, 12))):
    dx = -k * MOD
    for rid, src in (('F1-RAMP-%d' % jet, 'F1-RAMP-1'), ('F1-GATE-%d' % walk, 'F1-GATE-5'), ('F2-GATE-%d' % jet, 'F2-GATE-1'), ('F2-VIEW-%d' % jet, 'F2-VIEW-1')):
        s = next(r for r in P['rooms'] if r['id'] == src); r = dict(s); r['id'] = rid
        r['x0'] = round(s['x0'] + dx, 2); r['x1'] = round(s['x1'] + dx, 2)
        r['name'] = s['name'].replace('Gate 1', 'Gate %d' % jet).replace('Gate 5', 'Gate %d' % walk).replace('Ramp services 1', 'Ramp services %d' % jet).replace('View bay 1', 'View bay %d' % jet)
        P['rooms'].append(r)
    P['stands'].append({'id': 'STAND-%d' % jet, 'x': round(-62 - k * STEP, 2), 'nz': 120.0, 'kind': 'jetway', 'gate': 'F2-GATE-%d' % jet})
    P['stands'].append({'id': 'STAND-%d' % walk, 'x': round(-21.14 - k * STEP, 2), 'nz': 122.0, 'kind': 'walk-out', 'gate': 'F1-GATE-%d' % walk})
for g, kind, x, span, ln, code in WB:
    dx = x - 203.05
    for rid, src, nm in (('F2-GATE-%d' % g, 'F2-GATE-4', 'Gate %d (%s, jetways, glass wall onto the ramp)' % (g, kind)),
                         ('F2-VIEW-%d' % g, 'F2-VIEW-4', 'View bay %d (seats, windows onto the ramp)' % g),
                         ('F1-RAMP-%d' % g, 'F1-RAMP-4', 'Ramp services %d (under Gate %d)' % (g, g)),
                         ('F1-RAMP-%dB' % g, 'F1-GATE-8', 'Ramp services %d B (under Gate %d)' % (g, g))):
        s = next(r for r in P['rooms'] if r['id'] == src); r = dict(s); r['id'] = rid; r['name'] = nm
        r['x0'] = round(s['x0'] + dx, 2); r['x1'] = round(s['x1'] + dx, 2)
        if src == 'F1-GATE-8': r['kind'] = 'ramp'
        P['rooms'].append(r)
    P['stands'].append({'id': 'STAND-%d' % g, 'x': x, 'nz': 95.0, 'kind': 'jetway', 'gate': 'F2-GATE-%d' % g, 'type': kind})
for r in P['rooms']:
    if r['id'] in ('F2-WALK', 'F1-BAGHALL'): r['x0'], r['x1'] = -241.1, 551.2
P['rooms'].append({'id': 'F2-TAXFREE-W', 'f': 2, 'name': 'Tax Free (the big walk-through shop beside the baggage claim)', 'x0': -241.1, 'z0': 8, 'x1': -80, 'z1': 52, 'kind': 'taxfree', 'glass': 'n'})
P['rooms'].append({'id': 'F1-TAXFREE-UNDER', 'f': 1, 'name': 'Under the tax free (empty for now)', 'x0': -241.1, 'z0': 8, 'x1': -80, 'z1': 52, 'kind': 'empty'})
P['rooms'].append({'id': 'F1-EAST-WING', 'f': 1, 'name': 'East wing (empty for now)', 'x0': 156, 'z0': 0, 'x1': 551.2, 'z1': 52, 'kind': 'empty'})
json.dump(P, open(D('terminal-plan.json'), 'w'), indent=1)
print('stands', [s['id'] for s in A['stands']]); print('rooms', len(P['rooms']), 'plan stands', len(P['stands']))
