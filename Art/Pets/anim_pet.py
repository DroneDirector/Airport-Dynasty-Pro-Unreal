# The moves for a pet rigged by rig_pet.py (owner, 2026-11-15: "stand, sit, lie flat on the floor ... walk fast; the
# dog runs fast, only when given a command; the cat can roll over, but normally never does"). Every move is keyed on
# the skeleton as an action in <Name>.blend, all in place (the game moves the pet; its speeds go to
# Models/<Kind>/<Name>/<Name>_moves.json):
#   Idle (stand: breathing, tail, a look round, an ear flick)       WalkFast (dog: trot, cat: fast walk)
#   SitDown / SitIdle / StandUp                                      LieDown / LieIdle / GetUp (lying flat, head down)
#   Run (dog only, a gallop, on command)                             RollOver (cat only, from lying, rare)
# The paws are placed on the ground and the legs bend to reach them (two-bone reach in the leg's own plane), the body,
# neck, head, ears and tail are turned on top. Units in the poses: percent of the pet's length; angles in degrees.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Pets/anim_pet.py -- <Dogs|Cats> <Name>
import bpy, json, math, os, sys
from mathutils import Vector, Matrix, Euler, Quaternion

a = sys.argv[sys.argv.index('--') + 1:]
KIND, NAME = a[0], a[1]
HERE = os.path.dirname(os.path.abspath(__file__))
D = os.path.join(HERE, 'Models', KIND, NAME)
FPS = 30
bpy.ops.wm.open_mainfile(filepath=os.path.join(D, NAME + '.blend'))
sc = bpy.context.scene; sc.render.fps = FPS
rig = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in bpy.data.objects if o.type == 'MESH'][0]
for act in list(bpy.data.actions): bpy.data.actions.remove(act)   # (a run replaces the moves, never adds copies)
for m in mesh.modifiers: m.show_viewport = False            # (posing is fast without the skin following each step)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='POSE')
PB = rig.pose.bones
for pb in PB: pb.rotation_mode = 'QUATERNION'
DOG = KIND == 'Dogs'
B = json.load(open(os.path.join(HERE, 'Models', 'Views', NAME + '_bones.json')))
L = B['L']
U = L / 100                                                 # one percent of the length, in metres
REST = {b.name: b.matrix_local.copy() for b in rig.data.bones}
def rhead(n): return REST[n].to_translation()
def rtail(n): return rig.data.bones[n].tail_local.copy()
PIV = rhead('pelvis')
TAILS = sorted([b.name for b in rig.data.bones if b.name.startswith('tail_')])
LEGS = {'fl': ('upperarm_l', 'lowerarm_l', 'hand_l', 'fingers_l'), 'fr': ('upperarm_r', 'lowerarm_r', 'hand_r', 'fingers_r'),
        'hl': ('thigh_l', 'calf_l', 'foot_l', 'toes_l'), 'hr': ('thigh_r', 'calf_r', 'foot_r', 'toes_r')}
# which way each knee bends (seen from the side, against the line hip -> ankle): measured on the rest skeleton
XAX = Vector((1, 0, 0))
BEND = {}
for k, (u, l, p, t) in LEGS.items():
    j, kn, w = rhead(u), rhead(l), rhead(p)
    d = (w - j).normalized(); perp = XAX.cross(d)
    BEND[k] = 1 if (kn - j).dot(perp) > 0 else -1

def upd(): bpy.context.view_layer.update()
def rot(pitch=0, roll=0, yaw=0):                           # world turn: pitch + = nose down, about the side axis
    return (Matrix.Rotation(math.radians(yaw), 4, 'Z') @ Matrix.Rotation(math.radians(roll), 4, 'Y')
            @ Matrix.Rotation(math.radians(pitch), 4, 'X'))
def local(n, pitch=0, yaw=0, roll=0):                      # bone's own turn: pitch + = its tip up
    PB[n].rotation_quaternion = Euler((math.radians(pitch), math.radians(roll), math.radians(yaw)), 'XYZ').to_quaternion()
