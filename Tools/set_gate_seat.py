# The gate seating's pieces (Tools/make_gate_seat.py, imported to /Game/Babylon/GateSeat): solid to walk into (their
# own shape as the collision) and Nanite on.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_gate_seat.py" -unattended -nullrhi
import unreal
eal = unreal.EditorAssetLibrary
for n in ('SeatModule', 'LegModule', 'ArmModule'):
    sm = unreal.load_asset('/Game/Babylon/GateSeat/gateseat/StaticMeshes/' + n)
    if not sm: continue
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    ns = sm.get_editor_property('nanite_settings'); ns.enabled = True; sm.set_editor_property('nanite_settings', ns)
    eal.save_loaded_asset(sm, only_if_is_dirty=False)
    unreal.log('ADP_SEATS %s ready' % n)
# the shells in the same dark blue as the Babylon seats were (navy2, owner 2026-11-10); the metal stays
mel = unreal.MaterialEditingLibrary
sh = unreal.load_asset('/Game/Babylon/GateSeat/gateseat/Materials/Shell')
if sh:
    mel.set_material_instance_vector_parameter_value(sh, 'BaseColorFactor', unreal.LinearColor(0.1098, 0.1765, 0.3216, 1))
    mel.set_material_instance_scalar_parameter_value(sh, 'RoughnessFactor', 0.6)
    eal.save_loaded_asset(sh, only_if_is_dirty=False)
    unreal.log('ADP_SEATS shell dark blue')
unreal.SystemLibrary.quit_editor()
