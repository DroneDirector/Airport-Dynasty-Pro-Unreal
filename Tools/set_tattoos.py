# Tattoos on the MetaHumans (owner, 2026-11-15): the shared MetaHuman skin master (M_skin_unified_baked, the body's and
# the head's) gets a tattoo layer - a picture in the skin's own UV layout (Art/Tattoos/place_tattoos.py) whose ink lies
# under the skin: the skin's colour darkened toward the ink where there is ink, a little soft, the skin's detail and
# light through it. Parameter "Tattoo" (default: none). The pictures are imported to /Game/People/Tattoos.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_tattoos.py" -unattended
# The samples are shown at run time: UnrealEditor.exe ... -game -UIShot -UIBody -TattooSample
import os, stat, sys
import unreal
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(ROOT, 'Art', 'Tattoos', 'Placed')
DEST = '/Game/People/Tattoos'
def log(m): unreal.log('ADP_TAT ' + m)
for d in (os.path.join(ROOT, 'Content', 'People', 'Common', 'Lookdev_UHM', 'Skin', 'Materials'),):
    for dp, _, fs in os.walk(d):
        for f in fs: os.chmod(os.path.join(dp, f), stat.S_IWRITE | stat.S_IREAD)

# ---- the pictures ----
if not eal.does_directory_exist(DEST): eal.make_directory(DEST)
# (T_Tattoo_None.png, a see-through 4 x 4, is made by Art/Tattoos/place_tattoos.py: Unreal's Python has no numpy)
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith('.png'): continue
    t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, f); t.destination_path = DEST; t.automated = True; t.replace_existing = True; t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    name = os.path.splitext(os.path.basename(t.filename))[0]
    tex = unreal.load_asset(DEST + '/' + name)
    if not tex: continue
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP); tex.set_editor_property('address_y', unreal.TextureAddress.TA_WRAP)
    eal.save_loaded_asset(tex)
log('pictures: %d' % len(tasks))

# ---- the skin master: the tattoo layer before its attributes go out ----
MP = '/Game/People/Common/Lookdev_UHM/Skin/Materials/M_skin_unified_baked'
m = unreal.load_asset(MP)
done = any(isinstance(e, unreal.MaterialExpressionTextureSampleParameter2D) and str(e.get_editor_property('parameter_name')) == 'Tattoo'
           for e in unreal.MaterialEditingLibrary.get_material_selected_nodes(m) or []) if False else False
src_node = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
src_out = mel.get_material_property_input_node_output_name(m, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES) if hasattr(mel, 'get_material_property_input_node_output_name') else ''
log('attributes come from %s (%s)' % (src_node.get_class().get_name() if src_node else None, src_out))
if src_node and src_node.get_class().get_name() == 'MaterialExpressionMakeMaterialAttributes' and src_node.get_editor_property('desc') == 'ADP tattoo':
    log('tattoo layer already there')
