# Blueprint of the pushback tug (towbar type, A320/B737 class) - owner 2026-11-21: blueprint first, his OK before modelling;
# every part attached (nothing floating), curves only where the real vehicle curves, a full connected interior with its
# inside dimensions.
# Real figures (Eagle XL-30 class spec sheet, aviationspares.com): L 5105, W 2515, deck 1321, with cab 2134, wheelbase 2311,
# track 2108, tyres 315/80R22.5 (dia ~1076), step 533, ground clearance 228 (mm); 3-person seating, 4WD, 4-wheel steering.
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, Polygon, FancyBboxPatch, Arc, Wedge

L, W, DECK, CABH, WB, TR, TD, STEP, GC = 5105, 2515, 1321, 2134, 2311, 2108, 1076, 533, 228
CFLOOR = 600                      # the cab floor sits low inside the body (so a seated driver fits under 2134)
CX0, CX1 = 1300, 2750             # cab front/back along the length (side view)
CY0, CY1 = W / 2 - 120 - 1400, W / 2 - 120   # cab across (top view): the left side, driver sees the nose gear past it
BG, INK, DIM, ACC, IK, BODY, DARK, GLASS = '#05080c', '#d8e2ee', '#7fa7d9', '#f4b740', '#4cd18a', '#1a2430', '#0d1117', '#1e3a52'
OUT = 'C:/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal/Blueprints/Vehicles/PushbackTug/'
plt.rcParams.update({'font.family': 'DejaVu Sans', 'text.color': INK})


def axes(fig, rect, title, xl, yl):
    a = fig.add_axes(rect, facecolor=BG)
    a.set_xlim(*xl); a.set_ylim(*yl); a.set_aspect('equal'); a.axis('off')
    a.set_title(title, color=ACC, fontsize=14, loc='left', fontweight='bold')
    return a


def box(a, x, y, w, h, fc=BODY, ec=INK, lw=1.6, z=2, r=0):
    if r: a.add_patch(FancyBboxPatch((x + r, y + r), w - 2 * r, h - 2 * r, boxstyle='round,pad=%d' % r, fc=fc, ec=ec, lw=lw, zorder=z))
    else: a.add_patch(Rectangle((x, y), w, h, fc=fc, ec=ec, lw=lw, zorder=z))


def poly(a, pts, fc=BODY, ec=INK, lw=1.6, z=3):
    a.add_patch(Polygon(pts, closed=True, fc=fc, ec=ec, lw=lw, zorder=z))


def dim(a, x1, y1, x2, y2, t, vert=False, fs=10):
    a.annotate('', (x1, y1), (x2, y2), arrowprops=dict(arrowstyle='<->', color=DIM, lw=1))
    a.text((x1 + x2) / 2, (y1 + y2) / 2, t, color=DIM, fontsize=fs, ha='center', va='center', rotation=90 if vert else 0,
           bbox=dict(fc=BG, ec='none', pad=1), zorder=9)


def ik(a, x, y, n, dx=70, dy=70, r=45):
    a.add_patch(Circle((x, y), r, fc=IK, ec='none', zorder=8))
    a.text(x + dx, y + dy, n, color=IK, fontsize=8.5, zorder=8)


def label(a, x, y, t, **k):
    a.text(x, y, t, color=k.pop('color', INK), fontsize=k.pop('fs', 8.5), ha=k.pop('ha', 'center'), va=k.pop('va', 'center'), zorder=9, **k)


fig = plt.figure(figsize=(26, 17), dpi=110, facecolor=BG)
fig.text(0.03, 0.975, 'PUSHBACK TUG  ·  towbar type for A320 / B737  ·  blueprint v1 for your OK', color=ACC, fontsize=20, fontweight='bold', va='top')
fig.text(0.03, 0.952, 'all sizes in mm  ·  green = IK points  ·  every part sits on or is bolted to the part under it (nothing floats)', color=DIM, fontsize=11, va='top')

# ---------------- SIDE VIEW (front to the right) ----------------
a = axes(fig, [0.02, 0.50, 0.50, 0.42], 'SIDE VIEW  (left side, front to the right)', (-900, 7000), (-550, 2950))
a.plot([-850, 6250], [0, 0], color='#3a4656', lw=1)
fx, rx = L / 2 + WB / 2, L / 2 - WB / 2
# axles first (the wheels hang on them, the frame sits on them)
for x in (fx, rx): box(a, x - 90, TD / 2 - 60, 180, 120, fc='#2b3644', z=4)
# the body: a straight steel slab (the real thing is flat plate), sloped nose plate at the front (straight, not curved)
poly(a, [(0, GC), (0, DECK), (L - 700, DECK), (L, DECK - 260), (L, GC + 140), (L - 140, GC)], z=3)
# wheel arches: the only real curves on the body (round cut-outs over each tyre)
for x in (fx, rx):
    a.add_patch(Wedge((x, TD / 2), TD / 2 + 70, 0, 180, fc=BG, ec=INK, lw=1.6, zorder=3.5))
    a.add_patch(Arc((x, TD / 2), TD + 260, TD + 260, theta1=8, theta2=172, color=INK, lw=2.2, zorder=4))   # fender lip, welded to the deck
