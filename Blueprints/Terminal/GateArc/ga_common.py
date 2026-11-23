# Gate arc drawings: the sheet, the labels and gate 9 in plan (the game's traced walls, Data/holomap-walls.json).
import json, os, sys, math
import numpy as np
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, Circle, Wedge
from matplotlib.collections import LineCollection
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..')); sys.path.insert(0, HERE)
from ga_geom import *
OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Gate-Arc'); os.makedirs(OUT, exist_ok=True)
WJ = json.load(open(os.path.join(ROOT, 'Data', 'holomap-walls.json'))); WALLS, JETS = WJ['floors'], WJ['jetways']
BG, FG, DIM = '#05080c', '#f0f5fa', '#9fb0bf'
NEW, GLASS, RED, OK, WARN, JET, OLD = '#ffd166', '#9fe8ff', '#ff5566', '#3ddc6a', '#ff8a5b', '#ffb25a', '#4aa8c8'
W, H, DPI = 32, 18, 120
plt.rcParams['font.family'] = 'DejaVu Sans'


def sheet(no, title, sub=''):
    fig = plt.figure(figsize=(W, H), dpi=DPI); fig.patch.set_facecolor(BG)
    fig.text(0.012, 0.978, 'GATE ARC  ·  ' + no + '  ·  ' + title, color=FG, fontsize=27, weight='bold', va='top')
    if sub: fig.text(0.012, 0.938, sub, color=DIM, fontsize=16.5, va='top')
    fig.text(0.988, 0.008, 'Airport Dynasty Pro · gate 9 as the example · drawing only, nothing built · metres', color=DIM, fontsize=12.5, ha='right')
    return fig


def check(fig, name):
    fig.canvas.draw(); r = fig.canvas.get_renderer(); T = []
    for t in fig.findobj(matplotlib.text.Text):
        if not t.get_visible() or not t.get_text().strip(): continue
        a = t.axes
        if a is not None and t.get_clip_on():
            x, y = t.get_transform().transform(t.get_position())
            if not a.bbox.contains(x, y): continue
        T.append((t, t.get_window_extent(r)))
    n = 0
    for i in range(len(T)):
        b = T[i][1]
        if b.x0 < 0 or b.y0 < 0 or b.x1 > W * DPI or b.y1 > H * DPI: print('  OFF SHEET', name, repr(T[i][0].get_text()[:40])); n += 1
        for j in range(i + 1, len(T)):
            c = T[j][1]; ox = min(b.x1, c.x1) - max(b.x0, c.x0); oy = min(b.y1, c.y1) - max(b.y0, c.y0)
            if ox > 2 and oy > 2: print('  OVERLAP', name, repr(T[i][0].get_text()[:34]), '/', repr(T[j][0].get_text()[:34])); n += 1
    print(name, 'texts', len(T), 'problems', n)


def save(fig, name):
    check(fig, name); fig.savefig(os.path.join(OUT, name), facecolor=BG, dpi=DPI); plt.close(fig)


def ax_plan(fig, rect, x0, x1, z0, z1, aspect=True):
    ax = fig.add_axes(rect); ax.set_facecolor(BG); ax.set_xlim(x0, x1); ax.set_ylim(z0, z1); ax.axis('off')
    if aspect: ax.set_aspect('equal')
    return ax


def box(ax, x0, z0, x1, z1, fc='#141c24', ec='#5a6a7a', lw=1.4, ls='-', z=2, alpha=1.0):
    ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc=fc, ec=ec, lw=lw, ls=ls, zorder=z, alpha=alpha))


def txt(ax, x, z, s, col=FG, fs=14, **k):
    k.setdefault('ha', 'center'); k.setdefault('va', 'center')
    return ax.text(x, z, s, color=col, fontsize=fs, zorder=k.pop('zorder', 40), clip_on=True, linespacing=1.2, **k)


