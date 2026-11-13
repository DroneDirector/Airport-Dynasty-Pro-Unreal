# The pavement's soft edge where it meets the grass (owner, 2026-11-12, made photoreal 2026-11-13: "the leading edges
# photorealistic"): /Game/Edges/M_EdgeBand, the see-through material the airfield lays along the whole paving's outline
# (Airfield.cpp PaveTaxiways / EdgePoly, EdgeRect for the car park). Built on real CC0 scans (Poly Haven, Art/Ground/Scans):
#   - aerial_grass_rock: moss and gravel - grit blown onto the pavement's last metre and a strip of gravelly soil and moss
#     where the asphalt stops (real airfield edges: no clean line, the grass never quite reaches the pavement)
#   - sparse_grass: the worn verge, thin grass over soil, fading into the airfield's grass
# The edge line wanders (world-space noise, never repeating), so nowhere is it ruler-straight; the band's vertex colour:
# R asphalt (1) or concrete (0, less grit), G the runway's ends (more), A its fade where bands end.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_edge_bands.py" -unattended
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DIR = '/Game/Edges'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def imp(sid, kind):
    f = os.path.join(ROOT, 'Art', 'Ground', 'Scans', sid, '%s_%s_2k.jpg' % (sid, 'nor_dx' if kind == 'nor' else 'diff'))
    name = 'T_%s_%s' % (sid, kind); dest = '/Game/Ground/Scans/' + sid; path = dest + '/' + name
    if not eal.does_asset_exist(path):
        t = unreal.AssetImportTask(); t.filename = f; t.destination_path = dest; t.destination_name = name
        t.automated = True; t.replace_existing = True; t.save = False
        tools.import_asset_tasks([t])
    a = unreal.load_asset(path)
    if not a: raise RuntimeError('import failed: ' + f)
    if kind == 'nor':
        a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False)
    a.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    eal.save_loaded_asset(a, only_if_is_dirty=False)
    return a
GRIT, GRITN = imp('aerial_grass_rock', 'diff'), imp('aerial_grass_rock', 'nor')
VERGE, VERGEN = imp('sparse_grass', 'diff'), imp('sparse_grass', 'nor')

