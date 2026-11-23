# the new parts from the redrawn sheets, part 2: Floor 2 (+6.0)
H = 6.0
def cut(x0, x1, z0, z1, h0, h1):
    """the real pieces inside the box are cut out for the AFTER pictures (the uncut originals only show in BEFORE)"""
    n = 0; lo = P(x0, z0, h0); hi = P(x1, z1, h1)
    for o in list(EX.objects) + [q for q in NEWC.objects if q.get('cutcopy')]:
        if o.type != 'MESH' or o.get('before_only'): continue
        a, b = bb(o)
        if any(b[i] <= lo[i] or a[i] >= hi[i] for i in range(3)): continue
        me = o.data.copy(); bm = bmesh.new(); bm.from_mesh(me); bm.transform(o.matrix_world)
        for i in range(3):
            for v in (lo[i], hi[i]):
                nrm = Vector((0, 0, 0)); nrm[i] = 1; co = Vector((0, 0, 0)); co[i] = v
                bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=co, plane_no=nrm, dist=1e-5)
        dead = [f for f in bm.faces if all(lo[i] - 1e-4 < f.calc_center_median()[i] < hi[i] + 1e-4 for i in range(3))]
        if not dead: bm.free(); bpy.data.meshes.remove(me); continue
        bmesh.ops.delete(bm, geom=dead, context='FACES'); bm.to_mesh(me); bm.free()
        if o.get('cutcopy'): o.data = me; o.matrix_world = Matrix(); BBC.pop(o.name, None); n += 1; continue
        c = bpy.data.objects.new(o.name + '_cut', me); NEWC.objects.link(c); c['cutcopy'] = 1; o['before_only'] = 1; c['grp'] = o.get('grp'); n += 1
    bpy.context.view_layer.update(); print('CUT', (x0, x1, z0, z1), n, flush=True)
for a, b in V2['OPEN_F2'] + ((228.3, 230.7),): cut(a, b, 51.2, 52.8, 6.02, 10.2); gold(a, 51.9, b, 52.1, 6.1, 10.2, 'f2', 0.07)
cut(237.4, 239.7, 67.2, 68.8, 6.02, 9.2); cut(241.5, 247.5, 67.2, 68.8, 6.02, 9.2); cut(255.0, 255.8, 70, 77, 6.02, 9.2)
WX = V2['WALLX']
# the glass passport wall across the walkway at x 240, floor to ceiling
quad([P(WX, 52.2, H), P(WX, 67.8, H), P(WX, 67.8, 11.4), P(WX, 52.2, 11.4)], 'GLASS', F)
for z in (52.25, 54.6, 57.2, 59.8, 62.4, 65.0, 67.75): box(WX - 0.07, z - 0.05, WX + 0.07, z + 0.05, H, 11.4, 'MUL', F)
for h in (6.02, 8.6, 11.3): box(WX - 0.08, 52.2, WX + 0.08, 67.8, h, h + 0.12, 'MUL', F)
gold(WX - 0.12, 52.2, WX + 0.12, 67.8, 6.1, 11.4, 'f2', 0.06)
sign('PASSPORT CONTROL  ·  GATES 13-16   ->', WX - 0.2, 60, 9.6, 11.5, 0.85, 'w', F, 'SIGNYT'); sign('NO ENTRY  ·  gates 1-12 by transfer', WX + 0.2, 60, 9.6, 10, 0.7, 'e', F)
# the new floor x 228-551, z 20-52: slab, marble, glass balustrade over the promenade
a = Fp['slab']; box(*a, 5.45, 6.0, 'PLAST', F); box(*a, 6.0, 6.03, 'MARBLE', F); gold(*a, 6.05, 6.05, 'f2', 0.12, True)
box(228, 19.95, 551.2, 20.02, 6.0, 7.1, 'SHOPG', F); box(228, 19.92, 551.2, 20.05, 7.1, 7.16, 'STEEL', F); box(227.95, 20, 228.05, 52, 6, 11.5, 'GLASS', F)
for x in range(234, 551, 12):
    for z in (22, 36): cyl(P(x, z, 0), P(x, z, 5.45), 0.28, 'WHITE')
