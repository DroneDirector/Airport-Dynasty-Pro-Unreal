# GA-8: the seats along the curve from above (ceiling and roof off), with the widths; the option with an aisle through the rows.
from ga_common import *
RAW = os.environ.get('GA_OUT') or 'F:/ADP-Export/ga_raw'
CX, CZ, HW = -216.5, 77.0, 21.0; HH = HW * 9 / 16


def pic(src, dst, title, opt):
    p = os.path.join(RAW, src + '.png')
    if not os.path.exists(p): return
    fig = plt.figure(figsize=(W, H), dpi=DPI); fig.patch.set_facecolor(BG); ax = fig.add_axes([0, 0, 1, 1]); ax.imshow(plt.imread(p), extent=(CX - HW, CX + HW, CZ - HH, CZ + HH), interpolation='none'); ax.axis('off'); ax.set_xlim(CX - HW, CX + HW); ax.set_ylim(CZ - HH, CZ + HH)
    bb = dict(fc='#05080c', ec='none', pad=3, alpha=0.85)

    def wd(a, b, t, off=(0, 0), col='#ffd166', fs=19):
        ax.annotate('', xy=a, xytext=b, arrowprops=dict(arrowstyle='<->', color=col, lw=3, shrinkA=0, shrinkB=0), zorder=20)
        ax.text((a[0] + b[0]) / 2 + off[0], (a[1] + b[1]) / 2 + off[1], t, color=col, fontsize=fs, weight='bold', ha='center', va='center', bbox=bb, zorder=21)

    for t, off in ((SEC_W[4], (-0.95, 0.35)), (SEC_E[6], (0.95, 0.1))): wd(A.pt(t, SOFF + KNEE), A.pt(t, -0.07), '1.55', off)
    xw, zw = A.pt(SEC_W[0] - SP / 2, SOFF); wd((-233.55, zw), (xw - 0.05, zw), '1.5', (0, -0.8)); xe, ze = A.pt(SEC_E[-1] + SP / 2, SOFF); wd((xe + 0.05, ze), (-213.58, ze), '1.2', (0, 0.8))
    xa, za = A.pt(SEC_W[-1] + SP / 2, SOFF); xb, zb = A.pt(SEC_E[0] - SP / 2, SOFF); wd((xa + 0.03, za - 0.95), (xb - 0.03, zb - 1.2), '2.42', (0, -0.65), '#3ddc6a', 22)
    k = 77.4 + KNEE; wd((-231.2, k), (-231.2, 80.06), '2.2', (0.95, -0.3)); wd((-220.8, k), (-220.8, 82.2), '4.4', (0.9, -0.6)); wd((-217.6, 80.75), (-217.6, 82.34), '1.6', (-0.9, 0.0))
    wd((-233.55, 74.4), ((ROWS_OPT[0][0] if opt else ROW0) - .34, 74.4), '1.4' if opt else '2.6', (0, 0.75)); wd(((ROWS_OPT[1][-1] if opt else ROW0 + 36 * .62) + .34, 74.4), (-205.44, 74.4), '1.3' if opt else '2.5', (0, 0.75))
    gx = A.pt(GAP_T, SOFF)[0]
    if opt:
        for z in ROWZ: wd((-224.69, z), (-222.27, z), '2.42', (0, 0), '#3ddc6a', 17)
        ax.annotate('', xy=(gx, 84.4), xytext=(gx, 67.0), arrowprops=dict(arrowstyle='-|>', color='#3ddc6a', lw=7, mutation_scale=40, alpha=0.65), zorder=15); ax.text(-222.0, 66.5, 'door from the walkway', color='#f0f5fa', fontsize=17, ha='left', va='center', bbox=bb, zorder=21)
    else:
        ax.annotate('', xy=(gx, 84.4), xytext=(gx, 78.1), arrowprops=dict(arrowstyle='-|>', color='#3ddc6a', lw=7, mutation_scale=40, alpha=0.75), zorder=15)
        for x in (-232.3, -206.7): ax.annotate('', xy=(x, 79.4), xytext=(x, 69.2), arrowprops=dict(arrowstyle='-|>', color='#3ddc6a', lw=5, mutation_scale=32, alpha=0.6), zorder=14)
        ax.text(-219.5, 75.9, "today's three rows of 37: no aisle through them, the way to the gap goes round their two ends", color='#f0f5fa', fontsize=18, ha='center', va='center', bbox=bb, zorder=21)
    for x, z, s in ((-227.9, 79.9, '12 seats'), (-219.3, 80.0, 'bin'), (-214.6, 80.2, 'desk'), (-211.7, 83.0, 'jetway entrance'), (-204.0, 75, 'stair'), (-200.8, 80.6, 'lift'), (-219.6, 83.9, '12 seats')):
        ax.text(x, z, s, color='#f0f5fa', fontsize=17, ha='center', va='center', bbox=bb, zorder=21)
    ax.text(CX - HW + 0.4, CZ + HH - 0.45, title, color='#ffd166', fontsize=24, weight='bold', va='top', bbox=dict(fc='#05080c', ec='none', pad=6, alpha=0.88), zorder=22)
    check(fig, dst); fig.savefig(os.path.join(OUT, dst), dpi=DPI, facecolor=BG); plt.close(fig)


pic('seats', 'GA-8-Seats-Top.png', 'The seats along the curve from above (ceiling off): clear widths in metres, knees counted', False)
pic('seatsopt', 'GA-8b-Seats-Top-Option.png', 'OPTION: the same aisle through the three rows too (4 seats of each row moved to its ends, still 37 each)', True)
