# The jetways' finishes (owner, 2026-11-17, to-do 84a fixes 3, 4, 6, 7, 9, 12), on the export after
# Tools/jetway_realorder.py (in place, names and mesh numbers kept, only faces re-assigned and textures replaced):
#  - a light grey metal-plate texture for the inside walls (jwWall: light panels, fine seams, a soft brushed grain) -
#    the old one was nearly a flat colour, so the rotunda's walls looked plain white;
#  - every inside wall face still in the darker brushed metal (jwAluB) or plain white (jwPanel) - the cab's pillars and
#    frames, the tunnels' posts, the rotunda's sleeve - to that light grey plate (inside only; the outside keeps its own);
#  - the tunnels' outer wall faces in the X-frame's painted metal (jwTruss);
#  - the seals (the joints' bellows and the cab's canopy, jwFolds): dark grey coated fabric, slightly worn;
#  - the rotunda floor: the wheelchair sign and the yellow-black stripe across it gone (the carpet under them shows).
#   python Tools/jetway_finish.py [glb]      (default F:/ADP-Export/jetways.glb, rewritten in place)
import json, struct, sys, io, math
import numpy as np
from PIL import Image, ImageFilter

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/ADP-Export/jetways.glb'
b = open(PATH, 'rb').read()
jl = struct.unpack('<I', b[12:16])[0]; g = json.loads(b[20:20 + jl])
BIN = bytearray(b[20 + jl + 8: 20 + jl + 8 + struct.unpack('<I', b[20 + jl:24 + jl])[0]])
N = g['nodes']; par = {}
for i, nd in enumerate(N):
    for c in nd.get('children', []): par[c] = i
idx = {nd.get('name'): i for i, nd in enumerate(N)}
MAT = {m.get('name'): i for i, m in enumerate(g['materials'])}
rnd = np.random.default_rng(17)

def acc_read(ai):
    a = g['accessors'][ai]; bv = g['bufferViews'][a['bufferView']]
    off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]
    return np.frombuffer(BIN, dt, a['count'] * nc, off).reshape(a['count'], nc).copy()

def acc_new(arr, ctype, atype, target=None):
    global BIN
    while len(BIN) % 4: BIN.append(0)
    off = len(BIN); BIN += arr.tobytes()
    bv = {'buffer': 0, 'byteOffset': off, 'byteLength': arr.nbytes}
    if target: bv['target'] = target
    g['bufferViews'].append(bv)
    g['accessors'].append({'bufferView': len(g['bufferViews']) - 1, 'componentType': ctype, 'count': int(arr.shape[0]), 'type': atype})
    return len(g['accessors']) - 1

def trs(nd):
    M = np.eye(4)
    if 'matrix' in nd: return np.array(nd['matrix'], float).reshape(4, 4).T
    s = nd.get('scale', [1, 1, 1]); x, y, z, w = nd.get('rotation', [0, 0, 0, 1]); t = nd.get('translation', [0, 0, 0])
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M[:3, :3] = R * np.array(s)[None, :]; M[:3, 3] = t
    return M

def world(i):
    M = np.eye(4)
    while i is not None:
        M = trs(N[i]) @ M; i = par.get(i)
    return M

def descendants(i):
    out = []; todo = [i]
    while todo:
        k = todo.pop(); out.append(k); todo += N[k].get('children', [])
    return out

