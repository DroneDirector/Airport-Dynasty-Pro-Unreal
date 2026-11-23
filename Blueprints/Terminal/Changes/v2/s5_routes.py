# Sheets 5a-5c: the passenger routes on both floors (departures, arrivals, transfers), and why nobody mixes who must not.
from v2_common import *
X0, X1, Z0, Z1 = -128, 556, -28, 92


def two(no, title, sub):
    fig = sheet(no, title, sub); A = []
    for k, fl in enumerate((2, 1)):
        ax = ax_plan(fig, [0.008, 0.578 - k * 0.355, 0.984, 0.345], X0, X1, Z0 if fl == 1 else -6, Z1)
        fills(ax, fl); gates(ax, fl, fs=13, x0=X0, x1=X1); (after_f1 if fl == 1 else after_f2)(ax, lw=1.0); walls(ax, fl, lw=1.0)
        if fl == 2:
            for g in JETWAYS:
                for x in JETWAYS[g]: ax.plot([x, x], [82, 89], color=OK, lw=3, zorder=11)
        fig.text(0.012, 0.925 - k * 0.367, 'FLOOR 2' if fl == 2 else 'GROUND FLOOR', color=FG, fontsize=18, weight='bold', va='top')
        A.append(ax)
    return fig, A[0], A[1]


def words(fig, L):
    for i, (c, t) in enumerate(L): fig.text(0.016, 0.205 - i * 0.0305, '■', color=c, fontsize=17, va='center'); fig.text(0.03, 0.205 - i * 0.0305, t, color=FG, fontsize=15.5, va='center')


# ------------------------------------------------------------ 5a departures
fig, U, D = two('SHEET 5a', 'DEPARTURES', 'Blue: Schengen flights. Pink: non-Schengen flights. Same route until the passport wall.')
route(D, [(73, -24), (73, 13), (70, 13), (70, 46), (12, 48), (12, 31), (12, 20)], SCH, 5); route(D, [(80, -24), (80, 13), (77, 13), (77, 40), (72, 46)], NS, 4)
route(D, [(192, -24), (192, 2), (178, 20)], NS, 4); route(D, [(170, 20), (165, 14), (152, -2), (110, -2), (104, 4), (82, 26)], NS, 4)
route(U, [(18, 15), (12, 38), (12, 62), (-60, 62), (-60, 72)], SCH, 5); route(U, [(12, 62), (181, 62), (181, 72)], SCH, 5)
route(U, [(14, 40), (14, 56.5), (234, 56.5), (234, 42), (252, 42), (274, 42), (274, 56.5), (306, 56.5), (306, 72)], NS, 4)
for x in (385, 458, 530): route(U, [(x - 60, 56.5), (x, 56.5), (x, 72)], NS, 3.5)
txt(D, 78, 7, 'check-in', FG, 13, bbox=dict(fc=BG, ec='none', pad=1)); txt(D, 96, 36, 'security', FG, 13); txt(D, 33, 20, 'escalators up', FG, 13); txt(D, 206, 14, 'long-haul check-in', NEW, 13); txt(D, 131, -8, 'link gallery', NEW, 13)
txt(U, 36, 24, 'up from the\nescalator room', FG, 13); txt(U, 205, 30, 'passport\ncontrol out', SCH, 14); txt(U, 274, 33, 'duty free', DF, 13); txt(U, 370, 38, 'concourse', NEW, 15); txt(U, 246, 89, 'glass wall at x 240', GLASS, 13)
words(fig, [(SCH, 'SCHENGEN: entrance, check-in / kiosk / bag drop, boarding-pass gate, security, escalators, walkway, gates 1-12 (tax free, food and bar on the way). They can never pass the wall.'),
            (NS, 'NON-SCHENGEN: the same, then passport control OUT beside the walkway, duty free, concourse or walkway, gates 13-16.'),
            (NEW, 'LONG-HAUL CHECK-IN is in the East Hall; from there the link gallery leads back to the one security point (about 115 m indoors).'),
            (OK, 'FORBIDDEN MIXING, checked: a Schengen boarding pass opens nothing beyond x 240; after the passport check there is no way back except through staff or transfer control.'),
            (GLASS, 'REAL RULE, security: one central screening for both sides (Regulation 300/2008). REAL RULE, border: exit check in Switzerland before boarding (Schengen Borders Code, Annex VI 2.1).')])
save(fig, 'TCv2-5a-Routes-Departures.png')

