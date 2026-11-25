# Performance log

## 2026-11-25 - packaged run with the passengers in (to-do 269, 285)
Packaged Shipping, 3840x1080, the owner's settings, own frame generation on, all 144 people in the package; career test save 3;
91 of 91 walking legs right, 0 problems, loading 22.3 s. Milliseconds made / GPU / shown a second:
- at the start 15.6 / 14.8 / 128; they come (10x) 17.2 / 16.1 / 116; beside walkers 18.1 / 16.9 / 111, 17.0 / 15.0 / 118, 19.6 / 17.8 / 102;
  hall 16.4 / 15.5 / 122 and 20.1 / 19.2 / 99; more come (10x) 18.8 / 17.2 / 105 (worst 216); outside the doors 17.9 / 14.8 / 112;
  road west 16.9 / 15.6 / 118, east 18.4 / 17.5 / 109; a minute among 48 people 21.0 / 20.0 / 95; apron towards the hall 20.5 / 19.8 / 97.
- 24 to 53 people about, up to 24 walking. The graphics card is the limit and it rises with the people (205). 1 % slowest 18-36 ms.
- Graphics memory 5506 -> 6597 MB of 6951; the texture pool went 2000 -> 1000 MB, 1600 wanted.
- Left: 50-66 ms of game thread once a minute (wall clock :15, not the clean-up); 812 ms and 216 ms while people load or come at 10x;
  50-60 ms at camera cuts; the passengers' trips 2.1 ms a frame.
