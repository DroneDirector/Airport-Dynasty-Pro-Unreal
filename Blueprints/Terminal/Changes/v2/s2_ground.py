# Sheets 2a-2e: the ground floor AFTER, enlarged, every new piece numbered with its real size; 2e the forecourt.
from v2_common import *
which = sys.argv[1:] or list('abcde')
sz = lambda b: '%.1f x %.1f m' % (abs(b[2] - b[0]), abs(b[3] - b[1]))


def forecourt(ax, x0, x1):
    """the forecourt's lanes, from Data/roads.json (the road's middle at z -46.75; the terminal side on its right)"""
    box(ax, -80, -21.5, 241, 0, '', fc='#12161a', ec='none', z=0)
    ax.add_patch(Polygon([(241, 0), (241, -21.5), (282, -18.6), (300, -18.6), (340, 0)], fc='#12161a', ec='none', zorder=0))
    for z0, z1, c in ((-25, -21.5, '#1e242b'), (-28.5, -25, '#171c22'), (-31.6, -28.5, '#1e242b'), (-38.75, -31.6, '#171c22'), (-42.25, -38.75, '#0f2016'), (-50.25, -42.25, '#171c22')):
        box(ax, -180, z0, 560, z1, '', fc=c, ec='#2c3640', lw=0.8, z=0)
    for a, b in COACH: box(ax, a, -24.9, b, -21.6, '', fc='#2a2410', ec='#e8c800', lw=1.6, z=3)
    for a, b in SETDOWN: box(ax, a, -31.5, b, -28.6, '', fc='#1b2733', ec='#c9d6e2', lw=1.0, z=3)
    for x in (48.6, 74.8, 101.0): box(ax, x - 1.5, -54, x + 1.5, -21.5, '', fc='none', ec='#ffffff', lw=1.6, ls=':', z=4)


COACH = [(148.5 - 16 * k - 13, 148.5 - 16 * k) for k in range(3)] + [(81.4, 94.4), (55.2, 68.2), (33.1, 46.1), (17.1, 30.1)]
SETDOWN = [(235.2 - 6.8 * k - 6, 235.2 - 6.8 * k) for k in range(12)]
sys.path.insert(0, os.path.join(ROOT, 'Tools')); import wave_roof as _WR      # the one wave roof's own pillars, two under every bow
PIL = [x for _, x in _WR.PILLARS]


def pillars(ax):
    for x in PIL: ax.add_patch(Circle((x, -9), 0.9, fc='#e8eef5', ec='#05080c', zorder=15))


def sec(no, title, sub, x0, x1, z0, z1, rect):
    fig = sheet(no, title, sub); ax = ax_plan(fig, rect, x0, x1, z0, z1)
    forecourt(ax, x0, x1); fills(ax, 1); gates(ax, 1, fs=15, x0=x0, x1=x1); after_f1(ax, lw=2); walls(ax, 1, lw=1.8); pillars(ax)
    return fig, ax


