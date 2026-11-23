# To-do 213 detailed renders, part 2: the NEW parts of the accepted plan (TC-2a/2b/3/4, TC-6, the non-Schengen quick
# explainer, to-do 212's additions) added to the REAL terminal (F:/ADP-Export/tc_exist.blend = build_scene.py's scene with
# every massing part removed). The wave roof and its tree pillars already stand to x 551 in the game's own layout
# (ROOF-E1..E4), so they are NOT rebuilt: the new glass runs up to the REAL soffit. Saves F:/ADP-Export/tc_after.blend
#   blender -b F:/ADP-Export/tc_exist.blend --python build_new.py
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
exec(open(os.path.join(HERE, 'new_lib.py'), encoding='utf-8').read())
exec(open(os.path.join(HERE, 'roof_swap.py'), encoding='utf-8').read())


BBC = {}
def bb(o):
    c_ = BBC.get(o.name)
    if c_ is not None: return c_
    b = [o.matrix_world @ Vector(c) for c in o.bound_box]
    r = ([min(v[i] for v in b) for i in range(3)], [max(v[i] for v in b) for i in range(3)]); BBC[o.name] = r; return r


# ---------------------------------------------------------------- the Babylon leftovers the game does not use: its own
# aircraft + jetways on the apron (the game's come from jetways.glb / A320) and its roads, cars and car park
kill = []
for o in list(EX.objects):
    if o.type != 'MESH': continue
    if o.name in ('Green', 'Red'): kill.append(o); continue
    if o.get('grp') == 'SITE':
        a, b = bb(o); zc = (a[1] + b[1]) / 2
        if o.name.startswith(('SITE/ROAD', 'SITE/CARPARK', 'SITE/GRASS', 'SITE/KERB', 'SITE/SIDE', 'SITE/APRON')) or zc > 83 or -100 < zc < -20: kill.append(o)
bpy.data.batch_remove(set(kill)); print('REMOVED', len(kill), flush=True)
for m in bpy.data.materials:                                   # the terminal's glass: clear, real reflections
    if m.name.lower().startswith(('glass', 'x_glass')) and m.use_nodes:
        pass

# the game's own cuts (Data/terminal-cuts.json: [x0, h0, z0, x1, h1, z1] in plan metres, per layout node) taken out of the real pieces
import re as _re
LN = json.load(open(os.path.join(ROOT, 'Data', 'terminal-layout.json')))['nodes']
BYN = {}
for o in EX.objects:
    if o.type == 'MESH': BYN.setdefault(_re.sub(r'\.\d{3,}$', '', o.name), []).append(o)
for e in json.load(open(os.path.join(ROOT, 'Data', 'terminal-cuts.json'))):
    for o in BYN.get(LN[e['node']]['name'], []):
        hit = 0
        for x0, h0, z0, x1, h1, z1 in e['boxes']:
            lo, hi = P(min(x0, x1), min(z0, z1), min(h0, h1)), P(max(x0, x1), max(z0, z1), max(h0, h1)); a, b = bb(o)
            if any(b[i] <= lo[i] or a[i] >= hi[i] for i in range(3)): continue
            me = o.data.copy(); bm = bmesh.new(); bm.from_mesh(me); bm.transform(o.matrix_world)
            for i in range(3):
                for v in (lo[i], hi[i]):
                    n_ = Vector((0, 0, 0)); n_[i] = 1; c_ = Vector((0, 0, 0)); c_[i] = v
                    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=c_, plane_no=n_, dist=1e-5)
            dead = [f for f in bm.faces if all(lo[i] - 1e-4 < f.calc_center_median()[i] < hi[i] + 1e-4 for i in range(3))]
            if dead: bmesh.ops.delete(bm, geom=dead, context='FACES'); bm.to_mesh(me); o.data = me; o.matrix_world = Matrix(); bpy.context.view_layer.update(); BBC.pop(o.name, None); hit += len(dead)
            bm.free()
        if hit: print('GAMECUT', e['node'], o.name, hit, flush=True)
