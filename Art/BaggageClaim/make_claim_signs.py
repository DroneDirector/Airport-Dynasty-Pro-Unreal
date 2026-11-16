# The baggage claim concept's screens and signs (to-do 72): one flight screen per belt and the hall's overhead sign,
# drawn as pictures for the concept renders (Art/BaggageClaim/render_claim_concept.py) and later the game.
#   python Art/BaggageClaim/make_claim_signs.py
import os
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__)); F = 'C:/Windows/Fonts/'
def f(n, s): return ImageFont.truetype(F + n, s)
BELTS = [(1, [('VT 245', 'Copenhagen', 'Bags on belt'), ('VT 318', 'London', 'Expected 14:40')]),
         (2, [('VT 211', 'Oslo', 'Bags on belt'), ('VT 402', 'Reykjavik', 'Last bags')]),
         (3, [('VT 377', 'Amsterdam', 'Expected 14:55'), ('VT 290', 'Vagar', 'Expected 15:10')]),
         (4, [('VT 156', 'Stockholm', 'Bags on belt'), ('VT 433', 'Edinburgh', 'Expected 15:20')]),
         (5, [('VT 288', 'Frankfurt', 'Expected 15:05'), ('VT 509', 'Paris', 'Expected 15:35')])]
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
d.text((1990, 128), 'Belts 1 - 5   >   Customs   >   Exit', font=f('segoeuib.ttf', 60), fill=(15, 20, 30), anchor='rm')
im.save(os.path.join(HERE, 'sign_claim.png'))
im = Image.new('RGB', (1024, 256), (40, 44, 50)); d = ImageDraw.Draw(im)
d.text((40, 128), 'BELT INPUT  |  AUTHORISED STAFF ONLY', font=f('bahnschrift.ttf', 50), fill=(250, 210, 60), anchor='lm')
im.save(os.path.join(HERE, 'sign_input.png'))
print('SIGNS done')
# the outside lettering over the arrivals doors, and the arrivals hall's desks
im = Image.new('RGB', (2048, 400), (0, 0, 0)); d = ImageDraw.Draw(im)
d.text((1024, 200), 'ARRIVALS', font=f('bahnschrift.ttf', 300), fill=(255, 255, 255), anchor='mm')
im.save(os.path.join(HERE, 'sign_arrivals.png'))
for name, text, col in (('desk_vega', 'VEGA  Car rental', (255, 150, 80)), ('desk_taxi', 'Taxi  |  Bus  |  Info', (245, 196, 40)), ('desk_cafe', 'SOLVA  Coffee', (230, 200, 160)), ('sign_exit', 'Exit  >  Taxi  Bus  Car park', (245, 196, 40))):
    im = Image.new('RGB', (1024, 192), (14, 22, 34)); d = ImageDraw.Draw(im)
    d.text((512, 96), text, font=f('bahnschrift.ttf', 92), fill=col, anchor='mm')
    im.save(os.path.join(HERE, name + '.png'))
print('SIGNS 2 done')
