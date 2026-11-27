# Hangar ground power unit, electric 400 Hz (to-do 364 A, batch 2), our own model, no brands. No free photo found; built
# from published sizes of mobile static frequency converters: 90 kVA, 115/200 V 400 Hz from the hangar mains, cabinet
# about 1.1 x 0.7 x 1.1 m on 4 braked castors (200 mm), push handle, a 70 mm2 cable (about 45 mm thick, 20 m) on a reel
# with a crank, the 6-pin aircraft plug in a holster, a panel with display, start / stop, key switch and an E-stop.
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hq_detail import *

reset(); detail_materials('HGPU')
R = 0.10; CT = 2 * R + 0.07; Z0 = CT + 0.004
CX, CY0, CY1, CZ = 0.36, -0.55, 0.55, 1.15
f = D()
f.sheet((-CX - 0.02, CY0 - 0.02, Z0), (CX + 0.02, CY1 + 0.02, Z0 + 0.08), 'G', 0.006)                  # base frame
f.sheet((-CX, CY0, Z0 + 0.078), (CX, CY1, CZ), 'Y', 0.02)
for s in (-1, 1):
    louvres(f, s * (CX - 0.002), -0.35, 0.35, Z0 + 0.2, 8, 'Y')
    for y in (-0.45, 0.45):
        for z in (Z0 + 0.15, CZ - 0.06): f.bolt((s * (CX - 0.004), y, z), (s, 0, 0), 0.006)
f.label((CX + 0.0005, 0.0, CZ - 0.2), (1, 0, 0), 0.2, 0.1, 'LV'); f.label((-CX - 0.0005, 0.0, CZ - 0.2), (-1, 0, 0), 0.2, 0.1, 'LV')
PY = CY0 - 0.006; N = (0, -1, 0)
f.sheet((-CX + 0.03, PY, CZ - 0.38), (CX - 0.03, CY0 + 0.004, CZ - 0.04), 'G', 0.006)                   # panel
for x in (-CX + 0.05, CX - 0.05):
    for z in (CZ - 0.36, CZ - 0.06): f.bolt((x, PY, z), N, 0.006)
f.sheet((-0.2, PY - 0.01, CZ - 0.19), (0.06, PY + 0.001, CZ - 0.07), 'BK', 0.004)                        # display bezel
f.box((-0.19, PY - 0.0115, CZ - 0.18), (0.05, PY - 0.009, CZ - 0.08), 'KB', 0.001)
f.box((-0.192, PY - 0.0125, CZ - 0.182), (0.052, PY - 0.0105, CZ - 0.078), 'GL', 0.001)
f.label((0.17, PY - 0.0006, CZ - 0.1), N, 0.14, 0.07, 'LV')
for s in (-1, 1):                                                                       # push handle at the front end, panel stays clear
    f.tube((s * 0.3, CY1 - 0.004, Z0 + 0.62), (s * 0.3, CY1 + 0.12, Z0 + 0.62), 0.017, 'G')
    f.box((s * 0.3 - 0.035, CY1 - 0.004, Z0 + 0.585), (s * 0.3 + 0.035, CY1 + 0.008, Z0 + 0.655), 'G', 0.003)   # welded flange, bolted on
    for dz in (-0.022, 0.022): f.bolt((s * 0.3 + 0.022, CY1 + 0.008, Z0 + 0.62 + dz), (0, 1, 0), 0.005)
f.tube((-0.3, CY1 + 0.12, Z0 + 0.62), (0.3, CY1 + 0.12, Z0 + 0.62), 0.017, 'G')
f.tube((-0.2, CY1 + 0.12, Z0 + 0.62), (0.2, CY1 + 0.12, Z0 + 0.62), 0.021, 'BK', 14, weld=False)
# cable reel on top (axis x) on two welded stands, plug holster on the right side
RC = (0.0, 0.18, CZ + 0.27); RR = 0.25
for s in (-1, 1):
    f.bar((s * 0.3, RC[1] - 0.15, CZ - 0.004), (s * 0.3, RC[1], RC[2]), 0.04, 0.012, 'G', 0.003, up=(1, 0, 0))
    f.bar((s * 0.3, RC[1] + 0.15, CZ - 0.004), (s * 0.3, RC[1], RC[2]), 0.04, 0.012, 'G', 0.003, up=(1, 0, 0))
    f.sheet((s * 0.3 - 0.03, RC[1] - 0.2, CZ - 0.004), (s * 0.3 + 0.03, RC[1] + 0.2, CZ + 0.008), 'G', 0.003)
    f.lz([(0.035, -0.01), (0.035, 0.01), (0.0, 0.01)], 'ST', frame((s * 0.3, RC[1], RC[2]), (s, 0, 0)), 16)
HP = (CX + 0.06, 0.25, Z0 + 0.45)
f.lz([(0.06, -0.12), (0.065, -0.12), (0.065, 0.0), (0.06, 0.0), (0.06, -0.12)], 'BK', frame(HP, (0, 0, 1)), 24, caps=False)
f.box((CX - 0.004, HP[1] - 0.03, HP[2] - 0.1), (CX + 0.0, HP[1] + 0.03, HP[2] - 0.02), 'G', 0.002)
f.bar((CX - 0.003, HP[1], HP[2] - 0.06), (HP[0] - 0.062, HP[1], HP[2] - 0.06), 0.03, 0.06, 'G', 0.003)
f.hose([(0.27, RC[1] + 0.05, RC[2] - 0.2), (CX + 0.12, RC[1] + 0.1, CZ - 0.1), (CX + 0.14, HP[1] + 0.12, HP[2] - 0.25),
        (HP[0], HP[1], HP[2] - 0.3), (HP[0], HP[1], HP[2] - 0.17)], 0.022, 'HS', 16, 6)