TOP = 9.8; h3 = H + 0.03
# passport control out: the queue hall, 8 EES kiosks, 4 booths, 8 e-gates
box(*Fp['queue'], h3, h3 + 0.005, 'MARBLE', F); gold(*Fp['queue'], 6.1, TOP, 'f2', 0.07); gold(*Fp['line'], 6.1, TOP, 'f2')
for k in range(8): kiosk(234 + (k % 4) * 2.4, 27 + (k // 4) * 3.5, F, h3)
for k in range(4): booth(249.5, 25 + k * 2.9, 254.5, 27.2 + k * 2.9, h3, F)
for k in range(8): egate(249.8, 37.6 + k * 1.6, h3, F, 3.6, 'x')
for (z0, z1) in ((24, 25), (36.6, 37.4), (50.4, 52)): quad([P(252, z0, H), P(252, z1, H), P(252, z1, 8.8), P(252, z0, 8.8)], 'GLASS', F)
box(251.9, 24, 252.1, 52, 8.6, 9.6, 'SIGNBG', F); text('PASSPORT CONTROL', 251.85, 38, 9.1, 0.7, 'SIGNYT', F, 'w'); sign('NON-SCHENGEN  ·  GATES 13-16', 248, 38, 9.2, 13, 0.7, 'w', F)
for z in range(27, 50, 3):
    for x in range(242, 246, 3): cyl(P(x, z, H), P(x, z, H + 0.95), 0.04, 'STEEL', F, 8); box(x, z - 0.02, x + 3, z + 0.02, H + 0.85, H + 0.92, 'NAVY', F)
people(232, 247, 33, 51, 34, h3, F); people(228, 239, 53.5, 66, 16, H, F); people(242, 300, 54, 57, 14, H, F); people(242, 540, 63, 66, 40, H, F)
box(230.9, 24, 231.0, 52, 6, 9, 'SHOPG', F)                     # the closed transfer lane back to the Schengen side
for z in (20.06, 23.95): box(228, z - 0.03, 307, z + 0.03, 6, 9, 'SHOPG', F)
gold(*Fp['strip'], 6.1, 9.0, 'f2', 0.05)
# the rooms
def room(k, name, fr, kind, fl='MARBLE', ts=0.8): a = Fp[k]; shop(a[0], a[1], a[2], a[3], h3, TOP, name, F, fr, kind, fl, ts); gold(*a, 6.1, TOP, 'f2')
room('dutyfree', 'DUTY FREE', 'nwe', 'shelf'); room('shop1', 'SHOPS', 'n', 'shelf'); room('wc1', 'WC', 'n', 'none'); room('transfer', 'TRANSFER CENTRE', 'n', 'none', 'MARBLE', 0.7)
a = Fp['transfer']; box(a[0] + 6, 44, a[0] + 20, 45, h3, h3 + 1.1, 'DESK', F)
for x in (312, 321): box(x, 28, x + 2, 38, h3, h3 + 0.8, 'STEEL', F); box(x - 0.2, 31, x + 2.2, 33.5, h3, h3 + 1.8, 'DARK', F)
for k in range(3): booth(328, 27 + k * 3, 332, 29.2 + k * 3, h3, F)
a = Fp['food']; box(*a, h3, h3 + 0.01, 'OAK', F); gold(*a, 6.1, TOP, 'f2')
for i, t in enumerate(('BURGERS', 'PASTA', 'SUSHI', 'BAKERY', 'FONDUE', 'BAR')):
    x = 343 + i * 9; box(x, 29, x + 7.6, 30, h3, h3 + 1.1, 'DWOOD', F); box(x, 25, x + 7.6, 25.2, h3, TOP, 'PLAST', F); box(x, 29.4, x + 7.6, 29.7, 8.7, 9.6, 'SIGNBG', F); text(t, x + 3.8, 29.75, 9.15, 0.62, 'SIGNTX', F, 'n'); box(x, 25.2, x + 7.6, 25.3, 7.4, 8.4, 'SHOPLT' if i % 2 else 'SCREEN', F)
for x in range(345, 397, 4):
    for z in (34, 38, 42, 46): table(x, z, h3, F)
room('prayer', 'PRAYER + QUIET', 's', 'none', 'CARPET', 0.6); room('wc2', 'WC  ·  FAMILY', 'n', 'none', 'MARBLE', 0.7); room('lounge', 'PREMIUM LOUNGE', 'ns', 'lounge', 'CARPET')
a = Fp['seating']; box(*a, h3, h3 + 0.01, 'OAK', F); gold(*a, 6.1, TOP, 'f2')
for x in range(474, 516, 6):
    for z in (26, 30, 34, 38, 42, 46): seats(x, z, h3, 7, F)
room('shops2', 'SHOPS', 'n', 'shelf'); room('wc3', 'WC', 'n', 'none')
for x in range(310, 548, 14): plant(x, 21.4, h3, F, 1.0); seats(x + 2, 22.4, h3, 6, F)
people(309, 550, 20.8, 23.6, 45, h3, F); people(343, 397, 32, 50, 60, h3, F); people(473, 519, 25, 50, 40, h3, F); people(259, 289, 26, 50, 30, h3, F); people(428, 468, 26, 50, 14, h3, F)
fids(258.5, 53.2, 8.2, 5, 5, 'n', F, 'GATES 13-16'); sign('DUTY FREE  ·  FOOD  ·  LOUNGE  ·  WC', 274, 52.3, 10.7, 12, 0.7, 'n', F); sign('FOOD COURT', 370, 52.3, 10.7, 8, 0.7, 'n', F); sign('SEATING', 496, 52.3, 10.7, 6, 0.7, 'n', F); sign('PASSPORT CONTROL', 235.4, 52.3, 10.7, 7.6, 0.7, 'n', F, 'SIGNYT')
lamps(232, 550, (30, 38, 46), 11.55, RF, 7)
box(228, 34, 551.2, 52, 11.7, 11.8, 'OAK', RF)                   # the wood ceiling under the flat roof
for x in range(229, 551, 3):                                   # the band of glass between the flat roof and the wave (z 34)
    quad([P(x, 34, 12.4), P(x + 3, 34, 12.4), P(x + 3, 34, max(12.4, soffit(x + 3, 33.6))), P(x, 34, max(12.4, soffit(x, 33.6)))], 'GLASS', RF)
# gate 13's swing lobby, in the gap beside gate 4, on columns
a = Fp['lobby']; box(*a, 5.45, 6.03, 'PLAST', F); box(*a, 6.03, 6.05, 'DWOOD', F); box(a[0], 81.85, a[2], 82, 6, 10.6, 'GLASS', F); box(a[0], a[1], a[2], a[3], 10.6, 10.9, 'ZINC', RF); gold(*a, 6.05, 10.9, 'f2'); sign('SWING GATE 13', 246, 68.6, 9.8, 6, 0.6, 'n', F)
for x in (239, 246, 253):
    cyl(P(x, 80, 0), P(x, 80, 5.45), 0.3, 'WHITE', F); box(x - 0.05, 81.8, x + 0.05, 82.05, 6, 10.6, 'MUL', F)
# (gate 4 does not swing: no gallery, no door to its view bay - owner 2026-11-23)
for g in GX:                                                    # the arrivals cores: the jetway vestibule and the way down
    x = GX[g]
    for (a, b, c, d) in ((x + 20, 77, x + 25, 82), (x + 25, 69, x + 38.4, 82)):
        for q in ((a, b, c, b + 0.05), (a, b, a + 0.05, d), (c - 0.05, b, c, d)): box(*q, H, 9.6, 'SHOPG', F)
        gold(a, b, c, d, 6.1, 9.6, 'f2', 0.06)
    for k in (0, 2.2): quad([P(x + 27 + k, 70, 6.1), P(x + 28.4 + k, 70, 6.1), P(x + 28.4 + k, 80, 0.1), P(x + 27 + k, 80, 0.1)], 'STEEL', F)
