# The ground floor drawn BEFORE (from Data/terminal-plan.json and the arrivals wing script) and AFTER (the proposal).
import json
from tc_common import *
PLAN = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
R = {r['id']: r for r in PLAN['rooms']}
SHORT = {'F1-HALL': '', 'F1-ENTR-W': '', 'F1-ENTR-C': '', 'F1-ENTR-E': '', 'F1-MUSEUM': 'escalator room\n(airside)', 'F1-SECURITY': 'SECURITY\n6 lanes',
         'F1-AIRSIDE': 'airside lobby', 'F1-LOUNGE': 'lounge', 'F1-HBS': 'hold bag\nscreening', 'F1-L3': 'L3', 'F1-EBS': 'EBS',
         'F1-BAG-EAST': '', 'F1-MAKEUP': 'bag make-up', 'F1-CARTLANE': 'cart lane', 'F1-STAFF-ENTR': 'staff',
         'F1-WC-M': 'WC', 'F1-WC-F': 'WC', 'F1-SERVICE': 'service', 'F1-ESC-PASS': '', 'F1-AIRSIDE-CORR': 'airside corridor',
         'F1-STORE-W': '', 'F1-TAXFREE-UNDER': 'under the tax free (empty)', 'F1-BAGHALL': 'baggage hall (sorting, under the gate walkway)'}
FC = {'hall': '#18222c', 'security': '#1a2a1d', 'baggage': '#2a2210', 'lounge': '#10261a', 'airside': '#10261a', 'empty': '#0d1116',
      'baghall': '#1d1a10', 'corridor': '#2a2210', 'staffentr': '#1a1a26'}


def y_box(ax, *a, **k):
    k.setdefault('fc', '#2a2410'); k.setdefault('ec', NEW); k.setdefault('col', NEW)
    box(ax, *a, **k)


def claim_wing(ax, after, fs=8):
    box(ax, -80, -4, 0, 8, 'ARRIVALS HALL WEST (meeters)\n80 x 12 = 960 m²', fc='#18222c', fs=fs)
    box(ax, -80, 8, 0, 16, '', fc='#1a2430')
    box(ax, -44, 8, -36, 16, 'green', fc='#10301a', ec=OK, col=OK, fs=fs - 1)
    box(ax, -54, 8, -48, 16, 'red', fc='#301010', ec='#ff5050', col='#ff8080', fs=fs - 1)
    box(ax, -80, 16, 0, 52, '', fc='#18222c')
    ax.text(-40, 47, 'CLAIM HALL WEST (Schengen), 5 belts', color=DIM, ha='center', fontsize=fs)
    for cx in (-60.8, -48.15, -35.5, -22.85, -10.2):
        box(ax, cx - 2.2, 21, cx + 2.2, 41, fc='#2b3a48', ec=BELT, lw=1)
    box(ax, -80, 34.5, -68.2, 52, 'esc.', fc='#101820', fs=fs - 1)
    if after:
        y_box(ax, -78.5, 17, -70, 24, 'lost &\nfound', fs=fs - 1)
        y_box(ax, -6, 42, -1, 51, 'over-\nsize\nbelt', fs=fs - 1.5)
        for xt in (-68, -30, -16): box(ax, xt, 17, xt + 6, 19, fc=NEW, ec=NEW)
        ax.text(-30, 20.3, 'trolley bays', color=NEW, fontsize=fs - 1.5, ha='left')


