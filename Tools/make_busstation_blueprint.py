# Blueprint sheet of the Chablais bus station's STRUCTURE (island platform + glazed barrel vault), to
# docs/Pictures/busstation-blueprint/: plan.png, section-cross.png, section-long.png, details.png.
# Black background, light lines, dimensioned, metres. Dimensions come from Art/BusStation/busstn_dims.py, the same
# file the model script builds from.
#   python Tools/make_busstation_blueprint.py
import os, sys, math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, Polygon

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Art', 'BusStation'))
import busstn_dims as D

OUT = os.path.join(ROOT, 'docs', 'Pictures', 'busstation-blueprint')
os.makedirs(OUT, exist_ok=True)
BG, INK, DIM, STL, GLS, CON, YEL = '#000000', '#e8edf2', '#8fa3b5', '#cfe3f5', '#5fa8d3', '#7d8a96', '#f2c230'
plt.rcParams.update({'font.size': 11, 'text.color': INK, 'font.family': 'DejaVu Sans'})


def sheet(title, w=22, h=12):
    fig, ax = plt.subplots(figsize=(w, h), facecolor=BG)
    ax.set_facecolor(BG); ax.set_aspect('equal'); ax.axis('off')
    ax.set_title('Chablais bus station - structure - ' + title, color=INK, fontsize=18, pad=16)
    return fig, ax


def save(fig, name, note):
    fig.text(0.01, 0.015, note, color=DIM, fontsize=10, ha='left', va='bottom')
    fig.text(0.99, 0.015, 'Airport Dynasty Pro - own design - metres - 2026-11-27', color=DIM, fontsize=10,
             ha='right', va='bottom')
    fig.savefig(os.path.join(OUT, name), facecolor=BG, dpi=140, bbox_inches='tight')
    plt.close(fig); print('busstation blueprint:', name)


def dim(a, p, q, t, off=(0, 0), c=DIM, fs=11):
    a.annotate('', p, q, arrowprops=dict(arrowstyle='<->', color=c, lw=1.1))
    a.text((p[0] + q[0]) / 2 + off[0], (p[1] + q[1]) / 2 + off[1], t, color=c, ha='center', va='center',
           fontsize=fs, bbox=dict(fc=BG, ec='none', pad=1.5))


def tick(a, x0, x1, y0, y1, c=DIM, lw=0.8, ls='-'):
    a.plot([x0, x1], [y0, y1], color=c, lw=lw, ls=ls)


# ------------------------------------------------------------------ 1  plan
fig, a = sheet('plan', 24, 11)
a.set_xlim(-14, 164); a.set_ylim(-13.5, 18.5)
# the carriageway side (-y) and the terminal pavement side (+y) as background bands
a.add_patch(Rectangle((-10, -14.5), 170, 14.5 - D.PL_HALF, fc='#0a0d10', ec=DIM, lw=0.6))
a.text(75, -13.4, 'bus carriageway and the 12 sawtooth bays A1-A12  (step 2, not this model)', color=DIM, ha='center')
a.add_patch(Rectangle((-10, D.PL_HALF), 170, 12.5, fc='#0a0d10', ec=DIM, lw=0.6))
a.text(75, 15.8, 'terminal pavement  (step 2)', color=DIM, ha='center')
# platform
a.add_patch(Rectangle((0, -D.PL_HALF), D.PL_LEN, D.PL_WID, fc='#15191d', ec=INK, lw=1.8))
for s in (-1, 1):
    ye = s * D.PL_HALF
    a.add_patch(Rectangle((0, ye - (D.KERB_W if s > 0 else 0)), D.PL_LEN, D.KERB_W, fc='#2a3037', ec=INK, lw=0.9))
    yt = ye - s * (D.TACT_OFF + D.TACT_W)
    a.add_patch(Rectangle((0, min(yt, yt + s * D.TACT_W)), D.PL_LEN, D.TACT_W, fc=YEL, alpha=0.55, ec='none'))
    yd = s * (D.PL_HALF - D.DRN_OFF)
    tick(a, 0, D.PL_LEN, yd, yd, GLS, 1.6)
