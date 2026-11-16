# The baggage belts' sounds (to-do 72), from Freesound CC0 HQ previews in raw/ (sources in SOURCES.md): mono 48 kHz WAVs
#   BeltStart.wav  the carousel switched on: the motor's whir winding up with a metal clank as the plates take up
#   BeltRun.wav    the carousel running: a steady close conveyor rumble, cut to loop without a seam (crossfaded ends)
#   BeltClack.wav  one plate joint clacking (played now and then as plates pass the curves / a bag drops on)
#   python Art/Sounds/BaggageClaim/make_claim_sounds.py
import os, wave
import numpy as np, miniaudio
HERE = os.path.dirname(os.path.abspath(__file__)); RAW = os.path.join(HERE, 'raw'); SR = 48000
def load(n):
    d = miniaudio.decode_file(os.path.join(RAW, n), output_format=miniaudio.SampleFormat.FLOAT32, nchannels=1, sample_rate=SR)
    return np.frombuffer(d.samples, dtype=np.float32).copy()
def save(n, x, peak=0.8):
    x = x / max(1e-6, np.abs(x).max()) * peak
    with wave.open(os.path.join(HERE, n), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((x * 32767).astype(np.int16).tobytes())
    print('SOUND', n, round(len(x) / SR, 2), 's')
def fade(x, a=0.02, b=0.3):
    n0, n1 = int(a * SR), int(b * SR); x = x.copy()
    x[:n0] *= np.linspace(0, 1, n0); x[-n1:] *= np.linspace(1, 0, n1); return x
def first_loud(x, thr=0.08):
    e = np.abs(x) > thr * np.abs(x).max(); i = int(np.argmax(e)); return max(0, i - int(0.05 * SR))
# start-up: from where the motor first sounds, 6 s
s = load('438652.mp3'); i = first_loud(s); save('BeltStart.wav', fade(s[i:i + 6 * SR], 0.01, 0.8))
# running: 20 s from the middle of the recording, its end crossfaded into its start (seamless loop)
r = load('519027.mp3'); m = len(r) // 2; seg = r[m - 10 * SR:m + 10 * SR + SR].copy(); X = SR
loop = seg[:-X].copy(); loop[:X] = loop[:X] * np.linspace(0, 1, X) + seg[-X:] * np.linspace(1, 0, X)
save('BeltRun.wav', loop, 0.6)
# one clack
c = load('827583.mp3'); i = first_loud(c); save('BeltClack.wav', fade(c[i:i + int(0.6 * SR)], 0.002, 0.2), 0.7)
