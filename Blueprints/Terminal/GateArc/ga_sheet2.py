# GA-2-Section-Detail.png: the section through the bay, the section along the corridor, the portal in plan and elevation.
from ga_common import *
A = Arc(3.0); EL = A.mull()
fig = sheet('GA-2', 'SECTION THROUGH THE BAY, AND HOW THE CORRIDOR PASSES THE CURVED GLASS', 'Heights above the ground floor (0.00). Grey: today, unchanged. Yellow: new. Orange: the jetway J-9, unchanged.')
CO, EX = '#39434d', '#8fa2b3'


def cut(ax, x0, z0, x1, z1, new=False, z=5): box(ax, x0, z0, x1, z1, fc='#6b5a1e' if new else CO, ec=NEW if new else EX, lw=1.4, z=z)


# ---------------------------------------------------------------- 1: section at the deepest point (x -217.78)
fig.text(0.012, 0.900, '1   Section at the deepest point, x -217.78 (looking east)', color=FG, fontsize=18, weight='bold')
ax = ax_plan(fig, [0.008, 0.475, 0.60, 0.415], 66.6, 99.2, -0.9, 13.4)
ax.plot([66.6, 99.2], [0, 0], color='#7b8894', lw=3); box(ax, 66.6, -0.9, 99.2, 0, fc='#1a1f24', ec='none')
cut(ax, 66.6, 5.63, 82.0, 5.99); cut(ax, 66.6, 11.40, 82.0, 12.0); cut(ax, 81.85, 0, 82.08, 5.4); cut(ax, 67.9, 8.2, 68.1, 11.4)
cut(ax, 82.0, 5.40, 85.0, 5.99, True); cut(ax, 82.0, 11.40, 85.0, 12.0, True); ax.plot([ZG, ZG], [6, 11.4], color=GLASS, lw=1.5, ls='--'); cut(ax, 84.84, 6.0, 84.98, 11.4, True)
box(ax, 82.0, JBOT, 94.0, JTOP, fc='none', ec='none'); ax.add_patch(Polygon([(82, 5.70), (94, 5.01), (94, 8.41), (82, 9.10)], fc='#3a2e22', ec=JET, lw=1.6, zorder=2, alpha=0.75))
for z in (86.0, 92.0): box(ax, z - 0.15, 0, z + 0.15, 5.4 - (z - 82) * JSL, fc='#3a2e22', ec=JET, z=2)
box(ax, ROT[1] - ROTR, 5.08, ROT[1] + ROTR, 8.40, fc='#0c1c2c', ec=JET, lw=1.6, z=3); box(ax, ROT[1] + 0.1, 0, ROT[1] + 0.9, 5.08, fc='#3a2e22', ec=JET, z=2)
for z in (71.4, 74.4, 77.4): box(ax, z - 0.4, 6.0, z + 0.4, 6.95, fc='#1d3a66', ec='#33558a')
box(ax, 82.45, 6.0, 83.13, 6.95, fc='#7a5cff', ec='#b7a6ff'); box(ax, 80.05, 6, 80.75, 7.05, fc='#c8ccd0', ec='#fff')
for z, h in ((79.2, 6.0), (90.0, 0.0)):                             # a person 1.75 m, for scale
    ax.add_patch(Circle((z, h + 1.62), 0.13, fc=FG, zorder=20)); box(ax, z - 0.2, h + 0.75, z + 0.2, h + 1.47, fc=FG, ec='none', z=20); box(ax, z - 0.14, h, z + 0.14, h + 0.75, fc=FG, ec='none', z=20)
