# Which faces of a terminal piece lie in a box (plan metres) - to see how a merged wall is split before cutting it.
#   python Tools/terminal_faces.py <node> x0 y0 z0 x1 y1 z1
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terminal_where as TW
node = int(sys.argv[1]); box = [float(v) for v in sys.argv[2:8]]
piece, M = TW.piece_to_plan([node])[node]
cur = json.load(open(TW.D('terminal-pieces.json')))
L = json.load(open(TW.D('terminal-layout.json'))); piece_now = cur.get('terminal_mesh_%d' % L['nodes'][node]['mesh'], [piece])[0]
job = os.path.join(TW.ROOT, 'Saved', 'terminal-faces-job.json').replace(os.sep, '/')
json.dump({'piece': piece_now, 'm': M, 'box': box}, open(job, 'w'))
ue = os.path.join(TW.ROOT, 'Saved', 'terminal_faces_ue.py').replace(os.sep, '/')
open(ue, 'w').write('''import json, unreal
J = json.load(open(r"%s"))
sm = unreal.load_asset('/Game/Terminal/Meshes/' + J['piece']); d = sm.get_static_mesh_description(0); M = J['m']; b = J['box']
for p in range(d.get_polygon_count() + 64):
    pid = unreal.PolygonID(p)
    if not d.is_polygon_valid(pid): continue
    pts = []
    for v in d.get_polygon_vertices(pid):
        q = d.get_vertex_position(v); pts.append([sum(M[i][k] * c for k, c in enumerate((q.x, q.y, q.z))) + M[i][3] for i in range(3)])
    c = [sum(x[i] for x in pts) / len(pts) for i in range(3)]
    if b[0] <= c[0] <= b[3] and b[1] <= c[1] <= b[4] and b[2] <= c[2] <= b[5]:
        unreal.log('ADP_FACE %%d x %%.2f..%%.2f y %%.2f..%%.2f z %%.2f..%%.2f' %% (p, min(x[0] for x in pts), max(x[0] for x in pts), min(x[1] for x in pts), max(x[1] for x in pts), min(x[2] for x in pts), max(x[2] for x in pts)))
''' % job)
ED = r'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
subprocess.run([ED, os.path.join(TW.ROOT, 'AirportDynastyPro.uproject'), '-ExecutePythonScript=' + ue, '-unattended', '-nullrhi', '-nosplash'], capture_output=True)
for l in open(os.path.join(TW.ROOT, 'Saved', 'Logs', 'AirportDynastyPro.log'), encoding='utf-8', errors='ignore'):
    if 'ADP_FACE' in l or 'LogPython: Error' in l: print(l[l.find('ADP_FACE') if 'ADP_FACE' in l else 0:].rstrip())
