# Sheet 1: the design hour from the stands and each area's capacity today vs what it needs (IATA ADRM level of service).
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tc_common import *
fig = sheet('SHEET 1 · CAPACITY', 'The busiest hour estimated from the stands, then every passenger area measured against the IATA level-of-service space '
            'standard (Airport Development Reference Manual, "optimum" band). Green = enough today; orange = short; pink = missing today.')
# ---- the design hour
ax = fig.add_axes([0.012, 0.60, 0.30, 0.31]); ax.axis('off')
rows = [('Stands', '12 narrow-body (A320, 180 seats) + 4 wide-body (1 A380 520, 3 B777 368)'),
        ('Turns in the busiest hour', 'narrow-body 0.67 per stand (every ~90 min), wide-body 0.33 (every ~3 h)'),
        ('Load factor', '85 percent'),
        ('Departures, busiest hour', '%d narrow-body + %d wide-body = %d passengers' % (DEP_NB, DEP_WB, PH_DEP)),
        ('Arrivals, busiest hour', 'the same, %d passengers' % PH_DEP),
        ('Non-Schengen (gates 13-16)', '%d departing / h; arriving surge: A380 + B777 in 30 min = %d' % (DEP_WB, SURGE_NS)),
        ('Checked bags', '65 %% of departing passengers drop a bag (%d / h)' % (PH_DEP * 0.65))]
ax.text(0, 1.0, 'THE DESIGN HOUR', color=NEW, fontsize=17, weight='bold', va='top')
for i, (a, b) in enumerate(rows):
    y = 0.86 - i * 0.12
    ax.text(0, y, a, color=FG, fontsize=12.5, weight='bold', va='top'); ax.text(0.0, y - 0.05, b, color=DIM, fontsize=12, va='top')
# ---- the IATA standards used
ax = fig.add_axes([0.012, 0.10, 0.30, 0.46]); ax.axis('off')
ax.text(0, 1.0, 'IATA LEVEL OF SERVICE (optimum, m² per person)', color=NEW, fontsize=17, weight='bold', va='top')
std = [('Check-in queue (with trolleys)', '1.8 - 2.3', '2.0'), ('Kiosk / bag-drop queue', '1.3 - 1.8', '1.5'),
       ('Security queue', '1.0 - 1.2', '1.2'), ('Passport control queue', '1.0 - 1.2', '1.2'),
       ('Gate lounge (60 % seated)', '1.5 - 1.7', '1.5'), ('Baggage claim hall', '1.5 - 1.7', '1.7'),
       ('Arrivals hall (meeters + greeters)', '2.0 - 2.3', '2.3'),
       ('Max wait (economy)', 'check-in 10-20 min, security 5-10, passport 5-10, first bag <= 15 min', '')]
ax.text(0.0, 0.90, 'area', color=DIM, fontsize=12); ax.text(0.62, 0.90, 'IATA band', color=DIM, fontsize=12); ax.text(0.86, 0.90, 'used', color=DIM, fontsize=12)
for i, (a, b, c) in enumerate(std):
    y = 0.83 - i * 0.095
    ax.text(0, y, a, color=FG, fontsize=12.5)
    ax.text(0.62 if c else 0.0, y if c else y - 0.045, b, color=DIM if c else '#b8c4d0', fontsize=12.5 if c else 11.5)
    ax.text(0.86, y, c, color=NEW, fontsize=12.5, weight='bold')
# ---- the areas: capacity today vs need
# (name, today's figure, needed figure, unit, today text, after text)
A = []
cq_need = PH_DEP * 0.55 * 12 / 60 * 2.0
A.append(('Check-in + bag-drop throughput', 20 * 24, PH_DEP * 0.65, 'bags/h', '20 desks x 24/h = 480 /h',
          '20 desks + 6 bag-drop (Grand Hall) + 8 desks + 4 bag-drop (East Hall) = 1272 /h'))
