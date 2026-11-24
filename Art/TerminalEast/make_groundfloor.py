# Terminal east, part 3, step 1: the Floor 1 (ground floor) rooms of the East Wing x 156..551.2, z 0..52, geometry only.
# The rooms' partitions, doorways and ceilings already stand in the shell (make_terminal_east.py: 'the partitions, ground floor',
# 'the ceilings, ground floor' - 5.4 m rooms, walls 20 cm, plaster, doorways 1.2 / 1.6 / 1.8 m x 2.25 m). They are NOT built again
# here. What this adds, in the plan's metres and the terminal's own materials, inside the enclosed rooms of Data/terminal-plan.json:
#   east_g_solid_*   each enclosed room's own floor finish over the hall marble (8 mm, held 12 mm off the walls, cut round the
#                    columns), a steel skirting round its walls (stopping at the doorways), and every 1.2..1.8 m doorway of those
#                    rooms lined: oak jambs and head (4 cm, 1 cm proud of both wall faces) and a steel threshold strip.
# The doorways stay open gaps (no leaves): people already walk through them; the doors come with the fittings (later steps).
# The public halls (East Hall, promenade, customs, claim, passport hall, coach station, link) keep the shell's marble.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalEast/make_groundfloor.py
#   then Tools/import_east.py east_g_solid_1 ..., Tools/terminal_edit.py (Tools/terminal_groundfloor_edit.py)
import bpy, bmesh, json, os, sys, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(ROOT, 'Art', 'TerminalAnnex', 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
OAK, STEEL = 'MI_T_oak', 'MI_T_steel'
TILE, CARPET, TERRAZZO, CONCRETE = 'MI_T_tile', 'MI_T_carpetGrey', 'MI_T_terrazzo', 'MI_T_concrete'
S = Piece('east_g_solid')
Y0 = 0.01                                              # the shell's floor top (its marble)
T = 0.2                                                # the shell's partitions
SK, SKH, FH = 0.012, 0.1, 0.008                        # skirting depth and height, the finish's thickness
LIN, PROUD = 0.04, 0.01                                # door lining: jamb width, how far it stands out of each wall face
COLS = [(x, z) for a, x, y, z in json.load(open(os.path.join(HERE, 'place.json')))['cols'] if a == 'east_col_g']
CR = 0.42                                              # half the square cut round a column (its foot: r 0.36)

# ---- the rooms: inner wall faces x0, x1, z0, z1; the floor; the doorways per side ('-z' the face at z0 ...) as (u0, u1)
def d(c, w=1.2): return (c - w / 2, c + w / 2)
ROOMS = [
    ('F1-EAST-WC',          160.1, 177.9, 40.1, 51.9, TILE,     {'-z': [d(163.0), d(169.0), d(175.0)]}),
    ('F1-EAST-FAMILY',      178.1, 189.9, 40.1, 51.9, TILE,     {'-z': [d(184.0, 1.6)]}),
    ('F1-EAST-TROLLEY',     190.1, 209.9, 40.1, 51.9, CONCRETE, {'-z': [(196.0, 204.0)]}),
    ('F1-EAST-BAGSERV',     210.1, 227.9, 40.1, 51.9, TERRAZZO, {'-z': [(216.0, 222.0)]}),
    ('F1-CUSTOMS-E-OFFICE', 228.1, 235.9, 40.1, 51.9, CARPET,   {'-z': [d(232.0)]}),
    ('F1-PC-OFFICE',        312.1, 323.9, 6.1, 19.7,  CARPET,   {'+z': [d(318.0)], '+x': [d(13.0)]}),
    ('F1-PC-SECOND',        358.1, 371.9, 41.1, 50.9, CARPET,   {'-z': [d(364.0)]}),
    ('F1-CREW-CENTRE',      372.1, 403.9, 6.1, 25.9,  CARPET,   {'-z': [d(388.0, 1.8)], '+z': [d(388.0, 1.6)], '+x': [d(15.0, 1.6)]}),
    ('F1-CANTEEN',          404.1, 435.9, 6.1, 25.9,  TILE,     {'-z': [d(420.0, 1.8)], '+z': [d(420.0, 1.6)], '-x': [d(15.0, 1.6)], '+x': [d(15.0, 1.6)]}),
    ('F1-GROUND-OFF',       436.1, 469.9, 6.1, 25.9,  CARPET,   {'-z': [d(450.0, 1.8)], '+z': [d(450.0, 1.6)], '-x': [d(15.0, 1.6)]}),
    ('F1-LOCKERS',          372.1, 403.9, 26.1, 51.9, TILE,     {'-z': [d(388.0, 1.6)], '-x': [d(30.0, 1.6)], '+x': [d(38.0, 1.6)]}),
    ('F1-STAFFSEC',         404.1, 435.9, 26.1, 51.9, TERRAZZO, {'-z': [d(420.0, 1.6)], '-x': [d(38.0, 1.6)], '+x': [d(38.0, 1.6)]}),
    ('F1-POLICE',           436.1, 469.9, 26.1, 51.9, CARPET,   {'-z': [d(450.0, 1.6)], '-x': [d(38.0, 1.6)]}),
]

def minus(sp, a, b): return [s for p, q in sp for s in ((p, min(q, a)), (max(p, b), q)) if s[1] - s[0] > 1e-4]

def floor(x0, x1, z0, z1, m):
    """The finish x0..x1, z0..z1 (already held off the walls), a square cut round every column standing in it; side faces only on
    its outer edge and round the cuts (nothing between its own cells)."""
    holes = [(x - CR, x + CR, z - CR, z + CR) for x, z in COLS if x0 < x < x1 and z0 < z < z1]
    xs = sorted({x0, x1} | {v for h in holes for v in h[:2]}); zs = sorted({z0, z1} | {v for h in holes for v in h[2:]})
    def inh(a, b, c, e): return any(h[0] <= a and b <= h[1] and h[2] <= c and e <= h[3] for h in holes)
    for a, b in zip(xs, xs[1:]):
        for c, e in zip(zs, zs[1:]):
            if inh(a, b, c, e): continue
            sd = ['+y']
            if abs(a - x0) < 1e-6 or inh(2 * a - b, a, c, e): sd.append('-x')
            if abs(b - x1) < 1e-6 or inh(b, 2 * b - a, c, e): sd.append('+x')
            if abs(c - z0) < 1e-6 or inh(a, b, 2 * c - e, c): sd.append('-z')
            if abs(e - z1) < 1e-6 or inh(a, b, e, 2 * e - c): sd.append('+z')
            S.box(a, b, Y0, Y0 + FH, c, e, m, sides=tuple(sd))
    return len(holes)

def skirting(side, at, u0, u1, holes):
    """Steel skirting along one inner face (the plane at), from u0 to u1 along it, stopping at the doorways."""
    sg = 1 if side[0] == '+' else -1; a, b = sorted((at, at - sg * SK))
    looks = ('-' if sg > 0 else '+') + side[1]                                 # (into the room)
    for p, q in _cut(u0, u1, holes):
        ends = ('-x', '+x') if side[1] == 'z' else ('-z', '+z')
        if side[1] == 'z': S.box(p, q, Y0, Y0 + SKH, a, b, STEEL, sides=(looks, '+y') + ends)
        else: S.box(a, b, Y0, Y0 + SKH, p, q, STEEL, sides=(looks, '+y') + ends)
def _cut(u0, u1, holes):
    sp = [(u0, u1)]
    for h0, h1 in holes: sp = minus(sp, h0, h1)
    return sp

LINED = {}
def lining(side, at, h0, h1, top=2.25):
    """Oak jambs and head in a doorway of the wall whose room face is at (the wall's middle 10 cm behind it), a steel threshold."""
    sg = 1 if side[0] == '+' else -1; c = at + sg * T / 2; key = (side[1], round(c, 2), round(h0, 2))
    if key in LINED: return                                                   # (a doorway between two rooms: once)
    LINED[key] = 1; p, q = c - T / 2 - PROUD, c + T / 2 + PROUD
    if side[1] == 'z':
        S.box(h0, h0 + LIN, Y0, top, p, q, OAK, sides=('+x', '-z', '+z')); S.box(h1 - LIN, h1, Y0, top, p, q, OAK, sides=('-x', '-z', '+z'))
        S.box(h0 + LIN, h1 - LIN, top - LIN, top, p, q, OAK, sides=('-y', '-z', '+z'))
        S.box(h0 + LIN, h1 - LIN, Y0, Y0 + 0.006, c - T / 2, c + T / 2, STEEL, sides=('+y', '-z', '+z'))
    else:
        S.box(p, q, Y0, top, h0, h0 + LIN, OAK, sides=('+z', '-x', '+x')); S.box(p, q, Y0, top, h1 - LIN, h1, OAK, sides=('-z', '-x', '+x'))
        S.box(p, q, top - LIN, top, h0 + LIN, h1 - LIN, OAK, sides=('-y', '-x', '+x'))
        S.box(c - T / 2, c + T / 2, Y0, Y0 + 0.006, h0 + LIN, h1 - LIN, STEEL, sides=('+y', '-x', '+x'))

NCUT = 0
for name, x0, x1, z0, z1, m, doors in ROOMS:
    # skirting: the z faces run the room's full width, the x faces stop at them (no two strips in one corner)
    skirting('-z', z0, x0, x1, doors.get('-z', [])); skirting('+z', z1, x0, x1, doors.get('+z', []))
    skirting('-x', x0, z0 + SK, z1 - SK, doors.get('-x', [])); skirting('+x', x1, z0 + SK, z1 - SK, doors.get('+x', []))
    NCUT += floor(x0 + SK, x1 - SK, z0 + SK, z1 - SK, m)
    for side, hs in doors.items():
        at = {'-z': z0, '+z': z1, '-x': x0, '+x': x1}[side]
        for h0, h1 in hs:
            if h1 - h0 <= 2.0: lining(side, at, h0, h1)
# the faces round a doorway's lining in the wall at the ZN side, the promenade wall's side etc. are the shell's (left as they are)

# ======================================================================================================== out: in pieces along the building (Lumen)
BINX = [300.0, 400.0]
def split(o):
    bm = bmesh.new(); bm.from_mesh(o.data)
    for x in BINX: bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(-x, 0, 0), plane_no=(1, 0, 0), dist=1e-5)
    grp = {}; bm.faces.ensure_lookup_table(); bm.faces.index_update()
    for f in bm.faces: grp.setdefault(sum(-f.calc_center_median().x > b for b in BINX) + 1, []).append(f.index)
    out = []
    for n, idx in sorted(grp.items()):
        c = bm.copy(); c.faces.ensure_lookup_table(); c.faces.index_update(); keep = set(idx)
        bmesh.ops.delete(c, geom=[f for f in c.faces if f.index not in keep], context='FACES')
        used = sorted({f.material_index for f in c.faces}); mp = {u: k for k, u in enumerate(used)}
        for f in c.faces: f.material_index = mp[f.material_index]
        me = bpy.data.meshes.new('%s_%d' % (o.name, n)); c.to_mesh(me); c.free()
        for u in used: me.materials.append(o.data.materials[u])
        q = bpy.data.objects.new(me.name, me); bpy.context.scene.collection.objects.link(q); out.append(q)
    bm.free(); bpy.data.objects.remove(o); return out
for f_ in glob.glob(os.path.join(HERE, 'east_g_*.fbx')): os.remove(f_)
BIGP = []
for o in split(S.finish()):
    assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
    print('GROUND', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    BIGP.append(o.name)
json.dump({'big': BIGP, 'rooms': [r[0] for r in ROOMS]}, open(os.path.join(HERE, 'ground_place.json'), 'w'), indent=0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'GroundFloor.blend'))
print('GROUND saved', BIGP, 'linings', len(LINED), 'column cuts', NCUT)
