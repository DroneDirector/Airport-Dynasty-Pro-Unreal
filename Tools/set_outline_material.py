# The interaction outline (owner, 2026-11-14: "a very thin glow all the way around" everything you can pick up or use -
# not doors, not vehicles - like games do it, so you know what you can interact with).
# How it's done: the things within reach are drawn into the custom-depth stencil (value 1, nothing else on screen); this
# post-process material looks at each pixel's neighbours and, just outside a marked shape, lays a thin soft glowing line
# over the picture. Run with UnrealEditor-Cmd ... -ExecutePythonScript=<this file>.
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER = '/Game/Interact'


def log(t):
    unreal.log('ADP_OUTLINE ' + t)


def mat(name):
    path = FOLDER + '/' + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return at.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


HLSL = r'''
float2 UV = GetDefaultSceneTextureUV(Parameters, 14);
float2 Px = View.BufferSizeAndInvSize.zw * Thick;
float c = SceneTextureLookup(UV, 25, false).r;
bool In = c > 0.5 && c < 1.5;
// the nearest marked pixel among the neighbours (a thin ring of two pixels' reach), softened by its distance
float ring = 0.0;
for (int i = -2; i <= 2; i++)
{
    for (int j = -2; j <= 2; j++)
    {
        float s = SceneTextureLookup(UV + float2(i, j) * Px, 25, false).r;
        float d = length(float2(i, j));
        if (s > 0.5 && s < 1.5) ring = max(ring, saturate(1.6 - d * 0.55));
    }
}
float edge = In ? 0.0 : ring;
float3 col = SceneTextureLookup(UV, 14, false).rgb;
// a gentle breathing, so it reads as a hint, not a frame
float pulse = 0.82 + 0.18 * sin(View.GameTime * 2.4);
return col + Glow.rgb * edge * Glow.a * pulse;
'''

m = mat('PP_InteractOutline')
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
loc = next((getattr(unreal.BlendableLocation, n) for n in dir(unreal.BlendableLocation) if 'AFTER_TONEMAPPING' in n.upper() and 'REPLACING' not in n.upper()), None)
if loc is not None:
    m.set_editor_property('blendable_location', loc)
c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
c.set_editor_property('code', HLSL)
c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
ins = []
for n in ('Glow', 'Thick', 'Stencil', 'Scene'):
    ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
c.set_editor_property('inputs', ins)
g = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, -100)
g.set_editor_property('parameter_name', 'Glow'); g.set_editor_property('default_value', unreal.LinearColor(1.0, 0.86, 0.55, 0.9))
mel.connect_material_expressions(g, 'RGBA', c, 'Glow')
t = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 50)
t.set_editor_property('parameter_name', 'Thick'); t.set_editor_property('default_value', 0.75)
mel.connect_material_expressions(t, '', c, 'Thick')
# (the scene textures the code reads must be in the graph for the shader to bind them)
sid = unreal.SceneTextureId
for k, (name, pin) in enumerate([('PPI_CUSTOM_STENCIL', 'Stencil'), ('PPI_POST_PROCESS_INPUT0', 'Scene')]):
    st = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 200 + 120 * k)
    st.set_editor_property('scene_texture_id', getattr(sid, name))
    mel.connect_material_expressions(st, 'Color', c, pin)
mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)
log('ready: PP_InteractOutline')
