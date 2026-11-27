# Chablais Airport bus station, taxi rank and forecourt - the plan

Owner, 2026-11-27: "make the bus stop area look very, very, very, very nice. And also a bus terminal or something
outside, a big one. And lanes for many buses. And also a few taxi spots outside. Don't draw the taxis, just a taxi
spot."

This is built BEFORE the town coach hub behaviour (to-do 373) and before Chatterbox, on his word.

## What is already there - reuse it, do not lay a second forecourt
- `Data/Sites/chablais/roads.json` is the site's own road file (the top-level `Data/roads.json` is the old site -
  leave it alone). Roads carry typed lanes, `paint`, `crossings`, `lights` and `props`, all driven off a distance
  `s` along the road and an `edge` index.
- `AIRPORT-FORECOURT` runs from the east roundabout at (1856.26, -115.17) west to (-744, -46.75), 2603.9 m long.
  `s` is measured from the east end, so **x is about 1856 - s** along the straight part, at y -46.75.
- The terminal faces it: its landside wall is **world y = 0**, running **x -241 to 551**; the main doors are at
  **x 42 to 114** (plan room F1-HALL), with yellow crossings already laid at s 1758.6, 1784.85 and 1810.98.
- The forecourt's **right** side (right normal is +y, so right = the terminal side) already has, outward from the
  centreline: Driving 3.5, Shoulder 1.0, Island 3.5, Driving 3.6, Driving 3.55, **Parking 3.1, Bus 3.5,
  Parking 3.5**, Sidewalk up to 13.1 m. That reaches to about y -8.4, leaving roughly 8 m of clear ground before
  the terminal wall.
- Two `taxishelter` props already stand on the pavement (edge 8) at s 1661.92 and s 1899.92.
- `routes` in the same file already holds to-do 373's one-way coach loop ("coach-to-town" and back through
  `TERMINAL-RETURN`), written to end "straight into the bay". **The bays this plan builds are those bays.**
- `levels` holds level pads with a margin and blend (the TOWN-HUB entry is the pattern to copy).

Generated files that are never committed: `Data/Sites/chablais/road-ground.json`, `paxnav.bin`.

## The look - Swiss, glazed, light
Reference actually looked at (commercial use allowed, attribution in SOURCES):
- **Chur Busbahnhof**, `Chur_Busbahnhof_ext_2015_wide.jpg`, Cayambe, CC BY-SA 4.0 - a long **glazed barrel vault on
  white tubular steel arches**, cable-braced, over an open bus deck. This is the language: airy, white steel, glass,
  daylight through the roof onto the coaches. Chablais is in Switzerland and this is what a good Swiss bus station
  looks like.
- `Aylesbury_bus_station_3.JPG`, Arriva436, CC BY 3.0 - a dark enclosed under-deck station. **Looked at and
  rejected as the look.** Useful only for two details worth keeping: a coloured resin walking route across the
  deck, and disc luminaires hung from the roof.

**Design our own vault - do not copy Chur's building.** No real brands, no real operator liveries, no logos.

The bar for finish is the A320 clean after-pictures and `Art/CoachSeat`: real materials, bevelled edges, glass that
looks like glass, lamps with housing, reflector and lens. Not a boxy stand-in.

## The layout
A dedicated bus-only carriageway and island platform on the terminal side of the forecourt, east of the main doors
where the pavement is widest - the player walks out of the main doors, turns left, and it is right there.

- **Bus station run: x 180 to 520** (s 1336 to 1676), about 340 m of frontage. The vault covers the middle
  **150 m** of it; the rest is open platform and the approach tapers.
- Take out the two **Parking** lanes (3.1 + 3.5 m) over that run and give the width to the station. Keep the
  existing Bus lane as the through lane. That buys about 6.6 m without touching the driving lanes or the island.
- **Island platform** between the bus carriageway and the terminal pavement: 8 m wide, kerb 0.18 m, tactile edge
  strip, drainage channel.
- **Sawtooth bays, 12 of them** ("lanes for many buses"): each bay 3.6 m wide on a 25 degree sawtooth, 14 m deep,
  so a 13 m coach pulls in and drives out forward without reversing. Numbered **A1-A12** on the platform posts and
  painted on the deck. The route steps in `routes` end at these.
- **Taxi rank: 6 marked spots** on the pavement side, west of the station at x 60 to 150, by the main doors -
  a painted box each, 2.5 x 6.0 m, "TAXI" legend, a feeder lane and a head-of-rank marker. **No taxi vehicles
  modelled.** The two existing `taxishelter` props stay and the rank is laid around them.
- Crossings from the island to the terminal doors, yellow, matching the three already at s 1758-1811.
- A **level pad** for the whole station in `levels`, town-sized margin and blend like TOWN-HUB, so the deck is flat.
- The holomap is updated when the ground changes.

## Nothing floats - the owner's rule, 2026-11-27
"Make sure that all arches, all geometry is connected, so nothing floats in the air or is just barely hanging."

