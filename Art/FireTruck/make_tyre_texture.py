# The fire truck's tyre sidewall (owner, 2026-11-11: the tyre's own detail, depth and markings, as in his photos; our own
# markings, an invented tyre maker). One square map covering the whole sidewall (1.5 m across, the wheel's centre in
# the middle); the model maps the sidewall faces onto it flat, as seen from the side. Raised lettering round the
# sidewall (maker, tread name, size and load, TUBELESS, M+S, a date code), fine rib rings at the bead and at the
# shoulder, a moulded rubber grain; a height-derived normal map gives the relief.
# python Art/FireTruck/make_tyre_texture.py  ->  Art/FireTruck/T_Tyre_Sidewall.png, T_Tyre_Sidewall_N.png
import math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
N = 2048
SPAN = 1.5                                       # metres across the map
PX = N / SPAN                                    # pixels per metre


def arc_text(img, text, radius_m, centre_deg, size_px, bold=True, spacing=1.0):
    """Raised characters set round the sidewall at radius_m, centred on centre_deg (0 = top, clockwise), upright to a
    reader standing beside the wheel at that point."""
    font = ImageFont.truetype('arialbd.ttf' if bold else 'arial.ttf', size_px)
    r = radius_m * PX; c = N / 2
    widths = [font.getbbox(ch)[2] - font.getbbox(ch)[0] + size_px * 0.12 for ch in text]
    total = sum(widths) * spacing
    ang = math.radians(centre_deg) - (total / r) / 2
    for ch, w in zip(text, widths):
        a = ang + (w * spacing / 2) / r
        if ch.strip():
            tile = Image.new('L', (size_px * 2, size_px * 2), 0)
            ImageDraw.Draw(tile).text((size_px, size_px), ch, fill=255, font=font, anchor='mm')
            tile = tile.rotate(-math.degrees(a), resample=Image.BICUBIC)
            x = c + r * math.sin(a); y = c - r * math.cos(a)
            img.paste(255, (int(x - size_px), int(y - size_px)), tile)
        ang += w * spacing / r


def main():
    yy, xx = np.mgrid[0:N, 0:N]
    rr = np.hypot(xx - N / 2, yy - N / 2) / PX                    # metres from the wheel's centre
    h = np.zeros((N, N))
    # fine rib rings near the bead and below the shoulder, a smooth raised band carrying the lettering
    for r0, r1, k in ((0.452, 0.49, 7), (0.60, 0.635, 5)):
        band = (rr > r0) & (rr < r1)
        h[band] += 0.35 * (np.cos((rr[band] - r0) / (r1 - r0) * math.pi * 2 * k) * 0.5 + 0.5)
    h += 0.15 * np.clip(1 - np.abs(rr - 0.545) / 0.05, 0, 1)
    letters = Image.new('L', (N, N), 0)
    arc_text(letters, 'DYNASTY  TERRA-X', 0.545, 0, 64)
    arc_text(letters, '1600 R25  176F  XL', 0.545, 180, 46)
    arc_text(letters, 'TUBELESS', 0.545, 90, 34)
    arc_text(letters, 'M+S', 0.545, 270, 40)
    arc_text(letters, 'RADIAL  STEEL BELTED', 0.505, 0, 24, bold=False)
    arc_text(letters, 'DOT 4X7K 2526', 0.505, 180, 24, bold=False)
    lt = np.asarray(letters.filter(ImageFilter.GaussianBlur(1.2))).astype(float) / 255
    h += lt * 0.8
    rng = np.random.default_rng(31)
    grain = np.asarray(Image.fromarray((rng.random((N, N)) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.0))).astype(float) / 255
    h += grain * 0.05
    # the colour: near-black rubber, the lettering and the ribs catch a touch more light (moulded, worn shiny)
    tone = 14 + lt * 10 + h * 6 + (grain - 0.5) * 5
    rgb = np.dstack([tone, tone, tone + 1])
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).save(os.path.join(HERE, 'T_Tyre_Sidewall.png'))
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 5.0
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 5.0
    n = np.dstack([-dx, dy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8)).save(os.path.join(HERE, 'T_Tyre_Sidewall_N.png'))
    print('tyre sidewall written')


if __name__ == '__main__':
    main()
