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
import cargo_doors as CD
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
    wall=mat('M_Cargo_WallCladding', (0.74, 0.76, 0.77), tex='CladdingAged'),
    wall2=mat('M_Cargo_WallHighBay', (0.74, 0.76, 0.77), tex='CladdingAged'),
    band=mat('M_Cargo_CladdingBand', (0.06, 0.22, 0.45), tex='CladdingBandAged'),
    plinth=mat('M_Cargo_Plinth', (0.50, 0.50, 0.49), tex='Grime'),
    boom=mat('M_Cargo_BoomWhite', (0.82, 0.83, 0.83), tex='PaintWhite'),
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
    # (Galvanised's scan colour is near black: see steel_sets. A reimport keeps existing materials: after changing a set
    #  here run Tools/repoint_mi_textures.py on them, else they keep the old pictures)
    steel=mat('M_Cargo_Steel', (0.62, 0.64, 0.66), 0.4, 1.0, tex='SteelGalv'),
    chequer=mat('M_Cargo_ChequerPlate', (0.55, 0.57, 0.59), 0.45, 1.0, tex='ChequerPlate'),
    apron=mat('M_Cargo_Apron', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    yard=mat('M_Cargo_TruckCourt', (0.16, 0.16, 0.16), tex='Asphalt'),
    slab=mat('M_Cargo_Slab', (0.50, 0.50, 0.49), tex='FloorConcrete'),
    yellow=mat('M_Cargo_MarkYellow', (0.90, 0.68, 0.05), tex='LineYellow'),
    white=mat('M_Cargo_MarkWhite', (0.85, 0.86, 0.85), tex='LineWhite'),
    red=mat('M_Cargo_MarkRed', (0.75, 0.10, 0.08), tex='LineRed'),
    green=mat('M_Cargo_MarkGreen', (0.10, 0.50, 0.22), tex='LineGreen'),
    pv=mat('M_Cargo_Solar', (0.03, 0.05, 0.12), 0.15, 0.2, tex='Solar'),
    skylight=mat('M_Cargo_SkylightGlass', (0.55, 0.62, 0.66), 0.04, 0.0, 0.0, 0.30),
    hvac=mat('M_Cargo_Plant', (0.42, 0.45, 0.47), tex='PaintGrey'),
    uld=mat('M_Cargo_ULD', (0.70, 0.72, 0.74), 0.4, 1.0, tex='Aluminium'),
    truck=mat('M_Cargo_Truck', (0.82, 0.83, 0.83), tex='PaintWhite'),
    cab=mat('M_Cargo_TruckCab', (0.10, 0.25, 0.50), tex='PaintBlue'),
    fence=mat('M_Cargo_Fence', (0.62, 0.64, 0.66), 0.5, 1.0, tex='SteelGalv'),
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
    lo = min(x1 - x0, y1 - y0, z1 - z0)
    if m not in FLAT and lo >= 0.06:                 # step B3: every solid part gets bevelled edges that catch the light
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(0.02, 0.15 * lo), segments=1, affect='EDGES')
    me = bpy.data.meshes.new('SM_Cargo_' + name); bm.to_mesh(me); bm.free(); me.materials.append(M[m])
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    return o


FLAT = {'yard', 'apron', 'slab', 'yellow', 'white', 'red', 'green', 'glass', 'skylight', 'exitsign', 'sign', 'pv'} |        {k for k in M if k.startswith('fl_')}


def joinm(name, objs):
    """Like join, keeping each part's own material (several slots, none empty)."""
    objs = [o for o in objs if o]
    if not objs: return None
    mats = []
    for o in objs:
        if o.data.materials[0] not in mats: mats.append(o.data.materials[0])
    bm = bmesh.new()
    for o in objs:
        me = o.data.copy(); me.transform(o.matrix_world); k = mats.index(o.data.materials[0])
        for pg in me.polygons: pg.material_index = k
        bm.from_mesh(me); bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new('SM_Cargo_' + name); bm.to_mesh(me); bm.free()
    for mt in mats: me.materials.append(mt)
    for o in objs:
        d = o.data; bpy.data.objects.remove(o); bpy.data.meshes.remove(d)
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    return o


def cyl(name, cx, cy, z0, r, h, m, seg=16):
    """An upright cylinder (fans, pipes)."""
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=h)
    for v in bm.verts: v.co = Vector((v.co.x + cx, v.co.y + cy, v.co.z + z0 + h / 2))
    me = bpy.data.meshes.new('SM_Cargo_' + name); bm.to_mesh(me); bm.free(); me.materials.append(M[m])
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    return o