def aim(n, direction):                                     # the bone pointed along a world direction (shortest turn)
    rest = REST[n]; ry = (rest.to_3x3() @ Vector((0, 1, 0))).normalized()
    q = ry.rotation_difference(direction.normalized())
    head = PB[n].head.copy() if PB[n].parent is None or not rig.data.bones[n].use_connect else PB[n].parent.tail.copy()
    PB[n].matrix = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ rest.to_3x3().to_4x4()
    upd()

def reach(j, w, l1, l2, sign, nrm):                        # two-bone reach: the knee between j and w
    d = w - j; D = max(abs(l1 - l2) + 1e-4, min(d.length, (l1 + l2) * 0.999)); dn = d.normalized()
    w = j + dn * D
    a_ = (l1 * l1 - l2 * l2 + D * D) / (2 * D); h = math.sqrt(max(0.0, l1 * l1 - a_ * a_))
    perp = nrm.cross(dn).normalized()
    return j + dn * a_ + perp * h * sign, w

DEFAULT = dict(pelvis=(0, 0, 0, 0, 0), spine=(0, 0, 0), spine_yaw=(0, 0, 0), neck=(0, 0), neck_yaw=(0, 0), head=(0, 0, 0),
               ears=(0, 0), tail=None, tail_yaw=None,
               legs={k: dict(paw=(0, 0, 0), pitch=0, toe=0, body=False) for k in LEGS}, scap=(0, 0))

def pose(P):
    """Lay one pose on the skeleton. P: pelvis (forward, up, pitch, roll, yaw) - forward/up in % of length; spine,
    neck: their bones' own pitch; head (pitch, yaw, roll); ears; tail: pitch per tail bone; legs: per leg the paw's
    move from where it stands (forward, out, up; with body=True it moves with the body: lying on the back), pitch:
    the paw's turn (+ = its tip back), toe: the toes' own turn; scap: the shoulder blades' pitch."""
    for pb in PB: pb.rotation_quaternion = Quaternion(); pb.location = Vector()
    upd()
    f, up, pi, ro, ya = P['pelvis']
    T = Matrix.Translation(PIV + Vector((0, -f * U, up * U))) @ rot(pi, ro, ya) @ Matrix.Translation(-PIV)
    PB['pelvis'].matrix = T @ REST['pelvis']; upd()
    for i, n in enumerate(('spine_01', 'spine_02', 'spine_03')): local(n, P['spine'][i], P['spine_yaw'][i])
    for i, n in enumerate(('neck_01', 'neck_02')): local(n, P['neck'][i], P['neck_yaw'][i])
    local('head', *P['head'])
    local('ear_l', P['ears'][0]); local('ear_r', P['ears'][1])
    tp = P['tail'] or [0] * len(TAILS); ty = P['tail_yaw'] or [0] * len(TAILS)
    for i, n in enumerate(TAILS): local(n, tp[i], ty[i])
    local('scapula_l', P['scap'][0]); local('scapula_r', P['scap'][1])
    upd()
    Rb = T.to_3x3()
    for k, (u, l, p, t) in LEGS.items():
        g = P['legs'][k]; side = 1 if k.endswith('l') else -1
        paw0, wr0 = rtail(p), rhead(p)
        mv = Vector((side * g['paw'][1] * U, -g['paw'][0] * U, g['paw'][2] * U))
        R = Matrix.Rotation(math.radians(g['pitch']), 3, 'X')
        if g.get('body'):
            paw = T @ (paw0 + mv); pdir = Rb @ (R @ (paw0 - wr0))
        else:
            paw = paw0 + mv; pdir = R @ (paw0 - wr0)
        w = paw - pdir                                          # the wrist / hock that puts the paw there
        j = PB[u].head.copy()
        l1 = (rtail(u) - rhead(u)).length; l2 = (rtail(l) - rhead(l)).length
        nrm = (Rb @ XAX) if g.get('body') else XAX
        knee, w = reach(j, w, l1, l2, BEND[k], nrm)
        aim(u, knee - j); aim(l, w - knee); aim(p, pdir)
        tdir = (rtail(t) - rhead(t))
        tdir = Matrix.Rotation(math.radians(g['toe']), 3, 'X') @ tdir
        if g.get('body'): tdir = Rb @ tdir
        aim(t, tdir)

