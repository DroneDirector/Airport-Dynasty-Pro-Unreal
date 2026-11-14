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
TLM_SURF = next((getattr(unreal.TranslucencyLightingMode, n) for n in dir(unreal.TranslucencyLightingMode) if n.upper().startswith('TLM_SURFACE')), None)
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
    for n in names:
        if not mel.connect_material_expressions(P[n][0], P[n][1], c, n): log('WIRE FAILED %s (%s)' % (n, P[n][0].get_class().get_name()))

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

// how dirty this part of the truck still is (0 washed .. 1 as dirty as it gets): 6 lengths x 2 sides, set by the game
// as the wash hose's jet cleans it (Origin: the truck's middle, AxisF: its forward, Z0..Z2: the 12 areas)
float3 zLp = WP - Origin.xyz;
float zFa = clamp(dot(zLp, AxisF.xyz) / 100.0 + 6.0, 0.0, 11.999) / 2.0;
float3 zAr = normalize(cross(float3(0, 0, 1), AxisF.xyz));
float zSd = saturate(dot(zLp, zAr) / 40.0 + 0.5);
float zList[12] = { Z0.x, Z0.y, Z0.z, Z0.w, Z1.x, Z1.y, Z1.z, Z1.w, Z2.x, Z2.y, Z2.z, Z2.w };
int zI0 = min(int(zFa), 5), zI1 = min(zI0 + 1, 5); float zFt = saturate(zFa - zI0);
float zone = lerp(lerp(zList[zI0], zList[zI1], zFt), lerp(zList[6 + zI0], zList[6 + zI1], zFt), zSd);
// part-washed: the dirt goes in runs down the panel (narrow vertical stripes, each its own pace), the higher parts clean
// first, as the water carries it down; above 1 dirtier than usual (a truck back from the mud)
float zRun = frac(sin(floor(dot(zLp, zAr) / 3.0 + dot(zLp, AxisF.xyz) / 3.0) * 91.7) * 43758.5);
float zC = saturate(zone);
float zone2 = saturate(zC * 1.5 - 0.25 + (zRun - 0.5) * 0.6 * (1.0 - abs(zC * 2.0 - 1.0)) - (h - 0.6) * 0.35 * (1.0 - zC)) + max(zone - 1.0, 0.0) * 1.5;
// beads of water left by the hose (B0..B2: the 12 areas; drying in ~20 minutes, blown off by driving: Blow pushes them
// back along the truck), on the outward faces only
float zBl[12] = { B0.x, B0.y, B0.z, B0.w, B1.x, B1.y, B1.z, B1.w, B2.x, B2.y, B2.z, B2.w };
float zBA = lerp(lerp(zBl[zI0], zBl[zI1], zFt), lerp(zBl[6 + zI0], zBl[6 + zI1], zFt), zSd);
zBA *= saturate(dot(normalize(WN), normalize(float3(zLp.xy, 0.001))) * 3.0 + 0.4);
float3 zAn = abs(WN);
float2 zQ = zAn.z > max(zAn.x, zAn.y) ? WP.xy : (zAn.x > zAn.y ? WP.yz : WP.xz);
zQ += float2(-dot(AxisF.xyz, float3(1, 1, 0)), 0.0) * Blow;
float2 zG = floor(zQ / 1.4), zF = frac(zQ / 1.4) - 0.5;
float zR1 = frac(sin(dot(zG, float2(12.9898, 78.233))) * 43758.5453), zR2 = frac(sin(dot(zG, float2(39.3468, 11.135))) * 24634.6345);
float zBead = (zR1 < zBA ? 1.0 : 0.0) * (1.0 - smoothstep((0.1 + 0.25 * zR2) * 0.7, 0.1 + 0.25 * zR2, length(zF - (float2(zR1, zR2) - 0.5) * 0.45)));
float dirt = saturate(pow(saturate(1.0 - h / 2.4), 1.3) * (0.55 + 0.6 * n1) + streak * 0.3 * saturate(1.0 - h / 3.0) - 0.1) * Dirt * zone2;
// fine wear chips: the dark primer showing, more low down and on the steps
float chip = smoothstep(0.965, 0.985, VN(p * 38.0)) * (0.35 + 0.65 * saturate(1.0 - h / 1.6)) * Wear;
float3 col = lerp(base, float3(0.2, 0.16, 0.11), dirt * 0.85);
// heavy grime (owner, 2026-11-14, his reference picture: a truck driven hard off-road - mottled grey-brown grime over
// the whole body, top to bottom, speckled, the black parts and wheels caked): beyond 1 on the dirt scale, washed off in
// runs from the top down like the rest
float3 g2 = p * 4.3; float3 gf = frac(g2); gf = gf * gf * (3 - 2 * gf); float3 gi = floor(g2);
float n2 = lerp(lerp(lerp(VN(gi), VN(gi + float3(1,0,0)), gf.x), lerp(VN(gi + float3(0,1,0)), VN(gi + float3(1,1,0)), gf.x), gf.y),
                lerp(lerp(VN(gi + float3(0,0,1)), VN(gi + float3(1,0,1)), gf.x), lerp(VN(gi + float3(0,1,1)), VN(gi + float3(1,1,1)), gf.x), gf.y), gf.z);
