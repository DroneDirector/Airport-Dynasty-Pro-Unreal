# The airport express coach, part 2: the interior (run from Tools/make_coach.py, which made the shell: it uses its
# outer volume, openings and materials). His interior blueprint + interior wireframe sheet (2026-11-19):
#   inside width 2.41 (aisle 0.45, seat pairs 0.98), aisle headroom 1.96, floor 1.40 (200 mm deck), lockers 0.45 deep and
#   0.45 high from 1.45 over the floor, ceiling space 0.25, steps 3 x 0.35 at both doors, the driver on his own lower floor
#   (1.00) behind a partition, WC on the driver's side before the last row, raised back bench, wheelchair space with
#   fold-up seats, carry-on luggage rack by the front door, info screens, extinguisher + first aid.
# No gaps (owner): the linings are the shell's own inside face moved in (so they meet the skin everywhere, the window and
# door reveals included), and every panel runs a little into the ones it meets.
# Each swappable part is its own model: CoachInterior (the fixed fit-out), CoachDriverSeat, CoachSteeringWheel,
# CoachGuideSeat, CoachFoldSeat; the passenger seats are CoachSeat (pairs) + CoachBench, placed from Data/coach-layout.json.

T_LINING = K.tex('T_Coach_WallLining', (0.50, 0.52, 0.55), 0.05, 1.5)
carpet = K.blur(K.rng.random((K.N, K.N)), 1)
T_WALLCARPET = K.tex('T_Coach_WallCarpet', (0.10, 0.13, 0.20), 0.18, 3.5, pattern=carpet)
speck = (K.rng.random((K.N, K.N)) > 0.985).astype(float)
T_FLOOR = K.tex('T_Coach_FloorVinyl', (0.11, 0.115, 0.125), 0.08, 2.5, pattern=0.85 + speck * 0.6)
T_CEIL = K.tex('T_Coach_Ceiling', (0.70, 0.72, 0.74), 0.03, 1.0)
T_LOCKER = K.tex('T_Coach_Locker', (0.06, 0.10, 0.17), 0.04, 1.5)
T_WC = K.tex('T_Coach_WCPanel', (0.08, 0.22, 0.24), 0.04, 1.0)
T_DASH = K.tex('T_Coach_Dashboard', (0.025, 0.028, 0.032), 0.10, 4.0)
T_STEPEDGE = K.tex('T_Coach_StepEdge', (0.85, 0.65, 0.05), 0.05, 2.0)
T_SCREEN = K.tex('T_Coach_Screen', (0.02, 0.06, 0.09), 0.05, 0.5, pattern=((np.abs(((K.yy * 9) % 1) - 0.5) < 0.08)).astype(float))
T_POLE = K.tex('T_Coach_PoleRed', (0.55, 0.05, 0.04), 0.04, 0.8)
T_LEATHER = K.tex('T_Coach_DriverLeather', (0.03, 0.035, 0.045), 0.10, 4.0, pattern=K.blur(K.rng.random((K.N, K.N)), 2))
M['lining'] = mat('Coach_WallLining', T_LINING, 0.0, 0.55)
M['wallcarpet'] = mat('Coach_WallCarpet', T_WALLCARPET, 0.0, 0.95)
M['floor'] = mat('Coach_FloorVinyl', T_FLOOR, 0.0, 0.55)
M['ceiling'] = mat('Coach_Ceiling', T_CEIL, 0.0, 0.6)
M['locker'] = mat('Coach_Locker', T_LOCKER, 0.0, 0.4)
M['wc'] = mat('Coach_WCPanel', T_WC, 0.0, 0.35)
M['dash'] = mat('Coach_Dashboard', T_DASH, 0.0, 0.7)
M['stepedge'] = mat('Coach_StepEdge', T_STEPEDGE, 0.0, 0.5)
M['screen'] = mat('Coach_InfoScreen', T_SCREEN, 0.0, 0.1, (0.15, 0.55, 0.75), 1.2)
M['pole'] = mat('Coach_PoleRed', T_POLE, 0.3, 0.3, coat=0.5)
M['leather'] = mat('Coach_DriverLeather', T_LEATHER, 0.0, 0.6)
M['warm'] = mat('Coach_WarmLight', K.tex('T_Coach_WarmLight', (0.95, 0.85, 0.65), 0.01, 0.3), 0.0, 0.3, (1.0, 0.86, 0.62), 2.0)
M['reading'] = mat('Coach_ReadingLight', T_WHITE, 0.0, 0.2, (1.0, 0.96, 0.88), 0.4)

IN_W = 1.205                     # half the inside width
ROOF_IN = FLOOR + 1.96           # the ceiling, 3.36
LOCK_Z0, LOCK_Z1, LOCK_D = FLOOR + 1.45, ROOF_IN, 0.45
def bx(name, d0, d1, y0, y1, z0, z1, m, part='CoachInterior', r=0.004):
    return put(box(name, d2x(max(d0, d1)), d2x(min(d0, d1)), min(y0, y1), max(y0, y1), min(z0, z1), max(z0, z1), m, r), part)

