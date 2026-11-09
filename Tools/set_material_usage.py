# Every base material in the project marked for use on instanced meshes (and with Nanite), recompiled and saved, so
# the game never falls back to Unreal's grey default material for them; then each Babylon material instance checked
# for which base material it ends up on.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_material_usage.py" -unattended -nullrhi
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
fixed = []
for path in eal.list_assets('/Game', recursive=True):
    if not path.split('.')[0].split('/')[-1]:
        continue
    data = eal.find_asset_data(path)
    if data.asset_class_path.asset_name != 'Material':
        continue
    m = unreal.load_asset(path)
    changed = False
    for flag in ('used_with_instanced_static_meshes', 'used_with_nanite'):
        try:
            if not m.get_editor_property(flag):
                m.set_editor_property(flag, True)
                changed = True
        except Exception:
            pass
    if changed:
        mel.recompile_material(m)
    eal.save_loaded_asset(m, only_if_is_dirty=False)
    fixed.append('%s=%s' % (m.get_name(), m.get_editor_property('used_with_instanced_static_meshes')))
unreal.log('ADP_USAGE: ' + ', '.join(fixed))

# every link of the Babylon materials' chain pointed at the project's copies (Content/Babylon/Masters)
relinked = 0
for path in eal.list_assets('/Game/Babylon', recursive=True):
    a = unreal.load_asset(path)
    if not isinstance(a, unreal.MaterialInstanceConstant):
        continue
    par = a.get_editor_property('parent')
    if par and par.get_path_name().startswith('/InterchangeAssets/'):
        copy = '/Game/Babylon/Masters/' + par.get_name()
        if eal.does_asset_exist(copy):
            mel.set_material_instance_parent(a, unreal.load_asset(copy))
            mel.update_material_instance(a)
            eal.save_loaded_asset(a, only_if_is_dirty=False)
            relinked += 1
unreal.log('ADP_USAGE: relinked %d' % relinked)

# where the Babylon materials end up
roots = {}
for path in eal.list_assets('/Game/Babylon/Terminal', recursive=True):
    a = unreal.load_asset(path)
    if isinstance(a, unreal.MaterialInstanceConstant):
        b = a
        while isinstance(b, unreal.MaterialInstance):
            b = b.get_editor_property('parent')
        k = b.get_path_name() if b else 'None'
        roots[k] = roots.get(k, 0) + 1
unreal.log('ADP_USAGE: Babylon materials end on ' + str(roots))
unreal.SystemLibrary.quit_editor()
