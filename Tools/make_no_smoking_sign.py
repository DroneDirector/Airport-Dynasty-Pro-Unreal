# The no-smoking pictogram for the terminal entrance doors' glass (owner, 2026-11-12): our own drawing, the usual
# form (a red ring and bar over a cigarette with its smoke), on a clear background, so it reads as a sticker on glass.
#   python Tools/make_no_smoking_sign.py   ->   Art/Signs/no_smoking.png (1024 x 1024, RGBA)
from PIL import Image, ImageDraw, ImageFilter
from pathlib import Path
import math

OUT = Path(__file__).resolve().parent.parent / 'Art' / 'Signs' / 'no_smoking.png'
S = 1024
img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
RED, WHITE, GREY, ORANGE, DARK = (204, 24, 30, 255), (248, 248, 246, 255), (150, 150, 150, 255), (222, 150, 70, 255), (60, 60, 60, 255)
c, r = S / 2, S * 0.46
# a faint white disc behind (the sticker's own white, slightly see-through)
d.ellipse((c - r, c - r, c + r, c + r), fill=(255, 255, 255, 215))
# the cigarette: a white tube with the orange filter, a grey ash end and three wisps of smoke
w, h = S * 0.56, S * 0.09
x0, y0 = c - w / 2, c - h / 2
d.rounded_rectangle((x0, y0, x0 + w, y0 + h), radius=h * 0.2, fill=WHITE, outline=DARK, width=6)
d.rectangle((x0 + w * 0.72, y0 + 3, x0 + w, y0 + h - 3), fill=ORANGE)
d.rectangle((x0, y0 + 3, x0 + w * 0.08, y0 + h - 3), fill=GREY)
d.line((x0 + w * 0.72, y0, x0 + w * 0.72, y0 + h), fill=DARK, width=5)
smoke = Image.new('RGBA', (S, S), (0, 0, 0, 0))
sd = ImageDraw.Draw(smoke)
for k in range(3):
    pts = []
    for i in range(0, 40):
        t = i / 39
        pts.append((x0 - S * 0.02 - t * S * 0.11 + math.sin(t * 6.3 + k * 2.1) * S * 0.025, y0 + h / 2 - t * S * 0.14 - k * S * 0.035))
    sd.line(pts, fill=(90, 90, 90, 200), width=14)
smoke = smoke.filter(ImageFilter.GaussianBlur(5))
img.alpha_composite(smoke)
# the ring and the bar over everything
d = ImageDraw.Draw(img)
ring = int(S * 0.075)
d.ellipse((c - r, c - r, c + r, c + r), outline=RED, width=ring)
ang = math.radians(45)
dx, dy = math.cos(ang) * (r - ring / 2), math.sin(ang) * (r - ring / 2)
d.line((c - dx, c - dy, c + dx, c + dy), fill=RED, width=ring)
img.save(OUT)
print('wrote', OUT)