a.text(2, -D.PL_HALF + 1.05, 'tactile edge strip 400 wide, 500 back from the kerb face', color=YEL, fontsize=9)
a.text(2, D.PL_HALF - D.DRN_OFF + 0.5, 'slot drain, 25 mm slot, both sides', color=GLS, fontsize=9)
# arches and their feet
for i in range(D.N_ARCH):
    x = i * D.BAY
    tick(a, x, x, D.EAVE_S_Y - 1.2, D.EAVE_N_Y + 1.2, STL, 1.2)
    for s in (-1, 1):
        a.add_patch(Rectangle((x - D.FOOT_PAD / 2, D.COL_Y[s] - D.FOOT_PAD / 2), D.FOOT_PAD, D.FOOT_PAD,
                              fc='#333a41', ec=INK, lw=0.8))
        a.add_patch(Circle((x, D.COL_Y[s]), D.ARCH_R, fc=STL, ec=INK, lw=0.7))
for s in (-1, 1):
    tick(a, 0, D.PL_LEN, D.EAVE_Y_S[s], D.EAVE_Y_S[s], GLS, 1.0, '--')
for s in (-1, 1):
    a.text(30, D.EAVE_Y_S[s] + s * 0.55, 'eaves beam / gutter line, y = %+.2f' % D.EAVE_Y_S[s],
           color=GLS, fontsize=9, ha='center')
for b in D.BRACE_BAYS:
    x0 = b * D.BAY
    tick(a, x0, x0 + D.BAY, -6.5, 6.5, '#d08f3a', 1.0, '--'); tick(a, x0, x0 + D.BAY, 6.5, -6.5, '#d08f3a', 1.0, '--')
    a.text(x0 + D.BAY / 2, 9.4, 'cable braced bay', color='#d08f3a', fontsize=8, ha='center')
dim(a, (0, D.EAVE_S_Y - 3.4), (D.PL_LEN, D.EAVE_S_Y - 3.4), '%.0f  platform and vault' % D.PL_LEN, (0, -0.8))
dim(a, (0, D.EAVE_S_Y - 2.1), (D.BAY, D.EAVE_S_Y - 2.1), '%.2f' % D.BAY, (0, -0.8))
a.text(D.BAY * 3, D.EAVE_S_Y - 2.1 - 0.8, '%d bays x %.2f = %.0f,  %d arches' % (D.N_BAY, D.BAY, D.PL_LEN, D.N_ARCH),
       color=DIM, fontsize=10, ha='left')
dim(a, (-4.2, -D.PL_HALF), (-4.2, D.PL_HALF), '%.1f' % D.PL_WID, (-1.9, 0))
dim(a, (-8.5, D.COL_S_Y), (-8.5, D.COL_N_Y), '%.1f column span' % D.SPAN, (-2.6, 2.6))
a.text(0, D.EAVE_N_Y + 1.6, 'origin 0,0,0 = west end, on the centreline, ground z 0', color=INK, fontsize=10)
a.plot([0], [0], marker='+', ms=16, color=INK)
save(fig, 'plan.png', 'Plan. Island platform %.0f x %.0f m. The vault leans north: eaves y -6.50 to +14.00.' % (D.PL_LEN, D.PL_WID))

# ------------------------------------------------------------------ 2  cross section
fig, a = sheet('cross section (looking east)', 15, 12)
a.set_xlim(-11.5, 17.5); a.set_ylim(-3.3, 14.4)


def arc_poly(dr0, dr1, t0, t1, n=120, **kw):
    pts = [D.arch_pt(t0 + (t1 - t0) * i / n, dr0) for i in range(n + 1)]
    pts += [D.arch_pt(t1 - (t1 - t0) * i / n, dr1) for i in range(n + 1)]
    return Polygon(pts, closed=True, **kw)


T0 = D.TE          # the arch runs eaves to eaves; the columns hold it up under the arc
# ground
tick(a, -11.5, 17.5, 0, 0, DIM, 1.0)
a.text(-11.2, -0.45, 'ground z 0', color=DIM, fontsize=9)
# glazing, purlins, arch
a.add_patch(arc_poly(D.RP - 0.10, D.RP + 0.14, -D.TE, D.TE, fc=GLS, alpha=0.55, ec=GLS, lw=1.0))
a.add_patch(arc_poly(-D.ARCH_R, D.ARCH_R, -T0, T0, fc=STL, ec=INK, lw=1.4))
for k in D.PUR_K:
    y, z = D.arch_pt(D.row_t(k), D.RP - D.R)
    a.add_patch(Circle((y, z), D.PUR_R, fc='#9fb6c9', ec=INK, lw=0.6))
