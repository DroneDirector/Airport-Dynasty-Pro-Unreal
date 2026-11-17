# The tax free's extra finishes (16 gates, owner 2026-11-17; his plan docs/TaxFree/): plain colours for the goods on the
# shelves, velvet for the plaza's seats - copies of the terminal's own MI_T_white (the surface master, its 'BC' colour) -
# and the aurora ceiling's glowing teal and lilac - copies of the terminal's white light MI_T_l_FFFFFF with its colour.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_taxfree_materials.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary
M = '/Game/Terminal/Materials/'
def log(*a): unreal.log('ADP_TFMAT ' + ' '.join(str(x) for x in a))
for f in glob.glob(os.path.join(ROOT, 'Content', 'Terminal', 'Materials', '*.uasset')): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
def hexc(h): return unreal.LinearColor(*(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)), 1.0)
def dup(src, name):
    if EAL.does_asset_exist(M + name): return unreal.load_asset(M + name)
    return EAL.duplicate_asset(M + src, M + name)
SURF = {'tf_pink': 'E8A9B8', 'tf_rose': 'C45A72', 'tf_cream': 'EFE4D2', 'tf_lilac': 'B9A6D6', 'tf_plum': '5B2A4E',
        'tf_amber': 'B5701F', 'tf_bottle': '2E5E3A', 'tf_wine': '6E1A2A', 'tf_navyp': '22335C', 'tf_sky': '7FB6D9',
        'tf_red': 'C8322E', 'tf_yellow': 'E9C44A', 'tf_choc': '4A2A18', 'tf_wool': 'D9D2C3', 'tf_slate': '3B3F46',
        'tf_velvet': '5A2A5E', 'tf_walnut': '3A2618', 'tf_stone': 'D6D2CB'}
for name, h in SURF.items():
    m = dup('MI_T_white', 'MI_T_' + name)
    MEL.set_material_instance_vector_parameter_value(m, 'BC', hexc(h))
    if name in ('tf_velvet',): MEL.set_material_instance_scalar_parameter_value(m, 'R', 0.85)
    if name in ('tf_bottle', 'tf_wine', 'tf_amber'): MEL.set_material_instance_scalar_parameter_value(m, 'R', 0.18)
    MEL.update_material_instance(m); EAL.save_loaded_asset(m, only_if_is_dirty=False); log(name, h)
src = unreal.load_asset(M + 'MI_T_l_FFFFFF')
names = [str(p.get_editor_property('parameter_info').get_editor_property('name')) for p in src.get_editor_property('vector_parameter_values')]
log('light params', names)
for h in ('40FFD0', 'B48CFF', 'FFC870'):
    if EAL.does_asset_exist(M + 'MI_T_l_' + h): EAL.delete_asset(M + 'MI_T_l_' + h)
    m = dup('MI_T_l_FFFFFF', 'MI_T_l_' + h)
    for n in names:
        if n in ('BC', 'EC'): MEL.set_material_instance_vector_parameter_value(m, n, hexc(h))   # (OS: its texture tiling, kept)
    MEL.update_material_instance(m); EAL.save_loaded_asset(m, only_if_is_dirty=False); log('light', h)
log('done')
unreal.SystemLibrary.quit_editor()
