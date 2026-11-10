# M_LumenOccluder: a material nothing is drawn with (masked, nothing kept), for invisible pieces that only tell
# Unreal's lighting (Lumen) where a wall or a roof is: used where a shape is built at run time and Lumen can't see it
# (the control tower's curved shells), so daylight doesn't flood in where there is a roof.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/make_lumen_occluder.py" -unattended -nullrhi
import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
path = '/Game/Materials/Lumen/M_LumenOccluder'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
else:
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_LumenOccluder', '/Game/Materials/Lumen', unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
m.set_editor_property('used_with_instanced_static_meshes', True)
k = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 0); k.set_editor_property('r', 0.0)
mel.connect_material_property(k, '', unreal.MaterialProperty.MP_OPACITY_MASK)
c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 100); c.set_editor_property('constant', unreal.LinearColor(0.2, 0.2, 0.2, 1))
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log('ADP_OCCLUDER saved')
unreal.SystemLibrary.quit_editor()
