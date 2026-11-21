# Blueprint v1: security before the escalators (owner 2026-11-21: "how can security lead you onto those escalators?").
# Today the Grand Hall opens straight into the escalator room (the old museum, x 0-36): its escalators rise to Floor 2,
# which is airside - so they bypass security. The fix keeps the escalators and every column: the hall's opening to the
# escalator room is closed; the escalator room is reached only from the airside lobby behind security, through a new
# airside corridor where the cleaners store and the plant room are (both move to the new east block).
import json, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
BG, FG, DIM = '#0b1118', '#e8eef5', '#8a99a8'
LAND, AIR, NEWC, RED, ESC = '#2a2210', '#10261a', '#7be07b', '#ff6b6b', '#3fd0ff'
X0, X1, Z0, Z1 = -6, 118, -9, 58
fig, ax = plt.subplots(figsize=(24, 13.5), dpi=120)
fig.patch.set_facecolor(BG); ax.set_facecolor(BG)
CHANGE = {'F1-CLEAN': 'Cleaners store\n-> moved to the east block', 'F1-PLANT': 'Plant room\n-> moved to the east block', 'F1-MUSEUM': 'ESCALATOR ROOM\n(now AIRSIDE)'}
AIRSIDE = {'F1-AIRSIDE', 'F1-LOUNGE', 'F1-MUSEUM'}
for r in P['rooms']:
    if r.get('f') != 1: continue
    x0, z0, x1, z1 = r['x0'], r['z0'], r['x1'], r['z1']
    if x1 < X0 or x0 > X1 or z0 > Z1: continue
    x0c, x1c, z1c = max(x0, X0), min(x1, X1), min(z1, Z1)
    fc = AIR if r['id'] in AIRSIDE else ('#141414' if r['id'] in ('F1-CLEAN', 'F1-PLANT') else '#17212c')
    ec = RED if r['id'] in ('F1-CLEAN', 'F1-PLANT') else '#5a6a7a'
    ax.add_patch(Rectangle((x0c, z0), x1c - x0c, z1c - z0, fc=fc, ec=ec, lw=1.4, ls='--' if r['id'] in ('F1-CLEAN', 'F1-PLANT') else '-'))
    name = CHANGE.get(r['id'], r['name'].split('(')[0].strip())
    if r['id'] in ('F1-CLEAN', 'F1-PLANT'): continue
    if x1c - x0c > 5 and r['id'] not in ('F1-BAGHALL',):
        ax.text((x0c + x1c) / 2, (z0 + z1c) / 2 + (8 if r['id'] == 'F1-MUSEUM' else 0), name, color=ESC if r['id'] == 'F1-MUSEUM' else DIM, ha='center', va='center', fontsize=11, weight='bold' if r['id'] == 'F1-MUSEUM' else 'normal')
# the columns: kept, every one
for c in P['columns']:
    ax.add_patch(Rectangle((c[1] - 0.45, c[2] - 0.45), 0.9, 0.9, fc='#cfd8e0', ec='none'))
ax.text(46, 3.3, 'columns kept', color='#cfd8e0', fontsize=8.5, ha='center')
# the escalators (kept): two pairs, foot at the hall side (x ~34), top at the Floor 2 landing edge (x 21)
for zc in (12.33, 16.05):
    for d in (-0.81, 0.81):
        ax.add_patch(Rectangle((21, zc + d - 0.8), 13, 1.6, fc='#0f2a36', ec=ESC, lw=1.2))
    ax.annotate('', xy=(21.5, zc), xytext=(33, zc), arrowprops=dict(arrowstyle='->', color=ESC, lw=1.6))
