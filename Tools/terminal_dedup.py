# The true-Unreal terminal (to-do 42), part 2: which of the terminal's 2739 imported meshes are the same piece.
# The Babylon export saved every copy of a piece (window bays, columns, seats, lamps...) as a mesh of its own, so Unreal
# couldn't draw them as one instanced mesh. Every mesh's geometry is read (positions, UVs, triangles) and compared
# exactly: the same shape with the same material is one piece. A copy whose geometry is the same but moved (the export
# baked its place into the vertices) counts too: its offset is kept, so it lands exactly where it was.
# Result: Data/terminal-pieces.json {mesh: [piece, offset x, y, z]} (Unreal units, the mesh's own space).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/terminal_dedup.py" -unattended -nullrhi
import unreal, json, os, hashlib
eal = unreal.EditorAssetLibrary
PML = unreal.ProceduralMeshLibrary
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = '/Game/Babylon/Terminal/terminal/StaticMeshes'
def log(m): unreal.log('ADP_DEDUP ' + m)
groups = {}            # signature -> canonical mesh name
out = {}
n = 0
for p in sorted(eal.list_assets(BASE, recursive=False, include_folder=False), key=lambda s: int(s.split('_')[-1].split('.')[0]) if s.split('_')[-1].split('.')[0].isdigit() else 0):
    s = unreal.load_asset(p)
    if not isinstance(s, unreal.StaticMesh): continue
    name = s.get_name()
    mat = s.get_material(0); mat = mat.get_name() if mat else ''
    h = hashlib.sha1(mat.encode())
    mn = None
    secs = s.get_num_sections(0)
    data = []
    for k in range(secs):
        v, tri, nrm, uv, tan = PML.get_section_from_static_mesh(s, 0, k)
        data.append((v, tri, uv))
        for q in v:
            if mn is None: mn = [q.x, q.y, q.z]
            else: mn = [min(mn[0], q.x), min(mn[1], q.y), min(mn[2], q.z)]
    if mn is None: continue
    for v, tri, uv in data:
        h.update(b'S')
        # positions relative to the piece's own corner, to a tenth of a millimetre
        h.update(','.join('%d,%d,%d' % (round((q.x - mn[0]) * 100), round((q.y - mn[1]) * 100), round((q.z - mn[2]) * 100)) for q in v).encode())
        h.update(','.join(str(i) for i in tri).encode())
        h.update(','.join('%d,%d' % (round(t.x * 10000), round(t.y * 10000)) for t in uv).encode())
    sig = h.hexdigest()
    if sig not in groups: groups[sig] = (name, mn)
    can, cmn = groups[sig]
    out[name] = [can, round(mn[0] - cmn[0], 3), round(mn[1] - cmn[1], 3), round(mn[2] - cmn[2], 3)]
    n += 1
    if n % 300 == 0: log('%d meshes read, %d pieces' % (n, len(groups)))
json.dump(out, open(os.path.join(ROOT, 'Data', 'terminal-pieces.json'), 'w'), separators=(',', ':'))
moved = sum(1 for k, v in out.items() if v[0] != k and (abs(v[1]) + abs(v[2]) + abs(v[3])) > 0.01)
log('%d meshes, %d pieces (%d copies, %d of them moved)' % (n, len(groups), n - len(groups), moved))
unreal.SystemLibrary.quit_editor()
