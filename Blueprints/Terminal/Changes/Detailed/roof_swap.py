# The smooth wave roof (Data/terminal-layout.json, commit a145376, 2026-11-23) for the AFTER pictures: what that commit
# took out of the layout (the eight roof copies, their pillars and lamps) shows only in BEFORE, what it put in (wave_*
# pieces, skylights, downlights, pillars) only in AFTER. Run by build_new.py after new_lib.py; rebuilds the soffit BVH.
import subprocess
from mathutils import Quaternion
CC = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1))); CCI = CC.inverted()
RR = Matrix.Translation((-OX, 0, 0)) @ Matrix.Rotation(math.pi, 4, 'Z')


def layout(rev):
    return json.loads(subprocess.run(['git', 'show', rev + ':Data/terminal-layout.json'], capture_output=True, cwd=ROOT).stdout)


def instances(L):
    N, out = L['nodes'], {}

    def loc(n):
        t = Matrix.Translation(n.get('translation', (0, 0, 0))); q = n.get('rotation'); s = n.get('scale')
        return t @ (Quaternion((q[3], q[0], q[1], q[2])).to_matrix().to_4x4() if q else Matrix()) @ (Matrix.Diagonal((*s, 1)) if s else Matrix())

    def walk(i, M):
        n = N[i]; W = M @ loc(n)
        if 'asset' in n or 'mesh' in n:
            w = RR @ CC @ W @ CCI; key = n.get('asset') or 'Mesh_%d' % n['mesh']
            out.setdefault((key,) + tuple(round(v, 2) + 0.0 for r in w for v in r), []).append((n['name'], w))
        for c in n.get('children', []): walk(c, W)
    for r0 in L['roots']: walk(r0, Matrix())
    return out


OLD, NEW_ = instances(layout('a145376~1')), instances(layout('a145376'))
gone = {k: v for k, v in OLD.items() if k not in NEW_}; fresh = {k: v for k, v in NEW_.items() if k not in OLD}
oldmat = {}
n_gone = n_new = 0
for o in list(EX.objects):
    if o.type != 'MESH': continue
    base = o.data.name.split('.')[0]
    for part in ('WAVE-TOP', 'WAVE-SOFFIT', 'FASCIA', 'EDGE-LIGHT'):
        if part in o.name and o.material_slots: oldmat[part] = o.material_slots[0].material
    k = (base,) + tuple(round(v, 2) + 0.0 for r in o.matrix_world for v in r)
    if k in gone: o['before_only'] = 1; n_gone += 1
PARTM = {'wave_top': 'WAVE-TOP', 'wave_soffit': 'WAVE-SOFFIT', 'wave_fascia': 'FASCIA', 'wave_edge': 'EDGE-LIGHT'}
FBXM = {}


def fbx(name):
    if name in FBXM: return FBXM[name]
    p = os.path.join(ROOT, 'Art', 'TerminalAnnex', name + '.fbx'); before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=p)
    new = [o for o in bpy.data.objects if o not in before]; ms = [o for o in new if o.type == 'MESH']; ms[0].data.transform(ms[0].matrix_world); me = ms[0].data; me.name = 'X_' + name
    print('FBX', name, len(ms), [m.name if m else None for m in me.materials], flush=True)
    part = next((v for k, v in PARTM.items() if name.startswith(k)), None)
    if part and oldmat.get(part):
        for i in range(len(me.materials)): me.materials[i] = oldmat[part]
    for o in new: bpy.data.objects.remove(o)
    FBXM[name] = me; return me


SOFF = []
for k, lst in fresh.items():
    me = fbx(k[0]) if not k[0].startswith('Mesh_') else bpy.data.meshes.get(k[0])
    if me is None: print('MISSING', k[0]); continue
    for nm, w in lst:
        o = bpy.data.objects.new(nm, me); NEWC.objects.link(o); o.matrix_world = w; o['cutcopy'] = 1; o['grp'] = 'ROOF' if 'ROOF' in nm else 'PILLAR'; n_new += 1
        if 'SOFFIT' in nm: SOFF.append(o)
bpy.context.view_layer.update()
VV, FF = [], []
for o in SOFF:
    b = len(VV); VV += [o.matrix_world @ v.co for v in o.data.vertices]; FF += [tuple(b + i for i in p.vertices) for p in o.data.polygons]
BVH = BVHTree.FromPolygons(VV, FF)
print('ROOF SWAP: gone', len(gone), 'tagged', n_gone, 'new', n_new, 'soffit pieces', len(SOFF), [(x, round(soffit(x), 1)) for x in (110, 150, 200, 300, 400, 500, 550)], flush=True)
