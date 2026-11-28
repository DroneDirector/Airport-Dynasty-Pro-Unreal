# The airplane test's recorder (owner 2026-11-28: "make sure that the video recording is good quality"): the whole screen
# the game fills, at its own size (3840 x 1080 here, DPI aware), 60 fps, captured on the graphics card (ffmpeg ddagrab) and
# encoded there (h264_nvenc, constant quality 19, preset p6, up to 80 Mbit), no mouse; the game's OWN sound, recorded inside
# the game (PerfViews "sound record" with -RecordAudio: a wav on F:), put in at the end as AAC 256k, in step with the picture.
# It does not start or close the game (Tools/airplane_test.sh starts it through perf_views.sh); it watches the game's log:
#   starts on "SOUND RECORDING STARTS", stops on "SOUND RECORDING STOPS" (or when the game ends, or the limit).
# Never the desktop: while recording, the window in front must be the game's (UnrealEditor / AirportDynastyPro); if another
# window stays in front for 2 s the recording stops there, and the summary says so.
#   python Tools/record_test.py <out.mp4> [log file] [limit minutes]
import sys, os, time, re, subprocess, ctypes, ctypes.wintypes as wt, json
import imageio_ffmpeg

OUT = os.path.abspath(sys.argv[1])
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'Saved', 'Logs', 'Views.log')
LIMIT = float(sys.argv[3]) * 60 if len(sys.argv) > 3 else 40 * 60   # (the whole test is about 6 min; the start may take 15)
WAIT_START = 25 * 60
FF = imageio_ffmpeg.get_ffmpeg_exe()
SUM = os.path.splitext(OUT)[0] + '-recorder.txt'
lines = []
def say(*a):
    t = ' '.join(str(x) for x in a); print(t, flush=True); lines.append(time.strftime('%H:%M:%S ') + t)
    open(SUM, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')

u = ctypes.windll.user32
try: ctypes.windll.shcore.SetProcessDpiAwareness(2)   # (per-monitor aware: the real pixels, not the scaled ones)
except Exception: u.SetProcessDPIAware()

def front_is_game():
    h = u.GetForegroundWindow(); pid = wt.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(pid))
    k = ctypes.windll.kernel32.OpenProcess(0x1000, False, pid.value); name = ''
    if k:
        buf = ctypes.create_unicode_buffer(520); n = wt.DWORD(520)
        if ctypes.windll.kernel32.QueryFullProcessImageNameW(k, 0, buf, ctypes.byref(n)): name = os.path.basename(buf.value)
        ctypes.windll.kernel32.CloseHandle(k)
    return name.lower().startswith(('unrealeditor', 'airportdynastypro')), name, h

def game_running():
    out = subprocess.run(['tasklist', '/fo', 'csv', '/nh'], capture_output=True, text=True).stdout.lower()
    return 'unrealeditor' in out or 'airportdynastypro' in out

class Tail:
    def __init__(s, p): s.p, s.at = p, 0
    def new(s):
        try:
            with open(s.p, 'rb') as f:
                if os.path.getsize(s.p) < s.at: s.at = 0   # (a fresh log)
                f.seek(s.at); b = f.read(); s.at += len(b)
            return b.decode('utf-8', 'ignore')
        except OSError: return ''

def monitor_of(h):
    # (the screen the game is on, its real size; ddagrab's output index follows the adapters' order - the first is the main)
    class MI(ctypes.Structure): _fields_ = [('cb', wt.DWORD), ('rc', wt.RECT), ('work', wt.RECT), ('flags', wt.DWORD)]
    m = u.MonitorFromWindow(h, 2); mi = MI(); mi.cb = ctypes.sizeof(MI); u.GetMonitorInfoW(m, ctypes.byref(mi))
    return mi.rc.right - mi.rc.left, mi.rc.bottom - mi.rc.top, bool(mi.flags & 1)

def refresh_hz():
    # (the screen's refresh: captured at it, every frame the screen really showed is caught - at 60 against a 143 Hz
    #  screen, ddagrab's own clock repeated or skipped about 4 frames a second, the judder in the videos)
    try:
        dc = u.GetDC(0); hz = ctypes.windll.gdi32.GetDeviceCaps(dc, 116); u.ReleaseDC(0, dc)   # (116 = VREFRESH)
        return hz if 60 <= hz <= 240 else 60
    except Exception: return 60

