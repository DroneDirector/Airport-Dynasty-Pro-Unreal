# The cargo terminal's moving doors and its ULD / pallet places, from cargo_layout (one source for all of them):
#   hinged()   - personnel, escape, fire-wall and office doors: built into the model as D-CARGO-... nodes, each with its
#                own hinge node and leaf part (ABabylonModel's door system: E / X, a closer shuts it 4 s after you've gone)
#   sliding()  - the office lobby's glass doors (D-CARGO-LOBBY-...: open by themselves as you come)
#   rollers()  - airside ULD doors, special doors, truck dock doors, drive-in doors and the rooms' sectional doors: the
#                game's roller doors (ARollerDoor, placed by Airfield.cpp from Data/cargo-doors.json; they roll up)
#   slots()    - Data/cargo-slots.json: ETV rack lanes and depth places, roller decks, truck docks, floor overflow squares
# All in the model's own frame (metres): x along the building from its west end, y from the airside face (y 0) to the
# landside face (y 70), z up from the hall floor.
#   python Art/CargoTerminal/cargo_doors.py      writes Data/cargo-doors.json and Data/cargo-slots.json
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import cargo_layout as CL
from cargo_layout import L, D, ETV

ESC_H, ESC_GAP_H = 2.1, 2.2                    # escape / personnel leaf height, its opening's height
LOBBY = (14.0, 16.0, 2.4)                      # office lobby's sliding glass doors: x from, x to, clear height
LOBBY_Y = (D + 0.15, D + 4.0)                  # inner door in the landside wall, outer door in the vestibule's front
SLAT = '/Game/Terminal/Meshes/roller_slat'     # the roller curtain's leaf parts (a later pass may swap them per door kind)
BAR = '/Game/Terminal/Meshes/roller_bar'


def _num(prefix, items):
    return [('%s-%02d' % (prefix, i + 1),) + tuple(it) for i, it in enumerate(items)]


def hinged():
    """(id, cx, cy, z0, out, width, height): out = the way the leaf swings ('-y', '+y', '+x', '-x'), width = opening."""
    out = []
    out += _num('D-CARGO-ESC-AIR', [(x, -0.15, 0.0, '-y', 1.0, ESC_H) for x in CL.ESC_AIR])
    out += _num('D-CARGO-ESC-LAND', [(x, D + 0.15, 0.0, '+y', 1.0, ESC_H) for x in CL.ESC_LAND])
    out += _num('D-CARGO-ESC-EAST', [(L + 0.15, y, 0.0, '+x', 1.0, ESC_H) for y in CL.ESC_EAST])
    out += _num('D-CARGO-FW', [(x, y, 0.0, '+x', 1.2, ESC_H) for x, y in CL.FW_DOORS])
    g = [d for d in CL.doors_from_rooms(CL.ground_rooms(), CL.SKIP_X, CL.SKIP_Y) if d[3] <= 1.2]
    out += _num('D-CARGO-ROOM', [(cx, cy, 0.0, '+y' if o == 'h' else '+x', w, ESC_H) for cx, cy, o, w in g])
    for f, rooms in CL.UPPER.items():
        z = CL.FLOOR_Z[f] + 0.3
        rr = [(x0, y0, x1, y1, 4.3, doors, 1.0) for _, x0, y0, x1, y1, k, doors in rooms if k != 'aisle']
        out += _num('D-CARGO-F%d-ROOM' % f, [(cx, cy, z, '+y' if o == 'h' else '+x', w, ESC_H)
                                             for cx, cy, o, w in CL.doors_from_rooms(rr, (0.0, CL.OFF[1]), CL.SKIP_Y)])
    return out


def sliding():
    """(id, cx, cy, z0, width, height): two glass leaves each, along x."""
    x0, x1, h = LOBBY
    return [('D-CARGO-LOBBY-IN', (x0 + x1) / 2, LOBBY_Y[0], 0.0, x1 - x0, h),
            ('D-CARGO-LOBBY-OUT', (x0 + x1) / 2, LOBBY_Y[1], 0.0, x1 - x0, h)]


def room_sectionals():
    """The ground floor's wide room doors (cx, cy, axis, width, clear height) - the header sits at the clear height."""
    return [(cx, cy, o, w, 4.5 if w >= 4 else 3.4) for cx, cy, o, w in
            CL.doors_from_rooms(CL.ground_rooms(), CL.SKIP_X, CL.SKIP_Y) if w > 1.2]


