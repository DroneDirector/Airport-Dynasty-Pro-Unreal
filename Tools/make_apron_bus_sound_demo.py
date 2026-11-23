# The apron bus's sound demo (owner 2026-11-23: "perfect, use that one"): a 40 s run - power up and the compressor,
# driving off (whine rising, AVAS fading out over 20 km/h), cruising at 30, regenerating to a stop, the stop brake,
# kneeling, doors open, chime, doors close, rising. THE REFERENCE for the game's MetaSounds: the same layers, gains and
# speed curves below are what the bus must sound like in Unreal. Writes Art/Sounds/ApronBus/ApronBus-SoundDemo.wav.
#   python -I Tools/make_apron_bus_sound_demo.py      (after Tools/make_apron_bus_sounds.py)
import os, wave
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 48000; D = os.path.join(ROOT, 'Art', 'Sounds', 'ApronBus')

# #region ── Helpers ──
def L(n):
    w = wave.open(os.path.join(D, n + '.wav')); return np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32) / 32768
def var_pitch(loopa, ratio):                           # a loop played with a pitch ratio per sample (as a MetaSound pitch input)
    ph = np.cumsum(ratio) % len(loopa); i = ph.astype(int); f = ph - i
    return loopa[i] * (1 - f) + loopa[(i + 1) % len(loopa)] * f
# #endregion

# #region ── The run and the mix (the reference curves) ──
T = 40.0; N = int(T * SR); t = np.arange(N) / SR; out = np.zeros(N, np.float32)
def put(n, at, g=1.0):
    a = L(n); s = int(at * SR); e = min(N, s + len(a)); out[s:e] += g * a[:e - s]
v = np.interp(t, [0, 6, 16, 22, 29, 40], [0, 0, 30, 30, 0, 0])        # km/h
acc = np.gradient(v) * SR                                              # km/h per second
put('ABReady', 0.5, 0.8); put('ABCompressorStart', 2.0, 0.5)
comp = L('ABCompressorLoop'); cs, ce = int(3.6 * SR), int(9 * SR)
out[cs:ce] += 0.45 * np.tile(comp, 10)[:ce - cs] * np.minimum(1, np.linspace(0, 30, ce - cs)) * np.minimum(1, np.linspace(30, 0, ce - cs))
put('ABAirDryerPurge', 9.0, 0.7); put('ABBrakeRelease', 5.3, 0.6)
out += 0.12 * np.tile(L('ABACLoop'), 9)[:N]                                                    # A/C always on
out += 0.55 * var_pitch(L('ABDriveLoop'), 0.85 + 0.3 * np.clip(v / 40, 0, 1)) * np.clip(v / 20, 0, 1)
pull = np.clip(acc / 3, 0, 1)                                                                   # accelerator
out += 0.5 * var_pitch(L('ABMotorWhineLoop'), np.clip(v / 30, 0.1, 1.4)) * np.clip(v / 3, 0, 1) * (0.3 + 0.7 * pull) * (acc >= -0.01)
brake = np.clip(-acc / 4, 0, 1)                                                                 # regeneration
out += 0.45 * var_pitch(L('ABRegenLoop'), np.clip(v / 30, 0.1, 1.4)) * brake * np.clip((v - 1) / 3, 0, 1)
avg = np.where(v <= 20, 1, np.clip((25 - v) / 5, 0, 1)) * (t > 5.5) * (t < 29.2)               # AVAS (off with doors open)
out += 0.22 * var_pitch(L('ABAVASLoop'), 1 + 0.008 * v) * avg
put('ABStopBrakeApply', 29.4, 0.6); put('ABKneelDown', 29.9, 0.6); put('ABDoorOpen', 31.0, 0.8)
put('ABDoorChime', 35.0, 0.4); put('ABDoorClose', 35.9, 0.8); put('ABKneelUp', 37.3, 0.5)
out = out / np.abs(out).max() * 0.9
with wave.open(os.path.join(D, 'ApronBus-SoundDemo.wav'), 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((out * 32767).astype(np.int16).tobytes())
print('demo written')
# #endregion
