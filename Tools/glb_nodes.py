# The node tree of a model exported from the Babylon edition (glTF binary): every object's name, parent-child links,
# mesh and transform, written to Data/babylon-<name>.json for ABabylonModel, which places the imported meshes from it.
# Nothing else is read or changed.
#   python Tools/glb_nodes.py F:/ADP-Export/lounge.glb lounge
import json
import struct
import sys
from pathlib import Path

src, name = sys.argv[1], sys.argv[2].lower()
b = Path(src).read_bytes()
n = struct.unpack('<I', b[12:16])[0]
g = json.loads(b[20:20 + n])
keep = ('name', 'children', 'mesh', 'translation', 'rotation', 'scale', 'matrix')
nodes = [{k: v for k, v in node.items() if k in keep} for node in g['nodes']]
# how Unreal's importer named the meshes: after the glTF mesh's number (<name>_mesh_<n>, the terminal) or after their
# object (the lounge); read from the imported files themselves
meshes_dir = Path(__file__).resolve().parent.parent / 'Content' / 'Babylon' / name.capitalize() / name / 'StaticMeshes'
by_node = not (meshes_dir / ('%s_mesh_0.uasset' % name)).exists()
roots = g['scenes'][g.get('scene', 0)]['nodes']
out = Path(__file__).resolve().parent.parent / 'Data' / ('babylon-%s.json' % name)
out.write_text(json.dumps({'source': src + ' from Tools/BabylonExport, the node tree only', 'assetNames': 'node' if by_node else 'mesh', 'roots': roots, 'nodes': nodes}, separators=(',', ':')), encoding='utf-8')
print('%s: %d nodes, %d with meshes' % (out, len(nodes), sum('mesh' in x for x in nodes)))
