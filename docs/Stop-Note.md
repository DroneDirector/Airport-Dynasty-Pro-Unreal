# Stop note - 2026-11-19 night

**Done**
- To-do 144 (coach working parts) built, tested, committed: doors + bays on E/X with kneel + hiss, walk in, every
  passenger seat sittable, lamps, 2 real beams (0.8 ms), mirrors (1.6 ms), sounds, airport name plate.
  Fixed on the way: first seat pair standing outside the coach (Tools/glb_nodes.py mesh names first), info screens hung
  to 1.56 m (now slim bars, 1.81 m clear), crash on opening a door (sounds freed by the GC).
- To-do 146 route built and working: -UICoachWalk / `bash Tools/perf_walk.sh coaches [offscreen|4k|screen]`, test package
  in F:/ADP-Packaged/test-coaches (no version bump, nothing published). Readings (packaged, drawn by the game):
  | run | drawn | slowest 1 % | bad frames |
  |---|---|---|---|
  | Full HD 1920x1080 (forced) | 50 fps | 30 fps | 5 |
  | 4K 3840x2160 (forced) | 3 fps | 2 fps | 1089 - graphics card overflow, the whole game, not the coaches |
  | his screen 3840x1080 full screen | 13 fps | 7 fps | 1 - frame generation added 0 pictures: SUSPECT |
  Day and night, inside the coach and outside all the same in each run.
- To-do 100-107, 110, 112, 142 ticked done on the list (already released / done).

**Half-done**
- To-do 139 updater fix written in Source/.../UI/Updater.cpp (log kept, waits for the game folder to be free, copy
  retries, every file checked, version set in Windows) - script tested on a fake folder, NOT compiled, NOT committed.
- His-screen run result looks wrong (13 fps, frame generation 0): find why before trusting it.

**Next**
1. Check his GameUserSettings (resolution scale, frame generation) after tonight's runs; rerun `coaches screen`.
2. Compile + commit 139; mark 139 / 144 / 146 on the list.
3. To-do 70 (A320 fin lettering): render both fin sides to see which reads mirrored.