dim(ax, 82.0, 85.0, 12.75, '3.00'); dim(ax, 0, 5.40, 83.6, '5.40 clear', vertical=True); dim(ax, 6.0, 11.36, 69.6, '5.36', vertical=True); txt(ax, 85.25, 5.72, '0.60', NEW, 13, ha='left'); txt(ax, 85.25, 11.72, '0.60', NEW, 13, ha='left')
lead(ax, 85.4, 4.6, 88.3, 4.1, '0.85 slab to columns', NEW, 13); ax.plot([85.0, 85.85], [4.6, 4.6], color=NEW, lw=2); dim(ax, 85.0, 93.92, 9.9, '8.9 to the rotunda'); dim(ax, 85.0, 98.9, 0.75, 'A320 nose 35.4 from the glass (z 120) →', fs=13.5)
txt(ax, 75, 9.3, 'GATE 9', FG, 18, weight='bold'); txt(ax, 75, 2.8, 'RAMP SERVICES 9 (unchanged, flat wall)', DIM, 14); txt(ax, 95.84, 6.8, 'rotunda', FG, 13); txt(ax, 90.2, 7.2, 'corridor (beyond, x -211.7)', JET, 13)
lead(ax, 84.9, 10.4, 90.6, 10.9, 'curved glass + mullion', NEW, 14); lead(ax, 84.2, 11.75, 77.6, 12.95, 'roof edge on the same arc', NEW, 14); lead(ax, 83.2, 5.6, 78.2, 4.4, 'floor slab cantilever 3.0', NEW, 14); lead(ax, ZG, 9.0, 78.6, 10.4, "today's flat glass (gone)", GLASS, 13.5)
lead(ax, 82.9, 6.9, 80.2, 8.3, 'new seats', '#b7a6ff', 13.5)
txt(ax, 97.6, 12.6, 'roof 12.00 · ceiling 11.36\nfloor 6.00 · soffit 5.40', FG, 13.5, ha='right')
# ---------------------------------------------------------------- 2: section along the corridor's axis
fig.text(0.012, 0.445, "2   Section along the corridor's axis, x -211.70", color=FG, fontsize=18, weight='bold')
ax = ax_plan(fig, [0.008, 0.028, 0.36, 0.405], 79.2, 89.0, 4.2, 12.6)
cut(ax, 79.2, 5.63, 82.0, 5.99); cut(ax, 79.2, 11.4, 82, 12.0); cut(ax, 82.0, 11.4, A.z(JX) + 0.09, 12.0, True)
f = lambda z: 6.0 - (z - 82) * JSL
ax.add_patch(Polygon([(82, f(82) - .3), (89, f(89) - .3), (89, f(89)), (82, f(82))], fc='#5a4630', ec=JET, lw=1.6, zorder=4)); ax.add_patch(Polygon([(82, f(82) + 2.95), (89, f(89) + 2.95), (89, f(89) + 3.1), (82, f(82) + 3.1)], fc='#5a4630', ec=JET, lw=1.6, zorder=4))
box(ax, 82.7, f(83.5), 84.3, f(83.5) + 2.75, fc='#2c2a28', ec='#8a7a66', z=3); zc = A.z(JX)
cut(ax, zc - .16, 9.10, zc + .16, 9.25, True, 8); cut(ax, zc - .07, 9.25, zc + .07, 11.4, True, 7); box(ax, zc - .08, f(zc) + 3.1, zc + .08, 9.10, fc='#101010', ec='#c0c0c0', z=9); box(ax, zc - .08, 5.40, zc + .08, f(zc) - .3, fc='#101010', ec='#c0c0c0', z=9)
box(ax, 81.96, 6, 82.04, 8.25, fc=OK, ec=OK, z=8); cut(ax, 81.85, 8.39, 81.97, 9.10, True, 8); box(ax, 81.99, 9.1, 82.15, 9.25, fc='#c9d3dc', ec=FG, z=8)
box(ax, 85.85, 4.2, 86.15, f(86) - .3, fc='#3a2e22', ec=JET, z=3)
dim(ax, 82.0, zc, 7.2, '%.2f inside the bay' % (zc - 82), fs=13.5); dim(ax, 9.25, 11.36, 83.2, '2.1', vertical=True, fs=13.5); dim(ax, zc + .09, 85.85, 5.0, '1.3', fs=13)
lead(ax, 82.0, 7.9, 80.6, 8.8, 'door: not moved', OK, 13.5); lead(ax, zc, 9.18, 87.0, 9.9, 'portal head 9.10 - 9.25', NEW, 13.5); lead(ax, zc, 9.0, 87.0, 8.0, 'rubber joint', FG, 13.5); lead(ax, 88, f(88) - .1, 87.6, 4.9, 'corridor floor, 5.77 %\ndown, as today', JET, 13)
lead(ax, zc, 10.6, 86.9, 11.2, 'curved glass', NEW, 13.5); txt(ax, 83.5, 6.5, "its solid wall bay\n(z 82.7 - 84.3)", '#d8c8b4', 12.5); lead(ax, 82.07, 9.2, 80.6, 10.2, 'his gate trim\nstays', '#c9d3dc', 13)
# ---------------------------------------------------------------- 3: the portal in plan
fig.text(0.385, 0.445, '3   The portal in plan (floor 2)', color=FG, fontsize=18, weight='bold')
ax = ax_plan(fig, [0.385, 0.028, 0.30, 0.405], -216.0, -207.4, 81.0, 86.6)
box(ax, JCL, 82.0, JCR, 86.6, fc='#2a231c', ec=JET, lw=2, z=3); box(ax, -216, 81, -207.4, 82.0, fc='#0f1b26', ec='none', z=1)
for s in (-1, 1): box(ax, JX + s * 1.55 - .06, 82, JX + s * 1.55 + .06, 86.6, fc='#5a4630', ec=JET, z=4)
for x in (-213.15, -210.25): box(ax, x - .15, 85.85, x + .15, 86.15, fc=JET, ec=JET, z=5)
tl, tr = TJL, TJR
for a, b in ((A.t(-216.2), tl - .075), (tr + .075, A.t(-207.2))): arcline(ax, A, NEW, 6, t0=a, t1=b, zorder=10)
arcline(ax, A, NEW, 1.6, t0=tl, t1=tr, zorder=10, ls='--')
for i, u in enumerate(EL):
    x, z = A.pt(u)
    if -216 < x < -207.4 and i not in (12, 13, 14):
        a = u / A.R; c, s = math.cos(a), math.sin(a); ax.add_patch(Polygon([(x + p * c + q * s, z - p * s + q * c) for p, q in ((-.04, -.07), (.04, -.07), (.04, .07), (-.04, .07))], fc=FG, ec=FG, zorder=12))
    if i in (13, 14): ax.plot(x, z, 's', ms=9, mfc='none', mec=NEW, mew=2, zorder=12)