def rollers():
    """(id, kind, cx, cy, z0, axis, width, height): axis 'x' = the curtain runs along x (a long face), 'y' along y."""
    out = []
    out += _num('R-CARGO-AIR', [('uld', x, -0.15, 0.0, 'x', 6.0, 5.2) for x in CL.AIR_DOORS])
    out += _num('R-CARGO-SPC', [('special', x, -0.15, 0.0, 'x', 4.0, 4.5) for x in CL.SPC_AIR_DOORS])
    out += _num('R-CARGO-HORSE', [('special', x, -0.15, 0.0, 'x', 5.0, 4.5) for x in CL.HORSE_AIR_DOORS])
    out += _num('R-CARGO-DOCK', [('dock', x, D + 0.15, 1.2, 'x', 3.0, 3.5) for x in CL.DOCKS + CL.SPC_DOCKS])
    out += _num('R-CARGO-DRIVEIN', [('drivein', x, D + 0.15, 0.0, 'x', 5.0, 5.0) for x in CL.DRIVE_IN])
    out += _num('R-CARGO-ROOM', [('sectional', cx, cy, 0.0, 'x' if o == 'h' else 'y', w, h) for cx, cy, o, w, h in room_sectionals()])
    return out


def wall_gaps():
    """Openings in the outer walls: {'s'|'n'|'e': [(a, b, z_bottom, z_top)]} along x (s, n) or y (e)."""
    g = {'s': [], 'n': [], 'e': []}
    for _, kind, cx, cy, z0, ax, w, h in rollers():
        if kind == 'sectional': continue
        g['s' if cy < 0 else 'n'].append((cx - w / 2, cx + w / 2, z0, z0 + h))
    for _, cx, cy, z0, o, w, h in hinged():
        if 'ESC-AIR' in _: g['s'].append((cx - w / 2, cx + w / 2, 0.0, ESC_GAP_H))
        elif 'ESC-LAND' in _: g['n'].append((cx - w / 2, cx + w / 2, 0.0, ESC_GAP_H))
        elif 'ESC-EAST' in _: g['e'].append((cy - w / 2, cy + w / 2, 0.0, ESC_GAP_H))
    g['n'].append((LOBBY[0], LOBBY[1], 0.0, LOBBY[2]))
    for k in g:
        g[k].sort()
        for (a0, b0, _, _), (a1, b1, _, _) in zip(g[k], g[k][1:]):
            assert b0 + 0.3 <= a1, 'outer wall openings overlap on face %s at %.2f' % (k, a1)
    return g


# #region ── Places for ULDs and pallets (data only: the cargo system fills them later) ──
ULD = (3.18, 2.44)                       # PMC / PAG main-deck pallet base 125 x 96 in: the long side along the lane's x
DEPTH_PITCH = 3.35                       # two pallets deep in a 7.5 m bay: back post 0.4 m, 2 x 3.35 m, 0.4 m to the aisle
KEEP_STATIC = lambda i, j, row: (i + j * 2 + row) % 3 == 0   # the third of the lanes that keeps a fixed ULD at the back


def overflow_squares():
    """Marked floor squares (x0, y0, x1, y1) in the ETV hall's landside area, clear of the dock levellers (y > 67)."""
    return [(ETV[0] + 2.0 + i * 4.0, 59.0, ETV[0] + 5.6 + i * 4.0, 62.4) for i in range(36)]