# tyres + rims on the axles
for x in (fx, rx):
    a.add_patch(Circle((x, TD / 2), TD / 2, fc=DARK, ec=INK, lw=1.8, zorder=5))
    a.add_patch(Circle((x, TD / 2), 290, fc='#2b3644', ec=DIM, lw=1, zorder=6))
    for k in range(10): a.add_patch(Circle((x + 200 * __import__('math').cos(k * 0.628), TD / 2 + 200 * __import__('math').sin(k * 0.628)), 18, fc=INK, ec='none', zorder=7))
# bumpers bolted to the frame ends (rear 101 mm plate, front 38 mm plate)
box(a, -101, GC + 120, 101, 470, fc='#2b3644', z=4); box(a, L, GC + 160, 38, 400, fc='#2b3644', z=4)
# hitches: jaw blocks welded to the bumpers
box(a, -261, 480, 160, 140, fc='#3c4a5c', z=5); box(a, L + 38, 480, 160, 140, fc='#3c4a5c', z=5)
label(a, -560, 900, 'REAR hitch\n(pull)', fs=9); label(a, L + 380, 1250, 'FRONT hitch\n(push: the towbar\nto the nose gear)', fs=9)
# the cab: sits IN the body (floor at 600), straight posts, the roof edge gently rounded (real cabs are)
box(a, CX0, CFLOOR, CX1 - CX0, CABH - CFLOOR, fc='#101823', lw=1.8, z=4, r=60)
box(a, CX0 + 110, DECK + 60, (CX1 - CX0) / 2 - 150, CABH - DECK - 200, fc=GLASS, ec=DIM, lw=1, z=5, r=30)   # front door window
box(a, (CX0 + CX1) / 2 + 40, DECK + 60, (CX1 - CX0) / 2 - 150, CABH - DECK - 200, fc=GLASS, ec=DIM, lw=1, z=5, r=30)
a.plot([(CX0 + CX1) / 2] * 2, [CFLOOR + 40, CABH - 40], color=INK, lw=1.4, zorder=6)   # B-pillar / door edge
box(a, (CX0 + CX1) / 2 - 330, 1050, 120, 40, fc=INK, ec=INK, z=6)                      # door handle
label(a, (CX0 + CX1) / 2 - 270, 960, 'door handle', fs=8)
# beacon on a base plate on the roof
box(a, (CX0 + CX1) / 2 - 130, CABH, 260, 30, fc='#3c4a5c', z=6); box(a, (CX0 + CX1) / 2 - 80, CABH + 30, 160, 120, fc=ACC, ec=ACC, z=6, r=30)
label(a, (CX0 + CX1) / 2, CABH + 290, 'amber beacon (on its base plate)', color=ACC, fs=8.5)
# mirrors on arms bolted to the A-pillar
a.plot([CX1, CX1 + 260], [1700, 1780], color=INK, lw=2, zorder=6); box(a, CX1 + 240, 1640, 70, 260, fc='#2b3644', z=6)
# the step: a tread on two brackets welded to the body side
box(a, CX0 + 380, STEP - 40, 560, 40, fc=ACC, ec=ACC, z=6)
for x in (CX0 + 420, CX0 + 860): a.plot([x, x], [STEP - 40, GC + 60], color=ACC, lw=2, zorder=6)
label(a, CX0 + 660, STEP - 330, 'step 533 (on 2 brackets)', color=ACC, fs=8.5)
# grab handle on the cab's front post, bolted at both ends
a.plot([CX1 - 40, CX1 + 40, CX1 + 40, CX1 - 40], [1050, 1050, 1650, 1650], color=ACC, lw=2.5, zorder=7)
# deck items, each sitting on the deck: ballast plates behind, the engine cover in front
box(a, 200, DECK, 950, 150, z=4); box(a, 3000, DECK, 1700, 170, z=4, r=40)
label(a, 675, DECK + 330, 'ballast plates (bolted)', fs=8.5); label(a, 3850, DECK + 350, 'engine cover (hinged, rounded edges)', fs=8.5)
# lights set into the body
for x in (60, L - 160): box(a, x, DECK - 330, 90, 120, fc='#e04040', ec='none', z=6)
box(a, L - 40, DECK - 420, 40, 140, fc='#f4f0d0', ec='none', z=6)
# exhaust: stack clamped to the cab's back wall
box(a, CX0 - 90, DECK, 70, 820, fc='#3c4a5c', z=5); label(a, CX0 - 420, DECK + 760, 'exhaust\n(clamped\nto cab)', fs=8)
ik(a, (CX0 + CX1) / 2 + 200, CFLOOR + 420, 'IK seat (driver)'); ik(a, (CX0 + CX1) / 2 + 560, CFLOOR + 820, 'IK wheel L/R', dx=-700, dy=60)
ik(a, CX1 - 160, CFLOOR + 90, 'IK pedals'); ik(a, CX0 + 660, STEP, 'IK step', dy=-190)
ik(a, CX1 + 40, 1350, 'IK grab handle', dx=120, dy=-230); ik(a, (CX0 + CX1) / 2 - 270, 1070, 'IK door', dx=-560, dy=120)
dim(a, 0, -320, L, -320, 'overall length 5105'); dim(a, rx, -160, fx, -160, 'wheelbase 2311')
dim(a, 6800, 0, 6800, DECK, 'deck 1321', vert=True); dim(a, 6550, 0, 6550, CABH, 'with cab 2134', vert=True)
label(a, fx, -500, 'tyres 315/80 R22.5  (Ø ~1076)  ·  ground clearance 228', color=DIM, fs=9)

