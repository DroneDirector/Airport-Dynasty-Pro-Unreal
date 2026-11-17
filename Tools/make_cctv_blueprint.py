# CCTV plan (owner, 2026-11-17): where the terminal's cameras go on Floor 1 and Floor 2, as real airports place them -
# every way in and out, check-in and bag drop, the security lanes and the airside door, every stair, escalator and lift,
# the gate walkway, each gate room and its door to the jetway or the apron, the baggage claim and customs, the crew
# side's baggage hall and ramp doors, the staff entrance, the tax free (its doors, pay lounge, cellar, tobacco room,
# boutiques). Each camera: a number, its spot and the way it looks (a view cone), and a list with what it watches.
# Big text for the phone; black background (documents rule). Drawing only - nothing in the game yet.
#   python Tools/make_cctv_blueprint.py   ->  docs/Blueprints/CCTV-*.png
import json, os, math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Wedge, Circle
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'Blueprints'); os.makedirs(OUT, exist_ok=True)
P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
BG, INK, DIM, CAM, CONE, NEWC = '#000000', '#e8edf2', '#5b6773', '#ff4f6d', '#ff4f6d', '#35d0e0'
OLD = {'F1-MUSEUM', 'F1-PLANT', 'F2-WC-M', 'F2-WC-F', 'F2-FAMILY'}
TODAY = {1: [('Escalator room', 0, 0, 36, 30), ('Plant room', 6, 30, 36, 52), ('BAGGAGE CLAIM', -80, 16, 0, 52),
             ('Toilets', -80, 8, -56, 16), ('Customs', -56, 8, -24, 16), ('Customs office', -24, 8, 0, 16), ('ARRIVALS HALL', -80, -4, 0, 8)],
         2: [('Landing', 0, 0, 21, 30), ('WC', 0, 0, 14, 5), ('WC', 0, 5, 14, 10), ('Family', 0, 10, 14, 22)]}

# (floor, x, z, looking (degrees: 0 east, 90 north/airside, -90 south/kerb), what it watches)
C = []
def cam(f, x, z, a, what): C.append((f, x, z, a, what))
# ---- Floor 1: landside, check-in, security ----
for x in (50, 75, 101): cam(1, x, 1.0, 90, 'Entrance %s - people coming in' % {50: 'West', 75: 'Main', 101: 'East'}[x])
cam(1, 40, 28, -30, 'Grand Hall west: check-in desks'); cam(1, 110, 28, -150, 'Grand Hall east: bag drop and kiosks')
cam(1, 75, 29, -90, 'Grand Hall middle: the whole hall')
cam(1, 52, 31, 0, 'Security: the queue into the lanes'); cam(1, 88, 41, 180, 'Security: the lanes and trays')
cam(1, 70, 51, -90, 'Airside lobby: past security (no way back)'); cam(1, 60, 43, 45, 'Airside stairs and escalator to Floor 2')
cam(1, 18, 2, 90, 'Escalator room (from the arrivals side)')
cam(1, 112, 50, -135, 'Lounge entrance'); cam(1, 154, 50, -135, 'Staff entrance (badge door)')
cam(1, 134, 2, 90, 'Staff corridor')
# ---- Floor 1: arrivals ----
cam(1, -78, 50, -45, 'Baggage claim: escalators down from Floor 2'); cam(1, -78, 18, 30, 'Baggage claim: belts 1-2')
cam(1, -2, 18, 150, 'Baggage claim: belts 4-5'); cam(1, -40, 50, -90, 'Baggage claim: belt 3 and the whole hall')
cam(1, -40, 15, -90, 'Customs: green and red channels'); cam(1, -2, -2, 150, 'Arrivals hall: meeting rail and exit doors')
cam(1, -78, -2, 30, 'Arrivals hall west')
# ---- Floor 1: the crew side (baggage hall under the walkway, ramp doors) ----
for x in range(-230, 551, 60): cam(1, x, 53, 0, 'Baggage hall (sorting belts) at x %d' % x)
for r in P['rooms']:
    if r.get('f') == 1 and r['id'].startswith(('F1-RAMP-', 'F1-GATE-')) and not r['id'].endswith('B'):
        cam(1, r['x0'] + 1.0, 81, -45, '%s: door to the apron' % r['name'].split(' (')[0])
cam(1, -239, 10, 45, 'Under the tax free (empty for now): its door')
cam(1, 158, 2, 45, 'East wing (empty for now)')
# ---- Floor 2: walkway, landing, food, bar ----
cam(2, 58, 51, 0, 'Floor 2 landing: top of the stairs and escalator'); cam(2, 15, 31, 45, 'Food walk'); cam(2, 145, 31, 135, 'The bar')
cam(2, 6.5, 50, 90, 'Link to the gate walkway')
for x in range(-235, 551, 50): cam(2, x, 53, 0, 'Gate walkway at x %d (moving walkways)' % x)
for r in P['rooms']:
    if r.get('f') == 2 and r['id'].startswith('F2-GATE-'):
        n = r['id'].split('-')[-1]
        cam(2, r['x0'] + 1.0, 81, -45, 'Gate %s: the gate desk and the door to the jetway' % n)
