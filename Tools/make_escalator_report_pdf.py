# The new escalator - a short write-up for the owner (2026-11-18), black background (his rule).
# Out: docs/Escalator-New-2026-11-18.pdf
import os
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.pdfbase.pdfmetrics import stringWidth
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'Escalator-New-2026-11-18.pdf')
W, H = A4; M = 48
BG, INK, DIM, HEAD = HexColor('#000000'), HexColor('#e8e8ec'), HexColor('#9aa0aa'), HexColor('#78e6aa')
SECTIONS = [
 ('What it is', ['Every escalator in the terminal is new (the Grand Hall pair and the arrivals pair), modelled on your photos and the concept you approved. Same height and slope as before (6.0 m, 30 degrees, 1 m steps), sitting flush in the ground floor and in the Floor 2 floor.']),
 ('The handrail', ['A black rubber C-profile that runs as a real loop: along the top of the glass, round a smooth round newel (a full half circle, 34 cm radius) at BOTH ends - on the ground floor and on the upper floor - and back underneath into a black rubber inlet on the decking with a stainless lip and rubber lips. It runs with the steps.']),
 ('The glass', ['The terminal\'s own glass (unchanged, as you asked). It sits inside the handrail all the way round its round ends; its lower edge rises gently along the flat ends to just over the handrail\'s return, with brushed stainless below.']),
 ('Steps and metal', ['Steps: grooved, cleated, yellow edges at the front and sides, about 5 % reflective (matte). Metal - skirts, decking, clamps, floor plates, cladding: brushed stainless, about 25 % reflective, the brushing laid at real size (50 cm tiles), seamless, no stretching.']),
 ('Small parts', ['Countersunk screws on the floor plates; a red emergency-stop mushroom on a yellow plate and a key switch at each end; green / red direction lights low in the skirts; black skirt brushes along both skirts; anti-slide studs on the decking between a pair; the slots at the ends closed (no see-through).']),
 ('Crown lights', ['A 3.2 m brushed-stainless crown ring with 20 crown points (glowing tips), a warm LED band round its inside, four spokes to a hub, hung on four thin steel cables from ceiling roses - one over the Grand Hall escalators, one over the arrivals escalators. Performance: the crown only glows by its material; its real light is ONE terminal lamp under the hub (distance-limited, like the others), no flares, no extra shadow lights.']),
 ('Files', ['Model: Art/Escalator/make_escalator.py (the arrivals pair: ESC_SHORT=1), Art/Escalator/Escalator.blend, EscalatorShort.blend. Crown: Art/TerminalAnnex/make_crown_light.py. Concept: docs/Blueprints/EscalatorNew. Pictures (8K PNG): docs/Pictures/Escalator/escalator-v2-8K.']),
 ('Still open', ['To-do 109: the stair beside the arrivals escalators - its handrail down to a small screwed floor base (on hold until you say go).']),
]
def wrap(t, size, width):
    out, line = [], ''
    for w_ in t.split(' '):
        tryl = (line + ' ' + w_).strip()
        if stringWidth(tryl, 'Helvetica', size) > width: out.append(line); line = w_
        else: line = tryl
    if line: out.append(line)
    return out
c = canvas.Canvas(OUT, pagesize=A4)
def page(): c.setFillColor(BG); c.rect(0, 0, W, H, stroke=0, fill=1)
page(); y = H - M
c.setFillColor(INK); c.setFont('Helvetica-Bold', 20); c.drawString(M, y - 20, 'Airport Dynasty Pro - the new escalator'); y -= 34
c.setFillColor(DIM); c.setFont('Helvetica', 10); c.drawString(M, y - 10, '2026-11-18 - what it is made of, and its crown lights'); y -= 34
for head, paras in SECTIONS:
    if y < M + 80: c.showPage(); page(); y = H - M
    c.setFillColor(HEAD); c.setFont('Helvetica-Bold', 13); c.drawString(M, y - 13, head); y -= 22
    for p_ in paras:
        for ln in wrap(p_, 10, W - 2 * M):
            c.setFillColor(INK); c.setFont('Helvetica', 10); c.drawString(M, y - 10, ln); y -= 13.5
        y -= 8
c.save(); print(OUT)
