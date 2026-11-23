# The owner's floor-2 changes to the terminal (2026-11-15, docs/Requests/Terminal-Floor2-Sections.md), applied to the
# terminal's own layout: always from the untouched copy (Data/terminal-layout-original.json) to Data/terminal-layout.json,
# so it can be run again after any change here. Plan metres: x east along the building, z north, y up; the layout's
# own x is the plan's -x.
#   python Tools/terminal_edit.py          (then Tools/terminal_cut.py for the openings in merged pieces)
import json, os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terminal_where as TW

ORIG = TW.D('terminal-layout-original.json'); OUT = TW.D('terminal-layout.json')
if not os.path.exists(ORIG): shutil.copy(OUT, ORIG)
L = json.load(open(ORIG)); N = L['nodes']
byname = {}
for i, n in enumerate(N): byname.setdefault(n['name'], i)
parent = {}
for i, n in enumerate(N):
    for c in n.get('children', []): parent[c] = i
log = []

def drop(i, why):
    p = parent.get(i)
    if p is not None and i in N[p].get('children', []):
        N[p]['children'].remove(i); log.append('out: %s (%s)' % (N[i]['name'], why))

def subtree_box(i):
    """The plan box of everything under node i (from the original layout's places)."""
    return BOX.get(i)

# the plan boxes of every subtree, from the original layout
_b = TW.boxes(ORIG)
BOX = {}
for k, nm, b in _b:
    j = k
    while j is not None:
        o = BOX.get(j)
        BOX[j] = b if o is None else (min(o[0], b[0]), min(o[1], b[1]), min(o[2], b[2]), max(o[3], b[3]), max(o[4], b[4]), max(o[5], b[5]))
        j = parent.get(j)

# ---- the museum: the round building goes, its inside moves to the new museum hall (x -36..0, z 0..52) ----
m = byname['museum']
ROUND = ('shell-', 'rib-', 'ribL-', 'sky-', 'rim-', 'gallery-', 'edge-', 'balustrade-', 'floor-')
for c in list(N[m]['children']):
    nm = N[c]['name']; b = BOX.get(c)
    if nm.startswith(ROUND): drop(c, 'the round museum building')
    elif b and b[0] > 35.0: drop(c, 'its old door from the Grand Hall')
# its inside: removed with the museum hall (owner, 2026-11-16: "delete the museum and everything that was in it" - the
# baggage claim's arrivals wing takes its place, Art/TerminalAnnex/make_arrivals_wing.py)
drop(m, 'the museum and everything in it (the baggage claim takes its place)')

# ---- the plant room's lamps over the strip that becomes museum (x < 6) ----
for c in list(N[byname['F1-PLANT']]['children']):
    b = BOX.get(c)
    if '/LAMP-' in N[c]['name'] and b and b[3] < 6.0: drop(c, 'over the museum strip')

# the plant room's shell (its floor and ceiling are single slabs over its whole width): rebuilt 6 m narrower by the
# annex (x 6..36), its door and equipment kept
for c in list(N[byname['F1-PLANT']]['children']):
    if '/STRUCTURE-' in N[c]['name']: drop(c, 'rebuilt by the annex, x 6..36')

# ---- Floor 2: the link's south glass opens onto the escalator landing ----
for c in list(N[byname['F2-LINK']]['children']):
    if N[c]['name'].startswith('w#'): drop(c, 'the link opens onto the landing')
# the food walk's south glass (x 14-56): out; rebuilt from x 21 (the annex), the part over the landing open
for c in list(N[byname['F2-FOOD']]['children']):
    if N[c]['name'].startswith('w#'): drop(c, 'rebuilt by the annex from x 21')

# ---- the Floor 2 toilets and family room: the three rooms together onto the landing's west side, turned round so
#      their doors open onto the landing (x 0..14, z 0..22; their old place goes to the bar) ----
#   layout: a half turn about y, then moved: old middle (plan 97, 41) -> new middle (plan 7, 11)
for nm in ('F2-WC-M', 'F2-WC-F'):
    i = byname[nm]
    N[i]['rotation'] = [0, 1, 0, 0]
    N[i]['translation'] = [-7.0 - 97.0, 0.0, 11.0 + 41.0]
    log.append('moved: %s onto the escalator landing' % nm)
