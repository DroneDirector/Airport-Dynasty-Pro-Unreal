# Blueprints of the potable water truck and the toilet truck (to-do 212): docs/Blueprints/Apron/WaterTruck.* and ToiletTruck.*
# The views are the real models (flat orthographic), the dimensions measured on them.   python Tools/make_smalltrucks_blueprint.py WaterTruck|ToiletTruck
import json, math, os, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); BL = r'C:/Program Files/Blender Foundation/Blender 3.3/blender.exe'
NAME = next(a for a in sys.argv if a in ('WaterTruck', 'ToiletTruck')); KEY = NAME.lower(); WATER = NAME == 'WaterTruck'; ART = os.path.join(ROOT, 'Art', NAME)
VIEWS = {'left': ('L', (0, 0, 1.25), 6.8, (1700, 900), 0), 'right': ('R', (0, 0, 1.25), 6.8, (1700, 900), 0), 'front': ('F', (0, 0, 1.25), 3.4, (900, 900), 0), 'rear': ('B', (0, 0, 1.25), 3.4, (900, 900), 0),
         'top': ('T', (0, 0, 0), 6.8, (1700, 850), 0), 'under': ('D', (0, 0, 0), 6.8, (1700, 700), 0), 'work': ('B', (0, 0, 1.75), 4.6, (1150, 1000), 0 if WATER else .73), 'works': ('L', (-1.6, 0, 1.75), 4.6, (1150, 1000), 0 if WATER else .73)}
