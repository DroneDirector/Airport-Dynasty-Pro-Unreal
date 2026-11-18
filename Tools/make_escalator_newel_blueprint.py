# Escalator newel - now vs planned (owner, 2026-11-18, his two reference photos): the handrail runs round the newel and
# down into an inlet housing at the floor (where the moving handrail goes back inside), and the glass follows inside the
# handrail's frame all the way round, as on real escalators. Black background (his rule). Out: docs/Blueprints/
#   python Tools/make_escalator_newel_blueprint.py
import math, os
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle, FancyBboxPatch
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Escalator-Newel-Now-vs-New.png')
BG, INK, DIM, GLASS, RAIL, STEEL, NEW, OLD = '#000000', '#e8e8ec', '#8a909a', '#7fc8ff', '#1a1a1a', '#b9bec6', '#55e0a0', '#ff8a65'
HR, ANG, RU = 0.93, math.radians(30), 2.6

def step_line(u):
    """the step line near the top: flat beyond u=0 (the top comb), the 2.6 m curve, then the 30 degree slope (downhill to the left)"""
    if u >= 0: return 0.0
    cu = -RU * math.sin(ANG)                    # where the curve meets the slope
    if u >= cu: return -(RU - math.sqrt(max(0.0, RU * RU - u * u)))
    return -(RU - RU * math.cos(ANG)) + (u - cu) * math.tan(ANG)

def rail(ax, pts, lw=7):
    xs, ys = zip(*pts); ax.plot(xs, ys, color='#000000', lw=lw + 2.5, solid_capstyle='round', zorder=6); ax.plot(xs, ys, color='#3a3a3a', lw=lw, solid_capstyle='round', zorder=7)

def base(ax, u0, u1):
    us = [u0 + (u1 - u0) * i / 200 for i in range(201)]
    deck = [(u, step_line(u) + 0.12) for u in us]
    ax.fill_between([p[0] for p in deck], [p[1] - 1.2 for p in deck], [p[1] for p in deck], color='#5c6068', zorder=1)   # the cladding
    ax.plot([u0, u1], [0, 0], color=DIM, lw=0.8, ls=':', zorder=1)
    ax.fill_between([0.0, u1], [-0.03, -0.03], [0.0, 0.0], color='#c9b48a', zorder=2)                                   # the floor (oak)
    return us

def frame(ax, title, col):
    ax.set_facecolor(BG); ax.set_aspect('equal'); ax.set_xlim(-3.3, 2.1); ax.set_ylim(-1.75, 1.55); ax.axis('off')
    ax.set_title(title, color=col, fontsize=13, fontweight='bold', loc='left')

fig = plt.figure(figsize=(18, 11), facecolor=BG)
# ---- NOW ----
ax = fig.add_axes([0.02, 0.43, 0.47, 0.5]); frame(ax, 'NOW - side of the top end (the hall escalators)', OLD)
us = base(ax, -3.3, 1.55)
COMB, UHO, R = 0.25, 0.75, 0.32
ue = COMB + UHO
HH = lambda u: step_line(min(u, COMB)) + HR
top = [(u, HH(u)) for u in us if u <= ue]
loop = [(ue + R * math.sin(a), HH(ue) - R + R * math.cos(a)) for a in [i * math.pi / 40 for i in range(41)]]
glass_low = lambda u: step_line(u) + 0.12 + max(0.0, 1 - (ue - u) / 3.0) ** 2 * (HH(ue) - 2 * R + 0.05 - 0.12) if u > ue - 3.0 else step_line(u) + 0.12
g = [(u, HH(u) - 0.05) for u in us if u <= ue] + [(ue + (R - 0.05) * math.sin(a), HH(ue) - R + (R - 0.05) * math.cos(a)) for a in [i * math.pi / 40 for i in range(41)]] + [(u, glass_low(u)) for u in reversed(us) if u <= ue]
ax.add_patch(Polygon(g, closed=True, fc=GLASS, ec=GLASS, alpha=0.35, zorder=4))
rail(ax, top + loop)
ax.annotate('handrail turns under and just stops\n(no way back inside)', xy=(ue + 0.05, HH(ue) - 2 * R), xytext=(0.2, -1.0), color=OLD, fontsize=10, arrowprops=dict(arrowstyle='->', color=OLD))
ax.annotate('glass ends in a low wedge,\nits edge not following the rail', xy=(ue - 0.6, glass_low(ue - 0.6) + 0.05), xytext=(-2.9, 1.25), color=OLD, fontsize=10, arrowprops=dict(arrowstyle='->', color=OLD))