# (the family room's door is on its east side already: moved only, so it opens onto the landing, not into the museum)
N[byname['F2-FAMILY']]['translation'] = [90.0, 0.0, -20.0]; log.append('moved: F2-FAMILY onto the escalator landing')
# ... and gone again (owner 2026-11-21: "remove the family room" - people walk off the escalators freely): its glass front
# (inside the men's toilets' own wall at z 10, which stays), its east wall with the door, frame and sign, its lamps,
# furniture and its floor/ceiling/roof pieces; the annex lays the landing's dark wood and its ceiling there
# (make_annex_west.py) and its lamps are the landing's (below, F2-ESC-LANDING-W)
drop(byname['F2-FAMILY'], 'the family room: open landing at the escalators top')

# ---- the bar: 32 m west at its full size (x 72..114, z 30..52), open to the gate walkway (its north doors and the
#      sign on that wall out; the wall itself is cut by Tools/terminal_cut.py) ----
i = byname['F2-BAR']; N[i]['translation'] = [32.0, 0.0, 0.0]; log.append('moved: F2-BAR 32 m west')
for c in list(N[i]['children']):
    if N[c]['name'] in ('D-F2-BAR-1', 'D-F2-BAR-2', 'M-F2-03'): drop(c, 'the bar opens onto the walkway')
# ---- tax free and the airside landing: their west 16 m (x 56..72), their shells rebuilt by the annex ----
for rn in ('F2-TAXFREE', 'F2-LANDING'):
    for c in list(N[byname[rn]]['children']):
        nm = N[c]['name']; b = BOX.get(c)
        if '/STRUCTURE-' in nm or nm.startswith('w#'): drop(c, 'rebuilt by the annex, x 56..72')
        elif b and b[3] > 71.8: drop(c, 'beyond x 72: the bar is there now')

# ---- the plant room's machines out of the museum strip: the air handler and the pump 6 m east, the pipes ending at
#      the plant room's new west wall (x 6.2..35) ----
for c in N[byname['F1-PLANT']]['children']:
    nm = N[c]['name']
    if nm.endswith(('/AIR-HANDLER-001', '/PUMP-001')):
        t = N[c].get('translation', [0, 0, 0]); N[c]['translation'] = [t[0] - 6.0, t[1], t[2]]; log.append('moved: ' + nm + ' 6 m east')
    elif nm.endswith('/PIPES-001'):
        # (each pipe itself shortened: a parent's stretch would act along the pipes' own turned axes, not the building's)
        t = N[c]['translation']; N[c]['translation'] = [-(6.2 + 35.0) / 2.0, t[1], t[2]]
        for k in N[c].get('children', []):
            sc = N[k].get('scale', [1, 1, 1])
            if sc[1] > 20: N[k]['scale'] = [sc[0], 35.0 - 6.2, sc[2]]
        log.append('shortened: the plant room pipes to x 6.2..35')
# ---- a canopy over the museum's door as over the main hall's entrances: the same 25 cm white slab with its lit
#      underside, on two round pillars (9 m wide here, for the 5 m door) ----
# ---- seats (owner, 2026-11-15: "all the chairs in each separate gate room should be there, and every chair in the
#      main corridor past the gates on the second floor ... other than that, you can remove chairs"): the seat rows stay
#      in the gate rooms, the view bays beside them and the Floor 2 gate walkway; every other row goes ----
KEEP_SEATS = ('F1-GATE-', 'F2-GATE-', 'F2-VIEW-', 'F2-WALK')
n_seats = 0
for i, n in enumerate(N):
    if '/SEATS-' in n['name'] and not n['name'].startswith(KEEP_SEATS) and i in parent and i in N[parent[i]].get('children', []):
        N[parent[i]]['children'].remove(i); n_seats += 1
log.append('out: %d seat rows outside the gates and the gate walkway' % n_seats)

# ---- the museum's door (owner, 2026-11-15: "make the museum door open ... not a single door"): a copy of the main
#      entrance's double sliding glass door (5 m, two leaves sliding apart by themselves), its sign left out, in the
#      museum hall's south wall at x -18 ----
def copy_tree(k, skip=()):
    n = dict(N[k]); kids = [copy_tree(c, skip) for c in N[k].get('children', []) if not N[c]['name'].startswith(skip)]
    if kids: n['children'] = kids
    elif 'children' in n: del n['children']
    N.append(n); return len(N) - 1
