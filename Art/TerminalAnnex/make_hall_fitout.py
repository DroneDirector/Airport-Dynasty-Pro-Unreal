# West Wing part 3 (to-do 234/213, sheet 2b: Blueprints/Terminal/Changes/v2/s2_ground.py): the Grand Hall's own self-service
# fittings. Plan metres: x along the building, z into it from the landside front (hall x 42..113.9, z 0..30, floor top 0.012).
#   annex_hallfit_solid    one model in the plan's metres: the airline service / ticket desk in the hall's north-east corner
#                          (x 108.3..113.8, z 24.6..29.65: clear of bag drop point 8, which ends at z 24.2), its staff worktop,
#                          screens, back fitment with SERVICE DESK; the two trolley bays' rails, posts and TROLLEYS signs
#                          (x 43.0..48.0 and 108.8..113.8, z 2.8..4.0); the steel side fences that close each e-gate bank to
#                          the security wall (so nobody walks round the gates)
#   annex_hallfit_egate    a boarding-pass e-gate cabinet (instanced: five per security door, 1.0 m lanes between them, at
#                          z 26.35..27.45; the reader at the hall's end, the flaps retracted: not modelled, the lanes stay open)
#   annex_hallfit_trolley  a luggage trolley (instanced: four in each bay, side by side, not nested)
#   annex_hallfit_bagdrop  a self bag drop machine (instanced: six at island 2's east end, on six old placeholder points)
#   reused, instanced      east_g_fit_ochair (the desk's staff chairs); the self check-in kiosks are the owner's own
#                          SM_CheckinKiosk, placed by Data/terminal-props.json (not here)
#   hallfit_place.json     the instanced pieces' places
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_hall_fitout.py
#   then Tools/import_annex.py annex_hallfit_solid annex_hallfit_egate annex_hallfit_trolley annex_hallfit_bagdrop,
#   Tools/terminal_edit.py (Tools/terminal_hallfit_edit.py)
import bpy, bmesh, json, math, os
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
csrc = open(os.path.join(ROOT, 'Art', 'TerminalEast', 'make_concourse.py'), encoding='utf-8').read()
BLK, WHITE = 'MI_T_black', 'MI_T_signWhite'
S = Piece('annex_hallfit_solid'); SG = S
exec(csrc[csrc.index('# ---- letters ----'):csrc.index('# ---- walls with')])        # text(), band() (on SG)
OAK, MAR, STEEL, RUB, DARK, NAVY = 'MI_T_oak', 'MI_T_marbleDark', 'MI_T_steel', 'MI_T_rubber', 'MI_T_dark', 'MI_T_navy'
GLOSS, SCR, LW, LG = 'MI_T_whiteGloss', 'MI_T_screenBlack', 'MI_T_l_FFFFFF', 'MI_T_l_20FF40'
FL = 0.012                                              # the hall's floor top (its terrazzo)
PL_ = []
ON_FLOOR = ('+y', '-x', '+x', '-z', '+z')

def B(P, x0, x1, y0, y1, z0, z1, m, sides='all'):
    """a box; one standing on the floor has no bottom face (nothing lies on the floor's face)"""
    if y0 <= FL + 0.0005 and sides == 'all': sides = ON_FLOOR
    P.box(x0, x1, y0, y1, z0, z1, m, sides=sides)
def place(nm, asset, x, z, r=0, y=FL): PL_.append(['%s-%03d' % (nm, sum(1 for p in PL_ if p[0].startswith(nm + '-')) + 1), asset, round(x, 4), y, round(z, 4), r])
def no(*k): return tuple(s for s in ('+y', '-y', '+x', '-x', '+z', '-z') if s not in k)
def band(n, at, u0, u1, y0, y1, s, size, m=WHITE, d=0.08):
    """make_concourse.py's band() without the band's hidden back face, its letters 1.5 mm off the band (find_zfight.py clean)"""
    c = (u0 + u1) / 2; sg = 1 if n[0] == '+' else -1; a, b = sorted((at, at + sg * d)); back = ('-' if sg > 0 else '+') + n[1]
    if n[1] == 'z': SG.box(u0, u1, y0, y1, a, b, BLK, sides=no(back)); text(SG, s, c, (y0 + y1) / 2, at + sg * (d + 0.001), n, size, m, maxw=(u1 - u0) - 0.3)
    else: SG.box(a, b, y0, y1, u0, u1, BLK, sides=no(back)); text(SG, s, at + sg * (d + 0.001), (y0 + y1) / 2, c, n, size, m, maxw=(u1 - u0) - 0.3)

