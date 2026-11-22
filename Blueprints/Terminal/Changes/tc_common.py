# To-do 213 (owner 2026-11-22): the terminal changes for real departures and arrivals - shared numbers, colours and
# the BEFORE / AFTER geometry for every sheet (plan metres: x along the building, z = 0 the landside face, z grows airside).
import os, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch, Polygon
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Terminal-Changes')
os.makedirs(OUT, exist_ok=True)
BG, FG, DIM, GRID = '#05080c', '#e8eef5', '#8a99a8', '#33414f'
NEW, NS, OK, BELT, TUN, WARN, GLASS = '#ffd166', '#ff6b9a', '#3ddc6a', '#3fd0ff', '#c58cff', '#ff8a5b', '#9fe8ff'
W, H, DPI = 32, 18, 120          # 3840 x 2160 (4K)


def sheet(title, sub=''):
    fig = plt.figure(figsize=(W, H), dpi=DPI); fig.patch.set_facecolor(BG)
    fig.text(0.012, 0.972, 'AIRPORT DYNASTY PRO  ·  TERMINAL CHANGES FOR REAL DEPARTURES + ARRIVALS  ·  ' + title,
             color=FG, fontsize=22, weight='bold', va='top')
    if sub: fig.text(0.012, 0.945, sub, color=DIM, fontsize=13, va='top')
    fig.text(0.988, 0.008, 'to-do 213 · blueprint for the owner\'s OK · nothing built yet · all sizes in metres',
             color=DIM, fontsize=11, ha='right')
    return fig


def save(fig, name):
    p = os.path.join(OUT, name); fig.savefig(p, facecolor=BG, dpi=DPI); print(p); return p


def box(ax, x0, z0, x1, z1, txt='', fc='#141c24', ec='#5a6a7a', col=DIM, fs=9, lw=1.2, ls='-', rot=0, weight='normal', z=2):
    ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc=fc, ec=ec, lw=lw, ls=ls, zorder=z))
    if txt:
        ax.text((x0 + x1) / 2, (z0 + z1) / 2, txt, color=col, ha='center', va='center', fontsize=fs, rotation=rot,
                weight=weight, zorder=z + 1)


def dim(ax, a, b, at, txt=None, vertical=False, col=NEW, fs=9, off=0.0):
    """a dimension line from a to b along x (or z when vertical) at coordinate `at`"""
    t = txt if txt is not None else '%.1f' % abs(b - a)
    if vertical:
        ax.annotate('', xy=(at, a), xytext=(at, b), arrowprops=dict(arrowstyle='<->', color=col, lw=1.0))
        ax.text(at + off, (a + b) / 2, t, color=col, fontsize=fs, rotation=90, ha='center', va='center',
                bbox=dict(fc=BG, ec='none', pad=0.5))
    else:
        ax.annotate('', xy=(a, at), xytext=(b, at), arrowprops=dict(arrowstyle='<->', color=col, lw=1.0))
        ax.text((a + b) / 2, at + off, t, color=col, fontsize=fs, ha='center', va='center',
                bbox=dict(fc=BG, ec='none', pad=0.5))


def arrow(ax, pts, col=NEW, lw=2.0, ms=14):
    for a, b in zip(pts[:-1], pts[1:]):
        ax.add_patch(FancyArrowPatch(a, b, arrowstyle='-|>', mutation_scale=ms, color=col, lw=lw, zorder=6))


# ---------------------------------------------------------------- the design hour (from the stands)
NB_STANDS, WB_STANDS = 12, 4
A320_SEATS, A380_SEATS, B777_SEATS, LF = 180, 520, 368, 0.85
WB_AVG = (A380_SEATS + 3 * B777_SEATS) / 4.0
DEP_NB = NB_STANDS * 0.67 * A320_SEATS * LF       # each narrow-body stand turns every ~90 min
DEP_WB = WB_STANDS * 0.33 * WB_AVG * LF           # each wide-body stand every ~3 h
PH_DEP = DEP_NB + DEP_WB                           # design peak-hour departures (= arrivals)
SURGE_NS = A380_SEATS * LF + B777_SEATS * LF       # an A380 + a B777 arriving within 30 min

# ---------------------------------------------------------------- the wave roof and the facade (from the game)
# The Grand Hall's wave roof: x 24..126, z -16..34, three crests over the hall (x 45, 75, 105), lowest point 12.3 m;
# the arrivals wing has an exact copy at -102 (x -78..24). Tree pillars C-7/C-8 at x 56 / 94, z -9 (copies at -46/-8).
ROOF_SPAN = 102.0
ROOF_COPIES_BEFORE = [24.0, -78.0]
ROOF_COPIES_AFTER = [24.0, -78.0, 126.0, 228.0, 330.0, 432.0]
PILLAR_OFFS = (32.0, 70.0)


def roof_under(xs, x_start):
    import numpy as np
    u = np.asarray(xs) - (x_start + 51.0)
    return 16.2 + 3.9 * np.cos(2 * np.pi * u / 30.0)   # the underside (the glass runs up to it)


def roof_top(xs, x_start):
    return roof_under(xs, x_start) + 2.4
