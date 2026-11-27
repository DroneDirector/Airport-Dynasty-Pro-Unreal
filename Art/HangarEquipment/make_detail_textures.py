# Our own small pictures for the hangar carts (to-do 364 A, batch 2): a gauge dial and plain warning / info labels, no
# brands, no words (the loader mirrors x; pictograms only). Run once with the system Python (Pillow):
#   python Art/HangarEquipment/make_detail_textures.py
import os, math
from PIL import Image, ImageDraw, ImageFilter
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Textures'); os.makedirs(D, exist_ok=True)
S = 512

def dial():
    im = Image.new('RGB', (S, S), (238, 238, 232)); d = ImageDraw.Draw(im); c = S / 2
    d.ellipse((4, 4, S - 4, S - 4), outline=(30, 30, 30), width=6)
    a0, a1 = math.radians(225), math.radians(-45)              # 270 deg sweep, clockwise
    d.arc((60, 60, S - 60, S - 60), -(-45) - 40, 45, fill=(200, 25, 20), width=18)   # red zone at the top end
    d.arc((60, 60, S - 60, S - 60), 135, 175, fill=(20, 140, 40), width=18)          # green zone low
    for k in range(51):
        a = a0 + (a1 - a0) * k / 50; L = 46 if k % 10 == 0 else (32 if k % 5 == 0 else 20)
        r1 = c - 52; r0 = r1 - L; w = 6 if k % 10 == 0 else 3
        d.line((c + r0 * math.cos(a), c - r0 * math.sin(a), c + r1 * math.cos(a), c - r1 * math.sin(a)), fill=(20, 20, 20), width=w)
    for k in range(6):                                            # numeral stand-ins: short bars by the major ticks
        a = a0 + (a1 - a0) * k / 5; r = c - 135
        d.rectangle((c + r * math.cos(a) - 9, c - r * math.sin(a) - 5, c + r * math.cos(a) + 9, c - r * math.sin(a) + 5), fill=(40, 40, 40))
    d.rectangle((c - 50, c + 110, c + 50, c + 124), fill=(60, 60, 60))
    im.filter(ImageFilter.SMOOTH).save(os.path.join(D, 'HQ_GaugeFace.png'))

def label(name, bg, pict):
    im = Image.new('RGB', (S, S // 2), bg); d = ImageDraw.Draw(im)
    d.rectangle((6, 6, S - 7, S // 2 - 7), outline=(15, 15, 15), width=8)
    if pict in ('warn', 'volt'):
        d.polygon([(40, 210), (130, 40), (220, 210)], fill=(15, 15, 15)); d.polygon([(70, 192), (130, 78), (190, 192)], fill=bg)
        if pict == 'warn':
            d.rectangle((122, 100, 138, 158), fill=(15, 15, 15)); d.ellipse((120, 166, 140, 186), fill=(15, 15, 15))
        else:
            d.polygon([(138, 88), (108, 146), (132, 146), (118, 186), (156, 128), (132, 128), (148, 88)], fill=(15, 15, 15))
    elif pict == 'info':
        d.ellipse((50, 60, 186, 196), fill=(20, 60, 150)); d.rectangle((110, 110, 126, 176), fill=(240, 240, 240)); d.ellipse((108, 82, 128, 102), fill=(240, 240, 240))
    elif pict == 'gas':
        d.ellipse((50, 60, 186, 196), outline=(15, 15, 15), width=12); d.rectangle((100, 40, 136, 70), fill=(15, 15, 15))
    for k in range(5):                                            # text stand-in lines
        d.rectangle((250, 50 + 34 * k, 470 - 40 * (k % 2), 64 + 34 * k), fill=(25, 25, 25))
    im.save(os.path.join(D, name))

dial()
label('HQ_Label_Warning.png', (240, 190, 20), 'warn')
label('HQ_Label_Volt.png', (240, 190, 20), 'volt')
label('HQ_Label_Info.png', (236, 236, 230), 'info')
label('HQ_Label_Gas.png', (236, 236, 230), 'gas')
print('ok')