# ======================================================================================================== service desk
# customers at its west (x < 108.35), the staff behind it; a 1.0 m high counter with a 0.76 m accessible end at the north
S.box(108.3, 113.0, FL, 1.05, 24.6, 24.8, OAK, sides=no('-y'))                          # the south panel (staff way in east of it)
B(S, 108.35, 109.15, FL, 1.0, 24.8, 28.6, OAK, sides=no('-y', '-z', '+y'))                # the high counter
S.box(108.3, 109.2, 1.0, 1.04, 24.8, 28.6, MAR, sides=no('-z'))
B(S, 108.4, 109.1, FL, 0.76, 28.6, 29.6, OAK, sides=no('-y', '-z', '+y'))               # the accessible low end
S.box(108.35, 109.15, 0.76, 0.80, 28.6, 29.65, MAR, sides=no('-z'))
S.box(109.15, 109.7, 0.72, 0.75, 24.8, 29.6, OAK, sides=no('-x', '-z'))                   # the staff's worktop
band('-x', 108.35, 25.3, 28.1, 0.45, 0.8, 'SERVICE DESK', 0.17, WHITE, d=0.02)
for zc in (25.6, 27.2, 29.1):                                                           # the staff's screens, facing them (+x)
    S.box(109.25, 109.5, 0.75, 0.762, zc - 0.1, zc + 0.1, DARK, sides=no('-y'))
    S.box(109.36, 109.40, 0.762, 1.0, zc - 0.02, zc + 0.02, DARK, sides=no('-y', '+x'))
    S.box(109.40, 109.44, 0.95, 1.3, zc - 0.27, zc + 0.27, BLK)
    S.box(109.44, 109.445, 0.98, 1.27, zc - 0.25, zc + 0.25, SCR, sides=no('-x'))
B(S, 113.3, 113.8, FL, 2.6, 25.0, 29.6, NAVY)                                            # the back fitment
band('-x', 113.3, 25.2, 29.4, 2.0, 2.5, 'SERVICE DESK', 0.32, WHITE, d=0.03)
band('-x', 113.3, 25.6, 29.0, 1.55, 1.85, 'TICKETS - REBOOKING', 0.15, WHITE, d=0.02)
for zc in (25.6, 27.2, 29.1): place('DESK/CHAIR', 'east_g_fit_ochair', 110.2, zc, -90)

# ======================================================================================================== trolley bays
def bay(x0, x1, zc, sign_x):
    xm = (x0 + x1) / 2
    for zl in (zc - 0.45, zc + 0.45):
        posts = ((x0, x0 + 0.06), (xm - 0.03, xm + 0.03), (x1 - 0.06, x1))
        for a, b in posts: B(S, a, b, FL, 0.9, zl - 0.03, zl + 0.03, STEEL)
        for (a0, a1), (b0, b1) in zip(posts, posts[1:]):
            for y in (0.4, 0.85): S.box(a1, b0, y, y + 0.04, zl - 0.02, zl + 0.02, STEEL, sides=no('-x', '+x'))
    B(S, sign_x - 0.04, sign_x + 0.04, FL, 2.0, zc - 0.04, zc + 0.04, STEEL, sides=no('-y', '+y'))
    S.box(sign_x - 0.4, sign_x + 0.4, 2.0, 2.35, zc - 0.03, zc + 0.03, NAVY)
    for nn, zz in (('+z', zc + 0.031), ('-z', zc - 0.031)): text(S, 'TROLLEYS', sign_x, 2.175, zz, nn, 0.16, WHITE, maxw=0.7)
    for dx in (-1.8, -0.6, 0.6, 1.8): place('TROLLEY', 'annex_hallfit_trolley', xm + dx, zc, 90)
