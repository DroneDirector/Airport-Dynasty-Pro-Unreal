# Sheet 2b: the Grand Hall, the link gallery and the East Hall, BEFORE | AFTER side by side, with the new pieces' sizes.
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tc_ground import *
fig = sheet('SHEET 2b · GRAND HALL + EAST HALL, BEFORE | AFTER',
            'Close-up of the landside halls (x 38-232). The Grand Hall keeps 72 x 30 m; the new pieces sit outside its queue zone. '
            'The East Hall is the same 72 m wide, deeper (40 m), and joined to it by the glass link gallery in front of the baggage block.')
for k, after in enumerate((False, True)):
    ax = fig.add_axes([0.008 + k * 0.497, 0.30, 0.488, 0.60]); ax.set_facecolor(BG)
    ground(ax, after, x0=30, x1=240, fs=8)
    ax.set_xlim(36, 234); ax.set_ylim(-16, 56); ax.set_aspect('equal'); ax.axis('off')
    fig.text(0.008 + k * 0.497, 0.74, 'AFTER' if after else 'BEFORE', color=NEW if after else FG, fontsize=20, weight='bold')
    dim(ax, 42, 114, -8, '72.0 Grand Hall (unchanged)'); dim(ax, 0, 30, 39, '30.0', vertical=True)
    dim(ax, 114, 156, -8, '42.0 baggage block')
    if after:
        dim(ax, 156, 228, -8, '72.0 East Hall'); dim(ax, 0, 40, 230.5, '40.0', vertical=True)
        dim(ax, 107, 156, -12.5, '49.0 link gallery, 4.0 deep (flush with the vestibules)')
        dim(ax, 101, 113, 13.0, '12.0', fs=7); dim(ax, 57.5, 64.2, 1.8, '6.7', fs=7); dim(ax, 160, 190, 16.3, '30.0', fs=7)
        arrow(ax, [(101, -2), (130, -2), (160, -2), (170, 14)], col=NEW, lw=1.6)
        ax.text(133, -6.0, 'landside walk Grand Hall <-> East Hall', color=NEW, fontsize=8, ha='center')
        arrow(ax, [(228, 20), (213, 20)], col=NS, lw=2.0)
        ax.text(229, 23.5, 'arrivals\nout of customs', color=NS, fontsize=8, ha='left')
    else:
        box(ax, 156, 0, 234, 52, 'EAST WING - EMPTY TODAY', fc='#0d1116', col=WARN, fs=13)
    clip(ax)
# the pieces and their real sizes
ax = fig.add_axes([0.012, 0.03, 0.976, 0.24]); ax.axis('off'); ax.set_xlim(0, 100); ax.set_ylim(0, 1)
ax.text(0, 0.98, 'THE NEW PIECES, REAL SIZES (from manufacturers\' data and photos; every model our own, blueprint first)', color=NEW, fontsize=14, weight='bold', va='top')
P = [('Self-service kiosk', '0.6 w x 0.6 d x 1.8 h, 1.15 m apart, 1.5 m queue in front', '12 in the Grand Hall (2 x 6), 4 in the East Hall'),
     ('Self bag-drop machine', '2.0 w x 2.6 d (scale belt + screen + barrier), belt to the tunnel hatch', '6 Grand Hall + 4 East Hall'),
     ('Service / ticket desk', '5.5 x 5.5 m with 3 positions, back office door', 'Grand Hall east wall; info + PRM desk in the East Hall'),
     ('Boarding-pass e-gate', '0.9 lane (1.0 wide PRM lane), 1.6 m glass flaps, scanner', '3 + 1 wide at each of the two security doors'),
     ('Security lane, automated (ATRS)', '4 parallel loading stations, 30 m long, CT scanner, tray return under the belt', 'the 6 lanes, the same room'),
     ('Passport booth', '2.2 w x 2.2 d, 2 officers face to face, 2.1 h glass', '6 in (F1), 3 out (F2)'),
     ('E-gate (passport)', '0.9 w x 2.4 d, two glass doors, face camera', '8 in (F1), 4 out (F2)'),
     ('Customs channel', 'green 6 m wide, red 5 m with an inspection bench + office', 'west (exists) + east (new)'),
     ('Oversize belt', '1.2 w belt, 2.5 h opening with roller door', 'claim west, claim east, East Hall drop (OS)'),
     ('Lost-and-found desk', '8 x 7 m office, counter 3 m, bag shelving behind', 'claim west, claim east'),
     ('Trolley bay', '6.0 x 1.2 m, holds ~30 nested trolleys', 'every entrance + both claim halls'),
     ('Wide-body claim belt', 'flat-plate racetrack 8 x 34 m (~80 m frontage), feed from below', '2 in the east claim hall')]
for i, (a, b, c) in enumerate(P):
    col, row = i // 6, i % 6
    x = col * 50; y = 0.82 - row * 0.14
    ax.text(x, y, a, color=FG, fontsize=11.5, weight='bold', va='top'); ax.text(x + 15, y, b, color=DIM, fontsize=10.5, va='top')
    ax.text(x + 15, y - 0.065, c, color=NEW, fontsize=10.5, va='top')
save(fig, 'TC-2b-Halls-Before-After.png')