# (the museum's own door and canopy went with it, 2026-11-16) - the arrivals wing's way out instead: two copies of the
# main entrance's double sliding door in its kerb front (z -4) at x -43.5 and -36.5, and the entrances' canopy over
# both, 17 m wide on two pillars
for k, x in enumerate((-43.5, -36.5)):
    d = copy_tree(byname['D-F1-ENTR-C-1'], skip=('sign',))
    N[d]['name'] = 'D-F1-ARRIVALS-%d' % (k + 1); N[d]['translation'] = [-x, 0.0, -4.0]
    N[byname['F1']]['children'].append(d); log.append('new: arrivals sliding door %d' % (k + 1))
cp = copy_tree(byname['KERB/CANOPY-001'])
N[cp]['name'] = 'KERB/CANOPY-ARRIVALS'; N[cp]['translation'] = [40.0, 3.8, -6.0]
for c in N[cp]['children']:
    sc = N[c].get('scale', [1, 1, 1]); tr = N[c].get('translation', [0, 0, 0])
    if sc[0] > 5: N[c]['scale'] = [sc[0] + 4.0, sc[1], sc[2]]
    elif abs(tr[0]) > 5: N[c]['translation'] = [8.1 if tr[0] > 0 else -8.1, tr[1], tr[2]]
N[byname['F1']]['children'].append(cp); log.append('new: the arrivals canopy on two pillars')
# the plant pot at the gate walkway's west end stood in the arrivals doorway (x -80..-72): out
for i, n in enumerate(N):
    if n['name'].endswith('F2-WALK/PLANT-001') and i in parent: drop(i, 'in the arrivals doorway')

# ---- the wave roof over the arrivals wing (owner, 2026-11-16): a copy of the Grand Hall's wave roof (x 24..126, z -16..34)
#      exactly as it is (owner, 2026-11-16: the same in every direction), placed right against it on its west side
#      (x -78..24) so the two become one roof - the wave's two ends are the same height, so it runs on without a seam;
#      it may reach past the terminal's west end (the gates will be extended); and its two big tree pillars on the kerb
#      (C-7, C-8) copied to the same places under it (x -46, -8) ----
# (only the wave itself: the gates' solar panel field and rooftop units hang under ROOF too, and copied they floated west
#  of the building at 12 m - owner, 2026-11-17: 'parts that apparently were copied over')
rf = copy_tree(byname['ROOF'], skip=('ROOF/SOLAR-PANELS', 'ROOF/ROOF-UNIT'))
N[rf]['name'] = 'ROOF-ARRIVALS'; N[rf]['translation'] = [102.0, 0.0, 0.0]
N[0]['children'].append(rf); log.append('new: the wave roof over the arrivals wing (full size, joined to the main one, x -78..24)')
for src, x in (('C-7', 56.0 - 102.0), ('C-8', 94.0 - 102.0)):
    c = copy_tree(byname[src]); N[c]['name'] = src + '-ARRIVALS'; N[c]['translation'] = [-x, 0.0, -9.0]
    N[parent[byname[src]]]['children'].append(c); log.append('new: tree pillar %s under the arrivals roof' % src)

# ---- the new parts (Art/TerminalAnnex, Tools/import_annex.py): placed as they are (made in the plan's metres) ----
def add(parent_name, name, asset):
    N.append({'name': name, 'mesh': 90000 + len(N), 'asset': asset})
    N[byname[parent_name]]['children'].append(len(N) - 1); log.append('new: ' + name)
add('F1', 'F1-ANNEX-WEST/SOLID', 'annex_west_solid')
add('F1', 'F1-ANNEX-WEST/GLASS', 'annex_west_glass')
add('F1', 'F1-ARRIVALS/SOLID', 'annex_arrivals_solid')      # the arrivals wing (Art/TerminalAnnex/make_arrivals_wing.py)
add('F1', 'F1-ARRIVALS/GLASS', 'annex_arrivals_glass')
# lamps like the rest of the terminal's (their lights come with them): the escalator room under the landing and in its
# high part, the landing, the museum
def lamps(parent_name, prefix, xs, zs, y):
    k = 0
    for x in xs:
        for z in zs:
            k += 1
            N.append({'name': '%s/LAMP-%03d' % (prefix, k), 'translation': [-x, y, z], 'children': [len(N) + 1]})
            N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186})
            N[byname[parent_name]]['children'].append(len(N) - 2)
