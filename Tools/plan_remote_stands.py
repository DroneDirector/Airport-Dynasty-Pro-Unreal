# The remote stands proposal (owner 2026-11-23: stands "behind the gates", Billund-style, for the apron buses): a plan of
# the terminal apron from Data/airfield.json with the proposed remote apron, stands R1-R5, the bus road, the bus depot
# with chargers, the bus gate and the bus route drawn on it. Writes Blueprints/Airfield/RemoteStands-Proposal.png.
#   python3 Tools/plan_remote_stands.py
import json, os, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Blueprints', 'Airfield'); os.makedirs(OUT, exist_ok=True)
d = json.load(open(os.path.join(ROOT, 'Data', 'airfield.json'), encoding='utf-8'))
fig, ax = plt.subplots(figsize=(19.2, 11.5), dpi=200); fig.patch.set_facecolor('#081426'); ax.set_facecolor('#081426')
C = dict(ink='#e8f0ff', sub='#96afd2', acc='#ffc446', grn='#6ee68c', red='#ff6b6b', blue='#7fc4ff', twy='#2a4a75')
def R(x0, y0, x1, y1, **k): ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, **k))
# #region ── What is there today ──
R(-241, 30, 551, 82, fc='#33507a', ec=C['ink'], lw=1); ax.text(155, 55, 'TERMINAL', color=C['ink'], ha='center', va='center', fontsize=16, weight='bold')
R(-265, 82, 570, 238, fc='#173257', ec=C['sub'], lw=0.8)
for t in d['taxiways']:
    if t['id'] in ('A', 'T', 'P1', 'P2', 'P3'):
        (x0, y0), (x1, y1) = t['pts'][0], t['pts'][-1]
        if t['id'] == 'A': x0, x1 = -330, 620
        if x0 == x1: R(x0 - 12.5, min(y0, y1), x0 + 12.5, max(y0, y1), fc=C['twy'], ec='none')
        else: R(min(x0, x1), y0 - 12.5, max(x0, x1), y0 + 12.5, fc=C['twy'], ec='none')
        ax.plot([x0, x1], [y0, y1], color=C['acc'], lw=1.2)
ax.text(625, 386, 'Taxiway A', color=C['acc'], fontsize=12, va='center'); ax.text(575, 218, 'Taxilane T', color=C['acc'], fontsize=12, va='center')
for n, x in (('P1', -130), ('P2', 150), ('P3', 362)): ax.text(x + 16, 360, n, color=C['acc'], fontsize=12, weight='bold')
for s in d['stands']:
    if s.get('cargo'): continue
    x, n, sp, L = s['x'], s['nose'], s['span'], s['len']
    R(x - sp / 2, n, x + sp / 2, n + L, fc='none', ec=C['blue'], lw=0.8); ax.plot([x, x], [n, n + L], color=C['blue'], lw=0.6)
    if s['jetway']: R(x + 3, 82, x + 16, n + 9, fc=C['red'], alpha=0.35, ec='none')
    ax.text(x, n + L / 2, s['id'].split('-')[1], color=C['ink'], fontsize=10, ha='center', va='center', weight='bold')
ax.plot([-255, 560], [176, 176], color=C['sub'], lw=2, ls='--'); ax.text(-200, 181, 'service lane (behind the tails)', color=C['sub'], fontsize=10)
# #endregion
# #region ── The proposal ──
R(-255, 236, 106, 330, fc=C['grn'], alpha=0.10, ec=C['grn'], lw=1.5, ls='--')
for i, x in enumerate((-86, -45.1, -4.3, 36.6, 77.4)):          # code C, nose north, pushed back onto taxilane T
    R(x - 18, 272, x + 18, 316.5, fc='none', ec=C['grn'], lw=1.6); ax.plot([x, x], [240, 316.5], color=C['acc'], lw=0.8)
    ax.text(x, 288, f'R{i + 1}', color=C['grn'], fontsize=13, ha='center', va='center', weight='bold')
    R(x - 9.5, 300, x - 6.5, 314, fc=C['acc'], ec='none')           # the bus by the forward left door
ax.plot([-245, 106], [323, 323], color=C['grn'], lw=2.2); ax.text(-60, 327, 'remote service road (bus road)', color=C['grn'], fontsize=10)
R(-235, 246, -158, 304, fc='#1e3f2a', ec=C['grn'], lw=1.2)
for k in range(6):
    xb = -229 + k * 12; R(xb, 252, xb + 3, 266, fc=C['acc'] if k < 3 else 'none', ec=C['acc'], lw=0.8); ax.plot([xb + 1.5], [249.5], 's', color=C['blue'], ms=4)
ax.text(-300, 280, 'BUS DEPOT\n6 charging bays\n(3 buses + 3 spare)\n+ the GSE park', color=C['ink'], fontsize=10, ha='right', va='center')
ax.plot([-298, -236], [280, 280], color=C['ink'], lw=0.8)
R(-290, 82, -265, 176, fc=C['grn'], alpha=0.18, ec=C['grn'], lw=1.2, ls='--')
R(-262, 90, -259, 104, fc=C['acc'], ec='none'); R(-262, 110, -259, 124, fc=C['acc'], ec='none')
ax.text(-298, 129, 'BUS GATE\nnew ground-floor\nbus lounge door,\n2 bus bays,\napron 25 m wider', color=C['ink'], fontsize=10, ha='right', va='center')
ax.plot([-260, -260, -245, -245, 77], [125, 176, 176, 323, 323], color=C['acc'], lw=2.6)
ax.text(-250, 212, 'marked crossing\nof taxilane T', color=C['acc'], fontsize=9, ha='right', va='center')
# #endregion
ax.set_xlim(-430, 700); ax.set_ylim(20, 470); ax.set_aspect('equal'); ax.axis('off')
ax.text(-425, 462, 'REMOTE STANDS PROPOSAL  ·  behind the gates, Billund-style (stands on both sides of the apron taxilane)', color=C['ink'], fontsize=17, weight='bold', va='top')
ax.text(-425, 440, 'green = new  ·  red = jetway zones (no vehicles)  ·  yellow line = the apron bus route (never under a jetway or across its markings)  ·  metres, north up', color=C['sub'], fontsize=11, va='top')
plt.savefig(os.path.join(OUT, 'RemoteStands-Proposal.png'), facecolor=fig.get_facecolor(), bbox_inches='tight')
print('plan written')
