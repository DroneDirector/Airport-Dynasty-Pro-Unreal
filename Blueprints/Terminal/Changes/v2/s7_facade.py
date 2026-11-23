# Sheet 7: the landside facade BEFORE / AFTER with the double skin, from the game's real roof copies and pillars.
from v2_common import *
import numpy as np
COPIES = [(-245.1, -161.55), (-161.55, -78.0), (-78.0, 24.0), (24.0, 126.0), (126.0, 233.3), (233.3, 340.6), (340.6, 447.9), (447.9, 555.2)]
PIL = {(-78.0, 24.0): (-46, -8), (24.0, 126.0): (56, 94), (126.0, 233.3): (159.7, 199.6), (233.3, 340.6): (266.96, 306.9), (340.6, 447.9): (374.3, 414.2), (447.9, 555.2): (481.6, 521.5)}
lx = np.array(ROOF['x']); EY, ET, TC = np.array(ROOF['edge_y']), np.array(ROOF['edge_top']), np.array(ROOF['top_crest'])


def edge(x):                  # the roof edge's underside (what the glass is seen up to) at plan x
    for a, b in COPIES:
        if a <= x <= b: return float(np.interp(24 + (x - a) / (b - a) * 102, lx, EY))
    return 12.0


def roofs(ax):
    for a, b in COPIES:
        xs = a + (lx - 24) / 102 * (b - a)
        ax.fill_between(xs, ET, TC, color='#b9c4cf', ec='none', zorder=6); ax.fill_between(xs, EY, ET, color='#f2f6fa', ec='#ffffff', lw=0.8, zorder=7)
        for p in PIL.get((a, b), ()):
            ax.plot([p, p], [0, 4.9], color='#e8eef5', lw=4, zorder=9)
            for dx, dy in ((-4.3, 13.5), (0.2, 12.0), (2.0, 13.0), (4.4, 12.5)): ax.plot([p, p + dx], [4.9, min(dy, edge(p + dx))], color='#e8eef5', lw=2.2, zorder=9)


def glass(ax, a, b, top=None, step=2.5, col='#123448', z=3):
    xs = np.linspace(a, b, max(60, int((b - a) * 3))); t = np.array([edge(x) if top is None else top for x in xs])
    ax.fill_between(xs, 0, t, color=col, zorder=z)
    for x in np.arange(a, b + 0.01, step): ax.plot([x, x], [0, edge(x) if top is None else top], color='#7fb8cc', lw=0.7, zorder=z + 0.1)
    ax.plot([a, b], [5.4, 5.4], color='#7fb8cc', lw=0.7, zorder=z + 0.1)


def front(ax, after):
    ax.add_patch(Rectangle((-241.1, 0), 792.3, 12.0, fc='#161c22', ec='none', zorder=1))          # the pier behind
    ax.add_patch(Rectangle((-80, 5.4), 80, 6.6, fc='#6b4a32', ec='none', zorder=3)); glass(ax, -80, 0, 5.4)
    glass(ax, 0, 42, 12.0, 1.78); glass(ax, 42, 114, None)
    ax.add_patch(Rectangle((114, 0), 42, 12.0, fc='#8f989f', ec='none', zorder=2.5))
    for k in range(10): ax.add_patch(Rectangle((115.5 + k * 4.15, 7.0), 2.1, 2.6, fc='#1b3a4a', ec='#cfe3f0', lw=0.8, zorder=2.8))
    for v0, v1 in ((44, 56), (67, 83), (95, 107)): ax.add_patch(Rectangle((v0, 0), v1 - v0, 3.8, fc='#2a5a72', ec=GLASS, lw=1.2, zorder=5))
    if after:
        glass(ax, 156, 551.2)
        xs = np.linspace(114, 156, 120); t = [edge(x) for x in xs]
        ax.fill_between(xs, 0, t, color='#2a6a88', alpha=0.45, zorder=3.4)
        for x in np.arange(114, 156.01, 2.5): ax.plot([x, x], [0, edge(x)], color=GLASS, lw=1.0, zorder=3.6)
        ax.plot([114, 156], [5.4, 5.4], color=GLASS, lw=1.0, zorder=3.6); ax.add_patch(Rectangle((107, 0), 7, 3.8, fc='#2a5a72', ec=NEW, lw=1.4, zorder=5))
        ax.plot([228, 551], [6, 6], color=NEW, lw=2.2, zorder=4)                                   # the concourse's floor seen through the glass
        for d0, d1 in ENTR_E: ax.add_patch(Rectangle((d0, 0), d1 - d0, 3.8, fc='#2a5a72', ec=NEW, lw=1.4, zorder=5))
    roofs(ax); ax.plot([-250, 562], [0, 0], color='#ffffff', lw=1.5, zorder=10)


