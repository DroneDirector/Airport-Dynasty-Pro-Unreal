# A glTF binary from Blender: its meshes' own names taken out, so Unreal's importer names them <file>_mesh_<n> as it
# does the Babylon edition's (whose meshes have none) and ABabylonModel places them the same way (by mesh number, each
# where its node puts it). The objects' (nodes') names stay. Nothing else changes.
#   python Tools/glb_strip_mesh_names.py Art/Monitor/monitor169.glb
import json, struct, sys
from pathlib import Path

for src in sys.argv[1:]:
    b = Path(src).read_bytes()
    n = struct.unpack('<I', b[12:16])[0]
    g = json.loads(b[20:20 + n])
    rest = b[20 + n:]
    for m in g.get('meshes', []):
        m.pop('name', None)
    j = json.dumps(g, separators=(',', ':')).encode('utf-8')
    j += b' ' * ((4 - len(j) % 4) % 4)
    out = b[:12] + struct.pack('<I', len(j)) + b'JSON' + j + rest
    out = out[:8] + struct.pack('<I', len(out)) + out[12:]
    Path(src).write_bytes(out)
    print('%s: %d meshes unnamed' % (src, len(g.get('meshes', []))))