# ---------------------------------------------------------------- the linings: the shell's inside face, moved in
lin = dup(outer, 'Lining', M['lining'])
bm = bmesh.new(); bm.from_mesh(lin.data); bm.normal_update()
for v in bm.verts: v.co -= v.normal * 0.0352      # (the skin's inside face, a hair in)
bm.to_mesh(lin.data); bm.free()
solidify(lin, 0.035)
low_cut = [box('cut', XR - 1, XF + 1, -2, 2, -1, 0.38),
           box('cut', d2x(9.45), d2x(1.72), -2, 2, -1, FLOOR - 0.15), box('cut', XR - 1, d2x(10.75), -2, 2, -1, FLOOR - 0.15),
           box('cut', d2x(10.75), d2x(9.45), 0.0, 2, -1, FLOOR - 0.15)]
boolean(lin, opening_boxes() + windscreen_wrap() + [cyl('arch', Vector((ax, 0, WR)), 0.625, W + 0.4, 'y', M['paint'], 64) for ax in (AX_F, AX_R)] + low_cut)
put(lin, 'CoachInterior')
# carpet on the walls below the windows (from the floor to the window sill), the plastic above
wc_ = dup(lin, 'WallCarpet', M['wallcarpet'])
boolean(wc_, [box('cut', XR - 1, XF + 1, -2, 2, FLOOR - 0.02, WIN_Z[0] - 0.02)], 'INTERSECT')
for v in wc_.data.vertices: v.co.y -= math.copysign(0.0015, v.co.y)
put(wc_, 'CoachInterior')

# ---------------------------------------------------------------- floors: the deck, the driver's lower floor, the wells
deck = box('Deck', XR + 0.12, d2x(0.30), -(IN_W + 0.03), IN_W + 0.03, FLOOR - 0.20, FLOOR, M['floor'], 0.0)
boolean(deck, [box('cut', d2x(1.66), d2x(0.20), RIGHT * 1.30, RIGHT * 0.55, 0, 2),            # the front door's steps
               box('cut', d2x(1.66), d2x(0.20), 0.07, 1.30, 0, 2),                           # the driver's floor, lower
               box('cut', d2x(10.62), d2x(9.58), RIGHT * 1.30, RIGHT * 0.55, 0, 2)])         # the middle door's steps
put(deck, 'CoachInterior')
bx('DriverFloor', 0.18, 1.66, 0.07, IN_W + 0.03, 1.00 - 0.15, 1.00, M['floor'])
bx('DriverRiser', 1.62, 1.66, 0.07, IN_W + 0.03, 1.00, FLOOR, M['floor'], r=0.002)
bx('DriverRiserSide', 0.20, 1.66, 0.05, 0.07, 1.00, FLOOR, M['floor'], r=0.002)
# the steps: three 0.35 m risers from the door sill (0.35) up to the floor
for di, (d0, d1) in enumerate(((0.53, 1.66), (9.58, 10.62))):
    for k in range(3):
        top = 0.35 + 0.35 * (k + 1) - (0.0 if k < 2 else 0.0)
        y_in = RIGHT * (W / 2 - 0.04) + (-RIGHT) * (0.25 * k)      # each step a tread further in
        y0, y1 = sorted((RIGHT * (W / 2 - 0.04), RIGHT * 0.55))
        tread_y0 = RIGHT * (W / 2 - 0.04 - 0.24 * k)
        bx(f'Step_{di}{k}', d0 + 0.01, d1 - 0.01, tread_y0, RIGHT * 0.55, 0.36 if k == 0 else 0.35 + 0.35 * k, top, M['floor'], r=0.003)
        bx(f'StepEdge_{di}{k}', d0 + 0.02, d1 - 0.02, tread_y0 - RIGHT * 0.0, tread_y0 - RIGHT * 0.04, top - 0.002, top + 0.002, M['stepedge'], r=0.001)
    bx(f'StepWellWall_{di}', d0 - 0.02, d0 + 0.01, RIGHT * (W / 2 - 0.04), RIGHT * 0.55, 0.35, FLOOR, M['lining'], r=0.0)
    bx(f'StepWellWallB_{di}', d1 - 0.01, d1 + 0.02, RIGHT * (W / 2 - 0.04), RIGHT * 0.55, 0.35, FLOOR, M['lining'], r=0.0)
    # handrails up the steps (owner's wireframe: poles by the door)
    for dd in (d0 + 0.06, d1 - 0.06):
        put(tube(f'DoorRail_{di}{dd:.1f}', [(d2x(dd), RIGHT * 0.58, 0.70), (d2x(dd), RIGHT * 0.58, FLOOR + 1.0), (d2x(dd), RIGHT * 0.58, ROOF_IN - 0.02)], 0.017, M['chrome'], 4, bez=False), 'CoachInterior')
