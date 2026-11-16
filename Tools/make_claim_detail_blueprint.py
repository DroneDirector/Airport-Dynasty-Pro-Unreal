# The baggage claim's detail blueprint (owner, 2026-11-16): the claim hall and the sorting hall's input side behind it,
# drawn to the concept's sizes (Art/BaggageClaim/render_claim_concept.py, docs/Baggage-Claim-Concept.md): every belt
# with its plates, island, feed shaft, hood and flaps; each input conveyor with its cart spot; the bag's way from where
# the crew put it to the belt; escalators, stair, lift, screens, customs; dimensions. Below it a side section along
# belt 2's line showing the heights. Black background, light lines (documents rule).
#   python Tools/make_claim_detail_blueprint.py   ->  docs/Baggage-Claim-Detail-Blueprint.png
import math, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, FancyArrowPatch, Circle
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BG, INK, DIM, WALL, NEW, BAG, FLOW, STEEL, RUB, CREW = '#000000', '#e8edf2', '#6f7c88', '#c9d2da', '#35d0e0', '#f2b544', '#ff5a5a', '#9fb0bf', '#4a4f55', '#f5d020'
X0, X1, Y0, Y1 = -44.0, 0.0, 16.0, 52.0
SY0, SY1 = 52.3, 68.0
BELTS = [(-31.0, 31.0, 1), (-19.0, 31.0, 2), (-7.0, 31.0, 3)]
L, RC = 15.0, 1.8

def track(cx, cy, r, l=L, seg=40):
    pts = [(cx + r * math.cos(math.pi * i / seg), cy + l / 2 + r * math.sin(math.pi * i / seg)) for i in range(seg + 1)]
    pts += [(cx + r * math.cos(math.pi + math.pi * i / seg), cy - l / 2 + r * math.sin(math.pi + math.pi * i / seg)) for i in range(seg + 1)]
    return pts
def path_at(cx, cy, s, r=RC):
    per = 2 * L + 2 * math.pi * r; s %= per
    if s < L: return (cx + r, cy - L / 2 + s), (0, 1)
    s -= L
    if s < math.pi * r: a = s / r; return (cx + r * math.cos(a), cy + L / 2 + r * math.sin(a)), (-math.sin(a), math.cos(a))
    s -= math.pi * r
    if s < L: return (cx - r, cy + L / 2 - s), (0, -1)
    s -= L; a = math.pi + s / r; return (cx + r * math.cos(a), cy - L / 2 + r * math.sin(a)), (-math.sin(a), math.cos(a))
def rect(ax, x0, y0, x1, y1, fc='none', ec=WALL, lw=1.0, ls='-', z=2, alpha=1):
    ax.add_patch(Rectangle((min(x0, x1), min(y0, y1)), abs(x1 - x0), abs(y1 - y0), fc=fc, ec=ec, lw=lw, ls=ls, zorder=z, alpha=alpha))
def dim(ax, a, b, text, off, horiz=True, col=DIM):
    # a dimension line with end ticks and its value
    if horiz:
        y = a[1] + off; ax.plot([a[0], b[0]], [y, y], color=col, lw=0.7); ax.plot([a[0], a[0]], [y - 0.4, y + 0.4], color=col, lw=0.7); ax.plot([b[0], b[0]], [y - 0.4, y + 0.4], color=col, lw=0.7)
        ax.text((a[0] + b[0]) / 2, y + 0.35, text, color=col, fontsize=6.5, ha='center', va='bottom')
    else:
        x = a[0] + off; ax.plot([x, x], [a[1], b[1]], color=col, lw=0.7); ax.plot([x - 0.4, x + 0.4], [a[1], a[1]], color=col, lw=0.7); ax.plot([x - 0.4, x + 0.4], [b[1], b[1]], color=col, lw=0.7)
        ax.text(x - 0.35, (a[1] + b[1]) / 2, text, color=col, fontsize=6.5, ha='right', va='center', rotation=90)
def arrow(ax, pts, col, lw=1.4, ls='--', ms=9):
    for a, b in zip(pts, pts[1:]): ax.add_patch(FancyArrowPatch(a, b, arrowstyle='-|>', mutation_scale=ms, color=col, lw=lw, ls=ls, zorder=6))