This is a build rule for every part of this job, not a wish:
- **Every arch carries down to the ground.** Each arch foot sits on a real base plate, on a real footing, on the
  deck - modelled, not implied. An arch that stops in mid-air, or lands on nothing, is wrong.
- **The arches are tied to each other**: purlins, the glazing diagrid and the cable bracing run arch to arch, so the
  vault is one connected structure and not a row of separate hoops.
- **Everything hanging is hung from something.** Luminaires have hangers or brackets reaching a real arch member;
  signs have posts or arms; gutters sit on brackets. Nothing is attached to thin air and nothing rests on a
  single-point touch.
- **Real contact, real overlap.** Parts meet with a proper joint - a plate, a weld fillet, a bolted flange, a socket
  - not two surfaces just grazing each other. No part balances on a corner or an edge.
- Checked before hand-back: `python Tools/find_gaps.py Art/BusStation/BusStation.blend` reports no loose parts, and
  `Tools/find_zfight.py` is clean. A loose part in that report is a defect to fix, not to explain.

## The station itself (`Art/BusStation/`)
Built by script in Blender 3.3, real metres, one object per part, named `BusStn_<Group>_<Part>`.
- **Vault**: white tubular steel arches about 22 m span, 9 m to the crown, every 7.5 m over 150 m; a glazed diagrid
  between them, cable bracing, rainwater gutters down the arch feet into the platform drains.
- **Waiting hall** on the platform, 30 x 9 m: glass walls, sliding doors, benches, a ticket machine, a departure
  board, bins, heaters. All seats sittable.
- **Platform furniture**: bay signs A1-A12 lit from inside, timetable cases, benches, bins, handrails, bollards,
  a clock.
- **Lighting**: disc luminaires hung from the arches and column uplights, all through the lamp system with a
  distance limit, no flares - and they really light the deck and the coaches.

## Steps - one builder, one step, finished before the next
1. **The station model** - `Art/BusStation/make_busstation.py` -> `BusStation.blend` / `.glb`, imported with
   `Tools/import_model.py`. Blueprint sheet first, black background, then the model. Checks: `find_zfight.py`,
   `find_gaps.py`.
2. **The forecourt layout** - the bus carriageway, island, 12 sawtooth bays, 6 taxi spots, paint, kerbs, crossings,
   lights, the level pad, the holomap. In `Data/Sites/chablais/roads.json` and the road/ground code.
3. **The coaches use it** - bays wired to the existing `routes` loop, coaches pull in, kneel, open, load, pull out
   forward. Taxi spots stay empty.
4. **To-do 373** - passengers spawn in the town hub, ride a coach to a bay, walk in through the main doors, and the
   other way round.

No game run and no publish without the owner's word.

## The vault is approved - owner, 2026-11-27
He saw `docs/Pictures/busstation-vault.png` (the 16 m span, open both sides, 5.50 m clear, real glass) and said:
"Yeah, I like the look. You can go ahead." So the vault's form, span, column lines and materials are settled and
are not to be redesigned - the waiting hall, furniture, lighting and everything after it are built to match it.

## The ground truth - measured from the laid-out forecourt, 2026-11-27
Any builder touching the station or the deck uses THESE numbers, not its own. Read off
`Data/Sites/chablais/roads.json` at the station run; `y` is the world y, the terminal's landside wall is y 0.

| | y from | y to | width |
|---|---|---|---|
| Driving / Shoulder / Island / Driving / Driving | -46.75 | -31.60 | the through forecourt |
| Parking (closed over the run) | -31.60 | -31.55 | 0.05 |
| Bus through lane | -31.55 | -28.05 | 3.50 |
| **bay apron** - coaches stand and swing here | -28.05 | -18.35 | 9.70 |
| **island platform** - centreline y -14.35 | -18.35 | -10.35 | 8.00 |
| pavement to the terminal doors | -10.35 | +0.05 | 10.40 |

The vault's columns are at **y -17.35 and y -1.35** (16 m span, leaning toward the terminal), one metre in from the
platform's bus-side kerb and 1.35 m clear of the terminal wall, with the roof cantilevering 3.5 m south over the bay
noses. A vault centred on the platform instead puts a column line at y -25.35, in the middle of the apron where the
coaches park - that was built once and had to be redone. Nothing structural stands between y -28.05 and y -18.35.

## Pictures - the owner's go, 2026-11-27
"You're welcome to send some renders of the bus station area with buses and people when you get to that point."

So: once the station is imported, the bays are laid out, coaches use them and passengers walk the platform - step 4
done, not before - run the game for pictures and send them. 3840x2160 PNG. The whole subject in frame: coaches
standing in the bays and people on the platform and at the crossings, not an empty deck. I look at every one myself
before any of them goes to him, against the A320 clean after-pictures as the bar. Earlier steps get the blueprint
sheet and at most one still render of the model, as usual - this go is for the finished area with life in it.

## The order is settled - owner, 2026-11-27 18:45
He was asked whether to run the game now just to drive the town-to-airport road, or to finish the station first and
run it once at the end. His answer: "Finish the station first, hold furniture, lighting, import coaches in the
[bays], and then run the game once for the pictures with a whole route."

