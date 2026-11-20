# The cargo terminal's layout in one place: zones, every room and working area, the aisles and the equipment, in
# metres. Used by make_cargo_terminal.py (the Blender model), make_cargo_blueprints.py (the plans) and
# finish_cargo_sheets.py (labels on the views), so all three always agree.
# Local frame: X along the building from its west end (plan x 200), Y from the airside face (plan y 1231, apron at -Y)
# to the landside face (Y = 70, truck court at +Y). Freight only - nothing here is for passengers.

# #region ── Building and zones ──
L, D = 390.0, 70.0
H_MAIN, H_ETV, H_OFF, H_SPEC = 16.0, 24.0, 14.0, 12.0
BAY = 15.0                                  # column grid along the building (15 x 35 m)
OFF = (0.0, 30.0)                           # office block, 3 floors
EXP = (30.0, 135.0)                         # export
ETV = (135.0, 285.0)                        # high-bay ULD store worked by elevating transfer vehicles (ETV)
IMP = (285.0, 345.0)                        # import
SPC = (345.0, 390.0)                        # special cargo
ZONES = [('off', OFF, H_OFF, 'Offices, 3 floors (14 m)'), ('exp', EXP, H_MAIN, 'Export (16 m)'),
         ('etv', ETV, H_ETV, 'High-bay ULD store (24 m)'), ('imp', IMP, H_MAIN, 'Import (16 m)'),
         ('spc', SPC, H_SPEC, 'Special cargo (12 m)')]
CANOPY_D, CANOPY_H = 12.0, 9.0              # airside canopy over the ULD doors
YARD_D = 140.0                              # truck court depth (to the moved fence)
AIR_DOORS = [EXP[0] + 7.5 + i * BAY for i in range(21)]          # 6 m wide ULD doors, 37.5 ... 337.5
SPC_AIR_DOORS = [352.5, 367.5, 382.5]                            # animals, perishables, frozen straight to the aircraft
DOCKS = [EXP[0] + 6 + i * 13.0 for i in range(24)]               # truck docks 36 ... 335, 3 x 3.5 m doors at 1.2 m
SPC_DOCKS = [352.0, 362.0, 372.0]                                # sealed cool-chain docks
DRIVE_IN = [383.0]                                               # drive-in door: armoured vans, animal transporters
# #endregion

