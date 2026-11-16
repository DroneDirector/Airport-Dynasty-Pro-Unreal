# Baggage claim: concept (to-do 72)

These are concept renders, not the game model: a look to agree on before building. The Blender file is
`Art/BaggageClaim/BaggageClaim_concept.blend`, made by `Art/BaggageClaim/render_claim_concept.py`.

Plan metres follow `terminal-plan.json`: x runs along the building and z runs north (airside). Heights are above the Floor 1 floor.

## Sizes

| Part | Where (plan x, plan z) | Size |
|---|---|---|
| Claim hall | x -44 to 0, z 16 to 52 | 44 m x 36 m, ceiling 5.4 m |
| Arrivals corridor (Floor 2, above the hall's north part) | x -44 to 0, z 40 to 52 | 44 m x 12 m, floor at 6.0 m |
| Escalators (2, down) + stair + lift | x -43.6 to -38.2, z 39 to 51.5; lift x -36.6 to -34.4, z 47.6 to 50.4 | 6.0 m rise, about 30 degrees |
| Customs (green / red) | x -44 to -16, z 8 to 16 | doorway x -32 to -24 |
| Arrivals hall | x -44 to 0, z -4 to 8 | doors to the kerb at x -30 to -18 |
| Sorting hall input side (crew) | behind the north wall, z 52.3 to 68 | ceiling 5.4 m |

## The belts (3, flat-plate carousels)

| | |
|---|---|
| Centres | x -31, -19, -7; z 31 |
| Shape | racetrack: 15 m straight, 1.8 m centre-line radius |
| Belt length (where bags can be taken) | 41.3 m each (IATA: about 37-40 m for a large narrow-body) |
| Overall footprint | 19.9 m x 4.9 m (stainless skirt 0.40 m high) |
| Plates | 0.92 m wide, 0.3 m long, top 0.45 m above the floor, dark rubber |
| Centre island | 2.6 m wide, 0.62 m high, stainless sides, dark top |
| Speed | 0.46 m/s (90 ft/min, the usual carousel speed) |
| Gap between belts | 7.1 m of walking room |
| Feed shaft | at each island's north end: an opening 1.85 x 1.2 m, an inclined belt rising at 18 degrees out of it onto the plates, under a stainless hood with rubber flaps |
| Screens | one per belt, 2.0 x 1.125 m, hung 3.0-4.1 m up over the north end, showing its flights |

How the bags travel in the game:

1. The crew put each bag on its belt's input conveyor in the sorting hall. Each input conveyor is 6 m long and 0.75 m high, has a cart parking spot beside it, and runs to a yellow-framed opening in the wall with rubber flaps.
2. The bag rides into the wall and is hidden there; there is no basement.
3. The same bag then rises out of the shaft in the claim hall, comes through the flaps and drops onto the plates.
4. It goes round at 0.46 m/s until someone takes it.

## Render cameras

The same data is in `Art/BaggageClaim/cameras.json`.

| Render | From (x, z, height) | Looking at (x, z, height) | Lens | Shows |
|---|---|---|---|---|
| 1 | -39.2, 41.6, 2.8 | -14.0, 25.0, 0.4 | 18 mm | Halfway down the stair: the whole hall, all three belts |
| 2 | -15.5, 18.2, 1.65 | -20.0, 38.0, 1.1 | 22 mm | A passenger by the customs doorway looking north: belts, screens, basalt wall, escalators |
| 3 | -15.6, 35.3, 1.55 | -18.4, 37.1, 0.75 | 30 mm | Close up at belt 2: a bag coming up out of the feed shaft through the flaps |
| 4 | -15.2, 60.8, 1.75 | -19.4, 52.6, 0.95 | 24 mm | Crew side in the sorting hall: belt 2's input conveyor into the wall, a baggage cart |

## Materials in the concept

| Surface | Material |
|---|---|
| Floor | Polished terrazzo in 1.2 m slabs |
| Ceiling | Oak slats with LED lines between them |
| East wall | Dark basalt feature wall (the museum side) |
| West wall | Glass wall onto the airfield |
| Belt skirts, islands and hoods | Brushed stainless steel |
| Plates and flaps | Black rubber |
| Sorting hall | Worn concrete floor and walls (our CC0 scans), dark steel beams, yellow safety frames and lines |
