# Your walking body as your own MetaHuman (owner, 2026-11-12): the mannequin's animation set (Content/Characters/
# Mannequins/Anims/Unarmed: the idle / walk / run blend space, jump, the animation blueprint) carried over to the
# MetaHuman body skeleton as copies with the suffix _MH beside the originals. The mannequin and the MetaHuman body share
# their bone names, so the mannequin's IK rig is the MetaHuman plugin's own rig put on the mannequin mesh, and the
# retargeter the plugin's with that rig as its source (/Game/People/Rigs).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/retarget_player_anims.py" -unattended -nullrhi
import unreal

eal = unreal.EditorAssetLibrary
def log(m): unreal.log('ADP_RETARGET ' + m)

body = unreal.load_asset('/Game/People/Built/MH_900_player/Body/SKM_MH_900_player_BodyMesh')
manny = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
mh_rig = unreal.load_asset('/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig')
mh_rtg = unreal.load_asset('/MetaHumanCharacter/Animation/Retargeting/RTG_MH_IKRig')
if not (body and manny and mh_rig and mh_rtg):
    unreal.log_error('ADP_RETARGET missing: body %s manny %s rig %s rtg %s' % (bool(body), bool(manny), bool(mh_rig), bool(mh_rtg)))
    unreal.SystemLibrary.quit_editor(); raise SystemExit
RIGS = '/Game/People/Rigs'
if not eal.does_directory_exist(RIGS): eal.make_directory(RIGS)
# the mannequin's rig: the MetaHuman rig's chains on the mannequin mesh (the same bones)
if eal.does_asset_exist(RIGS + '/IK_Manny_MH'): eal.delete_asset(RIGS + '/IK_Manny_MH')
manny_rig = eal.duplicate_asset(mh_rig.get_path_name(), RIGS + '/IK_Manny_MH')
rc = unreal.IKRigController.get_controller(manny_rig)
log('mannequin rig mesh set: %s' % rc.set_skeletal_mesh(manny))
eal.save_loaded_asset(manny_rig, only_if_is_dirty=False)
# the retargeter: mannequin -> MetaHuman
if eal.does_asset_exist(RIGS + '/RTG_Manny_MH'): eal.delete_asset(RIGS + '/RTG_Manny_MH')
rtg = eal.duplicate_asset(mh_rtg.get_path_name(), RIGS + '/RTG_Manny_MH')
tc = unreal.IKRetargeterController.get_controller(rtg)
tc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, manny_rig)
tc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, mh_rig)
try:
    tc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, manny)
    tc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, body)
except Exception as e:
    log('preview meshes: ' + str(e))
tc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
eal.save_loaded_asset(rtg, only_if_is_dirty=False)
log('retargeter ready: ' + rtg.get_path_name())
# the animation set carried over
assets = []
for p in eal.list_assets('/Game/Characters/Mannequins/Anims/Unarmed', recursive=True, include_folder=False):
    a = unreal.load_asset(p)
    if isinstance(a, (unreal.AnimSequence, unreal.BlendSpace, unreal.AnimBlueprint, unreal.AnimMontage)) and not a.get_name().endswith('_MH'):
        assets.append(unreal.AssetData(a.get_path_name()) if hasattr(unreal, 'AssetData') and False else a)
log('%d animation assets to carry over' % len(assets))
ar = unreal.AssetRegistryHelpers.get_asset_registry()
datas = [ar.get_asset_by_object_path(a.get_path_name()) for a in assets]
try:
    out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(datas, manny, body, rtg, '', '', '', '_MH', True, True)
    log('retargeted %d assets' % len(out))
    # the copies come out at the content root and unsaved: filed under /Game/People/Anims and saved
    DST = '/Game/People/Anims'
    if not eal.does_directory_exist(DST): eal.make_directory(DST)
    for d in out:
        a = d.get_asset()
        if not a: continue
        src = a.get_path_name(); name = a.get_name()
        dst = DST + '/' + name
        if eal.does_asset_exist(dst): eal.delete_asset(dst)
        if not src.startswith(DST): eal.rename_asset(src, dst)
        eal.save_asset(dst, only_if_is_dirty=False)
        log('  ' + dst)
except Exception as e:
    unreal.log_error('ADP_RETARGET batch failed: ' + str(e))
eal.save_directory('/Game/People/Anims', only_if_is_dirty=False, recursive=True)
log('DONE')
unreal.SystemLibrary.quit_editor()
