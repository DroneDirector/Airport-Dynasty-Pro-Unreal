# The neon material (owner, 2026-11-12): an unlit tube that glows in one colour, for the character-creation screen's
# vertical neon lights (and any neon after): /Game/Materials/Neon/M_Neon, parameters Color and Strength.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/make_neon_material.py" -unattended -nullrhi
import unreal

DIR, NAME = '/Game/Materials/Neon', 'M_Neon'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
if eal.does_asset_exist(DIR + '/' + NAME): eal.delete_asset(DIR + '/' + NAME)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
col.set_editor_property('parameter_name', 'Color'); col.set_editor_property('default_value', unreal.LinearColor(0.6, 0.9, 1.0, 1))
strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 200)
strength.set_editor_property('parameter_name', 'Strength'); strength.set_editor_property('default_value', 20.0)
mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 60)
mel.connect_material_expressions(col, '', mul, 'A')
mel.connect_material_expressions(strength, '', mul, 'B')
mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat, only_if_is_dirty=False)
unreal.log('ADP_NEON made ' + mat.get_path_name())
unreal.SystemLibrary.quit_editor()
