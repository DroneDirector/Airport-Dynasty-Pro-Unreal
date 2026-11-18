# Read-only: what the terminal's glass and the escalator steps are made of (parent material, blend mode, translucency
# lighting mode, scalar/vector/texture parameters) - for to-do 103/104 (2026-11-18).
import unreal
def show(path):
    m = unreal.load_asset(path)
    if not m: unreal.log('ADP_INS missing %s' % path); return
    p = m
    while isinstance(p, unreal.MaterialInstance): p = p.get_editor_property('parent')
    unreal.log('ADP_INS %s parent %s blend %s lighting %s shading %s' % (path, p.get_path_name(), p.get_editor_property('blend_mode'),
               p.get_editor_property('translucency_lighting_mode'), p.get_editor_property('shading_model')))
    if isinstance(m, unreal.MaterialInstance):
        for s in m.get_editor_property('scalar_parameter_values'): unreal.log('ADP_INS   scalar %s = %s' % (s.parameter_info.name, s.parameter_value))
        for s in m.get_editor_property('vector_parameter_values'): unreal.log('ADP_INS   vector %s = %s' % (s.parameter_info.name, s.parameter_value))
        for s in m.get_editor_property('texture_parameter_values'): unreal.log('ADP_INS   texture %s = %s' % (s.parameter_info.name, s.parameter_value.get_path_name() if s.parameter_value else None))
    for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(p): unreal.log('ADP_INS   parent scalar %s' % n)
for a in ('/Game/Terminal/Materials/MI_T_glass', '/Game/Terminal/Materials/MI_T_jwAluB', '/Game/Terminal/Materials/MI_T_steel', '/Game/Terminal/Materials/MI_T_liftBrushed'):
    show(a)
