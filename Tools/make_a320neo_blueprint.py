# A320neo exterior reference blueprint (gear down), drawn only from Art/A320/blueprint/a320neo-data.json
# (sources in Art/A320/blueprint/SOURCES.md). Four 3840x2160 sheets to docs/Pictures/a320neo-blueprint/:
#   ext-side.png, ext-top.png, ext-front.png, ext-details.png
# One 3D description of the airframe (x aft from the nose tip, y starboard, z up from the ground at MRW fwd CG) is
# projected into every view, so the outlines line up across the sheets. Each sheet has ONE scale (stated in its
# title block). Bright lines/values = measured (printed in a source); dim lines/values = est. (scaled from the
# Airbus drawings, photos or standards). No logos, no livery.
#   python Tools/make_a320neo_blueprint.py
import json, math, os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Circle, Ellipse, Rectangle, FancyBboxPatch
from scipy.interpolate import PchipInterpolator

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = json.load(open(os.path.join(ROOT, 'Art', 'A320', 'blueprint', 'a320neo-data.json')))
OUT = os.path.join(ROOT, 'docs', 'Pictures', 'a320neo-blueprint')
os.makedirs(OUT, exist_ok=True)
BG, INK, DIM, EST, ACC, HID = '#000000', '#e8edf2', '#9fd0ff', '#6d7884', '#f2c230', '#56606a'
plt.rcParams.update({'font.family': 'DejaVu Sans', 'text.color': INK, 'font.size': 9})
G, W, T, E, DR = D['gear_down'], D['wing'], D['tail'], D['engines'], D['doors']
O = D['overall']; GC = G['ground_clearances_table']

# ---------------------------------------------------------------- airframe (one 3D description)
def pchip(pts):
    a = np.array(pts); return PchipInterpolator(a[:, 0], a[:, 1])
# fuselage side profile from the traced side silhouette (S1 p48) and the section summary
ZTOP = pchip([(0, 3.09), (.2, 3.57), (.66, 3.92), (1.34, 4.38), (2.04, 4.92), (2.92, 5.39), (3.83, 5.67), (4.7, 5.83), (5.61, 5.91), (6, 5.92),
              (25, 5.92), (28.68, 5.96), (30.2, 5.94), (33.0, 5.70), (35.38, 5.58), (36.86, 5.45), (37.57, 5.29)])
ZBOT = pchip([(0, 3.09), (.07, 2.85), (.28, 2.6), (.64, 2.39), (1.11, 2.21), (1.79, 2.02), (3.3, 1.81), (4.75, 1.78), (23.86, 1.80), (25.37, 1.88),
              (26.62, 2.0), (28.24, 2.27), (30, 2.66), (32.18, 3.14), (35, 3.93), (36.72, 4.4), (37.57, 4.76)])
HW = pchip([(0, 0), (.06, .22), (.31, .5), (.75, .84), (1.12, 1.05), (2.58, 1.58), (3.72, 1.83), (4.69, 1.96), (5.46, 1.98), (27.96, 1.98),
            (28.7, 1.9), (30.31, 1.66), (32.06, 1.34), (35, .85), (37, .4), (37.41, .26), (37.57, .12)])
L = O['length']; XS = np.linspace(0, L, 400)
BFX = (10.4, 21.6)                                   # belly fairing extent (est., side silhouette p48), bottom BF1
def zbelly(x):
    z = ZBOT(x); a, b = BFX
    if a < x < b: z = min(z, 1.62 + (ZBOT(x) - 1.62) * (abs(x - 16.3) / (16.3 - a if x < 16.3 else b - 16.3)) ** 2.2)
    return z
# wing: straight LE (S1 p49), inner TE unswept to the kink, outer TE to the sharklet root; span scaled +1 % to 35.80
YR, YK, YT = 1.98, W['kink_y_approx'], W['TE_tip_y_at_sharklet_root']
def xle(y): return 13.03 + (y - 2.28) * (20.25 - 13.03) / (16.28 - 2.28)
def xte(y): return 18.89 if y <= YK else 18.89 + (y - YK) * (21.89 - 18.89) / (YT - YK)
DIH = math.radians(W['dihedral_deg'])
def zc(y): return 2.80 + (y - YR) * math.tan(DIH)          # chord plane, gives wingtip lower surface 4.05 (W2)
def tc(y): return 0.152 + (0.108 - 0.152) * (y - YR) / (YT - YR)
def airfoil(c, t, n=60):
    """supercritical-like section (est.): upper, lower as (x/c, z/c) lists from LE to TE"""
    s = (1 - np.cos(np.linspace(0, math.pi, n))) / 2
    th = 5 * t * (.2969 * np.sqrt(s) - .126 * s - .3516 * s ** 2 + .2843 * s ** 3 - .1036 * s ** 4)
    cam = .018 * np.sin(math.pi * s ** 1.4) + .012 * s ** 3
    return s * c, (cam + th * .9) * c, (cam - th * 1.1 + .01 * s ** 2) * c
# sharklet (S1 p49/p52): plan footprint and side outline; tip at W1 6.70
SHK_SIDE = [(20.25, 4.17), (20.9, 4.3), (21.55, 4.55), (22.05, 5.05), (22.69, 6.62), (22.82, 6.70), (23.09, 6.70), (23.32, 5.2), (23.45, 4.62), (23.1, 4.28), (22.3, 4.12), (21.89, 4.10)]
SHK_PLAN = [(20.25, 16.29), (20.45, 16.61), (20.82, 17.09), (21.3, 17.48), (21.81, 17.62), (23.32, 17.9), (23.45, 17.86), (22.38, 17.67), (22.21, 17.45), (21.89, 16.29)]
SHK_FRONT = [(16.29, 4.06), (16.9, 4.18), (17.4, 4.42), (17.72, 4.85), (17.86, 5.6), (17.9, 6.70), (17.80, 6.70), (17.64, 5.62), (17.48, 4.95), (17.15, 4.55), (16.7, 4.32), (16.29, 4.26)]
# fin + tail (S1 p48 traced; fin z stretched 3.4 % so the tip meets VT 11.98)
KF = (11.98 - 5.96) / (11.78 - 5.96)
def fz(z): return 5.96 + (z - 5.96) * KF if z > 5.96 else z
FIN = [(28.68, 5.96), (29.0, 6.02), (29.63, 6.2), (30.03, 6.39), (30.73, 6.91), (34.78, 11.57), (35.18, 11.78), (36.74, 11.78), (35.38, 5.65)]
FIN = [(x, fz(z)) for x, z in FIN]
HTP = [(32.7, 1.85), (35.45, 6.225), (36.69, 6.225), (35.52, .81)]
ENG = E['LEAP_1A']; ENGY = E['centreline_y']; ENGZ = E['nacelle_bottom_ground']['MRW_fwd'] + ENG['nacelle_max_height'] / 2; X0E = E['inlet_lip_x_from_nose']
SEC = np.cumsum([0, ENG['sections']['inlet'], ENG['sections']['fan_cowl'], ENG['sections']['thrust_reverser'], ENG['sections']['primary_nozzle'], ENG['sections']['plug']])
RN = pchip([(0, 1.10), (.12, 1.19), (.35, 1.245), (.9, 1.26), (2.0, 1.25), (2.6, 1.20), (3.4, 1.10), (4.23, .93)])
RC = pchip([(4.23, .64), (4.46, .55)]); RP = pchip([(4.46, .40), (4.9, .30), (5.25, .15), (5.44, .03)])
def nac(s):                                    # outer radius of the visible engine body along s from the inlet lip
    return float(RN(s)) if s <= 4.23 else float(RC(s)) if s <= 4.46 else float(RP(s))
SN = np.linspace(0, 5.44, 120)
AXZ = 0.53; ANZ = 0.36                           # loaded axle heights: main 46x17R20, nose 30x8.8R15 (est. deflection)
MX, NX = G['MLG_axle_x_from_nose'], G['nose_to_NLG_axle']; MY, MS = G['MLG_axle_y'], G['main_wheel_spacing'] / 2
MT, NT = G['main_tyre'], G['nose_tyre']
MPZ = AXZ + G['MLG_lengths'][1]                  # MLG pivot height (axle + 2.265, est. reading of S1 fig)