for s in (-1, 1):
    y, z = D.arch_pt(s * D.TE, D.RP - D.R)
    a.add_patch(Rectangle((y - D.EAVE_W / 2, z - D.EAVE_W / 2), D.EAVE_W, D.EAVE_W, fc='#9fb6c9', ec=INK, lw=0.8))
    gy, gz = y + s * D.GUT_DY, z + D.GUT_DZ
    a.add_patch(Polygon([(gy - D.GUT_W / 2, gz + D.GUT_D), (gy - D.GUT_W / 2, gz), (gy + D.GUT_W / 2, gz),
                         (gy + D.GUT_W / 2, gz + D.GUT_D)], closed=False, fc='none', ec=INK, lw=2.0))
    a.plot([y + s * 0.1, gy], [z - 0.1, gz], color=INK, lw=1.2)
    a.text(gy + s * 1.5, gz + 0.95, 'gutter on brackets', color=DIM, fontsize=9, ha='center')
    a.text(gy + s * 1.5, gz + 0.50, 'off the arch tip', color=DIM, fontsize=9, ha='center')
    # cable chord level
    cy, cz = D.arch_pt(s * 0.57, D.CABLE_DR)
    a.add_patch(Circle((cy, cz), 0.09, fc='#d08f3a', ec='none'))
    # foot: pad, pedestal, plate, column
    fy = D.COL_Y[s]
    zax = D.arch_pt(D.T_COL[s])[1]
    zsof = zax - D.ARCH_R / math.cos(D.T_COL[s])
    a.add_patch(Rectangle((fy - D.FOOT_PAD / 2, 0), D.FOOT_PAD, D.FOOT_PAD_Z, fc='#333a41', ec=INK, lw=1.0))
    a.add_patch(Rectangle((fy - D.PED_W / 2, 0), D.PED_W, D.PED_Z, fc='#3b434b', ec=INK, lw=1.0))
    a.add_patch(Rectangle((fy - D.PLATE_W / 2, D.PED_Z), D.PLATE_W, D.PLATE_T, fc=STL, ec=INK, lw=1.0))
    # column up to the springing, with its head plate
    a.add_patch(Rectangle((fy - D.COL_R, D.FOOT_Z), D.COL_OD, zsof - D.FOOT_Z, fc=STL, ec=INK, lw=1.2))
    a.add_patch(Rectangle((fy - D.COL_HEAD / 2, zsof - 0.055), D.COL_HEAD, 0.065, fc=STL, ec=INK, lw=1.0))
    a.add_patch(Rectangle((fy + s * 0.55 - D.DOWN_OD / 2, 0.07), D.DOWN_OD, D.EAVE_Z + D.GUT_DZ - 1.1,
                          fc='#9fb6c9', ec=INK, lw=0.6))
    dim(a, (fy, -0.75), (0.0, -0.75), '%.1f off the platform centreline' % abs(fy), (0, -0.28))
a.text(3.0, 13.2, 'glazed barrel vault: white tubular steel arches, glazing diagrid between the purlins', color=INK,
       ha='center', fontsize=12)
# platform in the section
a.add_patch(Rectangle((-D.PL_HALF, -0.05), D.PL_WID, D.DECK_Z + 0.05, fc='#15191d', ec=INK, lw=1.6))
for s in (-1, 1):
    a.add_patch(Rectangle((s * D.PL_HALF - (D.KERB_W if s > 0 else 0), D.DECK_Z - D.KERB_DEEP), D.KERB_W,
                          D.KERB_DEEP, fc='#2a3037', ec=INK, lw=0.9))
    yt = s * (D.PL_HALF - D.TACT_OFF - D.TACT_W)
    a.add_patch(Rectangle((min(yt, yt + s * D.TACT_W), D.DECK_Z), D.TACT_W, D.TACT_T + 0.03, fc=YEL, ec='none'))
    yd = s * (D.PL_HALF - D.DRN_OFF)
    a.add_patch(Rectangle((yd - D.DRN_W / 2, D.DECK_Z - D.DRN_D), D.DRN_W, D.DRN_D, fc=BG, ec=GLS, lw=1.1))
a.add_patch(Rectangle((-7.7, D.RAFT_TOP - D.RAFT_T), 22.0, D.RAFT_T, fc='#20262c', ec=DIM, lw=0.8, ls='--'))
a.text(3.3, D.RAFT_TOP - D.RAFT_T / 2, 'foundation raft, under the ground - pads, pedestals and the platform slab all'
                                     ' sit on it', color=DIM, fontsize=9, ha='center', va='center')
