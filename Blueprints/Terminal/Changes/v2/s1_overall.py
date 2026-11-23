# Sheet 1a / 1b: the whole terminal BEFORE / AFTER (both floors), the area table and what changes overall.
from v2_common import *
X0, X1, Z0, Z1 = -248, 560, -17, 96


def plan(fig, y, title, col, floor, after):
    ax = ax_plan(fig, [0.012, y, 0.976, 0.196], X0, X1, Z0, Z1)
    fills(ax, floor); gates(ax, floor, fs=11)
    if after: (after_f1 if floor == 1 else after_f2)(ax, lw=1.2)
    walls(ax, floor, lw=0.9)
    fig.text(0.012, y + 0.2, title, color=col, fontsize=17, weight='bold', va='bottom')
    return ax


fig = sheet('SHEET 1a', 'THE WHOLE TERMINAL, BEFORE AND AFTER',
            'White and light-blue lines: the building as it really is in the game (traced walls). Yellow: new. Pink: non-Schengen. Teal: transfers. '
            'Forecourt at the bottom, aircraft at the top. No gate, stand or jetway moves.')
A = plan(fig, 0.712, 'GROUND FLOOR, BEFORE', FG, 1, False)
box(A, 156, -7, 551.2, 52, '', fc='none', ec=WARN, ls='--', lw=2)
txt(A, 353, 30, 'OPEN GRASS UNDER THE WAVE ROOF: no floor, no walls, no glass today', WARN, 16, weight='bold')
txt(A, 353, 12, 'eight tree pillars stand here already', DIM, 14)
txt(A, -160, 30, 'empty', DIM, 13); txt(A, -40, 30, 'CLAIM WEST', FG, 13); txt(A, 21, 15, 'ESCALATORS', FG, 12); txt(A, 78, 9, 'GRAND HALL', FG, 14, weight='bold')
txt(A, 70, 36, 'SECURITY', FG, 12); txt(A, 135, 30, 'BAGS', FG, 13); txt(A, -60, 60, 'baggage hall', DIM, 12); train_route(A, 1.8)
txt(A, 200, 62.5, 'departure bag trains', '#ff8080', 12)
B = plan(fig, 0.480, 'GROUND FLOOR, AFTER', NEW, 1, True); train_route(B, 1.8)
txt(B, -160, 30, 'empty', DIM, 13); txt(B, 21, 15, 'ESCALATORS', FG, 12); txt(B, 78, 9, 'GRAND HALL', FG, 14, weight='bold'); txt(B, 70, 36, 'SECURITY', FG, 12)
txt(B, 135, 30, 'BAGS', FG, 13); txt(B, -40, 47, 'duty free', NEW, 12); txt(B, 192, 15, 'EAST HALL', NEW, 15, weight='bold')
txt(B, 270, 35.5, 'CLAIM\nEAST', NS, 12, weight='bold'); txt(B, 349, 24, 'PASSPORT\nCONTROL IN', NS, 12, weight='bold'); txt(B, 421, 35, 'STAFF + OPERATIONS', STAFF, 14, weight='bold')
txt(B, 510, 30, 'COACH + BUS\nSTATION', NEW, 14, weight='bold'); txt(B, 420, 58, 'closed arrivals corridor above this line', NS, 12)
txt(B, 350, -12, 'glass front to the wave roof all the way', GLASS, 13); txt(B, 131, -10, 'link gallery', NEW, 12)
C = plan(fig, 0.248, 'FLOOR 2, BEFORE', FG, 2, False)
box(C, 156, 0, 551.2, 52, '', fc='none', ec=WARN, ls='--', lw=2)
txt(C, 353, 25, 'NOTHING HERE ON FLOOR 2 (open air under the roof)', WARN, 16, weight='bold')
txt(C, -180, 30, 'TAX FREE', FG, 14); txt(C, -99, 25, 'esc. hall', DIM, 12); txt(C, 36, 37, 'FOOD', FG, 12); txt(C, 93, 41, 'BAR', FG, 13); txt(C, 136, 30, 'OFFICES', FG, 13)
txt(C, 10, -6, 'toilets', BELT, 12); txt(C, -40, 60, 'gate walkway', DIM, 12)
D = plan(fig, 0.016, 'FLOOR 2, AFTER', NEW, 2, True)
txt(D, -180, 30, 'TAX FREE', FG, 14); txt(D, -99, 25, 'esc. hall', DIM, 12); txt(D, 36, 37, 'FOOD', FG, 12); txt(D, 93, 41, 'BAR', FG, 13); txt(D, 136, 30, 'OFFICES', FG, 13)
txt(D, 10, -6, 'toilets', BELT, 12); txt(D, 192, 25, 'open: the East Hall\nseen from above', DIM, 13); txt(D, 274, 38, 'DUTY\nFREE', DF, 13, weight='bold')
txt(D, 323, 38, 'TRANSFER\nCENTRE', TRF, 12, weight='bold'); txt(D, 370, 38, 'FOOD', NEW, 14, weight='bold'); txt(D, 412, 45, 'WC', BELT, 12)
txt(D, 448, 38, 'LOUNGE', NEW, 14, weight='bold'); txt(D, 496, 38, 'SEATING', NEW, 14, weight='bold'); txt(D, 536, 31, 'SHOPS', NEW, 11)
txt(D, 420, 60, 'non-Schengen walkway (the same walkway)', NS, 13); txt(D, 205, 8, 'passport\ncontrol out', NS, 12); txt(D, 225, 90, 'swing lobby', NEW, 12)
txt(D, 430, 8, 'NON-SCHENGEN CONCOURSE (new floor, 323 x 32 m)', NEW, 15, weight='bold')
save(fig, 'TCv2-1a-Overall-Before-After.png')

