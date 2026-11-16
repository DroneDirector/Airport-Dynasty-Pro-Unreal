# The terminal's LED adverts into the game (to-do 52; after Art/Adverts/make_adverts.py and make_led_screen.py):
#   /Game/Adverts/T_AdvertAtlas   our eight made-up brands, 4 x 2 (sRGB, no mips beyond what the screens need)
#   /Game/Adverts/M_LedAdvert     the LED face: shows advert A, wipes over to advert B as Blend goes 0 -> 1 (per copy:
#                                 custom data 0 A, 1 B, 2 Blend - the game drives them, Adverts.cpp), each advert slowly
#                                 drifting and zooming (moving, never still), a light sheen sweeping now and then, the
#                                 LED dots showing close up (fading out further off - no moire), bright as a real LED wall
#   /Game/Adverts/SM_LedScreen    the cabinet: frame the jetways' dark metal, the face M_LedAdvert; Nanite
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_adverts.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'Adverts'); DEST = '/Game/Adverts'
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary
def log(*a): unreal.log('ADP_ADS ' + ' '.join(str(x) for x in a))
for _f in glob.glob(os.path.join(ROOT, 'Content', 'Adverts', '*.uasset')): os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)
if not EAL.does_directory_exist(DEST): EAL.make_directory(DEST)
# the atlas
t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, 'AdvertAtlas.png'); t.destination_path = DEST; t.destination_name = 'T_AdvertAtlas'
t.automated = True; t.replace_existing = True; t.save = True
AT.import_asset_tasks([t])
tex = unreal.load_asset(DEST + '/T_AdvertAtlas')
tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
tex.set_editor_property('never_stream', True)   # (sampled in a custom node: the streamer can't see how big it is drawn - kept whole)
EAL.save_loaded_asset(tex, only_if_is_dirty=False)
# the material
MP = DEST + '/M_LedAdvert'
m = unreal.load_asset(MP) if EAL.does_asset_exist(MP) else AT.create_asset('M_LedAdvert', DEST, unreal.Material, unreal.MaterialFactoryNew())
MEL.delete_all_material_expressions(m)
E = lambda cls, x, y: MEL.create_material_expression(m, cls, x, y)
uv = E(unreal.MaterialExpressionTextureCoordinate, -1400, 0)
tm = E(unreal.MaterialExpressionTime, -1400, 100)
cd = []
for i in range(3):
    c = E(unreal.MaterialExpressionPerInstanceCustomData, -1400, 200 + i * 80); c.set_editor_property('data_index', i); cd.append(c)
rnd = E(unreal.MaterialExpressionPerInstanceRandom, -1400, 460)
pd = E(unreal.MaterialExpressionPixelDepth, -1400, 540)
to = E(unreal.MaterialExpressionTextureObjectParameter, -1400, 620); to.set_editor_property('parameter_name', 'Atlas'); to.set_editor_property('texture', tex)
gl = E(unreal.MaterialExpressionScalarParameter, -1400, 720); gl.set_editor_property('parameter_name', 'Glow'); gl.set_editor_property('default_value', 1.5)
c = E(unreal.MaterialExpressionCustom, -800, 0)
c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
c.set_editor_property('code', r"""
float t = Time;
// one advert's place in the 4 x 2 atlas, its picture drifting and breathing slowly (moving, never a still)
float2 cellA = float2(fmod(A, 4.0), floor(A / 4.0)), cellB = float2(fmod(B, 4.0), floor(B / 4.0));
float z = 0.955 + 0.03 * sin(t * 0.21 + Rnd * 6.3);
float2 drift = float2(sin(t * 0.13 + Rnd * 4.0), cos(t * 0.11 + Rnd * 2.0)) * 0.012;
float2 q = (UV - 0.5) * z + 0.5 + drift;
q = clamp(q, 0.002, 0.998);
float3 ca = Tx.SampleLevel(TxSampler, (cellA + q) / float2(4.0, 2.0), 0).rgb;
float3 cb = Tx.SampleLevel(TxSampler, (cellB + q) / float2(4.0, 2.0), 0).rgb;
// the change-over: a soft wipe from left to right
float w = smoothstep(Blend * 1.2 - 0.2, Blend * 1.2, UV.x);
float3 col = lerp(cb, ca, w);
// a light sheen crossing the screen every 12 s or so
float sw = frac(t / 12.0 + Rnd);
float sheen = exp(-pow((UV.x + UV.y * 0.4 - sw * 2.2 + 0.3) / 0.06, 2.0)) * 0.25;
col += sheen;
// the LED dots: a 384 x 216 grid, seen close up only (gone from about 8 m)
float2 g = frac(UV * float2(384.0, 216.0)) - 0.5;
float dotm = smoothstep(0.5, 0.25, length(g));
float near = saturate(1.0 - PD / 800.0);
col *= lerp(1.0, 0.45 + 0.75 * dotm, near * 0.8);
return col * Glow;
""")
ins = []
for n in ('UV', 'Time', 'A', 'B', 'Blend', 'Rnd', 'PD', 'Tx', 'Glow'):
    ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
c.set_editor_property('inputs', ins)
for a_, n in ((uv, 'UV'), (tm, 'Time'), (cd[0], 'A'), (cd[1], 'B'), (cd[2], 'Blend'), (rnd, 'Rnd'), (pd, 'PD'), (to, 'Tx'), (gl, 'Glow')):
    MEL.connect_material_expressions(a_, '', c, n)
MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
bc = E(unreal.MaterialExpressionConstant3Vector, -500, 200); bc.set_editor_property('constant', unreal.LinearColor(0.01, 0.01, 0.012, 1))
MEL.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
r = E(unreal.MaterialExpressionConstant, -500, 300); r.set_editor_property('r', 0.22)
MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
m.set_editor_property('used_with_instanced_static_meshes', True)
m.set_editor_property('used_with_nanite', True)
MEL.recompile_material(m); EAL.save_asset(MP)
log('material made')
# the cabinet
path = DEST + '/SM_LedScreen'
if EAL.does_asset_exist(path): EAL.delete_asset(path)
o = unreal.FbxImportUI()
o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
sd = o.get_editor_property('static_mesh_import_data')
sd.set_editor_property('combine_meshes', True); sd.set_editor_property('generate_lightmap_u_vs', False); sd.set_editor_property('auto_generate_collision', True)
sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, 'SM_LedScreen.fbx'); t.destination_path = DEST
t.automated = True; t.replace_existing = True; t.save = True; t.options = o
AT.import_asset_tasks([t])
sm = unreal.load_asset(path)
MAP = {'AdFrame': '/Game/Terminal/Materials/MI_T_jwDark', 'AdScreen': MP}
mats = []
for s_ in sm.get_editor_property('static_materials'):
    slot = str(s_.get_editor_property('material_slot_name'))
    mi = unreal.load_asset(MAP.get(slot, MAP['AdFrame']))
    if not mi: log('MISSING material for', slot)
    s_.set_editor_property('material_interface', mi); mats.append(s_)
sm.set_editor_property('static_materials', mats)
ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', True); sm.set_editor_property('nanite_settings', ns)
EAL.save_loaded_asset(sm)
log('SM_LedScreen', [(str(s_.get_editor_property('material_slot_name')), s_.get_editor_property('material_interface').get_name()) for s_ in mats], sm.get_bounds().box_extent)
unreal.SystemLibrary.quit_editor()
