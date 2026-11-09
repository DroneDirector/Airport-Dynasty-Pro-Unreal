# The first floor's marble (the owner's 3 x 3 tile photo, Art/Marble/source.jpg) made seamless: the nine stones cut
# out of their grout and set again on an even grid, with the same grout line (its colour and width, from the photo)
# between every two stones and round the outside too, so the tile repeats with a joint at its edges exactly like the
# joints inside it. Plus a normal map (the grout a fine groove, the stone very slightly uneven) and a roughness map
# (polished stone, the veins a touch less shiny, the grout matte).
#   python Tools/make_marble.py  ->  Art/Marble/T_Marble_D.png, T_Marble_N.png, T_Marble_R.png (2048 x 2048)
import numpy as np
from PIL import Image
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'Marble'
src = np.asarray(Image.open(D / 'source.jpg').convert('RGB')).astype(np.float32)
# the photo's grout lines (2 px each) and the stones between them
GX = [(332, 333), (665, 666)]
edges = [0, 332, 334, 665, 667, src.shape[0]]
spans = [(0, 332), (334, 665), (667, src.shape[0])]
grout = np.concatenate([src[332:334, :].reshape(-1, 3), src[:, 332:334].reshape(-1, 3), src[665:667, :].reshape(-1, 3), src[:, 665:667].reshape(-1, 3)]).mean(0)
S, G = 330, 2                       # each stone 330 px, the grout 2 px (as the photo)
P = S + G
out = np.zeros((3 * P, 3 * P, 3), np.float32)
mask = np.ones((3 * P, 3 * P), np.float32)          # 1 on stone, 0 in the grout
for j, (y0, y1) in enumerate(spans):
    for i, (x0, x1) in enumerate(spans):
        stone = Image.fromarray(src[y0:y1, x0:x1].round().astype(np.uint8)).resize((S, S), Image.LANCZOS)
        out[j * P + G:(j + 1) * P, i * P + G:(i + 1) * P] = np.asarray(stone, np.float32)
for k in range(3):
    out[k * P:k * P + G, :] = grout; out[:, k * P:k * P + G] = grout
    mask[k * P:k * P + G, :] = 0; mask[:, k * P:k * P + G] = 0
# a shift of one grout width, so the joint round the edge sits half on each side (it meets itself when tiled)
out = np.roll(out, -1, axis=(0, 1)); mask = np.roll(mask, -1, axis=(0, 1))
# the veins a little clearer (the stone's own contrast raised about its mean), so lit floors never look plain white
mean = out[mask > 0].mean(0)
out = np.where(mask[..., None] > 0, mean + (out - mean) * 1.35, out)
N = 2048
alb = Image.fromarray(np.clip(out, 0, 255).round().astype(np.uint8)).resize((N, N), Image.LANCZOS)
alb.save(D / 'T_Marble_D.png')
m = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).resize((N, N), Image.BILINEAR), np.float32) / 255.0

def blur(a, s):
    fy = np.fft.fftfreq(a.shape[0])[:, None]; fx = np.fft.fftfreq(a.shape[1])[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(a) * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2)))).astype(np.float32)

A = np.asarray(alb, np.float32).mean(2) / 255.0
h = blur(m, 1.5) * 1.0 + (A - blur(A, 8)) * 0.15        # the grout a groove; the stone almost flat
dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 4.0
dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 4.0
n = np.dstack([-dx, dy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
Image.fromarray(((n * 0.5 + 0.5) * 255).round().astype(np.uint8)).save(D / 'T_Marble_N.png')
veins = np.clip((blur(A, 20) - A) * 4.0, 0, 1)
r = 0.08 + veins * 0.08 + (1 - m) * 0.6
Image.fromarray((np.clip(r, 0, 1) * 255).round().astype(np.uint8)).save(D / 'T_Marble_R.png')
print('marble maps', N, 'grout colour', grout.round())
