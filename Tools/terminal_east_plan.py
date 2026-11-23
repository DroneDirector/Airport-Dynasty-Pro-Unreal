# The terminal's east side in Data/terminal-plan.json (owner 2026-11-23, v2 approved, part 1): the one empty east wing room
# gives way to the real rooms (numbers: Blueprints/Terminal/Changes/v2/v2_common.py) - names for the map and the signs. No
# plan doors yet: passengers are not sent there before parts 2-5 are in (their doors and routes come then). Safe to run again.
#   python Tools/terminal_east_plan.py
import json, os, shutil
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); F = os.path.join(ROOT, 'Data', 'terminal-plan.json')
raw = open(F, encoding='utf-8', newline='').read(); P = json.loads(raw)
R = [('F1-LINK', 'Link gallery (Grand Hall to East Hall)', 107, -4, 162, 0, 'corridor', 's'), ('F1-EAST-HALL', 'East Hall', 156, 0, 228, 40, 'hall', 's'),
     ('F1-EAST-LANE', 'Staff lane', 156, 40, 160, 52, 'corridor', ''), ('F1-EAST-WC', 'Toilets (East Hall)', 160, 40, 178, 52, 'wc', ''), ('F1-EAST-FAMILY', 'Family and quiet room', 178, 40, 190, 52, 'wc', ''),
     ('F1-EAST-TROLLEY', 'Trolley store', 190, 40, 210, 52, 'store', ''), ('F1-EAST-BAGSERV', 'Baggage service', 210, 40, 228, 52, 'office', ''),
     ('F1-PROMENADE', 'Landside promenade', 228, 0, 470, 6, 'hall', 's'), ('F1-CUSTOMS-E', 'Customs (east)', 228, 6, 236, 40, 'customs', ''), ('F1-CUSTOMS-E-OFFICE', 'Customs office', 228, 40, 236, 52, 'office', ''),
     ('F1-CLAIM-E', 'Baggage claim east (non-Schengen)', 236, 6, 312, 52, 'claim', ''), ('F1-PC-LINE', 'Passport control in', 312, 20, 324, 52, 'customs', ''), ('F1-PC-OFFICE', 'Border police office', 312, 6, 324, 20, 'office', ''),
     ('F1-PC-HALL', 'Passport hall', 324, 6, 372, 52, 'hall', ''), ('F1-PC-SECOND', 'Second-line checks', 358, 41, 372, 51, 'office', ''),
     ('F1-CREW-CENTRE', 'Crew centre', 372, 6, 404, 26, 'office', ''), ('F1-CANTEEN', 'Staff canteen', 404, 6, 436, 26, 'office', ''), ('F1-GROUND-OFF', 'Ground handling and airline offices', 436, 6, 470, 26, 'office', ''),
     ('F1-LOCKERS', 'Lockers and changing', 372, 26, 404, 52, 'office', ''), ('F1-STAFFSEC', 'Staff security', 404, 26, 436, 52, 'office', ''), ('F1-POLICE', 'Police and border guard', 436, 26, 470, 52, 'office', ''),
     ('F1-COACH-STATION', 'Coach and bus station', 470, 0, 551.2, 52, 'hall', 'se')]
R = [dict(id=i, f=1, name=n, x0=a, z0=b, x1=c, z1=d, kind=k, **({'glass': g} if g else {})) for i, n, a, b, c, d, k, g in R]
for k, (a, b) in enumerate(((188, 200), (290, 300), (396, 406), (514, 526))):
    R.append(dict(id='F1-ENTR-EAST%d' % (k + 1), f=1, name='East entrance %d (vestibule)' % (k + 1), x0=a, z0=-4, x1=b, z1=0, kind='vestibule', glass='nswe'))
# Floor 2 (part 2a, 2026-11-23, sheet 3b): the concourse's rooms. No plan doors: passengers are not sent in before part 5 (the closed rooms
# are 'empty'; passport control's way through is shut by its glass leaves)
R += [dict(id='F2-' + i, f=2, name=n, x0=a, z0=b, x1=c, z1=d, kind=k) for i, n, a, b, c, d, k in (
    ('PC-QUEUE', 'Passport control out: queue', 231, 24, 248, 52, 'customs'), ('PC-LINE', 'Passport control out', 248, 24, 256, 52, 'customs'), ('TR-EXIT', 'Transfers to Schengen', 228, 24, 231, 52, 'empty'),
    ('TR-LANE', 'Transfer lane', 228, 20, 310.5, 24, 'empty'), ('DUTYFREE', 'Tax Free (non-Schengen)', 258, 24, 290, 52, 'empty'), ('SHOP-1', 'Books and news', 292, 24, 305, 38, 'empty'), ('WC-1', 'Toilets', 292, 40, 305, 52, 'wc'),
    ('TRANSFER', 'Transfer centre', 307, 24, 340, 52, 'empty'), ('FOODCOURT', 'Food court', 342, 24, 398, 52, 'corridor'), ('PRAYER', 'Prayer and quiet room', 400, 24, 424, 37, 'empty'), ('WC-2', 'Toilets, family, baby', 400, 39, 424, 52, 'wc'),
    ('LOUNGE-P', 'Premium lounge', 426, 24, 470, 52, 'empty'), ('SEATING', 'Seating, kids, work desks', 472, 24, 520, 52, 'corridor'), ('SHOPS-2', 'Shops', 522, 24, 551.2, 38, 'empty'), ('WC-3', 'Toilets', 522, 40, 551.2, 52, 'wc'),
    ('GALLERY', 'Window gallery', 310.5, 20, 551.2, 24, 'corridor'))]
# (the 2 m lanes between the rooms)
R += [dict(id='F2-LANE-%d' % k, f=2, name='Lane', x0=a, z0=b, x1=c, z1=d, kind='corridor') for k, (a, b, c, d) in enumerate(((256, 24, 258, 52), (290, 24, 292, 52), (292, 38, 305, 40), (305, 24, 307, 52), (340, 24, 342, 52),
    (398, 24, 400, 52), (400, 37, 424, 39), (424, 24, 426, 52), (470, 24, 472, 52), (520, 24, 522, 52), (522, 38, 551.2, 40)))]
ids = {r['id'] for r in R} | {'F1-EAST-WING', 'F2-CONCOURSE'}
P['rooms'] = [r for r in P['rooms'] if r['id'] not in ids] + R
nl = '\r\n' if '\r\n' in raw else '\n'
open(F, 'w', encoding='utf-8', newline='').write(json.dumps(P, indent=1 if '\n ' in raw else None, ensure_ascii=raw.isascii()).replace('\n', nl))
if os.path.exists(os.path.join(ROOT, 'Content', 'Data', 'terminal-plan.json')): shutil.copy(F, os.path.join(ROOT, 'Content', 'Data', 'terminal-plan.json'))
print('plan: %d rooms (%d east)' % (len(P['rooms']), len(R)))
