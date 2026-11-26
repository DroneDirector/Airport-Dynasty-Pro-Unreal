# The knock on your office door (OfficeCall.cpp): a real recording, CC0 from Freesound - Art/Sounds/Office/SOURCES.md.
# "Knocking On a Wooden Door" by ExplosiveTamale, https://freesound.org/people/ExplosiveTamale/sounds/448359/ (CC0):
# its three knocks at 12.49 / 12.705 / 12.915 s, cut 12.40-13.35 s, mono 48 kHz, peak to -2 dB, a 0.15 s fade out.
# The knocks then land at 0.09 / 0.305 / 0.515 s in Knock.wav - OfficeCall's hand IK hits the door at those times.
#   python -I Tools/make_office_knock_sound.py <the HQ preview 448359_4844765-hq.mp3>
import sys, os, subprocess, wave, numpy as np, imageio_ffmpeg
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = sys.argv[1]
raw = subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-v', 'quiet', '-i', src, '-ac', '1', '-ar', '48000', '-f', 's16le', '-'], capture_output=True).stdout
x = np.frombuffer(raw, np.int16).astype(np.float32) / 32768
a, b = int(12.40 * 48000), int(13.35 * 48000)
y = x[a:b].copy()
y -= y.mean()
y *= 10 ** (-2 / 20) / max(np.abs(y).max(), 1e-6)
n = int(0.15 * 48000); y[-n:] *= np.linspace(1, 0, n)
y[:240] *= np.linspace(0, 1, 240)
out = os.path.join(ROOT, 'Art', 'Sounds', 'Office', 'Knock.wav')
with wave.open(out, 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000); w.writeframes((y * 32767).astype(np.int16).tobytes())
print('wrote', out, '%.2f s' % (len(y) / 48000))
