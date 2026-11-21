# Blueprint v1 of the departure baggage system (owner 2026-11-21): drawn on the terminal's own ground-floor plan
# (Data/terminal-plan.json); black background, light text (his documents rule). Output: Blueprints/Terminal/Departure-Baggage-v1.png
import json, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
REMOVED = {'F1-OFFICE-1', 'F1-OFFICE-2', 'F1-STAFF-PASS', 'F1-OPS', 'F1-STAFF-WC', 'F1-CREW', 'F1-STAFF-CORR', 'F1-ACADEMY', 'F1-OFFICE-3', 'F1-OFFICE-4', 'F1-STAFF-ENTR'}
BG, FG, DIM = '#0b1118', '#e8eef5', '#8a99a8'
NEW = '#f5b335'; BELT = '#3fd0ff'; OVER = '#c58cff'; ROAD = '#7be07b'

fig, ax = plt.subplots(figsize=(26, 15), dpi=120)
fig.patch.set_facecolor(BG); ax.set_facecolor(BG)
X0, X1, Z0, Z1 = 30, 205, -8, 90
for r in P['rooms']:
    if r.get('f') != 1: continue
    x0, z0, x1, z1 = r['x0'], r['z0'], r['x1'], r['z1']
    if x1 < X0 or x0 > X1: continue
    x0c, x1c = max(x0, X0), min(x1, X1)
    rem = r['id'] in REMOVED
    if rem or r['id'] == 'F1-EAST-WING': continue
    if r['id'] == 'F1-RAMP-4': ax.add_patch(Rectangle((x0c, z0), x1c - x0c, z1 - z0, fc='#17212c', ec='#5a6a7a')); continue
    if r['id'] == 'F1-HALL': r = dict(r, name='Grand Hall (check-in)')   # (the freed block is drawn new below)
    ax.add_patch(Rectangle((x0c, z0), x1c - x0c, z1 - z0, fill=True, fc='#17212c' if not rem else '#141414', ec='#ff6b6b' if rem else '#5a6a7a', lw=1.2, ls='--' if rem else '-'))
    name = r['name'].split('(')[0].strip()
    if rem: name = name + '\n(removed)'
    if x1c - x0c > 6:
        ax.text((x0c + x1c) / 2, 57 if r['id'] == 'F1-BAGHALL' else 26 if r['id'] == 'F1-HALL' else (z0 + z1) / 2, name, color=DIM, ha='center', va='center', fontsize=11)
# the existing arrivals system (kept): the claim hall belts and the trunk conveyor in the baggage hall
for bx in (-60.8, -48.15, -35.5, -22.85, -10.2):
    ax.add_patch(FancyBboxPatch((bx - 1.8, 31 - 7.5 - 1.8), 3.6, 15 + 3.6, boxstyle='round,pad=0,rounding_size=1.8', fc='none', ec='#5fa8d3', lw=1.2))
ax.add_patch(Rectangle((-80, 30), 80, 22, fill=False, ec='#5fa8d3', lw=1.5))

ax.plot([30, 183], [63, 63], color='#5fa8d3', lw=2.5)
ax.text(40, 64.6, 'arrivals trunk conveyor (kept, runs west to the claim hall)', color='#5fa8d3', fontsize=12.5)

# ---- NEW: departure baggage system ----
# 1. the weigh / tag belts at the 24 desks and the island collectors behind them (z 18.4)
for isl, xs in enumerate((44.0, 71.0)):
    for k in range(12):
        x = xs + 1.05 + k * 2.08
        ax.add_patch(Rectangle((x - 0.45, 16.9), 0.9, 1.0, fc=BELT, ec='none'))
    ax.plot([xs, xs + 25], [18.6, 18.6], color=BELT, lw=3)
ax.text(70, 11.5, '1  24 check-in desks: each a WEIGH + TAG belt (scale, label printer) onto the island collector', color=BELT, ha='center', fontsize=11.2)
# 2. the overhead link (3.5 m up, enclosed, over the hall's east end) to the new baggage block
ax.plot([96, 114], [18.6, 18.6], color=OVER, lw=4, ls=(0, (4, 2)))
ax.text(105, 21.3, '2  OVERHEAD LINK\n   3.5 m up, enclosed\n   (nothing on the floor)', color=OVER, ha='center', fontsize=10.2)
# 3. the new block where the offices / academy were (x 114-156, z 0-52)
ax.add_patch(Rectangle((114, 0), 42, 52, fill=False, ec=NEW, lw=2.2))
ax.text(135, 86.5, 'NEW DEPARTURE BAGGAGE BLOCK - in the old office / academy area (x 114-156): all its rooms removed', color=NEW, ha='center', fontsize=11.9, weight='bold')
ax.add_patch(Rectangle((114.5, 1), 27, 21, fc='#2a2210', ec=NEW, lw=1.3))
ax.text(128, 19.5, '3  HOLD BAGGAGE SCREENING', color=NEW, ha='center', fontsize=10.8, weight='bold')
for zz in (7, 13):
    ax.plot([114.5, 141], [zz, zz], color=BELT, lw=2.5)
    ax.add_patch(Rectangle((124, zz - 1.3), 6, 2.6, fc='#5a4510', ec=NEW, lw=1))
    ax.text(127, zz, 'CT scanner', color=FG, ha='center', va='center', fontsize=8.5)
