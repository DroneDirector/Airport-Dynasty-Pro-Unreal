# The skeleton for a pet made by make_pet.py: bones at the joint places in pet_skeletons.json (percent of the pet's
# length, measured on its side and top views), the skin bound to them, saved back into <Name>.blend for the moves
# (anim_pet.py). Bone names follow the game's other skeletons (pelvis, spine_01.., thigh_l..), left = the pet's own
# left (+X, the nose looking along -Y). Also writes Models/Views/<Name>_bones.json for the joints picture.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Pets/rig_pet.py -- <Dogs|Cats> <Name>
import bpy, json, os, sys
import numpy as np
from mathutils import Vector

a = sys.argv[sys.argv.index('--') + 1:]
KIND, NAME = a[0], a[1]
HERE = os.path.dirname(os.path.abspath(__file__))
D = os.path.join(HERE, 'Models', KIND, NAME)
J = json.load(open(os.path.join(HERE, 'pet_skeletons.json'), encoding='utf-8'))[NAME]
bpy.ops.wm.open_mainfile(filepath=os.path.join(D, NAME + '.blend'))
for o in [o for o in bpy.data.objects if o.type == 'ARMATURE']: bpy.data.objects.remove(o)
ob = [o for o in bpy.data.objects if o.type == 'MESH'][0]
ob.modifiers.clear(); ob.parent = None; ob.vertex_groups.clear()
co = np.empty(len(ob.data.vertices) * 3); ob.data.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
y0 = co[:, 1].min(); L = co[:, 1].max() - y0

def P(p, side=1):                       # percent (x out, y back from the nose, z up) -> metres
    return Vector((side * p[0] * L / 100, y0 + p[1] * L / 100, p[2] * L / 100))

PARENT = {'pelvis': 'root', 'spine_01': 'pelvis', 'spine_02': 'spine_01', 'spine_03': 'spine_02',
          'neck_01': 'spine_03', 'neck_02': 'neck_01', 'head': 'neck_02', 'ear': 'head',
          'scapula': 'spine_03', 'upperarm': 'scapula', 'lowerarm': 'upperarm', 'hand': 'lowerarm', 'fingers': 'hand',
          'thigh': 'pelvis', 'calf': 'thigh', 'foot': 'calf', 'toes': 'foot'}
SIDED = {'ear', 'scapula', 'upperarm', 'lowerarm', 'hand', 'fingers', 'thigh', 'calf', 'foot', 'toes'}
LEG = {'upperarm', 'lowerarm', 'hand', 'fingers', 'thigh', 'calf', 'foot', 'toes', 'scapula'}

arm = bpy.data.armatures.new(NAME + '_Skeleton'); rig = bpy.data.objects.new('SK_' + NAME, arm)
bpy.context.scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode='EDIT')
eb = arm.edit_bones
def bone(name, h, t, parent, up):
    b = eb.new(name); b.head, b.tail = h, t
    b.align_roll(up)
    if parent: b.parent = eb[parent]; b.use_connect = (parent in eb and (eb[parent].tail - h).length < 1e-4)
    return b
bone('root', Vector((0, 0, 0)), Vector((0, -0.15 * L, 0)), None, Vector((0, 0, 1)))
for k, v in J.items():
    if k == 'tail':
        for i in range(len(v) - 1):
            bone('tail_%02d' % (i + 1), P(v[i]), P(v[i + 1]), 'pelvis' if i == 0 else 'tail_%02d' % i, Vector((0, 0, 1)))
        continue
    up = Vector((0, -1, 0)) if k in LEG else Vector((0, 0, 1))      # legs: bend about their side axis, nose-ward is +Z
    for side, sfx in ([(1, '_l'), (-1, '_r')] if k in SIDED else [(1, '')]):
        par = PARENT[k] + (sfx if PARENT[k] in SIDED else '')
        bone(k + sfx, P(v[0], side), P(v[1], side), par, up)
# the parents are made in file order; fix any made before their parent existed
for b in eb:
    if b.name == 'root' or b.parent: continue
    k = b.name[:-2] if b.name.endswith(('_l', '_r')) else b.name
    if k in PARENT: b.parent = eb[PARENT[k] + (b.name[-2:] if PARENT[k] in SIDED else '')]
bones = {b.name: [list(b.head), list(b.tail)] for b in eb}
bpy.ops.object.mode_set(mode='OBJECT')
for b in arm.bones:                      # root carries no skin
    pass

# the skin: Blender's heat weights (each point follows the bones it is nearest to along the body, not through the air)
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_set(type='ARMATURE_AUTO')
grp = {g.index: g.name for g in ob.vertex_groups}
w = np.zeros(len(ob.data.vertices))
for v in ob.data.vertices:
    w[v.index] = sum(g.weight for g in v.groups)
print('RIG bones', len(arm.bones), 'unweighted points', int((w < 1e-3).sum()), 'of', len(w))
if 'root' in ob.vertex_groups: ob.vertex_groups.remove(ob.vertex_groups['root'])
# points the heat weights missed (loose bits: whiskers, fur tufts) follow the bone nearest to them
miss = np.where(w < 1e-3)[0]
if len(miss):
    segs = [(n, Vector(h), Vector(t)) for n, (h, t) in bones.items() if n != 'root']
    for i in miss:
        p = ob.data.vertices[i].co
        def dist(s):
            _, h, t = s; d = t - h; u = max(0.0, min(1.0, (p - h).dot(d) / d.length_squared)); return (h + d * u - p).length
        n = min(segs, key=dist)[0]
        g = ob.vertex_groups.get(n) or ob.vertex_groups.new(name=n); g.add([int(i)], 1.0, 'REPLACE')
os.makedirs(os.path.join(HERE, 'Models', 'Views'), exist_ok=True)
json.dump({'L': L, 'y0': y0, 'bones': bones}, open(os.path.join(HERE, 'Models', 'Views', NAME + '_bones.json'), 'w'))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(D, NAME + '.blend'))
print('RIG saved', NAME)