# ---------------------------------------------------------------- drawing helpers
class Sheet:
    def __init__(s, wm, title, sub):
        s.f = plt.figure(figsize=(25.6, 14.4), dpi=150, facecolor=BG); s.a = s.f.add_axes((0, 0, 1, 1)); s.a.set_facecolor(BG); s.a.axis('off')
        s.wm, s.hm = wm, wm * 2160 / 3840; s.a.set_xlim(0, s.wm); s.a.set_ylim(0, s.hm); s.a.set_aspect('equal'); s.title, s.sub = title, sub
        s.k = 3840 / wm
    def poly(s, pts, lw=1.2, c=INK, fill=True, z=2, ls='-', fc=BG, closed=True):
        s.a.add_patch(Polygon(pts, closed=closed, fc=fc if fill else 'none', ec=c, lw=lw, zorder=z, ls=ls, joinstyle='round'))
    def line(s, pts, lw=.8, c=INK, z=3, ls='-'):
        p = np.array(pts); s.a.plot(p[:, 0], p[:, 1], color=c, lw=lw, zorder=z, ls=ls, solid_capstyle='round')
    def text(s, x, y, t, c=INK, fs=9, ha='center', va='center', rot=0, z=9, box=True, w='normal'):
        s.a.text(x, y, t, color=c, fontsize=fs, ha=ha, va=va, rotation=rot, zorder=z, fontweight=w,
                 bbox=dict(fc=BG, ec='none', pad=1.2) if box else None)
    def hdim(s, x1, x2, y, t, est=False, e1=None, e2=None, fs=9, tx=None):
        c = EST if est else DIM
        for xx, ee in ((x1, e1), (x2, e2)):
            if ee is not None: s.line([(xx, ee), (xx, y + (.25 if y > ee else -.25))], .5, c, 6)
        s.a.annotate('', (x1, y), (x2, y), arrowprops=dict(arrowstyle='<|-|>', color=c, lw=.8, mutation_scale=9, shrinkA=0, shrinkB=0), zorder=6)
        s.text((x1 + x2) / 2 if tx is None else tx, y, t + (' est.' if est else ''), c, fs)
    def vdim(s, x, z1, z2, t, est=False, e1=None, e2=None, fs=9, side=0, tz=None):
        c = EST if est else DIM
        for zz, ee in ((z1, e1), (z2, e2)):
            if ee is not None: s.line([(ee, zz), (x + (.25 if x > ee else -.25), zz)], .5, c, 6)
        s.a.annotate('', (x, z1), (x, z2), arrowprops=dict(arrowstyle='<|-|>', color=c, lw=.8, mutation_scale=9, shrinkA=0, shrinkB=0), zorder=6)
        zt = (z1 + z2) / 2 if tz is None else tz
        if side == 0: s.text(x, zt, t + (' est.' if est else ''), c, fs, rot=90)
        else: s.text(x + side * .15, zt, t + (' est.' if est else ''), c, fs, ha='left' if side > 0 else 'right', box=True)
    def lab(s, x, y, t, tx, ty, est=False, fs=8.5):
        c = EST if est else ACC
        s.a.annotate('', (x, y), (tx, ty), arrowprops=dict(arrowstyle='-', color=c, lw=.6, shrinkA=0, shrinkB=2), zorder=8)
        s.a.plot([x], [y], 'o', color=c, ms=2.5, zorder=8)
        s.text(tx, ty, t + (' (est.)' if est else ''), c, fs, ha='left' if tx >= x else 'right')
    def block(s, scale_note, notes=()):
        a, wm, hm = s.a, s.wm, s.hm; bw, bh = wm * .29, hm * .205; x0, y0 = wm - bw - wm * .008, hm * .012
        a.add_patch(Rectangle((x0, y0), bw, bh, fc=BG, ec=INK, lw=1.4, zorder=20))
        r = bh / 8
        rows = [('A320neo - reference blueprint', 15, 'bold'), (s.title, 12, 'bold'), (s.sub, 9, 'normal'), (scale_note, 9, 'normal'),
                ('Units: metres. Ground = MRW, forward CG (17 %). Engine shown: CFM LEAP-1A.', 8.5, 'normal'),
                ('Data: Art/A320/blueprint/a320neo-data.json  |  Sources: S1 Airbus A320 AC Rev Jul 01/26; S2-S5 (SOURCES.md)', 7.5, 'normal'),
                ('Airport Dynasty Pro  |  drawn 2026-11-27 by Tools/make_a320neo_blueprint.py  |  no logo, no livery', 7.5, 'normal')]
        for i, (t, fs, w) in enumerate(rows):
            a.text(x0 + bw * .02, y0 + bh - r * (i + .75), t, color=INK, fontsize=fs, fontweight=w, va='center', zorder=21)
            if i in (0, 2, 4): a.plot([x0, x0 + bw], [y0 + bh - r * (i + 1.25)] * 2, color=HID, lw=.6, zorder=21)
        # legend
        ly = y0 + r * .55; lx = x0 + bw * .02
        a.plot([lx, lx + bw * .05], [ly, ly], color=INK, lw=1.4, zorder=21); a.text(lx + bw * .06, ly, 'measured / printed in a source', color=INK, fontsize=8, va='center', zorder=21)
        a.plot([lx + bw * .42, lx + bw * .47], [ly, ly], color=EST, lw=1.4, zorder=21); a.text(lx + bw * .48, ly, 'est. = estimated', color=EST, fontsize=8, va='center', zorder=21)
        a.plot([lx + bw * .70, lx + bw * .75], [ly, ly], color=HID, lw=1.2, ls='--', zorder=21); a.text(lx + bw * .76, ly, 'hidden / far side', color=HID, fontsize=8, va='center', zorder=21)
        for i, t in enumerate(notes):
            a.text(wm * .008, hm * .012 + i * hm * .022, t, color=EST, fontsize=8, va='bottom', zorder=21)
    def save(s, name):
        s.f.savefig(os.path.join(OUT, name), facecolor=BG, dpi=150); plt.close(s.f); print('wrote', name)

def rrect(s, cx, cz, w, h, r, c=INK, lw=.7, z=5, fill=True, ls='-'):
    s.a.add_patch(FancyBboxPatch((cx - w / 2 + r, cz - h / 2 + r), w - 2 * r, h - 2 * r, boxstyle='round,pad=%f' % r, fc=BG if fill else 'none', ec=c, lw=lw, zorder=z, ls=ls))

def wheel_side(s, cx, cz, dia, rim_in, c=INK, z=7, hub=True):
    R = dia / 2; s.a.add_patch(Circle((cx, cz), R, fc=BG, ec=c, lw=1.1, zorder=z))
    rr = rim_in * .0254 / 2 + .02
    s.a.add_patch(Circle((cx, cz), rr, fc='none', ec=c, lw=.7, zorder=z)); s.a.add_patch(Circle((cx, cz), rr * .55, fc='none', ec=c, lw=.5, zorder=z))
    if hub:
        for k in range(8 if rim_in > 16 else 6):
            q = 2 * math.pi * k / (8 if rim_in > 16 else 6); s.a.plot([cx + rr * .7 * math.cos(q)], [cz + rr * .7 * math.sin(q)], 'o', color=c, ms=1.4, zorder=z)
    s.a.plot([cx - R, cx + R], [cz - R + .02] * 2, color=c, lw=.4, zorder=z)       # flat contact patch hint

# ---------------------------------------------------------------- SIDE
def side_engine(s, ox=0, oz=0, c=INK, z=4, k=1.0):
    up = [(ox + X0E + t, oz + ENGZ + nac(t)) for t in SN]; lo = [(ox + X0E + t, oz + ENGZ - nac(t)) for t in SN[::-1]]
    s.poly(up + lo, 1.2, c, z=z)
    for t in SEC[1:4]: s.line([(ox + X0E + t, oz + ENGZ - nac(t)), (ox + X0E + t, oz + ENGZ + nac(t))], .6, EST, z + .1)
    s.line([(ox + X0E + .06, oz + ENGZ - 1.02), (ox + X0E + .06, oz + ENGZ + 1.02)], .5, EST, z + .1)  # inlet highlight

def side_wing_port(s, P, c=INK, z=4):
    ys = np.linspace(YR, YT, 40); top = [P(xle(y), zc(y) + .01) for y in ys]
    cx, up, lo = airfoil(xte(YT) - xle(YT), tc(YT))
    tip = [P(xle(YT) + a, zc(YT) + b) for a, b in zip(cx, up)]
    te = [P(xte(y), zc(y) + .02) for y in ys[::-1]]
    cx0, up0, lo0 = airfoil(xte(YR) - xle(YR), tc(YR)); root = [P(xle(YR) + a, zc(YR) + b) for a, b in zip(cx0[::-1], lo0[::-1])]
    s.poly(top + tip + te + root, 1.1, c, z=z)
    s.line([P(xle(YR) + a, zc(YR) + b) for a, b in zip(cx0, up0)], .5, EST, z + .05)

