# The full remote row (owner 2026-11-23: room for more past the next links, east and west?) and, revised for wide-bodies (owner 2026-11-23: can an A321, A350 and B777 fit?): R1-R2 for code C
# (A321), a flexible stand R3 (2 x A321 or 1 x A350 / B777, set 12 m further south), the aircraft drawn to scale and
# the ICAO / EASA clearances measured. Writes Blueprints/Airfield/RemoteStands-Full.png.
#   python3 Tools/plan_remote_stands_fit.py
import json, os, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Blueprints', 'Airfield'); os.makedirs(OUT, exist_ok=True)
d = json.load(open(os.path.join(ROOT, 'Data', 'airfield.json'), encoding='utf-8'))
C = dict(ink='#e8f0ff', sub='#96afd2', acc='#ffc446', grn='#6ee68c', red='#ff6b6b', blue='#7fc4ff', twy='#2a4a75', ac='#dfe8f5')
fig, ax = plt.subplots(figsize=(19.2, 12.4), dpi=200); fig.patch.set_facecolor('#081426'); ax.set_facecolor('#081426')
def R(x0, y0, x1, y1, **k): ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, **k))
# #region ── Aircraft, to scale (span, length; fuselage width; wing root at 40 % of the length) ──
def plane(xc, ynose, dirn, span, L, fus, label, col=C['ac'], west=False):
    """dirn +1: nose north, -1: nose south; west=True: lying along x, nose to the west at (xc, ynose)."""
    def P(u, v):                               # u across (to the aircraft's right), v from the nose back along the body
        if west: return (xc + v, ynose + u)
        s = 1 if dirn > 0 else -1
        return (xc + s * u, ynose - s * v)
    root, tip = 0.42 * L, 0.58 * L
    poly = lambda pts: ax.add_patch(Polygon([P(u, v) for u, v in pts], fc=col, ec='none', alpha=0.9))
    poly([(-fus / 2, 3), (0, 0), (fus / 2, 3), (fus / 2, L - 4), (-fus / 2, L - 4)])
    for k in (-1, 1):
        poly([(k * fus / 2, root), (k * span / 2, tip), (k * span / 2, tip + 2.5), (k * fus / 2, root + 0.12 * L)])
        poly([(k * fus / 2, L - 9), (k * span * 0.17, L - 2), (k * span * 0.17, L - 0.5), (k * fus / 2, L - 4)])
    x, y = P(0, 0.5 * L + 6)
    ax.text(x, y, label, color='#081426', fontsize=9, ha='center', va='center', weight='bold', rotation=0 if west else 90)
def dim(x0, y0, x1, y1, t, col=C['acc'], off=(0, 0), ha='center'):
    ax.annotate('', xy=(x1, y1), xytext=(x0, y0), arrowprops=dict(arrowstyle='<->', color=col, lw=1.4))
    ax.text((x0 + x1) / 2 + off[0], (y0 + y1) / 2 + off[1], t, color=col, fontsize=10, ha=ha, va='center', weight='bold',
            bbox=dict(fc='#081426', ec='none', pad=1.5))
# #endregion
# #region ── The apron today (part) ──
R(-300, 30, 600, 82, fc='#33507a', ec=C['ink'], lw=1); ax.text(150, 55, 'TERMINAL', color=C['ink'], ha='center', va='center', fontsize=15, weight='bold')
R(-300, 82, 600, 238, fc='#173257', ec='none')
R(-300, 205.5, 560, 230.5, fc=C['twy'], ec='none'); ax.plot([-300, 560], [218, 218], color=C['acc'], lw=1.2); ax.text(-296, 224, 'Taxilane T', color=C['acc'], fontsize=11)
R(-300, 373.5, 600, 398.5, fc=C['twy'], ec='none'); ax.plot([-300, 600], [386, 386], color=C['acc'], lw=1.2); ax.text(-296, 392, 'Taxiway A', color=C['acc'], fontsize=11)
for n, x in (('P1', -130), ('P2', 150), ('P3', 362)):
    R(x - 12.5, 218, x + 12.5, 386, fc=C['twy'], ec='none'); ax.plot([x, x], [218, 386], color=C['acc'], lw=1.2); ax.text(x + 14, 362, n, color=C['acc'], fontsize=12, weight='bold')
