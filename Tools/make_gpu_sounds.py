# The ground power unit's sounds (owner 2026-11-21: every vehicle gets its doors' open/close sounds and its own kind's
# sounds): CC0 recordings from Freesound (Art/Sounds/GPU/SOURCES.md), HQ previews in raw/, cut here to mono 48 kHz WAVs.
# Same method as Tools/make_tug_sounds.py.
#   python -I Tools/make_gpu_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'Art', 'Sounds', 'GPU'); RAW = os.path.join(D, 'raw')
FF = imageio_ffmpeg.get_ffmpeg_exe(); SR = 48000

def load(name):
    out = subprocess.run([FF, '-v', 'quiet', '-i', os.path.join(RAW, name + '.mp3'), '-ac', '1', '-ar', str(SR), '-f', 's16le', '-'], capture_output=True).stdout
    return np.frombuffer(out, np.int16).astype(np.float32) / 32768.0
def save(name, a, peak=0.85):
    a = a / max(1e-6, np.abs(a).max()) * peak
    with wave.open(os.path.join(D, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((a * 32767).astype(np.int16).tobytes())
    print('%-18s %.2f s' % (name, len(a) / SR))
def fade(a, fin=0.005, fout=0.02):
    a = a.copy(); i, o = int(fin * SR), int(fout * SR)
    if i: a[:i] *= np.linspace(0, 1, i)
    if o: a[-o:] *= np.linspace(1, 0, o)
    return a
def rms(a, win=0.05):
    n = int(win * SR); k = len(a) // n
    return np.sqrt((a[:k * n].reshape(k, n) ** 2).mean(1))
def loop(a, xf=0.4):
    """A seamless loop: the end crossfaded into the start."""
    n = int(xf * SR); body, tail = a[:-n].copy(), a[-n:]
    body[:n] = body[:n] * np.linspace(0, 1, n) + tail * np.linspace(1, 0, n)
    return body
def events(a, thr=0.25, gap=0.12):
    """Start/end times of the loud parts (for splitting one recording into its events)."""
    r = rms(a, 0.01); on = r > thr * r.max(); ev = []; s = None; last = -1
    for i, v in enumerate(on):
        if v and s is None: s = i
        if v: last = i
        if s is not None and not v and (i - last) * 0.01 > gap: ev.append((s * 0.01, (last + 1) * 0.01)); s = None
    if s is not None: ev.append((s * 0.01, (last + 1) * 0.01))
    return ev
def cut(a, t0, t1): return a[int(max(0, t0) * SR): int(min(len(a) / SR, t1) * SR)]

# engine start: a diesel cranking, catching and settling to idle (the first 10 s; the revving after it is left out)
st = load('dieselstart'); ev = events(st, 0.15)
print('start events', ev[:4])
t0 = max(0.0, ev[0][0] - 0.15)
save('GPUStart', fade(cut(st, t0, t0 + 9.5), 0.01, 1.2))
# idle: the steadiest 10 s of the large diesel generator running, made a seamless loop (the game cross-fades start -> idle)
idl = load('idle'); r = rms(idl, 0.25); win = 40
best = min(range(len(r) - win), key=lambda i: r[i:i + win].std() / max(1e-6, r[i:i + win].mean()))
save('GPUIdle', loop(cut(idl, best * 0.25, (best + win) * 0.25 + 0.4)))
# engine stop: a running mobile generator throttling down and stopping (its last part, from 3 s before the drop)
td = load('throttledown'); r = rms(td, 0.5); full = np.median(r[:40])
drop = next(i for i in range(len(r)) if i > 20 and r[i] < 0.7 * full)
save('GPUStop', fade(cut(td, drop * 0.5 - 3.0, drop * 0.5 + 12.0), 0.3, 1.0))
# access doors (hinged canopy panels): one recording of a metal/plastic cabinet door opened then shut - split in two
dr = load('door'); ev = events(dr)
print('door events', ev)
o0, o1 = ev[0][0], ev[-2][1]; c0, c1 = ev[-1]                  # (the opening is a run of small clicks: latch + hinge)
save('GPUDoorOpen', fade(cut(dr, o0 - 0.08, o1 + 0.30), 0.003, 0.08))
save('GPUDoorClose', fade(cut(dr, c0 - 0.02, c1 + 0.35), 0.002, 0.08))
# the plug into the aircraft's receptacle: the heaviest metal plug-in event of the radio-plug recording
pm = load('plugmetal'); ev = events(pm, 0.3, 0.08)
print('plug events', ev)
a0, a1 = max(ev, key=lambda e: rms(cut(pm, e[0], e[1] + 0.01), 0.01).max())
save('GPUPlugConnect', fade(cut(pm, a0 - 0.02, a1 + 0.25), 0.002, 0.06), 0.8)
# pulling the plug out: the first unplug of the recording
up = load('unplug'); ev = events(up, 0.3, 0.08)
print('unplug events', ev)
u0, u1 = ev[0][0], ev[1][1]                                          # (the catch letting go + the plug sliding out)
save('GPUPlugDisconnect', fade(cut(up, u0 - 0.02, u1 + 0.25), 0.002, 0.06), 0.8)
