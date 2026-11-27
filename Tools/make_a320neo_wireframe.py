# A320neo STRUCTURAL wireframe blueprint - sheets 9 to 12 of the A320neo blueprint set.
# The outlines come from the same single 3D description as sheets 1-4 (Tools/make_a320neo_blueprint.py, imported
# here, data in Art/A320/blueprint/a320neo-data.json); the lattice drawn over them is the real structure:
# fuselage frames at their published stations, stringers, floor beams and seat tracks, keel, pressure bulkheads,
# centre wing box, wing/HTP/fin ribs and spars, gear bays, nacelle hoops.
# Sources for the frame stations and rib numbers: Art/A320/blueprint/SOURCES.md (S11 Airbus SRM fig 51-00-00 fig 6
# sheet 2, S12 Airbus AMM 06-31-53 fig 005A/006A, S13 ATA 06 notes, S1 Airbus AC).
# Four 3840x2160 sheets to docs/Pictures/a320neo-blueprint/:
#   wire-side.png, wire-plan.png, wire-front.png, wire-iso.png
# Bright lines/labels = measured (printed in a source); dim grey = estimated (interpolated or read off a drawing).
#   python Tools/make_a320neo_wireframe.py
import os, sys, math
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_a320neo_blueprint as BP
from matplotlib.patches import Rectangle, Circle

INK, DIM, EST, ACC, HID, BG = BP.INK, BP.DIM, BP.EST, BP.ACC, BP.HID, BP.BG
L = BP.L                                           # overall length 37.57 m, x aft from the nose tip
D, W, T, E, G, DR = BP.D, BP.W, BP.T, BP.E, BP.G, BP.DR

# ---------------------------------------------------------------- frame stations
# Airbus X datum is 2.5425 m forward of the nose tip (checked: MLG axle Airbus X 20.2525 = 17.71 from the nose, S1).
DATUM = 2.5425
# S11 - SRM 51-00-00 fig 6 sheet 2 (A320): section ends, stations in cm
STA_CM = {1: 350, 24: 950, 35: 1537, 47: 2136, 64: 3043, 70: 3366, 77: 3655, 87: 4011}
# S12 - AMM 06-31-53 fig 005A and 006A (A319/A320, stations in mm). The aft sections are the same length on the
# A320 (FR64-FR70 3226 vs 3230 mm, FR70-FR77 2890 vs 2890 mm), so these are mapped onto the A320 anchors above.
AMM_AFT = {64: 26695, 65: 27279, 66: 27860, 67: 28351, 68: 28842, 69: 29362, 70: 29921,
           71: 30321, 72: 30772, 73: 31172, 74: 31572, 75: 31885, 76: 32398, 77: 32811}
AMM_TAIL = {77: 32811, 78: 32923, 79: 33242, 80: 33561, 81: 33931, 82: 34301, 83: 34756,
            84: 35211, 85: 35666, 86: 36121, 87: 36379}
SECTIONS = [(1, 24, '11/12 nose'), (24, 35, '13/14 fwd'), (35, 47, '15 centre'),
            (47, 64, '16/17 aft cabin'), (64, 70, '18'), (70, 77, '19'), (77, 87, '19.1 tail')]

def build_frames():
    mm, meas = {}, set()
    for n, c in STA_CM.items(): mm[n] = c * 10.0; meas.add(n)
    for src, a, b in ((AMM_AFT, 64, 77), (AMM_TAIL, 77, 87)):      # fitted on the two measured A320 anchors
        k = (mm[b] - mm[a]) / (src[b] - src[a])
        for n, v in src.items():
            if n not in mm: mm[n] = mm[a] + (v - src[a]) * k; meas.add(n)
    for a, b, _ in SECTIONS[:4]:                                   # equal pitch inside each section (est.)
        for n in range(a + 1, b): mm[n] = mm[a] + (mm[b] - mm[a]) * (n - a) / (b - a)
    return {n: mm[n] / 1000.0 - DATUM for n in sorted(mm)}, meas

FX, FMEAS = build_frames()                       # FR number -> x from the nose tip (m); FMEAS = measured stations
FRN = sorted(FX)
PITCH = {a: (FX[b] - FX[a]) / (b - a) for a, b, _ in SECTIONS}

# features tied to named frames (identities printed in S1 / S11; the x follows from the stations above)
NLG_BAY = (9, 20)            # S1 2-9: "the NLG retracts forward into a compartment between FR9 and FR20"
DOOR1_FR = (16, 20)          # S1 fig 2-7-0-991-056/057 (airstairs at the fwd pax door)
APU_FR = (80, 84)            # S1 fig 2-7-0-991-032 (APU doors)
CARGO_FR = {'fwd': (24, 34), 'aft': (47, 59), 'bulk': (59, 65)}   # S1 fig 2-6-0-991-003 (FR24A..FR65)
JACK_FR, SHORE_FR, STAY_FR = 8, (15, 33, 42, 63, 64), (73, 74)    # S1 2-14-0 jacking and shoring
RPB_FR, WBOX_FR = 70, (37, 47)                                    # rear pressure bulkhead / centre wing box (est.)
FIN_SPAR_FR, THS_FR = (74, 80), 77                                # fin box and THS pivot frame (est.)

# ---------------------------------------------------------------- fuselage ring geometry
def ring_params(x):
    x = min(max(x, 0.001), L - 0.001)
    zt, zb, hw = float(BP.ZTOP(x)), float(BP.zbelly(x)), float(BP.HW(x))
    zb = max(zb, float(BP.ZBOT(x)))
    return (zt + zb) / 2, (zt - zb) / 2, max(hw, .02)

def ring_pt(x, t):
    """point on the frame ring at station x, ring angle t (0 = starboard side, pi/2 = crown) -> (y, z)"""
    cz, hh, hw = ring_params(x)
    z = cz + hh * math.sin(t)
    if math.sin(t) < 0: z = cz + hh * math.sin(t) * (1 - .04 * math.cos(t) ** 2)
    return hw * math.cos(t), z

def ring(x, n=140):
    cz, hh, hw = ring_params(x)
    return [(y, z) for y, z in BP.fus_section(cz, hw, hh, n)]

