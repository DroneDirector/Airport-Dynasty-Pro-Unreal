# The lounge fireplace's fire (made from scratch, no textures): M_Fire, flames drawn by the material itself on a few
# crossed panels over the logs: a tongue shape narrowing upwards, eaten into by rising noise so the flames lick and
# split, deep orange at the edges to yellow-white in the core; added to what's behind (as fire is). And its sound,
# S_FireCrackle (Tools/make_fire_sound.py), looping.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_fire.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/Materials/Fire'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

path = DEST + '/M_Fire'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
else:
    m = at.create_asset('M_Fire', DEST, unreal.Material, unreal.MaterialFactoryNew())
y = [0]
def E(cls, x=-900):
    y[0] += 60; return mel.create_material_expression(m, cls, x, y[0])
def op(cls, a, b):
    k = E(cls); mel.connect_material_expressions(a, '', k, 'A'); mel.connect_material_expressions(b, '', k, 'B'); return k
def mul(a, b): return op(unreal.MaterialExpressionMultiply, a, b)
def add(a, b): return op(unreal.MaterialExpressionAdd, a, b)
def sub(a, b): return op(unreal.MaterialExpressionSubtract, a, b)
def c1(v):
    k = E(unreal.MaterialExpressionConstant); k.set_editor_property('r', v); return k
def c3(r, g, b):
    k = E(unreal.MaterialExpressionConstant3Vector); k.set_editor_property('constant', unreal.LinearColor(r, g, b, 1)); return k
def mask(src, ch):
    k = E(unreal.MaterialExpressionComponentMask)
    for c in 'rgba': k.set_editor_property(c, c in ch)
    mel.connect_material_expressions(src, '', k, ''); return k
def fn(cls, a):
    k = E(cls); mel.connect_material_expressions(a, '', k, ''); return k
def lerp(a, b, t):
    k = E(unreal.MaterialExpressionLinearInterpolate)
    mel.connect_material_expressions(a, '', k, 'A'); mel.connect_material_expressions(b, '', k, 'B'); mel.connect_material_expressions(t, '', k, 'Alpha'); return k
def param(name, v):
    k = E(unreal.MaterialExpressionScalarParameter); k.set_editor_property('parameter_name', name); k.set_editor_property('default_value', v); return k

uv = E(unreal.MaterialExpressionTextureCoordinate, -1600)
u, v = mask(uv, 'r'), mask(uv, 'g')
flip = param('FlipV', 1.0)
h = lerp(v, sub(c1(1.0), v), flip)                 # 0 at the logs, 1 at the top
x = mul(sub(u, c1(0.5)), c1(2.0))                  # -1 … 1 across
T = E(unreal.MaterialExpressionTime, -1600)
seed = param('Seed', 0.0)
def sn(a):
    k = fn(unreal.MaterialExpressionSine, a); k.set_editor_property('period', 6.283185); return k
# the flames: several tongues side by side (a wave across), each swaying and flickering in time, the whole lot
# rising (the wobble's phase climbs with the height); no noise texture, only waves, so it is smooth at any size
ux = add(u, seed)
sway = mul(add(sn(add(mul(h, c1(5.0)), mul(T, c1(-4.3)))), mul(sn(add(mul(h, c1(9.0)), mul(T, c1(-6.1)))), c1(0.5))), mul(h, c1(0.09)))
xs = add(x, sway)                                                                    # the flames bent as they rise
tongues = add(c1(0.55), mul(sn(add(mul(ux, c1(17.0)), mul(T, c1(1.3)))), c1(0.45)))   # 0..1: a tongue's middle at 1
flick = add(c1(0.75), mul(mul(sn(add(mul(ux, c1(11.0)), mul(T, c1(7.0)))), sn(mul(T, c1(3.1)))), c1(0.25)))
# the tongue's height: each tongue reaching its own height, flickering
top = mul(add(c1(0.45), mul(tongues, c1(0.55))), flick)
half = add(mul(sub(c1(1.0), h), c1(0.8)), c1(0.08))
w = fn(unreal.MaterialExpressionSaturate, sub(c1(1.0), op(unreal.MaterialExpressionDivide, fn(unreal.MaterialExpressionAbs, xs), half)))
up = fn(unreal.MaterialExpressionSaturate, mul(sub(top, h), c1(3.0)))                # fading out at each tongue's tip
base = fn(unreal.MaterialExpressionSaturate, mul(h, c1(12.0)))
flame = fn(unreal.MaterialExpressionSaturate, mul(mul(mul(w, up), base), add(c1(0.6), mul(tongues, c1(0.6)))))
core = mul(mul(flame, flame), flame)
col = lerp(lerp(c3(0.9, 0.12, 0.01), c3(1.0, 0.45, 0.06), flame), c3(1.0, 0.8, 0.4), core)
glow = param('Glow', 6.0)
mel.connect_material_property(mul(mul(col, flame), glow), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)

# ---- the heat shimmer: M_FireHeat, nothing drawn but the picture behind bent by rising waves, strongest just over
# the flames and fading upwards and at the sides ----
path = DEST + '/M_FireHeat'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
else:
    m = at.create_asset('M_FireHeat', DEST, unreal.Material, unreal.MaterialFactoryNew())
y[0] = 0
uv = E(unreal.MaterialExpressionTextureCoordinate, -1600)
u, v = mask(uv, 'r'), mask(uv, 'g')
h = sub(c1(1.0), v)
T = E(unreal.MaterialExpressionTime, -1600)
edge = mul(fn(unreal.MaterialExpressionSaturate, mul(sub(c1(0.5), fn(unreal.MaterialExpressionAbs, sub(u, c1(0.5)))), c1(4.0))), fn(unreal.MaterialExpressionSaturate, mul(sub(c1(1.0), h), c1(1.6))))
wx = mul(add(sn(add(mul(h, c1(23.0)), mul(T, c1(-5.0)))), mul(sn(add(add(mul(u, c1(31.0)), mul(h, c1(17.0))), mul(T, c1(-7.3)))), c1(0.6))), mul(edge, c1(0.09)))
wy = mul(sn(add(mul(u, c1(19.0)), add(mul(h, c1(29.0)), mul(T, c1(-6.2))))), mul(edge, c1(0.06)))
nrm = op(unreal.MaterialExpressionAppendVector, op(unreal.MaterialExpressionAppendVector, wx, wy), c1(1.0))
mel.connect_material_property(nrm, '', unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(c1(1.0), '', unreal.MaterialProperty.MP_REFRACTION)
mel.connect_material_property(c1(0.0), '', unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(c3(0, 0, 0), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
try:
    m.set_editor_property('refraction_method', unreal.RefractionMode.RM_PIXEL_NORMAL_OFFSET)
except Exception as e:
    unreal.log_warning('ADP_FIRE refraction mode: %s' % e)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)

task = unreal.AssetImportTask()
task.filename = os.path.join(ROOT, 'Art', 'Fire', 'S_FireCrackle.wav')
task.destination_path = DEST
task.automated = True
task.replace_existing = True
task.save = True
at.import_asset_tasks([task])
s = unreal.load_asset(DEST + '/S_FireCrackle')
if s:
    s.set_editor_property('looping', True)
    eal.save_loaded_asset(s, only_if_is_dirty=False)
unreal.log('ADP_FIRE material and sound (%s)' % ('ok' if s else 'NO SOUND'))
unreal.SystemLibrary.quit_editor()
