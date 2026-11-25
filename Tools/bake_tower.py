# Saves the control tower's solid shells as Nanite meshes (/Game/Tower/Baked + Data/tower-baked.json): ATowerADC::BakeShells
# in the editor's world; with ADP_TOWER_CHECK=1 it only builds the tower with the saved ones, as the game does.
#   bash Tools/bake_tower.sh
import os
import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
try:
    n = unreal.TowerADC.bake_shells(w, os.environ.get('ADP_TOWER_CHECK') == '1')
    unreal.log('ADP_BAKE tower shells: %d' % n)
except Exception as e:
    unreal.log_error('ADP_BAKE failed ' + str(e))
unreal.SystemLibrary.quit_editor()