if 'a' in which:
    fig, ax = sec('SHEET 2a', 'GROUND FLOOR AFTER: THE WEST CLAIM WING (SCHENGEN ARRIVALS)',
                  'All rooms and walls stay. New pieces in yellow. White dots: the tree pillars.', -84, 44, -33, 56, [0.012, 0.03, 0.64, 0.88])
    W_ = WEST
    for k, n in (('df', 1), ('lf', 2), ('wc', 3), ('os', 4), ('cafe', 8), ('info', 7), ('kiosk', 9), ('atm', 10)):
        b = W_[k]; num(ax, (b[0] + b[2]) / 2, (b[1] + b[3]) / 2, n, fs=17)
    for k in ('trol1', 'trol2', 'trol3'): b = W_[k]; num(ax, b[2] + 1.6, 17.8, 5, fs=15)
    num(ax, -44.5, 6.4, 6, fs=16)
    txt(ax, -35.5, 31, 'BELTS 1-5 (unchanged)', BELT, 16, bbox=dict(fc=BG, ec='none', alpha=0.8)); txt(ax, -40, 12, 'green', OK, 15); txt(ax, -51, 12, 'red', '#ff8080', 15)
    txt(ax, -68, 12, 'toilets\n(today)', DIM, 15); txt(ax, -12, 12, 'customs\noffice', DIM, 15); txt(ax, -63.2, 46.5, 'escalators,\nstair, lift\n(today)', FG, 13)
    txt(ax, -28, 2.6, 'ARRIVALS HALL 80 x 12', FG, 16, weight='bold'); txt(ax, 21, 15, 'ESCALATOR ROOM\n(airside, unchanged)', FG, 16)
    txt(ax, 26, 37, 'service', DIM, 14); txt(ax, 43, 37, 'WC', DIM, 14); txt(ax, 27, 48, 'airside corridor', DIM, 14); txt(ax, -40, 53.9, 'baggage hall', DIM, 14)
    txt(ax, -60, -12, 'pavement', DIM, 14); box(ax, -75, -24.9, -5, -21.6, '', fc='#3a3010', ec=NEW, lw=2.2, z=5); num(ax, -40, -23.2, 11, fs=17)
    box(ax, -46, -20.5, -34, -18, '', fc='#2a2410', ec=NEW, lw=2, z=5); num(ax, -40, -19.2, 12, fs=15)
    dim(ax, -80, 0, -14.5, '80.0', fs=16); dim(ax, 16, 52, -82, '36.0', vertical=True, fs=16); dim(ax, -4, 8, -82, '12.0', vertical=True, fs=15)
    legend(fig, 0.668, 0.875, [(1, 'Arrivals duty free, %s (323 m²), by the escalators' % sz(W_['df'])), (2, 'Lost and found, %s' % sz(W_['lf'])),
           (3, 'Toilets in the claim hall, %s (none there today)' % sz(W_['wc'])), (4, 'Oversize belt, %s' % sz(W_['os'])), (5, 'Trolley bays, 3 of 8 x 1.6 m'),
           (6, 'Meeters\' rail, 25 m, across both customs exits'), (7, 'Information desk, %s' % sz(W_['info'])), (8, 'Coffee bar, %s, its terrace outside under the roof' % sz(W_['cafe'])),
           (9, 'Kiosk + SIM cards, %s' % sz(W_['kiosk'])), (10, 'Cash machines + currency exchange, %s' % sz(W_['atm'])),
           (11, 'TAXI RANK WEST: 10 cabs, 70 m of the kerb lane'), (12, 'Taxi shelter + sign, 12 x 2.5 m')], lh=0.031, fs=16)
    fig.text(0.668, 0.475, 'WHAT STAYS', color=NEW, fontsize=17, weight='bold')
    fig.text(0.668, 0.455, 'The five belts, customs, the arrivals hall, the escalators\nand every wall. Claim hall 2 674 m²; after the new pieces\n2 140 m² stay free, and 1 323 m² are needed.',
             color=FG, fontsize=16, va='top', linespacing=1.4)
    fig.text(0.668, 0.345, 'WHY THE TOILETS', color=NEW, fontsize=17, weight='bold')
    fig.text(0.668, 0.325, 'The wing\'s toilets open only onto the arrivals hall (doors at\nz 8). A passenger waiting for a bag has none today.', color=FG, fontsize=16, va='top', linespacing=1.4)
    fig.text(0.668, 0.245, 'WHY THE RANK IS HERE', color=NEW, fontsize=17, weight='bold')
    fig.text(0.668, 0.225, 'The kerb lane is empty from x -80 to 17. The seven coach\nstands start at x 17 and stay.\nCustoms here checks EVERY arrival, also from the EU:\nSwitzerland is outside the EU customs union.', color=FG, fontsize=16, va='top', linespacing=1.4)
    save(fig, 'TCv2-2a-Ground-West-Claim.png')