def merge(*ps, **kw):
    out = json.loads(json.dumps(DEFAULT));
    for p in ps:
        for k, v in p.items():
            if k == 'legs':
                for lk, lv in v.items(): out['legs'][lk].update(lv)
            else: out[k] = v
    for k, v in kw.items(): out[k] = v
    return out

def blend(p, q, t):                                        # in-between of two poses
    def mix(x, y):
        if isinstance(x, dict): return {k: mix(x[k], y[k]) for k in x}
        if isinstance(x, (list, tuple)): return [mix(i, j) for i, j in zip(x, y)]
        if isinstance(x, bool): return x if t < 0.5 else y
        if x is None or y is None: return None
        return x + (y - x) * t
    p = merge(p); q = merge(q)
    if p['tail'] is None: p['tail'] = [0] * len(TAILS)
    if q['tail'] is None: q['tail'] = [0] * len(TAILS)
    if p['tail_yaw'] is None: p['tail_yaw'] = [0] * len(TAILS)
    if q['tail_yaw'] is None: q['tail_yaw'] = [0] * len(TAILS)
    return mix(p, q)

def ease(t): return t * t * (3 - 2 * t)

ACTIONS = {}
def record(name, frames, fn, loop=False):
    """Key every frame of a move: fn(i, n) gives the pose of frame i of n."""
    act = bpy.data.actions.new(name); act.use_fake_user = True
    rig.animation_data_create(); rig.animation_data.action = act
    prev = {}
    for i in range(frames + (1 if loop else 0)):
        pose(fn(i % frames if loop else i, frames))
        for pb in PB:
            q = pb.rotation_quaternion.copy()
            if pb.name in prev: q.make_compatible(prev[pb.name])
            pb.rotation_quaternion = q; prev[pb.name] = q
            pb.keyframe_insert('rotation_quaternion', frame=i + 1)
            if pb.name == 'pelvis': pb.keyframe_insert('location', frame=i + 1)
    act.frame_range = (1, frames + (1 if loop else 0))
    ACTIONS[name] = dict(frames=frames, loop=loop)
    print('MOVE', name, frames, 'frames')

TAILN = len(TAILS)
def wave(i, n, cycles=1, phase=0.0): return math.sin(2 * math.pi * (cycles * i / n + phase))

# ---- the poses (measured against the side views; checked in the move sheets) ----
if DOG:
    TAIL_REST = [8, 4, 2, 0, 0]
    SIT = dict(pelvis=(6, -22, -35, 0, 0), spine=(4, 4, 6), neck=(-10, -6), head=(2, 0, 0), ears=(0, 0), scap=(-30, -30),
               tail=[-50, 10, 15, 10, 5],
               legs={'fl': dict(paw=(-3, 0, 0), pitch=0), 'fr': dict(paw=(-3, 0, 0), pitch=0),
                     'hl': dict(paw=(18, 1.5, 0), pitch=-75, toe=0), 'hr': dict(paw=(18, 1.5, 0), pitch=-75, toe=0)})
    LIE = dict(pelvis=(-2, -28, 0, 0, 0), spine=(0, 0, 0), neck=(-8, -10), head=(-6, 0, 0), ears=(-10, -10),
               tail=[-20, -10, 0, 0, 0], tail_yaw=[20, 15, 10, 5, 0],
               legs={'fl': dict(paw=(22, 0, 0), pitch=-80, toe=0), 'fr': dict(paw=(22, 0, 0), pitch=-80, toe=0),
                     'hl': dict(paw=(14, 4, 0), pitch=-80), 'hr': dict(paw=(14, 4, 0), pitch=-80)})
