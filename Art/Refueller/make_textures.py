# The refueller's own drawn texture set (to-do 235): ReflectorFacet - the faceted, vacuum-metallised bowl behind a lamp lens
# (small square facets, each tilted its own way, fine polish streaks). Colour, roughness, normal.   python Art/Refueller/make_textures.py
import os, numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); T = os.path.join(HERE, 'Textures'); N = 1024; C = 32
def out(n, s, im):
    d = os.path.join(T, n); os.makedirs(d, exist_ok=True); im.save(os.path.join(d, '%s_2K-JPG_%s.jpg' % (n, s)), quality=93)
rng = np.random.default_rng(235)
y, x = np.mgrid[0:N, 0:N]; cx, cy = x // C, y // C; fx, fy = (x % C) / C - .5, (y % C) / C - .5
tx, ty = rng.uniform(-.35, .35, (N // C, N // C)), rng.uniform(-.35, .35, (N // C, N // C))
nx, ny = tx[cy, cx], ty[cy, cx]
edge = np.maximum(abs(fx), abs(fy)) > .44                                     # the thin ridge between facets
nx = np.where(edge, np.sign(fx) * .6 * (abs(fx) >= abs(fy)), nx); ny = np.where(edge, np.sign(fy) * .6 * (abs(fy) > abs(fx)), ny)
nz = 1 / np.sqrt(nx * nx + ny * ny + 1)
out('ReflectorFacet', 'NormalGL', Image.fromarray((np.stack([nx * nz, -ny * nz, nz], -1) * 127.5 + 127.5).astype('uint8')))
streak = rng.normal(0, 1, N)[None, :].repeat(N, 0); streak = (streak - streak.min()) / np.ptp(streak)
col = .78 + .06 * streak - .10 * edge + rng.normal(0, .01, (N, N))
out('ReflectorFacet', 'Color', Image.fromarray((np.clip(np.stack([col, col, col * 1.02], -1), 0, 1) * 255).astype('uint8')))
out('ReflectorFacet', 'Roughness', Image.fromarray((np.clip(.08 + .05 * streak + .25 * edge, 0, 1) * 255).astype('uint8')))
