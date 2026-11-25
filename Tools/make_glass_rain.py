# Rain on the glass (to-do 295; how to use it: docs/Glass-Rain-Plan.md). Makes, rerunnable (assets are refilled, not replaced):
#   /Game/Weather/MPC_GlassRain - the weather's part, written every frame by UGlassRain (GlassRain.cpp)
#   /Game/Weather/MF_GlassRain  - the drops: static ones that grow and merge, runners with a beaded trail, the wipers' sweep
#   /Game/Truck/M_TruckGlass    - the vehicles' glass (tint, the wash hose's beads as before, the rain)
# M_TruckGlass is made HERE ONLY (set_water_materials.py no longer makes it), so either script may be run again at any time.
# Run with UnrealEditor-Cmd ... -ExecutePythonScript=<this file>.
import os
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
O = unreal.CustomMaterialOutputType
MP = unreal.MaterialProperty
FT = unreal.FunctionInputType


def log(t):
    unreal.log('ADP_GLASSRAIN ' + t)


def diskfile(path):
    return os.path.join(unreal.Paths.project_content_dir(), path.replace('/Game/', '') + '.uasset')


def open_asset(path, cls, factory):
    f = diskfile(path)
    if os.path.exists(f):
        os.chmod(f, 0o666)   # (write-protected after an LFS commit: the save would fail silently)
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    d, n = path.rsplit('/', 1)
    return at.create_asset(n, d, cls, factory)


def save(a, path):
    was = os.path.getmtime(diskfile(path)) if os.path.exists(diskfile(path)) else 0
    ok = eal.save_loaded_asset(a, only_if_is_dirty=False)
    now = os.path.getmtime(diskfile(path)) if os.path.exists(diskfile(path)) else 0
    log('saved %s: %s' % (path, 'ok' if ok and now > was else 'FAILED'))