# ------------------------------------------------------------------------------------------------ the textures
def tile_noise(n, scale, seed):
    r = np.random.default_rng(seed).random((n // scale, n // scale))
    im = Image.fromarray((r * 255).astype(np.uint8)).resize((n, n), Image.BICUBIC)
    return np.asarray(im, np.float32) / 255.0

def plate_texture(n=1024):
    """Light grey metal plate: 1 m tile, plates 1.0 x 0.5 m, 2 px seams, a soft horizontal brushed grain, screws."""
    base = np.full((n, n, 3), (201, 204, 207), np.float32)
    grain = np.asarray(Image.fromarray((rnd.random((n, n)) * 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(1)).resize((n, n)), np.float32) / 255.0
    streak = np.repeat(rnd.random((n, 1)), n, 1)          # brushed: rows of slightly different lightness
    streak = np.asarray(Image.fromarray((streak * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
    big = tile_noise(n, 128, 3)
    v = (grain - 0.5) * 3.0 + (streak - 0.5) * 5.0 + (big - 0.5) * 6.0
    base += v[..., None]
    # the seams: every 0.5 m down, every 1.0 m across (light edge above, dark line under: a slight relief)
    for y in (0, n // 2):
        base[(y - 2) % n: (y - 2) % n + 1] -= 34; base[y % n] -= 46; base[(y + 1) % n] += 10
    base[:, 0] -= 46; base[:, n - 1] -= 30; base[:, 1] += 8
    # screws at the plate corners, 3 cm in
    for (cy, cx) in ((30, 30), (30, n - 30), (n // 2 - 30, 30), (n // 2 - 30, n - 30), (n // 2 + 30, 30), (n // 2 + 30, n - 30), (n - 30, 30), (n - 30, n - 30)):
        yy, xx = np.ogrid[-6:7, -6:7]; m = (yy * yy + xx * xx) <= 30
        base[cy - 6:cy + 7, cx - 6:cx + 7][m] -= 40
        base[cy - 2:cy + 3, cx - 5:cx + 6] -= 18                       # its slot
    return Image.fromarray(np.clip(base, 0, 255).astype(np.uint8))

def fabric_texture(n=1024):
    """Dark grey coated fabric: a fine weave, a soft sheen variation, light grime towards the folds' bottoms."""
    yy, xx = np.mgrid[0:n, 0:n]
    weave = (np.sin(xx * 2 * math.pi / 6) * np.sin(yy * 2 * math.pi / 6)) * 3.0
    big = (tile_noise(n, 64, 5) - 0.5) * 10 + (tile_noise(n, 16, 7) - 0.5) * 5
    grime = np.clip((tile_noise(n, 256, 9) - 0.55) * 30, 0, 12)
    v = 54 + weave + big - grime
    rgb = np.stack([v, v + 2, v + 5], -1)
    specks = rnd.random((n, n)) > 0.9985
    rgb[specks] += 25
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))

def set_texture(mat_name, img):
    m = g['materials'][MAT[mat_name]]
    t = m['pbrMetallicRoughness']['baseColorTexture']['index']; im = g['images'][g['textures'][t]['source']]
    buf = io.BytesIO(); img.save(buf, 'PNG'); data = np.frombuffer(buf.getvalue(), np.uint8)
    global BIN
    while len(BIN) % 4: BIN.append(0)
    off = len(BIN); BIN += data.tobytes()
    g['bufferViews'].append({'buffer': 0, 'byteOffset': off, 'byteLength': int(data.nbytes)})
    im['bufferView'] = len(g['bufferViews']) - 1; im['mimeType'] = 'image/png'; im.pop('uri', None)
    m['pbrMetallicRoughness']['baseColorFactor'] = [1, 1, 1, 1]

set_texture('jwWall', plate_texture())
g['materials'][MAT['jwWall']]['pbrMetallicRoughness'].update({'metallicFactor': 0.25, 'roughnessFactor': 0.38})
set_texture('jwFolds', fabric_texture())
g['materials'][MAT['jwFolds']]['pbrMetallicRoughness'].update({'metallicFactor': 0.0, 'roughnessFactor': 0.62})

# ------------------------------------------------------------------------------------------------ faces re-assigned
def reassign(node_i, frame_i, from_mats, to_mat, pred):
    """Faces of node_i in from_mats for which pred(centroid, normal) (in frame_i's frame) -> to_mat (a new primitive)."""
    if 'mesh' not in N[node_i]: return 0
    M = np.linalg.inv(world(frame_i)) @ world(node_i)
    mesh = g['meshes'][N[node_i]['mesh']]; moved = 0; extra = []
    for p in mesh['primitives']:
        if g['materials'][p.get('material', 0)]['name'] not in from_mats or 'indices' not in p: continue
        P = acc_read(p['attributes']['POSITION']).astype(float)
        P = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]
        I = acc_read(p['indices']).reshape(-1, 3).astype(np.uint32)
        a, b2, c = P[I[:, 0]], P[I[:, 1]], P[I[:, 2]]
        n = np.cross(b2 - a, c - a); ln = np.linalg.norm(n, axis=1); n = n / np.maximum(ln, 1e-9)[:, None]
        # (the winding's normal may point either way: the vertex normals decide)
        if 'NORMAL' in p['attributes']:
            Nv = acc_read(p['attributes']['NORMAL']).astype(float) @ M[:3, :3].T
            avg = Nv[I[:, 0]] + Nv[I[:, 1]] + Nv[I[:, 2]]
            n = np.where((np.sum(n * avg, 1) < 0)[:, None], -n, n)
        m = pred((a + b2 + c) / 3, n) & (ln > 1e-10)
        if not m.any(): continue
        keep = I[~m].reshape(-1); sel = I[m].reshape(-1)
        if len(keep) == 0: keep = I[:1].reshape(-1)
        p['indices'] = acc_new(keep.reshape(-1, 1), 5125, 'SCALAR', 34963)
        q = {'attributes': dict(p['attributes']), 'indices': acc_new(sel.reshape(-1, 1), 5125, 'SCALAR', 34963), 'material': MAT[to_mat], 'mode': 4}
        extra.append(q); moved += int(m.sum())
    mesh['primitives'] += extra
    return moved

def drop(node_i, frame_i, mats, pred):
    if 'mesh' not in N[node_i]: return 0
    M = np.linalg.inv(world(frame_i)) @ world(node_i); gone = 0
    for p in g['meshes'][N[node_i]['mesh']]['primitives']:
        if g['materials'][p.get('material', 0)]['name'] not in mats or 'indices' not in p: continue
        P = acc_read(p['attributes']['POSITION']).astype(float); P = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]
        I = acc_read(p['indices']).reshape(-1, 3).astype(np.uint32)
        cen = (P[I[:, 0]] + P[I[:, 1]] + P[I[:, 2]]) / 3; m = pred(cen)
        if m.any():
            keep = I[~m].reshape(-1)
            if len(keep) == 0: keep = I[:1].reshape(-1)
            p['indices'] = acc_new(keep.reshape(-1, 1), 5125, 'SCALAR', 34963); gone += int(m.sum())
    return gone

HW = [1.32, 1.51, 1.70]; H = [2.40, 2.69, 2.98]; FY = [0.024, 0.012, 0.0]; L = [10.5, 10.1, 9.7]   # (jetway_realorder.py)
INSIDE_FROM = ('jwAluB', 'jwPanel')
stat = {'inside': 0, 'outside': 0, 'floor': 0, 'signs': 0}
jets = sorted({nm.split('/')[0] for nm in idx if nm and nm.startswith('J-') and '/' in nm})
for J in jets:
    # the tunnels: wall faces inside (facing the walkway) -> light grey plate; the outer wall faces -> painted metal
    for k in range(3):
        si = idx[f'{J}/TUNNEL-{k + 1}']
        for d in descendants(si):
            nm = N[d].get('name', '')
            if 'STAIR' in nm: continue
            def inside(c, n, k=k):
                sx = np.sign(c[:, 0])
                return ((np.abs(c[:, 0]) < HW[k] - 0.02) & (c[:, 1] > FY[k] + 0.03) & (c[:, 1] < H[k] + FY[k] - 0.02)
                        & (np.abs(n[:, 1]) < 0.3) & ((np.abs(n[:, 0]) < 0.6) | (n[:, 0] * sx < 0)))   # (facing in, or along the tunnel: the end posts)
            def outside(c, n, k=k):
                sx = np.sign(c[:, 0])
                return (np.abs(c[:, 0]) > HW[k] + 0.02) & (n[:, 0] * sx > 0.7) & (c[:, 1] > FY[k] - 0.3)
            stat['inside'] += reassign(d, si, INSIDE_FROM, 'jwWall', inside)
            stat['outside'] += reassign(d, si, ('jwWall',), 'jwTruss', outside)
    # the cab: its round wall and its front, inside (below the ceiling, facing in)
    cab = idx.get(f'{J}/CAB')
    if cab is not None:
        for part in (f'{J}/CAB', f'{J}/CAB-FRONT', f'{J}/CAB-SLEEVE'):
            pi = idx.get(part)
            if pi is None: continue
            for d in descendants(pi):
                if 'CONSOLE' in N[d].get('name', '') or 'CAB-DOOR' in N[d].get('name', ''): continue
                def cab_in(c, n):
                    r = np.hypot(c[:, 0], c[:, 2]); toward = -(c[:, 0] * n[:, 0] + c[:, 2] * n[:, 2]) / np.maximum(r, 1e-6)
                    round_in = (r < 1.68) & (toward > 0.5)
                    front = (c[:, 0] < -1.35) & (c[:, 0] > -3.15) & (np.abs(c[:, 2]) < 1.80)
                    cen = np.array([-2.25, 0, 0]); tow2 = ((cen[0] - c[:, 0]) * n[:, 0] + (cen[2] - c[:, 2]) * n[:, 2])
                    front_in = front & (tow2 > 0.05)
                    return (round_in | front_in) & (c[:, 1] > 0.03) & (c[:, 1] < 2.58) & (np.abs(n[:, 1]) < 0.3)
                stat['inside'] += reassign(d, cab, INSIDE_FROM, 'jwWall', cab_in)
    # the rotunda: inside its drum (its turning sleeve's and drum's faces facing the middle)
    rs = idx.get(f'{J}/ROTUNDA-SLEEVE')
    if rs is not None:
        for part in (f'{J}/ROTUNDA', f'{J}/ROTUNDA-SLEEVE', f'{J}/ROTUNDA-DRUM'):
            pi = idx.get(part)
            if pi is None: continue
            for d in descendants(pi):
                def rot_in(c, n):
                    r = np.hypot(c[:, 0], c[:, 2]); toward = -(c[:, 0] * n[:, 0] + c[:, 2] * n[:, 2]) / np.maximum(r, 1e-6)
                    return (r < 1.78) & (toward > 0.5) & (c[:, 1] > 0.03) & (c[:, 1] < 2.7) & (np.abs(n[:, 1]) < 0.3)
                stat['inside'] += reassign(d, rs, INSIDE_FROM, 'jwWall', rot_in)
                # the floor: the yellow-black stripe across it (inside the drum, not at its edge) goes
                stat['floor'] += drop(d, rs, ('jwHaz',), lambda c: (np.hypot(c[:, 0], c[:, 2]) < 1.45) & (np.abs(c[:, 1]) < 0.08))
            # the wheelchair sign lying on the floor (a picture node, flat, near the middle): gone
            for d in descendants(pi):
                nd = N[d]
                if 'mesh' in nd and N[d].get('name', '').startswith('jwp'):
                    Mw = np.linalg.inv(world(rs)) @ world(d); c = Mw[:3, 3]; up = Mw[:3, :3] @ np.array([0, 0, 1.0])
                    if abs(c[1]) < 0.1 and math.hypot(c[0], c[2]) < 1.6:
                        del nd['mesh']; stat['signs'] += 1

while len(BIN) % 4: BIN.append(0)
g['buffers'][0]['byteLength'] = len(BIN)
js = json.dumps(g, separators=(',', ':')).encode()
while len(js) % 4: js += b' '
out = bytearray(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(BIN)))
out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(BIN), 0x004E4942) + BIN
open(PATH, 'wb').write(out)
print('FINISH', jets, stat)
