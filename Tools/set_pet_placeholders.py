# The pet step's stand-ins into Unreal (Tools/make_pet_placeholders.py draws them): /Game/StagePet/T_CatPlaceholder and
# T_DogPlaceholder (their channels are masks: R the fill, G the outline, B the question mark, A where anything is), and
# M_PetPlaceholder: unlit, see-through, the fill in the chosen coat colour (Coat), the outline in the title screen's
# cyan and the question mark in its pink, glowing (Strength).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_pet_placeholders.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'PetPlaceholder')
DIR = '/Game/StagePet'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_PETPH ' + m)

tex = {}
for name, file in (('T_CatPlaceholder', 'cat.png'), ('T_DogPlaceholder', 'dog.png')):
    if eal.does_asset_exist(DIR + '/' + name): eal.delete_asset(DIR + '/' + name)
    t = unreal.AssetImportTask()
    t.filename = os.path.join(ART, file); t.destination_path = DIR; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    tools.import_asset_tasks([t])
    a = unreal.load_asset(DIR + '/' + name)
    a.set_editor_property('srgb', False)                                    # masks, not colours
    a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    a.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    eal.save_loaded_asset(a, only_if_is_dirty=False)
    tex[name] = a; log('texture ' + a.get_path_name())

MN = 'M_PetPlaceholder'
if eal.does_asset_exist(DIR + '/' + MN): eal.delete_asset(DIR + '/' + MN)
m = tools.create_asset(MN, DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('two_sided', True)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
smp = E(unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
smp.set_editor_property('parameter_name', 'Tex'); smp.set_editor_property('texture', tex['T_CatPlaceholder'])
smp.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
coat = E(unreal.MaterialExpressionVectorParameter, -900, 300); coat.set_editor_property('parameter_name', 'Coat'); coat.set_editor_property('default_value', unreal.LinearColor(0.58, 0.28, 0.05, 1))
strength = E(unreal.MaterialExpressionScalarParameter, -900, 450); strength.set_editor_property('parameter_name', 'Strength'); strength.set_editor_property('default_value', 6.0)
fillgain = E(unreal.MaterialExpressionScalarParameter, -900, 550); fillgain.set_editor_property('parameter_name', 'FillGain'); fillgain.set_editor_property('default_value', 0.9)
cyan = E(unreal.MaterialExpressionConstant3Vector, -900, 650); cyan.set_editor_property('constant', unreal.LinearColor(0.035, 0.87, 1.0, 1))
pink = E(unreal.MaterialExpressionConstant3Vector, -900, 750); pink.set_editor_property('constant', unreal.LinearColor(1.0, 0.05, 0.47, 1))
def mul(a, ao, b, bo, x, y):
    n = E(unreal.MaterialExpressionMultiply, x, y); mel.connect_material_expressions(a, ao, n, 'A'); mel.connect_material_expressions(b, bo, n, 'B'); return n
def add(a, b, x, y):
    n = E(unreal.MaterialExpressionAdd, x, y); mel.connect_material_expressions(a, '', n, 'A'); mel.connect_material_expressions(b, '', n, 'B'); return n
fill = mul(mul(coat, '', smp, 'R', -600, 300), '', fillgain, '', -400, 300)
edge = mul(mul(cyan, '', smp, 'G', -600, 650), '', strength, '', -400, 650)
qm = mul(mul(pink, '', smp, 'B', -600, 750), '', strength, '', -400, 750)
col = add(add(fill, edge, -200, 450), qm, -50, 500)
mel.connect_material_property(col, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(smp, 'A', unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)
log('material ' + m.get_path_name())
log('DONE')
unreal.SystemLibrary.quit_editor()
