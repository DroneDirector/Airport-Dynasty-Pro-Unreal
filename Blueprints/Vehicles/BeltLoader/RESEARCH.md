# Belt loader (electric, self-propelled) - research notes (2026-11-21)

A self-propelled conveyor that carries bags up from the baggage carts into a narrow-body's bulk holds
(A320 family: forward + aft holds, bulk compartment), and down again on arrival. The class built by TLD (NBL / NBL-E),
Mallaghan (belt loader, SkyBelt), Charlatte of America (CBL2000E), ACE, Phoenix MBL, Wollard. Battery-electric
versions run on an 80 V traction battery (lead-acid or Li-ion), the same family as our electric baggage tractor.

## Real units looked at (sizes)

| Unit | L x W x H (mm) | Boom / belt | Sill range | Weight | Power | Notes |
|---|---|---|---|---|---|---|
| **TLD NBL Electric** (dealer spec page, fortbrand.com) | ~8100 x ~2100 x ~1880 (operator canopy) | modular booms; "reaches the lowest doorsills on the market" | - | ~4100 kg | lead-acid / Li-ion / hydrogen / hybrid (older NBL-E: 80 V 450 Ah) | rear steps, handrails, operator station beside the boom |
| **Mallaghan belt loader** (catalogue listing) | - | 6.5 m boom / 8.0 m boom | 0.8-2.8 m (6.5 m boom), 0.8-3.6 m (8.0 m boom) | - | diesel / electric / petrol | |
| **Mallaghan SkyBelt** (fully electric) | - | - | ~838-4190 mm front sill | - | 80 V Li-ion | patented lift that keeps the boom's movement mostly vertical |
| **Charlatte of America CBL2000E** | - | 25 ft (7.6 m) conveyor, direct-drive belt | - | - | 80 V AC drive | flush weather-proof controls; most parts shared with the T137 tractor |
| Generic airport belt loader (dealer spec table) | - | 24 in (610 mm) belt | - | - | - | wheelbase 130 in (3302 mm) |

Typical figures across the class (spec tables + photos): belt 610 mm (24 in) wide, rough-top rubber, reversible,
~0.4-0.5 m/s; boom capacity ~1000 kg spread / ~100 kg per metre; boom angle up to ~20 deg; two hydraulic rams lift the
boom (front + rear lift points); an operator station on one side of the chassis beside the boom (open seat under a
canopy, or a closed cab for cold climates), push-button control boxes at the bottom and the top end of the boom (the
loader in the hold runs the belt), handrails along the upper boom, a rubber bumper at the top end that rests on the
fuselage below the door sill, amber beacon, work lamp at the boom top, 4-wheel chassis with front steering.

A320 bulk/aft hold sill (door bottom) at about 2.0-2.3 m above the ground depending on load -> ~2.10 m used for the
"raised" picture and the json (boom at -9.22 deg).

## Our design (no brand) - what was chosen and why

- **Overall 7841 mm** (boom bottom end -3831 to bumper pad +4010), **2160 wide** over the chassis (2550 over the
  mirrors), **2040 to the cab roof**, beacon top 2205. TLD NBL-E class length (~8.1 m) and width (~2.1 m).
- **Chassis** 5700 long, wheelbase 3300 (dealer table: 3302), track 1600, ground clearance 200, side decks at 950.
  6.50-10 class tyres 680 x 180 (lathed profile, no modelled tread). Front wheels steer +-30 deg.
- **Boom** 7600 long, belt 610 wide over two end drums 7440 apart (drums 200 dia, belt wraps them), slider bed under
  the top run, return run with idlers. Belt top at rest **976 mm** (bags put on at the bottom end ~1 m high).
  Hinged at its **lower (rear) end** (pivot x -3.20, z 0.76), raised by **two hydraulic rams** lying in a tunnel down the
  chassis middle (front ram base x -0.40, rear ram base x -2.70). Raised 0 to **20.5 deg** -> belt top end from
  **0.98 m to 3.42 m** (the class's 0.8/1.0-3.4/3.6 m range). A320 bulk hold 2.10 m = 9.22 deg.
- **Operator station: a half-cab on the left** (white, roof, windscreen, inner + rear glass, one hinged door with glass,
  kick step, grab handles inside + outside). Real units come both ways: the open seat under a canopy is the most
  common, but the European cold-climate units (and Iceland / Switzerland in our game) carry a closed cab, and the owner's
  crew standard wants a real door on every vehicle. Industrial suspension seat, steering column on a pedestal, pedals,
  direction stalk, key, e-stop, and a boom console with a joystick at the driver's right hand (boom up/down, belt
  forward/reverse).
- **Controls on the boom**: a push-button box at the **bottom end** (left channel, x -3.50) and at the **top end** (left
  channel, x +3.20): belt run (green), belt reverse, boom up/down toggle, red e-stop.
- **Top end**: rubber **bumper pad** on two arms hinged under the belt end (pivot x 3.86, z 0.755 boom-local) so it
  hangs upright against the fuselage as the boom tilts; **work lamp** on the right channel top; handrails both sides from
  x 0.90 to 3.45 (posts, top rail 700 above the channel, knee rail).
- **Right deck**: chequer-plate battery hatch (80 V battery under it), hydraulic tank with sight gauge and filler,
  extinguisher; charge socket and controller vent on the right side panel. Left rear deck: tool box.
- **Lamps** set into the steel: low + high beams, front indicators, side markers (front / middle / rear, both sides -
  over 6 m long), tail/brake, rear indicators, reversing lamps, red reflectors, amber roof beacon, boom work lamp.
- Colours: GSE yellow chassis + boom (the tractor's / tug's yellow), white cab + roof (the tractor's white), black rubber.

## Sounds (Freesound CC0) - see Art/Sounds/BeltLoader/SOURCES.md
Electric drive motor loop, belt rollers loop, hydraulic boom (pump + valve), reverse beeper, horn, door open/close.

## Sources
- TLD NBL Electric spec page: https://fortbrand.com/products/tld-nbl-electric/
- TLD NBL-E product page: https://www.tld-group.com/products/belt-loaders/nbl-e/
- TLD NBL (gas/diesel/electric): https://www.aerospecialties.com/aviation-ground-support-equipment-gse-products/aviation-baggage-cargo-service-equipment/belt-loaders/tld-nbl-belt-loader-gas-diesel-or-electric/
- Mallaghan belt loader listing: https://www.aeroexpo.online/prod/mallaghan/product-168807-79891.html
- Mallaghan SkyBelt: https://www.aviationpros.com/ground-support-worldwide/gse/product/55339984/mallaghan-skybelt-redefines-safety-and-efficiency-for-aircraft-belt-loading and https://aviationsouk.com/products/mallaghan-skybelt-belt-loader/
- Charlatte of America CBL2000E: https://www.aviationpros.com/gse/baggage-cargo/belt-loaders-conveyors/product/10027110/charlatte-of-america-cbl2000e
- Airport belt loader spec table (24 in belt, 130 in wheelbase): https://www.zieglercat.com/?p=229759