- 248: the roads' textures are fully resident, only the kerb's normal and roughness stay at 64 of 4096.
- 248, the cause: the textures are there, but the roads' scan (asphalt_pit_lane, 3 m) averages out to one flat colour and one flat sheen
  (brightness contrast 4 % over 10 cm patches, 1 % over 40 cm, the airfield's 11 % and 9 %), its big layer was the same scan, and the paint had no scan at all.
  Now M_Ground FarLook (patches of 40 cm to 20 m in colour and sheen; groad, gwalk, gkerb) and PaintRelief (the pavement's relief and wear
  through the paint; MI_roadwhite, MI_roadyellow, the airfield's paints); car parks and fills no longer get a lane's wear by their UVs;
  the kerbs' pictures stay in memory; the log says "Roads: <road> part n" with each surface's material, pictures and settings.

## 2026-11-21 late - the walkers' navmesh (PaxNav.cpp)
- Unreal's navigation (Recast, dynamic, agent 30 cm / 180 cm, 10 m tiles) is generated once in the background after the
  passenger flow is ready. It covers only the finished terminal rooms on both floors (plus 0.6 m over their walls, so
  the doorways join) and the landside car park and forecourt box. Unfinished rooms ("empty") are left out, and so is
  the rest of the world. Logged as "PaxFlow: navmesh ready (N tiles, X ms)".
- No rebuilds while the game runs: geometry is gathered lazily, transform changes don't update the nav octree
  (bUpdateNavOctreeOnComponentChange off), and people, pets, bags, belts, vehicles, jetways, doors, lifts, seats,
  grass and trees can't affect navigation. A sweep every 5 s catches anything spawned since.
- Per walker: each long leg (> 4 m) of a walk is found once on the navmesh, at most 4 new legs a frame across all
  walkers, and its corners are kept in the way. There is one more knee-height line trace per moved walker as the wall
  safety net. Far walkers are still moved every fourth frame. To be measured in the combined test.

## 2026-11-21 late - passengers' suitcase grip (PaxAnimInstance.cpp)
- Passengers now use their own anim instance (UPaxAnimInstance, still the single-node walk blend space). While a bag is
  carried it adds a layer after the blend space: 15 right finger/thumb joints closed round the handle, and the right
  upper arm and forearm held a little out (two short parent-chain walks). That is about 20 bone rotations per evaluated
  pose, worker thread, and none when no bag is carried. Update-rate optimisation and OnlyTickPoseWhenRendered stay on,
  and far walkers are still moved every fourth frame.
- The bag holder is attached to hand_r (no GetBoneLocation reads each frame). The two belt actors tick in
  TG_PostUpdateWork, so the bags read this frame's hand, with no extra work.

## 2026-11-21 evening - the passengers' trips (PaxFlow.cpp)
- Of each flight up to 36 departing / 30 arriving passengers walk as MetaHumans (the rest stay data in the simulation -
  never a slider, the schedule decides). Their bodies use update-rate optimisation and only tick their animation while
  seen (no change up close); those more than 60 m from the camera are moved every fourth frame (same speed). One floor
  trace per moved walker. ADP_PERF_SCOPE "pax flow" (2 ms) watches the whole manager. To be measured in the combined test.

## 2026-11-21 night - to-do 195: clouds (first step)
- The sky light's real-time capture is time-sliced (r.SkyLight.RealTimeReflectionCapture.TimeSlice 1, ADPGameMode.cpp):
  it redrew the sky and the clouds into the sky lighting every frame; now spread over several frames. No visible change
  (the sky changes slowly). Not yet measured at overcast/scattered (ProfileGPU) - the measured comparison is the next step.
- Trees (to-do 200): 1.81 million real trees, drawn only within 4 km of you in 1 km blocks (two blocks a step, no hitch),
  instanced and Nanite, far shadows off (the 10 cm photo carries the forests' look beyond); to be measured in the
  stress walk with frame generation on (Full HD, 4K, his screen; day and night).

## 2026-11-21 night - idle body movements (TugCrew.cpp, PaxAnimInstance.cpp, People.cpp)
- Ground crew: a movement is posed with the crew's existing IK rig (FTruckDriverRig) only while it plays (2-6 s, then
  4-20 s of the plain blend-space idle between) - the same cost as the old head-scratch fidgets; new per-pose work is a
  few extra bone turns (spine, neck, clavicles, wrist) and the body landmarks for the hand contacts.
- Passengers: the layer runs in the anim proxy (worker thread) only while a movement plays - two-bone IK per arm (and
  per leg for the weight shift), a component-space rebuild per set bone. Started only for walkers within 60 m of the
  camera (the existing near/far split; far ones get none) and only after 30 s standing still; bodies already tick their
  pose only while seen. Seated (People.cpp): the seat pose is re-posed each frame only while a seated movement plays,
  within 60 m. To be measured in the combined stress walk (crowds at the desks / gates).

## 2026-11-23: the terminal's east side, part 1 (the shell)

- Added: 17 models (Nanite, columns and entrances instanced; the glass not Nanite, in five pieces), 326 lamps (163 real
  lights, no shadows, 9 m reach, the terminal's lamp system), no flares. Editor game, 1920x1080, Chablais, 13:00, one frame
  each (stat unit), before -> after: forecourt x 350: frame 20.7 -> 20.3 ms, GPU 11.7 -> 11.8, draw calls 1287 -> 1421;
  Grand Hall: frame 17.4 -> 20.2, draw thread 17.3 -> 20.2, GPU 14.0 -> 14.2, draw calls 1753 -> 1819; kerb by the Grand
  Hall: frame 20.2 -> 21.7, GPU 14.7 -> 15.1, draw calls 1429 -> 1515. The draw thread pays, most likely for the lights.
  Not a stress walk: to be measured in a packaged build before the release.

## 2026-11-23: the gates' curved windows (10 gates)

- Added: 7 models (Nanite but the glass; each drawn 4, 6 or 10 times as one list), 240 seats in the seats' own lists, 60 lamps
  (30 real lights, the terminal's lamp system), no flares. Gone: 20 flat panes, 200 mullions, 4 dead doors. Each jetway
  corridor's first LED light now shines down only (a spot: it lit the bay's ceiling through the corridor's roof).
- Editor game, 1920x1080, Chablais, 13:00, 4 s averages (FPS chart) at fixed views, before -> after: inside gate 9 GPU
  14.8 -> 14.9 ms, render thread 10.3 -> 10.0 (the game thread 49 -> 45, the world still loading); gate 9 from the apron GPU
  11.7 -> 11.7, render thread 18.7 -> 19.5; inside gate 13 GPU 10.5 -> 10.8, render thread 16.4 -> 15.6; gates 2-4 from the
  apron GPU 14.1 -> 14.1, render thread 21.5 -> 21.6. No difference beyond the noise. Not a stress walk, not packaged.

## 2026-11-23: the Grand Hall's frame, measured (no change made)

- Editor game, 1920x1080, Chablais, midsummer 13:00, standing in the Grand Hall (plan 75, 12) looking north, 75 s:
  64 fps on average (15.6 ms), slowest 1 % 52 fps; frame generation off. (-UIPerfStand needs -UIShot with it since the
  title film: without it the game waits at the title and measures nothing.)
- One frame's average over 60: game thread's work 6.6 ms (then it waits for the draw thread), draw thread 15.6 ms. In the
  draw thread: the scene 12.6 ms, of it the 305 lamps one by one 2.3 ms, finding what is visible 2.5 ms (2.1 of them
  waiting for the workers), updating moved things 1.3 ms. So lamps are the largest single named part, not the meshes.
- Tried: each revolving door's turning wings in lists of their own (they share lists as long as the terminal): 64 fps before
  and after, every part the same to 0.1 ms - taken out again.
- Not measured: the same view without the east side's 163 lights, so how much of the 17.4 -> 20.2 ms (one frame each, another
  camera) is theirs is still open. Not stress-walked.

## 2026-11-23: the light under the wave roof, the paving to the doors (SoffitLights.cpp, east_pave)

- 34 long soft panels (no shadows, no flares, no haze, nothing on the glass, 400 m limit; the 16 inside off by day), one per
  40 m instead of a light per downlight; the east shell's solid in 36 Nanite pieces; 24 more lamp panels, 12 more lamp lights.
- Editor game, 1920x1080, 100 %, Chablais, one settled frame each (stat unit), the panels off -> on (same run, same place):
  13:00: forecourt x 350 frame 15.7 -> 15.7 ms, GPU 11.3 -> 11.4, draws 1413 -> 1419; Grand Hall 15.6 -> 15.6, GPU 13.4 -> 13.7,
  1774 -> 1777; kerb 17.0 -> 16.4, GPU 14.4 -> 14.5, 1479 -> 1501. 23:00: forecourt 14.4 -> 14.7, GPU 10.3 -> 10.8; Grand Hall
  15.0 -> 15.2, GPU 12.4 -> 13.1; kerb 15.5 -> 16.2, GPU 13.1 -> 13.3. Against the morning's numbers of the same three places
  (20.3 / 20.2 / 21.7 ms) the frame is shorter: the day's lamp and walking-map work is in it. Not a stress walk, not packaged.
- Found: the sky light gives NOTHING in the whole game - its real-time capture stays black from the start (x5 or off: no change;
  r.SkyLight.RealTimeReflectionCapture 0, or 0 then 1, and it lights). Not switched on: it changes every shade, interior and
  pane of glass in the game (blue shade, milky blue glass) - the owner's decision.

## 2026-11-25: the slow outdoor frames found: graphics memory overflowed (269; SiteTrees, GraphicsMemory, DefaultEngine.ini)

- Cause, and why it did not use to: the 525,000 trees out to 4 km (2026-11-21) all sat in the distance-field light scene
  (615,331 objects, 160 MB of its tables alone), on top of two engine pools reserved almost empty: virtual textures 318 MB
  for 0.3 MB used, Nanite streaming 512 MB for 83 MB used. Textures then grow in play (1036 -> 1590 MB in 5 min) until the
  8 GB card is over its budget; from there every frame swaps over the bus: 100-200 ms, and frame generation adds nothing.
- Changed (nothing visible): trees are in the light scene only within 300 m (out at 600; 615,331 -> 77,910 objects, tables
  160 -> 20 MB), the rim's far tufts never; virtual-texture pools 8 MB each, Nanite streaming 160 MB; a guard lowers the
  texture store in 2 s steps when the card has under 450 MB left and gives it back when there is room (logged).
- Packaged code, career, 3840x1080, own frame generation, memory taken by another buffer to reach the limit
  (before -> after code -> after code + config): game's memory 5830 -> 5680 -> 5185 MB. +1024 MB: 21/26 -> 13.7/18.2 ms;
  +1536: 199/104 (no generated frames) -> 21.1/24.5 ms (95/82 shown/s); +2048: 195/115 -> 16.5/22.2 ms (121/90 shown/s);
  +2560 still over (179/119). About 1 GB more room plus the guard.
- Not proven yet: a long session in the owner's hands, and the same test on the packaged release.
