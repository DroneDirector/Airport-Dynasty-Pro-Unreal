# The pushback tug's blueprint sheet v2: the Blender wireframe views (Tools/make_pushback_tug.py -- --wire) with the sizes
# (whole millimetres, owner 2026-11-21) and the notes.
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt, matplotlib.image as mpimg
D = 'C:/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal/Blueprints/Vehicles/PushbackTug/'
BG, INK, ACC, DIM, IK = '#071426', '#dbe6f3', '#f4b740', '#8fb4e6', '#4cd18a'
fig = plt.figure(figsize=(30, 30), dpi=100, facecolor=BG)
fig.text(0.02, 0.985, 'AIRPORT DYNASTY PRO · PUSHBACK TUG (towbar, A320 / B737) · BLUEPRINT v2 · WIREFRAME', color=INK, fontsize=30, fontweight='bold', va='top')
fig.text(0.02, 0.968, 'Every line is an edge of the real Blender mesh · sizes in whole millimetres · nothing floats: every part sits on, hangs on or is bolted to the part under it', color=DIM, fontsize=15, va='top')
views = [('1-three-quarter-front', 'THREE-QUARTER FRONT (left side, the cab)', [0.01, 0.66, 0.49, 0.29]),
         ('2-three-quarter-rear', 'THREE-QUARTER REAR (right side)', [0.50, 0.66, 0.49, 0.29]),
         ('3-side-left', 'SIDE (left)', [0.01, 0.45, 0.62, 0.20]),
         ('4-front', 'FRONT', [0.63, 0.45, 0.18, 0.20]),
         ('5-rear', 'REAR', [0.81, 0.45, 0.18, 0.20]),
         ('6-top', 'TOP (front to the right)', [0.01, 0.24, 0.49, 0.20]),
         ('7-cab-interior', 'CAB INTERIOR (roof and glass taken off)', [0.50, 0.24, 0.49, 0.20]),
         ('8-operator-seat', 'OPERATOR SEAT (suspension seat, from above the dash)', [0.01, 0.02, 0.40, 0.21])]
for f, title, r in views:
    a = fig.add_axes(r); a.imshow(mpimg.imread(D + 'wire/' + f + '.png')); a.axis('off')
    a.set_title(title, color=ACC, fontsize=17, loc='left', fontweight='bold')
n = fig.add_axes([0.43, 0.02, 0.56, 0.21], facecolor=BG); n.axis('off')
rows = [('OUTSIDE', ACC), ('Length 5105 · width 2515 · deck 1321 · height with the cab 2134 · wheelbase 2311 · track 2108', INK),
        ('Tyres 315/80 R22.5, diameter 1076 · ground clearance 228 · step 533 · 4-wheel drive, 4-wheel steering', INK),
        ('Body: flat plate steel, 35 chamfers on the edges (straight, as cut steel); round only at the wheel arches, the cab corners and the tyres', INK),
        ('Hitches front and rear, 2 jaw heights (380 and 620) and the pin; lights set into the bumpers; beacon on its base plate', INK),
        ('CAB (left side, between the axles, floor sunk to 600 over the ground)', ACC),
        ('Outside 1520 long (upper) / 1120 (lower) · 1400 wide · inside height 1484 from the floor to the roof liner', INK),
        ('Inside 1020 long at the floor · 1300 wide · front window 1180 wide · door 710 wide with its hinges on the front post', INK),
        ('Operator seat: suspension base with bellows (200), seat 480 wide, cushion top 390 over the floor, back 660 high, leaning back 12°, side bolsters, headrest on 2 posts, armrests, drive selector on the right armrest, lap belt', INK),
        ('2 fold-down crew seats on the right wall (380 wide), dash with display and switches, steering wheel 380 across on a tilted column, 3 pedals, park brake, extinguisher, radio, grab handle, sun visor, lamp', INK),
        ('IK POINTS (20): driver seat + back, hands L/R on the wheel, accelerator, brake, selector, park brake, crew seats 2 + 3 (seat + back), step, grab handles outside + inside, door outside + inside + hinge, towbar front + rear', IK),
        ('NEXT AFTER YOUR OK: photo textures (free licence only), the livery with the airport name at run time, into the game, then pictures', ACC)]
y = 0.98
for t, c in rows:
    n.text(0.0, y, t, color=c, fontsize=15 if c == ACC else 13.5, va='top', fontweight='bold' if c == ACC else 'normal', wrap=True); y -= 0.088 if c != ACC else 0.075
fig.savefig(D + 'PushbackTug-Blueprint-v2.png', facecolor=BG)
print('ok')
