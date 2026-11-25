# Rain on the buildings' outside glass (to-do 295 part B, docs/Glass-Rain-Plan.md): the drops are Tools/make_glass_rain.py's.
#  - /Game/Materials/Glass/MF_BuildingRain: wraps a glass material's own outputs; behind the static switch RainGlass (off: nothing
#    at all). Works in each instance's frame without its scale; outside is the side that is open to the sky: RainRoof, the
#    building's own picture of what stands above each place (cm, made in the game: GlassRain.cpp), read 90 cm before the pane.
#  - M_BuildingGlass (the terminal) and M_Glass (tower, hangar) get one call of it, nothing else of theirs changes.
#  - M_BabylonGlassRain + MI_Rain_*: copies of the jetways' and the fire station's imported glass with the call (the game swaps them in).
# May be run again at any time; MUST be run after Tools/set_building_glass.py (which rebuilds M_BuildingGlass and its instances).
import os, re, stat, unreal
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MP = unreal.MaterialProperty; FT = unreal.FunctionInputType; O = unreal.CustomMaterialOutputType; DIR = '/Game/Materials/Glass'


def log(*a): unreal.log('ADP_BUILDINGRAIN ' + ' '.join(str(x) for x in a))


def disk(p): return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), p.replace('/Game/', '') + '.uasset')


def opened(p, cls=None, factory=None):
    if os.path.exists(disk(p)): os.chmod(disk(p), stat.S_IWRITE | stat.S_IREAD)   # (write-protected after an LFS commit)
    if eal.does_asset_exist(p) or not cls: return unreal.load_asset(p)
    d, n = p.rsplit('/', 1); return at.create_asset(n, d, cls, factory())


def save(a, p):
    was = os.path.getmtime(disk(p)) if os.path.exists(disk(p)) else 0
    ok = eal.save_loaded_asset(a, only_if_is_dirty=False)
    log('saved', p, 'ok' if ok and os.path.exists(disk(p)) and os.path.getmtime(disk(p)) > was else 'FAILED')


def pixels(m):
    mel.recompile_material(m); s = mel.get_statistics(m); return '%d pixel instructions, %d samplers' % (s.num_pixel_shader_instructions, s.num_samplers)


def enum(en, n): return next(getattr(en, k) for k in dir(en) if k.upper() == n)


# #region ---- what the materials had (read before the function is made again: its calls lose their wires then)
PROPS = ((MP.MP_BASE_COLOR, 'OwnColor', 'Color'), (MP.MP_OPACITY, 'OwnOpacity', 'Opacity'), (MP.MP_ROUGHNESS, 'OwnRoughness', 'Roughness'), (MP.MP_SPECULAR, 'OwnSpecular', 'Specular'), (MP.MP_NORMAL, 'OwnNormal', 'Normal'))
BM = DIR + '/M_BabylonGlassRain'
if not eal.does_asset_exist(BM): eal.duplicate_asset('/Game/Babylon/Masters/M_Default', BM); save(unreal.load_asset(BM), BM)
HAD = {}
for path in ('/Game/Terminal/Masters/M_BuildingGlass', '/Game/Materials/M_Glass', BM):
    m = opened(path); c = mel.get_material_property_input_node(m, MP.MP_OPACITY); r = []
    if isinstance(c, unreal.MaterialExpressionMaterialFunctionCall) and c.get_editor_property('material_function').get_path_name().startswith(DIR + '/MF_BuildingRain.'):
        by = dict(zip([str(x) for x in mel.get_material_expression_input_names(c)], mel.get_inputs_for_material_expression(m, c)))
        for p, i, o in PROPS:
            n = by.get(i) or next((v for k, v in by.items() if k.split(' ')[0] == i), None)
            r.append((n, mel.get_input_node_output_name_for_material_expression(c, n) if n else ''))
        # (one node feeding several inputs - the imported master's - wears the property's own names)
        if path == BM: r = [(n, 'BaseColor' if o == 'Color' else o) for (n, _), (p, i, o) in zip(r, PROPS)]
    else:
        c = None
        for p, i, o in PROPS:
            n = mel.get_material_property_input_node(m, p); r.append((n, mel.get_material_property_input_node_output_name(m, p) if n else ''))
    if not r[0][0] or not r[1][0]:   # (a function remade without this care: the nodes are still there by name)
        k = {'M_BuildingGlass': ('Multiply_2', 'ComponentMask_4', 'ScalarParameter_14', '', ''), 'M_Glass': ('VectorParameter_0', 'ScalarParameter_0', 'ScalarParameter_1', 'ScalarParameter_2', ''),
             'M_BabylonGlassRain': ('MaterialFunctionCall_2',) * 5}[path.rsplit('/', 1)[1]]
        outs = {'M_Glass': ('RGB', '', '', '', ''), 'M_BabylonGlassRain': ('BaseColor', 'Opacity', 'Roughness', 'Specular', 'Normal')}.get(path.rsplit('/', 1)[1], ('',) * 5)
        r = [(unreal.find_object(m, 'MaterialExpression' + n) if n else None, o) for n, o in zip(k, outs)]
        log(path, 'wires by name', by if c else '', [x[0].get_editor_property('parameter_name') if x[0] and hasattr(x[0], 'parameter_name') else str(x[0] and x[0].get_name()) for x in r])
    if not r[0][0] or not r[1][0]: raise RuntimeError('%s has lost what it had: nothing changed' % path)
    HAD[path] = (m, c, r); log(path, 'call' if c else 'no call yet', [(n.get_name() if n else '-', o) for n, o in r])
