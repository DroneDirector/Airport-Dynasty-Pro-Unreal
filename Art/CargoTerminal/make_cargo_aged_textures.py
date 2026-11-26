# Step B3 (quality pass): the cargo terminal's cladding as it looks after a few winters, from the clean sets that
# make_cargo_textures.py writes (run that first; this one only reads them, so running it twice changes nothing):
#   CladdingAged / CladdingBandAged  the sandwich panels with their fixings (a screw pair at every rail, both sides of
#                                    each horizontal joint) and light dirt streaks running down from each joint
#   Grime                           the concrete plinth along the foot of the walls: splash dirt, darkest at the ground
# One tile = TILE[name] metres (make_cargo_textures.TILE). Seamless.   python Art/CargoTerminal/make_cargo_aged_textures.py
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__)); TEX = os.path.join(HERE, 'Textures')
rng = np.random.default_rng(7)


def load(name, gray=False):
    im = Image.open(os.path.join(TEX, name)).convert('L' if gray else 'RGB')
    return np.asarray(im, np.float32) / 255.0


def save(arr, name):
    Image.fromarray(np.clip(arr * 255 + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(TEX, name), quality=92)


def smooth_noise(n, cells, seed):
    """Seamless value noise: random cells, bilinear between them (wraps)."""
    r = np.random.default_rng(seed).random((cells, cells)).astype(np.float32)
    t = np.arange(n) / n * cells; i0 = np.floor(t).astype(int); f = (t - i0)[:, None]; i1 = (i0 + 1) % cells
    rows = r[i0] * (1 - f) + r[i1] * f
    f = (t - i0)[None, :]
    return rows[:, i0] * (1 - f) + rows[:, i1] * f


def aged(src, out, tile=4.0):
    c = load(src + '_Color.jpg'); n = load(src + '_Normal.jpg'); orm = load(src + '_ORM.jpg')
    S = c.shape[0]; ppm = S / tile
    z = (np.arange(S) + 0.5) / ppm                       # metres down the tile (image rows run down the wall)
    d = (z % 1.0)                                        # metres below the joint above
    x = (np.arange(S) + 0.5) / ppm
    # dirt streaks: thin vertical runs from the joint, fading down the panel, uneven along the wall
    runs = smooth_noise(S, 64, 1)[0] * 0.6 + smooth_noise(S, 16, 2)[0] * 0.4
    runs = np.clip((runs - 0.45) * 2.5, 0, 1)
    fade = np.exp(-d / 0.32)[:, None]
    streak = fade * runs[None, :] * 0.22 * (0.7 + 0.3 * smooth_noise(S, 32, 3))
    col = c * (1.0 - streak[..., None]) + np.array([0.20, 0.18, 0.15])[None, None, :] * streak[..., None] * 0.35
    # fixings: a self-drilling screw with a washer, every 1.0 m along, 40 mm above and below each joint
    yy, xx = np.mgrid[0:S, 0:S]
    rad = 0.008 * ppm
    dz = np.minimum(np.abs(d - 0.04), np.abs(d - 0.96))[:, None] * ppm
    dx = np.abs(((x + 0.5) % 1.0) - 0.5)[None, :] * ppm
    r2 = dz ** 2 + dx ** 2
    head = np.clip(1.0 - np.sqrt(r2) / rad, 0, 1)
    col = col * (1.0 - 0.25 * (head > 0)[..., None]) + 0.12 * head[..., None]
    # the screw head domes out of the normal map
    h = head ** 0.5
    gy, gx = np.gradient(h)
    nn = n * 2 - 1
    nn[..., 0] -= gx * 2.5; nn[..., 1] += gy * 2.5
    nn /= np.linalg.norm(nn, axis=2, keepdims=True)
    rough = orm.copy(); rough[..., 1] = np.clip(orm[..., 1] + 0.12 * streak - 0.15 * (head > 0), 0, 1)
    save(col, out + '_Color.jpg'); save(nn * 0.5 + 0.5, out + '_Normal.jpg'); save(rough, out + '_ORM.jpg')


def grime(tile=0.6):
    """Concrete plinth (the fire wall's concrete), splashed dirty from the ground up: darkest at the bottom of the tile."""
    c = load('FireWall_Color.jpg'); n = load('FireWall_Normal.jpg'); orm = load('FireWall_ORM.jpg')
    S = c.shape[0]
    v = 1.0 - (np.arange(S) + 0.5) / S                    # 1 at the top row, 0 at the bottom (the ground)
    splash = np.clip(1.0 - v / 0.55, 0, 1)[:, None] ** 1.6 * (0.6 + 0.4 * smooth_noise(S, 48, 4))
    spots = np.clip((smooth_noise(S, 96, 5) - 0.62) * 4, 0, 1) * np.clip(1 - v / 0.8, 0, 1)[:, None]
    dirt = np.clip(splash * 0.55 + spots * 0.3, 0, 0.8)
    col = c * (1.0 - dirt[..., None]) + np.array([0.16, 0.14, 0.11])[None, None, :] * dirt[..., None] * 0.6
    o = orm.copy(); o[..., 1] = np.clip(orm[..., 1] + 0.1 * dirt, 0, 1)
    save(col, 'Grime_Color.jpg'); save(n, 'Grime_Normal.jpg'); save(o, 'Grime_ORM.jpg')


if __name__ == '__main__':
    aged('Cladding', 'CladdingAged'); aged('CladdingBand', 'CladdingBandAged'); grime()
    print('aged textures written')