NSTR = 36                                         # stringers per section (est. - about 0.19 m pitch, STR1 at the crown)
STR_T = [math.pi / 2 - 2 * math.pi * k / NSTR for k in range(NSTR)]
FLOOR_Z = DR['passenger']['1L_1R']['sill_ground_MRW_fwd']          # 3.38 cabin floor (door-1 sill, measured)
HOLD_Z = DR['cargo']['fwd']['sill_ground_MRW_fwd']                 # 1.98 hold floor (cargo sill, measured)
SEAT_TRACKS = (.34, 1.09, 1.64)                                    # half-widths of the 6 seat tracks (est.)

def floor_hw(x, z=FLOOR_Z):
    cz, hh, hw = ring_params(x)
    if hh <= 0 or abs(z - cz) >= hh: return 0.0
    return hw * math.sqrt(max(0.0, 1 - ((z - cz) / hh) ** 2))

CAB = (FX[1], FX[RPB_FR])                                          # pressurised floor run

# ---------------------------------------------------------------- wing structure
YR, YK, YT = BP.YR, BP.YK, BP.YT
NRIB = 27                                         # 27 ribs per wing (S13); RIB1 at the side-of-fuselage, RIB27 at the tip
RIBY = [YR + (YT - YR) * k / (NRIB - 1) for k in range(NRIB)]      # even spacing fitted to the named ribs (est.)
RIB_NAMED = {9: 'jack pad at the rear spar (S1)', 4: 'shoring (S1)', 13: 'shoring (S1)', 22: 'shoring (S1)',
             14: 'refuel coupling (S1)', 15: 'refuel coupling (S1)', 19: 'overwing gravity cap (S1)',
             20: 'overwing gravity cap (S1)', 24: 'vent / surge (S1)', 25: 'vent / surge (S1)'}
FS_F, RS_F = 0.15, 0.60                           # front / rear spar as a fraction of local chord (est.)
def chord(y): return BP.xte(y) - BP.xle(y)
def fs(y): return BP.xle(y) + FS_F * chord(y)
def rs(y): return BP.xle(y) + RS_F * chord(y)
FTF_Y = W.get('flap_track_fairings_y') or [3.9, 6.2, 8.6, 11.0, 13.4]
SLAT_Y = W['slats']['joints_y_approx']
SPOIL_Y = W['spoilers']['span_y']
AIL_Y = W['aileron']['span_y']
HTP_SPAN = T['htp']['span'] if isinstance(T.get('htp'), dict) and 'span' in T['htp'] else 12.45
NHRIB = 14                                        # 14 HTP ribs per side, 11 fin ribs (S13)
NFRIB = 11

# ---------------------------------------------------------------- sheet with its own title block
class WSheet(BP.Sheet):
    def block(s, scale_note, notes=()):
        a, wm, hm = s.a, s.wm, s.hm; bw, bh = wm * .29, hm * .205; x0, y0 = wm - bw - wm * .008, hm * .012
        a.add_patch(Rectangle((x0, y0), bw, bh, fc=BG, ec=INK, lw=1.4, zorder=20)); r = bh / 8
        rows = [('A320neo - structural wireframe', 15, 'bold'), (s.title, 12, 'bold'), (s.sub, 9, 'normal'),
                (scale_note, 9, 'normal'),
                ('Units: metres, x aft from the nose tip. Frame stations: Airbus X minus 2.5425 m.', 8.5, 'normal'),
                ('Stations S11 SRM 51-00-00 fig 6/2 + S12 AMM 06-31-53 fig 005A/006A; ribs S13; outlines S1 (SOURCES.md)', 7.5, 'normal'),
                ('Airport Dynasty Pro  |  drawn 2026-11-27 by Tools/make_a320neo_wireframe.py  |  no logo, no livery', 7.5, 'normal')]
        for i, (t, fs_, w) in enumerate(rows):
            a.text(x0 + bw * .02, y0 + bh - r * (i + .75), t, color=INK, fontsize=fs_, fontweight=w, va='center', zorder=21)
            if i in (0, 2, 4): a.plot([x0, x0 + bw], [y0 + bh - r * (i + 1.25)] * 2, color=HID, lw=.6, zorder=21)
        ly = y0 + r * .55; lx = x0 + bw * .02
        a.plot([lx, lx + bw * .05], [ly, ly], color=INK, lw=1.4, zorder=21)
        a.text(lx + bw * .06, ly, 'measured station / number', color=INK, fontsize=8, va='center', zorder=21)
        a.plot([lx + bw * .40, lx + bw * .45], [ly, ly], color=EST, lw=1.4, zorder=21)
        a.text(lx + bw * .46, ly, 'est. (interpolated)', color=EST, fontsize=8, va='center', zorder=21)
        a.plot([lx + bw * .70, lx + bw * .75], [ly, ly], color=HID, lw=1.2, ls='--', zorder=21)
        a.text(lx + bw * .76, ly, 'outline / far side', color=HID, fontsize=8, va='center', zorder=21)
        for i, t in enumerate(notes):
            a.text(wm * .008, hm * .012 + i * hm * .022, t, color=EST, fontsize=8, va='bottom', zorder=21)

def fcol(n): return INK if n in FMEAS else EST

