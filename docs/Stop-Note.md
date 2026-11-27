# Stop note - Friday 27 November 2026: the Airbus A320neo model

You said: "And just stop the creation of the Airbus model." It is stopped. Nothing is running on it.

## Done
- The whole fuselage shell is built and pushed (last commit 2ca6b358): nose, radome, barrel, tail cone, APU,
  fin and tailplane roots, 38 cabin windows a side, every door its own hinged part modelled open, panel lines,
  skin joints, rivet rows, belly and wing-root fairing, service panels, probes, antennas.
- The cockpit windows were rebuilt against two licensed photos (head-on and side), scaled by the Door 1 opening.
  That round found and fixed the real position: the glazing had to move about 1 m aft, to straddle the measured
  pilot eye point.
- The nose seam work (the radome joint and the skin stations across the curved nose) is committed, so the two
  faults I found in the 4K render - a ring of latch boxes standing 13 mm proud, and the nose terraced into bands
  because the skin was only cut at frame stations - have a fix in the file.

## Half-done - measured but NOT built
Your two notes are written into `Art/A320neo/BUILD-PLAN.md` so no future builder can miss them, but the model does
not have them yet:
- **The window pillars.** Measured off the photos: the post between window 1 and 2 is raked about 30 deg leaning
  aft, the post between 2 and 3 is vertical - they are not alike, which is the thing that keeps getting built wrong.
  The gaps are 55 mm at the first post and 100 mm at the second, not equal.
- **The sill falls going aft** (4.46 m down to 4.28 m); an earlier round built it rising. Wrong sign.
- **The nose roundness** you objected to: the fix is committed but I never saw a render of it, so I cannot tell you
  it is round again. It is unjudged.

## Next, when you say so
Nothing until your word. Then: judge the nose in one render, build the pillar and sill corrections, and only after
that the wing - and I tell you the wing plan first, as you asked, before any of it is built.

## Testing issues
None - nothing was run. No game run, no publish.
