# The project's own materials (/Game/Materials) set "used with Nanite" and "used with instanced static meshes" and
# saved: without the flag a Nanite or instanced mesh draws Unreal's default material in the packaged game (the editor's
# log says "missing bUsedWithNanite=True! Default Material will be used in game").
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/fix_nanite_usage.py" -unattended -nullrhi
import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
n = 0
try:
    for p in eal.list_assets('/Game/Materials', recursive=True):
        a = unreal.load_asset(p)
        if not isinstance(a, unreal.Material):
            continue
        changed = False
        for prop in ('used_with_nanite', 'used_with_instanced_static_meshes'):
            try:
                if not a.get_editor_property(prop):
                    a.set_editor_property(prop, True)
                    changed = True
            except Exception:
                pass
        if changed:
            mel.recompile_material(a)
            eal.save_loaded_asset(a, False)
            n += 1
            unreal.log('ADP_NANITE: ' + a.get_path_name())
    unreal.log('ADP_NANITE: %d materials flagged' % n)
except Exception as e:
    unreal.log_error('ADP_NANITE: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