# #region ---- the weather's collection
MPC = '/Game/Weather/MPC_GlassRain'
SCALARS = ('GlassFill', 'GlassDry', 'GlassTime', 'GlassRun')
mpc = open_asset(MPC, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
have = {str(p.get_editor_property('parameter_name')) for p in mpc.get_editor_property('scalar_parameters')}
if not set(SCALARS) <= have:
    ps = []
    for n in SCALARS:
        p = unreal.CollectionScalarParameter(); p.set_editor_property('parameter_name', n); p.set_editor_property('default_value', 0.0); ps.append(p)
    mpc.set_editor_property('scalar_parameters', ps)
save(mpc, MPC)
# #endregion

# #region ---- the drops
RAIN = r'''
struct GR
{
  float3 h3(float2 p) { float3 q = frac(float3(p.xyx) * float3(.1031, .1030, .0973)); q += dot(q, q.yxz + 33.33); return frac((q.xxy + q.yzz) * q.zyx); }
  // one layer of standing drops: q where (cm, in the face's two axes), m those axes' true length on the sloped pane, one
  // drop of two merging parts at most in each cell, random in place, size, when it lands (few: how many cells ever get one)
  // and when it has dried (small ones first). Returns the surface's slope (xy) and how much is water (z)
  float3 beads(float2 q, float2 m, float2 rot, float cell, float seed, float fill, float dry, float few, float big)
  {
    float2 rq = float2(q.x * rot.x - q.y * rot.y, q.x * rot.y + q.y * rot.x);
    float2 g = floor(rq / cell), f = rq / cell - g - 0.5;
    float3 r = h3(g + seed * 37.1), s = h3(g * 1.37 + seed * 11.3 + 71.7);
    float grow = saturate((fill - r.x * few) / 0.3);
    float sz = lerp(0.3, 1.0, s.z * s.z);
    float dead = 0.14 + 0.62 * sz * big + 0.2 * s.y;
    grow = sqrt(grow) * saturate((dead - dry) / 0.13);
    if (grow <= 0.02) return 0;
    float Rm = cell * 0.36 * sz, R = Rm * grow, R2 = R * (0.45 + 0.35 * s.x);
    float lim = 0.5 - Rm / cell * 1.15;
    float2 c1 = (r.yz * 2 - 1) * lim, c2 = clamp(c1 + (s.xy - 0.5) * 1.5 * Rm / cell, -lim - 0.1, lim + 0.1);
    float2 d1 = (f - c1) * cell, d2 = (f - c2) * cell;
    d1 = float2(d1.x * rot.x + d1.y * rot.y, -d1.x * rot.y + d1.y * rot.x) * m * float2(1, 0.82 + 0.3 * s.y);
    d2 = float2(d2.x * rot.x + d2.y * rot.y, -d2.x * rot.y + d2.y * rot.x) * m;
    float w1 = max(0, 1 - dot(d1, d1) / (R * R * 1.5)), w2 = max(0, 1 - dot(d2, d2) / (R2 * R2 * 1.5));
    float F = w1 * w1 + 0.8 * w2 * w2;
    float2 sl = (w1 * d1 / R + 0.8 * w2 * d2 / R2) * 2.6;
    float cov = smoothstep(0.08, 0.22, F);
    return float3(sl * cov, cov);
  }
  // the runners: c across, a along (cm, growing the way the water goes). Returns the slope (across, along), the water, and
  // (w) how much of the standing drops it has swept up
  float4 runs(float c, float a, float seed, float act, float time)
  {
    float W = 5.0, ci = floor(c / W), x0 = (c / W - ci - 0.5) * W;
    float3 r = h3(float2(ci, seed * 53.7 + 3.1));
    float k = floor(90 + r.x * 130), L = 70 + 60 * r.y;
    float aa = a + r.z * L * 7.3, j = floor(aa / L), al = aa - j * L;
    float3 rc = h3(float2(ci * 3.1 + 17, j + seed * 19.9));
    float tt = time * k / 3600.0 + rc.x, n = floor(tt), x = tt - n;
    n = n - k * floor(n / k);   // (the clock wraps every hour without a jump: every column has a whole number of runs in it)
    float3 q = h3(float2(ci + n * 1.71, j * 2.3 + seed * 7.7 + n));
    float live = smoothstep(q.x, q.x + 0.08, act);
    if (live <= 0) return 0;
    float p = saturate(x / 0.7);
    float y = p + 0.048 * sin(p * 9.4 + q.y * 20) + 0.018 * sin(p * 25 + q.z * 30) - 0.048 * sin(q.y * 20) - 0.018 * sin(q.z * 30);   // (it creeps, stops and runs on)
    float a0 = L * 0.15 * q.y, len = L * (0.3 + 0.5 * q.z), head = a0 + len * max(0, y);
    float cx = W * ((q.y - 0.5) * 0.36 + 0.10 * sin(al * 0.21 + q.x * 40) + 0.05 * sin(al * 0.57 + q.z * 17));
    float dc = x0 - cx, fade = 1 - smoothstep(0.7, 0.98, x);
    float2 sl = 0; float cov = 0, clr = 0;
    float R = (0.24 + 0.2 * q.z) * live * smoothstep(0, 0.06, x) * (1 - smoothstep(0.88, 1.0, x)), da = al - head;
    float2 d = float2(dc, da * (da < 0 ? 0.5 : 1.0)) / max(R, 0.01);
    float w = max(0, 1 - dot(d, d) / 1.5) * step(0.03, R);
    cov = smoothstep(0.02, 0.2, w); sl = d * w * 2.2 * cov;
    if (da < 0 && al > a0)
    {
      float back = saturate(-da / len), keep = fade * live;
      // the film right behind it, then a row of small beads that dry from the top
      float film = exp(da / 7.0) * smoothstep(0.3, 0.12, abs(dc)) * keep * (1 - cov);
      sl.x += dc * 2.0 * film; cov = max(cov, film * 0.5);
      float bi = floor(al / 0.8); float3 b = h3(float2(bi + n * 3.3, ci * 5.1 + j + seed));
      float br = (0.06 + 0.12 * b.x * b.x) * step(b.y, 0.75) * saturate((1.2 * keep - back * 0.9 - 0.2 * b.z) / 0.2);
      float2 bd = float2(dc - (b.z - 0.5) * 0.25, al - (bi + 0.5) * 0.8 - (b.x - 0.5) * 0.3) / max(br, 0.01);
      float bw = max(0, 1 - dot(bd, bd) / 1.5) * step(0.03, br), bc = smoothstep(0.02, 0.25, bw);
      sl += bd * bw * 2.2 * bc; cov = max(cov, bc);
      clr = smoothstep(0.5, 0.25, abs(dc)) * keep * saturate(1.1 - back * 0.7);
    }
    return float4(sl, cov, clr);
  }
  // one blade: A0 pivot + inner radius, B0 where it lies at rest + outer radius, C0 its axis + the sweep (radians); T seconds
  // since the sweep began, the one before began that long before, seconds there and back, the sweep's number. h: how far the
  // blade's line passes beside the pivot. Returns: age of the last wipe (s), its number, the water pushed to the edge, how near below the edge
  float4 wipe(float3 P, float4 A0, float4 B0, float4 C0, float4 T, float h, out float3 edgeDir)
  {
    edgeDir = 0;
    float3 rel = P - A0.xyz; float off = dot(rel, C0.xyz); rel -= off * C0.xyz;
    float r = length(rel);
    if (abs(off) > 25 || r > B0.w + 18) return float4(1e4, 0, 0, 0);
    float3 tn = cross(C0.xyz, B0.xyz);
    float th = atan2(dot(rel, tn), dot(rel, B0.xyz));
    th -= asin(saturate(h / max(r, h + 0.01))) - asin(saturate(h / max(B0.w, 1)));
    float e = th / C0.w;
    bool inR = r > A0.w && r < B0.w;
    float t1 = T.z * acos(clamp(1 - 2 * saturate(e), -1, 1)) / 6.2831853, t2 = T.z - t1;
    float age = T.x >= t2 ? T.x - t2 : (T.x >= t1 ? T.x - t1 : T.x + T.y - t2);
    float id = T.w * 2 + (T.x >= t2 ? 1 : (T.x >= t1 ? 0 : -1));
    float edge = 0, below = 0;
    float3 rh = rel / max(r, 0.1), th3 = cross(C0.xyz, rh);
    float over = (th - C0.w) * r, hl = T.z * 0.5, ageEnd = T.x >= hl ? T.x - hl : T.x + T.y - hl;
    float lump = 0.55 + 0.45 * sin(r * 1.7 + T.w) * sin(r * 0.63 + 1.3 + T.w * 2);
    if (inR && over > 0 && over < 18)
    {
      float l = exp(-(over - 0.35) * (over - 0.35) * 9) * lump * exp(-ageEnd / 25.0);
      edge = l; edgeDir = th3 * (over - 0.35) * 5; below = (1 - over / 18) * exp(-ageEnd / 25.0);
    }
    float rim = r - B0.w;
    if (rim > 0 && rim < 1.4 && e > 0 && e < 1)
    {
      float l = exp(-(rim - 0.3) * (rim - 0.3) * 10) * (0.5 + 0.5 * sin(th * r * 1.3 + T.w)) * exp(-age / 25.0) * 0.7;
      if (l > edge) { edge = l; edgeDir = rh * (rim - 0.3) * 5; }
    }
    if (!inR || e < -0.01 || e > 1.01) age = 1e4;
    return float4(age, id, edge, below);
  }
};
Rough = 0; Spec = 0; Lens = 0;
float fill0 = saturate(Fill * Shelter * Outer);
[branch] if (fill0 < 0.002) return 0;
GR gr;
float cov = 0, steepAct = 0; float3 tilt = 0;
float far = smoothstep(1400, 2600, Dist);
float3 Nn = normalize(N), an = abs(Nn);
// the face: from the side it looks to most (a windscreen's slope counts as upright, only a roof is flat)
float3 U, V; float2 m; float face;
if (an.z > 0.82) { U = float3(1, 0, 0); V = float3(0, 1, 0); m = 1 / max(an.z, 0.5); face = Nn.z > 0 ? 1 : 2; }
else if (an.x > an.y) { U = float3(0, 1, 0); V = float3(0, 0, 1); m = float2(length(Nn.xy) / an.x, 1 / max(length(Nn.xy), 0.5)); face = Nn.x > 0 ? 3 : 4; }
else { U = float3(1, 0, 0); V = float3(0, 0, 1); m = float2(length(Nn.xy) / max(an.y, 0.1), 1 / max(length(Nn.xy), 0.5)); face = Nn.y > 0 ? 5 : 6; }
float2 q = float2(dot(P, U), dot(P, V));
float seed = frac(Seed * 0.731 + face * 0.377) * 17.0;
q += seed * float2(13.7, 7.9);
float fillS = pow(fill0, 1.5), dryS = Dry, age = 1e4; float3 eDir = 0, e3 = 0; float edge = 0, below = 0, wseed = 0;
[branch] if (WB0.w > 1)
{
  float4 w0 = gr.wipe(P, WA0, WB0, WC0, WT, WH, eDir), w1 = 0; float3 e1 = 0;
  age = w0.x; wseed = w0.y; edge = w0.z; below = w0.w; e3 = eDir;
  if (WB1.w > 1)
  {
    w1 = gr.wipe(P, WA1, WB1, WC1, WT, WH, e1);
    if (w1.x < age) { age = w1.x; wseed = w1.y + 0.5; }
    if (w1.z > edge) { edge = w1.z; e3 = e1; }
    below = max(below, w1.w);
  }
  if (age < 9e3) { edge = 0; below = 0; }
  // wiped: clear behind the blade; new drops land (a light scatter at once, then it fills as the rest does)
  if (age < 420) { float back = saturate(age / 300.0); fillS = min(fillS, (0.02 + 0.05 * Run) * saturate(age / 2.0) + back * 1.05); dryS = min(dryS, 1); seed += frac(wseed * 0.271) * 9.0 * (1 - step(419, age)); q += (seed - floor(seed)) * 3.1; }
}
[branch] if (far < 1)
{
  float nearS = 1 - smoothstep(450, 850, Dist), nearM = 1 - smoothstep(900, 1500, Dist);
  // a little thicker here, thinner there
  float uneven = 1 + 0.14 * sin(q.x * 0.11 + sin(q.y * 0.07) * 2 + seed) + 0.1 * sin(q.y * 0.19 + q.x * 0.05 + seed * 2);
  float fl = fillS * uneven;
  // the runners: down the pane where it is steep; with speed they lean back instead
  float4 run = 0; float2 rdir = float2(0, -1);
  float3 Gp = G - Nn * dot(G, Nn), Lp = Lean.xyz - Nn * dot(Lean.xyz, Nn);
  float steep = smoothstep(0.3, 0.75, length(Gp)), lw = saturate(Lean.w) * saturate(length(Lp) * 1.3);
  float act = Run * steep * smoothstep(0.1, 0.6, fillS) * saturate(age / 30.0) + below * 0.3 * Run;
  [branch] if (act > 0.002)
  {
    float2 gd = float2(dot(G, U), dot(G, V)); gd = gd / max(length(gd), 0.01);
    float2 ld = float2(dot(Lean.xyz, U), dot(Lean.xyz, V)); ld = ld / max(length(ld), 0.01);
    float ag = act * (1 - lw), ab = act * lw * 1.4;
    if (ag > 0.002) { run = gr.runs(dot(q, float2(gd.y, -gd.x)), dot(q, gd), seed, ag, Time); rdir = gd; }
    if (ab > 0.002) { float4 r2 = gr.runs(dot(q, float2(ld.y, -ld.x)), dot(q, ld), seed + 3.3, ab, Time); if (r2.z > run.z || ag <= 0.002) { r2.w = max(r2.w, run.w); run = r2; rdir = ld; } }
  }
  float keep = 1 - 0.9 * run.w;
  float3 b = gr.beads(q, m, float2(0.988, 0.156), 2.6, seed + 1, fl, dryS, 2.6, 1.0);
  if (nearM > 0) { float3 b1 = gr.beads(q + 31.7, m, float2(0.956, -0.292), 1.15, seed + 2, fl, dryS, 1.15, 0.6) * nearM; b.xy += b1.xy * (1 - b.z); b.z = max(b.z, b1.z); }
  if (nearS > 0) { float3 b2 = gr.beads(q + 77.3, m, float2(0.857, 0.515), 0.5, seed + 4, fl, dryS, 1.0, 0.3) * nearS; b.xy += b2.xy * (1 - b.z); b.z = max(b.z, b2.z); }
  b *= keep;
  float2 sl = b.xy * (1 - run.z) + float2(rdir.y, -rdir.x) * run.x + rdir * run.y;
  cov = max(b.z, run.z);
  tilt = U * sl.x + V * sl.y + e3 * edge * fillS;
  cov = max(cov, edge * 0.8 * saturate(fill0 * 2));
  tilt -= Nn * dot(tilt, Nn) * 0.5;
  float tl = length(tilt); tilt *= min(1, 1.6 / max(tl, 0.001));
  float g0 = dot(tilt, G);
  Lens = saturate(g0 * 0.9) * cov * (1 - far);          // (a drop shows the sky upside down: its lower edge is light)
  float nd = 1 - far; tilt *= nd; cov *= nd;
}
float haze = fillS * (1 - dryS) * smoothstep(700, 2200, Dist);
Rough = lerp(0.02, 0.05, cov) + 0.16 * haze;
Spec = cov;
return float4(tilt, saturate(cov * (0.10 + 0.3 * saturate(length(tilt) * 0.8)) + 0.04 * haze));
'''
MFP = '/Game/Weather/MF_GlassRain'
mf = open_asset(MFP, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
mel.delete_all_material_expressions_in_function(mf)
mf.set_editor_property('description', 'Rain on a pane (to-do 295, docs/Glass-Rain-Plan.md)')
mf.set_editor_property('expose_to_library', True)


def fe(cls, x, y, **kw):
    e = mel.create_material_expression_in_function(mf, cls, x, y)
    for k, v in kw.items():
        e.set_editor_property(k, v)
    return e


def con(a, b, pin='', out=''):
    if not mel.connect_material_expressions(a, out, b, pin):
        raise RuntimeError('could not connect %s to %s.%s' % (a.get_name(), b.get_name(), pin))


TS, TT = unreal.MaterialVectorCoordTransformSource, unreal.MaterialVectorCoordTransform
PS = unreal.MaterialPositionTransformSource
# (by value as a fall-back: the last names of the enumerations differ between versions)
def ename(en, *subs):
    for n in dir(en):
        if any(n.upper() == s for s in subs):
            return getattr(en, n)
    raise RuntimeError('%s: %s not in %s' % (en, subs, [n for n in dir(en) if n.isupper()]))


S_WORLD, S_INST = ename(TS, 'TRANSFORMSOURCE_WORLD'), ename(TS, 'TRANSFORMSOURCE_INSTANCE')
T_INST, T_WORLD, T_TAN = ename(TT, 'TRANSFORM_INSTANCE'), ename(TT, 'TRANSFORM_WORLD'), ename(TT, 'TRANSFORM_TANGENT')
P_WORLD, P_INST = ename(PS, 'TRANSFORMPOSSOURCE_WORLD'), ename(PS, 'TRANSFORMPOSSOURCE_INSTANCE')

wp = fe(unreal.MaterialExpressionWorldPosition, -1900, -400)
vn = fe(unreal.MaterialExpressionVertexNormalWS, -1900, -250)
pl = fe(unreal.MaterialExpressionTransformPosition, -1650, -400, transform_source_type=P_WORLD, transform_type=P_INST); con(wp, pl)
nl = fe(unreal.MaterialExpressionTransform, -1650, -250, transform_source_type=S_WORLD, transform_type=T_INST); con(vn, nl)
gw = fe(unreal.MaterialExpressionConstant3Vector, -1900, -100, constant=unreal.LinearColor(0, 0, -1, 0))
gl = fe(unreal.MaterialExpressionTransform, -1650, -100, transform_source_type=S_WORLD, transform_type=T_INST); con(gw, gl)

INPUTS = [('PositionCm', 'P', FT.FUNCTION_INPUT_VECTOR3, pl, 'Where on the pane, cm, in the part\'s own frame (default: its local position)'),
          ('PaneNormal', 'N', FT.FUNCTION_INPUT_VECTOR3, nl, 'The pane\'s normal in that frame (default: the vertex normal)'),
          ('Gravity', 'G', FT.FUNCTION_INPUT_VECTOR3, gl, 'Down in that frame, length 1 (default: the world\'s)'),
          ('PaneSeed', 'Seed', FT.FUNCTION_INPUT_SCALAR, 0.0, 'Any number: every object its own drops'),
          ('Shelter', 'Shelter', FT.FUNCTION_INPUT_SCALAR, 1.0, '1 in the open .. 0 under a roof'),
          ('Outer', 'Outer', FT.FUNCTION_INPUT_SCALAR, 1.0, '1 on the outer face, 0 on the inner'),
          ('Lean', 'Lean', FT.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), 'xyz: where the airflow drives the water (that frame), w how strongly 0..1'),
          ('WiperA0', 'WA0', FT.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), 'Blade 1: pivot, inner radius'), ('WiperB0', 'WB0', FT.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), 'its direction at rest, outer radius (0: no blade)'),
          ('WiperC0', 'WC0', FT.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), 'its axis, the sweep (radians)'),
          ('WiperA1', 'WA1', FT.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), 'Blade 2'), ('WiperB1', 'WB1', FT.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), ''), ('WiperC1', 'WC1', FT.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), ''),
          ('WiperT', 'WT', FT.FUNCTION_INPUT_VECTOR4, (1000, 1000, 1.5, 0), 'seconds since the sweep began; the one before: how long before that; seconds there and back; its number'),
          ('WiperH', 'WH', FT.FUNCTION_INPUT_SCALAR, 0.0, 'how far the blade\'s line passes beside its pivot, cm')]
