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
// smooth value noise: N one octave, F three (0..1, around 0.5)
struct FVd {
  float H(float3 p) { p = frac(p * 0.1031); p += dot(p, p.zyx + 31.32); return frac((p.x + p.y) * p.z); }
  float N(float3 p) {
    float3 i = floor(p), f = p - i; f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(H(i), H(i + float3(1,0,0)), f.x), lerp(H(i + float3(0,1,0)), H(i + float3(1,1,0)), f.x), f.y),
                lerp(lerp(H(i + float3(0,0,1)), H(i + float3(1,0,1)), f.x), lerp(H(i + float3(0,1,1)), H(i + float3(1,1,1)), f.x), f.y), f.z);
  }
  float F(float3 p) { return N(p) * 0.57 + N(p.yzx * 2.13 + 11.7) * 0.29 + N(p.zxy * 4.41 + 5.3) * 0.14; }
};
FVd Vd;
"""
# ---- the paint (colour; its roughness and lacquer are the node's two further outputs, so all is worked out once) ----
# Where things sit (to-do 261: they slid over a moving vehicle): chips, specks and grime by the part's own place (LP:
# the instance's frame, so they turn with a wheel), streaks and runs by the place on the vehicle (along, across, up).
# Own 1 (tyres, rims): a turning part - no streaks or runs, no change with the height. Rim 1 a rim, 0.6 a wheel disc
# (its size in TyreC / TyreS, VehicleDirt.cpp). TY: the tyre's relief (TYRE below; w the groove, below 0 the sidewall).
PAINT = NOISE + r"""
float3 p = LP / 100.0;                                    // metres, on the part itself
float px = max(length(LPX), length(LPY)) / 100.0;         // a pixel's size there: fine marks fade before they flicker
float h = (WP.z - GroundZ) / 100.0;                       // above the ground
float hE = lerp(h, 0.45, Own);
float2 uv = UV * OS.zw + OS.xy;
float3 base = BC.rgb * Base.Sample(BaseSampler, uv).rgb;
float n1 = Vd.N(p * 1.7 + 3.1), n2 = Vd.N(p * 4.3 + 19.7);

// how dirty this part of the truck still is (0 washed .. 1 as dirty as it gets): 6 lengths x 2 sides, set by the game
// as the wash hose's jet cleans it (Origin: the truck's middle, AxisF: its forward, Z0..Z2: the 12 areas)
float3 zLp = WP - Origin.xyz;
float zFa = clamp(dot(zLp, AxisF.xyz) / 100.0 + 6.0, 0.0, 11.999) / 2.0;
float3 zAr = normalize(cross(float3(0, 0, 1), AxisF.xyz));
float zSd = saturate(dot(zLp, zAr) / 40.0 + 0.5);
float zList[12] = { Z0.x, Z0.y, Z0.z, Z0.w, Z1.x, Z1.y, Z1.z, Z1.w, Z2.x, Z2.y, Z2.z, Z2.w };
int zI0 = min(int(zFa), 5), zI1 = min(zI0 + 1, 5); float zFt = saturate(zFa - zI0);
float zone = lerp(lerp(zList[zI0], zList[zI1], zFt), lerp(zList[6 + zI0], zList[6 + zI1], zFt), zSd);