A.append(('Check-in queue space', 756, cq_need, 'm²', 'front zone 72 x 15 m, 30 % walkways off = 756 m²', 'unchanged + East Hall queue 450 m² = 1206 m²'))
A.append(('Security lanes', 6 * 150, PH_DEP, 'pax/h', '6 ordinary lanes x 150 /h = 900 /h', '6 automated (ATRS) lanes x 280 /h = 1680 /h, same room'))
A.append(('Security queue space', 340, PH_DEP * 10 / 60 * 1.2, 'm²', 'x 50-90, z 19.5-30 less the walk-throughs = 340 m²', 'unchanged (ATRS halves the wait: queue fits)'))
A.append(('Gate lounge A320 (worst)', 476, 153 * 0.8 * 1.5, 'm²', 'gate room 34 x 14 = 476 m²', 'unchanged'))
A.append(('Gate lounge A380 (gate 13)', 952, 442 * 0.8 * 1.5, 'm²', 'gate + view bay 2 x 476 = 952 m²', 'unchanged'))
A.append(('Claim hall, Schengen (west)', 2674, DEP_NB * 0.25 * 1.7 + 5 * 160, 'm²', '80 x 36 less the escalator void = 2674 m², 5 belts', 'unchanged + lost & found, oversize belt, trolley bays'))
A.append(('Arrivals hall west (meeters)', 960, (DEP_NB * 0.5 * 25 / 60 + DEP_NB * 5 / 60) * 2.3, 'm²', '80 x 12 = 960 m²', 'unchanged'))
A.append(('Claim hall, non-Schengen (east)', 0, SURGE_NS * 0.6 * 1.7 + 2 * 300, 'm²', 'none - wide-body bags share the west belts', 'EAST CLAIM HALL 76 x 46 = 3496 m², 2 wide-body belts'))
A.append(('Passport control in', 0, SURGE_NS * 2, 'pax/h', 'none', '6 booths x 120 + 8 e-gates x 180 = 2160 /h'))
A.append(('Passport control out', 0, DEP_WB, 'pax/h', 'none', '3 booths x 120 + 4 e-gates x 180 = 1080 /h'))
A.append(('Arrivals hall east (meeters)', 0, (SURGE_NS * 0.8 + SURGE_NS * 0.17) * 2.3, 'm²', 'none', 'EAST HALL meeting side 1880 m²'))
ax = fig.add_axes([0.35, 0.07, 0.635, 0.84]); ax.set_facecolor(BG); ax.axis('off')
ax.set_xlim(0, 100); ax.set_ylim(-0.5, len(A) + 0.8)
ax.text(0, len(A) + 0.5, 'EACH AREA: TODAY vs NEEDED (bar = today as % of the need; 100 % line in white)', color=NEW, fontsize=17, weight='bold')
for i, (n, t, need, u, tt, at) in enumerate(A):
    y = len(A) - 1 - i
    pct = t / need * 100 if need else 0
    col = OK if pct >= 100 else (WARN if pct > 0 else NS)
    ax.text(0, y + 0.18, n, color=FG, fontsize=13, weight='bold', va='center')
    ax.text(0, y - 0.18, 'needs %d %s' % (round(need, -1), u), color=DIM, fontsize=11.5, va='center')
    x0, wbar = 24, 22
    ax.add_patch(Rectangle((x0, y - 0.25), wbar, 0.5, fc='#121a22', ec=GRID))
    ax.add_patch(Rectangle((x0, y - 0.25), wbar * min(pct, 200) / 200, 0.5, fc=col, ec='none'))
    ax.plot([x0 + wbar / 2] * 2, [y - 0.33, y + 0.33], color=FG, lw=1.5)
    ax.text(x0 + wbar + 0.8, y, ('%d %%' % pct) if pct else 'missing', color=col, fontsize=13, weight='bold', va='center')
    ax.text(53, y + 0.18, 'TODAY: ' + tt, color=DIM, fontsize=11.5, va='center')
    ax.text(53, y - 0.18, 'AFTER: ' + at, color=OK if 'unchanged' not in at else '#b8c4d0', fontsize=11.5, va='center')
    ax.plot([0, 100], [y - 0.48, y - 0.48], color='#1a2430', lw=0.8)
fig.text(0.35, 0.035, 'Short today: check-in throughput (480 of ~1100 bags/h) and security lanes (900 of ~1700 /h). Missing today: everything '
         'non-Schengen (gates 13-16 are the wide-body stands, long-haul). Gate lounges and the west claim/arrivals halls already meet the standard. '
         'The Grand Hall keeps its 2160 m²: the extra capacity comes from bag-drop machines, automated security lanes and the empty east wing.',
         color=FG, fontsize=12.5, wrap=True)
save(fig, 'TC-1-Capacity.png')