# the raised floor of the back bench
bx('BenchPlatform', 11.90, 12.70, -(IN_W + 0.03), IN_W + 0.03, FLOOR, FLOOR + 0.25, M['floor'])
bx('BenchPlatformEdge', 11.88, 11.92, -IN_W, IN_W, FLOOR + 0.248, FLOOR + 0.252, M['stepedge'], r=0.001)
# floor lights along both sides of the aisle
for side in (-1, 1):
    bx(f'AisleLight_{side}', 1.80, 11.85, side * 0.215, side * 0.232, FLOOR, FLOOR + 0.003, M['reading'], r=0.0)

# ---------------------------------------------------------------- ceiling and lockers
bx('Ceiling', 0.42, 12.70, -(IN_W - LOCK_D) - 0.01, IN_W - LOCK_D + 0.01, ROOF_IN, ROOF_IN + 0.02, M['ceiling'], r=0.0)
bx('CeilingFront', 0.42, 1.72, -(IN_W + 0.03), IN_W + 0.03, ROOF_IN, ROOF_IN + 0.02, M['ceiling'], r=0.0)
for side in (-1, 1):
    bx(f'CeilingLight_{side}', 1.8, 12.5, side * 0.62, side * 0.66, ROOF_IN - 0.004, ROOF_IN, M['warm'], r=0.0)
for k in range(10): bx(f'CeilingVent_{k}', 2.2 + k * 1.0, 2.5 + k * 1.0, -0.05, 0.05, ROOF_IN - 0.006, ROOF_IN, M['grille'], r=0.002)
def lockers(side, d0, d1):
    y_wall, y_in = side * (IN_W + 0.03), side * (IN_W - LOCK_D)
    bx(f'Locker_{side}_{d0:.1f}', d0, d1, y_wall, y_in, LOCK_Z0, LOCK_Z1 + 0.02, M['locker'])
    # lids every 1.2 m with handles; the warm light strip along the front lower edge (his wireframe: the beige line)
    n = max(1, round((d1 - d0) / 1.2))
    for k in range(n):
        a, b = d0 + (d1 - d0) * k / n, d0 + (d1 - d0) * (k + 1) / n
        bx(f'LockerLid_{side}_{a:.1f}', a + 0.008, b - 0.008, y_in - side * 0.002, y_in - side * 0.008, LOCK_Z0 + 0.06, LOCK_Z1 - 0.03, M['locker'], r=0.006)
        bx(f'LockerHandle_{side}_{a:.1f}', (a + b) / 2 - 0.10, (a + b) / 2 + 0.10, y_in - side * 0.008, y_in - side * 0.02, LOCK_Z0 + 0.09, LOCK_Z0 + 0.11, M['chrome'], r=0.004)
    bx(f'LockerLight_{side}_{d0:.1f}', d0, d1, y_in - side * 0.001, y_in - side * 0.03, LOCK_Z0 - 0.005, LOCK_Z0 + 0.02, M['warm'], r=0.002)
lockers(RIGHT, 1.72, 9.55); lockers(RIGHT, 10.65, 12.65)
lockers(LEFT, 1.72, 9.48); lockers(LEFT, 10.98, 12.65)

# ---------------------------------------------------------------- the WC (driver's side, before the last row)
bx('WC', 9.50, 10.96, LEFT * (IN_W + 0.03), LEFT * 0.23, FLOOR, ROOF_IN + 0.02, M['wc'])
bx('WCDoor', 9.70, 10.40, LEFT * 0.23, LEFT * 0.222, FLOOR + 0.03, FLOOR + 1.90, M['locker'], r=0.006)
bx('WCDoorHandle', 10.30, 10.36, LEFT * 0.222, LEFT * 0.205, FLOOR + 1.00, FLOOR + 1.03, M['chrome'], r=0.004)
bx('WCSign', 9.95, 10.15, LEFT * 0.222, LEFT * 0.219, FLOOR + 1.62, FLOOR + 1.72, M['screen'], r=0.003)
bx('WCLock', 10.30, 10.36, LEFT * 0.222, LEFT * 0.215, FLOOR + 1.10, FLOOR + 1.13, M['red'], r=0.003)

