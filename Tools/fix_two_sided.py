# Two-sided materials for models made for one-sided-off rendering (FlightGear aircraft: owner 2026-11-28, "you can see
# through some of the inner parts of the airplane ... through the engine"). Their engine intakes, exhausts, nacelle inner
# walls, gear bays, wheel wells, flap cavities and many skins are single sheets, some wound inwards (Art/A320/A320.glb:
# ~15 000 open edges over all its opaque materials). The geometry is NOT changed: each opaque material instance of the
# model is pointed at the two-sided copy of its own glTF base (MI_Default_Opaque -> MI_Default_Opaque_DS, the same
# M_Default master with Two Sided on, as the glTF importer itself does for doubleSided materials). Two-sided lighting
# flips the normal on the back face, so the inside of a nacelle is lit as a surface, not black. Nanite draws two-sided
# opaque materials. Masked (placard decals) and see-through glass are left as they are.
# import_model.py runs it after the material fix-up for the models in its TWO_SIDED list, so a re-import keeps it.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/fix_two_sided.py A320" -unattended -nullrhi
import sys
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MASTERS = '/Game/Babylon/Masters'
TO_DS = {'MI_Default_Opaque': 'MI_Default_Opaque_DS', 'MI_ClearCoat_Opaque': 'MI_ClearCoat_Opaque_DS'}

try:
    n = 0; seen = []
    for name in [a for a in sys.argv[1:] if not a.startswith('-')]:
        for p in eal.list_assets('/Game/Babylon/%s' % name, recursive=True):
            a = unreal.load_asset(p)
            if not isinstance(a, unreal.MaterialInstanceConstant):
                continue
            parent = a.get_editor_property('parent')
            pn = parent.get_name() if parent else ''
            if pn in TO_DS:
                ds = unreal.load_asset('%s/%s' % (MASTERS, TO_DS[pn]))
                if ds is None:
                    unreal.log_error('ADP_TWOSIDED: %s/%s missing (run fix_babylon_materials.py first)' % (MASTERS, TO_DS[pn]))
                    continue
                mel.set_material_instance_parent(a, ds)
                eal.save_loaded_asset(a, False)
                n += 1; pn = ds.get_name()
            seen.append('%s<-%s' % (a.get_name(), pn))
    unreal.log('ADP_TWOSIDED: %d materials made two-sided; %s' % (n, ', '.join(seen)))
except Exception as e:
    unreal.log_error('ADP_TWOSIDED: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
