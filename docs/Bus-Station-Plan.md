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