# ================================================================ SHEET 9 - side
def draw_side():
    s = WSheet(40.0, 'Sheet 9 of 12 - structural wireframe, side (port)',
               'Frames FR1-FR87 at their stations, stringers, floor and seat tracks, keel, bulkheads, wing box, fin and HTP spars.')
    ox, oz = 1.3, 4.3; P = lambda x, z: (ox + x, oz + z)
    XS = BP.XS
    # --- faint outline (same description as sheets 1-4)
    s.line([P(-1.5, 0), P(L + 1.0, 0)], .8, HID, 1)
    s.poly([P(x, BP.ZTOP(x)) for x in XS] + [P(x, BP.zbelly(x)) for x in XS[::-1]], .9, HID, False, 1.6)
    s.poly([P(x, z) for x, z in BP.FIN], .9, HID, False, 1.6)
    s.poly([P(x, z) for x, z in BP.HTP], .8, HID, False, 1.6, '--')
    BP.side_wing_port(s, P, HID, 1.5)
    s.line([P(BP.X0E + t, BP.ENGZ + BP.nac(t)) for t in BP.SN] + [P(BP.X0E + t, BP.ENGZ - BP.nac(t)) for t in BP.SN[::-1]], .8, HID, 1.5)
    BP.wheel_side(s, *P(G['MLG_axle_x_from_nose'], BP.AXZ), BP.MT['dia'], 20, HID, 1.5, False)
    BP.wheel_side(s, *P(G['nose_to_NLG_axle'], BP.ANZ), BP.NT['dia'], 15, HID, 1.5, False)
    # --- stringers (longitudinal lattice, est.)
    for t in STR_T:
        pts = [P(x, ring_pt(x, t)[1]) for x in np.linspace(.35, L - .25, 220)]
        s.line(pts, .35, EST, 2.0)
    # --- frames
    for n in FRN:
        x = FX[n]; cz, hh, hw = ring_params(x)
        s.line([P(x, cz - hh), P(x, cz + hh)], .75 if n in FMEAS else .5, fcol(n), 2.4)
    # frame numbers: every 5th plus every measured station
    for n in FRN:
        if n % 5 and n not in STA_CM: continue
        x = FX[n]; zt = float(BP.ZTOP(x))
        s.line([P(x, zt), P(x, zt + .35)], .4, fcol(n), 2.5)
        s.text(*P(x, zt + .95), 'FR%d' % n, fcol(n), 7.5, rot=90, box=False)
        if n in STA_CM:
            s.text(*P(x, zt + 2.05), 'STA %.3f' % (FX[n] + DATUM), DIM, 7, rot=90, box=False)
    s.text(*P(.4, 16.3), 'Bright frames = printed stations (SRM section ends, AMM tail frames). FR numbers every 5th frame; Airbus X station printed on the SRM ones.', DIM, 8.5, ha='left', box=False)
    # section breaks
    for i, (a, b, name) in enumerate(SECTIONS):
        s.text(*P((FX[a] + FX[b]) / 2, 14.55 if i % 2 == 0 else 15.05), 'SECTION %s   FR%d-FR%d   pitch %.0f mm%s' %
               (name, a, b, PITCH[a] * 1000, '' if a in (24, 35, 47) else ' est.'), INK if a in STA_CM else EST, 8)
        s.line([P(FX[a], 14.05), P(FX[a], 14.3)], .5, DIM, 2)
        s.line([P(FX[a], 14.3), P(FX[b], 14.3)], .5, DIM, 2)
    s.line([P(FX[87], 14.05), P(FX[87], 14.3)], .5, DIM, 2)
    # --- floor beams, seat tracks, hold floor, keel
    xs = [FX[n] for n in FRN if CAB[0] <= FX[n] <= CAB[1]]
    s.line([P(xs[0], FLOOR_Z), P(xs[-1], FLOOR_Z)], 1.1, INK, 3)
    s.text(*P(xs[-1] - 3.2, FLOOR_Z + .17), 'cabin floor beams + 6 seat tracks, one beam per frame', INK, 7.5, ha='right')
    for x in xs:
        s.line([P(x, FLOOR_Z), P(x, FLOOR_Z - .28)], .4, EST, 3)          # vertical strut to the frame below
    s.line([P(xs[0], FLOOR_Z + .06), P(xs[-1], FLOOR_Z + .06)], .4, EST, 3)
    hx = [FX[n] for n in FRN if FX[n] >= FX[24] and FX[n] <= FX[65]]
    s.line([P(hx[0], HOLD_Z), P(hx[-1], HOLD_Z)], .9, INK, 3)
    s.text(*P(hx[0] + .2, HOLD_Z - .3), 'cargo-hold floor (door sills 1.98 / 2.12 measured)', INK, 7.5, ha='left')
    kb = [FX[WBOX_FR[0]], FX[WBOX_FR[1]]]
    s.line([P(kb[0], float(BP.zbelly(kb[0])) + .14), P(kb[1], float(BP.zbelly(kb[1])) + .14)], 1.6, ACC, 3.2)
    s.lab(*P((kb[0] + kb[1]) / 2, float(BP.zbelly(16)) + .14), 'keel beam', *P(13.5, .55), True)
    # --- pressure bulkheads and wing box
    for n, t in ((1, 'fwd pressure bulkhead FR1'), (RPB_FR, 'rear pressure bulkhead FR%d' % RPB_FR)):
        x = FX[n]; cz, hh, hw = ring_params(x)
        s.line([P(x, cz - hh), P(x, cz + hh)], 2.0, ACC, 3.4)
        for k in range(10):
            z = cz - hh + 2 * hh * (k + .5) / 10; s.line([P(x, z), P(x + .22, z)], .4, ACC, 3.4)
        s.lab(*P(x, cz + hh * .6), t, *P(x + (2.6 if n == 1 else 2.2), cz + hh + (1.3 if n == 1 else 2.6)), n not in FMEAS)
    x1, x2 = FX[WBOX_FR[0]], FX[WBOX_FR[1]]
    s.poly([P(x1, 2.02), P(x2, 2.02), P(x2, 3.30), P(x1, 3.30)], 1.4, ACC, False, 3.5)
    s.text(*P((x1 + x2) / 2, 2.66), 'centre wing box FR%d-FR%d (est.)' % WBOX_FR, ACC, 8)
    # --- door and window cut-out surrounds
    pd = DR['passenger']
    for k, sill in (('1L_1R', pd['1L_1R']['sill_ground_MRW_fwd']), ('4L_4R', pd['4L_4R']['sill_ground_MRW_fwd'])):
        d = pd[k]; w, h = d['width'], d['height']
        for g, lw in ((0, 1.2), (.11, .6)):
            BP.rrect(s, *P(d['x_center'], sill + h / 2), w + 2 * g, h + 2 * g, .10, INK, lw, 4, False)
    ow = DR['overwing_exits']
    for x in ow['x_centers']:
        for g, lw in ((0, 1.1), (.10, .55)):
            BP.rrect(s, *P(x, ow['sill_ground_MRW_fwd'] + ow['height'] / 2), ow['width'] + 2 * g, ow['height'] + 2 * g, .08, INK, lw, 4, False)
    cg = DR['cargo']
    for k in ('fwd', 'aft', 'bulk'):
        c = cg[k]
        for g, lw, ls in ((0, 1.0, '--'), (.10, .5, '--')):
            BP.rrect(s, *P(c['x_center'], c['sill_ground_MRW_fwd'] + c['height'] / 2), c['width'] + 2 * g, c['height'] + 2 * g, .08, HID, lw, 4, False, ls)
    for x in DR['windows']['x_centers']:
        if any(abs(x - e) < .3 for e in ow['x_centers']): continue
        BP.rrect(s, *P(x, DR['windows']['center_z_MRW_fwd']), .25, .36, .07, EST, .45, 4, False)
    s.lab(*P(DR['windows']['x_centers'][6], DR['windows']['center_z_MRW_fwd'] + .18), 'window cut-out surround, one bay per frame (0.533 m)', *P(10.0, 7.9), True)
    s.lab(*P(pd['1L_1R']['x_center'], pd['1L_1R']['sill_ground_MRW_fwd'] + 1.9), 'door 1 cut-out, frames FR16 / FR20', *P(5.4, 9.3), False)
    # --- NLG bay, MLG bay, jacking and shoring frames
    for (a, b), z0, z1, t in ((NLG_BAY, 1.55, 2.60, 'NLG bay FR9-FR20'), ((45, 50), 1.70, 2.95, 'MLG bay FR45-FR50 (est.)')):
        s.poly([P(FX[a], z0), P(FX[b], z0), P(FX[b], z1), P(FX[a], z1)], .9, ACC, False, 3.6, '--')
        s.text(*P((FX[a] + FX[b]) / 2, z0 - .28), t, ACC, 7.5)
    s.lab(*P(FX[JACK_FR], float(BP.zbelly(FX[JACK_FR]))), 'fwd jacking point FR8', *P(5.6, .6), False)
    s.lab(*P((FX[73] + FX[74]) / 2, float(BP.zbelly(FX[73]))), 'tail safety stay FR73/FR74', *P(33.5, 1.0), False)
    for n in SHORE_FR:
        s.a.plot([P(FX[n], float(BP.zbelly(FX[n])))[0]], [P(FX[n], float(BP.zbelly(FX[n])))[1]], 'v', color=ACC, ms=3, zorder=6)
    s.text(*P(FX[42], .55), 'shoring cradles FR15 / FR33 / FR42 / FR63-64 (S1)', ACC, 7.5)
    # --- fin box, fin ribs, HTP spars
    fin = [(x, z) for x, z in BP.FIN]
    ftop, fbot = 11.78 * (11.98 / 11.78), 5.96
    for n, lab in zip(FIN_SPAR_FR, ('fin front spar', 'fin rear spar')):
        x = FX[n]
        xt = 34.9 if n == FIN_SPAR_FR[0] else 35.9
        s.line([P(x, float(BP.ZTOP(x))), P(xt, 11.6)], 1.0, EST, 3.6)
        s.text(*P(x + .1, 8.0), '%s FR%d (est.)' % (lab, n), EST, 7.5, rot=74, box=False)
    for k in range(1, NFRIB + 1):                          # 11 fin ribs (count measured, heights est.)
        f = k / (NFRIB + .5); z = 6.1 + (11.9 - 6.1) * f
        xa = 30.6 + (34.9 - 30.6) * f; xb = 35.4 + (35.95 - 35.4) * f
        s.line([P(xa, z), P(xb, z)], .45, EST, 3.5)
    s.lab(*P(33.4, 9.3), 'fin: 11 ribs (count measured), two spars, rudder hinges', *P(27.5, 10.6), True)
    hx0, hx1 = BP.HTP[0][0], BP.HTP[3][0]
    s.line([P(hx0 + .35, 1.95), P(hx1 + .9, 1.95)], 1.0, EST, 3.6)
    s.lab(*P(FX[THS_FR], 2.6), 'THS pivot frame FR%d (est.), HTP front and rear spars' % THS_FR, *P(30.4, 1.4), True)
    s.a.plot([P(FX[THS_FR], 2.35)[0]], [P(FX[THS_FR], 2.35)[1]], 'o', color=ACC, ms=4, zorder=7)
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (one scale for the whole sheet).' % s.k,
            ['Frame stations FR1/24/35/47/64/70/77/87 and FR64-FR87 are printed values; the frames between them are drawn at equal pitch inside each section (est.).',
             'Stringer count and pitch are estimated (about 36 per section, STR1 at the crown). Floor beam depth, keel, wing box and fin spar frames are estimated.'])
    s.save('wire-side.png')

