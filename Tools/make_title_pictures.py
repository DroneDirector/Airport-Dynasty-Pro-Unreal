# The title film's pictures for Unreal (Tools/set_title_bg.py imports them): the page's own .webp files as PNG, and
# each one's blurred copy made the way the page makes it (titleBlurOf: a tenth of the size, then smoothly up to half).
#   python Tools/make_title_pictures.py
from PIL import Image
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = os.path.join(ROOT, 'Airline-Manager-Pro-Babylon', 'www', 'assets', 'title'); out = os.path.join(ROOT, 'Art', 'Title')
os.makedirs(out, exist_ok=True)
for n in ['t1', 't2', 't3o', 't3i', 't4a', 't4b', 't5']:
    im = Image.open(os.path.join(src, n + '.webp')); im.load()
    im = im.convert('RGBA' if im.mode == 'RGBA' else 'RGB'); im.save(os.path.join(out, 'T_Title_%s.png' % n))
    w, h = im.size
    small = im.resize((max(8, round(w / 10)), max(8, round(h / 10))), Image.LANCZOS)
    small.resize((round(w / 2), round(h / 2)), Image.BICUBIC).save(os.path.join(out, 'T_Title_%s_blur.png' % n))
print('ok')
