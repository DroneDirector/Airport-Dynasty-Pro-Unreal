# Hired crew - your staff are the people in the 3D airport (to-do 206)

Part 1 (2026-11-23): the people and the pool - `StaffPeople.h/.cpp`; `adp.CrewList` dumps everyone. Part 2a: tug, pushback,
ground power. Part 2b: bags, loader, claim safety; the switch `adp.HiredCrew` is now 1 by default (0 = the stand-ins for
everything). All compiled, NOTHING run yet. Part 3: the hiring UI, built 2026-11-23 (compiled, type-checked, not run).

## Part 1 as built
- **Who:** the simulation's people (`FADPPerson`) of your airline, role `ground`, base = this airport, status `on`, no days
  off. Shift: `FADPPerson.Shift` (never set before) - early 06-14, late 14-22, night 22-06 given 2 : 2 : 1; the sim's head
  count (`ADPSchedule::OnShift`, 40% / 20% at night, at least 1) is topped up with the people due next ("called in").
- **Face:** new saved field `FADPPerson.Face`: a built ground MetaHuman of their sex (8 men, 8 women), first tried by the id,
  nobody else's while one is left; then the fire people (7 + 7, not used elsewhere); then shared, logged once. Needed in the
  save: without it a colliding person's face could change when somebody else leaves.
- **Where:** they come out of the 4 ramp rooms that have a staff door onto the apron (`D-F1-RAMP-1..4-2`), made / removed in
  the room behind the shut door when you are not in it; one direction at a time through the door; the door opens by badge
  (`ABabylonModel::StaffDoor`). Standing by in a row along the terminal, plan z 84.2, from 14 m west of the stand line.
- **Pool:** `FindFree(Duty, Near)`, `Claim(Id, Job)` (gives the actor; the caller moves it), `Release(Id)` (only from the
  apron, on their feet), `Missing(Task)`, `Needs(Task)`, `NumFree`, `IdOf(Actor)`.
- With the switch on before part 2 the stand-ins are still there too (the same 16 faces twice): only for testing part 1.
- No people limit exists in the game (real counts): every person on shift is there.

## The sim's roles and the 3D duties
`ground` (shown as "Baggage handlers" / ground crew) is one generic ramp role: it covers ALL of tug driver, pushback handler,
wing walker, GPU handler, tractor driver, baggage handler, belt loader operator. `fuel`, `catering`, `cleaning` have no 3D
job yet; `checkin`, `security`, `mech`, `captain`, `fo`, `fa`, `fire` are other plans (210, 211, 202). `gate`, `bus`,
`tower`, `ops` have a wage but nobody is made for them in the hiring pool (`ADPStaff::FillPool`).

| Task | Full team | Minimum to start |
|---|---|---|
| Ground power on / off | 1 GPU handler | 1 |
| Bags out (train, make-up) | tractor driver + 1 handler | driver |
| Bags in | tractor driver + 1 handler | driver |
| Belt loader | operator + 1 up at the hold | operator |
| Pushback | tug driver + handler + 3 wing / tail walkers | driver + handler |

One A320 turn-round at once: 4 at the bags and loader, 1 at the GPU, then 5 for the push: 5-6 people; 2 can do it one job
after another (the rule today in `ADPSchedule::MissingStaff`: 2 for the ground power, the bags and the pushback).

## Part 2a as built (2026-11-23, compiled, not run): the pushback and the ground power
`TugStaff.cpp` (new), the queue in `StaffPeople.cpp`, one-line hooks in `Tugs.cpp`, `TugCrew.cpp`,
`Turnaround.cpp`, `SimTraffic.cpp`.
- **Whose flight:** only the player's airline's (`ATugs::HiredJob`, by the planned leg / flight id). Other airlines keep the
  stand-ins; with the switch on at start these take faces none of your staff wear (then the fire people's), same number; your
  own new faces avoid the stand-ins'. Shared faces only when both lists run out.
- **Queue:** `AskTeam(key, task, departure, ...)` - the whole smallest team or nobody, the earliest departure first (a job
  that needs more people than are on shift holds nobody up); `AskExtra` (wing / tail walkers) only takes who no waiting job
  needs. Someone walking back from a job can be given the next one at once.
- **Tug:** `FTug.bHired`: while it waits the tug stays Parked with its `Key`, `Driver` / `Handler` nobody - never by itself
  (`HiredTick` asks every 2 s). Driver + handler come together; `FCrew` slots are reused, `Crew` is reserved at 512 and never moves.
- **Late:** `ASimTraffic::Ready` asks `ATugs::CrewReady`: no crew aboard = the flight isn't ready; its `Why`, `Hold` and `Est`
  move (boards, Overview), the log and a notice by `ADPSchedule::Tell` after a minute; the Overview's missing-staff line has
  the waiting jobs too (`ADPSchedule::Waiting3D`). A flight called off: the job is dropped, or the crew stands down.
- **Ground power:** `ArrivedHired`; nobody free = the `gpu` task waits; less than 12 min to off-blocks = left out.
- **Ways:** far walks go in to the terminal's front, along it at plan z 85.45 (under the jetways' fixed corridors, columns at
  z 88) and out at the gate (`SafeWay`, `AlongWall`); hired people stop for people and the player and step round them.
