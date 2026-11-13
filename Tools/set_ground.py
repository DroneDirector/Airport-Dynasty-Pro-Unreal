# The one master ground material (owner, 2026-11-13: "one master ground material, the same way for every part"):
# /Game/Materials/Ground/M_Ground - every paved surface outdoors (the airfield's runway, taxiways, aprons and markings,
# the terminal model's apron, asphalt and concrete, the rental car park) is an instance of it, so every ground effect
# is made once and shows everywhere, on every airport and every new piece:
#   - its own look: a colour with a gentle world-space variation (as the kit's M_Surface), or a picture tiled over the
#     surface (UseTexture: the Babylon terminal's apron, asphalt and concrete keep exactly their pictures)
#   - cracks (CrackStyle 0: asphalt, meandering cracks sealed with black tar; 1: concrete, sawn slab joints on a grid and
#     fine hairline cracks; 2: none, for paint), never repeating (world-space Voronoi, warped), patchy, amount by the
#     surface (CrackAmount) and the game (MPC_Weather Cracks); they hold water when wet
#   - wetness and puddles (MPC_Weather, APuddles): the ground darkens and turns glossy; puddles at random spots (more on
#     aprons: PuddleBias), soft wet edges, dark water with the sky reflected by Fresnel (strongest looking along the
#     ground, as real water), rain ripples, drying from their edges in
# Flat up-facing ground only for puddles and cracks (walls and kerb faces stay as they are).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_ground.py" -unattended -nullrhi
import unreal

eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_GROUND ' + str(m))

# ---- the weather's parameters (APuddles sets them; Cracks a global amount for the tuning panel)
MPC = '/Game/Weather/MPC_Weather'
PARAMS = (('Wetness', 0.0), ('PuddleAmount', 0.55), ('PuddleSize', 3.0), ('PuddleShine', 0.9), ('Ripples', 1.0), ('Rain', 0.0), ('Cracks', 1.0), ('Tyres', 1.0))
VPARAMS = (('RwyA', unreal.LinearColor(0, 0, 3000, 0)), ('RwyB', unreal.LinearColor(1, 0, 3000, 0)), ('RwyDisp', unreal.LinearColor(25000, 25000, 0, 0)))
mpc = unreal.load_asset(MPC)
if mpc is None:
    mpc = tools.create_asset('MPC_Weather', '/Game/Weather', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
have = {str(p.get_editor_property('parameter_name')) for p in mpc.get_editor_property('scalar_parameters')}
sps = list(mpc.get_editor_property('scalar_parameters'))
for n, v in PARAMS:
    if n not in have:
        p = unreal.CollectionScalarParameter(); p.set_editor_property('parameter_name', n); p.set_editor_property('default_value', v); sps.append(p)
mpc.set_editor_property('scalar_parameters', sps)
# the runway's centre line ends (world cm, z: half its width) and its displaced thresholds (cm): Airfield.cpp sets them
vh = {str(p.get_editor_property('parameter_name')) for p in mpc.get_editor_property('vector_parameters')}
vps = list(mpc.get_editor_property('vector_parameters'))
for n, v in VPARAMS:
    if n not in vh:
        p = unreal.CollectionVectorParameter(); p.set_editor_property('parameter_name', n); p.set_editor_property('default_value', v); vps.append(p)
mpc.set_editor_property('vector_parameters', vps); eal.save_loaded_asset(mpc, only_if_is_dirty=False)

# ---- the ground's pictures: real-surface scans (Poly Haven, CC0: Art/Ground/Scans) and our own layers (Art/Ground)
import os
ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
def imp(file, dest, name, kind):
    path = dest + '/' + name
    if not eal.does_asset_exist(path):
        t = unreal.AssetImportTask(); t.filename = file; t.destination_path = dest; t.destination_name = name
        t.automated = True; t.replace_existing = True; t.save = False
        tools.import_asset_tasks([t])
    a = unreal.load_asset(path)
    if kind == 'normal': a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False)
    elif kind == 'mask': a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS); a.set_editor_property('srgb', False)
    a.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    eal.save_loaded_asset(a, only_if_is_dirty=False)
    return a