# ================================================================ SHEET 10 - plan
def draw_plan():
    s = WSheet(68.0, 'Sheet 10 of 12 - structural wireframe, plan',
               'Floor grid, 27 wing ribs, spars, flap and slat tracks, hinge lines, HTP ribs, gear bays.')
    ox, oy = 1.8, 19.1; P = lambda x, y: (ox + x, oy + y)
    # --- faint outline
    s.poly([P(x, BP.HW(x)) for x in BP.XS] + [P(x, -BP.HW(x)) for x in BP.XS[::-1]], .9, HID, False, 1.6)
    for sg in (1, -1):
        BP.plan_wing(s, lambda x, y: P(x, y), sg, HID)
        s.poly([P(x, sg * y) for x, y in [(BP.HTP[0][0] + .2, 0), (BP.HTP[1][0] + .35, HTP_SPAN / 2), (BP.HTP[2][0] + .1, HTP_SPAN / 2), (BP.HTP[3][0], 0)]], .8, HID, False, 1.6)
    # --- floor grid
    for n in FRN:
        x = FX[n]; hw = BP.HW(x)
        s.line([P(x, -hw), P(x, hw)], .75 if n in FMEAS else .45, fcol(n), 2.4)
        fw = floor_hw(x)
        if CAB[0] <= x <= CAB[1] and fw > .2:
            s.line([P(x, -fw), P(x, fw)], 1.0, INK, 2.6)             # floor cross-beam
        if n % 5 == 0 or n in STA_CM:
            s.text(*P(x, hw + .55), 'FR%d' % n, fcol(n), 7.5, rot=90, box=False)
    for ty in SEAT_TRACKS:
        for sg in (1, -1):
            s.line([P(CAB[0] + 4.0, sg * ty), P(CAB[1] - .6, sg * ty)], .6, ACC, 2.8)
    s.lab(*P(9.0, SEAT_TRACKS[2]), 'seat tracks, 6 (positions est.); floor cross-beam on every frame', *P(15.0, 12.4), True)
    s.text(*P(24.0, 0), 'cabin centreline', DIM, 7, box=False)
    s.line([P(-1.0, 0), P(L + .8, 0)], .5, DIM, 2, '-.')
    # --- wing ribs and spars
    for sg in (1, -1):
        ys = np.linspace(YR, YT, 60)
        s.line([P(fs(y), sg * y) for y in ys], 1.3, INK, 3.2)
        s.line([P(rs(y), sg * y) for y in ys], 1.3, INK, 3.2)
        for k, y in enumerate(RIBY):
            n = k + 1
            xa, xb = BP.xle(y) + .03, BP.xte(y) - .03
            c = ACC if n in RIB_NAMED else EST
            s.line([P(xa, sg * y), P(xb, sg * y)], .8 if n in RIB_NAMED else .5, c, 3.3)
            if sg > 0: s.text(*P(xa - .55, y), '%d' % n, c, 7, box=False)
        # kink rib
        s.line([P(BP.xle(YK) + .02, sg * YK), P(BP.xte(YK), sg * YK)], 1.3, ACC, 3.4)
        # flap and slat tracks
        for y in FTF_Y:
            s.line([P(rs(y) - .15, sg * y), P(BP.xte(y) + 1.15, sg * y)], 1.0, ACC, 3.5)
        for i in range(len(SLAT_Y) - 1):
            for y in (SLAT_Y[i] + .35, SLAT_Y[i + 1] - .35):
                s.line([P(BP.xle(y) - .25, sg * y), P(BP.xle(y) + .95, sg * y)], .7, EST, 3.5)
        # hinge lines
        s.line([P(rs(SPOIL_Y[0]) + .25, sg * SPOIL_Y[0]), P(rs(SPOIL_Y[1]) + .25, sg * SPOIL_Y[1])], 1.1, DIM, 3.6, '--')
        s.line([P(rs(AIL_Y[0]) + .28, sg * AIL_Y[0]), P(rs(AIL_Y[1]) + .28, sg * AIL_Y[1])], 1.1, DIM, 3.6, '--')
        # HTP: 14 ribs a side, front and rear spar
        for k in range(NHRIB):
            y = (k + .5) * (HTP_SPAN / 2) / NHRIB
            f = y / (HTP_SPAN / 2); xa = 32.95 + (35.55 - 32.95) * f; xb = 35.5 + (36.6 - 35.5) * f
            s.line([P(xa, sg * y), P(xb, sg * y)], .5, EST, 3.3)
        s.line([P(33.1, sg * .1), P(35.62, sg * HTP_SPAN / 2)], 1.0, INK, 3.4)
        s.line([P(35.1, sg * .1), P(36.35, sg * HTP_SPAN / 2)], 1.0, INK, 3.4)
    s.lab(*P(fs(9.0), 9.0), 'front spar, 15 % chord', *P(9.0, 14.2), True)
    s.lab(*P(rs(9.0), 9.0), 'rear spar, 60 % chord (est.); MLG and flap tracks hang on it', *P(21.0, 15.0), True)
    s.lab(*P(BP.xte(YK), YK), 'kink rib, 39.2 % semi-span (measured)', *P(24.0, 5.2), False)
    s.lab(*P(BP.xle(RIBY[8]) + .6, RIBY[8]), 'RIB9: wing jacking pad at the rear spar (measured)', *P(9.0, -13.4), False)
    s.lab(*P(BP.xte(FTF_Y[2]) + .9, FTF_Y[2]), 'flap track fairings, 5 a side (measured y)', *P(26.5, 10.4), False)
    s.lab(*P(BP.xle(14.0) + .4, 14.0), 'slat tracks, 2 per slat', *P(14.0, 17.4), True)
    s.lab(*P(35.0, 5.4), 'HTP: 14 ribs a side (count measured), 2 spars', *P(30.0, 11.6), True)
    s.lab(*P(rs(12.0) + .28, 12.0), 'aileron / spoiler hinge lines', *P(27.5, -15.4), True)
    # --- gear bays
    s.poly([P(FX[NLG_BAY[0]], -.62), P(FX[NLG_BAY[1]], -.62), P(FX[NLG_BAY[1]], .62), P(FX[NLG_BAY[0]], .62)], 1.0, ACC, False, 3.8, '--')
    s.text(*P((FX[NLG_BAY[0]] + FX[NLG_BAY[1]]) / 2, 1.15), 'NLG bay FR9-FR20 (measured)', ACC, 7.5)
    for sg in (1, -1):
        s.poly([P(FX[45], sg * .75), P(FX[50], sg * .75), P(FX[50], sg * 3.35), P(FX[45], sg * 3.35)], 1.0, ACC, False, 3.8, '--')
    s.text(*P(FX[47], 4.0), 'MLG bay and gear doors FR45-FR50 (est.)', ACC, 7.5)
    for sg in (1, -1):
        BP.rrect(s, *P(G['MLG_axle_x_from_nose'], sg * G['MLG_axle_y']), 1.2, .95, .1, HID, .6, 3.9, False, '--')
    # ---- table of the frame stations in the free right-hand third
    tx, ty = 45.5, 36.9; rh = .70
    s.text(tx, ty + .9, 'FRAME STATIONS USED ON SHEETS 9-12', INK, 11, ha='left', w='bold', box=False)
    s.text(tx, ty + .1, 'x = Airbus X station minus 2.5425 m (nose tip = x 0)', DIM, 8.5, ha='left', box=False)
    hdr = [('FR', 0), ('Airbus X (m)', 2.6), ('x from nose', 7.4), ('source', 12.2)]
    for t, dx in hdr: s.text(tx + dx, ty - rh, t, DIM, 8.5, ha='left', box=False)
    s.line([(tx, ty - rh * 1.35), (tx + 18.5, ty - rh * 1.35)], .6, HID, 9)
    rows = [(n, 'SRM 51-00-00 fig 6/2') for n in sorted(STA_CM)]
    rows += [(n, 'AMM 06-31-53 fig 005A/006A') for n in range(65, 88)]
    for i, (n, src) in enumerate(rows):
        yy = ty - rh * (i + 2.3)
        c = INK if n in STA_CM else DIM
        for t, dx in (('FR%d' % n, 0), ('%.3f' % (FX[n] + DATUM), 2.6), ('%.3f' % FX[n], 7.4), (src, 12.2)):
            s.text(tx + dx, yy, t, c, 8, ha='left', box=False)
    yy = ty - rh * (len(rows) + 3.0)
    for i, t in enumerate(['Frames not in this table (FR2-FR23, FR25-FR34, FR36-FR46, FR48-FR63) are drawn at equal',
                           'pitch inside their section: 261 / 534 / 499 / 534 mm - estimated.',
                           'The AMM figures are A319 stations; the aft sections are the same, so they are fitted',
                           'onto the two printed A320 anchors FR64 and FR77 (shift 3.739 m, residual under 5 mm).']):
        s.text(tx, yy - i * .62, t, EST, 8, ha='left', box=False)
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (one scale for the whole sheet).' % s.k,
            ['Rib count (27 a wing, 14 a side on the HTP) is a printed figure; the rib y positions are evenly spaced and fitted to the named ribs (RIB4/9/13/14/15/19/20/22/24/25 from the Airbus AC) - est.',
             'Spar chord fractions, slat track positions, seat track y and the MLG bay frames are estimated; flap track fairing y and the kink station are printed values.'])
    s.save('wire-plan.png')

