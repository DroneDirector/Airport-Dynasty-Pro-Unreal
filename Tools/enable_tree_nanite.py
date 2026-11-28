# To-do 385: Nanite on for the Pine Imposter Pack's pines (masked two-sided cards), foliage shape kept by Preserve Area.
# Run headless: UnrealEditor-Cmd AirportDynastyPro.uproject -ExecutePythonScript=Tools/enable_tree_nanite.py -unattended -nullrhi
import unreal
for i in range(1, 11):
    p = "/Game/Pine_Imposter_Pack/Meshes/Imposter/SM_Imposter_%d" % i
    m = unreal.load_asset(p)
    if not m:
        unreal.log_error("TreeNanite: missing " + p); continue
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", True)
    try: ns.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.PRESERVE_AREA)
    except Exception as e: unreal.log_warning("TreeNanite: no Preserve Area on %s: %s" % (p, e))
    m.set_editor_property("nanite_settings", ns)
    unreal.EditorAssetLibrary.save_loaded_asset(m, False)
    m2 = unreal.load_asset(p)
    unreal.log("TreeNanite: %s nanite=%s tris=%d" % (p, m2.get_editor_property("nanite_settings").get_editor_property("enabled"), m2.get_num_triangles(0)))
