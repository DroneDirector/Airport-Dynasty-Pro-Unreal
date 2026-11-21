# The cargo terminal's layout in one place: zones, every room and working area, the aisles, every door and the
# equipment, in metres. Used by make_cargo_terminal.py (the Blender model), make_cargo_blueprints.py (the plans) and
# finish_cargo_sheets.py (labels on the views), so all three always agree.
# Local frame: X along the building from its west end (plan x 200), Y from the airside face (plan y 1231, apron at -Y)
# to the landside face (Y = 70, truck court at +Y). Freight only - nothing here is for passengers.
#
# Sized to the rules that apply at the Swiss site (the airport is in Switzerland):
#   - escape routes, VKF fire-protection directive 16-15: at most 35 m to an exit, 50 m where two exits can be reached
#     (and in very high rooms with several escape directions); doors at least 0.9 m clear, corridors 1.2 m; in office
#     and industrial buildings one stair per 900 m2 of floor  -> escape doors every 30 m on both long faces and in the
#     fire walls, three stairs in the office block (2,100 m2 a floor)
#   - forklift traffic routes, Suva: load width + 2 x 0.5 m where people walk too, + 0.4 m for two-way traffic
#     -> 2 x 2.5 m + 1.0 + 0.4 = 6.4 m, so every main aisle and corridor is 6.5 m
#   - toilets near the workplaces, rest rooms, changing rooms (ArGV 3 Art. 29-33) -> a WC and break room in every hall
#   - air cargo security, EU 2015/1998 (applied in Switzerland): cargo screened (or from a known consignor) before it
#     enters the secure part; high-risk cargo gets extra screening -> a security line between acceptance and the rest
#   - border control post, EU 2019/1014 (applied in Switzerland): covered unloading, inspection rooms with hot and cold
#     water, chilled / frozen / ambient storage held at once, a lockable room for detained goods, animal products kept
#     apart from other goods; for animals separate housing for registered horses, other hoofed animals and other
#     animals, an inspection room with restraint equipment, feed / bedding / manure stores, changing room with
#     showers, a dedicated access lane, each unit one integrated block -> the BCP block at the east end
#   - dangerous goods, Swiss storage guide: storage classes kept apart, incompatible ones in separate fire compartments,
#     liquids over spill sumps, no floor drains -> one walled room per class group
#   - radioactive material in transit, IAEA SSR-6: groups of packages kept at least 6 m apart -> a two-bay store
#     at the far corner, away from rooms people work in

# #region ── Building and zones ──
L, D = 465.0, 70.0
H_MAIN, H_ETV, H_OFF, H_SPEC = 16.0, 24.0, 14.0, 12.0
BAY = 15.0                                  # column grid along the building (15 x 35 m)
OFF = (0.0, 30.0)                           # office block, 3 floors
EXP = (30.0, 135.0)                         # export
ETV = (135.0, 285.0)                        # high-bay ULD store worked by elevating transfer vehicles (ETV)
IMP = (285.0, 345.0)                        # import
SPC = (345.0, 405.0)                        # special cargo
BCP = (405.0, 465.0)                        # border control post (animals unit + animal products unit)
ZONES = [('off', OFF, H_OFF, 'Offices, 3 floors (14 m)'), ('exp', EXP, H_MAIN, 'Export (16 m)'),
         ('etv', ETV, H_ETV, 'High-bay ULD store (24 m)'), ('imp', IMP, H_MAIN, 'Import (16 m)'),
         ('spc', SPC, H_SPEC, 'Special cargo (12 m)'), ('bcp', BCP, H_SPEC, 'Border control post (12 m)')]
A1, A2 = (23.5, 30.0), (50.5, 57.0)         # the two 6.5 m aisles / corridors that run through every hall
AISLE_W = 6.5
CANOPY_D, CANOPY_H = 12.0, 9.0              # airside canopy over the ULD doors
YARD_D = 140.0                              # truck court depth (to the moved fence)
FIRE_WALLS = [(EXP[0], H_MAIN), (ETV[0], H_ETV), (ETV[1], H_ETV), (SPC[0], H_MAIN), (BCP[0], H_SPEC)]
FIRE_SHUTTERS = [EXP[0], ETV[0], ETV[1], SPC[0]]           # 6.5 m fire shutters at both aisles; none into the BCP
BCP_SPLIT = 435.0                                          # full-height wall between the BCP's two units
# #endregion