def lead(ax, x, z, tx, tz, s, col=FG, fs=14, **k):
    """a label with a thin leader line from (x, z)"""
    k.setdefault('ha', 'center'); k.setdefault('va', 'center')
    ax.annotate(s, xy=(x, z), xytext=(tx, tz), color=col, fontsize=fs, zorder=41, linespacing=1.2,
                arrowprops=dict(arrowstyle='-|>', color=col, lw=1.2, shrinkA=2, shrinkB=1), bbox=dict(fc=BG, ec='none', pad=1.5), **k)


def dim(ax, a, b, at, t=None, vertical=False, col=NEW, fs=14):
    t = t if t is not None else '%.2f' % abs(b - a)
    if vertical:
        ax.annotate('', xy=(at, a), xytext=(at, b), arrowprops=dict(arrowstyle='<->', color=col, lw=1.5, shrinkA=0, shrinkB=0), zorder=30)
        ax.text(at, (a + b) / 2, t, color=col, fontsize=fs, rotation=90, ha='center', va='center', bbox=dict(fc=BG, ec='none', pad=1.2), zorder=31, clip_on=True)
    else:
        ax.annotate('', xy=(a, at), xytext=(b, at), arrowprops=dict(arrowstyle='<->', color=col, lw=1.5, shrinkA=0, shrinkB=0), zorder=30)
        ax.text((a + b) / 2, at, t, color=col, fontsize=fs, ha='center', va='center', bbox=dict(fc=BG, ec='none', pad=1.2), zorder=31, clip_on=True)


def arcline(ax, A, col, lw=3, off=0.0, t0=None, t1=None, **k):
    t = np.linspace(-A.L / 2 if t0 is None else t0, A.L / 2 if t1 is None else t1, 240); p = [A.pt(u, off) for u in t]
    ax.plot([q[0] for q in p], [q[1] for q in p], color=col, lw=lw, solid_capstyle='butt', **k)


def jetway(ax, lw=1.8, sweep=False):
    box(ax, JCL, 82.0, JCR, 94.0, fc='#4a3a28', ec=JET, lw=lw, z=6, alpha=0.95)
    for x in (-213.15, -210.25):
        for zc in (86.0, 92.0): box(ax, x - 0.15, zc - 0.15, x + 0.15, zc + 0.15, fc=JET, ec=JET, z=9)
    ax.add_patch(Circle(ROT, ROTR, fc='#0c1c2c', ec=JET, lw=lw, zorder=7))
    dx, dz = -0.2617, 0.9651                                  # the docked tunnel's direction (the cab's middle -219.63, 124.99)
    a = np.array(ROT) + np.array((dx, dz)) * 1.6; n = np.array((dz, -dx)) * 1.62; b = a + np.array((dx, dz)) * 12
    ax.add_patch(Polygon([a + n, b + n, b - n, a - n], fc='#3a3026', ec=JET, lw=lw, zorder=5))