# the airside hall's ceiling went right over the stairs to Floor 2 (owner 2026-11-16): one 40 x 10 m panel, rebuilt by
# the annex with the stairwell open (Art/TerminalAnnex/make_annex_west.py)
drop(1981, 'rebuilt by the annex (since 2026-11-21 closed: no stairwell)')
# the stairs from the airside lobby up to Floor 2 (S-01, x 58..66, z 44..50): gone (owner 2026-11-21 - passengers reach
# Floor 2 only by the escalators): its steps, its side balustrades and rails, and its old Floor 2 guardrail; the slab and
# the hall's ceiling are closed over it by the annex (make_annex_west.py), and the hall's lamp rows (every 4 m at z 44.5
# and 49.5, missing over the stairwell) filled in at x 60 and 64
drop(byname['S-01'], 'the airside lobby stairs: Floor 2 only by the escalators')
# (the lamps: appended at the very end, below, so no node added before them changes its index)
lamps('F1', 'F1-ESC', [3, 9, 15], [4, 10, 16, 22, 28], 5.33)
lamps('F1', 'F1-ESC-HIGH', [19, 25, 31], [4, 10, 16, 22, 28], 11.28)     # (the landing 5 m shorter, 2026-11-21: x 16..36 open to the roof)
lamps('F2', 'F2-ESC-LANDING', [14.5], [3, 9, 20, 26], 11.28)
# (the offices' lamps: room by room since 2026-11-21, with the partitions - below)
# the arrivals wing: the claim hall and the arrivals hall under their slatted ceilings (5.4 m), the escalators' void
lamps('F1', 'F1-CLAIM', [-66, -58, -50, -42, -34, -26, -18, -10, -2], [19, 25, 31, 37, 43, 49], 5.22)
lamps('F1', 'F1-ARRIVALS', [-76, -68, -60, -52, -44, -36, -28, -20, -12, -4], [-1, 5, 11], 5.22)
lamps('F1', 'F1-CLAIM-VOID', [-77, -72], [38, 44, 50], 11.28)
# the crown lights over the escalators (owner 2026-11-18: hanging on cables, a big crown; Art/TerminalAnnex/make_crown_light.py):
# the crown itself only glows (its material - no light of its own, no flare); its light is ONE terminal lamp under its hub
# (a LAMP node: the usual panel light with its distance limit), its glowing diffuser the crown's underside (performance
# lessons, 2026-11-17: every extra light and sprite costs - so one light per crown, nothing more)
for _nm, _asset, (_x, _y, _z) in (('F1-CROWN-HALL', 'crown_light_hall', (24.5, 8.6, 14.2)),   # (5 m west with the escalators, 2026-11-21)
                                     ('F1-CROWN-CLAIM', 'crown_light_claim', (-76.0, 8.0, 43.5))):
    N.append({'name': _nm, 'asset': _asset, 'mesh': 90000 + len(N), 'translation': [-_x, _y, _z]})
    N[byname['F1']]['children'].append(len(N) - 1)
    for _k, _dx in enumerate((-0.16, 0.16)):          # (two halves of one disc: the lamp system lights every other lamp)
        N.append({'name': '%s/LAMP-%03d' % (_nm, _k + 1), 'translation': [-(_x + _dx), _y + 0.03, _z], 'children': [len(N) + 1]})
        N.append({'name': 'box#lamp', 'scale': [0.32, 0.04, 0.64], 'mesh': 186})
        N[byname['F1']]['children'].append(len(N) - 2)
    log.append('new: %s (crown light over the escalators)' % _nm)
# ---- security before the escalators (owner 2026-11-21, Blueprints/Terminal/Security-Escalators-v3.png; the glass wall at
#      x 42, the corridor and the toilets' new door are Art/TerminalAnnex/make_annex_west.py) ----
# the plant room and the cleaners store: gone - the airside corridor runs through them
drop(byname['F1-PLANT'], 'the airside corridor and the service room take its place')
drop(byname['F1-CLEAN'], 'the airside corridor takes its place')
# the check-in islands 3.5 m east, clear of the new glass wall (desks 1-24, their chairs, belts, banners, queue posts)
n_ci = 0
for c in N[byname['F1-HALL']]['children']:
    nm = N[c]['name']
    if nm.startswith('F1-CHECKIN/') or nm.startswith('F1-HALL/OFFICE-CHAIR-') or nm.startswith('F1-HALL/QUEUE-POST-'):
        t = N[c]['translation']; N[c]['translation'] = [t[0] - 3.5, t[1], t[2]]; n_ci += 1
