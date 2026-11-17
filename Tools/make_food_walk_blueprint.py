# Food Walk entrances blueprint (owner, 2026-11-17, to-do 94 - first on the list, nothing else until he confirms):
# the wall between the Floor 2 landing and the Food Walk (x 56, from the old tax free room's corner at z 42 to the gate
# walkway at z 52) goes, and the same at the other end (x 14, the link to the walkway); in each opening a beautiful
# entrance portal, flush with the glass on one side and the wall end on the other. Plan from Data/terminal-plan.json.
# Big text for the phone; black background (documents rule). Drawing only.
#   python Tools/make_food_walk_blueprint.py  ->  docs/Blueprints/FoodWalk-*.png
import json, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'Blueprints'); os.makedirs(OUT, exist_ok=True)
P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json'), encoding='utf-8'))
BG, INK, DIM, CUT, NEW, GOLD = '#000000', '#e8edf2', '#6d7884', '#ff4f6d', '#35f0ff', '#ffc870'

ROOMS = {r['id']: r for r in P['rooms'] if r.get('f') == 2}
SHOW = ['F2-LINK', 'F2-FOOD', 'F2-LANDING', 'F2-TAXFREE', 'F2-WC-M', 'F2-WC-F', 'F2-FAMILY'] + ['F2-EAT-%d' % i for i in range(1, 6)]
LABEL = {'F2-LINK': 'Link', 'F2-FOOD': 'FOOD WALK\n(seating)', 'F2-LANDING': 'LANDING', 'F2-TAXFREE': 'old tax free room\n(empty now)',
         'F2-WC-M': 'WC', 'F2-WC-F': 'WC', 'F2-FAMILY': 'Family'}
# the two openings: (x, z from, z to)
EAST = (56.0, 42.0, 52.0)
WEST = (14.0, 30.0, 52.0)


def plan(ax, fig):
    for rid in SHOW:
        r = ROOMS[rid]
        ax.add_patch(Rectangle((r['x0'], r['z0']), r['x1'] - r['x0'], r['z1'] - r['z0'], fc='#0d1118', ec=DIM, lw=1.4))
        t = LABEL.get(rid, rid.replace('F2-EAT-', 'Eatery '))
        ax.text((r['x0'] + r['x1']) / 2, (r['z0'] + r['z1']) / 2, t, color=INK, ha='center', va='center', fontsize=12 if 'EAT' in rid else 15)
    w = ROOMS['F2-WALK']
    ax.add_patch(Rectangle((0, w['z0']), 100, w['z1'] - w['z0'], fc='#101a24', ec=DIM, lw=1.4))
    ax.text(50, (w['z0'] + w['z1']) / 2, 'GATE WALKWAY (main corridor)', color=INK, ha='center', va='center', fontsize=17, weight='bold')
    # the walls that go (red), the new portals (cyan)
    for (x, z0, z1), name in ((EAST, 'A'), (WEST, 'B')):
        ax.plot([x, x], [z0, z1], color=CUT, lw=7, solid_capstyle='butt', alpha=.9)
        ax.add_patch(Rectangle((x - 0.6, z0), 1.2, z1 - z0, fc='none', ec=NEW, lw=3, ls='--'))
        ax.text(x + (2.5 if name == 'A' else -2.5), z1 - 1.2, 'PORTAL ' + name, color=NEW, fontsize=16, weight='bold', ha='left' if name == 'A' else 'right')
    ax.text(57.5, 43.2, 'wall goes\n(red)', color=CUT, fontsize=13, va='bottom')
    ax.text(12.5, 31.0, 'wall goes\n(red)', color=CUT, fontsize=13, ha='right', va='bottom')
    ax.annotate('glass to the walkway', (56.2, 52), (64, 56.5), color=INK, fontsize=12, arrowprops=dict(arrowstyle='->', color=INK))
    ax.set_xlim(0, 106); ax.set_ylim(70, 26); ax.set_aspect('equal'); ax.axis('off')


def sheet(name, title, sub, draw):
    fig = plt.figure(figsize=(24, 13.5), facecolor=BG)
    ax = fig.add_axes([0.03, 0.05, 0.94, 0.8]); ax.set_facecolor(BG)
    fig.text(0.03, 0.95, title, color=INK, fontsize=32, weight='bold', va='top')
    fig.text(0.03, 0.89, sub, color=DIM, fontsize=17, va='top')
    draw(ax, fig)
    fig.savefig(os.path.join(OUT, name), dpi=80, facecolor=BG); plt.close(fig); print(name)


