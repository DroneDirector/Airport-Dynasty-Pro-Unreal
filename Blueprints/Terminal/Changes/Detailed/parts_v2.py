# the new parts placed from the redrawn sheets' own numbers (Blueprints/Terminal/Changes/v2/v2_common.py), part 1: forecourt + ground floor
V2 = {}
_s = open(os.path.join(HERE, '..', 'v2', 'v2_common.py'), encoding='utf-8').read()
exec(_s[_s.index('GATE_X0 ='):_s.index('def area(')], V2)
Gp, Fp, W2, HL, EHv, GX = V2['G'], V2['F'], V2['WEST'], V2['HALL'], V2['EH'], V2['GATE_X0']
# ---- the forecourt
box(241, -21.3, 551, 0, -0.06, 0.14, 'TERR', O); gold(241, -21.3, 551, 0, 0.16, 0.16, 'out', 0.07, True); box(241, -24.9, 551, -24.75, 0, 0.014, 'PAINT', O)
def rank(x0, x1, n, sx0, sx1):
    box(x0, -24.9, x1, -24.78, 0, 0.014, 'PYEL', O); box(x0, -24.9, x0 + 0.12, -21.6, 0, 0.014, 'PYEL', O); box(x1 - 0.12, -24.9, x1, -21.6, 0, 0.014, 'PYEL', O)
    for k in range(n):
        x = x0 + 1.0 + k * (x1 - x0) / n; car(x, -24.2, 'TAXI', True, O); box(x + 1.9, -23.5, x + 2.5, -23.1, 1.46, 1.66, 'SIGNYT', O)
    box(sx0, -20.6, sx1, -20.5, 0.14, 2.6, 'GLASS', O); box(sx0 - 0.4, -20.9, sx1 + 0.4, -18.0, 2.6, 2.78, 'ZINC', O)
    for x in (sx0, (sx0 + sx1) / 2, sx1): box(x - 0.06, -20.6, x + 0.06, -20.48, 0.14, 2.6, 'MUL', O); box(x - 0.06, -18.3, x + 0.06, -18.18, 0.14, 2.6, 'MUL', O)
    for x in (sx0 + 0.6, sx0 + 4.4, sx0 + 8.2): seats(x, -20.0, 0.14, 5, O)
    box(sx1 + 1.6, -20.8, sx1 + 1.9, -20.5, 0.14, 5.0, 'MUL', O)
    for fc in 'sn': sign('TAXI', sx1 + 1.75, -20.65, 4.6, 3.2, 1.0, fc, O, 'SIGNYT')
    sign('TAXI', (sx0 + sx1) / 2, -18.0, 2.35, 5, 0.45, 'n', O, 'SIGNYT'); sign('TAXI', (sx0 + sx1) / 2, -20.9, 2.35, 5, 0.45, 's', O, 'SIGNYT')
    gold(sx0 - 0.4, -20.9, sx1 + 0.4, -18.0, 0.16, 2.9, 'out', 0.06); gold(x0, -24.9, x1, -21.6, 0.05, 0.05, 'out', 0.07, True); people(sx0, sx1, -19.6, -18.2, 8, 0.14, O); people(x0 + 2, x1, -21, -20.3, 6, 0.14, O)
rank(-75, -5, 10, -46, -34); rank(158, 238, 11, 192, 204)
def paym(x, z, h=0.0):
    box(x, z, x + 0.8, z + 0.6, h, h + 1.75, 'NAVY', O); box(x + 0.1, z + 0.6, x + 0.7, z + 0.64, h + 1.0, h + 1.5, 'SCREEN', O); box(x + 0.1, z - 0.04, x + 0.7, z, h + 1.0, h + 1.5, 'SCREEN', O)
