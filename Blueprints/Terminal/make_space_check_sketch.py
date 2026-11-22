# Quick sketch (owner 2026-11-22): where the missing departure/arrival pieces would go, and whether the gates, stands
# and jetways change. Not a blueprint - a first idea for his OK.
import os, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
BG, FG, DIM = '#05080c', '#e8eef5', '#8a99a8'
NEW, NS, OK = '#ffd166', '#ff6b9a', '#3ddc6a'
fig, ax = plt.subplots(figsize=(26, 14), dpi=110); fig.patch.set_facecolor(BG); ax.set_facecolor(BG)


def box(x0, z0, x1, z1, txt='', fc='#141c24', ec='#5a6a7a', col=DIM, fs=9, lw=1.2, ls='-'):
    ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc=fc, ec=ec, lw=lw, ls=ls))
    if txt:
        ax.text((x0 + x1) / 2, (z0 + z1) / 2, txt, color=col, ha='center', va='center', fontsize=fs)


# the building as it is (grey)
box(-241, 0, 551, 52)
box(-241, 52, 551, 68, 'Floor 2 gate walkway (unchanged)', fc='#101820')
box(42, 0, 114, 30, fc='#18222c'); ax.text(78, 22, 'GRAND HALL', color=DIM, ha='center', fontsize=10)
box(50, 30, 90, 42, fc='#1a241a', ec='#5a8a62'); ax.text(70, 39.5, 'SECURITY', color=DIM, ha='center', fontsize=9)
box(-80, 0, 0, 52, fc='#18222c'); ax.text(-40, 40, 'BAGGAGE CLAIM WING', color=DIM, ha='center', fontsize=10)
# gates, stands and jetways: unchanged (green)
gates = {1: -55.6, 2: 25, 3: 105.5, 4: 186, 9: -216.7, 11: -136.2, 13: 272.4, 14: 352.2, 15: 424.5, 16: 496.8}
for g, x in gates.items():
    box(x - 17, 68, x + 17, 82, 'G%d' % g, fc='#10261a', ec=OK, col=OK, fs=9)
    ax.plot([x, x], [82, 90], color=OK, lw=3)
    ax.text(x, 92, 'jetway + stand\nunchanged', color=OK, ha='center', va='bottom', fontsize=7)
# NEW pieces (yellow)
box(46, 2, 76, 10, 'kiosks + bag drop', fc='#2a2410', ec=NEW, col=NEW, fs=8.5)
box(80, 2, 110, 10, 'kiosks + bag drop', fc='#2a2410', ec=NEW, col=NEW, fs=8.5)
box(42.5, 12, 47, 28, 'service\ndesk', fc='#2a2410', ec=NEW, col=NEW, fs=7.5)
for xd in (62, 78):
    box(xd - 3, 28.8, xd + 3, 31.2, fc=NEW, ec=NEW)
ax.text(70, 33.5, 'boarding-pass gates + trays', color=NEW, ha='center', fontsize=8.5)
box(-78, 2, -62, 12, 'lost &\nfound', fc='#2a2410', ec=NEW, col=NEW, fs=8)
box(-60, 2, -46, 12, 'oversize\nbelt', fc='#2a2410', ec=NEW, col=NEW, fs=8)
box(-20, 2, -2, 12, 'customs\ngreen / red', fc='#2a2410', ec=NEW, col=NEW, fs=8)
box(-80, -22, 0, 0, 'NEW ARRIVALS HALL (landside extension)\nmeeters & greeters, name signs, cafe, car rental, taxi / bus exit',
    fc='#2a2410', ec=NEW, col=NEW, fs=9, lw=2)
# non-Schengen (pink): the east pier, gates 13-16 (the big stands, long-haul)
box(230, 52, 551, 68, fc='none', ec=NS, lw=2.5, ls='--')
ax.text(390, 60, 'NON-SCHENGEN AREA  -  gates 13-16 (big stands, long-haul)', color=NS, ha='center', fontsize=10, weight='bold')
box(222, 52, 230, 68, fc=NS, ec=NS)
ax.text(226, 49, 'passport control OUT (booths + e-gates), glass wall across the walkway', color=NS, ha='center', va='top', fontsize=8.5)
box(250, 4, 340, 22, 'passport control IN (booths + e-gates),\nthen down to the claim wing', fc='#2a1020', ec=NS, col=NS, fs=8.5)
ax.annotate('', xy=(0, 13), xytext=(250, 13), arrowprops=dict(arrowstyle='->', color=NS, lw=1.5))
ax.text(125, 15, 'arrivals from gates 13-16', color=NS, ha='center', fontsize=8)
ax.set_xlim(-250, 560); ax.set_ylim(-26, 100); ax.set_aspect(3.2); ax.axis('off')
ax.set_title('QUICK SKETCH  -  where the missing pieces go  -  yellow = new   pink = non-Schengen   green = unchanged (all gates, stands, jetways)',
             color=FG, fontsize=13, loc='left', weight='bold')
fig.text(0.01, 0.02, 'Plan view, airside up (heights stretched to read). Everything new is inside the building or on the landside: no gate, stand '
         'or jetway moves. The only change on Floor 2 is a glass wall with passport control across the walkway before gates 13-16.',
         color=DIM, fontsize=10)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Space-Check-Sketch.png')
fig.savefig(out, facecolor=BG, bbox_inches='tight'); print(out)
