# The fire station's window glass in the game: every slot of its meshes whose Babylon material is named gl-... (the bay
# doors' window bands, docs/fire-station/sketch/station.ts) gets the terminal's own window glass (Tools/make_window_glass.py:
# MI_WindowGlass_Outer, see-through with its Fresnel mirror and refraction). Run after importing the station:
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs path>/Tools/set_station_glass.py [Firestation]"
import sys
import unreal
eal = unreal.EditorAssetLibrary
name = ([a for a in sys.argv[1:] if not a.startswith('-')] or ['Firestation'])[0]
outer = unreal.load_asset('/Game/Materials/Glass/MI_WindowGlass_Outer')
n = 0
if outer is None:
    unreal.log_error('ADP_GLASS: MI_WindowGlass_Outer is missing (Tools/make_window_glass.py)')
else:
    for path in eal.list_assets('/Game/Babylon/' + name, recursive=True):
        a = unreal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh): continue
        slots = a.get_editor_property('static_materials'); changed = False
        for s in slots:
            mi = s.material_interface
            if mi is not None and (mi.get_name().startswith('gl-') or str(s.material_slot_name).startswith('gl-')):
                s.material_interface = outer; changed = True
        if changed:
            a.set_editor_property('static_materials', slots)
            ns = a.get_editor_property('nanite_settings')
            if ns.enabled: ns.enabled = False; a.set_editor_property('nanite_settings', ns)   # translucent: drawn as an ordinary mesh
            eal.save_loaded_asset(a, False); n += 1
    unreal.log('ADP_GLASS: the terminal window glass on %d meshes of %s' % (n, name))
unreal.SystemLibrary.quit_editor()