log.append('moved: %d check-in parts 3.5 m east' % n_ci)
# ---- two 4 m walk-throughs opposite the security doors (owner 2026-11-21, Blueprints/Terminal/Checkin-Security-Flow-v1.png):
#      island 1's desks k 6 + 7 and island 2's k 1 + 2 (0-based) go with their chairs, screens and number signs; the
#      remaining 20 renumbered 1-20 west to east (each desk's number sign is its own piece: the n-th desk wears desk n's);
#      the islands' banners and the queue lines open there too; the old collector belts go (the bags go under the hall:
#      DepartureBelts) ----
hall = N[byname['F1-HALL']]['children']
GONE_DESKS = ('F1-CHECKIN/DESK-07', 'F1-CHECKIN/DESK-08', 'F1-CHECKIN/DESK-14', 'F1-CHECKIN/DESK-15')
GONE_CHAIRS = ('F1-HALL/OFFICE-CHAIR-007', 'F1-HALL/OFFICE-CHAIR-008', 'F1-HALL/OFFICE-CHAIR-014', 'F1-HALL/OFFICE-CHAIR-015')
sign_mesh = {}
for c in hall:
    nm = N[c]['name']
    if nm.startswith('F1-CHECKIN/DESK-'):
        sign_mesh[int(nm[-2:])] = [N[k]['mesh'] for k in N[c]['children'] if N[k]['name'].startswith('dn')][0]
for nm in GONE_DESKS + GONE_CHAIRS + ('F1-HALL/QUEUE-POST-003', 'F1-HALL/QUEUE-POST-004', 'F1-HALL/QUEUE-POST-007'):
    drop(byname[nm], 'a walk-through to security')
desks = sorted([c for c in hall if N[c]['name'].startswith('F1-CHECKIN/DESK-')], key=lambda c: -N[c]['translation'][0])
chairs = sorted([c for c in hall if N[c]['name'].startswith('F1-HALL/OFFICE-CHAIR-')], key=lambda c: -N[c]['translation'][0])
for n_, c in enumerate(desks, 1):
    N[c] = dict(N[c]); N[c]['name'] = 'F1-CHECKIN/DESK-%02d' % n_
    kids = []
    for k in N[c]['children']:
        if N[k]['name'].startswith('dn'):
            N.append(dict(N[k], mesh=sign_mesh[n_])); kids.append(len(N) - 1)
        else: kids.append(k)
    N[c]['children'] = kids
for n_, c in enumerate(chairs, 1):
    N[c] = dict(N[c]); N[c]['name'] = 'F1-HALL/OFFICE-CHAIR-%03d' % n_
log.append('renumbered: %d check-in desks, %d chairs' % (len(desks), len(chairs)))
# the belts and banners in two lengths each, open across the walk-throughs (plan x; their banner graphic on the longer)
# (the islands' old collector belts behind the desks go: each desk's weigh belt now drops through its hatch behind the
#  desk - where the old belt lay - into the collector in the tunnel under the hall)
for name in ('F1-CHECKIN/BELT-1', 'F1-CHECKIN/BELT-2'): drop(byname[name], 'the collector is in the tunnel under the hall now')
for name, pieces in (('F1-CHECKIN/BANNER-1', ((47.5, 60.4), (64.3, 72.5))), ('F1-CHECKIN/BANNER-2', ((74.5, 77.0), (80.9, 99.5)))):
    i = byname[name]; drop(i, 'split round the walk-through')
    longest = max(pieces, key=lambda p: p[1] - p[0])
    for k, (a, b) in enumerate(pieces):
        kids = []
        for c in N[i]['children']:
            n = dict(N[c])
            if 'scale' in n: n['scale'] = [n['scale'][0] - (25.0 - (b - a)), n['scale'][1], n['scale'][2]]
            elif (a, b) != longest: continue          # (the banner's graphic: once, unstretched)
            N.append(n); kids.append(len(N) - 1)
        t = N[i]['translation']
        N.append({'name': '%s-%s' % (name, 'AB'[k]), 'translation': [-(a + b) / 2, t[1], t[2]], 'children': kids})
        N[byname['F1-HALL']]['children'].append(len(N) - 1)
    log.append('split: %s round its walk-through' % name)

