# A Markdown document as a PDF in the owner's document style (memory documents-black-background: black background,
# light text): headings, paragraphs, bullet/numbered lists (nested), tables, **bold**, *italic*. A4, wrapped text.
#   python Tools/md_to_pdf.py docs/Arrival-Flow-Plan.md [out.pdf]
import re, sys, os
from reportlab.lib.pagesizes import A4, mm
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.pdfbase.pdfmetrics import stringWidth

BG, INK, DIM, HEAD, ACC, LINE = HexColor('#05080c'), HexColor('#e8eef5'), HexColor('#8a99a8'), HexColor('#7fd4ff'), HexColor('#ffd166'), HexColor('#2a3644')
W, H = A4; M = 16 * mm; TW = W - 2 * M

class Doc:
    def __init__(self, out):
        self.c = canvas.Canvas(out, pagesize=A4); self.y = 0; self.page = 0; self.new_page()
    def new_page(self):
        if self.page: self.c.showPage()
        self.page += 1; self.c.setFillColor(BG); self.c.rect(0, 0, W, H, fill=1, stroke=0)
        self.c.setFillColor(DIM); self.c.setFont('Helvetica', 7.5); self.c.drawRightString(W - M, 9 * mm, str(self.page))
        self.y = H - M
    def need(self, h):
        if self.y - h < 16 * mm: self.new_page()
    # runs of (text, bold, italic)
    @staticmethod
    def runs(text):
        out = []; pos = 0
        for m in re.finditer(r'\*\*(.+?)\*\*|\*(.+?)\*', text):
            if m.start() > pos: out.append((text[pos:m.start()], False, False))
            out.append((m.group(1), True, False) if m.group(1) is not None else (m.group(2), False, True)); pos = m.end()
        if pos < len(text): out.append((text[pos:], False, False))
        return out
    @staticmethod
    def font(b, i, size): return ('Helvetica-BoldOblique' if b and i else 'Helvetica-Bold' if b else 'Helvetica-Oblique' if i else 'Helvetica'), size
    def wrap(self, text, width, size):
        words = []
        for t, b, i in self.runs(text):
            for k, w in enumerate(re.split(r'(\s+)', t)):
                if w: words.append((w, b, i))
        lines, cur, cw = [], [], 0.0
        for w, b, i in words:
            f, s = self.font(b, i, size); ww = stringWidth(w, f, s)
            if w.isspace():
                if cur: cur.append((' ', b, i)); cw += ww
                continue
            if cw + ww > width and cur:
                while cur and cur[-1][0] == ' ': cur.pop()
                lines.append(cur); cur, cw = [], 0.0
            cur.append((w, b, i)); cw += ww
        if cur: lines.append(cur)
        return lines
    def draw_lines(self, lines, x, size, color, lead):
        for ln in lines:
            self.need(lead); xx = x
            for w, b, i in ln:
                f, s = self.font(b, i, size); self.c.setFont(f, s); self.c.setFillColor(ACC if b and color == INK else color)
                self.c.drawString(xx, self.y - size, w); xx += stringWidth(w, f, s)
            self.y -= lead
    def para(self, text, x=M, width=TW, size=9.5, color=INK, after=3):
        self.draw_lines(self.wrap(text, width, size), x, size, color, size * 1.38); self.y -= after
    def heading(self, text, level):
        size = {1: 18, 2: 13.5, 3: 11.5}.get(level, 11); self.need(size * 2.4)
        self.y -= 6 if level > 1 else 0
        self.c.setFont('Helvetica-Bold', size); self.c.setFillColor(HEAD if level > 1 else INK)
        for ln in self.wrap(text, TW, size): self.c.drawString(M, self.y - size, ''.join(w for w, _, _ in ln)); self.y -= size * 1.3
        if level <= 2: self.c.setStrokeColor(LINE); self.c.setLineWidth(0.6); self.c.line(M, self.y - 1, W - M, self.y - 1); self.y -= 5
        self.y -= 3
    def bullet(self, text, depth, marker):
        x = M + 4 * mm + depth * 6 * mm; size = 9.5
        lines = self.wrap(text, TW - (x - M) - 4 * mm, size)
        self.need(size * 1.38); self.c.setFont('Helvetica-Bold' if marker != '•' else 'Helvetica', size); self.c.setFillColor(HEAD)
        self.c.drawRightString(x - 1.5 * mm, self.y - size, marker)
        self.draw_lines(lines, x, size, INK, size * 1.38); self.y -= 1.5
    def table(self, rows):
        size = 8.2; n = len(rows[0]); lens = [max(len(re.sub(r'\*', '', r[k])) for r in rows) + 4 for k in range(n)]
        tot = sum(lens); widths = [TW * l / tot for l in lens]
        for ri, r in enumerate(rows):
            cells = [self.wrap(t, widths[k] - 3 * mm, size) for k, t in enumerate(r)]
            h = max(len(c) for c in cells) * size * 1.32 + 3 * mm; self.need(h)
            if ri == 0: self.c.setFillColor(HexColor('#0f1a26')); self.c.rect(M, self.y - h, TW, h, fill=1, stroke=0)
            x = M; y0 = self.y
            for k, cl in enumerate(cells):
                yy = y0 - 1.5 * mm
                for ln in cl:
                    xx = x + 1.5 * mm
                    for w, b, i in ln:
                        f, s = self.font(b or ri == 0, i, size); self.c.setFont(f, s); self.c.setFillColor(HEAD if ri == 0 else INK)
                        self.c.drawString(xx, yy - size, w); xx += stringWidth(w, f, s)
                    yy -= size * 1.32
                x += widths[k]
            self.y -= h; self.c.setStrokeColor(LINE); self.c.setLineWidth(0.4); self.c.line(M, self.y, W - M, self.y)
        self.y -= 5

def convert(src, out):
    d = Doc(out); lines = open(src, encoding='utf-8').read().splitlines(); i = 0; num = {}
    while i < len(lines):
        ln = lines[i]; s = ln.strip()
        if not s: i += 1; continue
        m = re.match(r'^(#{1,4})\s+(.*)', s)
        if m: d.heading(m.group(2), len(m.group(1))); num = {}; i += 1; continue
        if s.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                cells = [c.strip() for c in lines[i].strip().strip('|').split('|')]
                if not all(re.fullmatch(r':?-+:?', c) for c in cells): rows.append(cells)
                i += 1
            d.table(rows); continue
        m = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)', ln)
        if m:
            depth = len(m.group(1).expandtabs(4)) // 2 // 1
            depth = min(depth // 2 if depth > 1 else depth, 3)
            mk = m.group(2); marker = '•' if mk in '-*' else mk
            d.bullet(m.group(3), depth, marker); i += 1; continue
        d.para(s); i += 1
    d.c.save()

if __name__ == '__main__':
    src = sys.argv[1]; out = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(src)[0] + '.pdf'
    convert(src, out); print(out)