def slots():
    rows = []
    for r, (ya, yb) in enumerate(CL.ETV_RACKS):
        back, sgn = (ya, 1.0) if r == 0 else (yb, -1.0)          # the back of the lane is the side away from the ETV aisle
        for i, x in enumerate(CL.ETV_RACK_X):
            for j, z in enumerate(CL.ETV_LEVELS):
                lane = {'id': 'ETV-%s-%02d-L%d' % ('AB'[r], i + 1, j + 1), 'row': 'AB'[r], 'lane': i + 1, 'level': j + 1,
                        'x': round(x + 1.65, 3), 'z': round(z + 0.1, 3), 'aisleY': CL.ETV_AISLE[0] if r == 0 else CL.ETV_AISLE[1],
                        'depths': []}
                for d in range(2):
                    y = back + sgn * (0.4 + DEPTH_PITCH * (d + 0.5))
                    lane['depths'].append({'id': '%s-D%d' % (lane['id'], d + 1), 'depth': d + 1, 'x': lane['x'], 'y': round(y, 3),
                                           'z': lane['z'], 'staticULD': bool(d == 0 and KEEP_STATIC(i, j, int(ya)))})
                rows.append(lane)
    y0t, y1t = CL.TRANSFER
    air = [{'id': 'AIR-%02d' % (n + 1), 'door': 'R-CARGO-AIR-%02d' % (n + 1), 'deckOutside': [x, -3.25, 0.5],
            'transferInside': [x, round((y0t + y1t) / 2, 3), 0.55]} for n, x in enumerate(CL.AIR_DOORS)]
    buf = [{'id': 'BUF-%d-%02d' % (r + 1, k + 1), 'x': round(ETV[0] + 1 + 3.3 * (k + 0.5), 3), 'y': ya + 1.6, 'z': 0.5}
           for r, ya in enumerate((7.5, 12.5, 17.5)) for k in range(int((ETV[1] - ETV[0] - 2) / 3.3))]
    st = [{'id': '%s-%d' % ('BUILDUP' if k < len(CL.BUILDUP) else 'BREAKDOWN', (k if k < len(CL.BUILDUP) else k - len(CL.BUILDUP)) + 1),
           'x': cx, 'y': round((CL.STATION[1] + CL.STATION[2]) / 2, 3), 'z': 0.5} for k, cx in enumerate(CL.BUILDUP + CL.BREAKDOWN)]
    docks = [{'id': 'DOCK-%02d' % (n + 1), 'door': 'R-CARGO-DOCK-%02d' % (n + 1), 'leveller': [x, D - 1.5, 0.08],
              'truckBay': [x, D + 12.0, 0.0], 'sill': 1.2, 'special': x in CL.SPC_DOCKS} for n, x in enumerate(CL.DOCKS + CL.SPC_DOCKS)]
    ov = [{'id': 'OVF-%02d' % (n + 1), 'x': round((a + c) / 2, 3), 'y': round((b + d) / 2, 3), 'z': 0.03, 'size': [round(c - a, 2), round(d - b, 2)]}
          for n, (a, b, c, d) in enumerate(overflow_squares())]
    return {
        'note': 'Cargo terminal places, model-local metres (Art/CargoTerminal: x along the building from its west end, y from the '
                'airside face (0) to the landside face (70), z up from the hall floor). In the game: Airfield.cpp spawns the model '
                'at plan (cargoBuilding x0, y0) with yaw 180 and ABabylonModel::Place adds its own half turn, so the two cancel: '
                'plan x = x0 + x, plan y = y0 + y; Unreal world cm = model root location + (x, -y, z) * 100. '
                'ETV lanes: depth 1 is the deepest (against the back), depth 2 next to the ETV aisle. staticULD: a fixed ULD '
                'is modelled there (the cargo system must treat it as taken). Data only - written by Art/CargoTerminal/cargo_doors.py.',
        'uldBase': list(ULD), 'etvLanes': rows, 'airsideDecks': air, 'bufferPlaces': buf, 'workstations': st,
        'truckDocks': docks, 'overflowSquares': ov,
        'counts': {'lanes': len(rows), 'depthPlaces': 2 * len(rows), 'staticULDs': sum(d['staticULD'] for r in rows for d in r['depths'])}}
# #endregion


if __name__ == '__main__':
    root = os.path.normpath(os.path.join(HERE, '..', '..'))
    doors = {'note': 'Cargo terminal roller doors, model-local metres (see cargo-slots.json for the frame). Airfield.cpp places '
                     'each as an ARollerDoor: kind, centre of the curtain at its foot (x, y, z0), axis the curtain runs along, '
                     'clear width and height; slat / bar: its curtain parts (swap them for a better leaf without touching the wiring).',
             'rollers': [{'id': i, 'kind': k, 'x': round(x, 3), 'y': round(y, 3), 'z0': z, 'axis': a, 'width': w, 'height': h,
                          'slat': SLAT, 'bar': BAR} for i, k, x, y, z, a, w, h in rollers()]}
    s = slots()
    for nm, obj in (('cargo-doors.json', doors), ('cargo-slots.json', s)):
        for d in ('Data', 'Content/Data'):        # (the packed game reads Content/Data)
            with open(os.path.join(root, d, nm), 'w') as f: json.dump(obj, f, indent=1)
    wall_gaps()
    print('rollers %d, hinged %d, sliding %d, lanes %d, depth places %d, static ULDs %d, overflow %d' % (
        len(doors['rollers']), len(hinged()), len(sliding()), s['counts']['lanes'], s['counts']['depthPlaces'],
        s['counts']['staticULDs'], len(s['overflowSquares'])))