# ---------------------------------------------------------------------------------------------- 1b the table
fig = sheet('SHEET 1b', 'AREA TABLE, AND WHAT CHANGES OVERALL', 'Every size measured from the drawings on the other sheets. Areas are floor areas in m².')
g = lambda *k: round(sum(area(G[n]) for n in k)); f = lambda *k: round(sum(area(F[n]) for n in k))
rows = [('GRAND HALL', 2160, 2160, 'exactly the same; kiosks, bag drop and desk cover 112 m², none of it in the queues'),
        ('Landside halls, all', 2160 + 960, 2160 + 960 + g('link', 'east_hall', 'promenade', 'coach'), '+ link gallery 196, East Hall 2 880, promenade 1 452, coach station 4 222'),
        ('Check-in + bag-drop positions', 20, 20 + 6 + 8 + 4, '20 desks + 6 bag drop (Grand Hall) + 8 desks + 4 bag drop (East Hall); plus 12 kiosks'),
        ('Security lanes', 6, 7, 'the 7th lane fits in the same room: lanes 5.78 m apart instead of 6.17 (sheet 2g)'),
        ('Claim halls', 2674, 2674 + g('claim'), 'the west hall unchanged; a new non-Schengen hall of 3 496 with 2 wide-body belts'),
        ('Arrivals duty free', 0, 323 + 264, 'west claim hall 323, east claim hall 264 (both before customs)'),
        ('Passport control (in + out)', 0, g('pc_line', 'pc_hall', 'pc_office') + f('queue', 'line'), 'in: 2 760 on the ground floor; out: 700 on Floor 2'),
        ('Departure shops, Schengen side', 5368 + 192, 5368 + 192, 'your big tax free (5 368) and the small one by the food walk (192): enough, unchanged'),
        ('Departure shops, non-Schengen side', 0, f('dutyfree', 'shop1', 'shops2'), 'walk-through duty free 896 + shops 591'),
        ('Airside food, bar, lounge, seating: east', 0, f('food', 'lounge', 'seating', 'prayer'), 'food court + bar 1 568, lounge 1 232, seating 1 344, prayer / quiet 312; plus transfer centre 924'),
        ('Toilet blocks (public)', 3, 10, 'new: west claim, East Hall, east claim, coach station, 3 on the concourse'),
        ('Gate + view bays 13-16, each', 952, 952 - 204, '204 m² becomes the jetway vestibule and the closed way down; still 1.4 - 2.0 x the need'),
        ('Staff, crew, operations, police', 240, 240 + g('crew', 'canteen', 'ground_off', 'lockers', 'staffsec', 'police'), 'six new rooms in the east wing, with their own staff security'),
        ('East wing: ground floor in use', 0, 20540, 'all of it; today grass under the roof'),
        ('East wing: Floor 2', 0, f('slab'), 'a new floor at +6.0, x 228-551, z 20-52')]
y0 = 0.885
for x, t, c in ((0.016, 'AREA', NEW), (0.262, 'BEFORE', NEW), (0.318, 'AFTER', NEW), (0.372, 'WHAT CHANGES', NEW)): fig.text(x, y0, t, color=c, fontsize=17, weight='bold')
for i, (a, b, c, d) in enumerate(rows):
    y = y0 - 0.034 - i * 0.0335
    fig.text(0.016, y, a, color=FG, fontsize=17, weight='bold' if i == 0 else 'normal')
    fig.text(0.295, y, '{:,}'.format(b).replace(',', ' '), color=DIM, fontsize=17, ha='right')
    fig.text(0.352, y, '{:,}'.format(c).replace(',', ' '), color=OK if c > b else (FG if c == b else WARN), fontsize=17, weight='bold', ha='right')
    fig.text(0.372, y, d, color=DIM, fontsize=15.5)
fig.text(0.016, 0.325, 'HOW THE AIRPORT CHANGES OVERALL', color=NEW, fontsize=19, weight='bold')
L = ['The empty grass under the east roof becomes a real two-floor wing. The roof is there already and becomes one smooth wave; floors, glass, walls and rooms are added.',
     'From the forecourt: one glass front from the Grand Hall to the east end, up to the wave roof. The offices at x 114-156 keep their windows behind a second glass skin.',
     'Non-Schengen (UK, Ireland, long-haul) gets its own world: passport control out, walk-through duty free, food, lounge, toilets, prayer room, gates 13-16.',
     'Arriving non-Schengen passengers never meet departing ones: they go DOWN at their own gate and walk a closed corridor below to passport control.',
     'Gates 4 and 13 become swing gates: either side can be opened for them, never both.',
     'West arrivals get duty free, toilets, lost and found, cash and SIM; the forecourt gets two taxi ranks and pay machines.',
     'Nothing moves: Grand Hall, security room (now 7 lanes), gates, stands, jetways, offices, the bag system and the departure bag trains stay as they are.']
for i, t in enumerate(L):
    fig.text(0.018, 0.287 - i * 0.0335, '•', color=NEW, fontsize=18); fig.text(0.028, 0.287 - i * 0.0335, t, color=FG, fontsize=16.5)
fig.text(0.016, 0.035, 'TERMINAL EXTENSION PAST x 551: NOT NEEDED (sheet 6).', color=OK, fontsize=19, weight='bold')
save(fig, 'TCv2-1b-Area-Table.png')
