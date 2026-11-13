# The walk / run animations carried over to the MetaHuman (Tools/retarget_player_anims.py) kept the mannequin's
# forward motion in the PELVIS (the retarget moved it off the root): over one jog cycle the body ran ~10 m ahead of you
# and snapped back (owner, 2026-11-13: "my body glitches in front of me ... it should stay right under me").
# Here every looping walk / jog clip (MF_Unarmed_*_MH) is made to run in place: the pelvis's steady drift from the
# clip's first frame to its last is taken out (the sway, bounce and steps stay), so only the capsule moves you.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/fix_inplace_anims.py" -unattended -nullrhi
import unreal
eal = unreal.EditorAssetLibrary
L = unreal.AnimationLibrary
def log(m): unreal.log('ADP_INPLACE ' + m)
done = 0
for p in eal.list_assets('/Game/People/Anims', recursive=False, include_folder=False):
    a = unreal.load_asset(p)
    if not isinstance(a, unreal.AnimSequence) or not a.get_name().startswith('MF_Unarmed_'): continue
    n = L.get_num_frames(a)
    if n < 2: continue
    pos, rot, scl = [], [], []
    for f in range(n + 1):
        t = L.get_bone_pose_for_frame(a, 'pelvis', min(f, n), False)
        pos.append(t.translation); rot.append(t.rotation); scl.append(t.scale3d)
    d0, d1 = pos[0], pos[-1]
    dx, dy = d1.x - d0.x, d1.y - d0.y
    # 1. the steady drift out (first frame to last)
    lin = [(v.x - dx * f / float(n), v.y - dy * f / float(n)) for f, v in enumerate(pos)]
    # 2. what's left of the clip's uneven pace (the body surging ahead and dropping back by up to half a metre over a
    #    cycle) taken out too, by removing its slow part (a running average over one step, half the cycle): the hips'
    #    quick sway and bounce stay, and the hips sit right over your feet on average (centred under the capsule)
    W = max(2, n // 2)
    newpos = []
    for f, v in enumerate(pos):
        sx = sy = 0.0
        for k in range(-W // 2, W - W // 2):
            q = lin[(f + k) % n]; sx += q[0]; sy += q[1]
        newpos.append(unreal.Vector(lin[f][0] - sx / W, lin[f][1] - sy / W, v.z))
    c = a.get_editor_property('controller')
    c.open_bracket(unreal.Text('in place'), True)
    ok = c.set_bone_track_keys('pelvis', newpos, rot, scl, True)
    c.close_bracket(True)
    eal.save_loaded_asset(a, only_if_is_dirty=False)
    chk = L.get_bone_pose_for_frame(a, 'pelvis', n, False).translation
    mx = max(abs(p.x) for p in newpos); my = max(abs(p.y) for p in newpos)
    log('%s: %d frames, drift (%.0f, %.0f) cm taken out (set %s), hips now within (%.0f, %.0f) cm of centre' % (a.get_name(), n, dx, dy, ok, mx, my))
    done += 1
log('%d clips made in place' % done)
unreal.SystemLibrary.quit_editor()
