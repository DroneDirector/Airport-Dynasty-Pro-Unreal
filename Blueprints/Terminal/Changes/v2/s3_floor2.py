# Sheets 3a-3c: Floor 2 AFTER - the passport wall and the swing gates (before / after), the concourse, how a gate swings.
from v2_common import *
which = sys.argv[1:] or list('abc')
JW = JETWAYS


def jetways(ax):
    for g in JW:
        for x in JW[g]: ax.plot([x, x], [82, 90], color=OK, lw=6, zorder=11)


def walkway_belts(ax):
    for a, b in ((165, 235), (245, 315), (322.5, 392.5), (400, 470), (477.5, 547.5)):
        for z in (58.4, 60.1): box(ax, a, z, b, z + 1.5, '', fc='#26303a', ec='#4a5866', lw=0.8, z=3)


if 'a' in which:
    fig = sheet('SHEET 3a', 'FLOOR 2: THE PASSPORT WALL AND THE SWING GATES, BEFORE AND AFTER',
                'Gates 4 and 13 can serve either side. Everything else on Floor 2 west of x 228 stays as it is.')
    for k, after in enumerate((False, True)):
        ax = ax_plan(fig, [0.012, 0.50 - k * 0.455, 0.66, 0.42], 162, 332, 17, 92)
        fills(ax, 2); gates(ax, 2, fs=17, x0=162, x1=332); walkway_belts(ax)
        if after: after_f2(ax, lw=2)
        walls(ax, 2, lw=2); jetways(ax)
        fig.text(0.012, 0.925 - k * 0.455, 'AFTER' if after else 'BEFORE (today)', color=NEW if after else FG, fontsize=19, weight='bold', va='top') if k else None
        txt(ax, 201, 65, 'SCHENGEN WALKWAY' if after else 'GATE WALKWAY, 16 m wide', SCH if after else DIM, 15)
        if not after:
            gate_stairs(ax)
            ax.plot([276.4, 278.4], [82, 82], color='#ff5050', lw=6, zorder=14); txt(ax, 277.4, 85, 'dead door', '#ff8080', 12)
            fig.text(0.012, 0.915, 'BEFORE (today)', color=FG, fontsize=19, weight='bold', va='top')
            txt(ax, 245, 35, 'nothing here: open air under the wave roof', WARN, 17); txt(ax, 220, 75, 'view bay', DIM, 14); txt(ax, 306, 75, 'view bay (its 3 jetways)', DIM, 14)
            txt(ax, 246.2, 78, 'gap\n18.4 m', DIM, 14); txt(ax, 280, 65, 'moving walkways', DIM, 14); txt(ax, 330.5, 86, '')
            continue
        txt(ax, 290, 65, 'NON-SCHENGEN WALKWAY', NS, 15); txt(ax, 246.2, 73.5, 'SWING\nLOBBY', NEW, 14, weight='bold')
        for x, z, n, c in ((240, 54.5, 1, GLASS), (239.5, 40, 2, SCH), (238, 33.6, 3, NEW), (252, 31, 4, NS), (252, 43.5, 5, NS), (274, 38, 6, DF), (229.5, 40, 7, TRF), (266, 22, 7, TRF), 
                           (251, 79.6, 8, NEW), (313.5, 79.5, 9, NS), (297, 78.5, 10, NS), (222, 83.5, 11, NS), (192.4, 80, 12, NS)):
            num(ax, x, z, n, c, fs=15)
        for (x0, z0, x1, z1, n) in ((177.05, 68, 185.05, 68, 'a'), (215, 68, 225, 68, 'a'), (263.4, 68, 271.4, 68, 'b'), (301.4, 68, 311.4, 68, 'b')):
            ax.plot([x0, x1], [z0, z1], color=OK, lw=8, zorder=13)
        ax.plot([237.4, 239.7], [68, 68], color=SCH, lw=8, zorder=13); ax.plot([241.5, 247.5], [68, 68], color=NS, lw=8, zorder=13)
        ax.plot([237.05, 237.05], [70, 77], color=NEW, lw=8, zorder=13); ax.plot([255.4, 255.4], [70, 77], color=NEW, lw=8, zorder=13)
        num(ax, 181, 70.6, 'A', OK, fs=14); num(ax, 220, 70.6, 'A', OK, fs=14); num(ax, 267.4, 70.6, 'B', OK, fs=14); num(ax, 304, 70.6, 'B', OK, fs=14)
        num(ax, 238.5, 65.4, 'C', SCH, fs=14); num(ax, 244.5, 65.4, 'D', NS, fs=14); num(ax, 234.6, 73.5, 'E', NEW, fs=14); num(ax, 258, 73.5, 'F', NEW, fs=14)
        dim(ax, 248, 256, 18.4, '8', fs=13); dim(ax, 231, 248, 18.4, '17', fs=13); dim(ax, 258, 290, 18.4, '32', fs=13); txt(ax, 298.5, 31, 'shop', NEW, 13); txt(ax, 298.5, 46, 'toilets', BELT, 13); txt(ax, 323, 38, 'TRANSFER\nCENTRE', TRF, 15)
        txt(ax, 195, 35, 'open: the East Hall below', DIM, 15)
    legend(fig, 0.685, 0.895, [(1, 'Glass wall across the walkway at x 240, floor to ceiling;\none staff door (badge) and an alarmed escape door', GLASS), (2, 'Queue hall 17 x 28 m, off the walkway', SCH), (3, 'EES kiosks, 8', NEW),
                               (4, '4 passport booths; one wide for PRM, crew, families', NS), (5, '8 e-gates', NS), (6, 'Walk-through duty free 32 x 28 m', DF), (7, 'Transfers back to Schengen: closed glass lane', TRF),
                               (8, 'Swing lobby 18.4 x 14 m, in the gap between the gates,\non columns, 4.5 m clear below', NEW), (9, 'Vestibule 18.5 x 5 m at the jetways: doors to the gate\nside and to the arrivals side, never both', NS),
                               (10, 'Arrivals way down 14.8 x 7.5 m: 2 escalators, stair, lift', NS), (11, 'Gate 4\'s arrivals gallery: glass, outside the windows,\n2.6 m wide, 5.6 m clear below, over the bag trains', NS),
                               (12, 'Gate 4\'s jetway vestibule 8.8 x 4 m. Thick green on the\nglass: real jetway doors cut in; the dead door glazed', NS)], lh=0.0343, fs=15)
    fig.text(0.685, 0.49, 'THE DOORS', color=NEW, fontsize=18, weight='bold')
    legend(fig, 0.685, 0.462, [('A', 'Gate 4 to the Schengen walkway (glass doors, new)', OK), ('B', 'Gate 13 to the non-Schengen walkway (glass doors, new)', OK), ('C', 'Lobby to the Schengen walkway', SCH), ('D', 'Lobby to the non-Schengen walkway', NS),
                               ('E', 'Lobby to gate 4\'s view bay', NEW), ('F', 'Lobby to gate 13', NEW)], lh=0.031, fs=15)
    fig.text(0.685, 0.268, 'WHY THE QUEUE IS BESIDE THE WALKWAY', color=NEW, fontsize=17, weight='bold')
    fig.text(0.685, 0.25, 'The accepted sheet had the booths IN the walkway at x 240-248.\nThe moving walkway ends at x 235 and the next starts at x 245:\nonly 10 m, no room for a queue. The wall now stands in that gap,\nso no moving walkway is cut, and the booths and the queue sit in\nthe new floor beside it.', color=FG, fontsize=15, va='top', linespacing=1.35)
    fig.text(0.685, 0.142, 'THE STAIRS of gates 4 and 13 are turned along the walkway wall\n(2.8 x 11 m, foot west). Through the inner wall (green dots):\n7.8 m wide at gate 4, 3.1 m at gate 13 (it was 1.4 m).\nGates 14-16 stay non-Schengen, 1-3 and 5-12 Schengen.', color=FG, fontsize=15, va='top', linespacing=1.35)
    save(fig, 'TCv2-3a-Passport-Wall-Swing-Gates.png')

