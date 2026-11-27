# A320neo interior reference blueprint (cabin + cockpit), sheets 5-8, same house style as the exterior sheets
# (Tools/make_a320neo_blueprint.py, whose airframe outline and drawing helpers are imported). Data from
# Art/A320/blueprint/a320neo-data.json (groups cabin, cockpit, doors); sources in Art/A320/blueprint/SOURCES.md.
# Four 3840x2160 sheets to docs/Pictures/a320neo-blueprint/:
#   int-cabin-plan.png, int-cabin-sections.png, int-cockpit.png, int-cockpit-panels.png
# Bright = measured (printed in a source), dim = est. Coordinates: x aft from the nose tip, y starboard, z up from the
# ground (MRW fwd CG) unless a dimension says "above floor". No logos, no livery, no brand names on panels.
#   python Tools/make_a320neo_interior_blueprint.py
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_a320neo_blueprint as B
from make_a320neo_blueprint import BG, INK, DIM, EST, ACC, HID, rrect
from matplotlib.patches import Polygon, Rectangle, Circle, Arc

D = B.D; CB, CK, DR = D['cabin'], D['cockpit'], D['doors']
HW, ZTOP, ZBOT = B.HW, B.ZTOP, B.ZBOT
IN = .0254
GRN = '#7fd38a'                                   # escape slide / evacuation zones


class ISheet(B.Sheet):
    def block(s, scale_note, notes=(), units='Units: metres unless marked mm. Floor = cabin floor; ground = MRW, forward CG.'):
        a, wm, hm = s.a, s.wm, s.hm; bw, bh = wm * .29, hm * .205; x0, y0 = wm - bw - wm * .008, hm * .012
        a.add_patch(Rectangle((x0, y0), bw, bh, fc=BG, ec=INK, lw=1.4, zorder=20))
        r = bh / 8
        rows = [('A320neo - reference blueprint, interior', 15, 'bold'), (s.title, 12, 'bold'), (s.sub, 9, 'normal'), (scale_note, 9, 'normal'),
                (units, 8.5, 'normal'),
                ('Data: Art/A320/blueprint/a320neo-data.json  |  Sources: S1 Airbus A320 AC Rev Jul 01/26; S2, S6-S10 (SOURCES.md)', 7.5, 'normal'),
                ('Airport Dynasty Pro  |  drawn 2026-11-27 by Tools/make_a320neo_interior_blueprint.py  |  no logo, no livery', 7.5, 'normal')]
        for i, (t, fs, w) in enumerate(rows):
            a.text(x0 + bw * .02, y0 + bh - r * (i + .75), t, color=INK, fontsize=fs, fontweight=w, va='center', zorder=21)
            if i in (0, 2, 4): a.plot([x0, x0 + bw], [y0 + bh - r * (i + 1.25)] * 2, color=HID, lw=.6, zorder=21)
        ly = y0 + r * .55; lx = x0 + bw * .02
        a.plot([lx, lx + bw * .05], [ly, ly], color=INK, lw=1.4, zorder=21); a.text(lx + bw * .06, ly, 'measured / printed in a source', color=INK, fontsize=8, va='center', zorder=21)
        a.plot([lx + bw * .42, lx + bw * .47], [ly, ly], color=EST, lw=1.4, zorder=21); a.text(lx + bw * .48, ly, 'est. = estimated', color=EST, fontsize=8, va='center', zorder=21)
        a.plot([lx + bw * .70, lx + bw * .75], [ly, ly], color=HID, lw=1.2, ls='--', zorder=21); a.text(lx + bw * .76, ly, 'hidden / above', color=HID, fontsize=8, va='center', zorder=21)
        for i, t in enumerate(notes):
            a.text(wm * .008, hm * .012 + i * hm * .022, t, color=EST, fontsize=8, va='bottom', zorder=21)

    def rect(s, x, y, w, h, c=INK, lw=.7, z=5, fill=True, ls='-', fc=None):
        s.a.add_patch(Rectangle((x, y), w, h, fc=(fc or BG) if fill else 'none', ec=c, lw=lw, zorder=z, ls=ls))


# ================================================================ cabin layout (one table, used by plan and sections)
WI = 1.815                                        # inner wall half-width at seat level (3.63 / 2, S1)
YP_F, YP_Y = CB['two_class']['F']['pitch_in'] * IN, CB['two_class']['Y']['pitch_in'] * IN
SD_F, SD_Y, SD_1 = .95, .72, .68                  # seat footprint depth, front of cushion to back of backrest (est.)
EX1, EX2 = DR['overwing_exits']['x_centers']
X_EXROW = EX1 - (SD_Y + YP_Y) / 2                 # front edge of the Y row just ahead of exit 1 (exit centred in the gap)
Y_ROWS = [X_EXROW + (k - 6) * YP_Y for k in range(CB['two_class']['Y']['rows'])]       # 23 rows, exits behind Y rows 7 and 8
F_ROWS = [Y_ROWS[0] - YP_F * (3 - k) for k in range(CB['two_class']['F']['rows'])]
# 1-class, 30 rows at 28/29 in: exit rows at 0.85 (exit pitch), the rest 28.5 in average
P1 = 28.5 * IN; PE = EX2 - EX1
X1E = EX1 - (SD_1 + PE) / 2
O_ROWS = [X1E - P1 * k for k in range(11, 0, -1)] + [X1E, X1E + PE] + [X1E + 2 * PE + P1 * k for k in range(17)]
O_ROWS = sorted(O_ROWS)[:30]
D1, D4 = DR['passenger']['1L_1R'], DR['passenger']['4L_4R']
XBH = 3.95                                        # cockpit rear bulkhead / cockpit door (est.)
GAL_F = (XBH, 4.62); LAV_F = (XBH, 4.62)          # fwd galley starboard, fwd lav port (est., see notes)
LAV_A = (28.10, 29.10); GAL_A = tuple(CB['galley_aft']['x_approx'])
BIN_D = CB['overhead_bins']['depth_approx']


def wall(x): return float(HW(x)) - .16           # inner lining half-width at seat level (est. wall 0.16)


def seat_block(s, P, x, y0, y1, n, depth, c=INK, lw=.6, facing=1):
    """n seats side by side between y0<y1 (plan), front edge at x, forward facing (facing=1) or aft facing."""
    arm = .05; w = (y1 - y0 - arm * (n + 1)) / n; bk = .17 if depth < .8 else .22
    xb = x + depth - bk if facing > 0 else x
    xc = x if facing > 0 else x + bk
    s.poly([P(x, y0), P(x + depth, y0), P(x + depth, y1), P(x, y1)], lw, c, z=5)
    s.line([P(xb, y0), P(xb, y1)] if facing > 0 else [P(x + bk, y0), P(x + bk, y1)], lw * .8, c, 5.1)
    for i in range(n + 1):
        ya = y0 + i * (w + arm)
        s.poly([P(xc + .08, ya), P(xc + depth - bk, ya), P(xc + depth - bk, ya + arm), P(xc + .08, ya + arm)] if facing > 0 else
               [P(x + bk, ya), P(x + depth - .08, ya), P(x + depth - .08, ya + arm), P(x + bk, ya + arm)], lw * .6, c, z=5.2)
    for i in range(n):
        yc = y0 + arm + i * (w + arm)
        rrect(s, *P(xc + (depth - bk) / 2 + .02 if facing > 0 else x + bk + (depth - bk) / 2, yc + w / 2), depth - bk - .12, w - .04, .03, c, lw * .5, 5.3)
    return w


def cabin_outline(s, P, inner=True, x0=0.0, x1=B.L):
    xs = np.linspace(x0, x1, 300)
    s.poly([P(x, float(HW(x))) for x in xs] + [P(x, -float(HW(x))) for x in xs[::-1]], 1.2, INK, z=2)
    if inner:
        xi = np.linspace(XBH - .3, 31.4, 200)
        for sg in (-1, 1): s.line([P(x, sg * wall(x)) for x in xi], .5, EST, 2.5)


def door_plan(s, P, xc, w, sg, lab=None, slide=True, slide_len=4.7, slide_w=1.1):
    x0, x1 = xc - w / 2, xc + w / 2; yo = sg * float(HW(xc)); yi = sg * wall(xc)
    s.a.add_patch(Polygon([P(x0, yo), P(x1, yo), P(x1, yi), P(x0, yi)], closed=True, fc=BG, ec='none', zorder=4))
    s.line([P(x0, yo), P(x0, yi)], 1.0, INK, 4.1); s.line([P(x1, yo), P(x1, yi)], 1.0, INK, 4.1)
    # open door leaf: moves outward then forward, parallel to the skin (S1: 1.80 swing envelope)
    s.poly([P(x0 - w - .02, yo + sg * .12), P(x0 - .02, yo + sg * .12), P(x0 - .02, yo + sg * .27), P(x0 - w - .02, yo + sg * .27)], .6, EST, False, 4.2, '--')
    s.a.add_patch(Arc(P(x0, yo), 2 * .2, 2 * .2, theta1=90 if sg > 0 else 180, theta2=180 if sg > 0 else 270, color=EST, lw=.5, zorder=4.2))
    if slide:
        a = [P(xc - slide_w / 2, yo + sg * .3), P(xc + slide_w / 2, yo + sg * .3), P(xc + slide_w / 2 + .15, yo + sg * (.3 + slide_len)), P(xc - slide_w / 2 - .15, yo + sg * (.3 + slide_len))]
        s.a.add_patch(Polygon(a, closed=True, fc='none', ec=GRN, lw=.8, ls='--', zorder=3.5))
        s.line([P(xc, yo + sg * .35), P(xc, yo + sg * (slide_len + .1))], .5, GRN, 3.5, ':')


