# The true-Unreal terminal (to-do 42), part 3: the terminal's materials as real Unreal materials (owner, 2026-11-14).
# The Babylon export brought 708 materials on the generic glTF import masters. Here every one becomes an instance of
# the terminal's own masters, keeping exactly its colours, pictures, tiling, roughness, metal and clear coat:
#   M_TSurface (+_Coat lacquered, +_DS two-sided): the surfaces, with fine real detail by kind from CC0 scans (fabric
#     weave, carpet pile, wood grain, leather, plaster, concrete; Art/Terminal/Detail), laid on in world space so it
#     never stretches, a faint roughness variation, brushed streaks on brushed metal
#   M_TGlass (+_DS): real glass: thin-translucent (tints the light that passes, as glass does), clear straight on and a
#     mirror at a glancing angle (Fresnel), reflections from Lumen, faint smudges
#   M_TUnlit (+_Blend_DS): the lamps, glows and lit signs, as bright as before
# The textures are copied into /Game/Terminal/Textures (the terminal no longer needs the Babylon folder), the new
# instances are /Game/Terminal/Materials/MI_T_<name>, and every native piece (/Game/Terminal/Meshes) gets its own.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_terminal_materials.py" -unattended
import unreal, os, re
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_TMAT ' + m)
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OLD = '/Game/Babylon/Terminal/terminal'
T = '/Game/Terminal'
for d in (T + '/Masters', T + '/Materials', T + '/Textures', T + '/Detail'):
    if not eal.does_directory_exist(d): eal.make_directory(d)

# ---- the detail scans ----
det = {}
tasks = []
for f in sorted(os.listdir(os.path.join(ROOT, 'Art', 'Terminal', 'Detail'))):
    if not f.endswith('.jpg'): continue
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Terminal', 'Detail', f); t.destination_path = T + '/Detail'
    t.replace_existing = True; t.automated = True; t.save = False; tasks.append(t)
at.import_asset_tasks(tasks)
for f in os.listdir(os.path.join(ROOT, 'Art', 'Terminal', 'Detail')):
    if not f.endswith('.jpg'): continue
    tex = unreal.load_asset(T + '/Detail/' + f[:-4])
    if not tex: continue
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property('srgb', False)
    eal.save_loaded_asset(tex, only_if_is_dirty=False)
    det[f.split('_')[2]] = tex          # T_Detail_<kind>_N
log('detail scans: ' + ', '.join(sorted(det)))
WHITE = unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture')
BLACK = unreal.load_asset('/Engine/EngineResources/Black')
FLAT = unreal.load_asset('/Engine/EngineMaterials/DefaultNormal')

