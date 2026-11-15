# The owner's floor-2 terminal changes (2026-11-15) drawn back to him as blueprints before anything is built
# (memory: blueprint-before-build): his roof plan's sizes, the changes applied, a numbered note beside every change.
#   python docs/Requests/draw_floor2.py   -> docs/Requests/understood-1-roof-plan.png ... -4-east-end.png
# Sizes: metres, measured off his roof plan (1 m = 5.44 px there); x from the pier's west end, y from the pier's
# north (apron) face southwards to the kerb.
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
F = 'C:/Windows/Fonts/'
def font(sz, bold=False): return ImageFont.truetype(F + ('arialbd.ttf' if bold else 'arial.ttf'), sz)
BG = (11, 41, 74); GRID = (20, 55, 92); LINE = (205, 225, 245); OLD = (34, 80, 128); OLD2 = (24, 62, 104)
NEW = (22, 92, 94); NEWL = (80, 220, 200); NOTE = (255, 205, 110); RED = (255, 90, 90); GLASS = (110, 170, 225)
WHITE = (235, 242, 250)


class Sheet:
    def __init__(self, title, sub, w=2600, h=1500, s=6.0, ox=120, oy=260):
        self.im = Image.new('RGB', (w, h), BG); self.d = ImageDraw.Draw(self.im)
        self.s, self.ox, self.oy = s, ox, oy
        for x in range(0, w, 40): self.d.line([(x, 0), (x, h)], GRID, 1)
        for y in range(0, h, 40): self.d.line([(0, y), (w, y)], GRID, 1)
        self.d.text((60, 30), title, WHITE, font(40, True)); self.d.text((60, 85), sub, (170, 195, 220), font(22))
        self.notes = []
    def P(self, x, y): return (self.ox + x * self.s, self.oy + y * self.s)
    def rect(self, x0, y0, x1, y1, fill, out=LINE, wdt=2, label=None, sub=None, lc=WHITE):
        self.d.rectangle([self.P(x0, y0), self.P(x1, y1)], fill=fill, outline=out, width=wdt)
        if label:
            cx, cy = self.P((x0 + x1) / 2, (y0 + y1) / 2)
            self.d.text((cx, cy - (12 if sub else 0)), label, lc, font(22, True), anchor='mm')
            if sub: self.d.text((cx, cy + 16), sub, lc, font(15), anchor='mm')
    def poly(self, pts, fill, out=LINE, wdt=2): self.d.polygon([self.P(*p) for p in pts], fill=fill, outline=out, width=wdt)
    def line(self, a, b, col=LINE, wdt=2): self.d.line([self.P(*a), self.P(*b)], col, wdt)
    def dim(self, x0, x1, y, text):
        a, b = self.P(x0, y), self.P(x1, y)
        self.d.line([a, b], NOTE, 2); self.d.line([(a[0], a[1] - 8), (a[0], a[1] + 8)], NOTE, 2); self.d.line([(b[0], b[1] - 8), (b[0], b[1] + 8)], NOTE, 2)
        self.d.rectangle([((a[0] + b[0]) / 2 - 60, a[1] - 12), ((a[0] + b[0]) / 2 + 60, a[1] + 12)], fill=BG)
        self.d.text(((a[0] + b[0]) / 2, a[1]), text, NOTE, font(17, True), anchor='mm')
    def note(self, at, box, text):
        """A numbered callout: a dot at `at` (metres), a leader line to a text box at `box` (pixels)."""
        n = len(self.notes) + 1; self.notes.append(text)
        p = self.P(*at)
        self.d.line([p, box], NOTE, 2); self.d.ellipse([p[0] - 7, p[1] - 7, p[0] + 7, p[1] + 7], fill=NOTE)
        self.d.ellipse([box[0] - 17, box[1] - 17, box[0] + 17, box[1] + 17], fill=NOTE)
        self.d.text(box, str(n), BG, font(20, True), anchor='mm')
    def legend(self, x, y, w=1050):
        f = font(19); yy = y
        self.d.text((x, yy), 'WHAT I UNDERSTOOD (numbers on the drawing)', NOTE, font(22, True)); yy += 38
        for i, t in enumerate(self.notes, 1):
            words, lines, cur = t.split(), [], ''
            for wd in words:
                if self.d.textlength(cur + ' ' + wd, font=f) > w - 50: lines.append(cur); cur = wd
                else: cur = (cur + ' ' + wd).strip()
            lines.append(cur)
            self.d.ellipse([x, yy, x + 28, yy + 28], fill=NOTE); self.d.text((x + 14, yy + 14), str(i), BG, font(17, True), anchor='mm')
            for ln in lines: self.d.text((x + 42, yy + 3), ln, WHITE, f); yy += 26
            yy += 10
    def key(self, x, y):
        self.d.rectangle([x, y, x + 30, y + 20], fill=NEW, outline=NEWL, width=2); self.d.text((x + 40, y + 10), 'new or changed (2026-11-15)', NEWL, font(18), anchor='lm')
        self.d.rectangle([x, y + 30, x + 30, y + 50], fill=OLD, outline=LINE, width=2); self.d.text((x + 40, y + 40), 'as it is now', LINE, font(18), anchor='lm')
        self.d.line([(x, y + 70), (x + 30, y + 70)], RED, 4); self.d.text((x + 40, y + 70), 'where a wall used to end (extended from here)', RED, font(18), anchor='lm')
    def save(self, name): self.im.save(os.path.join(HERE, name)); print('saved', name)