# #endregion

# #region ---- the function
RAIN = re.search(r"RAIN = r'''(.*?)'''", open(os.path.join(ROOT, 'Tools', 'make_glass_rain.py'), encoding='utf-8').read(), re.S).group(1)
MFP = DIR + '/MF_BuildingRain'
mf = opened(MFP, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew)
for _ in range(12):   # (one call takes out only every other node)
    if mel.get_num_material_expressions_in_function(mf) == 0: break
    mel.delete_all_material_expressions_in_function(mf)
log('old function nodes left:', mel.get_num_material_expressions_in_function(mf))
mf.set_editor_property('description', 'Rain on a building pane (to-do 295, docs/Glass-Rain-Plan.md)')


def fe(cls, x, y, **kw):
    e = mel.create_material_expression_in_function(mf, cls, x, y)
    for k, v in kw.items(): e.set_editor_property(k, v)
    return e


def con(a, b, pin='', out=''):
    if not mel.connect_material_expressions(a, out, b, pin): raise RuntimeError('not connected: %s.%s -> %s.%s' % (a.get_name(), out, b.get_name(), pin))


def custom(code, out, ins, x, y, extra=()):
    c = fe(unreal.MaterialExpressionCustom, x, y, code=code, output_type=out, description='BUILDINGRAIN'); l = []
    for n in ins:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); l.append(ci)
    c.set_editor_property('inputs', l)
    if extra:
        ex = []
        for n, t in extra:
            co = unreal.CustomOutput(); co.set_editor_property('output_name', n); co.set_editor_property('output_type', t); ex.append(co)
        c.set_editor_property('additional_outputs', ex)
    for n, s in ins.items():
        if isinstance(s, tuple): con(s[0], c, n, s[1])
        else: con(s, c, n)
    return c


TS, TT, PS = unreal.MaterialVectorCoordTransformSource, unreal.MaterialVectorCoordTransform, unreal.MaterialPositionTransformSource


def c3(x, y, z, py): return fe(unreal.MaterialExpressionConstant3Vector, -2300, py, constant=unreal.LinearColor(x, y, z, 0))


def axis(x, y, z, py):
    t = fe(unreal.MaterialExpressionTransform, -2100, py, transform_source_type=enum(TS, 'TRANSFORMSOURCE_INSTANCE'), transform_type=enum(TT, 'TRANSFORM_WORLD'))
    con(c3(x, y, z, py), t); return t


