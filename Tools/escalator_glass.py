# Owner, 2026-11-18: "the escalator glasses - one of them is not tinted ... fix that throughout the airport where the
# escalator is in use". Every escalator balustrade shared the terminal's clear glass (MI_T_glass, 16 % opacity): in front
# of a dark background it read smoky, in front of the bright floor clear. Now the escalators have their own smoked glass,
# MI_T_escGlass (the same glass master and shine, a grey tint and more body), so every pane reads tinted whatever is
# behind it - on every escalator (SM_EscBody, SM_EscBodyShort) and the arrivals stair's guard beside them
# (Art/TerminalAnnex/make_arrivals_wing.py). The terminal's own glass (MI_T_glass) is unchanged.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/escalator_glass.py" -unattended -nullrhi
import unreal, os, stat
EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary
def log(*a): unreal.log('ADP_EGLASS ' + ' '.join(str(x) for x in a))
def writable(path):
    f = os.path.join(unreal.Paths.project_content_dir(), path.replace('/Game/', '') + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
src = unreal.load_asset('/Game/Terminal/Materials/MI_T_glass')
dst = '/Game/Terminal/Materials/MI_T_escGlass'
if EL.does_asset_exist(dst): writable(dst); mi = unreal.load_asset(dst)
else: mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_T_escGlass', '/Game/Terminal/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property('parent', src.get_editor_property('parent'))
for s in src.get_editor_property('scalar_parameter_values'): ML.set_material_instance_scalar_parameter_value(mi, s.parameter_info.name, s.parameter_value)
for s in src.get_editor_property('vector_parameter_values'): ML.set_material_instance_vector_parameter_value(mi, s.parameter_info.name, s.parameter_value)
for s in src.get_editor_property('texture_parameter_values'): ML.set_material_instance_texture_parameter_value(mi, s.parameter_info.name, s.parameter_value)
ML.set_material_instance_vector_parameter_value(mi, 'BC', unreal.LinearColor(0.20, 0.22, 0.24, 0.42))   # smoked grey, more body
ML.update_material_instance(mi); EL.save_loaded_asset(mi, only_if_is_dirty=False)
for n in ('SM_EscBody', 'SM_EscBodyShort'):
    p = '/Game/Escalator/' + n; writable(p); sm = unreal.load_asset(p); mats = []
    for m in sm.get_editor_property('static_materials'):
        if str(m.get_editor_property('material_slot_name')) == 'M_EscGlass': m.set_editor_property('material_interface', mi)
        mats.append(m)
    sm.set_editor_property('static_materials', mats); EL.save_loaded_asset(sm, only_if_is_dirty=False)
    log(n, [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name()) for m in sm.get_editor_property('static_materials') if 'Glass' in str(m.get_editor_property('material_slot_name'))])
log('done')
