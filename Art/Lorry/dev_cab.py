# Dev loop for the cab only: opens the scene saved just before the cab build (make_lorry.py with ADP_PRECAB=1),
# runs lorry_cab.build and saves F:/ADP-Export/lorry_work/dev/cab.blend for checks/renders. The real build stays make_lorry.py.
import bpy, os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, '..', 'CargoForklift'))
bpy.ops.wm.open_mainfile(filepath='F:/ADP-Export/lorry_work/precab.blend')
import lorry_cab as LC
sc = bpy.context.scene; PARTS = {o.name: o for o in sc.objects if o.type == 'MESH' and o.name != 'Tractor'}
t = time.time(); INFO, IKS = LC.build(sc, sc.objects['Tractor'], sc.objects['Trailer'], PARTS)
print('\n'.join('INFO ' + s for s in INFO)); print('BUILD %.1fs' % (time.time() - t))
for o in sc.objects:
    if o.type == 'MESH': assert len(o.data.materials) and all(o.data.materials), 'empty material slot on ' + o.name
bpy.ops.wm.save_as_mainfile(filepath='F:/ADP-Export/lorry_work/dev/cab.blend'); print('DONE')
