# The cargo terminal (freight only, no passengers), built in Blender to the airfield plan's footprint
# (Data/airfield.json cargoBuilding: plan x 200..590, y 1231..1301 = 390 x 70 m; its apron APRON-CARGO with the
# four code F freighter stands C1-C4 on the airside). Units: metres. Local frame: X along the building from its
# west end (plan x 200), Y from the airside face (plan y 1231, facing the apron at -Y) to the landside (+Y), Z up.
# The halls are hollow with every room inside (walls with 4 m forklift openings, cold rooms with their own ceilings),
# the office block has three floors, and the working equipment is in place; the room list is in cargo_layout.py.
# Every door is plain to see: overhead and sectional doors amber, personnel and escape doors green with an exit sign
# above, fire shutters red. The truck court on the landside needs the airport fence moved out from plan y 1320 to y 1441.
#   python3 make_cargo_terminal.py   ->  CargoTerminal.blend, CargoTerminal.glb (next to this file)
import bpy, bmesh, math, os, sys
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import cargo_layout as CL
from cargo_layout import L, D, H_MAIN, H_ETV, H_OFF, H_SPEC, OFF, EXP, ETV, IMP, SPC, BCP, CANOPY_D, CANOPY_H, YARD_D, A1, A2

bpy.ops.wm.read_factory_settings(use_empty=True)
SC = bpy.context.scene
COL = bpy.data.collections.new('CargoTerminal'); SC.collection.children.link(COL)
LOW = 6.0                                    # outer walls are split at 6 m so a cutaway can drop the upper part


# #region ── Materials: photoreal texture sets (make_cargo_textures.py, from CC0 scans; flat colours only for lit signs) ──
TEX = os.path.join(HERE, 'Textures')
import importlib.util as _iu
_sp = _iu.spec_from_file_location('cargo_tex', os.path.join(HERE, 'make_cargo_textures.py')); _ct = _iu.module_from_spec(_sp)
_sp.loader.exec_module(_ct); TILE = _ct.TILE                  # metres per texture tile, for the UVs
MAT_TILE = {}                                                 # material name -> metres per UV unit


