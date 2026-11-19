# The jetways' service doors (owner, 2026-11-19: "you can't get in or out through the service door"): the pieces round
# each doorway (the wall, the skin, the glass, the frame, and the sills cut open at the doorway - Tools/jetway_part5.py, nodes '#svc...', '#sill') were imported with a
# simple collision (one box round all of a piece's parts) that filled the doorway. They collide by their real shape now
# (complex as simple). Run after every jetway import (Tools/import_jetways.sh does).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/jetway_svc_collision.py" -unattended -nullrhi
import json, os, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EL = unreal.EditorAssetLibrary
d = json.load(open(os.path.join(ROOT, 'Data', 'babylon-jetways.json')))
ids = sorted({n['mesh'] for n in d['nodes'] if ('#svc' in n.get('name', '') or n.get('name', '').endswith('#sill')) and 'mesh' in n and 'lamp' not in n.get('name', '')})
n = 0
for i in ids:
    path = '/Game/Babylon/Jetways/jetways/StaticMeshes/jetways_mesh_%d' % i
    f = os.path.join(ROOT, 'Content', 'Babylon', 'Jetways', 'jetways', 'StaticMeshes', 'jetways_mesh_%d.uasset' % i)
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
    sm = unreal.load_asset(path)
    if not sm: continue
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EL.save_loaded_asset(sm, only_if_is_dirty=False); n += 1
unreal.log('ADP_SVCCOL %d service door pieces collide by their shape (%s)' % (n, ids[:8]))
