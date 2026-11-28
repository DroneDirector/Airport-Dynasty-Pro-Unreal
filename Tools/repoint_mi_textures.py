# A reimport keeps a material that already exists as it is: when a model script gives an existing material a new texture
# set, the material instance still shows the old pictures. This points its texture parameters at the new set.
# (2026-11-28, cargo terminal: M_Cargo_Steel / M_Cargo_Fence kept CargoLamps' near-black Galvanised scan.)
#   UnrealEditor-Cmd <project> -ExecutePythonScript="Tools/repoint_mi_textures.py <TexDir> <Old> <New> <MI path>..." -unattended -nullrhi
# e.g. /Game/Babylon/CargoTerminal/CargoTerminal/Textures Galvanised SteelGalv /Game/Babylon/CargoTerminal/CargoTerminal/Materials/M_Cargo_Steel
# Old -> New by name: <Old>_Color -> <TexDir>/<New>_Color (and _Normal, _ORM). Duplicates the import made (<New>_Color1 ...)
# are deleted when nothing uses them.
import sys
import unreal

EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary
tex_dir, old, new, mis = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
done = 0
for p in mis:
    mi = unreal.load_asset(p)
    if not isinstance(mi, unreal.MaterialInstanceConstant):
        unreal.log_error('ADP_REPOINT: %s is not a material instance' % p); continue
    for tp in mi.get_editor_property('texture_parameter_values'):
        t = tp.parameter_value
        if not t or not t.get_name().startswith(old + '_'): continue
        kind = t.get_name()[len(old) + 1:]
        nt = unreal.load_asset('%s/%s_%s' % (tex_dir, new, kind))
        if not nt:
            unreal.log_error('ADP_REPOINT: %s/%s_%s missing' % (tex_dir, new, kind)); continue
        ML.set_material_instance_texture_parameter_value(mi, tp.parameter_info.name, nt)
        done += 1
    ML.update_material_instance(mi)
    EL.save_loaded_asset(mi, only_if_is_dirty=False)
for kind in ('Color', 'Normal', 'ORM'):
    for k in range(1, 4):
        d = '%s/%s_%s%d' % (tex_dir, new, kind, k)
        if EL.does_asset_exist(d) and not EL.find_package_referencers_for_asset(d, False):
            EL.delete_asset(d)
unreal.log('ADP_REPOINT: %d texture parameters now on %s' % (done, new))
