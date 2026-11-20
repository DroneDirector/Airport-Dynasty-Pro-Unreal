# The game's own pictures where Unreal's would otherwise show, all from the cube icon (Icon-Cube-1024.png):
# - Content/Splash/Splash.bmp: the small window shown while the game starts loading (before the Pixel Norma film)
# - Content/Splash/EdSplash.bmp: the same while the editor opens
# - AirportDynastyPro.png: the project's picture in Epic's launcher
# - Installer/wizard-small.bmp: the picture in the installer's corner
# Unreal writes its own lines (name, version, loading) in the splash's lower left, so the bottom strip is kept empty.
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
ICON = Image.open(os.path.join(HERE, 'Icon-Cube-1024.png')).convert('RGBA')
BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
REG = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def splash(path, sub):
    W, H = 800, 450
    g = Image.radial_gradient('L').resize((W * 2, W * 2)).crop((W // 2, W - H // 2 - 60, W // 2 + W, W + H // 2 - 60))
    inner, outer = (60, 28, 116), (12, 6, 32)
    im = Image.merge('RGB', [g.point(lambda v, a=inner[i], b=outer[i]: int(a + (b - a) * min(1, v / 200))) for i in range(3)]).convert('RGBA')
    ic = ICON.resize((250, 250), Image.LANCZOS)
    im.alpha_composite(ic, (60, 50))
    d = ImageDraw.Draw(im)
    d.text((345, 95), 'AIRPORT', font=ImageFont.truetype(BOLD, 52), fill=(255, 255, 255))
    d.text((345, 152), 'DYNASTY PRO', font=ImageFont.truetype(BOLD, 52), fill=(120, 236, 255))
    d.text((347, 222), sub, font=ImageFont.truetype(REG, 22), fill=(255, 120, 210))
    d.line([(345, 262), (740, 262)], fill=(255, 63, 184), width=3)
    # (the strip below 340 px is left for Unreal's own text)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.convert('RGB').save(path, format='BMP')
    print('wrote', path)


splash(os.path.join(ROOT, 'Content', 'Splash', 'Splash.bmp'), 'Pixel Norma')
splash(os.path.join(ROOT, 'Content', 'Splash', 'EdSplash.bmp'), 'Pixel Norma  ·  editor')
ICON.resize((192, 192), Image.LANCZOS).save(os.path.join(ROOT, 'AirportDynastyPro.png'))
# the installer's corner picture, on the window's white
w = Image.new('RGBA', (110, 106), (255, 255, 255, 255))
w.alpha_composite(ICON.resize((100, 100), Image.LANCZOS), (5, 3))
w.convert('RGB').save(os.path.join(ROOT, 'Installer', 'wizard-small.bmp'), format='BMP')
print('done')
