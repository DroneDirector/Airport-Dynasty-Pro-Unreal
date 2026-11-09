# The long walkway's strip lamps' face: the owner's lamp photo (made seamless by Tools/make_walk_lamp.py) as a glowing
# material, M_WalkLamp. Unlit: the face is the light (the room is lit by the rectangle lights BabylonModel.cpp puts
# under each strip). The texture's width is the lamp's 30 cm width (frame on both long edges); it repeats along the
# lamp every 20 cm: 'Along' times over the strip (12 for the 2.4 m strips). Also the hall's round downlights (Round 1).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_walk_lamp.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/Materials/WalkLamp'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

task = unreal.AssetImportTask()
task.filename = os.path.join(ROOT, 'Art', 'WalkLamp', 'T_WalkLamp_E.png')
task.destination_path = DEST
task.automated = True
task.replace_existing = True
at.import_asset_tasks([task])
T = unreal.load_asset(DEST + '/T_WalkLamp_E')
eal.save_loaded_asset(T, only_if_is_dirty=False)

path = DEST + '/M_WalkLamp'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
else:
    m = at.create_asset('M_WalkLamp', DEST, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
def P(name, val, x, y):
    q = E(unreal.MaterialExpressionScalarParameter, x, y); q.set_editor_property('parameter_name', name); q.set_editor_property('default_value', val); return q
# laid on a flat panel (Unreal's plane: U along its X, V along its Y): along the lamp U x Along; across, V x Across,
# folded into the band Lo..Hi of the texture (the whole 0..1 for the strips, frame included; the diffuser only for
# the round downlights); Round 1 cuts the panel to a circle
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
m.set_editor_property('two_sided', True)   # (the model is mirrored, which turns the panels' faces round)
tc = E(unreal.MaterialExpressionTextureCoordinate, -1400, 0)
u = E(unreal.MaterialExpressionComponentMask, -1250, -60); u.set_editor_property('r', True); u.set_editor_property('g', False); u.set_editor_property('b', False); u.set_editor_property('a', False)
v = E(unreal.MaterialExpressionComponentMask, -1250, 60); v.set_editor_property('r', False); v.set_editor_property('g', True); v.set_editor_property('b', False); v.set_editor_property('a', False)
mel.connect_material_expressions(tc, '', u, ''); mel.connect_material_expressions(tc, '', v, '')
ua = E(unreal.MaterialExpressionMultiply, -1100, -60); mel.connect_material_expressions(u, '', ua, 'A'); mel.connect_material_expressions(P('Along', 12.0, -1250, -160), '', ua, 'B')
va = E(unreal.MaterialExpressionMultiply, -1100, 60); mel.connect_material_expressions(v, '', va, 'A'); mel.connect_material_expressions(P('Across', 1.0, -1250, 160), '', va, 'B')
vf = E(unreal.MaterialExpressionFrac, -980, 60); mel.connect_material_expressions(va, '', vf, '')
lo = P('Lo', 0.0, -980, 160); hi = P('Hi', 1.0, -980, 260)
vl = E(unreal.MaterialExpressionLinearInterpolate, -850, 60)
mel.connect_material_expressions(lo, '', vl, 'A'); mel.connect_material_expressions(hi, '', vl, 'B'); mel.connect_material_expressions(vf, '', vl, 'Alpha')
uv = E(unreal.MaterialExpressionAppendVector, -700, 0)
mel.connect_material_expressions(ua, '', uv, 'A'); mel.connect_material_expressions(vl, '', uv, 'B')
s = E(unreal.MaterialExpressionTextureSample, -550, 0); s.set_editor_property('texture', T)
mel.connect_material_expressions(uv, '', s, 'UVs')
e = E(unreal.MaterialExpressionMultiply, -350, 0)
mel.connect_material_expressions(s, 'RGB', e, 'A'); mel.connect_material_expressions(P('Glow', 4.0, -550, 200), '', e, 'B')
mel.connect_material_property(e, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
# the circle: 1 inside radius 0.5 of the panel's middle; with Round 0 everything is kept
c = E(unreal.MaterialExpressionConstant2Vector, -1250, 400); c.set_editor_property('r', 0.5); c.set_editor_property('g', 0.5)
d = E(unreal.MaterialExpressionDistance, -1100, 400); mel.connect_material_expressions(tc, '', d, 'A'); mel.connect_material_expressions(c, '', d, 'B')
inside = E(unreal.MaterialExpressionOneMinus, -850, 400)
dd = E(unreal.MaterialExpressionMultiply, -980, 400); dd.set_editor_property('const_b', 2.0); mel.connect_material_expressions(d, '', dd, 'A')
mel.connect_material_expressions(dd, '', inside, '')
rnd = P('Round', 0.0, -850, 500)
mask = E(unreal.MaterialExpressionLinearInterpolate, -700, 420)
one = E(unreal.MaterialExpressionConstant, -850, 330); one.set_editor_property('r', 1.0)
mel.connect_material_expressions(one, '', mask, 'A'); mel.connect_material_expressions(inside, '', mask, 'B'); mel.connect_material_expressions(rnd, '', mask, 'Alpha')
mel.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY_MASK)
m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log('ADP_LAMP: M_WalkLamp built')
unreal.SystemLibrary.quit_editor()
