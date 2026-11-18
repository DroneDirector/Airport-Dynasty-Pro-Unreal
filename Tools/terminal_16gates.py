# The 16 gates (owner, 2026-11-17; blueprints docs/Blueprints/16-Gates-*.png, Tools/make_16_gates_blueprint.py), part 1:
# the gate walkway (Floor 2) and the baggage hall under it (Floor 1) lengthened at both ends, and their gate rooms.
#   west  x -241.1 .. -80 : two gate pairs exactly like today's (copies of gate 1 / view bay 1 / ramp 1 / gate 5),
#                           gates 9 + 11 with jetways, 10 + 12 walk-out
#   east  x 242 .. 551.2  : four wide-body gates (13 = A380, 14-16 = B777), each a copy of today's gate-4 pair
#                           (gate room + view bay over ramp room + ground room), centred on its stand
# Everything is a copy of the terminal's own pieces (the same meshes: instanced, Nanite), each copy under its own name;
# long pieces (ceiling, sills, purlins, floor slabs, vault glass) are copied and scaled along the walkway only, laid so
# they meet the old ones edge to edge (no overlap: no flicker); the vault's ribs every 3 m as today; the end gables
# moved to the new ends. The walls with their openings come from Art/TerminalAnnex/make_pier_extension.py.
# Run from Tools/terminal_edit.py (exec, shares its N, byname, parent, BOX, copy_tree, drop, log).
import re

MOD = 80.55
W_END, E_END = -80.0 - 2 * MOD, 551.2
WB = [(13, 'A380', 289.4), (14, 'B777', 369.2), (15, 'B777', 441.5), (16, 'B777', 513.8)]   # gate, type, stand x

def find(name):
    return byname[name]

def copy_named(k, old, new, skip_sign=True):
    """Copy node k's subtree, every name with the old room id swapped for the new one, the signs left out."""
    pat = re.compile(re.escape(old) + r'(?!\d)')
    def rec(i):
        n = dict(N[i]); n['name'] = pat.sub(new, n['name'])
        kids = [rec(c) for c in N[i].get('children', []) if not (skip_sign and 'sign' in N[c]['name'].lower())]
        if kids: n['children'] = kids
        elif 'children' in n: del n['children']
        N.append(n); return len(N) - 1
    return rec(k)

def shifted(k, dx_plan, name=None, scale=None):
    """A group node holding node k (a copy), moved dx_plan metres east (layout x is -plan x)."""
    g = {'name': name or (N[k]['name'] + '@%+.1f' % dx_plan), 'translation': [-dx_plan, 0.0, 0.0], 'children': [k]}
    if scale: g['scale'] = scale
    N.append(g); return len(N) - 1

def scaled_span(k, s0, s1, a, b, name):
    """A copy of node k (plan span s0..s1 along x) stretched/shrunk along x only to lie exactly on a..b."""
    c = copy_named(k, '\0', '\0', skip_sign=False)
    kx = (b - a) / (s1 - s0)
    g = {'name': name, 'translation': [s0 * kx - a, 0.0, 0.0], 'scale': [kx, 1.0, 1.0], 'children': [c]}
    N.append(g); return len(N) - 1

# ---------------- the gate rooms ----------------
for k, (jet, walk) in ((2, (9, 10)), (1, (11, 12))):                 # k modules west of gates 1/5
    dx = -k * MOD
    for src, new in (('F2-GATE-1', 'F2-GATE-%d' % jet), ('F2-VIEW-1', 'F2-VIEW-%d' % jet),
                     ('F1-RAMP-1', 'F1-RAMP-%d' % jet), ('F1-GATE-5', 'F1-GATE-%d' % walk)):
        c = copy_named(find(src), src, new)
        N[parent[find(src)]]['children'].append(shifted(c, dx, new))
        log.append('new: %s (copy of %s, %.2f m west)' % (new, src, -dx))
for gate, kind, sx in WB:                                             # today's gate-4 pair, centred on the stand
    dx = sx - 203.05
    for src, new in (('F2-GATE-4', 'F2-GATE-%d' % gate), ('F2-VIEW-4', 'F2-VIEW-%d' % gate),
                     ('F1-RAMP-4', 'F1-RAMP-%d' % gate), ('F1-GATE-8', 'F1-RAMP-%dB' % gate)):
        c = copy_named(find(src), src, new)
        N[parent[find(src)]]['children'].append(shifted(c, dx, new))
        log.append('new: %s (%s gate, copy of %s at x %.1f)' % (new, kind, src, 169.05 + dx))

# ---------------- the walkway's vault and slabs: long pieces stretched only along x, edge to edge ----------------
def long_piece(name, s0, s1, parent_name):
    k = find(name); p = find(parent_name)
    N[p]['children'].append(scaled_span(k, s0, s1, W_END + (s0 + 80.0), s0, name + '-W'))
    N[p]['children'].append(scaled_span(k, s0, s1, s1, E_END + (s1 - 242.0), name + '-E'))
    log.append('new: %s lengthened both ways' % name)
