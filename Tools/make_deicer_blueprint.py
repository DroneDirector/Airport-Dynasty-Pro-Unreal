# The de-icer's sheet and its reach over an A320.  blender -b Art/Deicer/Deicer.blend --python Tools/make_deicer_blueprint.py -- views   then   python Tools/make_deicer_blueprint.py
# The second half searches, for every sample of wing, tailplane and fin, a boom pose (slew, lift, length) from the stops of the drive path with the nozzle 2-6 m off and cabin, boom and truck clear.
import sys, os, math, json
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); TMP = os.path.join(os.environ['TEMP'], 'deicer_bp'); os.makedirs(TMP, exist_ok=True); J = os.path.join(R, 'Art', 'Deicer', 'deicer-parts.json')
try: import bpy
except ImportError: bpy = None
if bpy:
    from mathutils import Matrix, Vector
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sh = sc.display.shading; sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_object_outline = True; sh.show_cavity = True; sc.render.film_transparent = True; sc.view_settings.view_transform = 'Standard'
    OB = [o for o in sc.objects if o.type == 'MESH']; bpy.context.view_layer.update()
    for m in bpy.data.materials:
        if 'Glass' in m.name: m.diffuse_color = (.35, .55, .65, .45)
    v = [o.matrix_world @ Vector(c) for o in OB for c in o.bound_box]; print('SIZE', [(round(min(p[i] for p in v), 3), round(max(p[i] for p in v), 3)) for i in range(3)])
    cd = bpy.data.cameras.new('C'); cd.type = 'ORTHO'; c = bpy.data.objects.new('C', cd); sc.collection.objects.link(c); sc.camera = c; cd.clip_end = 200
    def shot(n, at, d, w, h, up='Y'): c.location = Vector(at) + Vector(d) * 40; c.rotation_euler = (-Vector(d)).to_track_quat('-Z', up).to_euler(); cd.ortho_scale = max(w, h); sc.render.resolution_x, sc.render.resolution_y = int(w * 110), int(h * 110); sc.render.filepath = os.path.join(TMP, n + '.png'); bpy.ops.render.render(write_still=True)
    shot('side', (0, 0, 1.8), (0, 1, 0), 11, 4.2); shot('front', (0, 0, 1.8), (1, 0, 0), 4, 4.2); shot('rear', (0, 0, 1.8), (-1, 0, 0), 4, 4.2)
    c.rotation_euler = (0, 0, 0); c.location = (0, 0, 40); cd.ortho_scale = 11; sc.render.resolution_x, sc.render.resolution_y = 1210, 420; sc.render.filepath = os.path.join(TMP, 'top.png'); bpy.ops.render.render(write_still=True)
    SX, PX, PZ, TX = 2.15, 2.45, 2.86, -2.90
    def pose(lift, ext):
        L = Matrix.Translation((PX, 0, PZ)) @ Matrix.Rotation(math.radians(lift), 4, 'Y') @ Matrix.Translation((-PX, 0, -PZ)); t = L @ Vector((TX - ext, 0, PZ)) - Vector((TX, 0, PZ))
        for o in OB:
            n = o.name; M = None
            if n.startswith(('ChainLink', 'HoseLift', 'HoseTip', 'LiftB', 'LiftR')): o.hide_render = True
            elif n.startswith('BoomBase'): M = L
            elif n.startswith('BoomSec2'): M = L @ Matrix.Translation((-ext / 2, 0, 0))
            elif n.startswith('BoomSec3'): M = L @ Matrix.Translation((-ext, 0, 0))
            elif n.startswith(('Cabin', 'Joy', 'Spray', 'Noz', 'HoseNoz')): M = Matrix.Translation(t)
            if M: o.matrix_world = M @ o.matrix_world
    pose(55, 6); shot('up', (-1.0, 0, 6.8), (0, 1, 0), 16, 14)
    sys.exit(0)
