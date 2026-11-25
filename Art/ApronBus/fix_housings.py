# The finished apron bus's wheel housings, steps, floor wells and rear mud flaps made again (Tools/apronbus_housings.py), written to
# Art/ApronBus/apronbus-housings.glb; Tools/patch_apronbus.py apronbus-housings.glb puts them into apronbus.glb.
#   blender -b --python Art/ApronBus/fix_housings.py
import bpy, bmesh, os, sys, json, struct
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))); sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import vehicle_kit as K
import apronbus_housings as H
K.reset()
b = open(os.path.join(ROOT, 'Art', 'ApronBus', 'apronbus.glb'), 'rb').read(); n = struct.unpack('<I', b[12:16])[0]; g = json.loads(b[20:20 + n])
mats = {m['name']: bpy.data.materials.new(m['name']) for m in g['materials']}
def was(name):
    nd = next(x for x in g['nodes'] if x.get('name') == name); p = g['meshes'][nd['mesh']]['primitives']
    print('HOUS was', name, [mats[g['materials'][q['material']]['name']].name for q in p]); return mats[g['materials'][p[0]['material']]['name']]
slab = K.box('FloorSlab', (0, 0, 0.255), (13.80, 2.94, 0.15), was('FloorSlab'), bevel=0.01)
vin = K.box('FloorVinyl', (0, 0, 0.332 - 0.003), (13.62, 2.80, 0.004), was('FloorVinyl'))
H.wells(slab, vin)
for nm in 'FR':
    for s, sd in ((1, 'L'), (-1, 'R')): H.housing(nm, s, was(f'WheelHousing_{nm}{sd}'))
have = {x.get('name') for x in g['nodes']}
for nm in 'FR':
    for s in (1, -1):
        name = f'HousingStep_{(H.FX if nm == "F" else H.RX):+.0f}{s:+d}'
        if name in have: H.step(nm, s, was(name))
for sd in 'LR':
    for c in (f'MudFlap_{sd}R',): H.flap(sd, was(c))
K.uv_cube()
left = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o not in K.ALL]
for o in left: bpy.data.objects.remove(o)
for o in K.ALL:
    assert o.name in have and all(o.data.materials), o.name
    c = [v.co for v in o.data.vertices]; print('HOUS', o.name, len(o.data.polygons), [s.name for s in o.data.materials], [round(f(v[k] for v in c), 3) for k in range(3) for f in (min, max)])
    bm = bmesh.new(); bm.from_mesh(o.data); print('HOUS open edges', sum(not e.is_manifold for e in bm.edges), 'volume', round(bm.calc_volume(), 4))
for o in bpy.context.scene.objects: o.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(ROOT, 'Art', 'ApronBus', 'apronbus-housings.glb'), export_format='GLB', export_apply=True, export_colors=False)
print('HOUS DONE')
