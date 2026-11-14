# The wash hose's water (owner, 2026-11-14), and the trucks' glass with the beads it leaves:
#   M_WaterDrop  - the jet: one stream of joined pieces, clear by the nozzle and whitening as it breaks up (per-instance
#                  custom data 0: how white, 1: how much there is); the splashes and spray puffs where it lands
#   M_WetPatch   - the wet ground it leaves (a decal; Amount fades it as it dries)
#   M_TruckGlass - the trucks' windows: dark tinted glass, with beads of water where the hose washed (B0..B2, the 12
#                  areas as the paint's; Blow: how far the airflow has pushed them back), drying in about 20 minutes
# Run with UnrealEditor-Cmd ... -ExecutePythonScript=<this file>.
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER = '/Game/Truck'
O = unreal.CustomMaterialOutputType
TLM_SURF = next((getattr(unreal.TranslucencyLightingMode, n) for n in dir(unreal.TranslucencyLightingMode) if n.upper().startswith('TLM_SURFACE')), None)


def log(t):
    unreal.log('ADP_WATERMAT ' + t)


def mat(name):
    path = FOLDER + '/' + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return at.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def custom(m, code, out, ins, x=-400, y=0):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out); c.set_editor_property('description', 'WATER')
    l = []
    for n in ins:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); l.append(ci)
    c.set_editor_property('inputs', l)
    return c


def scalar(m, name, v, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1000, y)
    e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', v)
    return e


def vector(m, name, v, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1000, y)
    e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', unreal.LinearColor(*v))
    return e


def instdata(m, i, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -1000, y)
    e.set_editor_property('data_index', i)
    return e


def connect(a, b, pin, out=''):
    if not mel.connect_material_expressions(a, out, b, pin):
        raise RuntimeError('could not connect %s to %s' % (a.get_name(), pin))


# ---- the jet ----
m = mat('M_WaterDrop')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
if TLM_SURF is not None:
    m.set_editor_property('translucency_lighting_mode', TLM_SURF)
JET = r'''
// clear water by the nozzle, white where it has broken up into spray (W 0..1); the sky's light in it
float3 base = lerp(float3(0.62, 0.72, 0.80), float3(0.95, 0.97, 1.0), W);
return base;
'''
JETA = r'''
float edge = pow(1.0 - saturate(abs(dot(normalize(N), normalize(V)))), 1.5);
return saturate((lerp(0.6, 0.9, W) + edge * 0.35) * A);
'''
JETE = r'''
return lerp(float3(0.05, 0.07, 0.09), float3(0.22, 0.24, 0.26), W) * A;
'''
w = instdata(m, 0, 0); a = instdata(m, 1, 100)
nrm = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -1000, 200)
cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -1000, 300)
c = custom(m, JET, O.CMOT_FLOAT3, ['W'], y=-200); connect(w, c, 'W')
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
c = custom(m, JETA, O.CMOT_FLOAT1, ['W', 'A', 'N', 'V'], y=0)
connect(w, c, 'W'); connect(a, c, 'A'); connect(nrm, c, 'N'); connect(cam, c, 'V')
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY)
c = custom(m, JETE, O.CMOT_FLOAT3, ['W', 'A'], y=200); connect(w, c, 'W'); connect(a, c, 'A')
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
r_ = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 400); r_.set_editor_property('r', 0.08)
mel.connect_material_property(r_, '', unreal.MaterialProperty.MP_ROUGHNESS)
m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)

