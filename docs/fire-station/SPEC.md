# Fire station (ARFF) and fire trucks: the owner's brief (2026-11-11)

Status: design sketch stage. The sketch is made in the cloud chat (source in `sketch/`, Babylon offscreen renders used
only as a drawing aid; the real build is Unreal C++ like the hangars). NOT to build or publish until the owner says so.
When he does, the build is done by the PC chat, after it has finished building and publishing everything on its list.

## The building
- **Main design:** reference 1, the airport fire station: silver vertical panels; a glass stair tower with red behind
  the glass; a charcoal wing carrying the station's lettering and number; a flat roof with a thin dark overhanging edge;
  a flagpole and grassy planting.
- **Blended with reference 2** (two-storey station) so it reads as one design: a row of apparatus bays, upper-floor
  windows, an emblem with the station number on a tower, a flat roof with skylights, an apron in front.
- **Bay doors (reference 3):** the dark band with the bay numbers 1, 2, 3 …; wall lamps above the doors; all garage
  doors SHUT in the game.
- **Red sectional doors (reference 4):** doors with a band of windows; the forecourt.
- **Apparatus hall interior (references 5–7):** open-web steel roof joists and skylights; exhaust hose drops and hose
  reels; yellow lane lines on a light concrete floor; white walls and service doors.
- **Reference 8:** grey board-and-batten fibre-cement facade over the whole building, wrapped round the other parts
  with real joints and seams; a window above every garage door and every personnel door; a red-framed glass people
  door beside the last garage door.
- **Colours:** textures matched to the warmth of references 1 and 2. Photo-quality textures.
- **Lighting:** not washed out; the same strength as the terminal side.
- **Quick-start places** in the full-screen holomap (with the others), nothing clipping: outside straight on; outside
  at an angle from the side; inside the apparatus hall by the trucks.

## The training tower (owner, 2026-11-11: "more fancy, not only a square")
- A light concrete drill shaft (6 x 6 m, 22 m) on a charcoal plinth, white floor bands wrapping it every floor.
- Staggered drill windows on the front and side: dark openings, red frames and sills, red louvred shutters folded open.
- Two cantilevered balconies on the front (about 9 m and 17 m) on steel brackets, galvanised railings, a door onto each.
- A taller charcoal stair shaft beside it (26 m) with a full-height glass slot and a red crown with the emblem's ring,
  echoing the main entrance's stair tower.
- Overhanging roof slab with dark fascia, railing round three sides, roof hatch.
- Red rappelling arm out over the front with a pulley and two ropes hanging down.
- Hose-drying mast on the stair shaft with a red beacon (lit at night).
- A red stripe up the drill shaft's outer corner.

## The fire trucks
- Two ARFF trucks (6x6 airport crash tenders: chartreuse yellow, black chevron stripes, number) parked INSIDE behind
  the closed doors, in bays 2 and 3, noses to the doors.
- Our own livery and airport name; no real brand names or logos.
- Each truck is its own object (easy to swap later).
- A detailed cab interior that fits the body exactly, same quality as the outside; glass see-through from inside.
- **Drivable, like GTA:** doors open and shut with get-in/get-out animations; sitting down; realistic heavy-truck
  physics; brake lights, headlights, beam lights, sirens and warning lights switched on and off; engine start and
  driving sounds.
- **Views and controls:** first person from the driver's seat (mouse look, driving with the walking keys); a
  third-person vehicle view; one key each for changing the view, getting in, getting out, and each light set (head and
  beam lights, warning lights, sirens); all shown in the controls legend.
- **Returning to the station:** after you leave a truck and stay away for 30 minutes, the game asks before putting it
  back in its exact spot in the station (it is needed for emergencies): OK, or extend by 30 min, 1 h, 2 h, 3 h, or the
  rest of the day.