def draw_side():
    s = Sheet(44.0, 'Sheet 1 of 4 - side view (port), gear down', 'Exterior only; cabin and cockpit are another sheet. Cargo doors are on the right side (dashed).')
    ox, oz = 3.6, 5.6                                     # drawing origin: nose tip at ground level
    P = lambda x, z: (ox + x, oz + z); a = s.a
    # ground
    s.line([P(-2, 0), P(L + 1.2, 0)], .9, INK, 1); [s.line([P(x, 0), P(x - .35, -.3)], .4, HID, 1) for x in np.arange(-1.6, L + 1.2, .9)]
    # far side (starboard) pieces first: MLG wheel set, engine
    wheel_side(s, *P(MX + .05, AXZ), MT['dia'], 20, HID, 2, False)
    # fuselage (with fin, belly fairing)
    top = [P(x, ZTOP(x)) for x in XS]; bot = [P(x, zbelly(x)) for x in XS[::-1]]
    s.poly([P(x, z) for x, z in FIN], 1.3, INK, z=2.4)
    s.poly(top + bot, 1.4, INK, z=2.5)
    s.poly([P(x, z) for x, z in FIN[:6]] + [P(x, z) for x, z in FIN[6:]], 1.3, INK, z=2.6)
    s.line([P(x, ZTOP(x) - .002) for x in np.linspace(28.68, 35.38, 30)], .5, INK, 2.7, '--')
    # rudder hinge, APU / tail cone, fin fairing joint
    hl = T['rudder']['hinge_line_xz']; s.line([P(hl[0][0], fz(hl[0][1]) + .17), P(hl[1][0], fz(hl[1][1]))], .7, EST, 2.8)
    s.line([P(x, z) for x, z in [(36.72, 4.4), (36.86, 5.47)]], .6, EST, 2.8)
    s.line([P(33.6, ZBOT(33.6)), P(33.6, ZBOT(33.6) + .75), P(35.6, ZBOT(35.6) + .55), P(35.6, ZBOT(35.6))], .6, EST, 2.8)
    # radome joint, NLG bay doors, belly fairing joint, wing root fairing
    s.line([P(1.54, ZBOT(1.54) + .02), P(1.62, 3.3), P(1.54, ZTOP(1.54) - .02)], .6, EST, 2.8)
    s.line([P(3.4, ZBOT(3.4)), P(3.4, ZBOT(3.4) + .25), P(6.05, ZBOT(6.05) + .25), P(6.05, ZBOT(6.05))], .6, EST, 2.8)
    s.line([P(x, ZBOT(x) + .02) for x in np.linspace(BFX[0], BFX[1], 40)], .5, EST, 2.8, '--')
    s.line([P(x, z) for x, z in [(BFX[0], 2.0), (12.0, 2.95), (13.2, 3.25), (17.8, 3.25), (20.6, 2.95), (BFX[1], 2.0)]], .6, EST, 2.8)
    # cockpit windows (3 per side, est.)
    for q in ([(1.14, 4.2), (1.36, 4.58), (1.56, 4.58), (1.52, 4.18)], [(1.62, 4.18), (1.68, 4.62), (2.12, 4.72), (2.14, 4.17)], [(2.22, 4.17), (2.22, 4.73), (2.62, 4.82), (2.64, 4.18)]):
        s.poly([P(*p) for p in q], .8, INK, z=3)
    # passenger windows
    for x in DR['windows']['x_centers']:
        if any(abs(x - e) < .3 for e in DR['overwing_exits']['x_centers']): continue
        rrect(s, *P(x, DR['windows']['center_z_MRW_fwd']), .23, .33, .07, INK, .6, 3)
    # doors 1L, 4L, overwing exits
    pd = DR['passenger']
    for k, sill in (('1L_1R', GC['D1_fwd_pax_sill'][0]), ('4L_4R', GC['D4_aft_door'][0])):
        d = pd[k]; rrect(s, *P(d['x_center'], sill + d['height'] / 2), d['width'], d['height'], .1, INK, 1.0, 3)
        rrect(s, *P(d['x_center'], sill + d['height'] * .62), .2, .3, .05, INK, .5, 3.1)
        s.line([P(d['x_center'] - .2, sill + .9), P(d['x_center'] + .2, sill + .9)], .9, INK, 3.1)
    ow = DR['overwing_exits']
    for x in ow['x_centers']:
        rrect(s, *P(x, ow['sill_ground_MRW_fwd'] + ow['height'] / 2), ow['width'], ow['height'], .08, INK, .9, 3)
        rrect(s, *P(x, ow['sill_ground_MRW_fwd'] + .55), .23, .33, .07, INK, .5, 3.1)
    # cargo doors (right side, hidden) and bulk door
    cg = DR['cargo']
    for k, code in (('fwd', 'C1'), ('aft', 'C2'), ('bulk', 'C3')):
        d = cg[k]; sill = d['sill_ground_MRW_fwd']; rrect(s, *P(d['x_center'], sill + d['height'] / 2), d['width'], d['height'], .06, HID, .9, 3, False, '--')
    # service panels (est.)
    for x, zz, w_, h_ in ((3.6, 2.25, .35, .3), (27.0, 2.3, .35, .3), (28.0, 2.45, .35, .3)):
        rrect(s, *P(x, zz), w_, h_, .03, EST, .5, 3, False, ':' if x != 28.0 else '-')
    for x in (2.5, 2.85, 3.15): a.plot(*P(x, 3.75), 's', color=EST, ms=2, zorder=3)
    # far wing (starboard) visible over the fuselage? no: only the sharklet tip shows above the fuselage
    # near (port) wing, engine with pylon, sharklet
    side_wing_port(s, P, INK, 4)
    s.poly([P(*p) for p in SHK_SIDE], 1.1, INK, z=4.1)
    s.line([P(21.89, 4.1), P(22.3, 4.6), P(22.9, 6.55)], .5, EST, 4.2)
    pyl = [(12.6, ENGZ + 1.22), (13.5, ENGZ + 1.30), (14.85, zc(ENGY) + .05), (15.8, zc(ENGY) + .12), (17.6, zc(ENGY) - .05), (17.3, zc(ENGY) - .32),
           (16.2, ENGZ + .95), (X0E + SEC[3] - .2, ENGZ + .9)]
    s.poly([P(*p) for p in pyl], 1.0, INK, z=4.3)
    side_engine(s, ox, oz, INK, 4.5)
    # flap track fairings under the near wing (3), aileron, spoiler line (est.)
    for i, yf in enumerate(W['flap_track_fairings_y']):
        xr = xte(yf); zr = zc(yf) - .1; s.poly([P(xr - 1.7, zr + .05), P(xr - 1.1, zr - .25 - .03 * (2 - i)), P(xr + .55, zr - .18), P(xr + .7, zr + .02)], .8, INK, z=4.6)
    # MLG (port) side view
    side_mlg(s, P, MX, 0, INK, 6)
    side_nlg(s, P, NX, INK, 6)
    # HTP seen edge-on (near side over the tail cone), dihedral about 6 deg
    hz0 = GC['HT_htp_tip'][0] - 5.4 * math.tan(math.radians(6))
    s.poly([P(32.7, hz0 - .02), P(35.45, 5.44), P(36.69, 5.47), P(36.69, 5.43), P(35.52, hz0 + .03)], 1.0, INK, z=3.5)
    s.line([P(34.56, hz0 + .02), P(36.33, 5.47)], .5, EST, 3.6)
    # lights (est.): numbered markers, key in the table area
    lt = W['lights']
    LTS = [('L1', lt['beacon_top_x'], ZTOP(lt['beacon_top_x']) + .05, 0, .3), ('L2', lt['beacon_bottom_x'], zbelly(lt['beacon_bottom_x']) - .05, 0, -.3),
           ('L3', lt['wing_scan_light_x'], 3.55, -.35, .2), ('L4', 23.05, 4.45, .4, 0), ('L5', 37.5, 5.02, .35, .2), ('L6', 13.6, 2.45, 0, -.3),
           ('L7', NX + .07, 1.0, -.35, 0), ('L8', 34.2, 5.3, 0, .3)]
    for code, x, zz, dx, dz in LTS:
        a.plot(*P(x, zz), 'o', color=ACC, ms=3.2, zorder=8); s.text(*P(x + dx, zz + dz), code, ACC, 7.5, z=8.5)
    # labels: three rows above the fuselage, two rows below it
    s.lab(*P(2.85, 3.75), 'pitot / static probes', *P(2.3, 8.3), True)
    s.lab(*P(5.04, 5.23), 'door 1L', *P(4.6, 7.1))
    s.lab(*P(8.16, 2.6), 'fwd cargo door (right)', *P(7.4, 8.3))
    s.lab(*P(6.69, DR['windows']['center_z_MRW_fwd'] + .17), '%d windows per side, pitch %.3f (21 in)' % (DR['windows']['per_side_approx'], DR['windows']['pitch']), *P(7.2, 9.5), True)
    s.lab(*P(14.43, 4.77), 'overwing exits (2)', *P(12.6, 7.1))
    s.lab(*P(22.8, 6.6), 'sharklet %.2f high' % W['sharklet']['height'], *P(20.6, 8.3))
    s.lab(*P(22.69, 2.74), 'aft cargo door (right)', *P(24.3, 7.1))
    s.lab(*P(26.29, 2.6), 'bulk door (right)', *P(26.0, 8.3))
    s.lab(*P(29.53, 5.45), 'door 4L', *P(29.9, 7.1))
    s.lab(*P(33.4, 9.0), 'rudder', *P(31.4, 10.6), True)
    s.lab(*P(35.5, hz0 + .3), 'HTP (edge-on)', *P(36.4, 3.0), True)
    s.lab(*P(1.58, 3.0), 'radome joint', *P(-.3, 2.1), True)
    s.lab(*P(3.6, 2.25), 'ground power (right)', *P(.9, .75), True)
    s.lab(*P(4.7, 1.95), 'NLG bay FR9-FR20', *P(6.6, 1.3), True)
    s.lab(*P(20.6, 1.76), 'belly fairing', *P(21.1, 1.2), True)
    s.lab(*P(27.0, 2.3), 'potable water (right)', *P(26.2, .6), True)
    s.lab(*P(28.0, 2.45), 'waste (left)', *P(28.7, 1.4), True)
    s.text(*P(34.6, 4.4), 'APU doors FR80-84', EST, 7, box=False)
    # dimensions
    gy = oz - 1.0
    s.hdim(ox, ox + L, oz + 12.45, 'overall length %.2f' % L, False, oz + 3.2, oz + 5.4)
    s.hdim(ox, ox + NX, gy, 'nose to NLG axle %.2f' % NX, False, oz, oz + ANZ)
    s.hdim(ox + NX, ox + MX, gy, 'wheelbase %.2f' % G['wheelbase'], False, None, oz + AXZ)
    s.hdim(ox, ox + X0E, oz - 1.85, 'inlet lip %.2f' % X0E, False, None, oz + ENGZ - 1.1)
    s.hdim(ox + MX, ox + cg['bulk']['x_center'], gy, 'MLG to bulk door %.2f' % cg['bulk']['aft_of_MLG'], False, None, oz + 2.19)
    s.hdim(ox + X0E, ox + X0E + ENG['length_total'], oz - 1.85, 'LEAP-1A %.2f' % ENG['length_total'], False, None, oz + ENGZ - .55)
    s.vdim(ox + L + 2.0, oz, oz + 11.98, 'height VT %.2f' % GC['VT_fin_top'][0], False, None, ox + 36.74)
    s.vdim(ox + L + 1.1, oz + 5.92, oz + 11.98, 'fin %.2f' % T['fin']['height_above_fuselage_top'], False, ox + 30, None)
    s.vdim(ox - 2.6, oz + 1.78, oz + 5.92, 'fuselage %.2f' % O['fuselage_height_ext'], False, ox + 4.75, ox + 5.6)
    # ground clearances (S1 fig 2-3-0, MRW fwd CG): arrows from the ground, code + value under the ground
    vals = [('CP1', 1.7, 'CP1_cockpit_window'), ('D1', pd['1L_1R']['x_center'], 'D1_fwd_pax_sill'), ('C1', cg['fwd']['x_center'], 'C1_fwd_cargo'),
            ('F1', 9.2, 'F1_fus_bottom_fwd'), ('N1', X0E + 1.4, 'N1_nacelle_bottom'), ('D2', ow['x_centers'][0], 'D2_overwing_exit'),
            ('BF1', 16.3, 'BF1_belly_fairing'), ('F3', 20.0, 'F3_fus_top'), ('W2', 21.2, 'W2_wingtip'), ('C2', 22.4, 'C2_aft_cargo'),
            ('W1', 23.15, 'W1_sharklet_tip'), ('F2', 24.4, 'F2_fus_bottom_aft'), ('C3', cg['bulk']['x_center'], 'C3_bulk'),
            ('D4', pd['4L_4R']['x_center'], 'D4_aft_door'), ('HT', 35.9, 'HT_htp_tip'), ('AP', L - .05, 'AP_tail_apu')]
    for code, x, key in vals:
        h = GC[key][0]
        a.annotate('', P(x, h), P(x, 0), arrowprops=dict(arrowstyle='-|>', color=DIM, lw=.6, mutation_scale=7, shrinkA=0, shrinkB=0), zorder=5.5, alpha=.85)
        s.text(*P(x, -.45), '%s\n%.2f' % (code, h), DIM, 7.5)
    s.text(*P(-1.9, -.45), 'ground\nclearance', DIM, 7.5)
    s.lab(*P(NX + .3, ANZ - .2), 'nose tyres 2 x %s  (dia %.3f)' % (NT['size'], NT['dia']), *P(.4, -2.7))
    s.lab(*P(MX - .45, AXZ - .3), 'main tyres 2 x %s  (dia %.3f)' % (MT['size'], MT['dia']), *P(19.2, -2.7))
    # ground clearance table (all four columns of S1) + light key, top left
    tx0, tz0 = .6, 24.15; cols = ['MRW fwd', 'MRW aft', '45 t', 'jacked']
    a.text(tx0, tz0, 'GROUND CLEARANCES (S1 fig 2-3-0), metres', color=INK, fontsize=10, fontweight='bold', va='center')
    keys = [k for k in GC if k != 'cols']
    for half in range(2):
        bx = tx0 + half * 13.0
        a.text(bx, tz0 - .7, 'code', color=DIM, fontsize=8, va='center', fontweight='bold')
        for j, cn in enumerate(cols): a.text(bx + 5.6 + j * 1.75, tz0 - .7, cn, color=DIM, fontsize=8, va='center', ha='right', fontweight='bold')
        for i, k in enumerate(keys[half * 9:half * 9 + 9]):
            zz = tz0 - 1.25 - i * .5
            a.text(bx, zz, k.split('_')[0] + '  ' + ' '.join(k.split('_')[1:]), color=INK, fontsize=8, va='center')
            for j, v in enumerate(GC[k]): a.text(bx + 5.6 + j * 1.75, zz, '%.2f' % v, color=INK if j == 0 else DIM, fontsize=8, va='center', ha='right')
    a.text(tx0 + 26.0, tz0, 'LIGHTS (positions est.)', color=ACC, fontsize=10, fontweight='bold', va='center')
    key = ['L1  anti-collision beacon, top (red)', 'L2  anti-collision beacon, bottom (red)', 'L3  wing / engine scan light', 'L4  nav light + strobe (wingtip, both sides)',
           'L5  tail nav (white) + strobe', 'L6  landing light (wing root, lower)', 'L7  taxi + take-off lights (on the NLG)', 'L8  logo light (HTP upper surface)']
    for i, t in enumerate(key): a.text(tx0 + 26.0, tz0 - .7 - i * .5, t, color=ACC, fontsize=8, va='center')
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (one scale for the whole sheet).' % s.k,
            ['Side outline traced from Airbus AC fig 2-2-0 (p48); fin stretched 3.4 % so its tip meets the printed 11.98. Heights are the MRW fwd CG column of S1 fig 2-3-0-991-032.',
             'Window x positions vectorised from p48 (two near the overwing exits uncertain). Panel lines, lights, service panels, pylon and gear shapes: est. from photos and S1 gear figures.'])
    s.save('ext-side.png')

