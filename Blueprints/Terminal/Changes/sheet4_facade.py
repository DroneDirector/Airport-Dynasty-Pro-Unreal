# Sheet 4: the landside facade (seen from the forecourt, looking north/airside) BEFORE and AFTER - the Grand Hall's tall
# glass facade and tree pillars repeated under copies of the same wave roof along the whole east wing, so it reads as one building.
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from tc_common import *
fig = sheet('SHEET 4 · LANDSIDE FACADE BEFORE / AFTER',
            'Elevation from the forecourt, true scale (no stretch). The Grand Hall\'s wave roof (x 24-126) is continued east with its exact section and '
            'rhythm (crests every 30 m) to the end of the east wing; under every wave the same full-height glass, mullions every 2.5 m, and the same tree pillars.')
ROOFC, CEDAR, GL, MUL = '#3a5a7a', '#6b4a32', '#16384c', '#9fc8dc'


def wave(ax, xs0, span=ROOF_SPAN, phase=None):
    xs = np.linspace(xs0, xs0 + span, int(span * 4))
    ph = xs0 if phase is None else phase
    ax.fill_between(xs, roof_under(xs, ph), roof_top(xs, ph), color=ROOFC, ec='#cfe3f0', lw=1.0, zorder=6)
    return
    xs = np.linspace(xs0, xs0 + ROOF_SPAN, 300)
    ax.fill_between(xs, roof_under(xs, xs0), roof_top(xs, xs0), color=ROOFC, ec='#cfe3f0', lw=1.0, zorder=6)


def glass(ax, a, b, xs0, top=None, step=2.5):
    """full-height glass from the floor to the wave's underside (or to `top`), mullions every 2.5 m, transom at 5.4"""
    xs = np.linspace(a, b, 200)
    t = roof_under(xs, xs0) if top is None else np.full_like(xs, top)
    ax.fill_between(xs, 0, t, color=GL, alpha=0.95, zorder=3)
    for x in np.arange(a, b + 0.01, step):
        h = roof_under([x], xs0)[0] if top is None else top
        ax.plot([x, x], [0, h], color=MUL, lw=0.55, zorder=4)
    ax.plot(xs, np.minimum(t, 5.4), color=MUL, lw=0.55, zorder=4)


def pillar(ax, x, xs0):
    top = roof_under([x], xs0)[0]
    ax.plot([x, x], [0, 8], color='#e8eef5', lw=3.0, zorder=7)
    for dx in (-4.5, -1.5, 1.5, 4.5):
        ax.plot([x, x + dx], [8, roof_under([x + dx], xs0)[0]], color='#e8eef5', lw=1.6, zorder=7)


def solid(ax, a, b, h, c='#2a3440', t=''):
    ax.add_patch(Rectangle((a, 0), b - a, h, fc=c, ec='#5a6a7a', lw=0.8, zorder=2))
    if t: ax.text((a + b) / 2, h / 2, t, color=DIM, fontsize=8, ha='center', va='center', zorder=8)


def facade(ax, after):
    ax.add_patch(Rectangle((-241, 0), 792, 12, fc='#1c242c', ec='#3a4652', lw=0.6, zorder=1))     # the pier behind (gates)
    solid(ax, -80, 0, 5.4, GL); [ax.plot([x, x], [0, 5.4], color=MUL, lw=0.5, zorder=4) for x in np.arange(-80, 0.1, 2.5)]
    ax.add_patch(Rectangle((-80, 5.4), 80, 6.6, fc=CEDAR, ec='none', zorder=3))
    ax.text(-40, 2.5, 'arrivals wing', color=FG, fontsize=8, ha='center', zorder=8)
    solid(ax, 0, 42, 12, '#24303c', 'escalator room')
    glass(ax, 42, 114, 24.0); ax.text(78, 2.5, 'GRAND HALL (glass to the roof)', color=FG, fontsize=9, ha='center', zorder=8, weight='bold')
    for v0, v1 in ((44, 56), (67, 83), (95, 107)): ax.add_patch(Rectangle((v0, 0), v1 - v0, 3.8, fc='#2a4a60', ec=MUL, lw=0.8, zorder=5))
    if not after:
        solid(ax, 114, 156, 12, '#24303c', 'offices / bag block'); solid(ax, 156, 551.2, 12, '#20262e', 'east wing - closed wall, flat roof at 12 m')
        for xs0 in ROOF_COPIES_BEFORE: wave(ax, xs0)
        for xs0 in ROOF_COPIES_BEFORE: [pillar(ax, xs0 + o, xs0) for o in PILLAR_OFFS]
        return
    # AFTER: the Grand Hall's wave CONTINUED east (the same section and the same 30 m rhythm, no seam, no kink) to x 551,
    # glass to the roof everywhere under it (the link gallery, the East Hall, the promenade front, the coach station)
    glass(ax, 107, 551.2, 24.0)
    for xs0 in ROOF_COPIES_BEFORE: wave(ax, xs0)
    wave(ax, 126.0, span=425.2, phase=24.0)
    for xs0 in ROOF_COPIES_BEFORE: [pillar(ax, xs0 + o, xs0) for o in PILLAR_OFFS]
    for i, tr in enumerate(range(150, 551, 60)):
        pillar(ax, tr + (-4 if i % 2 == 0 else 4), 24.0)
    ax.add_patch(Rectangle((550.8, 0), 0.6, roof_top([551.2], 24.0)[0], fc='#cfe3f0', zorder=7))   # the end cap
    for (a, b, t) in ((107, 156, 'link gallery'), (156, 228, 'EAST HALL'), (228, 470, 'landside promenade (glass front, full height)'),
                      (470, 551.2, 'COACH + BUS STATION')):
        ax.text((a + b) / 2, 2.5, t, color=NEW, fontsize=9, ha='center', zorder=8, weight='bold')
    for d0, d1 in ((185, 197), (290, 300), (395, 405), (500, 512)):
        ax.add_patch(Rectangle((d0, 0), d1 - d0, 3.8, fc='#2a4a60', ec=MUL, lw=0.8, zorder=5))   # new entrance vestibules (revolving + sliding)