So there is **one game run**, at the end, and it covers everything at once:

1. The waiting hall and the platform furniture (in hand).
2. Lighting - disc luminaires on the arches and column uplights, through the lamp system with a distance limit, no
   flares - and the Unreal import. **Place the model at world y so model y 0 sits at world y -14.35.**
3. The coaches pull into A1-A12, kneel, open, load and pull out forward. Taxi spots stay empty.
4. **Then the one run**: coaches standing in the bays, people on the platform and at the crossings, and the route
   driven from the town hub to the bays so the road is seen to be whole. 3840x2160 PNG. I look at every picture
   against the A320 clean after-pictures before any of them goes to him.

Nothing is run before step 3 is done. No publish without his word.

## The road was checked - 2026-11-27, measured, not run
`Data/Sites/chablais/roads.json`: the town bus stop to the bus station bays is connected end to end -
TOWN-HUB, TOWN-HUB-STREET, TOWN-HUB-SPUR, CHABLAIS-LINK, AIRPORT-BACK-TO-TOWN, AIRPORT-ROUNDABOUT,
AIRPORT-FORECOURT, with TERMINAL-RETURN as the arrival link. About 8.8 km, every join within 2.75 m, driving lanes
both ways on every road of the route, and `coach-to-terminal` already ends `lane 7 from bay-12 to bay` - the new bays.

Two route steps look like 60 m and 80 m breaks and are not: they are the lane-change run-ups, `join` 10 and 12 m, and
`CoachLoop::LoopJoin` (Coaches.cpp:572) lays a Hermite curve across the whole separation, subdivided every metre. A
connectivity check must not flag consecutive steps on the same road as a gap.

The one real break found: **CARGO-ACCESS** touches nothing - 937.2 m of road whose nearest end is 1487.75 m from
every other road, although its own note calls it the cargo terminal's gate out to the public road. Written up as
**to-do 374** (commit 9b2b53f7). Not on the bus station's path, so not fixed here.

## The clock shows the game's own time - owner, 2026-11-27
"Have the clock show the in-game clock." The two-faced platform clock is not a painted dial: its hands turn with the
game's clock, the same time the rest of the game runs on, and they keep turning as time passes and when time is sped
up. Hour and minute hands both; the minute hand steps or sweeps, either is fine, but it must agree with the hour hand.

Built for it in the model: each hand (`BusStn_Clock_HandH` / `HandM`, one pair per face, W and E) is a part of its
own, modelled pointing at 12 o'clock, with its origin on its face's boss, so the runtime only sets a rotation.
The wiring is C++ at the import step - the lamp/props path, not a web layer, and no per-frame world scan.

## The waiting hall sits on the pavement, not the platform - 2026-11-27
The first build put it on the island platform and had to cut it to 30.0 x 3.60 m: the 8.00 m deck carries kerbs,
tactile strips, slot drains and the column footing pads, so a 9 m hall cannot stand there. A 3.6 m wide room 30 m
long is a corridor, not the hall the plan asks for.

The room is on the **pavement** instead - AS BUILT (commit b7b379c4): model y 4.35..12.00 (world y -10.00..-2.35),
**30.0 x 7.65 m**, under the vault. The north wall stops at y 12.00 because the north column line's 1.30 m footing
pads run y 12.35..13.65: the frame clears their face by 0.27 m and the roof edge stops 0.03 m short of it. The pavement is 0.18 m below the deck, so the hall's floor
and sill sit at pavement level with a real threshold at each door, and the doors face the platform. The platform is
left clear its whole length for passengers, benches and the bay signs.

## It stands in the world, lit, with the clock running - AS BUILT 2026-11-27
Imported (1036 nodes, 1101 uassets) and placed from `Airfield.cpp`, on the model site only: plan x 275.7558,
plan z -14.35 (world 27576, 1435), ground + sink + 0.03 m, actor yaw 180 - `Place` adds its half turn, so Blender
+x runs east along the platform and +y looks at the terminal, with model y 14.35 on the terminal's landside wall.
The x comes from the road's own bays (`roads.json` paint bays from 1456.73, pitch 8.51833 m): the twelve sign posts
sit on the twelve bay centres. The facing is checked at run time from the imported parts (hall panel vs. road-side
uplight, signs A01 vs. A12) and logged; the wrong way round warns.
`BusStationLife.cpp` lights it from `Data/busstation-lights.json`: 42 disc spots down from the vault (2400 cd,
22 m, draw 250 m), 42 uplights along their aim (650 cd, 13 m), 21 hall rect panels (900 cd, 10 m), all through
`AddLamp` - no shadows, no flares, no haze, dimmed by day - and the 105 lamp faces take the kit's `lamp` material.
The clock: four hand groups bound once on their own bosses, turned every second from the site's time (or
`ASunClock::HourOverride`), 30 deg an hour, 6 deg a minute, nothing cached, so a sped-up day moves them.
Open: the model labels the signs A1 west -> A12 east, the road numbers the bays A1 east -> A12 west, so the labels
are back to front (the positions are right). Fix in `make_busstation.py`'s sign loop.
