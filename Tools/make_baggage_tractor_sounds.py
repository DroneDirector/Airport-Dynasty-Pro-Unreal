# The electric baggage tow tractor's sounds (owner 2026-11-21: every vehicle gets its doors' open/close sounds and its own
# kind's sounds; this one is battery-electric, so no engine start - a contactor clunk and a ready chime instead): CC0
# recordings from Freesound (Art/Sounds/BaggageTractor/SOURCES.md), HQ previews in raw/, cut here to mono 48 kHz WAVs.
# Same method as Tools/make_gpu_sounds.py / Tools/make_tug_sounds.py.
#   python -I Tools/make_baggage_tractor_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'Art', 'Sounds', 'BaggageTractor'); RAW = os.path.join(D, 'raw')
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
def norm(a): return a / max(1e-6, np.sqrt((a ** 2).mean()))         # to unit RMS (for mixing layers)
def speed(a, rate):
    """Varispeed playback (pitch and tempo together): rate is a number or a per-output-sample array. Slowing down makes
    linear-interpolation images above SR/4, so follow it with lowpass()."""
    if np.isscalar(rate): pos = np.arange(0, len(a) - 1, rate)
    else: pos = np.cumsum(rate); pos = pos[pos < len(a) - 1]
    return np.interp(pos, np.arange(len(a)), a).astype(np.float32)
def lowpass(a, hz):
    """Cut everything above hz (soft 2 kHz roll-off)."""
    X = np.fft.rfft(a); f = np.fft.rfftfreq(len(a), 1 / SR); X[f > hz] *= np.clip(1 - (f[f > hz] - hz) / 2000, 0, 1)
    return np.fft.irfft(X, len(a)).astype(np.float32)
def place(n, *layers):
    """Mix (sound, start time, gain) layers into one n-second track."""
    out = np.zeros(int(n * SR), np.float32)
    for s, t, g in layers:
        i = int(t * SR); s = s[:len(out) - i]; out[i:i + len(s)] += s * g
    return out

# traction motor while driving: a steady electric motor hum (its steadiest 8 s) with an electric scooter's motor whine
# played an octave down on top (a tow tractor's motor is bigger and lower than a scooter's), made a seamless loop.
# The game pitches it with speed.
hum = load('elecmotor'); r = rms(hum, 0.25); win = 34
best = min(range(len(r) - win), key=lambda i: r[i:i + win].std() / max(1e-6, r[i:i + win].mean()))
hum = lowpass(cut(hum, best * 0.25, (best + win) * 0.25 + 0.4), 12000)
whine = lowpass(speed(cut(load('scooterloop'), 1.0, 1.0 + len(hum) / SR / 2 + 0.01), 0.5)[:len(hum)], 10000)
motor = norm(hum) + 0.5 * norm(whine)
save('BTMotorLoop', loop(motor))
# key-on: the main contactor pulling in (a loud thunk) and the drive system's ready chime (an electric car's ready beep)
con = load('contactor')
thunk_on = fade(cut(con, 21.84, 22.10), 0.001, 0.06)                # "the very loud thunk noises" = the contactor itself
ev = load('evstart'); chime = fade(cut(ev, 13.80, 14.70), 0.005, 0.15)
save('BTStart', fade(place(1.4, (norm(thunk_on), 0.03, 1.0), (norm(chime), 0.45, 1.6)), 0.0, 0.1))
# key-off: the contactor dropping out and the motor's whine winding down to nothing
thunk_off = fade(cut(con, 34.32, 34.60), 0.001, 0.06)
m = loop(motor)[:int(2.2 * SR)]; rate = np.linspace(1.0, 0.25, int(4 * SR)) ** 1.5
down = lowpass(speed(m, rate), 8000); down = down * np.linspace(1, 0, len(down)) ** 2
save('BTStop', fade(place(2.0, (norm(thunk_off), 0.02, 1.0), (norm(down), 0.0, 0.35)), 0.0, 0.15))
# reversing beeper: a generic machinery backup beeper, one beep + gap (loopable as it is)
save('BTReverseBeep', load('beeper'), 0.8)
# cab door: a small car door opened and shut from inside - split in two
dr = load('door')
save('BTDoorOpen', fade(cut(dr, 0.74, 1.62), 0.003, 0.08))
save('BTDoorClose', fade(cut(dr, 1.95, 2.95), 0.002, 0.10))
# horn: a small car's electric horn, one short beep (the second, longer beep of the recording)
hn = load('horn')
save('BTHorn', fade(cut(hn, 2.46, 3.05), 0.004, 0.12), 0.8)
# hitch pin dropped in: a steel bolt sliding home with its clunk, with a heavy steel pole's set-down impact under it for weight
bl = load('bolt'); pl = load('pole')
pin = fade(cut(bl, 12.45, 13.10), 0.003, 0.08)
k = int((12.80 - 12.45) * SR) + np.argmax(np.abs(cut(bl, 12.80, 12.95)))     # the clunk's onset in the bolt cut
weight = fade(cut(pl, 13.04, 13.60), 0.002, 0.15)
save('BTHitchPin', place(0.9, (pin / np.abs(pin).max(), 0.0, 1.0), (weight / np.abs(weight).max(), k / SR - 0.02, 0.5)), 0.85)
# hitch pin pulled out: the same bolt sliding out, ending in a small clink
save('BTPinOut', fade(cut(bl, 20.30, 21.00), 0.01, 0.08), 0.7)