def mat(name, rgb, rough=0.6, metal=0.0, emit=0.0, alpha=1.0, tex=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if emit:
        b.inputs['Emission' if 'Emission' in b.inputs else 'Emission Color'].default_value = (*rgb, 1); b.inputs['Emission Strength'].default_value = emit
    if alpha < 1:
        b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND'; m.shadow_method = 'HASHED'
    if tex:
        MAT_TILE[name] = TILE[tex]
        ext = 'png' if os.path.exists(os.path.join(TEX, tex + '_Color.png')) else 'jpg'
        ci = nt.nodes.new('ShaderNodeTexImage'); ci.image = bpy.data.images.load(os.path.join(TEX, '%s_Color.%s' % (tex, ext)), check_existing=True)
        nt.links.new(ci.outputs['Color'], b.inputs['Base Color'])
        if ext == 'png':                                      # glazing: the panes see-through, the mullions not
            nt.links.new(ci.outputs['Alpha'], b.inputs['Alpha']); m.blend_method = 'BLEND'; m.shadow_method = 'HASHED'
        ni = nt.nodes.new('ShaderNodeTexImage'); ni.image = bpy.data.images.load(os.path.join(TEX, tex + '_Normal.jpg'), check_existing=True)
        ni.image.colorspace_settings.name = 'Non-Color'
        nm = nt.nodes.new('ShaderNodeNormalMap'); nt.links.new(ni.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
        oi = nt.nodes.new('ShaderNodeTexImage'); oi.image = bpy.data.images.load(os.path.join(TEX, tex + '_ORM.jpg'), check_existing=True)
        oi.image.colorspace_settings.name = 'Non-Color'
        if bpy.app.version >= (3, 3, 0) and hasattr(bpy.types, 'ShaderNodeSeparateColor'):
            sp = nt.nodes.new('ShaderNodeSeparateColor'); nt.links.new(oi.outputs['Color'], sp.inputs[0]); o = sp.outputs
            nt.links.new(o[1], b.inputs['Roughness']); nt.links.new(o[2], b.inputs['Metallic'])
        else:
            sp = nt.nodes.new('ShaderNodeSeparateRGB'); nt.links.new(oi.outputs['Color'], sp.inputs['Image'])
            nt.links.new(sp.outputs['G'], b.inputs['Roughness']); nt.links.new(sp.outputs['B'], b.inputs['Metallic'])
    m.diffuse_color = (*rgb, alpha)
    return m


M = dict(
    wall=mat('M_Cargo_WallCladding', (0.74, 0.76, 0.77), tex='Cladding'),
    wall2=mat('M_Cargo_WallHighBay', (0.74, 0.76, 0.77), tex='Cladding'),
    band=mat('M_Cargo_CladdingBand', (0.06, 0.22, 0.45), tex='CladdingBand'),
    roof=mat('M_Cargo_Roof', (0.24, 0.26, 0.28), tex='Roof'),
    glass=mat('M_Cargo_Glass', (0.10, 0.13, 0.15), 0.05, tex='OfficeGlass'),
    door=mat('M_Cargo_Door', (0.92, 0.50, 0.06), tex='SectionalDoor'),
    esc=mat('M_Cargo_PersonnelDoor', (0.08, 0.42, 0.20), tex='PersonnelDoor'),
    exitsign=mat('M_Cargo_ExitSign', (0.20, 1.0, 0.40), emit=2.0),
    shutter=mat('M_Cargo_FireShutter', (0.70, 0.08, 0.06), tex='FireShutter'),
    concrete=mat('M_Cargo_FireRatedWall', (0.55, 0.55, 0.54), tex='FireWall'),
    lead=mat('M_Cargo_LeadShield', (0.30, 0.32, 0.36), 0.5, 1.0, tex='LeadSheet'),
    fl_bcp=mat('M_Cargo_Floor_BCP', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    shelter=mat('M_Cargo_DockShelter', (0.03, 0.03, 0.03), 0.8, tex='Rubber'),
    canopy=mat('M_Cargo_Canopy', (0.86, 0.88, 0.88), tex='CladdingWhite'),
    steel=mat('M_Cargo_Steel', (0.55, 0.57, 0.59), 0.4, 1.0, tex='Galvanised'),
    apron=mat('M_Cargo_Apron', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    yard=mat('M_Cargo_TruckCourt', (0.16, 0.16, 0.16), tex='Asphalt'),
    slab=mat('M_Cargo_Slab', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    yellow=mat('M_Cargo_MarkYellow', (0.90, 0.68, 0.05), tex='LineYellow'),
    white=mat('M_Cargo_MarkWhite', (0.85, 0.86, 0.85), tex='LineWhite'),
    red=mat('M_Cargo_MarkRed', (0.75, 0.10, 0.08), tex='LineRed'),
    green=mat('M_Cargo_MarkGreen', (0.10, 0.50, 0.22), tex='LineGreen'),
    pv=mat('M_Cargo_Solar', (0.03, 0.05, 0.12), 0.15, 0.2, tex='Solar'),
    skylight=mat('M_Cargo_SkylightGlass', (0.85, 0.88, 0.90), 0.15, 0.0, 0.0, 0.55),
    hvac=mat('M_Cargo_Plant', (0.42, 0.45, 0.47), tex='PaintGrey'),
    uld=mat('M_Cargo_ULD', (0.70, 0.72, 0.74), 0.4, 1.0, tex='Aluminium'),
    truck=mat('M_Cargo_Truck', (0.82, 0.83, 0.83), tex='PaintWhite'),
    cab=mat('M_Cargo_TruckCab', (0.10, 0.25, 0.50), tex='PaintBlue'),
    fence=mat('M_Cargo_Fence', (0.55, 0.57, 0.59), 0.5, 1.0, tex='Galvanised'),
    sign=mat('M_Cargo_Sign', (0.95, 0.97, 1.0), emit=1.0),
    part=mat('M_Cargo_Partition', (0.86, 0.88, 0.88), tex='CladdingWhite'),
    cold=mat('M_Cargo_InsulatedPanel', (0.86, 0.88, 0.88), tex='CladdingWhite'),
    rack=mat('M_Cargo_Rack', (0.85, 0.32, 0.04), tex='PaintOrange'),
    fork=mat('M_Cargo_Forklift', (0.95, 0.66, 0.04), tex='PaintYellow'),
    xray=mat('M_Cargo_XRay', (0.20, 0.50, 0.50), tex='PaintTeal'),
    cargo=mat('M_Cargo_Pallets', (0.52, 0.38, 0.22), tex='Cardboard'),
    fl_off=mat('M_Cargo_Floor_Offices', (0.36, 0.38, 0.40), tex='OfficeFloor'),
    fl_exp=mat('M_Cargo_Floor_Export', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    fl_etv=mat('M_Cargo_Floor_HighBay', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    fl_imp=mat('M_Cargo_Floor_Import', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    fl_spc=mat('M_Cargo_Floor_Special', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    fl_aisle=mat('M_Cargo_Floor_Aisle', (0.50, 0.50, 0.49), tex='FloorConcrete'),
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
lo, hi, bd = [], [], []
BAND = 1.0                                   # the coloured cladding band, one panel high, right over the 6 m split
for zk, (x0, x1), h, _ in CL.ZONES:
    m = 'wall2' if zk == 'etv' else 'wall'
    for y0, y1 in ((-0.3, 0.0), (D, D + 0.3)):
        lo.append(box('OW', x0, y0, 0, x1, y1, LOW, m)); bd.append(box('OB', x0, y0, LOW, x1, y1, LOW + BAND, 'band'))
        hi.append(box('OW', x0, y0, LOW + BAND, x1, y1, h, m))
for x0, x1, h in ((-0.3, 0.0, H_OFF), (L, L + 0.3, H_SPEC)):
    lo.append(box('OW', x0, -0.3, 0, x1, D + 0.3, LOW, 'wall')); bd.append(box('OB', x0, -0.3, LOW, x1, D + 0.3, LOW + BAND, 'band'))
    hi.append(box('OW', x0, -0.3, LOW + BAND, x1, D + 0.3, h, 'wall'))
join('OuterWallsLow', lo); join('CladdingBand', bd); join('OuterWallsHigh', hi)
join('Roofs', [box('Roof', x0 - 0.3, -0.3, h, x1 + 0.3, D + 0.3, h + 0.6, 'roof') for _, (x0, x1), h, _ in CL.ZONES])
# fire walls between fire zones stand 1 m through the roof; 6.5 m fire shutters at both aisles (none into the border
# control post) and 1.2 m escape doors in them
fwl, fwd, fws = [], [], []
for x, h in CL.FIRE_WALLS:
    gaps = [(a, b, 5.5) for a, b in (A1, A2)] if x in CL.FIRE_SHUTTERS else []
    gaps += [(y - 0.6, y + 0.6, 2.2) for fx, y in CL.FW_DOORS if fx == x]
    gaps.sort(); y = -0.5
    for a, b, hd in gaps + [(D + 0.5, D + 0.5, 0)]:
        if a > y: fwl.append(box('FW', x - 0.4, y, 0, x + 0.4, a, LOW, 'wall2'))
        if b > a:
            fwl.append(box('FW', x - 0.4, a, hd, x + 0.4, b, LOW, 'wall2'))
            if hd > 3: fws.append(box('FS', x - 0.5, a, hd - 0.6, x + 0.5, b, hd, 'shutter'))          # rolled-up shutter
            else: fwd.append(box('FD', x - 0.05, a + 0.1, 0, x + 0.05, b - 0.1, 2.1, 'esc'))           # escape door
        y = b
join('FireWallsLow', fwl); join('FireShutters', fws)
join('FireWallsHigh', [box('FW', x - 0.4, -0.5, LOW, x + 0.4, D + 0.5, h + 1.6, 'wall2') for x, h in CL.FIRE_WALLS])
# the full-height wall between the border control post's animals unit and its animal products unit
join('BCPSplitWall', [box('BW', CL.BCP_SPLIT - 0.2, 0, 0, CL.BCP_SPLIT + 0.2, D, H_SPEC, 'wall2')])
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
for name, x0, y0, x1, y1, zk, kind, doors, short in CL.ROOMS:
    if kind == 'aisle':
        aisles.append(box('FA', x0, y0, 0.0, x1, y1, 0.03, 'fl_aisle'))
        if x1 - x0 >= y1 - y0:
            lines += [box('AL', x0, y0 + 0.1, 0.03, x1, y0 + 0.3, 0.05, 'yellow'), box('AL', x0, y1 - 0.3, 0.03, x1, y1 - 0.1, 0.05, 'yellow')]
        else:
            lines += [box('AL', x0 + 0.1, y0, 0.03, x0 + 0.3, y1, 0.05, 'yellow'), box('AL', x1 - 0.3, y0, 0.03, x1 - 0.1, y1, 0.05, 'yellow')]
        continue
    floors.setdefault(zk, []).append(box('FL', x0 + 0.15, y0 + 0.15, 0.0, x1 - 0.15, y1 - 0.15, 0.03, 'fl_' + zk))
for zk, objs in floors.items(): join('Floor_' + zk, objs)
join('Floor_Aisles', aisles); join('AisleLines', lines)
ground = CL.ground_rooms()
SKX, SKY = CL.SKIP_X, CL.SKIP_Y
join('Partitions', [box('PW', *w, 'part') for w in CL.walls_from_rooms([r for r in ground if r[4] in (4.4, 5.0)], 0.0, SKX, SKY)])
join('ColdRoomWalls', [box('CW', *w, 'cold') for w in CL.walls_from_rooms([r for r in ground if r[4] == 7.0], 0.0, SKX, SKY)])
join('DGWalls', [box('GW', *w, 'concrete') for w in CL.walls_from_rooms([r for r in ground if r[4] == 6.0], 0.0, SKX, SKY)])
join('ColdCeilings', [box('CC', x0, y0, 7.0, x1, y1, 7.3, 'cold') for _, x0, y0, x1, y1, _, k, _, _ in CL.ROOMS if k == 'cold'] +
     [box('CC', x0, y0, 6.0, x1, y1, 6.3, 'cold') for _, x0, y0, x1, y1, _, k, _, _ in CL.ROOMS if k == 'dg'])
join('DGSumps', [box('SU', x0 + 0.8, y0 + 0.8, 0.03, x1 - 0.8, y1 - 0.8, 0.06, 'lead') for _, x0, y0, x1, y1, _, k, _, _ in CL.ROOMS if k == 'dg'])
# a security mesh fence where no room wall forms the line between unscreened and secure cargo
join('SecurityFence', [box('SF', a, CL.SECURITY_LINE[2] - 0.05, 0, b, CL.SECURITY_LINE[2] + 0.05, 3.0, 'fence') for a, b in CL.SECURITY_FENCE])


def door_parts(doors, z0, tag):
    """Room doors made plain to see: a personnel door is a green leaf with a green exit light above it; a wider
    opening gets amber jambs, an amber header and its door rolled up under the header."""
    lit, amber, green = [], [], []
    for cx, cy, o, w in doors:
        hw = w / 2
        if w <= 1.2:
            leaf = (cx - hw, cy - 0.06, z0, cx + hw, cy + 0.06, z0 + 2.1) if o == 'h' else (cx - 0.06, cy - hw, z0, cx + 0.06, cy + hw, z0 + 2.1)
            green.append(box(tag + 'P', *leaf, 'esc'))
            sign = (cx - 0.3, cy - 0.2, z0 + 2.25, cx + 0.3, cy + 0.2, z0 + 2.5) if o == 'h' else (cx - 0.2, cy - 0.3, z0 + 2.25, cx + 0.2, cy + 0.3, z0 + 2.5)
            lit.append(box(tag + 'X', *sign, 'exitsign'))
        else:
            hd = 4.5 if w >= 4 else 3.4
            for s in (-1, 1):
                j = (cx + s * hw - 0.15, cy - 0.3, z0, cx + s * hw + 0.15, cy + 0.3, z0 + hd + 0.4) if o == 'h' else \
                    (cx - 0.3, cy + s * hw - 0.15, z0, cx + 0.3, cy + s * hw + 0.15, z0 + hd + 0.4)
                amber.append(box(tag + 'J', *j, 'door'))
            hb = (cx - hw - 0.15, cy - 0.35, z0 + hd, cx + hw + 0.15, cy + 0.35, z0 + hd + 0.6) if o == 'h' else \
                 (cx - 0.35, cy - hw - 0.15, z0 + hd, cx + 0.35, cy + hw + 0.15, z0 + hd + 0.6)
            amber.append(box(tag + 'H', *hb, 'door'))
            amber.append(box(tag + 'T', *((cx - hw, cy - 0.3, z0 + 0.03, cx + hw, cy + 0.3, z0 + 0.06) if o == 'h' else
                                         (cx - 0.3, cy - hw, z0 + 0.03, cx + 0.3, cy + hw, z0 + 0.06)), 'door'))   # threshold strip
    return green, amber, lit


g, a_, lt = door_parts(CL.doors_from_rooms(ground, SKX, SKY), 0.0, 'G')
join('RoomDoorsPersonnel', g); join('RoomDoorsSectional', a_); join('ExitSigns', lt)
# office block upper floors (slab + its partitions and doors each)
for f, rooms in CL.UPPER.items():
    z = CL.FLOOR_Z[f]
    box(f'Office_F{f}_Slab', 0.0, 0.0, z, OFF[1], D, z + 0.3, 'slab')
    rr = [(x0, y0, x1, y1, 4.3, doors, 1.0) for _, x0, y0, x1, y1, k, doors in rooms if k != 'aisle']
    w_ = [box('UW', *w, 'part') for w in CL.walls_from_rooms(rr, z + 0.3, (0.0, OFF[1]), SKY)]
    g, a_, lt = door_parts(CL.doors_from_rooms(rr, (0.0, OFF[1]), SKY), z + 0.3, f'U{f}')
    join(f'Office_F{f}_Walls', w_); join(f'Office_F{f}_Doors', g + lt)
# the three stair cores rising through the block (one stair per 900 m2 of floor)
join('Office_Stairs', [box('SC', x0 + 0.3, y0 + 0.3, 0, x1 - 0.3, y1 - 0.3, H_OFF + 1.5, 'wall') for x0, y0, x1, y1 in CL.STAIRS])
box('ReceptionDesk', 14, 63, 0, 20, 64.2, 1.1, 'steel')
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
for ya, yb in CL.ETV_RACKS:
    xs = CL.ETV_RACK_X + [CL.ETV_RACK_X[-1] + 3.3]
    for x in xs:
        for yy in (ya, yb - 0.4):
            eq_rack.append(box('EC', x - 0.2, yy, 0, x + 0.2, yy + 0.4, 21.8, 'rack'))
    for z in CL.ETV_LEVELS[1:] + [21.6]:
        eq_rack.append(box('ED', xs[0], ya, z - 0.3, xs[-1], yb, z, 'rack'))
    for i, x in enumerate(CL.ETV_RACK_X):
        for j, z in enumerate(CL.ETV_LEVELS):
            if (i + j * 2 + int(ya)) % 3:
                eq_uld.append(box('EU', x + 0.2, ya + 0.6, z + 0.1, x + 3.1, yb - 0.6, z + 0.1 + (2.5 if (i + j) % 2 else 3.0), 'uld'))
for ya in (7.5, 12.5, 17.5):                                                                  # roller-bed buffer rows
    eq_steel.append(box('RB', ETV[0] + 1, ya, 0, ETV[1] - 1, ya + 3.2, 0.5, 'steel'))
e0, e1 = CL.ETV_AISLE
for x in CL.ETV_MACHINES:
    eq_steel += [box('EM', x - 0.6, e0 + 0.2, 0, x + 0.6, e0 + 1.2, 23.0, 'steel'), box('EM', x - 0.6, e1 - 1.2, 0, x + 0.6, e1 - 0.2, 23.0, 'steel'),
                 box('EC', x - 1.8, e0 + 0.2, 10.5, x + 1.8, e1 - 0.2, 11.0, 'steel')]
    eq_fork.append(box('EB', x - 1.0, e0 + 0.4, 0, x + 1.0, e1 - 0.4, 0.8, 'fork'))
# forklifts in the aisles, ready to go
for x, y in ((50, 26.75), (95, 53.75), (120, 26.75), (300, 26.75), (330, 53.75), (60, 66), (100, 66), (310, 66), (366, 26.75), (420, 53.75)):
    eq_fork += [box('FK', x - 0.6, y - 1.3, 0, x + 0.6, y + 1.3, 2.1, 'fork'), box('FM', x - 0.5, y - 1.7, 0, x + 0.5, y - 1.4, 3.6, 'steel')]
# border control post: horse stalls (4.05 x 4.7 m), stalls for other hoofed animals, kennels; radioactive bays
eq_steel += [box('HS', x0, y0, 0, x1, y1, 2.2, 'steel') for x0, y0, x1, y1 in CL.HORSE_STALLS]
eq_steel += [box('CS', x0, y0, 0, x1, y1, 1.8, 'steel') for x0, y0, x1, y1 in CL.CATTLE_STALLS]
eq_steel += [box('KN', 406 + i * 2.3, 31, 0, 408 + i * 2.3, 33, 1.4, 'steel') for i in range(6)]
eq_steel += [box('KN', 406 + i * 2.3, 37, 0, 408 + i * 2.3, 39, 1.4, 'steel') for i in range(6)]
eq_rack += [box('RA', x0, y0, 0, x1, y1, 1.2, 'lead') for x0, y0, x1, y1 in CL.RADIO_BAYS]
eq_cargo += [box('PB', 287 + i * 3.6, 60, 0, 289.6 + i * 3.6, 62.4, 1.6, 'cargo') for i in range(12)] + \
            [box('PB', 33 + i * 4.2, 66, 0, 35.8 + i * 4.2, 68.4, 1.6, 'cargo') for i in range(18)]
join('RollerDecksAndStations', eq_steel); join('ULDsInside', eq_uld); join('Racks', eq_rack)
join('PalletsInside', eq_cargo); join('Screening', eq_xray); join('Forklifts', eq_fork)
# #endregion

# #region ── Airside: canopy on columns, ULD transfer doors with roller decks, the airside service road ──
box('AirsideCanopy', EXP[0], -CANOPY_D, CANOPY_H, IMP[1], -0.3, CANOPY_H + 1.0, 'canopy')
join('CanopyColumns', [box('CC', x - 0.4, -CANOPY_D + 0.6, 0, x + 0.4, -CANOPY_D + 1.4, CANOPY_H, 'steel')
                       for x in [EXP[0] + i * CL.BAY for i in range(int((IMP[1] - EXP[0]) / CL.BAY) + 1)]])
join('AirsideDoors', [box('AD', x - 3.0, -0.5, 0, x + 3.0, -0.25, 5.2, 'door') for x in CL.AIR_DOORS])
join('RollerDecks', [box('RD', x - 2.0, -6.0, 0, x + 2.0, -0.5, 0.5, 'steel') for x in CL.AIR_DOORS])
join('SpecialDoors', [box('SD', x - 2.0, -0.5, 0, x + 2.0, -0.25, 4.5, 'door') for x in CL.SPC_AIR_DOORS] +
     [box('SD', x - 2.5, -0.5, 0, x + 2.5, -0.25, 4.5, 'door') for x in CL.HORSE_AIR_DOORS])
# escape doors (1.0 x 2.1 m) on every face, a green exit light above each
esc, sig = [], []
for x in CL.ESC_AIR:
    esc.append(box('ED', x - 0.5, -0.55, 0, x + 0.5, -0.25, 2.1, 'esc')); sig.append(box('EX', x - 0.35, -0.7, 2.3, x + 0.35, -0.3, 2.6, 'exitsign'))
for x in CL.ESC_LAND:
    esc.append(box('ED', x - 0.5, D + 0.25, 0, x + 0.5, D + 0.55, 2.1, 'esc')); sig.append(box('EX', x - 0.35, D + 0.3, 2.3, x + 0.35, D + 0.7, 2.6, 'exitsign'))
for y in CL.ESC_EAST:
    esc.append(box('ED', L + 0.25, y - 0.5, 0, L + 0.55, y + 0.5, 2.1, 'esc')); sig.append(box('EX', L + 0.3, y - 0.35, 2.3, L + 0.7, y + 0.35, 2.6, 'exitsign'))
join('EscapeDoors', esc); join('EscapeSigns', sig)
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
# the border control post's own access lane (green hatching) from the gate side to its docks
b0, b1 = CL.BCP_LANE
marks += [box('BL', b0 - 0.2, D + 22, 0.0, b0, D + YARD_D - 2, 0.02, 'green'), box('BL', b1, D + 22, 0.0, b1 + 0.2, D + YARD_D - 2, 0.02, 'green')]
marks += [box('BH', b0, y, 0.0, b1, y + 0.4, 0.02, 'green') for y in range(int(D + 26), int(D + YARD_D - 4), 8)]
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
                            for i in range(12) for j in range(3)])
# #endregion

# #region ── Sign on the airside face of the high bay ──
cu = bpy.data.curves.new('Sign', 'FONT'); cu.body = 'CARGO TERMINAL'; cu.size = 5.0; cu.extrude = 0.15
cu.align_x = 'CENTER'; cu.align_y = 'CENTER'
so = bpy.data.objects.new('SignText', cu); COL.objects.link(so)
so.location = ((ETV[0] + ETV[1]) / 2, -0.63, H_MAIN + (H_ETV - H_MAIN) / 2 + 0.2)
so.rotation_euler = (math.pi / 2, 0, 0)
cu.materials.append(M['sign'])
dg = bpy.context.evaluated_depsgraph_get(); dg.update()
me = bpy.data.meshes.new_from_object(so.evaluated_get(dg)); loc, rot = so.location.copy(), so.rotation_euler.copy()
bpy.data.objects.remove(so); bpy.data.curves.remove(cu)
o = bpy.data.objects.new('SignText', me); COL.objects.link(o); o.location = loc; o.rotation_euler = rot
# #endregion

# #region ── The cargo apron in front, with the four freighter stands' lead-in lines ──
box('CargoApron', -30, -165, -0.06, 450, -40, -0.01, 'apron')    # APRON-CARGO, plan x 170-650
st = []
for sx in (60.0, 147.5, 235.0, 322.5):         # C1-C4 (plan x 260, 347.5, 435, 522.5 -> local 60 ... 322.5)
    st.append(box('LI', sx - 0.15, -160, 0.0, sx + 0.15, -45, 0.02, 'yellow'))
    st.append(box('SB', sx - 6, -47, 0.0, sx + 6, -46.4, 0.02, 'yellow'))
    st.append(box('WT', sx - 40, -125, 0.0, sx + 40, -124.7, 0.02, 'white'))
join('StandMarkings', st)
# #endregion

# #region ── UVs: box-projected in world metres per material (a texture tile = TILE metres), so nothing is stretched ──
def world_uvs(o):
    me = o.data; mw = o.matrix_world; nm = mw.to_3x3()
    bm = bmesh.new(); bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        mt = me.materials[f.material_index] if len(me.materials) else None
        t = MAT_TILE.get(mt.name, 1.0) if mt else 1.0
        n = nm @ f.normal; ax = max(range(3), key=lambda k: abs(n[k]))
        for lp in f.loops:
            p = mw @ lp.vert.co
            if ax == 2: u, v = p.x * (1 if n.z > 0 else -1), p.y            # floors, roofs: seams run along y (the slope)
            elif ax == 1: u, v = p.x * (-1 if n.y > 0 else 1), p.z           # long walls: joints horizontal, read the right way
            else: u, v = p.y * (1 if n.x > 0 else -1), p.z                  # end walls
            lp[uv].uv = (u / t, v / t)
        f.smooth = False
    bm.to_mesh(me); bm.free()


def drop_contacts():
    """Faces where two parts touch (a wall on the slab, a shelter against the wall, a floor on the ground): each is
    covered by the other part's face lying on it the other way round, so nobody can see it, and the flicker check counts
    such pairs. Every face lying wholly on another part's face (same plane, facing it) is taken out."""
    from collections import defaultdict
    planes = defaultdict(list)                              # (axis, plane in mm) -> [(object, sign, bbox)]
    meshes = {}
    for o in COL.objects:
        if o.type != 'MESH': continue
        bm = bmesh.new(); bm.from_mesh(o.data); bm.faces.ensure_lookup_table(); meshes[o.name] = (o, bm)
        mw = o.matrix_world
        for f in bm.faces:
            n = mw.to_3x3() @ f.normal; ax = max(range(3), key=lambda k: abs(n[k]))
            if abs(n[ax]) < 0.999: continue
            ps = [mw @ v.co for v in f.verts]; ot = [k for k in range(3) if k != ax]
            bb = (min(p[ot[0]] for p in ps), max(p[ot[0]] for p in ps), min(p[ot[1]] for p in ps), max(p[ot[1]] for p in ps))
            planes[(ax, round(ps[0][ax] * 1000))].append((o.name, 1 if n[ax] > 0 else -1, bb, f.index))
    gone = defaultdict(set); e = 0.001
    for key, fs in planes.items():
        near = fs + planes.get((key[0], key[1] - 1), []) + planes.get((key[0], key[1] + 1), [])
        for name, sg, bb, idx in fs:
            for n2, s2, b2, _ in near:
                if n2 != name and s2 == -sg and b2[0] - e <= bb[0] and bb[1] <= b2[1] + e and b2[2] - e <= bb[2] and bb[3] <= b2[3] + e:
                    gone[name].add(idx); break
    # Faces of two parts lying in the same plane facing the same way (a sign on the glazing, a bumper on a door): the
    # smaller face is lifted 2 mm off the plane so the two never flicker.
    lift = defaultdict(set)
    for key, fs in planes.items():
        near = fs + planes.get((key[0], key[1] - 1), []) + planes.get((key[0], key[1] + 1), [])
        for name, sg, bb, idx in fs:
            if idx in gone[name]: continue
            a = (bb[1] - bb[0]) * (bb[3] - bb[2])
            for n2, s2, b2, i2 in near:
                if n2 == name or i2 in gone[n2]: continue
                if min(bb[1], b2[1]) - max(bb[0], b2[0]) > e and min(bb[3], b2[3]) - max(bb[2], b2[2]) > e:
                    if a < (b2[1] - b2[0]) * (b2[3] - b2[2]) or (a == (b2[1] - b2[0]) * (b2[3] - b2[2]) and name < n2):
                        lift[name].add(idx); break
    for name, (o, bm) in meshes.items():
        if lift[name]:
            inv = o.matrix_world.inverted().to_3x3(); mw3 = o.matrix_world.to_3x3()
            for i in lift[name]:
                f = bm.faces[i]; d = inv @ ((mw3 @ f.normal).normalized() * 0.002)
                for v in f.verts: v.co += d
    print('faces lifted off shared planes:', sum(len(v) for v in lift.values()), flush=True)
    for name, (o, bm) in meshes.items():
        if gone[name] or lift[name]:
            bmesh.ops.delete(bm, geom=[bm.faces[i] for i in gone[name]], context='FACES_ONLY')
            bm.to_mesh(o.data)
        bm.free()
    print('contact faces taken out:', sum(len(v) for v in gone.values()), flush=True)


drop_contacts()
for o in COL.objects:
    if o.type == 'MESH': world_uvs(o)
# every slot filled (the import's rule): no mesh without a material
assert all(len(o.data.materials) and all(o.data.materials) for o in COL.objects if o.type == 'MESH'), 'empty material slot'
# #endregion

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'CargoTerminal.blend'))
# for the game: the airfield draws the apron, its stands and their lines itself (Data/airfield.json APRON-CARGO,
# C1-C4) - the model's own copies of that paving would lie on it and flicker, so the game's file leaves them out
for nm in ('CargoApron', 'StandMarkings', 'AirsideRoad', 'ULDLanes'):
    ob = bpy.data.objects.get(nm)
    if ob: bpy.data.objects.remove(ob)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'CargoTerminal.glb'), export_format='GLB')
print('saved', len(bpy.data.objects), 'objects', flush=True)