MIST_CODE = r'''
// a soft round cloud: dense in the middle, feathered out, broken by drifting noise (three octaves of value noise)
float2 d = UV - 0.5;
float r = length(d) * 2.0;
float n = 0.0, amp = 0.55, fr = 3.0;
for (int i = 0; i < 3; i++)
{
    float2 p = UV * fr + Seed * 17.0 + float2(0, Time * 0.6 * (i + 1));
    float2 g = floor(p), f = frac(p); f = f * f * (3.0 - 2.0 * f);
    float a = frac(sin(dot(g, float2(127.1, 311.7))) * 43758.5453), b = frac(sin(dot(g + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(g + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), e = frac(sin(dot(g + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    n += lerp(lerp(a, b, f.x), lerp(c, e, f.x), f.y) * amp;
    amp *= 0.5; fr *= 2.1;
}
float cloud = saturate(1.0 - r) * saturate(1.0 - r);
return saturate(cloud * (0.45 + n) * 1.3 - 0.12) * Left * Fade;
'''
# ---- the spray where the jet lands: soft clouds of fine drops (camera-facing cards; custom data 0: its own pattern,
# 1: how much is left), feathered and broken up, fading softly where they touch the truck or the ground ----
m = mat('M_WaterMist')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', True)
vol = next((getattr(unreal.TranslucencyLightingMode, n) for n in dir(unreal.TranslucencyLightingMode) if 'VOLUMETRIC_NON_DIRECTIONAL' in n.upper()), None)
if vol is not None:
    m.set_editor_property('translucency_lighting_mode', vol)
MIST = MIST_CODE
uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
seed = instdata(m, 0, 100); left = instdata(m, 1, 200)
tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1000, 300)
df = mel.create_material_expression(m, unreal.MaterialExpressionDepthFade, -1000, 400)
fd = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -1200, 400); fd.set_editor_property('r', 30.0)
connect(fd, df, 'FadeDistance')
c = custom(m, MIST, O.CMOT_FLOAT1, ['UV', 'Seed', 'Left', 'Time', 'Fade'])
connect(uv, c, 'UV'); connect(seed, c, 'Seed'); connect(left, c, 'Left'); connect(tm, c, 'Time'); connect(df, c, 'Fade')
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY)
wc = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -200); wc.set_editor_property('constant', unreal.LinearColor(0.92, 0.95, 0.98, 1))
mel.connect_material_property(wc, '', unreal.MaterialProperty.MP_BASE_COLOR)
we = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -300); we.set_editor_property('constant', unreal.LinearColor(0.12, 0.13, 0.14, 1))
mel.connect_material_property(we, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)

# ---- the wet patches ----
m = mat('M_WetPatch')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
log('wet patch blend %s domain %s' % (m.get_editor_property('blend_mode'), m.get_editor_property('material_domain')))
WET = r'''
float2 d = UV - 0.5; float r = length(d) * 2.0;
float n = frac(sin(dot(floor(UV * 9.0), float2(12.9898, 78.233))) * 43758.5453);
return saturate(1.0 - smoothstep(0.55, 1.0, r + (n - 0.5) * 0.25));
'''
uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
c = custom(m, WET, O.CMOT_FLOAT1, ['UV']); connect(uv, c, 'UV')
amt = scalar(m, 'Amount', 1.0, 150)
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 0)
connect(c, mul, 'A'); connect(amt, mul, 'B')
mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_OPACITY)
dk = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -200); dk.set_editor_property('constant', unreal.LinearColor(0.05, 0.05, 0.05, 1))
mel.connect_material_property(dk, '', unreal.MaterialProperty.MP_BASE_COLOR)
rr = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 200); rr.set_editor_property('r', 0.08)
mel.connect_material_property(rr, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)

