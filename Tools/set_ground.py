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
PARAMS = (('Wetness', 0.0), ('PuddleAmount', 0.55), ('PuddleSize', 3.0), ('PuddleShine', 0.9), ('Ripples', 1.0), ('Rain', 0.0), ('Cracks', 1.0))
mpc = unreal.load_asset(MPC)
if mpc is None:
    mpc = tools.create_asset('MPC_Weather', '/Game/Weather', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
have = {str(p.get_editor_property('parameter_name')) for p in mpc.get_editor_property('scalar_parameters')}
sps = list(mpc.get_editor_property('scalar_parameters'))
for n, v in PARAMS:
    if n not in have:
        p = unreal.CollectionScalarParameter(); p.set_editor_property('parameter_name', n); p.set_editor_property('default_value', v); sps.append(p)
mpc.set_editor_property('scalar_parameters', sps); eal.save_loaded_asset(mpc, only_if_is_dirty=False)

# ---- the master
MP = '/Game/Materials/Ground/M_Ground'
if eal.does_asset_exist(MP): eal.delete_asset(MP)
m = tools.create_asset('M_Ground', '/Game/Materials/Ground', unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('tangent_space_normal', False)      # world-space normals (the airfield's procedural sheets have no tangents)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
def sp(name, v, x, y):
    e = E(unreal.MaterialExpressionScalarParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', v); return e
def vp(name, c, x, y):
    e = E(unreal.MaterialExpressionVectorParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', c); return e
def cp(name, y):
    c = E(unreal.MaterialExpressionCollectionParameter, -1400, y); c.set_editor_property('collection', mpc); c.set_editor_property('parameter_name', name); return c

# its own look: the kit's colour with a gentle variation, or a tiled picture
col = vp('Color', unreal.LinearColor(0.5, 0.5, 0.5, 1), -1400, -600)
var = sp('Variation', 0.15, -1400, -500)
noise = E(unreal.MaterialExpressionNoise, -1200, -500); noise.set_editor_property('scale', 0.02); noise.set_editor_property('levels', 4)
noise.set_editor_property('output_min', -1.0); noise.set_editor_property('output_max', 1.0)
nv = E(unreal.MaterialExpressionMultiply, -1000, -500); mel.connect_material_expressions(noise, '', nv, 'A'); mel.connect_material_expressions(var, '', nv, 'B')
one = E(unreal.MaterialExpressionConstant, -1000, -420); one.set_editor_property('r', 1.0)
add1 = E(unreal.MaterialExpressionAdd, -850, -480); mel.connect_material_expressions(one, '', add1, 'A'); mel.connect_material_expressions(nv, '', add1, 'B')
plain = E(unreal.MaterialExpressionMultiply, -700, -560); mel.connect_material_expressions(col, '', plain, 'A'); mel.connect_material_expressions(add1, '', plain, 'B')
uv = E(unreal.MaterialExpressionTextureCoordinate, -1400, -300)
tu = sp('TexU', 1.0, -1400, -220); tv = sp('TexV', 1.0, -1400, -160)
tuv = E(unreal.MaterialExpressionAppendVector, -1200, -200); mel.connect_material_expressions(tu, '', tuv, 'A'); mel.connect_material_expressions(tv, '', tuv, 'B')
uvs = E(unreal.MaterialExpressionMultiply, -1050, -280); mel.connect_material_expressions(uv, '', uvs, 'A'); mel.connect_material_expressions(tuv, '', uvs, 'B')
tex = E(unreal.MaterialExpressionTextureSampleParameter2D, -900, -300); tex.set_editor_property('parameter_name', 'BaseTex')
tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
mel.connect_material_expressions(uvs, '', tex, 'UVs')
texc = E(unreal.MaterialExpressionMultiply, -700, -300); mel.connect_material_expressions(tex, 'RGB', texc, 'A'); mel.connect_material_expressions(col, '', texc, 'B')
use = E(unreal.MaterialExpressionStaticSwitchParameter, -500, -450); use.set_editor_property('parameter_name', 'UseTexture'); use.set_editor_property('default_value', False)
mel.connect_material_expressions(texc, '', use, 'True'); mel.connect_material_expressions(plain, '', use, 'False')
rough = sp('Roughness', 0.9, -1400, -80); metal = sp('Metallic', 0.0, -1400, -20)
mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)

# the surface's own settings for the effects
bias = sp('PuddleBias', 0.0, -1400, 60); pon = sp('Puddles', 1.0, -1400, 120)
cstyle = sp('CrackStyle', 0.0, -1400, 180); camt = sp('CrackAmount', 0.6, -1400, 240)
cscale = sp('CrackScale', 3.0, -1400, 300); cwidth = sp('CrackWidth', 1.5, -1400, 360)
wp = E(unreal.MaterialExpressionWorldPosition, -1400, 420)
vn = E(unreal.MaterialExpressionVertexNormalWS, -1400, 480)
tm = E(unreal.MaterialExpressionTime, -1400, 540)
cps = {n: cp(n, 600 + 60 * i) for i, (n, _) in enumerate(PARAMS)}

COMMON = '''
float2 M = WP.xy / 100.0;                      // metres
float up = saturate((N.z - 0.92) / 0.06);
// ---- puddle field: fractal value noise in world space
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
float edge = (1.0 - saturate(Amount + Bias) * 0.6) + (1.0 - saturate(Wet)) * 0.30;   // drying raises the bar: shrinking from the edge
float pmask = up * saturate(Puddles) * step(0.02, Wet);
float puddle = smoothstep(edge, edge + 0.06, n) * pmask;                            // soft water's edge
float rim = smoothstep(edge - 0.10, edge, n) * pmask * saturate(Wet);               // the darker wet ground round it
// ---- cracks: a warped world-space Voronoi network, patchy; concrete also has sawn slab joints
float2 w2 = M * 0.45; float wn = 0, wa = 0.5;
[unroll] for (int j = 0; j < 3; j++) {
  float2 ip = floor(w2), f = frac(w2); f = f * f * (3 - 2 * f);
  float h00 = frac(sin(dot(ip, float2(41.3, 289.1))) * 15731.743);
  float h10 = frac(sin(dot(ip + float2(1, 0), float2(41.3, 289.1))) * 15731.743);
  float h01 = frac(sin(dot(ip + float2(0, 1), float2(41.3, 289.1))) * 15731.743);
  float h11 = frac(sin(dot(ip + float2(1, 1), float2(41.3, 289.1))) * 15731.743);
  wn += wa * lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y); w2 = w2 * 2.07 + 3.1; wa *= 0.5;
}
float2 pw = M / max(CScale, 0.3) + (wn - 0.45) * 0.8;
float2 ic = floor(pw), fc = frac(pw); float d1 = 8, d2 = 8;
[unroll] for (int y = -1; y <= 1; y++) [unroll] for (int x = -1; x <= 1; x++) {
  float2 g = float2(x, y);
  float2 o = frac(sin(float2(dot(ic + g, float2(127.1, 311.7)), dot(ic + g, float2(269.5, 183.3)))) * 43758.5453);
  float d = length(g + o - fc);
  if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) { d2 = d; }
}
float thr = CWidth * 0.01 / max(CScale, 0.3);
float crackLine = 1.0 - smoothstep(thr * 0.4, thr, d2 - d1);
float patch = saturate((wn - (1.0 - saturate(CAmount * Cracks) * 0.75)) * 5.0);
float isConcrete = step(0.5, Style) * (1.0 - step(1.5, Style));
float none = step(1.5, Style);
float crack = crackLine * patch * up * (1.0 - none);
float2 gj = abs(frac(M / 5.0 + 0.5) - 0.5) * 5.0;
float joint = (1.0 - smoothstep(0.004, 0.011, min(gj.x, gj.y))) * isConcrete * up * step(0.01, CAmount * Cracks);
float crackAll = saturate(crack + joint);
'''
SURF = COMMON + '''
float wetAll = saturate(Wet) * up * (1.0 - none * 0.5);
// asphalt: sealed with black tar, a little shiny; concrete: dark cracks and joints
float3 sealC = lerp(Col * 0.32, Col * 0.5, isConcrete);
float3 c = lerp(Col, sealC, crackAll);
float r = lerp(Rough, lerp(0.45, 0.85, isConcrete), crackAll);
c *= lerp(1.0, 0.62, wetAll) * lerp(1.0, 0.82, rim) * lerp(1.0, 0.7, crackAll * wetAll);
r = lerp(r, 0.30, wetAll);
c = lerp(c, Col * 0.22, puddle);                   // under water the ground goes dark
r = lerp(r, lerp(0.10, 0.02, saturate(Shine)), puddle);
return float4(c, r);
'''
NORMAL = COMMON + '''
// ripples: drops landing in the puddles while it rains; a faint stir from the wind otherwise
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
float stir = (frac(sin(dot(floor(WP.xy / 6.0), float2(12.9898, 78.233)) + T * 0.2) * 43758.5453) - 0.5) * 0.004;
float2 tilt = (acc * 0.35 * saturate(Ripples) * saturate(Rain) + stir) * puddle;
// the cracks sit a little below the surface (catch the light at their edges)
float2 cg = float2(ddx(crackAll), ddy(crackAll)) * 0.0;
return normalize(N + float3(tilt, 0.0));
'''
REFL = COMMON + '''
// how much of the sky the water shows (the colour comes from the sky in the reflected direction)
float wetAll = saturate(Wet) * up;
return saturate(puddle * lerp(0.35, 0.8, saturate(Shine)) + wetAll * 0.05);
'''
ALL = ['WP', 'N', 'Bias', 'Puddles', 'Wet', 'Amount', 'Size', 'Style', 'CAmount', 'CScale', 'CWidth', 'Cracks']
def custom(code, out_type, x, y, extra):
    c = E(unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out_type); c.set_editor_property('description', 'GROUND')
    ins = []
    for n_ in extra + ALL:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n_); ins.append(ci)
    c.set_editor_property('inputs', ins)
    src = {'WP': wp, 'N': vn, 'Bias': bias, 'Puddles': pon, 'Wet': cps['Wetness'], 'Amount': cps['PuddleAmount'], 'Size': cps['PuddleSize'],
           'Style': cstyle, 'CAmount': camt, 'CScale': cscale, 'CWidth': cwidth, 'Cracks': cps['Cracks'],
           'Col': use, 'Rough': rough, 'Shine': cps['PuddleShine'], 'T': tm, 'Ripples': cps['Ripples'], 'Rain': cps['Rain']}
    for n_ in extra + ALL: mel.connect_material_expressions(src[n_], '', c, n_)
    return c
cs = custom(SURF, unreal.CustomMaterialOutputType.CMOT_FLOAT4, -300, 0, ['Col', 'Rough', 'Shine'])
cn = custom(NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -300, 400, ['T', 'Ripples', 'Rain'])
cr = custom(REFL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -300, 800, ['Shine'])
rgb = E(unreal.MaterialExpressionComponentMask, -50, 0)
for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)): rgb.set_editor_property(ch, on)
aa = E(unreal.MaterialExpressionComponentMask, -50, 100)
for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)): aa.set_editor_property(ch, on)
mel.connect_material_expressions(cs, '', rgb, ''); mel.connect_material_expressions(cs, '', aa, '')
mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(aa, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(cn, '', unreal.MaterialProperty.MP_NORMAL)
# the sky in the water: the sky atmosphere's colour in the reflected direction, by Fresnel (the game's sky light gives
# Lumen's reflections no sky yet; when it does, this can be lowered)
rv = E(unreal.MaterialExpressionReflectionVectorWS, -300, 1000)
sky = E(unreal.MaterialExpressionSkyAtmosphereViewLuminance, -100, 1000); mel.connect_material_expressions(rv, '', sky, 'WorldDirection')
fr = E(unreal.MaterialExpressionFresnel, -100, 1100); fr.set_editor_property('exponent', 4.0); fr.set_editor_property('base_reflect_fraction', 0.04)
m1 = E(unreal.MaterialExpressionMultiply, 100, 1000); mel.connect_material_expressions(sky, '', m1, 'A'); mel.connect_material_expressions(fr, '', m1, 'B')
m2 = E(unreal.MaterialExpressionMultiply, 250, 1000); mel.connect_material_expressions(m1, '', m2, 'A'); mel.connect_material_expressions(cr, '', m2, 'B')
mel.connect_material_property(m2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('master ' + m.get_path_name())

# ---- the instances: every outdoor paved surface
def inst(path, params, tex=None, switch_tex=False):
    a = unreal.load_asset(path)
    if a is None: log('missing ' + path); return
    if not isinstance(a, unreal.MaterialInstanceConstant): log('not an instance ' + path); return
    mel.set_material_instance_parent(a, m)
    mel.set_material_instance_static_switch_parameter_value(a, 'UseTexture', switch_tex)
    if tex: mel.set_material_instance_texture_parameter_value(a, 'BaseTex', tex)
    for k, v in params.items():
        if isinstance(v, unreal.LinearColor): mel.set_material_instance_vector_parameter_value(a, k, v)
        else: mel.set_material_instance_scalar_parameter_value(a, k, v)
    mel.update_material_instance(a); eal.save_loaded_asset(a, only_if_is_dirty=False)
    log('%s -> M_Ground %s' % (path.split('/')[-1], params))

KIT = '/Game/Materials/Kit/'
def kitp(name):   # keep the kit instance's own colour, variation and roughness
    a = unreal.load_asset(KIT + name)
    out = {}
    for k in ('Color',):
        try: out[k] = mel.get_material_instance_vector_parameter_value(a, k)
        except Exception: pass
    for k in ('Variation', 'Roughness'):
        try: out[k] = mel.get_material_instance_scalar_parameter_value(a, k)
        except Exception: pass
    return out
for name, extra in (('MI_concrete', dict(CrackStyle=1.0, CrackAmount=0.6, CrackScale=2.5, CrackWidth=0.8, PuddleBias=0.0)),
                    ('MI_asphalt', dict(CrackStyle=0.0, CrackAmount=0.55, CrackScale=3.5, CrackWidth=2.2, PuddleBias=0.0)),
                    ('MI_apron', dict(CrackStyle=1.0, CrackAmount=0.6, CrackScale=2.5, CrackWidth=0.8, PuddleBias=0.12)),
                    ('MI_yellow', dict(CrackStyle=2.0, PuddleBias=0.0)),
                    ('MI_white', dict(CrackStyle=2.0, PuddleBias=0.0))):
    p = kitp(name); p.update(extra); inst(KIT + name, p)

# the terminal model's ground (Babylon): their own pictures and tiling kept exactly (BaseColorTexture, OffsetScale's
# z/w), now in the master
BAB = '/Game/Babylon/Terminal/terminal/Materials/'
for name, extra in (('apron', dict(CrackStyle=1.0, CrackAmount=0.6, CrackScale=2.5, CrackWidth=0.8, PuddleBias=0.12)),
                    ('concrete', dict(CrackStyle=1.0, CrackAmount=0.5, CrackScale=2.5, CrackWidth=0.8, PuddleBias=0.0)),
                    ('asphalt', dict(CrackStyle=0.0, CrackAmount=0.55, CrackScale=3.5, CrackWidth=2.2, PuddleBias=0.0)),
                    ('rentalAsphalt', dict(CrackStyle=0.0, CrackAmount=0.4, CrackScale=3.5, CrackWidth=2.0, PuddleBias=0.05)),
                    ('rentalConcrete', dict(CrackStyle=1.0, CrackAmount=0.4, CrackScale=2.5, CrackWidth=0.8, PuddleBias=0.05)),
                    ('rentalConcreteLight', dict(CrackStyle=1.0, CrackAmount=0.4, CrackScale=2.5, CrackWidth=0.8, PuddleBias=0.05))):
    a = unreal.load_asset(BAB + name)
    if a is None: log('missing ' + name); continue
    try:
        btex = mel.get_material_instance_texture_parameter_value(a, 'BaseColorTexture')
        os_ = mel.get_material_instance_vector_parameter_value(a, 'BaseColorTexture_OffsetScale')
        bcf = mel.get_material_instance_vector_parameter_value(a, 'BaseColorFactor')
        rf = mel.get_material_instance_scalar_parameter_value(a, 'RoughnessFactor')
    except Exception as e:
        # already ours (the tool ran before): keep what it set
        btex = mel.get_material_instance_texture_parameter_value(a, 'BaseTex')
        os_ = unreal.LinearColor(0, 0, mel.get_material_instance_scalar_parameter_value(a, 'TexU'), mel.get_material_instance_scalar_parameter_value(a, 'TexV'))
        bcf = mel.get_material_instance_vector_parameter_value(a, 'Color'); rf = mel.get_material_instance_scalar_parameter_value(a, 'Roughness')
    p = dict(Color=bcf, Roughness=rf, TexU=os_.b, TexV=os_.a); p.update(extra)
    inst(BAB + name, p, btex, True)
log('DONE')
unreal.SystemLibrary.quit_editor()