# ---------------- FRONT VIEW ----------------
b = axes(fig, [0.53, 0.50, 0.22, 0.42], 'FRONT VIEW', (-1750, 1750), (-550, 2950))
b.plot([-1700, 1700], [0, 0], color='#3a4656', lw=1)
for s in (-1, 1): box(b, s * TR / 2 - 160, 0, 320, TD, fc=DARK, z=3)
box(b, -TR / 2, TD / 2 - 60, TR, 120, fc='#2b3644', z=3.5)                                      # the axle between them
poly(b, [(-W / 2, GC + 150), (-W / 2, DECK), (W / 2, DECK), (W / 2, GC + 150), (W / 2 - 150, GC), (-W / 2 + 150, GC)], z=4)
box(b, -W / 2 + 120, CFLOOR + 200, 1400, CABH - CFLOOR - 200, fc='#101823', lw=1.8, z=5, r=60)   # cab on the left half
box(b, -W / 2 + 220, DECK + 60, 1200, CABH - DECK - 200, fc=GLASS, ec=DIM, lw=1, z=6, r=30)
b.plot([-W / 2 + 420, -W / 2 + 1000], [DECK + 120, CABH - 200], color=INK, lw=1.2, zorder=7)    # wiper on its spindle
box(b, -W / 2 + 820 - 130, CABH, 260, 30, fc='#3c4a5c', z=6); box(b, -W / 2 + 820 - 80, CABH + 30, 160, 120, fc=ACC, ec=ACC, z=6, r=30)
for s in (-1, 1): box(b, s * (W / 2 - 280) - 110, DECK - 200, 220, 110, fc='#f4f0d0', ec='none', z=6)
box(b, -260, 480, 520, 140, fc='#3c4a5c', z=6); label(b, 0, 330, 'front hitch (pin jaw)', fs=8.5)
for x in range(-1000, 1001, 400): box(b, x - 70, GC + 160, 140, 340, fc='none', ec='#c8a000', lw=1, z=6)
dim(b, -W / 2, -320, W / 2, -320, 'width 2515'); dim(b, -TR / 2, -160, TR / 2, -160, 'track 2108')
label(b, 600, 2500, 'the deck stays under\nthe A320 nose (~3.0 m)', fs=8.5)

# ---------------- REAR VIEW ----------------
r = axes(fig, [0.76, 0.50, 0.22, 0.42], 'REAR VIEW', (-1750, 1750), (-550, 2950))
r.plot([-1700, 1700], [0, 0], color='#3a4656', lw=1)
for s in (-1, 1): box(r, s * TR / 2 - 160, 0, 320, TD, fc=DARK, z=3)
box(r, -TR / 2, TD / 2 - 60, TR, 120, fc='#2b3644', z=3.5)
box(r, -W / 2, GC, W, DECK - GC, z=4)
box(r, W / 2 - 120 - 1400, CFLOOR + 200, 1400, CABH - CFLOOR - 200, fc='#101823', lw=1.8, z=5, r=60)   # cab (mirrored: on the right seen from behind)
box(r, W / 2 - 120 - 1250, DECK + 160, 1100, 520, fc=GLASS, ec=DIM, lw=1, z=6, r=30)                 # rear window
box(r, -W / 2 + 60, GC + 120, W - 120, 470, fc='#2b3644', z=5)                                       # rear bumper (101 mm plate)
box(r, -260, 480, 520, 140, fc='#3c4a5c', z=6); label(r, 0, 330, 'rear hitch', fs=8.5)
for s in (-1, 1): box(r, s * (W / 2 - 200) - 60, DECK - 330, 120, 120, fc='#e04040', ec='none', z=6)
box(r, -700, 760, 900, 260, fc='none', ec=DIM, lw=1, z=6); label(r, -250, 890, 'airport name\n(written at run time)', fs=8, color=DIM)

