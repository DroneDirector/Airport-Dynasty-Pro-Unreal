# The screen of your tablet (and later the phone) while you hold it (owner, 2026-11-18, to-do 117): an unlit material
# whose picture is a texture parameter "Pic" (the game sets the wallpaper / lock screen on it at run time), its
# brightness the scalar "Bright"; its default picture the owner's own drone photo (Art/Tablet/Wallpapers) - every texture
# parameter has a default (else untextured). Writes /Game/Tablet/T_WallSeaStack and /Game/Tablet/M_DeviceScreen.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_device_screen.py" -unattended -nullrhi
import unreal, os
AT = unreal.AssetToolsHelpers.get_asset_tools(); EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def log(*a): unreal.log('ADP_SCREEN ' + ' '.join(str(x) for x in a))
for f in ('T_WallSeaStack', 'M_DeviceScreen'):
    p = os.path.join(ROOT, 'Content', 'Tablet', f + '.uasset')
    if os.path.exists(p): os.chmod(p, 0o666)
t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Tablet', 'Wallpapers', 'owner-drone-sea-stack.jpg')
t.destination_path = '/Game/Tablet'; t.destination_name = 'T_WallSeaStack'; t.automated = True; t.replace_existing = True; t.save = True
AT.import_asset_tasks([t])
tex = unreal.load_asset('/Game/Tablet/T_WallSeaStack'); log('texture', tex)
path = '/Game/Tablet/M_DeviceScreen'
if EL.does_asset_exist(path): EL.delete_asset(path)
m = AT.create_asset('M_DeviceScreen', '/Game/Tablet', unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('used_with_instanced_static_meshes', True)
try: m.set_editor_property('used_with_nanite', True)
except Exception: pass
pic = ML.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
pic.set_editor_property('parameter_name', 'Pic'); pic.set_editor_property('texture', tex)
br = ML.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 250)
br.set_editor_property('parameter_name', 'Bright'); br.set_editor_property('default_value', 1.0)
mul = ML.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 100)
ML.connect_material_expressions(pic, 'RGB', mul, 'A'); ML.connect_material_expressions(br, '', mul, 'B')
ML.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
ML.recompile_material(m); EL.save_loaded_asset(m, only_if_is_dirty=False)
log('material', m, 'done')