SCAN = {}
for sid in ('asphalt_04', 'asphalt_02', 'concrete_floor_worn_001', 'brushed_concrete_04', 'grass_ground'):
    d = os.path.join(ROOT, 'Art', 'Ground', 'Scans', sid)
    SCAN[sid] = {k: imp(os.path.join(d, '%s_%s_4k.jpg' % (sid, f)), '/Game/Ground/Scans/' + sid, 'T_%s_%s' % (sid, k), kind)
                 for k, f, kind in (('diff', 'diff', 'color'), ('nor', 'nor_dx', 'normal'), ('rough', 'rough', 'mask'))}
LAYER = {n: imp(os.path.join(ROOT, 'Art', 'Ground', n + '.png'), '/Game/Ground/Layers', 'T_' + n, 'mask') for n in ('cracks_asphalt', 'cracks_concrete', 'tyre_rubber')}
log('scans and layers in')

# ---- the master
MP = '/Game/Materials/Ground/M_Ground'
# rebuilt in place (deleting it would ask about the instances that use it, and unattended that is a no)
m = unreal.load_asset(MP) if eal.does_asset_exist(MP) else None
if m: mel.delete_all_material_expressions(m)
else: m = tools.create_asset('M_Ground', '/Game/Materials/Ground', unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('tangent_space_normal', False)      # world-space normals (the airfield's procedural sheets have no tangents)
for flag in ('used_with_nanite', 'used_with_instanced_static_meshes'): m.set_editor_property(flag, True)   # kit pieces (lawns, kerbs) use it too
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
def sp(name, v, x, y):
    e = E(unreal.MaterialExpressionScalarParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', v); return e
def vp(name, c, x, y):
    e = E(unreal.MaterialExpressionVectorParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', c); return e
def cp(name, y):
    c = E(unreal.MaterialExpressionCollectionParameter, -1600, y); c.set_editor_property('collection', mpc); c.set_editor_property('parameter_name', name); return c
def tparam(name, tex, x, y, stype):
    e = E(unreal.MaterialExpressionTextureSampleParameter2D, x, y); e.set_editor_property('parameter_name', name)
    e.set_editor_property('texture', tex); e.set_editor_property('sampler_type', stype); return e
def tobj(name, tex, x, y):
    e = E(unreal.MaterialExpressionTextureObjectParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('texture', tex); return e

# 1. the surface: a plain colour (paint), a picture over the surface's own UVs (the terminal model's ground), or a real
#    scan in world space (UseScan: the airfield's asphalt and concrete) - world space, so it runs on seamlessly from one
#    piece of ground to the next
col = vp('Color', unreal.LinearColor(1, 1, 1, 1), -1600, -600)
var = sp('Variation', 0.0, -1600, -500)
noise = E(unreal.MaterialExpressionNoise, -1400, -500); noise.set_editor_property('scale', 0.02); noise.set_editor_property('levels', 4)
noise.set_editor_property('output_min', -1.0); noise.set_editor_property('output_max', 1.0)
nv = E(unreal.MaterialExpressionMultiply, -1250, -500); mel.connect_material_expressions(noise, '', nv, 'A'); mel.connect_material_expressions(var, '', nv, 'B')
add1 = E(unreal.MaterialExpressionAdd, -1100, -480); add1.set_editor_property('const_a', 1.0); mel.connect_material_expressions(nv, '', add1, 'B')
plain = E(unreal.MaterialExpressionMultiply, -950, -560); mel.connect_material_expressions(col, '', plain, 'A'); mel.connect_material_expressions(add1, '', plain, 'B')
uv = E(unreal.MaterialExpressionTextureCoordinate, -1600, -300)
tu = sp('TexU', 1.0, -1600, -220); tv = sp('TexV', 1.0, -1600, -160)
tuv = E(unreal.MaterialExpressionAppendVector, -1400, -200); mel.connect_material_expressions(tu, '', tuv, 'A'); mel.connect_material_expressions(tv, '', tuv, 'B')
uvs = E(unreal.MaterialExpressionMultiply, -1250, -280); mel.connect_material_expressions(uv, '', uvs, 'A'); mel.connect_material_expressions(tuv, '', uvs, 'B')
tex = tparam('BaseTex', unreal.load_asset('/Engine/EngineResources/DefaultTexture'), -1100, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
mel.connect_material_expressions(uvs, '', tex, 'UVs')
texc = E(unreal.MaterialExpressionMultiply, -950, -300); mel.connect_material_expressions(tex, 'RGB', texc, 'A'); mel.connect_material_expressions(col, '', texc, 'B')
use = E(unreal.MaterialExpressionStaticSwitchParameter, -750, -450); use.set_editor_property('parameter_name', 'UseTexture'); use.set_editor_property('default_value', False)
mel.connect_material_expressions(texc, '', use, 'True'); mel.connect_material_expressions(plain, '', use, 'False')
rough = sp('Roughness', 0.9, -1600, -80); metal = sp('Metallic', 0.0, -1600, -20)
mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
ssize = sp('ScanSize', 4.04, -1600, -760)
wpm = E(unreal.MaterialExpressionWorldPosition, -1600, -900)
wxy = E(unreal.MaterialExpressionComponentMask, -1450, -900)
for ch, on in (('r', True), ('g', True), ('b', False), ('a', False)): wxy.set_editor_property(ch, on)
mel.connect_material_expressions(wpm, '', wxy, '')
s100 = E(unreal.MaterialExpressionMultiply, -1450, -760); mel.connect_material_expressions(ssize, '', s100, 'A'); s100.set_editor_property('const_b', 100.0)
suv = E(unreal.MaterialExpressionDivide, -1300, -850); mel.connect_material_expressions(wxy, '', suv, 'A'); mel.connect_material_expressions(s100, '', suv, 'B')
# the scans without repeats (owner, 2026-11-13: "the asphalt doesn't look seamless from up high"): stochastic tiling,
# as the big games do it - every corner of a grid one scan wide shows the scan turned and shifted at random, the corners
# blended softly with the blend keeping the scan's contrast, so no square of it repeats anywhere
ANTI = r"""
float2 dx = ddx(UV), dy = ddy(UV);
float2 ip = floor(UV), f = frac(UV);
float4 mean = Tx.SampleLevel(TxSampler, float2(0.5, 0.5), 12);
float4 s[4]; float w[4]; float ws = 0;
[unroll] for (int k = 0; k < 4; k++) {
  float2 o = float2(k & 1, k >> 1), c = ip + o;
  float h1 = frac(sin(dot(c, float2(127.1, 311.7))) * 43758.5453);
  float h2 = frac(sin(dot(c, float2(269.5, 183.3))) * 43758.5453);
  float h3 = frac(sin(dot(c, float2(419.2, 371.9))) * 43758.5453);
  float a = h1 * 6.2831853, cs = cos(a), sn = sin(a);
  float2 ruv = float2(cs * UV.x - sn * UV.y, sn * UV.x + cs * UV.y) + float2(h2, h3) * 7.0;
  float2 rdx = float2(cs * dx.x - sn * dx.y, sn * dx.x + cs * dx.y), rdy = float2(cs * dy.x - sn * dy.y, sn * dy.x + cs * dy.y);
  float4 v = Tx.SampleGrad(TxSampler, ruv, rdx, rdy);
  if (IsNormal > 0.5) { float2 t = v.xy * 2.0 - 1.0; t.y = -t.y; v = float4(cs * t.x + sn * t.y, -sn * t.x + cs * t.y, 0, 0); mean = 0; }
  float2 b = 1.0 - abs(f - o); float ww = b.x * b.y; ww *= ww; ww *= ww;
  s[k] = v; w[k] = ww; ws += ww;
}
float4 acc = 0; float w2 = 0;
[unroll] for (int k2 = 0; k2 < 4; k2++) { float wn = w[k2] / ws; acc += wn * (s[k2] - mean); w2 += wn * wn; }
return mean + acc / sqrt(max(w2, 1e-4));
"""
def anti(name, tex, stype, uvn, isn, out, x, y):
    t_ = tobj(name, tex, x - 250, y); t_.set_editor_property('sampler_type', stype)
    c = E(unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', ANTI); c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4); c.set_editor_property('description', 'ANTITILE')
    ins = []
    for n_ in ('Tx', 'UV', 'IsNormal'):
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n_); ins.append(ci)
    c.set_editor_property('inputs', ins)
    k_ = E(unreal.MaterialExpressionConstant, x - 250, y + 80); k_.set_editor_property('r', isn)
    mel.connect_material_expressions(t_, '', c, 'Tx'); mel.connect_material_expressions(uvn, '', c, 'UV'); mel.connect_material_expressions(k_, '', c, 'IsNormal')
    m_ = E(unreal.MaterialExpressionComponentMask, x + 200, y)
    for ch, on in zip('rgba', out): m_.set_editor_property(ch, on)
    mel.connect_material_expressions(c, '', m_, '')
    return m_
big = E(unreal.MaterialExpressionMultiply, -1300, -1100); mel.connect_material_expressions(suv, '', big, 'A'); big.set_editor_property('const_b', 0.137)
C3, C1 = (True, True, True, False), (True, False, False, False)
sdiff = anti('ScanDiff', SCAN['asphalt_04']['diff'], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, suv, 0.0, C3, -1000, -1000)
snor = anti('ScanNormal', SCAN['asphalt_04']['nor'], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, suv, 1.0, C3, -1000, -800)
srgh = anti('ScanRough', SCAN['asphalt_04']['rough'], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, suv, 0.0, C1, -1000, -600)
sbig = anti('ScanDiffBig', SCAN['asphalt_04']['diff'], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, big, 0.0, C3, -1000, -1200)
usescan = sp('UseScan', 0.0, -1600, -1000)

# 2. the surface's own settings for the effects
bias = sp('PuddleBias', 0.0, -1600, 60); pon = sp('Puddles', 1.0, -1600, 120)
cstyle = sp('CrackStyle', 0.0, -1600, 180); camt = sp('CrackAmount', 0.6, -1600, 240)
tamt = sp('TyreAmount', 1.0, -1600, 300)
pdirt = sp('PaintDirt', 0.0, -1600, 330)
vcol = E(unreal.MaterialExpressionVertexColor, -1600, 400)
crackt = tobj('CrackLayer', LAYER['cracks_asphalt'], -1600, 360)
tyret = tobj('TyreLayer', LAYER['tyre_rubber'], -1600, 440)
wp = E(unreal.MaterialExpressionWorldPosition, -1600, 520)
vn = E(unreal.MaterialExpressionVertexNormalWS, -1600, 580)
tm = E(unreal.MaterialExpressionTime, -1600, 640)
cps = {n: cp(n, 700 + 60 * i) for i, (n, _) in enumerate(PARAMS)}
cvs = {n: cp(n, 1300 + 60 * i) for i, (n, _) in enumerate(VPARAMS)}

COMMON = r"""
float2 M = WP.xy / 100.0;
float up = saturate((N.z - 0.92) / 0.06);
float none = step(1.5, Style);
float isConcrete = step(0.5, Style) * (1.0 - none);
float3 surf = Col; float rgh = Rough;
if (UseScan > 0.5) {
  float lumBig = dot(ScanBig, float3(0.3, 0.59, 0.11)), lumSc = dot(ScanCol, float3(0.3, 0.59, 0.11));
  surf = ScanCol * Col * lerp(0.86, 1.14, saturate(lumBig / max(lumSc + 0.001, 0.02) * 0.5));
  rgh = ScanR;
}
float3 cl = Texture2DSample(CrackTex, CrackTexSampler, M / 16.0).rgb;
float ca = saturate(CAmount * Cracks) * (1.0 - none);
float seal = cl.r * ca * up * (1.0 - isConcrete);
float openC = cl.g * saturate(ca * 1.5) * up;
float stain = cl.b * ca * up;
float2 gj = abs(frac(M / 5.0 + 0.5) - 0.5) * 5.0;
float joint = (1.0 - smoothstep(0.004, 0.012, min(gj.x, gj.y))) * isConcrete * up * step(0.01, ca);
float2 A = RwyA.xy, B = RwyB.xy; float2 dir = normalize(B - A + 0.001); float L = length(B - A);
float s_ = dot(WP.xy - A, dir); float d_ = dot(WP.xy - A, float2(-dir.y, dir.x));
float halfW = RwyA.z;
float inRwy = step(0, s_) * step(s_, L) * step(abs(d_), halfW);
float tdA = smoothstep(RwyDisp.x + 5000, RwyDisp.x + 25000, s_) * (1 - smoothstep(RwyDisp.x + 60000, RwyDisp.x + 110000, s_));
float tdB = smoothstep(RwyDisp.y + 5000, RwyDisp.y + 25000, L - s_) * (1 - smoothstep(RwyDisp.y + 60000, RwyDisp.y + 110000, L - s_));
float rub = Texture2DSample(TyreTex, TyreTexSampler, float2(s_ / 6400.0, d_ / 3200.0 + 0.5)).r;
float tyre = saturate(rub * saturate(tdA + tdB) * inRwy * TyreAmt * Tyres) * up;
float2 q = M / max(Size, 0.3);
float n = 0, a = 0.55;
[unroll] for (int i = 0; i < 3; i++) {
  float2 ip = floor(q), f = frac(q); f = f * f * (3 - 2 * f);
  float h00 = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453);
  float h10 = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
  float h01 = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
  float h11 = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
  n += a * lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y); q = q * 2.03 + 17.0; a *= 0.5;
}
float edge = (1.0 - saturate(Amount + Bias) * 0.6) + (1.0 - saturate(Wet)) * 0.30;
float pmask = up * saturate(Puddles) * step(0.02, Wet);
float puddle = smoothstep(edge, edge + 0.06, n) * pmask;
float rim = smoothstep(edge - 0.10, edge, n) * pmask * saturate(Wet);
"""
SURF = COMMON + r"""
float3 c = surf; float r = rgh;
c *= lerp(1.0, 0.82, stain);
c = lerp(c, float3(0.022, 0.021, 0.02), seal);
r = lerp(r, 0.38, seal);
c *= lerp(1.0, 0.35, openC);
c *= lerp(1.0, 0.5, joint);
c = lerp(c, c * 0.28, tyre);
r = lerp(r, 0.55, tyre);
// the markings' own grime (PaintDirt: the paint instances; the vertex colour's green: how dirty this one marking is,
// different on every one): tyre-grey smudges and a brownish film, patchy, never covering it
float grime = saturate(PDirt * VC.g) * up;
if (grime > 0.001) {
  float gpat = smoothstep(0.25, 0.85, n * 0.7 + frac(sin(dot(floor(WP.xy / 70.0), float2(12.9898, 78.233))) * 43758.5453) * 0.3);
  c = lerp(c, c * float3(0.52, 0.48, 0.42), saturate(grime * (0.25 + 0.75 * gpat)));
  // worn through to the pavement in patches (reference: Kristiansand's apron), more where it is dirtier
  float worn = smoothstep(0.62, 0.78, n * 0.8 + gpat * 0.35) * saturate(grime * 1.6);
  c = lerp(c, float3(0.16, 0.16, 0.155), worn * 0.85);
  r = lerp(r, 0.85, grime * 0.6);
}
float wetAll = saturate(Wet) * up;
c *= lerp(1.0, 0.6, wetAll) * lerp(1.0, 0.82, rim) * lerp(1.0, 0.7, saturate(openC + joint) * wetAll);
r = lerp(r, 0.28, wetAll);
c = lerp(c, c * 0.35, puddle);
r = lerp(r, lerp(0.10, 0.02, saturate(Shine)), puddle);
return float4(c, r);
"""
NORMAL = COMMON + r"""
float3 nn = N;
if (UseScan > 0.5) nn = normalize(N + float3(ScanN.xy, 0) * 0.6 * (1.0 - puddle));   // ScanN: the scan's slope in world x, y (ANTI)
float2 uvr = WP.xy / 28.0; float2 acc = 0;
[unroll] for (int k = 0; k < 3; k++) {
  float2 g = uvr + float2(k * 0.37, k * 0.71);
  float2 cell = floor(g); float2 f = frac(g) - 0.5;
  float h = frac(sin(dot(cell + k * 7.0, float2(12.9898, 78.233))) * 43758.5453);
  float2 off = (float2(frac(h * 7.13), frac(h * 3.71)) - 0.5) * 0.6;
  float2 d = f - off; float dist = length(d) + 1e-4;
  float t = frac(T * 0.9 + h);
  float wave = sin((dist - t * 0.45) * 70.0) * saturate(1.0 - t) * saturate(1.0 - abs(dist - t * 0.45) * 14.0);
  acc += (d / dist) * wave;
}
float2 tilt = acc * 0.35 * saturate(Ripples) * saturate(Rain) * puddle;
return normalize(nn + float3(tilt, 0.0));
"""
REFL = COMMON + r"""
float wetAll = saturate(Wet) * up;
return saturate(puddle * lerp(0.35, 0.8, saturate(Shine)) + wetAll * 0.05);
"""
ALL = ['WP', 'N', 'Bias', 'Puddles', 'Wet', 'Amount', 'Size', 'Style', 'CAmount', 'Cracks', 'Col', 'Rough', 'UseScan', 'ScanCol', 'ScanBig', 'ScanR',
       'CrackTex', 'TyreTex', 'RwyA', 'RwyB', 'RwyDisp', 'TyreAmt', 'Tyres']
SRC = lambda: {'WP': (wp, ''), 'N': (vn, ''), 'Bias': (bias, ''), 'Puddles': (pon, ''), 'Wet': (cps['Wetness'], ''), 'Amount': (cps['PuddleAmount'], ''),
       'Size': (cps['PuddleSize'], ''), 'Style': (cstyle, ''), 'CAmount': (camt, ''), 'Cracks': (cps['Cracks'], ''), 'Col': (use, ''),
       'Rough': (rough, ''), 'UseScan': (usescan, ''), 'ScanCol': (sdiff, ''), 'ScanBig': (sbig, ''), 'ScanR': (srgh, ''),
       'ScanN': (snor, ''), 'CrackTex': (crackt, ''), 'TyreTex': (tyret, ''), 'RwyA': (cvs['RwyA'], ''), 'RwyB': (cvs['RwyB'], ''),
       'RwyDisp': (cvs['RwyDisp'], ''), 'TyreAmt': (tamt, ''), 'Tyres': (cps['Tyres'], ''), 'VC': (vcol, ''), 'PDirt': (pdirt, ''), 'Shine': (cps['PuddleShine'], ''),
       'T': (tm, ''), 'Ripples': (cps['Ripples'], ''), 'Rain': (cps['Rain'], '')}
def custom(code, out_type, x, y, extra):
    c = E(unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out_type); c.set_editor_property('description', 'GROUND')
    names = extra + ALL; ins = []
    for n_ in names:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n_); ins.append(ci)
    c.set_editor_property('inputs', ins)
    src = SRC()
    for n_ in names: mel.connect_material_expressions(src[n_][0], src[n_][1], c, n_)
    return c
cs = custom(SURF, unreal.CustomMaterialOutputType.CMOT_FLOAT4, -300, 0, ['Shine', 'VC', 'PDirt'])
cn = custom(NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -300, 400, ['ScanN', 'T', 'Ripples', 'Rain'])
cr = custom(REFL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -300, 800, ['Shine'])
rgb = E(unreal.MaterialExpressionComponentMask, -50, 0)
for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)): rgb.set_editor_property(ch, on)
aa = E(unreal.MaterialExpressionComponentMask, -50, 100)
for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)): aa.set_editor_property(ch, on)
mel.connect_material_expressions(cs, '', rgb, ''); mel.connect_material_expressions(cs, '', aa, '')
mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(aa, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(cn, '', unreal.MaterialProperty.MP_NORMAL)
rv = E(unreal.MaterialExpressionReflectionVectorWS, -300, 1000)
sky = E(unreal.MaterialExpressionSkyAtmosphereViewLuminance, -100, 1000); mel.connect_material_expressions(rv, '', sky, 'WorldDirection')
fr = E(unreal.MaterialExpressionFresnel, -100, 1100); fr.set_editor_property('exponent', 4.0); fr.set_editor_property('base_reflect_fraction', 0.04)
m1 = E(unreal.MaterialExpressionMultiply, 100, 1000); mel.connect_material_expressions(sky, '', m1, 'A'); mel.connect_material_expressions(fr, '', m1, 'B')
m2 = E(unreal.MaterialExpressionMultiply, 250, 1000); mel.connect_material_expressions(m1, '', m2, 'A'); mel.connect_material_expressions(cr, '', m2, 'B')
mel.connect_material_property(m2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('master ' + m.get_path_name())

# ---- the instances
def inst(path, params, tex=None, switch_tex=False, textures=None):
    a = unreal.load_asset(path)
    if a is None and path.startswith(KIT):   # a new kit surface (the taxiway shoulders): made here
        a = unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.split('/')[-1], KIT[:-1], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if a is None: log('missing ' + path); return
    if not isinstance(a, unreal.MaterialInstanceConstant): log('not an instance ' + path); return
    mel.set_material_instance_parent(a, m)
    mel.set_material_instance_static_switch_parameter_value(a, 'UseTexture', switch_tex)
    if tex: mel.set_material_instance_texture_parameter_value(a, 'BaseTex', tex)
    for k, v in (textures or {}).items(): mel.set_material_instance_texture_parameter_value(a, k, v)
    for k, v in params.items():
        if isinstance(v, unreal.LinearColor): mel.set_material_instance_vector_parameter_value(a, k, v)
        else: mel.set_material_instance_scalar_parameter_value(a, k, v)
    mel.update_material_instance(a); eal.save_loaded_asset(a, only_if_is_dirty=False)
    log('%s -> M_Ground' % path.split('/')[-1])
def scan(sid, big_sid=None):
    return {'ScanDiff': SCAN[sid]['diff'], 'ScanNormal': SCAN[sid]['nor'], 'ScanRough': SCAN[sid]['rough'], 'ScanDiffBig': SCAN[big_sid or sid]['diff']}
W = unreal.LinearColor(1, 1, 1, 1)
KIT = '/Game/Materials/Kit/'
inst(KIT + 'MI_gasphalt', dict(Color=unreal.LinearColor(0.68, 0.68, 0.70, 1),   # aged airport asphalt a shade darker than the concrete shoulders
     UseScan=1.0, ScanSize=4.04, CrackStyle=0.0, CrackAmount=0.7, TyreAmount=1.0, Variation=0.0),
     textures=dict(scan('asphalt_04', 'asphalt_02'), CrackLayer=LAYER['cracks_asphalt']))
# the taxiways' shoulders (Airfield PaveTaxiways): older, lighter asphalt with more cracks, no tyre rubber (real shoulders
# are paved for jet blast, rarely driven on)
inst(KIT + 'MI_shoulder', dict(Color=unreal.LinearColor(0.75, 0.75, 0.76, 1), UseScan=1.0, ScanSize=4.04, CrackStyle=0.0, CrackAmount=0.95, TyreAmount=0.0, Variation=0.0),
     textures=dict(scan('asphalt_04', 'asphalt_02'), CrackLayer=LAYER['cracks_asphalt']))
# the airfield's ground grass (owner, 2026-11-13: the edges must look real, so the grass they meet must too): a real
# grass-and-soil scan, a touch greener for a northern summer, no cracks, no tyre rubber, damp but no puddles
inst(KIT + 'MI_ggrass', dict(Color=unreal.LinearColor(0.66, 1.16, 0.50, 1), UseScan=1.0, ScanSize=2.51, CrackStyle=2.0, CrackAmount=0.0, TyreAmount=0.0, Puddles=0.0, Variation=0.0),
     textures=dict(scan('grass_ground'), CrackLayer=LAYER['cracks_asphalt']))
# the terminal model's own lawn: the same grass, so there is no lighter square round the terminal
inst('/Game/Babylon/Terminal/terminal/Materials/grass', dict(Color=unreal.LinearColor(0.66, 1.16, 0.50, 1), UseScan=1.0, ScanSize=2.51, CrackStyle=2.0, CrackAmount=0.0, TyreAmount=0.0, Puddles=0.0, Variation=0.0),
     textures=dict(scan('grass_ground'), CrackLayer=LAYER['cracks_asphalt']))
for nm, b_ in (('MI_gconcrete', 0.0), ('MI_gapron', 0.12)):
    inst(KIT + nm, dict(Color=unreal.LinearColor(1.12, 1.12, 1.10, 1), UseScan=1.0, ScanSize=3.0, CrackStyle=1.0, CrackAmount=0.6, TyreAmount=0.0, PuddleBias=b_, Variation=0.0),
         textures=dict(scan('concrete_floor_worn_001', 'brushed_concrete_04'), CrackLayer=LAYER['cracks_concrete']))
# the red of the mandatory instruction markings (the runway-ahead signs): its own paint, so the kit's red stays as it is
inst(KIT + 'MI_paintred', dict(Color=unreal.LinearColor(0.42, 0.02, 0.02, 1), UseScan=0.0, CrackStyle=0.0, CrackAmount=0.7, TyreAmount=1.0, Roughness=0.55, Variation=0.03, PaintDirt=1.0),
     textures=dict(CrackLayer=LAYER['cracks_asphalt']))
# the kit's white and yellow (aircraft, hangars, the tower, fittings) stay on the kit's own master: no road cracks on them
KITM = unreal.load_asset('/Game/Materials/Kit/M_Surface')
PAINT = {}
for name in ('MI_yellow', 'MI_white'):
    a = unreal.load_asset(KIT + name); PAINT[name] = mel.get_material_instance_vector_parameter_value(a, 'Color')
    if KITM and a.get_editor_property('parent') != KITM:
        mel.set_material_instance_parent(a, KITM); log(name + ' -> M_Surface')
    mel.set_material_instance_scalar_parameter_value(a, 'PuddleMask', 0.0)   # kit pieces (aircraft, walls) never puddle
    mel.update_material_instance(a); eal.save_loaded_asset(a, only_if_is_dirty=False)
# the airfield's painted markings: their own paint on the master, carrying the pavement's own cracks straight through them
# (owner, 2026-11-13: cracks that cross a marking show on it, in their own colours) and a light grime that differs on
# every marking (Airfield gives each one its own amount in the vertex colour)
for name, col in (('MI_paintyellow', PAINT['MI_yellow']), ('MI_paintwhite', PAINT['MI_white']), ('MI_paintblack', unreal.LinearColor(0.018, 0.018, 0.018, 1))):
    inst(KIT + name, dict(Color=col, UseScan=0.0, CrackStyle=0.0, CrackAmount=0.7, TyreAmount=1.0, Roughness=0.55 if name != 'MI_paintblack' else 0.75, Variation=0.03, PaintDirt=1.0),
         textures=dict(CrackLayer=LAYER['cracks_asphalt']))
BAB = '/Game/Babylon/Terminal/terminal/Materials/'
# the model's building materials stay the model's own (owner, 2026-11-13: buildings never share the ground's kit): its
# concrete is the terminal's floor slabs, the baggage hall, the kerb and the tower's base; the rental's concrete is the
# parking garage's decks, walls and pillars - back on the model's own master, their pictures and settings as imported
ORIG = unreal.load_asset(BAB + 'rentalTerrazzo')
ORIG = ORIG.get_editor_property('parent') if ORIG else None
for name in ('concrete', 'rentalConcrete', 'rentalConcreteLight'):
    a = unreal.load_asset(BAB + name)
    if a and ORIG and a.get_editor_property('parent') != ORIG:
        mel.set_material_instance_parent(a, ORIG); mel.update_material_instance(a); eal.save_loaded_asset(a, only_if_is_dirty=False); log(name + ' -> its own master again')
# the model's ground only: its road, apron and car park
for name, extra in (('apron', dict(CrackStyle=1.0, CrackAmount=0.5, PuddleBias=0.12)), ('asphalt', dict(CrackStyle=0.0, CrackAmount=0.5)),
                    ('rentalAsphalt', dict(CrackStyle=0.0, CrackAmount=0.35, PuddleBias=0.05))):
    a = unreal.load_asset(BAB + name)
    if a is None: log('missing ' + name); continue
    try:
        btex = mel.get_material_instance_texture_parameter_value(a, 'BaseColorTexture')
        os_ = mel.get_material_instance_vector_parameter_value(a, 'BaseColorTexture_OffsetScale')
        bcf = mel.get_material_instance_vector_parameter_value(a, 'BaseColorFactor')
        rf = mel.get_material_instance_scalar_parameter_value(a, 'RoughnessFactor')
        if btex is None: raise Exception('ours already')
    except Exception:
        btex = mel.get_material_instance_texture_parameter_value(a, 'BaseTex')
        os_ = unreal.LinearColor(0, 0, mel.get_material_instance_scalar_parameter_value(a, 'TexU'), mel.get_material_instance_scalar_parameter_value(a, 'TexV'))
        bcf = mel.get_material_instance_vector_parameter_value(a, 'Color'); rf = mel.get_material_instance_scalar_parameter_value(a, 'Roughness')
    p = dict(Color=bcf, Roughness=rf, TexU=os_.b, TexV=os_.a, UseScan=0.0, TyreAmount=0.0); p.update(extra)
    inst(BAB + name, p, btex, True, dict(CrackLayer=LAYER['cracks_concrete' if extra['CrackStyle'] > 0.5 else 'cracks_asphalt']))
log('DONE')
unreal.SystemLibrary.quit_editor()