else:
    X = 1200
    brk = mel.create_material_expression(m, unreal.MaterialExpressionBreakMaterialAttributes, X, 0)
    if not mel.connect_material_expressions(src_node, src_out or '', brk, ''): raise RuntimeError('tattoo layer: the skin attributes did not connect (the Break input is "Attr")')
    mk = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, X + 700, 0)
    mk.set_editor_property('desc', 'ADP tattoo')
    tat = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, X, 600)
    tat.set_editor_property('parameter_name', 'Tattoo')
    tat.set_editor_property('texture', unreal.load_asset(DEST + '/T_Tattoo_None'))
    tat.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tat.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, X - 200, 600)
    mel.connect_material_expressions(uv, '', tat, 'UVs')
    # the ink under the skin: base * (1 - a) + (ink * 0.85 + base * 0.12) * a; a = picture alpha * strength
    amt = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, X, 900); amt.set_editor_property('parameter_name', 'TattooStrength'); amt.set_editor_property('default_value', 0.92)
    a = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, X + 250, 800)
    mel.connect_material_expressions(tat, 'A', a, 'A'); mel.connect_material_expressions(amt, '', a, 'B')
    inkk = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, X + 250, 600); inkk.set_editor_property('const_b', 0.85)
    mel.connect_material_expressions(tat, 'RGB', inkk, 'A')
    basek = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, X + 250, 700); basek.set_editor_property('const_b', 0.12)
    mel.connect_material_expressions(brk, 'BaseColor', basek, 'A')
    inked = mel.create_material_expression(m, unreal.MaterialExpressionAdd, X + 400, 650)
    mel.connect_material_expressions(inkk, '', inked, 'A'); mel.connect_material_expressions(basek, '', inked, 'B')
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, X + 550, 400)
    mel.connect_material_expressions(brk, 'BaseColor', lerp, 'A'); mel.connect_material_expressions(inked, '', lerp, 'B'); mel.connect_material_expressions(a, '', lerp, 'Alpha')
    # every attribute through as it was, the colour through the ink
    names = ['BaseColor', 'Metallic', 'Specular', 'Roughness', 'Anisotropy', 'EmissiveColor', 'Opacity', 'OpacityMask', 'Normal', 'Tangent',
             'WorldPositionOffset', 'SubsurfaceColor', 'ClearCoat', 'ClearCoatRoughness', 'AmbientOcclusion', 'Refraction',
             'CustomizedUV0', 'CustomizedUV1', 'CustomizedUV2', 'CustomizedUV3', 'CustomizedUV4', 'CustomizedUV5', 'CustomizedUV6', 'CustomizedUV7',
             'PixelDepthOffset', 'ShadingModel', 'Displacement']
    ok = 0
    for n in names:
        try:
            if n == 'BaseColor': r = mel.connect_material_expressions(lerp, '', mk, 'BaseColor')
            else: r = mel.connect_material_expressions(brk, n, mk, n)
            ok += 1 if r else 0
        except Exception as e:
            log('attribute %s: %s' % (n, e))
    mel.connect_material_property(mk, '', unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    log('tattoo layer added (%d attributes through)' % ok)

# ---- the tattoo places (owner, 2026-11-15: tattoos chosen in the creator and at the tattooist, any design on any
# place - both arms, legs, fingers, the neck's sides, back and throat - and removable): six layers, each a place map in
# the skin's layout (Art/Tattoos/make_spots.py: angle round the limb as cos/sin in G/B, the way down in R, the place in
# A) and a design picture in that place's frame (Art/Tattoos/make_designs.py). A person's own material copy sets the
# layers (ADPTattoo::Wear); "TattooOn" 0 skips them all, so people without tattoos pay nothing.
def import_folder(src, dest, hdr):
    if not eal.does_directory_exist(dest): eal.make_directory(dest)
    ts = []
    for f in sorted(os.listdir(src)):
        if not f.endswith('.png'): continue
        t = unreal.AssetImportTask(); t.filename = os.path.join(src, f); t.destination_path = dest; t.automated = True; t.replace_existing = True; t.save = True
        ts.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(ts)
    for t in ts:
        tex = unreal.load_asset(dest + '/' + os.path.splitext(os.path.basename(t.filename))[0])
        if not tex: continue
        tex.set_editor_property('srgb', not hdr)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR if hdr else unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP); tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
        eal.save_loaded_asset(tex)
    return len(ts)
log('places %d, designs %d' % (import_folder(os.path.join(SRC, 'Spots'), DEST + '/Spots', True),
                               import_folder(os.path.join(ROOT, 'Art', 'Tattoos', 'Designs'), DEST + '/Designs', False)))
LAYERS = 6
# (one shared sampler for all twelve pictures: a sampler of their own each took the skin past the 16 a shader may have,
#  its shader failed to compile and the release's cook stopped, 2026-11-15)
_b = ['float3 c = Base;', '[branch] if (On > 0.5)', '{', '    float2 uv = frac(UV); float4 s; float4 d; float u; float a;']
for i in range(LAYERS):
    _b.append('    s = Spot%d.SampleLevel(View.SharedBilinearClampedSampler, uv, 0);' % i)
    _b.append('    if (s.a > 0.5) { u = frac(atan2(s.b * 2 - 1, s.g * 2 - 1) * 0.15915494 + 0.5); d = Design%d.SampleLevel(View.SharedBilinearClampedSampler, float2(u, s.r), 0); a = d.a * Strength; c = lerp(c, d.rgb * 0.85 + c * 0.12, a); }' % i)
