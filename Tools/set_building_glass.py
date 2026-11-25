# After it: Tools/set_building_rain.py (it puts the rain back on the panes, to-do 295)
# The buildings' window glass (owner, 2026-11-23: clear, reflective "but not overdone", and "slightly easier to look out than to
# look in"): one master, M_BuildingGlass, for every pane of the terminal (MI_T_glass, glassBlue, glassOuter, glassVault).
#  - clear, colourless with the faintest green, mirror-smooth; nothing milky: the glass has no light of its own, only what it
#    mirrors (a metal's reflection with the glass's own reflectance, added on top of what is seen through: AlphaComposite),
#  - real Fresnel: a little straight on, more and more at a slant, and that much less is seen through,
#  - the reflectance by the side you stand on: more from outside, little from inside (T_GlassInside, Tools/make_glass_inside.py,
#    looked up 40 cm to your side of the pane; under a glass roof: below it is inside); inside on both sides: as from inside;
#    outside on both sides (the jetways): in between,
#  - the reflections: Lumen's front layer at High and above, screen space below, as the glass had.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_building_glass.py" -unattended -nullrhi
import unreal, os, stat
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MP = unreal.MaterialProperty


def log(*a): unreal.log('ADP_GLASS ' + ' '.join(str(x) for x in a))


def writable(p):
    f = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), p.replace('/Game/', '') + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)