def draw_cabin_plan():
    s = ISheet(40.0, 'Sheet 5 of 8 - cabin plan, 2-class (150) and 1-class (180)',
               'From above at seat level, nose left, starboard up. Slides drawn on the port side only.')
    ox, oy = 1.1, 17.8; P = lambda x, y: (ox + x, oy + y); a = s.a
    cabin_outline(s, P)
    # cockpit outline only (sheet 7 has the detail)
    s.line([P(XBH, -wall(XBH)), P(XBH, -.375)], 1.0, INK, 5); s.line([P(XBH, .375), P(XBH, wall(XBH))], 1.0, INK, 5)
    s.a.add_patch(Arc(P(XBH, -.375), 1.5, 1.5, theta1=90 - 90, theta2=90, color=EST, lw=.5, zorder=5, angle=90))
    s.text(*P(2.5, 0), 'cockpit\n(sheet 7)', EST, 8, box=False)
    s.text(*P(XBH - .1, 0), 'cockpit door 0.75', EST, 6.5, rot=90, box=False, ha='right')
    # fwd galley (starboard) and lav (port)
    s.rect(*P(GAL_F[0], .42), GAL_F[1] - GAL_F[0], wall(4.3) - .42, EST, .8, 5)
    for k in range(3): s.line([P(GAL_F[0] + .05, .42 + .4 * k + .25), P(GAL_F[1] - .05, .42 + .4 * k + .25)], .4, EST, 5.1)
    s.text(*P(4.29, 1.0), 'G1\ngalley', EST, 7, box=False)
    s.rect(*P(LAV_F[0], -wall(4.3)), LAV_F[1] - LAV_F[0], wall(4.3) - .42, EST, .8, 5)
    a.add_patch(Circle(P(4.15, -1.1), .14, fc='none', ec=EST, lw=.5, zorder=5.1)); s.rect(*P(4.35, -.8), .22, .3, EST, .4, 5.1, False)
    s.text(*P(4.29, -1.55), 'lav A', EST, 7, box=False)
    # fwd attendant double jump seat on the lav aft wall, facing aft
    seat_block(s, P, 4.62, -1.1, -.45, 2, .32, EST, .5, facing=-1)
    s.lab(*P(4.78, -.78), 'attendant seats x2 (fold, face aft)', *P(3.0, -3.0), True, 7.5)
    # aft lavs, aft attendant seat, aft galley
    for sg in (-1, 1):
        y0 = .42 if sg > 0 else -wall(28.6); s.rect(*P(LAV_A[0], y0), LAV_A[1] - LAV_A[0], wall(29.1) - .42, EST, .8, 5)
        a.add_patch(Circle(P(28.6, sg * 1.15), .14, fc='none', ec=EST, lw=.5, zorder=5.1))
        s.text(*P(28.6, sg * .7), 'lav ' + ('E' if sg > 0 else 'D'), EST, 7, box=False)
    seat_block(s, P, LAV_A[1], -.38, .38, 2, .30, EST, .5, facing=-1)
    s.lab(*P(29.25, 0), 'attendant seats x2 (fold, face aft)', *P(31.5, -2.6), True, 7.5)
    ga = wall(GAL_A[1]); s.rect(*P(GAL_A[0], -ga), GAL_A[1] - GAL_A[0], 2 * ga, EST, .8, 5)
    for k in range(1, 6): s.line([P(GAL_A[0], -ga + 2 * ga * k / 6), P(GAL_A[1], -ga + 2 * ga * k / 6)], .4, EST, 5.1)
    s.text(*P(sum(GAL_A) / 2, 0), 'G5 aft galley\n(trolleys)', EST, 6.5, rot=90)
    # seats, 2-class
    rown = 0
    for x in F_ROWS:
        rown += 1
        for sg in (-1, 1):
            yb = CB['cross_section_F']['double_block']; ya = CB['cross_section_F']['aisle'] / 2
            seat_block(s, P, x, ya if sg > 0 else -ya - yb, ya + yb if sg > 0 else -ya, 2, SD_F)
        s.text(*P(x + SD_F / 2, 0), str(rown), ACC, 8, w='bold')
    for x in Y_ROWS:
        rown += 1
        for sg in (-1, 1):
            yb = CB['cross_section_Y']['triple_block']; ya = CB['cross_section_Y']['aisle'] / 2
            seat_block(s, P, x, ya if sg > 0 else -ya - yb, ya + yb if sg > 0 else -ya, 3, SD_Y)
        s.text(*P(x + SD_Y / 2, 0), str(rown), ACC, 7.5, w='bold')
    # seat letters at row 1 and row 4
    for t, y in zip('ACDF', (-1.45, -.70, .70, 1.45)): s.text(*P(F_ROWS[0] - .2, y), t, INK, 7, box=False)
    for t, y in zip('ABCDEF', (-1.55, -1.07, -.59, .59, 1.07, 1.55)): s.text(*P(Y_ROWS[-1] + SD_Y + .2, y), t, INK, 7, box=False)
    # class divider (curtain)
    xd = (F_ROWS[-1] + SD_F + Y_ROWS[0]) / 2
    for sg in (-1, 1): s.line([P(xd, sg * .3), P(xd, sg * wall(xd))], .9, EST, 5, '--')
    s.lab(*P(xd, -1.75), 'class divider (curtain)', *P(7.4, -2.9), True, 7.5)
    # overhead bin line (above)
    for sg in (-1, 1): s.line([P(5.5, sg * (WI - BIN_D)), P(28.0, sg * (WI - BIN_D))], .7, HID, 4.6, '--')
    # doors and exits
    for sg in (-1, 1):
        door_plan(s, P, D1['x_center'], D1['width'], sg, slide=sg < 0)
        door_plan(s, P, D4['x_center'], D4['width'], sg, slide=sg < 0)
        for xe in (EX1, EX2):
            yo = sg * float(HW(xe)); yi = sg * wall(xe); w = DR['overwing_exits']['width']
            s.a.add_patch(Polygon([P(xe - w / 2, yo), P(xe + w / 2, yo), P(xe + w / 2, yi), P(xe - w / 2, yi)], closed=True, fc=BG, ec=ACC, lw=.9, zorder=4.3))
    s.text(*P(D1['x_center'], 2.55), '1R', INK, 8, w='bold'); s.text(*P(D1['x_center'], -2.35), '1L', INK, 8, w='bold')
    s.text(*P(D4['x_center'], 2.35), '4R', INK, 8, w='bold'); s.text(*P(D4['x_center'], -2.30), '4L', INK, 8, w='bold')
    # port wing root (outline faint) with the overwing escape route and slide
    ys = np.linspace(1.98, 7.0, 20)
    s.line([P(B.xle(y), -y) for y in ys], .7, HID, 1.5); s.line([P(B.xte(y), -y) for y in ys], .7, HID, 1.5)
    s.line([P(B.xle(7.0) - .2, -7.0), P(B.xte(7.0) + .2, -7.0)], .5, HID, 1.5, '-.')
    s.text(*P(B.xle(6.0) + .2, -6.3), 'wing (port, cut)', HID, 7, ha='left', box=False)
    route = [P(EX1 - .3, -2.05), P(EX2 + .3, -2.05), P(18.3, -3.4), P(17.6, -3.4)]
    s.a.add_patch(Polygon(route, closed=True, fc='none', ec=GRN, lw=.8, ls='--', zorder=3.5))
    sl = [P(18.9, -2.0), P(20.13, -2.0), P(20.9, -6.0), P(19.4, -6.0)]
    s.a.add_patch(Polygon(sl, closed=True, fc='none', ec=GRN, lw=.9, ls='--', zorder=3.5))
    s.lab(*P(16.0, -2.7), 'overwing escape route on the wing (marked walkway)', *P(10.4, -5.3), True, 7.5)
    s.lab(*P(19.5, -2.1), 'overwing slide compartment FR47, 1.23 long', *P(22.6, -3.6), False, 7.5)
    s.lab(*P(20.0, -5.0), 'overwing slide off the flap TE, footprint', *P(22.6, -5.2), True, 7.5)
    s.lab(*P(D1['x_center'] + .4, -6.0), 'door 1L slide footprint 1.1 x 4.7', *P(6.6, -6.6), True, 7.5)
    s.lab(*P(D4['x_center'] - .4, -6.3), 'door 4L slide footprint 1.1 x 4.7', *P(26.0, -6.6), True, 7.5)
    s.lab(*P(EX1, -1.98), 'overwing exits 2 per side %.2f x %.2f (sill +%.2f above floor est.)' % (DR['overwing_exits']['width'], DR['overwing_exits']['height'], DR['overwing_exits']['sill_above_floor_approx']), *P(8.2, -4.2), False, 7.5)
    # dimensions (top band)
    yt = 2.25
    s.hdim(*[ox + v for v in (0, D1['x_center'])], oy + yt + 1.2, 'door 1 centre %.2f' % D1['x_center'], False, oy + 2, oy + 2)
    s.hdim(*[ox + v for v in (0, D4['x_center'])], oy + yt + 1.95, 'door 4 centre %.2f' % D4['x_center'], False, oy + 2, oy + 1.9)
    s.hdim(*[ox + v for v in (0, EX1)], oy + yt + 1.2, 'exit 1 %.2f' % EX1, False, oy + 2, oy + 2, tx=ox + EX1 - 2)
    s.hdim(ox + EX1, ox + EX2, oy + yt + .6, '%.2f' % (EX2 - EX1), False, oy + 2, oy + 2, 7.5)
    s.hdim(ox + F_ROWS[0], ox + F_ROWS[1], oy + yt, 'pitch 36 in = %.3f' % YP_F, False, oy + 1.8, oy + 1.8, 7.5)
    s.hdim(ox + Y_ROWS[1], ox + Y_ROWS[2], oy + yt, 'pitch 32 in = %.3f' % YP_Y, False, oy + 1.8, oy + 1.8, 7.5, tx=ox + Y_ROWS[2] + .7)
    s.hdim(ox + Y_ROWS[15], ox + Y_ROWS[15] + SD_Y, oy + yt, 'seat depth %.2f' % SD_Y, True, oy + 1.8, oy + 1.8, 7.5, tx=ox + Y_ROWS[15] + .9)
    s.hdim(ox + F_ROWS[0], ox + Y_ROWS[-1] + SD_Y, oy + yt + .6, 'seat zone %.2f (row 1 front to row 26 back)' % (Y_ROWS[-1] + SD_Y - F_ROWS[0]), True, oy + 1.8, oy + 1.8, 7.5, tx=ox + 21.2)
    s.hdim(ox + GAL_A[0], ox + GAL_A[1], oy + yt, '%.2f' % (GAL_A[1] - GAL_A[0]), True, oy + 1.5, oy + 1.5, 7, tx=ox + 30.55)
    s.hdim(ox + LAV_A[0], ox + LAV_A[1], oy + yt + .45, 'lav %.2f' % (LAV_A[1] - LAV_A[0]), True, oy + 1.8, oy + 1.8, 7)
    # width dims (right of the aft galley) - at a Y row and F row (rotated texts)
    xr = ox + 33.2
    s.vdim(xr, oy - WI, oy + WI, 'cabin %.2f' % CB['cross_section_Y']['total_width'], False, None, None, 7.5)
    s.vdim(xr + .9, oy - float(HW(20)), oy + float(HW(20)), 'fuselage %.2f' % D['overall']['fuselage_width_ext'], False, None, None, 7.5)
    xm = ox + Y_ROWS[-1] + SD_Y - .3
    s.vdim(xm + 1.0, oy + .32, oy + 1.82, 'triple 1.50', False, None, None, 6.5)
    s.vdim(xm + 1.0, oy - .32, oy + .32, '.64', False, None, None, 6.5)
    xf = ox + F_ROWS[0] - .85
    s.vdim(xf + .55, oy + .345, oy + 1.795, 'double 1.45', False, None, None, 6.5)
    s.vdim(xf + .55, oy - .345, oy + .345, '.69', False, None, None, 6.5)
    s.text(ox + 0.2, oy - 14.0, '2-CLASS 150: F 12 (4 abreast, 36 in, seat 0.57) rows 1-3  |  Y 138 (6 abreast, 32 in, seat 0.43 = 17 in) rows 4-26\n'
           'attendant seats 4  |  galleys 2  |  lavatories 3 (A fwd left, D/E aft)\n' 'dashed line over the seat backs = overhead bin front edge (pivot bins, depth %.2f est.)' % BIN_D, INK, 8.5, ha='left')
    # ---------------- 1-class strip
    oy1 = 7.7; P1f = lambda x, y: (ox + x, oy1 + y)
    cabin_outline(s, P1f)
    s.line([P1f(XBH, -wall(XBH)), P1f(XBH, wall(XBH))], 1.0, INK, 5)
    s.rect(*P1f(GAL_F[0], .42), GAL_F[1] - GAL_F[0], wall(4.3) - .42, EST, .8, 5); s.text(*P1f(4.29, 1.0), 'G1', EST, 7, box=False)
    s.rect(*P1f(LAV_F[0], -wall(4.3)), LAV_F[1] - LAV_F[0], wall(4.3) - .42, EST, .8, 5); s.text(*P1f(4.29, -1.1), 'lav A', EST, 6.5, box=False)
    for sg in (-1, 1):
        s.rect(*P1f(LAV_A[0], .42 if sg > 0 else -wall(28.6)), LAV_A[1] - LAV_A[0], wall(29.1) - .42, EST, .8, 5)
        s.text(*P1f(28.6, sg * 1.1), 'lav', EST, 6.5, box=False)
        for xe in (EX1, EX2):
            yo = sg * float(HW(xe)); yi = sg * wall(xe); w = DR['overwing_exits']['width']
            s.a.add_patch(Polygon([P1f(xe - w / 2, yo), P1f(xe + w / 2, yo), P1f(xe + w / 2, yi), P1f(xe - w / 2, yi)], closed=True, fc=BG, ec=ACC, lw=.9, zorder=4.3))
        door_plan(s, P1f, D1['x_center'], D1['width'], sg, slide=False); door_plan(s, P1f, D4['x_center'], D4['width'], sg, slide=False)
    s.rect(*P1f(GAL_A[0], -ga), GAL_A[1] - GAL_A[0], 2 * ga, EST, .8, 5); s.text(*P1f(sum(GAL_A) / 2, 0), 'G5', EST, 6.5, box=False)
    seat_block(s, P1f, 4.62, -1.1, -.45, 2, .32, EST, .5, facing=-1); seat_block(s, P1f, LAV_A[1], -.38, .38, 2, .30, EST, .5, facing=-1)
    seat_block(s, P1f, 4.62, .45, 1.1, 1, .32, EST, .5, facing=-1)
    for i, x in enumerate(O_ROWS):
        for sg in (-1, 1): seat_block(s, P1f, x, .32 if sg > 0 else -1.82, 1.82 if sg > 0 else -.32, 3, SD_1, INK, .5)
        s.text(*P1f(x + SD_1 / 2, 0), str(i + 1), ACC, 6.5, w='bold')
    s.hdim(ox + O_ROWS[2], ox + O_ROWS[3], oy1 + 2.5, '28.5 in = %.3f (28/29 in)' % P1, True, oy1 + 1.8, oy1 + 1.8, 7.5)
    s.hdim(ox + O_ROWS[11], ox + O_ROWS[12], oy1 + 2.5, 'exit row %.2f' % PE, True, oy1 + 1.8, oy1 + 1.8, 7.5, tx=ox + O_ROWS[12] + 1.2)
    s.text(ox + 0.2, oy1 - 2.45, '1-CLASS 180 (high density): Y 6 abreast, 28/29 in, rows 1-30\nattendant seats 5  |  galleys 2  |  lavatories 3', INK, 8.5, ha='left')
    s.text(ox + 13.5, oy1 - 2.45, 'row positions est.: rows laid so the overwing exits (x %.2f / %.2f) sit in the gaps;\naft row limited by the aft lavs' % (EX1, EX2), EST, 7.5, box=False, ha='left')
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (both layouts).' % s.k,
            ['Seat counts, abreast, pitches, seat/aisle widths, door and exit positions: S1 p69-74 (bright). Seat footprints, galley/lav sizes, row x positions, slide footprints: est.',
             'Fwd galley drawn starboard and lav A port (read from S1 fig p70, nose-down view, and photos); the data file note "galley left" read that figure the other way round.',
             'Cockpit door placed at x 3.95 (est.) so that galley and lav A fit ahead of door 1 (data file says 4.35 - not consistent with a 0.8 m lav ahead of door 1).'])
    s.save('int-cabin-plan.png')


