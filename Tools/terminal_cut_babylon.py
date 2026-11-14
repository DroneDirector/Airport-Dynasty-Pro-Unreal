# The true-Unreal terminal (to-do 42), part 4: the last ties to the Babylon-exported terminal cut.
#  - the four ground materials the terminal kept from its import (apron, asphalt, grass, rentalAsphalt, on M_Ground)
#    copied into /Game/Terminal/Materials as MI_T_<name>, and the native pieces moved onto them
#  - every asset outside /Game/Babylon/Terminal that still refers into it is listed (it must move before the old
#    folder stops being packed into the game: Config DirectoriesToNeverCook)
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/terminal_cut_babylon.py" -unattended
import unreal
eal = unreal.EditorAssetLibrary
def log(m): unreal.log('ADP_CUT ' + m)
OLD = '/Game/Babylon/Terminal'
T = '/Game/Terminal'
moved = {}
for n in ('apron', 'asphalt', 'grass', 'rentalAsphalt'):
    src = OLD + '/terminal/Materials/' + n
    dst = T + '/Materials/MI_T_' + n
    if not eal.does_asset_exist(src): continue
    if not eal.does_asset_exist(dst):
        eal.duplicate_asset(src, dst); eal.save_asset(dst, only_if_is_dirty=False)
    moved[n] = unreal.load_asset(dst)
n = 0
for p in eal.list_assets(T + '/Meshes', recursive=False, include_folder=False):
    s = unreal.load_asset(p)
    if not isinstance(s, unreal.StaticMesh): continue
    ch = False
    for i, slot in enumerate(s.get_editor_property('static_materials')):
        mi = slot.get_editor_property('material_interface')
        if mi and mi.get_path_name().startswith(OLD) and mi.get_name() in moved:
            s.set_material(i, moved[mi.get_name()]); ch = True
    if ch: eal.save_loaded_asset(s, only_if_is_dirty=False); n += 1
log('ground materials moved: %d, pieces moved onto them: %d' % (len(moved), n))
# who still points into the old folder
ar = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
outside = {}
for p in eal.list_assets(OLD, recursive=True, include_folder=False):
    pkg = p.split('.')[0]
    for r in ar.get_referencers(pkg, opts) or []:
        r = str(r)
        if not r.startswith(OLD):
            outside.setdefault(r, []).append(pkg.split('/')[-1])
for r, what in sorted(outside.items()):
    log('still refers into the old terminal: %s -> %s' % (r, ', '.join(sorted(set(what))[:6])))
log('%d assets outside still refer into it' % len(outside))
unreal.SystemLibrary.quit_editor()
