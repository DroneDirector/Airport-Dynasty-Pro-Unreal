# The last part of make_ambulift.py: the moving parts through their whole range, then the small trucks' own joining and saving.
BODYP = ('Box', 'Plat_', 'PlatRam', 'RailPlat', 'GateSlide', 'Bridge', 'DoorFront', 'DoorRear', 'Stretcher', 'Strap', 'BeltBay', 'Lever_Lift', 'Lever_Plat', 'Lever_Bridge', 'Blue', 'Beacon', 'Tail', 'NamePanel_L', 'NamePanel_R', 'Button_', 'Platform'); NOTB = ('TailBrake', 'BlueGrille', 'Button_Cab', 'Button_Horn', 'Button_Stab')
def isbody(n): return n.startswith(BODYP) and not n.startswith(NOTB)
def bv(o, m=None): m = m or o.matrix_world; return BVHTree.FromPolygons([m @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
def about(p, ax, deg): p = Vector(p); return Matrix.Translation(p) @ Matrix.Rotation(math.radians(deg), 4, ax) @ Matrix.Translation(-p)
T3 = Matrix.Translation
def aim(A, B0, B1): u0, u1 = (B0 - A).normalized(), (B1 - A).normalized(); return T3(A) @ u0.rotation_difference(u1).to_matrix().to_4x4() @ T3(-A)
EYE0, RB0, EA = EYE.copy(), RB.copy(), math.atan2(EYE.z - SZ0, SX - EYE.x); ER = math.hypot(EYE.z - SZ0, SX - EYE.x)
def pose(n, q=0, open_=0, rail=0, bridge=0, gate=0, arms=0, tail=0, trail=0, flap=0, door=0, legs=0):
    """Matrix of the part n. q lift (m); open_ 0 folded .. 90 platform level; rail 0 lying .. 90 standing; bridge / gate metres out; arms 0 floor .. -2 TA ground (deg, +down); tail 0 stands .. 90 level; trail rails up; flap 0 in line, 90 stop up, -12 ramp; door 0 .. .56."""
    U = T3((0, 0, q)); t = math.asin((H0 + q) / SL)
    if n.startswith('StabLeg'): return T3((0, 0, -legs))
    if n.startswith(('ScissorA_', 'ScissorB_')): sg = 1 if n[7] == 'A' else -1; c1 = Vector((SX - SL / 2 * math.cos(t), 0, SZ0 + (H0 + q) / 2)); return T3(c1) @ Matrix.Rotation((t - T0) * sg, 4, 'Y') @ T3(-m_)
    if n.startswith(('RamBarrel', 'RamRod')): a = EA + t - T0; E1 = Vector((SX - ER * math.cos(a), 0, SZ0 + ER * math.sin(a))); R = aim(RB0, EYE0, E1); return R if n[3] == 'B' else T3(E1 - RB0 - (E1 - RB0).normalized() * (EYE0 - RB0).length) @ R
    if not isbody(n): return Matrix.Identity(4)
    O = about((HXF, 0, HZF), 'Y', open_)
    if n.startswith('PlatRam'): B = BASE; E1 = O @ EYEF; R = aim(B, EYEF, E1); return U @ (R if 'Barrel' in n else T3(E1 - B - (E1 - B).normalized() * (EYEF - B).length) @ R)
    if n.startswith('Bridge'): return U @ O @ T3((0, 0, bridge))
    if n.startswith(('RailPlat_', 'GateSlide_')):
        sn = n.split('_')[1][0]; y, z = R_[sn]; F = FOLD @ RAILM[sn]; G = T3((gate, 0, 0)) if n[0] == 'G' else Matrix.Identity(4); return U @ O @ FOLD @ about((0, y, z), 'X', (90 - (min(90, 2 * rail) if sn == 'L' else max(0, 2 * rail - 90))) * (1 if sn == 'L' else -1)) @ G @ F.inverted()
    if n.startswith('Plat_'): return U @ O
    if n.startswith(('DoorFront', 'DoorRear')): return U @ T3((0, door * (1 if n[9 if n[4] == 'F' else 8] == 'L' else -1), 0))
    if n.startswith('Tail') and not n.startswith('TailArms'):
        tip = Vector((AX, 0, AZ)) + Matrix.Rotation(math.radians(arms), 3, 'Y') @ Vector((HX - AX, 0, HZ - AZ)); M = T3(tip - Vector((HX, 0, HZ))) @ about((HX, 0, HZ), 'Y', -tail)
        if n.startswith('TailRail_'): sn = n[9]; y, z = T_[sn]; return U @ M @ TFOLD @ about((0, y, z), 'X', (90 - (min(90, 2 * trail) if sn == 'L' else max(0, 2 * trail - 90))) * (1 if sn == 'L' else -1)) @ (TFOLD @ TRM[sn]).inverted()
        if n.startswith('TailFlap'): return U @ M @ TFOLD @ about((HX - TPL - .006, 0, BF - .012), 'Y', flap) @ TFOLD.inverted()
        return U @ M
    if n.startswith('TailArms'): return U @ about((AX, 0, AZ), 'Y', arms)
    return U
def grp(n): return next((g for g in ('ScissorA_', 'ScissorB_', 'RamBarrel_L', 'RamBarrel_R', 'RamRod_L', 'RamRod_R', 'PlatRamL', 'PlatRamR', 'Bridge', 'RailPlat_L', 'RailPlat_R', 'GateSlide_L', 'GateSlide_R', 'Plat_', 'DoorFrontL', 'DoorFrontR', 'DoorRearL', 'DoorRearR', 'TailArms', 'TailRail_L', 'TailRail_R', 'TailFlap', 'TailPlatform', 'StabLeg') if n.startswith(g)), 'body' if isbody(n) else 'fixed')
TOUCH = {frozenset(p) for p in (('ScissorA_', 'ScissorB_'), ('RamBarrel_L', 'RamRod_L'), ('RamBarrel_R', 'RamRod_R'), ('ScissorA_', 'RamRod_L'), ('ScissorA_', 'RamRod_R'), ('RailPlat_L', 'Plat_'), ('RailPlat_R', 'Plat_'), ('PlatRamL', 'Plat_'), ('PlatRamR', 'Plat_'), ('PlatRamL', 'body'), ('PlatRamR', 'body'), ('TailRail_L', 'TailPlatform'), ('TailRail_R', 'TailPlatform'), ('TailFlap', 'TailPlatform'), ('TailArms', 'TailPlatform'), ('RamBarrel_L', 'fixed'), ('RamBarrel_R', 'fixed'), ('StabLeg', 'fixed'), ('GateSlide_L', 'RailPlat_L'), ('GateSlide_R', 'RailPlat_R'))}
OKP = ('ScissorFootPin', 'ScissorFootRoller', 'BoxEarPin', 'BoxHingePin', 'BoxTailHanger', 'BoxTrack_', 'ScissorRail_', 'ScissorFoot_', 'BoxEarFront')
POSES = [('stowed', {}), ('legs', {'legs': .3}), ('q.5', {'q': .5}), ('q1.02', {'q': 1.02}), ('open30', {'q': 1.02, 'open_': 30}), ('open60', {'q': 1.02, 'open_': 60}), ('open90', {'q': 1.02, 'open_': 90}), ('rail45', {'q': 1.5, 'open_': 90, 'rail': 45}), ('rail22', {'q': 1.5, 'open_': 90, 'rail': 22}), ('rail67', {'q': 1.5, 'open_': 90, 'rail': 67}), ('out', {'q': 2.21, 'open_': 90, 'rail': 90, 'bridge': .5, 'gate': .40, 'door': .56}), ('q2.59', {'q': 2.59, 'open_': 90, 'rail': 90, 'bridge': .3, 'gate': .1}), ('top', {'q': 2.92, 'open_': 90, 'rail': 90}),
         ('tail30', {'tail': 30}), ('tail90', {'tail': 90}), ('trail', {'tail': 90, 'trail': 45}), ('tfloor', {'tail': 90, 'trail': 90, 'flap': 90, 'door': .56}), ('tmid', {'tail': 90, 'trail': 90, 'flap': 90, 'arms': -TA * 57.2958}), ('tground', {'tail': 90, 'trail': 90, 'flap': -12, 'arms': -2 * TA * 57.2958})]
def lift_check():
    bpy.context.view_layer.update(); bad0 = None
    for nm, kw in POSES:
        T = [(o.name, grp(o.name), bv(o, pose(o.name, **kw) @ o.matrix_world), o.name.startswith(OKP)) for o in ALL]; h = set()
        bb = {}
        for o in ALL: M = pose(o.name, **kw) @ o.matrix_world; c = [M @ Vector(v) for v in o.bound_box]; bb[o.name] = [(min(p[i] for p in c), max(p[i] for p in c)) for i in range(3)]
        mv = [x for x in T if x[1] not in ('fixed', 'body')]; lowbody = [x for x in T if x[1] == 'body' and bb[x[0]][2][0] < BF + kw.get('q', 0) + .1 and not x[3]]
        for a, g, t, k in mv + lowbody:
            for b, g2, t2, k2 in T:
                if g == g2 or (g == 'body' and g2 != 'fixed') or frozenset((g, g2)) in TOUCH or k or k2: continue
                if any(bb[a][i][0] > bb[b][i][1] or bb[b][i][0] > bb[a][i][1] for i in range(3)): continue
                if t.overlap(t2): h.add(tuple(sorted((a, b))))
        lo = min((pose(o.name, **kw) @ o.matrix_world @ v.co).z for o in ALL if o.name.startswith(('Tail', 'StabLeg')) for v in o.data.vertices); print(TAG, 'POSE', nm, 'lowest %.3f' % lo, len(h), sorted(h)[:7])
if '--nochk' not in sys.argv: lift_check()
if '--pose' in sys.argv:
    kw = dict(POSES)[sys.argv[sys.argv.index('--pose') + 1]]
    for o in ALL: o.matrix_world = pose(o.name, **kw) @ o.matrix_world
if '--views' in sys.argv:                                              # the blueprint's pictures (Tools/make_ambulift_blueprint.py)
    TMP = os.path.join(os.environ['TEMP'], 'ambulift_bp'); os.makedirs(TMP, exist_ok=True); sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading; sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_object_outline = True; sh.show_cavity = True; sc.world = bpy.data.worlds.new('W'); sc.world.color = (.012, .016, .022)
    for m in bpy.data.materials:
        if 'Glass' in m.name: m.diffuse_color = (.35, .55, .65, .45)
    cd = bpy.data.cameras.new('C'); cd.type = 'ORTHO'; c = bpy.data.objects.new('C', cd); sc.collection.objects.link(c); sc.camera = c; cd.clip_end = 200; M0 = {o.name: o.matrix_world.copy() for o in ALL}
    def shot(n, at, d, w, h, kw={}):
        for o in ALL: o.matrix_world = pose(o.name, **kw) @ M0[o.name]
        c.location = Vector(at) + Vector(d) * 40; c.rotation_euler = (0, 0, math.pi) if d[2] else (-Vector(d)).to_track_quat('-Z', 'Y').to_euler(); cd.ortho_scale = max(w, h); sc.render.resolution_x, sc.render.resolution_y = int(w * 150), int(h * 150); sc.render.filepath = os.path.join(TMP, n + '.png'); bpy.ops.render.render(write_still=True)
    shot('side', (0, 0, 2.1), (0, 1, 0), 11, 4.6); shot('front', (0, 0, 2.1), (1, 0, 0), 4, 4.6); shot('rear', (0, 0, 2.1), (-1, 0, 0), 4, 4.6); shot('top', (0, 0, 2), (0, 0, 1), 11, 4); shot('up', (.1, 0, 3.05), (0, 1, 0), 11.5, 6.97, {'q': 2.21, 'open_': 90, 'rail': 90, 'bridge': .5, 'gate': .4, 'legs': .3}); shot('tail', (-3.2, 0, 1.75), (0, 1, 0), 8.2, 4.0, {'tail': 90, 'trail': 90, 'flap': -12, 'arms': -2 * TA * 57.2958})
    sys.exit(0)
_st = SRC('Tools', 'small_truck.py'); _f = _st[_st.index('def finish_truck('):].replace('K.uv_cube();', 'K.uv_cube(); AFTER_UV();').replace('(2.590, .985, 1)', '(4.70, 1.18, 1)').replace(".68 if k[1] == 'L' else -.68", ".80 if k[1] == 'L' else -.80").replace('560, 400', '760, 520')
_f = _f.replace('((6.2, 5.2, 2.4), (.1, 0, 1.1), 42), ((-6.4, 4.6, 3.0), (-.3, 0, 1.1), 42), ((-4.6, -.6, 2.9), (-1.7, -.1, 1.3), 30), ((-5.0, -5.0, 2.6), (-.4, 0, 1.1), 40)', '((9.5, 7.5, 3.4), (.1, 0, 1.9), 40), ((-9.4, 6.2, 3.8), (-.5, 0, 1.9), 40), ((-3.3, .05, 2.75), (2.0, .0, 2.0), 18), ((6.2, 4.4, .7), (3.7, 1.0, .9), 45)'); assert '(9.5, 7.5' in _f
exec(compile(_f, 'small_truck finish', 'exec'))
GROUPS = ['ScissorA_', 'ScissorB_', 'RamBarrel_L', 'RamBarrel_R', 'RamRod_L', 'RamRod_R', 'PlatRamL_Barrel', 'PlatRamR_Barrel', 'PlatRamL_Rod', 'PlatRamR_Rod', 'Bridge', 'RailPlat_L', 'RailPlat_R', 'GateSlide_L', 'GateSlide_R', 'DoorFrontL', 'DoorFrontR', 'DoorRearL', 'DoorRearR', 'TailArms', 'TailRail_L', 'TailRail_R', 'TailFlap', 'TailPlatform', 'Lever_Stab', 'Lever_Lift', 'Lever_Plat', 'Lever_Bridge', 'Stretcher', 'Needle_Speed', 'Needle_Fuel'] + sorted(o.name for o in ALL if o.name.startswith('Button_')) + ['Lever_ParkBrake'] + ['StabLeg_' + k for k in ('FL', 'FR', 'RL', 'RR')] + ['Strap_%s_%d%d' % (n, k, e) for n in ('L0', 'L1', 'R0', 'R1') for k in (0, 1) for e in (0, 1)] + ['BeltBay%s_Tongue' % n for n in ('L0', 'L1', 'R0', 'R1')] + ['BoxSeatBelt%d_Tongue' % i for i in range(5)]
def gx(n):
    if n.startswith(('GearSelect', 'PtoLever')): return 'Lever_' + n.split('_')[0]
    if n.startswith('Dash_Dial') and n.endswith('_Glass'): return 'ClusterGlass'
    if n.startswith('Console'): return 'CabFittings'
    if n.startswith('Plat_'): return 'Platform'
    if n.startswith('Blue'): k = n.split('_')[0] + ('_' + n.split('_')[1][0] if n.startswith('BlueGrille') else ''); return k + ('_Cover_Glass' if n.endswith('_Glass') else '_Body')
    if n.startswith('Beacon'): return 'BoxBeacon'
    if n.startswith('Box') and n.endswith('_Glass'): return 'BoxGlass'
    if n.startswith('BoxSeat'): return 'BoxSeats'
    if n.startswith('BeltBay'): return 'BoxBayBelts'
    if n.startswith(('BoxHand', 'BoxCeil', 'BoxRailFloor', 'BoxMark', 'BoxHeater', 'BoxExt', 'BoxAid', 'BoxHammer', 'BoxExit', 'BoxPanel', 'BoxLamp', 'BoxSkirt', 'BoxFloorC', 'BoxSillDoor', 'BoxTrackDoor')): return 'BoxInside'
    if n == 'BoxShell': return 'BoxOutside'
    if n.startswith(('BoxUnder', 'BoxTrack_', 'BoxEar', 'BoxSill', 'BoxTail', 'BoxRam', 'BoxHinge')): return 'BoxUnderside'
    if n.startswith('Box'): return 'BoxOutside'
    if n.startswith(('Scissor', 'Ram', 'Stab', 'Hyd')): return 'LiftBase'
    return None
finish_truck(GROUPS, gx, [('DoorPassenger', (4.70, -1.18, 1), 'z', (15, 40, 68), ())], [],
    {'lift': {'q_m': [0, 2.92], 'floor_m': [BF, BF + 2.92], 'scissor': {'L': SL, 'x_fixed': SX, 'z_fixed': SZ0, 't_rad': 'asin((0.12 + q) / 4.60)', 't0_rad': round(T0, 5), 'centre': 'x = 2.35 - 2.30 cos t, z = 0.97 + (0.12 + q) / 2', 'A': '+(t - t0) about Blender y', 'B': '-(t - t0)'}, 'ram': {'base': [1.05, .70], 'eye_radius': round(ER, 4), 'eye_angle0': round(EA, 5)}, 'legs_down_m': .30},
     'platform': {'hinge': [HXF, HZF], 'open_deg_about_y': 90, 'min_q_m': 1.02, 'length': PL, 'width': 2 * PW, 'bumper_x_open': round(HXF + PL + .108, 3), 'bridge_out_m': [0, .5], 'gate_out_m': [0, .4], 'ram_base': [2.44, 1.02]}, 'tail': {'pivot': [AX, AZ], 'length': TL, 'arm_deg_floor_to_ground': round(-2 * math.degrees(TA), 2), 'hinge': [round(HX, 4), HZ], 'platform_open_deg_about_y': -90, 'flap_deg': [-12, 90]},
     'moves_with_body': {'prefixes': BODYP, 'not': NOTB}, 'door_slide_m': .56})