# ---- NEW ----
ax = fig.add_axes([0.51, 0.43, 0.47, 0.5]); frame(ax, 'NEW - side of the top end (as your side photo)', NEW)
us = base(ax, -3.3, 1.55)
R2 = 0.45                                   # a bigger, rounder newel
ue2 = COMB + 0.45
cz = HH(ue2) - R2
ex = ue2 + R2                               # the newel's end: the handrail comes down here, straight into the housing
ix0, ix1, iz0, iz1 = ex - 0.16, ex + 0.13, 0.12, 0.42      # the inlet housing at the end, on the decking
top = [(u, HH(u)) for u in us if u <= ue2]
loop = [(ue2 + R2 * math.sin(a), cz + R2 * math.cos(a)) for a in [i * math.pi / 2 / 40 for i in range(41)]]   # over the top and down the end
down = [(ex, cz - (cz - iz1 + 0.02) * t) for t in [i / 10 for i in range(11)]]                                # straight down into it
rail(ax, top + loop + down)
ax.add_patch(FancyBboxPatch((ix0, iz0), ix1 - ix0, iz1 - iz0, boxstyle='round,pad=0.0,rounding_size=0.05', fc=STEEL, ec='#e0e4ea', lw=1.2, zorder=8))
ax.add_patch(Rectangle((ex - 0.05, iz1 - 0.03), 0.10, 0.04, fc='#111111', ec='#777', lw=0.8, zorder=9))           # its mouth on top
# the glass: the handrail's frame, 6 cm inside it all the way round, down to the housing; stainless base below it
inset = 0.06
gtop = [(u, HH(u) - inset) for u in us if u <= ue2]
gl = [(ue2 + (R2 - inset) * math.sin(a), cz + (R2 - inset) * math.cos(a)) for a in [i * math.pi / 2 / 40 for i in range(41)]]
gdown = [(ex - inset, iz1 + 0.01)]
low_end = iz1 + 0.01
glass_low2 = lambda u: step_line(u) + 0.12 + (max(0.0, 1 - (ix0 - u) / 3.0) ** 2) * (low_end - 0.12) if u > ix0 - 3.0 else step_line(u) + 0.12
gbot = [(u, glass_low2(u)) for u in reversed(us) if u <= ix0]
ax.add_patch(Polygon(gtop + gl + gdown + [(ix0, low_end)] + gbot, closed=True, fc=GLASS, ec=GLASS, alpha=0.35, zorder=4))
base_pts = [(u, glass_low2(u)) for u in us if u <= ix0] + [(ix0, 0.12)] + [(u, step_line(u) + 0.12) for u in reversed(us) if u <= ix0]
ax.add_patch(Polygon(base_pts, closed=True, fc=STEEL, ec='none', alpha=0.9, zorder=3))                              # the stainless base
ax.annotate('handrail over the top, down the end and straight\ninto the inlet housing (it keeps moving, back inside)', xy=(ex, iz1 + 0.15), xytext=(0.0, -1.05), color=NEW, fontsize=10, arrowprops=dict(arrowstyle='->', color=NEW))
ax.annotate('inlet housing at the floor: stainless,\nblack rubber mouth with brushes', xy=(ix0 + 0.05, iz0 + 0.12), xytext=(-2.9, -0.6), color=NEW, fontsize=10, arrowprops=dict(arrowstyle='->', color=NEW))
ax.annotate('glass 6 cm inside the handrail all the way round\nthe end, down to the housing; stainless base below', xy=(ue2 + 0.25, cz + 0.1), xytext=(-2.9, 1.25), color=NEW, fontsize=10, arrowprops=dict(arrowstyle='->', color=NEW))

# ---- NEW, from the front (as your front photo) ----
ax = fig.add_axes([0.02, 0.03, 0.45, 0.36]); ax.set_facecolor(BG); ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-1.6, 1.6); ax.set_ylim(-0.15, 1.4)
ax.set_title('NEW - the newels from the front', color=NEW, fontsize=13, fontweight='bold', loc='left')
ax.fill_between([-1.6, 1.6], [-0.03, -0.03], [0, 0], color='#c9b48a')
ax.add_patch(Rectangle((-0.5, 0.0), 1.0, 0.02, fc='#6a6e76'))                                    # the floor plate
for sx in (-1, 1):
    gx = sx * 0.62
    ax.add_patch(Rectangle((gx - 0.012, 0.42), 0.024, 0.93 - 0.42 - 0.06, fc=GLASS, alpha=0.45))   # the glass (edge on)
    ax.add_patch(Rectangle((gx - 0.04, 0.89), 0.08, 0.05, fc='#2a2a2a', ec='#000', lw=1))         # the handrail on top
    ax.add_patch(FancyBboxPatch((gx - 0.14, 0.0), 0.28, 0.42, boxstyle='round,pad=0.0,rounding_size=0.05', fc=STEEL, ec='#e0e4ea', lw=1.2))   # the inlet housing
    ax.add_patch(Rectangle((gx - 0.05, 0.16), 0.10, 0.1, fc='#111111', ec='#777', lw=0.8))         # its mouth: the handrail goes in here
    ax.plot([gx, gx], [0.27, 0.42], color='#2a2a2a', lw=6)
ax.text(0.0, 1.25, 'handrail in its loop - into the housing low down, out again at the top', color=INK, fontsize=10, ha='center')
ax.text(0.0, 0.6, 'steps and their\nanimation unchanged', color=DIM, fontsize=10, ha='center')

# ---- notes ----
ax = fig.add_axes([0.5, 0.03, 0.48, 0.36]); ax.set_facecolor(BG); ax.axis('off')
notes = ['What changes (both ends, all escalators - the hall pair and the arrivals pair):',
         '1  The handrail: a bigger, rounder newel; over the top and down the end, straight INTO an inlet housing at the floor,',
         '    as on real escalators (the moving handrail returns inside). Same black rubber, it keeps running in the game.',
         '2  The inlet housing: stainless with a black rubber mouth (brushes) - modelled, with the hole the handrail goes into.',
         '3  The glass: follows the handrail\'s frame 6 cm inside it all the way, round the D-shaped end, down to the housing.',
         '    No glass outside the handrail anywhere; the stainless base under the glass at the ends stays.',
         '4  Kept as they are: the steps, their animation and speed, the combs, floor plates, decking, cladding, the colours',
         '    and materials (matte steps from today), the sounds.',
         '',
         'Your OK and I build it (about 2-3 working hours, then pictures from the front and the side).']
for i, t in enumerate(notes): ax.text(0.0, 0.95 - i * 0.095, t, color=INK if i else NEW, fontsize=10.5, transform=ax.transAxes, va='top', fontweight='bold' if i == 0 else 'normal')
os.makedirs(os.path.dirname(OUT), exist_ok=True)
fig.savefig(OUT, dpi=110, facecolor=BG); print(OUT)