for k, after in enumerate((False, True)):
    ax = fig.add_axes([0.012, 0.66 - k * 0.20, 0.976, 0.17]); ax.set_facecolor(BG)
    facade(ax, after)
    ax.set_xlim(-92, 562); ax.set_ylim(-6, 26); ax.set_aspect('equal'); ax.axis('off')
    ax.plot([-92, 562], [0, 0], color=FG, lw=1.2)
    fig.text(0.012, 0.84 - k * 0.20, 'AFTER (the proposal)' if after else 'BEFORE (today)', color=NEW if after else FG, fontsize=17, weight='bold')
    if after:
        dim(ax, 126, 551.2, -3.5, '425.2 the same wave continued (crests every 30 m, as over the Grand Hall)', fs=8)
    dim(ax, 24, 126, -3.5, '102.0 Grand Hall wave', fs=8); dim(ax, -78, 24, -3.5, '102.0 arrivals wave', fs=8)
# the close-up, true scale: x 100-240 before | after
for k, after in enumerate((False, True)):
    ax = fig.add_axes([0.012 + k * 0.494, 0.07, 0.484, 0.33]); ax.set_facecolor(BG)
    facade(ax, after)
    ax.set_xlim(100, 236); ax.set_ylim(-5, 25); ax.set_aspect('equal'); ax.axis('off')
    for t in ax.texts: t.set_clip_on(True)
    ax.plot([100, 236], [0, 0], color=FG, lw=1.2)
    fig.text(0.012 + k * 0.494, 0.41, ('AFTER' if after else 'BEFORE') + '  close-up x 100-236 (Grand Hall east end, link gallery, East Hall)',
             color=NEW if after else FG, fontsize=14, weight='bold')
    dim(ax, 0, 20.1, 237 if False else 101.5, '20.1 crest', vertical=True, fs=8) if False else None
    for h, t in ((12.3, '12.3 wave low point'), (22.5, '22.5 crest top'), (5.4, '5.4 transom')):
        ax.plot([100, 236], [h, h], color=GRID, lw=0.6, ls=':', zorder=0); ax.text(100.5, h + 0.4, t, color=DIM, fontsize=8, zorder=9)
    if after:
        dim(ax, 156, 228, -2.5, '72.0 East Hall: glass from the floor to the wave, like the Grand Hall', fs=8.5)
        ax.text(140, 23.6, 'tree pillars at x 146 / 214 (the Grand Hall’s C-7 / C-8 design); the old roof’s east lip (x 120-126) runs on into the wave', color=NEW, fontsize=8.5)
fig.text(0.012, 0.025, 'Roof: the Grand Hall’s wave is continued, not pasted: the same section and the same 30 m crest rhythm from x 126 to the east '
         'end (x 551), so there is no seam or kink anywhere east of the hall; it ends in a clean end cap. Glass: the Grand Hall’s - floor to the wave’s '
         'underside, mullions every 2.5 m, a transom at 5.4 m - along the link gallery, the East Hall, the promenade and the coach station. Pillars: '
         'the same tree pillars on the kerb line (z -9), one in every second trough (x 146, 214, 266, 334, 386, 454, 506). The west side is unchanged.',
         color=FG, fontsize=11.5, wrap=True)
save(fig, 'TC-4-Facade-Before-After.png')