# ================================================================ cross-sections
FL = D['overall']['cabin_floor_height_above_ground_approx']     # 3.38
HOLD_FL = 2.00                                                   # hold floor (est., about the fwd cargo door sill 1.98)
LD3 = dict(base=61.5 * IN, top=96 * IN, h=45 * IN, depth=60.4 * IN, kink=1.143 - .64)   # LD3-45 (AKH), IATA ULD spec


def lin_section(hw, hh, cz=3.85, n=160, t0=0, t1=2 * math.pi):
    t = np.linspace(t0, t1, n); return [(hw * math.cos(q), cz + hh * math.sin(q) * ((1 - .04 * math.cos(q) ** 2) if math.sin(q) < 0 else 1)) for q in t]


def seat_front(s, P, y0, y1, n, cls, c=INK):
    """front view of a seat block between y0<y1 (floor at 0)."""
    arm = .05 if cls == 'Y' else .07; w = (y1 - y0 - arm * (n + 1)) / n
    ph, bh, ah = (.43, 1.12, .62) if cls == 'Y' else (.45, 1.10, .64)
    for i in range(n):
        yc = y0 + arm + i * (w + arm) + w / 2
        rrect(s, *P(yc, (ph + bh) / 2 + .05), w - .02, bh - ph + .05, .06, c, .7, 5)                    # backrest
        rrect(s, *P(yc, ph + .03), w, .1, .03, c, .7, 5.2)                                              # cushion
        if cls == 'Y': rrect(s, *P(yc, bh - .03), w * .62, .14, .04, c, .5, 5.3)                        # headrest
        else: rrect(s, *P(yc, bh - .04), w * .55, .16, .05, c, .5, 5.3)
    for i in range(n + 1):
        ya = y0 + i * (w + arm); s.rect(*P(ya, ph - .02), arm, ah - ph + .04, c, .5, 5.4)
    for yy in (y0 + .1, y1 - .1): s.line([P(yy, 0), P(yy, ph - .03)], .6, c, 4.9)                      # legs on the seat tracks
    s.line([P(y0 + .1, .1), P(y1 - .1, .1)], .4, c, 4.9)


