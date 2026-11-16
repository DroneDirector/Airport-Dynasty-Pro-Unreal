# The baggage claim blueprint (owner, 2026-11-16): the terminal's layout on both floors with the arrivals wing in it.
# The wing takes the museum hall's place (the museum hall and everything in it removed - only that) and runs west flush
# to the end of the gates' pier: x -80..0. The terminal as it stands today at its west end (the escalator room, the
# plant room 6 m narrower, the Floor 2 landing with the toilets and family room - Art/TerminalAnnex/make_annex_west.py)
# is drawn over Data/terminal-plan.json, which is older there. Black background, light lines (documents rule).
#   python Tools/make_baggage_claim_blueprint.py   ->  docs/Baggage-Claim-Blueprint.png
import json, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, FancyArrowPatch
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
BG, INK, DIM, NEW, FLOW, BAG, GONE = '#000000', '#e8edf2', '#5b6773', '#35d0e0', '#ff5a5a', '#f2b544', '#b04040'
# the plan file's rooms that are no longer where it says (the west end rebuilt 2026-11-15)
OLD = {'F1-MUSEUM', 'F1-PLANT', 'F2-WC-M', 'F2-WC-F', 'F2-FAMILY'}
TODAY = {1: [('Escalator room', 0, 0, 36, 30), ('Plant room', 6, 30, 36, 52)],
         2: [('Landing', 0, 0, 21, 30), ('WC', 0, 0, 14, 5), ('WC', 0, 5, 14, 10), ('Family', 0, 10, 14, 22)]}
NEW_F1 = [
    ('F1-CLAIM', 'BAGGAGE CLAIM HALL  -  5 belts  -  80 x 36 m', -80, 16, 0, 52),
    ('F1-STORE', 'store', 0, 30, 6, 52),
    ('F1-ARR-WC', 'Toilets', -80, 8, -56, 16),
    ('F1-CUSTOMS', 'Customs (green / red)', -56, 8, -24, 16),
    ('F1-CUST-OFF', 'Customs office', -24, 8, 0, 16),
    ('F1-ARRIVALS', 'ARRIVALS HALL (meeting rail, desks, cafe)', -80, -4, 0, 8),
]
NEW_F2 = [('F2-ARR', 'Arrivals\ncorridor', -80, 40, -66, 52)]
BELTS = [(-60.8, 'Belt 1'), (-48.15, 'Belt 2'), (-35.5, 'Belt 3'), (-22.85, 'Belt 4'), (-10.2, 'Belt 5')]

def floor(ax, f, title, new):
    ax.set_facecolor(BG)
    for r in P['rooms']:
        if r['id'] in OLD or (r.get('floor') or (2 if r['id'].startswith('F2') else 1)) != f: continue
        x0, z0, x1, z1 = r['x0'], r['z0'], r['x1'], r['z1']
        ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fill=False, ec=DIM, lw=0.8))
        if (x1 - x0) > 9 and (z1 - z0) > 5:
            ax.text((x0 + x1) / 2, (z0 + z1) / 2, r['name'].split(' (')[0], color='#8d99a6', fontsize=5.2, ha='center', va='center')
    for name, x0, z0, x1, z1 in TODAY[f]:
        ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fill=False, ec=DIM, lw=0.8))
        ax.text((x0 + x1) / 2, (z0 + z1) / 2, name, color='#8d99a6', fontsize=5.0 if x1 - x0 > 10 else 4.2, ha='center', va='center')
    for s in P['stairs']:
        fl = 1 if s['from'].startswith('F1') else 2
        if fl == f or (f == 2 and s['to'].startswith('F2')):
            ax.add_patch(Rectangle((s['x0'], s['z0']), s['x1'] - s['x0'], s['z1'] - s['z0'], fc='#2a3440', ec=DIM, lw=0.5))
    for rid, name, x0, z0, x1, z1 in new:
        ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc=NEW + '22', ec=NEW, lw=1.6))
        lx, lz = {'F1-CLAIM': (-40, 49.2), 'F2-ARR': (-52, 45)}.get(rid, ((x0 + x1) / 2, (z0 + z1) / 2))
        ax.text(lx, lz, name, color=NEW, fontsize=6.0 if rid != 'F1-STORE' else 4.2, ha='center', va='center', fontweight='bold', rotation=90 if rid == 'F1-STORE' else 0)
    ax.set_title(title, color=INK, fontsize=10, loc='left')
    ax.set_aspect('equal'); ax.tick_params(colors=DIM, labelsize=6)
    for sp in ax.spines.values(): sp.set_color(DIM)
    ax.set_xlim(-90, 246); ax.set_ylim(-16, 86)
    ax.text(-88, -13.5, 'KERB / FORECOURT (landside)', color='#8d99a6', fontsize=6)
    ax.text(-88, 83.5, 'APRON (airside)', color='#8d99a6', fontsize=6)
def arrow(ax, pts, col=FLOW, lw=1.6):
    for a, b in zip(pts, pts[1:]):
        ax.add_patch(FancyArrowPatch(a, b, arrowstyle='-|>', mutation_scale=9, color=col, lw=lw, ls='--'))

