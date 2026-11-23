# Blueprint of the wheel chocks and the apron cones (to-do 212): docs/Blueprints/Apron/ApronSafety.png and .pdf
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, Polygon
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Apron'); os.makedirs(OUT, exist_ok=True)
BG, INK, DIM, YEL, ORG, RED = '#000000', '#e8edf2', '#8fa3b5', '#f2c230', '#ff7a2a', '#ff5555'
plt.rcParams.update({'font.size': 11, 'text.color': INK, 'font.family': 'DejaVu Sans'})
fig, ax = plt.subplots(2, 3, figsize=(24, 15), facecolor=BG); ax = ax.ravel()


def panel(a, t, x0, x1, y0, y1):
    a.set_facecolor(BG); a.set_aspect('equal'); a.axis('off'); a.set_xlim(x0, x1); a.set_ylim(y0, y1); a.set_title(t, color=INK, fontsize=16)


def dim(a, p, q, t, off=(0, 0)):
    a.annotate('', p, q, arrowprops=dict(arrowstyle='<->', color=DIM, lw=1.2))
    a.text((p[0] + q[0]) / 2 + off[0], (p[1] + q[1]) / 2 + off[1], t, color=DIM, ha='center', va='center', bbox=dict(fc=BG, ec='none', pad=1))


SEC = [(-.10, 0), (-.035, 0), (-.028, .008), (.028, .008), (.035, 0), (.10, 0), (.10, .022), (.028, .15), (-.028, .15), (-.10, .022)]


def chock(a, x, z=0):
    a.add_patch(Polygon([(x + p, z + q) for p, q in SEC], fc='#20242a', ec=INK, lw=1.5))


def rad(z):
    return .15 - .121 * (z - .034) / .716


a = ax[0]; panel(a, 'Chock: section and side (mm)', -.4, .85, -.12, .42)
chock(a, -.15); dim(a, (-.25, -.04), (-.05, -.04), '200'); dim(a, (-.28, 0), (-.28, .15), '150', (-.04, 0))
a.add_patch(Rectangle((.1, 0), .6, .15, fc='#20242a', ec=INK, lw=1.5)); a.add_patch(Rectangle((.145, .085), .51, .045, fc=YEL)); a.add_patch(Circle((.728, .062), .022, fc='none', ec=INK, lw=2))
for k in range(3):
    a.plot([.13, .67], [.02 + .02 * k] * 2, color=DIM, lw=1)
dim(a, (.1, -.04), (.7, -.04), '600'); a.text(.4, .2, 'reflective strip on both sides\ngrip ribs, steel ring for the rope', ha='center', color=DIM)
a.text(.25, .33, 'solid rubber, about 7.7 kg each; a pair on a 1.12 m rope, red grip in the middle', ha='center')

a = ax[1]; panel(a, 'Set at the wheel, from the side (mm)', -.9, 2.5, -.25, 1.5)
for x, r, d, t in ((0, .455, .378, 'nose wheel'), (1.6, .645, .454, 'main wheel')):
    a.add_patch(Circle((x, r), r, fc='#14181c', ec=INK, lw=1.5)); a.add_patch(Circle((x, r), r * .5, fc='none', ec=DIM)); chock(a, x - d); chock(a, x + d)
    dim(a, (x - d, -.1), (x + d, -.1), '%d' % round(2000 * d)); a.text(x, 2 * r + .06, t, ha='center')
a.plot([-.9, 2.5], [0, 0], color=DIM, lw=1)

a = ax[2]; panel(a, 'Cone (mm)', -.6, .6, -.15, .95)
a.add_patch(Rectangle((-.2, 0), .4, .034, fc='#20242a', ec=INK)); a.add_patch(Polygon([(-.15, .034), (.15, .034), (.029, .75), (-.029, .75)], fc=ORG, ec=INK))
for z0, z1 in ((.285, .415), (.505, .605)):
    a.add_patch(Polygon([(-rad(z0), z0), (rad(z0), z0), (rad(z1), z1), (-rad(z1), z1)], fc='#f4f6f8'))
dim(a, (-.2, -.06), (.2, -.06), '400'); dim(a, (.32, 0), (.32, .75), '750', (.08, 0)); dim(a, (-.3, .285), (-.3, .415), '130', (-.08, 0)); dim(a, (-.3, .505), (-.3, .605), '100', (-.08, 0))
a.text(0, .85, 'hollow: they stack, 60 a cone', ha='center', color=DIM)

a = ax[3]; panel(a, 'Rack at the head of the stand (mm)', -1.3, 1.4, -.3, 1.2)
a.add_patch(Rectangle((-.98, .22), 1.48, .04, fc='#9aa4ad'))
for x in (-.98, .46):
    a.add_patch(Rectangle((x, 0), .04, .22, fc='#9aa4ad'))