# #region ── Rooms and areas ──
# (name, x0, y0, x1, y1, zone, kind, doors, short)
#   kind: 'room' walled 5 m partitions, 'cold' insulated room with its own ceiling, 'open' working area marked on the
#   floor, 'aisle' kept-clear traffic lane.  doors: sides with a 4 m opening (n = landside, s = airside, e, w).
#   short: the label written in the room on the overall plan ('' = numbered there and listed).
ROOMS = [
    # Office block, ground floor: a 4 m corridor down the middle, the entrance lobby on the landside
    ('Forklift workshop & battery charging', 0, 0, 13, 12, 'off', 'room', 'se', ''),
    ('Waste, recycling & packaging store', 0, 12, 13, 24, 'off', 'room', 'e', ''),
    ('Staff lockers & changing rooms', 0, 24, 13, 36, 'off', 'room', 'e', ''),
    ("Truck drivers' lounge & WC", 0, 36, 13, 46, 'off', 'room', 'e', ''),
    ('Customs office', 0, 46, 13, 58, 'off', 'room', 'e', ''),
    ('Plant room & sprinkler pumps', 17, 0, 30, 12, 'off', 'room', 'w', ''),
    ('Server & IT room', 17, 12, 30, 24, 'off', 'room', 'w', ''),
    ('Staff security check (to airside)', 17, 24, 30, 36, 'off', 'room', 'we', ''),
    ('Import collection counter', 17, 36, 30, 46, 'off', 'room', 'w', ''),
    ('Export acceptance & documents counter', 17, 46, 30, 58, 'off', 'room', 'w', ''),
    ('Corridor', 13, 0, 17, 58, 'off', 'aisle', '', ''),
    ('Entrance lobby', 0, 58, 30, 70, 'off', 'open', '', 'LOBBY'),
    # Export
    ('ULD transfer lane (roller deck)', 30, 0, 345, 8, 'exp', 'aisle', '', ''),
    ('Truck unloading & acceptance', 30, 58, 135, 70, 'exp', 'open', '', 'TRUCK UNLOADING & ACCEPTANCE'),
    ('Forklift aisle 6 m', 30, 52, 135, 58, 'exp', 'aisle', '', ''),
    ('Weigh & measure (floor scales, dimensioner)', 30, 30, 46, 52, 'exp', 'open', '', ''),
    ('Security screening (X-ray, EDS, trace)', 46, 30, 78, 52, 'exp', 'room', 'ns', 'SECURITY\nSCREENING'),
    ('Export bulk store (pallet racks)', 78, 30, 112, 52, 'exp', 'open', '', 'BULK\nRACKS'),
    ('Mail & express', 112, 30, 135, 52, 'exp', 'room', 'ns', 'MAIL &\nEXPRESS'),
    ('Forklift aisle 6 m', 30, 24, 135, 30, 'exp', 'aisle', '', ''),
    ('ULD build-up, 8 stations with scales', 30, 8, 135, 24, 'exp', 'open', '', 'ULD BUILD-UP  ·  8 stations'),
    # High-bay ULD store
    ('ULD roller-bed buffer', 135, 48, 285, 62, 'etv', 'open', '', 'ULD ROLLER-BED BUFFER'),
    ('ULD racks, 4 levels', 135, 40, 285, 48, 'etv', 'open', '', 'ULD RACKS  ·  4 levels'),
    ('ETV aisle', 135, 30, 285, 40, 'etv', 'aisle', '', 'ETV AISLE  ·  2 ETVs  ·  360 ULD positions'),
    ('ULD racks, 4 levels', 135, 22, 285, 30, 'etv', 'open', '', 'ULD RACKS  ·  4 levels'),
    ('ULD roller-bed buffer', 135, 8, 285, 22, 'etv', 'open', '', 'ULD ROLLER-BED BUFFER'),
    ('ULD truck docks (road feeder)', 135, 62, 285, 70, 'etv', 'open', '', 'ULD truck docks (road feeder service)'),
    # Import
    ('ULD break-down, 4 stations', 285, 8, 345, 24, 'imp', 'open', '', 'ULD BREAK-DOWN'),
    ('Forklift aisle 6 m', 285, 24, 345, 30, 'imp', 'aisle', '', ''),
    ('Import bulk store (pallet racks)', 285, 30, 313, 52, 'imp', 'open', '', 'BULK\nRACKS'),
    ('Customs inspection', 313, 30, 329, 52, 'imp', 'room', 'ns', ''),
    ('Bonded store (caged)', 329, 30, 345, 52, 'imp', 'room', 'ns', ''),
    ('Forklift aisle 6 m', 285, 52, 345, 58, 'imp', 'aisle', '', ''),
    ('Import release & truck loading', 285, 58, 345, 70, 'imp', 'open', '', 'IMPORT RELEASE'),
    # Special cargo: rooms off two corridors, the airside row opening straight onto the apron
    ('Live animals (AVI)', 345, 0, 360, 13, 'spc', 'room', 'sn', ''),
    ('Perishables & fresh fish 2-4°C', 360, 0, 375, 13, 'spc', 'cold', 'sn', ''),
    ('Freezer -20°C', 375, 0, 390, 13, 'spc', 'cold', 'sn', ''),
    ('Cool corridor 4 m', 345, 13, 390, 17, 'spc', 'aisle', '', ''),
    ('Vet & border control inspection', 345, 17, 360, 28, 'spc', 'room', 'sn', ''),
    ('Pharma cool room 2-8°C', 360, 17, 375, 28, 'spc', 'cold', 'sn', ''),
    ('Pharma room 15-25°C', 375, 17, 390, 28, 'spc', 'cold', 'sn', ''),
    ('Corridor 6 m', 345, 28, 390, 34, 'spc', 'aisle', '', ''),
    ('Dangerous goods store (classes apart)', 345, 34, 357, 46, 'spc', 'room', 's', ''),
    ('Radioactive store', 357, 34, 364, 46, 'spc', 'room', 's', ''),
    ('Valuables strong room', 364, 34, 377, 46, 'spc', 'room', 's', ''),
    ('Human remains room', 377, 34, 390, 46, 'spc', 'room', 's', ''),
    ('Damaged cargo & repacking', 345, 46, 360, 56, 'spc', 'room', 'n', ''),
    ('Vulnerable cargo cage', 360, 46, 375, 56, 'spc', 'room', 'n', ''),
    ('Oversize X-ray', 375, 46, 390, 56, 'spc', 'room', 'n', ''),
    ('Cool-chain docks (sealed)', 345, 56, 390, 70, 'spc', 'open', '', 'COOL-CHAIN\nDOCKS'),
]
# numbers for the rooms that are too small to name on the overall plan
NUMBERED = [r for r in ROOMS if r[6] != 'aisle' and not r[8]]
NUM = {id(r): i + 1 for i, r in enumerate(NUMBERED)}