# ---------- 1. the roof plan, the changes applied ----------
S = Sheet('PASSENGER TERMINAL · FROM ABOVE · YOUR CHANGES AS I UNDERSTOOD THEM',
          'North (the apron) at the top, the kerb at the bottom, west on the left. Turquoise = new or changed. Numbered notes: my reading of each change.',
          w=2700, h=1330, s=6.0, ox=140, oy=220)
s = S
s.rect(0, 0, 323.5, 30, OLD, label='PIER: walkway and gates 1-8', sub='its south (landside) wall = the wall the new parts reach')
s.rect(86.6, 30, 194.9, 52.2, OLD2, label='FLOOR 2 ROOMS', sub='food walk, tax free, bar')
s.rect(194.9, 30, 237.1, 42.3, OLD2)
s.poly([(104.6, 52.2), (207, 52.2), (207, 89.5), (180, 94.5), (155.8, 96.5), (131, 94.5), (104.6, 89.5)], OLD, out=LINE)
s.d.text(s.P(155.8, 70), 'GRAND HALL', WHITE, font(24, True), anchor='mm'); s.d.text(s.P(155.8, 75), 'wave roof over it and the kerb', LINE, font(15), anchor='mm')
# the museum: its old block plus the stretch north to the pier wall, filling across the small strip to the floor-2 rooms
s.poly([(44.1, 30), (86.6, 30), (86.6, 52.2), (80.3, 52.2), (80.3, 82.4), (44.1, 82.4)], NEW, out=NEWL, wdt=3)
s.d.text(s.P(62, 42), 'MUSEUM', NEWL, font(26, True), anchor='mm'); s.d.text(s.P(62, 46), 'roof +12.00', NEWL, font(15), anchor='mm')
s.line((44.1, 52.2), (80.3, 52.2), RED, 4)
s.rect(80.3, 52.2, 116.5, 82.4, NEW, out=NEWL, wdt=3, label='ESCALATOR', sub='room, to Floor 2', lc=NEWL)
# offices stretched north to the pier wall, over the staff wing
s.rect(194.9, 30, 237.1, 82.4, NEW, out=NEWL, wdt=3, label='OFFICES', sub='Floor 2, over the staff wing (roof +12.00)', lc=NEWL)
s.line((194.9, 42.3), (237.1, 42.3), RED, 4)
s.rect(237.1, 42.3, 249.3, 52.2, NEW, out=NEWL, wdt=3, label='LIFT', sub='wing', lc=NEWL)
# the window: the offices' west wall along the Grand Hall
s.line((194.9, 52.55), (194.9, 82.05), GLASS, 9)
# the museum door, the open escalator entrance
mx = 62; s.d.rectangle([s.P(mx - 2.5, 82.4), s.P(mx + 2.5, 83.8)], fill=GLASS)
s.line((116.5, 53.5), (116.5, 81.0), (255, 120, 220), 6)
s.dim(44.1, 80.3, 104, '36.0'); s.dim(80.3, 116.5, 104, '36.0'); s.dim(116.5, 194.9, 104, '78.0 Grand Hall'); s.dim(194.9, 237.1, 104, '42.0'); s.dim(237.1, 249.3, 108, '12')
s.dim(44.1, 249.3, 113, '205.2 terminal with the new parts')
for yy, t in ((-3, ''),):
    pass