def hcyl(name, x0, x1, cy, cz, r, m, seg=12):
    """A cylinder lying along x from x0 to x1 (rollers)."""
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=x1 - x0)
    for v in bm.verts: v.co = Vector((v.co.z + (x0 + x1) / 2, v.co.y + cy, v.co.x + cz))
    me = bpy.data.meshes.new('SM_Cargo_' + name); bm.to_mesh(me); bm.free(); me.materials.append(M[m])
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    return o


def guide(e, s, c, z0, z1, along, m='steel'):
    """A sectional door's guide channel at an opening's edge e (s = +1/-1 into the opening), the curtain on line c:
    a web on the reveal and two flanges either side of the panels (they run 42 mm thick in it)."""
    def bx(u0, u1, v0, v1, n):
        u0, u1 = min(u0, u1), max(u0, u1)
        return box(n, u0, v0, z0, u1, v1, z1, m) if along == 'x' else box(n, v0, u0, z0, v1, u1, z1, m)
    return [bx(e, e + s * 0.008, c - 0.034, c + 0.034, 'GDw'), bx(e + s * 0.008, e + s * 0.06, c - 0.034, c - 0.026, 'GDf'),
            bx(e + s * 0.008, e + s * 0.06, c + 0.026, c + 0.034, 'GDf')]


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
GAPS = CD.wall_gaps()


def gapped(along, a, b, c0, c1, m, gaps):
    """A lower outer wall from a to b (along x or y; c0..c1 across) with its door openings (g0, g1, z bottom, z top) cut:
    solid between them, a sill below a raised opening (truck docks), a lintel above each up to LOW."""
    def bx(u0, u1, z0, z1):
        return box('OW', u0, c0, z0, u1, c1, z1, m) if along == 'x' else box('OW', c0, u0, z0, c1, u1, z1, m)
    out, u = [], a
    for g0, g1, zb, zt in gaps:
        if g1 <= a or g0 >= b: continue
        g0, g1 = max(g0, a), min(g1, b)
        if g0 > u: out.append(bx(u, g0, 0, LOW))
        if zb > 0: out.append(bx(g0, g1, 0, zb))
        out.append(bx(g0, g1, zt, LOW)); u = g1
    if b > u: out.append(bx(u, b, 0, LOW))
    return out


BAND = 1.0                                   # the coloured cladding band, one panel high, right over the 6 m split
for zk, (x0, x1), h, _ in CL.ZONES:
    m = 'wall2' if zk == 'etv' else 'wall'
    for y0, y1 in ((-0.3, 0.0), (D, D + 0.3)):
        lo += gapped('x', x0, x1, y0, y1, m, GAPS['s' if y0 < 0 else 'n']); bd.append(box('OB', x0, y0, LOW, x1, y1, LOW + BAND, 'band'))
        hi.append(box('OW', x0, y0, LOW + BAND, x1, y1, h, m))
for x0, x1, h in ((-0.3, 0.0, H_OFF), (L, L + 0.3, H_SPEC)):
    lo += gapped('y', -0.3, D + 0.3, x0, x1, 'wall', GAPS['e'] if x0 > 0 else []); bd.append(box('OB', x0, -0.3, LOW, x1, D + 0.3, LOW + BAND, 'band'))
    hi.append(box('OW', x0, -0.3, LOW + BAND, x1, D + 0.3, h, 'wall'))
join('OuterWallsLow', lo); join('CladdingBand', bd); join('OuterWallsHigh', hi)
join('Roofs', [box('Roof', x0 - 0.3, -0.3, h, x1 + 0.3, D + 0.3, h + 0.6, 'roof') for _, (x0, x1), h, _ in CL.ZONES])
# step B3: the outer roller doors' guide channels in both reveals, the spring shaft's head box inside over each one,
# and outside (not at docks, where the shelter is) a head flashing and jamb trims round the opening
od, trim = [], []
for rid, kind, cx, cy, z0, ax, w, h in CD.rollers():
    if kind == 'sectional': continue
    hw, zt, inn = w / 2, z0 + h, (1 if cy < 0 else -1)         # inn: +y on the south face (inside is y > 0)
    for s_ in (-1, 1): od += guide(cx + s_ * hw, -s_, cy, z0, zt, 'x')
    yi = 0.0 if cy < 0 else D; yo = -0.3 if cy < 0 else D + 0.3
    od.append(box('DH', cx - hw - 0.1, min(yi, yi + inn * 0.45), zt, cx + hw + 0.1, max(yi, yi + inn * 0.45), min(zt + 0.55, LOW - 0.02), 'hvac'))
    if kind != 'dock':
        a, b = sorted((yo, yo - inn * 0.06))
        trim.append(box('DF', cx - hw - 0.15, a, zt, cx + hw + 0.15, b, zt + 0.15, 'steel'))
        trim += [box('DT', x0_, a, z0, x1_, b, zt, 'steel') for x0_, x1_ in ((cx - hw - 0.15, cx - hw), (cx + hw, cx + hw + 0.15))]