def side_mlg(s, P, x, y, c, z):
    R = MT['dia'] / 2
    # leg (main fitting + sliding tube), torque links, side stay seen edge-on, fixed fairing
    s.poly([P(x - .13, AXZ + .25), P(x - .13, MPZ), P(x + .17, MPZ + .05), P(x + .17, AXZ + .25)], 1.0, c, z=z - .2)
    s.line([P(x - .14, AXZ + 1.1), P(x + .18, AXZ + 1.1)], .6, c, z - .1)
    s.poly([P(x - .4, AXZ + .65), P(x - .4, MPZ - .1), P(x - .16, MPZ - .05), P(x - .16, AXZ + .7)], .8, c, z=z - .15)
    s.line([P(x + .18, AXZ + .35), P(x + .42, AXZ + .7), P(x + .18, AXZ + 1.05)], .7, c, z - .1)
    s.line([P(x + .18, AXZ + 1.6), P(x + .9, MPZ - .05)], 1.2, EST, z - .1)
    wheel_side(s, *P(x, AXZ), MT['dia'], 20, c, z)

def side_nlg(s, P, x, c, z):
    r = math.radians(G['NLG_rake_deg']); top = (x + .25 + (2.2 - ANZ) * math.tan(r), 2.2)
    s.poly([P(x + .05, ANZ + .15), P(top[0] - .05, top[1]), P(top[0] + .12, top[1]), P(x + .2, ANZ + .15)], .9, c, z=z - .2)
    s.line([P(x + .22, ANZ + .2), P(x + .45, ANZ + .45), P(x + .27, ANZ + .7)], .6, c, z - .1)
    s.line([P(x + .3, ANZ + .9), P(x + .95, 1.83)], 1.0, EST, z - .1)                  # drag brace
    s.poly([P(x + .3, 1.15), P(x + .15, 1.78), P(x + 1.0, 1.78), P(x + .95, 1.3)], .7, EST, z=z - .3)   # aft door on strut
    s.a.plot(*P(x + .05, 1.0), 's', color=ACC, ms=3, zorder=z)
    wheel_side(s, *P(x, ANZ), NT['dia'], 15, c, z)

# ---------------------------------------------------------------- TOP (plan)
def plan_wing(s, P, sg, c=INK):
    ys = np.linspace(YR, YT, 30)
    out = [P(xle(y), sg * y) for y in ys] + [P(x, sg * y) for x, y in SHK_PLAN] + [P(xte(y), sg * y) for y in ys[::-1]]
    s.poly(out, 1.3, c, z=4)
    s.poly([P(x, sg * y) for x, y in SHK_PLAN], .7, c, False, 4.1)
    # slats (est. joints), front spar of the moving TE parts
    sl = [(2.45, 5.2)] + [(a, b) for a, b in zip([6.25, 9.1, 11.7, 14.2], [9.1, 11.7, 14.2, 16.2])]
    for i, (a, b) in enumerate(sl):
        cs = lambda y: .17 * (xte(y) - xle(y)) * (.95 if i else 1.0)
        s.poly([P(xle(a) + .01, sg * a), P(xle(b) + .01, sg * b), P(xle(b) + cs(b), sg * b), P(xle(a) + cs(a), sg * a)], .7, EST, False, 4.3)
        s.text(*P(xle((a + b) / 2) - .2, sg * (a + b) / 2 + sg * -.2 * 0), str(i + 1), EST, 7, box=False, rot=0, ha='right')
    # flaps (inner and outer), aileron
    fl = W['flaps']; ofl = fl['outer_front_line']
    fy = lambda y: 17.55 if y <= YK else ofl[0][0] + (y - ofl[0][1]) * (ofl[1][0] - ofl[0][0]) / (ofl[1][1] - ofl[0][1])
    for a, b in (fl['inner_y'], fl['outer_y']):
        yy = np.linspace(a, b, 10); s.poly([P(fy(y), sg * y) for y in yy] + [P(xte(y) - .01, sg * y) for y in yy[::-1]], .8, INK, False, 4.3)
    ai = W['aileron']['span_y']; ac = W['aileron']['chord_approx']
    s.poly([P(xte(ai[0]) - ac, sg * ai[0]), P(xte(ai[1]) - ac * .85, sg * ai[1]), P(xte(ai[1]), sg * ai[1]), P(xte(ai[0]), sg * ai[0])], .8, INK, False, 4.3)
    # spoilers 1-5 (S1 p66 stations), est. chord 0.65 ahead of the flap front line
    st = W['spoilers']['ground_extended_A_to_G']['stations_y_est']
    sp = [(st[0], st[1])] + [(st[2], st[3]), (st[3], st[4]), (st[4], st[5]), (st[5], st[6])]
    for i, (a, b) in enumerate(sp):
        s.poly([P(fy(a) - .7, sg * a), P(fy(b) - .6, sg * b), P(fy(b) + .05, sg * b), P(fy(a) + .05, sg * a)], .7, EST, False, 4.4)
        s.text(*P((fy(a) + fy(b)) / 2 - .32, sg * (a + b) / 2), str(i + 1), EST, 7, box=False)
    # flap track fairings (measured stations) + one on the inner flap at the pylon line
    for yf in W['flap_track_fairings_y']:
        x0 = fy(yf); s.poly([P(x0 - .1, sg * (yf - .14)), P(x0 + 2.25, sg * (yf - .1)), P(x0 + 2.55, sg * yf), P(x0 + 2.25, sg * (yf + .1)), P(x0 - .1, sg * (yf + .14))], .8, INK, True, 4.5)
    # wing spars (est.), nav light
    s.line([P(xle(y) + .15 * (xte(y) - xle(y)), sg * y) for y in ys], .4, HID, 4.2, ':')
    s.line([P(xle(y) + .62 * (xte(y) - xle(y)), sg * y) for y in ys], .4, HID, 4.2, ':')
    s.a.plot(*P(W['lights']['nav_wingtip_x'], sg * W['lights']['nav_wingtip_y']), 'o', color=('#ff5555' if sg < 0 else '#55dd66'), ms=3, zorder=5)
    return fy