def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    enc = subprocess.run([FF, '-hide_banner', '-encoders'], capture_output=True, text=True).stdout
    fil = subprocess.run([FF, '-hide_banner', '-filters'], capture_output=True, text=True).stdout
    if 'h264_nvenc' not in enc or 'ddagrab' not in fil: say('FAILED: this ffmpeg has no ddagrab / h264_nvenc:', FF); return 2
    tail = Tail(LOG); t0 = time.time(); a_ms = None; wav = None
    say('waiting for "SOUND RECORDING STARTS" in', LOG)
    while time.time() - t0 < WAIT_START:
        m = re.search(r'SOUND RECORDING STARTS (\S+) at unix ms (\d+)', tail.new())
        if m: name, a_ms = m.group(1), int(m.group(2)); break
        if time.time() - t0 > 60 and not game_running(): say('FAILED: the game ended before the recording began'); return 3
        time.sleep(0.2)
    if a_ms is None: say('FAILED: the start line never came'); return 3
    ok, exe, h = front_is_game()
    if not ok:
        time.sleep(1.0); ok, exe, h = front_is_game()
        if not ok: say('FAILED: the window in front is', exe or '?', '- not the game: nothing recorded (never the desktop)'); return 4
    w, hh, main_screen = monitor_of(h)
    idx = os.environ.get('REC_OUTPUT', '0' if main_screen else '1')
    raw = os.path.splitext(OUT)[0] + '-picture.mp4'
    hz = refresh_hz()
    cmd = [FF, '-y', '-hide_banner', '-loglevel', 'info', '-f', 'lavfi', '-i', 'ddagrab=output_idx=%s:framerate=%d:draw_mouse=0' % (idx, hz),
           '-c:v', 'h264_nvenc', '-preset', 'p6', '-tune', 'hq', '-rc', 'vbr', '-cq', '19', '-b:v', '60M', '-maxrate', '80M',
           '-bufsize', '160M', '-profile:v', 'high', '-g', '240', '-bf', '2', '-r', str(hz), '-movflags', '+faststart', raw]
    say('screen %dx%d at %d Hz (output %s), game %s; ffmpeg:' % (w, hh, hz, idx, exe), ' '.join(cmd[4:]))
    rec = subprocess.Popen(cmd, stdin=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
    v_ms = None; err = []
    import threading
    def read_err():
        nonlocal v_ms
        for ln in rec.stderr:
            if v_ms is None and ln.startswith('Output #0'): v_ms = int(time.time() * 1000)   # (written once the first frame is in)
            if 'frame=' not in ln: err.append(ln.rstrip())
            else: err[-1:] = err[-1:] if err and 'frame=' not in err[-1] else err[-1:]; last[0] = ln.strip()
    last = ['']
    th = threading.Thread(target=read_err, daemon=True); th.start()
    r0 = time.time(); away = 0.0; why = 'the stop line'
    while True:
        txt = tail.new()
        m = re.search(r'SOUND RECORDING STOPS (\S+) at unix ms (\d+) - written to (.+?\.wav)', txt)
        if m: wav = m.group(3).strip(); break
        if rec.poll() is not None: why = 'ffmpeg ended by itself'; break
        if time.time() - r0 > LIMIT: why = 'the limit (%.0f min)' % (LIMIT / 60); break
        if not game_running(): why = 'the game ended'; break
        ok, exe, _ = front_is_game()
        away = 0.0 if ok else away + 0.25
        if away >= 2.0: why = 'another window (%s) came in front - stopped, never the desktop' % (exe or '?'); break
        time.sleep(0.25)
    try: rec.communicate('q', timeout=30)
    except Exception: rec.kill()
    say('picture stopped by', why, 'after %.0f s;' % (time.time() - r0), last[0][:160])
    for e in err[-6:]:
        if 'rror' in e or 'arn' in e: say('ffmpeg:', e[:200])
    if not wav:
        # (the wav's name all the same, if the game ended first: the game stops the recording at its end too)
        wav = os.path.join(os.environ.get('REC_AUDIO_DIR', 'F:/ADP-Export/airplane-test'), name + '.wav')
    t1 = time.time()
    while time.time() - t1 < 60:   # (written in the background after the stop: wait until its size holds)
        if os.path.exists(wav):
            s1 = os.path.getsize(wav); time.sleep(1.5)
            if os.path.getsize(wav) == s1 and s1 > 44: break
        time.sleep(0.5)
    if not os.path.exists(wav): say('NO SOUND: the wav', wav, 'never came; the picture alone is', raw); os.replace(raw, OUT); return check(OUT, None)
    # (in step: the picture's first frame came v_ms, the sound began a_ms; the sound's earlier part is cut off)
    off = max(0.0, ((v_ms or a_ms) - a_ms) / 1000.0)
    say('sound', wav, '%.1f MB; picture began %.3f s after it' % (os.path.getsize(wav) / 1e6, off))
    # (the repeats out: the screen's refreshes that showed the same game frame are dropped, the game's own frames are laid
    #  evenly at their real average rate and the file made 60 fps - no repeated or skipped frame, the sound stays in step)
    DEC = 'mpdecimate=hi=256:lo=64:frac=0.1'
    n = subprocess.run([FF, '-hide_banner', '-i', raw, '-vf', DEC, '-an', '-f', 'null', '-'], capture_output=True, text=True).stderr
    fr = re.findall(r'frame=\s*(\d+)', n); uniq = int(fr[-1]) if fr else 0
    d = re.search(r'Duration: (\d+):(\d+):([\d.]+)', n); dur = int(d.group(1)) * 3600 + int(d.group(2)) * 60 + float(d.group(3)) if d else 0
    rate = uniq / dur if dur > 1 and uniq > 0 else 0
    vf = ('%s,setpts=N/%.5f/TB,fps=60' % (DEC, rate)) if 20 <= rate <= 150 else 'fps=60'
    say('game frames: %d unique in %.1f s = %.2f a second; picture filter %s' % (uniq, dur, rate, vf))
    mux = [FF, '-y', '-hide_banner', '-loglevel', 'error', '-i', raw, '-ss', '%.3f' % off, '-i', wav, '-map', '0:v:0', '-map', '1:a:0',
           '-vf', vf, '-c:v', 'h264_nvenc', '-preset', 'p6', '-tune', 'hq', '-rc', 'vbr', '-cq', '19', '-b:v', '60M', '-maxrate', '80M',
           '-bufsize', '160M', '-profile:v', 'high', '-g', '120', '-bf', '2', '-c:a', 'aac', '-b:a', '256k', '-shortest', '-movflags', '+faststart', OUT]
    r = subprocess.run(mux, capture_output=True, text=True)
    if r.returncode != 0: say('FAILED to put the sound in:', r.stderr[-300:]); os.replace(raw, OUT); return check(OUT, wav)
    os.remove(raw)
    return check(OUT, wav)

def check(path, wav):
    # (the file looked at: length, size, frame rate, frames counted, dropped ones; three frames to PNG for a look)
    r = subprocess.run([FF, '-hide_banner', '-i', path], capture_output=True, text=True).stderr
    d = re.search(r'Duration: (\d+):(\d+):([\d.]+)', r); v = re.search(r'Video: ([^\n]+)', r); a = re.search(r'Audio: ([^\n]+)', r)
    sec = int(d.group(1)) * 3600 + int(d.group(2)) * 60 + float(d.group(3)) if d else 0
    n = subprocess.run([FF, '-hide_banner', '-i', path, '-map', '0:v:0', '-c', 'copy', '-f', 'null', '-'], capture_output=True, text=True).stderr
    fr = re.findall(r'frame=\s*(\d+)', n); frames = int(fr[-1]) if fr else 0
    say('FILE', path, '%.0f MB, %.1f s, %d frames (%.2f fps average; 60 expected)' % (os.path.getsize(path) / 1e6, sec, frames, frames / sec if sec else 0))
    say('video:', v.group(1)[:160] if v else 'none'); say('audio:', a.group(1)[:120] if a else 'NONE')
    base = os.path.splitext(path)[0]
    for i, f in enumerate((0.1, 0.5, 0.9)):
        subprocess.run([FF, '-y', '-hide_banner', '-loglevel', 'error', '-ss', '%.1f' % (sec * f), '-i', path, '-frames:v', '1', '%s-frame%d.png' % (base, i + 1)])
    say('three frames:', base + '-frame1..3.png (view them before sending)')
    return 0 if a else 5

if __name__ == '__main__':
    try: sys.exit(main())
    except Exception as e: say('FAILED:', repr(e)); sys.exit(9)