joinm('OuterDoorGuides', od); join('OuterDoorTrims', trim)
# a concrete plinth 0.6 m high, 2 cm proud, round the foot of the outer walls (dirtiest at the ground); it stops short
# of every opening down to the ground, with room for the trims
def plinth_run(along, a, b, c0, c1, gaps):
    out, u = [], a
    for g0, g1, zb, zt in gaps:
        if zb > 0 or g1 <= a or g0 >= b: continue
        if g0 - 0.16 > u: out.append((u, g0 - 0.16))
        u = max(u, g1 + 0.16)
    if b > u: out.append((u, b))
    return [box('PL', u0, c0, 0, u1, c1, 0.6, 'plinth') if along == 'x' else box('PL', c0, u0, 0, c1, u1, 0.6, 'plinth') for u0, u1 in out]
join('Plinth', plinth_run('x', -0.32, L + 0.32, -0.32, -0.3, GAPS['s']) + plinth_run('x', -0.32, L + 0.32, D + 0.3, D + 0.32, GAPS['n']) +
     plinth_run('y', -0.3, D + 0.3, -0.32, -0.3, []) + plinth_run('y', -0.3, D + 0.3, L + 0.3, L + 0.32, GAPS['e']))
# gutters under the eaves and downpipes every 30 m (none in front of an opening or a fire wall; on the canopy sides
# they stop on the canopy roof)
gut, dp = [], []
def clear(x, gaps, m=1.0):
    return all(not (g0 - m < x < g1 + m) for g0, g1, _, _ in gaps) and all(abs(x - fx) > 1.0 for fx, _ in CL.FIRE_WALLS)
for _, (x0, x1), h, _ in CL.ZONES:
    for side in ('s', 'n'):
        ya, yb = (-0.48, -0.3) if side == 's' else (D + 0.3, D + 0.48)
        gut.append(box('GU', x0, ya, h + 0.25, x1, yb, h + 0.5, 'steel'))
        pa, pb = (-0.44, -0.34) if side == 's' else (D + 0.34, D + 0.44)
        for x in [x0 + 4 + 30 * i for i in range(int((x1 - x0 - 6) / 30) + 1)]:
            if not clear(x, GAPS[side]): continue
            if side == 's': zb = CANOPY_H + 1.0 if EXP[0] - 0.5 <= x <= IMP[1] + 0.5 else 0.0
            else: zb = 6.6 if EXP[0] - 0.5 <= x <= L + 0.5 else 0.0
            dp.append(box('DP', x - 0.05, pa, zb, x + 0.05, pb, h + 0.25, 'steel'))
join('Gutters', gut); join('Downpipes', dp)
# wall louvres (plant room air) high on the landside face, above the canopy
lv = []
for x in range(int(EXP[0]) + 12, int(L) - 6, 24):
    if not clear(x, GAPS['n'], 0.8): continue
    lv.append(box('LVf', x - 0.7, D + 0.3, 7.4, x + 0.7, D + 0.34, 8.4, 'hvac'))
    lv += [box('LVs', x - 0.65, D + 0.34, 7.48 + k * 0.12, x + 0.65, D + 0.40, 7.52 + k * 0.12, 'steel') for k in range(8)]