# ------------------------------------------------------------ 5b arrivals
fig, U, D = two('SHEET 5b', 'ARRIVALS', 'Green: from a Schengen country. Pink: from a non-Schengen country. The two never share a room.')
for x, z in ((181, 72), (100, 72), (20, 72)): route(U, [(x, z), (x, 62), (-40, 62), (-74, 62), (-74, 51)], ARRS, 4.5)
route(U, [(-143, 62), (-120, 62), (-84, 62)], ARRS, 4.5)
for g in GATE_X0: a = GATE_X0[g]; route(U, [(JETWAYS[g][0], 88), (JETWAYS[g][0], 79.5), (a + 46, 79.5), (a + 38, 78)], ARRN, 4)
route(D, [(-72, 48), (-72, 36), (-55, 18.8), (-40, 18.8), (-40, 2), (-40, -20)], ARRS, 5)
for g in (16, 15, 14): a = GATE_X0[g]; route(D, [(a + 42, 74), (a + 42, 65.6), (349, 65.6)], ARRN, 4)
route(D, [(297.8, 74), (297.8, 65.6), (337, 65.6)], ARRN, 4); route(D, [(343, 65.6), (343, 40), (326, 34)], ARRN, 5); route(D, [(326, 34), (306, 34), (296, 34)], ARRN, 5); route(D, [(296, 30), (284, 20.6), (270, 20.6), (238, 28)], ARRN, 5); route(D, [(238, 28), (224, 28), (192, 10), (192, -22)], ARRN, 5)
txt(D, -40, 49, 'duty free', NEW, 12, bbox=dict(fc=BG, ec='none', pad=1)); txt(D, -18, 12, 'customs', FG, 12); txt(D, -75, -22, 'taxi', NEW, 13); txt(D, 276, 43, 'claim', NS, 13); txt(D, 232, 44, 'customs', NS, 11, rotation=90); txt(D, 352, 24, 'passport\ncontrol in', NS, 14); txt(D, 440, 58, 'closed corridor', NS, 14); txt(D, 176, 14, 'East Hall', NEW, 13); txt(D, 222, -22, 'taxi', NEW, 13)
txt(U, 350, 60, 'no arriving passenger ever walks here', NS, 15); txt(U, -40, 57, 'walkway', DIM, 13)
words(fig, [(ARRS, 'SCHENGEN: gate, walkway, escalators down, claim hall west (duty free, toilets, trolley, belt, lost and found), customs green or red, one-way doors, arrivals hall, taxi / coach / car park.'),
            (ARRN, 'NON-SCHENGEN: jetway, down at the gate, closed corridor under the walkway, passport control in (EES kiosks, booths, e-gates), claim hall east, customs, East Hall, taxi / coach station.'),
            (OK, 'Schengen arrivals MAY meet departing Schengen passengers on the walkway: they were screened in a Schengen / EU airport (300/2008, 4.2 a).'),
            (OK, 'Non-Schengen arrivals never meet departing ones, not even from recognised countries: the passport check alone already demands it. So no search of the concourse is ever needed.')] +
      [(NEW, 'CUSTOMS for everyone: Switzerland is not in the EU customs union, so both claim halls end in red / green channels. The exits are one-way doors.')])
save(fig, 'TCv2-5b-Routes-Arrivals.png')

# ------------------------------------------------------------ 5c transfers
fig, U, D = two('SHEET 5c', 'TRANSFERS', 'Teal: from non-Schengen to Schengen. Orange: from Schengen to non-Schengen. Violet: non-Schengen to non-Schengen. Blue: Schengen to Schengen.')
route(U, [(100, 72), (100, 62), (-20, 62), (-20, 72)], SCH, 4.5)
route(U, [(20, 72), (20, 57), (234, 57), (234, 42), (256, 42), (274, 42), (274, 57), (306, 57), (306, 72)], WARN, 4.5)
route(U, [(332, 46), (311, 42), (311, 22), (229.5, 22), (229.5, 55), (222, 65), (181, 65), (181, 72)], TRF, 5)
route(U, [(334, 44), (337, 34), (352, 34), (370, 34)], TUN, 4.5); route(U, [(370, 34), (370, 60), (458, 60), (458, 72)], TUN, 4.5)
route(U, [(JETWAYS[16][0], 88), (JETWAYS[16][0], 79.5), (GATE_X0[16] + 38, 78)], '#ffffff', 4)
route(D, [(GATE_X0[16] + 42, 74), (GATE_X0[16] + 42, 65.6), (343, 65.6), (343, 48), (336, 45)], '#ffffff', 5)
txt(D, 333, 30, 'up to\ntransfers', TRF, 14); txt(D, 440, 58, 'arriving: the closed corridor', FG, 14); txt(U, 326, 29, 'TRANSFER\nCENTRE', TRF, 13, weight='bold'); txt(U, 268, 13, 'closed lane back to Schengen', TRF, 14); txt(U, 206, 36, 'passport\ncontrol out', WARN, 14)
words(fig, [(TRF, 'NON-SCHENGEN TO SCHENGEN: down at the gate, corridor, up into the transfer centre: security again (see below), passport control IN, then the closed lane to the Schengen walkway. Never through the claim hall.'),
            (WARN, 'SCHENGEN TO NON-SCHENGEN: along the walkway, passport control OUT like everyone, no new security check.'),
            (TUN, 'NON-SCHENGEN TO NON-SCHENGEN: transfer centre, security again if needed, NO passport check (they stay outside Schengen), straight into the concourse.'),
            (SCH, 'SCHENGEN TO SCHENGEN: just walk to the next gate. Through-checked bags go round in the baggage hall in all four cases.'),
            (NEW, 'REAL RULE, re-screening: transfer passengers from EU / EEA / Swiss airports and from recognised countries need none (300/2008, 4.1.2): today UK, USA, Canada, Singapore (Changi), Serbia (Belgrade), Montenegro,'),
            (NEW, 'Faroe Islands (Vágar) and Svalbard (2015/1998, attachment 4-B, version of 1 October 2026). All others, e.g. Turkey, the Gulf, Morocco, Japan, are screened again. Hence 2 lanes: one clean bypass, one full.')])
save(fig, 'TCv2-5c-Routes-Transfers.png')
