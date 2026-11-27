# Hydraulic ground cart ("mule") for A320 hydraulic system work (to-do 364 A, batch 2), our own model, no brands.
# Looked at: "U.S. Air Force Hydraulic Test Stand inspection" and "332nd Expeditionary Maintenance Squadron AGE
# Operations" (Wikimedia Commons, public domain): a box cabinet with louvred sides on a 4-wheel trailer with a tow bar,
# a panel of round white-faced gauges, T-handle valves, a sight glass, switches, a red E-stop and an hour meter; two
# thick hoses with quick-disconnect couplings coiled on hooks. Sizes: 3000 psi (207 bar), about 40 l/min, electric
# pump unit, cabinet 0.8 x 1.3 x 0.85 m on 4.00-8 wheels, hoses 1 in pressure and 1 1/4 in return, 10 m each.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_detail import *

reset(); detail_materials('HYC')
R = 0.205; ZF = 0.40; T = 0.06; XR = 0.36; YR0, YR1 = -0.98, 0.30; YF, YB = 0.72, -0.62
f = D()
for s in (-1, 1):
    f.sheet((s * XR - 0.04, YR0, ZF), (s * XR + 0.04, YR1, ZF + T), 'G', 0.005)
    f.bar((s * (XR - 0.02), YR1 - 0.02, ZF + 0.03), (s * 0.15, YF - 0.05, ZF + 0.03), 0.06, 0.05, 'G', 0.004)
    f.weld((s * (XR - 0.04), YR1 - 0.04, ZF + T + 0.002), (s * (XR - 0.08), YR1 + 0.01, ZF + T + 0.002), 'G')
for y in (YR0 + 0.02, -0.3, YR1 - 0.04): f.sheet((-XR + 0.04, y - 0.02, ZF + 0.005), (XR - 0.04, y + 0.02, ZF + T - 0.005), 'G', 0.004)
f.sheet((-0.2, YF - 0.2, ZF + 0.004), (0.2, YF + 0.06, ZF + 0.012), 'G', 0.003)
cart_frame(f, 0.46, YF, YB, ZF, R)
# cabinet: white folded sheet on a blue plinth, louvres both sides, seams of the removable side panels
CX, CY0, CY1, CZ0, CZ = XR + 0.03, YR0, YR1 - 0.02, ZF + T, 1.30
f.sheet((-CX, CY0, CZ0), (CX, CY1, CZ0 + 0.12), 'BL', 0.01)
f.sheet((-CX + 0.004, CY0 + 0.004, CZ0 + 0.118), (CX - 0.004, CY1 - 0.004, CZ), 'WH', 0.02)
for s in (-1, 1):
    for yy in ((CY0 + 0.08, -0.38), (-0.33, CY1 - 0.08)):
        louvres(f, s * (CX - 0.002), yy[0] + 0.06, yy[1] - 0.06, CZ - 0.3, 6)
        for z in (CZ0 + 0.2, CZ - 0.06):
            for y in yy: f.bolt((s * (CX - 0.004), y, z), (s, 0, 0), 0.006)
    f.box((s * (CX - 0.0035) - 0.002, -0.358, CZ0 + 0.14), (s * (CX - 0.0035) + 0.002, -0.352, CZ - 0.02), 'BK', 0.0005)   # panel seam
f.label((CX - 0.0035, 0.05, CZ - 0.45), (1, 0, 0), 0.16, 0.08, 'LW'); f.label((-CX + 0.0035, 0.05, CZ - 0.45), (-1, 0, 0), 0.16, 0.08, 'LI')
# control panel (rear, facing -y)
PY = CY0 - 0.004; N = (0, -1, 0); PZ = PY - 0.006
f.sheet((-CX + 0.03, PZ, CZ - 0.62), (CX - 0.03, PY + 0.004, CZ - 0.04), 'G', 0.006)
for x in (-CX + 0.05, CX - 0.05):
    for z in (CZ - 0.6, CZ - 0.06): f.bolt((x, PZ, z), N, 0.006)
