# Records ONLY the game window (never the desktop - owner, 2026-11-16) while a test runs: starts the game with the given
# switches, waits for a line in the log, records the window by its title with ffmpeg (gdigrab, from imageio-ffmpeg)
# until a second line appears or the game ends, with a time-out; the game is always closed at the end.
#   python Tools/record_window.py <out.mp4> "<start line>" "<stop line>" <game switches...>
#   (REC_W / REC_H: the window's size, 1600 x 900 unless set; REC_OUT=1920x1080 scales the video to exactly that)
# e.g. python Tools/record_window.py docs/Pictures/Jetway-Final/Jetway-Docking.mp4 "docking at real speed" "docked after" -UIShot -UIJetwayVideo
import sys, os, time, subprocess, csv, io
import imageio_ffmpeg

OUT, START, STOP, ARGS = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = r'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe'
LOG = os.path.join(ROOT, 'Saved', 'Logs', 'AirportDynastyPro.log')
LIMIT = 600

def log_has(line):
    try: return line in open(LOG, encoding='utf-8', errors='ignore').read()
    except OSError: return False

def window_title(pid):
    out = subprocess.run(['tasklist', '/v', '/fo', 'csv', '/fi', 'PID eq %d' % pid], capture_output=True, text=True).stdout
    rows = list(csv.reader(io.StringIO(out)))
    return rows[1][-1] if len(rows) > 1 else ''

if os.path.exists(LOG): os.remove(LOG)
RW, RH = os.environ.get('REC_W', '1600'), os.environ.get('REC_H', '900')
game = subprocess.Popen([EXE, os.path.join(ROOT, 'AirportDynastyPro.uproject'), '-game', '-windowed', '-ResX=' + RW, '-ResY=' + RH, '-ForceRes', '-WinX=' + os.environ.get('REC_X', '0'), '-WinY=' + os.environ.get('REC_Y', '30'), '-nosplash', '-NoLoadingScreen'] + ARGS)
t0 = time.time(); rec = None
try:
    while game.poll() is None and time.time() - t0 < LIMIT:
        if rec is None and log_has(START):
            title = window_title(game.pid)
            # (the game's picture area on screen, by its client rectangle: a DirectX window grabbed by title gives a still
            #  picture, so the screen area under it is recorded, the window brought to the front first)
            import ctypes, ctypes.wintypes as wt
            u = ctypes.windll.user32; u.SetProcessDPIAware()
            h = u.FindWindowW(None, title.strip()) or u.FindWindowW(None, title)
            u.SetForegroundWindow(h)
            rc = wt.RECT(); u.GetClientRect(h, ctypes.byref(rc)); pt = wt.POINT(0, 0); u.ClientToScreen(h, ctypes.byref(pt))
            w, hh = (rc.right - rc.left) // 2 * 2, (rc.bottom - rc.top) // 2 * 2
            print('recording window:', title, 'at', pt.x, pt.y, w, hh)
            rec = subprocess.Popen([imageio_ffmpeg.get_ffmpeg_exe(), '-y', '-f', 'gdigrab', '-framerate', '30', '-draw_mouse', '0',
                                    '-offset_x', str(pt.x), '-offset_y', str(pt.y), '-video_size', '%dx%d' % (w, hh), '-i', 'desktop',
                                    '-c:v', 'libx264', '-preset', 'medium', '-crf', '18', '-pix_fmt', 'yuv420p'] + (['-vf', 'scale=%s:flags=lanczos' % os.environ['REC_OUT'].replace('x', ':')] if os.environ.get('REC_OUT') else []) + [OUT], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if rec is not None and log_has(STOP): time.sleep(3); break
        time.sleep(0.5)
finally:
    if rec is not None:
        try: rec.communicate(b'q', timeout=30)
        except Exception: rec.kill()
    if game.poll() is None:
        time.sleep(8)
        if game.poll() is None: game.kill()
print('video:', OUT if rec is not None else 'none (the start line never came)', 'after %.0f s' % (time.time() - t0))
