# Every vertex of some terminal layout nodes' pieces, in the plan's metres, to a JSON file {node: [[x, y, z], ...]} -
# e.g. the wave roof (node 12892) for Art/TerminalAnnex/make_annex_west.py's wall up to the roof's underside.
#   python Tools/dump_piece_points.py <out.json> <node> [<node> ...]
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terminal_where as TW
out = os.path.abspath(sys.argv[1]).replace(os.sep, '/'); nodes = [int(a) for a in sys.argv[2:]]
A = TW.piece_to_plan(nodes)
cur = json.load(open(TW.D('terminal-pieces.json'))); L = json.load(open(TW.D('terminal-layout.json')))
job = {str(n): {'piece': cur.get('terminal_mesh_%d' % L['nodes'][n]['mesh'], [A[n][0]])[0], 'm': A[n][1]} for n in nodes}
jf = os.path.join(TW.ROOT, 'Saved', 'piece-points-job.json').replace(os.sep, '/'); json.dump(job, open(jf, 'w'))
ue = os.path.join(TW.ROOT, 'Saved', 'piece_points_ue.py').replace(os.sep, '/')
open(ue, 'w').write('''import json, unreal
J = json.load(open(r"%s")); R = {}
for n, j in J.items():
    sm = unreal.load_asset('/Game/Terminal/Meshes/' + j['piece']); d = sm.get_static_mesh_description(0); M = j['m']; pts = []
    for v in range(d.get_vertex_count() + 64):
        vid = unreal.VertexID(v)
        if not d.is_vertex_valid(vid): continue
        q = d.get_vertex_position(vid)
        pts.append([round(sum(M[i][k] * c for k, c in enumerate((q.x, q.y, q.z))) + M[i][3], 4) for i in range(3)])
    R[n] = pts
json.dump(R, open(r"%s", "w")); unreal.log("ADP_POINTS %%d nodes" %% len(R))
''' % (jf, out))
ED = r'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
subprocess.run([ED, os.path.join(TW.ROOT, 'AirportDynastyPro.uproject'), '-ExecutePythonScript=' + ue, '-unattended', '-nullrhi', '-nosplash'], capture_output=True)
print('points:', out, os.path.exists(out))
