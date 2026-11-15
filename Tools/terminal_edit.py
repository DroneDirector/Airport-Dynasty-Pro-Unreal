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
# its inside, kept as it stands, moved 44 m west and 10 m north (into the middle of the new hall)
t = N[m].get('translation', [0, 0, 0])
N[m]['translation'] = [t[0] + 44.0, t[1], t[2] + 10.0]
log.append('moved: the museum\'s inside 36 m west, 10 m north')

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
for nm in ('F2-WC-M', 'F2-WC-F', 'F2-FAMILY'):
    i = byname[nm]
    N[i]['rotation'] = [0, 1, 0, 0]
    N[i]['translation'] = [-7.0 - 97.0, 0.0, 11.0 + 41.0]
    log.append('moved: %s onto the escalator landing' % nm)

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

# ---- the museum's door (owner, 2026-11-15: "make the museum door open ... not a single door"): a copy of the main
#      entrance's double sliding glass door (5 m, two leaves sliding apart by themselves), its sign left out, in the
#      museum hall's south wall at x -18 ----
def copy_tree(k, skip=()):
    n = dict(N[k]); kids = [copy_tree(c, skip) for c in N[k].get('children', []) if not N[c]['name'].startswith(skip)]
    if kids: n['children'] = kids
    elif 'children' in n: del n['children']
    N.append(n); return len(N) - 1
d = copy_tree(byname['D-F1-ENTR-C-1'], skip=('sign',))
N[d]['name'] = 'D-F1-MUSEUM-ENTR-1'; N[d]['translation'] = [18.0, 0.0, 0.0]
N[byname['F1']]['children'].append(d); log.append('new: the museum double sliding door')

# ---- the new parts (Art/TerminalAnnex, Tools/import_annex.py): placed as they are (made in the plan's metres) ----
def add(parent_name, name, asset):
    N.append({'name': name, 'mesh': 90000 + len(N), 'asset': asset})
    N[byname[parent_name]]['children'].append(len(N) - 1); log.append('new: ' + name)
add('F1', 'F1-ANNEX-WEST/SOLID', 'annex_west_solid')
add('F1', 'F1-ANNEX-WEST/GLASS', 'annex_west_glass')
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
lamps('F1', 'F1-ESC', [3, 9, 15], [4, 10, 16, 22, 28], 5.33)
lamps('F1', 'F1-ESC-HIGH', [24, 30], [4, 10, 16, 22, 28], 11.28)
lamps('F2', 'F2-ESC-LANDING', [17], [3, 9, 20, 26], 11.28)
lamps('F2', 'F2-OFFICES', [118, 124, 130, 136, 146, 151], [4, 10, 16, 22, 28, 34, 40], 11.28)
lamps('F1', 'F1-MUSEUM-HALL', [-32, -26, -20, -14, -8, -2], [4, 12, 20, 28, 36, 44], 11.28)
json.dump(L, open(OUT, 'w'))
print('\n'.join(log))
print('layout written:', OUT)