# #region ── Rooms and areas ──
# (name, x0, y0, x1, y1, zone, kind, doors, short)
#   kind: 'room' walled, 'cold' insulated with its own ceiling, 'dg' fire-rated (EI 90) store with a spill sump,
#   'stair' stair core, 'open' working area marked on the floor, 'aisle' kept-clear traffic lane.
#   doors: sides with a door (n = landside, s = airside, e, w); its width comes from door_width().
#   short: the label written in the area on the overall plan ('' = numbered there and listed in the schedule).
ROOMS = [
    # Office block, ground floor: a 4 m corridor down the middle, the entrance lobby and stair 1 on the landside
    ('Forklift workshop & battery charging', 0, 0, 13, 12, 'off', 'room', 'e', ''),
    ('Waste, recycling & packaging store', 0, 12, 13, 24, 'off', 'room', 'e', ''),
    ('Staff lockers, changing rooms & showers', 0, 24, 13, 36, 'off', 'room', 'e', ''),
    ('Stair 3', 0, 36, 13, 42, 'off', 'stair', 'e', ''),
    ("Truck drivers' lounge & WC", 0, 42, 13, 50, 'off', 'room', 'e', ''),
    ('Customs office', 0, 50, 13, 58, 'off', 'room', 'e', ''),
    ('Plant room & sprinkler pumps', 17, 0, 30, 12, 'off', 'room', 'w', ''),
    ('Stair 2', 17, 12, 30, 18, 'off', 'stair', 'w', ''),
    ('Server & IT room', 17, 18, 30, 24, 'off', 'room', 'w', ''),
    ('Staff security check (to airside)', 17, 24, 30, 36, 'off', 'room', 'w', ''),
    ('Import collection counter', 17, 36, 30, 46, 'off', 'room', 'w', ''),
    ('Export acceptance & documents counter', 17, 46, 30, 58, 'off', 'room', 'w', ''),
    ('Corridor', 13, 0, 17, 58, 'off', 'aisle', '', ''),
    ('Stair 1 & lifts', 0, 58, 10, 70, 'off', 'stair', 'e', ''),
    ('Entrance lobby', 10, 58, 30, 70, 'off', 'open', '', 'LOBBY'),
    # Export: acceptance is unscreened; everything south of the security line is secure
    ('Truck unloading, weigh & measure', 30, 57, 135, 70, 'exp', 'open', '', 'TRUCK UNLOADING, WEIGH & MEASURE'),
    ('Acceptance aisle 6.5 m (unscreened)', 30, 50.5, 135, 57, 'exp', 'aisle', '', ''),
    ('Known-consignor intake (secure cargo)', 30, 30, 42, 50.5, 'exp', 'room', 'ns', ''),
    ('Security screening hall (X-ray, EDS, trace)', 42, 30, 74, 50.5, 'exp', 'room', 'ns', 'SECURITY\nSCREENING'),
    ('High-risk cargo screening (HRCM)', 74, 30, 82, 50.5, 'exp', 'room', 'ns', ''),
    ('Export bulk store (pallet racks)', 82, 30, 108, 50.5, 'exp', 'open', '', 'BULK\nRACKS'),
    ('Staff WC & break room (export)', 108, 30, 116, 50.5, 'exp', 'room', 's', ''),
    ('Mail & express (own screening)', 116, 30, 135, 50.5, 'exp', 'room', 'ns', 'MAIL &\nEXPRESS'),
    ('Forklift aisle 6.5 m', 30, 23.5, 135, 30, 'exp', 'aisle', '', ''),
    ('ULD build-up, 8 stations with scales', 30, 6, 135, 23.5, 'exp', 'open', '', 'ULD BUILD-UP  ·  8 stations'),
    ('ULD transfer lane (roller deck)', 30, 0, 345, 6, 'exp', 'aisle', '', ''),
    # High-bay ULD store
    ('ULD roller-bed buffer', 135, 6, 285, 23.5, 'etv', 'open', '', 'ULD ROLLER-BED BUFFER'),
    ('Forklift lane 6.5 m', 135, 23.5, 285, 30, 'etv', 'aisle', '', ''),
    ('ULD racks, 4 levels', 135, 30, 285, 37.5, 'etv', 'open', '', 'ULD RACKS  ·  4 levels'),
    ('ETV aisle', 135, 37.5, 285, 43, 'etv', 'aisle', '', 'ETV AISLE  ·  2 ETVs  ·  360 ULD positions'),
    ('ULD racks, 4 levels', 135, 43, 285, 50.5, 'etv', 'open', '', 'ULD RACKS  ·  4 levels'),
    ('Forklift lane 6.5 m', 135, 50.5, 285, 57, 'etv', 'aisle', '', ''),
    ('ULD truck docks (road feeder service)', 135, 57, 285, 70, 'etv', 'open', '', 'ULD truck docks (road feeder service)'),
    # Import
    ('ULD break-down, 4 stations', 285, 6, 345, 23.5, 'imp', 'open', '', 'ULD BREAK-DOWN'),
    ('Forklift aisle 6.5 m', 285, 23.5, 345, 30, 'imp', 'aisle', '', ''),
    ('Import bulk store (pallet racks)', 285, 30, 305, 50.5, 'imp', 'open', '', 'BULK\nRACKS'),
    ('Staff WC & break room (import)', 305, 30, 313, 50.5, 'imp', 'room', 's', ''),
    ('Customs inspection', 313, 30, 329, 50.5, 'imp', 'room', 'ns', ''),
    ('Bonded store (caged)', 329, 30, 345, 50.5, 'imp', 'room', 'ns', ''),
    ('Forklift aisle 6.5 m', 285, 50.5, 345, 57, 'imp', 'aisle', '', ''),
    ('Import release & truck loading', 285, 57, 345, 70, 'imp', 'open', '', 'IMPORT RELEASE'),
    # Special cargo: cold rooms open straight onto the apron; dangerous goods one room per class group
    ('Perishables & fresh fish 2-4°C', 345, 0, 363, 23.5, 'spc', 'cold', 'sn', ''),
    ('Freezer -20°C', 363, 0, 375, 23.5, 'spc', 'cold', 'sn', ''),
    ('Pharma cool room 2-8°C', 375, 0, 390, 23.5, 'spc', 'cold', 'sn', ''),
    ('Pharma room 15-25°C', 390, 0, 405, 23.5, 'spc', 'cold', 'sn', ''),
    ('Corridor 6.5 m', 345, 23.5, 405, 30, 'spc', 'aisle', '', ''),
    ('Staff WC & break room (special)', 345, 30, 353, 40, 'spc', 'room', 's', ''),
    ('Valuables strong room', 353, 30, 363, 40, 'spc', 'room', 's', ''),
    ('Vulnerable cargo cage', 363, 30, 371, 40, 'spc', 'room', 's', ''),
    ('Human remains room', 371, 30, 379, 40, 'spc', 'room', 's', ''),
    ('Oversize X-ray', 379, 30, 391, 40, 'spc', 'room', 's', ''),
    ('Damaged cargo & repacking', 391, 30, 405, 40, 'spc', 'room', 's', ''),
    ('DG class 3 flammable liquids', 345, 40, 354, 50.5, 'spc', 'dg', 'n', ''),
    ('DG class 2 gases', 354, 40, 360, 50.5, 'spc', 'dg', 'n', ''),
    ('DG class 4 flammable solids', 360, 40, 366, 50.5, 'spc', 'dg', 'n', ''),
    ('DG class 5 oxidizers & peroxides', 366, 40, 372, 50.5, 'spc', 'dg', 'n', ''),
    ('DG class 6 toxic & infectious', 372, 40, 378, 50.5, 'spc', 'dg', 'n', ''),
    ('DG class 8 corrosive', 378, 40, 384, 50.5, 'spc', 'dg', 'n', ''),
    ('DG class 9 lithium batteries', 384, 40, 392, 50.5, 'spc', 'dg', 'n', ''),
    ('Radioactive store (class 7)', 392, 40, 405, 50.5, 'spc', 'dg', 'n', ''),
    ('Corridor 6.5 m', 345, 50.5, 405, 57, 'spc', 'aisle', '', ''),
    ('Cool-chain & special docks', 345, 57, 405, 70, 'spc', 'open', '', 'COOL-CHAIN &\nSPECIAL DOCKS'),
    # Border control post: animals unit (west) and animal products unit (east), each one integrated block
    ('Horse stalls (registered horses), 8 x 20 m²', 405, 0, 423, 23.5, 'bcp', 'room', 'n', ''),
    ('Other hoofed animals (cattle, zoo), 4 stalls', 423, 0, 435, 23.5, 'bcp', 'room', 'n', ''),
    ('Animal corridor 6.5 m', 405, 23.5, 435, 30, 'bcp', 'aisle', '', ''),
    ('Pets & small animals (kennels, cattery, aviary)', 405, 30, 420, 40, 'bcp', 'room', 's', ''),
    ('Clinical inspection & restraint', 420, 30, 435, 40, 'bcp', 'room', 's', ''),
    ('Feed, bedding & litter store', 405, 40, 413, 50.5, 'bcp', 'room', 'n', ''),
    ('Manure & waste store', 413, 40, 420, 50.5, 'bcp', 'room', 'n', ''),
    ('Vet staff changing rooms & showers', 420, 40, 435, 50.5, 'bcp', 'room', 'n', ''),
    ('Animal corridor 6.5 m', 405, 50.5, 435, 57, 'bcp', 'aisle', '', ''),
    ('Animal unloading (covered, own lane)', 405, 57, 435, 70, 'bcp', 'open', '', 'ANIMAL\nUNLOADING'),
    ('Animal products chilled 0-4°C', 435, 0, 450, 23.5, 'bcp', 'cold', 'sn', ''),
    ('Animal products frozen -18°C', 450, 0, 465, 23.5, 'bcp', 'cold', 'sn', ''),
    ('Corridor 6.5 m', 435, 23.5, 465, 30, 'bcp', 'aisle', '', ''),
    ('Animal products ambient store', 435, 30, 450, 40, 'bcp', 'room', 's', ''),
    ('Detained goods (lockable)', 450, 30, 465, 40, 'bcp', 'room', 's', ''),
    ('Inspection room (cooled)', 435, 40, 447, 50.5, 'bcp', 'room', 'n', ''),
    ('Vet office & documents', 447, 40, 455, 50.5, 'bcp', 'room', 'n', ''),
    ('Changing rooms & WC (products)', 455, 40, 465, 50.5, 'bcp', 'room', 'n', ''),
    ('Corridor 6.5 m', 435, 50.5, 465, 57, 'bcp', 'aisle', '', ''),
    ('Covered unloading, animal products', 435, 57, 465, 70, 'bcp', 'open', '', 'PRODUCTS\nUNLOADING'),
]
NUMBERED = [r for r in ROOMS if r[6] != 'aisle' and not r[8]]
NUM = {id(r): i + 1 for i, r in enumerate(NUMBERED)}
WALLED = ('room', 'cold', 'dg', 'stair')