fig = plt.figure(figsize=(16.5, 21.0), facecolor=BG)
ax = fig.add_axes([0.05, 0.31, 0.90, 0.63]); ax.set_facecolor(BG); ax.set_aspect('equal')
ax.set_xlim(-50, 6); ax.set_ylim(9, 70); ax.axis('off')
# ---------------------------------------------------------------- walls
rect(ax, X0, Y0 - 0.3, X1 + 0.3, Y1 + 0.3, ec=WALL, lw=2.2)
rect(ax, X0 - 0.12, Y0, X0, Y1, fc='#1b3a44', ec=NEW, lw=0.8)                       # the west glass wall
for y in [Y0 + 2.5 * k for k in range(15)]: ax.plot([X0 - 0.3, X0 + 0.1], [y, y], color=STEEL, lw=0.6)
rect(ax, X1, Y0, X1 + 0.3, Y1, fc='#25282c', ec=WALL, lw=0.8)                        # the basalt wall
ax.plot([-32, -24], [Y0 - 0.15, Y0 - 0.15], color=BG, lw=4, zorder=3)                 # the customs doorway
ax.text(-28, Y0 - 1.6, 'to customs (doorway 8 m)', color=NEW, fontsize=7, ha='center')
rect(ax, X0, SY0, X1 + 8, SY1, ec=WALL, lw=1.6)                                      # the sorting hall
ax.text(-2, SY1 - 1.2, 'BAGGAGE SORTING HALL (crew only)  -  continues east under the walkway', color=STEEL, fontsize=7.5, ha='right')
ax.text(-22, Y0 + 1.2, 'BAGGAGE CLAIM HALL  44 x 36 m, ceiling 5.4 m', color=INK, fontsize=9, ha='center', fontweight='bold')
# the Floor 2 corridor's edge overhead and the escalator void
ax.plot([X0, X1], [40, 40], color=DIM, lw=0.8, ls=(0, (6, 4))); ax.text(-13, 39.2, 'Floor 2 corridor edge above', color=DIM, fontsize=5.5, ha='center')
# ---------------------------------------------------------------- escalators, stair, lift
for x0 in (-43.6, -41.9):
    rect(ax, x0, 39.0, x0 + 1.6, 51.5, fc='#0e2a30', ec=NEW, lw=0.9)
    for k in range(26): ax.plot([x0 + 0.15, x0 + 1.45], [40 + k * 0.4, 40 + k * 0.4], color='#2c6670', lw=0.5)
    arrow(ax, [(x0 + 0.8, 50.5), (x0 + 0.8, 40.2)], NEW, 1.0, '-', 7)
rect(ax, -40.2, 39.6, -38.2, 51.0, fc='#14303a', ec=NEW, lw=0.9)
for k in range(30): ax.plot([-40.2, -38.2], [39.6 + k * 0.38, 39.6 + k * 0.38], color='#2c6670', lw=0.4)
ax.text(-41.1, 52.6 - 1.6, '', color=NEW)
ax.text(-40.6, 45.3, 'ESCALATORS (2, down)   STAIR', color=NEW, fontsize=6.2, rotation=90, ha='center', va='center')
rect(ax, -36.6, 47.6, -34.4, 50.4, fc='#0e2a30', ec=NEW, lw=0.9); ax.plot([-36.6, -34.4], [47.6, 50.4], color=NEW, lw=0.5); ax.plot([-36.6, -34.4], [50.4, 47.6], color=NEW, lw=0.5)
ax.text(-35.5, 46.9, 'LIFT', color=NEW, fontsize=6, ha='center')
rect(ax, -43.0, 37.42, -37.0, 37.5, fc=BAG, ec=BAG, lw=0.5); ax.text(-40, 36.7, 'sign: Baggage claim / Belts 1-3', color=BAG, fontsize=5.5, ha='center')
# ---------------------------------------------------------------- the belts
for cx, cy, n in BELTS:
    ax.add_patch(Polygon(track(cx, cy, 2.45), closed=True, fc='#2a2f35', ec=STEEL, lw=1.3, zorder=3))          # skirt
    ax.add_patch(Polygon(track(cx, cy, 2.25), closed=True, fc=RUB, ec='#1a1c1f', lw=0.6, zorder=3))            # plates
    per = 2 * L + 2 * math.pi * RC; np_ = int(per / 0.3)
    for k in range(np_):
        (px, py), (tx, ty) = path_at(cx, cy, k * per / np_); nx, ny = ty, -tx
        ax.plot([px - nx * 0.46, px + nx * 0.46], [py - ny * 0.46, py + ny * 0.46], color='#202326', lw=0.35, zorder=4)
    ax.add_patch(Polygon(track(cx, cy, 1.30, L - 0.2), closed=True, fc='#3a4048', ec=STEEL, lw=1.1, zorder=5))  # island
    # the plates' way round (anticlockwise seen from above)
    for s in (2.0, 9.0, 24.0, 31.0):
        (px, py), (tx, ty) = path_at(cx, cy, s); ax.add_patch(FancyArrowPatch((px - tx * 0.6, py - ty * 0.6), (px + tx * 0.6, py + ty * 0.6), arrowstyle='-|>', mutation_scale=7, color=BAG, lw=1.0, zorder=7))
    # the feed shaft, its incline, hood and flaps
    rect(ax, cx - 0.45, cy + 5.4, cx + 1.4, cy + 6.6, fc='#000000', ec=FLOW, lw=1.0, z=6)
    rect(ax, cx - 0.5, cy + 5.4, cx + 0.75, cy + 6.6, fc='none', ec=STEEL, lw=1.2, ls='--', z=7)
    for k in range(8): ax.plot([cx + 0.73, cx + 0.73], [cy + 5.47 + k * 0.135, cy + 5.59 + k * 0.135], color=INK, lw=1.2, zorder=8)
    arrow(ax, [(cx - 0.3, cy + 6.0), (cx + 1.9, cy + 6.0)], FLOW, 1.4, '-', 8)
    ax.text(cx, cy, 'BELT %d' % n, color=BAG, fontsize=9, ha='center', va='center', rotation=90, fontweight='bold', zorder=8)
    ax.text(cx - 0.1, cy + 4.3, 'feed\nshaft', color=FLOW, fontsize=5.5, ha='center', va='top', zorder=8)
    # its screen
    rect(ax, cx - 1.0, cy + 10.4, cx + 1.0, cy + 10.5, fc=NEW, ec=NEW, lw=1.2, z=6)
    ax.text(cx, cy + 10.9, 'screen %d' % n, color=NEW, fontsize=5.5, ha='center')
