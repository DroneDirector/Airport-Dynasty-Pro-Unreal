# The terminal's LED adverts (to-do 52): eight made-up brands of our own, drawn here (no bought or copied artwork),
# each a 16:9 picture, packed 4 x 2 into one atlas the screens' material picks from (AdvertAtlas.png, 4096 x 2304).
#   python Art/Adverts/make_adverts.py
import os, math, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter
HERE = os.path.dirname(os.path.abspath(__file__)); W, H = 1024, 576
F = 'C:/Windows/Fonts/'
def font(name, size):
    for n in (name, 'bahnschrift.ttf', 'arialbd.ttf'):
        try: return ImageFont.truetype(F + n, size)
        except OSError: pass
def grad(top, bot):
    im = Image.new('RGB', (W, H)); d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / (H - 1); d.line([(0, y), (W, y)], fill=tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)))
    return im
def text(d, xy, s, f, fill, anchor='la', shadow=True):
    if shadow: d.text((xy[0] + 3, xy[1] + 3), s, font=f, fill=(0, 0, 0), anchor=anchor)
    d.text(xy, s, font=f, fill=fill, anchor=anchor)
def mountains(d, y0, col, seed, amp=120, step=60):
    r = random.Random(seed); pts = [(0, H)]; x = 0; y = y0
    while x <= W + step: pts.append((x, y)); x += step; y = y0 - r.random() * amp
    pts.append((W, H)); d.polygon(pts, fill=col)
DOT = '\u00b7'
ads = []
# 1 HRAUN outdoor jackets: a dark lava field under a red-orange evening, a jacket
im = grad((40, 18, 30), (230, 90, 40)); d = ImageDraw.Draw(im)
mountains(d, 420, (25, 22, 24), 3); mountains(d, 480, (12, 10, 12), 8, 60, 40)
d.polygon([(700, 150), (790, 130), (880, 150), (930, 330), (880, 340), (870, 250), (860, 440), (720, 440), (710, 250), (700, 340), (650, 330)], fill=(225, 70, 30))
d.line([(790, 135), (790, 440)], fill=(120, 30, 15), width=5)
text(d, (60, 70), 'HRAUN', font('bahnschrift.ttf', 140), (255, 240, 225)); text(d, (66, 225), 'Built for the weather.', font('segoeuib.ttf', 46), (255, 200, 160))
text(d, (66, 500), 'OUTERWEAR  %s  EST. 1987' % DOT, font('segoeui.ttf', 26), (230, 200, 190))
ads.append(im)
# 2 SOLVA coffee: warm cream, a big cup with steam and a rising sun
im = grad((250, 232, 205), (215, 165, 110)); d = ImageDraw.Draw(im)
d.ellipse([850, 30, 1010, 190], fill=(255, 190, 90)); d.rounded_rectangle([600, 250, 860, 480], 40, fill=(250, 248, 242)); d.ellipse([600, 230, 860, 290], fill=(90, 50, 25))
d.arc([830, 300, 930, 400], -90, 90, fill=(250, 248, 242), width=22)
for k in range(3): d.line([(680 + k * 50 + 12 * math.sin(t / 9.0 + k), 215 - t) for t in range(0, 110, 4)], fill=(255, 255, 255), width=7)   # (wavy steam rising)
text(d, (60, 90), 'S\u00d3LV\u00c1', font('georgiab.ttf', 130), (80, 40, 20), shadow=False); text(d, (66, 240), 'Wake up above the clouds.', font('segoeuib.ttf', 40), (110, 60, 30), shadow=False)
text(d, (66, 480), 'Roasted in the north  %s  now airside' % DOT, font('segoeui.ttf', 28), (90, 55, 30), shadow=False)
ads.append(im)
# 3 BRIMRUN watches: deep navy, a big watch face
im = grad((8, 14, 32), (20, 40, 80)); d = ImageDraw.Draw(im)
cx, cy, r = 760, 288, 200
d.ellipse([cx - r - 18, cy - r - 18, cx + r + 18, cy + r + 18], fill=(190, 160, 90)); d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(14, 22, 48))
for k in range(12):
    a = k / 12 * 2 * math.pi; d.line([(cx + math.sin(a) * (r - 30), cy - math.cos(a) * (r - 30)), (cx + math.sin(a) * (r - 8), cy - math.cos(a) * (r - 8))], fill=(220, 200, 140), width=8 if k % 3 == 0 else 4)
d.line([(cx, cy), (cx + 90, cy - 90)], fill=(235, 225, 200), width=10); d.line([(cx, cy), (cx - 40, cy - 150)], fill=(235, 225, 200), width=6); d.ellipse([cx - 12, cy - 12, cx + 12, cy + 12], fill=(190, 160, 90))
text(d, (60, 110), 'BRIMR\u00daN', font('georgiab.ttf', 80), (230, 210, 160)); text(d, (66, 245), 'Time. Tide. Precision.', font('segoeui.ttf', 42), (200, 210, 230))
text(d, (66, 480), 'NORDIC WATCHMAKERS', font('bahnschrift.ttf', 28), (190, 160, 90))
ads.append(im)
# 4 ELDUR hot springs: steam over a blue lagoon
im = grad((150, 190, 210), (40, 150, 170)); d = ImageDraw.Draw(im)
mountains(d, 300, (60, 70, 80), 11, 90); d.rectangle([0, 360, W, H], fill=(90, 200, 215))
for k in range(40):
    x = random.Random(k).random() * W; y = 330 + random.Random(k + 99).random() * 60; d.ellipse([x - 70, y - 30, x + 70, y + 30], fill=(235, 245, 248))
