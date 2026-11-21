# Blueprint v2 (owner 2026-11-21: "not the rooms - only the inventory: the animals, the phones, the PC, the desk, the
# wardrobes ... anything inside, not the room itself ... do not place them into each other, relocate"): the contents of
# the ground-floor offices, Operations and the Academy (Offices.cpp) placed on the EXISTING Floor 2 office floor over the
# old staff wing (Art/TerminalAnnex/make_annex_west.py: x 114-156, z 0-52, carpet, big window onto the Grand Hall at
# x 114 z 0-30, ten windows south, six east, the staff stair x 140-143 z 43-51 and the lift x 152.4-155.8 z 47.6-51.4).
# Each item with its real footprint and a clear walkway round it; none overlaps another, a wall or the stair/lift.
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BG, FG, DIM = '#0b1118', '#e8eef5', '#8a99a8'
fig, ax = plt.subplots(figsize=(18, 20), dpi=120); fig.patch.set_facecolor(BG); ax.set_facecolor(BG)
# the existing floor (unchanged): outline, windows, stair, lift
ax.add_patch(Rectangle((114, 0), 42, 52, fc='#15191f', ec='#9fb3c4', lw=2.4))
ax.plot([114, 114], [0.35, 29.65], color='#9fe8ff', lw=5, alpha=0.8); ax.text(113.2, 15, 'big window onto the Grand Hall', color='#9fe8ff', rotation=90, ha='right', va='center', fontsize=9)
for i in range(10): ax.plot([115.6 + i * 4.08, 117.8 + i * 4.08], [0, 0], color='#9fe8ff', lw=5)
for i in range(6): ax.plot([156, 156], [3 + i * 6.5, 5.2 + i * 6.5], color='#9fe8ff', lw=5)
ax.text(135, -1.6, 'windows onto the forecourt', color='#9fe8ff', ha='center', fontsize=9)
ax.add_patch(Rectangle((140, 43), 3, 8, fc='#241a2e', ec='#c58cff')); ax.text(141.5, 47, 'staff\nstair', color='#c58cff', ha='center', va='center', fontsize=8)
ax.add_patch(Rectangle((152.4, 47.6), 3.4, 3.8, fc='#241a2e', ec='#c58cff')); ax.text(154.1, 49.5, 'lift', color='#c58cff', ha='center', va='center', fontsize=8)
def item(x0, z0, w, d, name, col, fs=8.5):
    ax.add_patch(Rectangle((x0, z0), w, d, fc=col, ec='#e8eef5', lw=1.1)); ax.text(x0 + w / 2, z0 + d / 2, name, color=FG, ha='center', va='center', fontsize=fs)
def zone(x0, z0, x1, z1, name, col):
    ax.add_patch(Rectangle((x0, z0), x1 - x0, z1 - z0, fc='none', ec=col, lw=1.5, ls='--')); ax.text(x0 + 0.4, z1 - 0.5, name, color=col, fontsize=10.5, weight='bold', va='top')
# 1. your office corner (south-west: the hall window and the forecourt windows): your desk + PC + chair, your phone on
#    it, your pet's bed beside, your wardrobe
zone(115, 1, 127, 11, 'YOUR OFFICE CORNER', '#f5b335')
item(117, 3.5, 3.2, 1.6, 'your desk\nPC + phone', '#5a4510'); item(118, 5.5, 0.8, 0.8, 'chair', '#3a3a3a', 7)
ax.add_patch(Circle((122.6, 3.0), 0.7, fc='#7a3a1a', ec=FG)); ax.text(122.6, 3.0, 'pet', color=FG, ha='center', va='center', fontsize=8)
item(125.2, 7.0, 1.2, 0.6, 'wardrobe 1', '#4a3a2a', 7)
# 2. Offices 2-4: desks along the forecourt windows, each with its wardrobe, 2 m walkways between
for k, x in enumerate((129, 137, 145)):
    zone(x - 1, 1, x + 7, 11, 'OFFICE %d' % (k + 2), '#9fb3c4')
    item(x + 1, 3.5, 3.2, 1.6, 'desk + PC', '#3a4552'); item(x + 2, 5.5, 0.8, 0.8, 'chair', '#3a3a3a', 7)
    item(x + 4.6, 7.0, 1.2, 0.6, 'wardrobe %d' % (k + 2), '#4a3a2a', 7)
# 3. Operations: the scheduling desk facing its monitor wall, by the hall window
zone(115, 14, 131, 26, 'OPERATIONS', '#7be07b')
item(116.0, 15.0, 0.6, 9.0, 'monitor\nwall', '#12401f', 8); item(119.5, 17.5, 5.0, 2.2, 'scheduling desk (3 PCs)', '#1f4a2a'); item(120.5, 20.0, 0.8, 0.8, '', '#3a3a3a'); item(122.5, 20.0, 0.8, 0.8, '', '#3a3a3a')
# 4. the Academy: 20 student desks in 4 rows of 5, the teacher's desk, the board and screen on the north side
zone(133, 14, 155, 40, 'ACADEMY', '#6fa8ff')
item(136, 37.5, 16, 0.4, 'board + screen', '#1f3a6a', 8); item(141.5, 34.5, 3.0, 1.4, "teacher's desk", '#2a3a5a')
for r in range(4):
    for c in range(5):
        item(135.0 + c * 3.8, 17.0 + r * 4.0, 2.4, 1.2, '', '#2a3550')
ax.text(144, 15.0, '20 student desks + chairs (4 rows of 5, 1.4 m aisles)', color='#6fa8ff', ha='center', fontsize=8.5)
# the walkway from the stair and lift to everything (kept clear)
ax.add_patch(Rectangle((115, 11.5), 40, 2.2, fc='#1c2a1c', ec='none', alpha=0.6)); ax.text(135, 12.6, 'clear walkway (2.2 m)', color='#7be07b', ha='center', fontsize=8.5)
ax.add_patch(Rectangle((131.5, 13.7), 1.4, 36, fc='#1c2a1c', ec='none', alpha=0.6)); ax.add_patch(Rectangle((131.5, 41.5), 23.5, 1.4, fc='#1c2a1c', ec='none', alpha=0.6))
ax.text(122, 46, 'free floor\n(lounge chairs later)', color=DIM, ha='center', fontsize=9)
ax.set_xlim(110, 159); ax.set_ylim(-4, 54); ax.set_aspect('equal')
for s in ax.spines.values(): s.set_color('#33414f')
ax.tick_params(colors=DIM, labelsize=7); ax.set_xlabel('plan x (m)', color=DIM); ax.set_ylabel('plan z (m, airside up)', color=DIM)
ax.set_title('AIRPORT DYNASTY PRO  ·  OFFICE CONTENTS MOVED TO FLOOR 2  ·  BLUEPRINT v2', color=FG, fontsize=15, weight='bold', loc='left')
fig.text(0.012, 0.012, 'Only the contents move up (no new rooms): your desk, PC, phone and pet; Offices 2-4 desks; all 4 wardrobes; the Operations desk + monitor wall; the Academy (20 desks, teacher desk, board, screen). '
         'Placed on the existing Floor 2 office floor, each with its own footprint and a clear walkway; nothing inside anything else, nothing on the stair or lift.', color=FG, fontsize=10, wrap=True)
out = os.path.join(ROOT, 'Blueprints', 'Terminal', 'F2-Offices-v2.png'); fig.savefig(out, facecolor=BG, bbox_inches='tight'); print(out)
