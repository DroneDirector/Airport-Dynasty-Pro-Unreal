# Vending machines blueprint (to-do 73a, owner 2026-11-17): five different kinds for the terminal, all lit, no real brands
# (our own made-up names and graphics only), with the big pop-up keypad UI for buying (73c). Sizes from real machines
# (dimensions.com: beverage 200.7 x 94 x 85.1 cm, large snack 194 x 103.6 x 79 cm; combo ~183 x 99 x 84-89 cm).
# Drawing only - nothing modelled yet; for his OK and corrections first (blueprint-before-build rule).
# Big text for the phone; black background (documents rule).
#   python Tools/make_vending_blueprint.py   ->  docs/Blueprints/Vending-*.png
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle, Polygon
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'Blueprints'); os.makedirs(OUT, exist_ok=True)
BG, INK, DIM = '#000000', '#e8edf2', '#7d8a96'

# (file key, made-up name, what it sells, W, H, D cm, glow colour, kind, notes)
M = [
    ('Drinks', 'FROSTLINE', 'Cold drinks: cans, bottles, water, juice', 94, 200, 85, '#35f0ff', 'glass',
     ['Glass front, 6 lit shelves of cans and bottles', 'Cold (3-5 C): a light mist on the inside of the glass',
      'Cyan LED edge round the glass, lit top sign', 'Keypad panel right, coin/card slot, delivery flap below']),
    ('Snacks', 'NIBBLE BOX', 'Snacks: crisps, chocolate, sweets, nuts', 104, 194, 79, '#ff9a3c', 'glass',
     ['Glass front, 6 trays of spiral coils (8 per tray)', 'The coil turns once and the item drops',
      'Warm orange LED strips above each tray', 'Keypad panel right, delivery flap below']),
    ('Coffee', 'AURORA BREW', 'Hot drinks: coffee, tea, hot chocolate', 70, 183, 65, '#b48cff', 'solid',
     ['No glass: a big lit picture front (our own aurora + cup art)', 'Cup bay with its own light and a little steam',
      'Touch buttons for sugar / milk / strength', 'Takes 20-30 s: grinding and pouring sounds (Freesound CC0)']),
    ('Fresh', 'FJORD FRESH', 'Fresh food: sandwiches, salads, fruit, yoghurt', 99, 183, 86, '#5cff9d', 'glass',
     ['Glass front, cold, 5 shelves on a turning carousel', 'Green-white light, fresh look',
      'Each item has a little price tag on the shelf', 'Keypad panel right, big delivery door below']),
    ('Travel', 'GATE GEAR', 'Travel essentials: chargers, earphones, neck pillows, masks', 104, 194, 79, '#ff3fb8', 'glass',
     ['Glass front, 5 trays of spiral coils (bigger coils)', 'Pink neon edge (like the tax free)',
      'Items in clear boxes with our own made-up labels', 'Keypad panel right, delivery flap below']),
]