# dimensions of belt 1 and the gaps
dim(ax, (-31 - 2.45, 21.1), (-31 + 2.45, 21.1), '4.9 m', -2.0)
dim(ax, (-31 - 2.45, 31 - 9.95), (-31 - 2.45, 31 + 9.95), '19.9 m overall  (15 m straight)', -0.5, horiz=False)
dim(ax, (-31 + 2.45, 24.0), (-19 - 2.45, 24.0), '7.1 m walkway', 0)
dim(ax, (-19 + 2.45, 24.0), (-7 - 2.45, 24.0), '7.1 m walkway', 0)
ax.text(-19, 19.0, 'each belt: 41.3 m of plates you can reach, 0.92 m wide, top 0.45 m up, 0.46 m/s', color=BAG, fontsize=7, ha='center')
# the hall's own dimensions
dim(ax, (X0, Y0 - 0.3), (X1, Y0 - 0.3), '44.0 m', -3.2, col=WALL)
dim(ax, (X0, Y0), (X0, Y1), '36.0 m', -2.5, horiz=False, col=WALL)
# seats along the glass
for k in range(5): rect(ax, -43.2, 20 + k * 3.2, -42.6, 22.2 + k * 3.2, fc='#5a3d22', ec='#8a6a44', lw=0.5)
ax.text(-42.2, 27, 'seats', color='#8a6a44', fontsize=5.5, rotation=90, va='center')
# ---------------------------------------------------------------- the crew side: input conveyors
for cx, cy, n in BELTS:
    rect(ax, cx - 0.55, SY0, cx + 0.55, SY0 + 6.0, fc=RUB, ec=STEEL, lw=1.1, z=4)
    for k in range(20): ax.plot([cx - 0.5, cx + 0.5], [SY0 + 0.3 * k, SY0 + 0.3 * k], color='#202326', lw=0.35, zorder=5)
    ax.plot([cx - 0.8, cx + 0.8], [SY0 + 0.05, SY0 + 0.05], color=CREW, lw=3, zorder=6)                         # the yellow frame
    rect(ax, cx + 1.2, SY0 + 5.0, cx + 2.6, SY0 + 7.8, fc='#22262a', ec=CREW, lw=0.8, ls='--', z=4)            # the cart spot
    ax.text(cx + 1.9, SY0 + 8.3, 'cart', color=CREW, fontsize=5.5, ha='center')
    ax.plot([cx - 0.95, cx - 0.95], [SY0 + 0.4, SY1 - 0.5], color=CREW, lw=0.8)                                 # walking line
    ax.add_patch(Circle((cx, SY0 + 5.6), 0.35, fc=CREW, ec=CREW, zorder=7)); ax.text(cx - 1.3, SY0 + 5.6, 'crew\nput bags\nhere', color=CREW, fontsize=5.5, ha='right', va='center')
    ax.text(cx, SY0 + 3.0, 'INPUT %d\n6 m, 0.75 m high' % n, color=INK, fontsize=5.2, ha='center', va='center', rotation=90, zorder=8)
    # the bag's way: along the input, into the wall (flaps), hidden, up the shaft, onto the plates
    arrow(ax, [(cx, SY0 + 5.2), (cx, SY0 + 0.6)], BAG, 1.6, '-', 9)
    arrow(ax, [(cx, SY0 - 0.2), (cx, cy + 6.75)], FLOW, 1.2, (0, (2, 3)), 8)