s.d.text(s.P(-12, 15), 'N', WHITE, font(30, True), anchor='mm')
# notes
s.note((44.1, 31), (60, 150), 'MUSEUM, west wall: stretched north from where it ended (red line) all the way to the pier\'s outer (landside) wall - about 22 m further. The wall meets the pier wall flush, no gap, no overlap (no clipping).')
s.note((86.6, 31), (900, 150), 'MUSEUM, east side up north: it fills across the small strip you painted turquoise, so its wall meets the floor-2 rooms\' west wall and the pier wall. The museum is one room, L-shaped; it keeps its roof at +12.00.')
s.note((194.9, 31), (1300, 150), 'OFFICES, west wall: stretched north from where they ended (red line) all the way to the pier\'s outer wall, about 12 m further, over the staff wing. Meets the pier wall flush, no clipping.')
s.note((237.1, 31), (1700, 150), 'OFFICES, east wall: stretched north the same way to the pier wall; the lift wing stays where it is, on the offices\' east side.')
s.note((194.9, 67), (1300, 1200), 'THE BIG WINDOW: the offices\' west wall where it faces the Grand Hall (about 30 m) is glass from the office floor (+6.00) to the ceiling (+11.40), 35 cm of wall left at each end, the terminal\'s glass, frames between the panes like the main hall\'s big windows, every pane the same width (sheet 2). The new north part, beside the floor-2 rooms, stays wall.')
s.note((62, 83.5), (380, 1200), 'MUSEUM DOOR: in its south outside wall, facing the kerb (away from the apron), on the passenger side - the only way in, from outside (sheet 3).')
s.note((116.5, 67), (820, 1200), 'ESCALATOR ENTRANCE: from the Grand Hall on Floor 1 - no door and no wall there, open as wide as the walkway. The escalator starts on Floor 1 and ends exactly at Floor 2\'s height (+6.00), level with the landing (sheet 3).')
s.note((243, 47), (2150, 380), 'LIFT: reached from the staff area\'s hallway on Floor 1, a working lift; on Floor 2 its door opens into the offices (sheet 4).')
s.key(2150, 120)
s.legend(1750, 520, w=900)
s.save('understood-1-roof-plan.png')

# ---------- 2. the offices' west wall: the big window ----------
S = Sheet('OFFICES · WEST WALL ONTO THE GRAND HALL · THE BIG WINDOW',
          'Seen from the Grand Hall looking east. Glass from the office floor to the ceiling; 35 cm of wall at each end; panes all the same width.',
          w=2600, h=1300, s=60.0, ox=180, oy=-60)
s = S
L = 30.2; E = 0.35; M = 0.12; N = 10
pane = (L - 2 * E - (N - 1) * M) / N
s.rect(0, 5.4, L, 12.6, OLD, label=None)                          # the wall's face, floor 2 to the roof
s.d.text(s.P(L / 2, 5.8), 'roof +12.00', LINE, font(18), anchor='mm')
s.rect(0, 12.6, L, 13.2, OLD2)
x = E
for i in range(N):
    s.rect(x, 6.6, x + pane, 12.0, GLASS, out=WHITE, wdt=2)
    x += pane
    if i < N - 1: s.rect(x, 6.6, x + M, 12.0, WHITE, out=WHITE, wdt=1); x += M
