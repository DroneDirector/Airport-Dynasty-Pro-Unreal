# A lamp's own shine for the vehicles (owner, 2026-11-11: the lamp itself bright, the paint round it not lit): a soft
# round glow, additive and unlit, drawn on a disc that faces you (the trucks' LensGlow). Its colour and brightness are
# parameters, set per lamp in the game. Run once (and again only to change the look):
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs path>/Tools/make_lamp_glow.py"
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
DIR, NAME = '/Game/Materials/Lamps', 'M_LampGlow'
path = DIR + '/' + NAME
try:
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else at.create_asset(NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_particle_sprites', True)
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, -100)
    col.set_editor_property('parameter_name', 'Color'); col.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    bri = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 50)
    bri.set_editor_property('parameter_name', 'Brightness'); bri.set_editor_property('default_value', 20.0)
    # the glow's shape: bright in the middle, falling off softly to nothing at the disc's edge
    grad = mel.create_material_expression(m, unreal.MaterialExpressionMaterialFunctionCall, -700, 200)
    grad.set_editor_property('material_function', unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Gradient/RadialGradientExponential.RadialGradientExponential'))
    dens = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -950, 260); dens.set_editor_property('r', 3.0)
    mel.connect_material_expressions(dens, '', grad, 'Density')
    core = mel.create_material_expression(m, unreal.MaterialExpressionPower, -480, 200)   # a hotter core
    mel.connect_material_expressions(grad, '', core, 'Base')
    two = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -650, 330); two.set_editor_property('r', 1.6)
    mel.connect_material_expressions(two, '', core, 'Exp')
    a = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, -50)
    mel.connect_material_expressions(col, '', a, 'A'); mel.connect_material_expressions(bri, '', a, 'B')
    b = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 50)
    mel.connect_material_expressions(a, '', b, 'A'); mel.connect_material_expressions(core, '', b, 'B')
    mel.connect_material_property(b, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_asset(path)
    unreal.log('ADP_LAMP: ' + path + ' made')
except Exception as e:
    unreal.log_error('ADP_LAMP: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
