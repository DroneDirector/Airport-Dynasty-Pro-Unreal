# Imports a model exported from the Babylon edition (glTF) into Unreal exactly as it is: every object kept (no merging,
# no remeshing), its normals, UVs, materials and textures as they come, Nanite on for the meshes, and the scene placed
# into the Airport map. Run headless:
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/import_babylon_model.py F:/ADP-Export/terminal.glb Terminal" -unattended -nullrhi
import sys
import unreal

args = [a for a in sys.argv[1:] if not a.startswith('-')]
# --assets: only the meshes, materials and textures (the game places the model itself from Data/babylon-<name>.json)
assets_only = '--assets' in sys.argv
# --shared: the meshes as the glTF shares them (one asset per glTF mesh, <name>_mesh_<n>, at its own origin), so every
# copy of a desk or a chair is an instance of one mesh (the owner's rule: instanced wherever possible). The scene import
# that does this goes into a scratch level, thrown away after; the game places the model from Data/babylon-<name>.json
shared = '--shared' in sys.argv
SCRATCH = '/Game/ADPScratch/ImportScratch'
src = args[0] if args else 'F:/ADP-Export/terminal.glb'
name = args[1] if len(args) > 1 else 'Terminal'
dest = '/Game/Babylon/' + name
eal = unreal.EditorAssetLibrary

try:
    if shared:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level(SCRATCH)
    elif not assets_only:
        unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/Airport')
    # the import into the level: the glTF scene's objects as actors, their meshes, materials and textures as assets
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    source = unreal.InterchangeManager.create_source_data(src)
    world = unreal.EditorLevelLibrary.get_editor_world()
    try:
        if not assets_only or shared:
            params.import_level = world.get_editor_property('persistent_level')
    except Exception as e:
        unreal.log_warning('ADP_IMPORT: import_level ' + str(e))
    if assets_only and not shared:
        manager.import_asset(dest, source, params)
    else:
        manager.import_scene(dest, source, params)
    # the scene import runs in the background: wait until every object has been placed in the map
    manager.wait_until_all_tasks_done(False)
    placed = len(unreal.EditorLevelLibrary.get_all_level_actors())
    unreal.log('ADP_IMPORT: scene imported into %s, %d actors in the map' % (dest, placed))

    # Nanite on every mesh (see-through materials fall back by themselves); geometry itself untouched
    count = 0
    for path in eal.list_assets(dest, recursive=True):
        a = unreal.load_asset(path)
        if isinstance(a, unreal.StaticMesh):
            ns = a.get_editor_property('nanite_settings')
            if not ns.enabled:
                ns.enabled = True
                a.set_editor_property('nanite_settings', ns)
                eal.save_loaded_asset(a)
            count += 1
    unreal.log('ADP_IMPORT: %d meshes, Nanite on' % count)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    if shared:
        # the scratch level and its actors go; the meshes, materials and textures stay
        unreal.EditorLoadingAndSavingUtils.load_map('/Engine/Maps/Entry')
        for p in ('/Game/ADPScratch',):
            if eal.does_directory_exist(p):
                eal.delete_directory(p)
    elif not assets_only:
        unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(), '/Game/Maps/Airport')
    unreal.log('ADP_IMPORT: saved')
except Exception as e:
    unreal.log_error('ADP_IMPORT: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