s.line((0, 12.0), (L, 12.0), NOTE, 2); s.line((0, 6.6), (L, 6.6), NOTE, 2)
s.d.text(s.P(-0.5, 12.0), '+6.00 office floor', NOTE, font(18), anchor='rm'); s.d.text(s.P(-0.5, 6.6), '+11.40 ceiling', NOTE, font(18), anchor='rm')
s.d.text(s.P(0, 13.6), 'SOUTH (kerb side)', LINE, font(18), anchor='lm'); s.d.text(s.P(L, 13.6), 'NORTH (towards the floor-2 rooms)', LINE, font(18), anchor='rm')
s.dim(0, E, 4.6, '0.35'); s.dim(L - E, L, 4.6, '0.35'); s.dim(E, E + pane, 3.9, '%.2f' % pane); s.dim(0, L, 14.6, '%.1f m of wall along the Grand Hall' % L)
s.note((E / 2, 8.5), (300, 1060), 'Ends: 35 cm of wall left at each end of the window, south and north.')
s.note((E + pane + M / 2, 9.5), (900, 1060), 'Frames between the panes like the main hall\'s big windows, %d frames of %d cm, every gap the same.' % (N - 1, round(M * 100)))
s.note((L / 2, 8.7), (1500, 1060), 'Glass: the same glass as the terminal\'s windows, %d panes each %.2f m wide, floor (+6.00) to ceiling (+11.40), flush with the wall\'s face. From the offices you look down over the Grand Hall.' % (N, pane))
s.legend(140, 1110, w=2300)
s.save('understood-2-offices-window.png')

# ---------- 3. the west end from the kerb: museum door, escalator ----------
S = Sheet('WEST END · FROM THE KERB LOOKING NORTH · MUSEUM AND ESCALATOR ROOM',
          'The museum\'s door in its outside wall; the escalator from the Grand Hall (no wall, no door) up to the Floor 2 landing.',
          w=2600, h=1080, s=24.0, ox=200, oy=-380)
s = S
s.rect(0, 25.0, 36, 37.0, NEW, out=NEWL, wdt=3)
s.d.text(s.P(18, 28), 'MUSEUM', NEWL, font(28, True), anchor='mm')
s.rect(15.5, 33.5, 20.5, 37.0, GLASS, out=WHITE, wdt=2); s.d.text(s.P(18, 32.8), 'museum door (outside wall, kerb side)', WHITE, font(16), anchor='mm')
s.rect(36, 25.0, 72, 37.0, NEW, out=NEWL, wdt=3)
s.d.text(s.P(54, 26.3), 'ESCALATOR ROOM', NEWL, font(26, True), anchor='mm')
s.line((36, 31.0), (57, 31.0), NEWL, 4); s.d.text(s.P(46.5, 30.2), 'landing, Floor 2 (+6.00)', NEWL, font(16), anchor='mm')
s.line((57, 31.0), (67, 37.0), (255, 120, 220), 6); s.d.text(s.P(64, 33.3), 'escalator up', (255, 160, 230), font(17), anchor='mm')
s.line((67, 37.0), (72, 37.0), (255, 120, 220), 6)
s.d.text(s.P(64.5, 28.3), 'open to the roof', NEWL, font(16), anchor='mm')
s.rect(72, 25.0, 80, 37.0, OLD); s.d.text(s.P(76, 27.5), 'GRAND', WHITE, font(20, True), anchor='mm'); s.d.text(s.P(76, 29), 'HALL', WHITE, font(20, True), anchor='mm')
s.line((72, 25.2), (72, 37.0), BG, 8); s.d.text(s.P(72.4, 33.5), 'no wall', (255, 160, 230), font(16), anchor='lm')
s.line((-2, 37.0), (82, 37.0), WHITE, 3); s.line((-2, 31.0), (82, 31.0), (120, 150, 180), 1)
s.d.text(s.P(-1, 37.0), '±0.00 Floor 1', NOTE, font(18), anchor='rm'); s.d.text(s.P(-1, 31.0), '+6.00 Floor 2', NOTE, font(18), anchor='rm'); s.d.text(s.P(-1, 25.0), '+12.00 roof', NOTE, font(18), anchor='rm')
s.dim(0, 36, 38.6, '36.0 museum'); s.dim(36, 57, 38.6, '21.0 landing'); s.dim(57, 72, 38.6, '15.0 open to the roof')
s.note((18, 35), (420, 860), 'MUSEUM DOOR in its south outside wall (facing the kerb, away from the apron), on the passenger side: the museum is entered from outside only.')
s.note((72, 34), (1250, 860), 'ESCALATOR ENTRANCE on Floor 1 from the Grand Hall: no door and no wall in front of it, the opening as wide as the walkway.')
s.note((57, 31.0), (1950, 860), 'The escalator starts at Floor 1 and ends exactly level with Floor 2 (+6.00) at the landing - no step, no gap.')
s.legend(160, 910, w=2300)
s.save('understood-3-west-end.png')