def machine(ax, x0, m, side=False):
    key, name, sells, W, H, D, glow, kind, notes = m
    w = D if side else W
    # glow halo
    for k, a in ((14, .05), (9, .08), (5, .12)):
        ax.add_patch(FancyBboxPatch((x0 - k, -k / 2), w + 2 * k, H + k, boxstyle='round,pad=0,rounding_size=6', fc=glow, ec='none', alpha=a))
    ax.add_patch(Rectangle((x0, 0), w, H, fc='#0c0f18', ec=INK, lw=2.2))
    if side:
        ax.add_patch(Rectangle((x0 + w - 6, 8), 6, H - 16, fc=glow, ec='none', alpha=.5))
        ax.text(x0 + w / 2, H / 2, 'SIDE', color=DIM, ha='center', va='center', fontsize=16, rotation=90, weight='bold')
        return
    # top sign
    ax.add_patch(Rectangle((x0 + 6, H - 26), W - 12, 20, fc=glow, ec='none', alpha=.85))
    ax.text(x0 + W / 2, H - 16, name, color='#000000', ha='center', va='center', fontsize=15 if W > 80 else 12, weight='bold')
    pw = 24   # the keypad panel on the right
    if kind == 'glass':
        gx0, gx1, gy0, gy1 = x0 + 6, x0 + W - pw - 6, 48, H - 32
        ax.add_patch(Rectangle((gx0, gy0), gx1 - gx0, gy1 - gy0, fc='#13202c', ec=glow, lw=3))
        rows = 6 if W > 95 else 5
        if key == 'Fresh': rows = 5
        for r in range(rows):
            y = gy0 + 6 + r * (gy1 - gy0 - 8) / rows
            ax.plot([gx0 + 3, gx1 - 3], [y, y], color=glow, lw=1.5, alpha=.8)
            n = 8 if key in ('Snacks',) else 5 if key in ('Travel', 'Fresh') else 7
            cw = (gx1 - gx0 - 8) / n
            for c in range(n):
                cx = gx0 + 4 + c * cw
                if key in ('Snacks', 'Travel'):   # a coil and its item
                    ax.add_patch(Rectangle((cx + 2, y + 3), cw - 4, 13, fc='#2a3442', ec=DIM, lw=.6))
                    for t in range(3): ax.plot([cx + 3 + t * (cw - 6) / 3, cx + 3 + (t + .5) * (cw - 6) / 3], [y + 1, y + 2.5], color=DIM, lw=.8)
                elif key == 'Drinks':
                    ax.add_patch(FancyBboxPatch((cx + 3, y + 2), cw - 6, 18, boxstyle='round,pad=0,rounding_size=2', fc='#3b4b5e', ec=DIM, lw=.6))
                else:
                    ax.add_patch(Polygon([(cx + 2, y + 2), (cx + cw - 2, y + 2), (cx + cw / 2, y + 14)], fc='#3d5a48', ec=DIM, lw=.6))
                ax.text(cx + cw / 2, y - 3.5, '%s%d' % ('ABCDEF'[r], c + 1), color=INK, ha='center', va='center', fontsize=5.5)
    else:
        ax.add_patch(Rectangle((x0 + 6, 70), W - pw - 12, H - 102, fc='#1a1238', ec=glow, lw=3))
        ax.text(x0 + (W - pw) / 2, H - 70, 'AURORA\nART', color=glow, ha='center', va='center', fontsize=11, weight='bold')
        ax.add_patch(Rectangle((x0 + 12, 30), W - pw - 24, 34, fc='#05050a', ec=glow, lw=2))
        ax.text(x0 + (W - pw) / 2, 47, 'CUP BAY', color=INK, ha='center', va='center', fontsize=9)
    # keypad panel
    kx = x0 + W - pw - 2
    ax.add_patch(Rectangle((kx, 70), pw - 2, 70, fc='#1b2230', ec=INK, lw=1.2))
    ax.add_patch(Rectangle((kx + 3, 124), pw - 8, 10, fc=glow, ec='none', alpha=.7))   # little screen
    for r in range(4):
        for c in range(3):
            ax.add_patch(Rectangle((kx + 3 + c * 5.5, 82 + r * 9), 4.5, 6.5, fc='#cfd6de', ec='none'))
    ax.add_patch(Rectangle((kx + 4, 72), pw - 10, 5, fc='#000000', ec=DIM, lw=.8))       # card / coin slot
    # delivery flap
    if kind == 'glass':
        ax.add_patch(Rectangle((x0 + 10, 12), W - pw - 20, 26, fc='#05050a', ec=INK, lw=1.5))
        ax.text(x0 + (W - pw) / 2, 25, 'TAKE HERE', color=DIM, ha='center', va='center', fontsize=8)


def dims(ax, x0, w, h, d=None):
    ax.annotate('', (x0, -14), (x0 + w, -14), arrowprops=dict(arrowstyle='<->', color=INK, lw=1.3))
    ax.text(x0 + w / 2, -24, '%d cm' % w, color=INK, ha='center', va='center', fontsize=15, weight='bold')
    ax.annotate('', (x0 - 14, 0), (x0 - 14, h), arrowprops=dict(arrowstyle='<->', color=INK, lw=1.3))
    ax.text(x0 - 22, h / 2, '%d cm' % h, color=INK, ha='center', va='center', fontsize=15, weight='bold', rotation=90)


def sheet(name, title, sub, draw, w=24, h=13.5):
    fig = plt.figure(figsize=(w, h), facecolor=BG)
    ax = fig.add_axes([0.03, 0.06, 0.94, 0.78]); ax.set_facecolor(BG); ax.set_aspect('equal'); ax.axis('off')
    fig.text(0.03, 0.95, title, color=INK, fontsize=34, weight='bold', va='top')
    fig.text(0.03, 0.885, sub, color=DIM, fontsize=17, va='top')
    draw(ax, fig)
    fig.savefig(os.path.join(OUT, name), dpi=80, facecolor=BG)
    plt.close(fig)
    print(name)


def all_five(ax, fig):
    x = 0
    for m in M:
        machine(ax, x, m); dims(ax, x, m[3], m[4])
        ax.text(x + m[3] / 2, -46, m[1], color=m[6], ha='center', va='center', fontsize=19, weight='bold')
        ax.text(x + m[3] / 2, -62, m[2].split(':')[0], color=INK, ha='center', va='center', fontsize=15)
        x += m[3] + 70
    ax.set_xlim(-40, x - 30); ax.set_ylim(-75, 215)


sheet('Vending-1-All-Five.png', 'VENDING MACHINES - THE FIVE KINDS (front, to scale)',
      'Our own made-up names and graphics, no real brands. All lit. Keypad on the right of each; E / X opens the big pop-up keypad.', all_five)