long_piece('F2-WALK/CEILING/FLAT', -80.1, 242.1, 'F2-WALK/CEILING')
long_piece('F2-WALK/CEILING/SILLS', -80.1, 242.1, 'F2-WALK/CEILING')
for i in range(1, 6): long_piece('F2-WALK/CEILING/PURLIN-%d' % i if i > 1 else 'F2-WALK/CEILING/PURLIN-1', -80.0, 242.0, 'F2-WALK/CEILING/PURLINS')
long_piece('F2-WALK/STRUCTURE-terrazzo', -80.0, 242.0, 'F2-WALK')
long_piece('F2-WALK/STRUCTURE-concrete', -80.0, 242.0, 'F2-WALK')
# the vault glass: 46 m bays as today's, a little shorter or longer so whole bays fill each end
g1 = find('F2-WALK/CEILING/GLASS-1'); pc = find('F2-WALK/CEILING')
for a, b, n in ((W_END, -80.0, 4), (242.0, E_END, 7)):
    seg = (b - a) / n
    for j in range(n):
        N[pc]['children'].append(scaled_span(g1, -80.0, -34.0, a + j * seg, a + (j + 1) * seg, "F2-WALK/CEILING/GLASS-X%+.0f" % (a + j * seg)))
log.append('new: vault glass bays both ends')
# the ribs every 3 m, as today's (-78.5 .. 239.5)
rib = find('F2-WALK/CEILING/RIB'); pr = find('F2-WALK/CEILING/RIBS'); nr = 0
xs = [-78.5 - 3 * j for j in range(1, 200) if -78.5 - 3 * j > W_END + 1.0] + [239.5 + 3 * j for j in range(1, 200) if 239.5 + 3 * j < E_END - 1.0]
for x in xs:
    r = dict(N[rib]); r['name'] = 'F2-WALK/CEILING/RIB-X%+.1f' % x; r['translation'] = [-x, 0, 0]
    N.append(r); N[pr]['children'].append(len(N) - 1); nr += 1
log.append('new: %d vault ribs' % nr)
# the gables to the new ends
N[find('F2-WALK/CEILING/GABLE-W')]['translation'] = [-(W_END + 80.0), 0, 0]
N[find('F2-WALK/CEILING/GABLE-E')]['translation'] = [-(E_END - 242.0), 0, 0]
log.append('moved: the vault gables to x %.1f and %.1f' % (W_END, E_END))

# ---------------- the walkway's and the baggage hall's things, repeated along the new parts ----------------
def plan_x0x1(i):
    b = BOX.get(i)
    return (b[0], b[3]) if b else None
def repeat(parent_name, src_lo, src_hi, dx, lo, hi, skip):
    p = find(parent_name); n = 0
    for c in list(N[p]['children']):
        nm = N[c]['name']
        if any(s in nm for s in skip) or '@' in nm or nm.endswith(('-W', '-E')): continue
        r = plan_x0x1(c)
        if not r or r[0] < src_lo or r[1] > src_hi: continue
        if r[0] + dx < lo or r[1] + dx > hi: continue
        cp = copy_named(c, '\0', '\0', skip_sign=False)
        N[p]['children'].append(shifted(cp, dx, nm + '@%+.1f' % dx)); n += 1
    log.append('new: %d copies in %s (%+.1f m)' % (n, parent_name, dx))
SKIPW = ('STRUCTURE', 'CEILING/', 'MOVING-WALKWAY', 'WALKWAY-RAILS')
repeat('F2-WALK', -80.0, 81.2, -161.1, W_END + 0.3, -80.3, SKIPW)
repeat('F2-WALK', -80.0, 229.3, 322.2, 242.3, E_END - 0.3, SKIPW)
repeat('F1-BAGHALL', -80.0, 81.2, -161.1, W_END + 0.3, -80.3, ('STRUCTURE',))
repeat('F1-BAGHALL', -80.0, 229.3, 322.2, 242.3, E_END - 0.3, ('STRUCTURE',))
# the moving walkways and their rails: 70 m modules, 10 m apart west as today, 7.5 m apart east
pw = find('F2-WALK')
for cx in (-120.6, -200.6, 280.0, 357.5, 435.0, 512.5):
    for src in ('F2-WALK/MOVING-WALKWAY-001', 'F2-WALK/MOVING-WALKWAY-002', 'F2-WALK/WALKWAY-RAILS-001'):
        cp = copy_named(find(src), '\0', '\0', skip_sign=False)
        N[pw]['children'].append(shifted(cp, cx - (-40.0), src + '@x%+.1f' % cx))
log.append('new: 6 moving walkway pairs with their rails')

# ---------------- the new walls (Art/TerminalAnnex/make_pier_extension.py) ----------------
add('F1', 'F1-PIER-EXT/SOLID', 'pier_ext_solid')
add('F1', 'F1-PIER-EXT/GLASS', 'pier_ext_glass')

# ---------------- the car rental moved whole (16 gates: the east wing's room), beside the main car park outside the
#                  airport boundary: +242 m east, -231.3 m south (its parking in Data/roads.json moves the same) ----------------
_r = find('RENTAL'); _t = N[_r].get('translation', [0, 0, 0])
N[_r]['translation'] = [_t[0] - 242.0, _t[1], _t[2] - 231.3]
log.append('moved: the car rental to plan x %.0f, z %.0f' % (-N[_r]['translation'][0], N[_r]['translation'][2]))

