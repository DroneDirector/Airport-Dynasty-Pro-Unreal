# The imported Babylon model's materials, made usable on instanced meshes (the model is placed as instanced lists):
# Unreal's glTF base materials (/InterchangeAssets/gltf/...) are copied once into Content/Babylon/Masters with
# "used with instanced static meshes" and Nanite on, and the model's own material instances are pointed at the copies.
# Their colours, textures and settings are not changed.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/fix_babylon_materials.py Terminal" -unattended -nullrhi
import sys
import unreal

args = [a for a in sys.argv[1:] if not a.startswith('-')]
name = args[0] if args else 'Terminal'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MASTERS = '/Game/Babylon/Masters'
copies = {}

def copy_of(asset):
    """The project's copy of an Interchange base material (and of its own parents), made once."""
    path = asset.get_path_name().split('.')[0]
    if not path.startswith('/InterchangeAssets/'):
        return asset
    if path in copies:
        return copies[path]
    dest = MASTERS + '/' + asset.get_name()
    c = unreal.load_asset(dest) if eal.does_asset_exist(dest) else eal.duplicate_asset(path, dest)
    if isinstance(c, unreal.MaterialInstanceConstant):
        mel.set_material_instance_parent(c, copy_of(c.get_editor_property('parent')))
    elif isinstance(c, unreal.Material):
        c.set_editor_property('used_with_instanced_static_meshes', True)
        try:
            c.set_editor_property('used_with_nanite', True)
        except Exception:
            pass
        mel.recompile_material(c)
    eal.save_loaded_asset(c, False)
    copies[path] = c
    return c

try:
    n = 0
    for p in eal.list_assets('/Game/Babylon/%s' % name, recursive=True):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.MaterialInstanceConstant):
            parent = a.get_editor_property('parent')
            if parent and parent.get_path_name().startswith('/InterchangeAssets/'):
                mel.set_material_instance_parent(a, copy_of(parent))
                eal.save_loaded_asset(a, False)   # (re-parenting does not mark it changed: saved regardless)
                n += 1
    unreal.log('ADP_MATFIX: %d materials pointed at %d project copies' % (n, len(copies)))
except Exception as e:
    unreal.log_error('ADP_MATFIX: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