# ---------------------------------------------------------------- the driver's area
# the partition behind the driver (to locker height, a glazed top), the red pole at its end
bx('DriverPartition', 1.64, 1.68, 0.07, IN_W + 0.03, 1.00, FLOOR + 1.20, M['locker'])
bx('DriverPartitionGlass', 1.65, 1.67, 0.10, IN_W, FLOOR + 1.20, LOCK_Z0, M['glass'], part='CoachGlass', r=0.0)
put(tube('RedPole', [(d2x(1.70), 0.05, FLOOR), (d2x(1.70), 0.05, ROOF_IN)], 0.019, M['pole'], 2, bez=False), 'CoachInterior')
# the dashboard (owner, 2026-11-19: "come up with an improved look"): its own model, CoachDashboard - a sculpted
# wrap-around dash under the windscreen that swells towards the driver, a hooded digital instrument cluster, an angled
# centre screen (route + door cameras), the door console with lit buttons, a switch bank, the PA microphone, pedals,
# the roller sun blind and the overhead cab console
def screen_tex(name, kind):
    Wt, Ht = 1024, 384
    u, v = np.meshgrid(np.linspace(0, 1, Wt), np.linspace(0, 1, Ht))
    img = np.zeros((Ht, Wt, 3)); img[...] = (0.008, 0.012, 0.02)
    def ring(cx, cy, r0, r1, a0, a1, col, frac=1.0):
        x, y = (u - cx) * Wt / Ht, v - cy
        rr = np.hypot(x, y); aa = np.degrees(np.arctan2(y, x))
        m = (rr > r0) & (rr < r1) & (aa > a0) & (aa < a0 + (a1 - a0) * frac)
        img[m] = col
    if kind == 'cluster':
        # speed (left) and power / charge (right) arcs with ticks, a battery bar, warning lamps, a range bar
        # (owner: digital - the live speed, its arc and the charge are drawn by the game while driving: only the scales here,
        #  the arcs' fill, the centre panel's speed and the range bar left empty)
        for cx, frac, col in ((0.22, 0.0, (0.10, 0.85, 0.78)), (0.78, 0.0, (0.15, 0.65, 0.95))):
            ring(cx, 0.48, 0.30, 0.32, -210 + 180, 30 + 180, (0.10, 0.14, 0.18))
            ring(cx, 0.48, 0.30, 0.32, -30, 210, (0.10, 0.14, 0.18))
            ring(cx, 0.48, 0.26, 0.30, -30, 210, col, frac)
            for t in range(13):
                a = math.radians(-30 + t * 20); x0 = cx + 0.34 * math.cos(a) * Ht / Wt; y0 = 0.48 + 0.34 * math.sin(a)
                m = (np.abs(u - x0) < 0.004) & (np.abs(v - y0) < 0.012); img[m] = (0.7, 0.75, 0.8)
        m = (np.abs(u - 0.5) < 0.09) & (v > 0.18) & (v < 0.26); img[m] = (0.10, 0.14, 0.18)

        for i, c in enumerate(((0.9, 0.6, 0.1), (0.1, 0.8, 0.3), (0.2, 0.5, 1.0), (0.9, 0.2, 0.15))):
            m = np.hypot((u - (0.43 + i * 0.047)) * Wt / Ht, v - 0.82) < 0.025; img[m] = c
        m = (np.abs(u - 0.5) < 0.07) & (np.abs(v - 0.55) < 0.12); img[m] = (0.04, 0.07, 0.10)    # the centre panel
    else:
        # the centre screen: two door camera views and the route bar
        for i in range(2):
            m = (u > 0.03 + i * 0.49) & (u < 0.48 + i * 0.49) & (v > 0.25) & (v < 0.95)
            img[m] = np.dstack([0.10 + 0.25 * v, 0.14 + 0.25 * v, 0.18 + 0.3 * v])[m]
            m2 = m & (np.abs(((u * 40) % 1) - 0.5) < 0.02) & (v < 0.5); img[m2] = (0.9, 0.8, 0.2)
        m = (v < 0.18) & (v > 0.05) & (u > 0.03) & (u < 0.97); img[m] = (0.05, 0.40, 0.38)
    im = bpy.data.images.new(name, Wt, Ht, alpha=False)
    px = np.ones((Ht, Wt, 4), np.float32); px[..., :3] = img; im.pixels = px.ravel()
    im.filepath_raw = os.path.join(ART, name + '.png'); im.file_format = 'PNG'; im.save()
    m = mat('Coach_' + name[2:], (im, T_BLACK[1]), 0.0, 0.08, (1, 1, 1), 1.6)
    nt = m.node_tree; ti = [n for n in nt.nodes if n.type == 'TEX_IMAGE' and n.image == im][0]
    nt.links.new(ti.outputs['Color'], nt.nodes['Principled BSDF'].inputs['Emission'])   # (it glows its own picture)
    return m