for u in (tl, tr):
    x, z = A.pt(u); a = u / A.R; c, s = math.cos(a), math.sin(a); ax.add_patch(Polygon([(x + p * c + q * s, z - p * s + q * c) for p, q in ((-.075, -.16), (.075, -.16), (.075, .16), (-.075, .16))], fc='#c9d3dc', ec=FG, zorder=12))
for a, b in ((-213.425, JCL), (JCR, -209.975)): zz = A.z((a + b) / 2); box(ax, a, zz - .12, b, zz + .12, fc='#000', ec='#fff', z=13)
box(ax, -212.6, 81.93, -210.8, 82.05, fc=OK, ec=OK, z=9); box(ax, -213.55, 81.9, -213.41, 82.1, fc='#c9d3dc', ec=FG, z=9); box(ax, -209.99, 81.9, -209.85, 82.1, fc='#c9d3dc', ec=FG, z=9)
zl, zr = A.pt(tl)[1], A.pt(tr)[1]
dim(ax, 82.0, zl, -214.1, '%.2f' % (zl - 82), vertical=True, fs=14); dim(ax, 82.0, zr, -209.3, '%.2f' % (zr - 82), vertical=True, fs=14); dim(ax, JCL, JCR, 86.35, '3.42 corridor', fs=14); dim(ax, A.pt(tl)[0], A.pt(tr)[0], 82.75, '3.60 between jambs', fs=14); dim(ax, 84.78, 85.85, -212.75, '1.07', vertical=True, fs=13)
lead(ax, *A.pt(EL[12]), -215.0, 85.9, 'left jamb =\nthe 13th mullion', NEW, 13.5); lead(ax, *A.pt(tr), -208.7, 85.5, 'right jamb', '#c9d3dc', 13.5); lead(ax, *A.pt(EL[13]), -211.9, 83.9, 'mullions 14 and 15\nend on the head', NEW, 13); lead(ax, -211.7, 82.0, -211.7, 81.4, 'jetway door x -211.70, z 82.0: not moved', OK, 13.5)
txt(ax, -215.0, 83.0, 'bay floor\n6.00', FG, 13); txt(ax, -208.3, 82.7, 'bay floor\n6.00', FG, 13)
# ---------------------------------------------------------------- 4: the portal from the apron
fig.text(0.625, 0.900, '4   The portal seen from the apron', color=FG, fontsize=18, weight='bold')
ax = ax_plan(fig, [0.625, 0.475, 0.37, 0.415], -217.6, -204.6, 4.2, 12.6)
box(ax, -217.6, 11.4, -204.6, 12.0, fc='#6b5a1e', ec=NEW); box(ax, -217.6, 6.0, -204.6, 11.4, fc='#0d2029', ec='none', z=1)
for a, b in ((-217.6, -213.49), (-209.91, -204.6)): box(ax, a, 5.4, b, 6.0, fc='#6b5a1e', ec=NEW)
for i, u in enumerate(EL):
    x = A.pt(u)[0]
    if -217.6 < x < -204.6: box(ax, x - .04, 9.25 if i in (13, 14) else 6, x + .04, 11.4, fc=FG, ec='none', z=6)
