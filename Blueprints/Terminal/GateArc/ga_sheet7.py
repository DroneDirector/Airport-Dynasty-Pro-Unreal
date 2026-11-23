# GA-7-Clashes-Other-Gates.png: every clearance at gate 9, and what is different at the other 15 gates (in words).
from ga_common import *
fig = sheet('GA-7', 'CLASHES AT GATE 9, AND THE OTHER 15 GATES', 'Checked by numbers on the game\'s own data (airfield.json, Airfield.cpp, JetwayRigs, LightMasts.cpp). Blueprint v2\'s gate 4 gallery is on hold, so it is not drawn.')
fig.text(0.02, 0.895, 'GATE 9: CLEARANCES OF THE 3 M CURVE', color=NEW, fontsize=20, weight='bold', va='center')
R = [('Corridor columns (z 85.85), from the floor edge', '1.07 m', OK), ('Jetway tunnel anywhere in its swing (-41° to +11°)', '10.9 m', OK), ('Rotunda roof', '9.3 m', OK), ('A320 nose on its stand (z 120)', '35.4 m', OK),
     ('Tug box, nearest corner (beside the gate, not under it)', '5.8 m', OK), ('GPU box', '32.8 m', OK), ('Height under the floor edge (nothing drives there: red zone)', '5.40 m', OK), ('Ramp services door (3 m wide, 2.7 m high)', 'free', OK),
     ('Apron light masts (gates 1-4 only; none at gate 9)', '6.9 m +', OK), ('View bay 9\'s flat window: starts 2.16 m after the post', 'untouched', OK), ('Camera 78 of the CCTV plan (inside corner x -233, z 81)', 'stays', OK), ('Stair and lift: room to the glass at the stair, today 1.34', '2.1 m', OK),
     ('Service road: behind the aircraft tails (z 168), not here', 'no clash', OK)]
for i, (a, b, c) in enumerate(R):
    y = 0.862 - i * 0.0292; fig.text(0.02, y, a, color=FG, fontsize=15.5, va='center'); fig.text(0.452, y, b, color=c, fontsize=16, weight='bold', va='center', ha='right')
fig.text(0.02, 0.462, 'WHEN IT IS BUILT, THESE PARTS OF THE GAME MUST KNOW', color=NEW, fontsize=20, weight='bold', va='center')
T = ['The gate\'s room box (z 68-82) grows to z 85: passengers waiting, staff and the', 'seat search count anyone at z 82-85 as outside the gate today.',
     'Passenger paths and the holomap\'s walls: the curve and the glass entrance.', 'The new seats join the seat list; rain, sound and reflections see the new roof.',
     'Walking test: the gate 1 stress route passes the jetway door at z 83.6; it still fits.', 'Gate 1-8 windows are in the first model, 9-16 were copied: one curved', 'piece, placed 10 times (instanced, Nanite), replaces each flat window.',
     'Ceiling lamps follow the curve (material glow only, as today\'s).']
for i, s in enumerate(T): fig.text(0.02, 0.430 - i * 0.0272, s, color=FG, fontsize=15.5, va='center')
fig.text(0.02, 0.195, 'NOT CHECKED', color=WARN, fontsize=20, weight='bold', va='center')
for i, s in enumerate(['In the game itself (walking, frame rate, real sun): only drawn and rendered in Blender.', 'Structure: the 3 m cantilever is drawn as a 0.59 m slab, not calculated.', 'The jetways\' swing is his generator\'s own limit (0.45 rad), not a driven test.']): fig.text(0.02, 0.163 - i * 0.0272, s, color=FG, fontsize=15.5, va='center')
fig.text(0.50, 0.895, 'THE OTHER 15 GATES', color=NEW, fontsize=20, weight='bold', va='center')
G = [('1, 2, 3, 11', 'The same as gate 9: same room, door, lift and stair, shifted along x. The same curved piece fits\nunchanged (all measured). Apron masts 6.9 - 7.6 m away, tug boxes 4.1 - 5.2 m.', OK),
     ('4', 'The same curve fits (tug box 3.8 m away). Blueprint v2 hangs gate 4\'s arrivals gallery outside the windows (x 194.5 - 303,\nz 82.2 - 84.8): it would cut the curve\'s last 6.4 m (to z 83.9). On hold; see below.', WARN),
     ('5, 6, 7, 8, 10, 12', 'Ground-floor gates: no floor 2 window of their own. Their glass is on the ground floor, under the\nview bays, with the boarding doors in it. Left flat, like the view bays above. Your call.', DIM),
     ('13, 14, 15, 16', 'Their jetways start at the VIEW BAYS, so the gate windows have no corridor: the full curve, no collar,\n20 plain mullions. The old door at x +22 leads to nothing: it disappears with the flat window.\nNoses stand at z 95: 13.0 m to the curve. The B777 / A380 stand centre is on the gate / view bay wall.', OK),
     ('13', 'v2\'s gallery runs along its whole front: it would cross the curve from end to end. On hold; see below.\nThe bag-train roller door (x 267.4 - 271.4) stays free below: 5.40 m.', WARN)]
y = 0.858
for n, s, c in G:
    h = s.count('\n') + 1; fig.text(0.50, y, n, color=c if c != DIM else FG, fontsize=17, weight='bold', va='top'); fig.text(0.597, y, s, color=FG, fontsize=15, va='top', linespacing=1.35); y -= 0.0255 * h + 0.026
fig.text(0.50, 0.452, 'THE GALLERY OF GATES 4 AND 13 (if it comes back)', color=NEW, fontsize=20, weight='bold', va='center')
S = ['A  Recommended: it follows the curve on the outside, a ring of the same width (2.6 m) round gate 13\'s', '     and gate 4\'s curves, straight along view bay 4 and the swing lobby. Furthest point z 87.8; the A380\'s', '     nose stays 10.2 m away, as approved; 5.40 m clear for the bag trains.',
     'B  Only gate 13 swings (a question of v2\'s summary): no gallery, every curve is free.', 'C  Inside the curve: does not work. At the two ends the bulge is thinner than the 2.6 m gallery, and the', '     lift stands in the way.']
for i, s in enumerate(S): fig.text(0.50, 0.420 - i * 0.0272, s, color=FG, fontsize=15.5, va='center')
fig.text(0.50, 0.222, 'THE CURVE IN NUMBERS', color=NEW, fontsize=20, weight='bold', va='center')
for i, s in enumerate(['Chord 31.84 · depth 3.00 · radius 43.74 · arc 32.59 · end corners 21.3°', '19 curved panes of 1.715 x 5.4 m (8.4 mm bow) · 20 radial mullions', 'Floor + 64.1 m² (55.1 without the corridor) per gate · seats 111 → 135', 'Ten gates (1, 2, 3, 4, 9, 11, 13 - 16): + 587 m² with the corridors out']): fig.text(0.50, 0.190 - i * 0.0285, s, color=FG, fontsize=16, va='center')
save(fig, 'GA-7-Clashes-Other-Gates.png')