- **Decided here, to confirm with the owner:** the push goes with fewer than three walkers (the minimum is driver + handler,
  logged); your driver does not sit in the tug from 3 min before the aircraft arrives (it would bind a person through the
  whole turn-round) but comes 8 min before off-blocks.
- Not checked: the wall lane against real columns and the curved gates, people crossing the train's way at Ramp services 13.

## Part 2b as built (2026-11-23, compiled, not run): the bags, the belt loader, claim safety, on by default
`GSEStaff.cpp` (new), hooks in `GSE*.cpp`, `Turnaround.cpp`, `SimTraffic.cpp`, `TugStaff.cpp`, `StaffPeople.cpp`.
- **Whose:** your own flights (`AGSE::HiredFlight`, the turnaround's `bMine`); other airlines keep the stand-ins. A helper is
  hired when their vehicle's driver is, never a mix.
- **Queue:** `Wants` (arriving bags, departing bags, a loader alone) asked every 2 s through `AskTeam`, a minute ahead of the
  same flight's pushback; `EStaffTask::Bags` = driver + operator together when no loader is at the stand (no driver ever waits
  for an operator another driver waits for). Helpers and the hold loader by `AskExtra`: nobody = goes on without. A job with no
  tractor, carts or loader free claims nobody and waits. The train waits in the lane for its loader; the operator stays while
  one of your trains is for his stand.
- **Late for real:** `SimTraffic::Ready` asks `AGSE::HiredWaits` ("its bags wait: 1 belt loader operator (...)"); `BusyAt` holds
  the bulk task and the push; the Overview's line comes from `AskTeam`. A late crew still loads the first train (10 min at most).
- **Ways:** the wall lane to x 272.5, in by Ramp services 13 at x 272.5 / z 59.5, out at x 271.6 / z 58.6 (nobody head-on), or in to
  the aircraft outside its wingtip; `Release` only outdoors.
- **Claim safety:** a claimed person at no job for 8 s is released (`CheckClaims`). A body gone at work (`ATugs::LostTick`, 2a's
  jobs too): the vehicle stands, the next free person takes over the same acts (first in the queue, climbs in by the door);
  a helper or the hold loader is let go instead. All in the log by name.
- **New game:** fewer than 2 ground crew employed at this base = the handling agent's stand-ins work your flights, all of
  them, logged (`UStaffPeople::Hires`); this also replaces "no ground crew, no bags" for a fresh career until you hire.
- **Small team:** a waiting job that needs more than are on duty calls the next ones in (2 employed = both come).
- **Moving a gate GPU by tractor** (`StartGpu`) is only done by `-GseTest`: no real job to give to anybody.
- **To confirm with the owner:** the threshold of 2 and the agent for free; calling in off-shift people without extra pay; your
  trains start when their aircraft is at its stand (bags before that wait on the carousel).
- **Not checked:** everything in the game; the ways on foot past real wing, loader and train places; a tug pushing when its
  driver vanishes finishes that movement; a sidestep inside the hall is the crew's old one (no wall test).

