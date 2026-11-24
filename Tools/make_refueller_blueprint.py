# Blueprint of the aircraft refueller (to-do 212): docs/Blueprints/Apron/Refueller.png and .pdf
# The views are the real model (Art/Refueller/Refueller.blend, flat orthographic), the dimensions measured on it.
#   python Tools/make_refueller_blueprint.py
import json, math, os, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BL = r'C:/Program Files/Blender Foundation/Blender 3.3/blender.exe'
VIEWS = {'left': ((0, 30, 1.55), (0, 0, 1.55), 11.2, .1, (2240, 760)), 'front': ((30, 0, 1.55), (0, 0, 1.55), 4.0, .1, (900, 855)), 'rear': ((-30, 0, 1.55), (0, 0, 1.55), 4.0, .1, (900, 855)),
         'top': ((0, 0, 30), (0, 0, 0), 11.2, .1, (2240, 860)), 'right': ((0, -30, 1.55), (0, 0, 1.55), 11.2, .1, (2240, 760)), 'up': ((0, 30, 1.95), (0, 0, 1.95), 5.6, .1, (1120, 900)),
         'panel': ((2.545, 30, 1.75), (2.545, 0, 1.75), 1.5, 28.83, (760, 1150)), 'cab': ((0, 30, 1.95), (4.1, 0, 1.95), 2.6, 29.2, (900, 800))}