dim(a, (D.COL_S_Y, -1.55), (D.COL_N_Y, -1.55), '%.1f span, column to column' % D.SPAN, (0, -0.3))
dim(a, (D.EAVE_S_Y, -2.55), (D.COL_S_Y, -2.55), '%.1f cantilever' % D.CANT_S, (0, -0.3))
dim(a, (D.COL_N_Y, -2.55), (D.EAVE_N_Y, -2.55), '%.1f' % D.CANT_N, (0, -0.3))
dim(a, (D.YC, 0), (D.YC, D.CROWN), '%.1f to the crown' % D.CROWN, (1.5, 0))
dim(a, (-D.PL_HALF, 1.9), (D.PL_HALF, 1.9), '%.1f platform' % D.PL_WID, (0, 0.38))
dim(a, (D.EAVE_N_Y, 0), (D.EAVE_N_Y, D.EAVE_Z), '%.2f eaves' % D.EAVE_Z, (1.45, 0))
dim(a, (D.EAVE_S_Y, D.DECK_Z), (D.EAVE_S_Y, D.EAVE_Z), '%.2f clear over the deck' % (D.EAVE_Z - D.DECK_Z),
    (-1.6, 0))
a.text(-11.2, 11.7, 'the vault is not centred on the platform: it leans to the terminal (+y) and shelters the walk'
       ' to the doors', color=DIM, fontsize=10, ha='left')
a.text(-11.2, 11.1, 'south columns y -3.0 (1.0 m in from the bus-side kerb), north columns y +13.0 on the pavement',
       color=DIM, fontsize=10, ha='left')
a.text(-D.PL_HALF - 0.1, 0.95, 'kerb %.0f mm, deck top z %.2f' % (D.KERB_H * 1000, D.DECK_Z), color=DIM, fontsize=9,
       ha='right')
a.text(4.4, 8.4, 'arch tube %.0f OD\npurlin %.0f OD, %d rows\nglass %.0f mm' %
       (D.ARCH_OD * 1000, D.PUR_OD * 1000, len(D.PUR_K), D.PANE_T * 1000), color=DIM, fontsize=10)
save(fig, 'section-cross.png', 'Cross section. Arc radius %.3f m, centre z %.3f m.' % (D.R, D.CZ))

# ------------------------------------------------------------------ 3  long section
fig, a = sheet('long section on the centreline (looking north)', 24, 9)
a.set_xlim(-10, 162); a.set_ylim(-7.0, 14.2)
tick(a, -10, 162, 0, 0, DIM, 1.0)
for i in range(D.N_ARCH):
    x = i * D.BAY
    zsf = D.arch_pt(D.T_COL[-1])[1] - D.ARCH_R / math.cos(D.T_COL[-1])
    a.add_patch(Rectangle((x - D.ARCH_R, zsf), D.ARCH_OD, D.CROWN - zsf, fc=STL, ec=INK, lw=0.8))
    a.add_patch(Rectangle((x - D.COL_R, 0), D.COL_OD, zsf, fc=STL, ec=INK, lw=0.9))
    a.add_patch(Rectangle((x - D.PED_W / 2, 0), D.PED_W, D.PED_Z, fc='#3b434b', ec=INK, lw=0.7))
    if i % 2 == 0:
        a.add_patch(Rectangle((x + 0.55, 0.07), D.DOWN_OD, D.EAVE_Z + D.GUT_DZ - 1.1,
                              fc='#9fb6c9', ec=INK, lw=0.5))
for k in list(D.PUR_K) + [0, D.N_ROW]:
    z = D.arch_pt(D.row_t(k), D.RP - D.R)[1]
    eave = k in (0, D.N_ROW)
    tick(a, 0, D.PL_LEN, z, z, '#9fb6c9', 2.2 if eave else 1.1)
for b in D.BRACE_BAYS:
    x0 = b * D.BAY
    for s in (-1, 1):
        z0, z1 = D.arch_pt(0.30, D.CABLE_DR)[1], D.arch_pt(0.84, D.CABLE_DR)[1]
        tick(a, x0, x0 + D.BAY, z0 if s > 0 else z1, z1 if s > 0 else z0, '#d08f3a', 1.3)
    a.text(x0 + D.BAY / 2, D.CROWN + 0.55, 'braced', color='#d08f3a', fontsize=9, ha='center')
