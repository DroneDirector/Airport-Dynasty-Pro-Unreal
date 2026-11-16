# Our own lamp flare (owner, 2026-11-16: "create our own realistic flare" - Unreal's camera flares ghost): what a real
# camera and eye see looking into a bright lamp at night - a hot white-ish core, a soft glow round it, and a thin
# six-point star from the lens's aperture blades, the spikes thinning and fading outwards; additive, unlit, drawn on a
# sprite that faces the camera (AirportDynastyPro LampFlares.cpp puts one at each lamp face). Color and Intensity
# parameters (the game sets Intensity per lamp: by the dark and by how much the lamp faces you).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_lamp_flare.py" -unattended -nullrhi
import unreal
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
DIR = '/Game/Materials/Effects'; NAME = 'M_LampFlare'; path = DIR + '/' + NAME
if not eal.does_directory_exist(DIR): eal.make_directory(DIR)
try:
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else at.create_asset(NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    try: m.set_editor_property('used_with_sprites', True)
    except Exception as e: unreal.log('ADP_FLARE no sprite flag ' + str(e))
    tc = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 120); col.set_editor_property('parameter_name', 'Color'); col.set_editor_property('default_value', unreal.LinearColor(1.0, 0.85, 0.65, 1))
    it = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 240); it.set_editor_property('parameter_name', 'Intensity'); it.set_editor_property('default_value', 1.0)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -500, 0)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property('code', r"""
float2 d = UV - 0.5; float r = length(d) * 2.0;            // 0 in the middle, 1 at the sprite's edge
float a = atan2(d.y, d.x) + 0.26;                            // (the star a little turned, as a real lens's)
float core = exp(-r * r * 900.0) * 5.0;                      // the lamp itself, burnt almost white
float glow = exp(-r * 9.0) * 0.55 + exp(-r * 3.2) * 0.07;    // the soft bloom round it
float spike = pow(abs(cos(a * 3.0)), 900.0 * (0.3 + r)) * exp(-r * 4.5) * 0.9 * saturate(1.0 - r);   // six thin spikes
float k = (glow + spike) * Intensity * saturate(1.0 - r * 1.05);
float3 hot = lerp(Col.rgb, float3(1, 1, 1), 0.65);
return Col.rgb * k + hot * core * Intensity;
""")
    ins = []
    for n in ('UV', 'Col', 'Intensity'):
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
    c.set_editor_property('inputs', ins)
    for a_, n in ((tc, 'UV'), (col, 'Col'), (it, 'Intensity')): mel.connect_material_expressions(a_, '', c, n)
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m); eal.save_asset(path)
    unreal.log('ADP_FLARE: made')
except Exception as ex:
    unreal.log_error('ADP_FLARE: failed ' + str(ex))
unreal.SystemLibrary.quit_editor()
