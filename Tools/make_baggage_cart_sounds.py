# The baggage cart's sounds (owner 2026-11-21: every vehicle gets its own kind's sounds): CC0 recordings from Freesound
# (Art/Sounds/BaggageCart/SOURCES.md), HQ previews in raw/, cut here to mono 48 kHz WAVs. Same method as
# Tools/make_gpu_sounds.py. The cart has no motor and no lamps: rolling rattle, drawbar clank, coupling pin in/out,
# curtain up/down.
#   C:/Python311/python.exe -I Tools/make_baggage_cart_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'Art', 'Sounds', 'BaggageCart'); RAW = os.path.join(D, 'raw')
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
    r = rms(a, 0.01); on = r > thr * r.max(); ev = []; s = None; last = -1
    for i, v in enumerate(on):
        if v and s is None: s = i
        if v: last = i
        if s is not None and not v and (i - last) * 0.01 > gap: ev.append((s * 0.01, (last + 1) * 0.01)); s = None
    if s is not None: ev.append((s * 0.01, (last + 1) * 0.01))
    return ev
def cut(a, t0, t1): return a[int(max(0, t0) * SR): int(min(len(a) / SR, t1) * SR)]
def steadiest(a, secs, step=0.25, after=0.0):
    r = rms(a, step); win = int(secs / step); i0 = int(after / step)
    best = min(range(i0, len(r) - win), key=lambda i: r[i:i + win].std() / max(1e-6, r[i:i + win].mean()))
    return best * step

# rolling rattle (loop): a steel cart rolling on hard ground - its steadiest 6 s (after the first push), under it the
# heavy rumble of a large cart's roll-by at its closest 4 s, slowed 15 % for weight; the game pitches/levels it with speed
mc = load('metalcart'); t = steadiest(mc, 6.4, after=5.0); print('rattle window', t)
top = cut(mc, t, t + 6.4)
oc = load('orecart'); r = rms(oc, 0.25); pk = int(np.argmax(r)) * 0.25
low = cut(oc, pk - 2.6, pk + 2.9)
low = np.interp(np.arange(0, len(low) - 1, 0.85), np.arange(len(low)), low)[:len(top)]
mix = top.copy(); mix[:len(low)] += 0.45 * low / max(1e-6, np.abs(low).max()) * np.abs(top).max()
save('BCRollLoop', loop(mix), 0.7)
# drawbar clank: a heavy steel bar dropping on concrete (the drawbar let down onto its stop / springing up against it)
cb = load('crowbar2r'); ev = events(cb, 0.2); print('crowbar events', ev)
save('BCDrawbarClank', fade(cut(cb, ev[0][0] - 0.02, ev[-1][1] + 0.35), 0.002, 0.10))
# coupling pin dropping home (a steel latch falling shut) and lifted out (a gate latch lifted)
lt = load('latch'); ev = events(lt, 0.3, 0.08); print('latch events', ev)
a0, a1 = max(ev, key=lambda e: rms(cut(lt, e[0], e[1] + 0.01), 0.01).max())
save('BCPinIn', fade(cut(lt, a0 - 0.02, a1 + 0.30), 0.002, 0.06))
gl = load('gatelatch'); ev = events(gl, 0.3, 0.08); print('gate latch events', ev)
save('BCPinOut', fade(cut(gl, ev[0][0] - 0.03, ev[0][1] + 0.25), 0.002, 0.06))
# curtain: the PVC sheet rolled up (one tarpaulin handling movement) and pulled down (the next movement)
tp = load('tarpaulin'); ev = events(tp, 0.15, 0.25); print('tarp events', ev)
save('BCCurtainUp', fade(cut(tp, ev[0][0] - 0.05, ev[0][1] + 0.20), 0.01, 0.15), 0.8)
e2 = max(ev[1:], key=lambda e: e[1] - e[0]) if len(ev) > 1 else ev[0]
save('BCCurtainDown', fade(cut(tp, e2[0] - 0.05, e2[1] + 0.20), 0.01, 0.15), 0.8)
# the strap snapped tight on its hook at the end of closing: the quick plastic-tarp pull
pl = load('tarppull'); save('BCCurtainStrap', fade(pl, 0.005, 0.10), 0.7)