ax.text(-25.5, 46.6, 'hidden: the game carries the bag from\nthe wall straight to its belt\'s shaft\n(no belt under the floor, no basement)', color=FLOW, fontsize=6.4, ha='left', va='center')
dim(ax, (-19 + 0.55, SY0), (-19 + 0.55, SY0 + 6.0), '6.0 m', 0.9, horiz=False, col=STEEL)
# north arrow, scale bar, legend
ax.add_patch(FancyArrowPatch((3.5, 60), (3.5, 64), arrowstyle='-|>', mutation_scale=14, color=INK, lw=1.4)); ax.text(3.5, 64.6, 'N\n(airside)', color=INK, fontsize=7, ha='center', va='bottom')
for k in range(5): rect(ax, -44 + 2 * k, 9.6, -42 + 2 * k, 10.1, fc=INK if k % 2 == 0 else BG, ec=INK, lw=0.5)
ax.text(-44, 10.5, '0', color=INK, fontsize=6); ax.text(-34, 10.5, '10 m', color=INK, fontsize=6, ha='right')
fig.text(0.05, 0.975, 'AIRPORT DYNASTY PRO  -  BAGGAGE CLAIM, DETAIL BLUEPRINT (to-do 72)', color=INK, fontsize=16, fontweight='bold')
fig.text(0.05, 0.962, 'Plan view, Floor 1, plan metres (x along the building, z north).  White: walls.  Cyan: glass, escalators, stair, lift, screens.  Grey/black: belts, plates, islands.'
         '\nAmber: the bags\' way on the belts.  Red: the feed shafts and the hidden step.  Yellow: the crew side (input frames, cart spots, walking lines).', color='#a9b4bf', fontsize=9, va='top')

