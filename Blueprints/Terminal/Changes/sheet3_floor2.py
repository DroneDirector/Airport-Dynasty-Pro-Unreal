# Sheet 3: Floor 2 BEFORE / AFTER - the passport control glass wall before gate 13, the non-Schengen zone, the arrivals
# corridor and the arrivals mezzanine down to passport control in.
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tc_ground import *
fig = sheet('SHEET 3 · FLOOR 2 BEFORE / AFTER',
            'Plan view at Floor 2 (+6.0 m). Only the east end changes: a passport-control glass wall across the gate walkway before gate 13, '
            'a glazed arrivals corridor for gates 13-16 and a small arrivals mezzanine with escalators down. Every gate, view bay, jetway and stand stays.')
F2C = {'walk': '#141c24', 'gate2': '#10261a', 'view': '#10261a', 'taxfree': '#1a1626', 'food': '#1a1a14', 'office': '#1a1a26'}


def floor2(ax, after, x0=-90, x1=560, fs=7.5):
    for r in PLAN['rooms']:
        if r['f'] != 2 or r['x1'] < x0 or r['x0'] > x1: continue
        rid = r['id']
        if 'GATE' in rid:
            box(ax, r['x0'], r['z0'], r['x1'], r['z1'], 'G' + rid.split('-')[-1], fc='#10261a', ec=OK, col=OK, fs=fs); continue
        if 'VIEW' in rid:
            box(ax, r['x0'], r['z0'], r['x1'], r['z1'], 'view', fc='#10261a', ec='#2a6a3a', col='#4a8a5a', fs=fs - 1); continue
        t = {'F2-WALK': '', 'F2-TAXFREE-W': 'tax free', 'F2-ESCALATOR-HALL': 'escalator\nhall'}.get(rid, r['name'].split('(')[0][:16])
        box(ax, r['x0'], r['z0'], r['x1'], r['z1'], t, fc=F2C.get(r['kind'], '#141c24'), fs=fs - 1)
    ax.text(150, 60, 'GATE WALKWAY (moving walkways), 16 m wide', color=DIM, fontsize=fs + 1, ha='center')
    box(ax, 156, 0, 551.2, 52, 'below: the east wing (no Floor 2 today)' if not after else '', fc='none', ec='#2a3440', ls=':', col='#55626e', fs=fs + 2)
    # the stands and jetways (unchanged, both versions)
    for s in PLAN['stands']:
        if x0 < s['x'] < x1 and s['kind'] == 'jetway': ax.plot([s['x'], s['x']], [82, 90], color=OK, lw=3)
    if not after: return
    # the non-Schengen zone
    box(ax, 244, 52.3, 551, 82, '', fc='none', ec=NS, lw=2.2, ls='--', z=8)
    ax.text(470, 85.5, 'NON-SCHENGEN ZONE  (gates 13-16, the wide-body stands)', color=NS, fontsize=fs + 2.5, weight='bold', ha='center')
    # passport control out: the glass wall across the walkway
    box(ax, 240, 52, 248, 68, '', fc='#2a1020', ec=NS, lw=1.8, z=9)
    for k in range(3): box(ax, 241, 53 + k * 2.6, 247, 55.2 + k * 2.6, fc='#3a1a2a', ec=NS, z=10)
    for k in range(4): box(ax, 242, 61 + k * 1.5, 246, 61.9 + k * 1.5, fc=NS, ec=NS, z=10)
    ax.plot([248, 248], [52, 68], color=GLASS, lw=3, zorder=11)
    ax.annotate('PASSPORT CONTROL OUT\n3 booths + 4 e-gates, glass wall\nto the ceiling at x 248', xy=(244, 60), xytext=(205, 92),
                color=NS, fontsize=fs + 1.5, ha='center', arrowprops=dict(arrowstyle='->', color=NS))
    # the arrivals corridor (glazed, along the walkway's south side) and the swing doors at each gate
    box(ax, 255, 52, 372, 56, '', fc='#3a1a2a', ec=NS, lw=1.2, z=9)
    ax.text(290, 50.0, 'ARRIVALS CORRIDOR 4 m, glazed\n(arrivals kept apart from departures)', color=NS, fontsize=fs, ha='center', va='top')
    for gx in (272.4, 352.2, 424.5, 496.8):
        box(ax, gx - 3, 56, gx + 3, 68, '', fc='#2a1020', ec=NS, ls='--', z=9)
    box(ax, 372, 52, 551, 56, '', fc='#2a1020', ec=NS, ls='--', lw=1, z=9)
    ax.text(461, 50.0, 'gates 14-16: arrivals lane, glass sliding screens close it off while a flight disembarks', color=NS, fontsize=fs, ha='center', va='top')
    # the arrivals mezzanine (new Floor 2 slab over the east wing) with escalators + stair + lift down
    box(ax, 330, 40, 372, 52, 'ARRIVALS MEZZANINE 42 x 12\nescalators + stair + lift down\nto passport control in', fc='#2a1020', ec=NS, col=NS, fs=fs, lw=2, z=9)
    dim(ax, 240, 248, 71, '8', fs=fs); dim(ax, 330, 372, 37, '42.0', fs=fs); dim(ax, 52, 68, 236, '16.0', vertical=True, fs=fs)
    dim(ax, 248, 255.4, 47, '7.4', fs=fs)
    ax.text(244, 75, 'moving walkway stops\nat x 236 / restarts x 252', color=DIM, fontsize=fs - 1, ha='center')


for k, after in enumerate((False, True)):
    ax = fig.add_axes([0.012, 0.50 - k * 0.43, 0.976, 0.40]); ax.set_facecolor(BG)
    floor2(ax, after)
    ax.set_xlim(-92, 562); ax.set_ylim(-4, 96); ax.set_aspect('equal'); ax.axis('off'); clip(ax)
    fig.text(0.012, 0.905 - k * 0.43, 'AFTER (the proposal)' if after else 'BEFORE (today)', color=NEW if after else FG, fontsize=18, weight='bold')
fig.text(0.012, 0.055, 'WHY (real rules): Schengen Borders Code - passengers to and from non-Schengen countries pass border control, so gates 13-16 sit behind a passport '
         'wall; EU 2015/1998 - arriving passengers from non-EU airports must not mix with screened departing passengers, hence the arrivals corridor and the '
         'sliding screens. Departing passengers for 13-16 go through the normal security, along the walkway, then passport control out. Arriving passengers '
         'from 13-16 walk the corridor to the mezzanine, down to passport control in, the east claim hall, customs and the East Hall (Sheet 2). '
         'Nothing else on Floor 2 changes; the offices, academy, tax free, food walk and every gate stay as they are.', color=FG, fontsize=12, wrap=True)
save(fig, 'TC-3-Floor-2-Before-After.png')
