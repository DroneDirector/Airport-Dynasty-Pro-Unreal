# The people's bodies out to Blender, for fitting their clothes (Art/Clothing): a few built people's body meshes
# (a slim woman, an average woman, an average man, a heavy man, a very overweight woman) as FBX in Art/Clothing/Bodies,
# the skeleton with them, so a garment can be cut round the real shape and tested in the real poses.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/export_body_meshes.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(ROOT, 'Art', 'Clothing', 'Bodies')
os.makedirs(OUT, exist_ok=True)
# who: representative shapes (the numbers from the people plan)
WHO = {'woman_slim': 'MH_022_passenger', 'woman_average': 'MH_006_passenger', 'man_average': 'MH_078_ground',
       'man_heavy': 'MH_007_passenger', 'woman_obese': 'MH_120_passenger', 'teen_girl': 'MH_127_passenger'}
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
for tag, name in WHO.items():
    path = '/Game/People/Built/%s/Body/SKM_%s_BodyMesh' % (name, name)
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_warning('ADP_BODY missing: ' + path); continue
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = os.path.join(OUT, tag + '.fbx')
    task.automated = True; task.replace_identical = True; task.prompt = False
    opt = unreal.FbxExportOption()
    opt.collision = False; opt.level_of_detail = False; opt.vertex_color = False
    task.options = opt
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log('ADP_BODY %s -> %s (%s)' % (name, task.filename, 'ok' if ok else 'FAILED'))
unreal.log('ADP_BODY DONE')
unreal.SystemLibrary.quit_editor()