def ground(ax, after, x0=-90, x1=560, fs=8):
    for r in PLAN['rooms']:
        if r['f'] != 1 or r['x1'] < x0 or r['x0'] > x1: continue
        rid = r['id']
        if rid == 'F1-EAST-WING' and after: continue
        if rid.startswith('F1-GATE') or rid.startswith('F1-RAMP'):
            box(ax, r['x0'], r['z0'], r['x1'], r['z1'], ('G%s' % rid.split('-')[-1]) if 'GATE' in rid else '',
                fc='#10261a', ec=OK, col=OK, fs=fs - 1)
            continue
        txt = SHORT.get(rid, r['name'][:22])
        if rid == 'F1-EAST-WING': txt = 'EAST WING - EMPTY TODAY\n395 x 52 = 20 540 m²'
        box(ax, r['x0'], r['z0'], r['x1'], r['z1'], txt, fc=FC.get(r['kind'], '#141c24'), fs=fs + (3 if rid == 'F1-EAST-WING' else 0),
            col=WARN if rid == 'F1-EAST-WING' else DIM)
    claim_wing(ax, after, fs)
    for v in ('F1-ENTR-W', 'F1-ENTR-C', 'F1-ENTR-E'):
        r = R[v]; box(ax, r['x0'], r['z0'], r['x1'], r['z1'], fc='#18222c')
    ax.plot([42, 114], [0, 0], color=GLASS, lw=3, zorder=5)
    ax.text(78, 24.5, 'GRAND HALL  72 x 30 = 2160 m²', color=FG, fontsize=fs + 1, ha='center', weight='bold', zorder=9)
    # the check-in islands (both versions)
    for xs in (47.5, 74.5):
        box(ax, xs, 15.6, xs + 25, 19.1, fc='#2b3a48', ec='#c9d6e2', lw=0.8)
    ax.text(73.5, 13.6, '20 check-in desks (2 islands)', color=DIM, fontsize=fs - 1.5, ha='center')
    for xd in (62, 78): box(ax, xd - 3, 29.6, xd + 3, 30.4, fc='#7be07b', ec='none')
    ax.plot([48, 116], [19.6, 19.6], color=TUN, lw=2.5, ls=(0, (5, 2)), zorder=7)
    if not after: return
    # ---- AFTER: Grand Hall fittings ----
    for xk in (57.5, 84.5):
        for k in range(6): box(ax, xk + k * 1.15, 3.0, xk + k * 1.15 + 0.8, 4.0, fc=NEW, ec=NEW)
    ax.text(73.5, 5.6, '12 self-service kiosks (2 x 6)', color=NEW, fontsize=fs - 1.5, ha='center')
    y_box(ax, 101, 15.6, 113, 19.1, '6 bag-drop', fs=fs - 2)
    y_box(ax, 108, 23, 113.5, 28.5, 'service\ndesk', fs=fs - 2)
    for xd in (62, 78): box(ax, xd - 3, 28.2, xd + 3, 29.4, fc=NEW, ec=NEW)
    ax.text(70, 27.2, 'boarding-pass e-gates', color=NEW, fontsize=fs - 1.5, ha='center')
    for xt in (43, 108.5): box(ax, xt, 0.6, xt + 5, 1.8, fc=NEW, ec=NEW)
    ax.text(50, 70 - 70 + 33, '', color=NEW)
    ax.text(70, 36, 'SECURITY: 6 automated (ATRS) lanes', color=NEW, fontsize=fs - 1, ha='center', zorder=9)
    # the link gallery: a glass landside walk in front of the baggage block, Grand Hall <-> East Hall
    y_box(ax, 107, -4, 156, 0, 'LINK GALLERY 49 x 4', fs=fs - 1.5, lw=1.6)
    # the East Hall (landside, full height to the new wave roof)
    box(ax, 156, 0, 228, 40, '', fc='#22200e', ec=NEW, lw=2.2)
    ax.text(192, 37.3, 'EAST HALL  72 x 40 = 2880 m²', color=NEW, ha='center', fontsize=fs + 1, weight='bold')
    y_box(ax, 160, 18, 190, 22, 'long-haul check-in: 8 desks + 4 bag-drop', fs=fs - 2.5)
    y_box(ax, 191, 18, 195, 22, 'OS', fs=fs - 2.5)
    y_box(ax, 158, 2, 172, 10, 'cafe', fs=fs - 1)
    y_box(ax, 174, 2, 190, 7, 'shops', fs=fs - 1)
    y_box(ax, 196, 2, 226, 6, 'car-rental counters (5)', fs=fs - 1.5)
    box(ax, 200, 12, 226, 30, 'MEETERS +\nGREETERS', fc='#262410', ec=NEW, col=NEW, fs=fs, ls='--')
    y_box(ax, 158, 27, 172, 33, 'info / PRM\nassistance', fs=fs - 2)
    y_box(ax, 174, 27, 186, 33, 'first aid', fs=fs - 2)
    box(ax, 156, 40, 192, 52, 'WC, family room,\nquiet room', fc='#1a1f2a', ec=NEW, col=NEW, fs=fs - 1.5)
    box(ax, 192, 40, 228, 52, 'trolley store +\nbag-drop service', fc='#1a1f2a', ec=NEW, col=NEW, fs=fs - 1.5)
    # the landside promenade along the front of the east wing to the coach station
    box(ax, 228, 0, 470, 6, 'LANDSIDE PROMENADE  242 x 6  (shops, glass front)  ->  coach + bus station', fc='#262410', ec=NEW, col=NEW, fs=fs - 1)
    # customs east
    box(ax, 228, 6, 236, 40, 'CUSTOMS', fc='#1a2430', ec=NS, col=NS, fs=fs - 1.5, rot=90)
    box(ax, 228.5, 12, 235.5, 18, fc='#10301a', ec=OK); box(ax, 228.5, 28, 235.5, 33, fc='#301010', ec='#ff5050')
    box(ax, 228, 40, 236, 52, 'office', fc='#1a2430', ec=NS, col=NS, fs=fs - 2.5)
    # the east claim hall (non-Schengen)
    box(ax, 236, 6, 312, 52, '', fc='#24101a', ec=NS, lw=2.0)
    ax.text(274, 9.5, 'CLAIM HALL EAST (non-Schengen)  76 x 46 = 3496 m²', color=NS, ha='center', fontsize=fs - 0.5, weight='bold')
    for cx in (256, 284):
        box(ax, cx - 4, 14, cx + 4, 48, 'wide-\nbody\nbelt', fc='#2b3a48', ec=BELT, col=BELT, fs=fs - 2)
    y_box(ax, 300, 41, 311, 51, 'lost &\nfound', fs=fs - 2)
    y_box(ax, 300, 30, 311, 38, 'over-\nsize belt', fs=fs - 2)
    for xt in (241, 266): box(ax, xt, 12.5, xt + 7, 13.8, fc=NEW, ec=NEW)
    # passport control in
    box(ax, 312, 6, 324, 52, '', fc='#2a1020', ec=NS, lw=1.6)
    for k in range(6): box(ax, 313.5, 8 + k * 3, 322.5, 10.2 + k * 3, fc='#3a1a2a', ec=NS)
    for k in range(8): box(ax, 315, 27.5 + k * 2.6, 321, 28.7 + k * 2.6, fc=NS, ec=NS)
    ax.text(318, 26.2, '6 booths / 8 e-gates', color=NS, fontsize=fs - 2.5, ha='center')
    box(ax, 324, 6, 372, 52, '', fc='#24101a', ec=NS)
    ax.text(348, 26, 'PASSPORT CONTROL IN\nqueue 48 x 30\n(1 440 m²)', color=NS, fontsize=fs, ha='center', va='center')
    box(ax, 340, 40, 368, 51, 'escalators + stair + lift\ndown from the F2 mezzanine', fc='#3a1a2a', ec=NS, col=NS, fs=fs - 2.5, ls='--')
    # staff and operations
    for (a, b, c, d, t) in ((372, 6, 404, 26, 'crew centre\n(briefing, rest)'), (404, 6, 436, 26, 'staff canteen'),
                            (436, 6, 470, 26, 'ground handling\n+ airline offices'), (372, 26, 404, 52, 'lockers +\nchanging'),
                            (404, 26, 436, 52, 'STAFF SECURITY\n(2 lanes) -> airside'), (436, 26, 470, 52, 'police +\nborder guard')):
        box(ax, a, b, c, d, t, fc='#1c1c2a', ec='#9a8cff', col='#c8c0ff', fs=fs - 1.5)
    # the coach and bus station
    box(ax, 470, 0, 551.2, 52, 'COACH + BUS STATION\n81 x 52 = 4220 m²\nwaiting hall, ticket desks,\ndeparture boards, cafe, WC,\ndoors to the coach bays',
        fc='#22200e', ec=NEW, col=NEW, fs=fs, lw=2.2)
    # the belts re-routed (dashed purple = tunnel under the floor, cyan = feeds)
    ax.plot([116, 116], [19.6, 23], color=TUN, lw=2.5, ls=(0, (5, 2)), zorder=7)
    ax.plot([195, 151], [23.5, 23.5], color=TUN, lw=2.5, ls=(0, (5, 2)), zorder=7)
    box(ax, 150, 22.6, 153.5, 26, fc='#241a2e', ec=TUN, z=8)
    for cx in (256, 284): ax.plot([cx, cx], [48, 60], color=BELT, lw=2.5, zorder=7)
    ax.plot([156, 551.2], [0, 0], color=GLASS, lw=3, zorder=5)


def clip(ax):
    for t in ax.texts: t.set_clip_on(True)
