# Sheet 2a: the ground floor BEFORE (above) and AFTER (below), the same scale, aligned; dimensions and the area table.
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tc_ground import *
fig = sheet('SHEET 2a · GROUND FLOOR BEFORE / AFTER',
            'Plan view, landside (forecourt) at the bottom, airside up. BEFORE above, AFTER below at the same scale so every room lines up. '
            'Yellow = new or changed, pink = non-Schengen, violet = staff, green = gates (unchanged). The far west (tax free, gates 9-12) does not change.')
for k, after in enumerate((False, True)):
    ax = fig.add_axes([0.012, 0.545 - k * 0.41, 0.976, 0.37]); ax.set_facecolor(BG)
    ground(ax, after, fs=7.5)
    ax.set_xlim(-92, 562); ax.set_ylim(-14, 84); ax.set_aspect('equal'); ax.axis('off')
    ax.text(-92, 86, 'AFTER (the proposal)' if after else 'BEFORE (today)', color=NEW if after else FG, fontsize=17, weight='bold')
    dim(ax, -80, 0, -10, '80.0 claim wing'); dim(ax, 42, 114, -10, '72.0 Grand Hall'); dim(ax, 114, 156, -10, '42.0 bag block')
    if after:
        for a, b in ((156, 228), (228, 236), (236, 312), (312, 324), (324, 372), (372, 470), (470, 551.2)): dim(ax, a, b, -10)
        dim(ax, 0, 40, 153, '40', vertical=True)
    else:
        dim(ax, 156, 551.2, -10, '395.2 east wing (empty)')
    dim(ax, 0, 52, 557, '52', vertical=True); dim(ax, 0, 30, 38, '30', vertical=True)
# the area table
ax = fig.add_axes([0.012, 0.02, 0.976, 0.105]); ax.axis('off'); ax.set_xlim(0, 100); ax.set_ylim(0, 1)
for t, x in (('AREA TABLE (m²)', 0), ('before', 22), ('after', 28)): ax.text(x, 0.98, t, color=NEW, fontsize=12.5, weight='bold', va='top')
rows = [('Grand Hall (departures, landside)', 2160, 2160, 'the same size - nothing new takes its queue zone'),
        ('Landside public halls in all', 3120, 3120 + 196 + 2880 + 1452 + 4220, 'Grand Hall + arrivals west + link + East Hall + promenade + coach station'),
        ('Claim halls (Schengen + non-Schengen)', 2674, 2674 + 3496, 'west unchanged + the new east hall'),
        ('Passport control (F1 in + F2 out)', 0, 2000, 'east: booths + e-gates + queue; F2: the glass wall before gate 13'),
        ('East wing in use', 0, 20540, 'all of it')]
for i, (n, b, a, note) in enumerate(rows):
    y = 0.78 - i * 0.18
    ax.text(0, y, n, color=FG, fontsize=11.5, va='top'); ax.text(22, y, '%d' % b, color=DIM, fontsize=11.5, va='top')
    ax.text(28, y, '%d' % a, color=OK, fontsize=11.5, va='top', weight='bold'); ax.text(34, y, note, color=DIM, fontsize=11.5, va='top')
fig.text(0.62, 0.122, 'GRAND HALL AFTER: 12 kiosks (2 x 6 between the entrances), 6 bag-drop machines at the east end of the islands, the service desk '
         'on the east wall, boarding-pass e-gates at both security doors, trolley bays at the entrances. Security: the same 6 lanes as automated (ATRS) lanes.\n'
         'BELTS RE-ROUTED (purple dashed = tunnel under the floor): the Grand Hall tunnel runs on under the bag-drop row; the East Hall\'s desks drop into a new '
         'tunnel west under x 156 that rises in the make-up hall; the east claim belts are fed from the baggage hall under the walkway (cyan), like the west ones.\n'
         'NOTHING REMOVED: every room keeps its place and size; no gate, stand or jetway moves.',
         color=FG, fontsize=11, va='top', wrap=True)
save(fig, 'TC-2a-Ground-Floor-Before-After.png')
