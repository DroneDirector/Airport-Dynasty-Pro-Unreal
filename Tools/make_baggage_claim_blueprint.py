# The baggage claim blueprint (owner, 2026-11-16): the terminal's own layout (Data/terminal-plan.json, both floors)
# with the proposed arrivals wing drawn in: where arriving passengers come down from the gates' walkway, the baggage
# claim hall with three belts fed from the baggage sorting hall (each flight its own belt), customs, the arrivals hall
# and its doors to the kerb. Black background, light lines (documents rule). Writes docs/Baggage-Claim-Blueprint.png.
#   python Tools/make_baggage_claim_blueprint.py
import json, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, FancyArrowPatch
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
BG, INK, DIM, NEW, FLOW, BAG = '#000000', '#e8edf2', '#5b6773', '#35d0e0', '#ff5a5a', '#f2b544'

# the proposed rooms (plan metres: x along the building, z north / airside; floor 1 ground, floor 2 upper)
NEW_F1 = [
    ('F1-CLAIM', 'Baggage claim hall\n3 belts', -44, 16, 0, 52),
    ('F1-CUSTOMS', 'Customs\n(green / red)', -44, 8, -16, 16),
    ('F1-ARR-WC', 'Toilets', -16, 8, 0, 16),
    ('F1-ARRIVALS', 'Arrivals hall (meeters, taxi, car rental desks, cafe)', -44, -4, 0, 8),
]
NEW_F2 = [('F2-ARR', 'Arrivals corridor\n(down to baggage claim)', -44, 40, 0, 52)]
BELTS = [(-37, 22, -31, 44, 'Belt 1'), (-25, 22, -19, 44, 'Belt 2'), (-13, 22, -7, 44, 'Belt 3')]

def floor(ax, f, title, new):
    ax.set_facecolor(BG)
    for r in P['rooms']:
        if (r.get('floor') or (2 if r['id'].startswith('F2') else 1)) != f: continue
        x0, z0, x1, z1 = r['x0'], r['z0'], r['x1'], r['z1']
        ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fill=False, ec=DIM, lw=0.8))
        if (x1 - x0) > 9 and (z1 - z0) > 5:
            ax.text((x0 + x1) / 2, (z0 + z1) / 2, r['name'].split(' (')[0], color='#8d99a6', fontsize=5.2, ha='center', va='center')
    for s in P['stairs']:
        fl = 1 if s['from'].startswith('F1') else 2
        if fl == f or (f == 2 and s['to'].startswith('F2')):
            ax.add_patch(Rectangle((s['x0'], s['z0']), s['x1'] - s['x0'], s['z1'] - s['z0'], fc='#2a3440', ec=DIM, lw=0.5))
    for rid, name, x0, z0, x1, z1 in new:
        ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc=NEW + '22', ec=NEW, lw=1.6))
        lx, lz = {'F1-CLAIM': (-4, 34), 'F2-ARR': (-13, 46)}.get(rid, ((x0 + x1) / 2, (z0 + z1) / 2))
        ax.text(lx, lz, name, color=NEW, fontsize=6.4, ha='center', va='center', fontweight='bold', rotation=90 if rid == 'F1-CLAIM' else 0)
    ax.set_title(title, color=INK, fontsize=10, loc='left')
    ax.set_aspect('equal'); ax.tick_params(colors=DIM, labelsize=6)
    for sp in ax.spines.values(): sp.set_color(DIM)
    ax.set_xlim(-84, 246); ax.set_ylim(-14, 86)
    ax.text(-82, -11.5, 'KERB / FORECOURT (landside)', color='#8d99a6', fontsize=6)
    ax.text(-82, 83.5, 'APRON (airside)', color='#8d99a6', fontsize=6)
def arrow(ax, pts, col=FLOW, lw=1.6):
    for a, b in zip(pts, pts[1:]):
        ax.add_patch(FancyArrowPatch(a, b, arrowstyle='-|>', mutation_scale=9, color=col, lw=lw, ls='--'))