def section(s, ox, oy, title, cls, x_at, exit_row=False, hold='fwd'):
    P = lambda y, z: (ox + y, oy + z)              # z = ground height, y = lateral (port left as seen from aft? -> looking forward, port left)
    cz, hw, hh = 3.85, D['overall']['fuselage_width_ext'] / 2, D['overall']['fuselage_height_ext'] / 2
    s.poly([P(y, z) for y, z in B.fus_section(cz, hw, hh)], 1.3, INK, z=2)
    s.line([P(y, z) for y, z in lin_section(hw - .09, hh - .09)], .5, EST, 2.2, '--')                    # frames inner flange
    # cabin lining above the floor
    lin = [p for p in lin_section(1.82, hh - .155) if p[1] >= FL]
    s.line([P(y, z) for y, z in lin], .9, INK, 3)
    # floor + beams + seat tracks
    s.rect(*P(-1.88, FL - .02), 3.76, .02, INK, .9, 4)
    s.rect(*P(-1.85, FL - .2), 3.7, .18, EST, .5, 3.5, False)
    s.line([P(-1.85, FL - .11), P(1.85, FL - .11)], .3, EST, 3.5, ':')
    yb = CB['cross_section_Y' if cls == 'Y' else 'cross_section_F']; ya = yb['aisle'] / 2
    bl = yb['triple_block' if cls == 'Y' else 'double_block']; n = 3 if cls == 'Y' else 2
    Pf = lambda y, z: P(y, FL + z)
    seat_front(s, Pf, -ya - bl, -ya, n, cls); seat_front(s, Pf, ya, ya + bl, n, cls)
    # overhead bins (pivot type), PSU, ceiling
    bb = FL + CB['overhead_bins']['bottom_above_floor_approx']; ce = FL + CB['floor_to_ceiling_aisle_approx']
    for sg in (-1, 1):
        yw = 1.82 * math.cos(math.asin(min(1, (bb + .45 - cz) / (hh - .155))))
        pts = [(sg * (yw - .05), bb + .52), (sg * (1.82 - BIN_D - .02), bb + .52), (sg * (1.82 - BIN_D - .06), bb + .05), (sg * (1.82 - BIN_D), bb), (sg * 1.74, bb)]
        pts += [(sg * float(1.82 * math.cos(math.asin(min(1, (z - cz) / (hh - .155))))), z) for z in np.linspace(bb, bb + .52, 6)]
        s.poly([P(y, z) for y, z in pts], .8, EST, z=4)
        s.rect(*P(min(sg * (1.82 - BIN_D), sg * (1.82 - BIN_D + .42)), bb - .07), .42, .07, EST, .5, 4)                       # PSU
        s.line([P(sg * (1.82 - BIN_D - .06), bb + .52), P(sg * .45, ce), P(0, ce)], .7, EST, 4)
        # window in the wall
        zw = DR['windows']['center_z_MRW_fwd']; hwin = DR['windows']['size_drawn'][1]
        for z0 in (zw - hwin / 2, zw + hwin / 2):
            q = math.asin((z0 - cz) / hh); s.line([P(sg * hw * math.cos(q), z0), P(sg * 1.82 * math.cos(math.asin((z0 - cz) / (hh - .155))), z0)], .7, ACC, 4.5)
        if exit_row:
            ze0 = FL + DR['overwing_exits']['sill_above_floor_approx']; ze1 = ze0 + DR['overwing_exits']['height']
            for z0 in (ze0, ze1):
                q = math.asin((z0 - cz) / hh); s.line([P(sg * hw * math.cos(q), z0), P(sg * 1.80 * math.cos(math.asin((z0 - cz) / (hh - .155))), z0)], 1.0, ACC, 4.6)
    # below the floor
    if hold in ('fwd', 'aft'):
        s.line([P(-1.05, HOLD_FL), P(1.05, HOLD_FL)], 1.0, INK, 4)
        hl = [p for p in lin_section(hw - .12, hh - .12) if HOLD_FL <= p[1] <= FL - .22]
        s.line([P(y, z) for y, z in hl if y < 0], .7, EST, 3); s.line([P(y, z) for y, z in hl if y > 0], .7, EST, 3)
        s.line([P(-1.6, FL - .22), P(1.6, FL - .22)], .6, EST, 3)
        b, t, h, k = LD3['base'] / 2, LD3['top'] / 2, LD3['h'], LD3['kink']
        z0 = HOLD_FL + .03
        s.poly([P(-b, z0), P(b, z0), P(t, z0 + k), P(t, z0 + h), P(-t, z0 + h), P(-t, z0 + k)], 1.0, ACC, z=4.5)
        s.line([P(-b, z0 + .05), P(b, z0 + .05)], .4, ACC, 4.6)
        for yy in (-.4, .4): s.line([P(yy, z0 + .1), P(yy, z0 + h - .08)], .3, ACC, 4.6, ':')
        s.text(*P(0, z0 + h * .62), 'LD3-45 (AKH)', ACC, 7.5)
        s.hdim(ox - b, ox + b, oy + z0 - .17, 'base 1.56', False, oy + z0, oy + z0, 7)
        s.hdim(ox - t, ox + t, oy + z0 + h + .1, 'top 2.44', False, oy + z0 + h, oy + z0 + h, 7)
        s.vdim(ox + t + .14, oy + z0, oy + z0 + h, '1.14', False, ox + t, ox + t, 7)
        s.vdim(ox - t - .14, oy + z0, oy + z0 + k, '%.2f' % k, True, ox - b, ox - t, 6.5)
        s.text(*P(0, HOLD_FL + .22), '%s hold, roller floor (est. z %.2f)' % (hold, HOLD_FL), EST, 7)
    else:
        s.rect(*P(-1.55, 2.05), 3.1, 1.0, EST, .8, 3.8)
        s.text(*P(0, 2.55), 'centre wing box\n(centre fuel tank) - no hold here', EST, 7.5)
    # dimensions
    s.hdim(ox - ya - bl, ox - ya, oy + FL + 1.32, '%.2f' % bl, False, None, None, 7)
    s.hdim(ox - ya, ox + ya, oy + FL + 1.32, '%.2f' % yb['aisle'], False, None, None, 7)
    s.hdim(ox + ya, ox + ya + bl, oy + FL + 1.32, '%.2f' % bl, False, None, None, 7)
    s.hdim(ox - 1.815 if cls == 'Y' else ox - 1.79, ox + 1.815 if cls == 'Y' else ox + 1.79, oy + FL + .78, 'at seat level %.2f' % yb['total_width'], False, None, None, 7)
    sw = yb['seat_width']; y0s = ya + (.05 if cls == 'Y' else .07)
    s.hdim(ox + y0s, ox + y0s + sw, oy + FL + .28, '%.2f' % sw, False, None, None, 6.5)
    s.vdim(ox + .05, oy + FL, oy + ce, 'ceiling %.2f' % CB['floor_to_ceiling_aisle_approx'], True, None, None, 6.5, side=1, tz=oy + FL + 1.95)
    s.vdim(ox - 1.15, oy + FL, oy + bb, 'bin %.2f' % CB['overhead_bins']['bottom_above_floor_approx'], True, None, None, 6.5)
    s.hdim(ox - hw, ox + hw, oy + 6.22, 'fuselage %.2f' % (2 * hw), False, oy + cz, oy + cz, 7)
    s.vdim(ox + hw + .28, oy + cz - hh, oy + cz + hh, 'fuselage %.2f' % (2 * hh), False, ox + .2, ox + .2, 7)
    s.vdim(ox - hw - .3, oy + .0, oy + FL, 'floor %.2f above ground' % FL, True, None, ox - .3, 7)
    s.line([P(-hw - .5, 0), P(hw + .5, 0)], .6, HID, 1, '-.')
    s.text(ox, oy + 6.6, title, INK, 10, w='bold')
    s.text(ox, oy + 6.4, 'looking forward, x = %.2f from the nose, port on the left' % x_at, EST, 7.5)