for xz in (48.6, 74.8, 101.0):
    paym(xz + 2.9, -57.6); paym(xz + 4.4, -57.6); box(xz + 2.3, -58.5, xz + 5.7, -56, 2.4, 2.5, 'ZINC', O)
    for (a, b) in ((xz + 2.5, -58.4), (xz + 5.4, -58.4)): box(a, b, a + 0.1, b + 0.1, 0, 2.4, 'MUL', O)
    sign('P  PAY HERE', xz + 4, -55.95, 2.75, 3.0, 0.45, 'n', O); sign('P  PAY HERE', xz + 4, -58.55, 2.75, 3.0, 0.45, 's', O); gold(xz + 2.3, -58.5, xz + 5.7, -56, 0.05, 3.0, 'out', 0.06)
paym(-32.3, -2.4); paym(-30.2, -2.4); gold(-32.5, -2.8, -29, -0.8, 0.1, 1.9, 'out', 0.05); paym(217.3, 1.5, 0.06); paym(219.6, 1.5, 0.06); gold(217, 1, 221, 3, 0.1, 1.9, 'ground', 0.05)
for k in range(5):
    x = 472 + 16 * k
    for (a, b, c, d) in ((x, -24.9, x + 13, -24.7), (x, -21.8, x + 13, -21.6), (x, -24.9, x + 0.2, -21.6), (x + 12.8, -24.9, x + 13, -21.6)): box(a, b, c, d, 0, 0.016, 'PYEL', O)
    if k != 3: coach(x + 0.5, -24.5, O)
gold(472, -24.9, 549, -21.6, 0.05, 0.05, 'out', 0.08, True); people(165, 550, -16, -5, 70, 0.14, O); people(-75, 150, -16, -5, 50, 0.14, LAND)
# ---- the Grand Hall
for k in ('k1', 'k2'):
    a = HL[k]
    for i in range(6): kiosk(a[0] + 0.2 + i * 1.22, a[1] + 0.25)
    gold(*a, 0.05, 1.8, w=0.05)
a = HL['drop']
for i in range(6):
    x = a[0] + i * 2.0; box(x, 15.8, x + 1.5, 18.6, 0, 0.5, 'STEEL'); box(x + 0.3, 15.8, x + 1.2, 18.6, 0.5, 0.56, 'RUBBER'); box(x, 17.2, x + 0.3, 18.6, 0.5, 1.7, 'DARK'); box(x + 0.02, 17.18, x + 0.28, 17.2, 1.1, 1.6, 'SCREEN')
gold(*a, 0.05, 1.9, w=0.05); sign('BAG DROP', 107, 18.8, 3.0, 5, 0.6)
a = HL['serv']; box(a[0], a[1], a[0] + 1, a[3], 0, 1.05, 'DESK'); box(a[2] - 0.2, a[1], a[2], a[3], 0, 3.0, 'DWOOD'); sign('SERVICE + TICKETS', a[2] - 0.3, 25.75, 2.4, 4.6, 0.45, 'w'); gold(*a, 0.05, 3.0, w=0.05)
for k in ('eg1', 'eg2'):
    a = HL[k]; [egate(a[0] + 0.2 + i * 1.15, a[1] - 0.3, L=1.6) for i in range(5)]; gold(a[0], a[1] - 0.4, a[2], a[3], 0.05, 1.6, w=0.05)
