# Blueprint of the catering high-loader (to-do 212): docs/Blueprints/Apron/CateringTruck.png and .pdf
# The views are the real model (Art/CateringTruck/CateringTruck.blend, flat orthographic), the dimensions measured on it.
#   python Tools/make_cateringtruck_blueprint.py
import json, math, os, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BL = r'C:/Program Files/Blender Foundation/Blender 3.3/blender.exe'
ART = os.path.join(ROOT, 'Art', 'CateringTruck')
# name: (direction, centre, width, near clip, pixels)
VIEWS = {'left': ('L', (0, 0, 1.95), 10.2, .1, (2040, 900)), 'front': ('F', (0, 0, 1.95), 4.4, .1, (880, 900)), 'rear': ('B', (0, 0, 1.95), 4.4, .1, (880, 900)), 'top': ('T', (0, 0, 0), 10.2, .1, (2040, 800)),
         'up': ('L', (.4, 0, 3.0), 11.6, .1, (2320, 1300)), 'under': ('D', (0, 0, 0), 10.2, .1, (2040, 640)), 'box': ('L', (-.95, 0, 2.6), 7.2, 28.86, (1440, 560)), 'tail': ('L', (-3.6, 0, 1.6), 5.0, .1, (900, 700))}
if '--blender' in sys.argv:
    import bpy
    from mathutils import Vector, Matrix
    out = sys.argv[-1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading
    sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.show_object_outline = True; sh.object_outline_color = (1, 1, 1); sh.show_specular_highlight = False
    w = bpy.data.worlds.new('K'); w.use_nodes = False; w.color = (0, 0, 0); sc.world = w; sc.display.render_aa = '16'; sc.view_settings.view_transform = 'Standard'
    J = json.load(open(os.path.join(ART, 'cateringtruck-parts.json'))); PT = {k: Vector((v[0] / 100, -v[1] / 100, v[2] / 100)) for k, v in J['points_cm'].items()}
    with bpy.data.libraries.load(os.path.join(ROOT, 'Art', 'CateringKit', 'CateringKit.blend')) as (a, b): b.objects = [n for n in a.objects if n in ('Trolley', 'TrolleyDoor', 'GalleyBox', 'GalleyBoxDoor')]
    kit = {o.name: o for o in b.objects}; stock = []
    def stand(mesh, at, yaw, own=(0, 0, 0)):
        o = bpy.data.objects.new('Kit_' + mesh, kit[mesh].data); sc.collection.objects.link(o); R = Matrix.Rotation(math.radians(yaw), 4, 'Z'); o.matrix_world = Matrix.Translation(at) @ R @ Matrix.Translation(own); stock.append(o)
    for s in J['stock']:
        at = Vector((s['at_cm'][0] / 100, -s['at_cm'][1] / 100, s['at_cm'][2] / 100)); y = s['yaw_blender_deg']; stand(s['mesh'], at, y); stand(s['mesh'] + 'Door', at, y, (-.145, .409, .14) if s['mesh'] == 'Trolley' else (0, .1985, .271))
    for m in bpy.data.materials:
        bs = m.node_tree.nodes.get('Principled BSDF') if m.use_nodes else None
        c = bs.inputs['Base Color'].default_value if bs else (.3, .3, .3, 1); m.diffuse_color = (*[max(v, .05) for v in c[:3]], .25 if 'Glass' in m.name and 'Mirror' not in m.name else 1)
        if any(k in m.name for k in ('Tyre', 'BumperPlastic', 'BlackPlastic', 'DashPlastic', 'DarkSteel', 'Seal', 'FloorMat', 'Hose', 'Rubber')): m.diffuse_color = (.10, .11, .12, 1)
        if any(k in m.name for k in ('_Steel', 'RimSteel', 'Aluminium', 'Diamond', 'Chassis')): m.diffuse_color = (.40, .41, .43, 1)
    cd = bpy.data.cameras.new('C'); cam = bpy.data.objects.new('C', cd); sc.collection.objects.link(cam); sc.camera = cam; cd.type = 'ORTHO'
    X = J['scissor']; SL, SX, Z0, H0 = [X[k] / 100 for k in ('arm_cm', 'front_x_cm', 'z0_cm', 'h0_cm')]; BOXP, NOT = tuple(J['box']['moves_with_box']), tuple(J['box']['not']); PLT = ('Platform', 'Gate', 'RailPlatform')
    base = {o: o.matrix_world.copy() for o in bpy.data.objects}
    def pose(q, slide):
        t0, t = math.asin(H0 / SL), math.asin((H0 + q) / SL); c0 = Vector((SX - SL / 2 * math.cos(t0), 0, Z0 + H0 / 2)); c1 = Vector((SX - SL / 2 * math.cos(t), 0, Z0 + (H0 + q) / 2)); r0 = PT['PIV_Ram_L'].copy(); r0.y = 0; e0 = PT['PIV_RamEye_L'].copy(); e0.y = 0
        r1 = Vector((SX - 2.6 * math.cos(t), 0, Z0 + 2.6 * math.sin(t) - .012)); Rr = (e0 - r0).rotation_difference(r1 - r0).to_matrix().to_4x4()
        for o, m in base.items():
            n = o.name; o.matrix_world = m
            if o.type != 'MESH': continue
            if n.startswith('Kit_') or (n.startswith(BOXP) and not n.startswith(NOT)):
                M = Matrix.Translation((slide if n.startswith(PLT) else 0, 0, q))
                if slide:
                    g = lambda p, a, ax: Matrix.Translation(PT[p]) @ Matrix.Rotation(math.radians(a), 4, ax) @ Matrix.Translation(-PT[p])
                    if n.startswith('RailPlatform_L'): M = M @ g('PIV_RailPlatform_L', -90, 'X')
                    if n.startswith(('RailPlatform_R', 'Gate')): M = M @ g('PIV_RailPlatform_R', 90, 'X')
                    if n.startswith('RampFlap'): M = M @ g('PIV_RampFlap', 106, 'Y')
                    if n.startswith(('RollF',)): M = Matrix.Translation((0, 0, 90))
                o.matrix_world = M @ m
            elif n.startswith(('ScissorA', 'ScissorB')): o.matrix_world = Matrix.Translation(c1) @ Matrix.Rotation((t - t0) * (1 if n[7] == 'A' else -1), 4, 'Y') @ Matrix.Translation(-c0) @ m
            elif n.startswith('RamBarrel'): o.matrix_world = Matrix.Translation(r0) @ Rr @ Matrix.Translation(-r0) @ m
            elif n.startswith('RamRod'): o.matrix_world = Matrix.Translation(r1) @ Rr @ Matrix.Translation(-e0) @ m
            elif n.startswith('StabLeg') and q: o.matrix_world = Matrix.Translation((0, 0, -.30)) @ m
    for n, (d, at, wd, cs, res) in VIEWS.items():
        pose(2.505 if n == 'up' else 0, 2.95 if n == 'up' else 0)
        for o in bpy.data.objects: o.hide_render = o.type == 'EMPTY' or (n == 'box' and o.name.startswith(('Box_Wall', 'BoxDoor', 'NamePanel', 'BoxOutside')))
        at = Vector(at); v = {'L': (0, 30, 0), 'F': (30, 0, 0), 'B': (-30, 0, 0), 'T': (0, 0, 30), 'D': (0, 0, -30)}[d]; cam.location = at + Vector(v); cam.rotation_euler = (0, 0, 0) if d == 'T' else (math.pi, 0, 0) if d == 'D' else (-Vector(v)).to_track_quat('-Z', 'Y').to_euler()
        if n == 'box': cam.location.y = 30
        if n == 'tail':
            a = J['tail']; A = PT['PIV_TailArms']; H = PT['PIV_TailPlatform']
            for o, m in base.items():
                if o.name.startswith('TailArms'): o.matrix_world = Matrix.Translation(A) @ Matrix.Rotation(math.radians(a['deg_down'] - a['deg_up']), 4, 'Y') @ Matrix.Translation(-A) @ m
                if o.name.startswith('TailPlatform'):
                    H1 = A + Vector((-a['arm_cm'] / 100 * math.cos(math.radians(a['deg_down'])), 0, a['arm_cm'] / 100 * math.sin(math.radians(a['deg_down'])))); o.matrix_world = Matrix.Translation(H1) @ Matrix.Rotation(math.radians(-90), 4, 'Y') @ Matrix.Translation(-H) @ m
        cd.ortho_scale = wd * (max(res) / res[0]); cd.clip_start = cs; cd.clip_end = 70; sc.render.resolution_x, sc.render.resolution_y = res
        sc.render.filepath = os.path.join(out, n + '.png'); bpy.ops.render.render(write_still=True)
    sys.exit(0)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle
tmp = tempfile.mkdtemp(); OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Apron')
r = subprocess.run([BL, '-b', os.path.join(ART, 'CateringTruck.blend'), '--python', os.path.abspath(__file__), '--', '--blender', tmp], capture_output=True, text=True, timeout=200)
if 'Error' in r.stdout + r.stderr: print((r.stdout + r.stderr)[-1500:])
D = json.load(open(os.path.join(ART, 'cateringtruck-parts.json'))); PT = {k: (v[0] / 100, -v[1] / 100, v[2] / 100) for k, v in D['points_cm'].items()}
BG, INK, DIM, YEL, ORG, GRN = '#000000', '#e8edf2', '#9fb4c8', '#f2c230', '#ff7a2a', '#55dd88'
plt.rcParams.update({'font.size': 11, 'text.color': INK, 'font.family': 'DejaVu Sans'})
fig = plt.figure(figsize=(34, 32), facecolor=BG); gs = fig.add_gridspec(4, 4, width_ratios=[1.3, 1.3, 1, 1], height_ratios=[1, 1, 1.5, 1.1])
def panel(cell, t, img=None, flip=False):
    a = fig.add_subplot(cell); a.set_facecolor(BG); a.set_title(t, color=INK, fontsize=17); a.axis('off')
    if img:
        d, at, w, _, (rx, ry) = VIEWS[img]; h = w * ry / rx; im = plt.imread(os.path.join(tmp, img + '.png')); cx = at[0] if d in 'LTD' else 0; mid = at[2] if d in 'LFB' else 0; lo, hi = cx - w / 2, cx + w / 2
        a.imshow(im, extent=(hi, lo, mid - h / 2, mid + h / 2) if flip else (lo, hi, mid - h / 2, mid + h / 2)); a.set_aspect('equal'); a.set_xlim((hi, lo) if flip else (lo, hi)); a.set_ylim(mid - h / 2 - .15, mid + h / 2 + .15)
    return a
def dim(a, p, q, t, c=DIM):
    a.annotate('', p, q, arrowprops=dict(arrowstyle='<->', color=c, lw=1.4)); a.text((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, t, color=c, ha='center', va='center', fontsize=12, bbox=dict(fc=BG, ec='none', pad=1.5), rotation=90 if abs(p[0] - q[0]) < .01 else 0)
def mark(a, x, y, t, c=ORG, dx=.2, dy=.2): a.plot([x], [y], 'o', color=c, ms=5); a.annotate(t, (x, y), (x + dx, y + dy), color=c, fontsize=11, arrowprops=dict(arrowstyle='-', color=c, lw=.8))
def lines(a, L, fs=11):
    a.set_xlim(0, 1); a.set_ylim(0, 1); a.set_aspect('auto')
    for i, t in enumerate(L): a.text(.0, .95 - i * .95 / max(len(L), 1), t, fontsize=fs, va='top', color=INK)
x0, x1 = D['x_m']; fx, rx_ = PT['PIV_Wheel_FL'][0], PT['PIV_Wheel_RL'][0]
a = panel(gs[0, :2], 'Left side, driving (mm)', 'left', True)
dim(a, (x0, -.22), (x1, -.22), '%d overall' % round((x1 - x0) * 1000)); dim(a, (fx, 0), (rx_, 0), 'wheelbase 4900'); dim(a, (4.85, 0), (4.85, 3.76), '3760'); dim(a, (-4.237, 4.02), (2.363, 4.02), 'body 6600'); dim(a, (-4.7, 0), (-4.7, 1.5), 'floor 1500')
for k, t, dx, dy in (('PIV_BoxDoorL', 'side door 100 deg', -.2, .9), ('IK_Step_BoxDoorL_1', 'three steps', -.1, -.5), ('IK_Lever_Stab', 'legs: lever + stop', -.2, -.75), ('PIV_StabLeg_RL', 'leg, 300 down', .1, -.2), ('PIV_TailPlatform', 'tail lift, upright', 0, -.45)): mark(a, PT[k][0], PT[k][2], t, ORG, dx, dy)
a = panel(gs[0, 2], 'Front', 'front'); dim(a, (-1.25, -.12), (1.25, -.12), '2500'); dim(a, (-1.5, 4.05), (1.5, 4.05), '3000 over the mirrors'); dim(a, (-.66, 3.47), (.66, 3.47), 'door 1320')
a = panel(gs[0, 3], 'Rear, tail lift upright', 'rear'); dim(a, (-1.16, -.12), (1.16, -.12), 'tail lift 2320'); dim(a, (-1.04, 3.3), (1.04, 3.3), 'door 2080')
a = panel(gs[1, :2], 'From above: where to stand (green), turning circle of the doors (orange)', 'top')
for k, v in PT.items():
    if k.startswith(('IK_Stand', 'IK_Tail_Stand', 'IK_Tail_Control_Stand')): a.plot([v[0]], [v[1]], 's', color=GRN, ms=8)
for p, ang0, r_, sg, n in (('PIV_DoorDriver', 180, 1.17, -1, 68), ('PIV_DoorPassenger', 180, 1.17, 1, 68), ('PIV_BoxDoorL', 180, .69, -1, 100)):
    x, y, _ = PT[p]; th = [math.radians(ang0 + sg * k * n / 24) for k in range(25)]; a.plot([x + r_ * math.cos(q) for q in th], [y + r_ * math.sin(q) for q in th], color=ORG, lw=1.6)
a = panel(gs[1, 2:], 'Body inside, left wall taken away: trolleys lengthwise, two beside each other, aisle 1.05 m', 'box', True)
mark(a, PT['IK_Lever_Lift'][0], PT['IK_Lever_Lift'][2], 'lift + platform levers, stop', ORG, .9, .75); mark(a, PT['IK_Strap_L1'][0], PT['IK_Strap_L1'][2], 'straps', ORG, -.2, .75)
a = panel(gs[2, :3], 'At the A320 front right door (sill 3.87 m on our model): floor 4.005, platform out 2950, rails up, legs down', 'up', True)
dim(a, (-4.9, 0), (-4.9, 4.005), 'floor 4005'); dim(a, (PT['IK_Platform_Bumper'][0] + 2.95, 5.65), (2.363, 5.65), 'platform over the cab'); a.axhline(3.87, color=YEL, ls='--', lw=1); a.axhline(3.49, color=YEL, ls=':', lw=1)
fus = plt.Circle((PT['IK_Platform_Bumper'][0] + 2.95 + 2.00, 4.35), 2.03, fc='none', ec=YEL, lw=1.5); a.add_patch(fus); a.text(-4.0, 3.60, 'sills: front right 3.87, rear right 3.49 (measured on Art/A320/A320.glb)', color=YEL); a.set_xlim(6.7, -5.4)
mark(a, PT['IK_Platform_Bumper'][0] + 2.95, 3.87, 'rubber bumper', ORG, .5, -.6); mark(a, 2.4, 3.95, 'ramp flap down', ORG, .6, .95)
a = panel(gs[2, 3], 'Tail lift on the ground', 'tail', True); dim(a, (-5.9, 0.02), (-5.9, 1.5), '1500'); dim(a, (-4.2, .25), (-5.75, .25), '1600')
a = panel(gs[3, 0], 'Parking place (airfield plan metres; north up)'); a.axis('on'); a.tick_params(colors=DIM); a.set_xlim(-272, -215); a.set_ylim(78, 184); a.set_aspect('equal')
a.add_patch(Rectangle((-272, 70), 72, 12.6, fc='#24303c', ec=INK)); a.text(-243, 79.5, 'terminal', ha='center'); a.axhspan(167, 175, color='#1b2733'); a.text(-230, 171, 'service road', ha='center', va='center'); a.plot([-265, -265], [82, 182], color=DIM, lw=1)
a.plot([-245.4, -245.4, -205], [82.6, 166, 166], color='#ff5555', lw=2); a.text(-230, 140, 'stand 9', ha='center'); a.add_patch(Rectangle((-261.35, 87), 3.5, 8, fc='none', ec=DIM)); a.text(-252.5, 100, 'cleaning van', ha='center', color=DIM); a.add_patch(Rectangle((-262.15, 139.9), 3.9, 12.2, fc='none', ec=DIM)); a.text(-264, 132, 'refueller', color=DIM)
a.add_patch(Rectangle((-255.45, 140.5), 3.9, 11, fc='none', ec=YEL, lw=2)); a.add_patch(Rectangle((-254.75, 141.5), 2.5, 8.93, fc=YEL)); a.text(-253, 113, 'high-loader, middle\n(-253.5, 146.0),\nfacing north', color=YEL, ha='center')
th = [math.radians(90 - 90 * k / 20) for k in range(21)]; a.plot([-260.1 + 6.6 * math.cos(q) for q in th] + [-253.5, -253.5], [164.4 + 6.6 * math.sin(q) for q in th] + [164.4, 146], color=GRN, lw=2); a.plot([-225, -260.1], [171, 171], color=GRN, lw=2); a.text(-249, 157, 'backed in: 18 m straight', color=GRN, fontsize=10)
a = panel(gs[3, 1], 'From below: chassis, scissor, rams, beams', 'under')
a = panel(gs[3, 2], 'How it works')
lines(a, ['Scissor: 2 pairs of arms 4400, 1.6 deg down to 40.7 deg; lift 0 .. 2750: floor 1500 .. 4250,', 'platform deck 135 lower (1365 .. 4115). Two rams push the arms.', 'Platform comes out only from lift 1800 up (then 60 mm over the beacon);', 'rails fold flat into the cassette under the floor, the gate inside the right rail.', 'Order: legs down - lift - platform - rails - gate open - ramp flap - door.', 'Legs: 4 x 300 mm. Tail lift: ground to floor, goes up with the body.', 'Roll-up doors front 1320 x 1900, rear 2080 x 1900: 12 slats each into a drum.', 'Cab: 2 seats, two steps a side, the refueller\'s cab (Tools/truck_cab.py).', 'Load: 18 full trolleys 302 x 810 x 1030, 12 standard boxes (Art/CateringKit).', 'Airport name at run time: both body sides 3600 x 620, cab front, tail lift 1100 x 260.'])
a = panel(gs[3, 3], 'Built from')
lines(a, ['Photos looked at (Wikimedia Commons, shape only, nothing copied):', '"LSG Sky Chefs loading on to Virgin America (9224573769)":', 'A320 rear right door, arms, box, railed platform;', '"Frankfurt Airbus A320 D-AIZT": truck driving, body down;', '"Brussels airport jet airways": raised at a wide body.', 'Sizes: the published class of narrow-body high-loaders from memory', '(8.5 - 10 m, 2.5 m wide, floor about 1.2 - 1.5 to 4 m and more);', 'no maker\'s sheet was read. Trolley: the common full size.', 'Our own design, no maker\'s name.'])
fig.suptitle('CATERING HIGH-LOADER   (to-do 212)', color=INK, fontsize=32); fig.tight_layout(rect=(0, 0, 1, .97))
for e in ('png', 'pdf'): fig.savefig(os.path.join(OUT, 'CateringTruck.' + e), facecolor=BG, dpi=75)
print('blueprint', OUT)
