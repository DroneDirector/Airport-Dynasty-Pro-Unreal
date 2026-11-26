# The cargo terminal's lights and sounds, where they are (step B2): Data/cargo-lights.json (and Content/Data), model-local metres in the
# frame of cargo_layout.py (X along the building, Y from the airside face to the landside, Z up). CargoTerminalLife.cpp
# builds the fittings (instanced), gives each light to the model's lamp system (ABabylonModel::AddLamp: no shadows, a
# distance limit, dimmed by day as the terminal's) and plays the sounds at their spots (falloff, muffled through walls).
#   python Art/CargoTerminal/cargo_lights.py
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cargo_layout as CL
from cargo_layout import L, D, OFF, EXP, ETV, IMP, SPC, BCP, CANOPY_D, CANOPY_H, YARD_D, H_OFF

R = lambda v: round(v, 3)
fit, lights, sounds = [], [], []
REF_CD, REF_H = 230.0, 3.5          # a lamp of the terminal's hall at its height (as Hangar.cpp): the same light on the floor


def light(kind, x, y, z, cd, reach, aim=(0, 0, -1), cone=60.0, w=0.0, d=0.0):
    lights.append(dict(kind=kind, x=R(x), y=R(y), z=R(z), cd=round(cd), reach=R(reach), aim=[R(a) for a in aim], cone=cone, w=R(w), d=R(d)))