# ---------------------------------------------------------------- the landside from Data/roads.json's own lane widths
box(-1500, -900, 1900, 900, -0.5, -0.06, 'GRASS', LAND)
box(-265, 82, 570, 238, -0.06, -0.02, 'CONC', LAND)
box(-241, 52, 551.2, 82, -0.06, -0.03, 'CONC', LAND)
for (a, b, c) in ((-900, 938, (-51.25, -42.25)), (-480, 820, (-38.75, -21.5))): box(a, c[0], b, c[1], -0.06, 0.0, 'ASPH', LAND)
box(-480, -42.25, 820, -38.75, -0.06, 0.13, 'KERB', LAND); box(-480, -42.1, 820, -38.9, 0.1, 0.16, 'GRASS', LAND)
box(-480, -21.5, 820, -21.3, -0.06, 0.15, 'KERB', LAND); box(-120, -21.3, 241, 0, -0.06, 0.14, 'TERR', LAND); box(-480, -21.3, -120, -8, -0.06, 0.14, 'TERR', LAND); box(241, -21.3, 551, 0, -0.06, -0.02, 'GRASS', LAND)
for x in range(-900, 938, 9): box(x, -46.85, x + 4.5, -46.7, 0, 0.012, 'PAINT', LAND)
for x in range(-480, 820, 9): box(x, -35.2, x + 3, -35.08, 0, 0.012, 'PAINT', LAND); box(x, -28.55, x + 3, -28.43, 0, 0.012, 'PAINT', LAND)
for xc in (49.0, 75.0, 101.0):                                # the three zebra crossings
    for k in range(34):
        zz = -51 + k * 0.9
        if not (-42.4 < zz < -38.6): box(xc - 1.5, zz, xc + 1.5, zz + 0.45, 0, 0.013, 'PAINT', LAND)
for k in range(12): x = 235.8 - k * 6.8; box(x - 6, -31.6, x - 5.9, -28.5, 0, 0.013, 'PAINT', LAND); random.random() < 0.6 and car(x - 5.2, -31, 'CAR%d' % random.randrange(6), True, LAND)
for s, n in ((1711.1, 3), (1765.225, 1), (1791.415, 1), (1813.48, 2)):      # the 7 coach stands with their coaches
    for k in range(n):
        x1 = 938 - (s + k * 16 - 921.9); x0 = x1 - 13
        for (a, b, c, d) in ((x0, -24.9, x1, -24.7), (x0, -21.8, x1, -21.6), (x0, -24.9, x0 + 0.2, -21.6), (x1 - 0.2, -24.9, x1, -21.6)): box(a, b, c, d, 0, 0.014, 'PYEL', LAND)
        coach(x0 + 0.5, -24.5, LAND)
box(-93, -247, 367, -54.45, -0.06, 0.0, 'ASPH', LAND); box(398, -247.3, 554, -179, -0.06, 0.0, 'ASPH', LAND)
LOT = json.load(open(os.path.join(ROOT, 'Data', 'roads.json')))['lots'][0]
random.seed(5)
for r in LOT['rows']:
    if 'x0' not in r: continue
    x = r['x0']; z0, z1 = sorted((r['y'], r['y'] + r['dir'] * r['d']))
    while x < r['x1'] - 2.4:
        box(x, z0, x + 0.1, z1, 0, 0.012, 'PAINT', LAND)
        if random.random() < 0.55: car(x + 0.4, z0 + 0.3, 'CAR%d' % random.randrange(6), False, LAND)
        x += r.get('w', 2.5)
for x in range(-460, 820, 31):                                 # the lamp posts
    cyl(P(x, -20.6, 0), P(x, -20.6, 9), 0.09, 'STEEL', LAND, 8); box(x - 0.15, -22.6, x + 0.15, -20.6, 8.9, 9.0, 'STEEL', LAND)

# ================================================================ NEW, outside
def glass_front(xa, xb, z, tag='ground', top=None, gaps=(), foot=0.0):
    x = xa; Ls = 2.5
    tf = (lambda q: soffit(q, z)) if top is None else (lambda q: top)
    while x < xb - 0.01:
        e = min(x + Ls, xb)
        g = any(a - 0.01 <= x and e <= b + 0.01 for a, b in gaps); h0 = 3.8 if g else foot
        n = 4
        for k in range(n):
            u, w = x + (e - x) * k / n, x + (e - x) * (k + 1) / n
            quad([P(u, z, h0), P(w, z, h0), P(w, z, tf(w)), P(u, z, tf(u))], 'GLASS')
            hb, hc = tf(u), tf(w); gm = GOLDS[tag]
            quad([P(u, z - 0.3, hb - 0.45), P(w, z - 0.3, hc - 0.45), P(w, z - 0.3, hc - 0.2), P(u, z - 0.3, hb - 0.2)], 'GOLD', gm)
        box(x - 0.06, z - 0.16, x + 0.06, z + 0.16, h0, tf(x), 'MUL'); x = e
    box(xb - 0.06, z - 0.16, xb + 0.06, z + 0.16, 0, tf(xb), 'MUL')
    for (a, b) in (((xa, xb),) if not gaps else ((xa, xb),)): box(a, z - 0.1, b, z + 0.1, 5.35, 5.5, 'MUL'); box(a, z - 0.1, b, z + 0.1, 10.4, 10.52, 'MUL')
    box(xa, z - 0.45, xb, z - 0.2, 0.0, 0.2, 'GOLD', GOLDS[tag])


