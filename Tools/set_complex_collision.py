# The model's meshes collide by their own triangles (complex as simple): the imported Babylon meshes carry no simple
# collision shapes, so without this the vehicles' physics, their wheels' rays and the cameras' probes went straight
# through floors and walls (owner, 2026-11-11: the fire truck's wheels sank into the bay floor, it drove through the
# wall in the door openings). Run headless after importing a model:
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs path>/Tools/set_complex_collision.py Firestation"
import sys
import unreal
eal = unreal.EditorAssetLibrary
n = 0
for name in [a for a in sys.argv[1:] if not a.startswith('-')] or ['Firestation']:
    for path in eal.list_assets('/Game/Babylon/' + name, recursive=True):
        a = unreal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh): continue
        bs = a.get_editor_property('body_setup')
        if bs is None: continue
        if bs.get_editor_property('collision_trace_flag') != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
            bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            eal.save_loaded_asset(a, False); n += 1
unreal.log('ADP_COLLISION: %d meshes now collide by their triangles' % n)
unreal.SystemLibrary.quit_editor()
