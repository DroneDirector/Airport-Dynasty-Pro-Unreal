# Sheet 2f: the car park's entry and exit (today: none) - onto the road behind the car park (AIRPORT-BACK-TO-TOWN), two
# options for the junction, why not at the front. Positions from Data/roads.json (the same lot and roads at both sites).
from v2_common import *
R = json.load(open(os.path.join(ROOT, 'Data', 'roads.json'))); LOT = R['lots'][0]
fig = sheet('SHEET 2f', 'THE CAR PARK\'S ENTRY AND EXIT', 'NEW, for your OK. Today the 3 982 spaces have no way in or out for cars. Terminal at the top. The car park is the same at Chablais and in Iceland.',
            foot='NOT approved yet: for the owner\'s OK', ver='v2.1  ·  FOR YOUR OK')
JX = 120.0


def lot(ax, detail=False):
    box(ax, LOT['x0'], LOT['y0'], LOT['x1'], LOT['y1'], '', fc='#1a1d21', ec='#6a7480', z=1)
    for r in LOT['rows']:
        a, b = sorted((r['y'], r['y'] + r['dir'] * r['d']))
        box(ax, r['x0'], a, r['x1'], b, '', fc='#262c33', ec='none', z=2)
        if detail:
            x = r['x0']
            while x <= r['x1'] + 0.01: ax.plot([x, x], [a, b], color='#55606c', lw=0.8, zorder=3); x += r['w']


def road_back(ax, x0=-580, x1=420, cut=None):
    for z0, z1, c in ((-259.25, -258.5, '#2b333b'), (-258.5, -251.5, '#20262d'), (-251.5, -250.75, '#2b333b')):
        box(ax, x0, z0, x1, z1, '', fc=c, ec='none', z=1)
    ax.plot([x0, x1], [-255, -255], color='#e8e05a', lw=1.2, ls=(0, (6, 6)), zorder=3)


def cuts(ax, walk=True):
    for x0, x1 in ((-93, -87.5), (361.5, 367), (115, 122.5)): box(ax, x0, -247, x1, -54.45, '', fc='#123230', ec=TRF, lw=1.4, z=4)
    if walk:
        for x in (48.75, 73.75, 101.25): box(ax, x - 1.25, -235.95, x + 1.25, -54.45, '', fc='#3a3410', ec=NEW, lw=0.8, z=4)
    box(ax, 90, -247, 160, -225.95, '', fc='#2a2410', ec=NEW, lw=2, z=5)


# ---- the overview
ax = ax_plan(fig, [0.010, 0.515, 0.66, 0.405], -112, 384, -292, -14)
for z0, z1, c in ((-25, -21.5, '#1e242b'), (-38.75, -25, '#171c22'), (-42.25, -38.75, '#0f2016'), (-51.25, -42.25, '#20262d'), (-54.25, -51.25, '#3a3f45')): box(ax, -112, z0, 384, z1, '', fc=c, ec='none', z=1)
lot(ax); road_back(ax, -112, 384); cuts(ax)
box(ax, -80, -21.5, 241, -14, '', fc='#12161a', ec='none')
for x in (48.6, 74.8, 101.0):
    box(ax, x - 1.5, -54, x + 1.5, -21.5, '', fc='none', ec='#ffffff', lw=1.8, ls=':', z=6); box(ax, x + 2.2, -59, x + 6, -55.5, '', fc=NEW, ec=NEW, z=7)
for y0 in (-250,):
    route(ax, [(330, -253.2), (123.5, -253.2), (123.5, -232)], SCH, 4); route(ax, [(114, -226), (114, -253.2), (30, -253.2)], ARRS, 4); route(ax, [(116.5, -240), (116.5, -256.8), (330, -256.8)], ARRS, 4)