float heavy = saturate((zone - 1.0) * 1.4 + (zRun - 0.5) * 0.5 * saturate((2.0 - zone) * 2.0) - (h - 1.2) * 0.25 * saturate((2.0 - zone) * 2.0));
float grime = saturate((n1 * 0.65 + n2 * 0.55 - 0.32) * 1.7) * heavy * (0.65 + 0.35 * saturate(1.0 - h / 4.0));
float speck = step(0.9, VN(p * 55.0)) * heavy * 0.7;
float runs = saturate(VN(float3(floor(p.x * 14.0), floor(p.y * 14.0), floor(p.z * 1.5))) * 1.3 - 0.55) * heavy * saturate(1.0 - h / 3.2);
col = lerp(col, float3(0.34, 0.32, 0.27), saturate(grime * 0.85 + runs * 0.4));
col = lerp(col, float3(0.13, 0.115, 0.095), saturate(speck + saturate(1.0 - h / 0.9) * heavy * 0.55));
col = lerp(col, float3(0.09, 0.09, 0.095), chip);
// wet: darker and glossier where water lies (dirtier paint darkens more)
col *= lerp(1.0, 0.8, Wet * saturate(0.35 + dirt));
// freshly washed: a wet film on everything the water reached (paint, rubber, plastic), darker, drying with the beads
return col * (1.0 - 0.28 * zBead) * lerp(1.0, 0.82, saturate(zBA * 1.5));
"""
PROUGH = NOISE + r"""
float3 p = WP / 100.0; float h = (WP.z - GroundZ) / 100.0;

