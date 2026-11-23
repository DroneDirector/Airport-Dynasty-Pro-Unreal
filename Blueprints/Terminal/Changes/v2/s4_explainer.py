# Sheet 4: the explainer in plain words - a cut through the building at x 350 (the real wave roof, true scale), before and
# after, and a simple top view of Floor 2 past the passport wall.
from v2_common import *
fig = sheet('SHEET 4', 'THE NON-SCHENGEN CONCOURSE, EXPLAINED', 'A cut through the building at x 350 (next to gate 14), as if you sliced it and looked east. Forecourt left, aircraft right. True scale, real heights.')
cut = ROOF['cut350']; zs = [c[0] for c in cut if c[1] is not None]; so = [c[1] for c in cut if c[1] is not None]; to = [c[2] for c in cut if c[2] is not None]
zt = [c[0] for c in cut if c[2] is not None]


def person(ax, z, y, c='#ffffff'):
    ax.add_patch(Circle((z, y + 1.6), 0.17, fc=c, ec='none', zorder=40)); ax.plot([z, z], [y + 0.1, y + 1.4], color=c, lw=3, zorder=40)


def section(ax, after):
    ax.plot([-30, 125], [0, 0], color='#8896a4', lw=2, zorder=3)
    ax.fill_between(zt, [c for c in so], to, color='#cfd8e0', ec='#ffffff', lw=1, zorder=8)
    # the tree pillar in front (z -9), the walkway, its vault and gate 14 (existing)
    ax.plot([-9, -9], [0, 4.9], color='#e8eef5', lw=8, zorder=7); ax.plot([-9, -4.6], [4.9, 12.9], color='#e8eef5', lw=4, zorder=7); ax.plot([-9, -8.2], [4.9, 12.2], color='#e8eef5', lw=4, zorder=7)
    box(ax, 52, 0, 68, 5.63, '', fc='#1a170e', ec='#c9d6e2'); box(ax, 52, 5.63, 68, 6, '', fc='#8896a4', ec='none', z=4)
    box(ax, 52, 6, 68, 11.4, '', fc='#141c24', ec='#c9d6e2'); box(ax, 52, 11.4, 55, 12.17, '', fc='#8896a4', ec='none', z=4); box(ax, 65, 11.4, 68, 12.17, '', fc='#8896a4', ec='none', z=4)
    import numpy as np
    t = np.linspace(0, np.pi, 40); ax.plot(60 - 5 * np.cos(t), 12.17 + 3.48 * np.sin(t), color=GLASS, lw=3, zorder=6)
    box(ax, 68, 0, 82, 5.6, '', fc='#1a170e', ec='#c9d6e2'); box(ax, 68, 6, 82, 11.4, '', fc='#0e2216', ec=OK); box(ax, 68, 11.4, 82, 12, '', fc='#8896a4', ec='none', z=4); box(ax, 68, 5.6, 82, 6, '', fc='#8896a4', ec='none', z=4)
    ax.plot([82, 98, 111], [7.6, 6.9, 5.5], color=OK, lw=14, zorder=5, solid_capstyle='butt')
    ax.add_patch(Circle((116, 4.4), 3.1, fc='#dfe6ec', ec='#ffffff', zorder=6)); ax.add_patch(Rectangle((109, -0.0), 14, 0.15, fc='#8896a4'))