VEST = ((188, 200), (290, 300), (396, 406), (514, 526))
glass_front(156, 551.2, 0.0, gaps=((156, 161),) + VEST)
for z in range(0, 34, 3):                                      # the east end wall, glass to the roof
    quad([P(551.2, z, 0), P(551.2, min(z + 3, 34), 0), P(551.2, min(z + 3, 34), soffit(550.8, min(z + 3, 33.5))), P(551.2, z, soffit(550.8, z))], 'GLASS'); box(551.1, z - 0.06, 551.35, z + 0.06, 0, soffit(550.8, z), 'MUL')
box(551.1, 34, 551.4, 52, 0, 12, 'PLAST')
for v0, v1 in VEST:                                            # the new entrance vestibules (the Grand Hall's pattern)
    for (a, b, c, d) in ((v0, -4, v1, -3.96), (v0, -4, v0 + 0.04, 0), (v1 - 0.04, -4, v1, 0)): box(a, b, c, d, 0, 3.7, 'GLASS')
    box(v0 - 0.2, -4.4, v1 + 0.2, 0, 3.7, 3.95, 'ZINC'); gold(v0 - 0.2, -4.4, v1 + 0.2, 0, 0, 3.95)
    for x in (v0 + 0.1, (v0 + v1) / 2 - 1.4, (v0 + v1) / 2 + 1.4, v1 - 0.1): box(x - 0.06, -4.06, x + 0.06, -3.9, 0, 3.7, 'MUL')
for x, s in ((194, 'EAST HALL'), (295, 'PROMENADE'), (401, 'PROMENADE'), (520, 'COACH + BUS STATION')): sign(s, x, -4.5, 4.35, 8.5 if len(s) < 12 else 11, 0.75)
box(156, 34, 551.2, 52, 12.0, 12.4, 'ZINC', RF); gold(156, 34, 551.2, 52, 12.0, 12.45, 'roof')   # the flat roof behind the wave (z 34-52)
# ---- the double skin x 107-156: a glass skin at z -4 up to the wave, the block's own wall and the office windows stay behind it
glass_front(107, 160, -4.0)
for x in (107, 160):
    for z0, z1 in ((-4, -2), (-2, 0)): quad([P(x, z0, 0 if x == 107 else 0), P(x, z1, 0), P(x, z1, soffit(x, z1)), P(x, z0, soffit(x, z0))], 'GLASS')
    box(x - 0.06, -2.06, x + 0.06, -1.94, 0, soffit(x, -2), 'MUL')
box(107, -4, 160, 0, 0.14, 0.17, 'MARBLE')
for x in range(114, 157, 6):                                   # steel brackets tying the two skins (at the floor lines)
    for h in (5.42, 10.46): box(x - 0.06, -4, x + 0.06, 0, h, h + 0.12, 'MUL')
for x in (117, 129, 141, 153): plant(x, -0.8, 0.17, None, 1.1)
people(108, 158, -3.4, -1.4, 10, 0.17)
sign('EAST HALL  ->', 112, -2.0, 4.2, 4.0, 0.6, 'w')
exec(open(os.path.join(HERE, 'parts_v2.py'), encoding='utf-8').read())
exec(open(os.path.join(HERE, 'parts_v2_f2.py'), encoding='utf-8').read())
flush()

exec(open(os.path.join(HERE, 'merge.py'), encoding='utf-8').read())
# ---------------------------------------------------------------- light
for o in list(bpy.data.objects):
    if o.type == 'LIGHT': bpy.data.objects.remove(o)
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 2.6; sun.color = (1, 0.95, 0.87); sun.angle = math.radians(0.8); so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so)
so.rotation_euler = (Vector((0.45, 0.62, -0.72))).to_track_quat('-Z', 'Y').to_euler()        # from the south-west, high: the forecourt is the sunny side
nt = sc.world.node_tree; nt.nodes.clear(); ow = nt.nodes.new('ShaderNodeOutputWorld'); bg = nt.nodes.new('ShaderNodeBackground'); ra = nt.nodes.new('ShaderNodeValToRGB'); tc = nt.nodes.new('ShaderNodeTexCoord'); sx = nt.nodes.new('ShaderNodeSeparateXYZ')
nt.links.new(tc.outputs['Generated'], sx.inputs[0]); nt.links.new(sx.outputs['Z'], ra.inputs[0]); nt.links.new(ra.outputs[0], bg.inputs[0]); nt.links.new(bg.outputs[0], ow.inputs[0])
ra.color_ramp.elements[0].color = (0.62, 0.72, 0.82, 1); ra.color_ramp.elements[1].position = 0.6; ra.color_ramp.elements[1].color = (0.13, 0.28, 0.6, 1); bg.inputs[1].default_value = 0.75
sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'; sc.view_settings.exposure = 0.0
sc.cycles.max_bounces = 8; sc.cycles.transparent_max_bounces = 48; sc.cycles.caustics_reflective = sc.cycles.caustics_refractive = False
bpy.ops.wm.save_as_mainfile(filepath='F:/ADP-Export/tc_after.blend', compress=False); print('SAVED AFTER')