f.label((0.0, PZ - 0.0006, CZ - 0.085), N, 0.18, 0.05, 'LI'); f.label((-0.25, PZ - 0.0006, CZ - 0.6 + 0.04), N, 0.08, 0.04, 'LV')
f.sheet((0.12, PZ - 0.012, CZ - 0.58), (0.17, PZ + 0.001, CZ - 0.4), 'CR', 0.004)                     # reservoir sight glass
f.box((0.13, PZ - 0.016, CZ - 0.57), (0.16, PZ - 0.0105, CZ - 0.41), 'GL', 0.002)
f.box((0.134, PZ - 0.0128, CZ - 0.565), (0.156, PZ - 0.0115, CZ - 0.48), 'KB', 0.001)
for x, rr in ((-0.18, 0.016), (0.0, 0.02)):                                                                # couplings' ports
    f.lz([(rr * 1.5, -0.012), (rr * 1.5, 0.04), (rr * 1.1, 0.05), (rr * 1.1, 0.07), (0.0, 0.07)], 'CR', frame((x, PY, CZ - 0.7), N), 6)
# hose hooks on the right side and the two hoses coiled on them
HZ = CZ - 0.12
for y in (-0.62, -0.1):
    f.bar((CX - 0.006, y, HZ), (CX + 0.16, y, HZ), 0.03, 0.03, 'G', 0.004)
    f.bar((CX + 0.16, y, HZ - 0.015), (CX + 0.16, y, HZ + 0.06), 0.03, 0.03, 'G', 0.004)
coil(f, (CX + 0.09, -0.62, HZ - 0.19), 'x', 0.21, 0.02, 4)
coil(f, (CX + 0.09, -0.1, HZ - 0.19), 'x', 0.21, 0.017, 4)
CP = {}
for y, rr, tag in ((-0.62, 0.02, 'Return'), (-0.1, 0.017, 'Pressure')):
    e = (CX + 0.16, y + 0.12, CZ - 0.66); CP[tag] = (e, rr)
    f.hose([(CX + 0.13, y + 0.04, HZ - 0.38), (CX + 0.16, y + 0.1, CZ - 0.55), (e[0], e[1], e[2] + 0.05)], rr, 'HS', 14, 6)
for s in (-1, 1):                                                                                          # push handles
    f.bent_tube([(s * (CX + 0.03), CY0 + 0.0, ZF + 0.5), (s * (CX + 0.03), CY0 - 0.06, ZF + 0.5), (s * (CX + 0.03), CY0 - 0.06, ZF + 0.72),
                 (s * (CX + 0.03), CY0 + 0.0, ZF + 0.72)], 0.017, 0.035, 'G')                 # ONE tube bent twice
    f.tube((s * (CX + 0.03), CY0 - 0.06, ZF + 0.55), (s * (CX + 0.03), CY0 - 0.06, ZF + 0.67), 0.021, 'BK', 14, weld=False)
    for z in (ZF + 0.5, ZF + 0.72):                                                                       # welded mounting plates, bolted to the side
        f.box((s * (CX - 0.003) - 0.0 if s > 0 else -CX - 0.05, CY0 - 0.012, z - 0.035), (CX + 0.05 if s > 0 else -CX + 0.003, CY0 + 0.05, z + 0.035), 'G', 0.003)
        f.bolt((s * (CX + 0.05), CY0 + 0.032, z), (s, 0, 0), 0.006)
for z in (CZ0 + 0.22, CZ - 0.46):                                                                        # service door hinge leaves on the cabinet
    f.box((-CX - 0.006, CY1 - 0.096, z + 0.003), (-CX + 0.006, CY1 - 0.06, z + 0.057), 'CR', 0.002)
part('Frame', f, (0, 0, 0), None, 'root: the cart, rolls on its wheels')
cart_gear('Frame', 0.46, YF, YB, ZF, R)

for tag, x, z, r, does in (('System', -0.24, CZ - 0.2, 0.06, 'pump pressure 0..350 bar'), ('Delivery', -0.08, CZ - 0.2, 0.06, 'pressure to the aircraft 0..350 bar'),
                           ('Return', 0.08, CZ - 0.2, 0.06, 'return pressure 0..10 bar'), ('Flow', 0.24, CZ - 0.2, 0.06, 'flow 0..60 l/min'),
                           ('Temperature', -0.24, CZ - 0.37, 0.035, 'fluid temperature'), ('HourMeter', 0.25, CZ - 0.37, 0.03, 'pump hours')):
    gauge(tag, (x, PZ, z), N, r, 'Frame', does)