# ---------------- TOP VIEW ----------------
c = axes(fig, [0.02, 0.06, 0.42, 0.40], 'TOP VIEW  (front to the right)', (-900, 6300), (-1750, 1850))
box(c, 0, -W / 2, L, W, z=2)
for x in (rx, fx):
    for s in (-1, 1): box(c, x - TD / 2, s * TR / 2 - 160, TD, 320, fc=DARK, z=1.5)
box(c, -101, -W / 2 + 200, 101, W - 400, fc='#2b3644', z=3); box(c, L, -W / 2 + 250, 38, W - 500, fc='#2b3644', z=3)
box(c, CX0, CY0, CX1 - CX0, CY1 - CY0, fc='#101823', lw=1.8, z=4, r=60)
label(c, (CX0 + CX1) / 2, (CY0 + CY1) / 2, 'CAB', fs=11)
box(c, 3000, -W / 2 + 150, 1700, W - 300, fc='none', ec=DIM, lw=1, z=4, r=40); label(c, 3850, -300, 'engine cover', color=DIM, fs=9)
box(c, 200, -W / 2 + 150, 950, W - 300, fc='none', ec=DIM, lw=1, z=4); label(c, 675, 0, 'ballast', color=DIM, fs=9)
label(c, -560, 0, 'REAR', fs=10, rotation=90); label(c, L + 420, 0, 'FRONT', fs=10, rotation=90)
dim(c, 0, -1620, L, -1620, 'length 5105'); dim(c, 5950, -W / 2, 5950, W / 2, 'width 2515', vert=True)
dim(c, CX0, CY1 + 250, CX1, CY1 + 250, 'cab outside 1450')

# ---------------- INTERIOR: plan + section with inside sizes ----------------
d = axes(fig, [0.45, 0.06, 0.27, 0.40], 'CAB INTERIOR - plan (from above)', (CX0 - 200, CX1 + 250), (CY0 - 250, CY1 + 300))
IX0, IX1, IY0, IY1 = CX0 + 50, CX1 - 50, CY0 + 50, CY1 - 50          # inside of the 50 mm walls
box(d, CX0, CY0, CX1 - CX0, CY1 - CY0, fc='#101823', lw=2, z=1, r=60); box(d, IX0, IY0, IX1 - IX0, IY1 - IY0, fc='#151d28', ec=DIM, lw=1, z=2)
box(d, IX1 - 260, IY0, 260, IY1 - IY0, fc='#22303f', z=3)                                       # dashboard across the front wall
label(d, IX1 - 130, IY0 + 160, 'dash', fs=8, rotation=90)
my = (IY0 + IY1) / 2 + 230                                                                        # driver on the outer (left) side
box(d, IX1 - 260 - 120, my - 60, 120, 120, fc='#3c4a5c', z=4)                                    # steering column into the dash
d.add_patch(Circle((IX1 - 260 - 180, my), 190, fc='none', ec=INK, lw=3, zorder=5))               # steering wheel (round: it is)
box(d, IX0 + 380, my - 260, 480, 520, fc='#2b3644', z=4, r=50)                                   # driver seat on its base
box(d, IX0 + 300, my - 260, 90, 520, fc='#2b3644', z=4, r=30)                                    # its backrest
for k, yy in enumerate((my - 120, my + 0, my + 120)): box(d, IX1 - 260 - 60, yy - 30, 60, 60, fc=INK, ec=INK, z=5)  # 3 pedals on the firewall
box(d, IX0 + 900, IY0 + 230, 260, 200, fc='#3c4a5c', z=4, r=20); label(d, IX0 + 1030, IY0 + 330, 'console:\ngear + brake', fs=7)
for k, xx in enumerate((IX0 + 120, IX0 + 560)):                                                  # 2 fold-down seats on the inner wall
    box(d, xx, IY0, 380, 300, fc='#2b3644', z=4, r=40)