# ================================================================ SHEET 11 - front / sections
def draw_front():
    s = WSheet(26.0, 'Sheet 11 of 12 - frame sections at four stations',
               'Looking forward: stringers round the ring, floor beam and struts, keel, hold floor, wing box.')
    secs = [(9, 'FR9 - nose, NLG bay'), (30, 'FR30 - forward hold'), (41, 'FR41 - centre wing box'), (72, 'FR72 - aft, section 19')]
    for i, (n, title) in enumerate(secs):
        cx = 3.3 + i * 5.45; x = FX[n]
        P = lambda y, z: (cx - y, 3.4 + z - 1.2)
        cz, hh, hw = ring_params(x)
        s.poly([P(y, z) for y, z in ring(x)], 1.5, fcol(n), False, 3)
        # skin + frame depth (inner line)
        s.poly([P(y * .965, cz + (z - cz) * .965) for y, z in ring(x)], .5, EST, False, 3)
        for t in STR_T:                                           # stringer positions
            y, z = ring_pt(x, t); s.a.plot([P(y, z)[0]], [P(y, z)[1]], 'o', color=EST, ms=2.6, zorder=4)
        s.text(*P(0, cz + hh + .55), title, fcol(n), 10, w='bold')
        s.text(*P(0, cz + hh + .30), 'x %.2f from the nose  |  STA %.3f' % (x, x + DATUM), DIM, 7.5)
        s.text(*P(0, cz - hh - .32), 'frame %.2f wide x %.2f deep' % (2 * hw, 2 * hh), DIM, 7.5)
        # floor beam + struts + seat tracks
        fw = floor_hw(x)
        if fw > .3 and CAB[0] <= x <= CAB[1]:
            s.line([P(-fw, FLOOR_Z), P(fw, FLOOR_Z)], 1.6, INK, 5)
            s.line([P(-fw, FLOOR_Z - .11), P(fw, FLOOR_Z - .11)], .5, EST, 5)
            for sg in (1, -1):
                yb, zb = ring_pt(x, math.atan2(-1.0, sg * 1.25)); yb = sg * abs(yb)
                s.line([P(sg * fw * .62, FLOOR_Z), P(yb, zb)], .9, ACC, 5)   # floor strut
                for ty in SEAT_TRACKS:
                    if ty < fw: s.a.plot([P(sg * ty, FLOOR_Z + .04)[0]], [P(sg * ty, FLOOR_Z + .04)[1]], 's', color=ACC, ms=2.6, zorder=6)
            s.text(*P(0, FLOOR_Z + .20), 'floor beam, seat tracks', ACC, 6.5)
        hwf = floor_hw(x, HOLD_Z)
        if hwf > .3 and FX[24] <= x <= FX[65]:
            s.line([P(-hwf, HOLD_Z), P(hwf, HOLD_Z)], 1.1, INK, 5)
            s.text(*P(0, HOLD_Z + .17), 'hold floor', INK, 6.5)
        # keel beam
        if FX[WBOX_FR[0]] <= x <= FX[WBOX_FR[1]]:
            s.poly([P(-.22, cz - hh + .04), P(.22, cz - hh + .04), P(.22, cz - hh + .40), P(-.22, cz - hh + .40)], 1.2, ACC, False, 5)
            s.text(*P(0, cz - hh + .60), 'keel beam', ACC, 6.5)
            s.poly([P(-1.95, 2.02), P(1.95, 2.02), P(1.95, 3.30), P(-1.95, 3.30)], 1.3, ACC, False, 5)
            s.text(*P(0, 2.66), 'centre wing box (est.)', ACC, 7)
        if x > FX[RPB_FR]:
            s.text(*P(0, cz, ), 'aft of the rear pressure bulkhead FR70:', EST, 7.5)
            s.text(*P(0, cz - .36), 'unpressurised, no floor, no seat tracks', EST, 7.5)
        if n == 9:
            s.poly([P(-.62, cz - hh + .05), P(.62, cz - hh + .05), P(.62, cz - hh + 1.05), P(-.62, cz - hh + 1.05)], 1.0, ACC, False, 5, '--')
            s.text(*P(0, cz - hh + 1.25), 'NLG bay', ACC, 6.5)
    # wing box section (outboard, at RIB9) bottom left
    bx, bz = 2.4, 11.4; Q = lambda a, b: (bx + a, bz + b)
    y9 = RIBY[8]; c9 = chord(y9); t9 = float(BP.tc(y9)) * c9
    s.text(*Q(2.6, 1.15), 'WING BOX SECTION AT RIB9 (chord measured, box est.)', INK, 10, w='bold', box=False)
    cxs, up, lo = BP.airfoil(c9, float(BP.tc(y9)), 80)
    s.poly([Q(a, b) for a, b in zip(cxs, up)] + [Q(a, b) for a, b in zip(cxs[::-1], lo[::-1])], 1.2, HID, False, 3)
    ia = int(np.argmin(abs(cxs - FS_F * c9))); ib = int(np.argmin(abs(cxs - RS_F * c9)))
    s.line([Q(cxs[ia], lo[ia]), Q(cxs[ia], up[ia])], 1.4, INK, 4); s.line([Q(cxs[ib], lo[ib]), Q(cxs[ib], up[ib])], 1.4, INK, 4)
    s.line([Q(cxs[ia], up[ia]), Q(cxs[ib], up[ib])], 1.2, INK, 4); s.line([Q(cxs[ia], lo[ia]), Q(cxs[ib], lo[ib])], 1.2, INK, 4)
    for f in np.linspace(FS_F, RS_F, 7)[1:-1]:                       # upper and lower skin stringers
        i = int(np.argmin(abs(cxs - f * c9)))
        s.a.plot([Q(cxs[i], up[i] - .035)[0]], [Q(cxs[i], up[i] - .035)[1]], 'o', color=EST, ms=2, zorder=5)
        s.a.plot([Q(cxs[i], lo[i] + .035)[0]], [Q(cxs[i], lo[i] + .035)[1]], 'o', color=EST, ms=2, zorder=5)
    s.hdim(bx + cxs[ia], bx + cxs[ib], bz - .62, 'box %.2f (front to rear spar)' % (cxs[ib] - cxs[ia]), True, bz - .1, bz - .1, 8)
    s.hdim(bx, bx + c9, bz - 1.05, 'chord at RIB9 %.2f' % c9, False, bz - .55, bz - .55, 8)
    s.text(*Q(c9 * .5, .34), 'integral fuel tank between the spars', EST, 7.5, box=False)
    # ring angle / stringer note
    s.text(9.6, 13.5, 'STRINGERS: about %d a section, STR1 at the crown, numbering down each side (pitch est. 0.19 m).' % NSTR, EST, 9, ha='left', box=False)
    s.text(9.6, 13.1, 'Frame depth is drawn at 3.5 % of the ring (est.); skin thickness is not to scale.', EST, 9, ha='left', box=False)
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (all four sections and the wing box at the same scale).' % s.k,
            ['Section outlines come from the same superellipse as sheets 1-4 (fuselage 3.95 x 4.14 measured); cabin floor 3.38 and hold floor 1.98 are the printed door sills.',
             'Stringer positions, floor strut angles, keel beam size and the wing box corners are estimated.'])
    s.save('wire-front.png')