valve('Valve_Pressure', (-0.08, PZ, CZ - 0.37), N, 'Frame', 'wheel', 'BK', 'sets the delivery pressure (right = more)', 0.04)
valve('Valve_Bypass', (0.05, PZ, CZ - 0.37), N, 'Frame', 'T', 'KR', 'bypass open = no pressure to the aircraft')
valve('Valve_Supply', (-0.1, PZ, CZ - 0.53), N, 'Frame', 'lever', 'KB', 'opens the pressure line', 0.022)
valve('Valve_Return', (0.02, PZ, CZ - 0.53), N, 'Frame', 'lever', 'KG', 'opens the return line', 0.022)
valve('Button_Start', (-0.3, PZ, CZ - 0.47), N, 'Frame', 'button', 'KG', 'starts the pump motor')
valve('Button_Stop', (-0.22, PZ, CZ - 0.47), N, 'Frame', 'button', 'BK', 'stops the pump motor')
valve('EStop', (0.27, PZ, CZ - 0.5), N, 'Frame', 'mushroom', 'KR', 'emergency stop')
valve('Switch_Lamp', (-0.32, PZ, CZ - 0.535), N, 'Frame', 'toggle', 'BK', 'panel lamp on / off')
for tag, (e, rr) in CP.items():
    cp = D(); M = frame((0, 0, 0), (0, 0, -1))
    cp.fitting((0, 0, 0.027), (0, 0, 1), rr)
    cp.lz([(0.0, 0.0), (rr * 1.7, 0.0), (rr * 1.7, 0.06), (rr * 1.3, 0.07), (rr * 1.3, 0.09), (0.0, 0.09)], 'CR' if tag == 'Pressure' else 'BR', M, 20)
    cp.lz([(rr * 1.66, 0.015), (rr * 1.9, 0.02), (rr * 1.9, 0.04), (rr * 1.66, 0.045)], 'KR' if tag == 'Pressure' else 'KB', M, 20)
    part('Coupling_' + tag, cp.mesh('Coupling_' + tag, (0, 0, 0)), e, 'Frame', 'quick-disconnect coupling: off its coil to the aircraft ground service port; sleeve slides back to couple')
    ik('Coupling_' + tag, (e[0], e[1], e[2] - 0.04), 'hand', 'Coupling_' + tag)
    ctrl(tag + ' hose', 'Coupling_' + tag, 'Coupling_' + tag, 'take the coupling to the aircraft port and couple it (sleeve back, push, release)')
dr = D(); P = (-CX - 0.009, CY1 - 0.1, CZ0 + 0.16)                                                        # service door, left side
dr.sheet((-CX - 0.012, -0.3, CZ0 + 0.16), (-CX - 0.003, CY1 - 0.1, CZ - 0.36), 'WH', 0.004)
for z in (CZ0 + 0.22, CZ - 0.46): dr.tube((-CX - 0.009, CY1 - 0.1, z), (-CX - 0.009, CY1 - 0.1, z + 0.06), 0.007, 'CR', 10, weld=False)
dr.box((-CX - 0.03, -0.27, CZ0 + 0.4), (-CX - 0.011, -0.25, CZ0 + 0.52), 'BK', 0.004)
dr.label((-CX - 0.0125, -0.1, CZ - 0.5), (-1, 0, 0), 0.12, 0.06, 'LW')
part('Door_Service', dr, P, 'Frame', 'hinge about z: 0 = shut .. 110 deg open (pump, motor, filters)')
ik('Door_Service', (-CX - 0.035, -0.26, CZ0 + 0.46), 'hand', 'Door_Service'); ctrl('Service door', 'Door_Service', 'Door_Service', 'open / shut')
park_brake('Frame', (-XR - 0.05, YB + 0.3, ZF + 0.03), -1)
ik('Push_L', (-(CX + 0.03), CY0 - 0.06, ZF + 0.61), 'push', 'Frame'); ik('Push_R', (CX + 0.03, CY0 - 0.06, ZF + 0.61), 'push', 'Frame')
ik('Operator', (0, CY0 - 0.45, 0), 'stand', 'Frame')
finish('HydraulicCart', 'hydraulic ground cart (mule), 3000 psi',
       {'pressure_bar': 207, 'flow_l_min': 40, 'hoses': '2 x 10 m, 1 in pressure / 1 1/4 in return, quick-disconnect',
        'cabinet_m': [0.84, 1.26, 0.84], 'wheels': '4.00-8 pneumatic', 'steer_deg': 35},
       {'x_min': -0.52, 'x_max': 0.63, 'y_min': YR0 - 0.1, 'y_max': YF + 0.25, 'towbar_down_reach_y': YF + 1.2,
        'note': 'tow bar up; operator stands behind the panel (y about -1.4); hoses reach 10 m'},
       'Wikimedia Commons: U.S. Air Force Hydraulic Test Stand inspection; 332nd EMS AGE Operations (public domain); 3000 psi ground cart sizes')
