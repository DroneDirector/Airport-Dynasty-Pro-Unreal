# Part 5 of the jetway details (to-do 84c console, 84d rotunda column, 84 service door) - executed by
# Tools/jetway_details.py after its other parts (shares its g, N, par, MAT, Geo, add_node, rel, acc_read, acc_new ...).
for nm, col, em in (('jwLampGreen', (0.1, 0.75, 0.25), (0.05, 0.6, 0.15)), ('jwLampAmber', (1.0, 0.62, 0.1), (0.8, 0.45, 0.05))):
    if nm not in MAT:
        g['materials'].append({'name': nm, 'pbrMetallicRoughness': {'baseColorFactor': [*col, 1], 'metallicFactor': 0.0, 'roughnessFactor': 0.25}, 'emissiveFactor': list(em)})
        MAT[nm] = len(g['materials']) - 1

def frame_root(i):
    while i in par and not (N[i].get('name', '') in JROOTS): i = par[i]
    return i

def desc(i):
    out = []; todo = [i]
    while todo:
        k = todo.pop(); out.append(k); todo += N[k].get('children', [])
    return out

def drop_where(node_i, frame_i, pred, mats=None):
    if 'mesh' not in N[node_i]: return 0
    R0 = frame_root(node_i)
    M = np.linalg.inv(rel(R0, frame_i)) @ rel(R0, node_i); gone = 0
    for p in g['meshes'][N[node_i]['mesh']]['primitives']:
        if 'indices' not in p or (mats and g['materials'][p.get('material', 0)]['name'] not in mats): continue
        P = acc_read(p['attributes']['POSITION']).astype(float); P = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]
        I = acc_read(p['indices']).reshape(-1, 3).astype(np.uint32)
        a, b2, c2 = P[I[:, 0]], P[I[:, 1]], P[I[:, 2]]
        m = pred((a + b2 + c2) / 3, a, b2, c2)
        if m.any():
            keep = I[~m].reshape(-1)
            if len(keep) == 0: keep = I[:1].reshape(-1)
            p['indices'] = acc_new(keep.reshape(-1, 1), 5125, 'SCALAR', 34963); gone += int(m.sum())
    return gone