if 'b' in which:
    fig, ax = sec('SHEET 2b', 'GROUND FLOOR AFTER: GRAND HALL, LINK GALLERY, DOUBLE-SKIN FRONT',
                  'The Grand Hall keeps its 72 x 30 m. The offices at x 114-156 keep their windows behind a second glass skin.', 34, 162, -33, 56, [0.012, 0.03, 0.64, 0.88])
    for (x, z, n) in ((61.2, 5.6, 1), (88.2, 5.6, 1), (107, 17.3, 2), (110.7, 25.7, 3), (62, 26.9, 4), (78, 26.9, 4), (45.5, 3.4, 5), (111, 3.4, 5)): num(ax, x, z, n, fs=16)
    num(ax, 131, -2, 7, fs=16); num(ax, 135, -5.8, 8, fs=16, col=GLASS); num(ax, 70, 30.9, 6, fs=13)
    txt(ax, 78, 12.4, 'GRAND HALL  72 x 30 = 2 160 m² (unchanged)', FG, 18, weight='bold'); txt(ax, 60, 17.3, 'island 1', FG, 14); txt(ax, 87, 17.3, 'island 2', FG, 14)
    txt(ax, 70, 41.1, 'SECURITY, same room, 7 lanes', FG, 13); txt(ax, 70, 50, 'airside lobby', DIM, 14); txt(ax, 102, 41, 'lounge', DIM, 15); txt(ax, 39.5, 37, 'toilets', DIM, 13)
    txt(ax, 128, 11, 'hold-bag\nscreening', DIM, 15); txt(ax, 145.8, 8, 'level 3', DIM, 12); txt(ax, 145.8, 19, 'early\nbags', DIM, 12); txt(ax, 132, 33, 'bag make-up (unchanged)', DIM, 15)
    txt(ax, 124, 46, 'cart lane', DIM, 14); txt(ax, 146, 46, 'staff\nentrance', DIM, 14); train_route(ax, 3); txt(ax, 150, 54.2, 'bag trains', '#ff8080', 13)
    txt(ax, 80, 20.8, 'bag tunnel under the floor', TUN, 13)
    for x in (46, 75, 104):
        for z in (6, 22): ax.add_patch(Circle((x, z), 0.6, fc='#e8eef5', zorder=15))
    dim(ax, 42, 114, -12.5, '72.0', fs=16); dim(ax, 114, 156, -12.5, '42.0 double skin', fs=15); dim(ax, 107, 156, -6.6 - 10, '49.0 link gallery', fs=14); dim(ax, 0, 30, 37.5, '30.0', vertical=True, fs=15)
    txt(ax, 82, -28, 'coach stands (7 existing, yellow)  ·  zebras (white dotted)', DIM, 14)
    legend(fig, 0.668, 0.875, [(1, 'Self-service kiosks, 2 rows of 6 (7.5 x 1.0 m each)'), (2, 'Bag drop: 6 machines, 12.0 x 3.5 m, island 2\'s east end'),
           (3, 'Airline service / ticket desk, 5.5 x 5.5 m'), (4, 'Boarding-pass e-gates, 6.0 x 1.1 m at each security door'), (5, 'Trolley bays, 5.0 x 1.2 m'),
           (6, 'Security: 7 automatic lanes, 5.78 m apart (sheet 2g)'), (7, 'Link gallery, 49.0 x 4.0 m: Grand Hall to East Hall, inside'),
           (8, 'Second glass skin at z -4, floor to the wave roof', GLASS)], lh=0.031, fs=16)
    fig.text(0.668, 0.60, 'CHECK-IN DESKS (to-do 210)', color=NEW, fontsize=17, weight='bold')
    fig.text(0.668, 0.58, '20 agents\' desks (2 islands, existing), each with the\nhand-over counter; they show CLOSED at T-45. Late\npassengers go to the service desk (3).', color=FG, fontsize=16, va='top', linespacing=1.4)
    fig.text(0.668, 0.47, 'THE DOUBLE SKIN, x 114-156', color=NEW, fontsize=17, weight='bold')
    fig.text(0.668, 0.45, 'Inner skin: today\'s wall at z 0 with the Floor 2 office\nwindows, kept. Outer skin: the Grand Hall\'s glass at z -4,\nup to the wave roof (12.5 - 19.3 m here). Between them a\n4 m space: the link gallery on the ground, open air above,\nclosed at Floor 2 against fire spread. The skin joins the\nEntrance East vestibule (x 95-107), which stays.',
             color=FG, fontsize=16, va='top', linespacing=1.4)
    fig.text(0.668, 0.255, 'CHECKED', color=NEW, fontsize=17, weight='bold')
    fig.text(0.668, 0.235, 'Nothing new stands within 4 m in front of the check-in\nislands, on the six columns, the security doors or the\nroller door of the cart lane (x 127.9-135.7, z 52).\nThe offices\' east windows (x 156) will look into the East\nHall instead of outdoors: they stay as inside windows.', color=FG, fontsize=16, va='top', linespacing=1.4)
    save(fig, 'TCv2-2b-Ground-Grand-Hall.png')

