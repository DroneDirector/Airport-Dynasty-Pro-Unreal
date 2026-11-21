# The pushback tug's sounds (owner 2026-11-21: every vehicle gets its doors' open/close sounds and its own kind's sounds):
# CC0 recordings from Freesound (Art/Sounds/PushbackTug/SOURCES.md), HQ previews in raw/, cut here to mono 48 kHz WAVs.
#   python -I Tools/make_tug_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'Art', 'Sounds', 'PushbackTug'); RAW = os.path.join(D, 'raw')
FF = imageio_ffmpeg.get_ffmpeg_exe(); SR = 48000

def load(name):
    out = subprocess.run([FF, '-v', 'quiet', '-i', os.path.join(RAW, name + '.mp3'), '-ac', '1', '-ar', str(SR), '-f', 's16le', '-'], capture_output=True).stdout
    return np.frombuffer(out, np.int16).astype(np.float32) / 32768.0
def save(name, a, peak=0.85):
    a = a / max(1e-6, np.abs(a).max()) * peak
    with wave.open(os.path.join(D, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((a * 32767).astype(np.int16).tobytes())
    print('%-14s %.2f s' % (name, len(a) / SR))
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

# engine start: the whole start of a diesel tractor (the cranking, the catch, the settle)
st = load('start'); save('TugStart', fade(st, 0.002, 0.25))
# idle: the steadiest 8 s of the backhoe idling (lowest loudness spread), made a seamless loop
idl = load('idle'); r = rms(idl, 0.25); win = 32
best = min(range(len(r) - win), key=lambda i: r[i:i + win].std() / max(1e-6, r[i:i + win].mean()))
seg = idl[int(best * 0.25 * SR): int((best + win) * 0.25 * SR) + int(0.4 * SR)]
save('TugIdle', loop(seg))
# engine stop: the idle running down - slower and fading, as a diesel stops when the fuel is cut
stop = idl[int(best * 0.25 * SR): int(best * 0.25 * SR) + int(2.2 * SR)]
idx = np.cumsum(np.linspace(1.0, 0.55, len(stop))); idx = idx[idx < len(stop) - 1].astype(int)
stop = stop[idx] * np.linspace(1, 0, len(idx)) ** 1.5
save('TugStop', fade(stop, 0.01, 0.05), 0.7)
# reversing beeper: the clean synth beeper (loops as recorded); 4 beeps worth
bp = load('beeper'); save('TugReverseBeep', fade(bp[:int(min(len(bp) / SR, 4.0) * SR)], 0.003, 0.003), 0.6)
# door: one recording of a truck door opened and shut - split into its two events
dr = load('door'); ev = events(dr)
print('door events', ev)
o0, o1 = ev[0]; c0, c1 = ev[-1]
save('TugDoorOpen', fade(dr[int(max(0, o0 - 0.03) * SR): int(min(len(dr) / SR, o1 + 0.15) * SR)], 0.003, 0.05))
save('TugDoorClose', fade(dr[int(max(0, c0 - 0.02) * SR): int(min(len(dr) / SR, c1 + 0.35) * SR)], 0.002, 0.08))
# the towbar onto the hitch / the pin: the first heavy metal clang
cl = load('clang'); ev = events(cl, 0.3); a0, a1 = ev[0]
save('TugHitchClunk', fade(cl[int(max(0, a0 - 0.01) * SR): int(min(len(cl) / SR, a1 + 0.4) * SR)], 0.002, 0.1), 0.8)
# horn: the tractor's horn, one blast
hn = load('horn'); ev = events(hn, 0.2); h0, h1 = ev[0][0], ev[-1][1]
save('TugHorn', fade(hn[int(max(0, h0 - 0.01) * SR): int(min(len(hn) / SR, h1 + 0.1) * SR)], 0.005, 0.06), 0.8)
