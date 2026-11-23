# Sheet 2g: the 7th security lane - the same room (x 50-90, z 30-42), before / after, every size from the game's layout.
from v2_common import *
fig = sheet('SHEET 2g', 'THE 7TH SECURITY LANE', 'It fits in the same room: the lanes move closer together by 39 cm. No wall, door, toilet or lobby changes. Sizes measured from the game\'s own lane models.')
for k, n in enumerate((6, 7)):
    ax = ax_plan(fig, [0.012, 0.525 - k * 0.435, 0.70, 0.39], 46, 94, 25.5, 48.5)
    box(ax, 42, 0, 114, 30, '', fc='#1b2733', ec='none', z=1); box(ax, 50, 30, 90, 42, '', fc='#182a1d', ec='none', z=1); box(ax, 50, 42, 90, 52, '', fc='#0f2018', ec='none', z=1)
    box(ax, 90, 30, 114, 52, '', fc='#0f2018', ec='none', z=1); box(ax, 43, 30, 50, 44, '', fc='#131a20', ec='none', z=1)
    ax.v21 = n == 7
    sec_lanes(ax, n, lw=2, fc='#3a3410' if n == 7 else '#232b33', ec=NEW if n == 7 else '#aab8c4'); walls(ax, 1, lw=2.6)
    X = LANE_BELT_X if n == 7 else [53.0, 59.2, 65.3, 71.5, 77.7, 83.8]
    for i, x in enumerate(X): num(ax, x, 38.4, i + 1, NEW if n == 7 else '#aab8c4', fs=17)
    fig.text(0.014, 0.925 - k * 0.435, 'AFTER: 7 lanes' if n == 7 else 'TODAY: 6 lanes', color=NEW if n == 7 else FG, fontsize=20, weight='bold', va='top')
    txt(ax, 70, 45.2, 'airside lobby (10 m deep, unchanged)', FG, 15); txt(ax, 70, 27, 'Grand Hall', FG, 15); txt(ax, 92.2, 36, 'lounge', DIM, 13, rotation=90); txt(ax, 48, 36, 'toilets', DIM, 13, rotation=90)
    txt(ax, 70, 43.2, 'exit 10 m', GLASS, 13); txt(ax, 62, 28.8, 'entrance 6 m', GLASS, 13); txt(ax, 78, 28.8, 'entrance 6 m', GLASS, 13)
    dim(ax, 50, 90, 41.2, '40.0', fs=14); dim(ax, X[0] - 1.5, X[0] + 2.3, 33, '3.8', fs=13); dim(ax, X[2], X[3], 33.2, '%.2f' % (X[3] - X[2]), fs=14)
    dim(ax, X[0] + 2.3, X[1] - 1.5, 37.3, '%.1f' % (X[1] - X[0] - 3.8), fs=13); dim(ax, X[-1] + 2.3, 90, 37.3, '%.1f' % (90 - X[-1] - 2.3), fs=12); dim(ax, 30, 32, 69 if n == 6 else 66.5, '2.0', vertical=True, fs=12); dim(ax, 32, 40, 91, '8.0', vertical=True, fs=13) if False else None
    dim(ax, 40, 42, 79 if n == 6 else 78, '2.0', vertical=True, fs=12)
fig.text(0.73, 0.925, 'WHAT CHANGES', color=NEW, fontsize=20, weight='bold', va='top')
fig.text(0.73, 0.89, '• All lanes shift so the gaps are equal: 5.78 m apart\n   instead of 6.17 m.\n• Free width between two lanes: 2.0 m instead of 2.4 m\n   (the officer\'s chair 0.9 m, the rest free).\n• Lane 1\'s officer keeps 0.9 m to the west wall; lane 7\n   ends 0.5 m from the east wall.\n• The same lane model, 3.8 m wide with its arch and\n   the officer\'s desk, 8.0 m long.', color=FG, fontsize=17, va='top', linespacing=1.5)
fig.text(0.73, 0.655, 'WHAT STAYS', color=NEW, fontsize=20, weight='bold', va='top')
fig.text(0.73, 0.62, '• The walls, both 6 m entrances, the 10 m exit.\n• 2.0 m before the lanes (z 30-32) and behind them\n   (z 40-42).\n• The queue in the Grand Hall, the airside lobby, the\n   lounge door (x 90, z 47) and the toilets.', color=FG, fontsize=17, va='top', linespacing=1.5)
fig.text(0.73, 0.455, 'BELT MIDDLES FOR THE BUILDER', color=NEW, fontsize=20, weight='bold', va='top')
fig.text(0.73, 0.42, 'today  53.0  59.2  65.3  71.5  77.7  83.8\nnew    52.5  58.28  64.07  69.85  75.63  81.42  87.2\nThe passengers\' lane list (PaxFlow PXLANES) and its\n"6 lanes" loops change with them.', color=FG, fontsize=17, va='top', linespacing=1.5)
fig.text(0.73, 0.27, 'CAPACITY', color=NEW, fontsize=20, weight='bold', va='top')
fig.text(0.73, 0.235, '7 x 280 = 1 960 an hour for 1 686 people: 116 %.\nHonest limit: real automatic lanes are longer than 8 m\n(tray return, several people undressing at once). If a\nlane makes only 240 an hour, 7 lanes give 1 680:\nthat is exactly 100 %. Six lanes would give 85 %.', color=FG, fontsize=17, va='top', linespacing=1.5)
save(fig, 'TCv2-2g-Security-7-Lanes.png')
