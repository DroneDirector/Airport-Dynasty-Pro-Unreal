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

## Cockpit windows - rebuilt 2026-11-27 from two licensed photographs

| Source | Licence | Used for |
|--------|---------|----------|
| Wikimedia Commons, "EZY nose close up.jpg" | CC BY 3.0 | head-on cockpit window reference |
| Wikimedia Commons, "Lufthansa A320-211, D-AIPT, named Cottbus (5650386693).jpg" | CC BY 2.0 | side cockpit window spacing reference |

Both photographs were opened and measured (the side one scaled against the 0.81 m width of door 1);
nothing was copied from them. Attribution is required by both licences and is carried here.

Six panes, three a side, still built from ONE table (`CKWINS` in `make_a320neo.py`) and mirrored in y.
Each outline is now given as real side-view (x, z) corners and solved onto the skin by Newton
iteration, so the published metres land on the surface instead of being drawn in board coordinates.

What the photographs show, and what the script now builds:

* the three windows a side are ONE band under ONE straight, level top line - no staircase;
* 0.05 m of bare skin between the panes' frames (0.166 m glass to glass), not 0.09-0.18 m;
* the sill rises steadily aft, 0.23 m over the 1.52 m band;
* panes 0.664 / 0.578 / 0.507 m tall; the old ones were 0.34 m and read as cabin windows;
* a narrow 0.058 m frame standing 0.012 m proud, 0.040 m corner radius, rivet row along the top edge
  and down both posts - not the earlier 0.026 m rounded-square bezel;
* one wiper per windscreen, parked horizontal along the lower sill, pivot on a boss at the inboard
  bottom corner.

**The published number that could not be met.** `doors.cockpit_windows.x_range = [1.13, 2.65]` cannot
carry this band on this fuselage: at x 1.13 the crown of the nose is z 4.235, so the measured 4.95 m
top line would be 0.72 m above the skin, and only 0.07 m of skin exists between the crown and the
measured 4.17 m sill. A level line at z 4.95 first touches the skin at x 2.095. The band is therefore
set 1.02 m further aft - glazing x 2.011-3.602, z 4.233-4.892 - keeping every measured height, pane
size, post width and sill slope. Two consequences I cannot remove without changing the nose
cross-section itself: the windscreen's inboard top corner lands at |y| 0.28 instead of the measured
0.06 (a level line only reaches the centreline at the crown cusp), so the centre post is wider than
the real 0.12 m, and head-on the windscreen reads as a wedge rather than a trapezoid with a vertical
inboard edge. Settling that needs the nose section re-measured from a square-on photograph.

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
