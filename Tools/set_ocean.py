# The sea's material, M_Ocean (made from scratch, no textures): a deep North Atlantic blue-green with the sky in it.
#  - the swell and the ripples: four long waves and four short ones running with the wind, each a slope worked out
#    exactly (the normal in world space), so the light and reflections move as on real water; calmer far away
#  - the shore (the mesh's red: 1 at the waterline, 0 from 150 m out): the waves breaking into white foam in bands that
#    roll towards the beach, broken up so no two look alike, and the water lighter and greener in the shallows
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_ocean.py" -unattended -nullrhi
import unreal

DEST = '/Game/Materials/Ocean'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
path = DEST + '/M_Ocean'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
else:
    m = at.create_asset('M_Ocean', DEST, unreal.Material, unreal.MaterialFactoryNew())
y = [0]
def E(cls, x=-900):
    y[0] += 60; return mel.create_material_expression(m, cls, x, y[0])
def op(cls, a, b):
    k = E(cls); mel.connect_material_expressions(a, '', k, 'A'); mel.connect_material_expressions(b, '', k, 'B'); return k
def mul(a, b): return op(unreal.MaterialExpressionMultiply, a, b)
def add(a, b): return op(unreal.MaterialExpressionAdd, a, b)
def pw(a, b):
    k = E(unreal.MaterialExpressionPower); mel.connect_material_expressions(a, '', k, 'Base'); mel.connect_material_expressions(b, '', k, 'Exponent'); return k
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

wp = E(unreal.MaterialExpressionWorldPosition, -1800)
X, Y = mul(mask(wp, 'r'), c1(0.01)), mul(mask(wp, 'g'), c1(0.01))     # metres
T = E(unreal.MaterialExpressionTime, -1800)
# the waves: direction (dx, dy), wavelength L (m), amplitude A (m); speed from deep-water waves, c = sqrt(g L / 2 pi)
WAVES = [(0.92, 0.39, 62, 0.55), (0.70, 0.71, 38, 0.32), (0.98, -0.20, 23, 0.18), (0.55, 0.83, 14, 0.09),
         (0.80, 0.60, 6.5, 0.035), (0.30, 0.95, 4.1, 0.022), (0.99, 0.10, 2.6, 0.012), (0.62, -0.78, 1.7, 0.007)]
sx, sy = None, None
for dx, dy, L, A in WAVES:
    k = 2 * 3.14159265 / L
    w = k * (9.81 * L / (2 * 3.14159265)) ** 0.5
    ph = add(add(mul(X, c1(dx * k)), mul(Y, c1(dy * k))), mul(T, c1(-w)))
    cs = fn(unreal.MaterialExpressionCosine, ph); cs.set_editor_property('period', 6.283185)
    gx, gy = mul(cs, c1(A * k * dx)), mul(cs, c1(A * k * dy))       # the slope of this wave
    sx = gx if sx is None else add(sx, gx)
    sy = gy if sy is None else add(sy, gy)
# calmer looking far away (the small waves can't be seen, they'd only shimmer)
dist = E(unreal.MaterialExpressionPixelDepth)
calm = fn(unreal.MaterialExpressionSaturate, op(unreal.MaterialExpressionDivide, c1(60000.0), add(dist, c1(1.0))))
calm = add(mul(calm, c1(0.75)), c1(0.25))
# a lake (Chablais: Lake Geneva, 2026-11-20): the parameter Lake (0 the sea, 1 a lake, set by the game per site) makes the
# waves a third as steep and the shore's surf a gentle lap
lake = E(unreal.MaterialExpressionScalarParameter, -1800); lake.set_editor_property('parameter_name', 'Lake'); lake.set_editor_property('default_value', 0.0)
calm = mul(calm, lerp(c1(1.0), c1(0.32), lake))
# open water grows the waves, shelter calms them (the mesh's green: 0 a sheltered bay .. 1 open water, Tools/MakeOcean.py),
# and the real wind sets how big they are overall (the parameter Wind: 0 calm .. 1 a gale, from the live weather)
vcw = E(unreal.MaterialExpressionVertexColor, -1800)
wind = E(unreal.MaterialExpressionScalarParameter, -1800); wind.set_editor_property('parameter_name', 'Wind'); wind.set_editor_property('default_value', 0.45)
calm = mul(calm, add(c1(0.25), mul(fn(unreal.MaterialExpressionSquareRoot, mask(vcw, 'g')), c1(1.05))))
calm = mul(calm, add(c1(0.3), mul(wind, c1(1.5))))
nrm = fn(unreal.MaterialExpressionNormalize, op(unreal.MaterialExpressionAppendVector, op(unreal.MaterialExpressionAppendVector, mul(mul(sx, c1(-1)), calm), mul(mul(sy, c1(-1)), calm)), c1(1.0)))
mel.connect_material_property(nrm, '', unreal.MaterialProperty.MP_NORMAL)

# the shore: foam in bands rolling in, broken up by a slow pattern across the water
vc = E(unreal.MaterialExpressionVertexColor, -1800)
shore = mask(vc, 'r')
brk = add(fn(unreal.MaterialExpressionSine, add(mul(X, c1(0.09)), mul(T, c1(0.21)))), fn(unreal.MaterialExpressionSine, add(mul(Y, c1(0.13)), mul(T, c1(-0.17)))))
bands = fn(unreal.MaterialExpressionSine, add(add(mul(shore, c1(22.0)), mul(T, c1(-1.1))), mul(brk, c1(1.4))))
bands = fn(unreal.MaterialExpressionSaturate, mul(add(bands, c1(-0.55)), c1(3.5)))
foam = fn(unreal.MaterialExpressionSaturate, add(mul(bands, pw(shore, c1(1.6))), pw(shore, c1(10.0))))
foam = mul(foam, lerp(c1(1.0), c1(0.12), lake))
foam = mul(foam, fn(unreal.MaterialExpressionSaturate, add(c1(0.35), mul(wind, c1(1.3)))))
deep, shallow, white = c3(0.008, 0.035, 0.05), c3(0.03, 0.12, 0.12), c3(0.85, 0.88, 0.88)
water = lerp(deep, shallow, pw(shore, c1(2.0)))
mel.connect_material_property(lerp(water, white, foam), '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(lerp(c1(0.04), c1(0.75), foam), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(lerp(c1(0.5), c1(0.2), foam), '', unreal.MaterialProperty.MP_SPECULAR)

m.set_editor_property('two_sided', True)
m.set_editor_property('tangent_space_normal', False)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log('ADP_OCEAN: material saved')
unreal.SystemLibrary.quit_editor()