if '--blender' in sys.argv:
    import bpy
    from mathutils import Vector, Matrix
    out = sys.argv[-1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading
    sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.show_object_outline = True; sh.object_outline_color = (1, 1, 1); sh.show_specular_highlight = False
    w = bpy.data.worlds.new('K'); w.use_nodes = False; w.color = (0, 0, 0); sc.world = w; sc.display.render_aa = '16'; sc.view_settings.view_transform = 'Standard'
    for m in bpy.data.materials:
        b = m.node_tree.nodes.get('Principled BSDF') if m.use_nodes else None
        c = b.inputs['Base Color'].default_value if b else (.3, .3, .3, 1); m.diffuse_color = (*[max(v, .05) for v in c[:3]], .25 if 'Glass' in m.name and 'Mirror' not in m.name else 1)
        if any(k in m.name for k in ('Tyre', 'BumperPlastic', 'BlackPlastic', 'DashPlastic', 'DarkSteel', 'Seal', 'FloorMat', 'Hose')): m.diffuse_color = (.10, .11, .12, 1)
        if any(k in m.name for k in ('_Steel', 'RimSteel', 'Aluminium', 'Diamond')): m.diffuse_color = (.42, .43, .45, 1)
    cd = bpy.data.cameras.new('C'); cam = bpy.data.objects.new('C', cd); sc.collection.objects.link(cam); sc.camera = cam; cd.type = 'ORTHO'
    J = json.load(open(os.path.join(ROOT, 'Art', 'Refueller', 'refueller-parts.json')))['scissor']; SL, H0, LF = J['arm_cm'] / 100, J['h0_cm'] / 100, J['lift_cm'] / 100; x0, z0 = [v / 100 for v in J['base_cm']]; t0 = math.radians(J['deg0']); t1 = math.asin((H0 + LF) / (2 * SL))
    for n, (loc, at, wd, cs, res) in VIEWS.items():
        for o in bpy.data.objects:
            o.hide_render = (n == 'panel' and o.name.startswith('PanelDoorL')) or (n == 'cab' and o.name.startswith(('DoorDriver', 'Seats'))) or o.type == 'EMPTY'
        if n == 'up':                                                # the platform at the top, as the game moves it
            for o in bpy.data.objects:
                if o.name.startswith(('Platform', 'GatePlatform', 'LeverPlatform')): o.location.z += LF
                elif o.name.startswith('Scissor') and o.name[7] in 'AB' and o.name[8] in '12':
                    st = int(o.name[8]) - 1; c0 = Vector((x0 + SL * math.cos(t0) / 2, 0, z0 + (st + .5) * SL * math.sin(t0))); c1 = Vector((x0 + SL * math.cos(t1) / 2, 0, z0 + (st + .5) * SL * math.sin(t1)))
                    a = (t1 - t0) * (-1 if o.name[7] == 'A' else 1); o.matrix_world = Matrix.Translation(c1) @ Matrix.Rotation(a, 4, 'Y') @ Matrix.Translation(-c0)
        cam.location = loc; d = Vector(at) - Vector(loc)
        if n == 'cab': cam.location = (at[0], 30, at[2]); d = Vector((0, -1, 0))
        if n == 'up': cam.location = (-3.0, 30, 1.95); d = Vector((0, -1, 0))
        cam.rotation_euler = (0, 0, 0) if abs(d.z) > 1 else d.to_track_quat('-Z', 'Y').to_euler(); cd.ortho_scale = wd * (max(res) / res[0]); cd.clip_start = cs; cd.clip_end = 70; sc.render.resolution_x, sc.render.resolution_y = res
        sc.render.filepath = os.path.join(out, n + '.png'); bpy.ops.render.render(write_still=True)
    sys.exit(0)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
tmp = tempfile.mkdtemp(); OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Apron')
subprocess.run([BL, '-b', os.path.join(ROOT, 'Art', 'Refueller', 'Refueller.blend'), '--python', os.path.abspath(__file__), '--', '--blender', tmp], capture_output=True, timeout=200)
D = json.load(open(os.path.join(ROOT, 'Art', 'Refueller', 'refueller-parts.json'))); PT = {k: (v[0] / 100, -v[1] / 100, v[2] / 100) for k, v in D['points_cm'].items()}
BG, INK, DIM, YEL, ORG, GRN = '#000000', '#e8edf2', '#9fb4c8', '#f2c230', '#ff7a2a', '#55dd88'
plt.rcParams.update({'font.size': 11, 'text.color': INK, 'font.family': 'DejaVu Sans'})
fig = plt.figure(figsize=(34, 30), facecolor=BG); gs = fig.add_gridspec(4, 4, width_ratios=[1.45, 1.45, 1, 1], height_ratios=[1, 1.05, 1.15, 1.15])
def panel(cell, t, img=None, flip=False, mid=1.55, cx=0.0):
    a = fig.add_subplot(cell); a.set_facecolor(BG); a.set_title(t, color=INK, fontsize=17); a.axis('off')
    if img:
        w = VIEWS[img][2]; rx, ry = VIEWS[img][4]; h = w * ry / rx; im = plt.imread(os.path.join(tmp, img + '.png')); lo, hi = cx - w / 2, cx + w / 2
        a.imshow(im, extent=(hi, lo, mid - h / 2, mid + h / 2) if flip else (lo, hi, mid - h / 2, mid + h / 2)); a.set_aspect('equal'); a.set_xlim((hi, lo) if flip else (lo, hi)); a.set_ylim(mid - h / 2 - .1, mid + h / 2 + .1)
    return a
def dim(a, p, q, t, c=DIM):
    a.annotate('', p, q, arrowprops=dict(arrowstyle='<->', color=c, lw=1.4)); a.text((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, t, color=c, ha='center', va='center', fontsize=12, bbox=dict(fc=BG, ec='none', pad=1.5), rotation=90 if abs(p[0] - q[0]) < 1e-6 else 0)
def mark(a, x, y, t, c=ORG, dx=.2, dy=.2): a.plot([x], [y], 'o', color=c, ms=5); a.annotate(t, (x, y), (x + dx, y + dy), color=c, fontsize=11, arrowprops=dict(arrowstyle='-', color=c, lw=.8))
a = panel(gs[0, :2], 'Left side (mm)', 'left', True)
dim(a, (-5.11, -.22), (5.09, -.22), '10198 overall'); dim(a, (3.70, .0), (-1.20, .0), 'wheelbase 4900'); dim(a, (-1.20, .0), (-2.55, .0), '1350'); dim(a, (5.3, 0), (5.3, 3.08), '3080'); dim(a, (2.0, 3.25), (-3.4, 3.25), 'tank 5400 x 2430 x 1800 = 20 m3'); dim(a, (2.97, 3.25), (2.12, 3.25), 'meters 850'); dim(a, (-5.25, 0), (-5.25, 1.45), 'deck 1450')
for k, t, dx, dy in (('IK_Stand_Bond', 'bonding reel + clamp', .1, -.52), ('PIV_PanelDoorL', 'panel door 110 deg', -.25, .80), ('IK_Extinguisher_L', 'extinguisher (each side)', .1, -.95), ('IK_EmergencyL', 'fuel shut-off (each side)', -.25, -.10)):
    x, y, z = PT[k]; mark(a, x, .60 if 'Bond' in k else z, t, ORG, dx, dy)
a = panel(gs[0, 2], 'Front', 'front'); dim(a, (-1.225, -.22), (1.225, -.22), '2450'); dim(a, (-1.5, 3.3), (1.5, 3.3), '3000 over the mirrors'); dim(a, (-1.025, .1), (1.025, .1), 'track 2050')
a = panel(gs[0, 3], 'Rear', 'rear'); dim(a, (-1.17, 2.75), (1.17, 2.75), 'platform 2340'); dim(a, (1.75, 1.45), (1.75, 2.55), 'rail 1100'); dim(a, (-.31, .35), (.31, .35), 'steps 620')
a = panel(gs[1, :2], 'From above: door sweeps (orange), places to stand (green); steered front and tag axles', 'top', mid=0)
for p, ang0, r, sg, n in (('PIV_DoorDriver', 180, 1.17, -1, 68), ('PIV_DoorPassenger', 180, 1.17, 1, 68), ('PIV_PanelDoorL', 0, .78, 1, 110), ('PIV_PanelDoorR', 0, .78, -1, 110), ('PIV_GatePlatform', -90, .68, 1, 90)):
    x, y, _ = PT[p]; th = [math.radians(ang0 + sg * k * n / 24) for k in range(25)]; a.plot([x + r * math.cos(q) for q in th], [y + r * math.sin(q) for q in th], color=ORG, lw=1.6); a.plot([x, x + r * math.cos(th[-1])], [y, y + r * math.sin(th[-1])], color=ORG, lw=2.4)
for k, v in PT.items():
    if k.startswith(('IK_Stand', 'IK_ChockPlace', 'IK_PanelStand')): a.plot([v[0]], [v[1]], 's', color=GRN, ms=8)
a = panel(gs[1, 2:], 'Right side: locker, chocks, return line', 'right')
mark(a, PT['IK_Chock_0'][0], .6, 'two own wheel chocks', ORG, .3, -.45); mark(a, 1.0, .74, 'side lockers', ORG, -.2, -.6)
a = panel(gs[2, 0], 'Platform raised: 2 scissor stages, arms 1050, 8.5 to 35.2 deg', 'up', True, 1.95, -3.0); dim(a, (-4.6, 1.45), (-4.6, 2.35), 'lift 900'); dim(a, (-5.35, 0), (-5.35, 2.35), 'floor 2350')
a.axhline(3.4, color=YEL, ls='--', lw=1.2); a.text(-1.4, 3.47, 'A320 wing coupling about 3.2 - 3.5 m (taken from the job, B measures it on the aircraft)', color=YEL, fontsize=10, ha='center')
a = panel(gs[2, 1], 'Meter cabinet, door open: gauges, meter, printer, buttons, levers, dead man, sampler', 'panel', True, 1.75, 2.545)
for k, t in (('IK_Lever_Pump', 'pump'), ('IK_Lever_HoseL', 'hose 1'), ('IK_Lever_HoseR', 'hose 2'), ('IK_Lever_Defuel', 'defuel'), ('IK_Button_EStop', 'stop'), ('IK_Meter_Face', 'litres, run time'), ('IK_Deadman', 'dead man'), ('IK_Ticket', 'ticket')): a.plot([PT[k][0]], [PT[k][2]], 'o', color=ORG, ms=5); a.text(PT[k][0], PT[k][2] + .03, t, color=ORG, fontsize=9, ha='center')
a = panel(gs[2, 2], 'Cab, cut at the driver\'s door (2 seats)', 'cab', True, 1.95, 4.1)
for k, t in (('IK_Hand_Wheel_L', 'wheel'), ('IK_Foot_Accel', 'pedals'), ('IK_Seat_SeatDriver', 'seat'), ('IK_GearSelect', 'selector'), ('IK_Driver_Eye', 'eye'), ('IK_Step_DoorDriver_Low', 'step 490'), ('IK_Step_DoorDriver', 'step 830')): mark(a, PT[k][0], PT[k][2], t, ORG, .12, .10)
a = panel(gs[2, 3], 'Parking place (airfield plan metres; north up)'); a.axis('on'); a.tick_params(colors=DIM); a.set_xlim(-272, -215); a.set_ylim(78, 182); a.set_aspect('equal')
a.add_patch(Rectangle((-272, 70), 72, 12.6, fc='#24303c', ec=INK)); a.text(-243, 79.5, 'terminal front wall 82.6', ha='center', fontsize=11); a.axhspan(167, 175, color='#1b2733'); a.text(-236, 171, 'service road', ha='center', va='center', fontsize=11)
a.plot([-245.4, -245.4, -205], [82.6, 166, 166], color='#ff5555', lw=2); a.text(-230, 140, 'stand 9', ha='center', fontsize=12); a.add_patch(Rectangle((-261.35, 87), 3.5, 8, fc='none', ec=DIM)); a.text(-252, 91, 'cleaning van', ha='center', fontsize=10)
a.add_patch(Rectangle((-263.75, 121.9), 3.9, 12.2, fc='none', ec=YEL, lw=2)); a.add_patch(Rectangle((-263.03, 122.9), 2.45, 10.2, fc=YEL)); a.text(-251, 112, 'refueller, middle\n(-261.8, 128.0),\nfacing north', color=YEL, fontsize=11, ha='center'); a.plot([-248.6] * 2 + [-255.2 + 6.6 * math.cos(math.radians(k)) for k in range(0, 181, 10)] + [-261.8] * 2, [172, 143] + [143 - 6.6 * math.sin(math.radians(k)) for k in range(0, 181, 10)] + [147, 134], color=ORG, lw=1.4); a.plot([-265, -265], [82, 182], color=DIM, ls=':'); a.text(-268, 130, 'apron edge x -265', rotation=90, color=DIM, fontsize=10, va='center')
a = panel(gs[3, :2], 'Hoses and fittings'); a.set_xlim(0, 1); a.set_ylim(0, 1); a.set_aspect('auto')
H = D['hose']
for i, t in enumerate(('Two reels, 850 across, behind the tag axle under the platform; each %.1f m of 2-inch hose (64 outside) in 15 turns, 3 layers' % H['total_m'], 'Turns are single parts HoseTurn_L/R_00 .. 14 (inside layer first): 14 goes first as the hose pays out (%s m a turn by layer)' % ', '.join('%.2f' % H['turn_m'][k] for k in (0, 5, 10)), 'Way to the coupling: guide 4740 behind, 800 up; 2.4 m up to the deck, 1 to 4 m along, 2 m up = 6 to 9 m of hose: plenty', 'Nozzles: mouth up in the red cup of their holder, two handles, flow lever, swivel; lengths 265; underwing 2.5-inch bayonet form, 3 lugs', 'Bonding: reel 330 across, 16 turns of green-yellow cable, clamp on a brass stud; guide eye IK_BondOut', 'Tank 20 m3, 2 manholes, vents, spill coaming, top walk and rail; product line left, return line right; meter 2 x flanged, sampler', 'Platform 1410 x 2340, rails 1100 with toe boards, gate opens inwards, up / down lever and stop on the left rail; rail top 2550 lowered, 3450 raised, 250 kg', 'Fixed steps at the back: bumper 615, 820, 1140, deck 1450; stand-off hand rails; own grab posts on the platform', 'Signs in our own words: JET A-1, NO SMOKING, NO NAKED LIGHTS, 20 000 L, class 3 diamond, 30 / 1863, MAX 250 kg, BONDING', 'Airport name written at run time on NamePanel_L / _R (2400 x 420), NamePanel_Front (1240 x 85), NamePanel_Rear (600 x 130)')): a.text(.01, .94 - i * .098, t, fontsize=13)
a = panel(gs[3, 2:], 'Built from'); a.set_xlim(0, 1); a.set_ylim(0, 1); a.set_aspect('auto')
for i, t in enumerate(('Published sizes of 20 000 l rigid refuellers: 10 200 x 2 500 x 3 250', '(chinatanktruck.com, 20000 l aircraft refuelling truck) and 10 525 x 2 450 x 3 650, rail stowed', '(flofuel-sp.co.uk, 20000 l refueller); rigid 6x2, 20 m hose reels, underwing line, deck hoses', '(pdf.aeroexpo.online, 20 000 l aircraft refueller). Tyres 315/70 R22.5, dia 1.014.', 'Photos looked at (Wikimedia Commons), nothing copied:', '"Shell tanker truck at Helsinki-Vantaa Airport ... March 2025" - cab, cabinet behind it, oval tank,', '   pipe along the side, platform at the back, hazard diamond', '"Refuel EC-KNM Iberia (6218464950)" - rear platform, steps, panel, small white bonding reel, hose to the wing', '"Jet a1 truck refueling dsc04316" - platform rails, two hoses up to the coupling, signs', 'No brand, no logo; the shape is our own.', 'No fuel farm or depot exists in any plan: the parking place is a choice (see the plan).')): a.text(.02, .94 - i * .088, t, fontsize=13, color=INK)
fig.suptitle('AIRCRAFT REFUELLER, 20 m3   (to-do 212)', color=INK, fontsize=32); fig.tight_layout(rect=(0, 0, 1, .97))
for e in ('png', 'pdf'): fig.savefig(os.path.join(OUT, 'Refueller.' + e), facecolor=BG, dpi=75)
print('blueprint', OUT)
