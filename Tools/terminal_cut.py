# Cut boxes out of the terminal's pieces (owner's floor-2 changes, 2026-11-15: openings in merged walls, rooms taken
# out): Data/terminal-cuts.json lists {node: <layout node index or name path end>, boxes: [[x0,y0,z0,x1,y1,z1], ...]}
# in the plan's metres. Each such node's piece is copied with the boxes' faces taken out (Tools/terminal_cut_ue.py),
# and Data/terminal-pieces.json then points that node's mesh at the cut copy. Run again any time: it always cuts from
# the original piece.   python Tools/terminal_cut.py
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terminal_where as TW
ROOT = TW.ROOT
cuts = json.load(open(TW.D('terminal-cuts.json')))
P = json.load(open(TW.D('terminal-pieces.json')))
orig = json.load(open(TW.D('terminal-pieces-original.json'))) if os.path.exists(TW.D('terminal-pieces-original.json')) else P
if not os.path.exists(TW.D('terminal-pieces-original.json')): json.dump(P, open(TW.D('terminal-pieces-original.json'), 'w'))
L = json.load(open(TW.D('terminal-layout.json'))); N = L['nodes']
for i, n in enumerate(N):
    if 'mesh' in n and 'terminal_mesh_%d' % n['mesh'] in orig: P['terminal_mesh_%d' % n['mesh']] = orig['terminal_mesh_%d' % n['mesh']]
json.dump(P, open(TW.D('terminal-pieces.json'), 'w'))      # (the originals back first, so the matrices are the originals')
job = []
idx = [c['node'] for c in cuts]
A = TW.piece_to_plan(idx)
for c in cuts:
    piece, M = A[c['node']]
    job.append({'node': c['node'], 'piece': piece, 'new': piece + '_cut', 'm': M, 'boxes': c['boxes']})
jf = os.path.join(ROOT, 'Saved', 'terminal-cut-job.json'); json.dump(job, open(jf, 'w'))
ED = r'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
SCRIPT = os.path.join(ROOT, 'Tools', 'terminal_cut_ue.py').replace(os.sep, '/'); JOBF = jf.replace(os.sep, '/')
subprocess.run([ED, os.path.join(ROOT, 'AirportDynastyPro.uproject'), '-ExecutePythonScript=%s %s' % (SCRIPT, JOBF), '-unattended', '-nullrhi', '-nosplash'], capture_output=True)
log = open(os.path.join(ROOT, 'Saved', 'Logs', 'AirportDynastyPro.log'), encoding='utf-8', errors='ignore').read()
print('\n'.join(l[l.find('ADP_CUT'):] for l in log.splitlines() if 'ADP_CUT' in l or 'LogPython: Error' in l))
if 'ADP_CUT done' in log:
    for j in job: P['terminal_mesh_%d' % N[j['node']]['mesh']][0] = j['new']
    json.dump(P, open(TW.D('terminal-pieces.json'), 'w'))
    print('pieces map updated: %d cut pieces' % len(job))