bay(43.0, 48.0, 3.4, 42.55)                             # (the hall's west wall x 42, its east wall 113.9)
bay(108.8, 113.8, 3.4, 108.35)

# ======================================================================================================== e-gate fences
for dx_ in (62.0, 78.0):
    for fx in (dx_ - 3.1, dx_ + 3.1):
        posts = [(z - 0.025, z + 0.025) for z in (26.6, 28.15, 29.7)]
        for a, b in posts: B(S, fx - 0.025, fx + 0.025, FL, 1.05, a, b, STEEL)
        for (a0, a1), (b0, b1) in zip(posts, posts[1:]):
            for y in (0.45, 1.0): S.box(fx - 0.015, fx + 0.015, y, y + 0.03, a1, b0, STEEL, sides=no('-z', '+z'))
    for dx in (-2.8, -1.4, 0.0, 1.4, 2.8): place('EGATE', 'annex_hallfit_egate', dx_ + dx, 26.9, 0)

# ======================================================================================================== e-gate cabinet
E = Piece('annex_hallfit_egate')                        # its middle at 0, along z; passengers come from -z (the hall)
E.box(-0.18, 0.18, 0.0, 0.08, -0.53, 0.53, DARK, sides=('-x', '+x', '-z', '+z'))         # the kick plinth
E.box(-0.2, 0.2, 0.08, 0.98, -0.55, 0.55, GLOSS, sides=no('+y'))
E.box(-0.21, 0.21, 0.98, 1.0, -0.56, 0.56, STEEL)
E.box(-0.12, 0.12, 1.0, 1.03, -0.5, -0.3, BLK, sides=no('-y'))                           # the boarding pass reader
E.box(-0.08, 0.08, 1.03, 1.035, -0.47, -0.33, LW, sides=no('-y'))
E.box(-0.1, 0.1, 0.8, 0.86, -0.555, -0.55, LG, sides=no('+z'))                           # the green go light at the hall end
E.box(-0.06, 0.06, 0.8, 0.86, 0.55, 0.555, LG, sides=no('-z'))                           # and at the far end

# ======================================================================================================== trolley
T = Piece('annex_hallfit_trolley')                      # its middle at 0, its push handle at -z, from the floor
def wheel(P, cx, cy, cz, r, w, m):
    """an octagonal castor wheel, its axle along x"""
    a = [k * math.pi / 4 for k in range(8)]
    ring = lambda x: [(x, cy - r * math.cos(t), cz + r * math.sin(t)) for t in a]
    L, R = ring(cx - w / 2), ring(cx + w / 2)
    P.want.append((P.face(list(reversed(L)), m), (1, 0, 0))); P.want.append((P.face(R, m), (-1, 0, 0)))
    for k in range(8):
        j = (k + 1) % 8; am = (a[k] + a[j]) / 2
        P.want.append((P.face([L[k], L[j], R[j], R[k]], m), (0, -math.sin(am), -math.cos(am))))
for wx in (-0.24, 0.24):
    for wz in (-0.4, 0.4):
        wheel(T, wx, 0.064, wz, 0.06, 0.03, RUB)
        for s in (-1, 1): T.box(wx + s * 0.022 - 0.005, wx + s * 0.022 + 0.005, 0.064, 0.14, wz - 0.015, wz + 0.015, STEEL, sides=no('+y'))
        T.box(wx - 0.03, wx + 0.03, 0.14, 0.16, wz - 0.03, wz + 0.03, STEEL, sides=no('+y'))
T.box(-0.28, 0.28, 0.16, 0.19, -0.48, 0.48, STEEL)                                       # the deck
for s in (-1, 1):
    x0, x1 = sorted((s * 0.25, s * 0.28))
    T.box(x0, x1, 0.19, 1.0, -0.47, -0.44, STEEL, sides=no('-y', '+y'))                  # the uprights
    T.box(x0, x1, 0.55, 0.58, -0.44, 0.45, STEEL, sides=no('-z'))                        # the top side rails
    T.box(x0, x1, 0.19, 0.55, 0.42, 0.45, STEEL, sides=no('-y', '+y'))                   # the front posts
