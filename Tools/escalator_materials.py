# The escalators' own metal (owner, 2026-11-18: "the metal parts on the escalator can be slightly less reflective"):
# MI_T_escStainless (from the lifts' brushed steel, MI_T_liftBrushed) and MI_T_escFloorPlate (from MI_T_steel), each a
# little less metallic and a little rougher; the lifts and everything else keep theirs. Tools/import_escalator.py maps
# the escalator slots to them.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/escalator_materials.py" -unattended -nullrhi
import unreal, os, stat
EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary
def log(*a): unreal.log('ADP_ESCMAT ' + ' '.join(str(x) for x in a))
def make(src_name, dst_name, metal_k, rough_add):
    src = unreal.load_asset('/Game/Terminal/Materials/' + src_name); path = '/Game/Terminal/Materials/' + dst_name
    if EL.does_asset_exist(path):
        f = os.path.join(unreal.Paths.project_content_dir(), 'Terminal/Materials/' + dst_name + '.uasset')
        if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(dst_name, '/Game/Terminal/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', src.get_editor_property('parent'))
    vals = {}
    for s in src.get_editor_property('scalar_parameter_values'): ML.set_material_instance_scalar_parameter_value(mi, s.parameter_info.name, s.parameter_value); vals[str(s.parameter_info.name)] = s.parameter_value
    for s in src.get_editor_property('vector_parameter_values'): ML.set_material_instance_vector_parameter_value(mi, s.parameter_info.name, s.parameter_value)
    for s in src.get_editor_property('texture_parameter_values'): ML.set_material_instance_texture_parameter_value(mi, s.parameter_info.name, s.parameter_value)
    m = vals.get('M', 0.8); r = vals.get('R', 0.3)
    ML.set_material_instance_scalar_parameter_value(mi, 'M', round(m * metal_k, 3))
    ML.set_material_instance_scalar_parameter_value(mi, 'R', round(min(1.0, r + rough_add), 3))
    ML.update_material_instance(mi); EL.save_loaded_asset(mi, only_if_is_dirty=False)
    log(dst_name, 'from', src_name, 'M', m, '->', round(m * metal_k, 3), 'R', r, '->', round(min(1.0, r + rough_add), 3))
make('MI_T_liftBrushed', 'MI_T_escStainless', 0.75, 0.12)
make('MI_T_steel', 'MI_T_escFloorPlate', 0.8, 0.1)
log('done')
