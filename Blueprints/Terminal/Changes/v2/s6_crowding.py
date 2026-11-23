# Sheet 6: crowding per area against the IATA level of service, before / after, and whether an extension is needed.
from v2_common import *
fig = sheet('SHEET 6', 'IS IT CROWDED ANYWHERE?  Each area against the IATA standard, before and after',
            'Busiest hour, from the 16 stands: %d departing + %d arriving; %d an hour non-Schengen each way; worst arrival wave: an A380 + a B777 in 30 minutes = %d people.' % (PH, PH, DEP_WB, SURGE))
g = lambda k: area(G[k]); f = lambda k: area(F[k])
q10 = lambda perh, m2: perh * 10 / 60 * m2                      # a 10-minute queue
bags = PH * 0.65
claimw = DEP_NB * 0.25 * 1.7 + 5 * 160
claime = SURGE * 0.6 * 1.7 + 2 * 300
A380g, B777g = A380 * LF * 0.8 * 1.5, B777 * LF * 0.8 * 1.5
gate_after = 952 - AREA_CORE
wfree = 2674 - sum(area(WEST[k]) for k in ('df', 'lf', 'wc', 'os', 'trol1', 'trol2', 'trol3'))
efree = g('claim') - sum(area(G[k]) for k in ('df_east', 'wc_claim', 'lf_east', 'os_east')) - 3 * 2.6 * 13
pcq = g('pc_hall') - area(G['transfer_up']) - area(G['second_line']) - 2 * 2.6 * 13 - 48 * 14 * 0.5
nsdwell = DEP_WB * 1.5                                          # 90 minutes airside behind the wall
R = [('Check-in + bag drop', 'bags / h', 20 * 24, 28 * 24 + 10 * 60, bags, '28 desks x 24 + 10 bag drops x 60'),
     ('Check-in queue space', 'm²', 756, 756 + 450, PH * 0.55 * 12 / 60 * 2.0, 'Grand Hall front zone + East Hall'),
     ('Security lanes', 'people / h', 6 * 150, 7 * 280, PH, '7 automatic lanes (280 each, sheet 2g)'),
     ('Security queue', 'm²', 340, 340, q10(PH, 1.2), 'same space, 10-minute queue (shorter with 7 lanes)'),
     ('Gates 1-4, 9, 11 (A320)', 'm²', 952, 952, A320 * LF * 0.8 * 1.5, 'gate + view bay'),
     ('Gate 13 (A380)', 'm²', 952, gate_after, A380g, 'less the vestibule and the way down'),
     ('Gates 14-16 (B777)', 'm²', 952, gate_after, B777g, 'less the vestibule and the way down'),
     ('Passport control out', 'people / h', 0, 4 * 60 + 8 * 120, DEP_WB, '4 booths x 60 + 8 e-gates x 120 (with EES)'),
     ('  its queue hall', 'm²', 0, f('queue'), q10(DEP_WB * 2, 1.2), 'double the hour\'s average, 10 minutes'),
     ('Non-Schengen concourse', 'm²', 0, f('slab') - f('queue') - f('line') - f('strip') - f('exit_lane') - f('transfer'), nsdwell * 4.0, '90 minutes each, 4 m² per person'),
     ('Passport control in', 'people / h', 0, 6 * 50 + 8 * 120, SURGE * 60 / 40.0, '6 booths x 50 (EES) + 8 e-gates x 120; wave cleared in 40 min'),
     ('  its queue hall', 'm²', 0, pcq, SURGE * 0.75 * 1.2, '3 of 4 of the wave waiting at once'),
     ('Transfer security', 'people / h', 0, 2 * 150, PH * 0.10, '10 % transfer, 2 lanes x 150'),
     ('Claim hall west', 'm²', 2674, wfree, claimw, 'less duty free, toilets, desk, oversize belt'),
     ('Claim hall east', 'm²', 0, efree, claime, 'new, 2 wide-body belts'),
     ('Arrivals hall west', 'm²', 960, 960 - sum(area(WEST[k]) for k in ('cafe', 'info', 'kiosk', 'atm')), (DEP_NB * 0.5 * 25 / 60 + DEP_NB * 5 / 60) * 2.3, 'less coffee bar, desk, kiosk, exchange (kept small for this)'),
     ('East Hall: meeters', 'm²', 0, 2880 - 450 - 136 - sum(area(EH[k]) for k in ('cafe', 'shops', 'sim', 'atm', 'rental', 'info', 'firstaid', 'rail')), SURGE * 0.97 * 2.3, 'less the check-in zone and the fittings'),
     ('Arrivals corridor', 'people / h', 0, 4.5 * 60 * 55, SURGE * 2, '4.5 m wide, 55 people per metre and minute'),
     ('Kerb: taxis', 'cabs / h', 0, 21 * 12, PH * 0.12 / 1.5, 'arriving: 12 % by taxi, 1.5 per cab, 5 minutes each')]
