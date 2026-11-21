"""M_PaxFade (owner 2026-11-21): boarding passengers fade away one by one halfway along the jetway (no cabin yet).
A soft light that rises over them from the feet up (Up 0..1, as an overlay on their opaque MetaHuman materials), then -
the same material now in their slots, so the switch can't be seen - thins away to nothing (Out 0..1). Unlit translucent,
used with skeletal meshes; no textures. PaxFlow.cpp drives Up and Out through a dynamic instance per passenger.
  UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/make_pax_fade_material.py" -unattended -nullrhi"""
import os, stat
import unreal

eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
PATH = '/Game/Materials/PaxFade'; NAME = 'M_PaxFade'
# (an LFS checkout leaves .uasset files read-only: a save then silently keeps the old asset - lfs-readonly-imports)
f = os.path.join(unreal.Paths.project_content_dir(), 'Materials', 'PaxFade', NAME + '.uasset')
if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
if eal.does_asset_exist(PATH + '/' + NAME): eal.delete_asset(PATH + '/' + NAME)
m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', False)
m.set_editor_property('used_with_skeletal_mesh', True)
m.set_editor_property('used_with_static_lighting', False)

def node(cls, x, y): return mel.create_material_expression(m, cls, x, y)
def const(v, x, y):
    c = node(unreal.MaterialExpressionConstant, x, y); c.set_editor_property('r', v); return c
def scalar(name, v, x, y):
    s = node(unreal.MaterialExpressionScalarParameter, x, y); s.set_editor_property('parameter_name', name); s.set_editor_property('default_value', v); return s
def op(cls, a, b, x, y):
    n = node(cls, x, y); mel.connect_material_expressions(a, '', n, 'A'); mel.connect_material_expressions(b, '', n, 'B'); return n

up = scalar('Up', 0.0, -1200, 0)
out = scalar('Out', 0.0, -1200, 200)
# the height on the body: 0 at the feet, 1 at the head (from the mesh's own middle, the hips)
wp = node(unreal.MaterialExpressionWorldPosition, -1600, -300)
opw = node(unreal.MaterialExpressionObjectPositionWS, -1600, -200)
dz = op(unreal.MaterialExpressionSubtract, wp, opw, -1450, -250)
mz = node(unreal.MaterialExpressionComponentMask, -1300, -250); mz.set_editor_property('b', True); mel.connect_material_expressions(dz, '', mz, '')
hs = op(unreal.MaterialExpressionDivide, mz, const(180.0, -1300, -150), -1150, -250)
h = node(unreal.MaterialExpressionSaturate, -850, -250); mel.connect_material_expressions(op(unreal.MaterialExpressionAdd, hs, const(0.5, -1150, -150), -1000, -250), '', h, '')
# the light rising: saturate(Up * 1.6 - h * 0.6) - all of them covered at Up 1
rise_a = op(unreal.MaterialExpressionMultiply, up, const(1.6, -1200, 80), -1000, 0)
rise_b = op(unreal.MaterialExpressionMultiply, h, const(0.6, -850, -150), -700, -200)
rise = node(unreal.MaterialExpressionSaturate, -400, 0); mel.connect_material_expressions(op(unreal.MaterialExpressionSubtract, rise_a, rise_b, -550, 0), '', rise, '')
# then thinning away: * (1 - Out)
keep = node(unreal.MaterialExpressionOneMinus, -1000, 200); mel.connect_material_expressions(out, '', keep, '')
opac = op(unreal.MaterialExpressionMultiply, rise, keep, -200, 100)
mel.connect_material_property(opac, '', unreal.MaterialProperty.MP_OPACITY)
# a soft cool-white light, a little brighter at the outline (the same everywhere in both steps: the switch is unseen)
col = node(unreal.MaterialExpressionConstant3Vector, -700, 400); col.set_editor_property('constant', unreal.LinearColor(0.85, 0.92, 1.0, 1.0))
fz = node(unreal.MaterialExpressionFresnel, -700, 550); fz.set_editor_property('exponent', 2.5)
glow = op(unreal.MaterialExpressionAdd, const(0.7, -700, 650), op(unreal.MaterialExpressionMultiply, fz, const(1.1, -700, 720), -550, 600), -400, 600)
em = op(unreal.MaterialExpressionMultiply, col, glow, -200, 450)
mel.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_loaded_asset(m, False)
unreal.log('ADP_PAXFADE: ' + PATH + '/' + NAME + ' made')
