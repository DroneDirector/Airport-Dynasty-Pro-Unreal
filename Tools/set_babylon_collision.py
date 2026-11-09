# Collision for the imported Babylon models: every mesh under /Game/Babylon collides with its own exact shape (Unreal's
# "use complex collision as simple"), so you stand on the upper floor and stairs and walls stop you. The geometry is not
# changed at all; only how Unreal tests against it.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_babylon_collision.py" -unattended -nullrhi
import unreal

eal = unreal.EditorAssetLibrary
done = 0
for path in eal.list_assets('/Game/Babylon', recursive=True):
    data = eal.find_asset_data(path)
    if data.asset_class_path.asset_name != 'StaticMesh':
        continue
    m = unreal.load_asset(path)
    bs = m.get_editor_property('body_setup')
    if bs is None:
        continue
    if bs.get_editor_property('collision_trace_flag') != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
        bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.save_loaded_asset(m)
        done += 1
unreal.log('ADP_COLLISION: %d meshes now collide with their own shape' % done)
unreal.SystemLibrary.quit_editor()
