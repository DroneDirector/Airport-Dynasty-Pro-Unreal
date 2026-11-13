from reportlab.lib.pagesizes import mm
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase.pdfmetrics import stringWidth
W, H = 110*mm, 230*mm
G = {
 'none':   ('#9e9e9e', 'No dependencies, any time'),
 'people': ('#1e88e5', 'People chain'),
 'voice':  ('#e53935', 'Voice chain'),
 'field':  ('#43a047', 'Airfield chain'),
 'gfx':    ('#8e24aa', 'Graphics chain'),
 'rel':    ('#fb8c00', 'Releases'),
}
# (no, title, estimate, groups, needs, done)   a letter after the number is a part of a long job (each a 2-3 h session)
ITEMS = [
 ('4',   'Time of year (seasons) setting, moves the sun', 'done', ['none'], '', True),
 ('1',   'Fix the people builder', 'done', ['people'], '', True),
 ('26',  'Fire trucks: full getting-in / getting-out pass', 'done', ['none'], '', True),
 ('27',  'Truck engine sound', 'done', ['none'], '', True),
 ('28',  'Grass distance slider (was already in Settings)', 'done', ['none'], '', True),
 ('16',  'Voice comparison: Chatterbox / Kokoro, calm + angry', 'done', ['voice'], '', True),
 ('2',   'Re-fit the 120 people (runs on its own)', '2-4 h', ['people'], '1', False),
 ('3',   '7 very overweight people + 10 teenagers', '1 h', ['people'], '1', False),
 ('31',  'No-smoking sign on the entrance doors: compile + check', '15 min', ['none'], '', False),
 ('6',   'Airfield ground surface, runway slopes + crown: compile + look', '2 h', ['field'], '', False),
 ('7',   'Aircraft + vehicles on the slope: check', '30 min', ['field'], '6', False),
 ('8',   'Markings on the surface: check', '30 min', ['field'], '6', False),
 ('12',  'Edge bands: compile, look, tune the textures by eye', '1.5 h', ['field'], '8', False),
 ('29',  'F: backup of generated folders', '1 h', ['none'], '', False),
 ('20',  'Door rule: only sliding doors open themselves; E / X elsewhere', '2-3 h', ['none'], '', False),
 ('5',   'Release 0.17.7: truck pass, engine sound, sign, airfield', '1.5 h', ['rel'], '2, 3, 6-8, 12', False),
 ('21a', 'Hero people built at full (strand) hair quality', '2-3 h', ['people'], '2', False),
 ('21b', 'Settings option "Hair: cards / strands", near / far switch; the quality shows on the people everywhere they stand: the new-career gender screen, the creator, the wardrobe, the airport', '2-3 h', ['people'], '21a', False),
 ('21c', 'Moustaches and beard styles as a choice in the creator, put on the figure in front of you as you pick (and on the built people), at the same hair quality', '2-3 h', ['people'], '21b, 32b', False),
 ('32',  'New game: the "who are you" screen (she left / he right, neon tubes, floor fog, the chosen one steps into the light), hover + pad, confirm; then the other walks out of frame (her left, him right), the tubes fade and recede into the fog, camera still, UI on the free side', '2-3 h', ['people'], 'concept done', True),
 ('32a', 'Character creator: the parts library (faces x skin tones, bodies, hair fits), unattended', '19 h PC', ['people'], '2', False),
 ('32b', 'Character creator: the screen (face, skin, body, height, hair, clothes), saved, a switch turns it on; fog clears round you at shoes', '1 day (3 parts)', ['people'], '32, 32a, wardrobe', False),
 ('32c', 'New game: pets (pick one or none) and married or not, then the looks of the spouse (the creator again); then to the Overview', '2-3 h', ['people'], '32b', False),
 ('32d', 'Concept renders of every new-game step (creator, pets, spouse) in the neon style, for the owner to see first', '2 h', ['people'], '32', False),
 ('32e', 'New game economy: no demo content ever (kept outside as backup), difficulty as the last neon step, money per difficulty (hard: one used A320-class + crew + spare), crew always bought, one-aircraft starter warning', '3-4 h', ['people'], '32', True),
 ('32f', 'New game: Start game button, neon loading screen (animated bar, file names + percent, tips), no game loader or plaque after the title; into the Overview only when fully loaded', '2-3 h', ['people'], '32e', True),
 ('32g', 'New game: the airline founding (kind, name, ICAO code, callsign, your name; home base fixed at BIAP) as a neon step before the difficulty; the game form between Start game and the loading is gone', '2-3 h', ['people'], '32f', True),
 ('32h', 'New game: the loading screen also lists what Unreal itself loads (people, hair, terminal) with its real percentage', '1-2 h', ['people'], '32g', False),
 ('13a', 'Uniforms: cabin crew + pilots', '2 h', ['people'], '2, 3', False),
 ('13b', 'Uniforms: ground crew, security, fire crew', '2 h', ['people'], '13a', False),
 ('13c', 'Passengers: clothes variety', '2 h', ['people'], '13a', False),
 ('14a', 'Animations: walk / jog / sprint retargeted', '2-3 h', ['people'], '13a', False),
 ('14b', 'Animations: sit, lean, idle', '2-3 h', ['people'], '14a', False),
 ('30',  'Idle animation on people', '1 h', ['people'], '14b', False),
 ('15a', 'Smart objects: seats, walls, windows marked + the chooser', '2-3 h', ['people'], '14b', False),
 ('15b', 'Smart objects: sit / lean / stand at window in play', '2-3 h', ['people'], '15a', False),
 ('15c', 'Queues, families together, old people take seats, late sprint', '2-3 h', ['people'], '15b', False),
 ('15d', 'Bags, backpacks, wheeled suitcases: models + carrying', '3 h', ['people'], '15b', False),
 ('15e', 'Phone use: calls, scrolling, photos through the window', '2-3 h', ['people'], '15b', False),
 ('15f', 'Airside rule: nobody back out past security; leave by their flight', '2 h', ['people'], '15c', False),
 ('17',  'Talking faces: face animation from the audio', '4-6 h (2 parts)', ['people', 'voice'], '14b, 16', False),
 ('18a', 'Conversations: Qwen3-4B running locally, fed the game state', '3 h', ['voice'], '', False),
 ('18b', 'Conversations: our airport dialogue data + fine-tune', '3 h', ['voice'], '18a', False),
 ('18c', 'Conversations: E to talk, choice menus, subtitles', '3 h', ['people', 'voice'], '15b, 18a', False),
 ('18d', 'Conversations: Chatterbox voices streamed in the game', '3 h', ['voice'], '18c', False),
 ('18e', 'Conversations: morale, complaints, flight info hooks', '3 h', ['people', 'voice'], '18c', False),
 ('18f', 'Conversations: low-end fallback lines, Settings GPU / CPU / off', '2 h', ['voice'], '18d', False),
 ('19a', 'Academy: class routine, "class in session" sign, timetable', '3 h', ['people'], '15c', False),
 ('19b', 'Academy: knock, "come in", the teacher\'s reaction', '3 h', ['people', 'voice'], '19a, 18d, 20', False),
 ('9',   'Sunrise / sunset reflections on the tarmac', '2 h', ['field'], '8', False),
 ('10',  'Runway wear: skid marks, rubber, patches, cracks, stains', '3 h', ['field'], '8', False),
 ('11',  'Puddles (big ones too) + wet pavement after rain / hose (resend photos)', '4 h (2 parts)', ['field', 'gfx'], '9', False),
 ('22a', 'Cheap tier: ground decals (tyre marks, stains, patches)', '3 h', ['gfx'], '10', False),
 ('22b', 'Cheap tier: heat haze behind engines / APUs', '2 h', ['gfx'], '', False),
 ('22c', 'Cheap tier: lamp flares at night', '2 h', ['gfx'], '', False),
 ('22d', 'Cheap tier: virtual textures for big ground / building textures', '3 h', ['gfx'], '', False),
 ('23a', 'Advanced: hardware ray tracing for Lumen + its per-card setting', '3-4 h', ['gfx'], '22a', False),
 ('23b', 'Advanced: MegaLights + setting, tested with frame generation', '3-4 h', ['gfx'], '22c', False),
 ('23c', 'Advanced: Nanite ground displacement + setting', '3 h', ['gfx'], '22a', False),
 ('23d', 'Advanced: Substrate materials + setting (last)', '3-4 h', ['gfx'], '23a', False),
 ('25',  'Trim MetaHuman plugin weight; check update size', '1-2 h', ['rel'], '5', False),
 ('24a', 'Settings menu: frame, navigation, glass look', '3 h', ['gfx'], '', False),
 ('24b', 'Settings menu: Graphics page (all options known)', '3 h', ['gfx'], '24a, 23d', False),
 ('24c', 'Settings menu: Display, Sound, Controls', '2-3 h', ['gfx'], '24a', False),
 ('24d', 'Settings menu: Gameplay, Interface, Accessibility, Language', '2-3 h', ['gfx'], '24a', False),
 ('24e', 'Settings menu: Online, Saves and data; Save / Load / Export', '2-3 h', ['gfx'], '24a', False),
 ('24f', 'Settings menu: Android touch pass', '2 h', ['gfx'], '24e', False),
    # owner, 2026-11-13 (during the ground job)
    ('40a', 'Ground job: one master ground material, real CC0 scans, cracks + tar, tyre rubber, puddles, smooth taxiway fillets, photoreal edges, real grass', '11-16 h', ['field'], '', False),
    ('40b', 'Ground job: apron from the layout, ICAO/FAA markings (hold-short pattern A, red runway-ahead signs, stand lines), gate dirt, faint stand tyre marks', '3-4 h', ['field'], '40a', False),
    ('40c', 'Service roads: gate 1 to the control tower, the fire station to the taxiway by gate 1 and down to the taxiway below, vehicle markings, same detail', '2-3 h', ['field'], '40b', False),
    ('40d', 'Light snow layer on apron, taxiways, runway from winter reference photos', '3-4 h', ['field'], '40b', False),
    ('40e', 'Taxiway lights: blue edge + tiny inset green centre lights at real spacing, modelled fixtures (runway lights researched too)', '2-3 h', ['field'], '40a', False),
    ('40f', 'Graphics setting "Airport ground": Low 2K / Medium / High 4K / Ultra 8K + fine relief', '2 h', ['gfx'], '40a', False),
    ('41a', 'Fire truck: its paint on a master material with wear, dirt and wetness like the ground (its own red, not the markings red); compile + publish', '2-3 h', ['none'], '', False),
    ('41b', 'Fire truck: surfaces flickering through each other (z-fighting) - find and separate them', '1-2 h', ['none'], '', False),
    ('42', 'True Unreal build: CONVERT the Babylon-exported model (terminal building and fittings, rental garage) in Unreal - same geometry and textures, split into instanced Nanite pieces, materials on Unreal masters reusing the same textures, no tie to the Babylon export; rebuild only a piece too rough to convert, to the same shape; compare each with a Babylon render', '1.5-2.5 days', ['none'], '', False),
    ('43a', 'Jetways animated in Unreal (his docking: parked pulled half way back - wheels half as far out from the rotunda as docked, drives out to the front door when called, back after; stair, hoses, bellows, cab, canopy all moving with it)', '1 day', ['none'], '', False),
    ('43b', 'Stop bars: check every type nose-wheel setback and front-door position against the makers airport planning document; jetway docks per type', '2 h', ['field'], '43a', False),
    ('44', 'Weather-driven ground: snow builds up with how hard it snows and how cold it is (melts above 0), grass collecting more and more while the paving stays relatively free (ploughed); puddles from how much it rains - driven by the real-weather system', '3-4 h', ['field'], '', False),
    ('45', 'Per-stand wear settings: each stand in the layout file can set its own wear amount ("wear": 0.3 almost clean, 1.5 heavily used) and tyre marks ("tyres": 0 none, 1 strong); stands left unset keep their random look', '30 min', ['field'], '', False),
    ('46', 'Ground-equipment wear on the stands: rubber marks from pushback tugs and service trucks, tracks of the GPU (ground power unit), belt loaders and other ground equipment, where they really drive and park around the aircraft', '1-2 h', ['field'], '', False),
    ('47', 'Update notices kept apart: a PC update shows its news only in the PC version, an Android update only in the Android version (each reads its own version and release-notes files, never the other one)', '1 h', ['none'], '', False),
    ('40g', 'Iceland scenery: mountains torn into walls and steps - rebuild the Iceland/Faroes height tiles with feathered edges where they meet the world map', '3-4 h', ['field'], '', False),
]
c = canvas.Canvas('AirportDynastyPro-ToDo.pdf', pagesize=(W, H))
c.setTitle('Airport Dynasty Pro - to-do checklist')
BG, FG, DIM, RULE = HexColor('#1b1b1f'), white, HexColor('#b0b0b8'), HexColor('#44444c')
def header(y):
    c.setFillColor(BG); c.rect(0, 0, W, H, fill=1, stroke=0)
    c.setFillColor(FG); c.setFont('Helvetica-Bold', 11); c.drawString(8*mm, y, 'Airport Dynasty Pro - to-do checklist')
    c.setFont('Helvetica', 6.5); c.setFillColor(DIM)
    c.setFillColor(FG); c.setFont('Helvetica-Bold', 7)
    c.drawString(8*mm, y-10, 'The work is done in sections: every job of a day or more is split')
    c.drawString(8*mm, y-19, 'into parts of 2-3 hours, one part per session. The letters after a')
    c.drawString(8*mm, y-28, 'number (15a, 15b ...) are those parts, in order.')
    c.setFont('Helvetica', 6.5); c.setFillColor(DIM)
    c.drawString(8*mm, y-38, 'In build order. "needs": what must be done before. Times are estimates.')
    c.setFillColor(FG); c.setFont('Helvetica-Bold', 7)
    c.drawString(8*mm, y-48, 'ALL TIMES ARE WORKING HOURS, not clock time: "1 day" = 8 hours of work,')
    c.drawString(8*mm, y-57, 'so "1.5-2.5 days" = 12-20 hours of work. Calendar time can be shorter,')
    c.drawString(8*mm, y-66, 'since builds run on their own and work can go on into the evening.')
    c.setFont('Helvetica', 6.5); c.setFillColor(DIM)
    c.drawString(8*mm, y-76, 'Updated 2026-11-13.')
    c.setFillColor(FG)
    return y-83