if 'c' in which:
    fig, ax = sec('SHEET 2c', 'GROUND FLOOR AFTER: EAST HALL, CUSTOMS, CLAIM EAST, PASSPORT CONTROL IN',
                  'Everything right of x 156 is new: grass under the roof today. Ceilings: 5.4 m where the new Floor 2 lies above (z 20-52), up to the wave roof in front of it.', 152, 376, -34, 84, [0.012, 0.185, 0.976, 0.745])
    E = EH; c = lambda b: ((b[0] + b[2]) / 2, (b[1] + b[3]) / 2)
    for k, n in (('checkin', 1), ('os', 2), ('cafe', 3), ('shops', 4), ('sim', 5), ('atm', 6), ('rental', 7), ('info', 8), ('firstaid', 9), ('rail', 10)): num(ax, *c(E[k]), n, fs=13)
    for k, n in (('wc_hall', 11), ('family', 12), ('trolley', 13), ('bagserv', 14), ('staff_lane', 15), ('lf_east', 18), ('wc_claim', 19), ('df_east', 20), ('os_east', 21), ('pc_office', 22), ('transfer_up', 25), ('second_line', 26)):
        num(ax, *c(G[k]), n, fs=13)
    num(ax, 232, 46, 16, fs=13, col=NS); num(ax, 261, 36, 17, fs=13, col=BELT); num(ax, 290, 36, 17, fs=13, col=BELT); num(ax, 318, 29, 23, fs=13, col=NS); num(ax, 318, 45, 23, fs=13, col=NS); num(ax, 348, 38.5, 24, fs=13)
    num(ax, 330, 65.6, 27, fs=13, col=NS); num(ax, 343, 58, 27, fs=13, col=NS); num(ax, 300, 54, 28, fs=13, col=BELT)
    for x in (246, 276, 306, 336, 366): txt(ax, x, 13.5, 'stair', OK, 11, rotation=90)
    txt(ax, 205, 20, 'EAST HALL\n72 x 40 = 2 880 m²', NEW, 17, weight='bold'); txt(ax, 205, 12, 'glass to the wave roof', GLASS, 13)
    txt(ax, 232, 15, 'CUSTOMS', NS, 12, rotation=90); txt(ax, 276, 43, 'CLAIM EAST\n76 x 46', NS, 14, weight='bold'); txt(ax, 232, 28, 'green', OK, 11); txt(ax, 232, 36.3, 'red', '#ff8080', 11)
    txt(ax, 349, 24, 'PASSPORT CONTROL IN\nhall 48 x 46', NS, 14, weight='bold'); txt(ax, 298, 3, 'LANDSIDE PROMENADE 242 x 6', NEW, 14)
    train_route(ax, 3); txt(ax, 200, 60, 'departure bag trains: stay clear of everything new', '#ff8080', 14)
    for g in (13,):
        a, d = core_f1(g); txt(ax, (a[0] + a[2]) / 2, 75, 'arrivals\nlobby G%d' % g, NS, 12)
    txt(ax, 262, 78, 'bag trains\npassage', '#ff8080', 11); txt(ax, 349, 73, 'ramp\nservices 14', DIM, 11)
    box(ax, 158, -24.9, 238, -21.6, '', fc='#3a3010', ec=NEW, lw=2.2, z=5); num(ax, 198, -23.2, 29, fs=14)
    txt(ax, 238, -30, '12 set-down bays (existing)', DIM, 13, ha='left'); txt(ax, 320, -23, 'kerb lane + pavement extended east of x 241', NEW, 13)
    dim(ax, 156, 228, -13, '72.0', fs=14); dim(ax, 228, 236, -13, '8', fs=13); dim(ax, 236, 312, -13, '76.0', fs=14); dim(ax, 312, 324, -13, '12', fs=13); dim(ax, 324, 372, -13, '48.0', fs=14)
    legend(fig, 0.012, 0.172, [(1, 'Long-haul check-in: 8 desks + 4 bag drop, 30 x 4'), (2, 'Oversize drop 4 x 4'), (3, 'Café 13 x 9'), (4, 'Shops 11 x 5'), (5, 'Kiosk + SIM 7 x 4'), (6, 'ATM + exchange 7 x 4'),
           (7, 'Car rental, 5 counters, 24 x 4.5'), (8, 'Info + PRM help point 10 x 6'), (9, 'First aid 10 x 6'), (10, 'Meeters\' rail 18.5 m'), (11, 'Toilets 18 x 12'), (12, 'Family + quiet 12 x 12'),
           (13, 'Trolley store 20 x 12'), (14, 'Bag service 18 x 12'), (15, 'Staff door kept: 4 m lane'), (16, 'Customs office 8 x 12', NS), (17, 'Wide-body belts, 2 of 8 x 28', BELT), (18, 'Lost and found 10.5 x 8'),
           (19, 'Toilets 11 x 11'), (20, 'Arrivals duty free 24 x 11'), (21, 'Oversize belt 11 x 9'), (22, 'Border police 12 x 14', NS), (23, '6 booths + 8 e-gates, 12 x 32', NS), (24, 'EES kiosks, 16'),
           (25, 'Up to transfers 12 x 11'), (26, 'Second-line checks 13 x 11', NS), (27, 'Closed arrivals corridor 4.5 m', NS), (28, 'Claim belt inputs (2)', BELT), (29, 'TAXI RANK EAST: 11 cabs, 80 m')],
           cols=5, cw=0.198, lh=0.0255, fs=14.5)
    save(fig, 'TCv2-2c-Ground-East-Hall.png')