# ---------------- the wave roof over both new ends (owner, 2026-11-17: "as we did on the left side, so it covers the
#                  entire length of the new area"): copies of the Grand Hall's wave (x 24..126, z -16..34, only the wave -
#                  never the pier's solar field or rooftop units), joined end to end like the arrivals copy; each copy
#                  a little shorter (west, 2 of them) or longer (east, 4) along the building only, so whole waves fill
#                  each end - the zinc's seams run down the slope, along the building, so their spacing never changes;
#                  under each its two tree pillars on the kerb ----------------
def roof_copy(a, b, tag):
    c = copy_tree(byname['ROOF'], skip=('ROOF/SOLAR-PANELS', 'ROOF/ROOF-UNIT'))
    kx = (b - a) / 102.0
    g = {'name': 'ROOF-' + tag, 'translation': [24.0 * kx - a, 0.0, 0.0], 'scale': [kx, 1.0, 1.0], 'children': [c]}
    N.append(g); N[0]['children'].append(len(N) - 1)
    for src, x in (('C-7', 56.0), ('C-8', 94.0)):
        p = copy_tree(byname[src]); N[p]['name'] = src + '-' + tag
        N[p]['translation'] = [-(a + (x - 24.0) * kx), 0.0, -9.0]
        N[parent[byname[src]]]['children'].append(p)
    log.append('new: wave roof %s (x %.1f..%.1f, %.0f%% along the building) and its two tree pillars' % (tag, a, b, kx * 100))
for j in range(2):
    a = -78.0 - (j + 1) * (W_END - 4.0 - (-78.0)) / -2.0
    roof_copy(a, a + (-78.0 - (W_END - 4.0)) / 2.0, 'W%d' % (j + 1))
for j in range(4):
    L4 = (E_END + 4.0 - 126.0) / 4.0
    roof_copy(126.0 + j * L4, 126.0 + (j + 1) * L4, 'E%d' % (j + 1))

# ---------------- the TAX FREE (owner's plan, Art/TerminalAnnex/make_taxfree.py): its shell and one-off pieces, then
#                  every repeated model placed many times (instanced, Nanite), and lamps under its slat ceiling ----------------
import json as _json, math as _math
_tf = _json.load(open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Art', 'TerminalAnnex', 'taxfree_place.json')))
add('F2', 'TAXFREE/SHELL-SOLID', 'tf_shell_solid')
add('F2', 'TAXFREE/SHELL-GLASS', 'tf_shell_glass')
add('F2', 'TAXFREE/FLOOR', 'tf_floor')
add('F2', 'TAXFREE/AURORA', 'tf_aurora')
_cnt = {}
for _p in _tf['place']:
    a = _p['asset']; _cnt[a] = _cnt.get(a, 0) + 1
    q = _math.radians(_p['rot']) / 2.0
    nd = {'name': 'TAXFREE/%s-%03d' % (a.upper(), _cnt[a]), 'asset': a, 'mesh': 90000 + len(N),
          'translation': [-_p['x'], _p['y'], _p['z']], 'rotation': [0.0, _math.sin(q), 0.0, _math.cos(q)]}
    if _p.get('s'): nd['scale'] = _p['s']
    N.append(nd); N[byname['F2']]['children'].append(len(N) - 1)
    if a == 'tf_boutique':
        g = dict(nd); g['name'] = nd['name'] + '/GLASS'; g['asset'] = 'tf_boutique_glass'; g['mesh'] = 90000 + len(N)
        N.append(g); N[byname['F2']]['children'].append(len(N) - 1)
log.append('new: the tax free - %d placed models (%s)' % (len(_tf['place']), ', '.join('%s x%d' % kv for kv in sorted(_cnt.items()))))
# (owner 2026-11-18: the tax free ends at x -119.15; the freed part is the escalator hall, its own lamps under its ceiling)
lamps('F2', 'TAXFREE', [-238 + 4 * i for i in range(40) if -238 + 4 * i < -120.5], [15, 21, 27, 33, 39, 45], 10.48)
lamps('F2', 'F2-ESCALATOR-HALL', [-115.0 + 6 * i for i in range(6)], [12, 20, 28, 36, 44], 11.33)
lamps('F1', 'F1-TAXFREE-UNDER', [-230 + 20 * i for i in range(8)], [20, 40], 5.33)
# the TAX FREE's neon sign (3D letters and its logo, Art/TerminalAnnex/make_taxfree_sign.py) on the wall over its way in
# and its way out, facing the gate walkway
for _x in (-130.09, -228.1):        # (its way in moved to its new east end, 2026-11-18)
    N.append({'name': 'TAXFREE/NEON-SIGN-X%+.0f' % _x, 'asset': 'tf_neon_sign', 'mesh': 90000 + len(N), 'translation': [-(_x - 0.255), 10.5, 52.14]})
    N[byname['F2']]['children'].append(len(N) - 1)
