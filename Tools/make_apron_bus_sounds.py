# The apron bus's sounds (owner 2026-11-23: "real world sound effects, it is an EV bus, so make that realistic"): an
# electric low-floor bus on battery power, so no engine. What you hear on a real one:
#   the traction drive (a low rumble with the tyres and body) and the electric motor's whine rising and falling with
#   speed, a softer whine while it brakes on regeneration, the pedestrian warning sound (AVAS, required on electric
#   buses under UN R138 up to 20 km/h, its pitch rising with speed), the "ready" contactor clunk and chime, the electric
#   air compressor cutting in and out with the air dryer's purge, air brakes, kneeling, pneumatic doors with their
#   warning chime, the roof A/C, the indicator relay, the reverse beeper and the horn.
# Real recordings where the sound is a real machine's (CC0, Freesound, already in the repo; Art/Sounds/ApronBus/
# SOURCES.md), and our own synthesis only where the real thing is itself a made-up electronic sound (AVAS, the door
# chime, the indicator relay's tick) or plain noise (the A/C fans). Mono 48 kHz WAVs into Art/Sounds/ApronBus/.
#   python -I Tools/make_apron_bus_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg
from scipy.signal import butter, sosfilt, resample

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SND = os.path.join(ROOT, 'Art', 'Sounds'); D = os.path.join(SND, 'ApronBus'); os.makedirs(D, exist_ok=True)
FF = imageio_ffmpeg.get_ffmpeg_exe(); SR = 48000
rng = np.random.default_rng(23)

# #region ── Helpers ──
def load(path):
    p = os.path.join(SND, path)
    out = subprocess.run([FF, '-v', 'quiet', '-i', p, '-ac', '1', '-ar', str(SR), '-f', 's16le', '-'], capture_output=True).stdout
    a = np.frombuffer(out, np.int16).astype(np.float32) / 32768.0
    assert len(a) > SR // 10, 'missing or empty: ' + path
    return a