# the men's toilets' door onto the hall's side of the glass: x 39.5 -> 44.7 (its lobby: the annex)
t = N[byname['D-F1-WC-M-1']]['translation']; N[byname['D-F1-WC-M-1']]['translation'] = [t[0] - 5.2, t[1], t[2]]
log.append('moved: D-F1-WC-M-1 to x 44.7 (hall side of the glass wall)')
# the corridor's seats (gate rows of 10, backs to its north wall) and two flight screens facing them
def seat_row(name, x, z, n, half_turn):
    src = N[byname['F1-GATE-6/SEATS-001']]; kids = src['children']
    keep = kids[:3] + kids[3:3 + 2 * n]           # (the beam, its end legs, then seat + back pairs: the game rebuilds the row)
    new = []
    for k in keep:
        N.append(dict(N[k])); new.append(len(N) - 1)
    N.append({'name': name, 'translation': [-x, 0.0, z], 'children': new})
    if half_turn: N[-1]['rotation'] = [0, 1, 0, 0]
    N[byname['F1']]['children'].append(len(N) - 1); log.append('new: %s (%d seats)' % (name, n))
for k, x in enumerate((13.0, 24.0, 35.0)):
    seat_row('F1-AIRSIDE-CORR/SEATS-%03d' % (k + 1), x, 51.2, 10, True)
for k, x in enumerate((19.0, 30.0)):
    d = copy_tree(byname['M-F1-03']); N[d]['name'] = 'M-F1-CORR-%d' % (k + 1)
    N[d]['translation'] = [-x, 0.0, 44.6]; N[d]['rotation'] = [0, 0, 0, 1]
    N[byname['F1']]['children'].append(d); log.append('new: flight screen %d in the airside corridor' % (k + 1))
lamps('F1', 'F1-AIRSIDE-CORR', [7, 12, 17, 22, 27, 32, 37, 42, 47], [46, 50], 5.33)
lamps('F1', 'F1-ESC-PASS', [7, 13], [33, 38, 42], 5.33)
lamps('F1', 'F1-SERVICE', [21, 31], [34, 40], 5.33)

# ---- the departure baggage block (owner 2026-11-21, Blueprints/Terminal/Departure-Baggage-v2.png): the ground-floor
#      office block (offices 1-4, operations, staff toilets, crew room, staff corridor and passage, the Academy) cleared -
#      every wall, door, lamp, sign and piece of furniture; only their facades with the Floor 2 slab (STRUCTURE-white) and
#      their ceilings (STRUCTURE-offwhite) stay. The new rooms: Art/TerminalAnnex/make_bag_block.py (the belts:
#      DepartureBelts). The staff entrance hall stays as it is (its stair and lift up to the offices) ----
OFFBLOCK = ('F1-OFFICE-1', 'F1-OFFICE-2', 'F1-OFFICE-3', 'F1-OFFICE-4', 'F1-OPS', 'F1-STAFF-WC', 'F1-CREW', 'F1-STAFF-CORR',
            'F1-STAFF-PASS', 'F1-ACADEMY')
n_off = 0
for rn in OFFBLOCK:
    for c in list(N[byname[rn]]['children']):
        nm = N[c]['name']
        if nm.endswith(('/STRUCTURE-white', '/STRUCTURE-offwhite')): continue
        N[byname[rn]]['children'].remove(c); n_off += 1
log.append('out: %d parts of the ground-floor office block (its facades, slab and ceilings kept)' % n_off)
add('F1', 'F1-BAGBLOCK/SOLID', 'annex_bag_solid')
add('F1', 'F1-BAGBLOCK/GLASS', 'annex_bag_glass')
def door_copy(src, name, x, z):
    d = copy_tree(byname[src], skip=('sign',)); N[d]['name'] = name
    t = N[d]['translation']; N[d]['translation'] = [-x, t[1], z]
    N[byname['F1']]['children'].append(d); log.append('new: door %s' % name)
door_copy('D-F1-STAFF-CORR-1', 'D-F1-HBS-1', 118.0, 22.5)          # screening room -> sorting hall (secure, 1.6 m)
door_copy('D-F1-OFFICE-3-1', 'D-F1-L3-1', 150.0, 13.1)             # level-3 search room (in its glass front)
door_copy('D-F1-OFFICE-3-1', 'D-F1-EBS-1', 150.0, 19.0)            # early-bag store
lamps('F1', 'F1-HBS', [118, 124, 130, 136], [4, 10, 16], 5.33)
lamps('F1', 'F1-L3', [146], [4, 10], 5.33)
lamps('F1', 'F1-EBS', [146], [19], 5.33)
lamps('F1', 'F1-BAG-EAST', [153], [5, 12, 19], 5.33)
lamps('F1', 'F1-MAKEUP', [118, 124, 130, 136, 142, 148, 153], [26, 31, 36], 5.33)
lamps('F1', 'F1-CARTLANE', [118, 124, 130], [43, 48], 5.33)