M['cluster'] = screen_tex('T_Coach_Cluster', 'cluster')
M['centrescreen'] = screen_tex('T_Coach_CentreScreen', 'centre')
def screen(name, c, w, h, yaw, tilt, m, part='CoachDashboard'):
    # a flat screen with its own 0..1 UVs, facing -x (towards the driver) turned by yaw / tilted back by tilt
    bm = bmesh.new()
    vs = [bm.verts.new((0, y, z)) for (y, z) in ((-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2))]
    f = bm.faces.new(vs[::-1]); uvl = bm.loops.layers.uv.new('UVMap')
    for lp in f.loops: lp[uvl].uv = (0.5 - lp.vert.co.y / w, lp.vert.co.z / h + 0.5)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(c) @ Matrix.Rotation(math.radians(yaw), 4, 'Z') @ Matrix.Rotation(math.radians(-tilt), 4, 'Y'), verts=bm.verts)
    o = K.link(name, bm); o['keep_uv'] = 1; put(K.finish(o, m), part)
    # its frame (owner: the screen sits within its frame): a bezel 3 cm larger all round, the same tilt, the screen 2 mm in
    bz = rblock(name + 'Bezel', (-0.017, 0, 0), (0.03, w + 0.06, h + 0.06), 0.008, M['dash'], cuts=2)
    bz.data.transform(Matrix.Translation(c) @ Matrix.Rotation(math.radians(yaw), 4, 'Z') @ Matrix.Rotation(math.radians(-tilt), 4, 'Y'))
    put(bz, part)
    return o
DRV_Y = 0.62
def dash_deform(co, u, v, w):
    bulge = 0.20 * math.exp(-((co.y - DRV_Y) / 0.42) ** 2) + 0.05
    co.x -= bulge * ss(0.3, -1.0, u)                         # the rear edge swells towards the driver
    co.z -= 0.045 * ss(-0.3, 1.0, u) * ss(0.0, 1.0, w)       # the top falls away towards the windscreen
    co.z += 0.025 * ss(-1.0, -0.4, u) * ss(0.0, 1.0, w) * math.exp(-((co.y - DRV_Y) / 0.5) ** 2)
    return co
put(rblock('Dash', (d2x(0.33), 0.0, 1.035), (0.46, 2.34, 0.24), 0.05, M['dash'], cuts=10, segs=4, sub=1, deform=dash_deform), 'CoachDashboard')
put(rblock('DashKnee', (d2x(0.52), DRV_Y, 0.86), (0.10, 0.70, 0.22), 0.04, M['dash'], cuts=4, sub=1), 'CoachDashboard')
put(rblock('DemisterSlot', (d2x(0.13), 0.0, 1.11), (0.03, 2.0, 0.006), 0.002, M['grille'], cuts=6), 'CoachDashboard')
acc = [(d2x(0.56 + 0.20 * math.exp(-((y - DRV_Y) / 0.42) ** 2) - 0.05) + 0.0, y, 1.13 + 0.02 * math.exp(-((y - DRV_Y) / 0.5) ** 2)) for y in np.linspace(-1.12, 1.12, 29)]
put(tube('DashAccent', acc, 0.004, M['blade'], 2), 'CoachDashboard')
# the instrument hood and the cluster under it (facing the driver, tilted back)
put(rblock('ClusterHood', (d2x(0.68), DRV_Y, 1.252), (0.12, 0.64, 0.05), 0.022, M['dash'], cuts=4, sub=1,
           deform=lambda co, u, v, w: co + Vector((0, 0, 0.015 * (1 - v * v)))), 'CoachDashboard')
screen('Cluster', Vector((d2x(0.745), DRV_Y, 1.19)), 0.52, 0.17, 180, 45, M['cluster'])
# the centre screen on its arm, turned towards the driver
screen('CentreScreen', Vector((d2x(0.68), 0.14, 1.27)), 0.27, 0.16, 180 - 20, 12, M['centrescreen'])
put(cyl('CentreScreenArm', Vector((d2x(0.63), 0.14, 1.17)), 0.018, 0.12, 'z', M['dash'], 16), 'CoachDashboard')
# the door console between the driver and the door: sloped top, round lit buttons (front door, middle door, kneel,
# ramp, hazard)
put(rblock('DoorConsole', (d2x(0.80), 0.04, 1.14), (0.46, 0.24, 0.30), 0.04, M['dash'], cuts=4, sub=1,
           deform=lambda co, u, v, w: co + Vector((0, 0, -0.06 * ss(0, 1, w) * ss(-1, 1, -u)))), 'CoachDashboard')
for i, (col, dd) in enumerate((('blade', 0.62), ('blade', 0.70), ('amber', 0.78), ('blade', 0.86), ('red', 0.94))):
    zt = 1.29 - 0.06 * ss(-1, 1, -((d2x(dd) - d2x(0.80)) / 0.23)) + 0.002
    put(cyl(f'DoorButton_{i}', Vector((d2x(dd), 0.04, zt)), 0.022, 0.012, 'z', M['black'], 24, 0.003), 'CoachDashboard')
    put(lathe(f'DoorButtonRing_{i}', [(0.022, 0.0), (0.026, 0.003), (0.026, 0.008), (0.022, 0.009)], M[col], 24, 'z', (d2x(dd), 0.04, zt - 0.004)), 'CoachDashboard')
