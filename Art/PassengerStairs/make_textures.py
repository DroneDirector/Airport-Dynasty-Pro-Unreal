# The passenger stairs' own texture sets, made from ambientCG CC0 photos or drawn (no brand anywhere):
# PaintGSE (colour mottle and roughness of Paint004, grey, with our own fine orange-peel normal), LensPillow (pressed pillow optics of a lamp lens),
# Dials (speed and battery dial), Screen (the stair's own display).      C:/Python311/python.exe Art/PassengerStairs/make_textures.py
import os, math, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
HERE = os.path.dirname(os.path.abspath(__file__)); T = os.path.join(HERE, 'Textures'); P = os.path.join(HERE, '..', 'PushbackTug', 'Textures', 'Paint004', 'Paint004_2K-JPG_%s.jpg')
def out(n, s, im):
    d = os.path.join(T, n); os.makedirs(d, exist_ok=True); im.save(os.path.join(d, '%s_2K-JPG_%s.jpg' % (n, s)), quality=93)
def normal(h, k):
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * k; gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * k; nz = 1 / np.sqrt(gx * gx + gy * gy + 1)
    return Image.fromarray((np.stack([-gx * nz, gy * nz, nz], -1) * 127.5 + 127.5).astype('uint8'))
N = 1024
g = np.asarray(Image.open(P % 'Color').convert('L').resize((N, N), Image.LANCZOS)).astype(float); g = (g - g.mean()) * 1.6 + 236; out('PaintGSE', 'Color', Image.fromarray(np.clip(g, 0, 255).astype('uint8')).convert('RGB'))
r = np.asarray(Image.open(P % 'Roughness').convert('L').resize((N, N), Image.LANCZOS)).astype(float); r = (r - r.mean()) * 1.2 + 82; out('PaintGSE', 'Roughness', Image.fromarray(np.clip(r, 45, 160).astype('uint8')))
rng = np.random.default_rng(7); f = np.fft.fft2(rng.standard_normal((N, N))); k = np.fft.fftfreq(N) * N; K2 = np.sqrt(k[:, None] ** 2 + k[None] ** 2); h = np.fft.ifft2(f * np.exp(-((K2 - 130) / 60) ** 2)).real; out('PaintGSE', 'NormalGL', normal(h / h.std(), .055))
n = 512; y, x = np.mgrid[0:n, 0:n] / n; u = (x * 8) % 1 - .5; v = (y * 8) % 1 - .5; out('LensPillow', 'NormalGL', normal(-(u * u + v * v) * 24, 1.6)); e = np.maximum(abs(u), abs(v)); c = np.where(e > .46, 205, 245) + 8 * (1 - (u * u + v * v) * 2)
out('LensPillow', 'Color', Image.fromarray(np.clip(c, 0, 255).astype('uint8')).convert('RGB')); out('LensPillow', 'Roughness', Image.fromarray(np.where(e > .46, 70, 22).astype('uint8')))
mot = np.asarray(Image.open(os.path.join(T, 'PaintGSE', 'PaintGSE_2K-JPG_Color.jpg')).convert('L')).astype(float) / 236; y, x = np.mgrid[0:N, 0:N]; st = (((x + y) % (N // 2)) < N // 4)[..., None]
out('Chevron', 'Color', Image.fromarray(np.clip(np.where(st, (232, 168, 12), (26, 26, 28)) * mot[..., None], 0, 255).astype('uint8'))); [out('Chevron', q, Image.open(os.path.join(T, 'PaintGSE', 'PaintGSE_2K-JPG_%s.jpg' % q))) for q in ('NormalGL', 'Roughness')]
def font(s): return ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', s)
im = Image.new('RGB', (1024, 512), (10, 11, 13)); d = ImageDraw.Draw(im)
def dial(cx, cy, R, lo, hi, step, sub, title, unit, red):
    d.ellipse((cx - R, cy - R, cx + R, cy + R), fill=(16, 17, 20), outline=(150, 152, 156), width=4); A = lambda v: math.radians(225 - 270 * (v - lo) / (hi - lo))
    d.arc((cx - R + 14, cy - R + 14, cx + R - 14, cy + R - 14), -math.degrees(A(red[0])), -math.degrees(A(red[1])), fill=(205, 30, 24), width=10)
    for i in range(int(round((hi - lo) / sub)) + 1):
        v = lo + i * sub; big = abs(v / step - round(v / step)) < 1e-6; a = A(v); r0 = R - (40 if big else 26); d.line([(cx + r0 * math.cos(a), cy - r0 * math.sin(a)), (cx + (R - 12) * math.cos(a), cy - (R - 12) * math.sin(a))], fill=(240, 240, 236), width=6 if big else 3)
        if big: d.text((cx + (R - 64) * math.cos(a), cy - (R - 64) * math.sin(a)), '%g' % v, font=font(30), fill=(240, 240, 236), anchor='mm')
    d.text((cx, cy - 58), title, font=font(22), fill=(200, 202, 205), anchor='mm'); d.text((cx, cy + 92), unit, font=font(24), fill=(200, 202, 205), anchor='mm')
dial(256, 256, 236, 0, 30, 5, 1, 'SPEED', 'km/h', (25, 30)); dial(768, 256, 236, 0, 100, 25, 5, 'BATTERY', '%', (0, 15)); out('Dials', 'Color', im.filter(ImageFilter.SMOOTH)); out('Dials', 'Roughness', Image.new('L', (1024, 512), 120))
s = Image.new('RGB', (512, 256), (6, 18, 26)); d = ImageDraw.Draw(s); d.rectangle((0, 0, 511, 34), fill=(14, 60, 84)); d.text((12, 17), 'STAIR', font=font(22), fill=(230, 240, 244), anchor='lm'); d.text((500, 17), 'LEGS UP', font=font(20), fill=(120, 230, 140), anchor='rm')
pts = [(40, 220), (40, 196), (72, 196), (72, 174), (104, 174), (104, 152), (136, 152), (136, 130), (168, 130), (168, 108), (200, 108), (200, 86), (300, 86), (300, 100), (212, 100), (60, 220)]; d.line(pts + pts[:1], fill=(170, 210, 225), width=4)
d.line([(330, 86), (330, 220)], fill=(120, 230, 140), width=3); d.text((345, 120), 'HEIGHT', font=font(22), fill=(160, 190, 200), anchor='lm'); d.text((345, 166), '-.-- m', font=font(44), fill=(235, 245, 248), anchor='lm'); d.line([(20, 222), (490, 222)], fill=(90, 120, 130), width=3); out('Screen', 'Color', s)