ax = ax_plan(fig, [0.012, 0.735, 0.976, 0.185], -26, 122, -1.5, 20); section(ax, False)
fig.text(0.012, 0.918, 'TODAY', color=FG, fontsize=20, weight='bold', va='top')
txt(ax, 25, 5.5, 'GRASS, open air', WARN, 18, weight='bold'); txt(ax, 25, 2.6, 'nothing under the roof here', DIM, 15); txt(ax, 43, 13.5, 'no roof here\n(z 34-52)', WARN, 14)
txt(ax, 60, 2.8, 'baggage hall', FG, 13); txt(ax, 60, 8.7, 'walkway', FG, 14); txt(ax, 75, 2.8, 'ramp rooms', FG, 13); txt(ax, 75, 8.7, 'gate 14', OK, 14); txt(ax, 97, 10, 'jetway', OK, 14); txt(ax, 116, 9, 'aircraft', FG, 14)
txt(ax, -18, 1.4, 'forecourt', DIM, 14); txt(ax, 10, 18.4, 'wave roof (already there)', '#ffffff', 14)
ax = ax_plan(fig, [0.012, 0.465, 0.976, 0.245], -26, 122, -3.8, 20.5); section(ax, True)
fig.text(0.012, 0.715, 'AFTER', color=NEW, fontsize=20, weight='bold', va='top')
box(ax, 0, 0, 6, 5.5, '', fc='#2a2410', ec=NEW, z=3); box(ax, 6, 0, 52, 5.5, '', fc='#2a0f1c', ec=NS, z=3); box(ax, 20, 5.5, 52, 6, '', fc=NEW, ec=NEW, z=5)
pz = [(z, y) for z, y in zip(zs, so) if 20 <= z <= 34]; ax.add_patch(Polygon([(20, 6)] + pz + [(34, 6)], fc='#2b2510', ec='none', zorder=3)); box(ax, 34, 6, 52, 11.6, '', fc='#2b2510', ec='none', z=3); box(ax, 34, 11.6, 52, 12, '', fc=NEW, ec=NEW, z=5)
ax.plot([34, 34], [12, 13.4], color=GLASS, lw=7, zorder=7); ax.plot([0, 0], [0, 13.9], color=GLASS, lw=8, zorder=7); ax.plot([20, 20], [6, 7.1], color=GLASS, lw=5, zorder=7)
box(ax, 63.4, 0, 67.9, 3.2, '', fc='#45152c', ec=NS, z=6, lw=2.5); ax.plot([52, 52], [6.05, 9.6], color='#2b2510', lw=6, zorder=6)
for z, y, c in ((3, 0, '#ffffff'), (14, 0, NS), (40, 0, NS), (27, 6, NEW), (38, 6, NEW), (46, 6, NEW), (57, 6, NEW), (62, 6, NEW), (65.6, 0, NS), (73, 6, NEW), (-14, 0, '#ffffff')): person(ax, z, y, c)
txt(ax, 35.5, 8.6, 'CONCOURSE: duty free, food, bar, toilets, lounge, seats', NEW, 16, weight='bold', bbox=dict(fc='#2b2510', ec='none', pad=2)) if False else None
txt(ax, 36, 9.9, 'NEW CONCOURSE (Floor 2)', NEW, 18, weight='bold'); txt(ax, 27.5, 3.4, 'PASSPORT CONTROL IN  (ground floor)', NS, 16, weight='bold')
txt(ax, 57.8, 9.6, 'walkway', FG, 14); txt(ax, 57.6, 3, 'baggage\nhall', FG, 13); txt(ax, 75, 9.6, 'gate 14', OK, 15); txt(ax, 97, 10, 'jetway', OK, 14); txt(ax, 116, 9, 'aircraft', FG, 14)
txt(ax, 75, 4.2, 'ramp room', DIM, 12); txt(ax, 3, 4.3, 'front\nwalk', NEW, 12); txt(ax, 10.5, 10, 'open up to\nthe roof', NEW, 13); txt(ax, -18, 4, 'forecourt', DIM, 14); txt(ax, 10, 18.4, 'wave roof (unchanged)', '#ffffff', 14); txt(ax, 43, 14.2, 'new flat roof, glass band', NEW, 13)
ax.annotate('arrivals corridor\n(closed)', xy=(65.6, 0.6), xytext=(91, 2.2), color=NS, fontsize=14, ha='center', va='center', arrowprops=dict(arrowstyle='->', color=NS, lw=2), zorder=45)
for a, b, at, t in ((0, 20, -1.2, '20 front hall'), (20, 52, -1.2, '32 concourse above'), (52, 68, -1.2, '16 walkway'), (68, 82, -1.2, '14 gate')): dim(ax, a, b, at, t, fs=13)
dim(ax, 6, 15.5, 21.6, '9.5', vertical=True, fs=12); dim(ax, 6, 11.4, 50, '5.4', vertical=True, fs=12); dim(ax, 0, 5.4, 48, '5.4', vertical=True, fs=12); dim(ax, 0, 14, -1.6, '14.0', vertical=True, fs=12);
txt(ax, 46, -3, 'white: anyone   yellow: departing non-Schengen   pink: arriving non-Schengen', DIM, 14)
# ---- the top view
ax = ax_plan(fig, [0.012, 0.135, 0.976, 0.315], 196, 556, 9, 93)
fig.text(0.012, 0.447, 'FROM ABOVE: Floor 2 past the passport wall', color=FG, fontsize=20, weight='bold', va='top')
box(ax, 196, 52, 240, 68, 'walkway\n(Schengen)', fc='#12202e', ec=SCH, fs=15); box(ax, 240, 52, 551, 68, '', fc='#2a0f1c', ec=NS); txt(ax, 400, 60, 'THE SAME WALKWAY, now for non-Schengen flights', NS, 18, weight='bold')
ax.plot([240, 240], [50, 68], color=GLASS, lw=9, zorder=10)
for g in GATE_X0:
    a = GATE_X0[g]; box(ax, a, 68, a + 68, 82, 'gate %d' % g, fc='#0e2216', ec=OK, fs=17)
    for x in JETWAYS[g]: ax.plot([x, x], [82, 89], color=OK, lw=6)
