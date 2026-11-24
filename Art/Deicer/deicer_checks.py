# The second half of make_deicer.py: the moving parts through their whole range against everything else, then the small trucks' own joining and saving.
TUR, BOOM, CAB = ('Tur_', 'Lift'), ('BoomBase', 'BoomSec', 'ChainLink', 'HoseLift'), ('Cabin', 'Joy_', 'SprayPedal', 'Noz', 'HoseNozzle', 'HoseTip')
ML = TUR + BOOM + CAB
def bv(o, m=None): m = m or o.matrix_world; return BVHTree.FromPolygons([m @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
def pose(slew, lift, ext):
    """Matrices of the superstructure's groups. slew: degrees about z (0 = stowed, over the tanks), lift: degrees up, ext: metres out (0 .. 6, half to each section)."""
    S_ = Matrix.Translation((SX, 0, 0)) @ Matrix.Rotation(math.radians(slew), 4, 'Z') @ Matrix.Translation((-SX, 0, 0)); Lm = Matrix.Translation((PX, 0, PZ)) @ Matrix.Rotation(math.radians(lift), 4, 'Y') @ Matrix.Translation((-PX, 0, -PZ))
    B = S_ @ Lm; T0 = Vector((TX, 0, PZ)); t = Lm @ (T0 + Vector((-ext, 0, 0))) - T0
    return {'tur': S_, 'base': B, 's2': B @ Matrix.Translation((-ext / 2, 0, 0)), 's3': B @ Matrix.Translation((-ext, 0, 0)), 'cab': S_ @ Matrix.Translation(t)}
def pgroup(n): return 'tur' if n.startswith('Tur_') else 's2' if n.startswith('BoomSec2') else 's3' if n.startswith('BoomSec3') else 'base' if n.startswith('BoomBase') else 'cab' if n.startswith(CAB) else None
SKIP = ('ChainLink', 'HoseLift', 'HoseTip', 'HoseNozzle', 'Lift')
def boom_check():
    bpy.context.view_layer.update(); fixed = {o.name: bv(o) for o in ALL if not o.name.startswith(ML)}; mv = [o for o in ALL if pgroup(o.name) and not o.name.startswith(SKIP)]
    for sl, li, ex in [(0, 0, 0), (0, .5, 0), (0, 3, 0), (0, 6, 0), (0, 12, 0), (0, 20, 0), (0, 30, 0), (0, 60, 0), (0, 20, 6)] + [(a, 15, 0) for a in (30, 60, 90, 120, 150, 180, -60, -120)] + [(a, 25, 3) for a in (90, 180)] + [(180, 60, 6), (90, 45, 6)]:
        P_ = pose(sl, li, ex); T = [(o.name, pgroup(o.name), bv(o, P_[pgroup(o.name)] @ o.matrix_world)) for o in mv]; h = sorted({(a, b) for a, g, t in T for b, f in fixed.items() if t.overlap(f)}); pairs = {('base', 's2'), ('s2', 's3'), ('base', 's3'), ('tur', 'base'), ('s3', 'cab'), ('tur', 'cab'), ('base', 'cab')}
        i = sorted({(a, b) for a, g, t in T for b, g2, t2 in T if (g, g2) in pairs and t.overlap(t2)}); print(TAG, 'BOOM', sl, li, ex, 'fixed', len(h), h[:6], 'own', len(i), i[:6])
if '--nochk' not in sys.argv: boom_check()
_f = _st[_st.index('def finish_truck('):].replace('K.uv_cube();', 'K.uv_cube(); AFTER_UV();').replace('(2.590, .985, 1)', '(4.70, 1.18, 1)').replace(".68 if k[1] == 'L' else -.68", ".80 if k[1] == 'L' else -.80").replace("n.startswith(('Tyre_', 'HoseTurn'))", "n.startswith(('Tyre_', 'HoseTurn', 'ChainLink'))").replace('560, 400', '800, 560')
_f = _f.replace('((6.2, 5.2, 2.4), (.1, 0, 1.1), 42), ((-6.4, 4.6, 3.0), (-.3, 0, 1.1), 42), ((-4.6, -.6, 2.9), (-1.7, -.1, 1.3), 30), ((-5.0, -5.0, 2.6), (-.4, 0, 1.1), 40)', '((9.0, 8.0, 3.4), (0, 0, 1.5), 42), ((-9.2, 6.6, 3.6), (-.6, 0, 1.6), 42), ((3.35, .25, 2.35), (4.6, .35, 1.45), 16), ((-3.4, 4.6, 2.0), (-3.6, 0, 1.75), 34)'); assert '(9.0, 8.0' in _f
exec(compile(_f, 'small_truck finish', 'exec'))
GROUPS = ['HeaterDoor', 'PanelDoorL', 'PanelDoorR', 'Tur', 'LiftBarrel', 'LiftRod', 'BoomBase', 'BoomSec2', 'BoomSec3', 'Cabin', 'CabinDoor', 'CabinWiper', 'Joy_L', 'Joy_R', 'SprayPedal', 'NozPan', 'NozTilt', 'HandNozzle', 'HoseReel', 'HoseTail', 'HoseLift', 'HoseTip', 'HoseNozzle'] + ['FillCap_' + n for n in ('W', 'I', 'IV')] + ['CoupCap_' + n for n in ('W', 'I', 'IV')] + ['Valve_' + n for n in ('W', 'I', 'IV', 'Hand')] + ['GaugeNeedle_' + n for n in ('TempI', 'PressI', 'PressIV', 'Heater')] + ['Needle_Speed', 'Needle_Fuel']
def gx(n):
    if n.startswith(('GearSelect', 'PtoLever')): return 'Lever_' + n.split('_')[0]
    if n.startswith('Dash_Dial') and n.endswith('_Glass'): return 'ClusterGlass'
    if n.startswith('Console'): return 'CabFittings'
    if n.startswith(('Deck', 'Rest', 'RearStep', 'Cradle', 'Rail')): return 'RearDeck'
    if n.startswith(('Tank', 'Saddle', 'Sight', 'FillNeck', 'Vent')) and not n.endswith('_Glass'): return 'Tanks'
    if n.startswith('Panel'): return 'PumpPanel'
    if n.startswith(('Heater', 'Pump', 'Exhaust', 'AirInt', 'Slew', 'Reel', 'HandHolder')): return 'Modules'
    return None
finish_truck(GROUPS, gx, [('DoorPassenger', (4.70, -1.18, 1), 'z', (15, 40, 68), ()), ('HeaterDoor', (1.60, MY + .010, 1), 'z', (15, 45, 110), ()), ('PanelDoorL', (.62, MY + .010, 1), 'z', (15, 45, 110), ()), ('PanelDoorR', (.62, -MY - .010, 1), 'z', (-15, -45, -110), ()),
    ('SteeringWheel', tuple(SC), 'x', (), ()), ('NozTilt', (-4.66, 0, 1.17), 'y', (-25, 25, 45), ('HoseNozzle',)), ('Noz', (-4.66, 0, 1.06), 'z', (-60, -30, 30, 60), ('HoseNozzle', 'Cabin_Noz')), ('CabinWiper', (-4.462, 0, 2.765), 'x', (-40, 20, 60), ()), ('Joy_L', (-4.0, .37, 1.685), 'y', (-18, 18), ('Cabin_Joy',)), ('Joy_R', (-4.0, -.37, 1.685), 'x', (-18, 18), ('Cabin_Joy',)), ('FillCap_I_', (-1.07 + .16, .62, 2.424), 'y', (20, 60, 100), ()), ('FillCap_W', (-.03 + .16, .62, 2.424), 'y', (20, 60, 100), ()), ('FillCap_IV', (-2.175 + .16, .62, 2.424), 'y', (20, 60, 100), ())],
    [(('CabinDoor',), (1, 0, 0), (0, .1, .3, .56), ('Cabin_Track',))], {'boom': {'slew_axis': [SX, 0], 'lift_pivot': [PX, 0, PZ], 'tip_stowed': [TX, 0, PZ], 'length_retracted': round(PX - TX, 3), 'stroke_each': 3.0, 'length_out': round(PX - TX + 6, 3), 'lift_deg': [0, 60], 'lift_min_when_slewed': 15, 'stowed_heading': 'over the tanks, pointing back; slew about z', 'lift_foot': [1.82, 0, 2.54], 'lift_eye': [.75, 0, 2.55]},
     'chain': CHAIN, 'hand_hose_m': round(HOSE_M, 2), 'cabin': {'floor_z_stowed': CF + .01, 'door_slide_x': .56, 'nozzle_from_tip': [round(-4.948 - TX, 3), 0, round(1.17 - PZ, 3)], 'nozzle_pan': [-60, 60], 'nozzle_tilt': [-45, 45]}, 'tanks_l': {'water': 2600, 'type_I': 2400, 'type_IV': 2400}})