path = DIR + '/M_EdgeBand'
if eal.does_asset_exist(path): eal.delete_asset(path)
m = tools.create_asset('M_EdgeBand', DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('two_sided', False)
m.set_editor_property('tangent_space_normal', False)
m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
def tobj(tex, x, y, stype):
    e = E(unreal.MaterialExpressionTextureObject, x, y); e.set_editor_property('texture', tex); e.set_editor_property('sampler_type', stype); return e

COMMON = r"""
float2 w = WP.xy / 100.0;                       // metres, world
// value-noise fbm (never repeats along the airfield)
float nA = 0, nB = 0, nC = 0;
{
  float2 q = w * 0.32; float a = 0.5;
  [unroll] for (int i = 0; i < 4; i++) {
    float2 ip = floor(q), f = frac(q); f = f * f * (3 - 2 * f);
    float h00 = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453), h10 = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float h01 = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), h11 = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    float v = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
    if (i < 2) nA += v * a * 1.33; else nB += v * a * 4.0;
    nC += v * a;
    q = q * 2.17 + 31.0; a *= 0.5;
  }
}
float2 q2 = w * 3.1; float2 ip2 = floor(q2), f2 = frac(q2); f2 = f2 * f2 * (3 - 2 * f2);
float g00 = frac(sin(dot(ip2, float2(269.5, 183.3))) * 43758.5453), g10 = frac(sin(dot(ip2 + float2(1, 0), float2(269.5, 183.3))) * 43758.5453);
float g01 = frac(sin(dot(ip2 + float2(0, 1), float2(269.5, 183.3))) * 43758.5453), g11 = frac(sin(dot(ip2 + float2(1, 1), float2(269.5, 183.3))) * 43758.5453);
float fine = lerp(lerp(g00, g10, f2.x), lerp(g01, g11, f2.x), f2.y);
// across the band in metres from the drawn edge (V: 0 at 1.2 m on the pavement, 0.5 the edge, 1 at 1.4 m on the grass)
float x = V < 0.5 ? (V - 0.5) * 2.4 : (V - 0.5) * 2.8;
float more = saturate(VC.g * 0.6 + (1.0 - VC.r) * -0.35 + 0.35);   // runway ends: more; concrete: less
float d = x - (nA - 0.5) * 0.7 - (fine - 0.5) * 0.12;                // from the wandering true edge
// grit and moss blown onto the pavement's last metre, patchy
float grit = smoothstep(-1.0 * (0.5 + more), 0.0, d) * step(d, 0.0);
grit *= smoothstep(0.35, 0.75, fine * 0.6 + nB * 0.6) * 0.9;
// the soil and moss strip where the asphalt stops, its width wandering
float sw = 0.18 + 0.45 * nB;
float soil = 1.0 - smoothstep(sw * 0.55, sw, abs(d - sw * 0.5));
soil *= step(-0.02, d) + smoothstep(-0.1, 0.0, d);
soil = saturate(soil);
// the worn verge fading into the grass
float verge = (1.0 - smoothstep(0.15, 1.25 + 0.3 * nC, d)) * step(0.0, d) * (0.75 + 0.25 * fine);
float alpha = saturate(max(max(grit, soil), verge)) * VA;
alpha *= smoothstep(1.4, 1.1, x) * smoothstep(-1.2, -1.0, x);       // nothing at the band's own borders
// snow (MPC_Weather Snow): a low ploughed bank along the grass side, lumpy, and a dusting over the verge
float bank = saturate(SnowP) * (smoothstep(-0.15, 0.25, d) * (1.0 - smoothstep(0.7 + 0.3 * nB, 1.25, d))) * (0.65 + 0.35 * fine);
float snowE = saturate(bank * 1.6);
alpha = max(alpha, snowE * smoothstep(1.4, 1.1, x) * VA);
"""
SURF = COMMON + r"""
float3 gc = Texture2DSample(Grit, GritSampler, w / 15.0).rgb;
float3 vcol = Texture2DSample(Verge, VergeSampler, w / 2.0).rgb;
float3 gritC = gc * 0.85;
float3 soilC = lerp(gc, gc * float3(0.62, 0.56, 0.48), 0.5) * 0.8;       // damper, darker soil
float3 c = gritC * grit + soilC * soil + vcol * verge;
float s = grit + soil + verge + 1e-4;
float3 base = c / s; base = lerp(base, float3(0.80, 0.83, 0.88) * (0.85 + 0.15 * fine), snowE);
return float4(base, alpha);
"""
NORMAL = COMMON + r"""
float3 gn = Texture2DSample(GritN, GritNSampler, w / 15.0).rgb * 2.0 - 1.0;
float3 vn = Texture2DSample(VergeN, VergeNSampler, w / 2.0).rgb * 2.0 - 1.0;
float s = grit + soil + verge + 1e-4;
float2 t = (gn.xy * (grit + soil) + vn.xy * verge) / s;
return normalize(float3(t.x, -t.y, 1.0 / 0.8));
"""
wp = E(unreal.MaterialExpressionWorldPosition, -900, 0)
tc = E(unreal.MaterialExpressionTextureCoordinate, -900, 100)
vmask = E(unreal.MaterialExpressionComponentMask, -750, 100)
for ch, on in (('r', False), ('g', True), ('b', False), ('a', False)): vmask.set_editor_property(ch, on)
mel.connect_material_expressions(tc, '', vmask, '')
vc = E(unreal.MaterialExpressionVertexColor, -900, 200)
snowp = E(unreal.MaterialExpressionCollectionParameter, -900, 700); snowp.set_editor_property('collection', unreal.load_asset('/Game/Weather/MPC_Weather')); snowp.set_editor_property('parameter_name', 'Snow')
texs = {'Grit': tobj(GRIT, -900, 300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR), 'Verge': tobj(VERGE, -900, 400, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        'GritN': tobj(GRITN, -900, 500, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL), 'VergeN': tobj(VERGEN, -900, 600, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)}
def custom(code, out_type, y, names):
    c = E(unreal.MaterialExpressionCustom, -400, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out_type); c.set_editor_property('description', 'EDGE')
    ins = []
    for n_ in names:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n_); ins.append(ci)
    c.set_editor_property('inputs', ins)
    src = {'WP': (wp, ''), 'V': (vmask, ''), 'VC': (vc, ''), 'VA': (vc, 'A'), 'SnowP': (snowp, ''), **{k: (v, '') for k, v in texs.items()}}
    for n_ in names: mel.connect_material_expressions(src[n_][0], src[n_][1], c, n_)
    return c
cs = custom(SURF, unreal.CustomMaterialOutputType.CMOT_FLOAT4, 0, ['WP', 'V', 'VC', 'VA', 'SnowP', 'Grit', 'Verge'])
cn = custom(NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, 400, ['WP', 'V', 'VC', 'VA', 'SnowP', 'GritN', 'VergeN'])
rgb = E(unreal.MaterialExpressionComponentMask, -150, 0)
for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)): rgb.set_editor_property(ch, on)
aa = E(unreal.MaterialExpressionComponentMask, -150, 100)
for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)): aa.set_editor_property(ch, on)
mel.connect_material_expressions(cs, '', rgb, ''); mel.connect_material_expressions(cs, '', aa, '')
rough = E(unreal.MaterialExpressionConstant, -150, 200); rough.set_editor_property('r', 0.95)
mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(aa, '', unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(cn, '', unreal.MaterialProperty.MP_NORMAL)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log('ADP_EDGES ready: ' + path)
unreal.SystemLibrary.quit_editor()
