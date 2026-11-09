# The long walkway's glass barrel vault (F2-WALK/CEILING/GLASS-1..7 and its two gables, terminal meshes 827-835): the
# clear inner glass (as in the gates' and jetways' glass walls), so from inside you see the sky through it and the sun
# shines in; the facade glass mirrored too much at the steep angles you look up through a vault at. Geometry untouched.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_vault_glass.py" -unattended -nullrhi
import unreal

eal = unreal.EditorAssetLibrary
inner = unreal.load_asset('/Game/Materials/Glass/MI_WindowGlass_Inner')
done = 0
for i in range(827, 836):
    p = '/Game/Babylon/Terminal/terminal/StaticMeshes/terminal_mesh_%d' % i
    if not eal.does_asset_exist(p):
        continue
    sm = unreal.load_asset(p)
    slots = sm.get_editor_property('static_materials')
    for s in slots:
        s.material_interface = inner
    sm.set_editor_property('static_materials', slots)
    eal.save_loaded_asset(sm, only_if_is_dirty=False)
    done += 1
unreal.log('ADP_VAULT: clear glass on %d vault meshes' % done)
unreal.SystemLibrary.quit_editor()
