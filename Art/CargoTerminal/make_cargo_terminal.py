# The cargo terminal (freight only, no passengers), built in Blender to the airfield plan's footprint
# (Data/airfield.json cargoBuilding: plan x 200..590, y 1231..1301 = 390 x 70 m; its apron APRON-CARGO with the
# four code F freighter stands C1-C4 on the airside). Units: metres. Local frame: X along the building from its
# west end (plan x 200), Y from the airside face (plan y 1231, facing the apron at -Y) to the landside (+Y), Z up.
# The halls are hollow with every room inside (walls with 4 m forklift openings, cold rooms with their own ceilings),
# the office block has three floors, and the working equipment is in place; the room list is in cargo_layout.py.
# The truck court on the landside needs the airport fence moved out from plan y 1320 to about y 1441.
#   python3 make_cargo_terminal.py   ->  CargoTerminal.blend, CargoTerminal.glb (next to this file)
import bpy, bmesh, math, os, sys
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import cargo_layout as CL
from cargo_layout import L, D, H_MAIN, H_ETV, H_OFF, H_SPEC, OFF, EXP, ETV, IMP, SPC, CANOPY_D, CANOPY_H, YARD_D

bpy.ops.wm.read_factory_settings(use_empty=True)
SC = bpy.context.scene
COL = bpy.data.collections.new('CargoTerminal'); SC.collection.children.link(COL)
LOW = 6.0                                    # outer walls are split at 6 m so a cutaway can drop the upper part


