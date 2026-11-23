# To-do 213 detailed renders: gold NEW labels, grey EXISTING labels and a black caption bar with light text (Pillow).
#   python annotate.py <raw dir> <out dir> <shot name> "<caption>" ["<second line>"]
import json, os, sys
from PIL import Image, ImageDraw, ImageFont
RAW, OUT, shot, cap = sys.argv[1:5]; sub = sys.argv[5] if len(sys.argv) > 5 else ''
im = Image.open(os.path.join(RAW, shot + '_raw.png')).convert('RGB'); W, H = im.size; s = W / 3840.0
def font(sz, bold=False): return ImageFont.truetype('C:/Windows/Fonts/segoeui%s.ttf' % ('b' if bold else ''), int(sz * s))
F, FB, FC = font(36, True), font(54, True), font(34)
d = ImageDraw.Draw(im, 'RGBA'); GOLD, GREY = (255, 196, 80), (205, 216, 226)
lp = os.path.join(RAW, shot + '_labels.json'); placed = []
Ls = json.load(open(lp)) if os.path.exists(lp) else []
for L in sorted(Ls, key=lambda l: l['y']):
    c = GOLD if L['kind'] == 'NEW' else GREY
    x, y = L['x'] * W, L['y'] * H; t = (L['kind'] + '  ' if L['kind'] == 'NEW' else 'TODAY  ') + L['text']; b = d.textbbox((0, 0), t, font=F); w, h = b[2] + 36 * s, b[3] + 22 * s
    best = None
    for k in range(60):                                     # the nearest free place: straight up, then sideways, then down
        for dx in (0, -0.35, 0.35, -0.8, 0.8):
            bx, by = x - w / 2 + dx * w, y - (70 + k * 22) * s - h / 2
            if by < 8 * s: continue
            bx = min(max(bx, 10 * s), W - w - 10 * s)
            if all(bx > p[2] + 8 * s or bx + w < p[0] - 8 * s or by > p[3] + 8 * s or by + h < p[1] - 8 * s for p in placed): best = (bx, by); break
        if best: break
    if not best: continue
    bx, by = best; placed.append((bx, by, bx + w, by + h)); L['b'] = (bx, by, w, h, t, c, x, y)
for L in Ls:
    if 'b' in L: bx, by, w, h, t, c, x, y = L['b']; d.line([(x, y), (min(max(x, bx), bx + w), by + h)], fill=c + (235,), width=max(2, int(4 * s))); d.ellipse([x - 9 * s, y - 9 * s, x + 9 * s, y + 9 * s], fill=c + (255,), outline=(0, 0, 0, 255))
for L in Ls:
    if 'b' in L: bx, by, w, h, t, c, x, y = L['b']; d.rectangle([bx, by, bx + w, by + h], fill=(8, 10, 13, 225), outline=c + (255,), width=max(2, int(3 * s))); d.text((bx + 18 * s, by + 8 * s), t, font=F, fill=c)
bar = int(150 * s); out = Image.new('RGB', (W, H + bar), (0, 0, 0)); out.paste(im, (0, 0)); d = ImageDraw.Draw(out)
d.text((40 * s, H + 14 * s), cap, font=FB, fill=(235, 240, 245)); d.text((40 * s, H + 90 * s), sub or 'Gold line + NEW = new in the plan (v2 sheets), nothing built yet. TODAY = the game's own terminal, jetways and A320. Road, cars, coaches, people and grass are simple stand-ins.', font=FC, fill=(160, 172, 184))
os.makedirs(OUT, exist_ok=True); out.save(os.path.join(OUT, shot + '.png')); print(os.path.join(OUT, shot + '.png'))
