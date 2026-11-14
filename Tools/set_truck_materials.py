# The fire trucks' own Unreal materials (owner, 2026-11-14: to-do 41a + 41c).
#   M_TruckPaint: the trucks' paint (and their teal, chevrons, lettering, black parts, rims, tread plates) with what the
#     ground has: road dirt and spray rising from the bottom (thickest low, streaking up), fine wear chips showing the
#     dark primer (more low and on the steps), wetness from the weather (MPC_Weather Wetness: darker, glossier) - in the
#     truck's own colours and lacquer (clear coat). Its height above the ground comes from the game (GroundZ).
#   M_TruckLens: the lamps' lenses - coloured glass when off; lit, LED dots behind the lens with a bright core, glowing
#     strongly enough to bloom while keeping their colour; how bright each lens is comes per lens from the game
#     (per-instance custom data 0: 0 off .. 1 full), so every lamp flashes in its own pattern.
# FireTruck.cpp puts them on the trucks (copying each original material's own colours and settings).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_truck_materials.py" -unattended
import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_TRUCKMAT ' + m)
D = '/Game/Truck'
if not eal.does_directory_exist(D): eal.make_directory(D)
WHITE = unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture')
MPC = unreal.load_asset('/Game/Weather/MPC_Weather')

def mat(name):
    p = D + '/' + name
    m = unreal.load_asset(p)
    if m: mel.delete_all_material_expressions(m)
    else: m = at.create_asset(name, D, unreal.Material, unreal.MaterialFactoryNew())
    return m

def custom(m, code, out, ins, y):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, y)
    c.set_editor_property('code', code); c.set_editor_property('output_type', out); c.set_editor_property('description', 'TRUCK')
    l = []
    for n in ins:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); l.append(ci)
    c.set_editor_property('inputs', l)
    return c

def params(m, scal, vecs, texs, extra=True):
    P = {}; y = -700
    for n, v in vecs:
        e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y); y += 90
        e.set_editor_property('parameter_name', n); e.set_editor_property('default_value', unreal.LinearColor(*v)); P[n] = (e, 'RGBA')
    for n, v in scal:
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, y); y += 70
        e.set_editor_property('parameter_name', n); e.set_editor_property('default_value', v); P[n] = (e, '')
    for n, t in texs:
        e = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -900, y); y += 110
        e.set_editor_property('parameter_name', n); e.set_editor_property('texture', t); P[n] = (e, '')
    if extra:
        P['UV'] = (mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, y), '')
        P['WP'] = (mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, y + 60), '')
        P['WN'] = (mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, y + 120), '')
    return P

def wire(m, c, P, names):
    for n in names: mel.connect_material_expressions(P[n][0], P[n][1], c, n)

NOISE = r"""
#define VN(P) ( frac(sin(dot(floor(P), float3(127.1, 311.7, 74.7))) * 43758.5453) )
"""
# ---- the paint ----
PAINT = NOISE + r"""
float3 p = WP / 100.0;                                    // metres
float h = (WP.z - GroundZ) / 100.0;                       // above the ground
float2 uv = UV * OS.zw + OS.xy;
float3 base = BC.rgb * Base.Sample(BaseSampler, uv).rgb;
// value noise (smooth), two scales
float3 q = p * 1.7; float3 f = frac(q); f = f * f * (3 - 2 * f); float3 i0 = floor(q);
float n1 = lerp(lerp(lerp(VN(i0), VN(i0 + float3(1,0,0)), f.x), lerp(VN(i0 + float3(0,1,0)), VN(i0 + float3(1,1,0)), f.x), f.y),
                lerp(lerp(VN(i0 + float3(0,0,1)), VN(i0 + float3(1,0,1)), f.x), lerp(VN(i0 + float3(0,1,1)), VN(i0 + float3(1,1,1)), f.x), f.y), f.z);
// road dirt and spray: thickest at the bottom, fading by 1.3 m, broken up, with streaks running up the panels
float streak = VN(float3(floor(p.x * 9.0), floor(p.y * 9.0), floor(p.z * 0.6)));
float dirt = saturate(pow(saturate(1.0 - h / 1.3), 1.6) * (0.55 + 0.6 * n1) + streak * 0.25 * saturate(1.0 - h / 2.2) - 0.15) * Dirt;
// fine wear chips: the dark primer showing, more low down and on the steps
float chip = smoothstep(0.965, 0.985, VN(p * 38.0)) * (0.35 + 0.65 * saturate(1.0 - h / 1.6)) * Wear;
float3 col = lerp(base, float3(0.16, 0.14, 0.115), dirt * 0.75);
col = lerp(col, float3(0.09, 0.09, 0.095), chip);
// wet: darker and glossier where water lies (dirtier paint darkens more)
col *= lerp(1.0, 0.8, Wet * saturate(0.35 + dirt));
return col;
"""
PROUGH = NOISE + r"""
float3 p = WP / 100.0; float h = (WP.z - GroundZ) / 100.0;
float dirt = saturate(pow(saturate(1.0 - h / 1.3), 1.6) * 0.8 - 0.1) * Dirt;
float chip = smoothstep(0.965, 0.985, VN(p * 38.0)) * (0.35 + 0.65 * saturate(1.0 - h / 1.6)) * Wear;
float r = lerp(R, 0.75, saturate(dirt * 0.8 + chip));
return lerp(r, 0.06, Wet * 0.75);
"""
PCOAT = NOISE + r"""
float3 p = WP / 100.0; float h = (WP.z - GroundZ) / 100.0;
float dirt = saturate(pow(saturate(1.0 - h / 1.3), 1.6) * 0.8 - 0.1) * Dirt;
float chip = smoothstep(0.965, 0.985, VN(p * 38.0)) * Wear;
return CC * saturate(1.0 - dirt * 0.7 - chip) + Wet * 0.3;
"""
m = mat('M_TruckPaint')
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_CLEAR_COAT)
m.set_editor_property('two_sided', True)
m.set_editor_property('use_material_attributes', True)
P = params(m, [('R', 0.3), ('M', 0.05), ('CC', 1.0), ('CCR', 0.03), ('GroundZ', 0.0), ('Dirt', 0.5), ('Wear', 0.5)],
           [('BC', (1, 1, 1, 1)), ('OS', (0, 0, 1, 1))], [('Base', WHITE)])