# Office block upper floors (same 4 m corridor, three stairs). Floor 1 at 4.6 m, floor 2 at 9.2 m.
UPPER = {
    1: [('Staff canteen (apron view)', 0, 0, 30, 12, 'room', 'n'),
        ('Ground handler offices', 0, 12, 13, 24, 'room', 'e'), ("Forwarders' offices", 0, 24, 13, 36, 'room', 'e'),
        ('Stair 3', 0, 36, 13, 42, 'stair', 'e'), ('Training room', 0, 42, 13, 58, 'room', 'e'),
        ('Stair 2', 17, 12, 30, 18, 'stair', 'w'), ('Airline offices', 17, 18, 30, 30, 'room', 'w'),
        ('Meeting rooms', 17, 30, 30, 44, 'room', 'w'), ('WC', 17, 44, 30, 52, 'room', 'w'),
        ('First aid & rest room', 17, 52, 30, 58, 'room', 'w'),
        ('Corridor', 13, 12, 17, 58, 'aisle', ''), ('Stair 1, lifts & WC', 0, 58, 30, 70, 'stair', 's')],
    2: [('Cargo operations control centre (glazed, over the apron)', 0, 0, 30, 12, 'room', 'n'),
        ('Load control & ULD control', 0, 12, 13, 24, 'room', 'e'), ('Customs & border offices', 0, 24, 13, 36, 'room', 'e'),
        ('Stair 3', 0, 36, 13, 42, 'stair', 'e'), ('Management', 0, 42, 13, 58, 'room', 'e'),
        ('Stair 2', 17, 12, 30, 18, 'stair', 'w'), ('Security & CCTV room', 17, 18, 30, 30, 'room', 'w'),
        ('IT & print room', 17, 30, 30, 44, 'room', 'w'), ('WC', 17, 44, 30, 52, 'room', 'w'),
        ('Meeting room', 17, 52, 30, 58, 'room', 'w'),
        ('Corridor', 13, 12, 17, 58, 'aisle', ''), ('Stair 1, lifts & WC', 0, 58, 30, 70, 'stair', 's')],
}
FLOOR_Z = {1: 4.6, 2: 9.2}
STAIRS = [(0, 58, 10, 70), (17, 12, 30, 18), (0, 36, 13, 42)]      # the three stair cores through all floors
# #endregion