# ================================================================ SHEET 12 - isometric
AZ, EL = math.radians(36), math.radians(21)
def ISO(x, y, z):
    u = -x * math.cos(AZ) + y * math.sin(AZ)
    v = (x * math.sin(AZ) + y * math.cos(AZ)) * math.sin(EL) + z * math.cos(EL)
    return u, v

def draw_iso():
    s = WSheet(52.0, 'Sheet 12 of 12 - structural wireframe, isometric',
               'From the left front. Frames, stringers, wing and tail ribs and spars, gear, nacelle hoops.')
    ox, oy = 37.0, 9.2
    P = lambda x, y, z: (ox + ISO(x, y, z)[0], oy + ISO(x, y, z)[1])
    # --- frames (every frame, rings)
    for n in FRN:
        x = FX[n]
        s.line([P(x, y, z) for y, z in ring(x, 80)], .55 if n in FMEAS else .35, fcol(n), 2.5)
    for n in FRN:
        if n % 10 or n == 0: continue
        x = FX[n]; cz, hh, hw = ring_params(x)
        s.text(*P(x, 0, cz + hh + .55), 'FR%d' % n, fcol(n), 8, box=False)
    # --- stringers
    for t in STR_T:
        s.line([P(x, *ring_pt(x, t)) for x in np.linspace(.3, L - .2, 160)], .3, EST, 2.4)
    # --- floor grid and seat tracks
    for n in FRN:
        x = FX[n]; fw = floor_hw(x)
        if CAB[0] <= x <= CAB[1] and fw > .2: s.line([P(x, -fw, FLOOR_Z), P(x, fw, FLOOR_Z)], .5, ACC, 2.8)
    for ty in SEAT_TRACKS:
        for sg in (1, -1):
            s.line([P(x, sg * ty, FLOOR_Z) for x in np.linspace(CAB[0] + 4, CAB[1] - .6, 40)], .4, ACC, 2.8)
    # --- keel + bulkheads
    s.line([P(x, 0, float(BP.zbelly(x)) + .14) for x in np.linspace(FX[WBOX_FR[0]], FX[WBOX_FR[1]], 20)], 1.2, ACC, 3)
    for n in (1, RPB_FR):
        x = FX[n]; s.line([P(x, y, z) for y, z in ring(x, 80)], 1.6, ACC, 3.1)
    # --- wing: ribs, spars, box, sharklet
    for sg in (1, -1):
        ys = np.linspace(YR, YT, 50)
        for f, lw in ((FS_F, 1.2), (RS_F, 1.2)):
            s.line([P(BP.xle(y) + f * chord(y), sg * y, float(BP.zc(y)) + .02) for y in ys], lw, INK, 3.2)
        s.line([P(BP.xle(y), sg * y, float(BP.zc(y)) + .05) for y in ys], .7, HID, 3.1)
        s.line([P(BP.xte(y), sg * y, float(BP.zc(y)) + .02) for y in ys], .7, HID, 3.1)
        for k, y in enumerate(RIBY):
            n = k + 1; c = ACC if n in RIB_NAMED else EST
            cxs, up, lo = BP.airfoil(chord(y), float(BP.tc(y)), 26)
            zc0 = float(BP.zc(y))
            s.line([P(BP.xle(y) + a, sg * y, zc0 + b) for a, b in zip(cxs, up)] +
                   [P(BP.xle(y) + a, sg * y, zc0 + b) for a, b in zip(cxs[::-1], lo[::-1])], .45 if n not in RIB_NAMED else .75, c, 3.2)
        # sharklet: ribs (est.) between the tip chord and the small top chord
        zt0 = float(BP.zc(YT))
        for k in range(6):
            f = k / 5.0
            x0 = BP.xle(YT) + .10 + .95 * f; x1 = BP.xte(YT) - .10 - .55 * f
            zz = zt0 + .10 + 2.30 * f; yy = YT + .30 + .95 * f
            s.line([P(x0, sg * yy, zz), P(x1, sg * yy, zz)], .5 if k else .8, EST if k else HID, 3.2)
        for fx_ in (.12, .95):
            pts = [(BP.xle(YT) + .10 + .95 * (k / 5.) + fx_ * (BP.xte(YT) - BP.xle(YT) - .20 - 1.5 * (k / 5.)),
                    YT + .30 + .95 * (k / 5.), zt0 + .10 + 2.30 * (k / 5.)) for k in range(6)]
            s.line([P(a, sg * b, c) for a, b, c in pts], .5, EST, 3.2)
        # engine hoops
        for t in np.linspace(0, ENG_LEN(), 9):
            r = BP.nac(t); th = np.linspace(0, 2 * math.pi, 44)
            s.line([P(BP.X0E + t, sg * BP.ENGY + r * np.cos(a), BP.ENGZ + r * np.sin(a)) for a in th], .45, EST, 3.3)
        s.line([P(BP.X0E + t, sg * BP.ENGY, BP.ENGZ + BP.nac(t)) for t in BP.SN], .5, HID, 3.3)
        s.line([P(BP.X0E + t, sg * BP.ENGY, BP.ENGZ - BP.nac(t)) for t in BP.SN], .5, HID, 3.3)
        # pylon
        s.line([P(BP.X0E + 1.2, sg * BP.ENGY, BP.ENGZ + 1.2), P(14.6, sg * BP.ENGY, float(BP.zc(BP.ENGY)) - .1)], .6, EST, 3.3)
        s.line([P(BP.X0E + 4.1, sg * BP.ENGY, BP.ENGZ + 1.0), P(15.6, sg * BP.ENGY, float(BP.zc(BP.ENGY)) - .1)], .6, EST, 3.3)
        # main gear (leg + bogie)
        mx, my = G['MLG_axle_x_from_nose'], sg * G['MLG_axle_y']
        s.line([P(FX[47] + .15, sg * 2.1, 2.1), P(mx, my, BP.AXZ)], 1.2, INK, 3.5)
        for dy in (-G['main_wheel_spacing'] / 2, G['main_wheel_spacing'] / 2):
            th = np.linspace(0, 2 * math.pi, 40); R = BP.MT['dia'] / 2
            s.line([P(mx + R * np.cos(a), my + dy, BP.AXZ + R * np.sin(a)) for a in th], .5, HID, 3.5)
    # nose gear
    nx = G['nose_to_NLG_axle']
    s.line([P(nx - .2, 0, 2.3), P(nx, 0, BP.ANZ)], 1.1, INK, 3.5)
    for dy in (-G['nose_wheel_spacing'] / 2, G['nose_wheel_spacing'] / 2):
        th = np.linspace(0, 2 * math.pi, 36); R = BP.NT['dia'] / 2
        s.line([P(nx + R * np.cos(a), dy, BP.ANZ + R * np.sin(a)) for a in th], .5, HID, 3.5)
    # --- fin ribs and spars, HTP
    for k in range(1, NFRIB + 1):
        f = k / (NFRIB + .5); z = 6.1 + (11.9 - 6.1) * f
        xa = 30.6 + (34.9 - 30.6) * f; xb = 35.4 + (35.95 - 35.4) * f
        s.line([P(xa, 0, z), P(xb, 0, z)], .45, EST, 3.4)
    for n, x2 in zip(FIN_SPAR_FR, (34.9, 35.9)):
        s.line([P(FX[n], 0, float(BP.ZTOP(FX[n]))), P(x2, 0, 11.6)], .9, EST, 3.5)
    for sg in (1, -1):
        for k in range(NHRIB):
            y = (k + .5) * (HTP_SPAN / 2) / NHRIB; f = y / (HTP_SPAN / 2)
            xa = 32.95 + (35.55 - 32.95) * f; xb = 35.5 + (36.6 - 35.5) * f
            z = 1.95 + 1.55 * f
            s.line([P(xa, sg * y, z), P(xb, sg * y, z)], .4, EST, 3.4)
        s.line([P(33.1, sg * .1, 1.95), P(35.62, sg * HTP_SPAN / 2, 3.50)], .8, INK, 3.5)
        s.line([P(35.1, sg * .1, 1.95), P(36.35, sg * HTP_SPAN / 2, 3.50)], .8, INK, 3.5)
    # --- ground line and labels
    s.line([P(-1.5, -13, 0), P(-1.5, 13, 0)], .5, HID, 1)
    def labo(pt, t, dx, dy, est=False):
        s.lab(pt[0], pt[1], t, pt[0] + dx, pt[1] + dy, est)
    labo(P(FX[47], -2.6, 1.9), 'rear spar frame FR47 (measured) - MLG attachment', 1.2, -4.8)
    labo(P(FX[1], 0, 3.2), 'FR1 fwd pressure bulkhead (measured)', 1.0, -3.2)
    labo(P(FX[RPB_FR], 0, 4.4), 'FR70 rear pressure bulkhead', -4.0, 3.0, True)
    labo(P(16.0, -8.0, float(BP.zc(8.0))), '27 ribs a wing, front and rear spar', -6.5, -3.2)
    labo(P(BP.X0E + 2.0, -BP.ENGY, BP.ENGZ - BP.nac(2.0)), 'nacelle hoops and pylon members', -1.0, -2.4, True)
    labo(P(FX[30], 0, FLOOR_Z), 'cabin floor grid and seat tracks', 2.0, -4.0, True)
    labo(P(32.6, 0, 9.2), 'fin: 11 ribs, two spars', -2.5, 2.6, True)
    s.text(2.0, 28.5, 'ISOMETRIC: azimuth %d deg, elevation %d deg; no perspective, so every length is at the sheet scale only along its own axis.' % (math.degrees(AZ), math.degrees(EL)), EST, 9, ha='left', box=False)
    s.block('Scale: 1 m = %.1f px along the projected axes on this 3840 x 2160 sheet.' % s.k,
            ['Every frame FR1-FR87 is drawn as a ring at its station; measured stations are the brighter rings.',
             'Stringers, sharklet and fin ribs, pylon members and the gear leg lines are estimated.'])
    s.save('wire-iso.png')

def ENG_LEN(): return float(BP.SN[-1])

if __name__ == '__main__':
    draw_side(); draw_plan(); draw_front(); draw_iso()
    print('out', BP.OUT)
    print('frames: %d (measured stations: %s)' % (len(FRN), ','.join('FR%d' % n for n in sorted(FMEAS))))