y0, dy = 0.872, 0.0375
for x, t in ((0.016, 'AREA'), (0.178, 'NEEDED'), (0.245, 'TODAY'), (0.305, 'AFTER'), (0.36, 'TODAY, % of the need'), (0.555, 'AFTER, % of the need'), (0.75, 'HOW THE AFTER IS COUNTED')):
    fig.text(x, y0 + 0.03, t, color=NEW, fontsize=15.5, weight='bold')
fmt = lambda v: '{:,}'.format(int(round(v))).replace(',', ' ')
for i, (n, u, b, a, need, how) in enumerate(R):
    y = y0 - i * dy
    fig.text(0.016, y, n, color=FG, fontsize=16, va='center', weight='bold' if not n.startswith(' ') else 'normal')
    fig.text(0.178, y, fmt(need) + ' ' + u, color=DIM, fontsize=14, va='center'); fig.text(0.278, y, fmt(b) if b else 'none', color=DIM, fontsize=15, va='center', ha='right'); fig.text(0.338, y, fmt(a), color=FG, fontsize=15, va='center', ha='right')
    for x, v in ((0.36, b), (0.555, a)):
        p = v / need * 100; c = OK if p >= 105 else ('#e8e05a' if p >= 97 else (WARN if p > 0 else NS))
        fig.patches.append(Rectangle((x, y - 0.010), 0.13, 0.02, transform=fig.transFigure, fc='#121a22', ec='#33414f'))
        fig.patches.append(Rectangle((x, y - 0.010), 0.13 * min(p, 200) / 200, 0.02, transform=fig.transFigure, fc=c, ec='none'))
        fig.patches.append(Rectangle((x + 0.0648, y - 0.014), 0.0006, 0.028, transform=fig.transFigure, fc='#ffffff', ec='none'))
        fig.text(x + 0.136, y, 'missing' if p == 0 else '%d %%' % round(p), color=c, fontsize=15, va='center', weight='bold')
    fig.text(0.75, y, how, color=DIM, fontsize=13.5, va='center')
    print('%-28s need %6d today %4d%% after %4d%%' % (n, need, b / need * 100, a / need * 100))
fig.text(0.016, 0.142, 'EXTENSION EAST PAST x 551: NOT NEEDED. No area is short of space after the changes, so no new gates are needed either.', color=OK, fontsize=20, weight='bold')
L = [('#e8e05a', 'SECURITY with the 7th lane: 116 % at 280 people per lane and hour, 100 % if a lane makes only 240 (the room is short for automatic lanes). Its queue space stays just enough (101 %): with 7 lanes the queue moves faster.'),
     (DIM, 'Used: IATA optimum m² per person (ADRM): queues 1.2 (security, passport), 2.0 (check-in with trolleys), gates 1.5 for 80 % of the load, claim 1.7, arrivals hall 2.3. The ADRM tables are not free to read: these are the values'),
     (DIM, 'of the accepted sheet TC-1, not checked against the 12th edition. EES rates (booth 50-60, e-gate 120 an hour) are my own cautious guesses: Geneva and Zurich report long first-registration queues in 2026.')]
for i, (c, t) in enumerate(L): fig.text(0.016, 0.105 - i * 0.03, t, color=c, fontsize=14.5)
save(fig, 'TCv2-6-Crowding.png')
