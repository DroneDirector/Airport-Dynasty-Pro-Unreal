# The charging inlet's parts (Art/ApronBus/apronbus-inlet.glb, from Art/ApronBus/add_inlet.py) put into Art/ApronBus/apronbus.glb,
# Data/babylon-apronbus.json and Data/apronbus-riders.json, so that a later full import keeps them. Nothing else of the bus changes:
# the skirt and stripe get a new shape in their own places, the flap too, the inlet and two points come at the end. (277 E3a)
import json, os, shutil, struct
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def load(p):
    b = open(p, 'rb').read(); n = struct.unpack('<I', b[12:16])[0]
    return json.loads(b[20:20 + n]), b[28 + n:28 + n + struct.unpack('<I', b[20 + n:24 + n])[0]]


P = os.path.join(R, 'Art', 'ApronBus', 'apronbus.glb'); g, raw = load(P); h, add = load(os.path.join(R, 'Art', 'ApronBus', 'apronbus-inlet.glb'))
mats = {m['name']: i for i, m in enumerate(g['materials'])}; raw = bytearray(raw[:g['buffers'][0]['byteLength']])
while len(raw) % 4: raw.append(0)
done = {}


def acc(i):
    """An accessor of the small file copied to the big one."""
    if i in done: return done[i]
    a = dict(h['accessors'][i]); v = dict(h['bufferViews'][a['bufferView']]); d = add[v.get('byteOffset', 0):v.get('byteOffset', 0) + v['byteLength']]
    v.update(buffer=0, byteOffset=len(raw)); raw.extend(d)
    while len(raw) % 4: raw.append(0)
    g['bufferViews'].append(v); a['bufferView'] = len(g['bufferViews']) - 1; g['accessors'].append(a); done[i] = len(g['accessors']) - 1
    return done[i]


J = os.path.join(R, 'Data', 'babylon-apronbus.json'); lay = json.load(open(J, encoding='utf-8')); RP = os.path.join(R, 'Data', 'apronbus-riders.json'); rid = json.load(open(RP, encoding='utf-8'))
for nd in h['nodes']:
    name = nd['name']; old = next((x for x in g['nodes'] if x.get('name') == name), None)
    if 'mesh' not in nd:
        t = nd.get('translation', [0, 0, 0]); rid['points'][name] = [round(t[0], 4), round(-t[2], 4), round(t[1], 4)]
        assert old is None; g['nodes'].append(nd); lay['nodes'].append(nd); continue
    assert not any(k in nd for k in ('translation', 'rotation', 'scale')), name
    pr = [{'attributes': {k: acc(v) for k, v in p['attributes'].items()}, 'indices': acc(p['indices']), 'material': mats[h['materials'][p['material']]['name']]} for p in h['meshes'][nd['mesh']]['primitives']]
    if old is not None: g['meshes'][old['mesh']]['primitives'] = pr
    else:
        g['meshes'].append({'name': name, 'primitives': pr}); g['nodes'].append({'mesh': len(g['meshes']) - 1, 'name': name}); lay['nodes'].append({'mesh': len(g['meshes']) - 1, 'name': name})
for x, root in ((g['nodes'], g['scenes'][0]['nodes']), (lay['nodes'], lay['roots'])):
    for i in range(862, len(x)):
        if i not in root: root.append(i)
assert len(lay['nodes']) == len(g['nodes'])
g['buffers'][0]['byteLength'] = len(raw); js = json.dumps(g, separators=(',', ':')).encode('utf-8'); js += b' ' * (-len(js) % 4)
open(P, 'wb').write(struct.pack('<4sII', b'glTF', 2, 28 + len(js) + len(raw)) + struct.pack('<I4s', len(js), b'JSON') + js + struct.pack('<I4s', len(raw), b'BIN\0') + raw)
open(J, 'w', encoding='utf-8', newline='\n').write(json.dumps(lay, separators=(',', ':')))
open(RP, 'w', encoding='utf-8', newline='\n').write(json.dumps(rid, indent=1))
for f in (J, RP): shutil.copy(f, os.path.join(R, 'Content', 'Data', os.path.basename(f)))
print(len(g['nodes']), 'nodes', {k: rid['points'][k] for k in ('PIV_ChargeFlap', 'SOCK_Inlet')})
