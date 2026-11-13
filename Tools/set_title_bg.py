# The title screen's film drawn by Unreal itself (owner, 2026-11-13: the title's background stuttered in Unreal's web
# view, smooth on Babylon). The page (Airline-Manager-Pro-Babylon src/ui/splash.ts) drew it on a 2D canvas every frame;
# in Unreal's browser that runs mostly on the processor. Here the same film is one UI material, M_TitleBg, drawn on the
# graphics card behind the page (STitleBg, TitleBg.cpp feeds it the shot, the camera and the lights each frame): the
# same pictures, pans and zooms, the blur reveals and their sweeping bands, the glints, the colour leaks, the flashes,
# the glitch slices and the grain, every number copied from splash.ts (no cinema bars, as there). The neon logo, the menus, the
# captions, the scanlines and the vignette stay in the page, drawn over it as before.
# All the drawing is done in the page's own colour space (sRGB, as a browser's canvas blends), so it looks the same:
# the pictures are imported without sRGB conversion and the result turned to linear light only at the end.
#   python Tools/... none; UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_title_bg.py" -unattended
import unreal, os
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
def log(m): unreal.log('ADP_TITLEBG ' + m)
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DST = '/Game/Title'
if not eal.does_directory_exist(DST): eal.make_directory(DST)

# the pictures (Art/Title, made from the page's own .webp files and their blurred copies)
tasks = []
for f in sorted(os.listdir(os.path.join(ROOT, 'Art', 'Title'))):
    if not f.endswith('.png'): continue
    t = unreal.AssetImportTask()
    t.filename = os.path.join(ROOT, 'Art', 'Title', f); t.destination_path = DST; t.replace_existing = True; t.automated = True; t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for f in sorted(os.listdir(os.path.join(ROOT, 'Art', 'Title'))):
    if not f.endswith('.png'): continue
    tex = unreal.load_asset(DST + '/' + f[:-4])
    if not tex: log('missing ' + f); continue
    tex.set_editor_property('srgb', False)                       # blended as the page blends: in sRGB values
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP); tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    eal.save_loaded_asset(tex, only_if_is_dirty=False)
log('%d pictures' % len(tasks))