for k in ('tr1', 'tr2'): a = HL[k]; trolleys(a[0] + 0.3, a[1] + 0.3, 12); gold(*a, 0.05, 1.1, w=0.05)
people(50, 108, 4.5, 14, 40)
# ---- the west claim wing
a = W2['df']; shop(a[0], a[1], a[2], a[3], 0.0, 4.2, 'ARRIVALS DUTY FREE', None, 's'); gold(*a, 0.05, 4.3, w=0.06)
a = W2['lf']; box(a[2] - 1, a[1] + 0.5, a[2], a[3] - 0.5, 0, 1.05, 'DESK'); box(a[0], a[1], a[0] + 0.2, a[3], 0, 3, 'DWOOD'); sign('LOST + FOUND', a[2] - 0.5, 20.5, 3.2, 5.5, 0.55, 'e'); gold(*a, 0.05, 3.5, w=0.05)
a = W2['wc']; box(*a, 0, 3.4, 'PLAST'); sign('WC', a[2] + 0.1, 29, 2.6, 2.5, 0.7, 'e'); gold(*a, 0.05, 3.5, w=0.05)
a = W2['os']; box(*a, 0, 0.45, 'STEEL'); box(a[0] + 0.6, a[1] + 0.5, a[2] - 0.6, a[3], 0.45, 0.5, 'RUBBER'); sign('OVERSIZE', -3.4, a[1] - 0.1, 2.6, 3.5, 0.5); gold(*a, 0.05, 2.9, w=0.05)
for k in ('trol1', 'trol2', 'trol3'): a = W2[k]; trolleys(a[0] + 0.3, a[1] + 0.4, 20); gold(*a, 0.05, 1.1, w=0.05)
for k, t in (('cafe', 'CAFE'), ('info', 'INFORMATION'), ('kiosk', 'KIOSK + SIM'), ('atm', 'ATM + EXCHANGE')): a = W2[k]; box(a[0], a[1], a[2], a[1] + 0.8, 0, 1.1, 'DWOOD'); sign(t, (a[0] + a[2]) / 2, a[1] + 1.0, 2.8, min(a[2] - a[0], 7), 0.5, 'n'); gold(*a, 0.05, 3.2, w=0.05)
a = W2['rail']; box(a[0], a[1], a[2], a[3], 0.95, 1.02, 'STEEL'); gold(*a, 0.05, 1.1, w=0.05)
# ---- the East Hall
box(156, 0, 228, 40, 0, 0.06, 'MARBLE'); gold(156, 0, 228, 40, 0.1, 0.1, base=True)
box(156, 39.8, 228, 40, 0, 12, 'PLAST'); box(156, 39.7, 228, 39.8, 0, 3.2, 'DWOOD')
box(156, 40, 228, 52, 0, 0.05, 'MARBLE'); box(156, 40, 228, 52, 5.4, 5.6, 'PLAST', F)
for k in ('wc_hall', 'family', 'trolley', 'bagserv'): a = Gp[k]; box(a[0] - 0.08, 40, a[0] + 0.08, 52, 0, 5.4, 'PLAST'); gold(*a, 0.1, 5.35)
for t, x, w in (('WC', 169, 4), ('FAMILY + QUIET', 184, 8), ('TROLLEYS', 200, 6), ('BAG SERVICE', 219, 7)): sign(t, x, 39.6, 3.7, w, 0.6)
a = EHv['checkin']
for i in range(8):
    x = a[0] + 0.2 + i * 2.5; box(x, 24, x + 1.5, 25, 0, 1.1, 'DESK'); box(x + 1.5, 24, x + 2.3, 27.5, 0, 0.4, 'RUBBER'); box(x + 0.4, 24.7, x + 1.1, 24.8, 1.1, 1.5, 'DARK'); box(x - 0.1, 27.6, x + 2.2, 27.7, 1.9, 2.8, 'SCREEN')
for i in range(4):
    x = 181.2 + i * 2.15; box(x, 24, x + 1.5, 27, 0, 0.5, 'STEEL'); box(x + 0.25, 24, x + 1.25, 27, 0.5, 0.55, 'RUBBER'); box(x, 25.5, x + 0.25, 27, 0.5, 1.7, 'DARK'); box(x + 0.02, 25.48, x + 0.23, 25.5, 1.1, 1.6, 'SCREEN')
