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
import re, os
base = Path(__file__).resolve().parent.parent / 'Content' / 'Babylon'
meshes_dir = next((Path(d) / 'StaticMeshes' for d in (base / name.capitalize() / name, base / name.capitalize() / name.capitalize())
                   if (Path(d) / 'StaticMeshes').exists()), base / name.capitalize() / name / 'StaticMeshes')
by_node = not (meshes_dir / ('%s_mesh_0.uasset' % name)).exists()
# a third naming (owner's finished models from the phone chat, 2026-11-15): after the glTF MESH's own name ('Cube.001'
# -> Cube_001): each node then carries the imported file's name as "asset" (ABabylonModel uses it first)
have = {p.stem.lower(): p.stem for p in meshes_dir.glob('*.uasset')} if meshes_dir.exists() else {}
san = lambda t: re.sub(r'[^A-Za-z0-9_-]', '_', t)   # (Unreal keeps '-' in imported names)
by_meshname = 0
for nd, src_nd in zip(nodes, g['nodes']):
    if 'mesh' not in nd: continue
    if san(nd.get('name', '')).lower() in have: continue
    mn = san(g['meshes'][nd['mesh']].get('name', '')).lower()
    if mn in have: nd['asset'] = have[mn]; by_meshname += 1
if by_meshname: print('%d parts found under their mesh names' % by_meshname)
roots = g['scenes'][g.get('scene', 0)]['nodes']
out = Path(__file__).resolve().parent.parent / 'Data' / ('babylon-%s.json' % name)
# (--node-transforms: a model whose shared parts are one mesh copied by its nodes - the airport coach: placed by each node)
extra = {'nodeTransforms': True} if '--node-transforms' in sys.argv else {}
out.write_text(json.dumps({'source': src + ' from Tools/BabylonExport, the node tree only', 'assetNames': 'node' if by_node else 'mesh', 'roots': roots, 'nodes': nodes, **extra}, separators=(',', ':')), encoding='utf-8')
print('%s: %d nodes, %d with meshes' % (out, len(nodes), sum('mesh' in x for x in nodes)))
