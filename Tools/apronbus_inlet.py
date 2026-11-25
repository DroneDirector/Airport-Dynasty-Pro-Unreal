# The apron bus's charging inlet (to-do 277 E3a): an opening in the right skirt behind the last door, a tray with the CCS2 inlet
# in it and the flap on its hinge. Called by Tools/make_apron_bus.py; Art/ApronBus/add_inlet.py puts it into the finished model.
# The old flap (a plain plate at x -6.30, z 0.75) lay over the door post and under the stripe: this one stands clear of both.
import math
from mathutils import Matrix
import vehicle_kit as K, ccs2
X0, X1, Z0, Z1 = -6.620, -6.340, 0.605, 0.870   # the opening
FACE = -1.484                                   # the inlet's face (the skirt's outside is -1.495)
CX, CZ = -6.470, 0.737                          # the coupling's centre
HINGE = (-6.6235, -1.5035, (Z0 + Z1) / 2)       # the flap's hinge: upright, at its rear edge
OPEN = -110.0                                   # degrees about z up (outwards, then rearwards)
FRAME = Matrix.Translation((CX, FACE, CZ)) @ Matrix.Rotation(math.pi / 2, 4, 'Z')   # (x = into the bus = +y)


def cutter(o, x0=X0): return K.box('InletCut', ((x0 + X1) / 2, -1.48, (Z0 + Z1) / 2), (X1 - x0, 0.16, Z1 - Z0), o.data.materials[0], bevel=0.012)


def build(skirt, stripe, m_dark, m_plast, m_grey, m_pin, sock, piv):
    K.cut(skirt, cutter(skirt)); K.cut(stripe, cutter(stripe, X0 - 0.022))
    cx, cz, w, h = (X0 + X1) / 2, (Z0 + Z1) / 2, X1 - X0, Z1 - Z0
    tray = K.box('Inl_Tray', (cx, -1.4690, cz), (w + 0.002, 0.0520, h + 0.002), m_plast, bevel=0.013)    # (-1.495 .. -1.443, 1 mm into the skirt's cut edge)
    K.cut(tray, K.box('c', (cx, -1.4905, cz), (w - 0.006, 0.0910, h - 0.006), m_plast, bevel=0.010))      # (2 mm of back, open outside)
    inl = ccs2.inlet(m_grey, m_pin, back=0.0400)
    for o in inl: o.data.transform(FRAME); o.data.update()
    more = [K.cyl('Inl_Led', (CX + 0.085, -1.4455, CZ + 0.03), 0.012, 0.006, 'y', m_pin, 24, bevel=0.0015), K.box('Inl_Label', (CX + 0.085, -1.4455, CZ - 0.03), (0.06, 0.0045, 0.04), m_grey, bevel=0.001)]
    ccs2.join('ChargeInlet', [tray] + inl + more)
    f = K.box('ChargeFlap', (cx + 0.0005, -1.4990, cz), (w - 0.006, 0.008, h - 0.006), m_dark, bevel=0.0035)
    K.cut(f, K.cyl('c', (X1 - 0.028, -1.506, cz), 0.011, 0.010, 'y', m_dark, 24))
    parts = [f, K.cyl('Fl_Latch', (X1 - 0.028, -1.5005, cz), 0.0085, 0.004, 'y', m_pin, 24, bevel=0.0008)]
    for i, z in enumerate((cz - 0.085, cz + 0.085)):
        parts.append(K.cyl('Fl_Hinge%d' % i, (HINGE[0], HINGE[1], z), 0.0062, 0.045, 'z', m_dark, 20, bevel=0.001))
        parts.append(K.box('Fl_Arm%d' % i, (HINGE[0] + 0.008, -1.5005, z), (0.014, 0.0030, 0.040), m_dark))
    ccs2.join('ChargeFlap', parts)
    piv('ChargeFlap', HINGE)
    e = sock('Inlet', (CX, FACE, CZ)); e.rotation_euler = (0, 0, math.pi / 2)