box(160, 27.7, 195, 28, 0, 3.0, 'DWOOD'); sign('CHECK-IN  ·  long-haul 21-28', 170, 27.5, 3.6, 15, 0.8); sign('BAG DROP', 185.4, 27.5, 3.6, 6, 0.8); sign('OVERSIZE', 193, 27.5, 3.6, 4.4, 0.6)
box(191.2, 24, 194.8, 27.5, 0, 0.5, 'STEEL'); box(191.5, 24, 194.5, 27.5, 0.5, 0.55, 'RUBBER'); gold(160, 24, 195, 28, 0.1, 4.1)
for x in range(161, 190, 3):
    for z in (17, 19, 21): cyl(P(x, z, 0), P(x, z, 0.95), 0.04, 'STEEL', None, 8); box(x, z - 0.02, min(x + 3, 189), z + 0.02, 0.85, 0.92, 'NAVY')
a = EHv['cafe']; box(*a, 0.06, 0.08, 'DWOOD'); box(a[0] + 0.5, 8.4, 167, 9.4, 0, 1.1, 'DWOOD'); box(a[0] + 0.3, 10.7, a[2], 10.9, 0, 2.8, 'DWOOD'); sign('CAFE', 164, 10.6, 3.3, 4, 0.7)
for (x, z) in ((160, 3.4), (163, 5.4), (166, 3.4), (169, 5.4), (169.5, 8.4)): table(x, z, 0.08, None)
gold(*a, 0.1, 3.7)
a = EHv['shops']; shop(a[0], a[1], a[2], a[3], 0.06, 4.0, 'SHOPS', None, 'n'); gold(*a, 0.1, 4.1)
a = EHv['sim']; shop(a[0], a[1], a[2], a[3], 0.06, 3.6, 'KIOSK + SIM', None, 'n', tsize=0.5); gold(*a, 0.1, 3.7)
a = EHv['atm']; [box(a[0] + 0.6 + i * 1.6, 4.8, a[0] + 1.8 + i * 1.6, 5.6, 0, 2.0, 'NAVY') for i in range(4)]; [box(a[0] + 0.8 + i * 1.6, 5.6, a[0] + 1.6 + i * 1.6, 5.64, 1.0, 1.6, 'SCREEN') for i in range(4)]; sign('ATM + EXCHANGE', 211.5, 5.2, 2.6, 6, 0.55, 'n'); gold(*a, 0.1, 3.0)
for i in range(5):
    x = 200 + i * 5.4; box(x + 0.3, 34, x + 5.1, 34.9, 0, 1.1, 'DESK'); box(x + 0.2, 38.3, x + 5.2, 38.5, 0, 3.4, 'P%d' % (i % 7)); sign('CAR RENTAL %d' % (i + 1), x + 2.7, 38.2, 2.9, 4.6, 0.55)