# #region ── Doors ──
PERSON_WORDS = ('WC', 'office', 'Office', 'changing', 'Changing', 'lounge', 'counter', 'Stair', 'Server', 'check',
                'lockers', 'Plant', 'Waste', 'workshop')


def door_width(name, zone, kind):
    """Clear width of a room's door: 1.0 m personnel doors (offices, staff rooms, stairs), 3 m rapid doors for cold
    rooms and dangerous goods, 4 m sectional doors for forklift rooms, 4.5 m for the horse and cattle rooms."""
    if zone == 'off' or kind == 'stair' or any(w in name for w in PERSON_WORDS): return 1.0
    if kind in ('cold', 'dg'): return 3.0
    if 'Horse' in name or 'hoofed' in name: return 4.5
    return 4.0


AIR_DOORS = [EXP[0] + 7.5 + i * BAY for i in range(21)]          # 6 m wide ULD doors, 37.5 ... 337.5
SPC_AIR_DOORS = [354.0, 369.0, 382.5, 397.5, 442.5, 457.5]       # 4 m: cold rooms straight to the aircraft
HORSE_AIR_DOORS = [414.0, 429.0]                                 # 5 m: horse containers and animal crates
DOCKS = [EXP[0] + 6 + i * 13.0 for i in range(24)]               # truck docks 36 ... 335, 3 x 3.5 m doors at 1.2 m
SPC_DOCKS = [352.0, 362.0, 372.0, 382.0, 425.0, 441.0, 451.0, 460.0]   # sealed cool-chain and BCP docks
DRIVE_IN = [398.0, 412.0]                                        # 5 m drive-in: armoured vans, hearses; horse lorries
ESC_AIR = [15.0] + [45.0 + 30 * i for i in range(10)]            # escape doors 1.0 x 2.1 m, airside face
ESC_LAND = [42.5 + 26 * i for i in range(12)] + [390.0, 431.0, 446.0]   # landside face
ESC_EAST = [26.75, 53.75]                                        # east end, at the corridors
FW_DOORS = [(x, y) for x in (EXP[0], ETV[0], ETV[1]) for y in (12.0, 44.0)] + [(BCP[0], 26.75), (BCP[0], 53.75)]
SECURITY_LINE = (EXP[0], EXP[1], A2[0])                         # x from, x to, y: unscreened to the north of it
SECURITY_FENCE = [(82.0, 108.0)]                                 # mesh fence where no room wall forms the line
# #endregion

