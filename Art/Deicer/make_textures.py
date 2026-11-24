# The de-icer's own pictures: dials (one row: speed, fuel, type I temperature, type I pressure, type IV pressure, heater), the two screens, the boom's reach plate. Everything else is the photo sets of
# Art/PassengerStairs and Art/PushbackTug.      python Art/Deicer/make_textures.py
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter
T = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Textures')
def out(n, im, rough=90):
    d = os.path.join(T, n); os.makedirs(d, exist_ok=True); im.save(os.path.join(d, n + '_2K-JPG_Color.jpg'), quality=93)
    Image.new('L', (64, 64), rough).save(os.path.join(d, n + '_2K-JPG_Roughness.jpg')); Image.new('RGB', (64, 64), (128, 128, 255)).save(os.path.join(d, n + '_2K-JPG_NormalGL.jpg'))
def font(s): return ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', s)
im = Image.new('RGB', (3072, 512), (10, 11, 13)); d = ImageDraw.Draw(im)
def dial(i, lo, hi, step, sub, title, unit, red, face=(16, 17, 20), ink=(240, 240, 236)):
    cx, cy, R = 256 + 512 * i, 256, 240; d.ellipse((cx - R, cy - R, cx + R, cy + R), fill=face, outline=(150, 152, 156), width=4); A = lambda v: math.radians(225 - 270 * (v - lo) / (hi - lo))
    d.arc((cx - R + 14, cy - R + 14, cx + R - 14, cy + R - 14), -math.degrees(A(red[0])), -math.degrees(A(red[1])), fill=(205, 30, 24), width=12)
    for k in range(int(round((hi - lo) / sub)) + 1):
        v = lo + k * sub; big = abs(v / step - round(v / step)) < 1e-6; a = A(v); r0 = R - (40 if big else 26)
        d.line([(cx + r0 * math.cos(a), cy - r0 * math.sin(a)), (cx + (R - 12) * math.cos(a), cy - (R - 12) * math.sin(a))], fill=ink, width=7 if big else 4)
        if big: d.text((cx + (R - 68) * math.cos(a), cy - (R - 68) * math.sin(a)), '%g' % v, font=font(32), fill=ink, anchor='mm')
    g = tuple(int(c * .8) for c in ink); d.text((cx, cy - 62), title, font=font(24), fill=g, anchor='mm'); d.text((cx, cy + 96), unit, font=font(26), fill=g, anchor='mm')
W, K = (238, 238, 232), (24, 24, 26)
dial(0, 0, 100, 20, 5, 'SPEED', 'km/h', (90, 100)); dial(1, 0, 100, 25, 5, 'FUEL', '%', (0, 15)); dial(2, 0, 100, 20, 5, 'TYPE I', '\N{DEGREE SIGN}C', (85, 100), W, K); dial(3, 0, 16, 4, 1, 'TYPE I', 'bar', (12, 16), W, K); dial(4, 0, 10, 2, .5, 'TYPE IV', 'bar', (7, 10), W, K); dial(5, 0, 120, 20, 5, 'HEATER', '\N{DEGREE SIGN}C', (95, 120), W, K)
out('DialsDI', im.filter(ImageFilter.SMOOTH), 60)
s = Image.new('RGB', (1024, 256), (5, 14, 20)); d = ImageDraw.Draw(s)
def bar(x, lab, f, col): d.rectangle((x, 60, x + 44, 220), outline=(150, 170, 180), width=3); d.rectangle((x + 4, 216 - int(152 * f), x + 40, 216), fill=col); d.text((x + 22, 238), lab, font=font(20), fill=(200, 215, 222), anchor='mm')
for o in (0, 512): d.rectangle((o, 0, o + 511, 34), fill=(120, 62, 6)); d.text((o + 12, 17), 'DE-ICER' if o == 0 else 'BOOM', font=font(22), fill=(245, 240, 230), anchor='lm'); d.text((o + 500, 17), 'HEATER 82\N{DEGREE SIGN}C' if o == 0 else 'SPRAY OFF', font=font(22), fill=(245, 240, 230), anchor='rm')
bar(24, 'I', .82, (214, 120, 20)); bar(96, 'H2O', .65, (40, 130, 210)); bar(168, 'IV', .74, (60, 170, 70)); d.text((250, 80), 'MIX 60 / 40', font=font(34), fill=(235, 245, 248), anchor='lm'); d.text((250, 130), 'FREEZE -28\N{DEGREE SIGN}C', font=font(26), fill=(160, 190, 200), anchor='lm'); d.text((250, 176), 'BOOM STOWED', font=font(26), fill=(120, 230, 140), anchor='lm')
d.line([(540, 215), (560, 215), (560, 190), (760, 70)], fill=(230, 170, 60), width=8); d.rectangle((752, 62, 796, 116), outline=(235, 245, 248), width=4); d.rectangle((530, 200, 640, 226), outline=(160, 190, 200), width=4); bar(850, 'I', .82, (214, 120, 20)); bar(920, 'IV', .74, (60, 170, 70)); out('ScreenDI', s, 40)
for f in os.listdir(T): print(f)
open(os.path.join(T, 'SOURCES.md'), 'w').write('All drawn by make_textures.py (our own).\n')
