# Blueprint of the passenger stairs (to-do 212): docs/Blueprints/Apron/PassengerStairs.* - the views are the real model (flat orthographic), driven to three heights.
# python Tools/make_passengerstairs_blueprint.py [roof]      (roof: docs/Blueprints/Apron/PassengerStairsRoof.*, the roofed version)
import json, math, os, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); BL = r'C:/Program Files/Blender Foundation/Blender 3.3/blender.exe'; ROOF = 'roof' in sys.argv; NAME, KEY = ('PassengerStairsRoof', 'passengerstairsroof') if ROOF else ('PassengerStairs', 'passengerstairs'); ART = os.path.join(ROOT, 'Art', NAME)
VIEWS = {'left': ('L', (0, 0, 1.7), 7.6, (1700, 900), 0), 'at346': ('L', (.8, 0, 2.3), 9.2, (1700, 1000), 6), 'at384': ('L', (.8, 0, 2.3), 9.2, (1700, 1000), 8), 'front': ('F', (0, 0, 1.7), 4.2, (900, 900), 0), 'rear': ('B', (0, 0, 2.3), 5.2, (900, 1000), 8),
         'top': ('T', (.8, 0, 0), 9.2, (1700, 800), 8), 'joint': ('L', (-1.6, 0, 1.1), 2.4, (1000, 900), 2.5)}