import numpy as np, textwrap
from PIL import Image, ImageDraw, ImageFont; Image.init()
# the A320 (published sizes), nose at x 0, x aft, y right, z up; truck pose = x, y, heading (degrees from +x, anticlockwise seen from above with y up the sheet)
lerp = lambda a, b, t: np.array(a) + (np.array(b) - np.array(a)) * t; G = {}
G['wing'] = [lerp(lerp((11.9, 1.98, 3.3), (19.4, 16.9, 4.9), s), lerp((18.6, 1.98, 3.3), (20.9, 16.9, 4.9), s), c) for s in np.linspace(0, 1, 13) for c in np.linspace(0, 1, 5)] + [np.array((20.6 + .4 * k, 17.4 + .2 * k, 5.2 + 1.0 * k)) for k in (0, 1, 2)]
G['tailplane'] = [lerp(lerp((31.2, 1.0, 5.2), (34.9, 6.2, 5.9), s), lerp((34.6, 1.0, 5.2), (36.4, 6.2, 5.9), s), c) for s in np.linspace(0, 1, 6) for c in np.linspace(0, 1, 4)]
G['fin'] = [lerp(lerp((27.9, .15, 5.9), (34.2, .1, 11.76), s), lerp((35.0, .15, 5.9), (36.3, .1, 11.76), s), c) for s in np.linspace(0, 1, 8) for c in np.linspace(0, 1, 4)]
G['fuselage top'] = [np.array((x, 1.2, 5.45)) for x in np.arange(4, 30, 2)]
OBS = np.array([p for g in ('wing', 'tailplane', 'fin') for p in G[g]] + [np.array((x, 1.975 * math.sin(a), 3.9 + 1.975 * math.cos(a))) for x in np.arange(2, 33, 1) for a in np.linspace(0, math.pi, 7)] + [np.array((x, 5.75 + 1.1 * math.cos(a), 1.75 + 1.1 * math.sin(a))) for x in np.arange(10.4, 14.5, .6) for a in np.linspace(0, 6.28, 8)])
PATH = [(3.2, 6.8, 0), (9.0, 11.0, 45), (13.2, 17.2, 62), (19.5, 21.6, 20), (25.0, 17.0, -62), (24.3, 10.5, -60), (29.0, 9.6, -30), (33.5, 10.0, 0), (38.5, 8.2, -40)]
def fk(st, sl, li, ex):
    x, y, h = st; h = math.radians(h); L = 5.35 + ex; a = math.radians(sl); l = math.radians(li); r = lambda f: np.array((-math.cos(a) * f, -math.sin(a) * f)); P = r(-.30); T = r(-.30 + L * math.cos(l)); N = r(-.30 + L * math.cos(l) + 2.048); C0 = r(-.30 + L * math.cos(l) + 1.0)
    W = lambda p, z: np.array((x + math.cos(h) * (2.15 + p[0]) - math.sin(h) * p[1], y + math.sin(h) * (2.15 + p[0]) + math.cos(h) * p[1], z)); tz = 2.86 + L * math.sin(l); return W(P, 2.86), W(T, tz), W(C0, tz - .87), W(N, tz - 1.69)
def ok(p, d): return np.min(np.linalg.norm(OBS - p, axis=1)) > d
CAND = []
for si, st in enumerate(PATH):
    h = math.radians(st[2]); low = OBS[OBS[:, 2] < 4.0]; loc = np.stack([(low[:, 0] - st[0]) * math.cos(h) + (low[:, 1] - st[1]) * math.sin(h), -(low[:, 0] - st[0]) * math.sin(h) + (low[:, 1] - st[1]) * math.cos(h)], 1); g = np.hypot(np.maximum(np.abs(loc[:, 0]) - 5.0, 0), np.maximum(np.abs(loc[:, 1]) - 1.25, 0)); print('stop', si, st, 'truck to aircraft %.2f m' % g.min())
    for sl in range(-180, 180, 15):
        for li in (15, 20, 25, 30, 35, 40, 45, 50, 55, 60):
            for ex in (0, 1.5, 3, 4.5, 6):
                P, T, C, N = fk(st, sl, li, ex)
                if ok(C, 1.9) and ok(N, 1.2) and all(ok(P + (T - P) * k, 1.0) for k in (.4, .6, .8, 1)): CAND.append((si, sl, li, ex, N))
NS = np.array([c[4] for c in CAND]); out = {}; used = {}
for g, pts in G.items():
    miss = 0; worst = 0
    for p in pts:
        d = np.linalg.norm(NS - p, axis=1); up = NS[:, 2] >= p[2] - (3.0 if g == 'fin' else -.2); i = np.where((d >= 2) & (d <= 6) & up)[0]
        if not len(i): miss += 1; continue
        j = i[np.argmin(np.abs(d[i] - 4))]; worst = max(worst, float(d[i].min())); c = CAND[j]; used.setdefault(c[0], {}).setdefault(g, []).append((c[1], c[2], c[3]))
    out[g] = {'samples': len(pts), 'not_reached': miss, 'worst_nearest_m': round(worst, 2)}