_b += ['}', 'return c;']
LAYER_CODE = chr(10).join(_b)
mk = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
lerp = mel.get_inputs_for_material_expression(m, mk)[0]
A = mel.get_inputs_for_material_expression(m, lerp)[0]
if A and A.get_class().get_name() == 'MaterialExpressionCustom' and A.get_editor_property('description') == 'ADP tattoo layers':
    log('tattoo places already there')
    A.set_editor_property('code', LAYER_CODE)
else:
    brk = A
    X, Y = 600, 1300
    cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, X + 900, Y)
    cu.set_editor_property('description', 'ADP tattoo layers')
    cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ['Base', 'UV', 'On', 'Strength'] + ['Spot%d' % i for i in range(LAYERS)] + ['Design%d' % i for i in range(LAYERS)]
    ins = []
    for n in names:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
    cu.set_editor_property('inputs', ins)
    cu.set_editor_property('code', LAYER_CODE)
    if not mel.connect_material_expressions(brk, 'BaseColor', cu, 'Base'): raise RuntimeError('tattoo places: Base did not connect')
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, X, Y)
    mel.connect_material_expressions(uv, '', cu, 'UV')
    on = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, X, Y + 100); on.set_editor_property('parameter_name', 'TattooOn'); on.set_editor_property('default_value', 0.0)
    mel.connect_material_expressions(on, '', cu, 'On')
    st = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, X, Y + 200); st.set_editor_property('parameter_name', 'TattooLayerStrength'); st.set_editor_property('default_value', 0.92)
    mel.connect_material_expressions(st, '', cu, 'Strength')
    nspot = unreal.load_asset(DEST + '/Spots/T_TatSpot_None'); ndes = unreal.load_asset(DEST + '/Designs/T_TatDesign_None')
    for i in range(LAYERS):
        for kind, tex, y in (('Spot', nspot, Y + 300 + i * 220), ('Design', ndes, Y + 410 + i * 220)):
            p = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, X, y)
            p.set_editor_property('parameter_name', 'Tat%s_%d' % (kind, i)); p.set_editor_property('texture', tex)
            p.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if kind == 'Spot' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
            p.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
            if not mel.connect_material_expressions(p, '', cu, '%s%d' % (kind, i)): raise RuntimeError('tattoo places: %s%d did not connect' % (kind, i))
    if not mel.connect_material_expressions(cu, '', lerp, 'A'): raise RuntimeError('tattoo places: the layers did not reach the skin colour')
    log('tattoo places added (%d layers)' % LAYERS)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)

# ---- no tattoo is ever stored on a person's materials: the game puts each person's on at run time (ADPTattoo::Apply,
# a per-person material copy). An older run of this script stored the samples on the owner's character - they come off.
def clear(mi):
    vals = list(mi.get_editor_property('texture_parameter_values'))
    keep = [v for v in vals if str(v.get_editor_property('parameter_info').get_editor_property('name')) != 'Tattoo']
    if len(keep) != len(vals):
        mi.set_editor_property('texture_parameter_values', keep); mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, only_if_is_dirty=False); return 1
    return 0
cleared = 0
for folder in ('/Game/People/Built/MH_900_player/Body/Materials', '/Game/People/Built/MH_900_player/Face/Materials', '/Game/People/Common/Lookdev_UHM/Skin/Materials/Baked'):
    for p in eal.list_assets(folder, recursive=False):
        mi = unreal.load_asset(p)
        if isinstance(mi, unreal.MaterialInstanceConstant): cleared += clear(mi)
log('stored tattoos taken off %d materials' % cleared)
unreal.SystemLibrary.quit_editor()
