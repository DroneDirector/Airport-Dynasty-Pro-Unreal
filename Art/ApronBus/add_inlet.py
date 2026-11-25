# The charging inlet put into the finished apron bus (Art/ApronBus/apronbus.glb) without touching anything else of it: the skirt
# and stripe are taken from the glb, opened (Tools/apronbus_inlet.py), and written with the flap and the inlet to
# Art/ApronBus/apronbus-inlet.glb; Tools/patch_apronbus.py puts those into apronbus.glb. (The .blend is a newer Blender's and
# not changed; a full rebuild by Tools/make_apron_bus.py has the inlet too.)
#   blender -b --python Art/ApronBus/add_inlet.py      (runs once: from the glb as it was before the patch)
import bpy, bmesh, os, sys, json, struct
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))); sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import numpy as np
import vehicle_kit as K, apronbus_inlet as I
K.reset()
b = open(os.path.join(ROOT, 'Art', 'ApronBus', 'apronbus.glb'), 'rb').read(); n = struct.unpack('<I', b[12:16])[0]; g = json.loads(b[20:20 + n]); raw = b[28 + n:]
assert not any(x.get('name') == 'ChargeInlet' for x in g['nodes']), 'already in the glb'


def acc(i):
    a = g['accessors'][i]; v = g['bufferViews'][a['bufferView']]; t = {5126: '<f4', 5123: '<u2', 5125: '<u4', 5121: 'u1'}[a['componentType']]; c = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[a['type']]
    return np.frombuffer(raw, t, a['count'] * c, v.get('byteOffset', 0) + a.get('byteOffset', 0)).reshape(-1, c)


mats = {m['name']: bpy.data.materials.new(m['name']) for m in g['materials']}
old = {}
for name in ('Skirt_R0', 'Stripe_R0'):
    nd = next(x for x in g['nodes'] if x.get('name') == name); p = g['meshes'][nd['mesh']]['primitives'][0]
    P, T = acc(p['attributes']['POSITION']), acc(p['indices']).reshape(-1, 3)
    bm = bmesh.new(); vs = [bm.verts.new((float(q[0]), float(-q[2]), float(q[1]))) for q in P]
    for t in T: bm.faces.new([vs[int(k)] for k in t])
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5); bmesh.ops.dissolve_limit(bm, angle_limit=0.001, verts=bm.verts[:], edges=bm.edges[:]); bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    print('INLET node', name, {k: v for k, v in nd.items() if k != 'mesh'}, 'prims', len(g['meshes'][nd['mesh']]['primitives']), list(p['attributes']))
    old[name] = K.finish(K.link(name, bm), mats[g['materials'][p['material']]['name']], 40)
I.build(old['Skirt_R0'], old['Stripe_R0'], mats['AB_DarkTrim'], mats['AB_BlackPlastic'], mats['AB_SeatShell'], mats['AB_Stainless'],
        lambda nm, loc: K.ik(nm, loc, prefix='SOCK_'), lambda nm, loc: K.ik(nm, loc, prefix='PIV_'))
K.uv_cube()
for o in K.ALL:
    assert all(o.data.materials), o.name
    c = [v.co for v in o.data.vertices]; print('INLET', o.name, len(o.data.polygons), [s.name for s in o.data.materials], [round(f(v[k] for v in c), 3) for k in range(3) for f in (min, max)])
for o in bpy.context.scene.objects: o.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(ROOT, 'Art', 'ApronBus', 'apronbus-inlet.glb'), export_format='GLB', export_apply=True, export_colors=False)
print('INLET DONE')