# the switch bank (rockers) and the tachograph by the left wall
put(rblock('SwitchPod', (d2x(0.55), IN_W - 0.17, 1.12), (0.30, 0.26, 0.18), 0.03, M['dash'], cuts=2, sub=1), 'CoachDashboard')
for r_ in range(2):
    for c_ in range(5):
        put(rblock(f'Rocker_{r_}{c_}', (d2x(0.47 + r_ * 0.07), IN_W - 0.26 + c_ * 0.045, 1.213), (0.04, 0.025, 0.012), 0.004, M['black'], cuts=1), 'CoachDashboard')
        put(box(f'RockerLed_{r_}{c_}', d2x(0.47 + r_ * 0.07) + 0.012, d2x(0.47 + r_ * 0.07) + 0.016, IN_W - 0.26 + c_ * 0.045 - 0.004, IN_W - 0.26 + c_ * 0.045 + 0.004, 1.219, 1.221, M['amber'] if c_ == 2 else M['blade']), 'CoachDashboard')
put(rblock('Tachograph', (d2x(0.62), IN_W - 0.17, 1.03), (0.02, 0.18, 0.05), 0.004, M['black'], cuts=1), 'CoachDashboard')
# the PA microphone on its gooseneck
put(tube('MicNeck', [(d2x(0.62), -0.02, 1.17), (d2x(0.68), -0.03, 1.30), (d2x(0.76), -0.02, 1.36)], 0.007, M['black'], 12), 'CoachDashboard')
put(cyl('MicHead', Vector((d2x(0.78), -0.02, 1.37)), 0.018, 0.05, 'x', M['grille'], 16, 0.004), 'CoachDashboard')
# pedals: accelerator (organ type) and brake
put(rblock('Accelerator', (d2x(0.58), DRV_Y + 0.12, 1.08), (0.04, 0.09, 0.20), 0.01, M['rubber'], cuts=1,
           deform=lambda co, u, v, w: co + Vector((0.06 * w, 0, 0))), 'CoachDashboard')
put(rblock('BrakePedal', (d2x(0.60), DRV_Y - 0.08, 1.12), (0.04, 0.13, 0.12), 0.01, M['rubber'], cuts=1,
           deform=lambda co, u, v, w: co + Vector((0.05 * w, 0, 0))), 'CoachDashboard')
# the roller sun blind at the windscreen's top (half down on the driver's side) and the overhead cab console
put(cyl('BlindRoller', Vector((d2x(0.34), 0.62, 3.30)), 0.03, 1.10, 'y', M['dash'], 24, 0.006), 'CoachDashboard')
put(box('Blind', d2x(0.36), d2x(0.33), 0.10, 1.14, 2.95, 3.28, M['trim']), 'CoachDashboard')
put(rblock('CabConsole', (d2x(0.80), 0.62, ROOF_IN - 0.06), (0.40, 0.90, 0.12), 0.03, M['dash'], cuts=2, sub=1), 'CoachDashboard')
put(rblock('CabSpeaker', (d2x(0.80) - 0.201, 0.40, ROOF_IN - 0.07), (0.004, 0.20, 0.07), 0.01, M['grille'], cuts=1), 'CoachDashboard')
for i in range(4): put(rblock(f'CabSwitch_{i}', (d2x(0.80) - 0.201, 0.70 + i * 0.07, ROOF_IN - 0.07), (0.006, 0.04, 0.025), 0.004, M['black'], cuts=1), 'CoachDashboard')
put(cyl('CabLight', Vector((d2x(0.80), 0.95, ROOF_IN - 0.121)), 0.04, 0.006, 'z', M['reading'], 24), 'CoachDashboard')
# (the dash at a real height: its top 0.85 m over the driver's floor - the windscreen runs on down below it)
for o in list(K.ALL):
    if PART.get(o.name) == 'CoachDashboard' and not any(o.name.startswith(n) for n in ('Accelerator', 'BrakePedal', 'Blind', 'Cab')):
        o.data.transform(Matrix.Translation((0, 0, 0.60)))
