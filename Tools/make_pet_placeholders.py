# The pet step's stand-ins (owner, 2026-11-13): until the real 3D cats and dogs are made, a sitting cat (or dog) drawn as
# a silhouette with a big question mark above it, so it is plain they are to be replaced. Our own drawing (no licence).
# The picture's channels are masks the stage's material colours: R the body's fill (the chosen coat colour), G the
# outline (the title screen's cyan), B the question mark (its pink), A where anything is.
#   python Tools/make_pet_placeholders.py   ->  Art/PetPlaceholder/cat.png, dog.png
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageChops, ImageFont

OUT = Path(__file__).resolve().parent.parent / 'Art' / 'PetPlaceholder'
OUT.mkdir(parents=True, exist_ok=True)
S = 1024

def shape(draw_fn):
    m = Image.new('L', (S, S), 0); d = ImageDraw.Draw(m); draw_fn(d); return m

def cat(d):
    # a cat sitting, seen from the front-left: haunch, chest, head, ears, front legs, the tail round the paws
    d.ellipse((330, 560, 700, 960), fill=255)                      # haunch and body
    d.ellipse((390, 470, 600, 760), fill=255)                      # chest
    d.ellipse((395, 300, 615, 500), fill=255)                      # head
    d.polygon([(410, 360), (420, 225), (500, 320)], fill=255)      # left ear
    d.polygon([(600, 360), (595, 225), (515, 320)], fill=255)      # right ear
    d.rounded_rectangle((430, 700, 480, 965), 22, fill=255)       # front legs
    d.rounded_rectangle((515, 700, 565, 965), 22, fill=255)
    d.ellipse((415, 935, 495, 975), fill=255); d.ellipse((500, 935, 580, 975), fill=255)   # paws
    for i in range(60):                                            # the tail, curled round in front
        t = i / 59; a = math.radians(-20 + 200 * t)
        x = 520 + 230 * math.cos(a); y = 905 + 70 * math.sin(a)
        r = 30 - 12 * t; d.ellipse((x - r, y - r, x + r, y + r), fill=255)

def dog(d):
    # a dog sitting, seen side-on (its muzzle and hanging ear make it plainly a dog): haunch, chest, neck, head with a
    # long muzzle and a nose, a floppy ear, the front leg straight down, the back paw, the tail out behind
    d.ellipse((300, 610, 640, 965), fill=255)                      # haunch
    d.polygon([(420, 640), (560, 420), (660, 470), (640, 760), (470, 800)], fill=255)   # chest and neck
    d.ellipse((520, 300, 720, 480), fill=255)                      # skull
    d.rounded_rectangle((640, 365, 820, 455), 40, fill=255)       # muzzle
    d.ellipse((790, 360, 840, 405), fill=255)                      # nose
    d.polygon([(560, 320), (520, 330), (500, 470), (545, 500), (585, 400)], fill=255)   # the ear, hanging
    d.rounded_rectangle((585, 700, 640, 975), 24, fill=255)       # front leg
    d.ellipse((575, 945, 690, 990), fill=255)                      # front paw
    d.ellipse((330, 925, 520, 985), fill=255)                      # back paw
    for i in range(40):                                            # the tail, out behind along the floor
        t = i / 39; x = 320 - 150 * t; y = 900 + 50 * math.sin(t * 2.6); r = 26 - 11 * t
        d.ellipse((x - r, y - r, x + r, y + r), fill=255)

def question():
    m = Image.new('L', (S, S), 0); d = ImageDraw.Draw(m)
    # a fat neon question mark drawn as strokes: the hook, the stem, the dot
    cx, cy, R, w = 512, 105, 62, 28
    d.arc((cx - R, cy - R, cx + R, cy + R), start=180, end=420, fill=255, width=w)
    a = math.radians(60); x0 = cx + R * math.cos(a); y0 = cy + R * math.sin(a)
    d.line((x0, y0 - 6, cx, cy + R + 40), fill=255, width=w)
    d.line((cx, cy + R + 32, cx, cy + R + 60), fill=255, width=w)
    d.ellipse((cx - 20, cy + R + 85, cx + 20, cy + R + 125), fill=255)
    return m

def make(name, fn):
    # the animal a little smaller and low in the picture, so the question mark above it stands clear
    body = shape(fn).resize((int(S * 0.8), int(S * 0.8)), Image.LANCZOS)
    b = Image.new('L', (S, S), 0); b.paste(body, (int(S * 0.1), S - int(S * 0.8) - 10)); body = b
    edge = ImageChops.subtract(body.filter(ImageFilter.MaxFilter(13)), body.filter(ImageFilter.MinFilter(9)))
    glow = edge.filter(ImageFilter.GaussianBlur(10))
    outline = ImageChops.lighter(edge, glow.point(lambda v: min(255, v * 2)))
    q = question(); q = ImageChops.lighter(q, q.filter(ImageFilter.GaussianBlur(9)).point(lambda v: min(255, v * 2)))
    fill = body.point(lambda v: int(v * 0.85))
    alpha = ImageChops.lighter(ImageChops.lighter(fill, outline), q)
    img = Image.merge('RGBA', (fill, outline, q, alpha))
    img.save(OUT / f'{name}.png')
    # a look at it as the stage colours it (golden coat, cyan, pink) for checking by eye
    prev = Image.new('RGB', (S, S), (8, 6, 20))
    for mask, col in ((fill, (200, 145, 62)), (outline, (53, 240, 255)), (q, (255, 63, 184))):
        prev = Image.composite(Image.new('RGB', (S, S), col), prev, mask)
    prev.save(OUT / f'{name}_preview.png')
    print('made', OUT / f'{name}.png')

make('cat', cat)
make('dog', dog)
