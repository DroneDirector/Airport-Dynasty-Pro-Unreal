# Performance log

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
