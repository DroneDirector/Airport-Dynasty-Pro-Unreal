# Rain on the cargo terminal's outside glass (step B2): its imported glass (office windows, lobby, gatehouse: M_Cargo_Glass;
# the roof's skylight strips: M_Cargo_SkylightGlass) as copies standing on the rain's master, the same way
# Tools/set_building_rain.py makes the fire station's (MI_Rain_StationGlass). GlassRain.cpp swaps them in and keeps the
# glass under a roof dry. Run after Tools/set_building_rain.py (which makes MI_Rain_MI_Default_Blend_DS); touches nothing else.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_cargo_rain.py" -unattended -nullrhi
import os, stat, unreal
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
DIR = '/Game/Materials/Glass'; BP = DIR + '/MI_Rain_MI_Default_Blend_DS'


def log(*a): unreal.log('ADP_CARGORAIN ' + ' '.join(str(x) for x in a))


def disk(p): return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), p.replace('/Game/', '') + '.uasset')


bp = unreal.load_asset(BP)
if not bp: log('no', BP, '- run Tools/set_building_rain.py first'); unreal.SystemLibrary.quit_editor()
for src, n in (('/Game/Babylon/CargoTerminal/CargoTerminal/Materials/M_Cargo_Glass', 'MI_Rain_CargoGlass'),
               ('/Game/Babylon/CargoTerminal/CargoTerminal/Materials/M_Cargo_SkylightGlass', 'MI_Rain_CargoSkylight')):
    p = DIR + '/' + n
    if os.path.exists(disk(p)): os.chmod(disk(p), stat.S_IWRITE | stat.S_IREAD)
    if not eal.does_asset_exist(p): eal.duplicate_asset(src, p)
    mi = unreal.load_asset(p); s = unreal.load_asset(src)
    mel.set_material_instance_parent(mi, bp)
    mel.set_material_instance_static_switch_parameter_value(mi, 'RainGlass', True)
    mel.update_material_instance(mi)
    bad = [str(k) for k in mel.get_vector_parameter_names(s) if mel.get_material_instance_vector_parameter_value(s, k) != mel.get_material_instance_vector_parameter_value(mi, k)]
    bad += [str(k) for k in mel.get_scalar_parameter_names(s) if mel.get_material_instance_scalar_parameter_value(s, k) != mel.get_material_instance_scalar_parameter_value(mi, k)]
    bad += [str(k) for k in mel.get_texture_parameter_names(s) if mel.get_material_instance_texture_parameter_value(s, k) != mel.get_material_instance_texture_parameter_value(mi, k)]
    ok = eal.save_loaded_asset(mi, only_if_is_dirty=False)
    log(n, 'saved' if ok else 'SAVE FAILED', 'rain on:', mel.get_material_instance_static_switch_parameter_value(mi, 'RainGlass'), 'differs from its source:', bad)
log('ready')
unreal.SystemLibrary.quit_editor()
