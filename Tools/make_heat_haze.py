# Heat haze (to-do 22b, owner's list 2026-11-16): the shimmer of hot air behind a running APU (and later jet engines):
# see-through, it only bends what is behind it (refraction from a moving normal), rising and drifting, softly faded to
# nothing at its edges; a faint warm tint at most. Drawn on a disc that faces you (ParkedAircraft.cpp puts one behind each
# parked aircraft's APU exhaust). Strength and speed are parameters.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_heat_haze.py" -unattended -nullrhi
import unreal
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
DIR, NAME = '/Game/Materials/Effects', 'M_HeatHaze'
path = DIR + '/' + NAME
if not eal.does_directory_exist(DIR): eal.make_directory(DIR)
try:
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else at.create_asset(NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property('two_sided', True)
    for prop, val in (('refraction_method', getattr(unreal, 'RefractionMode', None) and getattr(unreal.RefractionMode, 'RM_PIXEL_NORMAL_OFFSET', None)),):
        if val is not None:
            try: m.set_editor_property(prop, val)
            except Exception as e: unreal.log('ADP_HAZE: no ' + prop + ' ' + str(e))
    tc = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -900, 120)
    sp = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 220); sp.set_editor_property('parameter_name', 'Speed'); sp.set_editor_property('default_value', 1.0)
    st = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 320); st.set_editor_property('parameter_name', 'Strength'); st.set_editor_property('default_value', 1.0)
    O = unreal.CustomMaterialOutputType
    def custom(code, out, inputs, y):
        c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -500, y)
        c.set_editor_property('code', code); c.set_editor_property('output_type', out)
        ins = []
        for n in inputs:
            ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
        c.set_editor_property('inputs', ins); return c
    # the moving normal: two layers of soft waves rising at different speeds, swaying a little
    NRM = r"""
float t = Time * Speed;
float2 p = UV * float2(5.0, 8.0);
float a = sin(p.x * 1.9 + sin(p.y * 1.3 + t * 0.7) * 1.6 + t * 0.4) + sin((p.y + t * 1.8) * 2.3 + p.x * 0.8);
float b = cos(p.x * 2.7 - t * 0.5 + cos((p.y + t * 2.6) * 1.7)) + cos((p.y + t * 1.2) * 3.1 - p.x * 1.1);
return normalize(float3(a * 0.5, b * 0.5, 4.0));
"""
    c = custom(NRM, O.CMOT_FLOAT3, ['UV', 'Time', 'Speed'], 0)
    mel.connect_material_expressions(tc, '', c, 'UV'); mel.connect_material_expressions(tm, '', c, 'Time'); mel.connect_material_expressions(sp, '', c, 'Speed')
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_NORMAL)
    # how much it bends: strongest in the middle and low down (by the exhaust), nothing at the edges
    MASK = r"""
float2 d = (UV - float2(0.5, 0.62)) * float2(2.0, 1.7);
float m = saturate(1.0 - dot(d, d));
return m * m * Strength;
"""
    k = custom(MASK, O.CMOT_FLOAT1, ['UV', 'Strength'], 260)
    mel.connect_material_expressions(tc, '', k, 'UV'); mel.connect_material_expressions(st, '', k, 'Strength')
    ior = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -200, 260)
    one = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -350, 200); one.set_editor_property('r', 1.0)
    hi = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -350, 330); hi.set_editor_property('r', 1.022)   # (subtle: hot air bends the view only a little)
    mel.connect_material_expressions(one, '', ior, 'A'); mel.connect_material_expressions(hi, '', ior, 'B'); mel.connect_material_expressions(k, '', ior, 'Alpha')
    mel.connect_material_property(ior, '', unreal.MaterialProperty.MP_REFRACTION)
    op = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 420)
    small = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -350, 470); small.set_editor_property('r', 0.035)
    mel.connect_material_expressions(k, '', op, 'A'); mel.connect_material_expressions(small, '', op, 'B')
    mel.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
    bc = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -350, -150); bc.set_editor_property('constant', unreal.LinearColor(0.45, 0.42, 0.38, 1))
    mel.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
    r1 = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -350, -60); r1.set_editor_property('r', 1.0)
    mel.connect_material_property(r1, '', unreal.MaterialProperty.MP_ROUGHNESS)
    s0 = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -350, 40); s0.set_editor_property('r', 0.0)
    mel.connect_material_property(s0, '', unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_asset(path)
    unreal.log('ADP_HAZE: ' + path + ' made')
except Exception as e:
    unreal.log_error('ADP_HAZE: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