# ---------- 4. the east end: lift from the staff hallway into the offices ----------
S = Sheet('EAST END · OFFICES AND LIFT WING · SEEN FROM THE KERB LOOKING NORTH',
          'A working lift from the staff area\'s hallway (Floor 1) up to the offices (Floor 2).',
          w=2600, h=1080, s=24.0, ox=200, oy=-380)
s = S
s.rect(0, 25.0, 42, 31.0, NEW, out=NEWL, wdt=3); s.d.text(s.P(21, 28), 'OFFICES (Floor 2)', NEWL, font(26, True), anchor='mm')
s.rect(0, 31.0, 42, 37.0, OLD); s.d.text(s.P(21, 33.2), 'STAFF WING (Floor 1)', WHITE, font(22, True), anchor='mm')
s.line((2, 35.0), (42, 35.0), (150, 180, 210), 2); s.d.text(s.P(30, 35.8), 'staff area hallway', LINE, font(16), anchor='mm')
s.rect(42, 23.8, 54, 37.0, NEW, out=NEWL, wdt=3); s.d.text(s.P(48, 25.2), 'LIFT WING', NEWL, font(20, True), anchor='mm')
s.rect(45, 26.0, 51, 37.0, (40, 120, 120), out=NEWL, wdt=2)
s.rect(42, 33.0, 43, 37.0, GLASS, out=WHITE); s.d.text(s.P(43.3, 32.5), 'lift door, Floor 1 (from the hallway)', WHITE, font(15), anchor='lm')
s.rect(42, 27.0, 43, 31.0, GLASS, out=WHITE); s.d.text(s.P(43.3, 26.5), 'lift door, Floor 2 (into the offices)', WHITE, font(15), anchor='lm')
s.line((-2, 37.0), (58, 37.0), WHITE, 3)
s.d.text(s.P(-1, 37.0), '±0.00 Floor 1', NOTE, font(18), anchor='rm'); s.d.text(s.P(-1, 31.0), '+6.00 Floor 2', NOTE, font(18), anchor='rm'); s.d.text(s.P(-1, 25.0), '+12.00 roof', NOTE, font(18), anchor='rm'); s.d.text(s.P(55, 23.8), '+13.20 lift wing', NOTE, font(18), anchor='lm')
s.dim(0, 42, 38.6, '42.0 offices over the staff wing'); s.dim(42, 54, 38.6, '12.0 lift wing')
s.note((42.5, 35), (500, 860), 'The lift is reached from the staff area\'s hallway on Floor 1 (staff only), through its door there.')
s.note((48, 30), (860, 860), 'A working lift: call it, the doors open and close, it rides between Floor 1 and Floor 2.')
s.note((42.5, 29), (1800, 860), 'On Floor 2 the lift\'s door opens straight into the offices - the offices\' way in.')
s.legend(160, 910, w=2300)
s.save('understood-4-east-end.png')