if '--blender' in sys.argv:
    import bpy
    from mathutils import Vector
    out = sys.argv[-1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading
    sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.show_object_outline = True; sh.object_outline_color = (1, 1, 1); sh.show_specular_highlight = False
    w = bpy.data.worlds.new('K'); w.use_nodes = False; w.color = (0, 0, 0); sc.world = w; sc.display.render_aa = '16'; sc.view_settings.view_transform = 'Standard'
    for m in bpy.data.materials:
        bs = m.node_tree.nodes.get('Principled BSDF') if m.use_nodes else None
        c = bs.inputs['Base Color'].default_value if bs else (.3, .3, .3, 1); m.diffuse_color = (*[max(v, .05) for v in c[:3]], .25 if 'Glass' in m.name and 'Mirror' not in m.name else 1)
        if any(k in m.name for k in ('Tyre', 'BumperPlastic', 'BlackPlastic', 'DashPlastic', 'DarkSteel', 'Seal', 'FloorMat', 'HoseRubber')): m.diffuse_color = (.10, .11, .12, 1)
        if any(k in m.name for k in ('_Steel', 'RimSteel', 'Aluminium', 'Diamond', 'Chassis')): m.diffuse_color = (.40, .41, .43, 1)
    cd = bpy.data.cameras.new('C'); cam = bpy.data.objects.new('C', cd); sc.collection.objects.link(cam); sc.camera = cam; cd.type = 'ORTHO'
    J = json.load(open(os.path.join(ART, KEY + '-parts.json'))); PF = tuple(J.get('lift', {}).get('moves', ())); base = {o: o.location.copy() for o in bpy.data.objects}
    for n, (d, at, wd, res, q) in VIEWS.items():
        for o, l in base.items():
            o.hide_render = o.type != 'MESH'
            if PF and o.name.startswith(PF): o.location = l + Vector((0, 0, q))
        v = {'L': (0, 30, 0), 'R': (0, -30, 0), 'F': (30, 0, 0), 'B': (-30, 0, 0), 'T': (0, 0, 30), 'D': (0, 0, -30)}[d]; cam.location = Vector(at) + Vector(v); cam.rotation_euler = (0, 0, 0) if d == 'T' else (math.pi, 0, 0) if d == 'D' else (-Vector(v)).to_track_quat('-Z', 'Y').to_euler()
        cd.ortho_scale = wd; cd.clip_start = .1; cd.clip_end = 70; sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(out, n + '.png'); bpy.ops.render.render(write_still=True)
    sys.exit(0)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
tmp = tempfile.mkdtemp(); OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Apron')
r = subprocess.run([BL, '-b', os.path.join(ART, NAME + '.blend'), '--python', os.path.abspath(__file__), '--', NAME, '--blender', tmp], capture_output=True, text=True, timeout=200)
if 'Error' in r.stdout + r.stderr: print((r.stdout + r.stderr)[-1500:])
D = json.load(open(os.path.join(ART, KEY + '-parts.json'))); PT = {k: (v[0] / 100, -v[1] / 100, v[2] / 100) for k, v in D['points_cm'].items()}
BG, INK, DIM, YEL, ORG, GRN = '#000000', '#e8edf2', '#9fb4c8', '#f2c230', '#ff7a2a', '#55dd88'
plt.rcParams.update({'font.size': 11, 'text.color': INK, 'font.family': 'DejaVu Sans'})
fig = plt.figure(figsize=(32, 30), facecolor=BG); gs = fig.add_gridspec(4, 4, height_ratios=[1, 1, 1.15, 1.1])
def panel(cell, t, img=None):
    a = fig.add_subplot(cell); a.set_facecolor(BG); a.set_title(t, color=INK, fontsize=17); a.axis('off')
    if img:
        d, at, w, (rx, ry), _ = VIEWS[img]; h = w * ry / rx; im = plt.imread(os.path.join(tmp, img + '.png')); mid = at[2] if d in 'LRFB' else 0; fl = d in 'LF'
        cx = at[0] if d in 'LRTD' else 0; lo, hi = cx - w / 2, cx + w / 2; E = (hi, lo) if fl else (lo, hi)
        if d == 'B': E = (hi, lo)
        if d == 'F': E = (lo, hi)
        a.imshow(im, extent=E + (mid - h / 2, mid + h / 2)); a.set_aspect('equal'); a.set_xlim(E); a.set_ylim(mid - h / 2 - .1, mid + h / 2 + .1)
    return a
def dim(a, p, q, t, c=DIM):
    a.annotate('', p, q, arrowprops=dict(arrowstyle='<->', color=c, lw=1.4)); a.text((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, t, color=c, ha='center', va='center', fontsize=12, bbox=dict(fc=BG, ec='none', pad=1.5), rotation=90 if abs(p[0] - q[0]) < .01 else 0)
def mark(a, x, y, t, c=ORG, dx=.2, dy=.2): a.plot([x], [y], 'o', color=c, ms=5); a.annotate(t, (x, y), (x + dx, y + dy), color=c, fontsize=11, arrowprops=dict(arrowstyle='-', color=c, lw=.8))
def lines(a, L, fs=11.5):
    a.set_xlim(0, 1); a.set_ylim(0, 1); a.set_aspect('auto')
    for i, t in enumerate(L): a.text(.0, .97 - i * .97 / max(len(L), 1), t, fontsize=fs, va='top', color=INK)
x0, x1 = D['x_m']; fx, rx_ = PT['PIV_Wheel_FL'][0], PT['PIV_Wheel_RL'][0]
a = panel(gs[0, :2], 'Left side (mm)', 'left'); dim(a, (x0, -.22), (x1, -.22), '%d overall' % round((x1 - x0) * 1000)); dim(a, (fx, 0), (rx_, 0), 'wheelbase %d' % round((fx - rx_) * 1000)); dim(a, (3.15, 0), (3.15, 2.52), '2520')
dim(a, (-3.1, 0), (-3.1, PT['IK_Platform_Work'][2]), 'platform %d' % round(PT['IK_Platform_Work'][2] * 1000))
for k, t, dx, dy in (('IK_Step_DoorDriver', 'step 440, floor 820', .3, -.35), ('IK_ChargeFlap', 'charging flap', 0, -.45), ('IK_Step_Platform_0', 'step', .3, -.25)) + ((('IK_Handle_PanelDoorL', 'fill point door 110 deg', -.1, .75), ('IK_Grab_Platform_R', 'grab rails', -.1, .5)) if WATER else (('IK_WasteCoupling_Mouth', 'waste coupling on its cradle', .1, .55), ('IK_SplashTray', 'splash tray', .5, -.35), ('IK_Lever_Vent', 'tank panel', .6, -.5))): mark(a, PT[k][0], PT[k][2], t, ORG, dx, dy)
a = panel(gs[0, 2:], 'Right side', 'right'); mark(a, PT['IK_Chock_0'][0], PT['IK_Chock_0'][2], 'own chocks', ORG, 0, -.5); mark(a, PT['IK_Name_R'][0], PT['IK_Name_R'][2], 'name written at run time', YEL, 0, .9)
a = panel(gs[1, 0], 'Front', 'front'); dim(a, (-1.0, -.12), (1.0, -.12), '2000'); dim(a, (-1.255, 2.65), (1.255, 2.65), '2510 over the mirrors')
a = panel(gs[1, 1], 'Rear', 'rear'); dim(a, (-.97, -.12), (.97, -.12), '1940')
a = panel(gs[1, 2:], 'From above: where to stand (green), doors (orange)', 'top')
for k, v in PT.items():
    if k.startswith('IK_Stand'): a.plot([v[0]], [v[1]], 's', color=GRN, ms=7)
for p, sg in (('PIV_DoorDriver', -1), ('PIV_DoorPassenger', 1)):
    x, y, _ = PT[p]; th = [math.radians(180 + sg * k * 68 / 24) for k in range(25)]; a.plot([x + 1.2 * math.cos(q) for q in th], [y + 1.2 * math.sin(q) for q in th], color=ORG, lw=1.6)
A = D['a320']; hm, hr = A['height_model_m'], A['height_real_m']; fl = PT['IK_Platform_Work'][2] + (0 if WATER else .73)
a = panel(gs[2, :2], 'At work, from behind: the panel at %.2f m (our model) / %.1f m (real), platform floor %.2f m' % (hm, hr, fl), 'work')
lat = -.35 if WATER else .0; a.plot([-2.2, 2.2], [hm, hm], color=YEL, ls='--', lw=1.2); a.plot([-2.2, 2.2], [hr, hr], color=YEL, ls=':', lw=1.2); a.text(2.1, hm + .04, "aircraft's belly, model", color=YEL); a.text(2.1, hr + .04, "real", color=YEL)
def man(a, x, f):
    a.plot([x, x], [f + .85, f + 1.45], color=GRN, lw=5); a.plot([x - .1, x, x + .1], [f, f + .85, f], color=GRN, lw=4); a.plot([x], [f + 1.62], 'o', color=GRN, ms=16); a.plot([x, x + .20, x + .22], [f + 1.40, f + 1.45, min(hm, f + 2.05) - .08], color=GRN, lw=3)
man(a, lat, fl); dim(a, (2.1, 0), (2.1, fl), '%d' % round(fl * 1000))
a = panel(gs[2, 2:], 'At work, left side', 'works'); man(a, PT['IK_Platform_Work'][0], fl); a.plot([-4, 1], [hm, hm], color=YEL, ls='--', lw=1.2); a.plot([-4, 1], [hr, hr], color=YEL, ls=':', lw=1.2)
a = panel(gs[3, 0], 'At the A320 (metres, nose up; behind the nose wheel)'); a.axis('on'); a.tick_params(colors=DIM); a.set_xlim(-7, 7); a.set_ylim(-34, -16); a.set_aspect('equal')
a.plot([-1.98, -1.98, -1.5, -.75, 0, .75, 1.5, 1.98, 1.98], [-16, -22, -26, -30, -32.5, -30, -26, -22, -16], color=INK); a.plot([-1.0, -6.2, -6.2, -.8], [-27.8, -30.5, -32, -30.5], color=DIM); a.plot([1.0, 6.2, 6.2, .8], [-27.8, -30.5, -32, -30.5], color=DIM)
a.plot([-.3], [-26.2], 'o', color='#4aa3ff', ms=9); a.plot([.8], [-26.2], 'o', color=ORG, ms=9); a.add_patch(Rectangle((-2.2, -32.2), 2.0, 5.8, fc='none', ec='#4aa3ff', lw=3 if WATER else 1)); a.add_patch(Rectangle((.5, -32.2), 2.0, 5.8, fc='none', ec=ORG, lw=1 if WATER else 3))
a.text(-6.8, -33.5, 'water left', color='#4aa3ff'); a.text(3.4, -33.5, 'toilet right', color=ORG); a.add_patch(Rectangle((2.4, -26.1), 4.4, 2.7, fc='none', ec=DIM, ls='--')); a.text(4.6, -24.8, 'catering\nat R2', color=DIM, ha='center')
a = panel(gs[3, 1], 'Parking place (airfield plan metres; north up)'); a.axis('on'); a.tick_params(colors=DIM); a.set_ylim(78, 184); a.set_aspect('equal'); a.axhspan(167, 175, color='#1b2733')
if WATER:
    a.set_xlim(-272, -215); a.add_patch(Rectangle((-272, 70), 72, 12.6, fc='#24303c', ec=INK)); a.plot([-265, -265], [82, 182], color=DIM); a.plot([-245.4, -245.4, -205], [82.6, 166, 166], color='#ff5555', lw=2); a.text(-230, 140, 'stand 9', ha='center')
    for x, y, w, h, t in ((-261.35, 87, 3.5, 8, 'van'), (-262.2, 139.9, 4, 12.2, 'refueller'), (-255.45, 140.5, 3.9, 11, 'catering')): a.add_patch(Rectangle((x, y), w, h, fc='none', ec=DIM)); a.text(x + w / 2, y + h / 2, t, color=DIM, ha='center', rotation=90, fontsize=9)
    hx, hy = -256.8, 117.0
else:
    a.set_xlim(528, 585); a.add_patch(Rectangle((570, 78), 15, 110, fc='#123018')); a.plot([570, 570], [82, 182], color=DIM); a.plot([528, 548.3, 548.3], [166, 166, 82], color='#ff5555', lw=2); a.text(536, 130, 'stand 16', ha='center'); hx, hy = 554.6, 110.0
a.add_patch(Rectangle((hx - 1.4, hy - 3.5), 2.8, 7, fc='none', ec=YEL, lw=2)); a.add_patch(Rectangle((hx - 1, hy - 2.9), 2, 5.8, fc=YEL)); a.text(hx, hy - 12, 'middle (%.1f, %.1f)\nfacing north' % (hx, hy), color=YEL, ha='center')
th = [math.radians(-180 * k / 20) for k in range(21)]; a.plot([hx + 9.6, hx + 9.6] + [hx + 4.8 + 4.8 * math.cos(q) for q in th] + [hx, hx], [hy + 49, hy + 15] + [hy + 15 + 4.8 * math.sin(q) for q in th] + [hy + 19, hy], color=GRN, lw=2); a.text(hx + 10, hy + 40, 'in', color=GRN)
a = panel(gs[3, 2], 'How it works')
H = D['hose'] if WATER else D['rinse_hose']
lines(a, ['Own chassis, 2 axles, electric, two seats; front wheels steer 38 deg, twin rear wheels.', 'Tank 2500 l stainless; A320 takes 200 l.', 'Hose %.1f m, 28 mm, on a reel (14 turns), twist-ring coupling 3/4 in in a capped cup.' % H['metres'], 'Pump panel: litres, bar, tank, pump / stop / lamp, stop button, valve lever, gloves.', 'Fill point behind the left side door (110 deg).', 'Fixed platform 1000, two steps on the left, rails 2000, sprung gate (80 deg in).', 'Panel reach: 1.6 - 1.95 m above the platform.', 'Never shares a side, a hose or a parking place with the toilet truck.'] if WATER else
      ['Same chassis as the water truck; waste tank 1500 l, blue rinse tank 550 l; A320: 177 l waste.', 'Waste hose %.1f m, 4 in (110 mm), round its rack on the tank; cam coupling with two handles,' % D['waste_hose']['metres'], 'cap, cradle, hinged splash tray; waste valve lever; emptying valve behind.', 'Rinse hose %.1f m, 32 mm, on its own reel (15 turns), 1 in coupling.' % H['metres'], 'Platform 1160 x 860 on a mast: floor 620 down, 1570 up (%d used at our A320);' % round(fl * 1000), 'lever and stop on its rail, second lever at the mast; gate 80 deg in; one step.', 'Signs: GLOVES - VISOR - APRON; KEEP AWAY FROM DRINKING WATER.', 'Never shares a side, a hose or a parking place with the water truck.'])
a = panel(gs[3, 3], 'Built from')
lines(a, ['Photos looked at (Wikimedia Commons, shape only, nothing copied):', '"Lavatory service vehicle at Cologne Bonn Airport" (low body behind a cab,', 'hose looped at the back);', '"Lavatory Service tanker at Belfast International Airport" (small truck, rear gear);', '"Aircraft lavatory service" (worker on a railed platform at the panel);', '"Fleet service 130311-F-VV898-013" (4 in hose, coupling, small hose beside it).', 'No free photo of a small water truck was found: its body follows the same trucks', 'and published makers\' figures (7.3 x 2.2 x 2.4 m, 3 m3; 6.1 m, 1 m3).', 'Panels: Airbus AC A320 5-4-9 / 5-4-10: both 31.3 m behind the nose; water 0.3 m left,', '2.6 m up, 3/4 in; waste 0.8 m right, 2.8 m up, 4 in and 1 in (ISO 17775).', 'Our A320 (Art/A320/A320.glb): 26.2 m behind the nose wheel, belly 2.93 / 3.13 m.'])
fig.suptitle('POTABLE WATER TRUCK   (to-do 212)' if WATER else 'TOILET TRUCK   (to-do 212)', color=INK, fontsize=32); fig.tight_layout(rect=(0, 0, 1, .97))
for e in ('png', 'pdf'): fig.savefig(os.path.join(OUT, NAME + '.' + e), facecolor=BG, dpi=70)
print('blueprint', OUT)
