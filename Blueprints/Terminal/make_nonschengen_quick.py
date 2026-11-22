# Quick explainer (owner 2026-11-22): the non-Schengen concourse - a cut through the building + a top view.
import os, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon
BG, FG, DIM, NEW, NS, OK = '#05080c', '#e8eef5', '#8a99a8', '#ffd166', '#ff6b9a', '#3ddc6a'
fig, (a1, a2) = plt.subplots(2, 1, figsize=(20, 15), dpi=100, gridspec_kw={'height_ratios': [1, 1.05]}); fig.patch.set_facecolor(BG)
def R(ax, x0, y0, x1, y1, t='', fc='#141c24', ec='#5a6a7a', col=DIM, fs=10, lw=1.3, ls='-'):
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=fc, ec=ec, lw=lw, ls=ls))
    if t: ax.text((x0 + x1) / 2, (y0 + y1) / 2, t, color=col, ha='center', va='center', fontsize=fs)
# 1. THE CUT (as if sliced from the forecourt to the aircraft, at gate 14)
ax = a1; ax.set_facecolor(BG)
ax.plot([-30, 120], [0, 0], color='#556', lw=2)
R(ax, -30, -0.4, 0, 0, fc='#333a44', ec='#333a44'); ax.text(-15, 1.5, 'FORECOURT\n(cars, taxis)', color=DIM, ha='center', fontsize=10)
R(ax, 82, -0.4, 120, 0, fc='#3a3f46', ec='#3a3f46'); ax.text(101, 1.2, 'APRON', color=DIM, ha='center', fontsize=10)
# ground floor
R(ax, 0, 0, 52, 6.5, 'GROUND FLOOR (new):  passport control IN  ·  non-Schengen claim hall  ·  customs', fc='#18222c', fs=10)
R(ax, 52, 0, 68, 6.5, 'baggage\nhall', fc='#161d24', fs=9)
# floor 2
R(ax, 0, 6.5, 20, 13, 'arrivals\ncorridor +\nmezzanine', fc='#2a1020', ec=NS, col=NS, fs=9)
R(ax, 20, 6.5, 52, 13, 'NEW NON-SCHENGEN CONCOURSE\nduty free · food · bar · toilets · lounge · seats', fc='#2a2410', ec=NEW, col=NEW, fs=11, lw=2.5)
R(ax, 52, 6.5, 68, 13, 'walkway\n(existing)', fc='#101820', fs=9)
R(ax, 68, 6.5, 82, 13, 'gate 14\n(existing)', fc='#10261a', ec=OK, col=OK, fs=9)
# roof (wave) + glass front
ax.add_patch(Polygon([(0, 13), (0, 22), (25, 24), (52, 21), (68, 18), (82, 16), (82, 13)], fc='#0d141b', ec='#7fd4ff', lw=1.5))
ax.text(30, 19.5, 'the continued curved roof', color='#7fd4ff', ha='center', fontsize=10)
ax.plot([0, 0], [0, 22], color='#9fe8ff', lw=4); ax.text(-1, 15, 'glass\nfront', color='#9fe8ff', ha='right', fontsize=9)
# jetway + aircraft
ax.plot([82, 96], [9.5, 6.5], color=OK, lw=5); ax.text(90, 9.6, 'jetway', color=OK, fontsize=9)
ax.add_patch(Polygon([(96, 2.5), (118, 2.5), (118, 9), (96, 9)], fc='#1e2a36', ec='#cfd8e3')); ax.text(107, 5.7, 'aircraft', color='#cfd8e3', ha='center', fontsize=10)
ax.annotate('', xy=(36, 10), xytext=(60, 10), arrowprops=dict(arrowstyle='->', color=NEW, lw=2.5))
ax.text(48, 13.8, 'from the walkway you step\nsideways into the concourse', color=NEW, ha='center', fontsize=9)
ax.set_xlim(-32, 121); ax.set_ylim(-2, 26); ax.set_aspect(1.6); ax.axis('off')
ax.set_title('1 · A CUT THROUGH THE BUILDING (forecourt on the left, aircraft on the right)', color=FG, fontsize=14, loc='left', weight='bold')
# 2. TOP VIEW OF FLOOR 2 EAST OF THE PASSPORT WALL
ax = a2; ax.set_facecolor(BG)
R(ax, 150, 52, 551, 68, '', fc='#101820'); ax.text(195, 60, 'walkway (Schengen)', color=DIM, ha='center', fontsize=10)
R(ax, 236, 52, 248, 68, '', fc=NS, ec=NS); ax.text(242, 70.5, 'PASSPORT\nCONTROL OUT', color=NS, ha='center', va='bottom', fontsize=9, weight='bold')
ax.text(400, 60, 'walkway (non-Schengen)', color=NS, ha='center', fontsize=10)
for g, x in ((13, 272.4), (14, 352.2), (15, 424.5), (16, 496.8)):
    R(ax, x - 17, 68, x + 17, 82, 'gate %d' % g, fc='#10261a', ec=OK, col=OK, fs=9)
R(ax, 250, 20, 551, 52, '', fc='#2a2410', ec=NEW, lw=2.5)
for x0, x1, t in ((252, 312, 'DUTY FREE'), (314, 372, 'FOOD COURT\n+ bar'), (374, 420, 'PREMIUM\nLOUNGE'), (422, 452, 'TOILETS\nfamily room'), (454, 500, 'SEATING\nquiet room'), (502, 549, 'TRANSFER desk\n+ security')):
    R(ax, x0, 24, x1, 48, t, fc='#3a3014', ec=NEW, col=NEW, fs=9)
ax.text(400, 16, 'NEW NON-SCHENGEN CONCOURSE (Floor 2, above the new halls)', color=NEW, ha='center', fontsize=12, weight='bold')
R(ax, 150, 20, 236, 52, 'Floor 2 here: open to the\nhall below (East Hall roof)', fc='#141c24', fs=9)
ax.plot([150, 551], [0, 0], color='#9fe8ff', lw=4); ax.text(350, -4, 'glass front onto the forecourt', color='#9fe8ff', ha='center', fontsize=10)
ax.set_xlim(145, 556); ax.set_ylim(-8, 86); ax.set_aspect(1.9); ax.axis('off')
ax.set_title('2 · FROM ABOVE: Floor 2 past the passport wall', color=FG, fontsize=14, loc='left', weight='bold')
fig.text(0.01, 0.01, 'Quick explainer, not to exact scale. Gates, jetways and stands stay as they are; the new concourse sits under the continued roof, above the new ground-floor halls.', color=DIM, fontsize=10)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'NonSchengen-Quick.png'); fig.savefig(out, facecolor=BG, bbox_inches='tight'); print(out)