# #region ── Materials (blueprint look in the check renders; real materials are set up in Unreal) ──
def mat(name, rgb, rough=0.6, metal=0.0, emit=0.0, alpha=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if emit:
        b.inputs['Emission Color'].default_value = (*rgb, 1); b.inputs['Emission Strength'].default_value = emit
    if alpha < 1: b.inputs['Alpha'].default_value = alpha
    m.diffuse_color = (*rgb, alpha)
    return m


M = dict(
    wall=mat('M_Cargo_WallCladding', (0.16, 0.30, 0.50)),
    wall2=mat('M_Cargo_WallHighBay', (0.20, 0.36, 0.58)),
    roof=mat('M_Cargo_Roof', (0.10, 0.20, 0.36)),
    glass=mat('M_Cargo_Glass', (0.30, 0.55, 0.85), 0.1, 0.2),
    door=mat('M_Cargo_Door', (0.09, 0.17, 0.30)),
    shelter=mat('M_Cargo_DockShelter', (0.04, 0.06, 0.10)),
    canopy=mat('M_Cargo_Canopy', (0.24, 0.42, 0.66)),
    steel=mat('M_Cargo_Steel', (0.55, 0.62, 0.70), 0.4, 0.6),
    apron=mat('M_Cargo_Apron', (0.05, 0.10, 0.19)),
    yard=mat('M_Cargo_TruckCourt', (0.06, 0.12, 0.22)),
    slab=mat('M_Cargo_Slab', (0.08, 0.14, 0.24)),
    yellow=mat('M_Cargo_MarkYellow', (1.0, 0.80, 0.15), emit=0.6),
    white=mat('M_Cargo_MarkWhite', (0.92, 0.95, 1.0), emit=0.4),
    red=mat('M_Cargo_MarkRed', (0.95, 0.20, 0.20), emit=0.4),
    pv=mat('M_Cargo_Solar', (0.05, 0.08, 0.20), 0.2, 0.3),
    skylight=mat('M_Cargo_Skylight', (0.45, 0.70, 0.95), 0.1, 0.0, 0.2),
    hvac=mat('M_Cargo_Plant', (0.42, 0.50, 0.60), 0.5, 0.4),
    uld=mat('M_Cargo_ULD', (0.70, 0.74, 0.80), 0.4, 0.7),
    truck=mat('M_Cargo_Truck', (0.30, 0.48, 0.70)),
    cab=mat('M_Cargo_TruckCab', (0.85, 0.88, 0.95)),
    fence=mat('M_Cargo_Fence', (0.60, 0.68, 0.78), 0.5, 0.5),
    sign=mat('M_Cargo_Sign', (0.95, 0.97, 1.0), emit=1.0),
    part=mat('M_Cargo_Partition', (0.62, 0.72, 0.84)),
    cold=mat('M_Cargo_InsulatedPanel', (0.86, 0.92, 0.98)),
    rack=mat('M_Cargo_Rack', (0.95, 0.55, 0.15)),
    fork=mat('M_Cargo_Forklift', (1.0, 0.78, 0.10)),
    xray=mat('M_Cargo_XRay', (0.35, 0.80, 0.75)),
    cargo=mat('M_Cargo_Pallets', (0.62, 0.48, 0.30)),
    fl_off=mat('M_Cargo_Floor_Offices', (0.14, 0.32, 0.60)),
    fl_exp=mat('M_Cargo_Floor_Export', (0.07, 0.40, 0.38)),
    fl_etv=mat('M_Cargo_Floor_HighBay', (0.42, 0.35, 0.10)),
    fl_imp=mat('M_Cargo_Floor_Import', (0.45, 0.25, 0.10)),
    fl_spc=mat('M_Cargo_Floor_Special', (0.45, 0.12, 0.25)),
    fl_aisle=mat('M_Cargo_Floor_Aisle', (0.10, 0.14, 0.20)),
)
# #endregion


# #region ── Helpers ──
def box(name, x0, y0, z0, x1, y1, z1, m):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector(((v.co.x + 0.5) * (x1 - x0) + x0, (v.co.y + 0.5) * (y1 - y0) + y0, (v.co.z + 0.5) * (z1 - z0) + z0))
    me = bpy.data.meshes.new('SM_Cargo_' + name); bm.to_mesh(me); bm.free(); me.materials.append(M[m])
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    return o


def join(name, objs):
    """Merge objects of one kind into one (fewer objects for Unreal)."""
    objs = [o for o in objs if o]
    if not objs: return None
    bm = bmesh.new()
    for o in objs:
        me = o.data.copy(); me.transform(o.matrix_world); bm.from_mesh(me); bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new('SM_Cargo_' + name); bm.to_mesh(me); bm.free()
    me.materials.append(objs[0].data.materials[0])
    for o in objs:
        d = o.data; bpy.data.objects.remove(o); bpy.data.meshes.remove(d)
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    return o


# #endregion


# #region ── Building shell: ground slab, outer walls (lower and upper part), roofs, fire walls ──
box('GroundSlab', 0, 0, -0.30, L, D, 0.0, 'slab')
lo, hi = [], []
for zk, (x0, x1), h, _ in CL.ZONES:
    m = 'wall2' if zk == 'etv' else 'wall'
    for y0, y1 in ((-0.3, 0.0), (D, D + 0.3)):
        lo.append(box('OW', x0, y0, 0, x1, y1, LOW, m)); hi.append(box('OW', x0, y0, LOW, x1, y1, h, m))
for x0, x1, h in ((-0.3, 0.0, H_OFF), (L, L + 0.3, H_SPEC)):
    lo.append(box('OW', x0, -0.3, 0, x1, D + 0.3, LOW, 'wall')); hi.append(box('OW', x0, -0.3, LOW, x1, D + 0.3, h, 'wall'))
join('OuterWallsLow', lo); join('OuterWallsHigh', hi)
join('Roofs', [box('Roof', x0 - 0.3, -0.3, h, x1 + 0.3, D + 0.3, h + 0.6, 'roof') for _, (x0, x1), h, _ in CL.ZONES])
FW = ((EXP[0], H_MAIN), (ETV[0], H_ETV), (ETV[1], H_ETV), (SPC[0], H_MAIN))
# fire walls between fire zones stand 1 m through the roof; each has two 5 m fire-shutter openings for forklifts
fwl = []
for x, h in FW:
    for y0, y1 in ((-0.5, 25.0), (30.0, 52.5), (57.5, D + 0.5)):
        fwl.append(box('FW', x - 0.4, y0, 0, x + 0.4, y1, LOW, 'wall2'))
    fwl.append(box('FW', x - 0.4, 25.0, 5.0, x + 0.4, 30.0, LOW, 'wall2')); fwl.append(box('FW', x - 0.4, 52.5, 5.0, x + 0.4, 57.5, LOW, 'wall2'))
join('FireWallsLow', fwl)
join('FireWallsHigh', [box('FW', x - 0.4, -0.5, LOW, x + 0.4, D + 0.5, h + 1.6, 'wall2') for x, h in FW])
# office block windows: the ground floor's apart from the upper floors' so a cutaway can keep it
wl, wu = [], []
for f in range(3):
    z0 = 1.2 + f * 4.6; z1 = z0 + 2.2; tgt = wl if f == 0 else wu
    tgt.append(box('OW', OFF[0] + 2, -0.45, z0, OFF[1] - 2, -0.25, z1, 'glass'))
    tgt.append(box('OW', OFF[0] + 2, D + 0.25, z0, 9, D + 0.45, z1, 'glass')) if f == 0 else \
        tgt.append(box('OW', OFF[0] + 2, D + 0.25, z0, OFF[1] - 2, D + 0.45, z1, 'glass'))
    tgt.append(box('OW', -0.45, 4, z0, -0.25, D - 4, z1, 'glass'))
wu.append(box('ControlRoom', OFF[0] + 3, -3.0, 9.6, OFF[1] - 3, -0.3, 13.2, 'glass'))
join('OfficeWindowsGround', wl); join('OfficeWindowsUpper', wu)
box('OfficeEntrance', 10, D + 0.3, 0, 20, D + 4, 4.2, 'glass')
box('OfficeEntranceCanopy', 8, D, 4.2, 22, D + 6, 4.7, 'canopy')
join('HighBayGlazing', [box('HG', ETV[0] + 3, -0.45, H_MAIN + 2, ETV[1] - 3, -0.25, H_ETV - 2, 'glass'),
                        box('HG', ETV[0] + 3, D + 0.25, H_MAIN + 2, ETV[1] - 3, D + 0.45, H_ETV - 2, 'glass')])
# #endregion

# #region ── Rooms: coloured floors, aisles with yellow edge lines, partitions with openings, cold-room ceilings ──
floors, aisles, lines = {}, [], []
ground = []
for name, x0, y0, x1, y1, zk, kind, doors, short in CL.ROOMS:
    if kind == 'aisle':
        aisles.append(box('FA', x0, y0, 0.0, x1, y1, 0.03, 'fl_aisle'))
        if x1 - x0 >= y1 - y0:
            lines += [box('AL', x0, y0 + 0.1, 0.03, x1, y0 + 0.3, 0.05, 'yellow'), box('AL', x0, y1 - 0.3, 0.03, x1, y1 - 0.1, 0.05, 'yellow')]
        else:
            lines += [box('AL', x0 + 0.1, y0, 0.03, x0 + 0.3, y1, 0.05, 'yellow'), box('AL', x1 - 0.3, y0, 0.03, x1 - 0.1, y1, 0.05, 'yellow')]
        continue
    floors.setdefault(zk, []).append(box('FL', x0 + 0.15, y0 + 0.15, 0.0, x1 - 0.15, y1 - 0.15, 0.03, 'fl_' + zk))
    if kind in ('room', 'cold'):
        h = 4.4 if zk == 'off' else (7.0 if kind == 'cold' else 5.0)
        ground.append((x0, y0, x1, y1, h, doors))
for zk, objs in floors.items(): join('Floor_' + zk, objs)
join('Floor_Aisles', aisles); join('AisleLines', lines)
SKX = (0.0, L, EXP[0], ETV[0], ETV[1], SPC[0]); SKY = (0.0, D)
join('Partitions', [box('PW', *w, 'part') for w in CL.walls_from_rooms([r for r in ground if r[4] != 7.0], 0.0, SKX, SKY)])
join('ColdRoomWalls', [box('CW', *w, 'cold') for w in CL.walls_from_rooms([r for r in ground if r[4] == 7.0], 0.0, SKX, SKY)])
join('ColdCeilings', [box('CC', x0, y0, 7.0, x1, y1, 7.3, 'cold') for _, x0, y0, x1, y1, _, k, _, _ in CL.ROOMS if k == 'cold'])
# office block upper floors (slab + its partitions each)
for f, rooms in CL.UPPER.items():
    z = CL.FLOOR_Z[f]
    box(f'Office_F{f}_Slab', 0.0, 0.0, z, OFF[1], D, z + 0.3, 'slab')
    rr = [(x0, y0, x1, y1, 4.3, doors) for _, x0, y0, x1, y1, k, doors in rooms if k != 'aisle']
    join(f'Office_F{f}_Walls', [box('UW', *w, 'part') for w in CL.walls_from_rooms(rr, z + 0.3, (0.0, OFF[1]), SKY)])
# stairs and lifts core rising through the block on the landside
box('Office_StairCore', 22, 60, 0, 29, 69, H_OFF + 1.5, 'wall')
box('ReceptionDesk', 12, 63, 0, 18, 64.2, 1.1, 'steel')
# #endregion

# #region ── Equipment: transfer roller decks, build-up and break-down stations, racks, screening, ETV store ──
eq_steel, eq_uld, eq_rack, eq_cargo, eq_xray, eq_fork = [], [], [], [], [], []
y0t, y1t = CL.TRANSFER
eq_steel.append(box('TR', EXP[0] + 0.5, y0t, 0, IMP[1] - 0.5, y1t, 0.55, 'steel'))           # roller-deck transfer lane
wd, sy0, sy1 = CL.STATION
for i, cx in enumerate(CL.BUILDUP + CL.BREAKDOWN):
    eq_steel.append(box('ST', cx - wd / 2, sy0, 0, cx + wd / 2, sy1, 0.5, 'steel'))           # roller-bed workstation
    eq_steel.append(box('SC', cx - wd / 2 - 0.2, sy1 + 0.3, 0, cx + wd / 2 + 0.2, sy1 + 3.3, 0.12, 'steel'))  # scale
    if i % 2 == 0: eq_uld.append(box('UL', cx - 1.55, sy0 + 1.0, 0.5, cx + 1.55, sy0 + 3.5 + 2.4, 2.9 if i % 4 else 3.1, 'uld'))
for zk, (xa, xb, rows) in CL.RACK_ROWS.items():
    for ry in rows:
        n = int((xb - xa) / 2.7)
        for i in range(n + 1):
            x = xa + i * (xb - xa) / n
            eq_rack += [box('RU', x - 0.06, ry, 0, x + 0.06, ry + 0.12, 9.0, 'rack'), box('RU', x - 0.06, ry + 2.28, 0, x + 0.06, ry + 2.4, 9.0, 'rack')]
        for z in (2.2, 4.4, 6.6, 8.8):
            eq_rack.append(box('RB', xa, ry, z - 0.12, xb, ry + 2.4, z, 'rack'))
            for i in range(n):
                if (i * 7 + int(z * 3) + int(ry)) % 3: eq_cargo.append(box('PL', xa + i * (xb - xa) / n + 0.3, ry + 0.2, z, xa + (i + 1) * (xb - xa) / n - 0.3, ry + 2.2, z + 1.5, 'cargo'))
eq_xray += [box('XR', *r[:2], 0, *r[2:], 2.2, 'xray') for r in CL.XRAY]
eq_steel += [box('SCL', x0, y0, 0, x1, y1, 0.1, 'steel') for x0, y0, x1, y1 in CL.SCALES]
# ETV store: rack columns and decks on both sides of the aisle, ULDs in two of three positions, the two ETVs
for ya, yb in ((22.0, 30.0), (40.0, 48.0)):
    xs = CL.ETV_RACK_X + [CL.ETV_RACK_X[-1] + 3.3]
    for x in xs:
        for yy in (ya, yb - 0.4):
            eq_rack.append(box('EC', x - 0.2, yy, 0, x + 0.2, yy + 0.4, 21.8, 'rack'))
    for z in CL.ETV_LEVELS[1:] + [21.6]:
        eq_rack.append(box('ED', xs[0], ya, z - 0.3, xs[-1], yb, z, 'rack'))
    for i, x in enumerate(CL.ETV_RACK_X):
        for j, z in enumerate(CL.ETV_LEVELS):
            if (i + j * 2 + int(ya)) % 3:
                eq_uld.append(box('EU', x + 0.2, ya + 1.0, z + 0.1, x + 3.1, yb - 1.0, z + 0.1 + (2.5 if (i + j) % 2 else 3.0), 'uld'))
for ya in (10.0, 15.5, 52.5, 58.0):                                                          # roller-bed buffers
    eq_steel.append(box('RB', ETV[0] + 1, ya, 0, ETV[1] - 1, ya + 3.2, 0.5, 'steel'))
for x in CL.ETV_MACHINES:
    eq_steel += [box('EM', x - 0.6, 30.8, 0, x + 0.6, 32.2, 23.0, 'steel'), box('EM', x - 0.6, 37.8, 0, x + 0.6, 39.2, 23.0, 'steel'),
                 box('EC', x - 1.8, 30.8, 10.5, x + 1.8, 39.2, 11.0, 'steel')]
    eq_fork.append(box('EB', x - 1.0, 31.0, 0, x + 1.0, 39.0, 0.8, 'fork'))
# forklifts in the aisles, ready to go
for x, y in ((50, 27), (95, 55), (120, 27), (300, 27), (330, 55), (60, 64), (100, 64), (310, 64), (366, 31), (380, 15)):
    eq_fork += [box('FK', x - 0.6, y - 1.3, 0, x + 0.6, y + 1.3, 2.1, 'fork'), box('FM', x - 0.5, y - 1.7, 0, x + 0.5, y - 1.4, 3.6, 'steel')]
# live-animal pens and the dangerous-goods cages
eq_steel += [box('AP', 347 + i * 3.4, 2, 0, 349.6 + i * 3.4, 6, 1.8, 'steel') for i in range(4)]
eq_rack += [box('DG', 346 + i * 2.8, 40, 0, 348.4 + i * 2.8, 45, 2.4, 'fence') for i in range(4)]
eq_cargo += [box('PB', 287 + i * 3.6, 60, 0, 289.6 + i * 3.6, 62.4, 1.6, 'cargo') for i in range(12)] + \
            [box('PB', 33 + i * 4.2, 60, 0, 35.8 + i * 4.2, 62.4, 1.6, 'cargo') for i in range(18)]
join('RollerDecksAndStations', eq_steel); join('ULDsInside', eq_uld); join('Racks', eq_rack)
join('PalletsInside', eq_cargo); join('Screening', eq_xray); join('Forklifts', eq_fork)
# #endregion

# #region ── Airside: canopy on columns, ULD transfer doors with roller decks, the airside service road ──
box('AirsideCanopy', EXP[0], -CANOPY_D, CANOPY_H, IMP[1], -0.3, CANOPY_H + 1.0, 'canopy')
join('CanopyColumns', [box('CC', x - 0.4, -CANOPY_D + 0.6, 0, x + 0.4, -CANOPY_D + 1.4, CANOPY_H, 'steel')
                       for x in [EXP[0] + i * CL.BAY for i in range(int((IMP[1] - EXP[0]) / CL.BAY) + 1)]])
join('AirsideDoors', [box('AD', x - 3.0, -0.5, 0, x + 3.0, -0.25, 5.2, 'door') for x in CL.AIR_DOORS])
join('RollerDecks', [box('RD', x - 2.0, -6.0, 0, x + 2.0, -0.5, 0.5, 'steel') for x in CL.AIR_DOORS])
join('SpecialDoors', [box('SD', x - 2.0, -0.5, 0, x + 2.0, -0.25, 4.5, 'door') for x in CL.SPC_AIR_DOORS])
box('AirsideRoad', -40, -40, -0.05, L + 60, 0, 0.0, 'yard')
join('ULDLanes', [box('UL', x - 2.5, -38, 0.0, x + 2.5, -30, 0.02, 'white') for x in CL.AIR_DOORS] +
     [box('ULb', -40, -40.2, 0.0, L + 60, -39.9, 0.02, 'yellow'), box('ULc', -40, -14.2, 0.0, L + 60, -13.9, 0.02, 'yellow')])
uld = []
for i, x in enumerate(CL.AIR_DOORS[::3]):
    uld += [box('DL', x - 1.6, -37.5, 0.4, x + 1.6, -31, 0.7, 'steel'), box('U', x - 1.5, -37, 0.7, x + 1.5, -31.5, 3.2 if i % 2 else 2.3, 'uld')]
# outside the east end: the empty ULD store (stacked) and the ULD repair shed; dollies and GSE at the west end
ex = CL.OUTSIDE
for i in range(8):
    for j in range(3):
        uld.append(box('EU', ex[0][1] + 1 + i * 5.1, ex[0][2] + 1 + j * 6.4, 0, ex[0][1] + 4.2 + i * 5.1, ex[0][2] + 5.8 + j * 6.4, 1.7 * (1 + (i + j) % 2), 'uld'))
join('ULDsOutside', uld)
box('ULDRepairShed', *ex[1][1:3], 0, *ex[1][3:5], 7.0, 'wall')
join('Dollies', [box('DO', ex[2][1] + 1 + i * 4.2, ex[2][2] + 2 + j * 9, 0.3, ex[2][1] + 4.4 + i * 4.2, ex[2][2] + 8.5 + j * 9, 0.6, 'steel')
                 for i in range(7) for j in range(3)])
# #endregion

# #region ── Landside: truck docks with levellers and shelters, drive-in doors, the truck court and its gate ──
DK = CL.DOCKS + CL.SPC_DOCKS
join('DockDoors', [box('DD', x - 1.5, D + 0.25, 1.2, x + 1.5, D + 0.5, 4.7, 'door') for x in DK])
join('DockShelters', [box('DS', x - 2.0, D + 0.3, 0.9, x + 2.0, D + 1.2, 5.2, 'shelter') for x in DK])
join('DockBumpers', [box('DB', x - 1.8, D + 0.3, 0.8, x + 1.8, D + 0.55, 1.2, 'shelter') for x in DK])
join('DockLevellers', [box('DL', x - 1.4, D - 3.0, 0.0, x + 1.4, D, 0.08, 'steel') for x in DK])
join('DriveInDoors', [box('DI', x - 2.5, D + 0.25, 0, x + 2.5, D + 0.5, 5.0, 'door') for x in CL.DRIVE_IN])
box('LandsideCanopy', EXP[0], D + 0.3, 6.0, L, D + 6.0, 6.6, 'canopy')
box('TruckCourt', -10, D, -0.05, L + 10, D + YARD_D, 0.0, 'yard')
marks = [box('TM', x - 0.1, D + 2, 0.0, x + 0.1, D + 22, 0.02, 'white') for x in [d + 6.5 for d in DK[:-1]]]
marks += [box('TMb', -10, D + 52, 0.0, L + 10, D + 52.3, 0.02, 'white')]
marks += [box('PB', 40 + i * 5, D + 110, 0.0, 40.2 + i * 5, D + 132, 0.02, 'white') for i in range(60)]
marks += [box('CP', -8 + i * 2.6, D + 20, 0.0, -7.9 + i * 2.6, D + 25, 0.02, 'white') for i in range(14)]
join('CourtMarkings', marks)
join('Fence', [box('F', -10, D + YARD_D - 0.1, 0, 150, D + YARD_D + 0.1, 2.6, 'fence'),
               box('F', 175, D + YARD_D - 0.1, 0, L + 10, D + YARD_D + 0.1, 2.6, 'fence')])
box('Gatehouse', 152, D + YARD_D - 10, 0, 160, D + YARD_D - 4, 3.2, 'glass')
box('GatehouseRoof', 150, D + YARD_D - 12, 3.2, 176, D + YARD_D - 2, 3.7, 'canopy')
box('GateBarrier', 162, D + YARD_D - 1, 1.0, 174, D + YARD_D - 0.7, 1.2, 'red')
tt, tc = [], []
for i, x in enumerate(DK):
    if i % 3 == 1: continue
    tt.append(box('TT', x - 1.25, D + 1.3, 1.1, x + 1.25, D + 14.9, 4.0, 'truck'))
    tc.append(box('TC', x - 1.25, D + 15.7, 0.9, x + 1.25, D + 21.3, 3.9, 'cab'))
join('Trailers', tt); join('Tractors', tc)
# #endregion

# #region ── Roof: skylight strips, smoke vents, solar panels, the special block's refrigeration plant ──
join('Skylights', [box('SK', x0 + 4, y, h + 0.6, x1 - 4, y + 2.5, h + 1.4, 'skylight') for (x0, x1), h in ((EXP, H_MAIN), (IMP, H_MAIN))
                   for y in (12, 34, 56)])
join('SmokeVents', [box('SV', x, y, h + 0.6, x + 3, y + 3, h + 1.8, 'hvac') for (x0, x1), h in ((EXP, H_MAIN), (IMP, H_MAIN), (ETV, H_ETV))
                    for x in range(int(x0) + 8, int(x1) - 6, 20) for y in (24, 44)])
join('SolarPanels', [box('PV', ETV[0] + 5, y, H_ETV + 0.9, ETV[1] - 5, y + 4.6, H_ETV + 1.5, 'pv') for y in range(6, int(D) - 8, 7)])
join('RefrigerationPlant', [box('RP', 350 + i * 9, 10 + j * 22, H_SPEC + 0.6, 356 + i * 9, 20 + j * 22, H_SPEC + 3.2, 'hvac')
                            for i in range(4) for j in range(3)])
# #endregion

# #region ── Sign on the airside face of the high bay ──
cu = bpy.data.curves.new('Sign', 'FONT'); cu.body = 'CARGO TERMINAL'; cu.size = 5.0; cu.extrude = 0.15
cu.align_x = 'CENTER'; cu.align_y = 'CENTER'
so = bpy.data.objects.new('SignText', cu); COL.objects.link(so)
so.location = ((ETV[0] + ETV[1]) / 2, -0.6, H_MAIN + (H_ETV - H_MAIN) / 2 + 0.2)
so.rotation_euler = (math.pi / 2, 0, 0)
cu.materials.append(M['sign'])
dg = bpy.context.evaluated_depsgraph_get(); dg.update()
me = bpy.data.meshes.new_from_object(so.evaluated_get(dg)); loc, rot = so.location.copy(), so.rotation_euler.copy()
bpy.data.objects.remove(so); bpy.data.curves.remove(cu)
o = bpy.data.objects.new('SignText', me); COL.objects.link(o); o.location = loc; o.rotation_euler = rot
# #endregion

# #region ── The cargo apron in front, with the four freighter stands' lead-in lines ──
box('CargoApron', -40, -165, -0.06, 460, -40, -0.01, 'apron')
st = []
for sx in (60.0, 147.5, 235.0, 322.5):         # C1-C4 (plan x 260, 347.5, 435, 522.5 -> local 60 ... 322.5)
    st.append(box('LI', sx - 0.15, -160, 0.0, sx + 0.15, -45, 0.02, 'yellow'))
    st.append(box('SB', sx - 6, -47, 0.0, sx + 6, -46.4, 0.02, 'yellow'))
    st.append(box('WT', sx - 40, -125, 0.0, sx + 40, -124.7, 0.02, 'white'))
join('StandMarkings', st)
# #endregion

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'CargoTerminal.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'CargoTerminal.glb'), export_format='GLB')
print('saved', len(bpy.data.objects), 'objects', flush=True)
