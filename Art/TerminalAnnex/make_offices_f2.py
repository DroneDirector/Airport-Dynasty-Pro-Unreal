# The Floor 2 offices (owner 2026-11-21, Blueprints/Terminal/F2-Offices-v2.png and his final word): the open office floor
# over the old staff wing (Art/TerminalAnnex/make_annex_west.py: x 114..156, z 0..52, floor +6.01, ceiling +11.32)
# partitioned - plaster to 1 m, glass to 3.3 m, plaster to the ceiling on the corridor sides, plaster full height between
# the offices - into
#   your office (x 114..126.3, z 0..9; owner 2026-11-26, Tools/render_owner_office.py: moved onto the Grand Hall window,
#   its west wall the window itself, its south wall the forecourt windows; the walkway's dead-end stretch x 114..118.7,
#   z 0..9 went into it, the walkway north of the corridor stays), Offices 2, 3, 4 (x 126.3..139.1, 139.1..147.3, 147.3..156),
#   the staff corridor (z 9..12) and its arm north (x 133..136) to the stair and lift,
#   Operations (x 118.7..133, z 12..26) and the Academy (x 136..156, z 12..34.5),
#   the staff lounge (x 114..133, z 26..52, open) and the stair and lift lobby (x 133..156, z 34.5..52).
# Every partition meets the outer walls between their windows. Doors: copies of the terminal's own (Tools/terminal_edit.py).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_offices_f2.py
#   then Tools/import_annex.py annex_offices_solid annex_offices_glass
import bpy, bmesh, json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
MATS = {}
def mat(name):
    if name not in MATS:
        m = bpy.data.materials.new(name); MATS[name] = m
    return MATS[name]


class Piece:
    def __init__(self, name):
        self.bm = bmesh.new(); self.name = name; self.mats = []
        self.uv = self.bm.loops.layers.uv.new('UVMap')
        self.want = []        # (face, the way it must face, in Blender's axes): set after Blender's own guess
    def mi(self, m):
        if m not in self.mats: self.mats.append(m)
        return self.mats.index(m)
    def face(self, pts, m):
        vs = [self.bm.verts.new((-x, -z, y)) for (x, y, z) in pts]
        f = self.bm.faces.new(vs); f.material_index = self.mi(m)
        # world-space UVs (the terminal's masters lay their detail in world space anyway)
        for l, (x, y, z) in zip(f.loops, pts): l[self.uv].uv = (x + z, y)
        return f
    def box(self, x0, x1, y0, y1, z0, z1, m, sides='all'):
        c = lambda i, j, k: (x1 if i else x0, y1 if j else y0, z1 if k else z0)
        F = {'-y': [c(0,0,0), c(1,0,0), c(1,0,1), c(0,0,1)], '+y': [c(0,1,0), c(0,1,1), c(1,1,1), c(1,1,0)],
             '-x': [c(0,0,0), c(0,0,1), c(0,1,1), c(0,1,0)], '+x': [c(1,0,0), c(1,1,0), c(1,1,1), c(1,0,1)],
             '-z': [c(0,0,0), c(0,1,0), c(1,1,0), c(1,0,0)], '+z': [c(0,0,1), c(1,0,1), c(1,1,1), c(0,1,1)]}
        out = {'-y': (0, 0, -1), '+y': (0, 0, 1), '-x': (1, 0, 0), '+x': (-1, 0, 0), '-z': (0, 1, 0), '+z': (0, -1, 0)}   # (plan -> Blender: x -> -x, z -> -y, y -> z)
        for k, f in F.items():
            if sides == 'all' or k in sides: self.want.append((self.face(f, m), out[k]))
    def finish(self):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        # every box face out of its box, whatever Blender guessed: a lone floor panel could come out facing down, and you
        # fell through it (owner 2026-11-16: stuck at the escalators' top, the new stair landing)
        flipped = 0
        for f, w in self.want:
            f.normal_update()
            if f.normal.x * w[0] + f.normal.y * w[1] + f.normal.z * w[2] < 0: f.normal_flip(); flipped += 1
        print('ANNEX %s: %d box faces turned the right way round' % (self.name, flipped))
        me = bpy.data.meshes.new(self.name); self.bm.to_mesh(me); self.bm.free()
        for m in self.mats: me.materials.append(mat(m))
        o = bpy.data.objects.new(self.name, me); bpy.context.scene.collection.objects.link(o)
        return o