part('Frame', f, (0, 0, 0), None, 'root: the unit, rolls on its castors')
for tag, x, y in (('FL', -0.28, 0.42), ('FR', 0.28, 0.42), ('RL', -0.28, -0.42), ('RR', 0.28, -0.42)):
    castor2(tag, x, y, 'Frame', R, 0.05, CT + 0.005)
rl = D()
for h in (-0.26, 0.26):
    rl.lathe([(0.06, h - 0.004), (RR, h - 0.004), (RR + 0.01, h), (RR, h + 0.004), (0.06, h + 0.004)], 'G', (0, 0, 0), 'x', 40)
    for k in range(6):
        a = 2 * math.pi * k / 6; rl.bolt((h + 0.005 * (1 if h > 0 else -1), 0.1 * math.cos(a), 0.1 * math.sin(a)), (1 if h > 0 else -1, 0, 0), 0.007)
rl.lathe([(0.1, -0.256), (0.1, 0.256)], 'G', (0, 0, 0), 'x', 32)
for k in range(10):                                                                     # wound cable, 2 layers
    h = -0.23 + 0.051 * k
    for rr in (0.124, 0.17):
        rl.lathe([(rr - 0.022 * math.cos(2 * math.pi * j / 10), h + 0.023 * math.sin(2 * math.pi * j / 10)) for j in range(11)], 'HS', (0, 0, 0), 'x', 40)
rl.tube((-0.32, 0, 0), (0.32, 0, 0), 0.02, 'CR', 14, weld=False)
rl.tube((0.33, 0, 0), (0.33, 0.0, -0.16), 0.01, 'G', 10, weld=False)
rl.tube((0.33, 0.0, -0.16), (0.41, 0.0, -0.16), 0.007, 'CR', 10, weld=False)
rl.tube((0.36, 0.0, -0.16), (0.42, 0.0, -0.16), 0.015, 'BK', 12, weld=False)
part('CableReel', rl.mesh('CableReel', (0, 0, 0)), RC, 'Frame', 'spins about x: pays out / winds in 20 m of cable; the crank turns with it')
ik('ReelCrank', (0.39, RC[1], RC[2] - 0.16), 'hand', 'CableReel'); ctrl('Cable reel', 'CableReel', 'ReelCrank', 'wind the cable in after use')
pl = D(); M = frame((0, 0, 0), (0, 0, -1))                                               # the 6-pin aircraft plug
pl.lz([(0.0, -0.17), (0.03, -0.17), (0.03, -0.12), (0.05, -0.1), (0.05, 0.0), (0.0, 0.0)], 'BK', Matrix.Identity(4), 24)
pl.lz([(0.054, -0.06), (0.058, -0.06), (0.058, 0.006), (0.054, 0.006), (0.054, -0.06)], 'KR', Matrix.Identity(4), 24, caps=False)
pl.tube((-0.06, 0.0, -0.12), (0.06, 0.0, -0.12), 0.01, 'BK', 10, weld=False)
for k, (px, py, rr) in enumerate(((-0.022, 0.012, 0.005), (0.0, 0.016, 0.005), (0.022, 0.012, 0.005), (-0.022, -0.014, 0.005), (-0.006, -0.02, 0.0025), (0.01, -0.02, 0.0025))):
    pl.tube((px, py, -0.002), (px, py, 0.045), rr, 'BR', 10, weld=False)
part('Plug', pl.mesh('Plug', (0, 0, 0)), (HP[0], HP[1], HP[2] - 0.115), 'Frame', 'out of its holster by hand into the aircraft ground power socket')
ik('Plug', (HP[0], HP[1], HP[2] - 0.2), 'hand', 'Plug'); ctrl('Power plug', 'Plug', 'Plug', 'take to the aircraft socket and push in; out again after')
valve('Button_Start', (-0.17, PY, CZ - 0.27), N, 'Frame', 'button', 'KG', 'power on to the aircraft')
valve('Button_Stop', (-0.09, PY, CZ - 0.27), N, 'Frame', 'button', 'BK', 'power off')
valve('KeySwitch', (0.0, PY, CZ - 0.27), N, 'Frame', 'knob', 'BK', 'unit on / off')
valve('EStop', (0.2, PY, CZ - 0.28), N, 'Frame', 'mushroom', 'KR', 'emergency stop')
valve('Switch_Mode', (0.1, PY, CZ - 0.27), N, 'Frame', 'toggle', 'BK', '28 V DC / 400 Hz output select')
ik('Push_L', (-0.15, CY1 + 0.12, Z0 + 0.62), 'push', 'Frame'); ik('Push_R', (0.15, CY1 + 0.12, Z0 + 0.62), 'push', 'Frame')
ik('Operator', (0, CY0 - 0.5, 0), 'stand', 'Frame')
finish('HangarGPU', 'hangar ground power unit, electric 400 Hz',
       {'power_kva': 90, 'output': '115/200 V 400 Hz', 'cable': '20 m, 70 mm2, 6-pin aircraft plug', 'castors_mm': 200},
       {'x_min': -0.5, 'x_max': 0.5, 'y_min': CY0 - 0.03, 'y_max': CY1 + 0.15, 'note': 'pushed by hand; cable reaches 20 m'},
       'No free photo found; published sizes of mobile 90 kVA 400 Hz frequency converters')
