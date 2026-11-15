# Where the Floor 2 bar could go (owner, 2026-11-15: "move it elsewhere on floor 2 at its full size ... draw me a
# blueprint ... no conflicts"): Floor 2 from above, the game's plan (Data/terminal-plan.json) with the approved changes,
# the bar's old place and two candidates with what each one touches.  python docs/Requests/draw_bar.py
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from draw_floor2 import Sheet, OLD, OLD2, NEW, NEWL, NOTE, RED, LINE, WHITE, BG, font
P = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Data', 'terminal-plan.json')))
s = Sheet('FLOOR 2 FROM ABOVE · WHERE THE BAR CAN GO',
          'The game\'s Floor 2 plan with the approved changes. North (the gates) at the top. Red dashed = the bar today. Turquoise = new.',
          w=2700, h=1250, s=10.5, ox=470, oy=870)
def R(x0, z0, x1, z1, fill, out=LINE, w=2, label=None, sub=None, lc=WHITE):
    s.rect(x0, -z1, x1, -z0, fill, out, w, label, sub, lc)
for r in P['rooms']:
    if r['f'] != 2 or r['z0'] > 60 or r['x1'] < -40 or r['x0'] > 175: continue
    if r['id'] == 'F2-WALK': R(-40, 52, 175, 68, OLD, label='WALKWAY TO THE GATES (Floor 2)'); continue
    if r['id'] == 'F2-BAR': continue
    R(max(-40, r['x0']), r['z0'], min(175, r['x1']), r['z1'], OLD2, label=r['id'][3:].replace('-', ' '), sub=None)
# the Grand Hall below (open to the roof, no Floor 2)
R(36, 0, 114, 30, (16, 50, 86), out=(90, 120, 160), label='GRAND HALL (open to the roof)')
# the approved changes
R(-36, 0, 0, 52, NEW, out=NEWL, w=3)
s.d.text(s.P(-18, -3), 'MUSEUM (Floor 1 below)', NEWL, font(18, True), anchor='mm')
R(0, 30, 6, 52, NEW, out=NEWL, w=3)
R(0, 0, 36, 30, NEW, out=NEWL, w=3, label='ESCALATOR ROOM', sub='landing x 0-21', lc=NEWL)
R(114, 0, 156, 52, NEW, out=NEWL, w=3, label='OFFICES (new)', sub='to the walkway', lc=NEWL)
R(156, 30, 168, 40, NEW, out=NEWL, w=3)
s.d.text(s.P(162, -35), 'LIFT', NEWL, font(16, True), anchor='mm')
# the bar today: dashed red
x0, z0, x1, z1 = 104, 30, 146, 52
for (a, b) in (((x0, z0), (x1, z0)), ((x1, z0), (x1, z1)), ((x1, z1), (x0, z1)), ((x0, z1), (x0, z0))):
    n = 30
    for i in range(0, n, 2):
        pa = (a[0] + (b[0] - a[0]) * i / n, -(a[1] + (b[1] - a[1]) * i / n)); pb = (a[0] + (b[0] - a[0]) * (i + 1) / n, -(a[1] + (b[1] - a[1]) * (i + 1) / n))
        s.line(pa, pb, RED, 4)
s.d.text(s.P(125, -41), 'BAR TODAY', RED, font(22, True), anchor='mm'); s.d.text(s.P(125, -37.5), '42 x 22 m', RED, font(17), anchor='mm')
# candidate 1: above the museum, full size, turned (22 across, 42 deep)
CAND = (255, 200, 90)
for (a, b) in (((-22, 10), (-0.4, 10)), ((-0.4, 10), (-0.4, 51.6)), ((-0.4, 51.6), (-22, 51.6)), ((-22, 51.6), (-22, 10))):
    s.line((a[0], -a[1]), (b[0], -b[1]), CAND, 5)
s.d.text(s.P(-11, -30), '1', CAND, font(60, True), anchor='mm'); s.d.text(s.P(-11, -24), 'BAR over the museum', CAND, font(17, True), anchor='mm'); s.d.text(s.P(-11, -21), '22 x 42 m (full size)', CAND, font(16), anchor='mm')
# candidate 2: same line, shifted west to end at the offices (x 72-114)
for (a, b) in (((72, 30.6), (114, 30.6)), ((114, 30.6), (114, 51.4)), ((114, 51.4), (72, 51.4)), ((72, 51.4), (72, 30.6))):
    s.line((a[0], -a[1]), (b[0], -b[1]), CAND, 5)
s.d.text(s.P(93, -41), '2', CAND, font(60, True), anchor='mm'); s.d.text(s.P(93, -34.5), 'BAR, same line, 42 x 22', CAND, font(17, True), anchor='mm')
s.d.text(s.P(-40, -71), 'N (gates)', WHITE, font(22, True), anchor='lm')
s.note((-11, -51.6), (300, 140), 'OPTION 1 - the bar above the museum, full size (22 m wide, 42 m long), turned to run north-south because the museum is 36 m wide; its east side on the escalator landing, its north end on the walkway (both open). The museum below becomes one storey (ceiling +5.40) under the bar; the west 14 m of the museum keep their full height. Conflicts: only the museum height under the bar - nothing else moves.')
s.note((93, -51.4), (1250, 140), 'OPTION 2 - the bar on the same east-west line at full size (42 x 22 m), shifted west so it ends where the offices begin (x 72-114). Conflicts: it lands on the Tax Free (x 56-90), the Family room and the Floor 2 toilets (x 90-104); those would have to move or shrink, and the stairs and lift from airside on the landing (x 56-90, north half) must stay clear.')
s.note((125, -30), (2200, 140), 'Today the bar is at x 104-146; the new offices (x 114-156) take 32 m of it, so it has to go somewhere.')
s.legend(140, 940, w=2450)
s.save('bar-options.png')
