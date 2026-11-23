# Gate arc: every clearance worked out from the numbers (plan metres); prints them.
from ga_geom import *
import math
def near(A, x, z, off=0.09): 
    o = (x - A.mid, z - A.cz); a = math.atan2(*o); return math.hypot(*o) - (A.R + off) if abs(a) <= A.a else min(math.hypot(x - q, z - ZG) for q in (A.x0, A.x1))
for d in (2, 3, 4):
    A = Arc(d); print('\n== depth', d)
    print('tip of the edge z %.2f' % (ZG + d + .09))
    for n, x, z in (('first corridor columns W', -213.3, COLZ), ('first corridor columns E', -210.1, COLZ), ('A320 nose', -225.42, 120.0), ('tug box nearest corner', -236.42, 87.0), ('GPU box', -230.62, 116.7),
                    ('mast (gates 1-4 only: same offset as gate 1 mast)', -43.0 - (-72.6) + X0, 90.0), ('jetway tunnel swung fully E, side', 0, 0)):
        if n.startswith('jetway'): continue
        print('  %-52s %.2f' % (n, near(A, x, z)))
    print('  %-52s %.2f' % ('rotunda (its roof edge)', near(A, *ROT) - 1.92))
    print('  %-52s %.2f' % ('net floor gained', A.area - (A.z(JX) - ZG) * 3.51))
    # tunnel swept +-0.45 rad about docked yaw: its near corners
    y0 = math.atan2(-219.63 + 211.7, 124.99 - 95.84); m = 1e9
    for k in range(-20, 21):
        y = y0 + 0.45 * k / 20
        for s in (-1.75, 1.75):
            x, z = ROT[0] + math.sin(y) * 1.5 + math.cos(y) * s, ROT[1] + math.cos(y) * 1.5 - math.sin(y) * s; m = min(m, near(A, x, z))
    print('  %-52s %.2f  (docked yaw %.1f deg, range %.1f..%.1f)' % ('moving tunnel anywhere in its swing', m, math.degrees(y0), math.degrees(y0 - .45), math.degrees(y0 + .45)))
    print('  stair top landing: flat %.2f m (80.5 to the glass) -> at x -204.0: %.2f, at the stair\'s middle' % (81.84 - 80.5, A.z(-204.0) - 0.07 - 80.5), ' at lift door x -202.3: %.2f' % (A.z(-202.3) - .07 - 80.5))
    print('  gap door wall to glass each side of corridor: W %.2f E %.2f' % (A.z(JCL) - 82, A.z(JCR) - 82))
# gates 13-16: nose y 95 at the gate/view boundary
A = Arc(3)
for g, x0, nx, half in ((13, 255.4, 289.4, 3.6), (14, 335.2, 369.2, 3.1), (15, 407.5, 441.5, 3.1), (16, 479.8, 513.8, 3.1)):
    s = x0 - X0; print(g, 'nose to arc %.2f' % near(A, nx - s, 95.0), 'arc x %.2f..%.2f' % (X0 + s, X1 + s))
for g, x0 in ((1, -72.6), (2, 7.95), (3, 88.5), (4, 169.05), (9, -233.7), (11, -153.15)):
    masts = [-43.0, -2.14, 38.71, 79.57, 120.43, 161.29, 202.14, 243.0]; s = x0 - X0; print('gate', g, 'arc %.2f..%.2f' % (x0, X1 + s), 'nearest mast %.2f' % min(near(A, m - s, 90.0) for m in masts))
# gate 4's gallery in blueprint v2: x 194.5..303, z 82.2..84.8
s = 169.05 - X0; print('gate 4 arc at x 194.5: z %.2f, at 200.89: %.2f' % (A.z(194.5 - s), A.z(min(200.89 - s, X1))))