y = H - 12*mm
y = header(y)
c.setFont('Helvetica', 6.5)
for key, (col, label) in G.items():
    c.setFillColor(HexColor(col)); c.rect(8*mm, y-5, 5, 5, fill=1, stroke=0)
    c.setFillColor(FG); c.drawString(8*mm+8, y-4, label); y -= 8
y -= 4
def wrap(text, width, font, size):
    words, lines, cur = text.split(), [], ''
    for w in words:
        t = (cur + ' ' + w).strip()
        if stringWidth(t, font, size) <= width: cur = t
        else: lines.append(cur); cur = w
    if cur: lines.append(cur)
    return lines
textw = W - 8*mm - 26*mm - 24*mm
for no, title, est, groups, needs, done in ITEMS:
    lines = wrap(title, textw, 'Helvetica', 7.5)
    rowh = max(14, 9*len(lines) + (8 if needs else 0) + 4)
    if y - rowh < 10*mm:
        c.showPage(); y = H - 12*mm; y = header(y); y -= 4
    x = 8*mm
    c.acroForm.checkbox(name='item_' + no, x=x, y=y-rowh+3, size=9, buttonStyle='check', borderWidth=1, borderColor=white, fillColor=HexColor('#2c2c33'), textColor=white, checked=done, forceBorder=True)
    x += 13
    for g in groups:
        c.setFillColor(HexColor(G[g][0])); c.rect(x, y-rowh+3, 5, 9, fill=1, stroke=0); x += 7
    x = 8*mm + 26
    c.setFillColor(DIM if done else FG); c.setFont('Helvetica-Bold', 7.5); c.drawString(x, y-8, no + '.')
    c.setFont('Helvetica', 7.5)
    for i, ln in enumerate(lines): c.drawString(x+16, y-8-9*i, ln)
    yy = y-8-9*len(lines)
    if needs:
        c.setFont('Helvetica-Oblique', 6.5); c.setFillColor(DIM); c.drawString(x+16, yy, 'needs ' + needs)
    c.setFillColor(DIM if done else FG); c.setFont('Helvetica', 7); c.drawRightString(W-6*mm, y-8, est)
    c.setStrokeColor(RULE); c.line(8*mm, y-rowh, W-6*mm, y-rowh)
    y -= rowh + 1
c.showPage(); c.save(); print('ok')
