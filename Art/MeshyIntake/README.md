# Meshy models - intake 2026-11-26

The owner's 11 Meshy models (commercial plan, see THIRD-PARTY-NOTICES.md). Not in the game yet; this sheet is
for the builders who take them on one by one.

- Source: `\Synology\home\Airport Dynasty Pro\3D Files\Meshy\26.11.2026` (each `.obj` there is a ZIP: obj + mtl + png;
  the `.stl` is ignored). Reach it with Python/PowerShell and `//Synology/home/...`; Git-Bash paths fail.
- Unzipped to `F:/ADP-Meshy/2026-11-26/<Name>/` (big data stays on F:, never in the repo).
- `meshy_intake.py` (Blender 3.3, headless) imports each OBJ, writes faces, raw bounding box and
  `F:/ADP-Meshy/2026-11-26/intake.json`, and renders `previews/<Name>.png` (1024x576, Workbench, textured, 3/4 front):
  `blender -b --factory-startup -P Art/MeshyIntake/meshy_intake.py -- F:/ADP-Meshy/2026-11-26 [Name ...]`

## What every model has in common
- One mesh object, one material ("Material") with one baked colour texture, no empty slots. No parts: everything
  that moves has to be cut out into its own named part with its pivot (or rebuilt) by the builder.
- Up axis **Y** (glTF style); Blender import with the default Y-up conversion stands the models upright (checked
  in all 11 previews). Front points to **-X** (nose, cab, forks), except where noted.
- Meshy normalises the longest side to about 1.9 units; scale is arbitrary. Raw size below is L (X) x W (Z) x H (Y).
- Very heavy, dense triangle soups (0.3 - 3 M faces): Nanite is a must, and the vehicles need a clean-up/retopo
  before parts can be cut.
- **Brands to remove from the textures** (no real brands on game models): Lorry (truck-maker style grille badge),
  CargoForklift ("CARRGO MAX" decals), XWayMover ("HUBTEX", "X-Way"), VehicleD3abbc ("TLD"), B757 ("BOEING" on the
  tail and nose). Not checked at full texture resolution: look at every texture before import.
- Proportions are Meshy's, not real: scaling uniformly to the real length gives the "uniform" size in the table;
  where that is off, the builder must fix it (non-uniform scale or reshaping), noted per row.

## Table

| Name | What it is | Faces | Raw L x W x H | Real size to scale to (L x W x H, m) | Cab interior | Parts that must move |
|---|---|---|---|---|---|---|
| Lorry | Road tractor (3 axles) with closed box semi-trailer (3 axles) | 1,532,684 | 1.905 x 0.422 x 0.532 | EU legal max 16.50 x 2.55 x 4.00. Uniform to 16.5 m gives W 3.65 / H 4.61: far too wide and high, needs reshaping | Yes (closed cab, glazed) | 12 wheels, front axle steering, trailer pivot on the fifth wheel, rear trailer doors, cab doors, landing legs, mirrors fold optional |
| CargoForklift | 3.5 t diesel/LPG counterbalance forklift, pneumatic tyres, open overhead guard | 1,989,672 | 1.903 x 0.789 x 1.191 | Approx. 4.0 (incl. 1.2 m forks) x 1.5 x 2.3 (overhead guard), mast lowered ~2.3. Uniform to 4.0 m gives W 1.66 / H 2.50: a bit big | Open seat + controls (no closed cab), seat and levers must be real | 4 wheels, rear-axle steering, mast tilt, inner mast lift, fork carriage, forks (side shift), levers |
| CrateFetcher | Heavy rough-terrain forklift with telescopic boom on the mast (container/crate handler), twin front tyres | 2,128,912 | 1.904 x 0.787 x 1.415 | Heavy 12-16 t class: approx. 6.5 (incl. forks) x 2.5 x 4.3. Uniform to 6.5 m gives W 2.69 / H 4.83: slightly tall | Yes (closed cab) | 6 wheels (twin front), rear steering, mast lift, carriage, forks, boom luff + telescope, rear hook/clamp, cab door |
| XWayMover | Electric multi-directional ULD/pallet transporter, roller deck, stand-on driver post at +X | 1,081,618 | 1.903 x 0.716 x 0.815 | Estimate (no published figure found in this step): approx. 4.8 x 1.8 x 2.05 (to top of driver post), deck carries one 2.44 x 3.18 m pallet. Check against the maker's data sheet before modelling | No (stand-on platform, control column) | 4 wheels, each steering ~90 deg (crab/side drive), deck rollers spin, control column/handles |
| VehicleD3abbc | **Two-platform ULD loader** (lower-deck/main-deck type): long scissor main platform with roller deck + raised bridge platform with operator console and rails at +X | 2,670,870 | 1.904 x 0.651 x 0.786 | 7 t loader class: approx. 9.0 x 3.1 x 3.7 (incl. bridge rails, lowered). Uniform to 9.0 m gives W 3.08 / H 3.72: fits | No (operator stands at the bridge console) | wheels + steering, main platform scissor lift, bridge platform lift, bridge extension/side shift, roller deck, guide rails, stabilisers, ladder |
| Vehicle1b0cf6 | **Single-platform ULD high-lift transporter**: driving chassis, scissor lift, roller deck with fold rails and console, 4 stabiliser jacks (shown raised, with a netted pallet on the deck) | 2,988,738 | 1.902 x 1.251 x 1.321 | Estimate: approx. 6.0 x 3.3 x 4.2 (raised as modelled), deck sized to a 3.18 x 2.44 m pallet; uniform to 6.0 m gives W 3.95 / H 4.17: too wide, needs reshaping. Deck pallet should be a separate cargo part | No (console on the deck) | 4 wheels + steering, scissor lift, roller deck, fold rails, 4 stabiliser jacks, console |
| B757 | Boeing 757-200, blended winglets, white | 490,806 | 1.904 x 1.525 x 0.535 | 47.32 x 41.07 (span with winglets; 38.05 without) x 13.56. Uniform to length gives span 37.9: wings short | Cockpit (seen through windows) | gear (wheels roll, nose steering), passenger + cargo doors, control surfaces, engine fans |
| B747 | Boeing 747-400 (winglets), white | 368,874 | 1.902 x 1.483 x 0.499 | 70.66 x 64.44 x 19.41. Uniform to length gives span 55.1: wings far too short | Cockpit | gear (body + wing gear, nose steering), doors, cargo doors, control surfaces, 4 engine fans |
| A380 | Airbus A380-800, white | 355,804 | 1.849 x 1.904 x 0.636 | 72.72 x 79.75 x 24.09. Uniform to length gives span 74.9: wings short | Cockpit | gear (nose, wing, body), doors on 2 decks, cargo doors, control surfaces, 4 engine fans |
| A320neo | Airbus A320neo (sharklets), white | 380,854 | 1.904 x 1.741 x 0.632 | 37.57 x 35.80 x 11.76. Uniform to length gives span 34.3: close | Cockpit | gear, doors, cargo doors, control surfaces, engine fans |
| A321 | Airbus A321ceo (wingtip fences), white | 315,702 | 1.903 x 1.515 x 0.535 | 44.51 x 34.10 x 11.76. Uniform to length gives span 35.4 / H 12.5: close | Cockpit | gear, doors (incl. overwing exits), cargo doors, control surfaces, engine fans |

Real figures: makers' published airport-planning data for the aircraft; EU 96/53/EC limits for the lorry; usual
data-sheet ranges for the forklifts and loaders (approximate, marked as such). Raw sizes and faces from
`intake.json`. The two unknown vehicles were identified from their previews only.