// the place on the vehicle (m: along, across, up), bent sideways so that what runs down it wanders and changes width
float3 vP = float3(dot(zLp, AxisF.xyz) / 100.0, dot(zLp, zAr) / 100.0, h);
float bend = (Vd.N(float3(vP.xy * 2.3, h * 1.6)) - 0.5) * 0.3 + (Vd.N(float3(vP.xy * 9.0 + 7.0, h * 4.5)) - 0.5) * 0.06;
float2 vB = vP.xy + bend;
float body = 1.0 - Own;
// spray streaks up the panels: long soft strokes of uneven width, each ending where the broad noise lets go
float streak = smoothstep(0.38, 0.82, Vd.N(float3(vB * 9.0, h * 0.7))) * smoothstep(0.25, 0.7, Vd.N(float3(vP.xy * 1.9 + 31.0, h * 1.5))) * body;
// part-washed: the dirt goes in runs down the panel (each its own pace), the higher parts clean first, as the water
// carries it down; above 1 dirtier than usual (a truck back from the mud)
float zRun = lerp(saturate((Vd.N(float3(vB * 21.0 + 13.0, h * 0.8)) * 0.7 + Vd.N(float3(vB * 47.0, h * 1.6)) * 0.3 - 0.5) * 2.0 + 0.5), n2, Own);
float zC = saturate(zone);
float zone2 = saturate(zC * 1.5 - 0.25 + (zRun - 0.5) * 0.6 * (1.0 - abs(zC * 2.0 - 1.0)) - (hE - 0.6) * 0.35 * (1.0 - zC)) + max(zone - 1.0, 0.0) * 1.5;
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
// road dirt and spray: thickest at the bottom, fading by 1.3 m, broken up, with streaks running up the panels
float dirt = saturate(pow(saturate(1.0 - hE / 2.4), 1.3) * (0.55 + 0.6 * n1) + streak * 0.4 * saturate(1.0 - h / 3.0) - 0.1) * Dirt * zone2;
// fine wear chips: the dark primer showing in small ragged flakes, gathered in places, more low down and on the steps
float chip = 0.0;
float chipNear = 1.0 - smoothstep(0.0035, 0.012, px);
if (Wear > 0.01 && chipNear > 0.0)
{
  float3 cp = p * 85.0 + (n2 - 0.5) * 3.0;
  float cn = Vd.N(cp) * 0.56 + Vd.N(cp.yzx * 2.3 + 4.7) * 0.29 + Vd.N(cp.zxy * 5.1 + 9.1) * 0.15;
  float ct = lerp(0.86, 0.7, smoothstep(0.35, 0.8, Vd.N(p * 5.5 + 41.0)));
  chip = smoothstep(ct, ct + 0.05, cn) * chipNear * (0.35 + 0.65 * saturate(1.0 - hE / 1.6)) * Wear;
}
float3 col = lerp(base, float3(0.2, 0.16, 0.11), dirt * 0.85);
// heavy grime (owner, 2026-11-14, his reference picture: a truck driven hard off-road - mottled grey-brown grime over
// the whole body, top to bottom, speckled, the black parts and wheels caked): beyond 1 on the dirt scale, washed off in
// runs from the top down like the rest
float heavy = saturate((zone - 1.0) * 1.4 + (zRun - 0.5) * 0.5 * saturate((2.0 - zone) * 2.0) - (hE - 1.2) * 0.25 * saturate((2.0 - zone) * 2.0));
if (heavy > 0.0)
{
  float grime = saturate((n1 * 0.65 + n2 * 0.4 + Vd.N(p * 13.0 + 2.2) * 0.15 - 0.32) * 1.7) * heavy * (0.65 + 0.35 * saturate(1.0 - hE / 4.0));
  float sp = Vd.N(p * 68.0 + n1 * 2.0) * 0.62 + Vd.N(p.zxy * 151.0 + 6.0) * 0.38;
  float speck = smoothstep(0.68, 0.8, sp) * heavy * 0.7 * (1.0 - smoothstep(0.006, 0.02, px));
  float runs = smoothstep(0.45, 0.85, Vd.N(float3(vB * 14.0 + 23.0, h * 0.9))) * smoothstep(0.2, 0.65, Vd.N(float3(vP.xy * 3.1 + 57.0, h * 1.3))) * 0.75 * heavy * saturate(1.0 - h / 3.2) * body;
  col = lerp(col, float3(0.34, 0.32, 0.27), saturate(grime * 0.85 + runs * 0.4));
  col = lerp(col, float3(0.13, 0.115, 0.095), saturate(speck + saturate(1.0 - hE / 0.9) * heavy * 0.55));
}
col = lerp(col, float3(0.09, 0.09, 0.095), chip);
// rims: brake dust towards the middle (the hose takes most of it), the outer edge scuffed to the bare metal by kerbs
float scuff = 0.0;
if (Rim > 0.3 && TyreS.x > 1.0)
{
  float3 rq = LP - TyreC.xyz;
  float2 rd = TyreS.z < 0.5 ? rq.yz : (TyreS.z < 1.5 ? rq.zx : rq.xy);
  float rr = length(rd) / TyreS.x;
  float brake = (1.0 - smoothstep(0.2, 0.92, rr)) * (0.45 + 0.75 * n2) * (0.35 + 0.65 * zC);
  col = lerp(col, float3(0.055, 0.048, 0.042), saturate(brake * 0.42));
  if (Rim > 0.8) scuff = smoothstep(0.86, 0.98, rr + (n2 - 0.5) * 0.1) * smoothstep(0.5, 0.66, Vd.F(p * 24.0)) * saturate(Wear * 1.4);
  col = lerp(col, float3(0.36, 0.36, 0.355), scuff * 0.6);
}
// tyres: the grooves dark when clean and lighter with dust; a film of dust on the sidewall, off its raised letters
float gro = saturate(TY.w), sid = saturate(-TY.w);
float dust = saturate(Dirt * (0.25 + 0.75 * zC) * 1.2);
col *= 1.0 - 0.5 * gro * (1.0 - dust * 0.5);
col = lerp(col, float3(0.2, 0.175, 0.14), saturate(gro * 0.4 * (0.4 + n2) + (sid > 0.0 ? (1.5 - sid) * 0.14 * (0.3 + 1.1 * n1) : 0.0)) * dust);
// wet: darker and glossier where water lies (dirtier paint darkens more)
col *= lerp(1.0, 0.8, Wet * saturate(0.35 + dirt));
float r0 = lerp(lerp(R, 0.75, saturate(dirt * 0.8 + chip)), 0.5, scuff);
OutRough = lerp(lerp(lerp(r0, 0.06, Wet * 0.75), min(r0, 0.14), saturate(zBA * 1.5)), 0.02, zBead);
OutCoat = saturate(CC * saturate(1.0 - dirt * 0.7 - chip - scuff) + Wet * 0.3);
// freshly washed: a wet film on everything the water reached (paint, rubber, plastic), darker, drying with the beads
return col * (1.0 - 0.28 * zBead) * lerp(1.0, 0.82, saturate(zBA * 1.5));
"""
PAINT_IN = ['UV', 'OS', 'Base', 'BC', 'WP', 'LP', 'LPX', 'LPY', 'GroundZ', 'R', 'CC', 'Dirt', 'Wear', 'Wet', 'Own', 'Rim', 'TyreC', 'TyreS', 'TY',
            'Origin', 'AxisF', 'Z0', 'Z1', 'Z2', 'B0', 'B1', 'B2', 'Blow', 'WN']
# ---- the tyre's relief ---- (M_VehicleDirt; TyreC its middle in its own frame, TyreS: outer radius, half its width, its
# axle 0 x / 1 y / 2 z, 1 or -1 for a mirrored one; TreadH a tile of tread, TreadH / SideH made by
# Tools/make_tyre_textures.py). Gives the slope (xyz, its own frame) and w: 0..1 in a groove, 0..-1 on the sidewall
# (-1 on a raised letter). Nothing at all is done on anything that is not a tyre.
TYRE = r"""
if (TyreS.x < 1.0 || Rim > 0.3) return float4(0, 0, 0, 0);
float3 q = LP - TyreC.xyz;
int ax = int(TyreS.z + 0.5);
float3 d = ax == 0 ? q.yzx : (ax == 1 ? q.zxy : q.xyz);          // (round, round, along the axle)
float3 dX = ax == 0 ? LPX.yzx : (ax == 1 ? LPX.zxy : LPX.xyz);
float3 dY = ax == 0 ? LPY.yzx : (ax == 1 ? LPY.zxy : LPY.xyz);
float R = TyreS.x, W = max(TyreS.y, 0.1), r = max(length(d.xy), 0.001), rr = r / R;
float2 er = d.xy / r, et = float2(-er.y, er.x);
float th = atan2(d.y, d.x) / 6.2831853;
float tw = smoothstep(0.895, 0.93, rr);
float sw = (1.0 - smoothstep(0.87, 0.9, rr)) * smoothstep(0.6, 0.64, rr) * smoothstep(0.35, 0.55, abs(d.z) / W);
float3 g = float3(0, 0, 0); float w = 0.0;
if (tw > 0.001)
{
  // the running face and the shoulders: whole tiles round the tyre, one tile over its width
  float nT = max(round(3.14159 * R / W), 3.0);
  float kU = nT / (6.2831853 * R), kV = 0.5 / W;                 // tiles per cm
  float2 uv = float2(th * nT, 0.5 + d.z * kV);
  float2 gx = float2(dot(et, dX.xy) * kU, dX.z * kV), gy = float2(dot(et, dY.xy) * kU, dY.z * kV);
  float e = max(1.5 / 512.0, 0.5 * max(length(gx), length(gy)));
  float h0 = TreadH.SampleGrad(TreadHSampler, uv, gx, gy).r;
  float hU = TreadH.SampleGrad(TreadHSampler, uv + float2(e, 0), gx, gy).r;
  float hV = TreadH.SampleGrad(TreadHSampler, uv + float2(0, e), gx, gy).r;
  float deep = 0.022 * W * tw;
  float sU = (hU - h0) / e * kU * deep, sV = (hV - h0) / e * kV * deep;
  g += float3(et * sU, sV);
  w += tw * (1.0 - h0);
}
if (sw > 0.001)
{
  // the sidewall: ribs and the lettering, read the right way round on both sides, twice round the tyre
  float sd = (d.z >= 0.0 ? 1.0 : -1.0) * (TyreS.w < 0.0 ? -1.0 : 1.0);
  float row = R >= 43.0 ? 0.0 : (R >= 32.0 ? 1.0 : (R >= 22.0 ? 2.0 : 3.0));   // the size fitting the tyre
  float kU = sd * 2.0 / (6.2831853 * r), kV = -1.0 / (R * 0.28 * 4.0);
  float2 uv = float2(th * 2.0 * sd, (row + clamp((0.9 - rr) / 0.28, 0.03, 0.97)) / 4.0);
  float2 gx = float2(dot(et, dX.xy) * kU, dot(er, dX.xy) * kV), gy = float2(dot(et, dY.xy) * kU, dot(er, dY.xy) * kV);
  float2 e = max(float2(1.0 / 1024.0, 1.0 / 512.0), 0.5 * max(abs(gx), abs(gy)));
  float h0 = SideH.SampleGrad(SideHSampler, uv, gx, gy).r;
  float hU = SideH.SampleGrad(SideHSampler, uv + float2(e.x, 0), gx, gy).r;
  float hV = SideH.SampleGrad(SideHSampler, uv + float2(0, e.y), gx, gy).r;
  float deep = 0.005 * R * sw;
  float sU = (hU - h0) / e.x * kU * deep, sV = (hV - h0) / e.y * kV * deep;
  g += float3(et * sU + er * sV, 0.0);
  w -= sw * (0.35 + 0.65 * h0);
}
g *= 1.0 / max(1.0, length(g) / 1.2);                            // (no slope steeper than a block's edge)
return float4(ax == 0 ? g.zxy : (ax == 1 ? g.yzx : g.xyz), w);
"""


def local_place(m, P):
    # the part's own place (the instance's frame, cm; Nanite and instanced lists alike) and how it changes over a pixel
    lp = mel.create_material_expression(m, unreal.MaterialExpressionLocalPosition, -1300, -200)
    P['LP'] = (lp, '')
    for n, cls, y in (('LPX', unreal.MaterialExpressionDDX, -200), ('LPY', unreal.MaterialExpressionDDY, -100)):
        x = mel.create_material_expression(m, cls, -1100, y)
        if not mel.connect_material_expressions(lp, '', x, ''): log('WIRE FAILED %s' % n)
        P[n] = (x, '')


def paint_node(m, P, mma):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, -300)
    c.set_editor_property('code', PAINT); c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3); c.set_editor_property('description', 'TRUCK')
    l = []
    for n in PAINT_IN:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); l.append(ci)
    c.set_editor_property('inputs', l)
    ao = []
    for n in ('OutRough', 'OutCoat'):
        co = unreal.CustomOutput(); co.set_editor_property('output_name', n); co.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1); ao.append(co)
    c.set_editor_property('additional_outputs', ao)
    wire(m, c, P, PAINT_IN)
    for o, i in (('return', 'BaseColor'), ('OutRough', 'Roughness'), ('OutCoat', 'ClearCoat')):
        if not mel.connect_material_expressions(c, o, mma, i): log('WIRE FAILED paint %s -> %s' % (o, i))
    return c


WHEEL_S = [('Own', 0.0), ('Rim', 0.0)]
WHEEL_V = [('TyreC', (0, 0, 0, 0)), ('TyreS', (0, 0, 0, 1))]

m = mat('M_TruckPaint')
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_CLEAR_COAT)
m.set_editor_property('two_sided', True)
m.set_editor_property('use_material_attributes', True)
P = params(m, [('R', 0.3), ('M', 0.05), ('CC', 1.0), ('CCR', 0.03), ('GroundZ', 0.0), ('Dirt', 0.5), ('Wear', 0.5), ('Blow', 0.0)] + WHEEL_S,
           [('BC', (1, 1, 1, 1)), ('OS', (0, 0, 1, 1)), ('Origin', (0, 0, 0, 0)), ('AxisF', (1, 0, 0, 0)), ('Z0', (1, 1, 1, 1)), ('Z1', (1, 1, 1, 1)), ('Z2', (1, 1, 1, 1)), ('B0', (0, 0, 0, 0)), ('B1', (0, 0, 0, 0)), ('B2', (0, 0, 0, 0))] + WHEEL_V, [('Base', WHITE)])
wet = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -900, 700)
wet.set_editor_property('collection', MPC); wet.set_editor_property('parameter_name', 'Wetness'); P['Wet'] = (wet, '')
O = unreal.CustomMaterialOutputType
local_place(m, P)
# (the fire trucks' tyres are modelled - tread and lettering - and keep their own material: no relief from here)
noty = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -900, 800)
noty.set_editor_property('code', 'return float4(0, 0, 0, 0);'); noty.set_editor_property('output_type', O.CMOT_FLOAT4); noty.set_editor_property('inputs', [])
P['TY'] = (noty, '')
mma = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
paint_node(m, P, mma)
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