wp = fe(unreal.MaterialExpressionWorldPosition, -2100, -600); vn = fe(unreal.MaterialExpressionVertexNormalWS, -2100, -500)
org = fe(unreal.MaterialExpressionTransformPosition, -2100, -400, transform_source_type=enum(PS, 'TRANSFORMPOSSOURCE_INSTANCE'), transform_type=enum(PS, 'TRANSFORMPOSSOURCE_WORLD'))
con(c3(0, 0, 0, -400), org)
rel = fe(unreal.MaterialExpressionSubtract, -1850, -500); con(wp, rel, 'A'); con(org, rel, 'B')
ax, ay = axis(1, 0, 0, -300), axis(0, 1, 0, -200)
tex = fe(unreal.MaterialExpressionTextureObjectParameter, -2100, 0, parameter_name='RainRoof', texture=unreal.load_asset('/Game/Terminal/Masters/T_GlassInside'),
         sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
box = fe(unreal.MaterialExpressionVectorParameter, -2100, 100, parameter_name='RainBox', default_value=unreal.LinearColor(0, 0, 0, 0))
on = fe(unreal.MaterialExpressionScalarParameter, -2100, 250, parameter_name='RainOn', default_value=0.0)
use = fe(unreal.MaterialExpressionScalarParameter, -2100, 320, parameter_name='RainRoofed', default_value=1.0)
mpc = unreal.load_asset('/Game/Weather/MPC_GlassRain')
col = [fe(unreal.MaterialExpressionCollectionParameter, -2100, 400 + 60 * i, collection=mpc, parameter_name=n) for i, n in enumerate(('GlassFill', 'GlassDry', 'GlassTime', 'GlassRun'))]
pir = fe(unreal.MaterialExpressionPerInstanceRandom, -2100, 700); dist = fe(unreal.MaterialExpressionPixelDepth, -2100, 760)
# the frame and the shelter. Axes: the instance's, without its scale. Side: 1 the pane's normal looks out, -1 in (both sides open
# or no picture: any fixed way, so a pane's two sheets wear the same drops)
FRAME = r'''
X = normalize(AX + float3(1e-5, 0, 0)); float3 ay = AY - X * dot(AY, X); Y = normalize(ay + float3(0, 1e-5, 0));
float3 Z = cross(X, Y);
float3 n = normalize(WN);
P = float3(dot(Rel, X), dot(Rel, Y), dot(Rel, Z));
G = float3(-X.z, -Y.z, -Z.z);
float side = dot(n, normalize(X + Y * 0.618 + Z * 0.382)) > 0 ? 1 : -1, open = 1;
[branch] if (Roofed > 0.5 && Fill * On > 0.002)
{
  if (abs(n.z) > 0.6)
  {
    open = smoothstep(-110, -50, WP.z - Texture2DSampleLevel(Roof, RoofSampler, saturate((WP.xy - Box.xy) * Box.zw), 0).r); side = n.z > 0 ? 1 : -1;
  }
  else
  {
    float2 q = normalize(n.xy + float2(1e-5, 0)) * 90;
    float a = smoothstep(-90, -30, WP.z - Texture2DSampleLevel(Roof, RoofSampler, saturate((WP.xy + q - Box.xy) * Box.zw), 0).r);
    float b = smoothstep(-90, -30, WP.z - Texture2DSampleLevel(Roof, RoofSampler, saturate((WP.xy - q - Box.xy) * Box.zw), 0).r);
    open = max(a, b); if (abs(a - b) > 0.2) side = a > b ? 1 : -1;
  }
}
Shelter = open * On; Side = side;
n *= side;
return float3(dot(n, X), dot(n, Y), dot(n, Z));
'''
F3, F1 = O.CMOT_FLOAT3, O.CMOT_FLOAT1
fr = custom(FRAME, F3, {'WP': wp, 'WN': vn, 'Rel': rel, 'AX': ax, 'AY': ay, 'Roof': tex, 'Box': (box, 'RGBA'), 'On': on, 'Roofed': use, 'Fill': col[0]}, -1500, -300,
            (('X', F3), ('Y', F3), ('P', F3), ('G', F3), ('Shelter', F1), ('Side', F1)))
one = fe(unreal.MaterialExpressionConstant, -1500, 300, r=1.0)
zero4 = fe(unreal.MaterialExpressionConstant4Vector, -1500, 400, constant=unreal.LinearColor(0, 0, 0, 0))
wt = fe(unreal.MaterialExpressionConstant4Vector, -1500, 500, constant=unreal.LinearColor(1000, 1000, 1.5, 0))
zero = fe(unreal.MaterialExpressionConstant, -1500, 600, r=0.0)
seed = fe(unreal.MaterialExpressionMultiply, -1500, 700); con(pir, seed, 'A'); con(fe(unreal.MaterialExpressionConstant, -1700, 760, r=50.0), seed, 'B')
ins = {'P': (fr, 'P'), 'N': (fr, 'return'), 'G': (fr, 'G'), 'Seed': seed, 'Shelter': (fr, 'Shelter'), 'Outer': one, 'Lean': zero4}
for n in ('WA0', 'WB0', 'WC0', 'WA1', 'WB1', 'WC1'): ins[n] = zero4
ins.update({'WT': wt, 'WH': zero, 'Fill': col[0], 'Dry': col[1], 'Time': col[2], 'Run': col[3], 'Dist': dist})
cu = custom(RAIN, O.CMOT_FLOAT4, ins, -1000, 0, (('Rough', F1), ('Spec', F1), ('Lens', F1)))
sl = fe(unreal.MaterialExpressionComponentMask, -750, 0, r=True, g=True, b=True, a=False); con(cu, sl, '', 'return')
al = fe(unreal.MaterialExpressionComponentMask, -750, 80, r=False, g=False, b=False, a=True); con(cu, al, '', 'return')
# the slope back to the world, round the pane's own normal (from the outside's point of view), in tangent space
nw = custom('return WN + Side * (X * S.x + Y * S.y + cross(X, Y) * S.z);', F3, {'WN': vn, 'Side': (fr, 'Side'), 'X': (fr, 'X'), 'Y': (fr, 'Y'), 'S': sl}, -550, -200)
tt = fe(unreal.MaterialExpressionTransform, -350, -200, transform_source_type=enum(TS, 'TRANSFORMSOURCE_WORLD'), transform_type=enum(TT, 'TRANSFORM_TANGENT')); con(nw, tt)
no = fe(unreal.MaterialExpressionNormalize, -200, -200); con(tt, no, 'VectorInput')
lens = fe(unreal.MaterialExpressionVectorParameter, -1000, 700, parameter_name='RainLens', default_value=unreal.LinearColor(0.5, 0.56, 0.62, 1))
A = {}
for k, (n, ty, dv) in enumerate((('OwnColor', FT.FUNCTION_INPUT_VECTOR3, (0.5, 0.5, 0.5)), ('OwnOpacity', FT.FUNCTION_INPUT_SCALAR, 0.1), ('OwnRoughness', FT.FUNCTION_INPUT_SCALAR, 0.02),
                                 ('OwnSpecular', FT.FUNCTION_INPUT_SCALAR, 0.5), ('OwnNormal', FT.FUNCTION_INPUT_VECTOR3, (0, 0, 1)))):
    y = 1000 + 110 * k
    d = fe(unreal.MaterialExpressionConstant, -900, y, r=dv) if isinstance(dv, float) else fe(unreal.MaterialExpressionConstant3Vector, -900, y, constant=unreal.LinearColor(dv[0], dv[1], dv[2], 0))
    A[n] = fe(unreal.MaterialExpressionFunctionInput, -700, y, input_name=n, input_type=ty, sort_priority=k, use_preview_value_as_default=True); con(d, A[n])
WET = (('Color', 'OwnColor', custom('return lerp(Own, Lc, L);', F3, {'Own': A['OwnColor'], 'Lc': lens, 'L': (cu, 'Lens')}, -200, 100)),
       ('Opacity', 'OwnOpacity', custom('return saturate(Own + (1 - Own) * R);', F1, {'Own': A['OwnOpacity'], 'R': al}, -200, 250)),
       ('Roughness', 'OwnRoughness', custom('return max(Own, R);', F1, {'Own': A['OwnRoughness'], 'R': (cu, 'Rough')}, -200, 400)),
       ('Specular', 'OwnSpecular', custom('return lerp(Own, 0.5, W);', F1, {'Own': A['OwnSpecular'], 'W': (cu, 'Spec')}, -200, 550)),
       ('Normal', 'OwnNormal', custom('return normalize(float3(Own.xy + R.xy, Own.z));', F3, {'Own': A['OwnNormal'], 'R': no}, -200, 700)))
for k, (n, own, wet) in enumerate(WET):
    sw = fe(unreal.MaterialExpressionStaticSwitchParameter, 100, 100 + 150 * k, parameter_name='RainGlass', default_value=False)
    con(wet, sw, 'True'); con(A[own], sw, 'False')
    con(sw, fe(unreal.MaterialExpressionFunctionOutput, 400, 100 + 150 * k, output_name=n, sort_priority=k))
mel.update_material_function(mf); save(mf, MFP)
# #endregion

# #region ---- into the materials: the call between what the property had and the property
def patch(path):
    m, call, had = HAD[path]; before = pixels(m) if not call else '(had it)'
    if call: mel.delete_material_expression(m, call)
    call = mel.create_material_expression(m, unreal.MaterialExpressionMaterialFunctionCall, 400, 900)
    if not call.set_material_function(mf): raise RuntimeError('function not set')
    for (p, i, o), (n, out) in zip(PROPS, had):
        if n and not mel.connect_material_expressions(n, out, call, i): raise RuntimeError('%s %s %s' % (path, i, out))
        if not mel.connect_material_property(call, o, p): raise RuntimeError('%s %s' % (path, o))
    log(path, 'before:', before, '| switch off:', pixels(m)); save(m, path); return m


def rain_on(mi, path, vec=(), sca=()):
    mel.set_material_instance_static_switch_parameter_value(mi, 'RainGlass', True)
    for k, v in vec: mel.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in sca: mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    log(path, 'rain on:', mel.get_material_instance_static_switch_parameter_value(mi, 'RainGlass'), '%d pixel instructions' % mel.get_statistics(mi).num_pixel_shader_instructions); save(mi, path)


# the terminal's panes (a mirror laid over what is seen through: its light parts a weaker mirror). glassBlue is indoors: none
patch('/Game/Terminal/Masters/M_BuildingGlass')
for n in ('glass', 'glassOuter', 'glassVault'):
    p = '/Game/Terminal/Materials/MI_T_' + n; rain_on(opened(p), p, vec=(('RainLens', unreal.LinearColor(0.26, 0.29, 0.32, 1)),))
# the tower's and the hangars' outside panes: an instance of their own, so every other M_Glass stays without
g = patch('/Game/Materials/M_Glass')
p = DIR + '/MI_Glass_Rain'; mi = opened(p, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew); mel.set_material_instance_parent(mi, g); rain_on(mi, p)
# the jetways' and the fire station's imported glass: copies of their master and instances
b = patch(BM)
for src, n, sca in (('/Game/Babylon/Jetways/jetways/Materials/glassOuter', 'MI_Rain_JetwayGlass', (('RainRoofed', 0.0), ('RainOn', 1.0))),
                    ('/Game/Babylon/Firestation/firestation/Materials/gl-bayDoor', 'MI_Rain_BayDoorGlass', ()), ('/Game/Babylon/Firestation/firestation/Materials/m12', 'MI_Rain_StationGlass', ())):
    p = DIR + '/' + n
    if not eal.does_asset_exist(p): eal.duplicate_asset(src, p)
    mi = opened(p); s = unreal.load_asset(src); old = s.get_editor_property('parent')
    while old.get_path_name().startswith(DIR): old = old.get_editor_property('parent')
    # (its parent holds the see-through and two-sided settings: a copy of that one too, standing on the rain's master)
    BP = DIR + '/MI_Rain_' + old.get_name()
    if not eal.does_asset_exist(BP): eal.duplicate_asset(old.get_path_name().split('.')[0], BP)
    bp = opened(BP)
    if bp.get_editor_property('parent') != b:
        log(BP, 'stood on', old.get_editor_property('parent').get_path_name())
        mel.set_material_instance_parent(bp, b); mel.update_material_instance(bp); save(bp, BP)
    mel.set_material_instance_parent(mi, bp)
    rain_on(mi, p, sca=sca)
    bad = [str(k) for k in mel.get_vector_parameter_names(old) if mel.get_material_instance_vector_parameter_value(s, k) != mel.get_material_instance_vector_parameter_value(mi, k)]
    bad += [str(k) for k in mel.get_scalar_parameter_names(old) if mel.get_material_instance_scalar_parameter_value(s, k) != mel.get_material_instance_scalar_parameter_value(mi, k)]
    o = bp.get_editor_property('base_property_overrides')
    log(n, 'on a copy of', old.get_name(), o.get_editor_property('override_blend_mode'), o.get_editor_property('blend_mode'), 'two-sided', o.get_editor_property('two_sided'))
    log(n, 'differ:', bad, 'color', mel.get_material_instance_vector_parameter_value(mi, 'BaseColorFactor'))
# #endregion
log('ready')
unreal.SystemLibrary.quit_editor()