if 'd' in which:
    fig, ax = sec('SHEET 2d', 'GROUND FLOOR AFTER: STAFF + OPERATIONS, COACH STATION, ARRIVALS CORRIDOR',
                  'The new floor of the concourse lies above z 20-52. The closed arrivals corridor runs under the gate walkway, beside the gates.', 366, 556, -40, 84, [0.012, 0.15, 0.976, 0.78])
    N = (('crew', 1, 'Crew centre'), ('canteen', 2, 'Staff canteen'), ('ground_off', 3, 'Ground handling +\nairline offices'), ('lockers', 4, 'Lockers +\nchanging'), ('staffsec', 5, 'STAFF SECURITY\n2 lanes, stair up, door\nto the baggage hall'), ('police', 6, 'Police +\nborder guard'))
    for k, n, t in N: b = G[k]; m = {'crew': 383, 'canteen': 414.5, 'ground_off': 445.5}.get(k, (b[0] + b[2]) / 2); txt(ax, m, (b[1] + b[3]) / 2 + 2, t, '#d0c8ff', 14); txt(ax, m, b[1] + 3.2, sz(b), DIM, 12)
    txt(ax, 516, 38, 'COACH + BUS STATION\n81 x 52 = 4 222 m²', NEW, 17, weight='bold'); txt(ax, 516, 28, 'waiting hall, ticket desks, boards, café, toilets', FG, 13)
    txt(ax, 420, 3, 'LANDSIDE PROMENADE, shops, glass front', NEW, 14); txt(ax, 400, 57.5, 'baggage hall: free for carts and ULDs (z 52-63.4)', DIM, 14); txt(ax, 455, 65.6, 'CLOSED ARRIVALS CORRIDOR 4.5 m, to passport control in', NS, 14)
    for g in (14, 15, 16):
        a, d = core_f1(g); txt(ax, (a[0] + a[2]) / 2, 75, 'arrivals\nlobby G%d' % g, NS, 13)
    for x, n in ((421.5, 15), (493.8, 16)): box(ax, x - 1.5, 63.4, x + 1.5, 67.9, '', fc='#101820', ec=STAFF, lw=2, z=9); txt(ax, x, 73, 'ramp\nservices %d' % n, DIM, 12)
    for x in (396, 426, 456, 486, 516, 546): txt(ax, x, 13.5, 'escape stair', OK, 11, rotation=90)
    for k in range(5): box(ax, 472 + 16 * k, -24.9, 485 + 16 * k, -21.6, '', fc='#2a2410', ec=NEW, lw=2, ls='--', z=5)
    txt(ax, 511, -27, '5 NEW coach bays 13 x 3.3 m (like your 7)', NEW, 14); txt(ax, 420, -19, 'pavement + kerb lane extended x 241-551', NEW, 14)
    dim(ax, 372, 470, -14, '98.0', fs=15); dim(ax, 470, 551.2, -14, '81.2', fs=15); dim(ax, 0, 52, 554, '52.0', vertical=True, fs=14); dim(ax, 291, 521, 70, '', fs=12) if False else None
    fig.text(0.014, 0.125, 'ESCAPE STAIRS (green): 11 of 1.8 m, 30 m apart, as v2 (0.6 m of stair per 60 people: 19.8 m serve 1 980). Their final form and the 35 m walking distance (VKF 16-15) are settled on sheet 9.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.095, 'STAFF CROSSINGS (violet squares in the corridor): the ramp rooms\' doors at x 349, 421 and 493 keep their way into the baggage hall through double doors that lock while passengers walk past.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.065, 'THE PILLARS (white dots): under the one smooth roof two under every bow, as at the Grand Hall (z -9): here x 326, 364, 416, 454, 506, 544. The old outer pillars moved 3 to 18 m.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.035, 'TODAY the forecourt pavement and kerb lane end at about x 241-340 (Data/roads.json): both must be lengthened to reach the coach station, at Chablais and in Iceland.', color=WARN, fontsize=15.5)
    save(fig, 'TCv2-2d-Ground-Coach-Station.png')