wet = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -900, 700)
wet.set_editor_property('collection', MPC); wet.set_editor_property('parameter_name', 'Wetness'); P['Wet'] = (wet, '')
O = unreal.CustomMaterialOutputType
mma = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
c = custom(m, PAINT, O.CMOT_FLOAT3, ['UV', 'OS', 'Base', 'BC', 'WP', 'GroundZ', 'Dirt', 'Wear', 'Wet'], -300); wire(m, c, P, ['UV', 'OS', 'Base', 'BC', 'WP', 'GroundZ', 'Dirt', 'Wear', 'Wet'])
mel.connect_material_expressions(c, '', mma, 'BaseColor')
c = custom(m, PROUGH, O.CMOT_FLOAT1, ['WP', 'GroundZ', 'R', 'Dirt', 'Wear', 'Wet'], -100); wire(m, c, P, ['WP', 'GroundZ', 'R', 'Dirt', 'Wear', 'Wet'])
mel.connect_material_expressions(c, '', mma, 'Roughness')
c = custom(m, PCOAT, O.CMOT_FLOAT1, ['WP', 'GroundZ', 'CC', 'Dirt', 'Wear', 'Wet'], 100); wire(m, c, P, ['WP', 'GroundZ', 'CC', 'Dirt', 'Wear', 'Wet'])
mel.connect_material_expressions(c, '', mma, 'ClearCoat')
mel.connect_material_expressions(P['CCR'][0], '', mma, 'ClearCoatRoughness')
mel.connect_material_expressions(P['M'][0], '', mma, 'Metallic')
mel.connect_material_property(mma, '', unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)

# ---- the lamp lens ----
LENS_BASE = r"""return BC.rgb * 0.22;"""
LENS_EMIT = r"""
float on = saturate(I);
if (on <= 0.001) return float3(0, 0, 0);
// LED dots behind the lens, a glow between them, a brighter core in the middle
float2 g = frac(UV * Dots) - 0.5;
float dot_ = smoothstep(0.38, 0.12, length(g));
float core = saturate(1.0 - length(UV - 0.5) * 1.6);
float k = (0.35 + 0.65 * dot_) * (0.75 + 0.5 * core);
// the colour kept: normalised so the brightest channel carries the glow (no washing out to white)
float3 c = BC.rgb / max(max(BC.r, BC.g), max(BC.b, 1e-3));
return c * k * Glow * on;
"""
m = mat('M_TruckLens')
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', True)
P = params(m, [('Glow', 40.0), ('Dots', 6.0), ('R', 0.06)], [('BC', (1, 1, 1, 1))], [])
pid = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -900, 800)
pid.set_editor_property('data_index', 0); P['I'] = (pid, '')
c = custom(m, LENS_BASE, O.CMOT_FLOAT3, ['BC'], -300); wire(m, c, P, ['BC'])
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
c = custom(m, LENS_EMIT, O.CMOT_FLOAT3, ['UV', 'BC', 'I', 'Glow', 'Dots'], -100); wire(m, c, P, ['UV', 'BC', 'I', 'Glow', 'Dots'])
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(P['R'][0], '', unreal.MaterialProperty.MP_ROUGHNESS)
one = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -150, 100); one.set_editor_property('r', 1.0)
mel.connect_material_property(one, '', unreal.MaterialProperty.MP_SPECULAR)
m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('ready: M_TruckPaint, M_TruckLens')
unreal.SystemLibrary.quit_editor()