def draw_cabin_sections():
    s = ISheet(16.0, 'Sheet 6 of 8 - cabin cross-sections', 'Business row, economy row, overwing exit row. Looking forward. Floor, seats, bins, lining, windows, hold.')
    xb = F_ROWS[1]; xy = Y_ROWS[17]; xe = (EX1 + EX2) / 2
    section(s, 2.75, 1.25, 'A-A  business row 2 (2+2)', 'F', xb + .4, hold='fwd')
    section(s, 8.0, 1.25, 'B-B  economy row 21 (3+3)', 'Y', xy + .4, hold='aft')
    section(s, 13.25, 1.25, 'C-C  overwing exit row (between exits)', 'Y', xe, exit_row=True, hold='wing')
    s.lab(13.25 - 1.88, 1.25 + 3.38 + .95, 'overwing exit hatch 0.50 x 0.90, sill +0.50 (est.)', 13.25 - 1.3, 1.25 + 5.85, False, 7)
    s.lab(8.0 + 1.97, 1.25 + 4.55, 'window, centre z 4.55 (est.)', 8.0 + 1.1, 1.25 + 6.05, True, 7)
    s.lab(2.75 + 1.55, 1.25 + 3.38 + 1.75, 'pivot bin', 2.75 + 1.0, 1.25 + 6.05, True, 7)
    s.text(8.0, .45, 'Also printed in S1 p73: 6 abreast "wider seat" option - seat 0.46, triple 1.58, aisle 0.48 (total 3.63).', DIM, 7.5, box=False)
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (all three sections).' % s.k,
            ['Cabin widths, seat/aisle widths, fuselage size, floor height: S1 p8, p72-74 (bright). LD3-45: IATA ULD spec, 61.5/96/45 in (bright). Seat, bin, PSU, lining, beam shapes: est.',
             'Hold floor and lining est.; fwd hold 3 containers, aft hold 4 containers (S1 p76). Bin bottom 1.55 and ceiling 2.22 above floor: scaled from S1 cross-section figure (est.).'])
    s.save('int-cabin-sections.png')


# ================================================================ cockpit
CF = CK['floor_z_ground_approx']