if 'b' in which:
    fig = sheet('SHEET 3b', 'FLOOR 2 AFTER: THE NON-SCHENGEN CONCOURSE', 'A new floor at +6.0 m over the ground-floor halls, x 228-551, z 20-52 (323 x 32 m = 10 342 m²). It opens straight off the walkway (green dotted openings).')
    ax = ax_plan(fig, [0.008, 0.40, 0.984, 0.52], 222, 556, 8, 93)
    fills(ax, 2); gates(ax, 2, fs=17, x0=222, x1=556); walkway_belts(ax); after_f2(ax, lw=2); walls(ax, 2, lw=2); jetways(ax)
    for k, t, c, fs in (('queue', 'PASSPORT\nQUEUE', SCH, 13), ('dutyfree', 'DUTY FREE\nwalk-through\n32 x 28', DF, 15), ('transfer', 'TRANSFER\nCENTRE\n33 x 28', TRF, 15), ('food', 'FOOD COURT, CAFÉ, BAR\n56 x 28', NEW, 16),
                        ('lounge', 'PREMIUM LOUNGE\n44 x 28', NEW, 15), ('seating', 'SEATING, KIDS,\nWORK DESKS\n48 x 28', NEW, 15)):
        b = F[k]; txt(ax, (b[0] + b[2]) / 2, 45.5 if k == 'queue' else (b[1] + b[3]) / 2, t, c, fs, weight='bold')
    txt(ax, 298.5, 31, 'SHOPS\n13 x 14', NEW, 12); txt(ax, 298.5, 46, 'TOILETS\n13 x 12', BELT, 12); txt(ax, 412, 30.5, 'PRAYER +\nQUIET 24 x 13', NEW, 13); txt(ax, 412, 45.5, 'TOILETS, FAMILY,\nBABY 24 x 13', BELT, 13)
    txt(ax, 536.6, 31, 'SHOPS\n29 x 14', NEW, 13); txt(ax, 536.6, 46, 'TOILETS\n29 x 12', BELT, 13); txt(ax, 430, 22, 'WINDOW GALLERY 4 m, over the promenade, view to the forecourt', NEW, 13); txt(ax, 268, 22, 'closed transfer lane', TRF, 12)
    txt(ax, 355, 64.8, 'NON-SCHENGEN WALKWAY (existing)', NS, 15); txt(ax, 246.2, 75, 'SWING\nLOBBY', NEW, 13)
    for g in GATE_X0: v, c = core_f2(g); txt(ax, (c[0] + c[2]) / 2, 78.2, 'way\ndown', NS, 12)
    for x in STAIRS: txt(ax, x, 11.5, 'stair', OK, 11)
    txt(ax, 430, 88.5, 'jetways (green) unchanged, their doors cut into the glass', OK, 14)
    dim(ax, 20, 52, 553.5, '32.0', vertical=True, fs=14)
    legend(fig, 0.014, 0.365, [('i', 'Transfer desk, 2 security lanes, 3 passport booths + 2 e-gates for transfers', TRF), ('ii', 'Stair, lift and 2 escalators up from the arrivals corridor (x 326-338, z 40-51)', TRF),
                               ('iii', 'Ceiling: the wave roof over z 20-34 (7 to 15 m high), a flat roof at 12 m over z 34-52 (5.4 m high, as the walkway)', GLASS),
                               ('iv', 'Between them a band of glass (z 34), 1 to 8 m high: daylight', GLASS)], lh=0.034, fs=15.5)
    fig.text(0.014, 0.215, 'FINISH: the Grand Hall\'s marble, the walkway\'s oak by the gates, the same lamps, signs, screens, seats and plants (to-do 213, 4c).', color=FG, fontsize=15.5)
    fig.text(0.014, 0.182, 'SCHENGEN PASSENGERS keep their own: the big tax free (5 366 m²), food walk, eateries, bar and toilets on their side. No passenger can pass the wall without a non-Schengen boarding pass and a passport check.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.149, 'FIRE ESCAPE: every spot of the concourse is within 35 m of one of the 11 stairs (30 m apart); the walkway\'s own gate stairs stay as further ways out.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.116, 'STRUCTURE: the slab stands on new columns in the walls below (claim hall, passport hall, staff rooms, coach station); the 5.4 m ceiling below is the same as in your west claim hall.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.083, 'CHANGED FROM THE ACCEPTED SHEET TC-3: its arrivals corridor ran on Floor 2 along z 52-56. That would close the concourse off from the walkway, and its screens would stop departing', color=WARN, fontsize=15.5)
    fig.text(0.014, 0.055, 'passengers at every arrival. So arrivals now go down at the gate. The mezzanine of TC-3 (x 330-372) is not needed any more.', color=WARN, fontsize=15.5)
    save(fig, 'TCv2-3b-Concourse.png')