joinm('WallLouvres', lv)
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
            # (its 1.2 m escape door: a hinged D-CARGO-FW-.. door, built with the other moving doors at the end)
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
lx0, lx1, lh = CD.LOBBY            # a glass vestibule; its sliding doors (D-CARGO-LOBBY-IN/OUT) are built at the end
join('OfficeEntrance', [box('OE', 12.0, D + 0.3, 0, 12.1, D + 3.95, 4.2, 'glass'), box('OE', 17.9, D + 0.3, 0, 18.0, D + 3.95, 4.2, 'glass'),
                        box('OE', 12.0, D + 3.95, 0, lx0, D + 4.05, 4.2, 'glass'), box('OE', lx1, D + 3.95, 0, 18.0, D + 4.05, 4.2, 'glass'),
                        box('OE', lx0, D + 3.95, lh, lx1, D + 4.05, 4.2, 'glass')])
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
            # (the leaf itself: a hinged D-CARGO-..ROOM-.. door, built with the other moving doors at the end)
            sign = (cx - 0.3, cy - 0.2, z0 + 2.25, cx + 0.3, cy + 0.2, z0 + 2.5) if o == 'h' else (cx - 0.2, cy - 0.3, z0 + 2.25, cx + 0.2, cy + 0.3, z0 + 2.5)
            lit.append(box(tag + 'X', *sign, 'exitsign'))
        else:
            hd = 4.5 if w >= 4 else 3.4
            for s in (-1, 1):
                j = (cx + s * hw - 0.15, cy - 0.3, z0, cx + s * hw + 0.15, cy + 0.3, z0 + hd + 0.4) if o == 'h' else \
                    (cx - 0.3, cy + s * hw - 0.15, z0, cx + 0.3, cy + s * hw + 0.15, z0 + hd + 0.4)
                e = cx + s * hw if o == 'h' else cy + s * hw         # the opening's edge on this side
                pj = (min(e, e + s * 0.15), cy - 0.3, z0, max(e, e + s * 0.15), cy + 0.3, z0 + hd + 0.4) if o == 'h' else                      (cx - 0.3, min(e, e + s * 0.15), z0, cx + 0.3, max(e, e + s * 0.15), z0 + hd + 0.4)
                amber.append(box(tag + 'J', *pj, 'door'))
                amber += guide(e, -s, cx if o != 'h' else cy, z0, z0 + hd, 'x' if o == 'h' else 'y')
            hb = (cx - hw - 0.15, cy - 0.35, z0 + hd, cx + hw + 0.15, cy + 0.35, z0 + hd + 0.6) if o == 'h' else \
                 (cx - 0.35, cy - hw - 0.15, z0 + hd, cx + 0.35, cy + hw + 0.15, z0 + hd + 0.6)
            amber.append(box(tag + 'H', *hb, 'door'))
            amber.append(box(tag + 'T', *((cx - hw, cy - 0.3, z0 + 0.03, cx + hw, cy + 0.3, z0 + 0.06) if o == 'h' else
                                         (cx - 0.3, cy - hw, z0 + 0.03, cx + 0.3, cy + hw, z0 + 0.06)), 'door'))   # threshold strip
    return green, amber, lit


g, a_, lt = door_parts(CL.doors_from_rooms(ground, SKX, SKY), 0.0, 'G')
join('RoomDoorsPersonnel', g); joinm('RoomDoorsSectional', a_); join('ExitSigns', lt)
# office block upper floors (slab + its partitions and doors each)
for f, rooms in CL.UPPER.items():
    z = CL.FLOOR_Z[f]
    box(f'Office_F{f}_Slab', 0.0, 0.0, z, OFF[1], D, z + 0.3, 'slab')
    rr = [(x0, y0, x1, y1, 4.3, doors, 1.0) for _, x0, y0, x1, y1, k, doors in rooms if k != 'aisle']
    w_ = [box('UW', *w, 'part') for w in CL.walls_from_rooms(rr, z + 0.3, (0.0, OFF[1]), SKY)]
    g, a_, lt = door_parts(CL.doors_from_rooms(rr, (0.0, OFF[1]), SKY), z + 0.3, f'U{f}')
    join(f'Office_F{f}_Walls', w_); join(f'Office_F{f}_Doors', lt)
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
    # (open high bay: the cargo system moves ULDs in and out, so only a third of the lanes keep a fixed ULD, at the
    #  lane's back place - Data/cargo-slots.json staticULD - the rest stays empty)
    back, sgn = (ya, 1.0) if ya < CL.ETV_AISLE[0] else (yb, -1.0)
    yc = back + sgn * (0.4 + CD.DEPTH_PITCH * 0.5)
    for i, x in enumerate(CL.ETV_RACK_X):
        for j, z in enumerate(CL.ETV_LEVELS):
            if CD.KEEP_STATIC(i, j, int(ya)):
                eq_uld.append(box('EU', x + 0.06, yc - CD.ULD[1] / 2, z + 0.1, x + 0.06 + CD.ULD[0], yc + CD.ULD[1] / 2, z + 0.1 + (1.6 if (i + j) % 2 else 2.4), 'uld'))
