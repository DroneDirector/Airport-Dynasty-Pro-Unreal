# A320neo - how this model is built

One page. Every builder on this model reads this file and its own step, nothing else.

Owner's standard (2026-11-27): insanely detailed and accurate, built from the blueprints, interior included,
**every moving part cut out as its own object**, nothing loose or floating, screws and fasteners modelled,
doors modelled open, gear modelled down.

## Where things live
- Script: `Art/A320neo/make_a320neo.py` (Blender 3.3, headless, real metres, one script, one section per step).
- Scene: `Art/A320neo/A320neo.blend` - each step opens it, adds its own objects, saves it again.
- Blueprint data: `Art/A320/blueprint/a320neo-data.json` (the numbers) and `docs/Pictures/a320neo-blueprint/*.png` (the 8 sheets).
- Sources: `Art/A320neo/SOURCES.md` - every photo and document used, with its licence. Commercial use only.
- Back up before overwriting anything: `bash Tools/backup.sh <file>`.

## The numbers that must come out right
Taken from the sheets; a part that disagrees with these is wrong, not the sheet.

| | m |
|---|---|
| overall length | 37.57 |
| span with sharklets | 35.80 |
| height to fin top | 11.98 |
| fuselage 3.95 wide x 4.14 high (superellipse) | |
| wheelbase 12.64, track 7.59 | |
| wing root chord 6.07 at y 1.98, tip chord 1.64 at y 16.29 | |
| sweep 25 deg at 1/4 chord, dihedral 5.1 deg, kink at 39.2 % semi-span | |
| sharklet 2.43 high (3.22 along the sheet) | |
| LEAP-1A nacelle 5.44 long, 2.60 x 2.52, fan 1.98 | |
| engine centrelines 11.50 apart, nacelle bottom 0.46 above ground | |
| HTP span 12.45, tip chord 1.24 | |
| cabin 3.63 wide, floor 3.38 above ground, ceiling 2.22 above the floor | |

### Cockpit windows - measured off photos, 2026-11-27
Two licensed photos, scaled by the Door 1 opening (1.85 m high): Commons "EZY nose close up.jpg" (CC BY 3.0,
head-on) and "Lufthansa A320-211, D-AIPT, named Cottbus (5650386693).jpg" (CC BY 2.0, near side). The photo band
measures 1.52 m long, the same as the blueprint - the blueprint band is right and must not be shifted.

Heights below are anchored on the cabin window centre line, z 4.55 (published), measured in the same photo -
not on the cockpit_windows z_range, which is "estimated" and disagrees with the photo.

- Glass **top edge level at z 4.83**, one straight line across all three windows - no staircase.
- **Sill FALLS going aft**: z 4.46 at the forward end of the band, z 4.28 at the aft end. (An earlier round
  built it rising aft - wrong sign.) So in side view W1 is the shallowest pane and W3 the deepest:
  glass depths about 0.37 / 0.44 / 0.55.
- **The two posts are not alike. This is the thing that is always got wrong:**
  - Post between W1 and W2: **raked about 30 deg, leaning aft at the top.**
  - Post between W2 and W3: **vertical.**
- **Gap between window frames: 55 mm at the W1/W2 post, 100 mm at the W2/W3 post.** The W2/W3 post is the
  structural one and is plainly the wider of the two.
- W3's aft edge leans about 8 deg forward at the top and its top-aft corner is chamfered.
- W1's own forward edge rakes aft at the top too, around 35 deg, as it wraps onto the front face.
- Front: W1 wraps onto the nose front face, pane |y| 0.06 -> 1.00, centre post **0.12 m total**, frame to |y| 1.06.
  W1 inboard edge vertical and taller, outboard edge raked aft and shorter.
- W2 sliding, near-rectangle with a raked forward edge. W3 trapezoid, aft edge sloping forward, narrower at the foot.
- Frames narrow (~0.06 band) and nearly flush, 12 mm proud, corner radius ~0.04; rivet row along the top and the
  posts. One wiper per windscreen parked along the sill, pivot at the inboard-bottom corner, arm ~0.75.

