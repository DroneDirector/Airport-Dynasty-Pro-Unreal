# Performance log - part 1

Every performance change to the game, newest last. Only performance work goes in here.

## Waiting for your OK (visual changes for speed)

- **Glass on Nanite meshes** (to-do 67): 12 meshes; Nanite can't draw their see-through parts. Plan: draw them the
  ordinary way, so the glass shows as designed.

## Changes

- **2026-11-17 - The night lag found: the lamp flares.** After the 16 gates the game felt very laggy. Measured in the
  shipping build at a fixed hour (new stand test switches -StandDay/-StandHour): by day 87 fps, at night 34-38 fps; the
  extension itself costs nothing (old and new build both 82 fps by day; today's build with yesterday's data just as slow
  at night). The cause: the lamp flares (to-do 22c, 2026-11-16), meant to be off by default, were on whenever the
  setting was missing from the settings file, and each street light and mast has its own sprite that the processor
  prepares every frame. Fixed: off unless switched on in Settings; when on, only those within 600 m. Night, shipping,
  frame generation on: 36 -> 87 fps (flares off), 86 fps (flares on). Also: moving parts (doors, jetways, belts,
  crowd, escalators) no longer rebuild their whole lists every frame; the lamp panels are static. Ruled out by test:
  the lamps' lights, merged lights (slower), per-door lists (slower), the tower, the new data. MegaLights needs hardware
  ray tracing (not on) - kept for later, with graphics memory first: at Cinematic the game uses ~7 GB of the 8 GB card.
- **2026-11-16 - The game's own performance log.** The game now writes `Saved/Logs/ADP-Perf.log` (next to ADP.log):
  every frame over 50 ms that is also 2.5 times slower than the frames around it, with what took the time (the game's
  own work, the render thread, the graphics card, a memory clean-up, files still loading) and where the camera was
  and which way it looked. Every 30 s: average fps, the slowest 1 % and the worst frame. The run before is kept as
  ADP-Perf-prev.log.
- **2026-11-16 - Nothing loaded by name is missing from the package.** 0.17.16 left out the escalator, road signs,
  street lights, land along the roads, fire-truck materials, interact glow and stand wear (the code asked for them
  and found nothing). The release now stops if any such folder is missing.
- **2026-11-16 - The baked roads are Nanite.** 607 road meshes (up to 26,080 triangles each, all casting shadows)
  were ordinary meshes in the shadow map's slow non-Nanite queue; now Nanite, same triangles and materials (all
  M_Ground, opaque, made for Nanite). Checked by pictures at the terminal's forecourt: they look the same.
- **2026-11-16 - Shadows and lamps follow what you do** (your OK). On foot or driving: the sun's shadows reach 2.6 km
  (were 42 km always), lamps drawn up to 150 m (963 had no limit), the floodlights (reach over 50 m) 600 m. Flying,
  in the drone or the flight viewer, or over 300 m up: 42 km, 600 m, floodlights 2.4 km. Switched on the fly
  (ADPHUD.cpp, twice a second); a lamp's own shorter limit kept. Measured in the packaged game (stress walk,
  off-screen, Cinematic 1920x1080): 87 -> 92 fps on average, slowest 1 % 68 -> 75 fps; standing in the Grand Hall
  89 -> 92 fps. Night pictures (hall, forecourt, apron, gate walkway): nothing went dark that should be lit.
- **2026-11-16 - The stutter fixed: the seats.** Found by the new stall watch (the game thread stuck in
  ASeats::Tick -> ASeats::Gather -> ABabylonModel::SeatSpots): every 15 s the seat system walked every piece of every
  building to find chairs again, 112 ms each time - the 120 ms hitch every 15 s, on the title screen too. Now it
  gathers only when a building is added or removed. Measured in the packaged game (stress walk, off-screen): bad frames
  26 -> 0, worst frame 133 -> 50 ms; standing still: 5 hitches -> none.
- **2026-11-16 - Watchers kept for good.** The stall watch (-StallWatch, on for every stress walk; names places in a
  packaged build with Tools/stall_names.sh); time guards (ADP_PERF_SCOPE) on every frame-by-frame scan, writing
  "SLOW WORK" with the name when one takes longer than its budget; the release stops on a new scan without a guard
  (Tools/find_world_scans.py, Data/world-scans-checked.json).

## 2026-11-18 - our frame generation always in place, switched live (to-do 111)
- Change: our frame generation's display chain is put in at every start (unless AMD's frame generation was chosen at
  that start) and switched on/off live from Settings; while off, each frame goes straight through as drawn.
- Measured (editor -game, 1600x900, airport mode, same scene, Tools: -UIFGToggle): without our chain (-NoOwnFrameGen)
  65 fps drawn; with it in place and switched off 65 fps drawn. Switched on: 2 pictures per drawn frame (64 fps drawn).
- Result: no measurable cost while off. Not a visual change.
