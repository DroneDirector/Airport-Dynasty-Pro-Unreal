# The charger's screen and warning sign (plain text, no brand): python Art/BusCharger/make_screen.py
import os
from PIL import Image, ImageDraw, ImageFont
W, H = 1024, 640
im = Image.new('RGB', (W, H), (6, 14, 24)); d = ImageDraw.Draw(im)


def f(s, b=False): return ImageFont.truetype('C:/Windows/Fonts/%s.ttf' % ('segoeuib' if b else 'segoeui'), s)


def mid(y, t, font, col): d.text(((W - d.textlength(t, font=font)) / 2, y), t, font=font, fill=col)


d.rectangle((0, 0, W, 104), fill=(14, 52, 84)); d.text((36, 22), 'BUS CHARGER', font=f(46, True), fill=(235, 242, 248)); d.text((W - 36 - d.textlength('150 kW DC', font=f(46)), 22), '150 kW DC', font=f(46), fill=(180, 214, 240))
mid(170, 'READY', f(150, True), (90, 226, 130)); mid(380, 'Plug in to start', f(60), (235, 242, 248))
d.rounded_rectangle((36, 500, W - 36, 596), 18, outline=(60, 110, 150), width=4); mid(516, 'CCS2      200-920 V      max 250 A', f(44), (180, 214, 240))
im.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'textures', 'screen.png'))
im = Image.new('RGB', (800, 400), (222, 224, 225)); d = ImageDraw.Draw(im)
d.polygon(((190, 30), (30, 310), (350, 310)), fill=(12, 12, 12)); d.polygon(((190, 74), (68, 290), (312, 290)), fill=(250, 196, 10))
d.polygon(((212, 130), (148, 218), (186, 218), (168, 282), (234, 192), (194, 192)), fill=(12, 12, 12))
for i, (t, b) in enumerate((('DANGER', True), ('High voltage', False), ('1000 V DC', False), ('In: 400 V 3~ 240 A', False))): d.text((380, 46 + i * 68), t, font=f(44, b), fill=(12, 12, 12))
d.text((44, 330), 'CCS2 DC 200-920 V   IP54', font=f(44), fill=(12, 12, 12))
im.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'textures', 'sign.png'))
