# Performance log

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