def draw_top():
    s = Sheet(72.0, 'Sheet 2 of 4 - plan view from above, gear down', 'Starboard wing up. Lower parts (engines aft of the LE, gear) dashed where the wing hides them.')
    ox, oy = 6.0, 18.9; P = lambda x, y: (ox + x, oy + y); a = s.a
    # gear (hidden): tyre footprints dashed
    for sg in (-1, 1):
        for d in (-MS, MS): s.a.add_patch(Rectangle(P(MX - MT['dia'] / 2, sg * MY + d - MT['width'] / 2), MT['dia'], MT['width'], fc='none', ec=HID, lw=.7, ls='--', zorder=4.7))
        s.a.add_patch(Rectangle(P(NX - NT['dia'] / 2, sg * .25 - NT['width'] / 2), NT['dia'], NT['width'], fc='none', ec=HID, lw=.7, ls='--', zorder=6.4))
    # engines (below the wing): full outline dashed, then wing over it; the part ahead of the LE is drawn solid after the wing
    for sg in (-1, 1):
        k = ENG['nacelle_max_width'] / ENG['nacelle_max_height']
        up = [P(X0E + t, sg * ENGY + nac(t) * k) for t in SN]; lo = [P(X0E + t, sg * ENGY - nac(t) * k) for t in SN[::-1]]
        s.poly(up + lo, .8, HID, False, 3.5, '--')
    for sg in (-1, 1): fyf = plan_wing(s, P, sg)
    fy9 = fyf(9.8)
    for sg in (-1, 1):
        k = ENG['nacelle_max_width'] / ENG['nacelle_max_height']; xl = xle(ENGY + 1.3) - X0E
        ss = SN[SN <= xl]; up = [P(X0E + t, sg * ENGY + nac(t) * k) for t in ss]; lo = [P(X0E + t, sg * ENGY - nac(t) * k) for t in ss[::-1]]
        s.poly(up + lo, 1.2, INK, z=4.8)
        for t in SEC[1:2]: s.line([P(X0E + t, sg * ENGY - nac(t) * k), P(X0E + t, sg * ENGY + nac(t) * k)], .5, EST, 4.9)
        s.poly([P(12.6, sg * (ENGY - .27)), P(xle(ENGY) + .2, sg * (ENGY - .27)), P(xle(ENGY) + .2, sg * (ENGY + .27)), P(12.6, sg * (ENGY + .27)), P(12.35, sg * ENGY)], .8, EST, z=4.9)
    # HTP + elevator
    for sg in (-1, 1):
        s.poly([P(x, sg * y) for x, y in HTP], 1.2, INK, z=5)
        hl = T['elevator']['hinge_line_xy']; s.line([P(hl[0][0] + .3, sg * hl[0][1]), P(hl[1][0], sg * min(hl[1][1], 6.2))], .7, INK, 5.1)
    # fuselage, fin on top
    s.poly([P(x, HW(x)) for x in XS] + [P(x, -HW(x)) for x in XS[::-1]], 1.4, INK, z=6)
    s.poly([P(28.68, 0), P(30.73, .2), P(35.18, .16), P(36.74, .02), P(36.74, -.02), P(35.18, -.16), P(30.73, -.2)], 1.0, INK, z=6.2)
    s.line([P(-1.0, 0), P(L + 1.0, 0)], .5, HID, 6.5, '-.')
    # cockpit windows top, emergency exits, doors (outline seen from above: hatch lines on the fuselage edge)
    for sg in (-1, 1):
        s.poly([P(1.13, sg * .35), P(1.45, sg * 1.0), P(2.6, sg * 1.45), P(2.65, sg * 1.2), P(1.6, sg * .78), P(1.3, sg * .1)], .7, INK, False, 6.3)
        for x, w in ((5.04, .81), (29.53, .81), (14.43, .5), (15.28, .5)): s.line([P(x - w / 2, sg * 1.86), P(x + w / 2, sg * 1.86)], 1.6, INK, 6.3)
    # dimensions
    s.hdim(ox, ox + L, oy + 20.75, 'overall length %.2f' % L, False, oy + .1, oy + 4.9)
    sp = O['span_with_sharklets']
    s.vdim(ox + L + 3.0, oy - sp / 2, oy + sp / 2, 'span %.2f (with sharklets)' % sp, False, ox + 23.45, ox + 23.45)
    s.vdim(ox + L + 1.4, oy - O['htp_span'] / 2, oy + O['htp_span'] / 2, 'HTP span %.2f' % O['htp_span'], False, ox + 36.69, ox + 36.69)
    s.hdim(ox, ox + W['rearmost_wingtip_x'], oy + 19.3, 'nose to rearmost wingtip %.2f' % W['rearmost_wingtip_x'], False, oy, oy + 17.9)
    s.hdim(ox + xle(YR), ox + xte(YR), oy + 3.0, 'root chord %.2f' % W['root_chord_at_fuselage'], False, oy + 2.0, oy + 2.0, 8)
    s.hdim(ox + 20.25, ox + 21.89, oy + 15.2, 'tip %.2f' % W['tip_chord_at_sharklet_root'], False, oy + 16.29, oy + 16.29, 8, tx=ox + 19.3)
    s.hdim(ox + 20.23, ox + 23.45, oy + 18.6, 'sharklet %.2f' % W['sharklet']['root_chord'], False, oy + 17.0, oy + 17.86, 8)
    s.hdim(ox, ox + X0E, oy - 7.8, 'inlet lip %.2f' % X0E, False, oy - 1.0, oy - ENGY - 1.3)
    s.vdim(ox + 3.6, oy, oy - ENGY, 'engine CL %.2f' % ENGY, False, None, ox + X0E)
    s.vdim(ox + 9.6, oy - HW(10), oy + HW(10), 'fuselage %.2f' % O['fuselage_width_ext'], False)
    s.vdim(ox + 19.6, oy - HW(19.6), oy - MY, '', False, None, ox + MX + .6); s.text(ox + 19.8, oy - 2.9, 'track/2 %.3f' % MY, DIM, 9, ha='left')
    s.hdim(ox + NX, ox + MX, oy - 3.0, 'wheelbase %.2f' % G['wheelbase'], False, oy - .3, oy - MY)
    s.hdim(ox + 35.45, ox + 36.69, oy + 7.0, 'HTP tip %.2f' % T['htp']['tip_chord'], False, oy + 6.225, oy + 6.225, 8)
    yy = W['flap_track_fairings_y']
    for i, yf in enumerate(yy): s.vdim(ox + 25.2 + 1.4 * i, oy, oy - yf, '%.2f' % yf, False, None, ox + xte(yf) + 2.0, 8)
    s.text(ox + 27.0, oy - 13.1, 'flap track fairings\nfrom the centreline', DIM, 8)
    s.vdim(ox + 29.9, oy - YT, oy, 'TE tip %.2f' % YT, False, ox + 21.9, None, 8)
    s.vdim(ox + 31.4, oy - O['span_with_sharklets'] / 2, oy - YT, '%.2f' % W['sharklet']['lateral_reach'], False, ox + 23.4, None, 8, side=1)
    # sweep
    x1, y1 = xle(4.0), 4.0; s.line([P(x1, y1), P(x1 + 9 * math.tan(math.radians(25)), y1 + 9)], .5, ACC, 7, ':')
    s.text(*P(x1 + .6, 11.4), '25 deg sweep\nat 1/4 chord', ACC, 8, box=False, ha='right')
    # labels (starboard wing)
    s.lab(*P(xle(3.8) + .4, 3.8), 'slat 1 (inboard of pylon)', *P(6.0, 7.5), True)
    s.lab(*P(xle(12.9) + .3, 12.9), 'slats 2-5', *P(10.5, 13.5), True)
    s.lab(*P(fy9 - .35, 9.8), 'spoilers 1-5', *P(9.5, 10.2), True)
    s.lab(*P(18.6, 3.6), 'inner flap (Fowler)', *P(24.0, 3.2))
    s.lab(*P(20.0, 10.6), 'outer flap (Fowler)', *P(24.0, 9.7))
    s.lab(*P(21.4, 15.0), 'aileron', *P(25.0, 14.6))
    s.lab(*P(xte(8.3) + 1.9, 8.3), 'flap track fairings', *P(24.0, 7.2))
    s.lab(*P(22.3, 17.6), 'nav light (green right, red left)', *P(26.2, 16.8), True)
    s.lab(*P(36.0, 3.5), 'elevator', *P(39.5, 4.3))
    s.lab(*P(32.8, .18), 'fin', *P(31.0, 2.9))
    s.lab(*P(12.0, -ENGY - 1.0), 'LEAP-1A nacelle (rest hidden under the wing)', *P(3.0, -10.0))
    s.lab(*P(xle(5.75) - .6, -5.75), 'pylon', *P(6.0, -11.6), True)
    s.lab(*P(MX, -MY - MS), 'MLG tyres (hidden)', *P(26.0, -9.0))
    s.lab(*P(2.0, .8), 'cockpit windows', *P(3.0, 4.6), True)
    s.lab(*P(xle(9.0) + 2.2, -9.0), 'spars', *P(10.0, -14.6), True)
    st = W['spoilers']['ground_extended_A_to_G']['stations_y_est']
    tab = ['Moving surfaces per side (y = metres from the centreline):',
           'slats 1-5: joints y %s (est. from p49)' % ', '.join('%.1f' % v for v in W['slats']['joints_y_approx']),
           'inner flap y %.2f-%.2f, outer flap y %.2f-%.2f (single-slotted Fowler)' % (*W['flaps']['inner_y'], *W['flaps']['outer_y']),
           'spoilers 1-5: stations y %s (est. from p66)' % ', '.join('%.2f' % v for v in st),
           'aileron y %.2f-%.2f, chord about %.2f' % (*W['aileron']['span_y'], W['aileron']['chord_approx']),
           'wing area %.1f m2, AR %.1f, taper %.2f, kink at %.1f %% semi-span' % (W['area_m2'], W['aspect_ratio'], W['taper_ratio'], 100 * W['kink_station_fraction_semispan']),
           'dihedral %.1f deg (est.), root t/c %.3f' % (W['dihedral_deg'], W['thickness_chord_root']),
           'HTP: span %.2f, tip chord %.2f, dihedral about %d deg (est.)' % (O['htp_span'], T['htp']['tip_chord'], T['htp']['dihedral_deg_approx'])]
    for i, t in enumerate(tab): s.text(50.0, 38.6 - i * 1.0, t, INK if i == 0 else EST if 'est' in t else INK, 8.5 if i else 9.5, ha='left', box=False, w='bold' if i == 0 else 'normal')
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (one scale for the whole sheet).' % s.k,
            ['Plan outline traced from Airbus AC fig 2-2-0 sheet 2 (p49), y scaled +1 % so the span meets the printed 35.80.',
             'Slat / spoiler chords, spars, pylon width: est. Flap front line of the outer flap traced from p49.'])
    s.save('ext-top.png')