Ground state everywhere: **MRW, forward CG (17 %)**, the left column of the clearance table on sheet 1.

## Naming
`A320N_<Group>_<Part>[_L|_R][_<n>]`, e.g. `A320N_Wing_FlapOuter_L`, `A320N_Gear_MLG_TorqueLink_R`,
`A320N_Door_Pax1L_Leaf`. Groups: Fus, Wing, Tail, Gear, Eng, Door, Cabin, Cockpit, Light.
Every part that moves, opens, turns or is pressed is **its own object with its own pivot at its real hinge or axis**,
and the pivot axis matches the real one (flap tracks run along the track, not along the world axes).

## Rules every step keeps
- Real metres, origin at the nose tip on the centreline, ground at z 0, +x aft, +y starboard, +z up.
- Nothing passes through anything else; real gaps at every seam; no faces lying on each other
  (`python Tools/find_zfight.py Art/A320neo/A320neo.blend`) and no loose parts (`python Tools/find_gaps.py ...`).
- Fasteners are modelled where a photo shows them: rivet rows along skin joints, screws round panels and lamp
  glasses, DZUS fasteners on cockpit panel edges. Rows follow the joint, they are not scattered.
- No empty material slots; every texture input of a script-made material has a default texture.
- Smooth normals with real bevels on every edge that catches light; no razor edges.
- No airline livery, no brand logos, no registration.
- Nanite-friendly: closed shells, no n-gons above 6 sides, no mirror modifiers left unapplied.

## Steps - one builder, one step, finished before the next
1. **Fuselage shell** - nose, radome, barrel, tail cone, APU, fin and tailplane roots; 38 windows a side;
   cockpit windows; every door its own part, hinged, **open**; panel lines, skin joints, rivet rows; belly and
   wing-root fairing; service panels, probes, antennas.
2. **Wing and sharklet** - one clean loft to the printed chords and sweep, kink at 39.2 %, spars and ribs where
   they show, root fairing joint, sharklet, the fixed trailing edge and the cut-outs the moving surfaces sit in.
3. **Moving surfaces + mechanism** - slats 1-5 and their tracks and drive; inner and outer Fowler flaps with
   tracks, carriages, rollers and screwjacks, inside the five flap track fairings; spoilers 1-5 with hinges and
   actuators; aileron with hinges and actuators. All joined to the wing, nothing floating. Modelled retracted,
   but hinged so they can be driven.
4. **Landing gear, down** - MLG leg, shock absorber, torque links, side and lock stay, bogie, 4 main wheels with
   carbon brakes, hinged and fixed fairings; NLG with 9 deg rake, drag brace, torque links, 2 wheels, steering,
   taxi/take-off lights, doors open; bays with real walls, ribs and pipework.
5. **Engines and pylons** - LEAP-1A: inlet, fan blades and spinner, fan cowl with latches, thrust reverser with
   cascades, core cowl, nozzle and plug; pylon with its fairings and the wing attachment.
6. **Cabin and cockpit** - 2-class 150 from sheet 5: seats, rails, bins, PSUs, lining, floor, galleys, lavatories,
   attendant seats, slide bustles; cockpit from sheets 7 and 8: seats, sidesticks, pedals, glareshield, FCU, six
   displays, overhead, pedestal, every panel block with its DZUS edge.
   **Seat quality (owner, 2026-11-27):** the cabin seats must be as good as the coach seats -
   `Art/CoachSeat/CoachSeat.blend` and its `T_Coach_*` texture set are the bar. That means a real shell, foam
   with a seam line and stitching, a separate cushion and back, armrests with their pivots, the tray table and
   its arm, the literature pocket, the recline mechanism and the leg frame on the seat track - not a boxy
   stand-in. Fabric with a woven weave, not flat colour.
7. **Textures** - from licensed photos: paint grain, skin joints, dirt streaks behind panels, brake dust on the
   wheels, soot aft of the exhaust, worn alloy inside the bays. Unstretched UVs, 4K where it shows.
8. **Checks and pictures** - find_zfight and find_gaps clean, part count and names listed, 3 renders plus the
   wireframe image the owner asked for.

Import into Unreal only when the owner says so; no step runs the game.
