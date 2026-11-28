"""Writes Data/aircraft-workpoints.json (hangar step 4): where hired mechanics stand to work on each fault area of a type.
Frame (as Data/aircraft-doors.json, but in metres): x to the aircraft's right, y aft, z up, the nose wheel at the root.
face: degrees, 0 toward the nose, 90 toward the right wing, 180 toward the tail, 270 toward the left wing.
hands: how high the hands work (m). role: in each pair one holds, one fastens. keepout: boxes nobody walks through
(gear legs with their wheels, engines), c = centre [x, y], half = half sizes [x, y].
A320 family from published Airbus airport-planning figures (wheelbase A319 11.04 / A320 12.64 / A321 16.90 m, main gear
track 7.59 m, engine centre 5.75 m out, fuselage plugs fore and aft of the wing). A type without an entry: the game
takes the entry of the model it is drawn with, else the A320's scaled to the model's real size (logged once)."""
import json, os

_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_doors = json.load(open(os.path.join(_root, 'Data', 'aircraft-doors.json'), encoding='utf-8'))['A320']['doors']
CARGO = {d['name']: (d['frame']['min'][1] / 100.0, d['frame']['max'][1] / 100.0) for d in _doors if d['name'].startswith('Cargo')}


def family(wheelbase, aft_plug, box_y_max):
    wb = wheelbase - 12.64          # (the wing, its engines, gear and flaps move with the main gear)
    af = aft_plug                   # (behind the wing: tail, aft cargo door)
    A = {}
    def pt(area, side, x, y, face, hands, role, z=0.0):
        A.setdefault(area, []).append({"side": side, "at": [round(x, 2), round(y, 2), z], "face": face, "hands": hands, "role": role})
    for s, sg in (("L", -1), ("R", 1)):
        l = 270 if sg > 0 else 90      # (toward the fuselage)
        o = 90 if sg > 0 else 270      # (toward the wingtip)
        # engine change 4-6: both sides of the nacelle and its inlet (cowl latches about 0.8-1.2 m up)
        pt("engine", s, sg * 4.05, 7.0 + wb, o, 1.1, "hold"); pt("engine", s, sg * 4.05, 8.7 + wb, o, 1.0, "fasten")
        pt("engine", s, sg * 7.45, 7.0 + wb, l, 1.1, "hold"); pt("engine", s, sg * 7.45, 8.7 + wb, l, 1.0, "fasten")
        pt("engine", s, sg * 5.2, 4.3 + wb, 180, 1.6, "hold"); pt("engine", s, sg * 6.3, 4.3 + wb, 180, 1.6, "fasten")
        # wheel change 2: beside the outer wheel of the main gear (axle about 0.6 m up)
        pt("wheels", s, sg * 5.1, 12.25 + wb, l, 0.6, "hold"); pt("wheels", s, sg * 5.1, 13.05 + wb, l, 0.6, "fasten")
        # gear 2-4: outboard, then fore and aft of the leg
        pt("maingear", s, sg * 5.1, 12.25 + wb, l, 1.2, "hold"); pt("maingear", s, sg * 5.1, 13.05 + wb, l, 1.0, "fasten")
        pt("maingear", s, sg * 3.8, 11.35 + wb, 180, 1.4, "hold"); pt("maingear", s, sg * 3.8, 13.95 + wb, 0, 1.4, "fasten")
        # flaps 2-4: under the trailing edges of the inner and the outer flap (3.3-3.8 m up: hands to their reach, a
        # work stand is not in the game yet)
        pt("flaps", s, sg * 5.6, 16.0 + wb, 180, 1.95, "hold"); pt("flaps", s, sg * 7.4, 16.5 + wb, 180, 1.95, "fasten")
        pt("flaps", s, sg * 10.6, 17.5 + wb, 180, 1.95, "hold"); pt("flaps", s, sg * 12.6, 18.4 + wb, 180, 1.95, "fasten")
        # spoilers 2-3: under the wing behind the main gear
        pt("spoilers", s, sg * 7.2, 14.8 + wb, 180, 1.95, "hold"); pt("spoilers", s, sg * 9.0, 15.5 + wb, 180, 1.95, "fasten")
        pt("spoilers", s, sg * 11.0, 16.3 + wb, 180, 1.95, "hold")
        # tail strike 3-6: the aft lower fuselage (about 2.5-3 m up)
        pt("tail", s, sg * 1.3, 23.4 + wb + af, l, 1.95, "hold"); pt("tail", s, sg * 1.3, 24.6 + wb + af, l, 1.95, "fasten")
        pt("tail", s, sg * 1.3, 25.8 + wb + af, l, 1.95, "hold")
    # the cargo doors are on the right: F the forward one, A the aft one
    # (the game's A320, the FlightGear model: the doors' frames as Tools/make_aircraft_doors.py measured them on it -
    #  Data/aircraft-doors.json Cargo1 and Cargo2: just ahead of the frame, its middle at the handle, just behind it)
    (f0, f1), (a0, a1) = CARGO['Cargo1'], CARGO['Cargo2']
    pt("cargodoor", "F", 2.95, f0 - 0.2, 270, 1.85, "hold"); pt("cargodoor", "F", 2.95, (f0 + f1) / 2, 270, 1.85, "fasten"); pt("cargodoor", "F", 2.95, f1 + 0.18, 270, 1.6, "hold")
    pt("cargodoor", "A", 2.95, a0 - 0.21 + wb + af, 270, 1.95, "hold"); pt("cargodoor", "A", 2.95, (a0 + a1) / 2 + wb + af, 270, 1.95, "fasten"); pt("cargodoor", "A", 2.95, a1 + 0.16 + wb + af, 270, 1.7, "hold")
    # the nose gear and the avionics bay hatch behind it (the belly about 1.9 m up there)
    pt("nosegear", "", -1.05, 0.2, 90, 1.0, "hold"); pt("nosegear", "", 1.05, 0.2, 270, 1.0, "fasten"); pt("nosegear", "", 0.0, -1.3, 180, 1.2, "hold")
    pt("avionics", "", 0.0, 2.3, 0, 1.9, "hold"); pt("avionics", "", 0.8, 1.9, 270, 1.9, "fasten")
    K = [{"name": "nose gear", "c": [0.0, 0.3], "half": [0.55, 0.9]}]
    for s, sg in (("left", -1), ("right", 1)):
        K.append({"name": s + " main gear", "c": [sg * 3.8, 12.64 + wb], "half": [0.75, 0.75]})
        K.append({"name": s + " engine", "c": [sg * 5.75, 7.9 + wb], "half": [1.25, 2.6]})
    return {"box": {"min": [-17.92, -5.22, 0.0], "max": [17.92, round(box_y_max, 2), 11.79]}, "areas": A, "keepout": K}

out = {"note": __doc__.strip().replace("\n", " "),
       "A320": family(12.64, 0.0, 32.50),
       "A319": family(11.04, -2.13, 28.62),
       "A321": family(16.90, 2.68, 39.29)}
out["A320"]["box"] = {"min": [-16.72, -5.22, 0.0], "max": [16.71, 32.50, 11.79]}   # (the model's own bounds: aircraft-doors.json)
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
for d in ("Data", "Content/Data"):
    if os.path.isdir(os.path.join(root, d)):
        with open(os.path.join(root, d, "aircraft-workpoints.json"), "w", encoding="utf-8") as f: json.dump(out, f, indent=1)
print("written", {k: sum(len(v) for v in out[k]["areas"].values()) for k in ("A320", "A319", "A321")})
