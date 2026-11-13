# docs/Answers.md -> docs/Answers.pdf: the answers to the owner's questions, kept so he doesn't need to ask again
# (owner, 2026-11-13). Add new answers to the .md, then run: python Tools/make_answers_pdf.py
import re
from pathlib import Path
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.colors import HexColor
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer

ROOT = Path(__file__).resolve().parent.parent
src = (ROOT / 'docs' / 'Answers.md').read_text(encoding='utf-8')
ss = getSampleStyleSheet()
h1 = ParagraphStyle('h1', parent=ss['Title'], textColor=HexColor('#35F0FF'), fontSize=20, spaceAfter=10)   # (owner: black pages)
h2 = ParagraphStyle('h2', parent=ss['Heading2'], textColor=HexColor('#FF3FB8'), spaceBefore=12, spaceAfter=6)
body = ParagraphStyle('b', parent=ss['BodyText'], fontSize=10.5, leading=14.5, spaceAfter=7, textColor=HexColor('#E8E6F0'))
def inline(t):
    t = t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    return re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', t)
flow = []
for block in re.split(r'\n\s*\n', src):
    b = ' '.join(l.strip() for l in block.strip().splitlines())
    if not b: continue
    if b.startswith('# '): flow.append(Paragraph(inline(b[2:]), h1))
    elif b.startswith('## '): flow.append(Paragraph(inline(b[3:]), h2))
    else: flow.append(Paragraph(inline(b), body))
def black(c, d):
    c.saveState(); c.setFillColor(HexColor('#000000')); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1); c.restoreState()
SimpleDocTemplate(str(ROOT / 'docs' / 'Answers.pdf'), pagesize=A4, leftMargin=48, rightMargin=48, topMargin=48, bottomMargin=48,
                  title='Airport Dynasty Pro - Answers').build(flow, onFirstPage=black, onLaterPages=black)
print('made docs/Answers.pdf')