if 'c' in which:
    fig = sheet('SHEET 3c', 'HOW A SWING GATE WORKS: FOUR CASES', 'Green = open. Red = locked and alarmed. Blue route: Schengen passengers. Pink route: non-Schengen passengers.')
    CASES = (('1  NORMAL', 'Gate 4 Schengen, gate 13 non-Schengen. Lobby closed on all sides.', dict(A=1, B=1, C=0, D=0, E=0, F=0), [('S', [(190, 60), (181, 60), (181, 72), (188, 74)]), ('N', [(280, 60), (306, 60), (306, 73), (312, 76.5)])]),
             ('2  GATE 4 SWUNG TO NON-SCHENGEN (London with an A320): departing', 'Doors A locked. The lobby opens to the non-Schengen side (D) and to gate 4 (E).', dict(A=0, B=1, C=0, D=1, E=1, F=0), [('N', [(300, 60), (244.5, 60), (244.5, 73.5), (205, 73.5), (195, 73.5), (192.5, 76.5)])]),
             ('3  GATE 4 SWUNG TO NON-SCHENGEN: arriving', 'Out of the jetway vestibule into the glass gallery, over to the way down at gate 13. Gate 13 can board all the while.', dict(A=0, B=1, C=0, D=0, E=0, F=0), [('N', [(191, 84), (191, 80), (195.5, 80), (195.5, 83.5), (301.5, 83.5), (301.5, 78), (292, 78)])]),
             ('4  GATE 13 SWUNG TO SCHENGEN', 'Doors B locked. The lobby opens to the Schengen side (C) and to gate 13 (F), through to the jetways. Arrivals walk the same way out.', dict(A=1, B=0, C=1, D=0, E=0, F=1), [('S', [(200, 60), (238.5, 60), (238.5, 72.8), (298, 72.8), (307, 72.8), (312, 76.5)])]))
    for i, (t, s, d, R) in enumerate(CASES):
        x, y = 0.012 + (i % 2) * 0.494, 0.585 - (i // 2) * 0.37
        ax = ax_plan(fig, [x, y, 0.482, 0.24], 168, 318, 50, 92)
        fills(ax, 2); gates(ax, 2, fs=15, x0=168, x1=318); walkway_belts(ax); after_f2(ax, lw=1.4); walls(ax, 2, lw=1.6); jetways(ax)
        D = {'A': [((177.05, 68), (185.05, 68)), ((215, 68), (225, 68))], 'B': [((263.4, 68), (271.4, 68)), ((301.4, 68), (311.4, 68))], 'C': [((237.4, 68), (239.7, 68))], 'D': [((241.5, 68), (247.5, 68))],
             'E': [((237.05, 70), (237.05, 77))], 'F': [((255.4, 70), (255.4, 77))]}
        for k, v in D.items():
            for p, q in v: ax.plot([p[0], q[0]], [p[1], q[1]], color=OK if d[k] else '#ff3030', lw=7, zorder=14)
        for k, (nx, nz) in {'A': (174.5, 70.3), 'B': (274, 70.5), 'C': (235, 65.6), 'D': (250.5, 65.6), 'E': (234, 78.2), 'F': (258.4, 78.2)}.items():
            num(ax, nx, nz, k, OK if d[k] else '#ff5050', fs=13)
        for s_, p in R: route(ax, p, SCH if s_ == 'S' else NS, lw=5)
        fig.text(x, y + 0.272, t, color=NEW, fontsize=18, weight='bold', va='center'); fig.text(x, y + 0.25, s, color=FG, fontsize=14.5, va='center')
    L = ['THE SWITCH, every time: Operations decides per flight; 1 the last flight\'s passengers have left; 2 a security officer walks the gate, view bay, lobby and gallery and finds them empty; 3 he checks both door sets locked;',
         '4 the access-control system unlocks the new side, never both (hardware interlock); 5 the gate screen, signs and boards change (\'London, non-Schengen\'); 6 an alarm calls security if a door is forced or stays open.',
         'REAL RULE: Regulation 300/2008, 4.2: screened departing passengers must not mix with arriving ones from countries without recognised security. If such a flight used the gate, the swept area is also searched before the',
         'next boarding (2015/1998, 1.1.3). Passengers from the UK, USA, Canada and the other recognised countries need only the passport separation. Doors are CCTV-covered and logged; the player passes staff doors with his badge.']
    for i, l in enumerate(L): fig.text(0.014, 0.155 - i * 0.031, l, color=FG if i < 2 else '#cfe3f0', fontsize=15)
    save(fig, 'TCv2-3c-Swing-Gate-Cases.png')
