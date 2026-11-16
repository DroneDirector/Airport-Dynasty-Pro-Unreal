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
 ('2',   'Re-fit the 120 people (runs on its own)', '3 h (PC runs it)', ['people'], '1', False),
 ('3',   '7 very overweight people + 10 teenagers', '3 h (PC runs it)', ['people'], '1', False),
 ('31',  'No-smoking sign on the entrance doors: compile + check', '15 min', ['none'], '', True),
 ('6',   'Airfield ground surface, runway slopes + crown: compile + look', '45 min', ['field'], '', True),
 ('7',   'Aircraft + vehicles on the slope: check', '15 min', ['field'], '6', True),
 ('8',   'Markings on the surface: check', '15 min', ['field'], '6', True),
 ('12',  'Edge bands: compile, look, tune the textures by eye', '45 min', ['field'], '8', True),
 ('29',  'F: backup of generated folders', '15 min + copy time', ['none'], '', False),
 ('20',  'Door rule: only sliding doors open themselves; E / X elsewhere', '30 min', ['none'], '', True),
 ('5',   'Release 0.17.7: truck pass, engine sound, sign, airfield', '1.5 h', ['rel'], '2, 3, 6-8, 12', True),
 ('21a', 'Hero people built at full (strand) hair quality', '45 min + PC build', ['people'], '2', False),
 ('64', 'Pixel Norma company splash (owner’s logo, Art/Splash/PixelNorma-source.jpg): 4K 3840x2160, logo about half the screen, centred on black, epic animation (sparks gather, neon switches on, pixel trail, letters light one by one); exactly his logo - layers cut from his image, sharp commercial-licence upscale, lettering traced; never ugly: no soft edges, halos, banding or seams; stills first for his OK; shown before the title', '6-10 h + 1-2 h PC (rough, not measured)', ['none'], '', False),
 ('21b', 'Settings option "Hair: cards / strands", near / far switch; the quality shows on the people everywhere they stand: the new-career gender screen, the creator, the wardrobe, the airport', '40 min', ['people'], '21a', False),
 ('21c', 'Moustaches and beard styles as a choice in the creator, put on the figure in front of you as you pick (and on the built people), at the same hair quality', '40 min', ['people'], '21b, 32b', False),
 ('32',  'New game: the "who are you" screen (she left / he right, neon tubes, floor fog, the chosen one steps into the light), hover + pad, confirm; then the other walks out of frame (her left, him right), the tubes fade and recede into the fog, camera still, UI on the free side', '2-3 h', ['people'], 'concept done', True),
 ('32a', 'Character creator: the parts library from Epic’s standard MetaHuman set (29 faces x skin-tone steps, bodies, the fits of 38 hairstyles, 18 brows, 16 beards, 16 moustaches; age-look steps if Unreal 5.7’s skin offers them - to check); the counts to be decided by the owner; time = builds x 4 min (measured)', 'not known until the counts are decided', ['people'], '2', False),
 ('32b', 'Character creator: the screen (face, skin, body, height, hair, beard, moustache, brows, hair colour and grey, freckles as their own choice, tattoos - our own skin layer and designs -, how old you look if supported, clothes), saved, a switch turns it on; fog clears round you at shoes; later a barber and a tattoo shop change hair/beard/tattoos and keep them in your save', '1.5 h', ['people'], '32, 32a, wardrobe', False),
 ('32c', 'New game: pets (pick one or none) and married or not, then the looks of the spouse (the creator again); then to the Overview', '40 min', ['people'], '32b', False),
 ('32d', 'Concept renders of every new-game step (creator, pets, spouse) in the neon style, for the owner to see first', '20 min', ['people'], '32', False),
 ('32e', 'New game economy: no demo content ever (kept outside as backup), difficulty as the last neon step, money per difficulty (hard: one used A320-class + crew + spare), crew always bought, one-aircraft starter warning', '3-4 h', ['people'], '32', True),
 ('32f', 'New game: Start game button, neon loading screen (animated bar, file names + percent, tips), no game loader or plaque after the title; into the Overview only when fully loaded', '2-3 h', ['people'], '32e', True),
 ('32g', 'New game: the airline founding (kind, name, ICAO code, callsign, your name; home base fixed at BIAP) as a neon step before the difficulty; the game form between Start game and the loading is gone', '2-3 h', ['people'], '32f', True),
 ('32h', 'New game: the loading screen also lists what Unreal itself loads (people, hair, terminal) with its real percentage', '20 min', ['people'], '32g', True),
 ('13a', 'Uniforms: cabin crew + pilots', '1 h', ['people'], '2, 3', False),
 ('13b', 'Uniforms: ground crew, security, fire crew', '1 h', ['people'], '13a', False),
 ('13c', 'Passengers: clothes variety', '1 h', ['people'], '13a', False),
 ('14a', 'Animations: walk / jog / sprint retargeted', '30 min', ['people'], '13a', False),
 ('14b', 'Animations: sit, lean, idle', '30 min', ['people'], '14a', False),
 ('30',  'Idle animation on people', '10 min', ['people'], '14b', False),
 ('15a', 'Smart objects: seats, walls, windows marked + the chooser', '40 min', ['people'], '14b', False),
 ('15b', 'Smart objects: sit / lean / stand at window in play', '40 min', ['people'], '15a', False),
 ('15c', 'Queues, families together, old people take seats, late sprint', '50 min', ['people'], '15b', False),
 ('15d', 'Bags, backpacks, wheeled suitcases: models + carrying', '1 h', ['people'], '15b', False),
 ('15e', 'Phone use: calls, scrolling, photos through the window', '40 min', ['people'], '15b', False),
 ('15f', 'Airside rule: nobody back out past security; leave by their flight', '20 min', ['people'], '15c', False),
 ('17',  'Talking faces: face animation from the audio', '1.5 h', ['people', 'voice'], '14b, 16', False),
 ('18a', 'Conversations: Qwen3-4B running locally, fed the game state', '40 min', ['voice'], '', False),
 ('18b', 'Conversations: our airport dialogue data + fine-tune', '1 h + PC training', ['voice'], '18a', False),
 ('18c', 'Conversations: E to talk, choice menus, subtitles', '40 min', ['people', 'voice'], '15b, 18a', False),
 ('18d', 'Conversations: Chatterbox voices streamed in the game', '40 min', ['voice'], '18c', False),
 ('18e', 'Conversations: morale, complaints, flight info hooks', '40 min', ['people', 'voice'], '18c', False),
 ('18f', 'Conversations: low-end fallback lines, Settings GPU / CPU / off', '20 min', ['voice'], '18d', False),
 ('19a', 'Academy: class routine, "class in session" sign, timetable', '40 min', ['people'], '15c', False),
 ('19b', 'Academy: knock, "come in", the teacher\'s reaction', '40 min', ['people', 'voice'], '19a, 18d, 20', False),
 ('9',   'Sunrise / sunset reflections on the tarmac', '1 h', ['field'], '8', True),
 ('10',  'Runway wear: skid marks, rubber, patches, cracks, stains', '40 min', ['field'], '8', True),
 ('11',  'Puddles (big ones too) + wet pavement after rain / hose (resend photos)', '2.5 h', ['field', 'gfx'], '9', True),
 ('22a', 'Cheap tier: ground decals (tyre marks, stains, patches)', '40 min', ['gfx'], '10', False),
 ('22b', 'Cheap tier: heat haze behind engines / APUs', '20 min', ['gfx'], '', False),
 ('22c', 'Cheap tier: lamp flares at night', '20 min', ['gfx'], '', False),
 ('22d', 'Cheap tier: virtual textures for big ground / building textures', '40 min', ['gfx'], '', False),
 ('23a', 'Advanced: hardware ray tracing for Lumen + its per-card setting', '40 min', ['gfx'], '22a', False),
 ('23b', 'Advanced: MegaLights + setting, tested with frame generation', '40 min', ['gfx'], '22c', False),
 ('23c', 'Advanced: Nanite ground displacement + setting', '40 min', ['gfx'], '22a', False),
 ('23d', 'Advanced: Substrate materials + setting (last)', '1 h', ['gfx'], '23a', False),
 ('25',  'Trim MetaHuman plugin weight; check update size', '20 min', ['rel'], '5', False),
 ('24a', 'Settings menu: frame, navigation, glass look', '40 min', ['gfx'], '', False),
 ('24b', 'Settings menu: Graphics page (all options known)', '40 min', ['gfx'], '24a, 23d', False),
 ('24c', 'Settings menu: Display, Sound, Controls', '30 min', ['gfx'], '24a', False),
 ('24d', 'Settings menu: Gameplay, Interface, Accessibility, Language', '30 min', ['gfx'], '24a', False),
 ('24e', 'Settings menu: Online, Saves and data; Save / Load / Export', '40 min', ['gfx'], '24a', False),
 ('24f', 'Settings menu: Android touch pass', '20 min', ['gfx'], '24e', False),
    # owner, 2026-11-13 (during the ground job)
    ('40a', 'Ground job: one master ground material, real CC0 scans, cracks + tar, tyre rubber, puddles, smooth taxiway fillets, photoreal edges, real grass', '8 h', ['field'], '', True),
    ('40b', 'Ground job: apron from the layout, ICAO/FAA markings (hold-short pattern A, red runway-ahead signs, stand lines), gate dirt, faint stand tyre marks', '2.5 h', ['field'], '40a', True),
    ('40c', 'Service roads: gate 1 to the control tower, the fire station to the taxiway by gate 1 and down to the taxiway below, vehicle markings, same detail', '2 h', ['field'], '40b', True),
    ('40d', 'Light snow layer on apron, taxiways, runway from winter reference photos', '2 h', ['field'], '40b', True),
    ('40e', 'Taxiway lights: blue edge + tiny inset green centre lights at real spacing, modelled fixtures (runway lights researched too)', '40 min', ['field'], '40a', False),
    ('40f', 'Graphics setting "Airport ground": Low 2K / Medium / High 4K / Ultra 8K + fine relief', '20 min', ['gfx'], '40a', False),
    ('41a', 'Fire truck: its paint on a master material with wear, dirt and wetness like the ground (its own red, not the markings red); compile + publish', '1.5 h', ['none'], '', True),
    ('41b', 'Fire truck: surfaces flickering through each other (z-fighting) - find and separate them, without changing the shape of the truck', '45 min', ['none'], '', True),
    ('41c', 'Fire truck lights on the truck itself (not the light cast on the ground): real warning lights instead of flat colour discs - LED segments behind a lens, bright core, flash patterns as on real trucks (reference photos/videos); the roof light bar the same, strong colours that flash and alternate in rhythm, the colours clearly visible', '1.5 h', ['none'], '41b', True),
    ('42', 'True Unreal build: CONVERT the Babylon-exported model (terminal building and fittings, rental garage) in Unreal - same geometry and textures, split into instanced Nanite pieces, materials on Unreal masters reusing the same textures, no tie to the Babylon export; rebuild only a piece too rough to convert, to the same shape; compare each with a Babylon render', '8 h', ['none'], '', True),
    ('43a', 'Jetways animated in Unreal (his docking: parked pulled half way back - wheels half as far out from the rotunda as docked, drives out to the front door when called, back after; stair, hoses, bellows, cab, canopy all moving with it)', '1.5 h', ['none'], '', False),
    ('43b', 'Stop bars: check every type nose-wheel setback and front-door position against the makers airport planning document; jetway docks per type', '20 min', ['field'], '43a', False),
    ('44', 'Weather-driven ground: snow builds up with how hard it snows and how cold it is (melts above 0), grass collecting more and more while the paving stays relatively free (ploughed); puddles from how much it rains - driven by the real-weather system', '50 min', ['field'], '', False),
    ('45', 'Per-stand wear settings: each stand in the layout file can set its own wear amount ("wear": 0.3 almost clean, 1.5 heavily used) and tyre marks ("tyres": 0 none, 1 strong); stands left unset keep their random look', '20 min', ['field'], '', True),
    ('46', 'Ground-equipment wear on the stands: rubber marks from pushback tugs and service trucks, tracks of the GPU (ground power unit), belt loaders and other ground equipment, where they really drive and park around the aircraft', '1 h', ['field'], '', True),
    ('47', 'Update notices kept apart: a PC update shows its news only in the PC version, an Android update only in the Android version (each reads its own version and release-notes files, never the other one)', '15 min', ['none'], '', False),
    ('48', 'AI drivers: NPC firefighters (and later other crews) walk to a vehicle, climb in with the same climb as yours (any MetaHuman at its own size), drive it along a route (fire station, apron, taxiways, runway) with steering, speed and stopping, and climb out at the scene', '1.5 h', ['field'], '', False),
    ('49', 'Shorts for your Tee outfit: the outfit holds only the tee, so the hips look see-through and cast no shadow; rebuild it with the kit shorts, body hidden only under them, whole shadow; front/back previews + in-game shot first; check the AI people in tees too (their fix rides the next people rebuild pass)', '20 min', ['none'], '', False),
    ('50', 'Small updates: binary patches inside each changed file (only the bytes that differ), the updater rebuilding the new file from the old one and checking it matches exactly; small changes tens of MB instead of 1.4 GB', '1 h', ['none'], '', True),
    ('51', 'FIRST AFTER THE TERMINAL: the update screen - its progress flashes wildly (fix); and its look exactly as the new-career steps (the neon look, its effects, glass and materials, not only the colours); then the small byte-level updates (50)', '30 min', ['rel'], '', True),
    ('52', 'Terminal advertising: huge LED advertisement screens inside the terminal where real airports put them (check-in hall, airside walkways, gate areas, baggage claim), moving adverts on bright LED panels, our own made-up brands', '1.5 h', ['gfx'], '', False),
    ('53', 'Tyre tracks off-road (question only, not ordered): deep-looking tracks behind the trucks on grass and soft ground, flattened grass, fading in time - decals 8-12 h, real ruts 16-24 h', '3-4 h', ['field'], '', False),
    ('54', 'Wash hose: a more powerful jet - about double the reach, so you can hit a truck without standing close; still cut off where it hits something (never through it)', '1 h', ['field'], '', True),
    ('55', 'Road network with MetaRoad Free (HANDOVER 3b, the owner map plans in Art/Maps): you add MetaRoad Free from Fab and install it to 5.7 first; licence + 5.7 check; then 10 parts with a stop after each - setup; forecourt lanes (wider, bus + taxi lanes); bays and car park [publish A]; terminal roundabout + road out both ends to 2 km; 2-5 km; 5-7.8 km + town roundabout [publish B]; main street; north loops; Torgvegur/Skolavegur/Sudurbogi; pavements, zebras, lights, plots [publish C]. Town buildings not built. Then scheduled vehicles (3 parts) and staff commutes (1)', '3 h left', ['field'], '', False),
    ('40g', 'Iceland scenery: mountains torn into walls and steps - rebuild the Iceland/Faroes height tiles with feathered edges where they meet the world map', '20 min + 35 min PC', ['field'], '', False),
    ('56', 'Your tablet, always with you: one button (a key on the keyboard; a pad button, or a combination if needed) brings it up into your hand - you see yourself take it out and hold it (first and third person), and on it the same as on your office PC and the operations room screens (the airline game, the ops monitor); the same button puts it away. Its own model from reference photos, instanced + Nanite', '1.5 h', ['none'], '', False),
    ('57', 'Self check-in kiosk rebuilt from the owner’s photos and close-ups (open sloping deck, wall on one side, photo textures, reflective screen, faces the entrances); the green lamp along the whole handle', '1 h', ['none'], '', True),
    ('58', 'Zinc standing-seam roof (owner’s texture) on top of the terminal’s wave roof and the car rental roof, true size, never stretched', '40 min', ['none'], '', True),
    ('59a', 'World terrain tiles rebuilt with the transpose fix (build only)', '35 min PC', ['field'], '', True),
    ('59b', 'World terrain packs uploaded (only when the owner says)', '1 h PC', ['rel'], '59a', False),
    ('60a', 'Land along the roads: our own Nanite ground in a band along the roads, the roads lying on it exactly at every distance; the globe’s picture on it, a flared rim, grass clumps; terrain detail locked at Normal', '2 h', ['field'], '', True),
    ('60b', 'Land along the roads: the pale streaks in the high view', '30 min', ['field'], '60a', False),
    ('60c', 'Land along the roads: stones along the edge (reference photos first)', '1 h', ['field'], '60a', False),
    ('61', 'Wash hose: jet runs straight about 28 m (lands about 37 m), more water left on the ground', '20 min', ['field'], '', True),
    ('62', 'New career loading screen: what Unreal loads (you, your hair, the terminal) with real percentages', '40 min', ['people'], '', True),
    ('63', 'Grass performance (done 2026-11-15): the cost was the fire station wash hose, simulating its cable against the world every frame while hidden on its hook (21-50 ms of the game thread, more on slow frames - the grass only looked like the cost); now simulated only while in a hand. The far grass also laid in distance bands (every 4 m to 1 km, 8 m at twice the size to 1.8 km, 12 m at three times beyond: the same ground covered from every height). Measured (-GrassPerf, same views, daytime): eye level 20.4 ms with grass, 19.2 without (49 against 52 fps; before: 15 fps); 450 m up 22.8 against 21.5 ms (44 against 47 fps; before: 26 fps)', 'done', ['field'], '', True),
    ('65', 'Sun shadows by what you do (done 2026-11-16): 2.6 km on foot or driving, 42 km flying / drone / flight viewer, switched on the fly', 'done', ['gfx'], '', True),
    ('66', 'Lamps by what you do (done 2026-11-16): 150 m on foot or driving (floodlights 600 m), 600 m flying (floodlights 2.4 km), switched on the fly', 'done', ['gfx'], '', True),
    # owner, 2026-11-16: share vehicles
    ('68a', 'Vehicle passengers: NPCs (and later other players) get in the same vehicle as you - climb in at a free seat with the same climb as yours, sit, ride, get out; you as passenger while they drive', '3 h', ['people'], '48', False),
    ('68b', 'Order an NPC: E at a person -> "Ride with me" (joins you as passenger) or "Drive me" (takes the wheel with you inside)', '2 h', ['people'], '68a', False),
    ('68c', 'NPC drives you: to a mission location or to your own waypoint set on the holographic map - route on the airfield and roads, steering, speed, stopping, parking at the end', '8-10 h', ['people', 'field'], '68b, 48', False),
    ('68d', 'Multiplayer in vehicles: other players as driver or passengers - needs network play first (not in the game yet), then seats shared between players', 'after network play', ['people'], '68a', False),
    ('67', 'PERF/LOOK, needs your OK: 12 Nanite meshes with see-through glass (bag-drop screens, stair balustrades, the gate walkway’s glass roof, the car rental) drawn the ordinary way so their glass shows as designed (Nanite can’t draw see-through parts)', '30 min', ['none'], '', False),
]
import os as _os
# (into docs/, wherever it is run from: it used to land in the current folder and docs/ kept an old copy, 2026-11-14)
OUT_PDF = _os.path.join(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))), 'docs', 'AirportDynastyPro-ToDo.pdf')
c = canvas.Canvas(OUT_PDF, pagesize=(W, H))
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
    c.drawString(8*mm, y-38, 'In build order. "needs": what must be done before.')
    c.setFillColor(FG); c.setFont('Helvetica-Bold', 7)
    c.drawString(8*mm, y-48, 'ALL TIMES ARE REAL CLOCK TIME: how long the job really takes, from how long')
    c.drawString(8*mm, y-57, 'such jobs have taken. "PC" = time the machine runs on its own.')
    c.drawString(8*mm, y-66, 'A green check mark = done.')
    c.setFont('Helvetica', 6.5); c.setFillColor(DIM)
    c.drawString(8*mm, y-76, 'Updated 2026-11-14 (evening).')
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
    if done:
        bx, by = x, y-rowh+3
        c.setFillColor(HexColor('#1f6f3a')); c.rect(bx, by, 9, 9, fill=1, stroke=0)
        c.setStrokeColor(HexColor('#7dffa8')); c.setLineWidth(1.6)
        p_ = c.beginPath(); p_.moveTo(bx+1.8, by+4.6); p_.lineTo(bx+3.8, by+2.2); p_.lineTo(bx+7.6, by+7.4); c.drawPath(p_, stroke=1, fill=0)
        c.setLineWidth(1)
    else:
        c.acroForm.checkbox(name='item_' + no, x=x, y=y-rowh+3, size=9, buttonStyle='check', borderWidth=1, borderColor=white, fillColor=HexColor('#2c2c33'), textColor=white, checked=False, forceBorder=True)
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
