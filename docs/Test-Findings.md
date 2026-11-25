# Airport Dynasty Pro - test findings

One continuous file of every test and what it found (newest last). Only measured facts; where something is not proven
yet it says so. The PDF of this file: docs/Test-Findings.pdf (Tools/make_findings_pdf.py).

## 2026-11-16 - First stress walks (editor build)

The stress walk ran in the editor build (Tools/perf_walk.sh editor). The editor's frame times are not the game's (it
draws more and checks more), so these numbers say where to look, not how fast the game is; the packaged game's own
walk follows below when it has run.

### Measured: an engine warning written about 7 times a frame (editor builds only)

- Unreal warned, nonstop: "Invalid material MI_T_glassOuter used on Nanite static mesh terminal_mesh_152. Only opaque
  or masked blend modes are currently supported." 69,033 such lines in about 7 minutes, each also copied into ADP.log
  and Problems.log by the game's problem log.
- The packaged game does not log engine warnings, so this costs nothing there; in editor test runs it does.
- Cause (file): see-through (translucent) materials on meshes drawn by Nanite, which can't draw them. 12 meshes:
    /Game/Terminal/Meshes/terminal_mesh_152 (105 copies, the bag-drop points' screens)
    terminal_mesh_827 and _834 (the gate walkway's glass roof and its gable)
    terminal_mesh_681, _663, _555 (stair balustrade glass), terminal_mesh_814 (the bar's back bar)
    terminal_mesh_2722 and _2730 (the car rental's glass and perforated walls), terminal_mesh_4, _179, _810, _931, _950
    /Game/Babylon/Firestation/firestation/StaticMeshes/firestation_mesh_96 (15 copies)
- Placed by: ABabylonModel (Source/AirportDynastyPro/Private/BabylonModel.cpp).
- Fix (WAITING FOR YOUR OK - it changes how that glass is drawn): draw those meshes the ordinary (non-Nanite) way,
  as Unreal's warning itself suggests; their glass would then show as designed.

### Measured: the world's contents (the scene audit at the start of the walk)

- 5,438 different meshes drawn. Nanite: 41.3 million triangles. Not Nanite: 1,201 meshes, 4.49 million triangles,
  at least 1,215 draw calls a view before shadows.
- 1,838 lights on; 15 cast shadows, all 15 movable (a shadow pass each, every frame): the Babylon models' point
  lights (BabylonModel.cpp).
- The largest non-Nanite meshes casting shadows are the globe's terrain tiles (Cesium3DTileset, up to 12,751 km
  across): these fill the virtual shadow map's "non-Nanite" queue - the engine says so on screen: "[VSM] Non-Nanite
  Marking Job Queue overflow. Performance may be affected."
- The roads: 607 baked road meshes (/Game/Roads/Baked, made by RoadNetwork.cpp's bake), not Nanite, up to 26,080
  triangles each, all casting shadows.
- The control tower (TowerADC.cpp): 8,713 instanced copies of Unreal's own cube and cylinder (instanced already, so
  few draw calls), not Nanite, casting shadows.

### Measured: a slow frame every 15 seconds (editor build)

- About 130 ms, exactly every 15 s, wherever the camera was. Not yet traced: the timetable screens' 15 s timer only
  redrew once in the run, so it is not them. To be checked in the packaged game.

### What is being done now (no change to the look)

- The baked roads become Nanite (same triangles, drawn the Nanite way: cheaper shadows).
- Unreal's cube, cylinder and sphere used by code-built things (the tower, hangars, fittings) get Nanite copies,
  except where a see-through material is on them.

### Waiting for your OK (they change the look)

- The globe's terrain tiles casting shadows: they cast shadows for thousands of km; turning their shadows off (or only
  far away) would cut the overflow, but distant mountains would stop shadowing themselves.
- The 1,838 lights: fading the small ones out beyond a distance (they are not seen far away) would cost less every
  frame; up close nothing changes, far away the faint glow of distant lamps would go.
- The glass on Nanite meshes (above).

## 2026-11-16 - The packaged game (0.17.17, your settings: Cinematic, 1920 x 1080)

### Measured: a stress walk while Windows was locked is not valid

- Run while the PC was locked: 5-7 fps all through. Windows holds back a locked screen's frames; the title screen in
  the same run did 254 fps. Such runs are thrown away; locked runs are now drawn off-screen
  (bash Tools/perf_walk.sh offscreen), which measures the game's own work.

### Measured: the stress walk drawn off-screen

- The whole walk (outside, Grand Hall, stairs, escalator, Floor 2, gate walkway, gate 1, jetway, apron):
  87 fps on average, the slowest 1 % of frames 68 fps, worst frame 133 ms, 26 bad frames in about 6 minutes.
- Every 30 s: 79 to 102 fps on average, the slowest 1 % 67 to 81 fps.
- Almost all bad frames are one hitch: 115 to 133 ms, exactly every 15.0 seconds, wherever you are and whatever you
  look at. That is the stutter you can feel.

### Measured: what the 15-second hitch is NOT (standing still 75 s in the Grand Hall, one thing off at a time)

- In each hitch the game's own work was 3 ms, the render thread 3 ms, the graphics card 11 ms: the frame is spent
  WAITING, not working. (The log said "graphics card"; that wording is wrong for this case and is being fixed.)
    with everything on:             89 fps, hitches every 15 s (152 ms)
    virtual shadow maps off:        89 fps, hitches every 15 s (140 ms)
    Lumen lighting off:             89 fps, hitches every 15 s (120 ms)
    volumetric clouds off:          89 fps, hitches every 15 s (126 ms)
    sound off:                      90 fps, hitches every 15 s (127 ms)
    texture streaming off:          89 fps, hitches every 15 s (120 ms)
    grass off:                      90 fps, hitches every 15 s (136 ms)
- Not the timetable screens (their 15 s timer redrew only once in the run). Not the sun clock (5 s and 120 s steps).
- Still to test: the menus' built-in browser, files written to disk, the holographic map.

## 2026-11-16 - The 15-second hitch: found and fixed

- More eliminations first: the menus' browser removed - still there; the title screen with no airport loaded - still
  there (every 15 s, 124 ms); no graphics device at all (-nullrhi) - still there. A plain program on the same PC had no
  stalls, so it was inside the game, not the graphics card, a driver or an overlay (not Razer's FPSRunner either).
- Found by a stall watch built for it (a thread that writes where the game thread is when it stops for 60 ms):
    ASeats::Tick -> ASeats::Gather -> ABabylonModel::SeatSpots (Seats.cpp, BabylonModel.cpp)
  The seat system re-collected every chair in every building every 15 s (first 3 times every 6 s - matching the
  first hitches' 6 s spacing), 112 ms each time.
- Fixed: gathered again only when a building is added or removed. Code only; nothing looks different.
- Measured after (packaged game, stress walk off-screen, Cinematic 1920x1080): 92 fps on average, slowest 1 %
  75 fps, worst frame 50 ms, 0 bad frames (before: 87 fps, 68 fps, 133 ms, 26 bad frames).

## 2026-11-16 - Changes made for speed (no change to the look up close)

- Sun shadows 2.6 km on foot or driving, 42 km flying/drone/flight viewer (your OK); lamps 150 m on foot (floodlights
  600 m), 600 m flying (floodlights 2.4 km) (your OK): 87 -> 92 fps average, slowest 1 % 68 -> 75 fps.
- The 607 baked road meshes drawn by Nanite (same triangles, cheaper shadows): in the next full build.

## Small issues, 25 Nov 2026 (compiled, not yet seen in the game)

- 278 shadow marking queue: large far terrain tiles and roads beyond 1.2 km cast no shadow; the engine's queue size is fixed.
- 279 creator on 32:9: the page stays in the middle 16:9, the camera keeps the height.
- 280 stuck at a gate: arriving passengers now go straight through the middle of the jetway's gate door. The rotunda itself measured 2.05 m free or more.
- 281 a weather chosen by a test stays until LIVE is pressed.
- 283 horn T / hold B + D-pad up; high beams L / hold B + left shoulder.
- 284 the tug's door sounds come from PushbackTug and are packed.
- Cargo stands: -TrafficTest and normal traffic put only cargo aircraft there.
- Push towards X1 ends facing the taxi way; STAND-9 turns tighter, its gear stops 3 m before the taxilane end (Tools/check_push.py: 0 findings).
- Passengers stuck on the way to a gate chair (plan -217.8, 61.3): they turned in 30 cm before the end of a moving walkway rail, on its wrong side. They now go on to the corner first while the way from where they are is shut (PaxFlow.cpp). Compiled; not yet seen in a game run.