T.box(-0.30, 0.30, 1.0, 1.04, -0.48, -0.43, RUB)                                         # the grip
T.box(-0.25, 0.25, 0.40, 0.43, 0.42, 0.45, STEEL, sides=no('-x', '+x'))                  # the bag stop
T.box(-0.25, 0.25, 0.19, 0.7, -0.465, -0.445, STEEL, sides=no('-x', '+x', '-y'))         # the back rest

# ======================================================================================================== bag drop machine
# sheet 2b item 2: six machines, 12.0 x 3.5 m, at island 2's east end (x 105.25..108.75, z 11.3..23.3, facing the hall's -x).
# One 2.0 m module (1.98 wide: 2 cm between machines), its middle at 0, its customer side at +z: the weigh belt where the bag
# is laid, its side guards, the tunnel hood (black opening, green go light), the kiosk tower (screen, printer slot, BAG DROP),
# the closed back cabinet over the take-away belt (the bags go on under the floor: sheet 2b's bag tunnel). Each one is put on
# one of the old placeholder points F1-BAGDROP/POINT-2,3,5,6,7,8 (POINT-1 and -4 stay, spare) - Tools/terminal_hallfit_edit.py
D = Piece('annex_hallfit_bagdrop')
B(D, -0.85, 0.35, 0.0, 0.40, 0.15, 1.65, STEEL, sides=('-x', '+x', '+z'))                  # the weigh belt's body
D.box(-0.82, 0.32, 0.40, 0.43, 0.15, 1.62, RUB, sides=no('-y', '-z'))                      # its belt
for x0, x1 in ((-0.88, -0.85), (0.35, 0.38)): D.box(x0, x1, 0.43, 1.05, 0.15, 1.65, GLOSS, sides=no('-z'))   # the side guards
D.box(-0.95, 0.45, 0.0, 1.6, -0.75, 0.15, GLOSS, sides=('+y', '-x', '+x', '+z'))           # the tunnel hood
D.box(-0.8, 0.3, 0.43, 1.2, 0.155, 0.16, BLK, sides=no('-z'))                              # its opening (the rubber curtain)
D.box(-0.8, 0.3, 1.35, 1.42, 0.155, 0.16, LG, sides=no('-z'))                              # the go light
B(D, 0.45, 0.95, 0.0, 1.45, 0.9, 1.4, GLOSS)                                                # the kiosk tower
D.box(0.5, 0.9, 1.0, 1.35, 1.40, 1.405, SCR, sides=no('-z'))                               # its screen
D.box(0.55, 0.85, 0.8, 0.84, 1.40, 1.405, BLK, sides=no('-z'))                             # the bag tag printer's slot
D.box(0.45, 0.95, 1.45, 1.75, 1.1, 1.2, NAVY, sides=no('-y'))                              # the header plate
text(D, 'BAG DROP', 0.7, 1.6, 1.2015, '+z', 0.09, WHITE, maxw=0.44)
B(D, -0.99, 0.99, 0.0, 1.6, -1.75, -0.75, GLOSS)                                            # the back cabinet
BAGDROP_POINTS = {}
for k, pt in enumerate((5, 2, 6, 3, 7, 8)):
    place('BAGDROP', 'annex_hallfit_bagdrop', 107.0, 12.3 + 2.0 * k, -90); BAGDROP_POINTS[PL_[-1][0]] = 'F1-BAGDROP/POINT-%d' % pt

# ======================================================================================================== out
OUT = []
for P_ in (S, E, T, D):
    o = P_.finish()
    assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
    print('HFIT', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    OUT.append(o.name)
json.dump({'big': ['annex_hallfit_solid'], 'place': PL_, 'points': BAGDROP_POINTS, 'models': OUT}, open(os.path.join(HERE, 'hallfit_place.json'), 'w'), indent=0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'HallFitout.blend'))
print('HFIT saved', len(OUT), 'models,', len(PL_), 'places')
