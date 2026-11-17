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
drop(1981, 'rebuilt by the annex with the stairwell open')
lamps('F1', 'F1-ESC', [3, 9, 15], [4, 10, 16, 22, 28], 5.33)
lamps('F1', 'F1-ESC-HIGH', [24, 30], [4, 10, 16, 22, 28], 11.28)
lamps('F2', 'F2-ESC-LANDING', [17], [3, 9, 20, 26], 11.28)
lamps('F2', 'F2-OFFICES', [118, 124, 130, 136, 146, 151], [4, 10, 16, 22, 28, 34, 40], 11.28)
# the arrivals wing: the claim hall and the arrivals hall under their slatted ceilings (5.4 m), the escalators' void
lamps('F1', 'F1-CLAIM', [-66, -58, -50, -42, -34, -26, -18, -10, -2], [19, 25, 31, 37, 43, 49], 5.22)
lamps('F1', 'F1-ARRIVALS', [-76, -68, -60, -52, -44, -36, -28, -20, -12, -4], [-1, 5, 11], 5.22)
lamps('F1', 'F1-CLAIM-VOID', [-77, -72], [38, 44, 50], 11.28)
json.dump(L, open(OUT, 'w'))
print('\n'.join(log))
print('layout written:', OUT)