else:
    TAIL_REST = [0, 0, 0, 0, 0, 0]
    SIT = dict(pelvis=(5, -26, -40, 0, 0), spine=(4, 6, 6), neck=(-10, -4), head=(4, 0, 0), ears=(0, 0), scap=(-30, -30),
               tail=[-50, -10, 0, 0, 0, 0], tail_yaw=[30, 30, 30, 25, 20, 10],
               legs={'fl': dict(paw=(0, 0, 0), pitch=0), 'fr': dict(paw=(0, 0, 0), pitch=0),
                     'hl': dict(paw=(16, 1.5, 0), pitch=-75, toe=0), 'hr': dict(paw=(16, 1.5, 0), pitch=-75, toe=0)})
    LIE = dict(pelvis=(-2, -20, 0, 0, 0), spine=(0, 0, 0), neck=(-8, -8), head=(-4, 0, 0), ears=(0, 0),
               tail=[-15, 0, 0, 0, 0, 0], tail_yaw=[20, 20, 15, 10, 5, 0],
               legs={'fl': dict(paw=(18, 0, 0), pitch=-80, toe=0), 'fr': dict(paw=(18, 0, 0), pitch=-80, toe=0),
                     'hl': dict(paw=(12, 3, 0), pitch=-80), 'hr': dict(paw=(12, 3, 0), pitch=-80)})
STAND = dict(tail=TAIL_REST)

def idle_over(base, i, n, wag=1.0):
    p = merge(base)
    br = wave(i, n, 2)                                      # two breaths in the loop
    p['spine'] = [p['spine'][0] - 0.4 * br, p['spine'][1] + 0.8 * br, p['spine'][2] - 0.4 * br]
    tb = p['tail'] or TAIL_REST
    if DOG:
        p['tail_yaw'] = [wag * 9 * wave(i, n, 3, -0.08 * k) for k in range(TAILN)]
        p['tail'] = [tb[k] + wag * 1.5 * wave(i, n, 6) for k in range(TAILN)]
    else:                                                   # a cat's slow tip curl
        p['tail_yaw'] = [(p['tail_yaw'] or [0] * TAILN)[k] + (k / TAILN) ** 2 * 18 * wave(i, n, 1, -0.1 * k) for k in range(TAILN)]
        p['tail'] = [tb[k] + (k / TAILN) * 8 * wave(i, n, 1, 0.25) for k in range(TAILN)]
    look = ease(min(1, max(0, (i - n * 0.35) / (n * 0.1)))) - ease(min(1, max(0, (i - n * 0.65) / (n * 0.1))))
    h = list(p['head']); h[1] += 18 * look; h[0] += 3 * look; p['head'] = h
    fl = math.exp(-((i - n * 0.8) / 2.5) ** 2)              # one ear flick
    p['ears'] = [p['ears'][0], p['ears'][1] - 25 * fl]
    return p

record('Idle', 180, lambda i, n: idle_over(STAND, i, n))
record('SitDown', 30, lambda i, n: blend(STAND, SIT, ease(i / (n - 1))))
record('SitIdle', 180, lambda i, n: idle_over(SIT, i, n, wag=0.6))
record('StandUp', 26, lambda i, n: blend(SIT, STAND, ease(i / (n - 1))))
def lie_down(i, n):
    t = i / (n - 1)
    if t < 0.45: return blend(STAND, SIT, ease(t / 0.45))   # down through sitting, as they do
    return blend(SIT, LIE, ease((t - 0.45) / 0.55))
record('LieDown', 44, lie_down)
def lie_idle(i, n):
    p = idle_over(LIE, i, n, wag=0.25)
    h = list(p['head']); h[1] = 0; p['head'] = h            # head stays down, breathing only
    return p
record('LieIdle', 180, lie_idle)
record('GetUp', 40, lambda i, n: lie_down(n - 1 - i, n))

