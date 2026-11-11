# The vehicles' dirt textures (owner, 2026-11-11: vehicles and crew get dirty over time), all tileable (periodic noise):
#   T_Dirt_Dust.png     a fine dust film: soft blotches and specks (grey, its strength in the alpha)
#   T_Dirt_Mud.png      mud splatter thrown up by the wheels: drops, flecks and runs (brown, alpha the coverage)
#   T_Dirt_Streaks.png  dirty water running down from edges: long vertical streaks (alpha the coverage)
# The vehicle materials blend them in by a dirt level (0..1), the mud and grime weighted low on the body and round the
# wheels, the streaks under edges. python Art/FireTruck/make_dirt_textures.py
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
N = 1024


def periodic_noise(n, falloff, seed, aspect=(1.0, 1.0)):
    rng = np.random.default_rng(seed)
    fy, fx = np.meshgrid(np.fft.fftfreq(n) * aspect[1], np.fft.fftfreq(n) * aspect[0], indexing='ij')
    r = np.sqrt(fx * fx + fy * fy); r[0, 0] = 1
    spec = (rng.normal(size=(n, n)) + 1j * rng.normal(size=(n, n))) / r ** falloff
    spec[0, 0] = 0
    a = np.real(np.fft.ifft2(spec))
    return (a - a.min()) / (a.max() - a.min())


def save_rgba(rgb, a, name):
    img = np.dstack([np.clip(rgb, 0, 255), np.clip(a, 0, 255)]).astype(np.uint8)
    Image.fromarray(img, 'RGBA').save(os.path.join(HERE, name))


def dust():
    big = periodic_noise(N, 1.6, 1); fine = periodic_noise(N, 0.6, 2)
    specks = (periodic_noise(N, 0.2, 3) > 0.83).astype(float)
    a = np.clip((big - 0.35) * 1.6, 0, 1) * 0.75 + fine * 0.2 + specks * 0.25
    tone = 150 + (fine - 0.5) * 30
    save_rgba(np.dstack([tone, tone * 0.96, tone * 0.88]), a * 255, 'T_Dirt_Dust.png')


def mud():
    rng = np.random.default_rng(5)
    a = np.zeros((N, N))
    yy, xx = np.mgrid[0:N, 0:N]
    for _ in range(900):                                          # drops: round, a few elongated (flung)
        cx, cy = rng.random() * N, rng.random() * N
        r = rng.gamma(1.6, 3.0) + 1.0; stretch = 1 + rng.random() * 2.5 * (rng.random() < 0.4)
        dx = (xx - cx + N / 2) % N - N / 2; dy = (yy - cy + N / 2) % N - N / 2
        d = np.sqrt((dx / stretch) ** 2 + dy ** 2)
        a = np.maximum(a, np.clip(1.2 - d / r, 0, 1))
    a = np.clip(a + (periodic_noise(N, 1.2, 6) - 0.55) * 0.8, 0, 1)
    shade = periodic_noise(N, 0.9, 7)
    rgb = np.dstack([62 + shade * 30, 48 + shade * 22, 34 + shade * 14])
    save_rgba(rgb, a * 255, 'T_Dirt_Mud.png')


def streaks():
    n = periodic_noise(N, 1.2, 9, aspect=(1.0, 0.06))             # stretched down the v axis: long runs
    fine = periodic_noise(N, 0.8, 10, aspect=(1.0, 0.25))
    a = np.clip((n - 0.45) * 2.2, 0, 1) * (0.6 + fine * 0.4)
    tone = 95 + fine * 40
    save_rgba(np.dstack([tone, tone * 0.93, tone * 0.82]), a * 255, 'T_Dirt_Streaks.png')


if __name__ == '__main__':
    dust(); mud(); streaks()
    print('dirt textures written')
