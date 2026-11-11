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

## The trucks' moving parts and weather (owner, 2026-11-11), for the driving phase
- **Steering:** front wheels about a kingpin 12 cm in from the wheel's centre, up to 25 deg each way (tested clear to
  28); **rear-axle steering** on the rearmost axle (Wheel_*3), the other way, up to 10 deg (clear to 14), its angle
  atan(1.65 * tan(front) / 5.4). Turning circle about 28 m. Constants KINGPIN, MAX_STEER, REAR_STEER_MAX in
  Art/FireTruck/make_panther.py.
- **Suspension moves with the driving:** per wheel the parts are separate objects with their pivots (see the header of
  the suspension block in make_panther.py): arms turn about their chassis axes, the spring scales along its local z,
  the damper rod slides in its body, uprights/brakes/hoses/sensors follow the wheel (and its steering), discs spin.
- **Rain on the glass (windscreen and the side windows, the doors' too):** drops gather and run down when the truck
  stands or crawls; with speed they stream back and sideways as on a real truck; the **wipers** (Wiper_L / Wiper_R,
  their own objects, swept about local Y from their spindles) clear what they sweep, so the view through it gets
  clearer. Side windows have no wipers: their drops only run and stream.
- **The wiper stalk works the wipers (owner, 2026-11-11):** the right-hand stalk on the steering column (Stalk_Wiper) is
  the switch; using it moves the stalk to its position and the wipers (Wiper_L / Wiper_R) sweep in that mode, parking
  at rest when off. Positions: off, intermittent, slow, fast; a short pull gives one wipe with washer spray. Shortcuts,
  in the truck's own driving controls (free keys there, checked against the game's other keys): **PC: K** (tap: next
  position, off - intermittent - slow - fast - off; hold: one wipe with washer); **PlayStation pad: D-pad right** (same:
  tap steps the position, hold washes). Both shown in the controls legend while driving. The sweep clears the rain on
  the glass where the blades pass (the wiped area clearer, the drops gathering back with time, faster in heavy rain).
- **Wiper speeds (as on real trucks):** intermittent: one sweep, then a pause of about 6 s at a standstill, shortening
  with road speed (about 2 s at 60 km/h); slow: about 45 sweeps a minute; fast: about 65 a minute; one wipe with
  washer: 3 sweeps and the spray. Changing position takes effect at the end of the current sweep; off parks the blades.

## Dirt and cleaning: every drivable vehicle and the crew (owner, 2026-11-11)
- **Every vehicle you can drive** (the fire trucks and all others) and **the crew's uniforms and gear** get dirty over
  time, slowly: a dirt level per vehicle / crew member, 0 clean to 1 filthy; "needs a wash" from about 0.6.
- **How fast, by season and weather** (game time):
  - normal (spring, autumn, mixed weather): needs a wash after about **1 to 2 weeks**;
  - summer, dry: about **a month**;
  - winter and very bad weather (rain, snow, slush, road salt, storms): about **twice a week**.
  Driving adds more than standing; wet runways, gravel and grass add mud; standing inside the station adds only dust.
  Rain washes some dust off but throws up road spray and mud. Fire and rescue work dirties the crew's gear fast
  (soot, foam, water), whatever the weather.
- **How it looks:** a fine dust film over everything; road grime heavier low down, on the wheels, arches and sills;
  mud splatter thrown up behind each wheel; dirty water streaks running down from the window and roof edges; a film on
  the glass (the wipers keep their sweep clear). Done in the vehicles' materials (world height and wheel position
  masks + the T_Dirt_* textures), no extra meshes, so it costs nearly nothing.
- **Cleaning (owner, 2026-11-11: no robot):** you wash the vehicles **yourself with the wash hose** outside the
  station (see below): walk over, take the hose, spray the vehicle clean. **Crew members do it the same way**: when
  one is told to (or decides to, when a vehicle passes a set dirt level), they walk to the hose, take it off the
  mount, drag it over and spray the vehicle as a player would, then put it back. The crew's clothing and gear: **laundry** (owner, 2026-11-11): for now an instant wash, a button in the Overview UI
  (the staff's clothes clean at once); a laundry building comes later (on the to-do list).
  Cleaning is optional: dirt is only looks, nothing breaks if you leave it. A Settings switch turns vehicle dirt off
  for players who want everything always clean (suggested, never forced).

## The wash hose outside the fire station (owner, 2026-11-11)
- **Part of the fire station, always (owner, 2026-11-11):** the wash hose with all its looks, features, effects and
  gameplay belongs to the fire station itself: it is placed in the station's own frame (on its wall, like the trucks in
  their bays, ADPFireStation::Place), so wherever the fire station is put or moved, the hose, its mount, its tap, the
  wash area, the puddles' place and its "return to the mount" spot go with it, all working the same.