cu = fe(unreal.MaterialExpressionCustom, -600, 0, code=RAIN, output_type=O.CMOT_FLOAT4, description='GLASSRAIN')
names = [i[1] for i in INPUTS] + ['Fill', 'Dry', 'Time', 'Run', 'Dist']
l = []
for n in names:
    ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); l.append(ci)
cu.set_editor_property('inputs', l)
ex = []
for n in ('Rough', 'Spec', 'Lens'):
    co = unreal.CustomOutput(); co.set_editor_property('output_name', n); co.set_editor_property('output_type', O.CMOT_FLOAT1); ex.append(co)
cu.set_editor_property('additional_outputs', ex)
for k, (nm, pin, ty, dv, desc) in enumerate(INPUTS):
    y = -400 + 90 * k
    i = fe(unreal.MaterialExpressionFunctionInput, -1150, y, input_name=nm, input_type=ty, description=desc, sort_priority=k, use_preview_value_as_default=True)
    if isinstance(dv, float):
        dv = fe(unreal.MaterialExpressionConstant, -1400, y, r=dv)
    elif isinstance(dv, tuple):
        dv = fe(unreal.MaterialExpressionConstant4Vector, -1400, y, constant=unreal.LinearColor(*dv))
    con(dv, i)
    con(i, cu, pin)