# ---------------------------------------------------------------- FRONT
def fus_section(cz=3.85, hw=1.975, hh=2.07, n=120, bulge=0.0):
    t = np.linspace(0, 2 * math.pi, n); y = hw * np.cos(t); z = cz + hh * np.sin(t)
    z = np.where(np.sin(t) < 0, cz + hh * np.sin(t) * (1 - .04 * np.cos(t) ** 2), z)
    return list(zip(y, z))

def draw_front():
    s = Sheet(43.0, 'Sheet 3 of 4 - front view, gear down', 'Seen from ahead: the aircraft\'s right (starboard) side is on the left of the sheet.')
    ox, oz = 21.5, 6.6; P = lambda y, z: (ox - y, oz + z); a = s.a
    s.line([P(-20, 0), P(20, 0)], .9, INK, 1)
    for y in np.arange(-19.6, 20, .9): s.line([P(y, 0), P(y + .35, -.3)], .4, HID, 1)
    # fin (behind)
    s.poly([P(-.28, 5.85), P(-.09, 11.98), P(.09, 11.98), P(.28, 5.85)], 1.1, INK, z=2)
    # HTP (dihedral about 6 deg, tip 5.46)
    for sg in (-1, 1):
        zr = GC['HT_htp_tip'][0] - (6.225 - .8) * math.tan(math.radians(6))
        s.poly([P(sg * .8, zr - .12), P(sg * 6.225, GC['HT_htp_tip'][0] - .02), P(sg * 6.225, GC['HT_htp_tip'][0] + .1), P(sg * .8, zr + .22)], 1.0, INK, z=2.1)
    # wings: band between lower and upper surface + LE line
    ys = np.linspace(YR - .3, YT, 40)
    for sg in (-1, 1):
        lo = [P(sg * y, zc(y) - .48 * tc(y) * (xte(y) - xle(y))) for y in ys]; up = [P(sg * y, zc(y) + .62 * tc(y) * (xte(y) - xle(y))) for y in ys[::-1]]
        s.poly(lo + up, 1.2, INK, z=3)
        s.line([P(sg * y, zc(y) + .02) for y in ys], .5, EST, 3.1)
        s.poly([P(sg * y, z) for y, z in SHK_FRONT], 1.1, INK, z=3.2)
        for yf in W['flap_track_fairings_y']:
            zz = zc(yf) - .48 * tc(yf) * (xte(yf) - xle(yf)); s.poly([P(sg * (yf - .14), zz + .05), P(sg * (yf - .1), zz - .35), P(sg * (yf + .1), zz - .35), P(sg * (yf + .14), zz + .05)], .7, INK, z=3.1)
        s.a.plot(*P(sg * W['lights']['nav_wingtip_y'], 4.2), 'o', color=('#55dd66' if sg > 0 else '#ff5555'), ms=3, zorder=4)
    # fuselage cross-section + belly fairing
    s.poly([P(-1.5, 2.0), P(-1.2, 1.62), P(1.2, 1.62), P(1.5, 2.0)], 1.0, INK, z=3.5)
    s.poly([P(y, z) for y, z in fus_section()], 1.4, INK, z=4)
    # nose seen head-on: radome, cockpit windows (est.)
    s.a.add_patch(Ellipse(P(0, 3.15), 1.3, 1.25, fc='none', ec=EST, lw=.7, zorder=4.2))
    for sg in (-1, 1):
        s.poly([P(sg * .08, 4.2), P(sg * .08, 4.62), P(sg * .82, 4.58), P(sg * .9, 4.2)], .8, INK, z=4.2)
        s.poly([P(sg * .98, 4.2), P(sg * .92, 4.56), P(sg * 1.42, 4.38), P(sg * 1.55, 4.05)], .8, INK, z=4.2)
        s.poly([P(sg * 1.62, 3.98), P(sg * 1.5, 4.32), P(sg * 1.78, 4.12), P(sg * 1.85, 3.82)], .8, INK, z=4.2)
        for zz in (2.45, 2.85): s.a.plot(*P(sg * 1.85, zz + .9), 's', color=EST, ms=2, zorder=4.3)
    # MLG (both) and engines
    for sg in (-1, 1):
        front_mlg(s, P, sg, INK, 5)
        pz = zc(ENGY) - .3
        s.poly([P(sg * (ENGY - .27), ENGZ + 1.0), P(sg * (ENGY - .22), pz + .1), P(sg * (ENGY + .22), pz + .1), P(sg * (ENGY + .27), ENGZ + 1.0)], 1.0, INK, z=5.5)
        front_engine(s, P, sg * ENGY, ENGZ, INK, 6)
    front_nlg(s, P, INK, 7)
    # dimensions
    sp = O['span_with_sharklets']
    s.hdim(ox - sp / 2, ox + sp / 2, oz + 13.2, 'span %.2f' % sp, False, oz + 6.8, oz + 6.8)
    s.hdim(ox - O['htp_span'] / 2, ox + O['htp_span'] / 2, oz + 12.3, 'HTP span %.2f' % O['htp_span'], False, oz + 5.6, oz + 5.6)
    s.hdim(ox - MY, ox + MY, oz - 1.4, 'track %.2f' % G['track'], False, oz + AXZ, oz + AXZ)
    s.hdim(ox - MY - MS - MT['width'] / 2, ox + MY + MS + MT['width'] / 2, oz - 2.4, 'outer main tyres %.2f' % G['outer_main_tyre_span'], False, oz, oz)
    s.hdim(ox + MY - MS, ox + MY + MS, oz - 3.35, '%.2f' % G['main_wheel_spacing'], False, oz + .1, oz + .1, tx=ox + MY + 1.4)
    s.hdim(ox - .25, ox + .25, oz - 3.35, '%.2f' % G['nose_wheel_spacing'], False, oz + .1, oz + .1, tx=ox + .9)
    s.hdim(ox - ENGY, ox + ENGY, oz - 4.3, 'engine centrelines %.2f' % (2 * ENGY), False, oz + ENGZ - 1.3, oz + ENGZ - 1.3)
    s.hdim(ox - O['fuselage_width_ext'] / 2, ox + O['fuselage_width_ext'] / 2, oz + 7.0, 'fuselage %.2f' % O['fuselage_width_ext'], False, oz + 3.85, oz + 3.85)
    s.vdim(ox + 19.6, oz, oz + 11.98, 'height VT %.2f' % GC['VT_fin_top'][0], False, ox, ox + .1)
    s.vdim(ox - 19.2, oz, oz + GC['W1_sharklet_tip'][0], 'W1 %.2f' % GC['W1_sharklet_tip'][0], False, None, ox - 17.85)
    s.vdim(ox - 18.3, oz, oz + GC['W2_wingtip'][0], 'W2 %.2f' % GC['W2_wingtip'][0], False, None, ox - 16.29)
    s.vdim(ox - 18.6, oz + 4.27, oz + 6.70, 'sharklet %.2f' % W['sharklet']['height'], False, ox - 17.6, None, side=1)
    s.vdim(ox + 7.75, oz, oz + GC['N1_nacelle_bottom'][0], 'N1 %.2f' % GC['N1_nacelle_bottom'][0], False, None, ox + ENGY, side=1, tz=oz + .9)
    s.vdim(ox + 8.9, oz + 0.46, oz + 0.46 + ENG['nacelle_max_height'], 'nacelle %.2f' % ENG['nacelle_max_height'], False, ox + ENGY, ox + ENGY, side=1)
    s.vdim(ox - 7.4, oz, oz + GC['HT_htp_tip'][0], 'HT %.2f' % GC['HT_htp_tip'][0], False, None, ox - 6.225, side=-1, tz=oz + 4.5)
    s.vdim(ox + 2.6, oz + 1.78, oz + 5.92, 'fuselage %.2f' % O['fuselage_height_ext'], False, ox + 1.0, ox + .3, side=1)
    s.vdim(ox + 2.6, oz, oz + 1.62, 'BF1 %.2f' % GC['BF1_belly_fairing'][0], False, None, ox + 1.2, side=-1, tz=oz + .75)
    s.vdim(ox - 12.4, oz + 5.92, oz + 11.98, 'fin %.2f' % T['fin']['height_above_fuselage_top'], False, ox, ox - .09, side=-1)
    # dihedral
    y1 = 8.0; s.line([P(-y1, zc(y1)), P(-y1 - 6, zc(y1))], .5, ACC, 7, ':'); s.line([P(-y1, zc(y1)), P(-y1 - 6, zc(y1) + 6 * math.tan(DIH))], .5, ACC, 7, ':')
    s.text(*P(-y1 - 6.2, zc(y1) + .25), 'dihedral %.1f deg (est.)' % W['dihedral_deg'], EST, 8, ha='left', box=False)
    s.lab(*P(-2.0, 4.6), 'cockpit windows', *P(-5.5, 9.2), True)
    s.lab(*P(.4, 2.6), 'radome', *P(3.5, 9.8), True)
    s.lab(*P(-ENGY - .5, ENGZ + .5), 'LEAP-1A: fan %.2f, nacelle %.2f x %.2f' % (ENG['fan_diameter'], ENG['nacelle_max_width'], ENG['nacelle_max_height']), *P(-8.6, 8.2))
    s.lab(*P(-MY, AXZ + 1.5), 'MLG leg + fixed fairing', *P(-2.2, -5.1))
    s.lab(*P(-.25, ANZ + .7), 'NLG, 9 deg rake', *P(1.6, -5.1))
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (one scale for the whole sheet).' % s.k,
            ['Fuselage section: superellipse from the printed 3.95 x 4.14; wing thickness from root t/c 0.152 to tip about 0.108 (est.); dihedral is the quoted 5.1 deg (est.).',
             'Gear track, tyre spans, sharklet, heights: printed in S1 fig 2-2-0 and 2-3-0 (MRW fwd CG).'])
    s.save('ext-front.png')

