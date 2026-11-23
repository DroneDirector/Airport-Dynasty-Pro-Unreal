# Blueprint of the cabin cleaning van and its kit (to-do 212): docs/Blueprints/Apron/CleaningVan.png and .pdf
# The views are the real model (Art/CleaningVan/CleaningVan.blend, flat orthographic), the dimensions measured on it.
#   python Tools/make_cleaningvan_blueprint.py
import json, math, os, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BL = r'C:/Program Files/Blender Foundation/Blender 3.3/blender.exe'
VIEWS = {  # name: (camera at, look, ortho width m, clip start, size px)
    'left': ((0, 20, 1.35), (0, 0, 1.35), 7.0, .1, (1800, 800)), 'front': ((20, 0, 1.35), (0, 0, 1.35), 3.1, .1, (900, 800)), 'rear': ((-20, 0, 1.35), (0, 0, 1.35), 3.1, .1, (900, 800)),
    'top': ((0, 0, 20), (0, 0, 0), 7.0, .1, (1800, 950)), 'secside': ((0, -20, 1.35), (0, 0, 1.35), 7.0, 19.95, (1800, 800)), 'secplan': ((0, 0, 20), (0, 0, 0), 7.0, 18.55, (1800, 700))}
if '--blender' in sys.argv:
    import bpy
    from mathutils import Vector
    out = sys.argv[-1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading
    sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.show_object_outline = True; sh.object_outline_color = (1, 1, 1); sh.show_specular_highlight = False
    w = bpy.data.worlds.new('K'); w.use_nodes = False; w.color = (0, 0, 0); sc.world = w; sc.display.render_aa = '16'; sc.view_settings.view_transform = 'Standard'
    for m in bpy.data.materials:
        b = m.node_tree.nodes.get('Principled BSDF') if m.use_nodes else None
        c = b.inputs['Base Color'].default_value if b else (.3, .3, .3, 1)
        m.diffuse_color = (*[max(v, .05) for v in c[:3]], .25 if 'Glass' in m.name and 'Mirror' not in m.name else 1)
        if any(k in m.name for k in ('Tyre', 'Plastic', 'DarkSteel', 'Seal', 'FloorMat')): m.diffuse_color = (.10, .11, .12, 1)
        if 'Steel' in m.name and 'Dark' not in m.name: m.diffuse_color = (.42, .43, .45, 1)
    cd = bpy.data.cameras.new('C'); cam = bpy.data.objects.new('C', cd); sc.collection.objects.link(cam); sc.camera = cam; cd.type = 'ORTHO'
    for n, (loc, at, wd, cs, res) in VIEWS.items():
        cam.location = loc; d = Vector(at) - Vector(loc)
        cam.rotation_euler = (0, 0, 0) if abs(d.z) > 1 else d.to_track_quat('-Z', 'Y').to_euler()
        cd.ortho_scale = wd; cd.clip_start = cs; cd.clip_end = 60; sc.render.resolution_x, sc.render.resolution_y = res
        for o in bpy.data.objects: o.hide_render = n == 'secside' and o.name.startswith(('SlideDoor', 'DoorPassenger'))
        sc.render.filepath = os.path.join(out, n + '.png'); bpy.ops.render.render(write_still=True)
    sys.exit(0)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
tmp = tempfile.mkdtemp()
subprocess.run([BL, '-b', os.path.join(ROOT, 'Art', 'CleaningVan', 'CleaningVan.blend'), '--python', os.path.abspath(__file__), '--', '--blender', tmp], capture_output=True, timeout=200)
OUT = os.path.join(ROOT, 'docs', 'Blueprints', 'Apron'); os.makedirs(OUT, exist_ok=True)
D = json.load(open(os.path.join(ROOT, 'Art', 'CleaningVan', 'cleaningvan-parts.json'))); KS = json.load(open(os.path.join(ROOT, 'Art', 'CleaningKit', 'sizes.json')))
PT = {k: (v[0] / 100, -v[1] / 100, v[2] / 100) for k, v in D['points_cm'].items()}
BG, INK, DIM, YEL, ORG, GRN = '#000000', '#e8edf2', '#9fb4c8', '#f2c230', '#ff7a2a', '#55dd88'
plt.rcParams.update({'font.size': 11, 'text.color': INK, 'font.family': 'DejaVu Sans'})
fig = plt.figure(figsize=(30, 25), facecolor=BG); gs = fig.add_gridspec(4, 3, width_ratios=[2.25, 1, 1], height_ratios=[1.05, 1.1, 1.05, .95])


def panel(cell, t, img=None, flip=False, mid=1.35):
    a = fig.add_subplot(cell); a.set_facecolor(BG); a.set_title(t, color=INK, fontsize=17); a.axis('off')
    if img:
        w = VIEWS[img][2]; rx, ry = VIEWS[img][4]; h = w * ry / rx; im = plt.imread(os.path.join(tmp, img + '.png'))
        a.imshow(im, extent=(w / 2, -w / 2, mid - h / 2, mid + h / 2) if flip else (-w / 2, w / 2, mid - h / 2, mid + h / 2)); a.set_aspect('equal')
        a.set_xlim((w / 2, -w / 2) if flip else (-w / 2, w / 2)); a.set_ylim(mid - h / 2 - .22, mid + h / 2 + .12)
    return a


def dim(a, p, q, t, c=DIM):
    a.annotate('', p, q, arrowprops=dict(arrowstyle='<->', color=c, lw=1.4)); a.text((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, t, color=c, ha='center', va='center', fontsize=12, bbox=dict(fc=BG, ec='none', pad=1.5), rotation=90 if abs(p[0] - q[0]) < .01 else 0)


def mark(a, x, y, t, c=ORG, dx=.10, dy=.10):
    a.plot([x], [y], 'o', color=c, ms=5); a.annotate(t, (x, y), (x + dx, y + dy), color=c, fontsize=10, arrowprops=dict(arrowstyle='-', color=c, lw=.8))


a = panel(gs[0, 0], 'Left side (mm)', 'left', True)
dim(a, (-2.999, -.12), (2.999, -.12), '5998'); dim(a, (2.051, .06), (-1.984, .06), 'wheelbase 4035'); dim(a, (3.2, 0), (3.2, 2.522), '2522'); dim(a, (2.999, 2.8), (2.051, 2.8), '948'); dim(a, (-1.984, 2.8), (-2.999, 2.8), '1015'); dim(a, (-3.2, 0), (-3.2, .575), 'floor 575')
a.text(-1.84, 1.66, 'airport name, written at run time\n1560 x 400', color='#000', ha='center', va='center', fontsize=10)
a = panel(gs[0, 1], 'Front', 'front'); dim(a, (-1.025, -.12), (1.025, -.12), '2050'); dim(a, (-.905, .12), (.905, .12), 'track 1810'); dim(a, (-1.28, 2.75), (1.28, 2.75), '2560 over the mirrors')
a = panel(gs[0, 2], 'Rear', 'rear'); dim(a, (-.781, 2.66), (.781, 2.66), 'opening 1562'); dim(a, (1.3, .54), (1.3, 2.33), '1790'); dim(a, (-.895, .12), (.895, .12), 'track 1790')
a = panel(gs[1, 0], 'From above: door sweeps (orange), places to stand (green); the slide door goes 125 out, 1300 back', 'top', mid=0)
for p, ang0, r, sg, n in (('PIV_DoorDriver', 180, 1.24, -1, 68), ('PIV_DoorPassenger', 180, 1.24, 1, 68), ('PIV_RearDoorL', -90, .82, -1, 100), ('PIV_RearDoorR', 90, .82, 1, 100)):
    x, y, _ = PT[p]; th = [math.radians(ang0 + sg * k * n / 24) for k in range(25)]
    a.plot([x + r * math.cos(q) for q in th], [y + r * math.sin(q) for q in th], color=ORG, lw=1.6); a.plot([x, x + r * math.cos(th[-1])], [y, y + r * math.sin(th[-1])], color=ORG, lw=2.4); mark(a, x, y, '%s %d deg' % (p[4:], n), ORG, .10, .34 if y > 0 else -.38)
a.annotate('', (-1.55, -1.16), (-.25, -1.16), arrowprops=dict(arrowstyle='->', color=ORG, lw=2.4))
for k, v in PT.items():
    if k.startswith('IK_Stand'): a.plot([v[0]], [v[1]], 's', color=GRN, ms=9)
a = panel(gs[1, 1:], 'Parking place (airfield plan metres; north up)'); a.axis('on'); a.tick_params(colors=DIM); a.set_xlim(-272, -200); a.set_ylim(78, 180); a.set_aspect('equal')
a.add_patch(Rectangle((-272, 70), 72, 12.6, fc='#24303c', ec=INK)); a.text(-236, 80, 'terminal front wall 82.6', ha='center', fontsize=11)
a.axhspan(167, 175, color='#1b2733'); a.text(-232, 171, 'service road behind the stands', ha='center', va='center', fontsize=11)
a.plot([-245.4, -245.4, -205], [82.6, 166, 166], color='#ff5555', lw=2); a.text(-225.4, 140, 'stand 9 (A321)', ha='center', fontsize=12)
a.add_patch(Rectangle((-240.4, 87), 4, 12, fc='none', ec=INK)); a.text(-238.4, 100, 'tug', ha='center', fontsize=10)
a.add_patch(Rectangle((-254.75, 87), 3.5, 8, fc='none', ec=YEL, lw=2)); a.add_patch(Rectangle((-254.02, 88), 2.05, 6, fc=YEL)); a.text(-249, 108, 'cleaning van, middle\n(-253.0, 91.0),\nfacing north', color=YEL, fontsize=11); a.annotate('', (-253, 168), (-253, 97), arrowprops=dict(arrowstyle='->', color=YEL, ls='--'))
a.plot([-265, -265], [82, 180], color=DIM, ls=':'); a.text(-267.5, 130, 'apron edge x -265', rotation=90, color=DIM, fontsize=10, va='center')
a = panel(gs[2, 0], 'Section: cab, crew seats, bulkhead, load space (seen from the right)', 'secside')
for k, t, dx, dy in (('IK_Hand_Wheel_L', 'wheel', .25, .45), ('IK_Foot_Accel', 'pedals', .50, -.30), ('IK_Seat_SeatDriver', 'driver', -.10, -.40), ('IK_Seat_SeatRow1', 'row of 3', .30, -.35), ('IK_Beacon', 'beacon', .3, .1)): mark(a, PT[k][0], PT[k][2], t, ORG, dx, dy)
dim(a, (-.93, .30), (-2.93, .30), 'load space 2000'); dim(a, (-2.35, .60), (-2.35, 2.47), '1890'); dim(a, (-.885, 2.75), (.375, 2.75), 'side door 1260'); dim(a, (-3.2, .86), (-3.2, 2.0), 'shelves 860 .. 2000')
a = panel(gs[2, 1:], 'Plan cut at 1450 (front to the right)', 'secplan', mid=0)
a = panel(gs[3, 0], 'The kit: one mesh each, origin = where the hand holds it (mm)'); a.set_xlim(0, 10); a.set_ylim(0, 6); a.set_aspect('auto')
HOW = (('BagFull', 'full bag, tied', 'neck in the fist'), ('BagEmpty', 'open bag', 'rim in the fist'), ('BagRoll', 'roll of bags', 'on its base'), ('Caddy', 'caddy: 3 sprays, 7 cloths', 'hand round the 30 x 26 grip'), ('Vacuum', 'cordless vacuum', 'grip; 2nd hand on the wand, 200 ahead, 170 below'), ('CleanCart', 'rubbish cart with its sack', 'on the ground; both hands on the bar, 1000 high'))
for i, (n, t, how) in enumerate(HOW):
    s = KS['items'][n]; a.text(.1, 5.5 - .62 * i, t, fontsize=14, color=YEL); a.text(3.2, 5.5 - .62 * i, ' x '.join('%d' % round(1000 * (q - p)) for p, q in zip(s['min'], s['max'])), fontsize=14); a.text(5.2, 5.5 - .62 * i, how, fontsize=12, color=DIM)
a.text(.1, 1.45, 'In the van (Stock_<item>_<n>, the same meshes): 3 caddies, 6 rolls, 2 full bags in the cage, the cart, 2 vacuums on their dock', fontsize=12.5); a.text(.1, .9, 'Right: cage for full bags (1020 x 465), lashing rail, the cart by the rear door; left: four shelves, tubs with blankets, covers, cloths', fontsize=12.5)
a = panel(gs[3, 1:], 'Built from'); a.set_xlim(0, 1); a.set_ylim(0, 1); a.set_aspect('auto')
TX = ['Published sizes of the European L3H2 panel van class: 5998 x 2050 x 2522, wheelbase 4035,', 'side door 1.25 x 1.75, rear opening 1562 x 1790, 1422 between the arches, tyres 225/75 R16:', 'vanreviewer.co.uk (peugeot/boxer and fiat/ducato, dimensions)', 'Photos looked at (Wikimedia Commons): "2015 Renault Master LM35 ... White, 11-2024, Front",', '"Fiat Ducato Kastenwagen L2 (III, Facelift), h 16082025", "Renault Master Z.E., IAA 2018",', '"Fiat Ducato centre console". Own outline, lamps, grille; no brand or logo; nothing traced.', 'Electric: no exhaust, charge flap in the nose. 1 + 1 seats in front, 3 behind, slide door right.', 'Marking: amber beacon, red-white blocks on both sides, red-yellow chevrons, reflectors.', 'Body and band = livery paints the game recolours; the name on 3 white panels at run time.', '%d parts after joining, %d triangles, %d pivots and points; flicker: %s' % (len(D['parts']), D['tris'], len(D['points_cm']), D['flicker'].strip()[:40])]
for i, t in enumerate(TX): a.text(.02, .95 - i * .095, t, fontsize=13, color=DIM if i == 2 else INK)
fig.suptitle('AIRCRAFT CABIN CLEANING VAN   (to-do 212)', color=INK, fontsize=30); fig.tight_layout(rect=(0, 0, 1, .97))
for e in ('png', 'pdf'): fig.savefig(os.path.join(OUT, 'CleaningVan.' + e), facecolor=BG, dpi=80)
print('blueprint', OUT)
