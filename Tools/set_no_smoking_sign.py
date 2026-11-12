# The no-smoking sticker (Art/Signs/no_smoking.png, Tools/make_no_smoking_sign.py) into Unreal: the picture to
# /Game/Signs/T_NoSmoking and its material M_NoSmoking (masked by the picture's alpha, two-sided, matte), which the game
# puts on the entrance doors' sliding leaves (BabylonModel.cpp, doors named D-F1-ENTR-*).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_no_smoking_sign.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DIR = '/Game/Signs'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
t = unreal.AssetImportTask()
t.filename = os.path.join(ROOT, 'Art', 'Signs', 'no_smoking.png'); t.destination_path = DIR; t.destination_name = 'T_NoSmoking'
t.automated = True; t.replace_existing = True; t.save = True
tools.import_asset_tasks([t])
tex = unreal.load_asset(DIR + '/T_NoSmoking')
if not tex: raise RuntimeError('texture import failed')
path = DIR + '/M_NoSmoking'
if eal.does_asset_exist(path): eal.delete_asset(path)
m = tools.create_asset('M_NoSmoking', DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
m.set_editor_property('two_sided', True)
ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -400, 0)
ts.set_editor_property('texture', tex)
rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 250); rough.set_editor_property('r', 0.55)
mel.connect_material_property(ts, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(ts, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log('ADP_SIGNS no-smoking sticker ready: ' + path)
unreal.SystemLibrary.quit_editor()
