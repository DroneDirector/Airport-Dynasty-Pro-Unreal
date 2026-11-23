# GA-1-Plan.png: gate 9 in plan before and after at large scale, the three depths compared.
from ga_common import *
A3 = Arc(3.0); A2, A4 = Arc(2.0), Arc(4.0)
fig = sheet('GA-1', 'GATE 9 IN PLAN: BEFORE, AFTER, AND THREE DEPTHS', "Floor 2 (floor 6.0 m, ceiling 11.36 m, roof 12.0 m). Measured on the game's own model. The jetway, its corridor, its door and the rotunda do not move.")
ZZ = (66.2, 98.6); XX = (-238.6, -196.4)
for k in (0, 1):
    ax = ax_plan(fig, [0.008 + 0.497 * k, 0.335, 0.487, 0.548], XX[0], XX[1], *ZZ)
    gate9(ax, arc=A3 if k else None); jetway(ax)
    ax.plot([-225.42] * 2, [84.5, 98.6], color='#d9b23a', lw=1.6, ls='--', zorder=1)
    box(ax, -240.42, 87, -236.42, 99, fc='none', ec='#dfe6ee', lw=1.6); box(ax, -222.42, 82.6, -209.42, 99, fc='#301018', ec=RED, lw=1.4, z=1, alpha=0.55)
    ax.plot([-204.99] * 2, [82.6, 98.6], color=RED, lw=2.2, zorder=1)
    txt(ax, -237.6, 93.0, 'tug box', FG, 13, rotation=90); txt(ax, -224.75, 93.8, 'stand 9 centre line', '#d9b23a', 13, rotation=90); txt(ax, -204.3, 94.2, 'stand boundary (red)', RED, 13, rotation=90)
    txt(ax, -219.3, 90.6, 'jetway no-parking\nzone (red)', '#ff8a95', 13); txt(ax, -211.7, 95.84, 'rotunda', FG, 12.5); txt(ax, -222.0, 69.4, 'GATE 9', FG, 22, weight='bold')
    txt(ax, -211.7, 89.6, 'J-9\nfixed\ncorridor', FG, 13, weight='bold'); txt(ax, -204.0, 74.8, 'stair', FG, 13, rotation=90, bbox=dict(fc='#22303c', ec='none', pad=2)); txt(ax, -198.0, 73, 'VIEW BAY 9', FG, 14, rotation=90, weight='bold')
    lead(ax, -200.8, 79.7, -201.2, 77.3, 'lift', FG, 13); lead(ax, -211.7, 81.9, -209.6, 79.6, 'jetway door', OK, 13, ha='left'); txt(ax, -213.9, 80.35, 'desk', FG, 12.5)
    dim(ax, JCL, JCR, 87.2, '3.42', fs=13)
    if not k:
        fig.text(0.012, 0.905, 'BEFORE (today)', color=FG, fontsize=21, weight='bold', va='top')
        lead(ax, -228.6, 81.95, -230.2, 88.0, 'one flat window, 34.0 m:\n12 panes of 1.758, the door,\n6 panes of 1.850', GLASS, 14)
        dim(ax, X0, XR, 67.2, '34.00'); dim(ax, X0, JX, 83.0, "22.00 to the corridor's axis"); dim(ax, 68, 82, -235.4, '14.00', vertical=True)
    else:
        fig.text(0.509, 0.905, 'AFTER: depth 3.0 m (recommended)', color=NEW, fontsize=21, weight='bold', va='top')
        n = new_seats(ax, A3); dim(ax, X0, X1, 67.2, 'chord 31.84 (outer corner to the lift)'); ax.plot([A3.mid] * 2, [ZG, ZG + 3], color=NEW, lw=1.2, ls=':', zorder=20)
        dim(ax, ZG, ZG + 3, -219.4, '3.00', vertical=True); ax.plot([-219.8, A3.mid], [ZG + 3] * 2, color=NEW, lw=1, ls=':')
        lead(ax, *A3.pt(-11.5), -229.2, 89.4, 'the true arc:\nradius 43.74, length 32.59\n20 radial mullions,\n19 curved panes of 1.715', NEW, 14.5)
        lead(ax, *A3.pt(8.0), -206.9, 87.6, 'portal', '#dfe6ee', 13.5); lead(ax, *A3.pt(SEC_W[3], SOFF - 0.4), -230.6, 79.4, '24 new seats:\n12 + 12', '#b7a6ff', 13)
        gx = A3.pt(GAP_T, SOFF)[0]; ax.annotate('', xy=(gx, 84.3), xytext=(gx, 78.1), arrowprops=dict(arrowstyle='-|>', color=OK, lw=4), zorder=25); txt(ax, gx + 0.5, 79.6, 'way to the\nglass, 2.42', OK, 13, ha='left')
        a_, b_ = A3.pt(SEC_E[9], SOFF + KNEE), A3.pt(SEC_E[9], -0.05); dim(ax, a_[1], b_[1], a_[0] + 0.25, '1.55', vertical=True, fs=12)
        dim(ax, 82, A3.z(-208.6), -208.4, '2.0', vertical=True, fs=12.5); dim(ax, 80.5, A3.z(-204.0), -203.6, '%.1f' % (A3.z(-204.0) - 80.57), vertical=True, fs=12.5)
        lead(ax, X0, ZG, -236.3, 84.6, 'corner\npost', FG, 13); lead(ax, X1, ZG, -199.4, 85.6, "post, then\ntoday's flat\nglass 2.16", FG, 13)
