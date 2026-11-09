# The second floor's wood: the owner's photo (Art/WoodFloor/source.jpg) made seamless and tileable, plus its detail
# maps. Three whole planks wide (cut at the photo's own plank gaps, so the sides join on a gap); each plank column
# shifted up or down and given a plank joint where it wraps, so the top and bottom join as plank ends do and the
# joints don't line up across the floor; the large light patches of the photo evened out so the tiling doesn't show.
#   python Tools/make_wood_floor.py   ->  Art/WoodFloor/T_WoodFloor_D.png (colour), _N (normal), _R (roughness),
#                                         T_WoodDetail_N.png (fine pores and fibres, tiled many times up close)
import numpy as np
from PIL import Image
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'WoodFloor'

def blur(a, s):
    """Gaussian blur that wraps round the edges (so it stays seamless): done in the frequency domain."""
    fy = np.fft.fftfreq(a.shape[0])[:, None]
    fx = np.fft.fftfreq(a.shape[1])[None, :]
    k = np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * k)).astype(np.float32)

def normal(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-dx, dy, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return Image.fromarray(((n * 0.5 + 0.5) * 255).round().astype(np.uint8))

src = np.asarray(Image.open(D / 'source.jpg').convert('RGB')).astype(np.float32)
H = src.shape[0]
X0, PL = 472, 525                       # the first plank gap (its left side) and the plank width, in the photo
img = src[:, X0:min(src.shape[1], X0 + 3 * PL)].copy()
W = img.shape[1]

# a plank joint as the photo draws one (column 2 of the cut, the real joint near y 1171): its profile, row by row
lum = img.mean(2)
mid = slice(PL + 30, 2 * PL - 30)
rows = lum[:, mid].mean(1)
jy = int(np.argmin(rows[1050:1300])) + 1050
ring = img[jy - 5:jy + 6, mid].mean(1)                       # 11 rows round the joint
edge = (ring[0] + ring[-1]) / 2
prof = ring - edge                                           # how much darker each row is than the wood beside it

out = img.copy()
for k, roll in enumerate((0, 690, 1380)):
    a, b = k * PL, ((k + 1) * PL if k < 2 else W)
    c = img[:, a:b]
    cl = c.mean(2)[:, 30:-30].mean(1)
    near = int(np.argmin(cl[1650:2000])) + 1650
    if cl[near] < cl.mean() - 12:
        # a real plank end close to the bottom would leave a short stub where the column wraps: end the column on
        # that joint instead (stretched the few per cent back to full height, along the grain where it doesn't show)
        c = np.asarray(Image.fromarray(c[:near + 5].round().astype(np.uint8)).resize((b - a, H), Image.LANCZOS), dtype=np.float32)
        out[:, a:b] = np.roll(c, roll, axis=0)
        continue
    c = np.roll(c, roll, axis=0)
    s = roll % H
    # across the wrap (now at y = roll) the two plank ends meet: a short fade so no hard step shows inside the joint,
    # then the joint itself over it
    top, bot = c[(s - 8) % H].copy(), c[(s + 7) % H].copy()
    for i in range(-7, 7):
        t = (i + 7) / 13.0
        c[(s + i) % H] = c[(s + i) % H] * 0.5 + (top * (1 - t) + bot * t) * 0.5
    for i in range(11):
        y = (s - 5 + i) % H
        c[y] = np.clip(c[y] + prof[i], 0, 255)
    out[:, a:b] = c

# the photo's lighting evened out (the planks keep their own tones)
L = out.mean(2)
lo = blur(L, 180)
gain = (lo.mean() / np.maximum(lo, 1)) ** 0.8
out = np.clip(out * gain[..., None], 0, 255)
alb = Image.fromarray(out.round().astype(np.uint8)).resize((2048, 2048), Image.LANCZOS)
alb.save(D / 'T_WoodFloor_D.png')

# height: the grain, and the gaps and joints as fine grooves
A = np.asarray(alb).astype(np.float32).mean(2) / 255.0
grain = A - blur(A, 5)
gap = np.clip((blur(A, 25) - blur(A, 1.2) - 0.07) * 7.0, 0, 1)
normal(grain * 1.0 - gap * 0.4, 7.0).save(D / 'T_WoodFloor_N.png')

# roughness: a satin finish (a soft sheen, never a mirror); open grain a little rougher, the gaps rough
r = 0.55 + np.clip(-grain * 2.5, -0.06, 0.12) + gap * 0.3
Image.fromarray((np.clip(r, 0, 1) * 255).round().astype(np.uint8)).save(D / 'T_WoodFloor_R.png')

# the fine detail normal: pores and fibres running along the grain (noise stretched along it), seamless
rng = np.random.default_rng(7)
n0 = rng.random((512, 512)).astype(np.float32)
fy = np.fft.fftfreq(512)[:, None]; fx = np.fft.fftfreq(512)[None, :]
fib = np.real(np.fft.ifft2(np.fft.fft2(n0) * np.exp(-2 * np.pi ** 2 * ((fx * 0.8) ** 2 + (fy * 9.0) ** 2)))).astype(np.float32)
fib = (fib - fib.mean()) / (fib.std() + 1e-6) * 0.02
normal(fib, 14.0).save(D / 'T_WoodDetail_N.png')
print('wood floor maps:', alb.size, 'cut', (W, H), 'joint row', jy)