W, T = 'MI_T_white', 0.2                      # walls 20 cm

def wall_x(x, z0, z1, y0, y1, m_in=W, m_out=W, holes=()):
    """A wall in the plane x (thickness T), from z0 to z1, y0 to y1, with rectangular holes (z0, z1, y0, y1)."""
    cuts_z = sorted({z0, z1} | {h[0] for h in holes} | {h[1] for h in holes})
    for za, zb in zip(cuts_z, cuts_z[1:]):
        spans = [(y0, y1)]
        for h in holes:
            if h[0] <= za and zb <= h[1]:
                spans = [s for sp in spans for s in ((sp[0], min(sp[1], h[2])), (max(sp[0], h[3]), sp[1])) if s[1] - s[0] > 1e-4]
        for ya, yb in spans: S.box(x - T / 2, x + T / 2, ya, yb, za, zb, m_in)

def wall_z(z, x0, x1, y0, y1, m=W, holes=()):
    cuts_x = sorted({x0, x1} | {h[0] for h in holes} | {h[1] for h in holes})
    for xa, xb in zip(cuts_x, cuts_x[1:]):
        spans = [(y0, y1)]
        for h in holes:
            if h[0] <= xa and xb <= h[1]:
                spans = [s for sp in spans for s in ((sp[0], min(sp[1], h[2])), (max(sp[0], h[3]), sp[1])) if s[1] - s[0] > 1e-4]
        for ya, yb in spans: S.box(xa, xb, ya, yb, z - T / 2, z + T / 2, m)

def slab(x0, x1, z0, z1, ytop, top='MI_T_terrazzo', under='MI_T_offwhite', thick=0.6):
    S.box(x0, x1, ytop - 0.02, ytop, z0, z1, top)
    S.box(x0, x1, ytop - thick, ytop - 0.02, z0, z1, 'MI_T_concrete', sides=('-x', '+x', '-z', '+z'))
    S.box(x0, x1, ytop - thick - 0.04, ytop - thick, z0, z1, under)

def glass_front_z(z, x0, x1, y0, y1, step=1.78, transoms=(3.0,)):
    """A glass front in the plane z: panes, mullions every `step`, transoms, head and sill (as the Grand Hall's)."""
    G.box(x0, x1, y0, y1, z - 0.012, z + 0.012, 'MI_T_glass')
    n = max(1, round((x1 - x0) / step)); st = (x1 - x0) / n
    for i in range(n + 1):
        x = x0 + i * st
        S.box(max(x0, x - 0.04), min(x1, x + 0.04), y0, y1, z - 0.075, z + 0.075, 'MI_T_mullion')
    for ty in transoms + (y0, y1):
        S.box(x0, x1, ty - 0.04, ty + 0.04, z - 0.075, z + 0.075, 'MI_T_mullion')


S = Piece('annex_offices_solid'); G = Piece('annex_offices_glass')
FY, CY = 6.01, 11.32                          # Floor 2's floor and ceiling
PL = 'MI_T_plaster'
SILL, HEAD = FY + 1.0, FY + 3.3               # the glass band
DOOR_H = FY + 2.25