ax.text(128, 3, '2 lines, EU Standard 3 (CT/EDS)\nbags kept in a queue if the X-ray needs a look', color=DIM, ha='center', fontsize=8.8)
ax.add_patch(Rectangle((142, 1), 13.5, 14, fc='#2a1414', ec='#ff8a8a', lw=1.3))
ax.text(148.75, 8, '4  LEVEL 3\nMANUAL SEARCH\n(rejected bags,\nwith the passenger\nif needed)', color='#ffb0b0', ha='center', va='center', fontsize=9.0)
ax.add_patch(Rectangle((142, 16), 13.5, 6, fc='#201a10', ec=NEW, lw=1))
ax.text(148.75, 19, 'early-bag store', color=NEW, ha='center', va='center', fontsize=8.8)
ax.plot([141, 141, 135], [7, 25, 25], color=BELT, lw=2.5); ax.plot([141, 141], [13, 13], color=BELT)
# 5. the sorter and the make-up carousels (by flight)
ax.add_patch(Rectangle((114.5, 23), 41, 3.5, fc='#10202a', ec=BELT, lw=1.2))
ax.text(135, 24.75, '5  SORTER (diverts each bag to its flight\'s make-up carousel - the tag scan)', color=BELT, ha='center', va='center', fontsize=9.5)
for i, cx in enumerate((124, 145)):
    ax.add_patch(FancyBboxPatch((cx - 8, 31), 16, 5, boxstyle='round,pad=0,rounding_size=2.5', fc='none', ec=BELT, lw=2.5))
    ax.text(cx, 33.5, 'MAKE-UP %d' % (i + 1), color=BELT, ha='center', va='center', fontsize=10.0)
ax.text(135, 29.2, '6  2 make-up carousels: the crew lift the bags onto the carts by flight', color=BELT, ha='center', fontsize=10.0)
# the tractor lane: carts round the carousels, out through a new roller door into the baggage hall
ax.add_patch(Rectangle((114.5, 38), 41, 13.5, fc='#0f1f10', ec=ROAD, lw=1.2, ls='--'))
ax.text(135, 44.5, '7  CART LANE: tractor trains pull up beside the carousels, load, drive out', color=ROAD, ha='center', fontsize=10.0)
ax.add_patch(Rectangle((128, 51.2), 8, 1.6, fc=ROAD, ec='none'))
ax.text(132, 53.6, 'new 8 m roller door', color=ROAD, ha='center', fontsize=9.0)
# 8. through the baggage hall to the apron: ramp services 4 becomes a vehicle passage
ax.annotate('', xy=(183, 68), xytext=(132, 53), arrowprops=dict(arrowstyle='->', color=ROAD, lw=2.5))
ax.add_patch(Rectangle((169.05, 68), 28, 14, fill=False, ec=ROAD, lw=2.5))
ax.text(183, 75, '8  RAMP SERVICES 4\n-> vehicle passage\n(4 m roller doors\nboth ends)', color=ROAD, ha='center', va='center', fontsize=9.5)
ax.annotate('to the apron and the stands', xy=(183, 86), xytext=(183, 82.5), color=ROAD, ha='center', fontsize=10.0, arrowprops=dict(arrowstyle='->', color=ROAD, lw=2))
ax.text(140, 59.5, 'the trains cross the baggage hall east of the arrivals conveyor - no belts crossed', color=ROAD, fontsize=9.5)

ax.set_xlim(X0, X1); ax.set_ylim(Z0, Z1); ax.set_aspect('equal')
for s in ax.spines.values(): s.set_color('#33414f')
ax.tick_params(colors=DIM, labelsize=7)
ax.set_xlabel('plan x (m, along the building)', color=DIM); ax.set_ylabel('plan z (m, airside up)', color=DIM)
ax.set_title('AIRPORT DYNASTY PRO  ·  DEPARTURE BAGGAGE SYSTEM  ·  BLUEPRINT v1  ·  GROUND FLOOR', color=FG, fontsize=16, weight='bold', loc='left')
notes = ('Rules: no belt crosses a room in use - the only part in a public room is the overhead link over the Grand Hall\'s east end (3.5 m up, enclosed, like real terminals); '
         'everything else is in the freed office / academy block (removed: offices 1-4, operations, staff toilets, crew room, staff corridor and passage, academy, staff entrance).\n'
         'Bag path: desk weigh belt -> island collector -> overhead link -> 2 CT screening lines (reject -> manual search) -> sorter -> make-up carousel of its flight -> the crew onto the carts -> tractor -> apron -> belt loader -> hold. '
         'Every bag keeps its tag (passenger + flight) the whole way. Arrivals (claim hall + trunk conveyor) unchanged.')
fig.text(0.012, 0.012, notes, color=FG, fontsize=11.2, wrap=True)
out = os.path.join(ROOT, 'Blueprints', 'Terminal', 'Departure-Baggage-v1.png')
fig.savefig(out, facecolor=BG, bbox_inches='tight')
print(out)
