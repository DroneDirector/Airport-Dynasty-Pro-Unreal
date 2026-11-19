# Stop note - 2026-11-19 evening

**Done**
- 0.17.28 published (strollers, sitting IK). Coach modelled (exterior, interior, bench, dashboard, wheels), 7 coaches parked
  in the 7 repainted forecourt stands (BUSES ONLY, clear of the crossings), 131 aspens on the grass island (Nanite leaves).
- To-do 142 done: shader cache recorded again with the forecourt, day + night (6608 pipelines, committed).
- To-dos 141-148 on the list (147 = NUMBER 2: crossings / night road textures; 148: coaches through the ground, later).

**Half-done (to-do 144, not compiled, not committed)**
- Source/AirportDynastyPro/Public/Coaches.h + Private/Coaches.cpp: doors (E/X, swing out, kneel + hiss), luggage bays,
  lamps (M_CoachLamp), beams (real light only on the nearest coach), mirrors (captures, nearest coach), drive hum,
  airport name on the rear plate, hip-wide capsule inside.
- Hooks: BabylonModel.cpp SeatSpots (coach seats, never the driver's), RoadNetwork.cpp (coaches handed to ACoaches),
  ADPHUD.cpp (E/X on coach doors/bays; -UICoachUse test).
- Tools/coach_setup_unreal.py (collision by shape, sounds, the two materials) written, NOT run yet.

**Next (when he says)**
1. Run Tools/coach_setup_unreal.py (UnrealEditor-Cmd), compile, run -UICoachUse, fix, send pictures, then pause.
2. Then to-do 146: the coaches' own stress test.