def partition(axis, at, a, b, glass=True, doors=()):
    """A partition on the line `axis` = at (axis 'x': a wall in the plane x = at running in z from a to b; 'z': in the plane
    z = at running in x), 12 cm thick, floor to ceiling; doors: (c0, c1) openings 2.25 m high."""
    t = 0.06
    def box(u0, u1, y0, y1, m, P=S, th=t):
        if axis == 'x': P.box(at - th, at + th, y0, y1, u0, u1, m)
        else: P.box(u0, u1, y0, y1, at - th, at + th, m)
    cuts = sorted({a, b} | {d[0] for d in doors} | {d[1] for d in doors})
    for u0, u1 in zip(cuts, cuts[1:]):
        if any(d[0] <= u0 and u1 <= d[1] for d in doors):
            if glass: box(u0, u1, DOOR_H, HEAD, 'MI_T_glass', G, 0.012); box(u0, u1, HEAD, CY, PL)
            else: box(u0, u1, DOOR_H, CY, PL)
            box(u0, u1, DOOR_H - 0.04, DOOR_H, 'MI_T_mullion', S, 0.075)
            continue
        if not glass: box(u0, u1, FY, CY, PL); continue
        box(u0, u1, FY, SILL, PL); box(u0, u1, HEAD, CY, PL)
        box(u0, u1, SILL, HEAD, 'MI_T_glass', G, 0.012)
        n = max(1, round((u1 - u0) / 1.5)); st = (u1 - u0) / n
        for i in range(n + 1):                                    # mullions every 1.5 m or so
            u = u0 + i * st
            box(max(u0, u - 0.03), min(u1, u + 0.03), SILL, HEAD, 'MI_T_mullion', S, 0.04)

# ---- your office and Offices 2-4, by the forecourt windows (between-office walls solid: pictures hang on them) ----
XS = [114.0, 126.3, 139.1, 147.3]
for x in XS[1:]: partition('x', x, 0.1, 8.94, glass=False)
partition('z', 9.0, 114.014, 155.9, doors=[(123.9, 125.1), (132.2, 133.4), (140.4, 141.6), (148.6, 149.8)])
# your office's west wall is the Grand Hall window (Art/TerminalAnnex/make_annex_west.py, floor to ceiling, 6 m over the
# hall): a glass guard inside it, a stainless shoe, clear glass to 1.0 m and a steel handrail on top, 25 cm clear of the
# window's frames (x 114.075) and of the forecourt wall and the corridor partition at its ends
GX, GZ0, GZ1 = 114.32, 0.2, 8.84
S.box(GX - 0.04, GX + 0.04, FY, FY + 0.1, GZ0, GZ1, 'MI_T_steel')
G.box(GX - 0.006, GX + 0.006, FY + 0.1, FY + 0.96, GZ0, GZ1, 'MI_T_glass')
S.box(GX - 0.025, GX + 0.025, FY + 0.96, FY + 1.0, GZ0, GZ1, 'MI_T_steel')
for z in (GZ0, GZ1):                                             # the rail's ends into the walls
    zc = z - 0.2 if z < 1 else z + 0.14
    S.box(GX - 0.02, GX + 0.02, FY + 0.97, FY + 0.99, min(zc, z), max(zc, z), 'MI_T_steel')
# ---- Operations (x 118.7..133, z 12..26) ----
partition('z', 12.0, 118.64, 133.06, doors=[(125.4, 126.6)])
partition('x', 118.7, 12.06, 26.0, glass=True)
partition('z', 26.0, 118.64, 133.06, glass=False)
partition('x', 133.0, 12.06, 25.94, glass=False)                   # its clocks' wall
# ---- the Academy (x 136..156, z 12..34.5) ----
partition('z', 12.0, 135.94, 155.9, doors=[(139.4, 140.6), (151.4, 152.6)])
partition('x', 136.0, 12.06, 34.5, glass=True)
partition('z', 34.5, 135.94, 155.9, glass=False)

# every flat face, for Tools/check_floors.py
FLAT = []
for P_ in (S, G):
    for f in P_.bm.faces:
        ys_ = [v.co.z for v in f.verts]
        if max(ys_) - min(ys_) < 1e-4:
            xs_ = [-v.co.x for v in f.verts]; zs_ = [-v.co.y for v in f.verts]
            FLAT.append([P_.name, round(min(xs_), 3), round(max(xs_), 3), round(ys_[0], 3), round(min(zs_), 3), round(max(zs_), 3), P_.mats[f.material_index]])
os.makedirs(os.path.join(HERE, '..', '..', 'Saved'), exist_ok=True)
json.dump(FLAT, open(os.path.join(HERE, '..', '..', 'Saved', 'annex-flat-annex_offices.json'), 'w'))
objs = [S.finish(), G.finish()]
for o in objs:
    print('OFFICES', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'OfficesF2.blend'))
print('OFFICES saved')