route(ax, [(101, -120), (101, -60)], '#ffffff', 2.5); route(ax, [(118.7, -225), (118.7, -140), (200, -140)], SCH, 2.5); route(ax, [(250, -107), (364.2, -107), (364.2, -241), (127, -241)], ARRS, 2.5)
txt(ax, 255, -150, 'CAR PARK (22 rows)', FG, 18, weight='bold'); txt(ax, 225, -32, 'forecourt: taxis, coaches, set-down', DIM, 14); txt(ax, -40, -46.5, 'through road', DIM, 13); txt(ax, 150, -17.5, 'TERMINAL', FG, 15)
txt(ax, 240, -266, 'ROAD TO THE TOWN (behind the car park), straight for 895 m', FG, 15); txt(ax, -60, -266, 'to the roundabout', DIM, 14); txt(ax, 345, -277, 'to the town', DIM, 14); txt(ax, 125, -236.5, 'PLAZA', NEW, 13, weight='bold', bbox=dict(fc='#2a2410', ec='none', pad=1))
txt(ax, 141, -57.3, 'pay machines', NEW, 13); txt(ax, 24, -150, 'footways\nto the zebras', NEW, 13); txt(ax, 147, -190, 'new middle\nlane 7.5 m', TRF, 13); txt(ax, -72, -190, 'new end\nlane', TRF, 13); txt(ax, 342, -190, 'new end\nlane', TRF, 13)


# ---- the two junctions
def plaza(ax):
    box(ax, 90, -247, 160, -225.95, '', fc='#24211a', ec=NEW, lw=2, z=4)
    for x0, c in ((109, ARRS), (112.8, ARRS), (120.2, SCH), (124, SCH)): box(ax, x0, -250.75, x0 + 3.5, -226, '', fc='#1c2229', ec='none', z=5)
    for x0 in (112.5, 116.3, 119.2, 123.7, 127.5): box(ax, x0, -241 if x0 < 117.5 else -236, x0 + (0.3 if x0 not in (116.3,) else 2.9), -230, '', fc='#59636d', ec='#aab8c4', lw=1, z=7)
    for x0 in (109, 112.8): ax.plot([x0, x0 + 3.4], [-236.5, -236.5], color='#ff5050', lw=5, zorder=9)
    for x0 in (120.2, 124): ax.plot([x0, x0 + 3.4], [-231.5, -231.5], color='#ff5050', lw=5, zorder=9)
    for x0 in (109, 112.8):
        for z in (-244.0, -249.4): box(ax, x0 + 0.8, z - 2.2, x0 + 2.6, z + 2.2, '', fc='#3d4b57', ec='#8fa0ae', lw=1, z=8)
    for x0 in (120.2, 124):
        for z in (-234.6, -240.2, -245.8): box(ax, x0 + 0.8, z - 2.2, x0 + 2.6, z + 2.2, '', fc='#3d4b57', ec='#8fa0ae', lw=1, z=8)
    txt(ax, 112.5, -228, 'EXIT 2', ARRS, 14, weight='bold'); txt(ax, 123.9, -228, 'ENTRY 2', SCH, 14, weight='bold')