def front_engine(s, P, y, z, c, zo, lab=False):
    k = ENG['nacelle_max_width'] / ENG['nacelle_max_height']
    s.a.add_patch(Ellipse(P(y, z), 2 * 1.26 * k, 2 * 1.26, fc=BG, ec=c, lw=1.3, zorder=zo))
    s.a.add_patch(Ellipse(P(y, z), 2 * 1.10 * k, 2 * 1.10, fc='none', ec=c, lw=.8, zorder=zo))
    s.a.add_patch(Circle(P(y, z), ENG['fan_diameter'] / 2, fc='none', ec=EST, lw=.6, zorder=zo))
    for i in range(18):
        q = 2 * math.pi * i / 18; q2 = q + .25
        s.line([P(y + .3 * math.cos(q), z + .3 * math.sin(q)), P(y + .97 * math.cos(q2), z + .97 * math.sin(q2))], .35, EST, zo + .1)
    s.a.add_patch(Circle(P(y, z), .3, fc=BG, ec=c, lw=.8, zorder=zo + .2))

def front_mlg(s, P, sg, c, zo):
    y = sg * MY; R = MT['dia'] / 2
    s.poly([P(y - .13, AXZ + .2), P(y - .13, MPZ), P(y + .13, MPZ), P(y + .13, AXZ + .2)], 1.0, c, z=zo)
    s.line([P(y - .45, AXZ), P(y + .45, AXZ)], 1.6, c, zo + .1)
    s.line([P(y - sg * .1, AXZ + 1.7), P(sg * 2.05, 2.55)], 1.2, EST, zo - .1)              # side stay to the fuselage
    s.poly([P(y + sg * .16, AXZ + .9), P(y + sg * .16, MPZ - .1), P(y + sg * .26, MPZ - .1), P(y + sg * .26, AXZ + .9)], .8, c, z=zo + .2)  # leg fairing
    for d in (-MS, MS): rrect(s, *P(y + d, AXZ), MT['width'], MT['dia'] - .02, .12, c, 1.1, zo + .3)
    for d in (-MS, MS):
        for e in (-.11, 0, .11): s.line([P(y + d + e, AXZ - R + .08), P(y + d + e, AXZ + R - .08)], .3, EST, zo + .4)

def front_nlg(s, P, c, zo):
    s.poly([P(-.09, ANZ + .1), P(-.09, 1.95), P(.09, 1.95), P(.09, ANZ + .1)], 1.0, c, z=zo)
    s.line([P(-.3, ANZ), P(.3, ANZ)], 1.4, c, zo + .1)
    for d in (-.25, .25):
        rrect(s, *P(d, ANZ), NT['width'], NT['dia'] - .02, .07, c, 1.1, zo + .2)
        for e in (-.055, .055): s.line([P(d + e, ANZ - .3), P(d + e, ANZ + .3)], .3, EST, zo + .3)
    for sg in (-1, 1): s.poly([P(sg * .5, 1.62), P(sg * .55, 1.0), P(sg * .58, 1.62)], .7, EST, z=zo - .2)   # aft doors on strut, open