cam(2, -78, 50, -90, 'Arrivals corridor: way down to baggage claim')
# ---- Floor 2: the tax free ----
cam(2, -93, 50, -135, 'Tax free: way IN'); cam(2, -226, 50, -45, 'Tax free: way OUT to gates 9-12')
cam(2, -235, 13, 45, 'Tax free: pay lounge (12 tills)'); cam(2, -210, 13, 120, 'Tax free: self-checkout kiosks')
cam(2, -140, 50, -90, 'Tax free: liquor and the wine cellar'); cam(2, -186, 13, 135, 'Tax free: tobacco room')
cam(2, -168, 20, 90, 'Tax free: aurora plaza bar'); cam(2, -82, 13, 135, 'Tax free: beauty')
for k, x in enumerate((-225.7, -217.2, -208.6, -199.9)): cam(2, x, 21, -90, 'Tax free: boutique %d (watches, jewellery)' % (k + 1))

def floor_sheet(f, half, xl):
    fig = plt.figure(figsize=(24, 9.2), facecolor=BG); ax = fig.add_axes([0.035, 0.07, 0.94, 0.83])
    ax.set_facecolor(BG); ax.set_aspect('equal'); ax.set_xlim(*xl); ax.set_ylim(-12, 90)
    for sp in ax.spines.values(): sp.set_color(DIM)
    ax.tick_params(colors=DIM, labelsize=11)
    ax.set_title('CCTV - FLOOR %d, %s HALF   (red dot = camera, cone = what it sees, number = list)' % (f, half.upper()), color=INK, fontsize=20, loc='left')
    for r in P['rooms']:
        if r['id'] in OLD or r.get('f') != f: continue
        if r['id'] in ('F1-HALL',) and False: continue
        ax.add_patch(Rectangle((r['x0'], r['z0']), r['x1'] - r['x0'], r['z1'] - r['z0'], fill=False, ec=DIM, lw=0.8))
        w = r['x1'] - r['x0']
        if w > 18 and r['z1'] - r['z0'] > 8 and xl[0] < (r['x0'] + r['x1']) / 2 < xl[1]:
            nm = r['name'].split(' (')[0]
            ax.text((r['x0'] + r['x1']) / 2, (r['z0'] + r['z1']) / 2, nm, color='#8d99a6', fontsize=11 if w > 30 else 9, ha='center', va='center')
    for nm, x0, z0, x1, z1 in TODAY[f]:
        ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fill=False, ec=DIM, lw=0.8))
        if x1 - x0 > 18 and xl[0] < (x0 + x1) / 2 < xl[1]: ax.text((x0 + x1) / 2, (z0 + z1) / 2, nm, color='#8d99a6', fontsize=11, ha='center', va='center')
    n = 0
    for i, (cf, x, z, a, what) in enumerate(C):
        if cf != f or not (xl[0] <= x <= xl[1]): continue
        ax.add_patch(Wedge((x, z), 9.0, a - 30, a + 30, fc=CONE, ec='none', alpha=0.18, zorder=3))
        ax.add_patch(Circle((x, z), 1.1, fc=CAM, ec='white', lw=0.6, zorder=5))
        ax.text(x + 1.6, z + 1.6, str(i + 1), color=INK, fontsize=12, fontweight='bold', zorder=6)
        n += 1
    ax.text(xl[0] + 3, -9, 'KERB (landside)', color='#8d99a6', fontsize=12); ax.text(xl[0] + 3, 86, 'APRON (airside)', color='#8d99a6', fontsize=12)
    fig.savefig(os.path.join(OUT, 'CCTV-Floor-%d-%s.png' % (f, half)), dpi=150, facecolor=BG); plt.close(fig)
    return n

halves = [('West', (-245, 30)), ('Middle', (25, 300)), ('East', (295, 556))]
counts = {}
for f in (1, 2):
    for h, xl in halves: counts[(f, h)] = floor_sheet(f, h, xl)
# the list
for part, rng in (('1', range(0, (len(C) + 1) // 2)), ('2', range((len(C) + 1) // 2, len(C)))):
    fig = plt.figure(figsize=(24, 13.5), facecolor=BG); ax = fig.add_axes([0.02, 0.02, 0.96, 0.94]); ax.axis('off')
    ax.text(0, 1.0, 'CCTV CAMERAS - LIST %s of 2   (%d cameras: Floor 1 %d, Floor 2 %d)' % (part, len(C), sum(1 for c in C if c[0] == 1), sum(1 for c in C if c[0] == 2)),
            color=NEWC, fontsize=22, fontweight='bold', va='top', transform=ax.transAxes)
    items = list(rng); col = (len(items) + 1) // 2
    for k, i in enumerate(items):
        f, x, z, a, what = C[i]
        cx, cy = (0.0 if k < col else 0.5), 0.94 - (k % col) * (0.92 / col)
        ax.text(cx, cy, '%3d   F%d   %s' % (i + 1, f, what), color=INK, fontsize=15, va='top', family='monospace', transform=ax.transAxes)
    fig.savefig(os.path.join(OUT, 'CCTV-List-%s.png' % part), dpi=150, facecolor=BG); plt.close(fig)
print('cameras', len(C), counts)