put(rblock('DashLower', (d2x(0.40), DRV_Y, 1.27), (0.30, 0.86, 0.50), 0.04, M['dash'], cuts=3, sub=1), 'CoachDashboard')
put(rblock('DoorConsoleBase', (d2x(0.80), 0.04, 1.62), (0.40, 0.20, 0.40), 0.03, M['dash'], cuts=2, sub=1), 'CoachDashboard')
# extinguisher and first aid by the partition
put(cyl('Extinguisher', Vector((d2x(1.82), LEFT * (IN_W - 0.10), FLOOR + 0.30)), 0.075, 0.55, 'z', M['pole'], 24, 0.01), 'CoachInterior')
bx('FirstAid', 1.74, 1.96, LEFT * (IN_W - 0.01), LEFT * (IN_W - 0.10), FLOOR + 0.90, FLOOR + 1.15, M['white'])
# the carry-on rack by the front door: three aluminium shelves on four posts (his wireframe)
for dd in (1.74, 2.26):
    for yy_ in (RIGHT * 0.60, RIGHT * (IN_W - 0.05)):
        put(tube(f'RackPost_{dd}{yy_:.1f}', [(d2x(dd), yy_, FLOOR), (d2x(dd), yy_, LOCK_Z0)], 0.016, M['alloy'], 2, bez=False), 'CoachInterior')
for k, z in enumerate((FLOOR + 0.15, FLOOR + 0.70, FLOOR + 1.25)):
    bx(f'RackShelf_{k}', 1.74, 2.26, RIGHT * 0.60, RIGHT * (IN_W - 0.05), z, z + 0.025, M['alloy'], r=0.004)
    bx(f'RackLip_{k}', 1.74, 2.26, RIGHT * 0.59, RIGHT * 0.61, z, z + 0.06, M['alloy'], r=0.003)
# wheelchair space: the floor sign, a restraint belt box on the wall
bx('WheelchairSign', 2.55, 2.85, RIGHT * 0.75, RIGHT * 1.05, FLOOR, FLOOR + 0.002, M['blade'], r=0.0)
bx('WheelchairBelt', 2.95, 3.05, RIGHT * (IN_W - 0.01), RIGHT * (IN_W - 0.06), FLOOR + 0.60, FLOOR + 0.80, M['black'])
bx('WheelchairButton', 2.40, 2.46, RIGHT * (IN_W - 0.01), RIGHT * (IN_W - 0.025), FLOOR + 0.90, FLOOR + 0.96, M['blade'])
# the info screens (the front one over the aisle by the partition, one half way)
for k, dd in enumerate((1.80, 6.97)):
    bx(f'InfoScreen_{k}', dd, dd + 0.03, -0.30, 0.30, ROOF_IN - 0.40, ROOF_IN - 0.05, M['black'], r=0.008)
    bx(f'InfoScreenFace_{k}', dd + 0.03, dd + 0.032, -0.28, 0.28, ROOF_IN - 0.38, ROOF_IN - 0.07, M['screen'], r=0.002)

# ---------------------------------------------------------------- the driver's seat, the wheel, the guide's seat, the fold-up seats
def driver_seat():
    c = Vector((0, 0, 0))   # made at its own origin (the floor under its middle), placed by the layout
    put(cyl('DSPedestal', Vector((0, 0, 0.16)), 0.13, 0.30, 'z', M['chassis'], 32, 0.01), 'CoachDriverSeat')
    put(lathe('DSBellow', [(0.10, 0.04), (0.12, 0.08), (0.10, 0.12), (0.12, 0.16), (0.10, 0.20), (0.12, 0.24), (0.10, 0.28)], M['rubber'], 32, 'z'), 'CoachDriverSeat')
    put(rblock('DSCushion', (0, 0.0, 0.40), (0.50, 0.50, 0.10), 0.035, M['leather'], cuts=6, sub=1,
               deform=lambda co, u, v, w: co + Vector((0, 0, 0.025 * ss(0.5, 0.95, abs(u)) * ss(-1, 1, w)))), 'CoachDriverSeat')
    br = rblock('DSBack', (0, 0.0, 0.0), (0.48, 0.11, 0.62), 0.04, M['leather'], cuts=6, sub=1,
                deform=lambda co, u, v, w: co + Vector((0, 0, 0)) - Vector((0, 0.045 * ss(0.5, 0.95, abs(u)) * ss(0.5, -1, v), 0)))
    hr = rblock('DSHead', (0, 0.0, 0.42), (0.28, 0.10, 0.16), 0.04, M['leather'], cuts=4, sub=1)
    for o in (br, hr):
        o.data.transform(Matrix.Translation((0, 0.25, 0.75)) @ Matrix.Rotation(-0.25, 4, 'X') @ Matrix.Translation((0, 0, 0)))
        put(o, 'CoachDriverSeat')
    for s_ in (-1, 1): put(rblock(f'DSArm_{s_}', (s_ * 0.28, 0.05, 0.62), (0.05, 0.32, 0.05), 0.02, M['dash'], cuts=2), 'CoachDriverSeat')
    put(rblock('DSLever', (0.27, -0.12, 0.36), (0.02, 0.10, 0.02), 0.008, M['black'], cuts=1), 'CoachDriverSeat')