for k in range(3):
    chock(a, -.72 + .48 * k - .115, .26); chock(a, -.72 + .48 * k + .115, .26)
a.add_patch(Rectangle((.54, .008), .46, .012, fc='#9aa4ad'))
for k in range(5):
    a.add_patch(Rectangle((.57, .02 + .06 * k), .4, .034, fc='#20242a', ec=INK))
a.add_patch(Polygon([(.62, .294), (.92, .294), (.80, 1.01), (.74, 1.01)], fc=ORG, ec=INK)); dim(a, (-.98, -.08), (1.0, -.08), '1980'); dim(a, (-1.08, 0), (-1.08, .26), '260', (-.1, 0))
a.text(-.25, .62, 'three pairs of chocks, five cones; 680 deep\nthe ropes towards the aircraft, their grips on top', ha='center', color=DIM)

a = ax[4]; panel(a, 'Places at the A320 (m from the nose wheel: right / aft)', -24, 24, 39, -14)
a.add_patch(Polygon([(0, -5.1), (1.9, -1), (1.98, 22), (.5, 32.4), (-.5, 32.4), (-1.98, 22), (-1.9, -1)], fc='#141a20', ec=INK))
for s in (-1, 1):
    a.add_patch(Polygon([(s * 1.9, 7), (s * 16.4, 13.7), (s * 16.4, 16.2), (s * 1.9, 14.8)], fc='#141a20', ec=INK)); a.add_patch(Rectangle((s * 5.6 - 1.1, 6.4), 2.2, 4, fc='#1c242c', ec=INK))
    a.add_patch(Polygon([(s * .8, 27.5), (s * 6.2, 30.3), (s * 6.2, 31.6), (s * .6, 31.3)], fc='#141a20', ec=INK))
for i, (x, y) in enumerate([(5.6, 4.9), (16.9, 14.9), (0, 33.4), (-16.9, 14.9), (-5.6, 4.9)]):
    a.add_patch(Circle((x, y), .55, fc=ORG)); a.text(x, y - 1.1, 'C%d  %.1f / %.1f' % (i + 1, x, y), ha='center', color=ORG, fontsize=9)
for x, y, t in ((.35, 0, '1'), (4.45, 12.8, '2'), (-4.45, 12.8, '3 (2 h or more)')):
    a.add_patch(Rectangle((x - .5, y - .9), 1, 1.8, fc=YEL)); a.text(x, y + 2.4, t, ha='center', color=YEL, fontsize=10)
a.add_patch(Rectangle((11.05, -8.3), 1.1, 2.4, fc='none', ec=YEL, lw=2)); a.text(11.6, -9, 'rack', ha='center', color=YEL); a.add_patch(Rectangle((5.2, -8.4), 4.3, 2.6, fc='none', ec=INK)); a.text(7.35, -6.8, 'GPU', ha='center')
a.add_patch(Rectangle((-16, -12.5), 13, 16.4, fc='none', ec=RED, hatch='//', alpha=.5)); a.text(-9.5, -9, 'jetway', ha='center', color=RED)

a = ax[5]; panel(a, 'Order of work', 0, 1, 0, 1); a.set_aspect('auto')
T = ['ARRIVAL: on blocks, beacon off', '1  nose chocks (by the rope, crouching, a hand on each)', '2  right main chocks; left main too when it stands 2 h or more', '3  cones in one round: C1 C2 C3 C4 C5 (3 right hand, 2 left)',
     'Vehicles come when the chocks are in (4 min longer at most)', '', 'DEPARTURE: the vehicles gone, the pushback crew there', '1  cones in: C5 C4 C3 C2 C1, back on the rack', '2  the tug hooked on: main chocks off, then the nose chocks',
     '3  the walkers go out; only then may the push begin', '', 'WHO: your flights - one hired person (the ground power queue);', 'before the push a wing walker, else the handler.', 'Other airlines: a free walker or the tug crew of the gate.', '',
     'Sizes: published data of 24 x 8 x 6 in aircraft chocks, a 750 mm', 'cone class; chocks only after the beacon is off (Milan airport rule).']
for i, t in enumerate(T):
    a.text(.02, .96 - i * .056, t, fontsize=13, color=YEL if ':' in t[:12] else INK)
fig.suptitle('WHEEL CHOCKS AND APRON CONES   (to-do 212)', color=INK, fontsize=24); fig.tight_layout(rect=(0, 0, 1, .96))
for e in ('png', 'pdf'):
    fig.savefig(os.path.join(OUT, 'ApronSafety.' + e), facecolor=BG, dpi=90)