## Chocks and cones as built (to-do 212, 2026-11-23, not run)
`TugChocks.cpp` (new), hooks in `Tugs.*`, `TugCrew.cpp` (walkers go out after the chocks; the rack is walked round), `TugStaff.cpp`,
`StaffPeople.*` (`EStaffTask::Chocks`, 1 person), `Turnaround.cpp`, `Airfield.cpp` (the rack's box). Models: `Art/ApronSafety`,
blueprint `docs/Blueprints/Apron/ApronSafety.png`.
- **On:** the `chocks` task asks (`ATugs::ChocksOn`): your flight - `AskTeam`, 2 min ahead of its other jobs; others - a free walker,
  else the gate's tug crew. Nose, right main (left too from 2 h on the ground), then five cones in one round. Aircraft standing here at load: in.
- **Vehicles:** `bulk`, `fwd`, `aft`, `water`, `toilet` wait for the chocks, 4 min over their own time at most. GPU and jetway do not wait.
- **Off:** the bags' vehicles gone and the pushback crew there: a wing walker (none: the handler) brings the cones in; hooked on:
  the mains, then the nose chocks; then the walkers go out. `ATugs::Push` is held by `ChkOut`.
- A worker lost: what they carried lies there, the next one goes on.
- **Not checked:** everything in the game - the hands on the rope, cones in the hands while walking (no carry animation: the items hang
  from the hand bones), the way under the fuselage behind the nose gear and beside the jetway's drive column, how late the push gets.

## Cabin cleaning van, part A as built (to-do 212, 2026-11-23): model, kit, blueprint, parking place - no code yet
Models `Art/CleaningVan` (make_cleaningvan.py) and `Art/CleaningKit` (make_cleaningkit.py), blueprint `docs/Blueprints/Apron/CleaningVan.png/.pdf`
(Tools/make_cleaningvan_blueprint.py). Own L3H2 electric crew van 5.998 x 2.050 x 2.522 m, wheelbase 4.035, tyre radius 37.2 cm, axles x +205.1 / -198.4 cm.
**What part B (driving, crew, IK) must know** - every point below is in `Art/CleaningVan/cleaningvan-parts.json` (`points_cm`, model root, Unreal cm, y = right):
- **Place:** `ABabylonModel::Place("CleaningVan")`, kit `Place("CleaningKit")` (meshes BagFull, BagEmpty, BagRoll, Caddy, Vacuum, CleanCart; in the kit file they
  stand 2 m apart along x, each with its own origin: where the hand holds it; `Art/CleaningKit/sizes.json` has sizes and second-hand points).
- **Parts as GSE.cpp names them:** `Tyre_/Rim_/WheelDisc_/Hub_/HubCap_/Nut_` + FL FR RL RR (prefix), `Spindle_/SteerArm_` FL FR (kingpins `PIV_Steer_F*`), the wheels
  turn about `PIV_Wheel_*`; `SteeringWheel` (prefix; `PIV_SteeringWheel`, axis (-0.7071, 0, 0.7071) as the belt loader); lenses end `_Lens`: LowBeam_ HighBeam_ IndicatorF_ IndicatorR_
  TailBrake_ Reverse_ SideMarker_ with `_L_`/`_R_`, TailBrake_C_Lens, `WorkLamp_Lens`, `BeaconLens`; SideMarker_* have no side in the middle of the name but L0../R0...
- **Doors (bind by prefix):** `DoorDriver` (left; hinge `PIV_DoorDriver`, opens 68 deg, checked against the body), `DoorPassenger` (68, mirrored), `SlideDoor` (right:
  first 12.5 cm out, 4 cm back with it, then straight back to 130 cm - checked at 4 / 60 / 130; arm and roller run in the rail at z 123.5), `RearDoorL/R` (right leaf first, 100 deg).
  Handles `IK_Handle_*`, steps `IK_Step_*`, where to stand `IK_Stand_*`, grab handles `IK_Grab_L/R`.
- **Seats:** `IK_Seat_SeatDriver`, `IK_Seat_SeatPassenger`, `IK_Seat_SeatRow0..2` (left, middle, right), buckles `IK_Buckle_*`, belt tongues `IK_BeltTongue_*`,
  wheel hands `IK_Hand_Wheel_L/R`, `IK_Foot_Accel/Brake/Rest`, selector `IK_GearSelect`, aisle in front of the row `IK_Aisle_Row*`.
- **Kit in the van:** `Stock_Caddy_0..2`, `Stock_BagRoll_0..5`, `Stock_BagFull_0..1` (cage; show them only after work), `Stock_CleanCart_0`, `Stock_Vacuum_0..1` (dock on the bulkhead): hide the one
  somebody takes and hold the kit's mesh; places in `stock` of the json. The cart has no ramp: two people lift it out over the rear step (`IK_Cart_Out`).
- **Name:** white panels `NamePanel_L`, `NamePanel_R` (156 x 40 cm, middles `IK_Name_L/R`) and the rear left leaf's panel (`IK_Name_Rear`, 64 x 15, moves with RearDoorL): write the
  airport's name there at run time (airfield.json), facing out. **Livery:** materials `CV_Livery_Body_Paint`, `CV_Livery_Main_Paint` as on the apron bus.
- **Parking place (chosen, not drawn yet):** airfield plan middle (-253.0, 91.0), facing north (hx 0, hy 1), box x -254.75..-251.25, y 87..95, west of stand 9 (its boundary is x -245.4,
  the apron ends at x -265): checked against the stands, the tug / GPU / chock boxes, the jetway zones, the light masts (none west of x -43) and the staff's row (z 84.2). Way out: straight
  north to the service road (y 167..175). B draws the white box in Airfield.cpp and checks the boxes other 212 pieces have taken by then.
- **Not there:** no van / electric drive, sliding-door or vacuum sound in the project (a CC0 one is needed); not run in the game; wipers have pivots (`PIV_Wiper_*`) but only move if B binds them.

## Cabin cleaning van, part B as built (2026-11-23, compiled, NOT run): the shared service vehicles
`Public/ServiceVehicles.h` (the table's fields), `Private/GSEService.cpp` (the table and all the work), hooks in `GSE*`, `Turnaround`, `StaffPeople`, `Tugs/TugCrew`,
`JetwayRigs` (`ServiceSteps`), `Airfield` (the box). The van is one of AGSE's vehicles (`FTractor.Svc`): the same driving, lamps, beacon, wheels, dirt and wash.
- **Flow:** the `clean` task asks 8 min ahead (`SvcAsk`); driver + 1 at least, up to 4 (`AskTeam`, then `AskExtra`); sliding door, seats; out, to x stand + 11.4 / 25.5 m
  behind the nose wheel; rear doors; bag roll, caddy, vacuum, bag roll; they wait at the van until the last passenger is off (`SvcGo`) and the jetway is docked; outside the
  left wingtip to the jetway's service stair, step by step on its real steps, the service door, the cab, the aircraft's door; 30 rows at 40 s each, shared;
  two full bags down; loaded; home; nose out. `clean` ends when all are off the aircraft, so boarding waits; the flight waits while the van is at the stand (45 min at most).
- **Parking place moved** to (-259.6, 91): from (-253, 91) the van could only get back in through stand 9. Way in: down x -247.2, round, straight back in.
- **Stand-ins:** other airlines, and yours with fewer than 2 cleaners: data with its time, as before.
- **Not there:** no cabin exists (to-do 209), so at the aircraft's door they are hidden and no wiping / vacuuming is seen; no E to sit or drive, no glow outline (the tugs and
  tractors have none either), no holomap mark (no vehicle has one); passenger door, wipers, pedals not moved; livery colours not set; rolls used and full bags change at the next start.
- **Not checked:** everything in the game - the hand on the stair rail, the walk past the jetway's drive column, knees at the sliding door, legs along the wing, the 40 s row.

### How to add the next vehicle (fuel, catering, water, toilet, stairs, de-icing, ambulift)
1. Model: the part and point names of `cleaningvan-parts.json` (`Tyre_FL` .., `PIV_Wheel_*`, `PIV_Steer_F*`, `PIV_DoorDriver`, `IK_Stand/Handle/Step_*`, `IK_Seat_*`, `IK_Grab_L`,
   `IK_Hand_Wheel_*`, `IK_Foot_*`, `IK_Aisle_Row0` = the cab floor; lenses `*_Lens`, `BeaconLens`).
2. Points: `Data/<name>-riders.json` = `{"points": {name: [x, y, z]}}` in Blender metres (y left).
3. Jobs: a duty and a task in `EStaffDuty` / `EStaffTask`; its role, name and team in `StaffPeople.cpp` (`RoleOf`, `DutyName`, `TaskName`, `Needs`); `b3D` true in `Sim/ADPCrew.cpp`.
4. One entry in `ADPService::All()`: task id of `Turnaround.cpp`, model, points, role, team, people, parking box (Airfield draws it), place at the stand
   (`Lat`, `Aft`, `Far`; right of the aircraft = negative), size, turning circle, doors, things carried, seats, name panels; `bJetway` false for work at the vehicle.
5. `Turnaround.cpp`: the three `clean` lines (`SvcFor` / `SvcAsk`, `SvcGo`, `SvcBusy`) for its task id.
6. Work of its own (hoses, a lifting body): a branch in `SvcCabin` (what `bIn` / `RowsDone` mean for it) and its moving parts in `SvcParts`.
7. Free places: check `Lat` / `Aft` against the van's and the belt loader's (right, 19.6 m aft), the train's (right, ahead of the wing), the GPU, tug and rack boxes.

## Part 2: every stand-in use and its replacement (line numbers of 2026-11-23, Source/AirportDynastyPro/Private)

| Where | Today | Replace with | Nobody free |
|---|---|---|---|
| TugCrew.cpp:104 `SetupCrew` (called by Tugs.cpp:84 Setup) | 16 roster people spawned, 2 per tug + 6 walkers | return at once when `UStaffPeople::On()` | - |
| new `ATugs::Enlist(Duty, Near, Job)` | - | FindFree + Claim, an `FCrew` with that actor in a FREE slot of `Crew` (indices are captured in the jobs' lambdas: reuse slots, never remove), `Spot` = where it stood; gives the index or INDEX_NONE | - |
| new `ATugs::Discharge(Ci)` | - | `Unpose`, walk out onto the apron, `Release(id)`, clear the slot (Rig, Rider, Anim, Q, Hold, Follow) | - |
| Tugs.cpp:251 `Assign`, 263 (driver boards) | `T.Driver` fixed | Enlist TugDriver + PushbackHandler into `T.Driver` / `T.Handler`. `T.Driver == INDEX_NONE` today means "the tug works by itself" (Tugs.cpp:263, 344, 360, 374, 575, 585, 623; TugCrew.cpp:547, 778): with the switch on it must mean "wait" | asked again every few seconds; `Tell` "pushback waits: " + Missing(Pushback); the flight runs late |
| Tugs.cpp:268 `Arrived` (`GpuConnect`) | handler or driver | Enlist GpuHandler, Discharge when the cable is in | GPU stays off (Turnaround's `gpu` task waits), asked again |
| Tugs.cpp:275 `Arriving`, 282 `Prepare` | driver sits ready 3 min before | Enlist TugDriver early | skip; Assign asks again |
| TugCrew.cpp:1084 `GpuDisconnect` | handler | the push's handler, else Enlist GpuHandler | push waits |
| TugCrew.cpp:555 `CrewWalkers`, 592 `WalkersDone` | 6 shared walkers, up to 3 | Enlist WingWalker up to 3, Discharge in WalkersDone | goes on with fewer, as now (logged) |
| GSECrew.cpp:81 `Borrow`, 107 `GiveBack` | nearest idle walker / tug crew | `Borrow(Near, Duty)` = Enlist; GiveBack's last act = Discharge | INDEX_NONE, as now |
| GSECrew.cpp:342 `StartTrain` | driver | Borrow TractorDriver | train not started, PollBags asks again; `Tell` bags wait |
| GSECrew.cpp:546 `StartGpu` (GPU towed) | driver | Borrow TractorDriver | not moved, as now |
| GSEBags.cpp:102 make-up, 255 unload | helper | Borrow BaggageHandler | driver alone (works today) |
| GSEArrivals.cpp:73 | helper | Borrow BaggageHandler | driver alone (works today) |
| GSELoader.cpp:128 operator, 188 hold loader | 2 | Borrow LoaderOperator, BaggageHandler | no operator: bags wait, `Tell`; no hold loader: as now |
| Turnaround.cpp ~124 `Crew`, `Need` | head count of the sim | `Missing(Task)` for gpu, bulk, tug; keep the count for bridge and doors (other staff, 115 / 210) | task waits, `ADPSchedule::Tell` says who |
| Turnaround.cpp ~163 `Crew >= 2` before `Gs->Departure` | sim count | `Missing(BagsOut).IsEmpty()` | later |
| Sim/ADPSchedule.cpp:185 `MissingStaff` | 2 on shift | keep (it also runs with no 3D world) | - |
| GSE.cpp:1066, Tugs.cpp:785 / 804 (the watch camera) | crew by index | unchanged (indices stay valid) | - |
| CoachIKTest.cpp:77 | takes ground MetaHumans by name | unchanged (a test) | - |

Keep in part 2: Release only from the apron (GSE's `WalkTo` knows the way out through Ramp services 13); a claimed person
finishes the job after their shift ends; the log and notices by `ADPSchedule::Tell`; the boards follow the planned leg's
`Est`, so a waiting push has to move it (SimTraffic.cpp:312-407 asks `Push` / `IsClear`); `FWho.bWorks` so E-talk knows they
already work for you. Not checked in part 1: the straight way back over a far apron (`Around` knows vehicles, not parked
aircraft - the same as `GiveBack` today).

**Question for the owner:** the other airlines' turn-rounds use the same 16 stand-ins ("their handling agent",
Turnaround `Crew = 99`). When the stand-ins go: does your crew work those flights too (for the handling fee, as the
airport's own handler), or does a small agent team stay for them?

## Part 3 as built (2026-11-23, compiled and type-checked, not run): hiring for every area
- **One table:** `Sim/ADPCrew.h/.cpp` - 21 areas, 23 jobs (3D job or not, counted by the simulation or not, shifts, can be offered
  in a talk) and the 4 shifts. Read by the hiring pool (`FillPool`, the hourly change), `StaffPeople` (`b3D` = which jobs are people in
  3D, the shifts) and the Crew screen (in the bridge's `crew` answer: `areas`, `jobs`, `shifts`). A new job is one line there.
- **New jobs:** water, deice, marshaller, cargo, lounge, shop, claim, parking; gate, bus, tower, ops and fire can now be hired. Wages:
  the existing list; jobs not in it get the existing general wage. No new balancing. Old saves get the new candidates on load.
- **Crew screen** (`screens/sim-crew.ts`): one button per area (people, how many missing), its need, staff (shift, wage, where and what
  now from `UStaffPeople::Status`, else on / off shift and "3D job coming"), manage (pay, shift, base, days off, contract, let go), the
  people looking for that work, adverts. Live every 5 s. Not flight crews, mechanics, firefighters: no 8 hour shift to set.
- **Needs:** the game's own rules (`ADPCrew::Need`: fleet, `TeamNeeds`, the fire category of your biggest aircraft); a job no rule
  needs shows "1 advised", which nothing depends on.
- **Reached from:** the Overview (Crew card with who is missing, the airport card's links), Timetable (its missing-staff line), every
  "hire" link (`go-hire`), the Staff app of the tablet and the computers (`simStaffApp`; `apt-staff.ts` only without a career).
- **Talk:** "would you like to work for you" - on yes the job is chosen (staff their own, a passenger any without a licence), hired by the
  existing `ADPStaff::Hire` with the first week's pay, their own face kept (`FADPPerson.Face`, any built person), starts after the next local
  midnight. Who works for you already says so (also the open point of 2b).
- Airport jobs are hired at this airport, not the hub (the same place today).
- **To confirm with the owner:** the tower cannot be offered in a talk (licence); the start the next day; gate agents do nothing yet
  (passenger service boards the flights).
- **Known limits:** a stand-in or passenger hired in a talk is still seen as before until the next load, so the same face can be
  there twice; clothes are theirs until the clothes work; a candidate has no picture on the Crew screen.

## Part 3: how it was before (kept for reference)
- **Screens** (Airline-Manager-Pro-Babylon/game/src/screens): `sim-crew.ts` - tabs (line 30-32; the `ground` tab = ground,
  fuel, catering, cleaning), hire tab with an advert per role (line 78), hire / dismiss / pay / interview (`sim-hire` line 84);
  built into www/game by `npm run build`. `apt-staff.ts` is the old web rota (its own staff, not the sim's).
- **Bridge:** UI/ADPBridgeSim.cpp: a person's JSON (line 41: status, home, shift), the actions hire / dismiss / pay / base /
  interview / advert / contract / dayoff (line 443-475); all work for any role.
- **Sim:** ADPStaff.cpp `BaseWage` (wages per role), `FillPool` (who can be hired), ADPGround `TeamNeeds`.
- **The newest example:** `checkin` "Passenger service": its own tab and advert, wage, pool entry, `ADPSchedule::OnShift` used
  by PaxFlow (lines 407, 508, 1609), "Hire them under Crew" in the notices.
- **Hiring today (checked in code 2026-11-23), Crew > Hire staff / its tabs:** captain, first officer, cabin crew, mechanics,
  passenger service (check-in and boarding), security, baggage handlers (= all ground crew), fuel drivers, catering, cleaners.
- **Gaps:**
  - ground crew: hireable, but called "Baggage handlers" - no tug driver / pushback / wing walker / GPU / tractor / loader
    duties shown, no shift set (bridge has no action), no "in 3D now / at work on", no needs per turn-round, no face.
  - fire: people are made for it, but no tab, no advert in Crew.
  - gate (boarding), bus drivers, tower, ops: a wage only - nobody made in the pool, no tab, no advert.
  - cargo / marshaller, de-icing, water / toilet service, lounge, shops, baggage claim, car park, medics, police: no role at all.
  - fuel, catering, cleaning: hireable, no 3D people or job; check-in and security: hireable, 3D people are 210 / 211.
  - People's "do you want to work for me" (People.cpp) makes no real `FADPPerson`.