fig = plt.figure(figsize=(16.5, 11.7), facecolor=BG)
a2 = fig.add_axes([0.04, 0.565, 0.92, 0.355]); a1 = fig.add_axes([0.04, 0.155, 0.92, 0.355])
floor(a2, 2, 'FLOOR 2 (gates level)  -  arriving passengers walk west along the walkway, through its end door onto the escalators', NEW_F2)
floor(a1, 1, 'FLOOR 1 (ground)  -  the arrivals wing in the museum hall\'s place, flush with the end of the gates: claim, customs, arrivals hall, out', NEW_F1)
# the museum hall's outline (removed), dotted
for ax in (a1,):
    ax.add_patch(Rectangle((-36, 0), 36, 52, fill=False, ec=GONE, lw=1.0, ls=(0, (2, 3))))
    ax.text(-18, 12.0, 'museum hall\n(removed with\neverything in it)', color=GONE, fontsize=5.0, ha='center', va='center')
# the escalators (2), stair and lift at the west end, down from the corridor
for ax in (a2, a1):
    ax.add_patch(Rectangle((-79.6, 39), 5.4, 12.5, fc='#14505a', ec=NEW, lw=0.8)); ax.text(-76.9, 45, 'esc.\n+\nstair', color=INK, fontsize=4.6, ha='center', va='center')
    ax.add_patch(Rectangle((-73.4, 47.6), 2.2, 2.8, fc='#14505a', ec=NEW, lw=0.8)); ax.text(-72.3, 46.6, 'lift', color=INK, fontsize=4.2, ha='center', va='center')
arrow(a2, [(200, 66), (120, 58), (40, 58), (-40, 58), (-76, 58), (-76.9, 51)])
a2.text(60, 63.5, 'arriving passengers (all four gates)', color=FLOW, fontsize=6.5)
a2.add_patch(Rectangle((-80, 51.6), 8, 0.8, fc=NEW, ec=NEW)); a2.text(-72, 53.6, 'open door from the walkway (8 m)', color=NEW, fontsize=5.5)
# floor 1: the belts, fed from the sorting hall through the north wall; the passengers' way round them and out
for cx, name in BELTS:
    a1.add_patch(FancyBboxPatch((cx - 2.45, 21.05), 4.9, 19.9, boxstyle='round,pad=0,rounding_size=2.4', fc='#3a2e10', ec=BAG, lw=1.3))
    a1.text(cx, 31, name, color=BAG, fontsize=5.6, ha='center', va='center', rotation=90, fontweight='bold')
    arrow(a1, [(cx, 60), (cx, 38.5)], BAG, 1.2)
a1.text(-58, 61, 'bags from the sorting hall: each flight to its own belt', color=BAG, fontsize=6)
arrow(a1, [(-76.9, 38.5), (-70, 30), (-55, 18.5), (-40, 17), (-40, 9), (-40, 3), (-40, -3.6)])
a1.text(-78, -10.5, 'out to the kerb (taxi, bus, car park)', color=FLOW, fontsize=5.5)
a1.add_patch(Rectangle((-48, -4.6), 16, 1.2, fc=NEW, ec=NEW)); a1.text(-40, -7.0, 'arrivals doors + canopy', color=NEW, fontsize=5.5, ha='center')
fig.text(0.04, 0.965, 'AIRPORT DYNASTY PRO  -  BAGGAGE CLAIM (ARRIVALS WING)  -  v2: 80 m, 5 belts, flush with the gates', color=INK, fontsize=14, fontweight='bold')
fig.text(0.04, 0.945, 'Grey: the terminal as it stands (west end as rebuilt 15 Nov).  Cyan: new.  Dotted red: the museum hall, removed.  Red dashes: the passengers\' way.  Amber: the bags\' way and the belts.', color='#a9b4bf', fontsize=8.5)
notes = [
    '1  The museum hall (x -36 to 0) and everything inside it is removed - nothing else. The wing runs from the escalator room\'s west wall (x 0) to the end of the gates\' pier (x -80): 80 m.',
    '2  Nothing stands above it: the Floor 2 toilets and family room start at x 0 on the landing and stay. The ground west of the museum (x -80 to -36) is empty.',
    '3  Arriving passengers walk the walkway to its west end, through an 8 m open door straight onto the escalators (2), the stair or the lift - their landing kept clear.',
    '4  Five belts, 19.9 x 4.9 m each, 7.75 m of walking room between and round them; each fed from its own input conveyor in the sorting hall behind the north wall.',
    '5  Out: through the customs doorway (green channel straight on, red to the desk), into the arrivals hall, through four sliding doors under a canopy to the kerb.',
    '6  The museum strip (x 0 to 6, z 30 to 52) becomes a store off the claim hall; the museum\'s kerb canopy is replaced by the arrivals canopy.',
]
fig.text(0.04, 0.012, '\n'.join(notes), color='#cfd7de', fontsize=7.6, va='bottom', linespacing=1.4)
out = os.path.join(ROOT, 'docs', 'Baggage-Claim-Blueprint.png')
fig.savefig(out, dpi=170, facecolor=BG)
print('BLUEPRINT', out)
