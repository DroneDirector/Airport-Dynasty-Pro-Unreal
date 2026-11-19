# The escalators' reflections (owner, 2026-11-19): the steps 1 % reflective, the black moving parts on them (the
# grooves, the dark step parts) at most 3 %, the handrails none at all; the escalator's metal (balustrade trim, deck,
# combs, truss) keeps the game's default. The steps/handrail get their OWN instances so the jetways that shared
# MI_T_jwDark / MI_T_jwRubber keep theirs. (Percentages as before: metallic 0, roughness so high that Lumen draws no
# mirror reflection - 1.0 none, 0.95 about 1 %, 0.9 about 3 %.) Tools/import_escalator.py maps the same on re-imports.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/escalator_reflections.py" -unattended -nullrhi
import unreal, os, stat
EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary
TM = '/Game/Terminal/Materials/'
def log(*a): unreal.log('ADP_ESCREF ' + ' '.join(str(x) for x in a))
def writable(path):
    f = os.path.join(unreal.Paths.project_content_dir(), path.replace('/Game/', '') + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
def own(src_name, dst_name, rough):
    src = unreal.load_asset(TM + src_name); path = TM + dst_name
    if EL.does_asset_exist(path): writable(path); mi = unreal.load_asset(path)
    else: mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(dst_name, TM[:-1], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', src.get_editor_property('parent'))
    for s in src.get_editor_property('scalar_parameter_values'): ML.set_material_instance_scalar_parameter_value(mi, s.parameter_info.name, s.parameter_value)
    for s in src.get_editor_property('vector_parameter_values'): ML.set_material_instance_vector_parameter_value(mi, s.parameter_info.name, s.parameter_value)
    for s in src.get_editor_property('texture_parameter_values'): ML.set_material_instance_texture_parameter_value(mi, s.parameter_info.name, s.parameter_value)
    ML.set_material_instance_scalar_parameter_value(mi, 'M', 0.0)
    ML.set_material_instance_scalar_parameter_value(mi, 'R', rough)
    ML.set_material_instance_scalar_parameter_value(mi, 'RV', 0.0)
    ML.set_material_instance_scalar_parameter_value(mi, 'BR', 0.0)
    ML.update_material_instance(mi); EL.save_loaded_asset(mi, only_if_is_dirty=False)
    log(dst_name, 'from', src_name, 'M 0 R', rough)
    return mi
step = own('MI_T_jwAluB', 'MI_T_escStep', 0.95)              # the steps: 1 %
dark = own('MI_T_jwDark', 'MI_T_escStepDark', 0.90)          # the black moving parts: 3 %
rail = own('MI_T_jwRubber', 'MI_T_escHandrail', 1.0)         # the handrails: none
yel = own('MI_T_yellow', 'MI_T_escStepYellow', 0.95)        # the steps' yellow edges: part of the steps, 1 %
STEPS = ('SM_EscStep', 'SM_StairStep')
for name in STEPS + ('SM_EscBody', 'SM_EscBodyShort'):
    path = '/Game/Escalator/' + name
    if not EL.does_asset_exist(path): continue
    writable(path); sm = unreal.load_asset(path); mats = []
    for m in sm.get_editor_property('static_materials'):
        slot = str(m.get_editor_property('material_slot_name'))
        if name in STEPS and slot == 'M_EscStepMetal': m.set_editor_property('material_interface', step)
        elif name in STEPS and slot in ('M_EscStepGroove', 'M_EscDark'): m.set_editor_property('material_interface', dark)
        elif name in STEPS and slot == 'M_EscYellow': m.set_editor_property('material_interface', yel)
        elif slot == 'M_EscHandrail': m.set_editor_property('material_interface', rail)
        mats.append(m)
    sm.set_editor_property('static_materials', mats); EL.save_loaded_asset(sm, only_if_is_dirty=False)
    log(name, [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name()) for m in sm.get_editor_property('static_materials')])
log('done')
