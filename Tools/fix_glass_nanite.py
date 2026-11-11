# Nanite off for the meshes drawn with see-through glass (Nanite draws only opaque and masked materials; these then draw
# as ordinary meshes, everything else stays Nanite). Run headless with the folder(s) under /Game/Babylon to check:
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs path>/Tools/fix_glass_nanite.py Firetruck1 Firetruck2"
import sys
import unreal
eal = unreal.EditorAssetLibrary
n = 0
for name in [a for a in sys.argv[1:] if not a.startswith('-')]:
    for path in eal.list_assets('/Game/Babylon/' + name, recursive=True):
        a = unreal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh): continue
        see_through = False
        for m in a.get_editor_property('static_materials'):
            mi = m.material_interface
            if mi is None: continue
            # (the see-through setting may sit on the instance, over an opaque master: the glass is known by its name)
            if 'glass' in mi.get_name().lower(): see_through = True
            base = mi.get_base_material() if hasattr(mi, 'get_base_material') else mi
            if base and base.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT: see_through = True
        if see_through:
            ns = a.get_editor_property('nanite_settings')
            if ns.enabled:
                ns.enabled = False; a.set_editor_property('nanite_settings', ns); eal.save_loaded_asset(a, False); n += 1
unreal.log('ADP_GLASS: Nanite off on %d see-through meshes' % n)