label(d, IX0 + 500, IY0 - 140, 'seats 2 + 3 fold down (hinged to the wall)', fs=8)
ik(d, IX0 + 620, my, 'IK driver', dx=-160, dy=-330, r=30); ik(d, IX1 - 260 - 180, my + 190, 'IK hand L', dx=-80, dy=90, r=30)
ik(d, IX1 - 260 - 180, my - 190, 'IK hand R', dx=-80, dy=-150, r=30); ik(d, IX1 - 300, my, 'IK pedals', dx=-330, dy=-60, r=30)
ik(d, IX0 + 310, IY0 + 150, 'IK seat 2', dx=-60, dy=200, r=30); ik(d, IX0 + 750, IY0 + 150, 'IK seat 3', dx=-60, dy=200, r=30)
dim(d, IX0, CY1 + 160, IX1, CY1 + 160, 'inside length 1350', fs=9); dim(d, CX1 + 150, IY0, CX1 + 150, IY1, 'inside width 1300', vert=True, fs=9)

e = axes(fig, [0.73, 0.06, 0.25, 0.40], 'CAB INTERIOR - section (side)', (CX0 - 250, CX1 + 450), (CFLOOR - 250, CABH + 350))
box(e, CX0, CFLOOR, CX1 - CX0, CABH - CFLOOR, fc='#101823', lw=2, z=1, r=60)
box(e, CX0 + 50, CFLOOR + 50, CX1 - CX0 - 100, CABH - CFLOOR - 110, fc='#151d28', ec=DIM, lw=1, z=2)
box(e, CX1 - 50 - 260, CFLOOR + 50, 260, 900, fc='#22303f', z=3)                                 # dash, standing on the floor
e.plot([CX1 - 310, CX1 - 560], [CFLOOR + 820, CFLOOR + 700], color='#8c9bb0', lw=6, zorder=4)   # column out of the dash
e.plot([CX1 - 600, CX1 - 520], [CFLOOR + 560, CFLOOR + 860], color=INK, lw=4, zorder=5)         # wheel rim (edge on)
box(e, CX0 + 250, CFLOOR + 50, 440, 240, fc='#3c4a5c', z=3)                                      # seat base bolted to the floor
box(e, CX0 + 230, CFLOOR + 290, 500, 110, fc='#2b3644', z=4, r=30)                               # cushion
e.add_patch(Polygon([(CX0 + 180, CFLOOR + 290), (CX0 + 260, CFLOOR + 290), (CX0 + 330, CFLOOR + 1050), (CX0 + 250, CFLOOR + 1050)], closed=True, fc='#2b3644', ec=INK, lw=1.4, zorder=4))  # backrest, leaning back
e.plot([CX1 - 300, CX1 - 400], [CFLOOR + 60, CFLOOR + 260], color=INK, lw=3, zorder=5)          # pedal on its hinge at the firewall
box(e, CX0 + 50, CABH - 170, CX1 - CX0 - 100, 60, fc='#22303f', z=3)                             # headliner with the light
box(e, (CX0 + CX1) / 2 - 80, CABH - 200, 160, 30, fc='#f4f0d0', ec='none', z=4)
ik(e, CX0 + 480, CFLOOR + 400, 'IK seat surface', dx=-40, dy=110, r=30); ik(e, CX0 + 290, CFLOOR + 800, 'IK back', dx=-430, dy=0, r=30)
ik(e, CX1 - 560, CFLOOR + 710, 'IK hands', dx=60, dy=150, r=30); ik(e, CX1 - 370, CFLOOR + 200, 'IK feet', dx=60, dy=-120, r=30)
dim(e, CX1 + 200, CFLOOR + 50, CX1 + 200, CABH - 60, 'inside height 1424', vert=True, fs=9)
dim(e, CX0 + 230, CFLOOR - 120, CX0 + 230, CFLOOR + 400, '', vert=True)
label(e, CX0 - 60, CFLOOR + 200, 'H-point\n400 over\nthe floor', color=DIM, fs=8)
label(e, (CX0 + CX1) / 2, CFLOOR - 170, 'cab floor 600 over the ground, sunk into the body', color=DIM, fs=8.5)

fig.text(0.03, 0.03, 'Livery: GSE yellow body, black/yellow chevrons on the bumpers, the airport name written at run time · no brand names · '
         'textures from free-licence photo materials (steel, rubber, glass, worn paint, dirt layers) · Nanite + instanced · '
         'lights glow by material · next after your OK: the Blender model, then front + back pictures', color=INK, fontsize=10.5)
fig.savefig(OUT + 'PushbackTug-Blueprint-v1.png', facecolor=BG)
print('ok')