ax.text(27.5, 20.2, 'ESCALATORS (kept)  up to Floor 2 -> the gates', color=ESC, ha='center', fontsize=10)
# 1. the hall's opening into the escalator room: closed (wall)
ax.plot([36, 36], [11, 19], color=RED, lw=5)
ax.text(37.2, 23.5, '1  opening CLOSED\n   (a glass wall: you still\n   see the escalators)', color=RED, fontsize=10)
# 2. the new airside corridor: airside lobby -> west through the old cleaners store and plant room -> south into the escalator room
ax.add_patch(Rectangle((4, 40), 48, 8, fc='#0f3a1a', ec=NEWC, lw=2.2))
ax.add_patch(Rectangle((4, 30), 12, 10, fc='#0f3a1a', ec=NEWC, lw=2.2))
ax.text(28, 44, '2  NEW AIRSIDE CORRIDOR (8 m wide, seats, flight screens)', color=NEWC, ha='center', va='center', fontsize=10.5, weight='bold')
ax.text(10, 35, 'opening\ninto the\nescalator\nroom', color=NEWC, ha='center', va='center', fontsize=8.5)
ax.add_patch(Rectangle((16, 30), 20, 10, fc='#101a14', ec='#4a6a52', lw=1))
ax.text(26, 35, 'staff / service room\n(lift machine, cleaners\' cupboard)', color=DIM, ha='center', va='center', fontsize=8.5)
ax.add_patch(Rectangle((4, 51.5), 48, 0.5, fc=DIM, ec='none'))
# 3. the passenger flow: entrances -> check-in -> security -> airside lobby -> corridor -> escalators -> Floor 2
flow = [(75, -3), (75, 10), (70, 28), (70, 36), (70, 44), (50, 44), (10, 44), (10, 32), (14, 22), (30, 14.2)]
for a, b in zip(flow[:-1], flow[1:]):
    ax.add_patch(FancyArrowPatch(a, b, arrowstyle='-|>', mutation_scale=18, color='#ffd166', lw=2.4))
ax.text(76.5, 5, '3  passengers: entrance -> check-in', color='#ffd166', fontsize=10)
ax.text(72, 33.5, 'SECURITY (6 lanes) - the only way airside', color='#ffd166', fontsize=10, weight='bold')
ax.text(72, 48.3, 'airside lobby: stairs + lift up (kept)', color='#ffd166', fontsize=9)
ax.text(30, 38.2, '-> west to the escalators', color='#ffd166', fontsize=9)
# legend
for i, (c, t) in enumerate(((LAND, 'landside (before security)'), (AIR, 'airside (after security)'), ('#0f3a1a', 'new'), ('#141414', 'removed / moved'))):
    ax.add_patch(Rectangle((84, 52 + 0 - i * 0.0, ), 0, 0))
    ax.add_patch(Rectangle((60 + i * 14.5, 54), 2.5, 2.2, fc=c, ec=DIM)); ax.text(63 + i * 14.5, 55.1, t, color=FG, fontsize=8.5, va='center')
ax.set_xlim(X0, X1); ax.set_ylim(Z0, Z1); ax.set_aspect('equal')
for s in ax.spines.values(): s.set_color('#33414f')
ax.tick_params(colors=DIM, labelsize=7)
ax.set_xlabel('plan x (m)', color=DIM); ax.set_ylabel('plan z (m, airside up)', color=DIM)
ax.set_title('AIRPORT DYNASTY PRO  ·  SECURITY BEFORE THE ESCALATORS  ·  BLUEPRINT v1  ·  GROUND FLOOR (west part)', color=FG, fontsize=15, weight='bold', loc='left')
fig.text(0.012, 0.01, 'Today the Grand Hall opens straight into the escalator room and the escalators reach Floor 2 (airside) without security. Fix: that opening becomes a glass wall; '
         'the escalator room is reached only from behind security (airside lobby -> new corridor through the old cleaners store + plant room, both moved to the new east block). '
         'Escalators, stairs, lift, security lanes and every column stay. Toilets stay landside (before security).', color=FG, fontsize=9.5, wrap=True)
out = os.path.join(ROOT, 'Blueprints', 'Terminal', 'Security-Escalators-v1.png')
fig.savefig(out, facecolor=BG, bbox_inches='tight'); print(out)