# ---------------------------------------------------------------- side section along belt 2 (x = -19), looking west
sx = fig.add_axes([0.05, 0.085, 0.90, 0.21]); sx.set_facecolor(BG); sx.axis('off'); sx.set_aspect(2.5)
sx.set_xlim(14, 66); sx.set_ylim(-1.4, 7.2)
sx.plot([18, 70], [0, 0], color=WALL, lw=1.6)                                             # the floor
sx.plot([18, 52], [5.4, 5.4], color=WALL, lw=1.2); sx.plot([52.3, 68], [5.4, 5.4], color=WALL, lw=1.2)
sx.add_patch(Rectangle((40, 5.4), 12.3, 0.6, fc='#30363c', ec=WALL, lw=0.8)); sx.text(46, 6.4, 'Floor 2 arrivals corridor (floor 6.0 m)', color=DIM, fontsize=6.5, ha='center')
sx.add_patch(Rectangle((52.0, 0), 0.3, 6.0, fc=WALL, ec=WALL))                              # the north wall
sx.text(52.15, 6.4, 'north wall', color=WALL, fontsize=6.5, ha='center')
# the belt in section: skirt, plates both sides, island, shaft and incline, hood with flaps
b0, b1 = 31 - 9.95, 31 + 9.95
sx.add_patch(Rectangle((b0, 0), b1 - b0, 0.40, fc='#2a2f35', ec=STEEL, lw=1.0))
sx.add_patch(Rectangle((b0 + 0.2, 0.42), b1 - b0 - 0.4, 0.03, fc=RUB, ec=RUB))
sx.add_patch(Rectangle((31 - 7.5 - 1.2, 0), 15 + 2.4 - 0.2, 0.62, fc='#3a4048', ec=STEEL, lw=1.0))
sx.add_patch(Rectangle((36.4, 0.0), 1.2, 0.64, fc=BG, ec=FLOW, lw=1.0))                     # the shaft opening
sx.plot([37.0, 37.0], [0.05, 0.66], color=FLOW, lw=1.6)
sx.add_patch(Rectangle((36.4, 0.64), 1.25, 0.56, fc='none', ec=STEEL, lw=1.2))
for k in range(5): sx.plot([36.45 + k * 0.25, 36.45 + k * 0.25], [0.68, 1.15], color=INK, lw=0.6)
sx.add_patch(FancyArrowPatch((37.0, 0.1), (37.0, 0.95), arrowstyle='-|>', mutation_scale=9, color=FLOW, lw=1.4))
sx.text(37.0, 2.2, 'bag rises out of the shaft (18 deg incline) under the hood, through the flaps, onto the plates', color=FLOW, fontsize=6.5, ha='center')
# the input conveyor and its frame
sx.add_patch(Rectangle((52.3, 0), 6.0, 0.70, fc='#1c1f22', ec=STEEL, lw=1.0)); sx.add_patch(Rectangle((52.3, 0.70), 6.0, 0.05, fc=RUB, ec=RUB))
sx.add_patch(Rectangle((52.3, 0.70), 0.1, 1.15, fc=CREW, ec=CREW))
sx.add_patch(FancyArrowPatch((57.8, 1.0), (52.6, 1.0), arrowstyle='-|>', mutation_scale=9, color=BAG, lw=1.4))
sx.text(55.3, 1.45, 'input conveyor (crew load at its far end)', color=BAG, fontsize=6.5, ha='center')
sx.add_patch(FancyArrowPatch((52.0, 0.9), (37.8, 0.9), arrowstyle='-|>', mutation_scale=9, color=FLOW, lw=1.0, ls=(0, (2, 3))))
sx.text(45.0, 0.95, 'hidden in the wall, then up the shaft', color=FLOW, fontsize=6, ha='center', va='bottom')
sx.add_patch(Rectangle((60, 0), 2.8, 0.42, fc='#22262a', ec=CREW, lw=0.8, ls='--')); sx.text(61.4, 0.6, 'cart', color=CREW, fontsize=6, ha='center')
# heights
for h, ty, t in ((0.45, 0.25, 'plates 0.45 m'), (0.62, 1.0, 'island 0.62 m'), (1.2, 1.75, 'hood 1.2 m'), (5.4, 5.0, 'ceiling 5.4 m')):
    sx.plot([20.0, 21.0], [h, h], color=DIM, lw=0.7); sx.plot([20.0, 19.6], [h, ty], color=DIM, lw=0.5); sx.text(19.5, ty, t, color=DIM, fontsize=6.5, va='center', ha='right')
sx.plot([58.4, 59.2], [0.75, 0.75], color=DIM, lw=0.7); sx.text(59.3, 0.95, 'input 0.75 m', color=DIM, fontsize=6.5, va='bottom')
sx.text(14.2, -1.2, 'SECTION A-A along belt 2 (x -19), looking west  -  heights drawn 2.5 x taller than lengths, to read them', color=INK, fontsize=8.5, fontweight='bold')
sx.text(31, -0.6, 'belt 2: 19.9 m', color=STEEL, fontsize=6.5, ha='center'); sx.text(55.3, -0.6, 'sorting hall (crew)', color=CREW, fontsize=6.5, ha='center'); sx.text(26, 3.0, 'claim hall', color=INK, fontsize=7, ha='center')
notes = ['1  The crew park a cart at the input conveyor and put each bag on it; the conveyor carries it 6 m to the wall and through the rubber flaps.',
         '2  In the game the bag is then hidden for a moment and placed in that belt\'s feed shaft (there is no basement or tunnel under the floor).',
         '3  An 18 degree incline lifts it out of the shaft, under the stainless hood and through its flaps, and drops it onto the plates - it looks as if it came up from below.',
         '4  The plates carry it round anticlockwise at 0.46 m/s until a passenger takes it; each flight is given its own belt, shown on the screen over that belt.']
fig.text(0.05, 0.02, '\n'.join(notes), color='#cfd7de', fontsize=9, va='bottom', linespacing=1.5)
out = os.path.join(ROOT, 'docs', 'Baggage-Claim-Detail-Blueprint.png'); fig.savefig(out, dpi=150, facecolor=BG); print('DETAIL', out)