# ---- the Floor 2 offices (owner 2026-11-21, Blueprints/Terminal/F2-Offices-v2.png + his final word): the partitions
#      (Art/TerminalAnnex/make_offices_f2.py), their doors (copies of the terminal's own toilet door, without its sign),
#      and the old ground-floor rooms' own things carried up (the meeting tables with their chairs, the paintings, the
#      operations room's clocks, the crew room's sofas, table, counter, coffee machine, lockers and TV - for the staff
#      lounge); the offices', operations room's and Academy's models are Offices.cpp's (Source/.../Offices.cpp) ----
add('F2', 'F2-OFFICES/SOLID', 'annex_offices_solid')
add('F2', 'F2-OFFICES/GLASS', 'annex_offices_glass')
F2Y = 6.01
for name, x, z in (('D-F2-OFFICE-1-1', 120.5, 9.0), ('D-F2-OFFICE-2-1', 132.8, 9.0), ('D-F2-OFFICE-3-1', 141.0, 9.0),
                   ('D-F2-OFFICE-4-1', 149.2, 9.0), ('D-F2-OPS-1', 126.0, 12.0), ('D-F2-ACADEMY-1', 140.0, 12.0),
                   ('D-F2-ACADEMY-2', 152.0, 12.0)):
    d = copy_tree(byname['D-F1-WC-M-1'], skip=('sign',)); N[d]['name'] = name
    N[d]['translation'] = [-x, F2Y, z]
    N[byname['F2']]['children'].append(d); log.append('new: door %s' % name)
def carry_up(nm, x, z, rot=None):
    """A ground-floor room's node up onto Floor 2 at plan (x, z) (it was dropped with its room above)."""
    i = byname[nm]; t = N[i].get('translation', [0, 0, 0])
    N[i]['translation'] = [-x, t[1] + F2Y, z]
    if rot is not None: N[i]['rotation'] = rot
    N[byname['F2']]['children'].append(i)
def carry_set(room, tx, tz):
    """A room's meeting table and its four chairs, as they stood round it, to the table's new place."""
    tt = N[byname[room + '/MEETING-TABLE-001']]['translation']; ox, oz = -tt[0], tt[2]
    for k in ('MEETING-TABLE-001', 'CHAIR-003', 'CHAIR-004', 'CHAIR-005', 'CHAIR-006'):
        t = N[byname[room + '/' + k]]['translation']
        carry_up(room + '/' + k, tx + (-t[0] - ox), tz + (t[2] - oz))
    log.append('moved up: %s meeting table and chairs to (%.1f, %.1f)' % (room, tx, tz))
carry_set('F1-OFFICE-1', 129.2, 4.0)       # your office, beside your desk
carry_set('F1-OFFICE-2', 128.5, 22.5)      # Operations: a briefing table
carry_set('F1-OFFICE-3', 125.0, 37.0)      # the staff lounge
carry_set('F1-OFFICE-4', 140.5, 30.0)      # the Academy's back
EAST = [0, -0.7071067811865475, 0, 0.7071067811865476]     # facing the plan's +x (on a room's west wall)
for room, x in (('F1-OFFICE-1', 118.78), ('F1-OFFICE-2', 131.08), ('F1-OFFICE-3', 139.18), ('F1-OFFICE-4', 147.38)):
    carry_up(room + '/PAINTING-001', x, 4.5, EAST); log.append('moved up: %s painting' % room)
carry_up('F1-OPS/CLOCKS-001', 132.72, 19.0); log.append('moved up: the operations clocks')
for k in ('SOFA-001', 'SOFA-002', 'TABLE-001', 'COUNTER-001', 'COFFEE-MACHINE-001', 'LOCKERS-001', 'TV-001'):
    t = N[byname['F1-CREW/' + k]]['translation']; carry_up('F1-CREW/' + k, -t[0], t[2])
log.append('moved up: the crew room into the staff lounge (x 114..129, z 42..52)')
for k, (x, z) in enumerate(((120.8, 50.6), (131.9, 27.2), (132.0, 50.6), (155.0, 10.5), (137.4, 50.6), (115.6, 31.2))):
    p_ = copy_tree(byname['F2-WALK/PLANT-002']); N[p_]['name'] = 'F2-OFFICES/PLANT-%03d' % (k + 1)
    N[p_]['translation'] = [-x, F2Y, z]; N[byname['F2']]['children'].append(p_)
