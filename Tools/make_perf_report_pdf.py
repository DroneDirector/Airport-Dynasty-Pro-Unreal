# The performance report for the owner (2026-11-18): what made the game slow lately, what was done about it, what is
# still open, and what he can ask for - black background, light text (his rule). Out: docs/Performance-Report-2026-11-18.pdf
#   python Tools/make_perf_report_pdf.py
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.pdfbase.pdfmetrics import stringWidth
import os
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'docs', 'Performance-Report-2026-11-18.pdf')
W, H = A4; M = 48
BG, INK, DIM, HEAD, OK, OPEN = HexColor('#000000'), HexColor('#e8e8ec'), HexColor('#9aa0aa'), HexColor('#4fc3f7'), HexColor('#81c784'), HexColor('#ffb74d')

SECTIONS = [
 ('Fixed', OK, [
  ('Night lag: the lamp flares (2026-11-17)',
   'Problem: after the 16 gates the game felt very laggy, worst at night (34-38 fps at night against 87 by day). '
   'Cause: the lamp flares were meant to be off by default but were on whenever the setting was missing; every street light and mast had its own sprite prepared by the processor every frame. '
   'Fix: off unless you switch them on; when on, only those within 600 m. Also moving parts (doors, jetways, belts, crowd, escalators) stopped rebuilding their whole lists every frame. '
   'Result: night 36 -> 87 fps (flares off), 86 fps (flares on).'),
  ('The stutter every 15 seconds: the seats (2026-11-16)',
   'Problem: a 120 ms hitch every 15 s, even on the title screen. '
   'Cause: the seat system searched every piece of every building for chairs again every 15 s (112 ms each time). '
   'Fix: it only searches when a building is added or removed. Result: bad frames 26 -> 0, worst frame 133 -> 50 ms.'),
  ('Shadows and lamps reached too far (2026-11-16, your OK)',
   'Problem: the sun\'s shadows were drawn 42 km out and 963 lamps had no distance limit, on foot as well. '
   'Fix: on foot or driving shadows reach 2.6 km, lamps 150 m, floodlights 600 m; flying, drone or high up: 42 km, 600 m, 2.4 km. '
   'Result: 87 -> 92 fps average, slowest 1 % 68 -> 75 fps; nothing went dark that should be lit.'),
  ('Roads drawn the slow way (2026-11-16)',
   'Problem: 607 road meshes (up to 26,000 triangles each, all casting shadows) were ordinary meshes in the slow shadow queue. '
   'Fix: made Nanite with the same triangles and materials; they look the same.'),
  ('Missing parts in the packaged game (2026-11-16)',
   'Problem: 0.17.16 shipped without the escalator, road signs, street lights and more (the game asked for them and found nothing). '
   'Fix: the release now stops if anything the game loads by name is missing from the package.'),
  ('Clouds and fps (2026-11-17, partly)',
   'Problem: Clear weather still showed broken clouds and fps fell below 20 (50-60 without clouds). '
   'Cause found: Clear was not really clear, the sky was re-captured with its clouds every frame, and the sample cloud material is heavy. '
   'Status: the cause is written up (to-do 82); the full fix with a quality slider is still open.'),
  ('Frame generation flicker in menus (2026-11-17)',
   'Problem: scrolling menus and screens inside the terminal flickered with our frame generation. '
   'Fix: no in-between pictures while a menu, Settings, the map or a device screen is open, and glass and screens no longer smear (in 0.17.22).'),
  ('Black menus at start (2026-11-16)',
   'Problem: title and menus sometimes stayed black. Cause: the browser for the menus was made during the heavy loading frame and timed out. '
   'Fix: helpers time out at 60 s and the menus open after loading.'),
 ]),
 ('Tools the game now has (so problems are found, not guessed)', HEAD, [
  ('The performance log', 'Saved/Logs/ADP-Perf.log: every bad frame with its cause (game, render thread, graphics card, memory clean-up, loading) and where the camera was; every 30 s the average fps, the slowest 1 % and the worst frame.'),
  ('The stall watch', 'Names exactly which part of the game was stuck when a frame stops for 60 ms or more.'),
  ('Time guards', 'Every frame-by-frame scan has a time budget; going over writes SLOW WORK with its name. The release stops on a new scan without a guard.'),
  ('The stress walk', 'The game walks the terminal by itself (halls, stairs, escalator, gates, jetway, apron) in the shipping build and reports.'),
  ('The stand test', 'Measures one place at a fixed day and hour (e.g. night vs day), to compare builds fairly.'),
 ]),
 ('Still open (on the to-do list)', OPEN, [
  ('82 Clouds faster + a quality slider', 'Clear = no clouds drawn, the sky captured only when the weather changes, a lighter cloud setting - same look.'),
  ('76 More fps at Cinematic', 'The game uses about 7.7 of the 8 GB graphics memory at 3840x1080; find what fills it, lighter pools without visible loss; 35 lights redraw moving shadows every frame.'),
  ('77 NVIDIA DLSS', 'DLSS as an upscaler choice with dynamic resolution; works best together with our frame generation (estimate Cinematic about 26 -> 40 real fps).'),
  ('75 No first-time freezes', 'A recorded shader cache shipped with the game, prepared behind the loading screen: no 0.1-5.6 s freezes the first time something is seen (e.g. at nightfall).'),
  ('67 Glass on Nanite meshes (needs your OK)', '12 meshes whose glass Nanite cannot draw; draw them the ordinary way so the glass shows as designed.'),
  ('Tonight\'s note (2026-11-17)', 'The stress walk gave about 20 fps for both 0.17.22 and 0.17.21 on the same evening, so the new jetways did not slow the game - the PC itself was slower then (this morning\'s walk gave 39 fps). Worth a re-run with nothing else open.'),
 ]),
 ('What you can ask me', HEAD, [
  ('"Run a stress walk"', 'in the shipping build, frame generation on, day and night, through the areas that changed; compared with the last walk.'),
  ('"Why is it slow at X?"', 'a stand test at that place and time, with the performance log and the stall watch naming the cause.'),
  ('"Compare Cinematic / Epic / High"', 'measured side by side, with pictures of the same scenes, your own setting left as it is.'),
  ('"Find more fps without losing the look"', 'every change measured; anything that changes the look waits for your OK.'),
  ('"Fix the open items"', '82 clouds, 76 Cinematic memory, 77 DLSS, 75 shader cache, 67 glass.'),
 ]),
]

