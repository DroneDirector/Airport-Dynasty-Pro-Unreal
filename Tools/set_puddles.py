# Puddles and wet ground, the fast way (owner, 2026-11-13, HANDOVER.md "NEWEST"): like The Matrix Awakens' city, the
# puddles are part of the ground's own material, so every pavement piece has them the moment it exists - nothing
# placed, nothing floating.
#   /Game/Weather/MPC_Weather: one global wetness (0 dry .. 1 soaked) and the puddle tuning (amount, size, shine,
#     ripples, rain) - set by the game (APuddles: the Rainy day switch, the tuning panel, later the real rain)
#   /Game/Materials/Kit/M_Surface (every kit surface): a PuddleMask parameter, 0 by default so walls and floors indoors
#     never get wet; the outdoor ground instances (concrete, asphalt, apron, the markings) set it to 1. Where it's on:
#     the ground darkens and gets glossy as it gets wet; puddles in world-space spots (random, never repeating, more on
#     aprons by their own mask), only on flat up-facing ground, a darker damp rim round each, a mirror-smooth water
#     surface (Lumen reflects the sky, the lights, the aircraft), rain ripples while it rains; drying shrinks each
#     puddle from its edge in.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_puddles.py" -unattended -nullrhi
import unreal

eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_PUDDLES ' + str(m))

# ---- the weather's parameters
MPC = '/Game/Weather/MPC_Weather'
PARAMS = (('Wetness', 0.0), ('PuddleAmount', 0.55), ('PuddleSize', 3.0), ('PuddleShine', 0.9), ('Ripples', 1.0), ('Rain', 0.0))
if eal.does_asset_exist(MPC): eal.delete_asset(MPC)
mpc = tools.create_asset('MPC_Weather', '/Game/Weather', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
sps = []
for n, v in PARAMS:
    p = unreal.CollectionScalarParameter(); p.set_editor_property('parameter_name', n); p.set_editor_property('default_value', v); sps.append(p)
mpc.set_editor_property('scalar_parameters', sps)
eal.save_loaded_asset(mpc, only_if_is_dirty=False)
log('collection %s' % [n for n, _ in PARAMS])

# ---- the surface material
m = unreal.load_asset('/Game/Materials/Kit/M_Surface')
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
# the original colour and roughness (taken from under our nodes if this tool ran before)
bc = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_BASE_COLOR)
rg = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_ROUGHNESS)
old = []
if isinstance(bc, unreal.MaterialExpressionStaticSwitchParameter):    # the switch from before: under it, our output
    sw_b, sw_r = bc, rg
    bc = mel.get_inputs_for_material_expression(m, sw_b)[1]    # (A = on: ours, B = off: the original)
    rg_orig = mel.get_inputs_for_material_expression(m, sw_r)[1]
    for e in (sw_b, sw_r, mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_NORMAL)):
        if e: mel.delete_material_expression(m, e)
    bc_orig = bc; bc = mel.get_inputs_for_material_expression(m, sw_b)[1] if False else bc
    rg = rg_orig
    # bc is now the original colour already (the switch's B input)
if isinstance(bc, unreal.MaterialExpressionComponentMask):          # our output split from before
    cust = mel.get_inputs_for_material_expression(m, bc)[0]
    ins = mel.get_inputs_for_material_expression(m, cust)
    old += [bc, cust, mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_NORMAL)]
    old.append(mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_ROUGHNESS))
    bc, rg = ins[0], ins[1]
    for e in list(mel.get_inputs_for_material_expression(m, cust))[2:]: old.append(e)
    nrm_c = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_NORMAL)
    if nrm_c: old += list(mel.get_inputs_for_material_expression(m, nrm_c))
    for e in old:
        if e and not isinstance(e, (unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter, unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd)):
            try: mel.delete_material_expression(m, e)
            except Exception: pass
log('original colour %s, roughness %s' % (bc.get_class().get_name(), rg.get_class().get_name()))

mask = E(unreal.MaterialExpressionScalarParameter, -900, 700); mask.set_editor_property('parameter_name', 'PuddleMask'); mask.set_editor_property('default_value', 0.0)
bias = E(unreal.MaterialExpressionScalarParameter, -900, 760); bias.set_editor_property('parameter_name', 'PuddleBias'); bias.set_editor_property('default_value', 0.0)
wp = E(unreal.MaterialExpressionWorldPosition, -900, 820)
vn = E(unreal.MaterialExpressionVertexNormalWS, -900, 880)
tm = E(unreal.MaterialExpressionTime, -900, 940)
def cp(name, y):
    c = E(unreal.MaterialExpressionCollectionParameter, -900, y); c.set_editor_property('collection', mpc); c.set_editor_property('parameter_name', name); return c