for k, (pn, pin) in enumerate((('GlassFill', 'Fill'), ('GlassDry', 'Dry'), ('GlassTime', 'Time'), ('GlassRun', 'Run'))):
    con(fe(unreal.MaterialExpressionCollectionParameter, -1150, 1000 + 60 * k, collection=mpc, parameter_name=pn), cu, pin)
con(fe(unreal.MaterialExpressionPixelDepth, -1150, 1260), cu, 'Dist')
# the slope out of the part's frame, onto the pane's normal, in tangent space (the two-sided turn is the engine's)
tx = fe(unreal.MaterialExpressionComponentMask, -300, -200, r=True, g=True, b=True, a=False); con(cu, tx, '', 'return')
tw = fe(unreal.MaterialExpressionTransform, -150, -200, transform_source_type=S_INST, transform_type=T_WORLD); con(tx, tw)
ad = fe(unreal.MaterialExpressionAdd, 0, -200); con(vn, ad, 'A'); con(tw, ad, 'B')
tt = fe(unreal.MaterialExpressionTransform, 150, -200, transform_source_type=S_WORLD, transform_type=T_TAN); con(ad, tt)
no = fe(unreal.MaterialExpressionNormalize, 300, -200); con(tt, no, 'VectorInput')
op = fe(unreal.MaterialExpressionComponentMask, -300, 0, r=False, g=False, b=False, a=True); con(cu, op, '', 'return')
for k, (nm, src, out) in enumerate((('Normal', no, ''), ('Roughness', cu, 'Rough'), ('Opacity', op, ''), ('Specular', cu, 'Spec'), ('Lens', cu, 'Lens'))):
    o = fe(unreal.MaterialExpressionFunctionOutput, 500, -200 + 120 * k, output_name=nm, sort_priority=k)
    con(src, o, '', out)