for ya in (7.5, 12.5, 17.5):                                                                  # roller-bed buffer rows
    eq_steel.append(box('RB', ETV[0] + 1, ya, 0, ETV[1] - 1, ya + 3.2, 0.5, 'steel'))
# (owner: no box stand-ins for machines or vehicles - the ETV masts/carriages and the forklifts come later as real models)
# marked floor squares for overflow ULDs in the high bay's landside strip (Data/cargo-slots.json overflowSquares)
ovf = []
for a, b, c, d in CD.overflow_squares():
    ovf += [box('OV', a, b, 0.03, c, b + 0.1, 0.05, 'yellow'), box('OV', a, d - 0.1, 0.03, c, d, 0.05, 'yellow'),
            box('OV', a, b + 0.1, 0.03, a + 0.1, d - 0.1, 0.05, 'yellow'), box('OV', c - 0.1, b + 0.1, 0.03, c, d - 0.1, 0.05, 'yellow')]
join('OverflowSquares', ovf)
# border control post: horse stalls (4.05 x 4.7 m), stalls for other hoofed animals, kennels; radioactive bays
eq_steel += [box('HS', x0, y0, 0, x1, y1, 2.2, 'steel') for x0, y0, x1, y1 in CL.HORSE_STALLS]
eq_steel += [box('CS', x0, y0, 0, x1, y1, 1.8, 'steel') for x0, y0, x1, y1 in CL.CATTLE_STALLS]
eq_steel += [box('KN', 406 + i * 2.3, 31, 0, 408 + i * 2.3, 33, 1.4, 'steel') for i in range(6)]
eq_steel += [box('KN', 406 + i * 2.3, 37, 0, 408 + i * 2.3, 39, 1.4, 'steel') for i in range(6)]
eq_rack += [box('RA', x0, y0, 0, x1, y1, 1.2, 'lead') for x0, y0, x1, y1 in CL.RADIO_BAYS]
eq_cargo += [box('PB', 287 + i * 3.6, 60, 0, 289.6 + i * 3.6, 62.4, 1.6, 'cargo') for i in (0, 1, 9)] + \
            [box('PB', 33 + i * 4.2, 66, 0, 35.8 + i * 4.2, 68.4, 1.6, 'cargo') for i in (0, 8, 15)]   # a few; the floor is for real cargo
join('RollerDecksAndStations', eq_steel); join('ULDsInside', eq_uld); join('Racks', eq_rack)
join('PalletsInside', eq_cargo); join('Screening', eq_xray)
# #endregion

# #region ── Airside: canopy on columns, ULD transfer doors with roller decks, the airside service road ──
box('AirsideCanopy', EXP[0], -CANOPY_D, CANOPY_H, IMP[1], -0.3, CANOPY_H + 1.0, 'canopy')
join('CanopyColumns', [box('CC', x - 0.4, -CANOPY_D + 0.6, 0, x + 0.4, -CANOPY_D + 1.4, CANOPY_H, 'steel')
                       for x in [EXP[0] + i * CL.BAY for i in range(int((IMP[1] - EXP[0]) / CL.BAY) + 1)]])
# (the ULD doors' curtains are the game's roller doors: Data/cargo-doors.json R-CARGO-AIR/SPC/HORSE-..)
# ULD roller decks in front of the doors (2026-11-28: were bare 0.5 m slabs): a steel bed between two side rails, and
# rollers (70 mm, every 250 mm) running across between the rails, their tops 10 mm under the rails' tops
rd, rr = [], []
for x in CL.AIR_DOORS:
    rd += [box('RD', x - 1.9, -6.0, 0, x + 1.9, -0.5, 0.40, 'steel'),
           box('RDr', x - 2.0, -6.0, 0, x - 1.9, -0.5, 0.5, 'steel'), box('RDr', x + 1.9, -6.0, 0, x + 2.0, -0.5, 0.5, 'steel')]
    rr += [hcyl('RDt', x - 1.9, x + 1.9, -5.8 + k * 0.25, 0.455, 0.035, 'steel') for k in range(21)]
