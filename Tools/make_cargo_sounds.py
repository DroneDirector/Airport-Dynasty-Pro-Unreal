# The cargo terminal's sounds (step B2): CC0 recordings from Freesound (Art/Sounds/CargoTerminal/SOURCES.md), HQ previews
# in raw/, cut here to mono 48 kHz WAVs. Same method as Tools/make_gpu_sounds.py. Loops are crossfaded end into start and
# levelled by loudness (RMS), not peak, so the code's volumes mean the same for each; the printer is one job, faded.
#   python -I Tools/make_cargo_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'Art', 'Sounds', 'CargoTerminal'); RAW = os.path.join(D, 'raw')
FF = imageio_ffmpeg.get_ffmpeg_exe(); SR = 48000


def load(name):
    out = subprocess.run([FF, '-v', 'quiet', '-i', os.path.join(RAW, name + '-hq.mp3'), '-ac', '1', '-ar', str(SR), '-f', 's16le', '-'], capture_output=True).stdout
    return np.frombuffer(out, np.int16).astype(np.float32) / 32768.0


def save(name, a, rms_to=None, peak=0.85):
    if rms_to: a = a / max(1e-6, np.sqrt((a ** 2).mean())) * rms_to
    a = a / max(1.0, np.abs(a).max() / peak) if rms_to else a / max(1e-6, np.abs(a).max()) * peak
    with wave.open(os.path.join(D, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(a, -1, 1) * 32767).astype(np.int16).tobytes())
    print('%-16s %6.2f s  rms %.3f  peak %.3f' % (name, len(a) / SR, np.sqrt((a ** 2).mean()), np.abs(a).max()))


def loop(a, xf=1.0, cut=0.3):
    """A seamless loop: the first and last moments (the recorder's start and stop) cut off, the end crossfaded into the start."""
    a = a[int(cut * SR):len(a) - int(cut * SR)]
    n = int(xf * SR); body, tail = a[:-n].copy(), a[-n:]
    body[:n] = body[:n] * np.sqrt(np.linspace(0, 1, n)) + tail * np.sqrt(np.linspace(1, 0, n))
    return body


def fade(a, fin=0.05, fout=0.5):
    a = a.copy(); i, o = int(fin * SR), int(fout * SR)
    a[:i] *= np.linspace(0, 1, i); a[-o:] *= np.linspace(1, 0, o); return a


save('CgRainRoofOut', loop(load('454142_612689')), 0.12)     # rain drumming on a metal porch roof, outside
save('CgRainRoofIn', loop(load('850595_18477344')), 0.12)     # rain on the metal roof heard from under it
save('CgHallHum', loop(load('451927_612689'), 2.0), 0.08)     # a big warehouse's room tone (its heavy hum)
save('CgPlantHum', loop(load('262885_4337854'), 1.5), 0.12)   # an outdoor heating / ventilation unit running
save('CgOfficeVent', loop(load('723701_12863902'), 1.5), 0.08)  # air from a ventilation outlet
save('CgPrinter', fade(load('212038_1756543')[int(0.2 * SR):]))  # an office printer printing (one job)
