# The test findings as one PDF (owner, 2026-11-16: every test's findings in one continuous file, sent to him as a PDF;
# black background, light text): docs/Test-Findings.md -> docs/Test-Findings.pdf. Markdown kept simple: "# " a title,
# "## " a section, "### " a sub-heading, "- " a point (indented "  - " under it), "    " a code or log line, else text.
#   python Tools/make_findings_pdf.py          (docs/Test-Findings.pdf)
#   python Tools/make_findings_pdf.py --mail   (Saved/Test-Findings-mail.pdf: standard PDF fonts, about 9 KB - small
#                                               enough to attach to an email through the mail tool)
import os, textwrap
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import stringWidth

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC, OUT = os.path.join(ROOT, 'docs', 'Test-Findings.md'), os.path.join(ROOT, 'docs', 'Test-Findings.pdf')
F = 'C:/Windows/Fonts/'
import sys
MAIL = '--mail' in sys.argv
if MAIL:
    OUT = os.path.join(ROOT, 'Saved', 'Test-Findings-mail.pdf')
    pdfmetrics.registerFont = lambda *a: None
else:
    pdfmetrics.registerFont(TTFont('UI', F + 'segoeui.ttf')); pdfmetrics.registerFont(TTFont('UIB', F + 'segoeuib.ttf')); pdfmetrics.registerFont(TTFont('Mono', F + 'consola.ttf'))
FN = {'UI': 'Helvetica', 'UIB': 'Helvetica-Bold', 'Mono': 'Courier'} if MAIL else {'UI': 'UI', 'UIB': 'UIB', 'Mono': 'Mono'}
W, H = A4; M = 48
BG, FG, DIM, CY, PK = HexColor('#101018'), HexColor('#ECECF4'), HexColor('#A9A9BD'), HexColor('#35F0FF'), HexColor('#FF3FB8')
c = canvas.Canvas(OUT, pagesize=A4, pageCompression=1); c.setTitle('Airport Dynasty Pro - test findings')
y = 0
def page():
    global y
    c.setFillColor(BG); c.rect(0, 0, W, H, fill=1, stroke=0); y = H - M
def need(h):
    global y
    if y - h < M: c.showPage(); page()
def wrap(text, font, size, width):
    words, lines, cur = text.split(' '), [], ''
    for w in words:
        t = (cur + ' ' + w).strip()
        if stringWidth(t, FN.get(font, font), size) <= width: cur = t
        else:
            if cur: lines.append(cur)
            cur = w
    if cur: lines.append(cur)
    return lines or ['']
def para(text, font='UI', size=10, col=FG, indent=0, gap=4, bullet=None):
    global y
    lines = wrap(text, font, size, W - 2 * M - indent - (12 if bullet else 0))
    for i, l in enumerate(lines):
        need(size + 4)
        c.setFillColor(col); c.setFont(FN.get(font, font), size)
        if bullet and i == 0: c.setFillColor(CY); c.drawString(M + indent, y - size, bullet); c.setFillColor(col)
        c.drawString(M + indent + (12 if bullet else 0), y - size, l)
        y -= size + 3
    y -= gap
page()
for raw in open(SRC, encoding='utf-8').read().split('\n'):
    line = raw.rstrip()
    if not line.strip(): y -= 4; continue
    if line.startswith('# '): need(40); para(line[2:], 'UIB', 20, PK, gap=10)
    elif line.startswith('## '): need(34); y -= 6; para(line[3:], 'UIB', 14, CY, gap=6)
    elif line.startswith('### '): need(24); para(line[4:], 'UIB', 11, FG, gap=3)
    elif line.startswith('    '): para(line.strip(), 'Mono', 8, DIM, indent=14, gap=1)
    elif line.lstrip().startswith('- '):
        ind = (len(line) - len(line.lstrip())) // 2 * 14
        para(line.lstrip()[2:].replace('**', ''), 'UI', 10, FG, indent=ind, gap=2, bullet='-')
    else: para(line.replace('**', ''), 'UI', 10, FG)
c.save()
print('written', OUT)
