# Nanite's backup mesh for every imported Babylon mesh kept at full detail (100% of the triangles). The backup is what
# Unreal draws where Nanite can't run (a graphics card without Shader Model 6, and some ray-traced effects); by default
# Unreal cuts it down hard (the wave roof to 3%), which showed as jagged edges. With Nanite running nothing changes:
# full detail up close, reduced only where it is smaller than a pixel.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_nanite_fallback.py" -unattended -nullrhi
import unreal

eal = unreal.EditorAssetLibrary
done = 0
for path in eal.list_assets('/Game/Babylon', recursive=True):
    if eal.find_asset_data(path).asset_class_path.asset_name != 'StaticMesh':
        continue
    m = unreal.load_asset(path)
    ns = m.get_editor_property('nanite_settings')
    if not ns.enabled:
        continue
    if ns.get_editor_property('fallback_percent_triangles') >= 1.0 and ns.get_editor_property('fallback_relative_error') == 0.0:
        continue
    ns.set_editor_property('fallback_target', unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
    ns.set_editor_property('fallback_percent_triangles', 1.0)
    ns.set_editor_property('fallback_relative_error', 0.0)
    m.set_editor_property('nanite_settings', ns)
    eal.save_loaded_asset(m, only_if_is_dirty=False)
    done += 1
unreal.log('ADP_NANITE: full-detail backup on %d meshes' % done)
unreal.SystemLibrary.quit_editor()
