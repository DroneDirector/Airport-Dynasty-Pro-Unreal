# The terminal's lit sign letters (terminal part 2a, 2026-11-23): copies of the plain glowing materials (M_TUnlit), brighter, so
# letters read as lit in the daylit gate walkway too (the plain ones looked grey there).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_sign_materials.py" -unattended -nullrhi
import unreal
EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary; D = '/Game/Terminal/Materials/'
for n, s, eb in (('signWhite', 'l_FFFFFF', 5.0), ('signAmber', 'l_FFC870', 5.0), ('signGreen', 'l_20FF40', 4.0), ('signRed', 'l_FF2020', 5.0)):
    if not EAL.does_asset_exist(D + 'MI_T_' + n): EAL.duplicate_asset(D + 'MI_T_' + s, D + 'MI_T_' + n)
    m = unreal.load_asset(D + 'MI_T_' + n); MEL.set_material_instance_scalar_parameter_value(m, 'EB', eb); MEL.update_material_instance(m); EAL.save_loaded_asset(m)
    unreal.log('ADP_SIGN %s EB %s' % (n, MEL.get_material_instance_scalar_parameter_value(m, 'EB')))
