# Performance log - part 1

Every performance change to the game, newest last. Only performance work goes in here.

## Waiting for your OK (visual changes for speed)

(none yet)

## Changes

- **2026-11-16 - The game's own performance log.** The game now writes `Saved/Logs/ADP-Perf.log` (next to ADP.log):
  every frame over 50 ms that is also 2.5 times slower than the frames around it, with what took the time (the game's
  own work, the render thread, the graphics card, a memory clean-up, files still loading) and where the camera was
  and which way it looked. Every 30 s: average fps, the slowest 1 % and the worst frame. The run before is kept as
  ADP-Perf-prev.log.
- **2026-11-16 - Nothing loaded by name is missing from the package.** 0.17.16 left out the escalator, road signs,
  street lights, land along the roads, fire-truck materials, interact glow and stand wear (the code asked for them
  and found nothing). The release now stops if any such folder is missing.
