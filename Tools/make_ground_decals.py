# Ground decals (to-do 22a, the cheap tier): marks laid on the airport's paving, drawn by our own patterns (no bought
# textures): M_DecalStain - an oil / fuel stain, a dark soft blotch with a darker rim and a faint sheen; M_DecalTyre -
# rubber deposits where wheels touch down, dark streaks along the decal's length; M_DecalPatch - a repair patch, newer
# darker asphalt in a square with a sealed edge. Deferred decals (they darken the ground's colour and change its
# roughness), their look varied per copy by a Seed parameter.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_ground_decals.py" -unattended -nullrhi
import unreal
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
DIR = '/Game/Ground/Decals'
if not eal.does_directory_exist(DIR): eal.make_directory(DIR)
O = unreal.CustomMaterialOutputType

NOISE = r"""
float h(float2 p){ return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float n(float2 p){ float2 i = floor(p), f = frac(p); f = f * f * (3 - 2 * f);
  return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), f.x), f.y); }
float fbm(float2 p){ float a = 0.5, s = 0; for (int k = 0; k < 5; k++) { s += a * n(p); p *= 2.03; a *= 0.5; } return s; }
"""
SHAPES = {
    'M_DecalStain': r"""
float2 d = (UV - 0.5) * 2.0;
float r = length(d) + (fbm(UV * 5.0 + Seed * 13.7) - 0.5) * 0.55;
float body = saturate((0.78 - r) * 6.0);
float rim = saturate(1.0 - abs(r - 0.7) * 14.0) * 0.6;
float k = saturate(body * (0.55 + 0.45 * fbm(UV * 11.0 + Seed * 3.1)) + rim);
return float4(k, 0, 0, 0);
""",
    'M_DecalTyre': r"""
float lane = abs(frac(UV.y * 2.0 + Seed * 0.37) - 0.5) * 2.0;
float streak = saturate((0.55 - lane) * 3.0) * (0.5 + 0.5 * fbm(float2(UV.x * 40.0, UV.y * 3.0) + Seed * 7.0));
float fade = saturate(UV.x * 4.0) * saturate((1.0 - UV.x) * 1.5);
float k = saturate(streak * fade * (0.6 + 0.4 * fbm(UV * 6.0 + Seed)));
return float4(k, 0, 0, 0);
""",
    'M_DecalPatch': r"""
float2 d = abs(UV - 0.5) * 2.0;
float sq = saturate((0.92 - max(d.x, d.y)) * 40.0);
float edge = saturate(1.0 - abs(max(d.x, d.y) - 0.93) * 30.0) * 0.8;
float k = saturate(sq * (0.45 + 0.25 * fbm(UV * 9.0 + Seed * 5.3)) + edge);
return float4(k, 0, 0, 0);
""",
}
COL = {'M_DecalStain': (0.025, 0.022, 0.02), 'M_DecalTyre': (0.02, 0.02, 0.02), 'M_DecalPatch': (0.05, 0.05, 0.052)}
ROUGH = {'M_DecalStain': 0.35, 'M_DecalTyre': 0.6, 'M_DecalPatch': 0.8}
for name, code in SHAPES.items():
    path = DIR + '/' + name
    try:
        m = unreal.load_asset(path) if eal.does_asset_exist(path) else at.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
        mel.delete_all_material_expressions(m)
        m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        tc = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
        sd = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 120); sd.set_editor_property('parameter_name', 'Seed'); sd.set_editor_property('default_value', 0.0)
        st = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 220); st.set_editor_property('parameter_name', 'Strength'); st.set_editor_property('default_value', 1.0)
        c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -550, 0)
        c.set_editor_property('code', code); c.set_editor_property('output_type', O.CMOT_FLOAT4)
        c.set_editor_property('additional_defines', []) if False else None
        ins = []
        for nme in ('UV', 'Seed'):
            ci = unreal.CustomInput(); ci.set_editor_property('input_name', nme); ins.append(ci)
        c.set_editor_property('inputs', ins)
        # (the noise helpers ride in a second custom node's declarations)
        c.set_editor_property('code', NOISE.replace('\n', ' ') and code)
        try:
            dec = unreal.CustomDefine() if hasattr(unreal, 'CustomDefine') else None
        except Exception:
            dec = None
        c.set_editor_property('additional_outputs', [])
        try: c.set_editor_property('include_file_paths', [])
        except Exception: pass
        # put the helpers inline: functions inside the Custom node body are not allowed, so a struct trick
        body = 'struct NZ { ' + NOISE.replace('float h(', 'float h(').replace('\n', ' ') + ' }; NZ z; ' + code.replace('fbm(', 'z.fbm(')
        c.set_editor_property('code', body)
        mel.connect_material_expressions(tc, '', c, 'UV'); mel.connect_material_expressions(sd, '', c, 'Seed')
        mask = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -300, 0)
        mask.set_editor_property('r', True); mask.set_editor_property('g', False); mask.set_editor_property('b', False); mask.set_editor_property('a', False)
        mel.connect_material_expressions(c, '', mask, '')
        op = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 60)
        mel.connect_material_expressions(mask, '', op, 'A'); mel.connect_material_expressions(st, '', op, 'B')
        mel.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
        bc = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -150); bc.set_editor_property('constant', unreal.LinearColor(*COL[name], 1))
        mel.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
        r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 200); r.set_editor_property('r', ROUGH[name])
        mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(m); eal.save_asset(path)
        unreal.log('ADP_DECAL: ' + path + ' made')
    except Exception as e:
        unreal.log_error('ADP_DECAL: ' + name + ' failed ' + str(e))
unreal.SystemLibrary.quit_editor()
