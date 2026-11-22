# Stop note - Saturday 21 November 2026, evening

## Done
- **0.18.1 built and published.** It is installed on your F: copy (F:\Airport Dynasty Pro Unreal; the exe says 0.18.1, the world terrain is kept) and uploaded to GitHub.
- **In 0.18.1:**
  - passengers travel car park → check-in → security → escalators → gate → jetway fade, and arrivals → claim belt → car park;
  - bags go all the way; tractors, carts and the belt loader work, and dirt and washing;
  - the aircraft waits for its passengers; ATC clearance comes before the tug comes out;
  - doors stay open; revolving doors can be ridden;
  - the walking map, suitcase grip, sideways steps, own pace and running, idle movements with IK, phone and tablet at the gate;
  - the terminal changes (stair, corridor, wood floors, family room);
  - the GPU cable floor; lit quality buttons; High at 87% for new installs; the Test (no limits) difficulty.
- **Release fixes:** the release script now traces the holomap on Chablais; the vehicle folders are always cooked.

## Half-done / not checked
- **Not tested:** the last fixes were not seen in a cinematic test (on your word): the test passengers linked to the gate's aircraft (to-do 207), the revolving doors, the idle movements, the phone/tablet, the faster walking map. The automatic start check passed (no crash).

## Next (Monday 23 November)
1. The cinematic test (`bash Tools/cinematic_test.sh`): check boarding at the gate's aircraft, the jetway fade, then ATC, then the tug; the walking map's ready time; the revolving doors; the idle movements; the phone/tablet. Fix what it shows.
2. **To-do 205, NUMBER 1:** every passenger walking, through layers of detail (the Matrix Awakens approach).
3. To-do 206 (NUMBER 3): hired staff in 3D. **Do not build until you say.**

## Sunday 22 November, morning - the cargo terminal started
- **Done:** the 5 blueprint sheets redrawn (docs/Blueprints/Cargo) and 23 reference photos of real cargo terminals.
- **Half-done:** three builders run until the weekly limit stops them:
  - the model, real materials and the import;
  - placement: pad, apron, truck court, moved fence, access road, holomap;
  - doors, lights, signs, equipment and sounds.
  What they commit is kept; uncommitted edits stay in the working tree.
- **Next (Monday):**
  1. the cinematic test and its fixes;
  2. finish the cargo terminal, then one look and publish;
  3. to-do 205.
