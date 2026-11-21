"""M_FlagCloth (to-do 168): the Swiss flags' fabric. The colour from the cloth's vertices (the red field and the white cross,
drawn by the game's own cloth simulation, Flags.cpp); light through the fabric (two-sided foliage: the cross shows with the
sun behind it); a soft sheen at grazing angles; a fine weave; darker and less rough when wet (Wet, 0..1, from the rain).
  UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/make_flag_material.py" -unattended -nullrhi"""
import unreal

eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
PATH = '/Game/Materials/Flag'; NAME = 'M_FlagCloth'
if eal.does_asset_exist(PATH + '/' + NAME): eal.delete_asset(PATH + '/' + NAME)
m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('two_sided', True)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
m.set_editor_property('used_with_static_lighting', False)

def node(cls, x, y): return mel.create_material_expression(m, cls, x, y)

vc = node(unreal.MaterialExpressionVertexColor, -900, 0)
wet = node(unreal.MaterialExpressionScalarParameter, -900, 300); wet.set_editor_property('parameter_name', 'Wet'); wet.set_editor_property('default_value', 0.0)
# the weave: a fine cross-hatch from the cloth's UVs (60 threads across), a few percent of the colour
tc = node(unreal.MaterialExpressionTextureCoordinate, -1300, 150); tc.set_editor_property('u_tiling', 60.0); tc.set_editor_property('v_tiling', 60.0)
fr = node(unreal.MaterialExpressionFrac, -1100, 150); mel.connect_material_expressions(tc, '', fr, '')
ms = node(unreal.MaterialExpressionComponentMask, -950, 150); ms.set_editor_property('r', True); ms.set_editor_property('g', True)
mel.connect_material_expressions(fr, '', ms, '')
d = node(unreal.MaterialExpressionDotProduct, -800, 150); mel.connect_material_expressions(ms, '', d, 'A'); mel.connect_material_expressions(ms, '', d, 'B')
weave = node(unreal.MaterialExpressionLinearInterpolate, -650, 150)
a = node(unreal.MaterialExpressionConstant, -800, 230); a.set_editor_property('r', 0.94)
b = node(unreal.MaterialExpressionConstant, -800, 280); b.set_editor_property('r', 1.0)
mel.connect_material_expressions(a, '', weave, 'A'); mel.connect_material_expressions(b, '', weave, 'B'); mel.connect_material_expressions(d, '', weave, 'Alpha')
col = node(unreal.MaterialExpressionMultiply, -500, 0); mel.connect_material_expressions(vc, '', col, 'A'); mel.connect_material_expressions(weave, '', col, 'B')
# wet: darker (a third) and glossier
darken = node(unreal.MaterialExpressionLinearInterpolate, -300, 0)
one = node(unreal.MaterialExpressionConstant, -500, 120); one.set_editor_property('r', 1.0)
dk = node(unreal.MaterialExpressionConstant, -500, 170); dk.set_editor_property('r', 0.62)
mel.connect_material_expressions(one, '', darken, 'A'); mel.connect_material_expressions(dk, '', darken, 'B'); mel.connect_material_expressions(wet, '', darken, 'Alpha')
base = node(unreal.MaterialExpressionMultiply, -150, 0); mel.connect_material_expressions(col, '', base, 'A'); mel.connect_material_expressions(darken, '', base, 'B')
mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = node(unreal.MaterialExpressionLinearInterpolate, -300, 300)
r0 = node(unreal.MaterialExpressionConstant, -500, 300); r0.set_editor_property('r', 0.78)
r1 = node(unreal.MaterialExpressionConstant, -500, 350); r1.set_editor_property('r', 0.4)
mel.connect_material_expressions(r0, '', rough, 'A'); mel.connect_material_expressions(r1, '', rough, 'B'); mel.connect_material_expressions(wet, '', rough, 'Alpha')
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
# light through the cloth: the same colour, a little deeper
sub = node(unreal.MaterialExpressionMultiply, -150, 450); half = node(unreal.MaterialExpressionConstant, -300, 500); half.set_editor_property('r', 0.55)
mel.connect_material_expressions(col, '', sub, 'A'); mel.connect_material_expressions(half, '', sub, 'B')
mel.connect_material_property(sub, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
# the sheen: a faint lift at grazing angles (Fresnel), as woven polyester flags have
fz = node(unreal.MaterialExpressionFresnel, -300, 650); fz.set_editor_property('exponent', 4.0)
sh = node(unreal.MaterialExpressionMultiply, -150, 650); k = node(unreal.MaterialExpressionConstant, -300, 720); k.set_editor_property('r', 0.02)
mel.connect_material_expressions(fz, '', sh, 'A'); mel.connect_material_expressions(k, '', sh, 'B')
mel.connect_material_property(sh, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_loaded_asset(m, False)
unreal.log('ADP_FLAG: ' + PATH + '/' + NAME + ' made')