stat5 = {'console': 0, 'columns': 0, 'doors': 0}
for J in sorted(JROOTS):
    ji = next(i for i, nd in enumerate(N) if nd.get('name') == J)
    s3 = next(i for i, nd in enumerate(N) if nd.get('name') == f'{J}/TUNNEL-3')
    # ---- the console (to-do 84c, his photos ref-console-1/2): the operator stands on its -z side
    con = next((i for i, nd in enumerate(N) if nd.get('name') == f'{J}/CAB-CONSOLE'), None)
    if con is not None:
        pics = []
        for c in N[con].get('children', []):
            if N[c].get('name', '').startswith(f'{J}/CAB-CONSOLE:') and 'mesh' in N[c] and '#new' not in N[c].get('name', ''): del N[c]['mesh']
            elif 'mesh' in N[c] and N[c].get('name', '').startswith('jwp'): pics.append(c)
        if pics:
            N[pics[0]]['translation'] = [0.0, 1.19, 0.215]
            for c in pics[1:]: del N[c]['mesh']
        G_ = {k: Geo() for k in ('cab', 'top', 'dark', 'chrome', 'rubber', 'red', 'yellow', 'green', 'amber', 'label', 'vent')}
        G_['cab'].box(-0.40, 0.40, 0.06, 0.90, -0.30, 0.30)
        G_['dark'].box(-0.42, 0.42, 0.0, 0.06, -0.32, 0.32)
        y0, y1, zf, zb = 0.92, 1.06, -0.34, 0.34
        G_['top'].beam((0, y0, zf), (0, y1, zb), 0.84, 0.012, (0, 1, 0))
        G_['cab'].box(-0.40, 0.40, 0.90, 0.98, 0.0, 0.30); G_['cab'].box(-0.40, 0.40, 0.90, 1.015, 0.17, 0.30)
        tvec = np.array([0, y1 - y0, zb - zf]); tvec /= np.linalg.norm(tvec); nvec = np.array([0, tvec[2], -tvec[1]])
        def on(x, z, lift=0.006):
            t = (z - zf) / (zb - zf); return np.array([x, y0 + (y1 - y0) * t, z]) + nvec * lift
        for (x, z) in ((-0.38, -0.31), (0.38, -0.31), (-0.38, 0.31), (0.38, 0.31)): G_['chrome'].cylinder(on(x, z), nvec, 0.008, 0.004, 10)
        for col in (-0.31, -0.23):
            for r in range(5):
                z = -0.25 + r * 0.065
                G_['chrome'].cylinder(on(col, z), nvec, 0.022, 0.008, 16); G_['dark'].cylinder(on(col, z, 0.012), nvec, 0.015, 0.012, 14)
        kp = on(-0.33, 0.13); G_['chrome'].cylinder(kp, nvec, 0.02, 0.012, 16); G_['dark'].box(-0.335, -0.325, kp[1] + 0.01, kp[1] + 0.025, 0.125, 0.135)
        jp = on(-0.06, -0.13)
        for k, rr in enumerate((0.06, 0.052, 0.044, 0.036, 0.028)): G_['rubber'].cylinder(jp + nvec * (k * 0.022), nvec, rr, 0.022, 18)
        G_['dark'].cylinder(jp + nvec * 0.11, nvec, 0.012, 0.09, 10); G_['rubber'].cylinder(jp + nvec * 0.19, nvec, 0.026, 0.06, 16)
        ep = on(0.16, -0.06)
        G_['yellow'].cylinder(ep, nvec, 0.062, 0.006, 24); G_['red'].cylinder(ep + nvec * 0.006, nvec, 0.028, 0.03, 18); G_['red'].cylinder(ep + nvec * 0.036, nvec, 0.046, 0.026, 24)
        for (x, z, k) in ((0.13, 0.13, 'red'), (0.22, 0.19, 'green'), (0.31, 0.13, 'amber'), (0.22, 0.07, 'red')):
            G_['chrome'].cylinder(on(x, z), nvec, 0.026, 0.006, 16); G_[k].cylinder(on(x, z, 0.012), nvec, 0.02, 0.014, 16)
            lp = on(x + 0.05, z, 0.007); G_['label'].box(lp[0] - 0.02, lp[0] + 0.02, lp[1] - 0.002, lp[1] + 0.002, lp[2] - 0.008, lp[2] + 0.008)
        G_['top'].box(-0.20, 0.20, 1.33, 1.345, 0.11, 0.30); G_['top'].box(-0.205, -0.195, 1.06, 1.34, 0.11, 0.30); G_['top'].box(0.195, 0.205, 1.06, 1.34, 0.11, 0.30)
        G_['dark'].box(-0.19, 0.19, 1.05, 1.33, 0.225, 0.27)
        for (vy0, vy1) in ((0.45, 0.70), (0.12, 0.32)):
            G_['vent'].box(0.08, 0.33, vy0, vy1, -0.33, -0.30)
            yy = vy0 + 0.02
            while yy < vy1 - 0.02: G_['vent'].beam((0.09, yy, -0.335), (0.32, yy, -0.335), 0.012, 0.004, (0, 0.6, -0.8)); yy += 0.022
        G_['yellow'].box(-0.31, -0.23, 0.62, 0.70, -0.305, -0.30)
        for (a0, a1, b0, b1) in ((-0.38, 0.05, 0.09, 0.094), (-0.38, 0.05, 0.86, 0.864), (-0.38, -0.376, 0.09, 0.864), (0.046, 0.05, 0.09, 0.864)):
            G_['dark'].box(a0, a1, b0, b1, -0.303, -0.30)
        G_['chrome'].box(-0.03, 0.0, 0.46, 0.56, -0.315, -0.30)
        for k, mat in (('cab', 'jwPanel'), ('top', 'jwAluB'), ('dark', 'jwDark'), ('chrome', 'jwChrome'), ('rubber', 'jwRubber'), ('red', 'jwRed'),
                       ('yellow', 'yellow'), ('green', 'jwLampGreen'), ('amber', 'jwLampAmber'), ('label', 'jwGauge'), ('vent', 'jwPanel')):
            add_node(con, f'{J}/CAB-CONSOLE:{mat}#new', mat, G_[k])
        stat5['console'] += 1
    # ---- the rotunda's column(s) (to-do 84d): the old ones go, the new ones stand where they stood
    ro = next((i for i, nd in enumerate(N) if nd.get('name') == f'{J}/ROTUNDA'), None)
    if ro is not None:
        pts = []
        for d in desc(ro):
            nd = N[d]
            if nd.get('name', '') == f'{J}/ROTUNDA:jwAluB' and 'mesh' in nd:
                M = np.linalg.inv(rel(ji, ro)) @ rel(ji, d)
                P = np.vstack([acc_read(p['attributes']['POSITION']) for p in g['meshes'][nd['mesh']]['primitives']]).astype(float)
                W = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]; pts.append(W[W[:, 1] < -1.0])
                drop_where(d, ro, lambda c, a, b2, c2: (a[:, 1] < -0.45) & (b2[:, 1] < -0.45) & (c2[:, 1] < -0.45))
        pts = np.vstack(pts) if pts else np.zeros((0, 3))
        cols = []
        for p in pts:
            if not any(math.hypot(p[0] - q[0], p[2] - q[1]) < 1.0 for q in cols): cols.append([p[0], p[2]])
        # (a column has its height of points; a foot plate's corner alone is no column)
        cols = [c for c in cols if (np.hypot(pts[:, 0] - c[0], pts[:, 2] - c[1]) < 1.0).sum() >= 20]
        ground = float(pts[:, 1].min()) if len(pts) else -5.3
        cols = [[float(np.mean(pts[np.hypot(pts[:, 0] - c[0], pts[:, 2] - c[1]) < 1.0][:, 0])), float(np.mean(pts[np.hypot(pts[:, 0] - c[0], pts[:, 2] - c[1]) < 1.0][:, 2]))] for c in cols]
        gc = Geo(); gp = Geo(); gb2 = Geo(); gd = Geo()
        for (cx, cz) in cols:
            base = np.array([cx, ground, cz]); top = -0.42
            gp.box(cx - 0.6, cx + 0.6, ground - 0.02, ground + 0.10, cz - 0.6, cz + 0.6)
            gc.cylinder(base + (0, 0.10, 0), (0, 1, 0), 0.40, 0.045, 32)
            for k in range(8):
                a = 2 * math.pi * k / 8; dv = np.array([math.cos(a), 0, math.sin(a)])
                gc.beam(base + (0, 0.17, 0) + dv * 0.23, base + (0, 0.42, 0) + dv * 0.23, 0.02, 0.10, tuple(dv))
                gb2.cylinder(base + (0, 0.145, 0) + np.array([math.cos(a + 0.39), 0, math.sin(a + 0.39)]) * 0.33, (0, 1, 0), 0.016, 0.05, 8)
            h = top - (ground + 0.145); rr = (0.22, 0.235, 0.25)
            for k in range(3):
                ya = ground + 0.145 + h * k / 3; yb = ground + 0.145 + h * (k + 1) / 3
                gc.cylinder((cx, ya, cz), (0, 1, 0), rr[k], yb - ya, 28)
                gc.cylinder((cx, yb - 0.03, cz), (0, 1, 0), rr[k] + 0.03, 0.06, 28)
            gc.box(cx - 0.32, cx + 0.32, top, top + 0.06, cz - 0.32, cz + 0.32)
            dirc = np.array([-cx, 0, -cz]); nrm = np.linalg.norm(dirc); dirc = dirc / nrm if nrm > 1e-6 else np.array([1.0, 0, 0])
            gc.beam((cx, top - 0.6, cz), (cx + dirc[0] * 0.8, -0.30, cz + dirc[2] * 0.8), 0.14, 0.14, (0, 1, 0))
            for k in range(4):
                a = math.pi / 4 + k * math.pi / 2; gb2.cylinder((cx + 0.26 * math.cos(a), top + 0.06, cz + 0.26 * math.sin(a)), (0, 1, 0), 0.014, 0.03, 8)
            gd.box(cx + 0.36, cx + 0.56, ground + 0.10, ground + 0.62, cz - 0.18, cz + 0.18)
        add_node(ro, f'{J}/ROTUNDA:jwTruss#column', 'jwTruss', gc); add_node(ro, f'{J}/ROTUNDA:jwDark#column-pad', 'jwDark', gp)
        add_node(ro, f'{J}/ROTUNDA:jwAluB#column-bolts', 'jwAluB', gb2); add_node(ro, f'{J}/ROTUNDA:jwPanel#column-box', 'jwPanel', gd)
        stat5['columns'] += len(cols)
    # ---- the service door in the cab-end section's side by the stair landing
    lr = next((par[i] for i, nd in enumerate(N) if nd.get('name', '').startswith(f'{J}/STAIR-LANDING:')), None)
    if lr is not None:
        lt = N[lr]['translation']; sg = 1.0 if lt[0] > 0 else -1.0; zd = lt[2]; X = HW[2]; FYc = FY[2]; Hc = H[2] + FYc
        W0, W1 = zd - 0.85, zd + 0.85; D0, D1 = zd - 0.50, zd + 0.50; DH = 2.10 + FYc
        for d in desc(s3):
            nm = N[d].get('name', '')
            if 'STAIR' in nm or '#' in nm: continue
            def cut(c, a, b2, c2, sg=sg):
                side = (c[:, 0] * sg > X - 0.10) & (c[:, 0] * sg < X + 0.30)
                zmin = np.minimum(np.minimum(a[:, 2], b2[:, 2]), c2[:, 2]); zmax = np.maximum(np.maximum(a[:, 2], b2[:, 2]), c2[:, 2])
                hit = (zmax > W0 - 0.05) & (zmin < W1 + 0.05)
                tall = (c[:, 1] > FYc + 0.02) & (c[:, 1] < Hc - 0.01)
                return side & tall & hit
            drop_where(d, s3, cut, ('glassOuter', 'jwWall', 'jwTruss', 'jwAluB', 'jwPanel', 'jwChrome'))
        # the sill's bead and ledge (and the channel above the floor) stop either side of the doorway
        for d in desc(s3):
            if N[d].get('name', '').endswith('#sill'):
                drop_where(d, s3, lambda c, a, b2, c2, sg=sg: (c[:, 0] * sg > X) & (c[:, 1] > FYc + 0.02)
                           & (np.maximum(np.maximum(a[:, 2], b2[:, 2]), c2[:, 2]) > D0 - 0.05) & (np.minimum(np.minimum(a[:, 2], b2[:, 2]), c2[:, 2]) < D1 + 0.05))
        gg2 = Geo(); gw = Geo(); gsk = Geo(); gfr = Geo()
        for (za, zb_) in ((0.02, W0), (W1, L[2] - 0.02)):
            gg2.box(sg * (X - 0.028), sg * (X + 0.028), 0.45 + FYc, Hc - 0.01, za, zb_)
            gw.box(sg * (X - 0.07), sg * (X + 0.06), FYc, 0.45 + FYc, za, zb_)
            gsk.box(sg * (X + 0.06), sg * (X + 0.07), FYc, 0.45 + FYc, za, zb_)
            gfr.box(sg * (X + 0.02), sg * (X + 0.11), 0.45 + FYc, Hc - 0.01, za - 0.04 if za > 1 else za, za + 0.04 if za > 1 else za + 0.04)
        for (za, zb_, ya, yb) in ((W0, D0, FYc, Hc), (D1, W1, FYc, Hc), (D0, D1, DH, Hc)):
            gw.box(sg * (X - 0.07), sg * (X + 0.06), ya, yb, za, zb_); gsk.box(sg * (X + 0.06), sg * (X + 0.07), ya, yb, za, zb_)
        for (za, zb_, ya, yb) in ((D0 - 0.04, D0, FYc, DH + 0.04), (D1, D1 + 0.04, FYc, DH + 0.04), (D0 - 0.04, D1 + 0.04, DH, DH + 0.04)):
            gfr.box(sg * (X - 0.075), sg * (X + 0.085), ya, yb, za, zb_)
        for zz in (W0 - 0.04, W1):
            gfr.box(sg * (X + 0.02), sg * (X + 0.11), FYc - 0.2, Hc, zz, zz + 0.04)
        gfr.box(sg * (X + 0.07), sg * (X + 0.16), DH + 0.12, DH + 0.24, zd - 0.12, zd + 0.12)
        add_node(s3, f'{J}/TUNNEL-3:glassOuter#svc', 'glassOuter', gg2); add_node(s3, f'{J}/TUNNEL-3:jwWall#svc-wall', 'jwWall', gw)
        add_node(s3, f'{J}/TUNNEL-3:jwTruss#svc-skin', 'jwTruss', gsk); add_node(s3, f'{J}/TUNNEL-3:jwAluB#svc-frame', 'jwAluB', gfr)
        lamp = Geo(); lamp.box(sg * (X + 0.16), sg * (X + 0.17), DH + 0.14, DH + 0.22, zd - 0.10, zd + 0.10)
        add_node(s3, f'{J}/TUNNEL-3:{J}/LED#svc-lamp', f'{J}/LED' if f'{J}/LED' in MAT else 'jwPanel', lamp)
        N.append({'name': f'{J}/SERVICE-DOOR', 'translation': [sg * (X - 0.005), FYc + 0.005, D0 + 0.005]})
        N[s3]['children'].append(len(N) - 1); par[len(N) - 1] = s3; lf = len(N) - 1
        LW, LH = (D1 - D0) - 0.01, DH - FYc - 0.01
        gl_ = Geo(); gwin = Geo(); gbar = Geo(); ghg = Geo()
        gl_.box(-0.025, 0.025, 0, LH, 0, LW)
        gwin.box(-0.028, 0.028, 1.25, 1.80, 0.30, 0.70)
        ghg.box(-0.03, 0.03, 1.22, 1.25, 0.27, 0.73); ghg.box(-0.03, 0.03, 1.80, 1.83, 0.27, 0.73)
        ghg.box(-0.03, 0.03, 1.22, 1.83, 0.27, 0.30); ghg.box(-0.03, 0.03, 1.22, 1.83, 0.70, 0.73)
        gbar.tube([(sg * 0.07, 0.75, LW - 0.10), (sg * 0.07, 1.05, LW - 0.10), (sg * 0.07, 1.35, LW - 0.10)], 0.016, 10)
        for yb in (0.75, 1.35): gbar.beam((sg * 0.025, yb, LW - 0.10), (sg * 0.07, yb, LW - 0.10), 0.02, 0.02, (0, 1, 0))
        for yh in (0.25, 1.05, 1.85): ghg.cylinder((sg * 0.03, yh, 0.005), (0, 1, 0), 0.018, 0.12, 12)
        add_node(lf, f'{J}/SERVICE-DOOR:jwWall', 'jwWall', gl_); add_node(lf, f'{J}/SERVICE-DOOR:glassOuter', 'glassOuter', gwin)
        add_node(lf, f'{J}/SERVICE-DOOR:jwChrome', 'jwChrome', gbar); add_node(lf, f'{J}/SERVICE-DOOR:jwAluB', 'jwAluB', ghg)
        stat5['doors'] += 1
print('PART5', stat5)