fig = sheet('SHEET 7', 'THE FRONT, SEEN FROM THE FORECOURT, BEFORE AND AFTER', 'True scale. The wave roof and the tree pillars are the real ones from the game and do not change at all: only glass and floors are added under them.')
for k, after in enumerate((False, True)):
    ax = ax_plan(fig, [0.008, 0.765 - k * 0.155, 0.984, 0.125], -92, 560, -5.5, 25); front(ax, after)
    fig.text(0.012, 0.893 - k * 0.155, 'AFTER' if after else 'BEFORE (today)', color=NEW if after else FG, fontsize=18, weight='bold', va='bottom')
    if not after: txt(ax, 353, -3.4, 'open grass under the roof, the pier\'s closed wall behind', WARN, 14)
    for a, b, t, c in (((42, 114, 'GRAND HALL', FG), (-80, 0, 'arrivals west', DIM), (0, 42, 'escalators', DIM), (114, 156, 'offices' if not after else 'double skin', DIM if not after else GLASS)) +
                       (() if not after else ((156, 228, 'EAST HALL', NEW), (228, 470, 'promenade, claim + passport behind, concourse above', NEW), (470, 551, 'coach station', NEW)))):
        txt(ax, (a + b) / 2, -3.4, t, c, 13)
for k, (x0, x1, t) in enumerate(((96, 232, 'CLOSE-UP x 96-232: Grand Hall\'s east end, the double skin, East Hall'), (340, 476, 'CLOSE-UP x 340-476: the concourse behind the glass'))):
    for j, after in enumerate((False, True)):
        ax = ax_plan(fig, [0.008 + k * 0.496, 0.375 - j * 0.205, 0.488, 0.19], x0, x1, -4.5, 24); front(ax, after)
        if after: dim(ax, *((114, 156) if k == 0 else (354.6, 356)), -2.6, '42.0 second skin at z -4, inner wall with the office windows behind', fs=12) if k == 0 else txt(ax, 408, -2.8, 'concourse floor (+6.0) seen behind the glass, yellow line', NEW, 13)
        else: txt(ax, (x0 + x1) / 2, -2.8, 'today', DIM, 13)
    fig.text(0.012 + k * 0.496, 0.575, t, color=NEW, fontsize=17, weight='bold')
L = ['CORRECTION to TC-4: it showed the roof as new east of x 126. It already runs to x 555 (four copies of the hall\'s wave, each stretched to 107.3 m, crests every 31.6 m) with eight tree pillars. Its new pillars are dropped.',
     'GLASS: the Grand Hall\'s own wall (z 0, mullions every 2.5 m, transom at 5.4 m) from x 156 to 551, up to the wave: 13.4 to 20.3 m high at z 0. Second skin: the same glass at z -4, x 114-156, 12.5 to 19.3 m high.',
     'NEW DOORS: four vestibules (x 186-198, 290-300, 396-406, 504-516), 4 m deep and 3.8 m high like yours. The east end (x 551) gets the same glass as an end wall, under the roof\'s last wave (it ends at x 555.2).']
for i, l in enumerate(L): fig.text(0.014, 0.125 - i * 0.032, l, color=WARN if i == 0 else FG, fontsize=15)
save(fig, 'TCv2-7-Facade.png')
