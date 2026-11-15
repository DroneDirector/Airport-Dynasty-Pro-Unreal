# The size of every native terminal piece (/Game/Terminal/Meshes), for Tools/terminal_where.py: each piece's local
# bounding box (cm, Unreal axes) -> Data/terminal-piece-bounds.json.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/terminal_piece_bounds.py" -unattended -nullrhi
import json, os
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = {}
for p in unreal.EditorAssetLibrary.list_assets('/Game/Terminal/Meshes', recursive=False):
    a = unreal.load_asset(p)
    if not isinstance(a, unreal.StaticMesh): continue
    bb = a.get_bounding_box()
    out[a.get_name()] = [round(bb.min.x, 2), round(bb.min.y, 2), round(bb.min.z, 2), round(bb.max.x, 2), round(bb.max.y, 2), round(bb.max.z, 2)]
json.dump(out, open(os.path.join(ROOT, 'Data', 'terminal-piece-bounds.json'), 'w'))
unreal.log('ADP_BOUNDS %d pieces' % len(out))
