# Editor script (277 E3a): the charging inlet's four parts into the imported apron bus, with the bus's own materials; no other asset of
# the bus is touched. UnrealEditor-Cmd <project> -ExecutePythonScript=Tools/import_apronbus_inlet.py -unattended -nullrhi
# (other parts made again: ADP_BUS_PARTS=apronbus-housings.glb)
import os
import unreal
eal = unreal.EditorAssetLibrary
TMP, BUS = '/Game/Babylon/ApronBusInletTmp', '/Game/Babylon/ApronBus/apronbus'
SRC = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + 'Art/ApronBus/' + (os.environ.get('ADP_BUS_PARTS') or 'apronbus-inlet.glb')
try:
    # as Tools/import_babylon_model.py does it (a scene import into a scratch level), so that the parts come out in the same axes
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level('/Game/ADPScratch/ImportScratch')
    man = unreal.InterchangeManager.get_interchange_manager_scripted()
    par = unreal.ImportAssetParameters(); par.is_automated = True
    try: par.import_level = unreal.EditorLevelLibrary.get_editor_world().get_editor_property('persistent_level')
    except Exception as e: unreal.log_warning('ADP_INLET import level: ' + str(e))
    man.import_scene(TMP, unreal.InterchangeManager.create_source_data(SRC), par); man.wait_until_all_tasks_done(False)
    mats = {p.split('.')[-1]: p for p in eal.list_assets(BUS + '/Materials')}
    for path in eal.list_assets(TMP, recursive=True):
        a = unreal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh): continue
        name = a.get_name(); tgt = BUS + '/StaticMeshes/' + name
        old = unreal.load_asset(tgt) if eal.does_asset_exist(tgt) else None
        if old: unreal.log('ADP_INLET old %s %s nanite %s' % (name, old.get_bounding_box(), old.get_editor_property('nanite_settings').enabled))
        for i, m in enumerate(a.get_editor_property('static_materials')):
            n = str(m.material_slot_name)
            if n not in mats: n = m.material_interface.get_name()
            a.set_material(i, unreal.load_asset(mats[n]))
        ns = a.get_editor_property('nanite_settings'); ns.enabled = True; a.set_editor_property('nanite_settings', ns)
        eal.save_loaded_asset(a, False); old = None
        if eal.does_asset_exist(tgt): unreal.log('ADP_INLET delete %s' % eal.delete_asset(tgt))
        ok = eal.rename_asset(path.split('.')[0], tgt); b = unreal.load_asset(tgt)
        unreal.log('ADP_INLET new %s %s %s mats %s' % (name, ok, b.get_bounding_box(), [s.material_interface.get_path_name().split('/')[-3] + '/' + s.material_interface.get_name() for s in b.get_editor_property('static_materials')]))
        eal.save_loaded_asset(b, False)
    unreal.EditorLoadingAndSavingUtils.load_map('/Engine/Maps/Entry')
    for p in ('/Game/ADPScratch', TMP):
        if eal.does_directory_exist(p): eal.delete_directory(p)
    unreal.log('ADP_INLET DONE')
except Exception as e:
    unreal.log_error('ADP_INLET FAILED ' + str(e))
unreal.SystemLibrary.quit_editor()