def draw_cockpit():
    s = ISheet(12.4, 'Sheet 7 of 8 - cockpit plan and side section', 'Plan from above (nose left, captain = port, down) and section on the centreline looking to port.')
    ox, oy = .55, 3.95; P = lambda x, y: (ox + x, oy + y)
    xs = np.linspace(0, 4.8, 200)
    s.poly([P(x, float(HW(x))) for x in xs] + [P(x, -float(HW(x))) for x in xs[::-1]], 1.2, INK, z=1)
    for sg in (-1, 1): s.line([P(x, sg * (float(HW(x)) - .12)) for x in np.linspace(1.0, 4.8, 80)], .5, EST, 1.5)
    s.line([P(4.8, -2.0), P(4.8, 2.0)], .5, HID, 1, '-.')
    # windows along the side (plan)
    wx = [(1.13, 1.62, 'W1'), (1.66, 2.16, 'W2 sliding'), (2.20, 2.65, 'W3')]
    for x0, x1, t in wx:
        for sg in (-1, 1): s.line([P(x, sg * float(HW(x))) for x in np.linspace(x0, x1, 12)], 2.4, ACC, 3)
    s.text(*P(1.9, -1.75), 'side windows W1-W3 (est. split of x 1.13-2.65)', EST, 7, ha='center', box=False)
    # panel, glareshield, pedals
    gx = CK['glareshield']['x_approx']; gw = CK['glareshield']['width_approx']
    s.poly([P(gx - .12, -gw / 2), P(gx + .06, -gw / 2), P(gx + .2, -.3), P(gx + .2, .3), P(gx + .06, gw / 2), P(gx - .12, gw / 2)], .9, EST, z=4)
    s.text(*P(gx - .25, 0), 'glareshield + main panel', EST, 7, rot=90, box=False)
    for yc in CK['display_units']['y_centers_approx']: s.rect(*P(CK['display_units']['x_approx'], yc - .092), .03, .184, INK, .6, 5)
    for yp in CK['rudder_pedals']['y']:
        for d in (-.09, .09): s.rect(*P(CK['rudder_pedals']['x'] - .05, yp + d - .04), .1, .08, EST, .5, 3.8)
    s.lab(*P(1.55, -.53), 'rudder pedals (under panel)', *P(.5, -1.5), True, 7)
    # pedestal
    px0, px1 = CK['pedestal']['x_range']; pw = CK['pedestal']['width_approx']
    s.rect(*P(px0, -pw / 2), px1 - px0, pw, INK, .9, 4)
    for yy in CK['pedestal']['MCDU']['y']: s.rect(*P(px0 + .02, yy - .073), .229, .146, EST, .5, 4.5)
    s.rect(*P(2.38, -.073), .40, .146, EST, .5, 4.5); s.text(*P(2.58, 0), 'thrust', EST, 6, box=False)
    for yy in (-.12, .12): s.rect(*P(2.42, yy - .02 if yy < 0 else yy - .02), .3, .04, EST, .4, 4.5)
    for yy in CK['pedestal']['RMP']['y']: s.rect(*P(2.80, yy - .073), .12, .146, EST, .5, 4.5)
    s.text(*P(2.25, 0), 'MCDU\n1   2', EST, 6, box=False, rot=90)
    s.lab(*P(2.86, .16), 'RMP 2', *P(3.0, 1.25), True, 7)
    s.lab(*P(2.7, 0), 'pedestal %.2f wide, x %.1f-%.1f' % (pw, px0, px1), *P(3.0, -1.35), True, 7)
    # side consoles + sidesticks + tillers
    for sg in (-1, 1):
        yo = sg * .95; s.poly([P(1.85, yo), P(2.85, yo), P(2.85, yo - sg * .22), P(1.85, yo - sg * .22)], .7, EST, z=3.5)
        s.a.add_patch(Circle(P(CK['sidesticks']['x'], sg * CK['sidesticks']['fo_y']), .045, fc=BG, ec=ACC, lw=1.0, zorder=6))
        s.a.add_patch(Circle(P(2.05, sg * .89), .05, fc='none', ec=EST, lw=.6, zorder=6))
    s.lab(*P(2.3, -.86), 'side stick (y 0.86, x 2.30)', *P(1.2, -2.1), True, 7)
    s.lab(*P(2.05, .89), 'tiller', *P(1.3, 1.8), True, 7)
    # seats
    for yc, t in ((CK['seats']['captain_y'], 'CAPT'), (CK['seats']['fo_y'], 'F/O')):
        w = CK['seats']['seat_width']; xr = CK['seats']['seat_ref_x']
        s.rect(*P(xr - .25, yc - w / 2), .48, w, INK, .9, 6); s.rect(*P(xr + .23, yc - w / 2 + .03), .12, w - .06, INK, .9, 6)
        for d in (-.18, .18): s.line([P(xr - .45, yc + d), P(xr + .5, yc + d)], .4, HID, 5.5, '--')
        s.text(*P(xr, yc), t, INK, 7, w='bold')
    s.lab(*P(2.1, -.71), 'seat rails fore-aft + lateral', *P(3.3, -2.0), True, 7)
    # overhead (above)
    ow = CK['overhead']['width_approx']; ox0, ox1 = CK['overhead']['x_range']
    s.rect(*P(ox0, -ow / 2), ox1 - ox0, ow, HID, .8, 3.2, False, '--')
    s.lab(*P(1.95, .42), 'overhead panel above (dashed)', *P(.35, 1.5), False, 6.5)
    # observer seats, bulkhead, door
    o1, o2 = CK['observer_seats']['first'], CK['observer_seats']['second']
    s.rect(*P(o1['x'] - .2, -.24), .42, .48, EST, .7, 5); s.text(*P(o1['x'], 0), '1st obs', EST, 6.5)
    s.rect(*P(3.52, 1.25), .4, .32, EST, .7, 5); s.text(*P(3.72, 1.41), '2nd obs', EST, 6)
    s.line([P(XBH, -float(HW(XBH)) + .12), P(XBH, -.375)], 1.2, INK, 6); s.line([P(XBH, .375), P(XBH, float(HW(XBH)) - .12)], 1.2, INK, 6)
    s.a.add_patch(Arc(P(XBH, .375), 1.5, 1.5, theta1=180, theta2=270, color=EST, lw=.6, zorder=6))
    s.line([P(XBH, .375), P(XBH - .75, .375)], .7, EST, 6)
    s.text(*P(XBH + .3, -1.0), 'lav A', EST, 7, box=False); s.text(*P(XBH + .3, 1.0), 'G1', EST, 7, box=False)
    s.lab(*P(XBH - .4, .2), 'cockpit door 0.75 x 1.85, opens in', *P(4.3, 2.35), True, 7)
    s.hdim(ox, ox + CK['seats']['seat_ref_x'], oy - 2.3, 'seat ref %.2f' % CK['seats']['seat_ref_x'], True, oy - .8, oy - 2.0, 7)
    s.hdim(ox, ox + XBH, oy - 2.65, 'cockpit bulkhead %.2f' % XBH, True, oy - 2.0, oy - 2.0, 7)
    s.vdim(ox + 3.05, oy + CK['seats']['captain_y'], oy + CK['seats']['fo_y'], '1.06', False, ox + 2.6, ox + 2.6, 6.5)
    s.vdim(ox + 1.25, oy - .95, oy + .95, 'consoles 1.90', True, ox + 1.85, ox + 1.85, 6.5)
    s.text(ox + 2.4, oy + 2.55, 'PLAN', INK, 10, w='bold')
    # ---------------- side section
    ZCUT = 2.3
    sx, sz = 6.3, -.6; Q = lambda x, z: (sx + x, sz + z)
    xs = np.linspace(0, 4.8, 200)
    s.poly([Q(x, float(ZTOP(x))) for x in xs] + [Q(x, max(float(ZBOT(x)), ZCUT)) for x in xs[::-1]], 1.2, INK, z=1)
    s.text(*Q(3.6, ZCUT - .14), 'section cut off below z %.1f' % ZCUT, HID, 6.5, box=False)
    s.line([Q(x, float(ZTOP(x)) - .1) for x in np.linspace(.9, 4.8, 80)], .5, EST, 1.5)
    s.line([Q(4.8, 1.6), Q(4.8, 6.0)], .5, HID, 1, '-.')
    s.rect(*Q(.9, CF - .04), 3.9, .04, INK, .9, 3)
    s.text(*Q(3.9, CF + .1), 'cockpit floor z %.2f (est.)' % CF, EST, 7, box=False)
    s.text(*Q(3.5, 2.6), 'avionics bay / NLG bay below (est.)', HID, 7, box=False)
    # windows (side): W1-W3 between z 4.16 and 4.95
    zl, zh = CK['windows']['z_ground_range']
    wpoly = [(1.13, 4.16), (1.34, 4.38), (1.62, 4.62), (2.16, 4.95), (2.65, 4.95), (2.65, 4.16)]
    s.poly([Q(x, z) for x, z in wpoly], .9, ACC, False, 3)
    for xx in (1.62, 2.16): s.line([Q(xx, 4.16), Q(xx, float(np.interp(xx, [1.13, 1.34, 1.62, 2.16, 2.65], [4.16, 4.38, 4.62, 4.95, 4.95])))], .7, ACC, 3)
    s.text(*Q(1.42, 4.30), 'W1', ACC, 6.5, box=False); s.text(*Q(1.9, 4.5), 'W2', ACC, 6.5, box=False); s.text(*Q(2.4, 4.55), 'W3', ACC, 6.5, box=False)
    # glareshield, panel, pedals
    gz = CF + CK['glareshield']['z_top_above_floor_approx']
    s.poly([Q(gx - .12, gz), Q(gx + .1, gz), Q(gx + .14, gz - .05), Q(1.80, gz - .08), Q(1.78, CF + .5), Q(1.70, CF + .5), Q(1.55, gz - .1)], .9, EST, z=4)
    s.rect(*Q(1.76, CF + CK['display_units']['center_z_above_floor_approx'] - .092), .03, .184, INK, .7, 5)
    s.text(*Q(1.68, CF + .7), 'DU', INK, 6, box=False)
    s.poly([Q(1.45, CF + .05), Q(1.62, CF + .05), Q(1.58, CF + .28), Q(1.52, CF + .28)], .6, EST, z=4)
    # pedestal
    pz = CF + CK['pedestal']['z_top_above_floor_approx']
    s.poly([Q(px0, CF + .45), Q(px0 + .05, pz), Q(px1, pz), Q(px1, CF), Q(px0, CF)], .8, EST, z=3.5)
    s.line([Q(2.45, pz), Q(2.36, pz + .2)], 1.4, ACC, 4); s.a.add_patch(Circle(Q(2.36, pz + .2), .025, fc=BG, ec=ACC, lw=.8, zorder=4))
    s.lab(*Q(2.36, pz + .2), 'thrust levers', *Q(.95, CF + .25), False, 6.5)
    # seat (captain, near side)
    xr = CK['seats']['seat_ref_x']; ch = CF + CK['seats']['cushion_height']
    s.poly([Q(xr - .25, ch), Q(xr + .23, ch), Q(xr + .23, ch - .1), Q(xr - .25, ch - .08)], .9, INK, z=6)
    s.poly([Q(xr + .2, ch), Q(xr + .33, ch + CK['seats']['back_height']), Q(xr + .43, ch + CK['seats']['back_height']), Q(xr + .32, ch - .05)], .9, INK, z=6)
    s.line([Q(xr, ch - .1), Q(xr, CF)], .8, INK, 6); s.line([Q(xr - .4, CF + .03), Q(xr + .45, CF + .03)], .6, HID, 6)
    s.line([Q(CK['sidesticks']['x'], CF + CK['sidesticks']['z_above_floor']), Q(CK['sidesticks']['x'] - .02, CF + CK['sidesticks']['z_above_floor'] + CK['sidesticks']['stick_height'])], 2.0, ACC, 6.5)
    s.rect(*Q(1.9, CF + .55), .95, .19, EST, .6, 6.2, False)
    s.text(*Q(2.35, CF + .48), 'side console', EST, 6, box=False)
    # overhead
    oz = float(ZTOP(2.3)) - .14
    s.poly([Q(ox0, float(ZTOP(ox0)) - .12), Q(ox1, float(ZTOP(ox1)) - .12), Q(ox1, float(ZTOP(ox1)) - .2), Q(ox0, float(ZTOP(ox0)) - .22)], .9, EST, z=4)
    s.text(*Q(2.45, float(ZTOP(2.45)) + .3), 'overhead panel x %.2f-%.2f' % (ox0, ox1), EST, 7, box=False)
    # observer seat + bulkhead + door
    s.rect(*Q(o1['x'] - .2, CF + .42), .42, .08, EST, .7, 5); s.line([Q(o1['x'] + .2, CF + .45), Q(o1['x'] + .25, CF + 1.2)], .8, EST, 5)
    s.text(*Q(o1['x'] + .05, CF + .64), '1st obs', EST, 6.5, box=False)
    s.line([Q(XBH, CF), Q(XBH, float(ZTOP(XBH)) - .1)], 1.1, INK, 5)
    s.rect(*Q(XBH - .03, CF), .06, CK['door_height'], EST, .7, 5.5)
    s.vdim(sx + XBH + .3, sz + CF, sz + CF + CK['door_height'], 'door 1.85', True, sx + XBH, sx + XBH, 6.5)
    # eye reference point and vision lines
    ex, ez = CK['pilot_eye']['x'], CK['pilot_eye']['z_ground']
    s.a.add_patch(Circle(Q(ex, ez), .045, fc=ACC, ec=ACC, lw=1, zorder=8))
    ld = math.radians(CK['pilot_eye']['look_down_deg'])
    s.line([Q(ex, ez), Q(ex - 2.6, ez - 2.6 * math.tan(ld))], .7, ACC, 7, '--')
    s.line([Q(ex, ez), Q(ex - 2.6, ez)], .5, HID, 7, ':')
    s.text(*Q(.45, ez + .1), 'horizontal', HID, 6.5, box=False)
    s.text(*Q(.25, ez - 2.6 * math.tan(ld) + .65), '%d deg over-nose\nvision (S1)' % CK['pilot_eye']['look_down_deg'], ACC, 7, box=False)
    s.lab(*Q(ex, ez), 'eye reference point x %.2f, z %.2f, y +/-%.2f (S1)' % (ex, ez, CK['pilot_eye']['y']), *Q(3.2, 5.25), False, 7)
    s.vdim(sx + ex + .25, sz + CF, sz + ez, 'eye %.2f above floor' % (ez - CF), True, sx + ex, sx + ex, 6.5)
    s.lab(*Q(gx, gz), 'glareshield top %.2f above floor' % (gz - CF), *Q(.3, 5.55), True, 7)
    s.vdim(sx + px1 - .15, sz + CF, sz + pz, 'pedestal %.2f' % (pz - CF), True, sx + px1, sx + px1, 6.5)
    s.vdim(sx + xr - .38, sz + CF, sz + ch, '%.2f' % (ch - CF), True, sx + xr - .25, sx + xr - .25, 6.5)
    s.hdim(sx, sx + ex, sz + 2.75, 'eye x %.2f' % ex, False, sz + ez - .1, sz + 2.85, 7)
    s.hdim(sx + 1.13, sx + 2.65, sz + 5.85, 'side windows x 1.13-2.65', True, sz + 4.95, sz + 4.95, 7)
    s.vdim(sx - .25, sz + 4.16, sz + 4.95, 'z 4.16-4.95', True, sx + 1.13, sx + 1.13, 6.5)
    s.text(sx + 2.4, oy + 2.55, 'SIDE SECTION (centreline, looking to port)', INK, 10, w='bold')
    s.block('Scale: 1 m = %.1f px on this 3840 x 2160 sheet (plan and section).' % s.k,
            ['Eye reference point, 20 deg over-nose view and the windscreen corner CP1 4.17: S1 p8, p57 (bright). All other cockpit sizes est. (S6-S9: standards, sim-builder data, photos).',
             'Observer seat 2 drawn at x 3.72 (data 3.80) and the bulkhead at x 3.95 (data door 4.35) so that galley G1 and lav A fit ahead of door 1 - both est.'])
    s.save('int-cockpit.png')


