# The kiosk's textures cut from the owner's photos (Art/Kiosk/References), cleaned (owner, 2026-11-14: colour noise out
# where it doesn't make it look odd, the look improved): the colour noise smoothed out of the colour channels only
# (the detail stays in the brightness), a light sharpening, the blacks evened. Writes T_Kiosk_Bay.png, T_Kiosk_Vents.png.
#   python Art/Kiosk/clean_textures.py
import os
from PIL import Image, ImageFilter, ImageEnhance
HERE = os.path.dirname(os.path.abspath(__file__))
def clean(src, box, size, out, contrast=1.06):
    im = Image.open(os.path.join(HERE, 'References', src)).convert('RGB').crop(box)
    im = im.resize(size, Image.LANCZOS)
    y, cb, cr = im.convert('YCbCr').split()
    cb = cb.filter(ImageFilter.GaussianBlur(3)); cr = cr.filter(ImageFilter.GaussianBlur(3))   # the colour noise out
    y = y.filter(ImageFilter.MedianFilter(3)).filter(ImageFilter.UnsharpMask(radius=2, percent=70, threshold=2))
    im = Image.merge('YCbCr', (y, cb, cr)).convert('RGB')
    im = ImageEnhance.Contrast(im).enhance(contrast)
    im.save(os.path.join(HERE, out)); print('wrote', out, im.size)
clean('kiosk_front.png', (403, 512, 702, 692), (1024, 616), 'T_Kiosk_Bay.png', 1.08)
clean('kiosk_back.jpg', (388, 950, 528, 1045), (512, 348), 'T_Kiosk_Vents.png', 1.04)