# ---- HLSL shared by the masters ----
NOISE = r"""
float hh(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
"""
UVX = "float2 uv = UV * OS.zw + OS.xy;\n"
CODE_BASE = UVX + r"""
float4 b = Base.Sample(BaseSampler, uv) * BC;
return b.rgb;
"""
CODE_ALPHA = UVX + r"""
return saturate(Base.Sample(BaseSampler, uv).a * BC.a);
"""
# the roughness: the material's own, a faint variation over the surface (smudges, wear), brushed streaks on brushed metal
CODE_ROUGH = r"""
float3 p = WP / 100.0;                              // metres
float3 q = floor(p * 2.0);
float n = 0;
{ float3 f = frac(p * 2.0); f = f * f * (3 - 2 * f);
  #define H(o) frac(sin(dot(q + o, float3(127.1, 311.7, 74.7))) * 43758.5453)
  n = lerp(lerp(lerp(H(float3(0,0,0)), H(float3(1,0,0)), f.x), lerp(H(float3(0,1,0)), H(float3(1,1,0)), f.x), f.y),
           lerp(lerp(H(float3(0,0,1)), H(float3(1,0,1)), f.x), lerp(H(float3(0,1,1)), H(float3(1,1,1)), f.x), f.y), f.z); }
float r = R + (n - 0.5) * RV;
// brushed: fine streaks along the surface (one axis of the world, the strongest one along it)
float st = frac(sin(floor(dot(p, float3(0, 0, 1)) * 400.0 + floor(p.x * 3.0) * 13.0) * 12.9898) * 43758.5453);
r += (st - 0.5) * 0.12 * BR;
return saturate(r);
"""
# the normal: the material's own normal map (tangent space), with the fine detail of its kind laid on from the world
# (three projections blended by the surface's facing, so it never stretches), returned in tangent space
CODE_NORMAL = UVX + r"""
float3 n = float3(0, 0, 1);
if (NS > 0) { float2 t = Nrm.Sample(NrmSampler, uv).rg * 2 - 1; t *= NS; n = normalize(float3(t, sqrt(saturate(1 - dot(t, t))))); }
if (DS > 0)
{
  float3 w = abs(WN); w = pow(w, 4); w /= (w.x + w.y + w.z);
  float3 p = WP / (100.0 * DT);
  float2 a = Det.Sample(DetSampler, p.yz).rg * 2 - 1, b = Det.Sample(DetSampler, p.xz).rg * 2 - 1, c = Det.Sample(DetSampler, p.xy).rg * 2 - 1;
  float2 d = (a * w.x + b * w.y + c * w.z) * DS;
  n = normalize(float3(n.xy + d, n.z));
}
return n;
"""
CODE_EMIT = UVX + r"""
return Emi.Sample(EmiSampler, uv).rgb * EC.rgb * EB;
"""

# (the "surface, forward shading" translucency lighting: its name differs between engine versions)
TLM_FWD = next((getattr(unreal.TranslucencyLightingMode, n) for n in dir(unreal.TranslucencyLightingMode) if 'FORWARD' in n.upper()), None)     or next((getattr(unreal.TranslucencyLightingMode, n) for n in dir(unreal.TranslucencyLightingMode) if n.upper().startswith('TLM_SURFACE')), None)
log('translucency lighting: %s' % TLM_FWD)

def custom(m, code, out, ins, y, extra=''):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out); c.set_editor_property('description', 'T')
    if extra: c.set_editor_property('additional_defines', [])
    cis = []
    for n_ in ins:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n_); cis.append(ci)
    c.set_editor_property('inputs', cis)
    return c

