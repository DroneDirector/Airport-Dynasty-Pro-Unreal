# Baggage claim: concept v2 (to-do 72)

These are concept renders, not the game model: a look to agree on before building. The Blender file is
`Art/BaggageClaim/BaggageClaim_concept.blend`, made by `Art/BaggageClaim/render_claim_concept.py`.

The blueprints are `docs/Baggage-Claim-Blueprint.png` (overview) and `docs/Baggage-Claim-Detail-Blueprint.png` (detail with a side section).

Plan metres: x runs along the building and z runs north (airside). Heights are above the Floor 1 floor.

## What changes in the terminal (owner, 2026-11-16)

- **Removed:** the museum hall (x -36 to 0) and everything inside it: its walls, lamps, the aircraft display, its door and its kerb canopy. Nothing else is removed.
- **Not touched:**
  - nothing stands above the museum hall;
  - the Floor 2 toilets and family room start at x 0 on the escalator landing and stay;
  - the ground west of it (x -80 to -36) is empty.
- **The arrivals wing:** fills x -80 to 0, flush with the end of the gates' pier.
- **The museum strip** (x 0 to 6, z 30 to 52) becomes a store room off the claim hall.

## Sizes

| Part | Where (plan x, plan z) | Size |
|---|---|---|
| Claim hall | x -80 to 0, z 16 to 52 | 80 m x 36 m, ceiling 5.4 m |
| Arrivals corridor (Floor 2) | x -80 to -66, z 40 to 52 | 8 m open door from the gates' walkway; floor at 6.0 m |
| Escalators (2, down) + stair | x -79.6 to -74.2, z 39 to 51.5 | 6.0 m rise; top and bottom landings kept clear |
| Lift | x -73.4 to -71.2, z 47.6 to 50.4 | |
| Customs | x -56 to -24, z 8 to 16 | green channel x -44 to -36, red channel and desk x -54 to -48 |
| Arrivals hall | x -80 to 0, z -4 to 8 | meeting rail, car rental + taxi desks, cafe |
| Arrivals doors | x -48 to -32 at the kerb front | 4 sliding doors under a canopy 32 x 9 m |
| Outside | | glass front and west end to 5.4 m, metal cladding to the 12 m roof line, ARRIVALS over the doors |

## The belts

There are 5 flat-plate carousels.

| | |
|---|---|
| Centres | x -60.8, -48.15, -35.5, -22.85, -10.2; z 31 |
| Shape | racetrack: 15 m straight, 1.8 m centre-line radius |
| Belt length (where bags can be taken) | 41.3 m each |
| Overall footprint | 19.9 x 4.9 m |
| Plates | 0.92 m wide, top 0.45 m above the floor |
| Speed | 0.46 m/s |
| Walking room | 7.75 m between belts and round them |
| Feed shaft | at each island's north end; bags rise up an 18 degree incline under a stainless hood with flaps |
| Crew side | per belt, an input conveyor in the sorting hall (6 m, 0.75 m high) into the north wall, with a cart spot |

## Render cameras

The same data is in `Art/BaggageClaim/cameras.json`.

| Render | From (x, z, height) | Looking at (x, z, height) | Lens | Shows |
|---|---|---|---|---|
| 1 | -75.2, 41.6, 2.8 | -40, 24, 0.4 | 18 mm | Halfway down the stair: the whole hall, all five belts |
| 2 | -36, 18.2, 1.65 | -44, 38, 1.1 | 22 mm | By the customs doorway looking north |
| 3 | -32.1, 35.3, 1.55 | -34.9, 37.1, 0.75 | 30 mm | A bag rising out of belt 3's feed shaft |
| 4 | -31.7, 60.8, 1.75 | -35.9, 52.6, 0.95 | 24 mm | Crew side: input conveyors into the wall, carts |
| 5 | -74.5, 31.5, 1.65 | -77.8, 47, 4.2 | 22 mm | The escalators and stair from below: free to use |
| 6 | -70, -2, 1.7 | -40, 8, 1.6 | 20 mm | Arrivals hall: customs exit, meeting rail, desks, doors |
| 7 | -40, -34, 1.7 | -40, -4, 4 | 24 mm | Outside: the arrivals doors and canopy |
| 8 | -125, -75, 32 | -38, 14, 4 | 28 mm | Outside from above: the whole wing |

## How it works in the game (owner, 2026-11-16; build these with to-do 72)

**The belts**
- A belt only runs once someone starts it. The plates and the feed incline move, and so do the bags on them.
- **Crew start button:** a ground crew member presses the green button on the yellow control box at the belt's loading end, in the sorting hall.
- **Passenger start/stop button:** a green button on each belt's skirt. One press starts the belt, the next press stops it, and so on.
- **The buttons:**
  - every button has the glowing interact outline, like everything else you can use (E / X);
  - the button visibly goes in and comes back out on each press.

**The bags**
- A bag stays fixed to the belt it sits on. It never slides or flies off as the belt turns.
- **Who may handle bags:**
  - only ground crew put bags on the input conveyors, and only they may take one back off;
  - in the claim hall, only passengers take bags off the belts.

**The rubber flaps**
- The flaps at all 10 openings (the 5 feed hoods in the claim hall and the 5 crew-side wall openings) swing aside like cloth when a bag pushes through. Then they fall back and settle into place.

**The escalators**
- The arrivals escalators are the same model as the escalator room's: moving steps and handrails, with the rounded newels at both ends.

- **Cedar feature wall:** "Japanese Cedar Planks", Poly Haven (CC0), https://polyhaven.com/a/japanese_cedar_planks (4K diffuse, normal, roughness in Art/BaggageClaim/textures).

**The belt sounds** (owner, 2026-11-16)
- **BeltStart:** plays once when a belt is started. It fades out at its end (0.8 s), so it never cuts off.
- **BeltRun:** loops seamlessly (its ends are crossfaded) for as long as the belt runs, with no fade or gap while it runs. When the belt is stopped, by the crew or by you in a test, it fades out over about 1.5 s as the motor spins down.
- **BeltClack:** a single click that is never cut off.

**Bags arriving**
- Every time an aircraft lands, the ground crew empty its hold (bags and cargo).
- They bring the bags to that flight's belt input and start the belt.
- The bags then come up on that belt in the claim hall.
