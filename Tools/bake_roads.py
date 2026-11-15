# Saves every road as finished models (/Game/Roads/Baked + Data/roads-baked.json): ARoadNetwork::BakeAll in the editor's
# world. Run again whenever Data/roads.json (or the road ground) changes - a changed road is built at run time until then.
#   bash Tools/bake_roads.sh
import unreal
w = unreal.EditorLevelLibrary.get_editor_world() if hasattr(unreal, 'EditorLevelLibrary') else unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
n = unreal.RoadNetwork.bake_all(w)
unreal.log('ADP_BAKE roads saved: %d' % n)
