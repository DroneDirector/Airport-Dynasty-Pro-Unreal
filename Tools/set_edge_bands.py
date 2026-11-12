# The pavement edges' pictures (Art/Edges, Tools/make_edge_textures.py) into Unreal, and the one material the
# airfield lays them with (Airfield.cpp EdgeBands): /Game/Edges/T_EdgeConcrete, T_EdgeAsphalt, T_EdgeAsphaltEnd and
# M_EdgeBand, a lit see-through material that picks the picture by the band's vertex colour (red: asphalt rather than
# concrete, green: the runway's ends with their bigger cracks) and fades by its alpha, so bands that meet merge.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_edge_bands.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DIR = '/Game/Edges'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
tex = {}
for name, f in (('T_EdgeConcrete', 'edge_concrete.png'), ('T_EdgeAsphalt', 'edge_asphalt.png'), ('T_EdgeAsphaltEnd', 'edge_asphalt_end.png')):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(ROOT, 'Art', 'Edges', f); t.destination_path = DIR; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    tex[name] = unreal.load_asset(DIR + '/' + name)
    if not tex[name]: raise RuntimeError('import failed: ' + f)
    # it tiles along the edge; across it must not wrap (the top and bottom rows are clear)
    tex[name].set_editor_property('address_x', unreal.TextureAddress.TA_WRAP)
    tex[name].set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    eal.save_loaded_asset(tex[name], only_if_is_dirty=False)

path = DIR + '/M_EdgeBand'
if eal.does_asset_exist(path): eal.delete_asset(path)
m = tools.create_asset('M_EdgeBand', DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('two_sided', False)
m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
def sample(name, y):
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -900, y)
    ts.set_editor_property('texture', tex[name])
    return ts
con, asp, end = sample('T_EdgeConcrete', -300), sample('T_EdgeAsphalt', 0), sample('T_EdgeAsphaltEnd', 300)
vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -900, 600)
# the picture: concrete -> asphalt by red, -> the ends by green
l1 = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -550, -100)
mel.connect_material_expressions(con, 'RGB', l1, 'A'); mel.connect_material_expressions(asp, 'RGB', l1, 'B'); mel.connect_material_expressions(vc, 'R', l1, 'Alpha')
l2 = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -350, -100)
mel.connect_material_expressions(l1, '', l2, 'A'); mel.connect_material_expressions(end, 'RGB', l2, 'B'); mel.connect_material_expressions(vc, 'G', l2, 'Alpha')
# its see-through-ness the same way, times the band's own fade
a1 = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -550, 200)
mel.connect_material_expressions(con, 'A', a1, 'A'); mel.connect_material_expressions(asp, 'A', a1, 'B'); mel.connect_material_expressions(vc, 'R', a1, 'Alpha')
a2 = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -350, 200)
mel.connect_material_expressions(a1, '', a2, 'A'); mel.connect_material_expressions(end, 'A', a2, 'B'); mel.connect_material_expressions(vc, 'G', a2, 'Alpha')
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 200)
mel.connect_material_expressions(a2, '', mul, 'A'); mel.connect_material_expressions(vc, 'A', mul, 'B')
rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -150, 400); rough.set_editor_property('r', 0.92)
mel.connect_material_property(l2, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log('ADP_EDGES ready: ' + path)
unreal.SystemLibrary.quit_editor()