// how dirty this part of the truck still is (0 washed .. 1 as dirty as it gets): 6 lengths x 2 sides, set by the game
// as the wash hose's jet cleans it (Origin: the truck's middle, AxisF: its forward, Z0..Z2: the 12 areas)
float3 zLp = WP - Origin.xyz;
float zFa = clamp(dot(zLp, AxisF.xyz) / 100.0 + 6.0, 0.0, 11.999) / 2.0;
float3 zAr = normalize(cross(float3(0, 0, 1), AxisF.xyz));
float zSd = saturate(dot(zLp, zAr) / 40.0 + 0.5);
float zList[12] = { Z0.x, Z0.y, Z0.z, Z0.w, Z1.x, Z1.y, Z1.z, Z1.w, Z2.x, Z2.y, Z2.z, Z2.w };
int zI0 = min(int(zFa), 5), zI1 = min(zI0 + 1, 5); float zFt = saturate(zFa - zI0);
float zone = lerp(lerp(zList[zI0], zList[zI1], zFt), lerp(zList[6 + zI0], zList[6 + zI1], zFt), zSd);
// part-washed: the dirt goes in runs down the panel (narrow vertical stripes, each its own pace), the higher parts clean
// first, as the water carries it down; above 1 dirtier than usual (a truck back from the mud)
float zRun = frac(sin(floor(dot(zLp, zAr) / 3.0 + dot(zLp, AxisF.xyz) / 3.0) * 91.7) * 43758.5);
float zC = saturate(zone);
float zone2 = saturate(zC * 1.5 - 0.25 + (zRun - 0.5) * 0.6 * (1.0 - abs(zC * 2.0 - 1.0)) - (h - 0.6) * 0.35 * (1.0 - zC)) + max(zone - 1.0, 0.0) * 1.5;
// beads of water left by the hose (B0..B2: the 12 areas; drying in ~20 minutes, blown off by driving: Blow pushes them
// back along the truck), on the outward faces only
float zBl[12] = { B0.x, B0.y, B0.z, B0.w, B1.x, B1.y, B1.z, B1.w, B2.x, B2.y, B2.z, B2.w };
float zBA = lerp(lerp(zBl[zI0], zBl[zI1], zFt), lerp(zBl[6 + zI0], zBl[6 + zI1], zFt), zSd);
zBA *= saturate(dot(normalize(WN), normalize(float3(zLp.xy, 0.001))) * 3.0 + 0.4);
float3 zAn = abs(WN);
float2 zQ = zAn.z > max(zAn.x, zAn.y) ? WP.xy : (zAn.x > zAn.y ? WP.yz : WP.xz);
zQ += float2(-dot(AxisF.xyz, float3(1, 1, 0)), 0.0) * Blow;
float2 zG = floor(zQ / 1.4), zF = frac(zQ / 1.4) - 0.5;
float zR1 = frac(sin(dot(zG, float2(12.9898, 78.233))) * 43758.5453), zR2 = frac(sin(dot(zG, float2(39.3468, 11.135))) * 24634.6345);
float zBead = (zR1 < zBA ? 1.0 : 0.0) * (1.0 - smoothstep((0.1 + 0.25 * zR2) * 0.7, 0.1 + 0.25 * zR2, length(zF - (float2(zR1, zR2) - 0.5) * 0.45)));
float dirt = saturate(pow(saturate(1.0 - h / 2.4), 1.3) * 0.8 - 0.1) * Dirt * zone2;
float chip = smoothstep(0.965, 0.985, VN(p * 38.0)) * (0.35 + 0.65 * saturate(1.0 - h / 1.6)) * Wear;
float r = lerp(R, 0.75, saturate(dirt * 0.8 + chip));
return lerp(lerp(lerp(r, 0.06, Wet * 0.75), min(r, 0.14), saturate(zBA * 1.5)), 0.02, zBead);
"""
PCOAT = NOISE + r"""
float3 p = WP / 100.0; float h = (WP.z - GroundZ) / 100.0;