mel.update_material_function(mf)
save(mf, MFP)
# #endregion

# #region ---- the vehicles' glass
BEADS = r'''
// the wash hose's beads: how wet this part is (the 12 areas, as the paint's), on outward-facing glass only
float3 bLp = WP - Origin.xyz;
float bFa = clamp(dot(bLp, AxisF.xyz) / 100.0 + 6.0, 0.0, 11.999) / 2.0;
float3 bAr = normalize(cross(float3(0, 0, 1), AxisF.xyz));
float bSd = saturate(dot(bLp, bAr) / 40.0 + 0.5);
float bList[12] = { B0.x, B0.y, B0.z, B0.w, B1.x, B1.y, B1.z, B1.w, B2.x, B2.y, B2.z, B2.w };
int bI0 = min(int(bFa), 5), bI1 = min(bI0 + 1, 5); float bFt = saturate(bFa - bI0);
float amount = lerp(lerp(bList[bI0], bList[bI1], bFt), lerp(bList[6 + bI0], bList[6 + bI1], bFt), bSd);
float3 bFlat = float3(bLp.xy, 0);
amount *= saturate(dot(normalize(N), normalize(bFlat + float3(0, 0, 0.001))) * 3.0 + 0.4);
// one at most in each 1.4 cm cell of the glass's own plane, a random size and place; fewer as it dries; pushed back along
// the truck by the airflow (Blow, cm)
float3 an = abs(N);
float2 q = an.z > max(an.x, an.y) ? WP.xy : (an.x > an.y ? WP.yz : WP.xz);
q += float2(-dot(AxisF.xyz, float3(1, 1, 0)), 0.0) * Blow;
float2 g = floor(q / 1.4), f = frac(q / 1.4) - 0.5;
float r1 = frac(sin(dot(g, float2(12.9898, 78.233))) * 43758.5453);
float r2 = frac(sin(dot(g, float2(39.3468, 11.135))) * 24634.6345);
float2 o = (float2(r1, r2) - 0.5) * 0.45;
float rad = 0.1 + 0.25 * r2;
float bead = (r1 < amount ? 1.0 : 0.0) * (1.0 - smoothstep(rad * 0.7, rad, length(f - o)));
'''
# the outer face: its normal looks away from the vehicle's middle line (upwards: away from its middle; the glass is in front of or
# behind it, beside it, or above); RainC: the model's middle in the part's frame, w 0 until the vehicle has wired it
OUTER = r'''
float3 d = P - RainC.xyz; d.z = d.z * 0.5 + 10;
return RainC.w > 0.5 ? smoothstep(-0.02, 0.06, dot(normalize(N), normalize(d * float3(1, 1.6, 1)))) : 0;
'''
TG = '/Game/Truck/M_TruckGlass'
m = open_asset(TG, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', True)
TLM = next((getattr(unreal.TranslucencyLightingMode, n) for n in dir(unreal.TranslucencyLightingMode) if n.upper().startswith('TLM_SURFACE')), None)
if TLM is not None:
    m.set_editor_property('translucency_lighting_mode', TLM)


def me(cls, x, y, **kw):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in kw.items():
        e.set_editor_property(k, v)
    return e


def vec(name, v, y):
    return me(unreal.MaterialExpressionVectorParameter, -1200, y, parameter_name=name, default_value=unreal.LinearColor(*v))


def sca(name, v, y):
    return me(unreal.MaterialExpressionScalarParameter, -1200, y, parameter_name=name, default_value=v)


def custom(code, out, ins, x, y):
    c = me(unreal.MaterialExpressionCustom, x, y, code=code, output_type=out, description='GLASS')
    cl = []
    for n in ins:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); cl.append(ci)
    c.set_editor_property('inputs', cl)
    return c


