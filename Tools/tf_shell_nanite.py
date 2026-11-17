# The tax free's shell (one mesh: floors, the promenade's curve, the aurora's thin ribbons, the round bar): Nanite simplified
# its thin curved strips away (jagged edges, missing ribbons - owner 2026-11-17); 15k faces drawn the ordinary way cost
# next to nothing, so it is drawn as it is.
import unreal
for n in ('tf_shell_solid',):
    sm = unreal.load_asset('/Game/Terminal/Meshes/' + n)
    ns = sm.get_editor_property('nanite_settings'); unreal.log('ADP_TFN before %s' % ns.get_editor_property('enabled'))
    ns.set_editor_property('enabled', False); sm.set_editor_property('nanite_settings', ns)
    unreal.EditorAssetLibrary.save_loaded_asset(sm, only_if_is_dirty=False)
    unreal.log('ADP_TFN %s tris %d' % (n, sm.get_num_triangles(0)))
unreal.SystemLibrary.quit_editor()
