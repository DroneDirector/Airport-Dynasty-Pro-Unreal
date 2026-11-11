# The hangars' textures, from the owner's own photos of a real hangar (Blueprints/Hangars/Reference): a flat, square-on
# piece of each surface cut out, its light evened out, made seamless (the piece blended with itself shifted by half)
# and given a faint normal map from its own relief. Where the photos only show a surface at a slant (the roof deck, the
# annex's fine corrugation) the texture is drawn here instead, in the photo's own colour.
#   python Tools/make_hangar_textures.py  ->  Art/Hangar/T_Hangar<Name>_D.png and _N.png
import numpy as np
from PIL import Image
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REF = ROOT / 'Blueprints' / 'Hangars' / 'Reference'
OUT = ROOT / 'Art' / 'Hangar'
OUT.mkdir(parents=True, exist_ok=True)


def blur(x, s):
    fy = np.fft.fftfreq(x.shape[0])[:, None]; fx = np.fft.fftfreq(x.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(x) * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2)))).astype(np.float32)


def blur3(a, s): return np.dstack([blur(a[..., c], s) for c in range(3)])


def seamless(a):
    """Seamless both ways: blended with itself shifted by half, first across, then down (each copy's seam lies where
    the other one counts fully)."""
    H, W = a.shape[:2]
    wx = np.clip((1 - np.abs(np.linspace(-1, 1, W))) * 2.0, 0, 1)[None, :, None]
    a = a * wx + np.roll(a, W // 2, axis=1) * (1 - wx)
    wy = np.clip((1 - np.abs(np.linspace(-1, 1, H))) * 2.0, 0, 1)[:, None, None]
    return a * wy + np.roll(a, H // 2, axis=0) * (1 - wy)


def even(a, s=60, k=0.85):
    """The photo's light patches and fall-off evened out (keeps the fine detail and the overall colour)."""
    L = a.mean(2); lo = blur(L, s)
    return np.clip(a * ((lo.mean() / np.maximum(lo, 1)) ** k)[..., None], 0, 255)


def normal(d, strength, s=4):
    g = d.mean(2) / 255.0; h = g - blur(g, s)
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength; dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-dx, dy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).round().astype(np.uint8)


def save(name, d, nstr=4.0, tone=1.0):
    d = np.clip(d * tone, 0, 255)
    Image.fromarray(d.round().astype(np.uint8)).save(OUT / f'T_Hangar{name}_D.png')
    Image.fromarray(normal(d, nstr)).save(OUT / f'T_Hangar{name}_N.png')
    print(name, d.shape, 'mean', d.reshape(-1, 3).mean(0).round(1))


def from_photo(name, photo, box, size, stretch_axis=None, nstr=4.0, tone=1.0, smooth=1.2, light=60):
    """box: (x0, y0, x1, y1) in the photo's pixels; size: (w, h) of the texture."""
    im = Image.open(REF / photo).convert('RGB').crop(box)
    if stretch_axis == 'v':        # ribs run vertically: keep them crisp, resample only along them
        im = im.resize((size[0], size[1]), Image.LANCZOS)
    else:
        im = im.resize(size, Image.LANCZOS)
    a = np.asarray(im, np.float32)
    a = blur3(a, smooth)            # the JPEG blocks smoothed away
    a = even(a, light)
    a = seamless(a)
    save(name, a, nstr, tone)
    return a


rng = np.random.default_rng(7)

# the outside walls and the main door's leaves: white vertically ribbed steel, square-on above the door (1-hall/03)
# (one tile = 1.5 m of wall: the photo's ribs are about 0.25 m apart)
from_photo('Clad', '1-hall-and-doors/03.jpg', (300, 300, 1500, 860), (1024, 1024), 'v', nstr=6.0, tone=0.92)
# the hall floor: polished dark grey concrete (1-hall/09, clear of the yellow line and the glare)
from_photo('Floor', '1-hall-and-doors/09.jpg', (150, 450, 1040, 1190), (2048, 2048), nstr=1.5, tone=1.0, smooth=0.8, light=120)
# the asphalt round the building (2-exterior/07, the car park in front of the annex)
from_photo('Asphalt', '2-exterior-back-corner/07.jpg', (90, 930, 750, 1200), (1024, 1024), nstr=3.0, tone=0.62, smooth=0.6)
# the crew rooms' wood veneer (3-lobby/02, the kitchen's wall cupboards, between two handles)
from_photo('Wood', '3-lobby-kitchen-corridor/02.jpg', (1012, 468, 1218, 640), (1024, 1024), nstr=1.0, tone=0.95, light=30)
# the roll-up doors' slats (4-lounge/05, the inner roll-up door, square-on)
from_photo('Slats', '4-lounge-and-store/05.jpg', (880, 720, 1080, 960), (512, 1024), 'v', nstr=5.0, tone=0.95, light=40)

# drawn: the roof deck seen from below (the photos show it only at a slant): dark grey trapezoid ribs, 0.25 m apart
S = 1024
t = (np.arange(S)[None, :] / S * 6) % 1.0
prof = np.where(t < 0.25, 1.0, np.where(t < 0.35, 1 - (t - 0.25) / 0.1, np.where(t < 0.75, 0.0, np.where(t < 0.85, (t - 0.75) / 0.1, 1.0))))
base = np.array([52, 56, 60], np.float32)
shade = 1 + 0.12 * (prof - 0.5) + np.where((t > 0.25) & (t < 0.35), -0.15, 0) + np.where((t > 0.75) & (t < 0.85), 0.12, 0)
d = np.repeat(shade, S, 0)[..., None] * base[None, None, :] + blur(rng.standard_normal((S, S)).astype(np.float32), 1.2)[..., None] * 3
save('Deck', seamless(d), 8.0)

# drawn: the annex's fine corrugated cladding (sine ribs, 76 mm apart), in the colour of the photo's silver panels
c = np.asarray(Image.open(REF / '2-exterior-back-corner/07.jpg').convert('RGB').crop((40, 300, 220, 420)), np.float32).reshape(-1, 3).mean(0)
t = np.arange(S)[None, :] / S * 20 * 2 * np.pi
shade = 1 + 0.09 * np.sin(t)
d = np.repeat(shade, S, 0)[..., None] * c[None, None, :] * 0.95 + blur(rng.standard_normal((S, S)).astype(np.float32), 1.0)[..., None] * 2.5
save('Corrugated', seamless(d), 10.0)

# drawn: the inside lining (the photos' panels carry the girts, which are modelled): white sheet with a faint
# micro-rib every 0.1 m and a joint every 1 m, in the colour of the photo's lining (1-hall/13)
c = np.asarray(Image.open(REF / '1-hall-and-doors/13.jpg').convert('RGB').crop((600, 600, 820, 700)), np.float32).reshape(-1, 3).mean(0)
t = (np.arange(S)[None, :] / S * 10) % 1.0
shade = 1 + 0.025 * np.cos(t * 2 * np.pi) - 0.12 * np.exp(-((np.arange(S)[None, :] % S) / 3.0) ** 2)
d = np.repeat(shade, S, 0)[..., None] * c[None, None, :] + blur(rng.standard_normal((S, S)).astype(np.float32), 1.0)[..., None] * 2
save('Liner', seamless(d), 4.0, 0.95)