# ---- gaits (in place): each paw goes back on the ground and forward through the air ----
def gait(i, n, phases, duty, stride, lift, bob, pitch_rock=0.0, flex=0.0, head_bob=0.0):
    t = i / n
    p = merge(STAND)
    legs = {}
    for k, ph in phases.items():
        s = (t - ph) % 1.0
        front = k.startswith('f')
        if s < duty:                                        # on the ground: from front to back
            u = s / duty; y = stride * (0.5 - u); z = 0
            pit = 25 * max(0, (u - 0.75) / 0.25) ** 2       # the heel lifts at the end
        else:                                               # in the air: back to the front
            u = (s - duty) / (1 - duty); e = ease(u)
            y = stride * (-0.5 + e); z = lift * math.sin(math.pi * u) * (1.0 if front else 0.8)
            pit = (70 if front else 45) * math.sin(math.pi * min(1, u * 1.3))
        legs[k] = dict(paw=(y, 0, z), pitch=pit, toe=0)
    p['legs'] = legs
    p['pelvis'] = (0, bob * math.cos(4 * math.pi * t) - abs(bob), pitch_rock * math.sin(2 * math.pi * t), 0, 0)
    p['spine'] = [flex * math.sin(2 * math.pi * t)] * 3
    p['head'] = (head_bob * math.cos(4 * math.pi * t) - pitch_rock * 0.5 * math.sin(2 * math.pi * t), 0, 0)
    p['tail'] = [TAIL_REST[k] + (10 if DOG else 15) * (1 - k / TAILN) for k in range(TAILN)]
    p['tail_yaw'] = [6 * math.sin(2 * math.pi * (t - 0.06 * k)) for k in range(TAILN)]
    return p

MOVES = {}
if DOG:
    n = 14; duty = 0.5; stride = 44                         # trot: diagonal pairs together
    record('WalkFast', n, lambda i, n_: gait(i, n_, {'fl': 0, 'hr': 0, 'fr': 0.5, 'hl': 0.5}, duty, stride, 9, 0.8,
                                              head_bob=1.5), loop=True)
    MOVES['WalkFast'] = stride * U / (duty * n / FPS)
    n = 11; duty = 0.3; stride = 60                         # gallop (rotary): hinds, then fronts, the back flexing
    record('Run', n, lambda i, n_: gait(i, n_, {'hl': 0, 'hr': 0.1, 'fr': 0.45, 'fl': 0.56}, duty, stride, 14, 2.5,
                                         pitch_rock=6, flex=7, head_bob=2), loop=True)
    MOVES['Run'] = stride * U / (duty * n / FPS)
else:
    n = 14; duty = 0.58; stride = 46                        # a fast walk: one paw after the other
    record('WalkFast', n, lambda i, n_: gait(i, n_, {'hl': 0, 'fl': 0.25, 'hr': 0.5, 'fr': 0.75}, duty, stride, 8, 0.5,
                                              head_bob=1), loop=True)
    MOVES['WalkFast'] = stride * U / (duty * n / FPS)
    # rolling over: from lying, over onto the back with the paws tucked up, and back down the other way round
    BACK = merge(LIE, pelvis=(-2, -29, 0, 180, 0), head=(-10, 0, 0), neck=(-25, -20), tail=[0] * TAILN,
                 legs={k: dict(paw=(10 if k.startswith('f') else 6, 0, 14), pitch=60, body=True) for k in LEGS})
    SIDE = merge(LIE, pelvis=(-2, -27, 0, 90, 0), neck=(-8, -8), head=(-4, 0, -40), legs={k: dict(paw=(8, 0, 6), pitch=40, body=True) for k in LEGS})
    def roll(i, n):
        t = i / (n - 1)
        if t < 0.25: return blend(LIE, SIDE, ease(t / 0.25))
        if t < 0.45: return blend(SIDE, BACK, ease((t - 0.25) / 0.2))
        if t < 0.6: return merge(BACK, tail_yaw=[10 * math.sin(6 * math.pi * (t - 0.45) / 0.15)] * TAILN)
        if t < 0.8: return blend(BACK, SIDE, ease((t - 0.6) / 0.2))
        return blend(SIDE, LIE, ease((t - 0.8) / 0.2))
    record('RollOver', 75, roll)

rig.animation_data.action = bpy.data.actions['Idle']
for m in mesh.modifiers: m.show_viewport = True
bpy.ops.object.mode_set(mode='OBJECT')
json.dump({'fps': FPS, 'moves': ACTIONS, 'speeds_m_per_s': MOVES}, open(os.path.join(D, NAME + '_moves.json'), 'w'), indent=1)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(D, NAME + '.blend'))
print('MOVES saved', NAME, MOVES)
