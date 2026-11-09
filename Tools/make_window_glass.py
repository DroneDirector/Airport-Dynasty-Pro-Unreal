# Real window glass for the Babylon model: a translucent, forward-shaded, two-sided glass whose see-through-ness
# follows the viewing angle (Fresnel: clearer straight on, more mirror at a slant), mirror-smooth, with the glass's own
# relief (the Babylon glass normal texture), Lumen front-layer reflections where the quality allows. By day from outside
# the bright sky it mirrors hides the inside; from inside the bright day outside shows through; at night, with no sky
# to mirror and the inside lit, you see straight in. Every glass slot of the model's meshes gets it (outer facades and
# vaults: Outer; inner glass walls: Inner), keeping each one's Babylon tint. Geometry untouched.
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
DIR = '/Game/Materials/Glass'
MASTER = DIR + '/M_WindowGlass'

try:
    if eal.does_asset_exist(MASTER):
        eal.delete_asset(MASTER)
    m = at.create_asset('M_WindowGlass', DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    for flag in ('used_with_instanced_static_meshes', 'used_with_nanite'):
        m.set_editor_property(flag, True)
    done = []
    # Lumen front-layer reflections (High and Epic quality, DefaultScalability.ini) and screen-space reflections (Low, Medium)
    for name in ('allow_front_layer_translucency', 'screen_space_reflections'):
        try:
            m.set_editor_property(name, True); done.append(name)
        except Exception:
            pass
    unreal.log('ADP_GLASS: front layer flag ' + str(done))

    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    def param(cls, name, value, x, y):
        e = node(cls, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', value); return e

    tint = param(unreal.MaterialExpressionVectorParameter, 'Tint', unreal.LinearColor(0.62, 0.74, 0.8, 1), -700, -300)
    mel.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
    # see-through-ness by angle: Lerp(straight on, at a slant, Fresnel)
    fres = node(unreal.MaterialExpressionFresnel, -700, 0)
    fres.set_editor_property('exponent', 4.0)
    lo = param(unreal.MaterialExpressionScalarParameter, 'OpacityStraight', 0.08, -700, 120)
    hi = param(unreal.MaterialExpressionScalarParameter, 'OpacitySlant', 0.55, -700, 220)
    lerp = node(unreal.MaterialExpressionLinearInterpolate, -400, 100)
    mel.connect_material_expressions(lo, '', lerp, 'A')
    mel.connect_material_expressions(hi, '', lerp, 'B')
    mel.connect_material_expressions(fres, '', lerp, 'Alpha')
    mel.connect_material_property(lerp, '', unreal.MaterialProperty.MP_OPACITY)
    for i, (name, prop, v) in enumerate([('Roughness', unreal.MaterialProperty.MP_ROUGHNESS, 0.03), ('Specular', unreal.MaterialProperty.MP_SPECULAR, 1.0), ('Metallic', unreal.MaterialProperty.MP_METALLIC, 0.0)]):
        mel.connect_material_property(param(unreal.MaterialExpressionScalarParameter, name, v, -400, 300 + i * 100), '', prop)
    # the glass's own relief (float-glass waviness), from Babylon's glass normal texture
    nrm = node(unreal.MaterialExpressionTextureSampleParameter2D, -700, 600)
    nrm.set_editor_property('parameter_name', 'GlassNormal')
    tex = unreal.load_asset('/Game/Babylon/Terminal/terminal/Textures/assets_textures_glass-normal_webp')
    if tex:
        nrm.set_editor_property('texture', tex)
        nrm.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(nrm, '', unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_asset(MASTER)

    def instance(name, tint_col, straight, slant):
        path = DIR + '/' + name
        mi = unreal.load_asset(path) if eal.does_asset_exist(path) else at.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, m)
        mel.set_material_instance_vector_parameter_value(mi, 'Tint', tint_col)
        mel.set_material_instance_scalar_parameter_value(mi, 'OpacityStraight', straight)
        mel.set_material_instance_scalar_parameter_value(mi, 'OpacitySlant', slant)
        eal.save_asset(path)
        return mi

    # outer glazing: low-iron laminated glass, a stronger mirror at a slant; inner glass walls: clearer
    outer = instance('MI_WindowGlass_Outer', unreal.LinearColor(0.56, 0.64, 0.68, 1), 0.1, 0.7)
    inner = instance('MI_WindowGlass_Inner', unreal.LinearColor(0.81, 0.89, 0.95, 1), 0.06, 0.35)

    # every glass slot of the model gets it
    swapped = 0
    for p in eal.list_assets('/Game/Babylon/Terminal/terminal/StaticMeshes', recursive=False):
        sm = unreal.load_asset(p)
        slots = sm.get_editor_property('static_materials')
        changed = False
        for s in slots:
            mi = s.material_interface
            n = mi.get_name() if mi else ''
            if n in ('glassOuter', 'glassVault') or n.startswith('gl-'):
                s.material_interface = outer; changed = True
            elif n in ('glass', 'glassBlue'):
                s.material_interface = inner; changed = True
        if changed:
            sm.set_editor_property('static_materials', slots)
            eal.save_loaded_asset(sm, only_if_is_dirty=False)
            swapped += 1
    unreal.log('ADP_GLASS: window glass on %d meshes' % swapped)
except Exception as e:
    unreal.log_error('ADP_GLASS: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