# the three depths
ax = ax_plan(fig, [0.008, 0.022, 0.60, 0.268], -236.6, -196.9, 79.4, 88.0)
box(ax, X0, 79.4, XR, 82, fc='#0f1b26', ec='none', z=1); ax.plot([X0, XR], [ZG, ZG], color=OLD, lw=2, zorder=3); ax.plot([X0, X0], [79.4, 82], color='#aab8c4', lw=4, zorder=8); ax.plot([XR, XR], [79.4, 82], color='#aab8c4', lw=3, zorder=8)
lx0, lz0, lx1, lz1 = LIFT; box(ax, lx0, lz0, lx1, lz1, fc='#22303c', ec=FG, z=9); txt(ax, -200.8, 80.6, 'lift', FG, 13)
box(ax, JCL, 82, JCR, 88, fc='#3a2e22', ec=JET, z=4)
for x in (-213.15, -210.25): box(ax, x - 0.15, 85.85, x + 0.15, 86.15, fc=JET, ec=JET, z=12)
SK = np.load(os.path.join(HERE, 'sketch_arc.npy')); ax.plot(SK[:, 0], SK[:, 1], color=RED, lw=9, alpha=0.30, zorder=2, solid_capstyle='round')
for q, c in ((A2, '#8fd0ff'), (A3, NEW), (A4, WARN)): arcline(ax, q, c, 5 if q is A3 else 3, zorder=10)
txt(ax, -219.3, 83.50, '2 m', '#8fd0ff', 17, weight='bold'); txt(ax, -219.3, 84.48, '3 m', NEW, 17, weight='bold'); txt(ax, -219.3, 85.46, '4 m', WARN, 17, weight='bold'); lead(ax, -228.6, 84.85, -231.4, 87.0, 'your sketch (best circle:\nradius 39.4, 4.3 m out)', RED, 14)
lead(ax, -210.2, 86.0, -205.4, 86.9, "the corridor's first columns (z 85.85):\n0.15 m from the 4 m arc's edge", WARN, 13.5); txt(ax, -211.7, 83.0, 'corridor', FG, 13.5)
fig.text(0.012, 0.318, 'THREE DEPTHS, SAME CHORD', color=FG, fontsize=20, weight='bold')
QS = (A2, A3, A4)
rows = [('', '2 m', '3 m', '4 m'), ('Radius',) + tuple('%.2f' % q.R for q in QS), ('Arc length',) + tuple('%.2f' % q.L for q in QS), ('Pane (19 of them)',) + tuple('%.3f' % q.p for q in QS),
        ('Angle at the two ends',) + tuple('%.1f°' % math.degrees(q.a) for q in QS), ('Floor gained, m²',) + tuple('%.1f' % q.area for q in QS),
        ('without the corridor, m²',) + tuple('%.1f' % (q.area - (q.z(JX) - ZG) * 3.51) for q in QS), ('Meets the corridor at z',) + tuple('%.2f' % q.z(JX) for q in QS),
        ('To its first columns', '2.00', '1.07', '0.15'), ("To the rotunda's roof", '10.16', '9.26', '8.34')]
for i, r in enumerate(rows):
    y = 0.300 - i * 0.0218
    fig.text(0.622, y, r[0], color=DIM, fontsize=14.5, va='center')
    for j, v in enumerate(r[1:]): fig.text(0.772 + 0.040 * j, y, v, color=('#8fd0ff', NEW, WARN)[j], fontsize=15, va='center', ha='center', weight='bold' if j == 1 or i == 0 else 'normal')
fig.text(0.622, 0.082, '3 m: it reads as a curve from the apron, and the glass meets the corridor\nwhere its first solid wall bay ends (z 84.3), so the corridor outside stays all\nglass. 2 m hardly shows. 4 m is 0.15 m from the columns: too tight.', color=FG, fontsize=14, va='top', linespacing=1.3)
fig.text(0.878, 0.300, 'WHAT MOVES', color=NEW, fontsize=16, weight='bold', va='center')
fig.text(0.878, 0.286, "Nothing inside has to move: the\nseats, desk, stair, lift and door\nstay. New: 24 seats along the\ncurve (111 become 135): two\nsections of 12, 1.55 m clear in\nfront of the knees, a 2.42 m way\nto the glass between them.\n\nThe corridor's first 2.2 - 2.7 m\nstands inside the bay as a glass\nentrance, its door in its place.\n\nBetween the stair and the glass:\n2.1 m, today 1.3.", color=FG, fontsize=13.5, va='top', linespacing=1.28)
save(fig, 'GA-1-Plan.png')
