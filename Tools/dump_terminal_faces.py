# The flat faces (floors, ceilings, slab tops) of every merged STRUCTURE piece the terminal's layout places, in each
# piece's own Unreal cm, for Tools/check_floors.py -> Saved/terminal-faces-all.json.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/dump_terminal_faces.py" -unattended -nullrhi
import json, os
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
L = json.load(open(os.path.join(ROOT, 'Data', 'terminal-layout.json'))); P = json.load(open(os.path.join(ROOT, 'Data', 'terminal-pieces.json')))
names = set()
for n in L['nodes']:
    if '/STRUCTURE-' in n['name'] and 'mesh' in n:
        k = 'terminal_mesh_%d' % n['mesh']
        if k in P: names.add(P[k][0])
out = {}
for nm in sorted(names):
    sm = unreal.load_asset('/Game/Terminal/Meshes/' + nm)
    if not sm: continue
    d = sm.get_static_mesh_description(0); faces = []
    for p in range(d.get_polygon_count() + 64):
        pid = unreal.PolygonID(p)
        if not d.is_polygon_valid(pid): continue
        vs = [d.get_vertex_position(v) for v in d.get_polygon_vertices(pid)]
        if not vs or max(q.z for q in vs) - min(q.z for q in vs) > 0.5: continue
        faces.append([[round(q.x, 2), round(q.y, 2), round(q.z, 2)] for q in vs] + [''])
    out[nm] = faces
json.dump(out, open(os.path.join(ROOT, 'Saved', 'terminal-faces-all.json'), 'w'))
unreal.log('ADP_FACES %d pieces' % len(out))