// how dirty this part of the truck still is (0 washed .. 1 as dirty as it gets): 6 lengths x 2 sides, set by the game
// as the wash hose's jet cleans it (Origin: the truck's middle, AxisF: its forward, Z0..Z2: the 12 areas)
float3 zLp = WP - Origin.xyz;
float zFa = clamp(dot(zLp, AxisF.xyz) / 100.0 + 6.0, 0.0, 11.999) / 2.0;
float3 zAr = normalize(cross(float3(0, 0, 1), AxisF.xyz));
float zSd = saturate(dot(zLp, zAr) / 40.0 + 0.5);
float zList[12] = { Z0.x, Z0.y, Z0.z, Z0.w, Z1.x, Z1.y, Z1.z, Z1.w, Z2.x, Z2.y, Z2.z, Z2.w };
int zI0 = min(int(zFa), 5), zI1 = min(zI0 + 1, 5); float zFt = saturate(zFa - zI0);
float zone = lerp(lerp(zList[zI0], zList[zI1], zFt), lerp(zList[6 + zI0], zList[6 + zI1], zFt), zSd);
// part-washed: the dirt goes in runs down the panel (narrow vertical stripes, each its own pace), the higher parts clean
// first, as the water carries it down; above 1 dirtier than usual (a truck back from the mud)
float zRun = frac(sin(floor(dot(zLp, zAr) / 3.0 + dot(zLp, AxisF.xyz) / 3.0) * 91.7) * 43758.5);
float zC = saturate(zone);
float zone2 = saturate(zC * 1.5 - 0.25 + (zRun - 0.5) * 0.6 * (1.0 - abs(zC * 2.0 - 1.0)) - (h - 0.6) * 0.35 * (1.0 - zC)) + max(zone - 1.0, 0.0) * 1.5;
// beads of water left by the hose (B0..B2: the 12 areas; drying in ~20 minutes, blown off by driving: Blow pushes them
// back along the truck), on the outward faces only
float zBl[12] = { B0.x, B0.y, B0.z, B0.w, B1.x, B1.y, B1.z, B1.w, B2.x, B2.y, B2.z, B2.w };
float zBA = lerp(lerp(zBl[zI0], zBl[zI1], zFt), lerp(zBl[6 + zI0], zBl[6 + zI1], zFt), zSd);
zBA *= saturate(dot(normalize(WN), normalize(float3(zLp.xy, 0.001))) * 3.0 + 0.4);
float3 zAn = abs(WN);
float2 zQ = zAn.z > max(zAn.x, zAn.y) ? WP.xy : (zAn.x > zAn.y ? WP.yz : WP.xz);
zQ += float2(-dot(AxisF.xyz, float3(1, 1, 0)), 0.0) * Blow;
float2 zG = floor(zQ / 1.4), zF = frac(zQ / 1.4) - 0.5;
float zR1 = frac(sin(dot(zG, float2(12.9898, 78.233))) * 43758.5453), zR2 = frac(sin(dot(zG, float2(39.3468, 11.135))) * 24634.6345);
float zBead = (zR1 < zBA ? 1.0 : 0.0) * (1.0 - smoothstep((0.1 + 0.25 * zR2) * 0.7, 0.1 + 0.25 * zR2, length(zF - (float2(zR1, zR2) - 0.5) * 0.45)));
float dirt = saturate(pow(saturate(1.0 - h / 2.4), 1.3) * 0.8 - 0.1) * Dirt * zone2;
float chip = smoothstep(0.965, 0.985, VN(p * 38.0)) * Wear;
return CC * saturate(1.0 - dirt * 0.7 - chip) + Wet * 0.3;
"""
m = mat('M_TruckPaint')
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_CLEAR_COAT)
m.set_editor_property('two_sided', True)
m.set_editor_property('use_material_attributes', True)
P = params(m, [('R', 0.3), ('M', 0.05), ('CC', 1.0), ('CCR', 0.03), ('GroundZ', 0.0), ('Dirt', 0.5), ('Wear', 0.5), ('Blow', 0.0)],
           [('BC', (1, 1, 1, 1)), ('OS', (0, 0, 1, 1)), ('Origin', (0, 0, 0, 0)), ('AxisF', (1, 0, 0, 0)), ('Z0', (1, 1, 1, 1)), ('Z1', (1, 1, 1, 1)), ('Z2', (1, 1, 1, 1)), ('B0', (0, 0, 0, 0)), ('B1', (0, 0, 0, 0)), ('B2', (0, 0, 0, 0))], [('Base', WHITE)])
wet = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -900, 700)
wet.set_editor_property('collection', MPC); wet.set_editor_property('parameter_name', 'Wetness'); P['Wet'] = (wet, '')
O = unreal.CustomMaterialOutputType
mma = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
c = custom(m, PAINT, O.CMOT_FLOAT3, ['UV', 'OS', 'Base', 'BC', 'WP', 'GroundZ', 'Dirt', 'Wear', 'Wet', 'Origin', 'AxisF', 'Z0', 'Z1', 'Z2', 'B0', 'B1', 'B2', 'Blow', 'WN'], -300); wire(m, c, P, ['UV', 'OS', 'Base', 'BC', 'WP', 'GroundZ', 'Dirt', 'Wear', 'Wet', 'Origin', 'AxisF', 'Z0', 'Z1', 'Z2', 'B0', 'B1', 'B2', 'Blow', 'WN'])
mel.connect_material_expressions(c, '', mma, 'BaseColor')
c = custom(m, PROUGH, O.CMOT_FLOAT1, ['WP', 'GroundZ', 'R', 'Dirt', 'Wear', 'Wet', 'Origin', 'AxisF', 'Z0', 'Z1', 'Z2', 'B0', 'B1', 'B2', 'Blow', 'WN'], -100); wire(m, c, P, ['WP', 'GroundZ', 'R', 'Dirt', 'Wear', 'Wet', 'Origin', 'AxisF', 'Z0', 'Z1', 'Z2', 'B0', 'B1', 'B2', 'Blow', 'WN'])
mel.connect_material_expressions(c, '', mma, 'Roughness')
c = custom(m, PCOAT, O.CMOT_FLOAT1, ['WP', 'GroundZ', 'CC', 'Dirt', 'Wear', 'Wet', 'Origin', 'AxisF', 'Z0', 'Z1', 'Z2', 'B0', 'B1', 'B2', 'Blow', 'WN'], 100); wire(m, c, P, ['WP', 'GroundZ', 'CC', 'Dirt', 'Wear', 'Wet', 'Origin', 'AxisF', 'Z0', 'Z1', 'Z2', 'B0', 'B1', 'B2', 'Blow', 'WN'])
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
