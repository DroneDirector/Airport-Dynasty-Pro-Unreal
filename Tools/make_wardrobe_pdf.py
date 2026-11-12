# docs/Wardrobe-Plan.md as a PDF the owner keeps on his tablet (dark, narrow, same look as the to-do checklist):
#   python Tools/make_wardrobe_pdf.py   (run in docs/)
import re
from pathlib import Path
from reportlab.lib.pagesizes import mm
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase.pdfmetrics import stringWidth

SRC = Path(__file__).resolve().parent.parent / 'docs' / 'Wardrobe-Plan.md'
OUT = SRC.with_suffix('.pdf')
W, H = 110 * mm, 230 * mm
BG, FG, DIM, RULE, HEAD = HexColor('#1b1b1f'), white, HexColor('#b0b0b8'), HexColor('#44444c'), HexColor('#8ab4f8')
L, R = 8 * mm, W - 6 * mm
c = canvas.Canvas(str(OUT), pagesize=(W, H))
c.setTitle('Airport Dynasty Pro - the wardrobe plan')
y = 0

def page():
    global y
    c.setFillColor(BG); c.rect(0, 0, W, H, fill=1, stroke=0)
    y = H - 12 * mm

def need(h):
    global y
    if y - h < 10 * mm:
        c.showPage(); page()

def wrap(text, width, font, size):
    words, lines, cur = text.split(), [], ''
    for w in words:
        t = (cur + ' ' + w).strip()
        if stringWidth(t, font, size) <= width: cur = t
        else: lines.append(cur); cur = w
    if cur: lines.append(cur)
    return lines

def para(text, font='Helvetica', size=7.5, col=FG, indent=0, gap=4):
    global y
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    for ln in wrap(text, R - L - indent, font, size):
        need(size + 3)
        c.setFont(font, size); c.setFillColor(col); c.drawString(L + indent, y - size, ln)
        y -= size + 2.5
    y -= gap

def table(rows):
    global y
    cols = len(rows[0])
    # the last column narrow (hours), the first a third, the middle the rest
    widths = [(R - L) * 0.3, (R - L) * 0.56, (R - L) * 0.14] if cols == 3 else [(R - L) * 0.76, (R - L) * 0.24]
    for i, row in enumerate(rows):
        bold = i == 0 or row[0].startswith('**')
        cells = [re.sub(r'\*\*(.+?)\*\*', r'\1', x.strip()) for x in row]
        font = 'Helvetica-Bold' if bold else 'Helvetica'
        lines = [wrap(cells[k], widths[k] - 4, font, 7) for k in range(cols)]
        h = max(len(l) for l in lines) * 8.5 + 4
        need(h)
        x = L
        for k in range(cols):
            c.setFont(font, 7); c.setFillColor(DIM if i == 0 else FG)
            for j, ln in enumerate(lines[k]):
                if k == cols - 1: c.drawRightString(x + widths[k] - 2, y - 7 - j * 8.5, ln)
                else: c.drawString(x + 1, y - 7 - j * 8.5, ln)
            x += widths[k]
        c.setStrokeColor(RULE); c.line(L, y - h + 1, R, y - h + 1)
        y -= h
    y -= 6

page()
rows = []
# the source's lines joined into paragraphs (a blank line, a heading or a table row ends one)
joined, buf = [], []
for raw in SRC.read_text(encoding='utf-8').splitlines() + ['']:
    s = raw.rstrip()
    if s and not s.startswith(('|', '#')): buf.append(s.strip()); continue
    if buf: joined.append(' '.join(buf)); buf = []
    joined.append(s)
for s in joined:
    if s.startswith('|'):
        cells = s.strip('|').split('|')
        if all(set(x.strip()) <= set('-: ') for x in cells): continue
        rows.append(cells); continue
    if rows: table(rows); rows = []
    if not s: continue
    if s.startswith('# '): need(20); c.setFont('Helvetica-Bold', 11); c.setFillColor(FG); c.drawString(L, y - 11, s[2:]); y -= 20
    elif s.startswith('## '): y -= 2; need(16); c.setFont('Helvetica-Bold', 8.5); c.setFillColor(HEAD); c.drawString(L, y - 9, s[3:]); y -= 15
    else: para(s)
if rows: table(rows)
c.showPage(); c.save(); print('wrote', OUT)
