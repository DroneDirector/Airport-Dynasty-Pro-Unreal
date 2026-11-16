# The baked roads drawn by Nanite (owner OK, 2026-11-16: same triangles, cheaper shadows - they were 607 ordinary
# meshes of up to 26,000 triangles, all casting shadows into the shadow map's slow non-Nanite queue). Only meshes whose
# every material is opaque or masked and made for Nanite are switched (a see-through one would draw wrong).
# Run by Tools/bake_roads.sh after every bake; on its own:
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/nanite_baked_roads.py" -unattended -nullrhi
import unreal
EAL = unreal.EditorAssetLibrary
done = skipped = already = 0
for p in EAL.list_assets('/Game/Roads/Baked', recursive=True, include_folder=False):
    sm = unreal.load_asset(p)
    if not isinstance(sm, unreal.StaticMesh): continue
    ok = True
    for m in sm.static_materials:
        base = m.material_interface.get_base_material() if m.material_interface else None
        if not base or base.get_editor_property('blend_mode') not in (unreal.BlendMode.BLEND_OPAQUE, unreal.BlendMode.BLEND_MASKED) or not base.get_editor_property('used_with_nanite'): ok = False
    if not ok: skipped += 1; continue
    ns = sm.get_editor_property('nanite_settings')
    if ns.enabled: already += 1; continue
    ns.enabled = True
    sm.set_editor_property('nanite_settings', ns)
    EAL.save_loaded_asset(sm, only_if_is_dirty=False)
    done += 1
unreal.log_warning('ADP_NANITE baked roads: %d made Nanite, %d already, %d left as they were (a material not for Nanite)' % (done, already, skipped))