P = {'BC': vec('BC', (0.015, 0.022, 0.026, 0.38), -400), 'Origin': vec('Origin', (0, 0, 0, 0), -300), 'AxisF': vec('AxisF', (1, 0, 0, 0), -200),
     'B0': vec('B0', (0, 0, 0, 0), -100), 'B1': vec('B1', (0, 0, 0, 0), 0), 'B2': vec('B2', (0, 0, 0, 0), 100), 'Blow': sca('Blow', 0.0, 200),
     'WP': me(unreal.MaterialExpressionWorldPosition, -1200, 300), 'N': me(unreal.MaterialExpressionVertexNormalWS, -1200, 400)}
outs = {k: 'RGBA' for k in ('BC', 'Origin', 'AxisF', 'B0', 'B1', 'B2')}
bead = custom(BEADS + 'return bead;', O.CMOT_FLOAT1, ['Origin', 'AxisF', 'B0', 'B1', 'B2', 'Blow', 'WP', 'N'], -700, -200)
for n in ('Origin', 'AxisF', 'B0', 'B1', 'B2', 'Blow', 'WP', 'N'):
    con(P[n], bead, n, outs.get(n, ''))
mplo = me(unreal.MaterialExpressionTransformPosition, -1000, 560, transform_source_type=P_WORLD, transform_type=P_INST); con(P['WP'], mplo)
mnlo = me(unreal.MaterialExpressionTransform, -1000, 640, transform_source_type=S_WORLD, transform_type=T_INST); con(P['N'], mnlo)
rc = vec('RainC', (0, 0, 0, 0), 520)
outer = custom(OUTER, O.CMOT_FLOAT1, ['P', 'N', 'RainC'], -700, 560)
con(mplo, outer, 'P'); con(mnlo, outer, 'N'); con(rc, outer, 'RainC', 'RGBA')
call = me(unreal.MaterialExpressionMaterialFunctionCall, -400, 700)
if not call.set_material_function(mf):
    raise RuntimeError('MF_GlassRain not set')