gold(*EHv['rental'], 0.1, 3.5)
a = EHv['info']; box(a[0], a[1], a[2], a[1] + 1, 0, 1.1, 'DESK'); box(a[0], a[3] - 0.2, a[2], a[3], 0, 3, 'DWOOD'); sign('i  INFORMATION + PRM', 163, a[3] - 0.3, 3.3, 9, 0.65); gold(*a, 0.1, 3.7)
a = EHv['firstaid']; box(*a, 0, 3.2, 'PLAST'); sign('+ FIRST AID', 175, a[1] - 0.1, 2.5, 5, 0.6, 's', None, 'SIGNTX', 'GRN'); gold(*a, 0.1, 3.3)
box(219, 12, 219.1, 31, 0.95, 1.03, 'STEEL'); [cyl(P(219.05, z, 0), P(219.05, z, 1.0), 0.03, 'STEEL', None, 8) for z in range(12, 32, 2)]; gold(*EHv['rail'], 0.1, 1.1); sign('MEETING POINT', 212, 20.5, 4.2, 8, 0.8, 'e'); cyl(P(212, 20.5, 0), P(212, 20.5, 3.8), 0.1, 'STEEL')
fids(192, 16, 3.6, 8, 8); cyl(P(189, 16, 0), P(189, 16, 3.6), 0.08, 'STEEL'); cyl(P(195, 16, 0), P(195, 16, 3.6), 0.08, 'STEEL')
people(157, 227, 3, 36, 70); people(214, 218.8, 12, 31, 16)
for (x, z) in ((158, 13), (174, 13), (190, 12), (206, 13), (224, 5)): plant(x, z, 0.06, None, 1.2)
for x in (200, 206): seats(x, 22, 0.06, 6); seats(x, 25, 0.06, 6)
# ---- the promenade, shop fronts and the 11 glass stairs
box(228, 0, 470, 6, 0, 0.06, 'MARBLE'); gold(228, 0, 470, 6, 0.1, 0.1, base=True); box(228, 5.9, 470, 6.1, 5.0, 6.2, 'PLAST')
NM = ('NEWS', 'COFFEE', 'GIFTS', 'TRAVEL', 'CHOCOLATE', 'FLOWERS', 'BAKERY', 'TOYS', 'PHARMACY', 'FASHION', 'BOOKS', 'WATCHES', 'SPORT', 'JUICE BAR', 'OPTICS', 'SOUVENIRS', 'PHONES')
for i, x in enumerate(range(230, 464, 14)):
    box(x + 12, 5.9, x + 14, 6.1, 0, 5, 'DWOOD'); box(x, 5.95, x + 12, 6.05, 0, 4.0, 'SHOPG'); box(x, 5.9, x + 12, 6.1, 4.0, 5.0, 'SIGNBG'); text(NM[i % 17], x + 6, 5.86, 4.5, 0.62, 'SIGNTX'); rnd = random.Random(i)
    if x > 236: box(x, 7.4, x + 12, 7.5, 0, 4, 'PLAST')
    box(x + 2, 6.6, x + 10, 6.8, 3.8, 3.85, 'SHOPLT')
for x in range(240, 466, 16): seats(x, 1.6, 0.06, 4); plant(x - 1.5, 1.6, 0.06, None, 1.0)
people(229, 469, 1.0, 5.2, 60)
for x in V2['STAIRS']:
    for k in range(20): box(x - 1.1, 8 + k * 0.55, x + 1.1, 8.55 + k * 0.55, k * 0.3, (k + 1) * 0.3, 'STEEL')
    for xs in (x - 1.3, x + 1.3): box(xs - 0.02, 7, xs + 0.02, 20, 0, 7.1, 'SHOPG')
    gold(x - 1.3, 7, x + 1.3, 20, 0.1, 6.0, w=0.05)
# ---- customs
box(*Gp['customs'], 0, 0.06, 'MARBLE'); box(*Gp['customs_office'], 0, 0.06, 'CARPET')
for xw in (228, 236):
    for (z0, z1) in ((6, 24), (32, 33.5), (39, 52)): box(xw - 0.1, z0, xw + 0.1, z1, 0, 5.4, 'PLAST')
for (z0, z1, m, t) in ((24, 32, 'GRN', 'NOTHING TO DECLARE'), (33.5, 39, 'RED', 'GOODS TO DECLARE')):
    box(228.3, z0 + 0.1, 235.7, z1 - 0.1, 0.07, 0.08, m); box(236.1, z0, 236.2, z1, 3.2, 4.2, m); text(t, 236.25, (z0 + z1) / 2, 3.7, 0.4, 'SIGNTX', None, 'e'); box(227.8, z0, 227.9, z1, 3.2, 4.2, 'SIGNBG'); text('EXIT', 227.75, (z0 + z1) / 2, 3.7, 0.5, 'SIGNTX', None, 'w')