log.append('new: 6 plants on the office floor')
for nm, xs, zs in (('F2-OFFICE-1', [121.5, 127.5], [2.8, 6.4]), ('F2-OFFICE-2', [135.0], [2.8, 6.4]), ('F2-OFFICE-3', [143.2], [2.8, 6.4]),
                   ('F2-OFFICE-4', [151.6], [2.8, 6.4]), ('F2-STAFF-CORR', [116, 122, 128, 134.5, 140, 146, 152], [10.5]),
                   ('F2-STAFF-CORR-N', [134.5], [17, 23, 29]), ('F2-WALKWAY', [116.3], [16, 22, 28]), ('F2-OPS', [122, 128], [15.5, 22]),
                   ('F2-ACADEMY', [140, 146, 152], [16, 22, 28]), ('F2-LOUNGE', [118, 124, 130], [31, 37, 43, 49]),
                   ('F2-STAFF-LOBBY', [137.5, 146, 151], [38, 44])):
    lamps('F2', nm, xs, zs, 11.28)

# the 16 gates (owner, 2026-11-17): Tools/terminal_16gates.py
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'terminal_16gates.py'), encoding='utf-8').read())
# ---- ramp services 13 (made by the 16 gates above): the departures vehicle passage (owner 2026-11-21) - its shell
#      rebuilt with 4 m roller door openings (Art/TerminalAnnex/make_bag_block.py; onto the baggage hall:
#      make_pier_extension.py); its two doors give way to the roller doors (ARollerDoor) ----
for i in [k for k, n in enumerate(N) if n['name'] in ('F1-RAMP-13/STRUCTURE-concrete', 'F1-RAMP-13/STRUCTURE-white', 'D-F1-RAMP-13-1', 'D-F1-RAMP-13-2')]:
    for p_ in N:
        if i in p_.get('children', []): p_['children'].remove(i); log.append('out: %s (ramp services 13: the vehicle passage)' % N[i]['name'])
# its lockers and one parked cart stood in the new lane (x 267.4..271.4): moved to its west side
for i, n in enumerate(N):
    if n['name'] in ('F1-RAMP-13/LOCKERS-001', 'F1-RAMP-13/BAGGAGE-CART-003') and n.get('translation'):
        t = n['translation']; n['translation'] = [-(259.4 if 'LOCKERS' in n['name'] else 257.4) + 86.35, t[1], t[2]]
        log.append('moved: %s out of the vehicle lane' % n['name'])
# the escalator landing's lamps where the family room was (x 0..14, z 10..22; its own lamp grid, under the landing's
# ceiling at +11.32 like the landing's own): appended last too
for _k, (_x, _z) in enumerate([(x, z) for x in (2.33, 7.0, 11.67) for z in (12.0, 16.0, 20.0)]):
    N.append({'name': 'F2-ESC-LANDING-W/LAMP-%03d' % (1 + _k), 'translation': [-_x, 11.28, _z], 'children': [len(N) + 1]})
    N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186})
    N[byname['F2']]['children'].append(len(N) - 2)
log.append('new: F2-ESC-LANDING-W/LAMP-001..009 (where the family room was)')
# the airside lobby's lamps over the closed stairwell (S-01 out, above): last, so every other node keeps its index
for _k, (_x, _z) in enumerate(((60, 44.5), (60, 49.5), (64, 44.5), (64, 49.5))):
    N.append({'name': 'F1-AIRSIDE/LAMP-%03d' % (17 + _k), 'translation': [-_x, 5.33, _z], 'children': [len(N) + 1]})
    N.append({'name': 'box#lamp', 'scale': [1.2, 0.04, 0.6], 'mesh': 186})
    N[byname['F1-AIRSIDE']]['children'].append(len(N) - 2)
log.append('new: F1-AIRSIDE/LAMP-017..020 (over the closed stairwell)')
# the one continuous wave roof (owner, 2026-11-23): Tools/terminal_wave.py, last, so every other node keeps its index
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'terminal_wave.py'), encoding='utf-8').read())
# the east side's shell (owner, 2026-11-23, v2 approved): Tools/terminal_east_edit.py, after the wave roof
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'terminal_east_edit.py'), encoding='utf-8').read())
json.dump(L, open(OUT, 'w'))
print('\n'.join(log))
print('layout written:', OUT)