for s in d['stands']:
    if s.get('cargo') or not (-300 < s['x'] < 600): continue
    if s['code'] == 'F':
        R(s['x'] - s['span'] / 2, s['nose'], s['x'] + s['span'] / 2, s['nose'] + s['len'], fc='none', ec=C['blue'], lw=0.8)
        ax.text(s['x'], s['nose'] + 35, '13\nA380', color=C['ink'], fontsize=10, ha='center', va='center'); continue
    x, n, sp, L = s['x'], s['nose'], s['span'], s['len']
    R(x - sp / 2, n, x + sp / 2, n + L, fc='none', ec=C['blue'], lw=0.8)
    if s['jetway']: R(x + 3, 82, x + 16, n + 9, fc=C['red'], alpha=0.35, ec='none')
    plane(x, n, -1, 35.8 if s['code'] == 'C' else 64.8, 44.5 if s['code'] == 'C' else 73.9, 4.0 if s['code'] == 'C' else 6.2, 'A321' if s['code'] == 'C' else 'B777')
    ax.text(x, n - 6, s['id'].split('-')[1], color=C['ink'], fontsize=9, ha='center', va='center')
# #endregion
# #region ── The revised remote row ──
for nm, xc in (('R1', -68.5), ('R2', -28.0)):
    R(xc - 18, 272, xc + 18, 318, fc='none', ec=C['grn'], lw=1.6); plane(xc, 318, 1, 35.8, 44.5, 4.0, 'A321')
    ax.text(xc, 268, nm, color=C['grn'], fontsize=13, ha='center', va='top', weight='bold')
R(-2.5, 260, 74, 337, fc='none', ec=C['grn'], lw=2.0, ls='--')
plane(35.75, 260 + 73.9, 1, 64.8, 73.9, 6.2, 'B777-300ER')
ax.text(35.75, 256, 'R3  flexible: 2 x A321  or  1 x A350 / B777', color=C['grn'], fontsize=11, ha='center', va='top', weight='bold')
for xc in (15.5, 56.0): R(xc - 18, 272, xc + 18, 318, fc='none', ec=C['grn'], lw=0.8, ls=':')
# east of P2: R4 flexible + R5 (code C); east of P3: R6, R7 (code E: A350 / B777), kept 51 m from P3 (A380s use it)
R(193.5, 260, 270, 337, fc='none', ec=C['grn'], lw=2.0, ls='--'); plane(231.75, 260 + 66.8, 1, 64.75, 66.8, 6.0, 'A350-900')
ax.text(231.75, 256, 'R4  flexible', color=C['grn'], fontsize=12, ha='center', va='top', weight='bold')
R(274.5, 272, 310.5, 318, fc='none', ec=C['grn'], lw=1.6); plane(292.5, 318, 1, 35.8, 44.5, 4.0, 'A321')
ax.text(292.5, 268, 'R5', color=C['grn'], fontsize=13, ha='center', va='top', weight='bold')
for nm, xc in (('R6', 445.5), ('R7', 518.0)):
    R(xc - 32.5, 260, xc + 32.5, 337, fc='none', ec=C['grn'], lw=1.6); plane(xc, 260 + 73.9, 1, 64.8, 73.9, 6.2, 'B777-300ER')
    ax.text(xc, 256, nm, color=C['grn'], fontsize=13, ha='center', va='top', weight='bold')
# west of P1: optional R0 (code C) if the bus depot moves west of taxilane T's end (apron about 45 m wider there)
R(-209.5, 272, -173.5, 318, fc='none', ec=C['acc'], lw=1.4, ls='--'); ax.text(-191.5, 268, 'R0 (option)', color=C['acc'], fontsize=12, ha='center', va='top', weight='bold')
R(-298, 244, -258, 304, fc='#3a2f10', ec=C['acc'], lw=1.0, ls='--'); ax.text(-278, 274, 'bus\ndepot\nmoved\nhere', color=C['acc'], fontsize=8, ha='center', va='center')
dim(-173.5, 345, -130, 345, '43.5', off=(0, 7)); dim(150, 345, 193.5, 345, '43.5', off=(0, 7))
dim(310.5, 345, 362, 345, '51.5', off=(0, 7)); dim(362, 345, 413, 345, '51', off=(0, 7))
ax.set_xlim(-300, 600); ax.set_ylim(25, 470); ax.set_aspect('equal'); ax.axis('off')
ax.text(-295, 466, 'REMOTE STANDS, THE FULL ROW  ·  8 stands (9 with R0), up to 4 wide-bodies, drawn to scale', color=C['ink'], fontsize=17, weight='bold', va='top')
ax.text(-295, 447, 'gaps to the links: 43.5 m (A350 / B777 on them), 51 m by P3 (the A380s)  ·  ICAO Annex 14 / EASA CS ADR-DSN legal minimums  ·  metres, north up',
        color=C['sub'], fontsize=11, va='top')
plt.savefig(os.path.join(OUT, 'RemoteStands-Full.png'), facecolor=fig.get_facecolor(), bbox_inches='tight')
print('written')