DIR = '/Game/Terminal/Masters'
# ---- the inside picture ----
TEX = DIR + '/T_GlassInside'; writable(TEX)
t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Terminal', 'Glass', 'T_GlassInside.png'); t.destination_path = DIR; t.destination_name = 'T_GlassInside'
t.replace_existing = True; t.automated = True; t.save = False
at.import_asset_tasks([t])
tex = unreal.load_asset(TEX)
for k, v in (('srgb', False), ('compression_settings', unreal.TextureCompressionSettings.TC_MASKS), ('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
             ('never_stream', True), ('address_x', unreal.TextureAddress.TA_CLAMP), ('address_y', unreal.TextureAddress.TA_CLAMP)): tex.set_editor_property(k, v)
eal.save_loaded_asset(tex, only_if_is_dirty=False)
# ---- the master ----
MASTER = DIR + '/M_BuildingGlass'; writable(MASTER)
m = unreal.load_asset(MASTER) if eal.does_asset_exist(MASTER) else at.create_asset('M_BuildingGlass', DIR, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', True)
m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
for n in ('used_with_instanced_static_meshes', 'used_with_nanite'): m.set_editor_property(n, True)
for n in ('allow_front_layer_translucency', 'screen_space_reflections'):
    try: m.set_editor_property(n, True)
    except Exception as e: log('no', n)


def ex(cls, x, y, **k):
    e = mel.create_material_expression(m, cls, x, y)
    for a, b in k.items(): e.set_editor_property(a, b)
    return e


def sca(n, v, y): return ex(unreal.MaterialExpressionScalarParameter, -900, y, parameter_name=n, default_value=v)


CODE = r"""
// which side: level, 40 cm from the pane to your side (a) and to the other (b); 1 = inside
float3 N = normalize(WN), V = normalize(CV);
float s = dot(N, V) < 0 ? -1 : 1;
float a, b;
if (abs(N.z) > 0.6) { a = V.z < 0 ? 1 : 0; b = 1 - a; }
else
{
  float2 o = normalize(N.xy + float2(0.00001, 0)) * s * 40.0;
  float2 p = WP.xy + o, q = WP.xy - o;
  float2 x = Texture2DSampleLevel(In, InSampler, float2((p.x * 0.01 + 250.0) / 819.2, (8.0 - p.y * 0.01) / 102.4), 0).rg;
  float2 y = Texture2DSampleLevel(In, InSampler, float2((q.x * 0.01 + 250.0) / 819.2, (8.0 - q.y * 0.01) / 102.4), 0).rg;
  a = WP.z > 585.0 ? x.g : x.r; b = WP.z > 585.0 ? y.g : y.r;
}
float f0 = Mid + (Out - Mid) * saturate(b - a);
f0 = lerp(f0, Ins, saturate(a));
float fr = f0 + (1 - f0) * pow(saturate(1 - abs(dot(N, V))), 5);
float h = lerp(HazeIn, HazeOut, saturate(b - a));
return float2(f0, saturate(h + fr * 0.85));
"""
NAMES = ['WP', 'WN', 'CV', 'In', 'Out', 'Ins', 'Mid', 'HazeOut', 'HazeIn']
ci = []
for n in NAMES:
    q = unreal.CustomInput(); q.set_editor_property('input_name', n); ci.append(q)
c = ex(unreal.MaterialExpressionCustom, -400, 0, code=CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2, description='Glass')
c.set_editor_property('inputs', ci)
src = {'WP': ex(unreal.MaterialExpressionWorldPosition, -900, -400), 'WN': ex(unreal.MaterialExpressionVertexNormalWS, -900, -340), 'CV': ex(unreal.MaterialExpressionCameraVectorWS, -900, -280),
       'In': ex(unreal.MaterialExpressionTextureObjectParameter, -900, -200, parameter_name='Inside', texture=tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
       'Out': sca('ReflectOutside', 0.14, 0), 'Ins': sca('ReflectInside', 0.035, 70), 'Mid': sca('ReflectBetween', 0.06, 140),
       'HazeOut': sca('DarkOutside', 0.10, 210), 'HazeIn': sca('DarkInside', 0.02, 280)}
for n in NAMES: mel.connect_material_expressions(src[n], '', c, n)


def mask(r, g, y):
    e = ex(unreal.MaterialExpressionComponentMask, -150, y, r=r, g=g, b=False, a=False); mel.connect_material_expressions(c, '', e, ''); return e


tint = ex(unreal.MaterialExpressionVectorParameter, -400, -250, parameter_name='Tint', default_value=unreal.LinearColor(0.97, 1.0, 0.985, 1))
mul = ex(unreal.MaterialExpressionMultiply, 0, -150)
mel.connect_material_expressions(mask(True, False, 0), '', mul, 'A'); mel.connect_material_expressions(tint, '', mul, 'B')
mel.connect_material_property(mul, '', MP.MP_BASE_COLOR)
mel.connect_material_property(mask(False, True, 80), '', MP.MP_OPACITY)
mel.connect_material_property(sca('Roughness', 0.02, 400), '', MP.MP_ROUGHNESS)
one = ex(unreal.MaterialExpressionConstant, -150, 300, r=1.0); mel.connect_material_property(one, '', MP.MP_METALLIC)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
# ---- the panes ----
for n in ('glass', 'glassOuter', 'glassVault', 'glassBlue'):
    path = '/Game/Terminal/Materials/MI_T_' + n; writable(path)
    mi = unreal.load_asset(path)
    mel.clear_all_material_instance_parameters(mi)
    mel.set_material_instance_parent(mi, m)
    if n == 'glassBlue': mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(0.88, 0.96, 1.0, 1))
    mel.update_material_instance(mi); eal.save_loaded_asset(mi, only_if_is_dirty=False)
    log(n, mi.get_editor_property('parent').get_name())
# ---- the wave roof's sheets: whole in the panes' reflections (Lumen sees them by their distance fields and cards) ----
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for k in ('soffit', 'top', 'fascia'):
    for s in 'wme':
        p = '/Game/Terminal/Meshes/wave_%s_%s' % (k, s)
        sm = unreal.load_asset(p)
        if not sm: continue
        b = sub.get_lod_build_settings(sm, 0)
        if b.get_editor_property('distance_field_resolution_scale') == 3.0: continue
        writable(p)
        for kk, v in (('distance_field_resolution_scale', 3.0), ('generate_distance_field_as_if_two_sided', True), ('max_lumen_mesh_cards', 32)): b.set_editor_property(kk, v)
        sub.set_lod_build_settings(sm, 0, b); eal.save_loaded_asset(sm, only_if_is_dirty=False); log('distance field', k, s)
log('done')