print(out); stops = [{'stop': i, 'x_aft_of_nose': s[0], 'y_right': s[1], 'heading_deg': s[2], 'does': {g: {'slew': sorted({p[0] for p in l}), 'lift': [min(p[1] for p in l), max(p[1] for p in l)], 'out_m': [min(p[2] for p in l), max(p[2] for p in l)]} for g, l in used.get(i, {}).items()}} for i, s in enumerate(PATH)]
D = json.load(open(J)); D['a320_reach'] = {'result': out, 'right_side_path': stops, 'note': 'mirror y and slew for the left side; heading 0 = nose to tail of the aircraft, positive turns away to the right; cabin >= 1.9 m, boom >= 1.0 m, nozzle 2-6 m'}; json.dump(D, open(J, 'w'), indent=1)
F = lambda s: ImageFont.truetype('C:/Windows/Fonts/arial.ttf', s); im = Image.new('RGB', (3000, 2000), (0, 0, 0)); d = ImageDraw.Draw(im); t = lambda x, y, s, z=26, c=(225, 230, 235): d.text((x, y), s, font=F(z), fill=c)
def put(n, x, y, w): i = Image.open(os.path.join(TMP, n + '.png')); i = i.resize((w, int(i.height * w / i.width)), Image.LANCZOS); im.paste(i, (x, y), i); return i.height
t(60, 40, 'DE-ICER  -  Airport Dynasty Pro, to-do 212', 46); put('side', 60, 130, 1400); put('top', 60, 740, 1400); put('front', 1500, 130, 500); put('rear', 2020, 130, 500); put('up', 1500, 740, 1150)
def dim(x0, x1, y, s): d.line([(x0, y), (x1, y)], fill=(255, 170, 60), width=2); [d.line([(x, y - 10), (x, y + 10)], fill=(255, 170, 60), width=2) for x in (x0, x1)]; d.text(((x0 + x1) / 2, y + 22), s, font=F(24), fill=(255, 190, 90), anchor='mm')
X = lambda x: 60 + 700 - x * 1400 / 11; dim(X(5.0), X(-4.95), 690, '9.95'); dim(X(3.5), X(-1.55), 655, 'wheelbase 5.05')
t(1600, 690, 'width 2.40 (2.92 over the mirrors)'); t(2120, 690, 'height 3.40 stowed, cab 2.76, beacons 2.91'); t(1560, 1770, 'boom 55 deg, 11.35 m: cabin floor 10.3 m, nozzle 10.5 m; lift 0 - 60 deg, slew 360 deg (never below 15 deg lift away from the rest)', 22)
for i, s in enumerate(['Tanks: water 2,600 l, type I 2,400 l, type IV 2,400 l; sight gauges both sides, caps on top, three fill couplings in the pump panel (left).', 'Turret behind the cab, boom 5.35 - 11.35 m (two telescopes of 3.0 m), hose chain of 70 links on top, rotary levelling motor at the cabin.', 'Glass spray cabin: one seat, two joysticks, screen, spray pedal, sliding door left (56 cm), wiper, four work lamps, type I and type IV nozzles.', 'Heater module with burner, louvred door, exhaust; hand hose %.1f m on a reel (right).' % D['hand_hose_m'], 'Reach over an A320 (nozzle 2 - 6 m off): ' + '; '.join('%s %d of %d' % (g, o['samples'] - o['not_reached'], o['samples']) for g, o in out.items()), 'Drive path, right side (x aft of the nose, y from the middle, heading): ' + ', '.join('%.1f/%.1f/%d' % s for s in PATH), 'Seen: Wikimedia Commons - Edinburgh 5709711 and 5709720 (geograph), Kyiv Zhuliany, Arlanda, airport deicing truck 01 (2009), Paderborn. Nothing copied.']): [t(60, 1330 + 62 * i + 26 * j, l, 23) for j, l in enumerate(textwrap.wrap(s, 112))]
# the plan of the path
ox, oy, k = 2640, 800, 27; q = lambda x, y: (ox + y * k * .62 - 60, oy + x * k * .9); d.line([q(0, 0), q(37.6, 0)], fill=(120, 160, 190), width=14); d.line([q(11.9, 2), q(19.4, 17), q(20.9, 17), q(18.6, 2)], fill=(120, 160, 190), width=3); d.line([q(31.2, 1), q(34.9, 6.2), q(36.4, 6.2), q(34.6, 1)], fill=(120, 160, 190), width=3); d.line([q(*s[:2]) for s in PATH], fill=(255, 170, 60), width=3); [d.ellipse((q(*s[:2])[0] - 8, q(*s[:2])[1] - 8, q(*s[:2])[0] + 8, q(*s[:2])[1] + 8), fill=(255, 170, 60)) for s in PATH]
o = os.path.join(R, 'docs', 'Blueprints', 'Apron', 'Deicer'); im.save(o + '.png'); im.save(o + '.pdf', resolution=200)
