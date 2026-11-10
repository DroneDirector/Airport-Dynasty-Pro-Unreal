# The lounge fireplace's sound, made from scratch here (our own, no recordings): a wood fire's soft roar (low,
# slowly breathing filtered noise) with crackles and pops of many sizes scattered through it, a few hisses; 24 s that
# loop seamlessly (everything is made round the loop, so its end runs into its start).
#   python Tools/make_fire_sound.py   ->   Art/Fire/S_FireCrackle.wav (44.1 kHz, mono, 16-bit)
import numpy as np, wave
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'Fire'
D.mkdir(parents=True, exist_ok=True)
SR, SEC = 44100, 24
N = SR * SEC
rng = np.random.default_rng(311)
f = np.fft.rfftfreq(N, 1 / SR)

def shaped(lo, hi, tilt):
    """Noise between lo and hi Hz with a gentle slope, made round the loop (FFT), so it wraps seamlessly."""
    spec = (rng.standard_normal(f.size) + 1j * rng.standard_normal(f.size))
    g = np.where((f > lo) & (f < hi), (np.maximum(f, 1) / 200.0) ** tilt, 0.0)
    g *= np.exp(-((f - hi) / (hi * 0.5)) ** 2 * (f > hi))
    x = np.fft.irfft(spec * g, N)
    return x / (np.abs(x).max() + 1e-9)

t = np.arange(N) / SR
# the roar: low rumble and a softer hiss, breathing slowly (periods that fit the loop exactly)
breath = 0.75 + 0.15 * np.sin(2 * np.pi * t * 3 / SEC) + 0.1 * np.sin(2 * np.pi * t * 7 / SEC + 1.3)
roar = shaped(30, 400, -0.6) * 0.32 * breath + shaped(800, 7000, -0.3) * 0.045 * breath

# the crackles: short bursts of bright noise with a quick decay, many small, some larger pops
out = roar.copy()
count = int(SEC * 9)
for i in range(count):
    at = rng.integers(0, N)
    big = rng.random() < 0.12
    length = int(SR * (rng.uniform(0.004, 0.02) if not big else rng.uniform(0.02, 0.06)))
    env = np.exp(-np.linspace(0, rng.uniform(5, 9), length))
    burst = rng.standard_normal(length)
    # brighter clicks: a simple high-pass (the difference of neighbours)
    burst = np.diff(np.concatenate([[0], burst])) * (0.6 if not big else 1.0)
    amp = rng.uniform(0.05, 0.25) * (2.6 if big else 1.0)
    idx = (at + np.arange(length)) % N
    out[idx] += burst * env * amp
    # now and then a quick run of small crackles after a pop
    if big and rng.random() < 0.6:
        for k in range(rng.integers(2, 6)):
            a2 = (at + int(SR * rng.uniform(0.02, 0.25))) % N
            l2 = int(SR * rng.uniform(0.003, 0.01))
            e2 = np.exp(-np.linspace(0, 7, l2))
            b2 = np.diff(np.concatenate([[0], rng.standard_normal(l2)]))
            out[(a2 + np.arange(l2)) % N] += b2 * e2 * rng.uniform(0.04, 0.12)
# a few soft hisses (steam from the wood)
for i in range(5):
    at = rng.integers(0, N); length = int(SR * rng.uniform(0.6, 1.4))
    env = np.sin(np.linspace(0, np.pi, length)) ** 2
    h = shaped(3000, 9000, 0.0)[:length] * 0.05
    out[(at + np.arange(length)) % N] += h * env

out = out / np.abs(out).max() * 0.85
pcm = (out * 32767).astype(np.int16)
with wave.open(str(D / 'S_FireCrackle.wav'), 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
print('S_FireCrackle.wav', SEC, 's')