cps = {n: cp(n, 1000 + 60 * i) for i, (n, _) in enumerate(PARAMS)}

NOISE = '''
float2 q = WP.xy / max(Size * 100.0, 10.0);
float n = 0, a = 0.55;
[unroll] for (int i = 0; i < 3; i++) {
  float2 ip = floor(q), f = frac(q); f = f * f * (3 - 2 * f);
  float h00 = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453);
  float h10 = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
  float h01 = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
  float h11 = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
  n += a * lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y); q = q * 2.03 + 17.0; a *= 0.5;
}
n = n / 1.0;
float up = saturate((N.z - 0.92) / 0.06);
float m_ = saturate(Mask);
float edge = (1.0 - saturate(Amount + Bias) * 0.6) + (1.0 - saturate(Wet)) * 0.30;   // drying raises the bar: the puddle shrinks from its edge
float puddle = saturate((n - edge) / 0.035) * up * m_ * step(0.02, Wet);
float rim = saturate((n - edge + 0.07) / 0.07) * up * m_ * Wet;
'''
SURF = NOISE + '''
float wetAll = saturate(Wet) * m_;
float3 c = Col * lerp(1.0, 0.6, wetAll) * lerp(1.0, 0.8, rim);
c = lerp(c, Col * 0.42, puddle);
float r = lerp(Rough, 0.32, wetAll);
r = lerp(r, lerp(0.14, 0.015, saturate(Shine)), puddle);
return float4(c, r);
'''
NORMAL = NOISE + '''
float2 uv = WP.xy / 28.0;
float2 acc = 0;
[unroll] for (int k = 0; k < 3; k++) {
  float2 g = uv + float2(k * 0.37, k * 0.71);
  float2 cell = floor(g); float2 f = frac(g) - 0.5;
  float h = frac(sin(dot(cell + k * 7.0, float2(12.9898, 78.233))) * 43758.5453);
  float2 off = (float2(frac(h * 7.13), frac(h * 3.71)) - 0.5) * 0.6;
  float2 d = f - off; float dist = length(d) + 1e-4;
  float t = frac(T * 0.9 + h);
  float wave = sin((dist - t * 0.45) * 70.0) * saturate(1.0 - t) * saturate(1.0 - abs(dist - t * 0.45) * 14.0);
  acc += (d / dist) * wave;
}
float2 tilt = acc * 0.35 * saturate(Ripples) * saturate(Rain) * puddle;
return normalize(N + float3(tilt, 0.0));   // world space: the ground's own normal, tilted by the ripples
'''
def custom(code, out_type, x, y, names):
    c = E(unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out_type); c.set_editor_property('description', 'PUDDLES')
    ins = []
    for n in names:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
    c.set_editor_property('inputs', ins)
    return c
cs = custom(SURF, unreal.CustomMaterialOutputType.CMOT_FLOAT4, -400, 600, ['Col', 'Rough', 'WP', 'N', 'Mask', 'Bias', 'Wet', 'Amount', 'Size', 'Shine'])
mel.connect_material_expressions(bc, '', cs, 'Col'); mel.connect_material_expressions(rg, '', cs, 'Rough')
mel.connect_material_expressions(wp, '', cs, 'WP'); mel.connect_material_expressions(vn, '', cs, 'N')
mel.connect_material_expressions(mask, '', cs, 'Mask'); mel.connect_material_expressions(bias, '', cs, 'Bias')
mel.connect_material_expressions(cps['Wetness'], '', cs, 'Wet'); mel.connect_material_expressions(cps['PuddleAmount'], '', cs, 'Amount')
mel.connect_material_expressions(cps['PuddleSize'], '', cs, 'Size'); mel.connect_material_expressions(cps['PuddleShine'], '', cs, 'Shine')
cn = custom(NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -400, 900, ['WP', 'N', 'Mask', 'Bias', 'Wet', 'Amount', 'Size', 'T', 'Ripples', 'Rain'])
mel.connect_material_expressions(wp, '', cn, 'WP'); mel.connect_material_expressions(vn, '', cn, 'N')
mel.connect_material_expressions(mask, '', cn, 'Mask'); mel.connect_material_expressions(bias, '', cn, 'Bias')
mel.connect_material_expressions(cps['Wetness'], '', cn, 'Wet'); mel.connect_material_expressions(cps['PuddleAmount'], '', cn, 'Amount')
mel.connect_material_expressions(cps['PuddleSize'], '', cn, 'Size'); mel.connect_material_expressions(tm, '', cn, 'T')
mel.connect_material_expressions(cps['Ripples'], '', cn, 'Ripples'); mel.connect_material_expressions(cps['Rain'], '', cn, 'Rain')
rgb = E(unreal.MaterialExpressionComponentMask, -150, 600); rgb.set_editor_property('r', True); rgb.set_editor_property('g', True); rgb.set_editor_property('b', True); rgb.set_editor_property('a', False)
aa = E(unreal.MaterialExpressionComponentMask, -150, 680); aa.set_editor_property('r', False); aa.set_editor_property('g', False); aa.set_editor_property('b', False); aa.set_editor_property('a', True)
mel.connect_material_expressions(cs, '', rgb, ''); mel.connect_material_expressions(cs, '', aa, '')
# a static switch: only the outdoor ground's instances compile the puddle code at all (indoor surfaces - the whole
# terminal kit - stay as cheap as before; with the code everywhere the GPU crawled)
def switch(on, off, x, y):
    s_ = E(unreal.MaterialExpressionStaticSwitchParameter, x, y); s_.set_editor_property('parameter_name', 'Puddles'); s_.set_editor_property('default_value', False)
    mel.connect_material_expressions(on, '', s_, 'True'); mel.connect_material_expressions(off, '', s_, 'False'); return s_