fig = plt.figure(figsize=(16.5, 11.7), facecolor=BG)
a2 = fig.add_axes([0.04, 0.565, 0.92, 0.355]); a1 = fig.add_axes([0.04, 0.155, 0.92, 0.355])
floor(a2, 2, 'FLOOR 2 (gates level)  -  arriving passengers off the jetways turn west along the walkway to the arrivals corridor', NEW_F2)
floor(a1, 1, 'FLOOR 1 (ground)  -  the new arrivals wing west of the museum: baggage claim, customs, arrivals hall, out to the kerb', NEW_F1)
# floor 2: the escalators down (two down, one up for staff / mistakes), a stair and a lift, at the corridor's west end
for ax in (a2, a1):
    ax.add_patch(Rectangle((-41, 41), 9, 9, fc='#14505a', ec=NEW, lw=0.8)); ax.text(-36.5, 45.5, 'escalators\n+ stair', color=INK, fontsize=5, ha='center', va='center')
    ax.add_patch(Rectangle((-30, 46), 3, 3, fc='#14505a', ec=NEW, lw=0.8)); ax.text(-28.5, 47.5, 'lift', color=INK, fontsize=4.5, ha='center', va='center')
arrow(a2, [(200, 66), (120, 58), (40, 58), (-22, 58), (-22, 50), (-33, 46)])
a2.text(60, 63.5, 'arriving passengers (all four gates)', color=FLOW, fontsize=6.5)
a2.text(-43, 53.2, 'door from the walkway (x -22)', color=NEW, fontsize=5.5)
# floor 1: the belts, fed from the sorting hall through the north wall; the flow down, round the belts, out
for x0, z0, x1, z1, name in BELTS:
    ax = a1
    ax.add_patch(FancyBboxPatch((x0, z0), x1 - x0, z1 - z0, boxstyle='round,pad=0,rounding_size=3', fc='#3a2e10', ec=BAG, lw=1.4))
    ax.text((x0 + x1) / 2, (z0 + z1) / 2, name, color=BAG, fontsize=6, ha='center', va='center', rotation=90, fontweight='bold')
    arrow(ax, [((x0 + x1) / 2, 60), ((x0 + x1) / 2, z1 + 0.5)], BAG, 1.3)
a1.text(-43, 61, 'bags from the sorting hall: each flight to its own belt', color=BAG, fontsize=6)
arrow(a1, [(-36.5, 40), (-43, 34), (-43, 20), (-30, 14), (-30, 9), (-20, 3), (-22, -3)])
a1.text(-12, 3.4, 'out to the kerb (taxi, bus, car park)', color=FLOW, fontsize=6)
a1.add_patch(Rectangle((-30, -4.6), 12, 1.2, fc=NEW, ec=NEW)); a1.text(-24, -6.6, 'arrivals doors', color=NEW, fontsize=5.5, ha='center')
fig.text(0.04, 0.965, 'AIRPORT DYNASTY PRO  -  BAGGAGE CLAIM (ARRIVALS WING)  -  PROPOSAL FOR YOUR OK', color=INK, fontsize=14, fontweight='bold')
fig.text(0.04, 0.945, 'Grey: the terminal as it is (terminal-plan.json, metres).  Cyan: new.  Red dashes: the passengers\' way.  Amber: the bags\' way and the belts.', color='#a9b4bf', fontsize=8.5)
notes = [
    '1  Where: a new wing west of the museum (x -44 to 0, 44 x 56 m) - the only side with free ground next to both the gates\' walkway (floor 2) and the baggage sorting hall (floor 1), and on the kerb side.',
    '2  Every arriving passenger walks the one walkway already there, then through a new door at its west end into the arrivals corridor - no crossing of departing passengers.',
    '3  Down by two escalators, a stair and a lift (lift for wheelchairs, prams, trolleys) straight into the claim hall.',
    '4  Three belts, each 22 m long, fed through the north wall from the sorting hall; a screen over each shows its flights - two or three flights share the hall at once, each on its own belt.',
    '5  Customs (green: nothing to declare, red: declare) between the belts and the arrivals hall, as at every international airport; toilets beside it.',
    '6  Arrivals hall: people meeting, taxi and bus desks, car rental desks, a small cafe; its own doors onto the kerb, west of the departures entrances.',
    '7  To check before building: the ground west of the museum (control tower site, roads, car parks) - the wing may need to move or shrink there.',
]
fig.text(0.04, 0.012, '\n'.join(notes), color='#cfd7de', fontsize=7.2, va='bottom', linespacing=1.35)
out = os.path.join(ROOT, 'docs', 'Baggage-Claim-Blueprint.png')
fig.savefig(out, dpi=170, facecolor=BG)
print('BLUEPRINT', out)