if 'e' in which:
    fig = sheet('SHEET 2e', 'THE FORECOURT: TAXI RANKS, COACH STANDS, PAY MACHINES', 'From Data/roads.json (the same at Chablais and in Iceland). Terminal at the top, car park at the bottom. Existing: grey, yellow outlines. New: solid yellow.')
    for k, (x0, x1, t) in enumerate(((-98, 160, 'WEST HALF'), (150, 556, 'EAST HALF'))):
        ax = ax_plan(fig, [0.012, 0.545 - k * 0.36, 0.976, 0.36], x0, x1, -76 if k == 0 else -80, 14 if k == 0 else 12)
        forecourt(ax, x0, x1); box(ax, -93, -90, 367, -54.45, '', fc='#1a1d21', ec='#4a545e', z=0); fills(ax, 1); after_f1(ax, lw=1.4); walls(ax, 1, lw=1.4); pillars(ax)
        if k == 0:
            box(ax, -75, -24.9, -5, -21.6, 'TAXI RANK WEST: 10 cabs, 70 m', fc=NEW, ec=NEW, col='#05080c', fs=15, z=5); box(ax, -46, -20.5, -34, -18, '', fc=NEW, ec=NEW, z=5); txt(ax, -29, -19.2, 'shelter + sign', NEW, 13, ha='left')
            txt(ax, 82, -18.8, '7 COACH STANDS (existing, 13 m, 3 m apart)', '#e8c800', 14); txt(ax, 0, -30, 'set-down and through lanes', DIM, 13); txt(ax, -55, -46, 'through road', DIM, 13); txt(ax, -55, -40.5, 'island', OK, 12)
            for x in (48.6, 74.8, 101.0): box(ax, x + 2.5, -58.5, x + 5.5, -56, '', fc=NEW, ec=NEW, z=6); txt(ax, x + 7, -57.2, '2 pay machines', NEW, 13, ha='left')
            txt(ax, 0, -66, 'CAR PARK (x -93 to 367)', FG, 16); txt(ax, 72, 7.2, 'GRAND HALL', FG, 15, weight='bold'); txt(ax, -42, 1.7, 'ARRIVALS HALL WEST', FG, 14)
            box(ax, -32.5, -2.8, -29, -0.8, '', fc=NEW, ec=NEW, z=8); txt(ax, -30.8, -7, '2 pay machines inside', NEW, 12)
            [txt(ax, x + 5.5, -35, 'zebra', '#ffffff', 12) for x in (48.6, 74.8, 101)]
        else:
            box(ax, 158, -24.9, 238, -21.6, 'TAXI RANK EAST: 11 cabs, 80 m', fc=NEW, ec=NEW, col='#05080c', fs=14, z=5); box(ax, 192, -20.5, 204, -18, '', fc=NEW, ec=NEW, z=5); txt(ax, 207, -19.2, 'shelter + sign', NEW, 13, ha='left')
            txt(ax, 195, -34, '12 set-down bays (existing)', DIM, 13); box(ax, 241, -21.5, 551, 0, '', fc='#2a2410', ec=NEW, ls='--', lw=2, z=0.5, alpha=0.5)
            txt(ax, 360, -13, 'NEW pavement, x 241-551 (21.5 m deep)', NEW, 15); box(ax, 241, -24.9, 470, -21.6, '', fc='none', ec=NEW, ls='--', lw=1.6, z=5); txt(ax, 355, -27.2, 'NEW kerb lane', NEW, 13)
            for i in range(5): box(ax, 472 + 16 * i, -24.9, 485 + 16 * i, -21.6, '', fc=NEW, ec=NEW, z=5)
            txt(ax, 511, -28, '5 NEW coach bays', NEW, 14); txt(ax, 192, 9.4, 'EAST HALL', NEW, 14, weight='bold'); txt(ax, 510, 6, 'COACH STATION', NEW, 14, weight='bold'); txt(ax, 250, -66, 'CAR PARK', FG, 16); txt(ax, 460, -66, 'car rental building (x 416-524) further south', DIM, 13)
            box(ax, 217, 1, 221, 3, '', fc=NEW, ec=NEW, z=8); txt(ax, 222, -4.5, '2 pay machines inside', NEW, 12)
    fig.text(0.014, 0.135, 'PAY MACHINES: 6 outdoors at the three zebras into the car park, 2 inside each arrivals hall (10 in all). Real practice: pay on foot before walking to the car; drivers then need a barrier at the vehicle exit.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.105, 'THE CAR PARK has no vehicle entry or exit in the data today: sheet 2f proposes one at its back, onto the road to the town (for your OK). Pay machines make sense only with it.', color=WARN, fontsize=15.5)
    fig.text(0.014, 0.075, 'TAXI RANKS use kerb that is empty today (x -80 to 17 and 154 to 241). The main rank should be the WEST one: every Schengen arrival comes out there, about 1 230 passengers in the busiest hour.', color=FG, fontsize=15.5)
    fig.text(0.014, 0.045, 'THE RAILWAY (later) and the road traffic (to-do 132) are not drawn. Lamps and drains along the kerb (roads.json) stay; the new bays keep clear of them when built.', color=DIM, fontsize=15.5)
    save(fig, 'TCv2-2e-Forecourt.png')
