# Stop note - 2026-11-27, evening

## Done
- Bus-station bay signs now count the way the road paints them (A1 at the east end), proved from the rebuilt
  model's own node positions. Commit `cdf14f46`, pushed.
- The plan doc corrected: plan y is not world y (`ToWorld` flips the sign).
- One full stress walk on the packaged **0.20.2** build (terminal route + coaches route, 13:00, visible,
  20:22-20:39, ended by itself). 65 fps drawn on average, slowest 1 % 43 fps, worst frame 71 ms, 3 bad frames
  after warm-up. Findings stored in memory (`stress-walk-1127-findings`) and indexed in the testing notes.
- A repeatable picture run `Tools/views/pics-cargo-hangar.txt` (commit `a30614e2`, pushed) and nine 4K PNGs of
  the cargo terminal and the hangars sent.

## Half-done
- Nothing is half-built. No code was changed for any of the seven issues below.
- One picture (`hangar-h1-inside-out`) came out a dud and was dropped, not retaken.

## Next, when the PC comes on again
He asked to be reminded: **the first job is fixing the seven issues the stress test found**, worst first.

1. A coach cannot be walked into - the feet stay 1.63 m out of the coach's own frame the whole aisle leg.
2. The coach's front door does not answer E ("nothing in reach" at 141 cm / 122 cm) - follows from 1.
3. The forecourt kerb `SM_RoadCurbs_1` (plan x 75.1, y -54.6) blocks the way to the zebra crossing.
4. Pathing gives up in three places and sets the walker down at the point: jetway to its cab, back to the
   rotunda (a Babylon instanced mesh at world -5030.0, -9696.2, 100.3), the crew-side baggage hall past the belts.
5. The load hitch at the start: 37 bad frames, one frame 3.6 s of game thread.
6. No grass colour tiles (`Tools/make_grass_area.py`); the office laptop has no imported `Screen` part.
7. A black untextured box on the cargo apron and flat-black slabs under every dock door - a missing material
   in the cargo dock kit.

Then, in the order agreed before: coaches into bays A1-A12, the bus-station picture run, to-do 373 (town hub
passengers), the hangar chain (366, 364, 367, 368, step 7, then the hangar visual test), to-do 374.

## Testing issues
The seven above, none fixed. Nothing else was run.
