# Do these materials compile? Loads each, compiles it and reports its pixel shader's size (0 = it failed and the game would
# show the grey default). ADP_CHECK_MATERIALS=/Game/A,/Game/B names them; the vehicles' by default.
import os, unreal
L = [x for x in os.environ.get('ADP_CHECK_MATERIALS', '').split(',') if x] or ['/Game/Truck/' + n for n in ('M_TruckPaint', 'M_TruckLens', 'M_VehicleDirt', 'M_TruckGlass')]
for p in L:
    m = unreal.load_asset(p)
    if not m: unreal.log_error('ADP_MATCHECK %s MISSING' % p); continue
    unreal.MaterialEditingLibrary.recompile_material(m)
    s = unreal.MaterialEditingLibrary.get_statistics(m)
    n = s.get_editor_property('num_pixel_shader_instructions')
    unreal.log('ADP_MATCHECK %s pixel %d samplers %d %s' % (p, n, s.get_editor_property('num_samplers'), 'OK' if n else 'FAILED'))
unreal.SystemLibrary.quit_editor()