def wrap(t, font, size, width):
    out, line = [], ''
    for w_ in t.split(' '):
        tryl = (line + ' ' + w_).strip()
        if stringWidth(tryl, font, size) > width: out.append(line); line = w_
        else: line = tryl
    if line: out.append(line)
    return out

c = canvas.Canvas(OUT, pagesize=A4)
def page():
    c.setFillColor(BG); c.rect(0, 0, W, H, stroke=0, fill=1)
y = [H - M]
def need(h):
    if y[0] - h < M: c.showPage(); page(); y[0] = H - M
page()
c.setFillColor(INK); c.setFont('Helvetica-Bold', 20); c.drawString(M, y[0] - 20, 'Airport Dynasty Pro - performance'); y[0] -= 30
c.setFillColor(DIM); c.setFont('Helvetica', 10); c.drawString(M, y[0] - 10, 'What made the game slow lately, what was done, what is open - 2026-11-18'); y[0] -= 34
for title, col, items in SECTIONS:
    need(40); c.setFillColor(col); c.setFont('Helvetica-Bold', 14); c.drawString(M, y[0] - 14, title); y[0] -= 26
    for head, body in items:
        lines = wrap(body, 'Helvetica', 9.5, W - 2 * M - 12)
        need(16 + 12.5 * len(lines) + 8)
        c.setFillColor(INK); c.setFont('Helvetica-Bold', 10.5); c.drawString(M + 6, y[0] - 11, head); y[0] -= 16
        c.setFillColor(DIM); c.setFont('Helvetica', 9.5)
        for ln in lines: c.drawString(M + 12, y[0] - 10, ln); y[0] -= 12.5
        y[0] -= 8
    y[0] -= 6
c.save(); print(OUT)
