# Owner, 2026-11-18:
#  104 - square patches on the glass: the terminal's glass master (M_TGlass_DS) was lit from the translucency lighting
#        volume (Surface TranslucencyVolume - a coarse 3D grid that shows as blocks on big panes); now per-pixel
#        (Surface ForwardShading): smooth, as glass should be.
#  103 - the escalator steps far too reflective: they shared the jetways' polished aluminium (MI_T_jwAluB: metallic 0.25,
#        roughness 0.36); now their own MI_T_escStep - the same grooved metal picture, about 5 % reflective (metallic 0.05,
#        roughness 0.78). The other escalator metal (balustrade trim, deck, combs) keeps its shine.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/fix_glass_steps.py" -unattended -nullrhi
import unreal, os, stat
EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary
def log(*a): unreal.log('ADP_FIX ' + ' '.join(str(x) for x in a))
def writable(path):
    f = os.path.join(unreal.Paths.project_content_dir(), path.replace('/Game/', '') + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)

# 104: the glass master's translucency lighting
gm = unreal.load_asset('/Game/Terminal/Masters/M_TGlass_DS')
writable('/Game/Terminal/Masters/M_TGlass_DS')
log('glass before', gm.get_editor_property('translucency_lighting_mode'))
gm.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
ML.recompile_material(gm); EL.save_loaded_asset(gm, only_if_is_dirty=False)
log('glass after', gm.get_editor_property('translucency_lighting_mode'))

# 103: the steps' own matte metal
src = unreal.load_asset('/Game/Terminal/Materials/MI_T_jwAluB')
dst_path = '/Game/Terminal/Materials/MI_T_escStep'
if EL.does_asset_exist(dst_path): writable(dst_path); mi = unreal.load_asset(dst_path)
else:
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_T_escStep', '/Game/Terminal/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property('parent', src.get_editor_property('parent'))
for s in src.get_editor_property('scalar_parameter_values'): ML.set_material_instance_scalar_parameter_value(mi, s.parameter_info.name, s.parameter_value)
for s in src.get_editor_property('vector_parameter_values'): ML.set_material_instance_vector_parameter_value(mi, s.parameter_info.name, s.parameter_value)
for s in src.get_editor_property('texture_parameter_values'): ML.set_material_instance_texture_parameter_value(mi, s.parameter_info.name, s.parameter_value)
ML.set_material_instance_scalar_parameter_value(mi, 'M', 0.05)
ML.set_material_instance_scalar_parameter_value(mi, 'R', 0.78)
ML.update_material_instance(mi); EL.save_loaded_asset(mi, only_if_is_dirty=False)
for name in ('SM_EscStep',):
    writable('/Game/Escalator/' + name)
    sm = unreal.load_asset('/Game/Escalator/' + name); mats = []
    for m in sm.get_editor_property('static_materials'):
        if str(m.get_editor_property('material_slot_name')) == 'M_EscStepMetal': m.set_editor_property('material_interface', mi)
        mats.append(m)
    sm.set_editor_property('static_materials', mats); EL.save_loaded_asset(sm, only_if_is_dirty=False)
    log(name, [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name()) for m in sm.get_editor_property('static_materials')])
log('done')