# #region ── Equipment (drawn on the plans and modelled) ──
BUILDUP = [EXP[0] + (EXP[1] - EXP[0]) / 8 * (i + 0.5) for i in range(8)]       # build-up station centres
BREAKDOWN = [IMP[0] + 7.5 + i * 15 for i in range(4)]                         # break-down station centres
STATION = (3.4, 8.0, 16.0)               # roller-bed workstation: width, y from, y to (ULD + scale behind)
RACK_ROWS = {                             # pallet rack rows, 2.4 m double rows with 3.6 m narrow aisles between
    'exp': (83.0, 107.0, [31.0, 37.0, 43.0]),
    'imp': (286.0, 304.0, [31.0, 37.0, 43.0])}
XRAY = [(45, 33, 48, 39), (45, 42, 48, 48), (52, 33, 55, 39), (52, 42, 55, 48), (60, 34, 70, 47)]  # 4 X-ray + 1 EDS
SCALES = [(36 + i * 9.0, 60.5, 40 + i * 9.0, 64.5) for i in range(11)]            # floor scales in the acceptance area
ETV_RACK_X = [ETV[0] + 1.5 + i * 3.3 for i in range(45)]   # 45 ULD positions a row, 4 levels, both sides = 360
ETV_RACKS = [(30.0, 37.5), (43.0, 50.5)]
ETV_LEVELS = [0.0, 5.4, 10.8, 16.2]
ETV_AISLE = (37.5, 43.0)
ETV_MACHINES = [175.0, 245.0]
TRANSFER = (0.8, 5.2)                     # roller-deck transfer lane along the airside wall (y range)
HORSE_STALLS = [(405.5 + i * 4.25, 0.5, 405.5 + (i + 1) * 4.25 - 0.2, 5.2) for i in range(4)] + \
               [(405.5 + i * 4.25, 17.8, 405.5 + (i + 1) * 4.25 - 0.2, 23.0) for i in range(4)]   # 4.05 x 4.7 m