joinm('RollerDecks', rd)
join('RollerDeckRollers', rr)
# escape doors (1.0 x 2.1 m) on every face, a green exit light above each
esc, sig = [], []
for x in CL.ESC_AIR:
    sig.append(box('EX', x - 0.35, -0.7, 2.3, x + 0.35, -0.3, 2.6, 'exitsign'))
for x in CL.ESC_LAND:
    sig.append(box('EX', x - 0.35, D + 0.3, 2.3, x + 0.35, D + 0.7, 2.6, 'exitsign'))
for y in CL.ESC_EAST:
    sig.append(box('EX', L + 0.3, y - 0.35, 2.3, L + 0.7, y + 0.35, 2.6, 'exitsign'))
join('EscapeSigns', sig)   # (the leaves: hinged D-CARGO-ESC-.. doors)
box('AirsideRoad', -40, -40, -0.05, L + 60, 0, 0.0, 'yard')
join('ULDLanes', [box('UL', x - 2.5, -38, 0.0, x + 2.5, -30, 0.02, 'white') for x in CL.AIR_DOORS] +
     [box('ULb', -40, -40.2, 0.0, L + 60, -39.9, 0.02, 'yellow'), box('ULc', -40, -14.2, 0.0, L + 60, -13.9, 0.02, 'yellow')])
# staged ULDs on their lanes, standing on their own base (2026-11-28: they stood on a floating 'box dolly' slab and,
# joined under that slab's steel material, rendered as one black box; owner: no box dollies - real ones come later)
uld = []
for i, x in enumerate(CL.AIR_DOORS[::3]):
    uld += [box('U', x - 1.5, -37, 0.0, x + 1.5, -31.5, 2.5 if i % 2 else 1.6, 'uld')]
# outside the east end: the empty ULD store (stacked) and the ULD repair shed; dollies and GSE at the west end
ex = CL.OUTSIDE
for i in range(8):
    for j in range(3):
        uld.append(box('EU', ex[0][1] + 1 + i * 5.1, ex[0][2] + 1 + j * 6.4, 0, ex[0][1] + 4.2 + i * 5.1, ex[0][2] + 5.8 + j * 6.4, 1.7 * (1 + (i + j) % 2), 'uld'))
join('ULDsOutside', uld)
box('ULDRepairShed', *ex[1][1:3], 0, *ex[1][3:5], 7.0, 'wall')
# (owner: no box dollies - real ones come later)
# #endregion

# #region ── Landside: truck docks with levellers and shelters, drive-in doors, the truck court and its gate ──
DK = CL.DOCKS + CL.SPC_DOCKS
# (dock door curtains: the game's roller doors R-CARGO-DOCK-..; the shelter is a frame of pads round the opening)
# dock shelters: a frame round the opening, slit black rubber curtains (two sides and a top) with yellow guide
# stripes for the driver, and laminated rubber bumpers on steel plates below the sill
sh = []
for x in DK:
    sh += [box('DSf', x - 2.0, D + 0.3, 1.2, x - 1.5, D + 1.1, 4.7, 'shelter'), box('DSf', x + 1.5, D + 0.3, 1.2, x + 2.0, D + 1.1, 4.7, 'shelter'),
           box('DSf', x - 2.0, D + 0.3, 4.7, x + 2.0, D + 1.1, 5.3, 'shelter')]
    for s_ in (-1, 1):
        a, b = sorted((x + s_ * 2.0, x + s_ * 1.15))
        sh.append(box('DSc', a, D + 1.1, 1.25, b, D + 1.12, 4.7, 'shelter'))
        a, b = sorted((x + s_ * 1.55, x + s_ * 1.45))
        sh.append(box('DSy', a, D + 1.12, 1.25, b, D + 1.125, 4.7, 'yellow'))
    sh += [box('DSt', x - 1.15 + i * 0.29, D + 1.1, 4.0, x - 1.15 + i * 0.29 + 0.28, D + 1.12, 4.7, 'shelter') for i in range(8)]
    sh.append(box('DSy', x - 2.0, D + 1.1, 4.95, x + 2.0, D + 1.105, 5.05, 'yellow'))