# ---------------------------------------------------------------- DETAILS
def draw_details():
    s = Sheet(16.5, 'Sheet 4 of 4 - details: landing gear, nacelle, wing sections', 'Main and nose gear down (static, MRW), LEAP-1A nacelle half-section, wing root and tip sections.')
    a = s.a
    # --- MLG side (port, looking inboard) at (1.9, 4.6) ground
    gx, gz = 2.3, 5.3; P = lambda x, z: (gx + x, gz + z)
    s.line([P(-1.2, 0), P(1.4, 0)], .8, INK, 1); s.text(*P(.1, 3.65), 'MAIN GEAR - side', INK, 11, w='bold', box=False)
    s.poly([P(-.15, AXZ + .25), P(-.15, MPZ + .35), P(.19, MPZ + .4), P(.19, AXZ + .25)], 1.1, INK, z=3)        # main fitting
    s.poly([P(-.11, AXZ + .1), P(-.11, AXZ + 1.0), P(.15, AXZ + 1.0), P(.15, AXZ + .1)], 1.0, INK, z=3.1)       # sliding tube
    s.line([P(-.16, AXZ + 1.0), P(.2, AXZ + 1.0)], 1.3, INK, 3.2); s.line([P(-.16, AXZ + 1.12), P(.2, AXZ + 1.12)], .6, INK, 3.2)
    s.line([P(.18, AXZ + .25), P(.5, AXZ + .65), P(.18, AXZ + 1.05)], 1.0, INK, 3.3)                        # torque links
    for p in (P(.18, AXZ + .25), P(.5, AXZ + .65), P(.18, AXZ + 1.05)): a.add_patch(Circle(p, .03, fc=BG, ec=INK, lw=.7, zorder=3.4))
    s.poly([P(-.5, AXZ + .75), P(-.5, MPZ + .2), P(-.2, MPZ + .25), P(-.2, AXZ + .8)], .9, INK, z=2.9)          # fixed fairing
    s.poly([P(-.5, MPZ + .2), P(-.55, MPZ + .55), P(-.2, MPZ + .6), P(-.2, MPZ + .25)], .8, EST, z=2.9)          # hinged fairing
    s.line([P(.2, AXZ + 1.55), P(1.15, MPZ + .1)], 2.0, EST, 3); s.line([P(.2, AXZ + 1.75), P(.9, MPZ + .5)], 1.0, EST, 3)   # side stay / lock stay
    s.line([P(-1.15, MPZ + .38), P(1.35, MPZ + .38)], .7, HID, 2, '--'); s.text(*P(1.0, MPZ + .25), 'wing / gear rib', HID, 7.5, box=False, ha='left')
    a.add_patch(Circle(P(0, MPZ + .3), .07, fc=BG, ec=INK, lw=1, zorder=3.5))
    wheel_side(s, *P(0, AXZ), MT['dia'], 20, INK, 4)
    a.add_patch(Circle(P(0, AXZ), .3, fc='none', ec=EST, lw=.6, ls='--', zorder=4.1))
    s.vdim(P(-1.0, 0)[0], gz, gz + MT['dia'], 'tyre %.3f' % MT['dia'], False, gx - .58, gx - .58, 8)
    s.vdim(P(-.85, 0)[0] + 1.85 + .55, gz + AXZ, gz + MPZ, 'axle to pivot %.3f' % G['MLG_lengths'][1], False, gx + .1, gx + .1, 8)
    s.text(*P(-.05, -.25), 'stroke %.2f  |  axle %.2f (est. loaded)  |  carbon brakes' % (G['MLG_stroke'], AXZ), DIM, 7.5, box=False)
    s.lab(*P(-.35, 1.9), 'fixed fairing', *P(-1.25, 2.6)); s.lab(*P(-.38, MPZ + .45), 'hinged fairing', *P(-1.25, 3.2), True)
    s.lab(*P(.4, AXZ + .6), 'torque links', *P(.85, .35)); s.lab(*P(.7, 2.35), 'side stay + lock stay', *P(.95, 3.45), True)
    s.lab(*P(.02, AXZ + 1.06), 'shock absorber', *P(.65, 2.0))
    # --- MLG front (looking aft) at (5.4, gz)
    hx = 6.0; Q = lambda y, z: (hx + y, gz + z)
    s.line([Q(-1.2, 0), Q(1.2, 0)], .8, INK, 1); s.text(*Q(0, 3.65), 'MAIN GEAR - front (left gear)', INK, 11, w='bold', box=False)
    s.poly([Q(-.14, AXZ + .15), Q(-.14, MPZ + .35), Q(.14, MPZ + .35), Q(.14, AXZ + .15)], 1.1, INK, z=3)
    s.line([Q(-.55, AXZ), Q(.55, AXZ)], 2.0, INK, 3.2)
    s.poly([Q(-.3, AXZ + .9), Q(-.3, MPZ + .3), Q(-.18, MPZ + .3), Q(-.18, AXZ + .9)], .9, INK, z=3.1)
    s.line([Q(.1, AXZ + 1.65), Q(1.25, MPZ + .2)], 2.0, EST, 2.9); s.text(*Q(1.05, MPZ + .55), 'to fuselage', EST, 7.5, box=False)
    for d in (-MS, MS):
        rrect(s, *Q(d, AXZ), MT['width'], MT['dia'] - .02, .12, INK, 1.2, 4)
        for e in (-.12, -.04, .04, .12): s.line([Q(d + e, AXZ - .5), Q(d + e, AXZ + .5)], .35, EST, 4.1)
        s.poly([Q(d - .08 * np.sign(d), AXZ - .3), Q(d - .2 * np.sign(d), AXZ - .3), Q(d - .2 * np.sign(d), AXZ + .3), Q(d - .08 * np.sign(d), AXZ + .3)], .6, EST, z=4.2)
    s.hdim(hx - MS, hx + MS, gz - .45, 'wheel spacing %.3f' % G['main_wheel_spacing'], False, gz + .02, gz + .02, 8)
    s.hdim(hx - MS - MT['width'] / 2, hx - MS + MT['width'] / 2, gz + 1.35, 'tyre width %.3f' % MT['width'], False, gz + AXZ + .5, gz + AXZ + .5, 8)
    s.text(*Q(0, -.85), '%s, %.1f bar; options %s' % (MT['size'], MT['pressure_bar'], ', '.join(MT['options'])), DIM, 7.5, box=False)
    # --- NLG side and front
    nx = 9.6; R_ = lambda x, z: (nx + x, gz + z)
    s.line([R_(-.9, 0), R_(1.5, 0)], .8, INK, 1); s.text(*R_(.3, 3.65), 'NOSE GEAR - side', INK, 11, w='bold', box=False)
    r = math.radians(G['NLG_rake_deg']); tz = 2.25; tx = .08 + (tz - ANZ) * math.tan(r)
    s.poly([R_(-.04, ANZ + .1), R_(tx - .06, tz), R_(tx + .12, tz), R_(.14, ANZ + .1)], 1.1, INK, z=3)
    s.line([R_(-.05, ANZ + .85), R_(.2, ANZ + .85)], 1.2, INK, 3.1)
    s.line([R_(.12, ANZ + .2), R_(.38, ANZ + .45), R_(.17, ANZ + .7)], .9, INK, 3.2)
    s.line([R_(.25, ANZ + 1.05), R_(1.25, 1.95)], 1.6, EST, 2.9)
    s.poly([R_(.3, 1.1), R_(.18, 1.8), R_(1.05, 1.8), R_(1.0, 1.25)], .8, EST, z=2.8)
    s.line([R_(-.8, 1.8), R_(1.45, 1.8)], .7, HID, 2, '--'); s.text(*R_(-.55, 1.95), 'fuselage', HID, 7.5, box=False)
    a.plot(*R_(.02, 1.05), 's', color=ACC, ms=4, zorder=4)
    wheel_side(s, *R_(0, ANZ), NT['dia'], 15, INK, 4)
    s.line([R_(0, ANZ), R_((tz - ANZ) * math.tan(r) * 1.1, tz + .1)], .5, ACC, 2.5, ':')
    s.text(*R_(.62, 2.45), '%d deg rake' % G['NLG_rake_deg'], ACC, 8, box=False)
    s.vdim(nx - .75, gz, gz + NT['dia'], 'tyre %.3f' % NT['dia'], False, nx - .38, nx - .38, 8)
    s.lab(*R_(.06, 1.05), 'taxi / T.O. lights', *R_(-.3, 2.9), True); s.lab(*R_(.75, 1.5), 'aft door on strut', *R_(1.0, 2.7), True)
    s.lab(*R_(.9, 1.6), 'drag brace', *R_(1.1, .95), True)
    s.text(*R_(.3, -.25), 'stroke %.2f  |  retracts forward %d deg' % (G['NLG_stroke'], G['NLG_retraction_deg']), DIM, 7.5, box=False)
    fx = 13.4; F = lambda y, z: (fx + y, gz + z)
    s.line([F(-.9, 0), F(.9, 0)], .8, INK, 1); s.text(*F(0, 3.65), 'NOSE GEAR - front', INK, 11, w='bold', box=False)
    s.poly([F(-.09, ANZ + .08), F(-.09, tz), F(.09, tz), F(.09, ANZ + .08)], 1.1, INK, z=3)
    s.line([F(-.34, ANZ), F(.34, ANZ)], 1.6, INK, 3.2)
    for d in (-.25, .25):
        rrect(s, *F(d, ANZ), NT['width'], NT['dia'] - .02, .07, INK, 1.2, 4)
        for e in (-.06, 0, .06): s.line([F(d + e, ANZ - .3), F(d + e, ANZ + .3)], .35, EST, 4.1)
    for sg in (-1, 1): s.poly([F(sg * .45, 1.8), F(sg * .52, 1.0), F(sg * .58, 1.8)], .8, EST, z=2.8)
    s.line([F(-.8, 1.8), F(.8, 1.8)], .7, HID, 2, '--')
    s.hdim(fx - .25, fx + .25, gz - .45, 'spacing %.2f' % G['nose_wheel_spacing'], False, gz + .02, gz + .02, 8)
    s.hdim(fx + .25 - NT['width'] / 2, fx + .25 + NT['width'] / 2, gz + 1.05, 'width %.3f' % NT['width'], False, gz + ANZ + .3, gz + ANZ + .3, 8, tx=fx + .62)
    s.text(*F(0, -.85), '%s, %.1f bar' % (NT['size'], NT['pressure_bar']), DIM, 7.5, box=False)
    # --- nacelle half section (upper half cut, lower half outside view)
    ex, ez = .9, 2.55; N = lambda x, z: (ex + x, ez + z)
    s.text(*N(2.7, 1.64), 'LEAP-1A NACELLE - section above the centreline, outside below', INK, 11, w='bold', box=False)
    up = [N(t, nac(t)) for t in SN]; lo = [N(t, -nac(t)) for t in SN[::-1]]
    s.poly(lo + [N(0, 0)] + up[:1], 1.1, INK, False, 3, closed=False)
    s.line(lo, 1.1, INK, 3); s.line(up, 1.1, INK, 3)
    inner = pchip([(0, 1.03), (.2, 1.0), (.75, .99), (2.25, .97), (3.4, .9), (4.23, .9)])
    s.line([N(t, float(inner(t))) for t in np.linspace(0, 4.23, 50)], .8, INK, 3)
    s.poly([N(.75, .3), N(.75, .99), N(.95, .99), N(.98, .3)], .9, INK, z=3.2)                  # fan blades
    s.poly([N(.35, 0), N(.75, .3), N(1.1, .34), N(1.1, 0)], .9, INK, z=3.2)                      # spinner
    s.poly([N(1.1, .34), N(1.4, .45), N(2.25, .6), N(3.6, .66), N(4.23, .64), N(4.46, .55), N(4.46, .40), N(4.9, .3), N(5.44, .03), N(5.44, 0), N(1.1, 0)], .9, EST, z=3.1)
    for t in (1.9, 2.4, 2.9, 3.3, 3.7, 4.05): s.line([N(t, .12), N(t, .5)], .4, EST, 3.2)
    for t in SEC[1:]:
        s.line([N(t, -nac(t)), N(t, -nac(t) - .25)], .5, DIM, 3)
    names = ['inlet', 'fan cowl', 'thrust reverser', 'nozzle', 'plug']
    secs = list(ENG['sections'].items()); vals = [secs[0][1], secs[2][1], secs[3][1], secs[4][1], secs[5][1]]
    for i in range(3):
        s.hdim(ex + SEC[i], ex + SEC[i + 1], ez - 1.5, '%s %.2f' % (names[i], vals[i]), False, None, None, 7.5)
    s.hdim(ex + SEC[3], ex + SEC[5], ez - 1.5, 'nozzle %.2f + plug %.2f' % (vals[3], vals[4]), False, None, None, 7.5)
    s.line([(ex + SEC[4], ez - 1.58), (ex + SEC[4], ez - 1.42)], .6, DIM, 6)
    s.hdim(ex, ex + ENG['length_total'], ez - 1.95, 'total %.2f' % ENG['length_total'], False, ez - 1.3, ez - .1, 8)
    s.vdim(ex - .35, ez - 1.26, ez + 1.26, 'nacelle %.2f' % ENG['nacelle_max_height'], False, ex + .9, ex + .9, 8)
    s.vdim(ex + .62, ez, ez + .99, 'fan %.2f dia' % ENG['fan_diameter'], False, None, None, 7.5)
    s.lab(*N(4.4, .93), 'fan nozzle', *N(4.6, 1.42), True); s.lab(*N(2.6, .62), 'core', *N(2.4, 1.42), True)
    s.lab(*N(.85, .7), 'fan blades', *N(1.25, 1.42), True)
    # --- wing sections
    wx, wz = 8.6, 3.3; Wp = lambda x, z: (wx + x, wz + z)
    s.text(*Wp(3.03, .8), 'WING SECTIONS (supercritical shape est.)', INK, 11, w='bold', box=False)
    c0 = W['root_chord_at_fuselage']; cx, up, lo = airfoil(c0, W['thickness_chord_root'], 90)
    s.poly([Wp(a, b) for a, b in zip(cx, up)] + [Wp(a, b) for a, b in zip(cx[::-1], lo[::-1])], 1.2, INK, z=3)
    s.line([Wp(0, .01), Wp(c0, .0)], .4, HID, 3, '-.')
    for f, n in ((.15, 'front spar'), (.62, 'rear spar')):
        i = int(np.argmin(abs(cx - f * c0))); s.line([Wp(cx[i], lo[i]), Wp(cx[i], up[i])], .6, EST, 3.1)
    i = int(np.argmin(abs(cx - .7 * c0))); s.line([Wp(cx[i], up[i] - .05), Wp(c0, .02)], .6, EST, 3.1)
    s.poly([Wp(0, 0), Wp(.06, .08), Wp(.9, .14), Wp(1.0, .02), Wp(.85, -.06), Wp(.06, -.07)], .6, EST, False, 3.2)
    s.hdim(wx, wx + c0, wz - .75, 'root chord %.2f (y %.2f)' % (c0, YR), False, wz - .05, wz, 8)
    tmax = max(up - lo); s.vdim(wx + c0 + .25, wz + float(np.min(lo)), wz + float(np.min(lo)) + tmax, 't/c %.3f' % W['thickness_chord_root'], False, None, None, 7.5, side=1)
    s.lab(*Wp(.5, .1), 'slat', *Wp(.25, .6), True); s.lab(*Wp(5.0, .12), 'flap (Fowler)', *Wp(5.2, .65), True)
    s.lab(*Wp(.15 * c0, .2), 'front spar', *Wp(1.3, -.55), True); s.lab(*Wp(.62 * c0, .2), 'rear spar', *Wp(3.4, -.55), True)
    c1 = W['tip_chord_at_sharklet_root']; tw = math.radians(-3); cx, up, lo = airfoil(c1, .108, 60)
    tpx, tpz = 7.6, 1.4; rot = lambda a, b: (tpx + a * math.cos(tw) - b * math.sin(tw), tpz + a * math.sin(tw) + b * math.cos(tw))
    s.poly([rot(a, b) for a, b in zip(cx, up)] + [rot(a, b) for a, b in zip(cx[::-1], lo[::-1])], 1.2, INK, z=3)
    s.hdim(tpx, tpx + c1, tpz - .55, 'tip chord %.2f (y %.2f)' % (c1, YT), False, tpz - .05, tpz - .1, 8)
    s.text(tpx, tpz + .32, 't/c about 0.108, washout about 3 deg (both est.)', EST, 7.5, ha='left', box=False)
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (one scale for every view here).' % s.k,
            ['Gear: axle and pivot heights, wheel spacings, tyres, strokes and rake printed in S1 (p8, p76-114); leg, link and stay shapes est. from S1 fig 2-9-0 drawings.',
             'Nacelle: section lengths, fan and nacelle sizes printed (S1 p123-141, S2); inner lines, core and plug shape est.  Wing sections: chords printed, profiles est.'])
    s.save('ext-details.png')

if __name__ == '__main__':
    draw_side(); draw_top(); draw_front(); draw_details()
    print('out', OUT)