def steering_wheel():
    # a bus wheel (0.50 across, four spokes), on its column; made flat at its own origin, tilted by the layout
    put(lathe('SWRim', [(0.235, 0.0), (0.245, -0.012), (0.257, 0.0), (0.245, 0.014), (0.235, 0.0)], M['leather'], 96, 'z'), 'CoachSteeringWheel')
    put(cyl('SWHub', Vector((0, 0, 0.0)), 0.075, 0.07, 'z', M['dash'], 32, 0.01), 'CoachSteeringWheel')
    put(cyl('SWBadge', Vector((0, 0, 0.036)), 0.045, 0.004, 'z', M['chrome'], 32), 'CoachSteeringWheel')
    for i, a in enumerate((0, 90, 180, 270)):
        o = rblock(f'SWSpoke_{i}', (0.155, 0, 0.0), (0.17, 0.035, 0.018), 0.008, M['dash'], cuts=2)
        o.data.transform(Matrix.Rotation(math.radians(a + 45), 4, 'Z')); put(o, 'CoachSteeringWheel')
    put(cyl('SWColumn', Vector((0, 0, -0.30)), 0.04, 0.55, 'z', M['dash'], 24), 'CoachSteeringWheel')
    put(cyl('SWColumnShroud', Vector((0, 0, -0.10)), 0.07, 0.16, 'z', M['dash'], 24, 0.01), 'CoachSteeringWheel')
def fold_seat(name, part):
    # a folding seat on its wall bracket (the guide's by the front door, two in the wheelchair space)
    put(rblock(name + 'Cushion', (0, -0.22, 0.45), (0.42, 0.40, 0.07), 0.025, M['wallcarpet'], cuts=4, sub=1), part)
    put(rblock(name + 'Back', (0, 0.0, 0.80), (0.42, 0.07, 0.50), 0.025, M['wallcarpet'], cuts=4, sub=1), part)
    put(rblock(name + 'Bracket', (0, 0.03, 0.40), (0.30, 0.04, 0.12), 0.01, M['chassis'], cuts=1), part)
    for s_ in (-1, 1): put(tube(name + f'Arm{s_}', [(s_ * 0.20, 0.03, 0.40), (s_ * 0.20, -0.30, 0.42)], 0.012, M['chrome'], 2, bez=False), part)
driver_seat(); steering_wheel(); fold_seat('GuideSeat', 'CoachGuideSeat'); fold_seat('FoldSeat', 'CoachFoldSeat')

# ---------------------------------------------------------------- the layout of the swappable parts
SEAT_ROWS = {RIGHT: [3.09 + 0.80 * k for k in range(8)] + [11.05], LEFT: [2.30 + 0.80 * k for k in range(9)] + [11.05]}
PAIR_Y = 0.225 + 0.49
def seat_place(cf, side):
    # the pair's origin is 0.50 behind its cushion's front; it faces forward (+x); the left side's pairs are mirrored
    # (the wall at +y), so its wall rail and bracket go to the wall
    return [d2x(cf + 0.50), side * PAIR_Y, FLOOR, 90.0, -1 if side == LEFT else 1]
INTERIOR_LAYOUT = {
    'seat_pairs': [seat_place(cf, s) for s in (RIGHT, LEFT) for cf in SEAT_ROWS[s]],
    'bench': [d2x(12.12 + 0.30), 0.0, FLOOR + 0.25, 90.0, 1],
    'driver_seat': [d2x(1.30), 0.62, 1.00, 90.0, 1],          # not sittable (owner: only the AI driver)
    'steering_wheel': [d2x(0.98), 0.62, 1.78, 90.0, 1, 30.0],  # 0.78 m over the driver's floor, tilted 30 deg from flat
    'guide_seat': [d2x(0.75), RIGHT * 0.30, FLOOR, 90.0, 1],
    'fold_seats': [[d2x(2.62), RIGHT * (IN_W - 0.05), FLOOR, 180.0, 1], [d2x(3.04), RIGHT * (IN_W - 0.05), FLOOR, 180.0, 1]],
    'sittable': ['seat_pairs', 'bench', 'guide_seat', 'fold_seats'],
    # (owner: the AI driver's hands lie naturally on the wheel - the rim's 9 and 3 o'clock points, for the hand IK;
    #  the wheel 0.47 m ahead of a seated driver's shoulders and 0.20 m below them, the rim 11 cm over the thighs)
    # the cluster is a screen: the game draws the live speed (centre panel), the speed and power arcs and the range bar onto it
    'cluster_live_areas': {'speed': [0.43, 0.43, 0.57, 0.67], 'speed_arc': [0.06, 0.12, 0.38, 0.86], 'power_arc': [0.62, 0.12, 0.94, 0.86], 'range_bar': [0.41, 0.19, 0.59, 0.25]},
    'wheel_grips': [[d2x(0.98) + 0.0, 0.62 + s_ * 0.245, 1.78] for s_ in (-1, 1)],
    'floor': FLOOR, 'aisle_half': 0.225, 'inside_half': IN_W}