def gate9(ax, seats=True, lw=2.0, arc=None):
    """the real gate 9, its walls, stair, lift, seats, desk; arc: draw the bay instead of the flat window"""
    box(ax, X0, 68, XR, 82, fc='#0f1b26', ec='none', z=1); box(ax, XR, 68, -165.7, 82, fc='#0c151d', ec='none', z=1); box(ax, -241.1, 52, -80, 68, fc='#0b1218', ec='none', z=1)
    for x0, x1 in ((-241.1, -225.7), (-217.7, -187.7), (-177.7, -165.7)): ax.plot([x0, x1], [68, 68], color='#aab8c4', lw=lw + 1, zorder=8)
    ax.plot([X0, X0], [68, 82], color='#aab8c4', lw=lw + 2, zorder=8); ax.plot([XR, XR], [68, 82], color='#aab8c4', lw=lw + 1, zorder=8)
    ax.plot([XR, -165.7], [ZG, ZG], color=OLD, lw=lw + 1.5, zorder=8)
    for k in range(20): ax.plot([XR + 1.7895 * k] * 2, [81.80, 82.0], color=OLD, lw=1.6, zorder=8)
    sx0, sz0, sx1, sz1 = STAIR; box(ax, sx0, sz0, sx1, sz1, fc='#22303c', ec='#8fa2b3', z=5)
    for k in range(1, 17): ax.plot([sx0, sx1], [sz0 + (sz1 - sz0) * k / 17] * 2, color='#8fa2b3', lw=0.8, zorder=6)
    lx0, lz0, lx1, lz1 = LIFT; box(ax, lx0, lz0, lx1, lz1, fc='#22303c', ec=FG, lw=1.6, z=9); ax.plot([lx0, lx1], [lz0, lz1], color=FG, lw=1, zorder=9); ax.plot([lx0, lx1], [lz1, lz0], color=FG, lw=1, zorder=9)
    if seats:
        for z in (71.4, 74.4, 77.4):
            for k in range(37): box(ax, -230.61 + 0.62 * k - 0.28, z - 0.24, -230.61 + 0.62 * k + 0.28, z + 0.24, fc='#1d3a66', ec='#33558a', lw=0.5, z=4)
        box(ax, -218.0, 80.05, -215.4, 80.75, fc='#c8ccd0', ec='#fff', lw=0.8, z=5); box(ax, -216.95, 81.06, -216.45, 81.54, fc='#666', ec='#aaa', lw=0.8, z=5)
    if arc is None:
        ax.plot([X0, -212.6], [ZG, ZG], color=GLASS, lw=lw + 2, zorder=8); ax.plot([-210.8, XR], [ZG, ZG], color=GLASS, lw=lw + 2, zorder=8)
        for x in [-233.70 + 1.7585 * k for k in range(13)] + [-210.8 + 1.85 * k for k in range(7)]: ax.plot([x, x], [81.75, 82.05], color=GLASS, lw=2, zorder=8)
        ax.plot([-212.6, -210.8], [82, 82], color=OK, lw=lw + 4, zorder=10)
    else:
        A = arc; t = np.linspace(-A.L / 2, A.L / 2, 200); p = [A.pt(u) for u in t]
        ax.add_patch(Polygon([(X0, ZG)] + p + [(X1, ZG)], fc='#2a2a12', ec='none', zorder=2))
        tl, tr = A.t(JCL - 0.09), A.t(JCR + 0.09)
        for a, b in ((-A.L / 2, tl), (tr, A.L / 2)): arcline(ax, A, NEW, lw + 3, t0=a, t1=b, zorder=10)
        ax.plot([X1, XR], [ZG, ZG], color=GLASS, lw=lw + 2, zorder=8)
        for i, u in enumerate(A.mull()):
            if i in (13, 14): continue
            a, b = A.pt(u, -0.28), A.pt(u, 0.28); ax.plot([a[0], b[0]], [a[1], b[1]], color=FG, lw=2, zorder=11)
        ax.plot([-212.6, -210.8], [82, 82], color=OK, lw=lw + 4, zorder=12)
        for s in (tl, tr):
            a, b = A.pt(s, -0.3), A.pt(s, 0.3); ax.plot([a[0], b[0]], [a[1], b[1]], color='#c9d3dc', lw=6, zorder=12)


def new_seats(ax, A):
    """the 24 seats along the curve: two sections of 12 with the way to the glass between them"""
    for t in SEC_W + SEC_E:
        x, z = A.pt(t, SOFF); a = t / A.R; c, s = math.cos(a), math.sin(a)
        P = [(x + u * c + v * s, z - u * s + v * c) for u, v in ((-.28, -.41), (.28, -.41), (.28, .27), (-.28, .27))]
        ax.add_patch(Polygon(P, fc='#7a5cff', ec='#b7a6ff', lw=0.6, zorder=6))
    return len(SEC_W + SEC_E)