a.add_patch(Rectangle((0, 0), D.PL_LEN, D.DECK_Z, fc='#2a3037', ec=INK, lw=1.2))
a.add_patch(Rectangle((0, D.RAFT_TOP - D.RAFT_T), D.PL_LEN + 0.6, D.RAFT_T, fc='#20262c', ec=DIM, lw=0.8, ls='--'))
dim(a, (0, -1.1), (D.PL_LEN, -1.1), '%.0f' % D.PL_LEN, (0, -0.35))
dim(a, (0, D.CROWN + 1.5), (D.BAY, D.CROWN + 1.5), '%.2f' % D.BAY, (0, 0.4))
dim(a, (D.BAY, D.CROWN + 1.5), (D.BAY * 2, D.CROWN + 1.5), '%.2f' % D.BAY, (0, 0.4))
a.text(D.BAY * 2.4, D.CROWN + 1.9, '%d arches at %.2f, %d bays' % (D.N_ARCH, D.BAY, D.N_BAY), color=DIM, fontsize=11)
a.text(75, -4.4, ha='center', s= 'purlins run the whole 150 m, arch to arch - the vault is one connected structure, not separate hoops',
       color=DIM, fontsize=11)
a.text(75, -5.6, ha='center', s= 'downpipe at every second column, on its clamps, into the footing', color=DIM, fontsize=11)
save(fig, 'section-long.png', 'Long section. Purlin lines shown at their arc heights on the centreline.')

# ------------------------------------------------------------------ 4  details
fig, ax = plt.subplots(1, 3, figsize=(22, 9), facecolor=BG)
for a, t in zip(ax, ('A  column foot (mm)', 'B  purlin to arch, glazing (mm)', 'C  gutter bracket (mm)')):
    a.set_facecolor(BG); a.set_aspect('equal'); a.axis('off'); a.set_title(t, color=INK, fontsize=15)
fig.suptitle('Chablais bus station - structure - details: nothing floats', color=INK, fontsize=18)

a = ax[0]; a.set_xlim(-1.1, 1.1); a.set_ylim(-0.75, 1.5)
a.add_patch(Rectangle((-1.05, -0.72), 2.1, 0.70, fc='#20262c', ec=DIM, lw=0.8, ls='--'))
a.text(0, -0.37, 'foundation raft', color=DIM, ha='center', fontsize=9)
tick(a, -1.1, 1.1, 0, 0, DIM)
a.add_patch(Rectangle((-D.FOOT_PAD / 2, -0.3), D.FOOT_PAD, 0.3 + D.FOOT_PAD_Z, fc='#333a41', ec=INK, lw=1.2))
a.add_patch(Rectangle((-D.PED_W / 2, 0), D.PED_W, D.PED_Z, fc='#3b434b', ec=INK, lw=1.2))
a.add_patch(Rectangle((-D.PLATE_W / 2, D.PED_Z), D.PLATE_W, D.PLATE_T, fc=STL, ec=INK, lw=1.2))
for s in (-1, 1):
    a.add_patch(Rectangle((s * 0.22 - D.BOLT_OD / 2, D.PED_Z - 0.02), D.BOLT_OD, D.PLATE_T + 0.07, fc=YEL, ec=INK, lw=0.6))
    a.add_patch(Polygon([(s * 0.12, D.PED_Z + D.PLATE_T), (s * 0.28, D.PED_Z + D.PLATE_T), (s * 0.12, D.PED_Z + 0.42)],
                        fc=STL, ec=INK, lw=1.0))
a.add_patch(Rectangle((-D.ARCH_R, D.PED_Z + D.PLATE_T - 0.01), D.ARCH_OD, 1.0, fc=STL, ec=INK, lw=1.4))
dim(a, (-D.FOOT_PAD / 2, -0.42), (D.FOOT_PAD / 2, -0.42), '%.0f pad' % (D.FOOT_PAD * 1000), (0, -0.07))
dim(a, (-0.95, 0), (-0.95, D.PED_Z), '%.0f' % (D.PED_Z * 1000), (-0.12, 0))
dim(a, (0.72, D.PED_Z), (0.72, D.PED_Z + D.PLATE_T), '%.0f plate' % (D.PLATE_T * 1000), (0.26, 0))
a.text(0, 1.3, 'tube - welded base shoe + 2 gussets - %.0f plate -\n4 holding-down bolts - %.0f pedestal - pad - raft'
       % (D.PLATE_T * 1000, D.PED_W * 1000), color=DIM, ha='center', fontsize=10)

