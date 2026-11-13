# Each stand's own wear (owner, 2026-11-13: "dirt on the gate area, spills, engine starts - don't make each gate look the
# same"; "tyre marks leading up to some stands only, not as strong as on the runway"): /Game/Ground/M_StandWear, the
# see-through material Airfield.cpp (Aprons) lays over every stand, its UVs the stand's own metres (U across the stand
# from its centre line, V along it from the nose back), its vertex colour R a seed (every stand different), G the tyre
# marks (0 on most stands), B 1 for wide-bodies. What it paints, as on real stands:
#   - drips and oil where the wheels stop: under the nose wheel and the main gear, darker and a little glossy
#   - soot fanning out behind the engines (engine starts), patchy
#   - a fuel/hydraulic spill or two at random spots near the wing roots, some stands none
#   - faint tyre streaks along the lead-in on some stands: the nose wheel's and the main gear's lines, broken
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_stand_wear.py" -unattended
import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = '/Game/Ground/M_StandWear'
if eal.does_asset_exist(path): eal.delete_asset(path)
m = tools.create_asset('M_StandWear', '/Game/Ground', unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
CODE = r"""
// value noise fbm
float n1 = 0, n2 = 0;
{
  float2 q = WP.xy / 100.0 * 0.45 + VC.r * 37.0; float a = 0.5;
  [unroll] for (int i = 0; i < 4; i++) {
    float2 ip = floor(q), f = frac(q); f = f * f * (3 - 2 * f);
    float h00 = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453), h10 = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float h01 = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), h11 = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    float v = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
    n1 += v * a; if (i >= 2) n2 += v * a * 3.0;
    q = q * 2.3 + 11.0; a *= 0.5;
  }
}
float c = UV.x, al = UV.y;                       // across, along (metres)
float seed = VC.r, big = VC.b;
float r1 = frac(seed * 91.7), r2 = frac(seed * 53.3), r3 = frac(seed * 17.9), r4 = frac(seed * 7.31);
float sc = lerp(1.0, 1.6, big);                  // wide-bodies: everything further out
float noseW = 5.1 * lerp(1.0, 1.3, big), mainA = noseW + 12.6 * sc, mainC = 3.8 * sc, engA = 11.0 * sc, engC = 5.75 * sc;
// drips where the wheels stop (the strongest, oldest stains)
// (2026-11-13: bigger and stronger, as on real stands: a broad oil stain where the nose wheel stops and a band down the
// centre line from years of aircraft, darker pools under the main gear)
float drip = exp(-(c * c) / 4.0 - pow(al - noseW, 2) / 14.0) * (0.75 + 0.25 * r1);
drip += exp(-(c * c) / 2.2) * smoothstep(noseW - 6.0, noseW, al) * (1.0 - smoothstep(mainA + 4.0, mainA + 14.0, al)) * 0.55;
drip += exp(-pow(abs(c) - mainC, 2) / 4.0 - pow(al - mainA, 2) / 12.0) * (0.65 + 0.35 * r2);
drip *= smoothstep(0.15, 0.6, n1 + 0.2);
// soot fanning out behind each engine (engine starts), stronger at some stands
float t = saturate((al - engA) / (22.0 * sc));
float fan = 0;
[unroll] for (int s = -1; s <= 1; s += 2) { float w = 0.8 + t * 4.5 * sc; fan += exp(-pow(c - s * engC, 2) / (2.0 * w * w)); }
fan *= smoothstep(engA - 1.0, engA + 3.0, al) * (1.0 - smoothstep(engA + 18.0 * sc, engA + 30.0 * sc, al)) * (0.5 + 0.5 * r3);
fan *= smoothstep(0.2, 0.7, n1);
// a spill or two near the wing roots (some stands none)
float2 sp1 = float2((r4 - 0.5) * 14.0 * sc, mainA + (r1 - 0.5) * 8.0), sp2 = float2((r2 - 0.5) * 18.0 * sc, mainA + (r3 - 0.2) * 10.0);
float spill = exp(-dot(float2(c, al) - sp1, float2(c, al) - sp1) / (3.0 + 5.0 * r2)) * step(0.35, r4);
spill += exp(-dot(float2(c, al) - sp2, float2(c, al) - sp2) / (0.8 + 2.0 * r1)) * step(0.6, r2) * 0.8;
spill *= smoothstep(0.3, 0.6, n2 * 0.6 + n1 * 0.5);
// faint tyre streaks along the lead-in (only where G > 0)
float tyre = 0;
if (VC.g > 0.01) {
  float lines = exp(-(c * c) / 0.05) + exp(-pow(abs(c) - mainC, 2) / 0.12) * 0.9;
  float broken = smoothstep(0.35, 0.75, frac(sin(floor(al * 0.7 + seed * 50.0) * 12.9898) * 43758.5453) * 0.6 + n2 * 0.5);
  tyre = lines * broken * VC.g * smoothstep(noseW - 1.0, noseW + 4.0, al) * 1.0;
}
// fade out at the patch's own edges
float edge = smoothstep(0.0, 3.0, al) * (1.0 - smoothstep(Len - 4.0, Len, al)) * (1.0 - smoothstep(Wd - 4.0, Wd, abs(c)));
float a = saturate(drip * 0.8 + fan * 0.6 + spill * 0.7 + tyre * 0.7) * edge;
float3 col = (float3(0.035, 0.032, 0.028) * (drip + spill) + float3(0.05, 0.048, 0.045) * fan + float3(0.03, 0.03, 0.03) * tyre) / max(drip + spill + fan + tyre, 1e-3);
float rough = lerp(0.9, 0.35, saturate((drip + spill) / max(drip + spill + fan + tyre, 1e-3)));
return float4(col, a) + float4(0, 0, 0, 0) * rough;
"""
ROUGH = CODE.replace("return float4(col, a) + float4(0, 0, 0, 0) * rough;", "return rough;")
wp = E(unreal.MaterialExpressionWorldPosition, -900, 0)
uv = E(unreal.MaterialExpressionTextureCoordinate, -900, 100)
vc = E(unreal.MaterialExpressionVertexColor, -900, 200)
ln = E(unreal.MaterialExpressionConstant, -900, 300); ln.set_editor_property('r', 46.5)
wd = E(unreal.MaterialExpressionConstant, -900, 380); wd.set_editor_property('r', 15.0)
# the patch's length and half-width differ for wide-bodies: picked from the vertex colour's blue
lnb = E(unreal.MaterialExpressionConstant, -900, 460); lnb.set_editor_property('r', 74.7)
wdb = E(unreal.MaterialExpressionConstant, -900, 540); wdb.set_editor_property('r', 22.0)
vb = E(unreal.MaterialExpressionComponentMask, -750, 200)
for ch, on in (('r', False), ('g', False), ('b', True), ('a', False)): vb.set_editor_property(ch, on)
mel.connect_material_expressions(vc, '', vb, '')
L = E(unreal.MaterialExpressionLinearInterpolate, -600, 320); mel.connect_material_expressions(ln, '', L, 'A'); mel.connect_material_expressions(lnb, '', L, 'B'); mel.connect_material_expressions(vb, '', L, 'Alpha')
W = E(unreal.MaterialExpressionLinearInterpolate, -600, 450); mel.connect_material_expressions(wd, '', W, 'A'); mel.connect_material_expressions(wdb, '', W, 'B'); mel.connect_material_expressions(vb, '', W, 'Alpha')
def custom(code, out, y):
    c = E(unreal.MaterialExpressionCustom, -350, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out); c.set_editor_property('description', 'STANDWEAR')
    ins = []
    for n_ in ('WP', 'UV', 'VC', 'Len', 'Wd'):
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n_); ins.append(ci)
    c.set_editor_property('inputs', ins)
    for n_, src in (('WP', wp), ('UV', uv), ('VC', vc), ('Len', L), ('Wd', W)): mel.connect_material_expressions(src, '', c, n_)
    return c
cs = custom(CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT4, 0)
cr = custom(ROUGH, unreal.CustomMaterialOutputType.CMOT_FLOAT1, 400)
rgb = E(unreal.MaterialExpressionComponentMask, -100, 0)
for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)): rgb.set_editor_property(ch, on)
aa = E(unreal.MaterialExpressionComponentMask, -100, 100)
for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)): aa.set_editor_property(ch, on)
mel.connect_material_expressions(cs, '', rgb, ''); mel.connect_material_expressions(cs, '', aa, '')
mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(aa, '', unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(cr, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log('ADP_STANDWEAR ready: ' + path)
unreal.SystemLibrary.quit_editor()
