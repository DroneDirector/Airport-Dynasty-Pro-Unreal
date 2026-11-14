# The shaped land along the roads (Saved/land-sections.bin, written by the game run with -DumpLand: ALandCorridor) into
# one glb of its blocks, LC_<block>, for the editor to import as Nanite meshes (Tools/set_land_corridor.py).
#   python Tools/land_nanite.py      (Tools/make_land_nanite.sh runs it all)
import json, struct
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parent.parent
src = (ROOT / 'Saved' / 'land-sections.bin').read_bytes()
off = 0; secs = []
while off < len(src):
    blk, nv, ni = struct.unpack_from('<iii', src, off); off += 12
    V = np.frombuffer(src, '<f4', nv * 3, off).reshape(-1, 3); off += nv * 12
    N = np.frombuffer(src, '<f4', nv * 3, off).reshape(-1, 3); off += nv * 12
    T = np.frombuffer(src, '<f4', nv * 2, off).reshape(-1, 2); off += nv * 8
    I = np.frombuffer(src, '<i4', ni, off); off += ni * 4
    secs.append((blk, V, N, T, I))
# Unreal (x, y, z) cm -> glTF metres: as the importer reads a glb back (Unreal x = gx, y = gz, z = gy)
bin_ = bytearray(); views = []; accs = []; meshes = []; nodes = []
def add(arr, target, comp, typ, minmax=False):
    while len(bin_) % 4: bin_.append(0)
    b = arr.tobytes(); views.append({'buffer': 0, 'byteOffset': len(bin_), 'byteLength': len(b), **({'target': target} if target else {})}); bin_.extend(b)
    a = {'bufferView': len(views) - 1, 'componentType': comp, 'count': len(arr), 'type': typ}
    if minmax: a['min'] = arr.min(0).tolist(); a['max'] = arr.max(0).tolist()
    accs.append(a); return len(accs) - 1
for blk, V, N, T, I in secs:
    P = np.stack([V[:, 0], V[:, 2], V[:, 1]], 1).astype('<f4') / 100.0
    Nn = np.stack([N[:, 0], N[:, 2], N[:, 1]], 1).astype('<f4')
    ip = add(P, 34962, 5126, 'VEC3', True); inn = add(Nn, 34962, 5126, 'VEC3'); it = add(T.astype('<f4'), 34962, 5126, 'VEC2')
    ii = add(I.astype('<u4'), 34963, 5125, 'SCALAR')
    name = 'LC_%03d' % blk
    meshes.append({'name': name, 'primitives': [{'attributes': {'POSITION': ip, 'NORMAL': inn, 'TEXCOORD_0': it}, 'indices': ii, 'material': 0}]})
    nodes.append({'name': name, 'mesh': len(meshes) - 1})
g = {'asset': {'version': '2.0', 'generator': 'Tools/land_nanite.py'}, 'scene': 0, 'scenes': [{'nodes': list(range(len(nodes)))}], 'nodes': nodes,
     'meshes': meshes, 'materials': [{'name': 'M_LandCorridor', 'doubleSided': True}], 'accessors': accs, 'bufferViews': views, 'buffers': [{'byteLength': len(bin_)}]}
while len(bin_) % 4: bin_.append(0)
j = json.dumps(g).encode(); j += b' ' * ((4 - len(j) % 4) % 4)
out = ROOT / 'Art' / 'Land' / 'land_corridor.glb'; out.parent.mkdir(parents=True, exist_ok=True)
with open(out, 'wb') as f:
    f.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(bin_)))
    f.write(struct.pack('<II', len(j), 0x4E4F534A)); f.write(j)
    f.write(struct.pack('<II', len(bin_), 0x004E4942)); f.write(bin_)
print('land_nanite: %d blocks, %d triangles -> %s' % (len(secs), sum(len(s[4]) for s in secs) // 3, out))