a = ax[1]; a.set_xlim(-0.55, 0.95); a.set_ylim(-0.45, 0.65)
a.add_patch(Circle((0, 0), D.ARCH_R, fc=STL, ec=INK, lw=1.4)); a.text(0, -0.03, 'arch', color='#10161c', ha='center', fontsize=9)
py = D.RP - D.R
a.add_patch(Circle((0, py), D.PUR_R, fc='#9fb6c9', ec=INK, lw=1.2))
a.add_patch(Rectangle((-0.17, D.ARCH_R - 0.03), 0.34, py - D.ARCH_R + 0.05, fc='none', ec=YEL, lw=1.4))
a.text(0.21, py - 0.03, 'purlin %.0f OD, 40 mm into the tube,\nwelded saddle cleat %.0f' %
       (D.PUR_OD * 1000, D.CLEAT_T * 1000), color=DIM, fontsize=10, va='center')
a.add_patch(Rectangle((-0.5, py - D.PANE_T / 2), 0.33, D.PANE_T, fc=GLS, alpha=0.5, ec=GLS, lw=1.0))
a.add_patch(Rectangle((-0.5, py - D.GL_BAR_D / 2), 0.09, D.GL_BAR_D, fc='#9fb6c9', ec=INK, lw=1.0))
a.text(-0.52, py + 0.18, 'glass %.0f, edge %.0f into the bar' % (D.PANE_T * 1000, 40), color=DIM, fontsize=10)
a.text(-0.52, -0.33, 'the pane mid-plane runs through the purlin axes, so\nevery edge is held, never grazing',
       color=DIM, fontsize=10)

a = ax[2]; a.set_xlim(-0.6, 1.3); a.set_ylim(-0.75, 0.75)
a.add_patch(Circle((0, 0), D.ARCH_R, fc=STL, ec=INK, lw=1.4))
a.add_patch(Rectangle((-D.EAVE_W / 2, py - D.EAVE_W / 2), D.EAVE_W, D.EAVE_W, fc='#9fb6c9', ec=INK, lw=1.0))
a.text(0.0, py + 0.24, 'eaves beam %.0f' % (D.EAVE_W * 1000), color=DIM, fontsize=9, ha='center')
gy, gz = D.GUT_DY, D.GUT_DZ
a.add_patch(Polygon([(gy - D.GUT_W / 2, gz + D.GUT_D), (gy - D.GUT_W / 2, gz), (gy + D.GUT_W / 2, gz),
                     (gy + D.GUT_W / 2, gz + D.GUT_D)], closed=False, fc='none', ec=INK, lw=2.4))
a.add_patch(Polygon([(0.05, -0.12), (gy + D.GUT_W / 2, gz - 0.01), (gy + D.GUT_W / 2, gz + 0.06), (0.05, 0.06)],
                    fc=STL, ec=INK, lw=1.0))
a.text(0.1, 0.42, 'bracket plate %.0f, %.0f mm into the arch tube,\nthe gutter sole bedded %.0f mm into it'
       % (D.BRK_T * 1000, 50, 20), color=DIM, fontsize=10)
dim(a, (gy - D.GUT_W / 2, gz - 0.15), (gy + D.GUT_W / 2, gz - 0.15), '%.0f' % (D.GUT_W * 1000), (0, -0.07))
dim(a, (gy + D.GUT_W / 2 + 0.12, gz), (gy + D.GUT_W / 2 + 0.12, gz + D.GUT_D), '%.0f' % (D.GUT_D * 1000), (0.14, 0))
fig.text(0.01, 0.015, 'Details A-C: every arch foot carries down to a real footing; every joint is a plate, a flange'
                      ' or a weld.', color=DIM, fontsize=10)
fig.savefig(os.path.join(OUT, 'details.png'), facecolor=BG, dpi=140, bbox_inches='tight')
plt.close(fig); print('busstation blueprint: details.png')
print('busstation blueprint: R %.4f  CZ %.4f  TE %.4f rad  eaves y %.4f' % (D.R, D.CZ, D.TE, D.EAVE_Y))