im = im.filter(ImageFilter.GaussianBlur(3)); d = ImageDraw.Draw(im)
text(d, (60, 70), 'ELDUR', font('bahnschrift.ttf', 130), (255, 255, 255)); text(d, (66, 215), 'Hot springs spa  %s  Soak in the north.' % DOT, font('segoeuib.ttf', 40), (240, 250, 255))
text(d, (66, 490), '40 min from the airport', font('segoeui.ttf', 30), (20, 60, 70), shadow=False)
ads.append(im)
# 5 NORDVIK BANK: clean white and teal, a card
im = grad((245, 248, 250), (220, 235, 238)); d = ImageDraw.Draw(im)
d.rounded_rectangle([600, 150, 940, 365], 26, fill=(0, 120, 125)); d.rounded_rectangle([640, 200, 700, 245], 8, fill=(220, 190, 110))
text(d, (640, 300), '4021  %s%s%s%s  %s%s%s%s  7718' % ((DOT,) * 8), font('bahnschrift.ttf', 26), (230, 250, 250), shadow=False)
d.ellipse([860, 170, 920, 230], fill=(255, 255, 255))
text(d, (60, 120), 'NORDVIK', font('bahnschrift.ttf', 110), (0, 90, 95), shadow=False); text(d, (66, 245), 'Banking that travels with you.', font('segoeui.ttf', 40), (30, 60, 65), shadow=False)
text(d, (66, 470), 'No fees abroad  %s  Open an account in 5 minutes' % DOT, font('segoeui.ttf', 28), (0, 110, 115), shadow=False)
ads.append(im)
# 6 PUFFIN PAY eSIM: black with bright orange, a phone
im = grad((20, 20, 24), (40, 40, 48)); d = ImageDraw.Draw(im)
d.rounded_rectangle([690, 70, 900, 500], 34, fill=(240, 120, 30)); d.rounded_rectangle([706, 100, 884, 470], 22, fill=(18, 18, 22))
for k in range(4): d.arc([745 - k * 22, 200 - k * 22, 845 + k * 22, 300 + k * 22], 220, 320, fill=(240, 120, 30), width=8)
d.ellipse([782, 270, 808, 296], fill=(240, 120, 30))
text(d, (60, 100), 'PUFFIN', font('bahnschrift.ttf', 120), (255, 255, 255)); text(d, (66, 225), 'PAY  %s  travel eSIM' % DOT, font('bahnschrift.ttf', 54), (240, 120, 30))
text(d, (66, 330), 'Land. Connect. Go.', font('segoeuib.ttf', 46), (230, 230, 235))
ads.append(im)
# 7 GLACIER BLUE water: ice blue, a bottle
im = grad((210, 235, 250), (120, 180, 225)); d = ImageDraw.Draw(im)
mountains(d, 470, (240, 248, 255), 21, 160, 80)
d.rounded_rectangle([745, 170, 845, 500], 30, fill=(150, 200, 235)); d.rectangle([775, 110, 815, 175], fill=(30, 80, 150)); d.rectangle([745, 280, 845, 360], fill=(30, 80, 150))
text(d, (795, 320), 'GB', font('bahnschrift.ttf', 40), (255, 255, 255), anchor='mm', shadow=False)
text(d, (60, 90), 'GLACIER', font('bahnschrift.ttf', 110), (20, 70, 140), shadow=False); text(d, (60, 200), 'BLUE', font('bahnschrift.ttf', 110), (60, 130, 200), shadow=False)
text(d, (66, 330), 'Pure as the ice it came from.', font('segoeui.ttf', 36), (20, 60, 110), shadow=False)
ads.append(im)
# 8 VEGA car rental: a road into the hills at sunset
im = grad((255, 150, 80), (120, 60, 120)); d = ImageDraw.Draw(im)
d.ellipse([430, 160, 600, 330], fill=(255, 220, 140)); mountains(d, 330, (70, 40, 80), 33, 70, 50)
d.polygon([(470, 330), (530, 330), (900, H), (100, H)], fill=(45, 45, 55))
for k in range(6):
    y = 345 + k * k * 9; w_ = 2 + k * 3; d.rectangle([500 - w_, y, 500 + w_, y + 6 + k * 3], fill=(250, 220, 120))
text(d, (60, 60), 'VEGA', font('bahnschrift.ttf', 120), (255, 255, 255)); text(d, (66, 190), 'Car rental', font('segoeuib.ttf', 46), (255, 235, 210))
text(d, (600, 470), 'Your road starts here.', font('segoeuib.ttf', 40), (255, 255, 255))
ads.append(im)

atlas = Image.new('RGB', (W * 4, H * 2))
for i, a in enumerate(ads):
    atlas.paste(a, ((i % 4) * W, (i // 4) * H)); a.save(os.path.join(HERE, 'ad_%d.png' % (i + 1)))
atlas.save(os.path.join(HERE, 'AdvertAtlas.png')); atlas.resize((2048, 576)).save(os.path.join(HERE, 'AdvertAtlas_preview.png'))
print('ADVERTS', len(ads))