box(230.5, 36.5, 233.5, 38, 0, 0.95, 'STEEL'); gold(228, 6, 236, 52, 0.1, 5.35); people(229, 235, 25, 31, 5)
# ---- the claim hall east
box(*Gp['claim'], 0, 0.06, 'MARBLE'); gold(*Gp['claim'], 0.1, 5.35)
for i, cx in enumerate(V2['BELTS_E']): belt(cx, 22, 50, 4); sign('BELT %d' % (6 + i), cx, 21.5, 4.3, 4, 0.7)
people(250, 256, 23, 49, 20); people(266, 285, 22, 49, 34); people(295, 299, 22, 46, 16)
a = Gp['df_east']; shop(a[0], a[1], a[2], a[3], 0.06, 4.2, 'ARRIVALS DUTY FREE', None, 'n'); gold(*a, 0.1, 4.3)
a = Gp['wc_claim']; shop(a[0], a[1], a[2], a[3], 0.06, 4.2, 'WC', None, 'n', kind='none'); gold(*a, 0.1, 4.3)
a = Gp['lf_east']; box(a[0], a[3] - 1, a[2], a[3], 0, 1.05, 'DESK'); box(a[0], a[1], a[2], a[1] + 0.2, 0, 3.2, 'DWOOD'); sign('LOST + FOUND', 254.7, a[3] - 0.5, 3.2, 7, 0.6, 'n'); gold(*a, 0.1, 3.6)
a = Gp['os_east']; box(*a, 0, 0.45, 'STEEL'); box(a[0] + 1, a[1] + 1, a[2], a[3] - 1, 0.45, 0.5, 'RUBBER'); sign('OVERSIZE', a[0], 46.5, 2.8, 4, 0.6, 'w'); gold(*a, 0.1, 3.2)
trolleys(240, 20, 18); trolleys(240, 12, 18); sign('CUSTOMS  ·  EXIT', 239, 28, 4.4, 9, 0.8, 'e')
# ---- passport control in
box(312, 6, 372, 52, 0, 0.06, 'MARBLE'); gold(*Gp['pc_line'], 0.1, 5.35); gold(*Gp['pc_hall'], 0.1, 5.35); gold(*Gp['pc_office'], 0.1, 5.35)
box(312, 6, 312.2, 20, 0, 5.4, 'PLAST'); box(323.8, 6, 324, 20, 0, 5.4, 'PLAST'); box(312, 19.8, 324, 20, 0, 5.4, 'PLAST'); text('BORDER POLICE', 324.1, 13, 3.4, 0.6, 'SIGNYT', None, 'e')
for k in range(6): booth(314.5, 20.8 + k * 2.9, 321.5, 22.9 + k * 2.9)
for k in range(8): egate(315.5, 39.1 + k * 1.6, along='x', L=4.0)
sign('PASSPORT CONTROL', 324.1, 34, 4.5, 16, 1.0, 'e')
for k in range(16): kiosk(341.4 + (k % 8) * 1.9, 31 + (k // 8) * 3.5)
gold(341, 30.6, 356.5, 35.5, 0.1, 1.9, w=0.05)
for z in range(21, 39, 3):
    for x in range(327, 336, 3): cyl(P(x, z, 0), P(x, z, 0.95), 0.04, 'STEEL', None, 8); box(x, z - 0.02, x + 3, z + 0.02, 0.85, 0.92, 'NAVY')
for k in ('transfer_up', 'second_line'): a = Gp[k]; [box(q[0], q[1], q[2], q[3], 0, 4.6, 'SHOPG') for q in ((a[0], a[1], a[2], a[1] + 0.06), (a[0], a[1], a[0] + 0.06, a[3]), (a[2] - 0.06, a[1], a[2], a[3]))]; gold(*a, 0.1, 4.6)
quad([P(328, 41, 0.1), P(330, 41, 0.1), P(330, 50, 6), P(328, 50, 6)], 'STEEL'); quad([P(333, 41, 0.1), P(335, 41, 0.1), P(335, 50, 6), P(333, 50, 6)], 'STEEL')
people(326, 341, 21, 39, 55); people(342, 356, 29, 38, 16)
# ---- staff and operations, the coach + bus station
for k, t in (('crew', 'CREW CENTRE'), ('canteen', 'STAFF CANTEEN'), ('ground_off', 'HANDLING + AIRLINES'), ('lockers', 'LOCKERS'), ('staffsec', 'STAFF SECURITY'), ('police', 'POLICE + BORDER GUARD')):
    a, b, c, d = Gp[k]; box(a, b, c, d, 0, 0.06, 'CARPET' if k in ('crew', 'ground_off', 'police') else 'MARBLE'); box(a - 0.08, b, a + 0.08, d, 0, 5.4, 'PLAST'); box(a, b - 0.08, a + 12, b + 0.08, 0, 5.4, 'PLAST'); box(a + 15, b - 0.08, c, b + 0.08, 0, 5.4, 'PLAST'); text(t, a + 6, b - 0.12, 3.2, 0.7, 'SIGNYT'); gold(a, b, c, d, 0.1, 5.35)
for x in range(376, 400, 5):
    for z in (11, 16, 21): box(x, z, x + 2.4, z + 1.2, 0.7, 0.75, 'OAK'); box(x + 0.6, z + 1.4, x + 1.4, z + 2.1, 0.4, 0.5, 'SEAT')
for x in range(408, 434, 5):
    for z in (11, 16, 21): table(x, z, 0.06, None)
for x in range(440, 468, 6):
    for z in (10, 16, 22, 32, 40): box(x, z, x + 2.8, z + 1.4, 0.7, 0.75, 'WHITE'); box(x + 1, z + 0.5, x + 1.7, z + 0.6, 0.75, 1.15, 'DARK')
for x in range(375, 403, 3): box(x, 30, x + 0.6, 49, 0, 1.9, 'NAVY')
for x in (410, 424): box(x, 30, x + 2, 46, 0, 0.8, 'STEEL'); box(x - 0.2, 36, x + 2.2, 39, 0, 1.9, 'DARK')
box(*Gp['coach'], 0, 0.06, 'MARBLE'); gold(*Gp['coach'], 0.1, 0.1, base=True); box(469.9, 6, 470.1, 52, 0, 5.4, 'PLAST')
for x in range(476, 546, 8):
    for z in (22, 25, 28, 31): seats(x, z, 0.06, 7)
fids(508, 36, 2.6, 14, 6, 's', None, 'COACH + BUS DEPARTURES'); box(486, 40, 506, 41, 0, 1.1, 'DESK'); box(486, 43.5, 506, 43.7, 0, 3, 'DWOOD'); sign('TICKETS', 496, 43.4, 3.4, 6, 0.8); gold(486, 40, 506, 43.7, 0.1, 3.9)
shop(512, 40, 532, 50, 0.06, 4.2, 'CAFE', None, 's', kind='table'); gold(512, 40, 532, 50, 0.1, 4.3); shop(536, 40, 549, 50, 0.06, 4.2, 'WC', None, 's', kind='none'); gold(536, 40, 549, 50, 0.1, 4.3)
people(474, 548, 4, 35, 60); [plant(x, 3, 0.06, None, 1.2) for x in (478, 498, 518, 540)]
# ---- the closed arrivals corridor under the gate walkway, the lobbies under the gates, the crossing to passport control
for (a, b, c, d) in (Gp['corridor'], Gp['cross']): box(a, b, c, d, 0.02, 0.06, 'CARPET'); gold(a, b, c, d, 0.1, 3.2, w=0.06)
a = Gp['corridor']; box(a[0], a[1], a[2], a[1] + 0.12, 0, 3.2, 'PLAST')
for g in GX: x = GX[g]; gold(x + 28.2, 68.1, x + 48, 81.9, 0.1, 5.3, w=0.06)
lamps(237, 469, (24, 36, 47), 5.32, F, 5); lamps(160, 226, (44, 49), 5.3, F, 5); lamps(474, 549, (26, 38), 5.32, F, 5)