# ================================================================ panels (sizes in mm)
def blk(s, x, y, w, h, t, est=True, fs=6.5, sub=None, c=None):
    c = c or (EST if est else INK); s.rect(x, y, w, h, c, .7, 5)
    s.text(x + w / 2, y + h / 2 + (.006 if sub else 0), t, c, fs, box=False)
    if sub: s.text(x + w / 2, y + h / 2 - .011, sub, c, fs - 1.2, box=False)


def draw_panels():
    s = ISheet(3.55, 'Sheet 8 of 8 - cockpit panels (front views, mm)', 'Glareshield, main instrument panel, overhead (plan, fwd up), pedestal (plan, fwd up). Labelled blocks; no logos.')
    mm = .001
    # ---------------- glareshield (top band) and main panel
    cx, gy = .92, 1.72; gw = CK['glareshield']['width_approx']
    s.rect(cx - gw / 2, gy, gw, .13, INK, 1.0, 4)
    fw, fh = [v * mm for v in CK['glareshield']['FCU']['pcb_mm_est']]
    s.rect(cx - fw / 2, gy + .017, fw, fh, EST, .9, 5)
    parts = [('SPD/MACH', .03), ('HDG/TRK', .075), ('ALT', .165), ('V/S FPA', .215)]
    for t, dx in parts:
        s.rect(cx - fw / 2 + dx - .018, gy + .017 + fh - .03, .036, .016, EST, .4, 5.5)
        s.a.add_patch(Circle((cx - fw / 2 + dx, gy + .017 + .033), .011, fc='none', ec=EST, lw=.5, zorder=5.5))
        s.text(cx - fw / 2 + dx, gy + .017 + .058, t, EST, 4.5, box=False)
    for i, t in enumerate(('AP1', 'AP2', 'A/THR')): s.rect(cx - .03 + i * .022, gy + .03, .018, .012, EST, .4, 5.5)
    s.text(cx, gy + .021, 'AP1 AP2 A/THR  LOC  EXPED  APPR', EST, 4, box=False)
    s.text(cx, gy + .14, 'FCU %d x %d (est.)' % tuple(CK['glareshield']['FCU']['pcb_mm_est']), EST, 6.5, box=False)
    ew, eh = [v * mm for v in CK['glareshield']['EFIS_ctl_panels']['pcb_mm_est']]
    for yy, t in zip(CK['glareshield']['EFIS_ctl_panels']['y'], ('EFIS CTL capt', 'EFIS CTL F/O')):
        s.rect(cx + yy - ew / 2, gy + .017, ew, eh, EST, .9, 5)
        s.a.add_patch(Circle((cx + yy - .035, gy + .05), .016, fc='none', ec=EST, lw=.5, zorder=5.5))
        s.a.add_patch(Circle((cx + yy + .035, gy + .05), .016, fc='none', ec=EST, lw=.5, zorder=5.5))
        s.text(cx + yy, gy + .093, 'BARO  QNH', EST, 4.5, box=False); s.text(cx + yy, gy + .021, 'CSTR WPT VOR NDB ARPT', EST, 4, box=False)
        s.text(cx + yy, gy + .14, '%s %d x %d' % (t, *CK['glareshield']['EFIS_ctl_panels']['pcb_mm_est']), EST, 6, box=False)
    for sg in (-1, 1):
        blk(s, cx + sg * .40 - .03, gy + .07, .06, .035, 'MASTER WARN', fs=4.2); blk(s, cx + sg * .40 - .03, gy + .03, .06, .035, 'MASTER CAUT', fs=4.2)
        blk(s, cx + sg * .32 - .025, gy + .05, .05, .04, 'CHRONO', fs=4.2); blk(s, cx + sg * .47 - .022, gy + .05, .044, .04, 'S/STICK PR.', fs=3.8)
    s.hdim(cx - gw / 2, cx + gw / 2, gy + .175, 'glareshield %.2f' % gw, True, gy + .13, gy + .13, 7)
    # main panel
    py0, py1 = 1.13, 1.7; s.poly([(cx - .85, py1), (cx + .85, py1), (cx + .85, py0 + .17), (cx + .30, py0 + .17), (cx + .30, py0 - .08), (cx - .30, py0 - .08), (cx - .30, py0 + .17), (cx - .85, py0 + .17)], 1.0, INK, z=3)
    du = CK['display_units']['size_mm'][0] * mm; dz = 1.50
    names = ['PFD 1', 'ND 1', 'E/WD (upper ECAM)', 'ND 2', 'PFD 2']
    for yc, t in zip(CK['display_units']['y_centers_approx'], names):
        s.rect(cx + yc - du / 2, dz - du / 2, du, du, INK, 1.0, 5); s.rect(cx + yc - .0825, dz - .0825, .165, .165, HID, .5, 5.1, False)
        s.text(cx + yc, dz + .01, t, INK, 6.5, box=False)
    s.rect(cx - du / 2, dz - du / 2 - .21 + .0, du, du, INK, 1.0, 5); s.rect(cx - .0825, dz - .21 - .0825, .165, .165, HID, .5, 5.1, False)
    s.text(cx, dz - .21, 'SD (lower ECAM)', INK, 6.5, box=False)
    s.hdim(cx + .5 - du / 2, cx + .5 + du / 2, dz - .114, 'DU 184 (7.25 in, S7)', False, None, None, 6.5)
    s.text(cx + .5, dz - .068, 'active 165 (S8)', DIM, 5.5, box=False)
    s.hdim(cx - .72, cx - .5, dz - .125, '220 c/c', True, dz - .092, dz - .092, 6)
    s.vdim(cx + .115, dz - .21, dz, '210', True, cx + .092, cx + .092, 6, side=1)
    # standby + centre instruments
    blk(s, cx - .29, dz + .01, .085, .085, 'ISIS', sub='standby', fs=6)
    blk(s, cx - .29, dz - .1, .085, .06, 'DDRMI', fs=5.5); blk(s, cx - .29, dz - .2, .085, .07, 'CLOCK', fs=5.5)
    blk(s, cx + .205, dz + .01, .085, .075, 'A/BRK', sub='LO MED MAX', fs=5.5); blk(s, cx + .205, dz - .08, .085, .06, 'BRK PRESS', fs=5)
    blk(s, cx + .205, dz - .26, .085, .15, 'L/G lever', sub='+ gear lights', fs=5.5)
    blk(s, cx - .29, dz - .30, .085, .07, 'A/SKID N/W', fs=5)
    for sg, t in ((-1, 'capt'), (1, 'F/O')):
        blk(s, cx + sg * .61 - .2, py0 + .19, .4, .05, 'PFD/ND BRT, XFR, LOUD SPKR, CONSOLE/FLOOR LT (%s)' % t, fs=4.8)
    s.text(cx, py0 - .06, 'centre panel continues down to the pedestal', EST, 5.5, box=False)
    s.hdim(cx - .85, cx + .85, py0 - .14, 'main panel 1.70', True, py0 + .17, py0 + .17, 7)
    s.text(cx, gy + .228, 'GLARESHIELD + MAIN INSTRUMENT PANEL (from the pilots\' eyes)', INK, 9, w='bold')
    # ---------------- overhead (plan, fwd up), rotated: x along the page, columns across
    ow = CK['overhead']['width_approx']; ox0, ox1 = CK['overhead']['x_range']; oL = ox1 - ox0
    ax0, ay0 = .08, .17; colw = CK['overhead']['dzus_column_width_mm'] * mm
    s.rect(ax0, ay0, ow, oL * .58, INK, 1.0, 3)
    s.text(ax0 + ow / 2, ay0 + oL * .58 + .03, 'OVERHEAD (looking up, fwd at top)', INK, 9, w='bold')
    edge = (ow - 5 * colw) / 2
    cols = [[('ADIRS', 3), ('FLT CTL', 1.2), ('EVAC', .8), ('EMER ELEC', 1.6), ('GPWS', .8), ('RCDR', .6), ('OXY', .8), ('CALLS', .7)],
            [('FIRE eng 1', 1.4), ('HYD', 1.6), ('FUEL', 2.2), ('ELEC', 2.6), ('ext LT', 1.0)],
            [('FIRE APU', 1.4), ('AIR COND', 2.6), ('ANTI ICE', 1.0), ('CAB PRESS', 1.4), ('APU', .8), ('SIGNS', .6), ('INT LT', .9)],
            [('FIRE eng 2', 1.4), ('FUEL', 1.6), ('ELEC', 1.2), ('CARGO', 1.0), ('VENT', .8), ('ENG', .8), ('WIPER', .8)],
            [('ADIRS', 1.4), ('FLT CTL', 1.6), ('CVR', .9), ('RAIN RPLNT', .8), ('CALLS', 1.0), ('WIPER', 1.4)]]
    top = ay0 + oL * .58 - .01; avail = oL * .58 * .78
    for ci, col in enumerate(cols):
        x = ax0 + edge + ci * colw; tot = sum(h for _, h in col); y = top
        for t, h in col:
            hh = avail * h / tot; y -= hh; s.rect(x + .003, y, colw - .006, hh - .004, EST, .6, 5); s.text(x + colw / 2, y + hh / 2, t, EST, 5.5, box=False)
    mb = ay0 + .01; s.rect(ax0 + edge, mb, 5 * colw, top - avail - mb - .008, EST, .6, 5)
    s.text(ax0 + ow / 2, mb + (top - avail - mb) / 2, 'aft: maintenance panel + circuit breakers (est.)', EST, 6, box=False)
    s.hdim(ax0 + edge, ax0 + edge + colw, ay0 + oL * .58 + .07, 'DZUS 146', False, None, None, 6)
    s.hdim(ax0, ax0 + ow, ay0 - .04, 'overhead %.2f wide' % ow, True, ay0, ay0, 6.5)
    s.text(ax0 + ow + .03, ay0 + oL * .29, 'drawn at 0.58 of length\nreal x %.2f-%.2f (%.2f long, est.)' % (ox0, ox1, oL), EST, 6.5, ha='left', box=False)
    # ---------------- pedestal (plan, fwd up) at true scale
    pcx, ptop = 2.1, 1.72; pw = CK['pedestal']['width_approx']; px0, px1 = CK['pedestal']['x_range']; plen = px1 - px0
    s.rect(pcx - pw / 2, ptop - plen, pw, plen, INK, 1.0, 3)
    s.text(pcx, ptop + .03, 'PEDESTAL (from above, fwd at top)', INK, 9, w='bold')
    mw, mh = [v * mm for v in CK['pedestal']['MCDU']['size_mm_approx']]
    for yy, t in zip(CK['pedestal']['MCDU']['y'], ('MCDU 1', 'MCDU 2')):
        s.rect(pcx + yy - mw / 2, ptop - .01 - mh, mw, mh, EST, .9, 5); s.rect(pcx + yy - mw / 2 + .012, ptop - .02 - .085, mw - .024, .075, HID, .5, 5.2, False)
        for r in range(5):
            for k in range(6): s.rect(pcx + yy - mw / 2 + .012 + k * .021, ptop - .12 - r * .02, .017, .014, EST, .3, 5.2, False)
        s.text(pcx + yy, ptop - .055, t, EST, 6, box=False)
    cy = ptop - .01; cw = .146
    blk(s, pcx - .055, cy - .06, .11, .06, 'SWTG', fs=5)
    blk(s, pcx - .055, cy - .175, .11, .11, 'ECAM CTL', sub='ENG BLEED ... CLR RCL', fs=5.5)
    blk(s, pcx - .055, cy - .23, .11, .05, 'DU RECONF', fs=4.5)
    tq = ptop - mh - .03
    s.rect(pcx - .073, tq - .40, .146, .40, INK, .9, 5)
    for d in (-.025, .025):
        s.rect(pcx + d - .012, tq - .17, .024, .08, ACC, .8, 5.5)
    s.text(pcx, tq - .05, 'THR LVRS\nTOGA FLX/MCT CL IDLE', ACC, 5, box=False)
    s.text(pcx, tq - .25, 'ENG 1/2 MASTER\nMODE CRANK/NORM/IGN', EST, 5, box=False)
    for sg in (-1, 1):
        s.rect(pcx + sg * .1 - .022, tq - .26, .044, .2, EST, .6, 5); s.text(pcx + sg * .1, tq - .16, 'trim\nwheel', EST, 4.6, box=False)
    blk(s, pcx - .29, tq - .2, .08, .18, 'SPD BRK', fs=5); blk(s, pcx + .21, tq - .2, .08, .18, 'FLAPS', fs=5)
    blk(s, pcx - .29, tq - .38, .08, .14, 'PARK BRK', fs=4.8); blk(s, pcx + .21, tq - .38, .08, .14, 'GRAVITY\nGEAR', fs=4.6)
    ay = tq - .43
    for sg, n in ((-1, '1'), (1, '2')):
        yy = pcx + sg * .16 - cw / 2
        blk(s, yy, ay - .1, cw, .1, 'RMP ' + n, fs=5.5); blk(s, yy, ay - .23, cw, .12, 'ACP ' + n, fs=5.5)
        blk(s, yy, ay - .29, cw, .05, 'LIGHTING', fs=4.8)
        blk(s, yy, ay - .36 + .0, cw, .06, 'FLOOD LT' if sg < 0 else 'PRINTER', fs=4.8)
    blk(s, pcx - .055 - .0, ay - .1, .11, .1, 'WX RADAR', fs=5); blk(s, pcx - .055, ay - .19, .11, .08, 'ATC/TCAS', fs=5)
    blk(s, pcx - .055, ay - .25, .11, .05, 'RUD TRIM', fs=5); blk(s, pcx - .055, ay - .31, .11, .05, 'CKPT DOOR', fs=4.8)
    s.hdim(pcx - pw / 2, pcx + pw / 2, ptop - plen - .04, 'pedestal %.2f' % pw, True, ptop - plen, ptop - plen, 6.5)
    s.hdim(pcx + .16 - mw / 2, pcx + .16 + mw / 2, ptop + .0 - .262, 'MCDU %d x %d' % tuple(CK['pedestal']['MCDU']['size_mm_approx']), True, None, None, 5.5)
    s.vdim(pcx + pw / 2 + .04, ptop - plen, ptop, '%.2f long' % plen, True, pcx + pw / 2, pcx + pw / 2, 6.5)
    s.hdim(pcx - .16 - cw / 2, pcx - .16 + cw / 2, ay - .385, '146 DZUS', False, None, None, 5.5)
    s.text(1.03, .66, 'DZUS panel width 146 mm (5.75 in), height in 9.5 mm (0.375 in) units: ARINC/DZUS standard (bright).\n'
           'All block positions and heights est. from photos and simulator-builder data (S6-S10).\n'
           'Every panel drawn as a labelled block with its size; real switch layouts are not copied.', EST, 6.5, ha='left', box=False)
    s.block('Scale: 1 m = %.0f px (1 mm = %.2f px); overhead drawn at 0.58 of its length.' % (s.k, s.k / 1000),
            ['Display unit size 7.25 in (S7) and active area 165 mm (S8) bright; FCU 264 x 96 and EFIS 161 x 97 from sim-builder PCB outlines (S9, est.).',
             'Centre panel standby block (ISIS, DDRMI, clock) and gear/autobrake block: positions est. from photos.'], units='Units: mm on panel labels, metres elsewhere. Captain on the left in every view.')
    s.save('int-cockpit-panels.png')


if __name__ == '__main__':
    draw_cabin_plan(); draw_cabin_sections(); draw_cockpit(); draw_panels()
    print('out', B.OUT)