CATTLE_STALLS = [(423.5, 0.5 + i * 5.6, 434.5, 5.9 + i * 5.6) for i in range(4)]
RADIO_BAYS = [(393.0, 41.0, 397.0, 45.0), (400.0, 44.5, 404.0, 49.5)]               # >= 6 m between the groups
OUTSIDE = [('Empty ULD store', 470, -36, 512, -16), ('ULD repair shed', 470, -14, 492, -2),
           ('Dolly & GSE parking + charging', -34, -36, -4, -6)]
BCP_LANE = (405.0, 465.0)                 # dedicated access lane in the truck court
# #endregion


# #region ── Partition walls and their doors from rooms (shared sides merged, door openings cut) ──
def _sides(x0, y0, x1, y1):
    return (('s', ('h', y0), x0, x1), ('n', ('h', y1), x0, x1), ('w', ('v', x0), y0, y1), ('e', ('v', x1), y0, y1))


def walls_from_rooms(rooms, z0, skip_x=(), skip_y=(), door_w=4.0, t=0.25):
    """Partition walls for a set of rooms: shared sides merged into one wall, an opening in the middle of every side
    listed in the room's doors, sides on the outer walls or fire walls left out (those exist already).
    rooms: (x0, y0, x1, y1, height, doors[, door width]). Returns boxes (x0, y0, z0, x1, y1, z1)."""
    seg, gaps, hgt = {}, {}, {}
    for r in rooms:
        x0, y0, x1, y1, h, doors = r[:6]; dw = r[6] if len(r) > 6 else door_w
        for side, key, a, b in _sides(x0, y0, x1, y1):
            if key[0] == 'h' and key[1] in skip_y: continue
            if key[0] == 'v' and key[1] in skip_x: continue
            seg.setdefault(key, []).append((a, b)); hgt[key] = max(hgt.get(key, 0), h)
            if side in doors:
                m = (a + b) / 2; gaps.setdefault(key, []).append((m - dw / 2, m + dw / 2))
    out = []
    for key, iv in seg.items():
        iv.sort(); merged = [list(iv[0])]
        for a, b in iv[1:]:
            if a <= merged[-1][1] + 1e-6: merged[-1][1] = max(merged[-1][1], b)
            else: merged.append([a, b])
        for g0, g1 in gaps.get(key, []):               # cut the openings
            nm = []
            for a, b in merged:
                if g1 <= a or g0 >= b: nm.append([a, b]); continue
                if g0 > a: nm.append([a, g0])
                if g1 < b: nm.append([g1, b])
            merged = nm
        for a, b in merged:
            if b - a < 0.3: continue
            if key[0] == 'h': out.append((a, key[1] - t / 2, z0, b, key[1] + t / 2, z0 + hgt[key]))
            else: out.append((key[1] - t / 2, a, z0, key[1] + t / 2, b, z0 + hgt[key]))
    return out


def doors_from_rooms(rooms, skip_x=(), skip_y=()):
    """Every room door as (centre x, centre y, 'h' or 'v' wall, width), shared doors listed once."""
    seen = {}
    for r in rooms:
        x0, y0, x1, y1, h, doors = r[:6]; dw = r[6] if len(r) > 6 else 4.0
        for side, key, a, b in _sides(x0, y0, x1, y1):
            if side not in doors: continue
            if key[0] == 'h' and key[1] in skip_y: continue
            if key[0] == 'v' and key[1] in skip_x: continue
            m = (a + b) / 2
            c = (m, key[1]) if key[0] == 'h' else (key[1], m)
            k = (round(c[0], 2), round(c[1], 2))
            seen[k] = (c[0], c[1], key[0], max(dw, seen.get(k, (0, 0, '', 0))[3]))
    return list(seen.values())


def ground_rooms():
    """The ground floor's walled rooms as (x0, y0, x1, y1, height, doors, door width)."""
    out = []
    for name, x0, y0, x1, y1, zk, kind, doors, short in ROOMS:
        if kind not in WALLED: continue
        h = 4.4 if zk == 'off' else {'cold': 7.0, 'dg': 6.0}.get(kind, 5.0)
        out.append((x0, y0, x1, y1, h, doors, door_width(name, zk, kind)))
    return out


SKIP_X = tuple([0.0, L] + [x for x, _ in FIRE_WALLS] + [BCP_SPLIT])
SKIP_Y = (0.0, D)
# #endregion