box(ax, -213.574, 5.4, -213.424, 9.1, fc='#c9d3dc', ec=FG, z=7); box(ax, -209.976, 5.4, -209.826, 9.1, fc='#c9d3dc', ec=FG, z=7); box(ax, -213.574, 9.1, -209.826, 9.25, fc='#c9d3dc', ec=FG, z=7)
box(ax, JCL, f(84.49) - .3, JCR, f(84.49) + 3.1, fc='#2a231c', ec=JET, lw=2, z=5); box(ax, -213.25, f(84.49), -210.15, f(84.49) + 2.75, fc='#1a1510', ec='#8a6a48', z=6); box(ax, -212.6, 6, -210.8, 8.25, fc='#123a22', ec=OK, z=7)
dim(ax, -213.574, -209.826, 9.55, '3.75 over the collar', fs=14); dim(ax, 9.25, 11.4, -207.6, '2.15', vertical=True, fs=13.5); dim(ax, A.pt(EL[10])[0], A.pt(EL[11])[0], 10.6, '1.71', fs=13.5); dim(ax, -209.826, A.pt(EL[15])[0], 8.6, '1.5', fs=13.5)
txt(ax, -211.7, 7.2, 'door at z 82\n(2.5 m behind)', OK, 13); txt(ax, -216.2, 11.7, 'roof edge', FG, 13); txt(ax, -215.9, 5.7, 'floor edge, white, 0.60', FG, 13); txt(ax, -207.2, 5.7, 'same height as the roof edge', FG, 13)
txt(ax, -216.05, 7.6, 'curved\nglass', GLASS, 14); txt(ax, -207.6, 7.0, 'curved\nglass', GLASS, 14); txt(ax, -211.7, 4.75, 'black rubber joint all round: the corridor stands on its own columns, the bay never loads it', FG, 12.5)
# ---------------------------------------------------------------- the notes
fig.text(0.700, 0.445, 'HOW IT IS DRAWN', color=NEW, fontsize=18, weight='bold')
notes = ["TRUE ARC: one circle, radius 43.74, the same for the glass, the floor", "edge (+0.09) and the roof edge (+0.09).",
         "PANES CURVED, not faceted: each 1.715 pane bows 8.4 mm. In the game", "the mesh is curved too (a step every 12 cm, 0.04 mm off the circle).",
         "20 MULLIONS as today (80 x 140), turned radial, 1.715 apart", "(today 1.758 and 1.850). No pane is a sliver: the smallest is 1.5 m.",
         "ENDS: 21.3° corners on a post, at the outer wall and at the lift. The", "2.16 m in front of the lift stays flat and lines up with the view bay.",
         "SLAB: slot 3.51 m wide for the corridor. Soffit 5.40, level with the", "ground floor's ceiling line; nothing below it changes.",
         "CORRIDOR: not touched. It meets the curve at 8°, so its left side is", "0.5 m deeper than its right; a 150 mm steel collar (the face of his", "gate trim) takes up the difference.",
         "THE DOOR'S WALL: today's glass either side of the door is cut to the", "corridor's width (3.4 x 3.1 m) and stands free: the glass entrance."]
for i, s in enumerate(notes): fig.text(0.700, 0.420 - i * 0.0232, s, color=FG if not s.split(':')[0].isupper() else FG, fontsize=14, va='center')
save(fig, 'GA-2-Section-Detail.png')
