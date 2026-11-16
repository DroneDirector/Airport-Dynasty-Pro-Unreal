# The baggage claim concept's screens and signs (to-do 72): one flight screen per belt and the hall's overhead sign,
# drawn as pictures for the concept renders (Art/BaggageClaim/render_claim_concept.py) and later the game.
#   python Art/BaggageClaim/make_claim_signs.py
import os
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__)); F = 'C:/Windows/Fonts/'
def f(n, s): return ImageFont.truetype(F + n, s)
BELTS = [(1, [('VT 245', 'Copenhagen', 'Bags on belt'), ('VT 318', 'London', 'Expected 14:40')]),
         (2, [('VT 211', 'Oslo', 'Bags on belt'), ('VT 402', 'Reykjavik', 'Last bags')]),
         (3, [('VT 377', 'Amsterdam', 'Expected 14:55'), ('VT 290', 'Vagar', 'Expected 15:10')])]
for n, rows in BELTS:
    im = Image.new('RGB', (1024, 576), (10, 18, 30)); d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 1024, 150], fill=(245, 196, 40))
    d.text((40, 75), 'BELT %d' % n, font=f('bahnschrift.ttf', 110), fill=(15, 20, 30), anchor='lm')
    d.text((990, 75), 'Baggage claim', font=f('segoeui.ttf', 44), fill=(15, 20, 30), anchor='rm')
    for i, (fl, place, st) in enumerate(rows):
        y = 230 + i * 160
        d.text((40, y), fl, font=f('bahnschrift.ttf', 64), fill=(255, 255, 255), anchor='lm')
        d.text((300, y), place, font=f('segoeuib.ttf', 64), fill=(255, 255, 255), anchor='lm')
        d.text((40, y + 70), st, font=f('segoeui.ttf', 42), fill=(110, 220, 160) if 'belt' in st or 'Last' in st else (242, 181, 68), anchor='lm')
    im.save(os.path.join(HERE, 'screen_belt%d.png' % n))
im = Image.new('RGB', (2048, 256), (14, 22, 34)); d = ImageDraw.Draw(im)
d.rectangle([0, 0, 2048, 256], fill=(245, 196, 40))
d.text((60, 128), 'Baggage claim', font=f('bahnschrift.ttf', 130), fill=(15, 20, 30), anchor='lm')
d.text((1990, 128), 'Belts 1 - 3   >   Customs   >   Exit', font=f('segoeuib.ttf', 80), fill=(15, 20, 30), anchor='rm')
im.save(os.path.join(HERE, 'sign_claim.png'))
im = Image.new('RGB', (1024, 256), (40, 44, 50)); d = ImageDraw.Draw(im)
d.text((40, 128), 'BELT 1-3 INPUT  |  AUTHORISED STAFF', font=f('bahnschrift.ttf', 64), fill=(250, 210, 60), anchor='lm')
im.save(os.path.join(HERE, 'sign_input.png'))
print('SIGNS done')