# Office block upper floors (same 4 m corridor). Floor 1 at 4.6 m, floor 2 at 9.2 m.
UPPER = {
    1: [('Staff canteen (apron view)', 0, 0, 30, 14, 'room', 'n'),
        ('Ground handler offices', 0, 14, 13, 30, 'room', 'e'), ('Airline offices', 17, 14, 30, 30, 'room', 'w'),
        ("Forwarders' offices", 0, 30, 13, 44, 'room', 'e'), ('Meeting rooms', 17, 30, 30, 44, 'room', 'w'),
        ('Training room', 0, 44, 13, 58, 'room', 'e'), ('First aid & rest room', 17, 44, 30, 58, 'room', 'w'),
        ('Corridor', 13, 14, 17, 58, 'aisle', ''), ('Stairs, lifts & WC', 0, 58, 30, 70, 'room', 's')],
    2: [('Cargo operations control centre (glazed, over the apron)', 0, 0, 30, 14, 'room', 'n'),
        ('Load control & ULD control', 0, 14, 13, 30, 'room', 'e'), ('Security & CCTV room', 17, 14, 30, 30, 'room', 'w'),
        ('Customs & border offices', 0, 30, 13, 44, 'room', 'e'), ('IT & print room', 17, 30, 30, 44, 'room', 'w'),
        ('Management', 0, 44, 13, 58, 'room', 'e'), ('Meeting room', 17, 44, 30, 58, 'room', 'w'),
        ('Corridor', 13, 14, 17, 58, 'aisle', ''), ('Stairs, lifts & WC', 0, 58, 30, 70, 'room', 's')],
}
FLOOR_Z = {1: 4.6, 2: 9.2}
# #endregion

# #region ── Equipment (drawn on the plans and modelled) ──
BUILDUP = [EXP[0] + (EXP[1] - EXP[0]) / 8 * (i + 0.5) for i in range(8)]       # build-up station centres
BREAKDOWN = [IMP[0] + 7.5 + i * 15 for i in range(4)]                         # break-down station centres
STATION = (3.4, 9.5, 17.5)               # roller-bed workstation: width, y from, y to (ULD + scale)
RACK_ROWS = {                             # pallet rack rows, 2.4 m double rows with 3.6 m narrow aisles between
    'exp': (79.0, 111.0, [31.0, 37.0, 43.0, 49.0]),
    'imp': (286.0, 312.0, [31.0, 37.0, 43.0, 49.0])}
XRAY = [(49, 33, 52, 39), (49, 43, 52, 49), (56, 33, 59, 39), (56, 43, 59, 49), (64, 34, 72, 48)]  # 4 X-ray + 1 EDS
SCALES = [(33, 33, 37, 37), (39, 33, 43, 37), (33, 41, 37, 45), (39, 41, 43, 45)]
ETV_RACK_X = [ETV[0] + 1.5 + i * 3.3 for i in range(45)]   # 45 ULD positions a row, 4 levels, both sides = 360
ETV_LEVELS = [0.0, 5.4, 10.8, 16.2]
ETV_MACHINES = [175.0, 245.0]
TRANSFER = (1.5, 6.0)                     # roller-deck transfer lane along the airside wall (y range)
# outside, airside: empty ULD store and the ULD repair shed; GSE and dolly parking at the west end
OUTSIDE = [('Empty ULD store', 404, -36, 446, -16), ('ULD repair shed', 404, -14, 426, -2),
           ('Dolly & GSE parking + charging', -34, -36, -4, -6)]
# #endregion


# #region ── Partition walls from rooms (shared sides merged, door openings cut) ──
def walls_from_rooms(rooms, z0, skip_x=(), skip_y=(), door_w=4.0, t=0.25):
    """Partition walls for a set of rooms: shared sides merged into one wall, a door_w opening in the middle of every
    side listed in the room's doors, sides on the outer walls or fire walls left out (those exist already).
    rooms: (x0, y0, x1, y1, height, doors). Returns lists of (x0, y0, z0, x1, y1, z1) per material key."""
    seg, gaps, hgt = {}, {}, {}
    for x0, y0, x1, y1, h, doors in rooms:
        for side, key, a, b in (('s', ('h', y0), x0, x1), ('n', ('h', y1), x0, x1),
                                ('w', ('v', x0), y0, y1), ('e', ('v', x1), y0, y1)):
            if key[0] == 'h' and key[1] in skip_y: continue
            if key[0] == 'v' and key[1] in skip_x: continue
            seg.setdefault(key, []).append((a, b)); hgt[key] = max(hgt.get(key, 0), h)
            if side in doors:
                m = (a + b) / 2; gaps.setdefault(key, []).append((m - door_w / 2, m + door_w / 2))
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
# #endregion
