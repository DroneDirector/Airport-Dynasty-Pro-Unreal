# Blueprint v1: check-in -> security without the belts or the desks in the way (owner 2026-11-21: "they have to be able
# to go through security ... the belt can't be in their way, and neither can the check-in area"). The two check-in islands
# (moved 3.5 m east for the glass wall) each lose 2 desks opposite the security doors (x 62, x 78): 4 m walk-throughs; the
# desks' belts drop their bags through a hatch behind each desk onto a collector in a service tunnel under the Grand Hall,
# which runs east and rises inside the departure baggage block - nothing on the floor or overhead in the hall.
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BG, FG, DIM = '#0b1118', '#e8eef5', '#8a99a8'
BELT, TUN, WALK = '#3fd0ff', '#c58cff', '#ffd166'
fig, ax = plt.subplots(figsize=(24, 12), dpi=120); fig.patch.set_facecolor(BG); ax.set_facecolor(BG)
def room(x0, z0, x1, z1, name, fc='#17212c', ec='#5a6a7a', col=DIM, fs=10):
    ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc=fc, ec=ec, lw=1.3)); ax.text((x0 + x1) / 2, (z0 + z1) / 2, name, color=col, ha='center', va='center', fontsize=fs)
room(42, 0, 114, 30, '', '#141c24')
ax.text(78, 28.2, 'GRAND HALL (landside)', color=DIM, ha='center', fontsize=11)
room(50, 30, 90, 42, 'SECURITY (6 lanes)', '#1d2a1d', '#7be07b', '#7be07b', 11)
room(50, 42, 90, 52, 'airside lobby -> Floor 2', '#10261a', '#5a8a62', DIM)
room(43, 30, 50, 44, 'WC', fc='#17212c'); room(90, 30, 114, 52, 'Lounge (airside)', '#10261a')
room(114, 0, 156, 52, 'DEPARTURE\nBAGGAGE BLOCK', '#2a2210', '#f5b335', '#f5b335', 11)
ax.plot([42, 42], [0, 30], color='#9fe8ff', lw=5); ax.text(41.2, 15, 'glass wall', color='#9fe8ff', rotation=90, ha='right', va='center', fontsize=9)
for x0, w in ((44, 12), (67, 16), (95, 12)): ax.add_patch(Rectangle((x0, -4), w, 4, fc='#17212c', ec='#5a6a7a'))
ax.text(50, -2, 'Entrance West', color=DIM, ha='center', fontsize=9); ax.text(75, -2, 'Main Entrance', color=DIM, ha='center', fontsize=9); ax.text(101, -2, 'Entrance East', color=DIM, ha='center', fontsize=9)
for xd in (62, 78): ax.add_patch(Rectangle((xd - 3, 29.6), 6, 0.8, fc='#7be07b', ec='none'))
# the desks: 24 positions; the 4 opposite the security doors removed (2 per island)
REMOVED = {6, 7, 13, 14}   # (0-based: desks 7-8 and 14-15)
n = 0
for isl, xs in enumerate((47.5, 74.5)):
    for k in range(12):
        x = xs + 1.05 + k * 2.08; idx = isl * 12 + k
        if idx in REMOVED:
            ax.add_patch(Rectangle((x - 0.9, 15.6), 1.8, 1.6, fc='none', ec='#ff6b6b', ls='--')); continue
        n += 1
        ax.add_patch(Rectangle((x - 0.9, 15.6), 1.8, 1.6, fc='#2b3a48', ec='#c9d6e2', lw=1))   # the desk
        ax.add_patch(Rectangle((x - 0.45, 17.3), 0.9, 1.1, fc=BELT, ec='none'))              # its weigh + tag belt
        ax.add_patch(Rectangle((x - 0.35, 18.5), 0.7, 0.6, fc=TUN, ec='none'))               # the drop hatch behind
        ax.text(x, 16.4, str(n), color=FG, ha='center', va='center', fontsize=7)
ax.text(66, 13.7, '20 check-in desks (desks numbered 1-20); each: weigh + tag belt, then a drop hatch behind the desk', color=BELT, ha='center', fontsize=9.5)
# the walk-throughs and the passenger flow
for xd in (62, 78):
    ax.add_patch(Rectangle((xd - 2.1, 15.3), 4.2, 4.2, fc='#2a2410', ec=WALK, lw=1.5))
    ax.text(xd, 21.0, '4 m walk-\nthrough', color=WALK, ha='center', fontsize=8.5)
for path in (((50, 0.5), (56, 9), (62, 14.5), (62, 22), (62, 29.5)), ((75, 0.5), (75, 9), (78, 14.5), (78, 22), (78, 29.5)), ((101, 0.5), (96, 9), (80, 13.5), (78.5, 14.8))):
    for a, b in zip(path[:-1], path[1:]): ax.add_patch(FancyArrowPatch(a, b, arrowstyle='-|>', mutation_scale=16, color=WALK, lw=2.2))
ax.text(56, 6.2, 'queues on the\nentrance side', color=WALK, ha='center', fontsize=8.5)
# the service tunnel under the hall (dashed): along the islands' back, east to the block, up inside it
ax.plot([48, 99.5], [19.6, 19.6], color=TUN, lw=4, ls=(0, (5, 2)))
ax.plot([99.5, 116], [19.6, 19.6], color=TUN, lw=4, ls=(0, (5, 2)))
ax.add_patch(Rectangle((114.5, 17.8), 4, 3.6, fc='#241a2e', ec=TUN)); ax.text(116.5, 23.0, 'rises inside\nthe block', color=TUN, ha='center', fontsize=8.5)
ax.text(82, 23.8, 'SERVICE TUNNEL ~2 m under the hall floor: the collector belt from every desk\'s hatch to the baggage block (bags on belts all the way)', color=TUN, ha='center', fontsize=9.5)
ax.text(78, 25.7, 'nothing on the floor or overhead between check-in and security', color=FG, ha='center', fontsize=9.5, weight='bold')
ax.set_xlim(38, 160); ax.set_ylim(-6, 54); ax.set_aspect('equal')
for s in ax.spines.values(): s.set_color('#33414f')
ax.tick_params(colors=DIM, labelsize=7); ax.set_xlabel('plan x (m)', color=DIM); ax.set_ylabel('plan z (m, airside up)', color=DIM)
ax.set_title('AIRPORT DYNASTY PRO  ·  CHECK-IN -> SECURITY, BELTS UNDER THE FLOOR  ·  BLUEPRINT v1', color=FG, fontsize=15, weight='bold', loc='left')
fig.text(0.012, 0.012, 'The two islands lose 2 desks each opposite the security doors (x 62, x 78): two 4 m walk-throughs. Every remaining desk keeps its weigh + tag belt; the bag goes through a hatch behind the desk onto '
         'the collector in a service tunnel under the hall, which rises inside the departure baggage block (replaces the overhead link). Security unchanged: its doors line up with the walk-throughs; then the airside lobby -> Floor 2.', color=FG, fontsize=9.5, wrap=True)
out = os.path.join(ROOT, 'Blueprints', 'Terminal', 'Checkin-Security-Flow-v1.png'); fig.savefig(out, facecolor=BG, bbox_inches='tight'); print(out)