joinm('DockShelters', sh)
bp = []
for x in DK:
    for s_ in (-1, 1):
        a = x + s_ * 1.3
        bp.append(box('DBp', a - 0.17, D + 0.3, 0.6, a + 0.17, D + 0.32, 1.15, 'steel'))
        bp += [box('DBr', a - 0.13, D + 0.32, 0.64 + k * 0.1, a + 0.13, D + 0.46, 0.72 + k * 0.1, 'shelter') for k in range(5)]
        bp.append(box('DBf', a - 0.14, D + 0.46, 0.62, a + 0.14, D + 0.47, 1.13, 'steel'))
joinm('DockBumpers', bp)
join('DockLevellers', [box('DL', x - 1.4, D - 3.0, 0.0, x + 1.4, D, 0.08, 'chequer') for x in DK])
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
Y = D + YARD_D
joinm('Gatehouse', [box('GHb', 152, Y - 10, 0, 160, Y - 4, 1.0, 'wall'), box('GHg', 152.05, Y - 9.95, 1.0, 159.95, Y - 4.05, 2.8, 'glass'),
                    box('GHf', 152, Y - 10, 2.8, 160, Y - 4, 3.2, 'band')] +
      [box('GHm', x - 0.03, Y - 10.0, 1.0, x + 0.03, Y - 9.95, 2.8, 'steel') for x in (153.5, 155.5, 157.5)] +
      [box('GHm', x - 0.03, Y - 4.05, 1.0, x + 0.03, Y - 4.0, 2.8, 'steel') for x in (153.5, 155.5, 157.5)])
box('GatehouseRoof', 150, Y - 12, 3.2, 176, Y - 2, 3.7, 'canopy')
# the truck court gate: the barrier cabinet, its boom (own part 'GateBoom', origin on the pivot so it can swing up)
# in red and white 1 m stripes, and the rest post at the far end
joinm('GateBarrierCabinet', [box('GBc', 160.9, Y - 1.35, 0.08, 161.9, Y - 0.95, 1.1, 'fork'),
                             box('GBk', 160.85, Y - 1.4, 0, 161.95, Y - 0.9, 0.08, 'steel'),
                             box('GRp', 173.5, Y - 0.95, 0, 173.7, Y - 0.75, 0.86, 'fork')])
bm_ = [box('GBh', 161.25, Y - 0.93, 0.8, 161.75, Y - 0.77, 1.1, 'steel')]
bm_ += [box('GBs', 161.75 + i, Y - 0.92, 0.88, 161.75 + i + 1 if i < 11 else 174.0, Y - 0.78, 1.02, 'red' if i % 2 else 'boom') for i in range(12)]
GATE_BOOM = joinm('GateBoom', bm_); GATE_PIVOT = Vector((161.5, Y - 0.85, 0.95))
# (owner: no box lorries at the docks - real trucks come later)
# #endregion

# #region ── Roof: skylight strips, smoke vents, solar panels, the special block's refrigeration plant ──
join('Skylights', [box('SK', x0 + 4, y, h + 0.6, x1 - 4, y + 2.5, h + 1.4, 'skylight') for (x0, x1), h in ((EXP, H_MAIN), (IMP, H_MAIN))
                   for y in (12, 34, 56)])
join('SkylightKerbs', [b for (x0, x1), h in ((EXP, H_MAIN), (IMP, H_MAIN)) for y in (12, 34, 56) for b in (
    box('SKk', x0 + 3.85, y - 0.15, h + 0.6, x1 - 3.85, y, h + 0.95, 'steel'), box('SKk', x0 + 3.85, y + 2.5, h + 0.6, x1 - 3.85, y + 2.65, h + 0.95, 'steel'),
    box('SKk', x0 + 3.85, y, h + 0.6, x0 + 4, y + 2.5, h + 0.95, 'steel'), box('SKk', x1 - 4, y, h + 0.6, x1 - 3.85, y + 2.5, h + 0.95, 'steel'))])
join('SmokeVents', [box('SV', x, y, h + 0.6, x + 3, y + 3, h + 1.8, 'hvac') for (x0, x1), h in ((EXP, H_MAIN), (IMP, H_MAIN), (ETV, H_ETV))
                    for x in range(int(x0) + 8, int(x1) - 6, 20) for y in (24, 44)])
