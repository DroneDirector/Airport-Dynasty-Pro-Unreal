# Taxi bus (minibus taxi): research

The taxi bus waits at the landside kerb outside the terminal, in the bus and taxi lanes, and takes up to 8
passengers with their luggage to town or to the hotels. It is our own design, in the class of the long high-roof
passenger vans. It has no brand names, no logos and no copied styling.

## #region ── Real sizes (long high-roof passenger vans) ──

| Item | Real (class) | Our model |
|---|---|---|
| Length | 5,490 (L2 electric van), 5,620-5,700 (LDV V80 long), 5,915 (HiAce Commuter L2H2), ~5,930 (long wheelbase vans) | 5,930 |
| Width (body) | 1,950-2,000 | 2,020 |
| Height | 2,180-2,550 depending on roof | 2,400 (high roof), 2,590 over the taxi sign |
| Wheelbase | 3,665-3,850 (long vans) | 3,665 |
| Overhangs | front ~1,000, rear ~1,265 | 1,000 / 1,265 |
| Tyres | 235/65 R16C (light commercial) | 235/65 R16C, 711 across |
| Track | ~1,700 | 1,720 |
| Cabin floor | ~550-600 above the road | 580, with an entry step at 360 under the sliding door |
| Turning circle | 11,000-12,400 kerb to kerb | (game: 12.4 m) |
| Seats | 8 passengers + driver: the most a taxi may carry on an ordinary car licence (category B, EU) | 8 + 1 |

## #endregion

## #region ── What the model has ──

- **Body:** a hollow shell with a rounded outline, wheel arches with liners and lips, and mud flaps.
- **Doors:**
  - Two hinged front doors with windows, trim, armrests, pockets, hinges, handles and mirrors (with
    indicator repeaters).
  - The sliding door on the kerb side, with privacy glass, a handle inside and out, three tracks (top, waist and
    bottom), an entry step with a yellow edge, and a grab pole.
  - Two rear doors split 50/50, with windows, hinges, a handle, a rear wiper and a reversing camera.
- **Front:** a grille with bars, headlamps, indicators, fog lamps, a bumper with its lower lip, a blank plate,
  a cowl, and wipers parked on their pivots.
- **Rear:** tail and brake lamp columns, indicators, reversing lamps, a third brake light, a bumper with a step plate
  and a blank plate.
- **Roof:** the lit TAXI sign box (a plain lit panel, with no text in the mesh), roof rails on feet and an antenna
  fin.
- **Under the van:**
  - Front struts, springs and steering knuckles.
  - A rear beam axle with leaf springs, dampers and the differential.
  - The drive shaft, the sump, a battery tray and a cooler box.
- **Inside:**
  - The floor board with a rubber mat and wheelchair anchor tracks.
  - Front and rear wheel houses, lower lining, window ledges and the headliner.
  - Ceiling lights, air ducts with vents, roof grab handles, and a dome light with the taxi sign switch.
  - The interior mirror and sun visors.
- **Seats (8 + 1):**
  - The driver, the front passenger, two seats in row 2, two in row 3, and a three-place rear bench.
  - The aisle runs on the kerb side.
  - Every seat has a cushion, bolsters, a back, a headrest on posts, and a three-point belt with its buckle.
  - The row 2 and 3 seats have a hand grip on the back.
- **Driver's place:**
  - The dashboard with its vents, the cluster hood and screen, and a centre screen.
  - The **taximeter** (with a lit display) and the **card terminal** on its arm.
  - The gear selector, the park brake switch, the steering column, the wheel and its spokes, the pedals on their
    posts, and a foot rest.
- **Luggage area:** behind the bench, with four tie-down rings. The folding **wheelchair ramp** (two channels with
  treads and a hinge bar) stands upright against the rear doors.

## #endregion

## #region ── IK points and pivots ──

- **People:**
  - Every seat has seat, back, head and buckle points (36).
  - The driver's hands on the wheel, the pedals, the taximeter, the gear selector and the card terminal.
  - The roof grab handles and the entry grab pole.
  - The outside door handles and the entry step.
  - Luggage spots and the wheelchair space.
- **Pivots** (named PIV_…): the front doors, the sliding door, the rear doors, the wipers, the ramp hinge and the
  front steering knuckles.

## #endregion

## #region ── Lights and sounds (for Unreal) ──

- **Lights:**
  - Headlamps, fog lamps, indicators and repeaters, tail and brake lamps, reversing lamps, the third brake
    light and side markers.
  - The TAXI sign (lit when the taxi is free), the taximeter display, the ceiling and dome lights, and the screens.
- **Sounds** (Sonniss GDC bundles or CC0):
  - The engine or electric drive, and the sliding door running and latching.
  - Hinged doors, the indicator relay, the taximeter beep, the card terminal beep, the reverse beeper and the
    horn.

## #endregion

## #region ── Sources ──

- vehiclesizes.com: Toyota HiAce VI Commuter L2H2 (5,915 x 1,950 x 2,280) and the Farizon SV L2H2 passenger van
  (5,490 x 1,980 x 2,180).
- carexpert.co.nz: LDV V80 (lengths 4,950-5,700, width 1,998, heights 2,132-2,552, wheelbases 3,100 and 3,850, turning
  circle 12,400) and the HiAce (turning circle 11,000, width between the wheelhouses 1,265).
- EU driving licence category B: up to 8 passenger seats besides the driver's.

## #endregion