# halls: twin-strip LED high-bay fittings every 7.5 m on wires from the roof, 1.5 m under it, in three rows;
# one light for each two of them (every 15 m), as bright on the floor as the terminal's hall
ROWS = (12.0, 40.25, 62.0)
for zk, (x0, x1), h, _ in CL.ZONES:
    if zk == 'off': continue
    zf = h - 1.5
    n = int(round((x1 - x0) / 7.5))
    for y in ROWS:
        for i in range(n): fit.append(dict(kind='highbay', x=R(x0 + 3.75 + 7.5 * i), y=y, z=R(zf), top=h))
        for i in range(n // 2): light('spot', x0 + 7.5 + 15 * i, y, zf - 0.1, REF_CD * (zf / REF_H) ** 2 * 4, zf * 1.6, cone=70)

# office block: 0.6 m panels flush under each ceiling on a 5 m grid, clear of stairs and of walls reaching the ceiling;
# a soft panel of light per 15 x 17.5 m (as the terminal's pooled lamps)
ceil = {0: CL.FLOOR_Z[1], 1: CL.FLOOR_Z[2], 2: H_OFF}
walls = {0: [], 1: [w for w in CL.walls_from_rooms([(x0, y0, x1, y1, 4.3, dr, 1.0) for _, x0, y0, x1, y1, k, dr in CL.UPPER[1] if k != 'aisle'],
                                                   CL.FLOOR_Z[1] + 0.3, (0.0, OFF[1]), CL.SKIP_Y)], 2: []}
def clear(x, y, f):
    if any(sx0 - 0.5 < x < sx1 + 0.5 and sy0 - 0.5 < y < sy1 + 0.5 for sx0, sy0, sx1, sy1 in CL.STAIRS): return False
    return not any(w[0] - 0.45 < x < w[3] + 0.45 and w[1] - 0.45 < y < w[4] + 0.45 and w[5] > ceil[f] - 0.1 for w in walls[f])
for f, zc in ceil.items():
    for i in range(6):
        for j in range(14):
            x, y = 2.5 + 5 * i, 2.5 + 5 * j
            if clear(x, y, f): fit.append(dict(kind='panel', x=x, y=y, z=R(zc)))
    for i in range(2):
        for j in range(4): light('rect', 7.5 + 15 * i, 8.75 + 17.5 * j, zc - 0.05, 1100, 12.0, w=12.0, d=14.0)

# airside canopy (9 m): a floodlight under it over every ULD door
for x in CL.AIR_DOORS:
    fit.append(dict(kind='canopy', x=x, y=-5.0, z=CANOPY_H)); light('spot', x, -5.0, CANOPY_H - 0.15, REF_CD * (CANOPY_H / REF_H) ** 2 * 2, 20.0, cone=65)
# landside canopy (6 m): one over every dock and drive-in door
for x in CL.DOCKS + CL.SPC_DOCKS + CL.DRIVE_IN:
    fit.append(dict(kind='canopy', x=x, y=74.0, z=6.0)); light('spot', x, 74.0, 5.85, REF_CD * (6.0 / REF_H) ** 2 * 1.5, 13.0, cone=65)
# dock lights: a lamp on the wall inside beside each truck dock door, shining out into the trailer
for x in CL.DOCKS:
    fit.append(dict(kind='dock', x=x + 2.3, y=D - 0.1, z=2.8)); light('spot', x + 2.3, D - 0.3, 2.7, 400, 9.0, aim=(-0.35, 1.0, -0.25), cone=40)
# the office entrance canopy and the gatehouse lane roof
for x in (11.5, 18.5):
    fit.append(dict(kind='canopy', x=x, y=73.5, z=4.2)); light('spot', x, 73.5, 4.05, 450, 9.0, cone=70)
for x in (164.5, 171.5):
    fit.append(dict(kind='canopy', x=x, y=D + YARD_D - 7, z=3.2)); light('spot', x, D + YARD_D - 7, 3.05, 350, 8.0, cone=70)
# truck court: 16 m masts along the fence (clear of the gate), two floods each aimed back at the court
for x in (60.0, 120.0, 210.0, 280.0, 350.0, 430.0):
    y = D + YARD_D - 4.0
    fit.append(dict(kind='mast', x=x, y=y, z=16.0)); light('spot', x, y - 0.6, 15.6, 40000, 95.0, aim=(0, -1.0, -0.45), cone=55)

# sounds: (name, x, y, z, falloff m, volume; rain 'in' / 'out': its volume follows the rain, off when dry)
def snd(n, x, y, z, fall, vol, rain=''): sounds.append(dict(sound=n, x=x, y=y, z=z, falloff=fall, vol=vol, rain=rain))
for x in (80.0, 210.0, 315.0, 375.0, 435.0): snd('CgHallHum', x, 35.0, 6.0, 45.0, 0.4)
for x in (60.0, 110.0, 170.0, 230.0, 270.0, 315.0, 375.0, 435.0): snd('CgRainRoofIn', x, 35.0, 10.0, 40.0, 0.8, 'in')
for x in (60.0, 140.0, 220.0, 300.0): snd('CgRainRoofOut', x, -6.0, 9.5, 30.0, 1.0, 'out')
for x in (60.0, 160.0, 260.0, 360.0, 440.0): snd('CgRainRoofOut', x, 73.0, 6.5, 30.0, 1.0, 'out')
snd('CgRainRoofOut', 15.0, 73.0, 4.5, 20.0, 0.8, 'out'); snd('CgRainRoofOut', 163.0, D + YARD_D - 7, 3.5, 20.0, 0.8, 'out')
for x in (355.0, 375.0): snd('CgPlantHum', x, 26.0, 14.5, 60.0, 0.6)   # the special block's refrigeration plant on the roof
for f, z in ((0, 0.0), (1, CL.FLOOR_Z[1] + 0.3), (2, CL.FLOOR_Z[2] + 0.3)): snd('CgOfficeVent', 15.0, 30.0, z + 3.2, 18.0, 0.35)
printers = [dict(x=8.0, y=30.0, z=z + 1.0) for z in (0.0, CL.FLOOR_Z[1] + 0.3, CL.FLOOR_Z[2] + 0.3)]

out = dict(note='Cargo terminal lights and sounds, model-local metres (Art/CargoTerminal/cargo_lights.py); kinds: highbay, panel, canopy, dock, mast',
           fittings=fit, lights=lights, sounds=sounds, printers=printers)
root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
for d in ('Data', 'Content/Data'):        # (the packed game reads Content/Data)
    with open(os.path.join(root, d, 'cargo-lights.json'), 'w') as fh: json.dump(out, fh, indent=1)
print('%d fittings, %d lights, %d sounds' % (len(fit), len(lights), len(sounds)))