join('SolarPanels', [box('PV', ETV[0] + 5, y, H_ETV + 0.9, ETV[1] - 5, y + 4.6, H_ETV + 1.5, 'pv') for y in range(6, int(D) - 8, 7)])
# the special block's refrigeration plant: condensing units with their fan rings, on steel rails
cu = []
for i in range(6):
    for j in range(3):
        x0_, y0_, z_ = 350 + i * 18, 10 + j * 22, H_SPEC + 0.6
        cu += [box('RPr', x0_ + 0.5, y0_ + d, z_, x0_ + 11.5, y0_ + d + 0.2, z_ + 0.25, 'steel') for d in (1.0, 8.8)]
        cu.append(box('RPu', x0_, y0_, z_ + 0.25, x0_ + 12, y0_ + 10, z_ + 2.25, 'hvac'))
        for a in range(4):
            for b in range(2):
                fx, fy = x0_ + 1.5 + a * 3, y0_ + 2.5 + b * 5
                cu += [cyl('RPf', fx, fy, z_ + 2.25, 1.2, 0.35, 'steel', 24), cyl('RPg', fx, fy, z_ + 2.6, 1.05, 0.01, 'shelter', 24)]
joinm('RefrigerationPlant', cu)
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


def door_nodes():
    """Every hinged and sliding door as its own named node tree for ABabylonModel's door system:
    D-CARGO-<..> (an empty at the door's foot, turned so its -y is the side the leaf swings to) -> hinge_<id> (an
    empty on the hinge side) -> Leaf_<id> (the leaf, its own part: one mesh per size, shared so the game instances it;
    a later pass may swap that mesh without touching the wiring). Sliding glass doors: D-CARGO-LOBBY-.. ->
    box_<id>_L / _R, a unit glass leaf scaled to size (the door system reads leaf size and slide from the scale)."""
    shared = {}
    def leaf_mesh(w, h):
        k = (round(w, 2), round(h, 2))
        if k not in shared:
            bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
            for v in bm.verts:
                v.co = Vector(((v.co.x + 0.5) * w + 0.02, v.co.y * 0.05, (v.co.z + 0.5) * h + 0.01))
            me = bpy.data.meshes.new('SM_Cargo_DoorLeaf_%d_%d' % (round(w * 100), round(h * 100))); bm.to_mesh(me); bm.free()
            me.materials.append(M['esc'])
            t = bpy.data.objects.new('tmp_uv', me); COL.objects.link(t); world_uvs(t); bpy.data.objects.remove(t)
            shared[k] = me
        return shared[k]
    def empty(name, loc, rz=0.0, parent=None):
        e = bpy.data.objects.new(name, None); COL.objects.link(e); e.parent = parent; e.location = loc; e.rotation_euler = (0, 0, rz)
        return e
    turn = {'-y': 0.0, '+y': math.pi, '+x': math.pi / 2, '-x': -math.pi / 2}
    hd = CD.hinged()
    for did, cx, cy, z0, out, w, h in hd:
        d = empty(did, (cx, cy, z0), turn[out])
        hg = empty('hinge_' + did, (-w / 2, 0, 0), 0, d)
        o = bpy.data.objects.new('Leaf_' + did, leaf_mesh(w - 0.04, h)); COL.objects.link(o); o.parent = hg
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0); gm = bpy.data.meshes.new('SM_Cargo_GlassLeaf'); bm.to_mesh(gm); bm.free()
    gm.materials.append(M['glass']); t = bpy.data.objects.new('tmp_uv', gm); COL.objects.link(t); world_uvs(t); bpy.data.objects.remove(t)
    sl = CD.sliding()
    for did, cx, cy, z0, w, h in sl:
        d = empty(did, (cx, cy, z0))
        for side, s in (('L', -1), ('R', 1)):
            o = bpy.data.objects.new('box_%s_%s' % (did, side), gm); COL.objects.link(o); o.parent = d
            o.location = (s * w / 4, 0, (h - 0.05) / 2 + 0.01); o.scale = (w / 2 - 0.02, 0.05, h - 0.05)
    print('moving doors: %d hinged, %d sliding' % (len(hd), len(sl)), flush=True)


drop_contacts()
for o in COL.objects:
    if o.type == 'MESH': world_uvs(o)
for o in COL.objects:                      # round parts shade smooth (world_uvs set every face flat)
    if o.name in ('RollerDeckRollers',):
        for f in o.data.polygons: f.use_smooth = abs(f.normal.x) < 0.9
door_nodes()
# the gate boom's origin on its pivot (the boom swings up about the model's y axis there)
from mathutils import Matrix
GATE_BOOM.data.transform(Matrix.Translation(-GATE_PIVOT)); GATE_BOOM.location = GATE_PIVOT
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