for k, (t, x) in enumerate((('OPTION 1 (recommended): a give-way junction', 0.010), ('OPTION 2: a roundabout like yours (42 m)', 0.343))):
    ax = ax_plan(fig, [x, 0.035, 0.325, 0.435], 72, 168, -297 if k else -290, -214)
    lot(ax, True); plaza(ax)
    box(ax, 115, -225.95, 122.5, -214, '', fc='#123230', ec=TRF, lw=1.4, z=4)
    if k == 0:
        road_back(ax, 60, 180); box(ax, 107.5, -251, 129, -246.5, '', fc='#1c2229', ec='none', z=4.5)
        for i in range(8): ax.add_patch(Polygon([(109.2 + i * 1.0, -249.9), (109.9 + i * 1.0, -249.9), (109.55 + i * 1.0, -250.6)], fc='#ffffff', zorder=10))
        ax.add_patch(Polygon([(112.5, -246), (72, -253.2), (168, -257)], fc=NEW, ec=NEW, alpha=0.10, zorder=3)); ax.plot([112.5, 72], [-246, -253.2], color=NEW, lw=1.5, ls='--', zorder=11); ax.plot([112.5, 168], [-246, -257], color=NEW, lw=1.5, ls='--', zorder=11)
        route(ax, [(165, -253.2), (126, -253.2), (126, -248)], SCH, 4); route(ax, [(110.7, -247), (110.7, -253.2), (76, -253.2)], ARRS, 4); route(ax, [(114.5, -247), (114.5, -256.8), (165, -256.8)], ARRS, 4)
        txt(ax, 88, -263.5, 'sight line: 140 m each way\nfrom 5 m back (80 km/h)', NEW, 13); txt(ax, 143, -263.5, 'white give-way\nteeth', FG, 13); dim(ax, 108.5, 127.8, -269.5, '19.3 mouth', fs=13); dim(ax, -250.75, -231.5, 131.5, '19 = 3 cars', vertical=True, fs=12)
        txt(ax, 140, -243, 'from the town\nyou turn right:\nnobody to cross', DIM, 13); txt(ax, 120, -281, 'nothing south of the road is touched', DIM, 14)
    else:
        box(ax, 60, -259.25, 180, -250.75, '', fc='#20262d', ec='none', z=1); cx, cz = 118.5, -272.0
        ax.add_patch(Polygon([(60, -259.25), (60, -250.75), (100, -259.5), (99, -268)], fc='#20262d', zorder=2)); ax.add_patch(Polygon([(180, -259.25), (180, -250.75), (137, -259.5), (138, -268)], fc='#20262d', zorder=2))
        for r, c in ((21, '#20262d'), (10.0, '#8a8f94'), (8.5, '#1d3a22')): ax.add_patch(Circle((cx, cz), r, fc=c, ec='#aab8c4', lw=1, zorder=5))
        box(ax, 108.5, -253.5, 128, -246.5, '', fc='#1c2229', ec='none', z=4.5); txt(ax, cx, cz, 'island', OK, 13)
        route(ax, [(165, -256), (138, -264), (134.5, -270)], SCH, 4); route(ax, [(133, -263), (126, -256), (125.5, -248)], SCH, 4); route(ax, [(112, -247), (111.5, -255), (104, -264)], ARRS, 4); route(ax, [(103, -268), (76, -258)], ARRS, 4)
        txt(ax, 86, -283, 'needs 35 m of land\nsouth of the road', WARN, 13); txt(ax, 152, -283, 'slows everyone,\nno left turns', FG, 13); dim(ax, 97.5, 139.5, -295, '42.0', fs=13)
    fig.text(x + 0.004, 0.478, t, color=NEW if k == 0 else FG, fontsize=18, weight='bold')
# ---- the words
T = [('THE PLAN', NEW, ['In and out at the BACK, onto the road to the town, at x 120.', '2 entry + 2 exit lanes with barriers (red). Pay on foot at 10', 'machines before walking to the car; the ticket opens the exit.',
                        'Inside: end lanes 5.5 m, a middle lane 7.5 m (today\'s end', 'strips are 3 m: too narrow), three footways to the zebras.', '3 982 spaces become 3 758 (-224, that is 6 %).']),
     ('WHY NOT AT THE FRONT', WARN, ['The forecourt road has a solid double line: exit to the right', 'only. Right is east, and at CHABLAIS the road ends there', '(x 1856): the town road there exists only in Iceland.',
                                      'It would also cross the pavement and meet every taxi and coach.']),
     ('THE NUMBERS', NEW, ['Busiest hour, a quarter by own car, 1.4 a car: 301 cars an hour', 'each way. 2 lanes: 580 in, 680 out (290 and 340 a lane, Wuppertal', 'University). Queue past the 6 places at the entry: 0.4 % of the time.']),
     ('THE RULES', NEW, ['Leaving a car park you give way to everyone (VRV art. 15.3).', 'Sight at 80 km/h: 110-140 m, seen from 5 m back (SN 640 273):', 'here 680 m west, 215 m east to the bend. No trees in the way.',
                         'A permit is needed: canton or commune (Vaud LRou art. 32):', 'only if traffic stays fluid and safe.', 'No pavement at the back: cars and walkers never cross.']),
     ('FOUND ON THE WAY', WARN, ['At Chablais the forecourt\'s one-way lanes start at that dead end:', 'no car can reach them. A link from the town road is needed', '(for the traffic job). Swiss paint: centre lines white, zebras', 'yellow; the game has it the other way round.'])]
y = 0.918
for h, c, L in T:
    fig.text(0.682, y, h, color=c, fontsize=18, weight='bold', va='top'); y -= 0.033
    for l in L: fig.text(0.682, y, l, color=FG, fontsize=15.5, va='top'); y -= 0.0262
    y -= 0.017
fig.text(0.682, y - 0.002, 'MY PICK: OPTION 1. Option 2 only if the traffic job later\nbrings many cars on this road.', color=OK, fontsize=17, weight='bold', va='top')
save(fig, 'TCv2-2f-Car-Park-Exit.png')
