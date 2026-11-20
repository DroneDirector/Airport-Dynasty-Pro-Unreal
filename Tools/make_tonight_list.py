"""The 2026-11-20 list (to-do 160-199) on its own, in working order, black background (his rule for documents).
  python -I Tools/make_tonight_list.py  ->  docs/Tonight-List.pdf"""
import sys, re
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
src = (ROOT / 'Tools/make_todo_pdf.py').read_text(encoding='utf-8')
ns = {'__file__': str(ROOT / 'Tools/make_todo_pdf.py')}
exec(src[:src.index('def header')], ns)          # only the ITEMS table
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.pdfbase.pdfmetrics import stringWidth
items = [i for i in ns['ITEMS'] if re.match(r'^(1[6-9]\d)$', i[0]) and 160 <= int(i[0]) <= 200 and not i[5]]
W, H = A4; M = 40
c = canvas.Canvas(str(ROOT / 'docs/Tonight-List.pdf'), pagesize=A4)
def page():
    c.setFillColor(HexColor('#000000')); c.rect(0, 0, W, H, fill=1, stroke=0)
def wrap(t, w, f, s):
    out, line = [], ''
    for word in t.split():
        if stringWidth(line + ' ' + word, f, s) > w and line: out.append(line); line = word
        else: line = (line + ' ' + word).strip()
    if line: out.append(line)
    return out
page(); y = H - M
c.setFillColor(HexColor('#ff4fd8')); c.setFont('Helvetica-Bold', 18); c.drawString(M, y, 'Airport Dynasty Pro - the 2026-11-20 list'); y -= 18
c.setFillColor(HexColor('#cccccc')); c.setFont('Helvetica', 9)
c.drawString(M, y, '%d items, in working order. Built at full speed; nothing published until every item is ready.' % len(items)); y -= 22
for no, title, est, groups, needs, done in items:
    lines = wrap(title, W - 2 * M - 40, 'Helvetica', 8.5)
    need = 14 + 11 * len(lines)
    if y - need < M: c.showPage(); page(); y = H - M
    c.setFillColor(HexColor('#4fe3ff')); c.setFont('Helvetica-Bold', 11); c.drawString(M, y, no)
    c.setFillColor(HexColor('#ffd34f')); c.setFont('Helvetica', 8.5); c.drawRightString(W - M, y, est if est and est != '?' else '')
    yy = y
    c.setFillColor(HexColor('#eeeeee')); c.setFont('Helvetica', 8.5)
    for l in lines: c.drawString(M + 40, yy, l); yy -= 11
    y = yy - 8
c.save(); print(len(items), 'items')
