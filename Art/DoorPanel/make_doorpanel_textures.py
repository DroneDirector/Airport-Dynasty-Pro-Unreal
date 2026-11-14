# The door button panel's face and its E key sign (owner, 2026-11-14: his reference photo of a garage-door panel - our
# own drawing of it): brushed stainless steel with fine scratches, "GARAGE DOOR" over a rule, "OPENING DOORS" beside the
# green button, "CLOSING DOOR" beside the red one; and a small dark sign with the E key. The buttons, their bezels and the
# screws are meshes (make_doorpanel.py); their places here match the photo's layout.
#   python Art/DoorPanel/make_doorpanel_textures.py
import os, random
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 1080, 1440          # 3:4, the plate's 12 x 16 cm
DARK = (24, 24, 26)


def font(sz, bold=True):
    return ImageFont.truetype('arialbd.ttf' if bold else 'arial.ttf', sz)


def steel():
    random.seed(7)
    img = Image.new('RGB', (W, H), (168, 170, 172))
    d = ImageDraw.Draw(img)
    # the brushing: fine horizontal streaks of slightly different brightness
    for y in range(H):
        v = 168 + int(random.gauss(0, 5))
        d.line([(0, y), (W, y)], fill=(v, v + 2, v + 4))
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    d = ImageDraw.Draw(img)
    # a few fine scratches and the odd smudge, as on a panel people press every day
    for _ in range(70):
        x, y = random.randint(0, W), random.randint(0, H)
        l = random.randint(20, 160); a = random.uniform(-0.6, 0.6)
        v = random.choice([140, 200])
        d.line([(x, y), (x + l, y + int(l * a))], fill=(v, v, v + 2), width=1)
    smudge = Image.new('L', (W, H), 0); sd = ImageDraw.Draw(smudge)
    for _ in range(14):
        x, y, r = random.randint(0, W), random.randint(0, H), random.randint(40, 140)
        sd.ellipse([x - r, y - r, x + r, y + r], fill=random.randint(10, 26))
    smudge = smudge.filter(ImageFilter.GaussianBlur(40))
    img = Image.composite(Image.new('RGB', (W, H), (120, 118, 112)), img, smudge)
    # darker at the rim, where dirt collects
    rim = Image.new('L', (W, H), 0); rd = ImageDraw.Draw(rim); rd.rectangle([0, 0, W - 1, H - 1], outline=60, width=26)
    img = Image.composite(Image.new('RGB', (W, H), (110, 110, 108)), img, rim.filter(ImageFilter.GaussianBlur(14)))
    return img


def face():
    img = steel(); d = ImageDraw.Draw(img)
    # the layout as the photo: a title over a rule, the two buttons on the left third, their words beside them
    d.text((W / 2, H * 0.157), 'GARAGE DOOR', fill=DARK, font=font(118), anchor='mm')
    d.line([(W * 0.11, H * 0.22), (W * 0.89, H * 0.22)], fill=DARK, width=7)
    for (v, a, b) in [(0.376, 'OPENING', 'DOORS'), (0.685, 'CLOSING', 'DOOR')]:
        d.text((W * 0.555, H * (v - 0.035)), a, fill=DARK, font=font(78), anchor='lm')
        d.text((W * 0.555, H * (v + 0.035)), b, fill=DARK, font=font(78), anchor='lm')
    img.save(os.path.join(HERE, 'T_DoorPanel_Face.png'))


def key():
    # the E key sign: a keyboard key's top, white on near-black, with a soft bevel
    S = 512
    img = Image.new('RGB', (S, S), (20, 20, 22)); d = ImageDraw.Draw(img)
    d.rounded_rectangle([60, 60, S - 60, S - 60], radius=56, fill=(232, 232, 228))
    d.rounded_rectangle([86, 80, S - 86, S - 100], radius=44, fill=(246, 246, 242))
    d.text((S / 2, S / 2 - 8), 'E', fill=(20, 20, 22), font=font(250), anchor='mm')
    img.save(os.path.join(HERE, 'T_DoorPanel_Key.png'))


if __name__ == '__main__':
    face(); key()
    print('door panel textures written')
