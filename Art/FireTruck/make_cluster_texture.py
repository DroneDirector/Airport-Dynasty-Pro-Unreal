# The fire truck's gauge face (owner, 2026-11-11: a shield-shaped cluster with a chrome rim, in the manner of his
# reference picture, our own dial art): a charcoal carbon-weave face, two big dials in the middle (rev counter and
# km/h), four small ones (temperature and oil at the top corners, fuel and volts lower down), an LCD at the bottom with
# the odometer and the clock. Blue scales, white numerals. The needles are separate meshes in the model (they move),
# so none are drawn here. The shield's outline is shared with make_panther.py (SHIELD below, u/v 0..1, v up).
# python Art/FireTruck/make_cluster_texture.py  ->  Art/FireTruck/T_Cluster_Face.png
import math, os

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 2048, 962                     # the shield is 0.62 m x 0.29 m
BLUE, BLUE_D, WHITE = (52, 104, 255), (30, 62, 170), (232, 236, 240)


def shield(n=48):
    """The outline: an arched top, shoulders, then straight down to a rounded point (u, v), anticlockwise from the tip."""
    out = [(0.5, 0.02), (0.53, 0.035), (0.55, 0.065)]            # the rounded tip, its right half
    out += [(0.99, 0.585), (1.0, 0.62), (1.0, 0.88)]              # up the right side to the shoulder
    for k in range(1, n):                                          # the arched top, right to left
        u = 1.0 - k / n
        out.append((u, 0.9 + 0.1 * math.sin(math.pi * u)))
    out += [(0.0, 0.88), (0.0, 0.62), (0.01, 0.585)]               # down the left side
    out += [(0.45, 0.065), (0.47, 0.035)]                          # the tip's left half
    return out


SHIELD = shield()


def px(u, v): return (u * W, (1 - v) * H)


def font(sz, bold=True):
    from PIL import ImageFont
    return ImageFont.truetype('arialbd.ttf' if bold else 'arial.ttf', sz)


def main():
    from PIL import Image, ImageDraw, ImageFilter   # (Blender's own Python imports SHIELD only)
    img = Image.new('RGB', (W, H), (10, 10, 11)); d = ImageDraw.Draw(img)
    # the carbon weave: small alternating diagonal cells, faint
    cell = 14
    for y in range(0, H, cell):
        for x in range(0, W, cell):
            c = 40 if ((x // cell) + (y // cell)) % 2 else 33
            if ((x // cell) + (y // cell)) % 2: d.polygon([(x, y), (x + cell, y), (x + cell, y + cell)], fill=(c, c, c + 2))
            else: d.polygon([(x, y), (x, y + cell), (x + cell, y + cell)], fill=(c, c, c + 2))
    face = Image.new('RGB', (W, H), (36, 37, 39)); img = Image.blend(img, face, 0.55); d = ImageDraw.Draw(img)

    def dial(cu, cv, r, lo, hi, step, label, minor=2, a0=-120, a1=120, numfmt=lambda x: str(int(x)), big=True):
        cx, cy = px(cu, cv); R = r * W
        n = int(round((hi - lo) / step)) * minor
        for i in range(n + 1):
            a = math.radians(a0 + (a1 - a0) * i / n)
            major = i % minor == 0
            r0, r1 = R * (0.8 if major else 0.86), R * 0.97
            w = int(R * (0.045 if major else 0.028))
            d.line([(cx + math.sin(a) * r0, cy - math.cos(a) * r0), (cx + math.sin(a) * r1, cy - math.cos(a) * r1)], fill=BLUE, width=max(3, w))
            if major:
                val = lo + (hi - lo) * i / n
                rt = R * 0.64
                f = font(int(R * (0.17 if big else 0.2)))
                t = numfmt(val)
                d.text((cx + math.sin(a) * rt, cy - math.cos(a) * rt), t, fill=WHITE, font=f, anchor='mm')
        if big:   # the blue inner arc round the hub
            rr = R * 0.33
            d.arc([cx - rr, cy - rr, cx + rr, cy + rr], start=-90 + a0 - 90 + 90, end=-90 + 0, fill=BLUE, width=int(R * 0.06))
        d.text((cx, cy + R * (0.55 if big else 0.62)), label, fill=WHITE, font=font(int(R * (0.13 if big else 0.22))), anchor='mm')
        # the hub's blue ring (the needle's black cap is a mesh on top)
        hr = R * (0.13 if big else 0.22)
        d.ellipse([cx - hr * 1.35, cy - hr * 1.35, cx + hr * 1.35, cy + hr * 1.35], outline=BLUE, width=int(hr * 0.3))

    # the two big dials: rev counter (x1000) and km/h
    dial(0.355, 0.64, 0.135, 0, 3, 0.5, 'RPM x1000', minor=2, numfmt=lambda x: ('%g' % x))
    dial(0.645, 0.64, 0.135, 0, 140, 20, 'km/h', minor=4)
    # the small ones: temperature and oil at the top corners, fuel and volts lower down (a quarter sweep each)
    dial(0.13, 0.79, 0.055, 40, 120, 40, 'TEMP °C', minor=2, a0=-60, a1=60, big=False)
    dial(0.87, 0.79, 0.055, 0, 8, 4, 'OIL bar', minor=2, a0=-60, a1=60, big=False)
    dial(0.29, 0.36, 0.05, 0, 1, 0.5, 'FUEL', minor=2, a0=-60, a1=60, big=False, numfmt=lambda x: {0: 'E', 0.5: '½', 1: 'F'}[x])
    dial(0.71, 0.36, 0.05, 18, 30, 6, 'VOLTS', minor=2, a0=-60, a1=60, big=False)
    # the LCD: odometer and clock, cool blue backlight
    lx0, ly0 = px(0.42, 0.33); lx1, ly1 = px(0.58, 0.13)
    d.rounded_rectangle([lx0 - 6, ly0 - 6, lx1 + 6, ly1 + 6], radius=10, fill=(20, 22, 26))
    d.rounded_rectangle([lx0, ly0, lx1, ly1], radius=6, fill=(70, 150, 225))
    d.text(((lx0 + lx1) / 2, ly0 + (ly1 - ly0) * 0.2), 'ODOMETER km', fill=(18, 40, 90), font=font(24, False), anchor='mm')
    d.text(((lx0 + lx1) / 2, ly0 + (ly1 - ly0) * 0.45), '012480.6', fill=(18, 40, 90), font=font(40), anchor='mm')
    d.text(((lx0 + lx1) / 2, ly0 + (ly1 - ly0) * 0.78), '12:12', fill=(18, 40, 90), font=font(52), anchor='mm')
    # outside the shield: black (the mesh only shows the inside)
    mask = Image.new('L', (W, H), 0); ImageDraw.Draw(mask).polygon([px(u, v) for u, v in SHIELD], fill=255)
    out = Image.composite(img, Image.new('RGB', (W, H), (8, 8, 8)), mask.filter(ImageFilter.GaussianBlur(1)))
    out.save(os.path.join(HERE, 'T_Cluster_Face.png'))
    print('cluster texture written')


if __name__ == '__main__':
    main()