if ROOF: VIEWS.update({'left': ('L', (0, 0, 2.3), 7.6, (1700, 1150), 0), 'at346': ('L', (.8, 0, 2.9), 9.2, (1700, 1280), 6), 'at384': ('L', (.8, 0, 3.0), 9.2, (1700, 1320), 8), 'front': ('F', (0, 0, 2.4), 5.2, (900, 1000), 0), 'rear': ('B', (0, 0, 3.2), 7.0, (900, 1000), 8), 'hi': ('L', (.8, 0, 3.2), 9.2, (1700, 1400), 10)})
if '--blender' in sys.argv:
    import bpy
    from mathutils import Vector, Matrix
    out = sys.argv[-1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading
    sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.show_object_outline = True; sh.object_outline_color = (1, 1, 1); sh.show_specular_highlight = False
    w = bpy.data.worlds.new('K'); w.use_nodes = False; w.color = (0, 0, 0); sc.world = w; sc.display.render_aa = '16'; sc.view_settings.view_transform = 'Standard'
    C = {'Livery_Body': (.75, .77, .78), 'Livery_Main': (.04, .36, .34), 'Name': (.85, .85, .80), 'Yellow': (.9, .6, .05), 'Chev': (.9, .6, .05), 'Platform': (.34, .36, .38), 'Amber': (.95, .45, .05), 'RedL': (.8, .05, .04), 'SignRed': (.8, .05, .04), 'White': (.95, .95, .92),
         'Reflector': (.7, .72, .75), 'Seat': (.06, .07, .16), 'TreadAlu': (.40, .41, .43)}
    for m in bpy.data.materials: m.diffuse_color = (*next((c for k, c in C.items() if k in m.name), (.40, .41, .43) if any(k in m.name for k in ('_Steel', 'Rim', 'Alu', 'Diamond', 'Chassis', 'Trim', 'HeadL')) else (.10, .11, .12)), .25 if m.name == 'PS_Glass' else .5 if 'SmokedRoof' in m.name else 1)
    cd = bpy.data.cameras.new('C'); cam = bpy.data.objects.new('C', cd); sc.collection.objects.link(cam); sc.camera = cam; cd.type = 'ORTHO'; J = json.load(open(os.path.join(ART, KEY + '-parts.json')))['stairs']; B0 = {o: o.matrix_world.copy() for o in bpy.data.objects}
    G, R = J['going'], J['rise']; rot = Matrix.Rotation(math.radians(J['tread_fold_deg']), 4, 'Y')
    for n, (d, at, wd, res, f) in VIEWS.items():
        for o, m in B0.items():
            o.hide_render = o.type != 'MESH' or (n == 'joint' and o.name in ('OuterFittings', 'Underside', 'Body')); u = Matrix.Translation(Vector((G, 0, R)) * f); g = Matrix.Translation((.108 if f else 0, 0, 0))
            if o.name.startswith(('Up', 'StairUp', 'StepLights_Up', 'NamePanel_Plat')): o.matrix_world = u @ m
            elif o.name.startswith('GateSlide'): o.matrix_world = u @ g @ m
            elif o.name.startswith('TreadLow') and 2 <= int(o.name[-2:]) <= 1 + int(f): o.matrix_world = m @ rot
            elif o.name.startswith('Lift_'):
                A, T = B0[bpy.data.objects['PIV_Lift_' + o.name[5]]].translation, B0[bpy.data.objects['IK_LiftTop_' + o.name[5]]].translation; T2 = T + Vector((G, 0, R)) * f; e = (T2 - A).length - (T - A).length; q = (T - A).rotation_difference(T2 - A).to_matrix().to_4x4()
                o.matrix_world = Matrix.Translation((T2 - A).normalized() * e * int(o.name[7]) / 3) @ Matrix.Translation(A) @ q @ Matrix.Translation(-A) @ m
            elif o.name.startswith('Stab_') and f: o.matrix_world = Matrix.Translation((0, 0, -J['jack_stroke'])) @ m
            else: o.matrix_world = m
        v = {'L': (0, 30, 0), 'F': (30, 0, 0), 'B': (-30, 0, 0), 'T': (0, 0, 30)}[d]
        cam.location = Vector(at) + Vector(v); cam.rotation_euler = (0, 0, 0) if d == 'T' else (-Vector(v)).to_track_quat('-Z', 'Y').to_euler(); cd.ortho_scale = wd; cd.clip_start = 29.42 if n == 'joint' else .1; cd.clip_end = 70      # joint: cut open between the side panels
        sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(out, n + '.png'); bpy.ops.render.render(write_still=True)
    sys.exit(0)
src = open(os.path.join(ROOT, 'Tools', 'make_smalltrucks_blueprint.py'), encoding='utf-8').read()
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle
tmp = tempfile.mkdtemp(); OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Apron')
r = subprocess.run([BL, '-b', os.path.join(ART, NAME + '.blend'), '--python', os.path.abspath(__file__), '--', '--blender'] + (['roof'] if ROOF else []) + [tmp], capture_output=True, text=True, timeout=200)
if 'Error' in r.stdout + r.stderr: print((r.stdout + r.stderr)[-1500:])
D = json.load(open(os.path.join(ART, KEY + '-parts.json'))); PT = {k: (v[0] / 100, -v[1] / 100, v[2] / 100) for k, v in D['points_cm'].items()}; Q = D['stairs']; G, R = Q['going'], Q['rise']
BG, INK, DIM, YEL, ORG, GRN = '#000000', '#e8edf2', '#9fb4c8', '#f2c230', '#ff7a2a', '#55dd88'
plt.rcParams.update({'font.size': 11, 'text.color': INK, 'font.family': 'DejaVu Sans'})
fig = plt.figure(figsize=(32, 32), facecolor=BG); gs = fig.add_gridspec(4, 4, height_ratios=[1.1, 1.2, 1.15, 1.1])
exec(compile(src[src.index('def panel('):src.index('x0, x1 = D[')], 'panels', 'exec'))
x0, x1 = D['x_m']; fx, rx_ = PT['PIV_Wheel_FL'][0], PT['PIV_Wheel_RL'][0]
def man(a, x, f): a.plot([x - .1, x, x + .1], [f, f + .85, f], color=GRN, lw=4); a.plot([x, x], [f + .85, f + 1.45], color=GRN, lw=5); a.plot([x], [f + 1.64], 'o', color=GRN, ms=13)
a = panel(gs[0, :2], 'Left side, lowest (driving), mm', 'left'); dim(a, (x0, -.25), (x1, -.25), '%d overall' % round((x1 - x0) * 1000)); dim(a, (fx, 0), (rx_, 0), 'wheelbase %d' % round((fx - rx_) * 1000)); dim(a, (3.4, 0), (3.4, D['size_m'][2]), '%d' % round(D['size_m'][2] * 1000)); dim(a, (-3.3, 0), (-3.3, 2.32), 'platform 2320')
for k, t, dx, dy in (('IK_Step_DoorDriver', 'step 270, floor 480', .5, -.35), ('IK_Name_L', 'name written at run time', -.3, -.6), ('IK_Stab_FL', 'four jacks, stroke 141', .35, .55), ('IK_LiftTop_L', 'lift cylinder, 4 stages', .9, .5), ('IK_Beacon', 'beacon', .3, .2)): mark(a, PT[k][0], PT[k][2], t, ORG, dx, dy)
a = panel(gs[0, 2], 'Front', 'front'); dim(a, (-1.0, -.12), (1.0, -.12), '2000'); dim(a, (-1.235, 2.2), (1.235, 2.2), '2470 over the mirrors')
a = panel(gs[0, 3], 'Rear, platform 3.84', 'rear'); dim(a, (-.64, .5), (.64, .5), 'clear 1277'); dim(a, (-.95, 5.0), (.95, 5.0), 'platform 1900')
for cell, n, f, sill, nm in ((gs[1, :2], 'at346', 6, 3.49, 'L2 (aft)'), (gs[1, 2:], 'at384', 8, 3.87, 'L1 (front)')):
    a = panel(cell, 'At %s: platform %.2f, sill of our A320 %.2f (real 3.4)' % (nm, 2.32 + R * f, sill), n); xs = 1.95 + G * f; a.add_patch(Rectangle((xs, sill), .3, 1.85, fc='none', ec=YEL, lw=2)); a.add_patch(Circle((xs + 1.98, sill + .45), 1.98, fc='none', ec=YEL, lw=1.5)); a.plot([xs, xs + 2], [sill, sill], color=YEL, lw=2); a.plot([xs, xs + 1], [3.4, 3.4], color=YEL, ls=':')
    man(a, 1.2 + G * f, 2.32 + R * f); man(a, -2.75 + G * 5 + .12, .198 + R * 5); dim(a, (-2.95, 0), (-2.95, 2.32 + R * f), '%d' % round((2.32 + R * f) * 1000)); dim(a, (x1, 2.15), (xs, 2.15), '%d' % round((xs - x1) * 1000))
a = panel(gs[2, :2], 'From above at 3.84, gates out to the skin: feet and hands (green), doors (orange)', 'top')
for k, v in PT.items():
    u = G * 8 if k.startswith(('IK_TreadUp', 'IK_Rail_Up', 'IK_Rail_Plat', 'IK_Stand_Gate', 'IK_Plat')) else 0
    if k.startswith(('IK_Stand', 'IK_Tread', 'IK_Rail', 'IK_Plat_Agent')): a.plot([v[0] + u], [v[1]], 's', color=GRN, ms=4)
for p, sg in (('PIV_DoorDriver', -1), ('PIV_DoorPassenger', 1)):
    x, y, _ = PT[p]; th = [math.radians(180 + sg * k * 68 / 24) for k in range(25)]; a.plot([x + .96 * math.cos(q) for q in th], [y + .96 * math.sin(q) for q in th], color=ORG, lw=1.6)
if ROOF:
    a = panel(gs[2, 2], 'Section of the roofs (treads, hand rails, hoops), m'); a.axis('on'); a.tick_params(colors=DIM); a.set_xlim(-1.1, 1.1); a.set_ylim(-.15, 2.6); a.set_aspect('equal')
    def arch(w, hs, b, h0, c, lw): th = [math.pi * i / 200 for i in range(201)]; a.plot([w] + [w * math.copysign(abs(math.cos(q)) ** (2 / 3.6), math.cos(q)) for q in th] + [-w], [h0] + [hs + b * abs(math.sin(q)) ** (2 / 3.6) for q in th] + [h0], color=c, lw=lw)
    arch(.685, 1.582, .705, .59, YEL, 5); arch(.770, 1.58, .795, .62, INK, 5); arch(.968, 1.582, .705, .767, ORG, 2)
    for w, z0, z1, c in ((.655, -.1, .912, YEL), (.720, -.1, .88, INK)): a.plot([w, w], [z0, z1], color=c, lw=2); a.plot([-w, -w], [z0, z1], color=c, lw=2)
    for w, z, c in ((.655, 1.012, YEL), (.722, .985, INK)): a.plot([w, -w], [z, z], 'o', color=c, ms=7)
    a.plot([-.648, .648], [.032, .032], color=YEL, lw=4); a.plot([-.688, .688], [0, 0], color=INK, lw=3); man(a, .33, .032); man(a, -.33, .032); dim(a, (0, .032), (0, 2.272), 'middle 2240'); dim(a, (.56, .032), (.56, 2.085), '2053 or more'); a.text(0, -.1, 'yellow: upper roof   white: lower roof   orange: hood of the platform', ha='center', fontsize=10)
else: a = panel(gs[2, 2], 'The joint, cut open (2.5 treads up): uncovered treads open, covered ones folded', 'joint')
if ROOF: a = panel(gs[2, 3], 'Highest: platform 4.22, the roofs still overlap 0.48 m', 'hi'); a = fig.add_axes([0, 0, .001, .001]); a.axis('off')
else: a = panel(gs[2, 3], 'Treads')
a.set_xlim(-.1, 1.35); a.set_ylim(-.35, .75); a.set_aspect('equal'); f = .5
for k in range(4):
    X, Z = G * k, R * k
    if k < 2: a.plot([X, X + G], [Z, Z], color=INK, lw=4)
    else: a.plot([X + G - G * math.cos(math.atan2(R, G)), X + G], [Z - G * math.sin(math.atan2(R, G)), Z], color=DIM, lw=3)
    if k >= 1: a.plot([X + G * f, X + G * (1 + f) + .012], [Z + R * f + .032] * 2, color=YEL, lw=4); a.plot([X + G * (1 + f)] * 2, [Z + R * f + .03, Z + R * (1 + f) + .03], color=YEL, lw=1)
dim(a, (0, -.08), (G, -.08), '280'); dim(a, (-.05, 0), (-.05, R), '190'); a.text(0, -.2, 'white: lower treads   grey: folded under the upper flight\nyellow: upper flight, 32 above the lower nosing line', fontsize=11, va='top')
a = panel(gs[3, 0], 'At the A320 (metres, nose up; behind the nose wheel)'); a.axis('on'); a.tick_params(colors=DIM); a.set_xlim(-14, 6); a.set_ylim(-34, 8); a.set_aspect('equal')
a.plot([0, -1.2, -1.98, -1.98, -1.5, -.75, 0, .75, 1.5, 1.98, 1.98, 1.2, 0], [5, 3, 0, -22, -26, -30, -32.5, -30, -26, -22, 0, 3, 5], color=INK); a.plot([-1.98, -14, -14, -1.98], [-8.5, -15, -17, -14.5], color=DIM); a.plot([1.98, 6, 6], [-8.5, -10.7, -13], color=DIM)
for y, o, sk, an, t in ((.30, 6.19, 2.0, 0, 'L1'), (-24.77, 5.20, 1.57, 11, 'L2')):
    c, s_ = math.cos(math.radians(an)), math.sin(math.radians(an)); P = [(-sk - (X * c + Y * s_), y - X * s_ + Y * c) for X, Y in ((o - sk + 2.98, -1), (0, -1), (0, 1), (o - sk + 2.98, 1), (o - sk + 2.98, -1))]; a.plot([p[0] for p in P], [p[1] for p in P], color=YEL, lw=2); a.text(-o - 2.4, y + 1.6, t, color=YEL); a.annotate('', (-o - 3.2, y - 7.3 * s_), (-o - 6.5, y - 10.5 * s_), arrowprops=dict(arrowstyle='->', color=GRN, lw=2))
a = panel(gs[3, 1], 'Parking place (airfield plan metres; north up)'); a.axis('on'); a.tick_params(colors=DIM); a.set_xlim(528, 585); a.set_ylim(78, 184); a.set_aspect('equal'); a.axhspan(167, 175, color='#1b2733'); a.add_patch(Rectangle((570, 78), 15, 110, fc='#123018')); a.plot([528, 548.3, 548.3], [166, 166, 82], color='#ff5555', lw=2); a.text(536, 130, 'stand 16', ha='center')
a.add_patch(Rectangle((553.2, 106.5), 2.8, 7, fc='none', ec=DIM)); a.text(551, 103, 'toilet truck', color=DIM); a.plot([554.6, 554.6], [113.5, 166], color=DIM, ls=':'); a.plot([564.2, 564.2], [125, 166], color=DIM, ls=':'); hx, hy = 551.0 if ROOF else 557.9, 140.0
a.add_patch(Rectangle((hx - 1.3, hy - 3.2), 2.6, 6.4, fc='none', ec=YEL, lw=2)); a.add_patch(Rectangle((hx - 1, hy - 2.99), 2, 5.97, fc=YEL)); a.text(559.5, hy - 9, 'middle (%.1f, %.1f)\nfacing north%s' % (hx, hy, ', the open one at 557.9' if ROOF else ''), color=YEL)
th = [math.radians(-180 * k / 20) for k in range(21)]; ROOF or a.plot([hx + 9, hx + 9] + [hx + 4.5 + 4.5 * math.cos(q) for q in th] + [hx, hx], [hy + 30, hy + 15] + [hy + 15 + 4.5 * math.sin(q) for q in th] + [hy + 19, hy], color=GRN, lw=2); ROOF or a.text(hx + 10, hy + 22, 'in', color=GRN)
a = panel(gs[3, 2], 'How it works')
lines(a, ['Own small electric vehicle, two seats, 30 km/h; glass roof to see the platform.', 'One slope of 34.2 deg at every height: the upper flight with the platform slides up', 'along the lower one (3384 mm), carried by two 4-stage cylinders and rollers.', 'Platform 2320 .. 4220; whole treads at 2320 + 190 n (3460 = L2, 3840 = L1).', 'Covered lower treads fold down about their back edge (34.2 deg).', 'Step onto the upper flight: 32 .. 222 at any height.',
          'Upper treads closed; yellow nosings, three grit bands; step lights.', 'Handrails both sides 985 over the nosings (both flights run past each other).', 'Platform 1500 x 1900, sliding gates each side (stroke 400, 108 to the skin),', 'rubber bumper, emergency stops (platform, ground, cab).', 'Four jacks take the load before anybody walks on it.', 'Cab: speed and battery dials, stair display, stair lever (yellow), leg lever (blue).', 'Origin to bumper: 1.95 + 0.28 n m. Turning circle about 4.5 m, swept 7.1 m.', 'Size %.2f x 2.00 (2.47 mirrors) x %.2f m; %d triangles.' % (D['size_m'][0], D['size_m'][2], D['tris'])])
if ROOF:
    a = panel(gs[3, 3], 'The roof, and what it was built from')
    lines(a, ['Everything below the roof is the open version, piece for piece (same points).', 'Hoops dia. 30 on 12 x 40 straps, four bolts each, outside the hand rails;', 'smoked polycarbonate 4 mm under screwed aluminium strips; gutters at the edges.', 'Upper roof inside the lower one: 22 mm clear at the gutter, 12 heights checked;', 'rubber lips at both ends. Hood over the platform 1940 wide, open to the aircraft.', 'Side walls of the hood end 480 behind the bumper (the door swings there), the visor', '120 behind it, 2117 or more over the platform (door top 1880).', 'LED strips under the roof: with the step lights.', 'Height when low %.2f m (apron only), highest %.2f m.' % (D['size_m'][2], D['size_m'][2] + 1.9), 'Photos looked at (shape only, nothing copied): aviationpros.com - CKTS and', 'British Airways covered stairs (2024/03), TIMSAN; CNEOUP-type canopy stairs', '(ikrorwxhjkrqlr5q.ldycdn.com, smoked canopy on posts); aeroexpo: SOVAM', 'SPS9-19 (172526), AccessAir dual canopy (175130).'])
a = panel(gs[3, 3], 'Built from') if not ROOF else fig.add_axes([0, 0, .001, .001])
if ROOF: a.axis('off'); lines = lambda *k: None
lines(a, ['Photos looked at (Wikimedia Commons, shape only, nothing copied):', '"Spring Airlines aircraft with passenger boarding stairs @ PVG"', '(low cab under the platform, telescopic side panels);', '"Embarque por escalerilla en un Boeing 737-800 de Ryanair"', '(chassis, lift frame, jacks, platform sides);', '"Eurowings - Airbus A320 - D-ABZK - Cologne Bonn - boarding with 2 stairs".', 'Published figures: class 2.2 - 5.8 m, A318-A321 sills 3.42 - 3.73 m',
          '(door sill table of a maker), comparable stairs 6.2 - 8.2 m x 2.2 m.', 'Sills of our A320 measured on Art/A320/A320.glb: 3.87 / 3.49 m.', 'Not built: canopy (the roofed version: PassengerStairsRoof).', 'Textures: ambientCG CC0 (paint, plastic, rubber, metal, fabric), own dials.'])
[fig.delaxes(x) for x in list(fig.axes) if x.get_position().width < .002]; fig.suptitle('PASSENGER STAIRS, SELF-PROPELLED%s   (to-do 212)' % (', WITH A ROOF' if ROOF else ''), color=INK, fontsize=32); fig.tight_layout(rect=(0, 0, 1, .97))
for e in ('png', 'pdf'): fig.savefig(os.path.join(OUT, NAME + '.' + e), facecolor=BG, dpi=70)
print('blueprint', OUT)