def params(m):
    P = {}
    y = -600
    def tobj(n, tex, samp):
        nonlocal y
        e = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -900, y); y += 110
        e.set_editor_property('parameter_name', n); e.set_editor_property('texture', tex); e.set_editor_property('sampler_type', samp)
        P[n] = (e, '')
    def vec(n, v):
        nonlocal y
        e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y); y += 90
        e.set_editor_property('parameter_name', n); e.set_editor_property('default_value', unreal.LinearColor(*v))
        P[n] = (e, 'RGBA')
    def sca(n, v):
        nonlocal y
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, y); y += 70
        e.set_editor_property('parameter_name', n); e.set_editor_property('default_value', v)
        P[n] = (e, '')
    tobj('Base', WHITE, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tobj('Nrm', FLAT, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    tobj('Emi', BLACK, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tobj('Det', FLAT, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    vec('BC', (1, 1, 1, 1)); vec('OS', (0, 0, 1, 1)); vec('EC', (0, 0, 0, 1))
    for n, v in (('R', 0.5), ('M', 0.0), ('NS', 0.0), ('DS', 0.0), ('DT', 1.0), ('RV', 0.0), ('BR', 0.0), ('EB', 1.0), ('CC', 0.0), ('CCR', 0.1), ('Dirt', 0.0)):
        sca(n, v)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, y); P['UV'] = (uv, '')
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, y + 60); P['WP'] = (wp, '')
    wn = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, y + 120); P['WN'] = (wn, '')
    return P

def wire(m, c, P, names):
    for n_ in names:
        e, o = P[n_]
        mel.connect_material_expressions(e, o, c, n_)

def master(name, kind, two_sided):
    path = T + '/Masters/' + name
    m = unreal.load_asset(path)
    if m: mel.delete_all_material_expressions(m)
    else: m = at.create_asset(name, T + '/Masters', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', two_sided)
    P = params(m)
    O = unreal.CustomMaterialOutputType
    if kind in ('surface', 'coat'):
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_CLEAR_COAT if kind == 'coat' else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        # (the clear coat's own inputs are reached through one material-attributes node, the same for both)
        m.set_editor_property('use_material_attributes', True)
        mma = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
        c = custom(m, CODE_BASE, O.CMOT_FLOAT3, ['UV', 'Base', 'BC', 'OS'], -300); wire(m, c, P, ['UV', 'Base', 'BC', 'OS'])
        mel.connect_material_expressions(c, '', mma, 'BaseColor')
        c = custom(m, CODE_ROUGH, O.CMOT_FLOAT1, ['WP', 'R', 'RV', 'BR'], -100); wire(m, c, P, ['WP', 'R', 'RV', 'BR'])
        mel.connect_material_expressions(c, '', mma, 'Roughness')
        c = custom(m, CODE_NORMAL, O.CMOT_FLOAT3, ['UV', 'OS', 'Nrm', 'NS', 'Det', 'DS', 'DT', 'WP', 'WN'], 100); wire(m, c, P, ['UV', 'OS', 'Nrm', 'NS', 'Det', 'DS', 'DT', 'WP', 'WN'])
        mel.connect_material_expressions(c, '', mma, 'Normal')
        c = custom(m, CODE_EMIT, O.CMOT_FLOAT3, ['UV', 'OS', 'Emi', 'EC', 'EB'], 300); wire(m, c, P, ['UV', 'OS', 'Emi', 'EC', 'EB'])
        mel.connect_material_expressions(c, '', mma, 'EmissiveColor')
        mel.connect_material_expressions(P['M'][0], '', mma, 'Metallic')
        if kind == 'coat':
            mel.connect_material_expressions(P['CC'][0], '', mma, 'ClearCoat')
            mel.connect_material_expressions(P['CCR'][0], '', mma, 'ClearCoatRoughness')
        mel.connect_material_property(mma, '', unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    elif kind == 'glass':
        # real glass, the dependable way: see-through and lit, almost clear looking straight through, turning into a
        # mirror toward a glancing angle (Fresnel, as real panes do), its reflections from Lumen, a faint tint of its
        # own colour, faint smudges
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        m.set_editor_property('translucency_lighting_mode', TLM_FWD)
        GL = r"""
float3 p = WP / 100.0;
float n = frac(sin(dot(floor(p * 3.0), float3(127.1, 311.7, 74.7))) * 43758.5453);
float sm = smoothstep(0.55, 1.0, n) * Dirt;                       // faint smudges here and there
float c = saturate(dot(normalize(CV), normalize(WN)));
float fr = 0.04 + 0.96 * pow(1 - abs(c), 5);                     // Fresnel: 4 % straight on, a mirror at a grazing angle
return float4(BC.rgb * 0.06, saturate(BC.a * 0.3 + fr * 0.7 + sm * 0.05));   // (clear: little haze of its own)
"""
        cv = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -900, 900); P['CV'] = (cv, '')
        c = custom(m, GL, O.CMOT_FLOAT4, ['WP', 'WN', 'CV', 'BC', 'Dirt'], -300); wire(m, c, P, ['WP', 'WN', 'CV', 'BC', 'Dirt'])
        rgb = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, -300)
        for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)): rgb.set_editor_property(ch, on)
        aa = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, -220)
        for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)): aa.set_editor_property(ch, on)
        mel.connect_material_expressions(c, '', rgb, ''); mel.connect_material_expressions(c, '', aa, '')
        mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(aa, '', unreal.MaterialProperty.MP_OPACITY)
        mel.connect_material_property(P['R'][0], '', unreal.MaterialProperty.MP_ROUGHNESS)
        one = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -150, -60); one.set_editor_property('r', 1.0)
        mel.connect_material_property(one, '', unreal.MaterialProperty.MP_SPECULAR)
        c = custom(m, CODE_NORMAL, O.CMOT_FLOAT3, ['UV', 'OS', 'Nrm', 'NS', 'Det', 'DS', 'DT', 'WP', 'WN'], 100); wire(m, c, P, ['UV', 'OS', 'Nrm', 'NS', 'Det', 'DS', 'DT', 'WP', 'WN'])
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_NORMAL)
    elif kind in ('unlit', 'unlit_blend'):
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT if kind == 'unlit_blend' else unreal.BlendMode.BLEND_OPAQUE)
        UL = UVX + r"""
return (Base.Sample(BaseSampler, uv).rgb * BC.rgb + Emi.Sample(EmiSampler, uv).rgb * EC.rgb) * EB;
"""
        c = custom(m, UL, O.CMOT_FLOAT3, ['UV', 'OS', 'Base', 'BC', 'Emi', 'EC', 'EB'], -300); wire(m, c, P, ['UV', 'OS', 'Base', 'BC', 'Emi', 'EC', 'EB'])
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if kind == 'unlit_blend':
            c = custom(m, CODE_ALPHA, O.CMOT_FLOAT1, ['UV', 'Base', 'BC', 'OS'], -100); wire(m, c, P, ['UV', 'Base', 'BC', 'OS'])
            mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY)
    elif kind == 'surface_blend':
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        m.set_editor_property('translucency_lighting_mode', TLM_FWD)
        c = custom(m, CODE_BASE, O.CMOT_FLOAT3, ['UV', 'Base', 'BC', 'OS'], -300); wire(m, c, P, ['UV', 'Base', 'BC', 'OS'])
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
        c = custom(m, CODE_ALPHA, O.CMOT_FLOAT1, ['UV', 'Base', 'BC', 'OS'], -200); wire(m, c, P, ['UV', 'Base', 'BC', 'OS'])
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY)
        mel.connect_material_property(P['R'][0], '', unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(P['M'][0], '', unreal.MaterialProperty.MP_METALLIC)
        c = custom(m, CODE_EMIT, O.CMOT_FLOAT3, ['UV', 'OS', 'Emi', 'EC', 'EB'], 300); wire(m, c, P, ['UV', 'OS', 'Emi', 'EC', 'EB'])
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property('used_with_nanite', True) if kind in ('surface', 'coat', 'unlit') else None
    m.set_editor_property('used_with_instanced_static_meshes', True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, only_if_is_dirty=False)
    return m

MASTERS = {
    ('surface', False): 'M_TSurface', ('surface', True): 'M_TSurface_DS', ('coat', False): 'M_TSurface_Coat', ('coat', True): 'M_TSurface_Coat_DS',
    ('glass', False): 'M_TGlass', ('glass', True): 'M_TGlass_DS', ('unlit', False): 'M_TUnlit', ('unlit', True): 'M_TUnlit_DS',
    ('unlit_blend', True): 'M_TUnlit_Blend_DS', ('surface_blend', True): 'M_TSurface_Blend_DS', ('surface_blend', False): 'M_TSurface_Blend',
}
MS = {k: master(v, k[0], k[1]) for k, v in MASTERS.items()}
log('masters ready: %d' % len(MS))

# ---- the kind of each surface: its detail scan, how big one tile of it is (m), its strength, roughness variation, brushed ----
def kind_of(name):
    n = name.lower()
    if re.match(r'^(fabric|rentalseat|seat)', n): return ('fabric', 0.12, 0.35, 0.04, 0)
    if n.startswith('carpet') or n == 'jwcarpet': return ('carpet', 0.6, 0.45, 0.05, 0)
    if re.match(r'^(oak|walnut|timber|herring|lacquer|rentaldeck|rentalslats|jwslats)', n): return ('wood', 1.2, 0.25, 0.06, 0)
    if n.startswith('leather'): return ('leather', 0.35, 0.4, 0.06, 0)
    if re.match(r'^(plaster|offwhite|white$|rentalsoffit)', n): return ('plaster', 1.6, 0.18, 0.05, 0)
    if re.match(r'^(concrete|kerb|rentalconcrete|rentalpaving)', n): return ('concrete', 2.0, 0.3, 0.06, 0)
    if re.match(r'^(steel|liftbrushed|metal$|beamm|mullion|rentalmullion|rentalperforated)', n): return (None, 1, 0, 0.03, 1.0)
    if re.match(r'^(chrome|brass|copper|gold|liftmirror|jwchrome)', n): return (None, 1, 0, 0.02, 0.15)
    if re.match(r'^(terrazzo|marbledark|tile|tiledark|porcelain|epoxy|rentalterrazzo|liftfloor)', n): return (None, 1, 0, 0.08, 0)
    return (None, 1, 0, 0, 0)

def is_glass(name, parent):
    n = name.lower()
    return 'blend' in (parent or '').lower() and (n.startswith('glass') and n != 'glassouter' or n.startswith('gl_') or n == 'gl' or n.startswith('rentalcarglass'))

# ---- the textures, copied into the terminal's own folder once ----
tex_copy = {}
def own(tex):
    if not tex: return None
    p = tex.get_path_name().split('.')[0]
    if not p.startswith(OLD): return tex
    if p in tex_copy: return tex_copy[p]
    dst = T + '/Textures/' + tex.get_name()
    if not eal.does_asset_exist(dst):
        eal.duplicate_asset(p, dst)
        eal.save_asset(dst, only_if_is_dirty=False)
    t = unreal.load_asset(dst)
    if not t: log('texture copy failed: ' + p); t = tex
    tex_copy[p] = t
    return t

def set_textures(mi, pairs):
    # (written into the instance directly: the library call refuses these picture slots)
    vals = []
    for n_, t in pairs:
        if not t: continue
        v = unreal.TextureParameterValue()
        info = unreal.MaterialParameterInfo(); info.set_editor_property('name', n_)
        v.set_editor_property('parameter_info', info); v.set_editor_property('parameter_value', t)
        vals.append(v)
    mi.set_editor_property('texture_parameter_values', vals)

def tparam(mi, n):
    for tp in mi.get_editor_property('texture_parameter_values'):
        if str(tp.get_editor_property('parameter_info').get_editor_property('name')) == n: return tp.get_editor_property('parameter_value')
    return None
def vparam(mi, n, d):
    for vp in mi.get_editor_property('vector_parameter_values'):
        if str(vp.get_editor_property('parameter_info').get_editor_property('name')) == n:
            c = vp.get_editor_property('parameter_value'); return (c.r, c.g, c.b, c.a)
    return d
def sparam(mi, n, d):
    for sp in mi.get_editor_property('scalar_parameter_values'):
        if str(sp.get_editor_property('parameter_info').get_editor_property('name')) == n: return sp.get_editor_property('parameter_value')
    return d

made = {}
counts = {}
for p in eal.list_assets(OLD + '/Materials', recursive=False, include_folder=False):
    old = unreal.load_asset(p)
    if not isinstance(old, unreal.MaterialInstanceConstant): continue
    par = old.get_editor_property('parent'); pn = par.get_name() if par else ''
    name = old.get_name()
    if 'Ground' in pn: made[name] = old; continue           # (the apron and lawn stay on the ground's own master)
    ds = pn.endswith('_DS')
    # (glass by its name, or by what it is: see-through, smooth, lit - a pane like m-4)
    if is_glass(name, pn) or ('Default_Blend' in pn and vparam(old, 'BaseColorFactor', (1, 1, 1, 1))[3] < 0.5 and sparam(old, 'RoughnessFactor', 1.0) <= 0.1): k = ('glass', ds)
    elif pn.startswith('MI_Unlit'): k = ('unlit_blend', True) if 'Blend' in pn else ('unlit', ds)
    elif 'Blend' in pn: k = ('surface_blend', ds)
    elif 'ClearCoat' in pn: k = ('coat', ds)
    else: k = ('surface', ds)
    dst = T + '/Materials/MI_T_' + name
    mi = unreal.load_asset(dst)
    if not mi: mi = at.create_asset('MI_T_' + name, T + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, MS[k])
    bc = vparam(old, 'BaseColorFactor', (1, 1, 1, 1))
    if k[0].startswith('unlit'):
        hx = re.match(r'^l_([0-9A-Fa-f]{2,6})', name)
        if hx:                                    # a lamp named by its colour (l_FFF4D8): that colour, full
            h = (hx.group(1) + '000000')[:6]
            bc = (int(h[0:2], 16) / 255.0, int(h[2:4], 16) / 255.0, int(h[4:6], 16) / 255.0, 1.0)
        elif max(bc[:3]) < 0.01:                  # a glow left black by the import (the ceiling's downlights): warm white
            bc = (1.0, 0.957, 0.847, 1.0)
    os_ = vparam(old, 'BaseColorTexture_OffsetScale', (0, 0, 1, 1))
    ec = vparam(old, 'EmissiveFactor', (0, 0, 0, 1))
    mel.set_material_instance_vector_parameter_value(mi, 'BC', unreal.LinearColor(*bc))
    mel.set_material_instance_vector_parameter_value(mi, 'OS', unreal.LinearColor(*os_))
    mel.set_material_instance_vector_parameter_value(mi, 'EC', unreal.LinearColor(*ec))
    pairs = [(dstp, own(tparam(old, src))) for src, dstp in (('BaseColorTexture', 'Base'), ('EmissiveTexture', 'Emi'), ('NormalTexture', 'Nrm'))]
    if tparam(old, 'NormalTexture'): mel.set_material_instance_scalar_parameter_value(mi, 'NS', sparam(old, 'NormalScale', 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, 'R', sparam(old, 'RoughnessFactor', 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, 'M', sparam(old, 'MetallicFactor', 1.0 if not tparam(old, 'BaseColorTexture') else 0.0) if k[0] != 'glass' else 0.0)
    mel.set_material_instance_scalar_parameter_value(mi, 'CC', sparam(old, 'ClearCoatFactor', 0.0))
    mel.set_material_instance_scalar_parameter_value(mi, 'EB', sparam(old, 'EmissiveStrength', 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, 'CCR', sparam(old, 'ClearCoatRoughnessFactor', 0.1))
    if k[0] == 'glass':
        mel.set_material_instance_scalar_parameter_value(mi, 'Dirt', 1.0)
        mel.set_material_instance_scalar_parameter_value(mi, 'R', min(0.06, sparam(old, 'RoughnessFactor', 0.03)))
    else:
        dk, size, strength, rv, br = kind_of(name)
        if dk and dk in det:
            pairs.append(('Det', det[dk]))
            mel.set_material_instance_scalar_parameter_value(mi, 'DS', strength)
            mel.set_material_instance_scalar_parameter_value(mi, 'DT', size)
        mel.set_material_instance_scalar_parameter_value(mi, 'RV', rv)
        mel.set_material_instance_scalar_parameter_value(mi, 'BR', br)
    set_textures(mi, pairs)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    made[name] = mi
    counts[MASTERS[k]] = counts.get(MASTERS[k], 0) + 1
log('materials: %d (%s); textures copied: %d' % (len(made), ', '.join('%s %d' % kv for kv in sorted(counts.items())), len(tex_copy)))

# ---- every native piece onto its new material ----
n = 0
for p in eal.list_assets(T + '/Meshes', recursive=False, include_folder=False):
    s = unreal.load_asset(p)
    if not isinstance(s, unreal.StaticMesh): continue
    sm = s.get_editor_property('static_materials')
    changed = False
    for i, slot in enumerate(sm):
        mi = slot.get_editor_property('material_interface')
        if not mi: continue
        nm = mi.get_name()
        if nm.startswith('MI_T_'): continue
        new = made.get(nm)
        if new and new != mi: s.set_material(i, new); changed = True
    if changed: eal.save_loaded_asset(s, only_if_is_dirty=False); n += 1
log('pieces on the new materials: %d' % n)
unreal.SystemLibrary.quit_editor()