CODE = r"""
// ---- the film, one pixel (page pixels: 0..W, 0..H), as splash.ts draws it ----
float W = P0.x, H = P0.y, iw = P0.z, ih = P0.w;
float s = P1.x; float2 o = P1.yz; float mode = P1.w;           // mode: -1 nothing yet, 0 a picture, 1 the flight deck, 2 the flight
float t = P2.x, kick = P2.y, lvl = P2.z, rk = P2.w;            // rk: the reveal's progress (d shots), -1 none
float flip = P3.x, flash = P3.y, gl = P3.z, gseed = P3.w;
float2 isz = float2(iw, ih);
float3 BG = float3(5, 3, 12) / 255.0;
float2 pix = UV * float2(W, H);
float3 outc = 0;
int passes = gl > 0.02 ? 2 : 1;
for (int rp = 0; rp < 2; rp++)
{
  if (rp >= passes) break;
  float2 q = pix;
  // the glitch: six random slices shoved sideways, and a faint copy a few pixels over (screen)
  if (gl > 0.02)
  {
    if (rp == 1) q.x -= 6.0 * gl;
    for (int b = 0; b < 6; b++)
    {
      float r1 = frac(sin((b + 1) * 12.9898 + gseed * 78.233) * 43758.5453);
      float r2 = frac(sin((b + 1) * 39.346 + gseed * 11.135) * 24634.6345);
      float r3 = frac(sin((b + 1) * 73.156 + gseed * 52.235) * 13758.937);
      float y0 = r1 * H, hh = H * (0.01 + r2 * 0.05);
      if (q.y >= y0 && q.y < y0 + hh) q.x -= (r3 - 0.5) * W * 0.06 * gl;
    }
  }
  float3 c = BG;
  float2 ip = (q - o) / s;                                     // the picture's own pixels
  if (mode >= 0)
  {
    // the pictures: c over c, each only inside its own frame
    #define TAKE(TX, U) ( (all((U) >= 0) && all((U) <= isz)) ? TX.SampleLevel(TX##Sampler, (U) / isz, log2(max(1.0, 1.0 / s))) : float4(0,0,0,0) )
    float4 a4, b4;
    if (mode < 0.5) { a4 = TAKE(Img, ip); c = lerp(c, a4.rgb, a4.a); }
    else if (mode < 1.5)
    {
      // the flight deck: the view outside drifting and settling behind the window frame
      float2 V = float2(800, 250); float ang = P4.z, cs = cos(-ang), sn = sin(-ang);
      float2 d = ip - (V + float2(P4.x, P4.w)); d = float2(cs * d.x - sn * d.y, sn * d.x + cs * d.y) / P4.y;
      b4 = TAKE(Img2, d + V); c = lerp(c, b4.rgb, b4.a);
      a4 = TAKE(Img, ip); c = lerp(c, a4.rgb, a4.a);
    }
    else
    {
      // the flight: the clouds sliding by, the aircraft riding gently over them
      float2 u = (ip - float2(iw * 0.5 + P4.x, ih * 0.5)) / 1.2 + isz * 0.5;
      a4 = TAKE(Img, u); c = lerp(c, a4.rgb, a4.a);
      float rr = P4.z, cs = cos(-rr), sn = sin(-rr);
      float2 d = ip - float2(700, 380 + P4.y); d = float2(cs * d.x - sn * d.y, sn * d.x + cs * d.y);
      b4 = TAKE(Img2, d + float2(700, 380)); c = lerp(c, b4.rgb, b4.a);
      // the wisps of cloud drifting past (screen)
      for (int i = 0; i < 5; i++)
      {
        float sp = 0.18 + i * 0.05, wx = (fmod(t * sp + i * 0.37, 1.6) - 0.3) * W, wy = H * (0.72 + 0.06 * sin(i * 7.3) + 0.04 * i), wr = H * (0.22 + 0.05 * (i % 3));
        float g = saturate(1.0 - length(float2((q.x - wx) / 2.6, (q.y - wy) / 0.5)) / wr) * 0.10;
        c = c + g * float3(1.0, 190.0 / 255, 200.0 / 255) * (1 - c);
      }
    }
    // a detail shot: the blurred picture round the edges, the dark sweep and its two bands of light
    if (rk >= 0)
    {
      float4 bl;
      if (mode > 1.5)
      {
        float2 u = (ip - float2(iw * 0.5 + P4.x, ih * 0.5)) / 1.2 + isz * 0.5;
        bl = TAKE(Blur, u);
        float rr = P4.z, cs = cos(-rr), sn = sin(-rr);
        float2 d = ip - float2(700, 380 + P4.y); d = float2(cs * d.x - sn * d.y, sn * d.x + cs * d.y);
        float4 b2 = TAKE(Blur2, d + float2(700, 380)); bl.rgb = lerp(bl.rgb * bl.a, b2.rgb, b2.a); bl.a = saturate(bl.a + b2.a * (1 - bl.a)); bl.rgb = bl.a > 0 ? bl.rgb / bl.a : 0;
      }
      else bl = TAKE(Blur, ip);
      float r0 = min(W, H) * 0.18, r1 = max(W, H) * 0.62, dist = length(q - float2(W, H) * 0.5);
      bl.a *= saturate((dist - r0) / (r1 - r0));
      c = lerp(c, bl.rgb, bl.a);
      float k = rk, dir = flip > 0.5 ? -1 : 1;
      float E1 = saturate(min(1, k / 0.55)); E1 = E1 * E1 * (3 - 2 * E1);
      float edge = -0.35 + 1.8 * E1, soft = 0.32;
      float gx0 = dir > 0 ? 0 : W, gx1 = dir > 0 ? W : 0;
      float2 A = float2(gx0, H * 0.15), B = float2(gx1, H * 0.85), AB = B - A;
      float p = saturate(dot(q - A, AB) / dot(AB, AB));
      float s1 = saturate(edge - soft), s2 = saturate(edge), s3 = saturate(edge + 0.18);
      float da = p <= s1 ? 0 : p <= s2 ? 0.55 * (p - s1) / max(1e-4, s2 - s1) : p <= s3 ? lerp(0.55, 0.96, (p - s2) / max(1e-4, s3 - s2)) : 0.96;
      c = lerp(c, float3(4, 2, 10) / 255.0, da);
      float2 A2 = float2(gx0, H * 0.1), B2 = float2(gx1, H * 0.9), AB2 = B2 - A2;
      float p2 = saturate(dot(q - A2, AB2) / dot(AB2, AB2));
      for (int n = 0; n < 2; n++)
      {
        float ke = saturate((k - 0.45) / 0.15); ke = ke * ke * (3 - 2 * ke);
        float kb = saturate((k - 0.55) / 0.42); kb = kb * kb * (3 - 2 * kb);
        float pos = n == 0 ? edge - 0.02 : -0.3 + 1.7 * kb, wdt = n == 0 ? 0.1 : 0.14, ba = n == 0 ? 0.55 * (1 - ke) : 0.32;
        float3 bc = n == 0 ? float3(1.0, 214.0 / 255, 236.0 / 255) : float3(150.0 / 255, 240.0 / 255, 1.0);
        if (pos < -0.3 || pos > 1.3 || ba <= 0) continue;
        float u0 = saturate(pos - wdt), u1 = saturate(pos), u2 = saturate(pos + wdt * 0.6);
        float bb = p2 <= u0 ? 0 : p2 <= u1 ? ba * (p2 - u0) / max(1e-4, u1 - u0) : p2 <= u2 ? ba * (1 - (p2 - u1) / max(1e-4, u2 - u1)) : 0;
        c = c + bb * bc * (1 - c);
      }
    }
    // the glints: beacons, strobes, the sun's spark, each a glow and a long thin flare (added light)
    for (int l = 0; l < 3; l++)
    {
      float4 L = l == 0 ? L0 : l == 1 ? L1 : L2; float3 LC = l == 0 ? LC0 : l == 1 ? LC1 : LC2;
      if (L.z <= 0) continue;
      float2 X = o + L.xy * s; float r = 26.0 * s * (0.6 + L.z * 0.6);
      float d = length(q - X) / r;
      float ga = d < 0.25 ? lerp(0.95 * min(1, L.z), 0.35 * L.z, d / 0.25) : d < 1 ? lerp(0.35 * L.z, 0, (d - 0.25) / 0.75) : 0;
      c += LC * ga;
      float2 e2 = float2((q.x - X.x) / 9.0, (q.y - X.y) / 0.32);
      float fa = saturate(1 - length(e2) / r) * 0.4 * L.z;
      if (abs(e2.x) < r && abs(e2.y) < r) c += float3(140.0 / 255, 200.0 / 255, 1.0) * fa;
      c = min(c, 1);
    }
  }
  // the light leaks, pink, cyan and amber, breathing with the beat (screen)
  {
    float2 Cc[3] = { float2(W * (0.08 + 0.06 * sin(t * 0.21)), H * (0.2 + 0.1 * sin(t * 0.17))), float2(W * (0.95 - 0.05 * sin(t * 0.19)), H * (0.85 + 0.08 * cos(t * 0.23))), float2(W * 0.6, -H * 0.1) };
    float Rr[3] = { W * 0.55, W * 0.5, W * 0.45 };
    float Aa[3] = { 0.13 + 0.12 * kick, 0.10 + 0.08 * lvl, 0.06 + 0.06 * kick };
    float3 Kc[3] = { float3(1.0, 40.0 / 255, 170.0 / 255), float3(40.0 / 255, 220.0 / 255, 1.0), float3(1.0, 150.0 / 255, 60.0 / 255) };
    for (int j = 0; j < 3; j++) { float g = Aa[j] * saturate(1 - length(q - Cc[j]) / Rr[j]); c = c + g * Kc[j] * (1 - c); }
  }
  // the flash on every cut
  c = lerp(c, float3(1.0, 236.0 / 255, 250.0 / 255), saturate(flash * 0.75));
  if (rp == 0) outc = c; else outc = outc + 0.35 * gl * c * (1 - outc);
}
// the film grain (overlay); no cinema bars: the film fills the whole screen (owner, 2026-11-09, as the PC title page)
float nz = (128.0 + (frac(sin(dot(floor(pix + gseed * 256.0), float2(12.9898, 78.233))) * 43758.5453) - 0.5) * 120.0) / 255.0;
float3 ov = outc < 0.5 ? 2 * outc * nz : 1 - 2 * (1 - outc) * (1 - nz);
outc = lerp(outc, ov, 0.09);
// from the page's sRGB values to the linear light Unreal draws with
return pow(saturate(outc), 2.2);
"""