sheet('FoodWalk-1-Plan.png', 'FLOOR 2 - FOOD WALK OPENED UP (plan, from above)',
      'Red: the walls that go. Cyan: the new entrance portals A (landing side) and B (link side), each flush wall-to-glass. Please confirm or correct.', plan)


def portal(ax, fig):
    # elevation of portal A seen from the landing: 10 m wide (z 42..52), floor to the 5.4 m ceiling (F2 ceiling 11.4 - F2 6)
    W, H = 10.0, 5.4
    ax.add_patch(Rectangle((0, 0), W, H, fc='#0d1118', ec=DIM, lw=1))
    # side frames: dark walnut pillars with a vertical warm LED line, flush with the wall end (left) and the glass (right)
    for x0 in (0, W - 0.6):
        ax.add_patch(Rectangle((x0, 0), 0.6, H, fc='#5a3a24', ec=INK, lw=1.2))
        ax.add_patch(Rectangle((x0 + 0.27, 0.15), 0.06, H - 0.3, fc=GOLD, ec='none'))
    # header: a slatted wood canopy with a backlit sign and a soft cove line under it
    ax.add_patch(Rectangle((0, H - 1.3), W, 1.3, fc='#3d2817', ec=INK, lw=1.2))
    for k in range(40): ax.plot([0.15 + k * 0.25, 0.15 + k * 0.25], [H - 1.25, H - 0.05], color='#7a5236', lw=1.5)
    ax.add_patch(FancyBboxPatch((W / 2 - 2.6, H - 1.05), 5.2, 0.8, boxstyle='round,pad=0,rounding_size=0.12', fc='#120d08', ec=GOLD, lw=2))
    ax.text(W / 2, H - 0.65, 'FOOD WALK', color=GOLD, ha='center', va='center', fontsize=30, weight='bold')
    ax.add_patch(Rectangle((0.6, H - 1.36), W - 1.2, 0.06, fc=GOLD, ec='none'))
    # planters either side, inside the opening, leaving 7 m to walk through
    for cx in (1.35, W - 1.35):
        ax.add_patch(Rectangle((cx - 0.6, 0), 1.2, 0.9, fc='#2b2b2b', ec=INK, lw=1))
        for r, dx, dy in ((0.55, 0, 1.4), (0.45, -0.35, 1.15), (0.45, 0.35, 1.15)):
            ax.add_patch(Circle((cx + dx, dy), r, fc='#3f8f4a', ec='#2c6a35', lw=1))
    # floor: the landing's tiles meeting the Food Walk wood on a brass strip under the portal
    ax.plot([0, W], [0, 0], color=GOLD, lw=4)
    ax.annotate('', (2, -0.35), (W - 2, -0.35), arrowprops=dict(arrowstyle='<->', color=INK, lw=1.3))
    ax.text(W / 2, -0.7, '7 m clear to walk through', color=INK, ha='center', fontsize=17, weight='bold')
    ax.text(0.3, -1.2, 'left: flush with the old tax free room wall end', color=DIM, fontsize=14)
    ax.text(W - 0.3, -1.2, 'right: flush with the glass to the walkway', color=DIM, fontsize=14, ha='right')
    notes = ['Walnut pillars with a warm LED line (like the tax free woods)', 'Slatted wood header, backlit FOOD WALK sign, soft cove light',
             'A brass strip in the floor where the landing meets the Food Walk', 'Two planters, 7 m clear between them',
             'Portal B the same, at the link end (FOOD WALK on both)', 'Instanced + Nanite (sign glass and LED not Nanite)']
    for k, n in enumerate(notes): ax.text(W + 1.0, H - 0.3 - k * 0.75, '-  ' + n, color=INK, fontsize=17, va='top')
    ax.set_xlim(-0.5, W + 13); ax.set_ylim(-1.6, H + 0.4); ax.set_aspect('equal'); ax.axis('off')


sheet('FoodWalk-2-Portal.png', 'THE ENTRANCE PORTAL (front view, to scale)', 'Portal A seen from the landing; portal B the same at the other end. A first idea - tell me what to change.', portal)