flat = vn   # outdoors without puddles: the vertex normal itself (world space)
mel.connect_material_property(switch(rgb, bc, 50, 600), '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(switch(aa, rg, 50, 700), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(switch(cn, flat, 50, 900), '', unreal.MaterialProperty.MP_NORMAL)
# the sky in the water (the game's sky light gives reflections no sky yet, so the puddles reflect the real sky
# themselves): the sky atmosphere's own colour in the reflected direction, by the water's Fresnel, on the puddles (and
# a little on the wet ground round them)
PUD = NOISE + '''
float wetAll = saturate(Wet) * m_;
return saturate(puddle * lerp(0.6, 1.0, saturate(Shine)) + wetAll * 0.08);
'''
cp_ = custom(PUD, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -400, 1200, ['WP', 'N', 'Mask', 'Bias', 'Wet', 'Amount', 'Size', 'Shine'])
for a_, b_ in ((wp, 'WP'), (vn, 'N'), (mask, 'Mask'), (bias, 'Bias'), (cps['Wetness'], 'Wet'), (cps['PuddleAmount'], 'Amount'), (cps['PuddleSize'], 'Size'), (cps['PuddleShine'], 'Shine')):
    mel.connect_material_expressions(a_, '', cp_, b_)
rv = E(unreal.MaterialExpressionReflectionVectorWS, -400, 1350)
sky = E(unreal.MaterialExpressionSkyAtmosphereViewLuminance, -200, 1350)
mel.connect_material_expressions(rv, '', sky, 'WorldDirection')
fr = E(unreal.MaterialExpressionFresnel, -200, 1450); fr.set_editor_property('exponent', 5.0); fr.set_editor_property('base_reflect_fraction', 0.02)
m1 = E(unreal.MaterialExpressionMultiply, 0, 1350); mel.connect_material_expressions(sky, '', m1, 'A'); mel.connect_material_expressions(fr, '', m1, 'B')
m2 = E(unreal.MaterialExpressionMultiply, 150, 1350); mel.connect_material_expressions(m1, '', m2, 'A'); mel.connect_material_expressions(cp_, '', m2, 'B')
zero = E(unreal.MaterialExpressionConstant3Vector, 150, 1450); zero.set_editor_property('constant', unreal.LinearColor(0, 0, 0, 1))
mel.connect_material_property(switch(m2, zero, 300, 1350), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
# normals in world space (the airfield's sheets are procedural meshes without tangents: a tangent-space normal came
# out wrong there and spoilt the reflections)
m.set_editor_property('tangent_space_normal', False)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('surface material: puddles in')

# ---- the outdoor ground: puddles on (aprons a little more: ground crews' standing water by the stands)
for name, bias_v in (('concrete', 0.0), ('asphalt', 0.0), ('apron', 0.12), ('yellow', 0.0), ('white', 0.0)):
    mi = unreal.load_asset('/Game/Materials/Kit/MI_' + name)
    if not mi: log('no MI_' + name); continue
    mel.set_material_instance_static_switch_parameter_value(mi, 'Puddles', True)
    mel.set_material_instance_scalar_parameter_value(mi, 'PuddleMask', 1.0)
    mel.set_material_instance_scalar_parameter_value(mi, 'PuddleBias', bias_v)
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    log('MI_%s: puddles (bias %.2f)' % (name, bias_v))
log('DONE')
unreal.SystemLibrary.quit_editor()