path = DST + '/M_TitleBg'
m = unreal.load_asset(path)
if m: mel.delete_all_material_expressions(m)
else: m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_TitleBg', DST, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
cu = E(unreal.MaterialExpressionCustom, -300, 0)
cu.set_editor_property('code', CODE); cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3); cu.set_editor_property('description', 'TITLE')
names = ['UV', 'Img', 'Img2', 'Blur', 'Blur2', 'P0', 'P1', 'P2', 'P3', 'P4', 'L0', 'L1', 'L2', 'LC0', 'LC1', 'LC2']
ins = []
for n in names:
    ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
cu.set_editor_property('inputs', ins)
uv = E(unreal.MaterialExpressionTextureCoordinate, -700, -200)
mel.connect_material_expressions(uv, '', cu, 'UV')
first = unreal.load_asset(DST + '/T_Title_t1')
y = -100
for n in ['Img', 'Img2', 'Blur', 'Blur2']:
    tp = E(unreal.MaterialExpressionTextureObjectParameter, -700, y); y += 120
    tp.set_editor_property('parameter_name', n)
    if first: tp.set_editor_property('texture', first)
    tp.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    mel.connect_material_expressions(tp, '', cu, n)
for n in ['P0', 'P1', 'P2', 'P3', 'P4', 'L0', 'L1', 'L2', 'LC0', 'LC1', 'LC2']:
    vp = E(unreal.MaterialExpressionVectorParameter, -700, y); y += 90
    vp.set_editor_property('parameter_name', n)
    mel.connect_material_expressions(vp, 'RGBA', cu, n)   # (all four numbers: '' gives only three)
mel.connect_material_property(cu, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)
log('ready: ' + path)
unreal.SystemLibrary.quit_editor()