box(ax, 228, 20, 256, 52, 'passport\ncontrol\nOUT', fc='#14283d', ec=SCH, fs=15, weight='bold'); box(ax, 258, 24, 305, 52, 'DUTY FREE\nshops', fc='#33163a', ec=DF, fs=17, weight='bold')
box(ax, 307, 24, 340, 52, 'TRANSFERS', fc='#123230', ec=TRF, fs=15, weight='bold'); box(ax, 342, 24, 398, 52, 'FOOD + BAR', fc='#2a2410', ec=NEW, fs=17, weight='bold'); box(ax, 400, 24, 424, 52, 'toilets,\nfamily,\nprayer', fc='#10222e', ec=BELT, fs=13)
box(ax, 426, 24, 470, 52, 'LOUNGE', fc='#2a2410', ec=NEW, fs=17, weight='bold'); box(ax, 472, 24, 520, 52, 'SEATING', fc='#2a2410', ec=NEW, fs=17, weight='bold'); box(ax, 522, 24, 551, 52, 'shops,\ntoilets', fc='#2a2410', ec=NEW, fs=14)
box(ax, 256, 20, 551, 24, '', fc='#20200f', ec=NEW); txt(ax, 410, 16, 'window gallery: you look down on the promenade and out to the forecourt', NEW, 15)
route(ax, [(205, 56), (236, 56), (236, 45), (262, 45), (280, 45), (280, 60)], '#ffffff', lw=3.5); ax.annotate('', xy=(370, 45), xytext=(370, 58), arrowprops=dict(arrowstyle='<->', color='#ffffff', lw=3.5), zorder=26)
ax.annotate('', xy=(496, 45), xytext=(496, 58), arrowprops=dict(arrowstyle='<->', color='#ffffff', lw=3.5), zorder=26); txt(ax, 213, 36, 'glass wall: no way\nthrough without a\npassport check', GLASS, 14)
L = ['1  You come along the walkway, as today.   2  A glass wall stops everyone; non-Schengen passengers turn right into passport control.   3  Behind it: duty free, then food, lounge, seats.',
     '4  The walkway goes on as before to gates 13-16: step in and out of the concourse wherever you like.   5  Below your feet: passport control IN, the claim hall, staff rooms and the coach station.',
     '6  Passengers who ARRIVE never come here: they go down at their own gate (pink, the closed corridor under the walkway) to passport control in.']
for i, l in enumerate(L): fig.text(0.014, 0.105 - i * 0.03, l, color=FG, fontsize=16)
save(fig, 'TCv2-4-Concourse-Explained.png')
