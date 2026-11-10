# A seamless, tileable grey metal for the apron light mast: hot-dip galvanised steel (no rust), 2048 x 2048.
# Writes metal_basecolor.png, metal_roughness.png, metal_normal.png (OpenGL / Blender), metal_normal_dx.png (Unreal)
# and metal_height.png next to this script. Every layer is built from periodic noise (FFT), so all edges tile.
#   python make_metal.py
import os
import numpy as np
from PIL import Image

N = 2048
rng = np.random.default_rng(62)
HERE = os.path.dirname(os.path.abspath(__file__))

def noise(beta, aniso=1.0):
    """Periodic fractal noise: white noise shaped in frequency space (1/f^beta); aniso > 1 stretches it along x."""
    fx = np.fft.fftfreq(N)[None, :] / aniso
    fy = np.fft.fftfreq(N)[:, None]
    f = np.sqrt(fx * fx + fy * fy); f[0, 0] = 1
    spec = (rng.normal(size=(N, N)) + 1j * rng.normal(size=(N, N))) / f ** beta
    spec[0, 0] = 0
    n = np.real(np.fft.ifft2(spec))
    return (n - n.mean()) / n.std()

def spangle(cells=220):
    """Zinc spangle: large flat crystal patches of slightly different brightness (periodic Voronoi)."""
    pts = rng.random((cells, 2)) * N
    val = rng.normal(size=cells)
    yy, xx = np.mgrid[0:N:4, 0:N:4].astype(np.float32)
    best = np.full(xx.shape, 1e9, np.float32); out = np.zeros(xx.shape, np.float32)
    for (px, py), v in zip(pts, val):
        dx = np.abs(xx - px); dx = np.minimum(dx, N - dx)
        dy = np.abs(yy - py); dy = np.minimum(dy, N - dy)
        d = dx * dx + dy * dy
        m = d < best; best[m] = d[m]; out[m] = v
    return np.array(Image.fromarray(out).resize((N, N), Image.NEAREST))

mottle = noise(1.6)            # broad cloudy variation
fine = noise(0.6)              # fine grain
brushed = noise(1.1, aniso=14) # faint streaks along the length (rolling / wiping)
sp = spangle()
pits = (noise(0.2) > 3.1).astype(np.float32)   # a few tiny pinholes

# the colour: neutral light grey with a faint cool tint, patches and streaks very subtle
base = 0.60 + 0.035 * mottle + 0.011 * sp + 0.012 * brushed + 0.01 * fine - 0.12 * pits
rgb = np.stack([base * 0.985, base * 0.995, base * 1.01], -1)
Image.fromarray((np.clip(rgb, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)).save(os.path.join(HERE, 'metal_basecolor.png'))

# roughness: satin zinc, rougher in the cloudy patches, smoother along the streaks
rough = 0.38 + 0.06 * mottle + 0.02 * sp - 0.03 * brushed + 0.02 * fine + 0.2 * pits
Image.fromarray((np.clip(rough, 0, 1) * 255).astype(np.uint8)).save(os.path.join(HERE, 'metal_roughness.png'))

# height and normal: gentle orange-peel of the coating, the spangle edges, the streaks
h = 0.5 * noise(1.3) + 0.25 * fine + 0.15 * brushed + 0.12 * sp - 1.5 * pits
h = (h - h.min()) / (h.max() - h.min())
Image.fromarray((h * 65535).astype(np.uint16)).save(os.path.join(HERE, 'metal_height.png'))
gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 6.0
gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 6.0
nz = np.ones_like(h)
n = np.stack([-gx, gy, nz], -1); n /= np.linalg.norm(n, axis=-1, keepdims=True)
Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8)).save(os.path.join(HERE, 'metal_normal.png'))
n[..., 1] *= -1
Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8)).save(os.path.join(HERE, 'metal_normal_dx.png'))
print('metal maps written to', HERE)
