# A320neo - sources for the model (step 1, fuselage shell)

Own model, built by script from published dimensions and from the drawing sheets already in this repo.
No model file, mesh or texture was copied, bought or downloaded. No livery, logo or registration is modelled.

## Honest note on photographs
The builder that wrote `make_a320neo.py` has **no web access**, so no new reference photographs were
fetched or viewed for this step. Everything below was already inside the repository. Where a shape could
not be taken from a drawing it was taken from the dimension tables, and the places where that leaves a
judgement call are listed in "Open points" at the bottom.

## Dimensions and structure
| # | Source | Used for | Licence / use |
|---|--------|----------|----------------|
| S1 | Airbus S.A.S., *A320 Aircraft Characteristics - Airport and Maintenance Planning* (AC), rev. Jul 01/26, fig. 2-2-0 (sheets 1-2), 2-3-0, 2-6-0 | overall length 37.57 m, fuselage 3.95 x 4.14 m, cabin floor 3.38 m, door sizes and sill heights, cargo door sizes, fin/tailplane root stations, APU exhaust station | Published free of charge by Airbus for airport and operator planning; figures and dimensions used as data, drawings not reproduced |
| S11 | Airbus SRM 51-00-00, fig. 6 sheet 2 (frame station list) | frame stations FR1-FR87, barrel section breaks FR24/35/47/64/70/77 | dimension data only |
| S12 | Airbus AMM 06-31-53, fig. 005A / 006A (aft and tail frame stations) | FR64-FR87 stations in millimetres | dimension data only |
| S13 | ATA 06 zone and station diagrams | nose landing-gear bay FR9-FR20, rear pressure bulkhead FR70, centre wing box FR37-FR47, APU doors FR80-FR84 | dimension data only |

All of the above are condensed into `Art/A320/blueprint/a320neo-data.json`, which this script reads; the
frame-station arithmetic is the same as in `Tools/make_a320neo_wireframe.py`.

## Drawing sheets viewed while modelling (in this repository)
`docs/Pictures/a320neo-blueprint/` - produced by `Tools/make_a320neo_blueprint.py` and
`Tools/make_a320neo_wireframe.py` from the data above, so they carry no third-party licence:

* `ext-side.png`, `ext-top.png`, `ext-front.png` - outer shape, window pitch, door positions
* `wire-side.png`, `wire-plan.png`, `wire-front.png` - frames as rings, stringer runs, floor beams,
  keel beam, pressure bulkheads, door surrounds
* `wire-iso.png` (sheet 12) - isometric structural wireframe; used to check that the barrel, the nose
  taper and the tail cone follow the real frame rings and stringer lines

## Cockpit windows - how the six panes were derived (no photograph was available)

Six panes, three a side, built from ONE table (`CKPT` in `make_a320neo.py`) and mirrored in y, so the
port and starboard sets are identical by construction, not placed twice by hand.

Anchors, all from `Art/A320/blueprint/a320neo-data.json` (read only):

* `doors.cockpit_windows.CP1_ground_MRW_fwd_measured = 4.17` m - the measured ground clearance of
  point CP1 at maximum ramp weight, forward CG. CP1 is the lower aft corner of the No. 1 windscreen;
  the model puts that corner at 4.17 m.
* `cockpit.windows.x_range = [1.13, 2.65]` m aft of the nose tip and `z_ground_range = [4.16, 4.95]` m -
  the band the whole glazing has to live inside. The model's glazing runs x 1.18-2.66, z 4.04-4.88.
* `cockpit.pilot_eye = (x 2.41, y 0.53, z 4.56)` with a 20 deg over-the-nose look-down - used to check
  that the windscreen lower edge really is below the eye line.

Shape: each pane is a rounded rectangle on the fuselage surface whose UPPER edge follows a constant
ring angle (so it lies along the cockpit roof line) while the LOWER edge rakes with x. That makes the
windscreens trapezoids that wrap round the nose instead of flat rectangles on the side, which is what
`ext-front.png` shows. The rake is applied inside `rr_points` through the `SHEAR` table.

Deviations I cannot prove: the forward lower corner of the windscreen comes out at 4.04 m, 0.12 m below
the published 4.16 m minimum, because the pane wraps over the nose where the skin falls away; and the
highest glazing point is 4.88 m against a published 4.95 m. Both need a square-on photograph measured
against a known length (door height, or the 0.533 m cabin window pitch) to settle, and I have no web
access in this environment, so no photograph was fetched, viewed or measured.

## Correction made during the build

The radome joint plane was first put at x = 1.55 m by eye. It is FR1, x = 0.958 m aft of the nose tip
(S11). With the joint at FR1 the published cockpit-window band (1.13-2.65) sits correctly aft of it;
with 1.55 it did not. The script now uses `X_NOSE = FX[1]`.

## Look standard
`docs/Pictures/a320-panels/A320-clean-cowl-AFTER-open.png`, `-wheels-AFTER.png`, `-door-AFTER-open.png`
(this project's own renders) - the finish the shell renders were compared against.

## Open points (not proven from a photograph)
* The exact shape of the slide bustle on the back of the passenger doors, the shape of the door handle
  recess, and the exact blade shapes of the VHF/ATC/DME aerials are reasoned from the dimension data and
  normal practice, not traced from a photograph. They should be checked against real photographs before
  texturing (step 7).
* "4 passenger doors and 2 service doors" in the build plan: the real aircraft has 1L/1R/4L/4R only,
  with 1R and 4R used as the service doors. Built as 1L/1R/4L/4R plus two true service access doors
  (forward avionics hatch on the keel at x 4.00 m, aft equipment door lower left at x 24.50 m).
* Positions of the SATCOM, GPS, radio-altimeter and DME aerials are typical, not type-certified exact.