# ---- the trucks' glass, with the hose's beads ----
BEADS = r'''
// how wet this part is (the 12 areas, as the paint's), on outward-facing glass only (not the cluster's inside)
float3 bLp = WP - Origin.xyz;
float bFa = clamp(dot(bLp, AxisF.xyz) / 100.0 + 6.0, 0.0, 11.999) / 2.0;
float3 bAr = normalize(cross(float3(0, 0, 1), AxisF.xyz));
float bSd = saturate(dot(bLp, bAr) / 40.0 + 0.5);
float bList[12] = { B0.x, B0.y, B0.z, B0.w, B1.x, B1.y, B1.z, B1.w, B2.x, B2.y, B2.z, B2.w };
int bI0 = min(int(bFa), 5), bI1 = min(bI0 + 1, 5); float bFt = saturate(bFa - bI0);
float amount = lerp(lerp(bList[bI0], bList[bI1], bFt), lerp(bList[6 + bI0], bList[6 + bI1], bFt), bSd);
float3 bFlat = float3(bLp.xy, 0);
amount *= saturate(dot(normalize(N), normalize(bFlat + float3(0, 0, 0.001))) * 3.0 + 0.4);
// the beads: one at most in each 1.4 cm cell of the glass's own plane, a random size and place; fewer as it dries;
// pushed back along the truck by the airflow (Blow, cm)
float3 an = abs(N);
float2 q = an.z > max(an.x, an.y) ? WP.xy : (an.x > an.y ? WP.yz : WP.xz);
q += float2(-dot(AxisF.xyz, float3(1, 1, 0)), 0.0) * Blow;
float2 g = floor(q / 1.4), f = frac(q / 1.4) - 0.5;
float r1 = frac(sin(dot(g, float2(12.9898, 78.233))) * 43758.5453);
float r2 = frac(sin(dot(g, float2(39.3468, 11.135))) * 24634.6345);
float2 o = (float2(r1, r2) - 0.5) * 0.45;
float rad = 0.1 + 0.25 * r2;
float bead = (r1 < amount ? 1.0 : 0.0) * (1.0 - smoothstep(rad * 0.7, rad, length(f - o)));
'''
m = mat('M_TruckGlass')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', True)
if TLM_SURF is not None:
    m.set_editor_property('translucency_lighting_mode', TLM_SURF)
P = {'BC': vector(m, 'BC', (0.015, 0.022, 0.026, 0.38), -400), 'Origin': vector(m, 'Origin', (0, 0, 0, 0), -300),
     'AxisF': vector(m, 'AxisF', (1, 0, 0, 0), -200), 'B0': vector(m, 'B0', (0, 0, 0, 0), -100), 'B1': vector(m, 'B1', (0, 0, 0, 0), 0),
     'B2': vector(m, 'B2', (0, 0, 0, 0), 100), 'Blow': scalar(m, 'Blow', 0.0, 200),
     'WP': mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1000, 300),
     'N': mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -1000, 400)}
outs = {'BC': 'RGBA', 'Origin': 'RGBA', 'AxisF': 'RGBA', 'B0': 'RGBA', 'B1': 'RGBA', 'B2': 'RGBA'}
names = ['BC', 'Origin', 'AxisF', 'B0', 'B1', 'B2', 'Blow', 'WP', 'N']
for code, outt, prop, y in [(BEADS + 'return lerp(BC.rgb, float3(0.55, 0.62, 0.68), bead * 0.6);', O.CMOT_FLOAT3, unreal.MaterialProperty.MP_BASE_COLOR, -200),
                            (BEADS + 'return saturate(BC.a + bead * 0.35);', O.CMOT_FLOAT1, unreal.MaterialProperty.MP_OPACITY, 0),
                            (BEADS + 'return bead * float3(0.05, 0.055, 0.06);', O.CMOT_FLOAT3, unreal.MaterialProperty.MP_EMISSIVE_COLOR, 200)]:
    c = custom(m, code, outt, names, y=y)
    for n in names:
        connect(P[n], c, n, outs.get(n, ''))
    mel.connect_material_property(c, '', prop)
r_ = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 400); r_.set_editor_property('r', 0.02)
mel.connect_material_property(r_, '', unreal.MaterialProperty.MP_ROUGHNESS)
s_ = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 500); s_.set_editor_property('r', 0.9)
mel.connect_material_property(s_, '', unreal.MaterialProperty.MP_SPECULAR)
m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)

log('ready: M_WaterDrop, M_WaterMist, M_WetPatch, M_TruckGlass')
unreal.SystemLibrary.quit_editor()
