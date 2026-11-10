# The kerb's paving outside the Grand Hall (between the hall and the road, under the wave roof): the owner's 3 x 3
# brown-grey tiles (Art/KerbTiles/source.png) made seamless: the nine tiles cut out of their grout and set again on an
# even grid, the same dark grout between every two and round the outside. Separate layers for the light, not baked
# into the colour: a normal map (the grout a groove, the tiles' surface faintly uneven) and a roughness map (matte
# concrete tiles, the grout rougher), so light only gives them a weak sheen.
#   python Tools/make_kerb_tiles.py  ->  Art/KerbTiles/T_Kerb_D.png, T_Kerb_N.png, T_Kerb_R.png (4096 x 4096)
import numpy as np
from PIL import Image
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'KerbTiles'
src = np.asarray(Image.open(D / 'source.png').convert('RGB')).astype(np.float32)
# the tiles in the photo (inside their grout, a few pixels in from their slightly uneven edges)
# (cut 60 px further in on every side: the photo's tiles have lighter worn rims there, which made a pattern)
spans_x = [(122, 592), (727, 1194), (1328, 1800)]
spans_y = [(120, 590), (724, 1194), (1328, 1800)]
spans = None
grout = np.array([28.0, 25.0, 23.0])
S, G = 600, 5                       # each stone 330 px, the grout 2 px (as the photo)
P = S + G
out = np.zeros((3 * P, 3 * P, 3), np.float32)
stones = []
mask = np.ones((3 * P, 3 * P), np.float32)          # 1 on stone, 0 in the grout
for j, (y0, y1) in enumerate(spans_y):
    for i, (x0, x1) in enumerate(spans_x):
        stone = np.asarray(Image.fromarray(src[y0:y1, x0:x1].round().astype(np.uint8)).resize((S, S), Image.LANCZOS), np.float32)
        out[j * P + G:(j + 1) * P, i * P + G:(i + 1) * P] = stone
        stones.append((j, i))
# every tile evened out (owner: no checkerboard, no light rims): its broad light and dark patches flattened, its tone
# brought to the same colour as the rest with only a slight natural difference, its fine texture kept
stone_mean = np.mean([out[j * P + G:(j + 1) * P, i * P + G:(i + 1) * P].reshape(-1, 3).mean(0) for j, i in stones], axis=0)
for k, (j, i) in enumerate(stones):
    sl = (slice(j * P + G, (j + 1) * P), slice(i * P + G, (i + 1) * P))
    st = out[sl]
    Ls = st.mean(2)
    lo = np.asarray(Image.fromarray(Ls).filter(ImageFilter.GaussianBlur(40)) if False else Ls, np.float32)
    # a wide blur that doesn't bleed past the tile: blurred in a mirrored copy
    big = np.pad(Ls, S // 2, mode='reflect')
    fy = np.fft.fftfreq(big.shape[0])[:, None]; fx = np.fft.fftfreq(big.shape[1])[None, :]
    lo = np.real(np.fft.ifft2(np.fft.fft2(big) * np.exp(-2 * (np.pi * 45) ** 2 * (fx ** 2 + fy ** 2))))[S // 2:S // 2 + S, S // 2:S // 2 + S]
    flat = st * (Ls.mean() / np.maximum(lo, 1))[..., None]
    tone = stone_mean * (1.0 + 0.035 * np.sin(k * 2.39))            # a slight difference from tile to tile
    out[sl] = flat * (tone / np.maximum(flat.reshape(-1, 3).mean(0), 1))
for k in range(3):
    out[k * P:k * P + G, :] = grout; out[:, k * P:k * P + G] = grout
    mask[k * P:k * P + G, :] = 0; mask[:, k * P:k * P + G] = 0
# a shift of one grout width, so the joint round the edge sits half on each side (it meets itself when tiled)
out = np.roll(out, -2, axis=(0, 1)); mask = np.roll(mask, -2, axis=(0, 1))
# the veins a little clearer (the stone's own contrast raised about its mean), so lit floors never look plain white
mean = out[mask > 0].mean(0)
out = np.where(mask[..., None] > 0, mean + (out - mean) * 1.0, out)
N = 4096                       # high resolution: fine grain added at full size below
alb = Image.fromarray(np.clip(out, 0, 255).round().astype(np.uint8)).resize((N, N), Image.LANCZOS)
m0 = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).resize((N, N), Image.BILINEAR), np.float32) / 255.0
def blur0(x, s):
    fy = np.fft.fftfreq(x.shape[0])[:, None]; fx = np.fft.fftfreq(x.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(x) * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2)))).astype(np.float32)
# the tiles' own fine concrete grain and tiny pores at full resolution (on the tiles, not the grout); seamless
rng = np.random.default_rng(5)
g = blur0(rng.standard_normal((N, N)).astype(np.float32), 0.8); g /= g.std() + 1e-6
pores = np.clip(blur0((rng.random((N, N)) > 0.998).astype(np.float32), 1.0) * 30.0, 0, 1)
A0 = np.asarray(alb, np.float32) + ((g * 3.0 - pores * 14.0) * m0)[..., None]
alb = Image.fromarray(np.clip(A0, 0, 255).round().astype(np.uint8))
alb.save(D / 'T_Kerb_D.png')
m = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).resize((N, N), Image.BILINEAR), np.float32) / 255.0

def blur(a, s):
    fy = np.fft.fftfreq(a.shape[0])[:, None]; fx = np.fft.fftfreq(a.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(a) * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2)))).astype(np.float32)

A = np.asarray(alb, np.float32).mean(2) / 255.0
h = blur(m, 2.0) * 1.0 + (A - blur(A, 6)) * 0.6        # the grout a groove; the stone almost flat
dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 4.0
dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 4.0
n = np.dstack([-dx, dy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
Image.fromarray(((n * 0.5 + 0.5) * 255).round().astype(np.uint8)).save(D / 'T_Kerb_N.png')
veins = np.clip((blur(A, 20) - A) * 4.0, 0, 1)
r = 0.78 + veins * 0.06 + (1 - m) * 0.15
Image.fromarray((np.clip(r, 0, 1) * 255).round().astype(np.uint8)).save(D / 'T_Kerb_R.png')
print('kerb tile maps', N, 'grout colour', grout.round())