con(outer, call, 'Outer')
con(mplo, call, 'PositionCm'); con(mnlo, call, 'PaneNormal')
con(sca('RainSeed', 0.0, 700), call, 'PaneSeed'); con(sca('RainOwn', 1.0, 760), call, 'Shelter')
for k, (pn, nm, dv) in enumerate((('RainLean', 'Lean', (0, 0, 0, 0)), ('WipA0', 'WiperA0', (0, 0, 0, 0)), ('WipB0', 'WiperB0', (0, 0, 0, 0)), ('WipC0', 'WiperC0', (0, 0, 0, 0)),
                                  ('WipA1', 'WiperA1', (0, 0, 0, 0)), ('WipB1', 'WiperB1', (0, 0, 0, 0)), ('WipC1', 'WiperC1', (0, 0, 0, 0)), ('WipT', 'WiperT', (1000, 1000, 1.5, 0)))):
    con(vec(pn, dv, 820 + 100 * k), call, nm, 'RGBA')
con(sca('WipH', 0.0, 1700), call, 'WiperH')
bc = custom('return lerp(lerp(BC.rgb, float3(0.55, 0.62, 0.68), bead * 0.6), float3(0.5, 0.56, 0.62), Lens);', O.CMOT_FLOAT3, ['BC', 'bead', 'Lens'], -100, -300)
con(P['BC'], bc, 'BC', 'RGBA'); con(bead, bc, 'bead'); con(call, bc, 'Lens', 'Lens')
oa = custom('return saturate(BC.a + bead * 0.35 + (1 - BC.a) * Rain);', O.CMOT_FLOAT1, ['BC', 'bead', 'Rain'], -100, -100)
con(P['BC'], oa, 'BC', 'RGBA'); con(bead, oa, 'bead'); con(call, oa, 'Rain', 'Opacity')
em = custom('return bead * float3(0.05, 0.055, 0.06);', O.CMOT_FLOAT3, ['bead'], -100, 100); con(bead, em, 'bead')
ro = custom('return max(0.02, Rough);', O.CMOT_FLOAT1, ['Rough'], -100, 250); con(call, ro, 'Rough', 'Roughness')
sp = custom('return lerp(0.9, 0.5, Wet);', O.CMOT_FLOAT1, ['Wet'], -100, 350); con(call, sp, 'Wet', 'Specular')
for e, prop in ((bc, MP.MP_BASE_COLOR), (oa, MP.MP_OPACITY), (em, MP.MP_EMISSIVE_COLOR), (ro, MP.MP_ROUGHNESS), (sp, MP.MP_SPECULAR)):
    mel.connect_material_property(e, '', prop)
mel.connect_material_property(call, 'Normal', MP.MP_NORMAL)
m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m)
st = mel.get_statistics(m)
log('M_TruckGlass: pixel instructions %s, samplers %s' % (st.num_pixel_shader_instructions, st.num_samplers))
save(m, TG)
# #endregion
log('ready: MPC_GlassRain, MF_GlassRain, M_TruckGlass')
unreal.SystemLibrary.quit_editor()