def save(name, a, peak=0.85):
    a = a / max(1e-6, np.abs(a).max()) * peak
    with wave.open(os.path.join(D, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((a * 32767).astype(np.int16).tobytes())
    print('%-22s %5.2f s' % (name, len(a) / SR))
def fade(a, fin=0.005, fout=0.02):
    a = a.copy(); i, o = int(fin * SR), int(fout * SR)
    if i: a[:i] *= np.linspace(0, 1, i)
    if o: a[-o:] *= np.linspace(1, 0, o)
    return a
def cut(a, t0, t1): return a[int(max(0, t0) * SR): int(min(len(a) / SR, t1) * SR)]
def rms(a, win=0.05):
    n = int(win * SR); k = len(a) // n
    return np.sqrt((a[:k * n].reshape(k, n) ** 2).mean(1))
def loop(a, xf=0.4):
    n = int(xf * SR); body, tail = a[:-n].copy(), a[-n:]
    body[:n] = body[:n] * np.linspace(0, 1, n) + tail * np.linspace(1, 0, n)
    return body
def speed(a, k):                                    # play k times faster (pitch up), k < 1 slower and deeper
    return resample(a, int(len(a) / k)).astype(np.float32)
def bp(a, lo, hi, order=4): return sosfilt(butter(order, [lo, hi], 'bandpass', fs=SR, output='sos'), a)
def hp(a, f, order=4): return sosfilt(butter(order, f, 'highpass', fs=SR, output='sos'), a)
def lp(a, f, order=4): return sosfilt(butter(order, f, 'lowpass', fs=SR, output='sos'), a)
def norm(a): return a / max(1e-6, np.abs(a).max())
def mix(*parts):
    n = max(len(p) for p, _ in parts); out = np.zeros(n, np.float32)
    for p, g in parts: out[:len(p)] += g * p
    return out
def steadiest(a, win_s, step=0.1):
    r = rms(a, step); w = int(win_s / step)
    i = min(range(len(r) - w), key=lambda i: r[i:i + w].std() / max(1e-6, r[i:i + w].mean()))
    return cut(a, i * step, i * step + win_s)
def pink(n):
    w = rng.standard_normal(n); f = np.fft.rfft(w); f[1:] /= np.sqrt(np.arange(1, len(f))); return norm(np.fft.irfft(f, n)).astype(np.float32)
t_ = lambda s: np.arange(int(s * SR)) / SR
# #endregion

# #region ── The recordings (CC0, in the repo already; see SOURCES.md) ──
TROLLEY = load('Coach/CoachDrive.wav')                 # an electric trolleybus driving (felix.blume)
BUSDOOR = load('Coach/CoachDoor.wav')                  # a bus door opening (BabelFishTank)
AIR = load('Coach/CoachKneel.wav')                     # truck air brakes' hiss (Filmscore)
MOTOR = load('BaggageTractor/raw/elecmotor.mp3')       # an electric motor's hum (derjuli)
SCOOT = load('BaggageTractor/raw/scooterloop.mp3')     # an electric scooter's motor whine (pkri)
CONT = load('BaggageTractor/raw/contactor.mp3')        # contactor / relay thunks (tt_runscript)
EVSTART = load('BaggageTractor/raw/evstart.mp3')       # an electric car's ready beep (evsecrets)
PUMP = load('BeltLoader/raw/hydraulics.mp3')           # an electric pump motor running (nuckan)
BEEPER = load('BeltLoader/raw/truckbeeper.mp3')        # a truck's reverse beeper (Disasteradio)
HORN = load('PushbackTug/raw/horn.mp3')                # a heavy vehicle's horn (Camilo_Vargas13)
# #endregion

# #region ── Driving: the drive rumble, the motor whine, regeneration, the AVAS ──
# drive: the trolleybus's 12 s of driving (rumble, tyres, body), the very lowest end tamed; the game fades it in with speed
drive = hp(TROLLEY, 30, 2)
save('ABDriveLoop', loop(drive, 1.0))
# motor whine at about 30 km/h: the electric motor's hum and the scooter motor's whine taken down to a big traction motor's
# range, plus the motor's own tonal "orders" (what makes an electric bus sing: a set of related tones rising with speed)
mot = steadiest(MOTOR, 8.0); sco = speed(steadiest(SCOOT, 4.2), 0.55)
n = int(8.0 * SR); sco = np.tile(sco, 3)[:n]
T = t_(8.0); f0 = 310.0                                # 8 s with whole cycles: 310 Hz x 8 s, the orders at 1, 1.5, 2, 3, 4.5
orders = sum(g * np.sin(2 * np.pi * round(f0 * k * 8) / 8 * T + k) for k, g in ((1, 1.0), (1.5, 0.35), (2, 0.55), (3, 0.25), (4.5, 0.12)))
orders = orders * (1 + 0.08 * np.sin(2 * np.pi * 0.5 * T))     # a slow beat, as the real tones drift against each other
whine = mix((norm(hp(mot, 120)), 0.55), (norm(hp(sco, 150)), 0.45), (norm(orders), 0.30))
save('ABMotorWhineLoop', loop(whine, 0.6))
# regeneration (braking with the motor): the same tones, softer, darker and with less of the scooter's edge
regen = mix((norm(lp(hp(mot, 100), 1800)), 0.6), (norm(lp(orders, 2500)), 0.45), (norm(lp(hp(sco, 150), 1500)), 0.15))
save('ABRegenLoop', loop(regen, 0.6), peak=0.7)
# AVAS (the pedestrian warning sound; electronic on every real EV bus, so our own): a soft two-band tone (one band under
# 1.6 kHz as UN R138 asks) with a breath of filtered noise, steady; the game raises its pitch with speed and stops it
# above 20 km/h. Forward: steady. Reverse: the same, pulsing.
T = t_(4.0)
tone = sum(g * np.sin(2 * np.pi * round(f * 4) / 4 * T + i) for i, (f, g) in enumerate(((220, 1.0), (440, 0.6), (660, 0.35), (880, 0.25),
                                                                                      (1320, 0.18), (1760, 0.10), (223.25, 0.4), (443.25, 0.2))))
breath = bp(pink(len(T)), 500, 2400) * 0.25
avas = norm(tone) * 0.8 + norm(breath) * 0.2
save('ABAVASLoop', loop(avas, 0.5), peak=0.7)
T = t_(1.0); env = np.clip(np.sin(np.pi * np.clip(T / 0.55, 0, 1)), 0, 1) ** 0.6        # 0.55 s on, 0.45 s off
save('ABAVASReverseLoop', np.tile(norm(tone[:SR]) * env, 2), peak=0.7)
# #endregion

# #region ── Powering up and down ──
thunk_in = cut(CONT, 21.80, 22.30); thunk_out = cut(CONT, 34.28, 34.78)
chime = cut(EVSTART, 13.8, 14.7)
ready = np.zeros(int(1.9 * SR), np.float32)
ready[:len(thunk_in)] += norm(thunk_in) * 0.9
k = int(0.12 * SR); ready[k:k + len(thunk_in)] += norm(thunk_in) * 0.5         # the second (precharge -> main) contactor
c0 = int(0.75 * SR); ready[c0:c0 + len(chime)] += norm(chime) * 0.6
save('ABReady', fade(ready, 0.002, 0.15))
wd = speed(whine[:int(6 * SR)], 1.0)                                              # the whine winding down to nothing
k = np.linspace(1.0, 0.15, len(wd)); idx = np.cumsum(k); idx = idx[idx < len(wd) - 1].astype(int)
wind = wd[idx] * np.linspace(1, 0, len(idx)) ** 1.5
off = np.zeros(len(wind) + int(0.6 * SR), np.float32); off[:len(wind)] += norm(wind) * 0.35
off[int(0.15 * SR):int(0.15 * SR) + len(thunk_out)] += norm(thunk_out) * 0.9
save('ABShutdown', fade(off, 0.002, 0.3))
# #endregion

# #region ── Air: the electric compressor, the dryer purge, brakes, kneeling ──
comp = steadiest(PUMP, 2.5)
comp = mix((norm(hp(comp, 60)), 1.0), (norm(hp(speed(comp, 0.8), 60)), 0.35))   # a deeper body under it
save('ABCompressorLoop', loop(comp, 0.5), peak=0.75)
cs = np.zeros(int(1.6 * SR), np.float32); cs[:len(thunk_in)] += norm(thunk_in) * 0.8
ramp = np.tile(comp, 2)[:int(1.4 * SR)] * np.linspace(0, 1, int(1.4 * SR)) ** 0.7; cs[int(0.2 * SR):int(0.2 * SR) + len(ramp)] += norm(comp)[0] * 0 + ramp * 0.7
save('ABCompressorStart', fade(cs, 0.002, 0.08), peak=0.75)
h = hp(AIR, 400)
r = rms(h, 0.02); pk = int(np.argmax(r)) * 0.02
purge = cut(h, pk - 0.03, pk + 0.75) * np.exp(-np.linspace(0, 4.5, int(0.78 * SR)))[:len(cut(h, pk - 0.03, pk + 0.75))]
save('ABAirDryerPurge', fade(purge, 0.003, 0.05))                                  # the "pssht" when the compressor stops
save('ABBrakeRelease', fade(cut(h, pk - 0.02, pk + 0.38) * np.linspace(1, 0.2, len(cut(h, pk - 0.02, pk + 0.38))), 0.003, 0.06), peak=0.7)
save('ABStopBrakeApply', fade(lp(cut(h, pk + 0.4, pk + 0.75), 4000), 0.01, 0.08), peak=0.55)   # the bus-stop brake as the doors open
save('ABKneelDown', fade(hp(AIR, 150), 0.02, 0.5))                                 # air let out of the bellows: the body sinks 80 mm
save('ABKneelUp', fade(lp(speed(hp(AIR, 150), 0.92), 3500), 0.15, 0.6), peak=0.6)  # bellows refilling, softer
# #endregion

# #region ── Doors: open, close, the warning chime ──
save('ABDoorOpen', fade(BUSDOOR, 0.005, 0.1))
T = t_(0.25); thud = np.sin(2 * np.pi * 70 * T) * np.exp(-T * 22) + 0.5 * lp(rng.standard_normal(len(T)), 400) * np.exp(-T * 40)
mv = speed(BUSDOOR, 1.08)[int(0.1 * SR):]                                         # the leaves swinging back in on air
close = np.zeros(len(mv) + int(0.3 * SR), np.float32); close[:len(mv)] += norm(mv) * 0.8
close[len(mv) - int(0.05 * SR):len(mv) - int(0.05 * SR) + len(thud)] += norm(thud) * 0.9       # the rubber edges meeting
save('ABDoorClose', fade(close, 0.01, 0.12))
T = t_(0.16); beep = (np.sin(2 * np.pi * 1600 * T) + 0.3 * np.sin(2 * np.pi * 3200 * T)) * np.minimum(1, np.minimum(T, 0.16 - T) / 0.008)
ch = np.zeros(int(0.78 * SR), np.float32)
for i in range(3): s = int(i * 0.26 * SR); ch[s:s + len(beep)] += beep
save('ABDoorChime', ch, peak=0.6)
# #endregion

# #region ── Cab and the rest: A/C, indicator relay, reverse beeper, horn ──
T = t_(6.0); fan = norm(bp(pink(len(T)), 90, 1800)) * 0.8                          # roof A/C: the fans' air and their hum
blade = sum(g * np.sin(2 * np.pi * round(f * 6) / 6 * T) for f, g in ((118, 1.0), (236, 0.4), (354, 0.2)))
save('ABACLoop', loop(fan + 0.15 * norm(blade) * (1 + 0.1 * np.sin(2 * np.pi * 0.33 * T)), 0.8), peak=0.5)
T = t_(0.03)
def click(f, dec): return (np.sin(2 * np.pi * f * T) * 0.6 + hp(rng.standard_normal(len(T)), 1500) * 0.4) * np.exp(-T * dec)
ind = np.zeros(int(0.8 * SR), np.float32); ind[:len(T)] += click(3100, 260); s = int(0.4 * SR); ind[s:s + len(T)] += click(2400, 220) * 0.8
save('ABIndicatorLoop', ind, peak=0.5)                                             # tick on, tock off: 75 flashes a minute
b = BEEPER; r = rms(b, 0.01); on = np.where(r > 0.3 * r.max())[0]
first = on[0] * 0.01; nxt = next(on[i] for i in range(1, len(on)) if on[i] - on[i - 1] > 10) * 0.01
save('ABReverseBeep', fade(cut(b, first - 0.01, nxt - 0.01), 0.003, 0.01))
r = rms(HORN, 0.02); on = np.where(r > 0.3 * r.max())[0]
save('ABHorn', fade(cut(HORN, on[0] * 0.02 - 0.02, on[-1] * 0.02 + 0.1), 0.005, 0.08))
# #endregion
print('apron bus sounds done')
