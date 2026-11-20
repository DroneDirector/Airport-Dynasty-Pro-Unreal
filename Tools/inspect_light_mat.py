import unreal
m = unreal.load_asset('/Game/Terminal/Materials/MI_T_l_FFFFFF')
p = m.get_editor_property('parent'); unreal.log('ADP_LM parent %s' % p.get_path_name())
for v in m.get_editor_property('vector_parameter_values'): unreal.log('ADP_LM v %s %s' % (v.parameter_info.name, v.parameter_value))
for v in m.get_editor_property('scalar_parameter_values'): unreal.log('ADP_LM s %s %s' % (v.parameter_info.name, v.parameter_value))
n = unreal.load_asset('/Game/Terminal/Materials/MI_T_neonTeal')
for v in n.get_editor_property('vector_parameter_values'): unreal.log('ADP_LM neon v %s %s' % (v.parameter_info.name, v.parameter_value))
unreal.SystemLibrary.quit_editor()
