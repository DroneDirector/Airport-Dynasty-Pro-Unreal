# The electric belt loader's sounds (owner 2026-11-21: every vehicle gets its doors' open/close sounds and its own kind's
# sounds; belt loader: drive motor, belt motor + rollers, boom hydraulics, reverse beeper, horn). CC0 recordings from
# Freesound (Art/Sounds/BeltLoader/SOURCES.md), HQ previews in raw/, cut here to mono 48 kHz WAVs. Same method as
# Tools/make_baggage_tractor_sounds.py.
#   python -I Tools/make_belt_loader_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'Art', 'Sounds', 'BeltLoader'); RAW = os.path.join(D, 'raw')
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
def cut(a, t0, t1): return a[int(max(0, t0) * SR): int(min(len(a) / SR, t1) * SR)]
def norm(a): return a / max(1e-6, np.sqrt((a ** 2).mean()))
def speed(a, rate):
    if np.isscalar(rate): pos = np.arange(0, len(a) - 1, rate)
    else: pos = np.cumsum(rate); pos = pos[pos < len(a) - 1]
    return np.interp(pos, np.arange(len(a)), a).astype(np.float32)
def lowpass(a, hz):
    X = np.fft.rfft(a); f = np.fft.rfftfreq(len(a), 1 / SR); X[f > hz] *= np.clip(1 - (f[f > hz] - hz) / 2000, 0, 1)
    return np.fft.irfft(X, len(a)).astype(np.float32)
def place(n, *layers):
    out = np.zeros(int(n * SR), np.float32)
    for s, t, g in layers:
        i = int(t * SR); s = s[:len(out) - i]; out[i:i + len(s)] += s * g
    return out
def steadiest(a, secs, step=0.25):
    r = rms(a, step); win = int(secs / step)
    i = min(range(len(r) - win), key=lambda i: r[i:i + win].std() / max(1e-6, r[i:i + win].mean()))
    return cut(a, i * step, (i + win) * step + 0.4)

# drive motor (traction): the steady electric motor hum with a scooter motor's whine an octave down (the tractor's
# recipe), played 10 % slower - a heavier, lower machine. The game pitches it with speed.
hum = lowpass(steadiest(load('elecmotor'), 8.5), 12000)
whine = lowpass(speed(cut(load('scooterloop'), 1.0, 1.0 + len(hum) / SR / 2 + 0.01), 0.5)[:len(hum)], 10000)
drive = lowpass(speed(norm(hum) + 0.45 * norm(whine), 0.9), 9000)
save('BLDriveLoop', loop(drive))
# belt running: a real package-centre conveyor (its steadiest 20 s) with the rollers' resonant clatter under it
conv = steadiest(load('conveyor'), 20.0)
clat = load('motorclatter'); clat = steadiest(cut(clat, 6.0, len(clat) / SR), 20.0)[:len(conv)]
save('BLBeltLoop', loop(norm(conv) + 0.35 * norm(np.pad(clat, (0, len(conv) - len(clat))))))
# belt start: the motor winding up with its clank (first 3 s of the clatter recording) into the running belt
start = fade(cut(load('motorclatter'), 0.0, 3.0), 0.003, 0.6)
save('BLBeltStart', fade(place(3.4, (norm(start), 0.0, 1.0), (norm(conv)[:int(1.6 * SR)] * np.linspace(0, 1, int(1.6 * SR)), 1.8, 0.8)), 0.0, 0.2))
# boom hydraulics: a forklift's lift hydraulics (pump + oil flow). Up: the pump contactor click, then the steadiest
# 2.5 s made a loop; down: the same flow 12 % slower and softer (the boom lowers through the valve under its own weight);
# stop: the valve closing - the flow pitching down fast with the contactor dropping out.
hyd = load('hydraulics'); con = load('contactor')
hloop = loop(lowpass(steadiest(hyd, 2.5), 14000), 0.3)
save('BLBoomUpLoop', hloop)
save('BLBoomDownLoop', loop(lowpass(speed(hloop, 0.88), 9000), 0.3), 0.6)
click = fade(cut(con, 21.84, 22.10), 0.001, 0.06)
save('BLBoomStart', fade(place(1.2, (norm(click), 0.0, 0.7), (norm(hloop)[:int(1.1 * SR)] * np.linspace(0, 1, int(1.1 * SR)) ** 0.5, 0.06, 1.0)), 0.0, 0.1))
rate = np.linspace(1.0, 0.35, int(0.9 * SR)); down = lowpass(speed(hloop, rate), 7000); down *= np.linspace(1, 0, len(down)) ** 1.5
save('BLBoomStop', fade(place(1.1, (norm(down), 0.0, 1.0), (norm(fade(cut(con, 34.32, 34.60), 0.001, 0.06)), 0.35, 0.6)), 0.0, 0.1))
# reversing beeper: a truck's reversing beeper (louder, longer chassis than the tractor), loopable as it is
save('BLReverseBeep', load('truckbeeper'), 0.8)
# horn: the small electric horn (as on the tractor), one beep
hn = load('horn')
save('BLHorn', fade(cut(hn, 2.46, 3.05), 0.004, 0.12), 0.8)       # (its second, longer beep)
# cab door: a small car door opened and shut (split in two)
dr = load('door')
save('BLDoorOpen', fade(cut(dr, 0.74, 1.62), 0.003, 0.08))
save('BLDoorClose', fade(cut(dr, 1.95, 2.95), 0.002, 0.10))