- On the station's outside wall by the forecourt, a **wall-mounted hose** (used by you and by the crew alike): a galvanised wall
  bracket with the hose coiled on it, a tap, the spray gun hooked on the bracket. Model: Art/WashHose/WashHose.blend
  (wall mount + tap, the coiled hose as stowed, the spray gun its own object).
- **Pick it up:** walk to the mount and press the interact key (E / pad X): "Take the hose". The spray gun goes into
  your hand and the hose pays off the coil behind you as you walk.
- **A lifelike hose:** a physics cable (Unreal's cable / rope simulation, about 25 m long) from the tap to the gun,
  heavy and a little stiff: it lies on the ground, drags and slides, bends round corners and obstacles, can't pass
  through walls, vehicles or you, and pulls back on you at full length. It flexes and bounces as you move. Its mesh
  follows the simulated points (a rubber hose, dark green, with a fabric texture), generated each frame.
- **Carrying it (owner, 2026-11-11):** you (or the crew member washing) hold the nozzle in the right hand and a bit of
  the hose in the left, a short slack loop between the hands. From the hands the hose curves gently down to the ground
  and from there lies on the ground all the way back to the tap. Walking away pulls it along: the part on the ground
  is dragged and slides behind you (rubbing friction, it snakes and catches on kerbs, wheels and corners and comes free
  as you pull), the part from the ground up to your hands lifts and swings with your steps. Walking back, the slack
  drops to the ground and lies in loose curves where you went, still connected to the tap, until you hang it back up.
  The hose's points are simulated as one physical cable (ground contact and friction, its weight, bending stiffness);
  your hands are pinned points on it, so the hand-held part follows the hands' animation.
- **Spray:** hold **E / pad X** to spray (spraying is an interaction, owner 2026-11-11); water jet and mist (Niagara), wet darkening where it
  lands; spraying a vehicle washes its dirt off where the water hits (the dirt system's level drops there, faster up
  close), mud runs off down the panels. Works on the fire trucks and any vehicle you bring over.
- **How long a wash takes (owner, 2026-11-11):** a good clean of a vehicle takes about **3 to 5 minutes** of spraying,
  and only if you spray **all its sides** (front, back, both sides, the wheels and arches): the dirt is tracked per area
  of the vehicle, so a side you skip stays dirty. Closer and longer on a spot cleans it faster.
- **Gradual both ways (owner, 2026-11-11):** dirt and mud build up gradually and wash off gradually, never in one
  step: under the spray the mud softens, thins and runs off in streaks, a spot coming clean over seconds, a whole
  vehicle over the minutes above; out on the airfield mud and grime gather little by little, splash by splash.
- **You see the mud run off (owner, 2026-11-11):** while you spray, brown muddy water runs down the panels in rivulets,
  following the body's shapes, dripping off sills, arches and bumpers onto the ground; the water reaching the ground is
  muddy at first, the puddles round the vehicle tinted brown, turning clear as the vehicle comes clean.
- **A rainbow in the spray (owner, 2026-11-11):** sometimes, as in real life, a gentle rainbow shows in the water's
  arc and mist: only while the hose is actually spraying, the sun is out (not behind clouds) and fairly low, and the
  sun is **behind the viewer** (a rainbow sits about 42 deg from the point straight opposite the sun, so you see it
  looking away from the sun, in the drops that lie on that ring). Faint and soft, red on the outside, violet inside,
  only where there are enough drops; it moves with your view and vanishes as you turn. Done in the spray's particle
  material (each drop's colour from the angle between the sun and the view through it), no extra cost.
- **Puddles and wet ground, one system with the rain (owner, 2026-11-11):** the hose leaves the same quality of water
  on the ground as heavy rain leaves on the runways and the apron: real puddles (reflecting the sky, the lights and the
  vehicles, rippling under drops and spray, gathering in the low spots of the surface) and, around them, a darker,
  shinier wet surface where you washed. One ground-wetness system for the whole airport (a wetness map over the
  ground, painted wholesale by rain and locally by the hose's water, read by the asphalt, concrete and paving
  materials: darker and glossier with wetness, puddles where it is wettest and the surface lowest; drying over time).
  The game has no rain puddles yet either: this is built once and used by both.
- **The ground gets wet:** where the water lands the ground darkens and shines wet, puddles forming in low spots; it
  stays wet **a few minutes** after you stop, drying from the edges in. How wet, how far and how long depend on how
  long you sprayed (a quick rinse dries in a minute or two, a full wash leaves it wet for several); sun and wind dry it
  faster, rain keeps it wet.
- **Spraying people (owner, 2026-11-11):** if NPCs or other players (multiplayer) are near you and the spraying jet
  hits them, they are knocked over by the water: hit from the front they fall over backwards, from behind forwards,
  from their left or right side they fall sideways away from it (the direction from where the jet strikes them
  relative to which way they face). A short ragdoll fall, then they get back up. The jet must actually be spraying and
  reach them (the jet's length below decides the reach); a mist setting only wets them.
- **The jet's length: twist the nozzle (owner, 2026-11-11):** as on a real adjustable nozzle, turning its head on its
  axis changes the water from a wide, short mist (a few metres) to a tight, long jet (up to about 12 m), smoothly in
  between; your hand turns the nozzle as you adjust it. Shortcuts while holding the hose: **PC: mouse wheel** (up
  tighter and longer, down wider and shorter); **pad: D-pad up / down**.
- **The hose's keys (owner, 2026-11-11, agreed):** hold **E / pad X** to spray; a quick **tap of E / X** puts the
  hose down where you stand (and picks it up again from the ground); **middle mouse / D-pad left** locks the trigger
  on or off (so it keeps spraying when you let go or put it down); **mouse wheel / D-pad up-down** twists the nozzle.
- **Drop it, pick it up any time (owner, 2026-11-11):** you can put the hose down wherever you are; it lies there, and
  if it was spraying (trigger locked on) it keeps spraying, the hose squirming a little from the water's push. Walk up
  to the gun on the ground and press E / X to pick it up again, wherever it lies.
- **Left far away:** if you go off and leave the hose lying on the ground (about 40 m from it, or out of the area), it
  fades out where it lies and fades in back on its wall mount, coiled, the water off.
- **Put it back:** at the mount, the prompt changes to "Put hose back": the hose returns to its coil on the bracket
  instantly (no rewinding animation), the gun back on its hook.
- Shown in the controls legend while you hold it (spray, put back).
- **Interaction keys (owner, 2026-11-11, the whole game):** every interaction with anything is **E on PC, X on the
  gamepad**: taking and putting back the hose, spraying it (hold), getting in and out, doors, switches, devices.
  The wipers and the wiper stalk are not interactions (owner, 2026-11-11): you don't click the stalk; the driving
  control K / D-pad right works them, and the stalk moves on its own to show the position.

## The trucks' water cannons (owner, 2026-11-11)
- From the driver's seat you **remote-control the turrets**: the roof turret on the cab and the bumper turret, aimed
  with the joystick on the driver's armrest (its stick moves as you aim). All the hose's water effects apply, scaled
  up: the jet and its breaking into drops and mist, the rainbow in the spray (sun behind you), the puddles and dark
  wet ground where it lands (the same ground-wetness system), mud washing off what it hits (it can wash vehicles too).
- **Real-world numbers** (in the range of today's 6x6 airport crash tenders; ballpark, to tune):
  - roof turret: about **6,000 L/min** water, reach about **80-90 m** on the straight jet; adjustable from a straight
    jet to a wide spray (shorter, about 30-40 m) as on the hose;
  - bumper turret: about **1,000-2,000 L/min**, reach about **40-60 m**;
  - water tank about **12,500 L** (the roof turret empties it in about 2 minutes), refilled at the station;
  - the jet's force falls off with distance; up close it is far stronger than the hose.
- **Hitting people:** NPCs and other players hit by the jet fall over the same way as with the hose (backwards from
  the front, forwards from behind, sideways from the sides), from much farther away.
- **Controls:** spraying holds **E / pad X** (spraying is an interaction, as with the
  hose); aiming the turret: mouse / right stick while in turret control; the jet's width: mouse wheel / D-pad up-down
  (as the hose's nozzle); **T** switches turret control on/off and **Tab** switches between the roof and the bumper
  turret (owner, 2026-11-11, agreed); the pad buttons for those two are chosen once the pad's driving controls are set.