for i, m in enumerate(M, 2):
    def one(ax, fig, m=m):
        machine(ax, 0, m); dims(ax, 0, m[3], m[4])
        machine(ax, m[3] + 70, m, side=True)
        ax.annotate('', (m[3] + 70, -14), (m[3] + 70 + m[5], -14), arrowprops=dict(arrowstyle='<->', color=INK, lw=1.3))
        ax.text(m[3] + 70 + m[5] / 2, -24, '%d cm deep' % m[5], color=INK, ha='center', va='center', fontsize=15, weight='bold')
        tx = m[3] + m[5] + 130
        ax.text(tx, 190, 'SELLS', color=m[6], fontsize=19, weight='bold', va='top')
        ax.text(tx, 176, m[2].split(': ')[-1], color=INK, fontsize=17, va='top')
        ax.text(tx, 150, 'HOW IT LOOKS AND WORKS', color=m[6], fontsize=19, weight='bold', va='top')
        for k, n in enumerate(m[8]):
            ax.text(tx, 136 - k * 15, '-  ' + n, color=INK, fontsize=17, va='top')
        ax.text(tx, 62, 'IN THE GAME', color=m[6], fontsize=19, weight='bold', va='top')
        for k, n in enumerate(['Instanced + Nanite (glass and lights not Nanite)', 'Lit at night: its glow lights the floor round it',
                               'Walk up, look through the glass at what is inside', 'E / X at the keypad: the big pop-up keypad (sheet 7)']):
            ax.text(tx, 48 - k * 15, '-  ' + n, color=INK, fontsize=17, va='top')
        ax.set_xlim(-40, tx + 330); ax.set_ylim(-40, 215)
    sheet('Vending-%d-%s.png' % (i, m[0]), '%d.  %s  -  %s' % (i - 1, m[1], m[2].split(':')[0].upper()),
          'Front and side, to scale (cm). Made-up name and art.', one)


def keypad(ax, fig):
    # the pop-up: dark glass panel, neon edges, the big keys, the product shown, the price, pay
    ax.add_patch(FancyBboxPatch((0, 0), 560, 300, boxstyle='round,pad=0,rounding_size=14', fc='#0b0820', ec='#35f0ff', lw=3))
    ax.text(24, 280, 'NIBBLE BOX  ·  choose your snack', color='#35f0ff', fontsize=20, weight='bold', va='top')
    # display
    ax.add_patch(FancyBboxPatch((24, 196), 250, 60, boxstyle='round,pad=0,rounding_size=8', fc='#03020a', ec='#ff3fb8', lw=2))
    ax.text(40, 226, 'B4', color='#ffffff', fontsize=34, weight='bold', va='center')
    ax.text(110, 236, 'Sea-salt crisps', color=INK, fontsize=15, va='center')
    ax.text(110, 214, '2.50', color='#ff3fb8', fontsize=19, weight='bold', va='center')
    # keys
    labels = ['A', 'B', 'C', '1', '2', '3', 'D', 'E', 'F', '4', '5', '6', 'CLEAR', '0', 'OK', '7', '8', '9']
    for k, l in enumerate(labels):
        r, c = divmod(k, 6)
        x, y = 24 + c * 42, 140 - r * 46
        ok = l == 'OK'
        ax.add_patch(FancyBboxPatch((x, y), 36, 38, boxstyle='round,pad=0,rounding_size=6', fc='#ff3fb8' if ok else '#1a1640', ec='#35f0ff', lw=1.5))
        ax.text(x + 18, y + 19, l, color='#000000' if ok else '#ffffff', fontsize=13 if len(l) > 1 else 19, weight='bold', ha='center', va='center')
    # the product, big, on the right
    ax.add_patch(FancyBboxPatch((300, 100), 236, 156, boxstyle='round,pad=0,rounding_size=10', fc='#13202c', ec='#ff9a3c', lw=2))
    ax.text(418, 178, '[ the item, in 3D,\n  turning slowly ]', color=INK, fontsize=15, ha='center', va='center')
    ax.add_patch(FancyBboxPatch((300, 34), 236, 50, boxstyle='round,pad=0,rounding_size=10', fc='#35f0ff', ec='none'))
    ax.text(418, 59, 'PAY 2.50', color='#000000', fontsize=22, weight='bold', ha='center', va='center')
    ax.text(24, -20, 'Mouse: click the keys.  Keyboard: type the letter and number.  Pad: D-pad moves, X presses, B backs out.',
            color=DIM, fontsize=15, va='top')
    steps = ['1  Walk up, E / X at the keypad: the pop-up opens (game blurred behind)',
             '2  Press a letter and a number (B4): the item lights up in the glass and shows here',
             '3  OK, then PAY: the money comes off your wallet',
             '4  The coil turns (or the arm moves), the item drops - sound and animation',
             '5  The pop-up closes; E / X at the flap: take it out']
    for k, s in enumerate(steps):
        ax.text(600, 280 - k * 34, s, color=INK, fontsize=17, va='top')
    ax.text(600, 100, 'EXACT FLOW TO BE AGREED WITH YOU FIRST', color='#ff3fb8', fontsize=17, weight='bold', va='top')
    ax.set_xlim(-10, 1300); ax.set_ylim(-50, 310)


sheet('Vending-7-Keypad-UI.png', '7.  THE BIG POP-UP KEYPAD (buying)', 'The machine keypad is too small to hit, so E / X opens this. Neon style like the title screen.', keypad)
