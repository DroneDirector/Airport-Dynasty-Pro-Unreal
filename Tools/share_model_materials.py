# A model made as a variant of another (the roofed passenger stairs): its meshes use the first one's materials of the same name, and its own
# copies of them and of their textures are deleted (they doubled 370 MB). Run after every Tools/import_model.py of the variant; the first one
# must be imported already. Editor closed:
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/share_model_materials.py PassengerStairsRoof PassengerStairs" -unattended -nullrhi
import sys
import unreal
eal = unreal.EditorAssetLibrary
try:
    dst, src = sys.argv[1], sys.argv[2]
    D, S = '/Game/Babylon/%s/%s' % (dst, dst.lower()), '/Game/Babylon/%s/%s' % (src, src.lower())
    shared, own = 0, set()
    for path in eal.list_assets(D + '/StaticMeshes', recursive=True):
        m = unreal.load_asset(path)
        if not isinstance(m, unreal.StaticMesh):
            continue
        ch = False
        for i, sm in enumerate(m.get_editor_property('static_materials')):
            mi = sm.material_interface
            n = mi.get_name() if mi else ''
            other = '%s/Materials/%s' % (S, n)
            if n and mi.get_path_name().startswith(D) and eal.does_asset_exist(other):
                m.set_material(i, unreal.load_asset(other)); ch = True; shared += 1
            elif not n:
                unreal.log_error('ADP_SHARE: empty slot %d in %s' % (i, path))
            elif not eal.does_asset_exist(other):
                own.add(n)
        if ch:
            eal.save_loaded_asset(m, False)
    gone = [0, 0]
    for path in eal.list_assets(D + '/Materials', recursive=True):
        n = path.split('/')[-1].split('.')[0]
        if n not in own and eal.does_asset_exist('%s/Materials/%s' % (S, n)):
            gone[0] += eal.delete_asset(path)
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    keep = set()
    opt = unreal.AssetRegistryDependencyOptions()
    for n in own:
        for d in reg.get_dependencies('%s/Materials/%s' % (D, n), opt) or []:
            keep.add(str(d))
    for path in eal.list_assets(D + '/Textures', recursive=True):
        if path.split('.')[0] not in keep:
            gone[1] += eal.delete_asset(path)
    unreal.log('ADP_SHARE: %d slots use %s, own %s, deleted %d materials, %d textures' % (shared, src, sorted(own), gone[0], gone[1]))
except Exception as e:
    unreal.log_error('ADP_SHARE: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
