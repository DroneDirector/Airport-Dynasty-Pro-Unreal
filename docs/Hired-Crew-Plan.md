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
- **Not there:** no cabin exists (to-do 209), so at the aircraft's door they are hidden and no wiping / vacuuming is seen.
- **Not checked:** everything in the game - the hand on the stair rail, the walk past the jetway's drive column, knees at the sliding door, legs along the wing, the 40 s row.

## Part C (2026-11-23, compiled, NOT run): you in every service vehicle (`ServiceDrive.h/.cpp`, hooks in `GSEService.cpp`)
- **E at a door** (`SvcUse`): you step to the door, hand on the handle, door, climb and seat - the crew's own pose functions on your own body. Driver's door: you drive
  (30 km/h, throttle / brake keys, V = from behind); the crew's door: the seat nearest it; the `Cab` seat (own door): also to ride along with the crew. E again: braked, out.
- **Free or taken** (`SvcSeatFree`): idle = all free; on a job the driver's seat only while the whole crew is up in the aircraft, the crew's seats not; `Cab` always while it stands.
- **The job is never lost:** a waiting job says "you have the cleaning van"; taken at the stand, the crew wait there, and once you have got out its driver fetches it back
  (`SvcFetch`) and they go on (`SvcReturn`); the flight waits only for the cabin when the van is more than 60 m off. Left where it cannot be driven from: the reason says so.
- **Solid:** a box against you on foot; while you drive: walls, vehicles, tugs, GPUs stop it, people ahead stop it in time, only the airfield's own surface. It does not pull
  away with a door open. E at the load doors opens / shuts them (one leaf after the other; no hand pose of yours for that yet).
- **Stock:** full bags and used rolls stay in the vehicle (`bUsed`) and change only after 30 s shut in its box with nobody in it; left elsewhere, the next crew drives it to its box first.
- **Wipers** (rain, drive on), **pedals**, the right foot on the pressed one, hands going round with the wheel: shared, for the crew and for you.
- **Livery:** `SvcLivery` puts `FADPAirline::LiveryBody / LiveryMain` ("#rrggbb") on the model's livery paint. NOTHING sets those two yet: no vehicle in the game takes the
  airline's colours so far and the airline has no colours; empty = the model's own.
- **Holomap:** every vehicle and cart of AGSE with its nose (`AGSE::MapShapes`). (The glow was taken out again: the owner wants none on vehicles.)
- **Not checked:** all of it in the game; the mirrored climb on the passenger's side, the wipers on the curved glass (they lift about 1 cm), the glow's thin line round a van.

### How to add the next vehicle (fuel, catering, water, toilet, stairs, de-icing, ambulift)
1. Model: the part and point names of `cleaningvan-parts.json` (`Tyre_FL` .., `PIV_Wheel_*`, `PIV_Steer_F*`, `PIV_DoorDriver`, `IK_Stand/Handle/Step_*`, `IK_Seat_*`, `IK_Grab_L`,
   `IK_Hand_Wheel_*`, `IK_Foot_*`, `IK_Aisle_Row0` = the cab floor; lenses `*_Lens`, `BeaconLens`).
2. Points: `Data/<name>-riders.json` = `{"points": {name: [x, y, z]}}` in Blender metres (y left).
3. Jobs: a duty and a task in `EStaffDuty` / `EStaffTask`; its role, name and team in `StaffPeople.cpp` (`RoleOf`, `DutyName`, `TaskName`, `Needs`); `b3D` true in `Sim/ADPCrew.cpp`.
4. One entry in `ADPService::All()`: task id of `Turnaround.cpp`, model, points, role, team, people, parking box (Airfield draws it), place at the stand
   (`Lat`, `Aft`, `Far`; right of the aircraft = negative), size, turning circle, doors, things carried, seats, name panels; `bJetway` false for work at the vehicle.
5. `Turnaround.cpp`: the three `clean` lines (`SvcFor` / `SvcAsk`, `SvcGo`, `SvcBusy`) for its task id.
6. Work of its own (hoses, a lifting body): a branch in `SvcCabin` (what `bIn` / `RowsDone` mean for it) and its moving parts in `SvcParts`.
7. Yours too, without code: parts `Pedal_Accel` / `Pedal_Brake` with `PIV_Pedal_*`, `IK_Foot_Brake`; in the table `Cab` (a seat with its own door), `Wipers` (part prefix,
   pivot, the blade end's place lying down and wiped up), `LiveryBody` / `LiveryMain` (material names), `PlayerSpeed`; `Doors[1..]` are the load doors, a vehicle with none leaves only `Doors[0]`.
   Nothing of its own may hide or show in view: put what changes behind `bUsed` / `SvcRestock`. Hoses and tanks have no hook yet: step 6.
8. Free places: check `Lat` / `Aft` against the van's and the belt loader's (right, 19.6 m aft), the train's (right, ahead of the wing), the GPU, tug and rack boxes.

## Aircraft refueller, part A as built (to-do 212, 2026-11-24): model, blueprint, parking place - no code yet
Model `Art/Refueller` (make_refueller.py; cab skin by the new `Tools/vehicle_skin.py`, for the next vehicles too), blueprint `docs/Blueprints/Apron/Refueller.png/.pdf`, riders `Data/refueller-riders.json`
(written by the build). Own rigid 6x2, 20 m3: 10.198 x 2.45 (3.00 over the mirrors) x 3.08 m; axles x +370 / -120 / -255 cm, wheelbase 490, tyre radius 50.7. All points: `Art/Refueller/refueller-parts.json`.
**What part B must know**
- **Place:** `ABabylonModel::Place("Refueller")`. Wheels FL FR RL RR as everywhere (`Tyre_` + `Rim_`; all of a wheel's metal is in `Rim_`); the drive axle is twin (`Tyre_RL` + `Tyre_RL_In`, one pivot).
  **Third axle:** `Tyre_TL/TR`, `Rim_TL/TR`, `Spindle_TL/TR`, `PIV_Wheel_T*`, `PIV_Steer_T*`: new for AGSE - spin with the rear, steer OPPOSITE to the front (tan t = 1.35 / 4.90 x tan front; front 38 = tag 12).
- **Drive:** KCentreX 0, HalfL 510, HalfW 123, TurnR 660, MaxSteer 38 (steer sweep to 38 clear), DoorDeg 68, KFloor 119. Steering wheel axis as the belt loader's. Pedals, wipers (`WiperL/R`, tips `IK_WiperTip_*`),
  `BeaconLens`, lenses as the van's. Cab floor 1.18 m: two steps per side (`IK_Step_Door*_Low` 0.49, `IK_Step_Door*` 0.83), grab handles `IK_Grab_*` outside, `IK_GrabIn_*` inside: the shared climb knows ONE step.
- **Doors:** `DoorDriver` / `DoorPassenger` (hinge in front, 68), `PanelDoorL` (pump panel) / `PanelDoorR` (locker): hinge at the BACK, Unreal yaw -110 left / +110 right; stand `IK_Stand_PanelDoor*`, afterwards `IK_Stand_*_Open`, work at `IK_PanelStand`.
- **Panel** (left): `Lever_Pump/HoseL/HoseR/Defuel` (about y through `PIV_Lever_*`, hand `IK_Lever_*`), `GaugeNeedle0..2` (about y, 0 straight up; the dial runs -135 .. +135), `Meter_Face` (write the litres at `IK_Meter_Face`, 28 x 8.5 cm),
  `Printer_Ticket` + `IK_Ticket`, buttons `IK_Button_*`, dead man: `Deadman` (taken in the hand), `Deadman_Button` (its button, a part of its own: move it with it), `DeadmanLead` (the stowed lead: hide it, a thin cable from `IK_DeadmanLead`). `WorkLamp_Lens` = the cabinet's lamp.
- **Hoses:** `HoseReel_L/R` (spin about `PIV_HoseReel_*`), 15 turns each `HoseTurn_L/R_00..14` (00-04 inside, 10-14 outside; the only parts with an origin of their own, the reel centre; hide from 14 down as it pays out: 2.23 / 1.85 / 1.46 m a turn, 27.7 m in all), `HoseTail_L/R` (the stowed run reel - guide - nozzle:
  hide it when the nozzle is taken), then a `UCableComponent` (as the wash hose and the GPU cable: width 6.4, 36 segments, stiff, colliding) from `IK_HoseOut_*` to `IK_Nozzle_*_Hose`. `Nozzle_L/R`: `PIV_Nozzle_*`, two hands `IK_Nozzle_*_Hand_L/R`,
  mating face `IK_Nozzle_*_Mouth` (mouth UP on its holder: it goes on to the wing's coupling the same way up). May glow.
- **Platform:** parts `Platform` + `LeverPlatform` + `GatePlatform` (about `PIV_GatePlatform`, inwards 90) go straight up 0 .. 90 cm (floor 145 .. 235). Scissor: `ScissorA1/B1/A2/B2` (one mesh each, left and right arm): with lift q (cm) `t = asin((31 + q) / 210)`;
  stage s = 1, 2: centre x = -472 + 52.5 cos t, z = 107.5 + (s - 0.5) x 105 sin t (pivots `PIV_Scissor1/2`), A turned nose up and B nose down by t - 8.49 deg. `RamBarrel` / `RamRod` lie from `PIV_Ram` to `PIV_RamEye`: aim both, or leave them.
  Up the back by `IK_Step_Platform_0..3` (hands `IK_Grab_RearSteps_*`, then `IK_Grab_Platform_*`), stand `IK_Platform_Work_L/R` (floor `IK_Platform_Floor`), lever `IK_LeverPlatform` (dead-man style).
  At the wing: the coupling is said to be 3.2 - 3.5 m up; from the lowest floor (1.45 m) a 1.75 m person's head is at 3.20, the hands reach 3.6, and the rails are at 2.55 (3.45 raised): **lift little or nothing on the A320**, measure the coupling on the aircraft first.
- **Bonding first:** `BondReel` (spin about `PIV_BondReel`, crank `IK_BondReel_Crank`), `BondClamp` (in the hand, `IK_BondClamp`), `BondTail` (hide), thin cable from `IK_BondOut`. **Chocks:** `Chock_0/1` (`IK_Chock_*`) to `IK_ChockPlace_0/1` (before / behind the left drive wheel).
  **Extinguishers** `Extinguisher_L/R`, shut-offs `IK_EmergencyL/R`.
- **Name:** `NamePanel_L/R` (240 x 42 cm, `IK_Name_L/R`), `NamePanel_Front` (124 x 8.5), `NamePanel_Rear` (60 x 13, `IK_Name_Rear`). Livery paints `RF_Livery_Body_Paint`, `RF_Livery_Main_Paint` (a fuel company's truck: leave them unless the owner wants the airline's).
- **Parking place (MOVED in part B to (-260.2, 146))** - part A had chosen **(no fuel farm, depot or fuel stand exists in any plan, to-do or airfield.json):** middle (-261.8, 128.0), facing north, box x -263.75..-259.85, y 121.9..134.1: 14 m west of stand 9's line (x -245.4), 1.25 m inside
  the apron edge (x -265), 27 m north of the cleaning van's box and 9 m north of where the van turns, 39 m from the terminal, no mast near. With x -261.8 the shared `SvcDriveHome` loop (2 x 6.6 m beside the box) just fits between the apron edge and stand 9's line
  (worst outer corner about x -265.2 / -245.2, y 133..143): B checks it in `Apron()`. It crosses the mouth of ROAD-TOWER (x -250, y 171).
- **Not there:** no diesel truck, pump, reel or scissor sound checked for; nothing run in the game; the tank top has no ladder of its own (nobody goes up there).

## Aircraft refueller, part B as built (to-do 212, 2026-11-24, compiled, NOT run)
`Private/GSEFuel.cpp`, `Public/GSEFuel.h` (its own parts and work), one table line in `GSEService.cpp`, hooks in `GSE*`, `ServiceDrive`, `StaffPeople`, `TugStaff`, `Turnaround`, `ADPEconomy`.
- **Parking box moved** to (-260.2, 146), facing north: beside stand 9 the shared loop only fitted by centimetres (a 10 m truck sweeps 18.7 m). New shared way home, `HomeRoad`: along the
  service road past the corner onto the tower road's mouth, backed round it and 18 m straight back; 0.6 - 2 m of paving outside its swept path (worked out, not driven).
- **At the stand:** 11.08 m right, 7.45 m behind the nose wheel, facing the terminal, the platform under the coupling. In: down a lane 19.5 m out, two 9 m bends behind the wing, straight under the
  outer wing (3.18 m under 3.75 m); out: backed the same way (new table fields `InLat/InAft/InR`). Swept with its mirrors: 1.1 m and more from cones, engine, belt loader, train.
- **Coupling (measured on Art/A320/A320.glb):** 9.9 m out, 11.75 m behind the nose wheel, wing underside 3.80 m. The wing has NO modelled coupling or panel. Platform 38 cm up: rails 2.93 m.
- **Work, in stages** (a relief goes on from the last finished one): two chocks, bonding clamp on the main gear leg, panel door and hose valve, (last passenger off), nozzle + hose, platform, on,
  down, pump lever, dead man held, off, hose wound in, valve, door, clamp, chocks, home. Hose, bonding cable, dead man lead: `UCableComponent`; reels, hose turns, needles, litres move.
- **Fuel:** `ADPEconomy::LegFuelKg` is the one number: what `BookLeg` charges is what is pumped (0.8 kg a litre, 1000 l a minute, at most 19500 l); Turnaround's `fuel` task lasts as long.
- **Waits:** the flight waits with the reason (called / connecting / litres / disconnecting / leaving). Boarding does not wait, as before. `fuel` waits for the chocks like the other vehicles.
- **People:** one hired fuel driver (`EStaffDuty::FuelDriver`, role fuel, `b3D` true); fewer than two hired, other airlines, stands over 36 m: data with its time, as before.
- **Yours:** E at both cab doors (two-step climb, shared with the crew), right locker door by E; the driver's seat never while its driver has the hose out. The panel (pump) is not worked by you.
- **No glow on any vehicle** (owner): `SvcGlow` and its hook removed.
- **Not there:** truck, pump, reel, scissor sounds; a second bought fuel truck is not a second vehicle in 3D; no fuel farm (filled up by 30 s shut in its box).
- **Not checked:** everything in the game - the way under the wing, the steps of the platform, the hose on the rail, the clamp reach over the wheel, the wipers' upper place (worked out), the turn signs of the levers.

## Catering high-loader, part A as built (to-do 212, 2026-11-24): model, kit, blueprint, parking place - no code yet
Models `Art/CateringTruck` (make_cateringtruck.py; cab, seats, doors, wheels by the new `Tools/truck_cab.py` = the refueller's cab as a text for every next truck) and `Art/CateringKit` (make_cateringkit.py),
blueprint `docs/Blueprints/Apron/CateringTruck.png/.pdf`, riders `Data/cateringtruck-riders.json`. Own 2-axle truck 8.93 x 2.50 (3.00 over the mirrors) x 3.76 m (3.92 with the body's beacon); x -437.8 .. 455.3,
axles +316.3 / -173.7, wheelbase 490, tyre radius 50.7. All points and numbers: `Art/CateringTruck/cateringtruck-parts.json` (`scissor`, `box`, `tail`, `stock`, `points_cm`; root, Unreal cm, y right).
**What part B (driving, crew, IK) must know**
- **Place:** `ABabylonModel::Place("CateringTruck")`, kit `Place("CateringKit")` (instanced). Wheels FL FR RL RR, twin `Tyre_RL` + `Tyre_RL_In` (`Rim_RL` likewise), steering wheel, pedals, wipers, doors, two-step climb, seats (2) exactly as the refueller.
  HalfL 456, HalfW 123, TurnR 660 (38 deg is clear), DoorDeg 68. **Never under a wing: 3.76 m body.**
- **Goes up with the body** (straight up, lift q 0 .. 275 cm, floor 150 .. 425): every part whose name starts with a prefix of `box.moves_with_box` except `box.not` (`BoxStep*`, `TailBrake*` stay): BoxInside / BoxOutside / BoxUnder, Box_WallL/R, BoxDoorL, Roll*, RampFlap,
  Platform, RailPlatform_*, GatePlatform, Tail*, NamePanel_L/R, WorkLamp_Lens, Lever_Lift / Lever_Plat, BoxLight*_Lens (inside lamps), BoxMark*_Lens, BoxRearWork_Lens, BoxBeaconLens (2nd beacon; `BeaconLens` on the cab). Points IK_Box*, IK_Roll*, IK_Platform*, IK_Tail* too.
- **Scissor:** `t = asin((12 + q) / 440)`; centre x = 136.3 - 220 cos t, z = 110 + (12 + q) / 2; `ScissorA` (low in front) turns nose DOWN / tail up by t - 1.563 deg about that centre, `ScissorB` the other way. Rams: base `PIV_Ram_L/R`, eye on A:
  x = 136.3 - 260 cos t, z = 110 + 260 sin t - 1.2, same y; `RamBarrel_*` turns about the base to look at the eye, `RamRod_*` keeps its eye end on the eye. Checked by mesh overlap at q 0 / 60 / 150 / 175 / 205 / 275.
- **Legs:** `StabLeg_FL/FR/RL/RR` straight down 30 cm (then on the ground). Lever `Lever_Stab` (about x through its PIV), hand `IK_Lever_Stab`, stand `IK_Stand_Stab`; stop `IK_Button_StabEStop`. Legs down BEFORE the lift, up after.
- **Into the body:** left side door `BoxDoorL` (hinge in front `PIV_BoxDoorL`, 100 deg), three steps `IK_Step_BoxDoorL_0..2` (41 / 71 / 101 cm, floor 150), rails `IK_Grab_BoxDoor_F/R`, `IK_Stand_BoxDoorL`, `IK_Box_In`. Only with q = 0. Crew ride up inside.
- **Lift panel** inside left of the front door: `Lever_Lift`, `Lever_Plat` (about y through their PIV), `IK_Lever_*`, `IK_Button_EStop`, stand `IK_BoxPanel_Stand`.
- **Platform:** `Platform` + `RailPlatform_L/R` + `GatePlatform` slide +x 0 .. 295 cm; **only at q >= 180** (6 cm over the cab's beacon). Deck `IK_Platform_Work` = 13.5 cm under the floor, so deck at the sill: q = sill - 136.5 (R1 3.87 m: 250.5; R2 3.49: 212.5).
  The rails are modelled lying in the cassette: up = about x through `PIV_RailPlatform_L` / `_R`, left one -90, right one +90 in Blender (right first, it lies on top). The gate lies inside the right rail: it goes with that rail, then opens about z
  through `PIV_GatePlatform` (about 90 across the end; it is 134 long). Hands `IK_Rail_L/R_Hand`, `IK_Handle_GatePlatform` are given standing. Touching: `IK_Platform_Bumper`. `RampFlap` about y through its PIV, forward-down 106 deg, after the front door's lower slats are up.
- **Roll-up doors:** `RollF_00..11` / `RollR_00..11` (00 = lowest, with the grips `IK_Handle_RollF_In/Out`, `RollR`): move all up together by d, hide a slat when its middle passes z 347 (`PIV_RollF/R`, inside the hood); fully open d = 190.
- **Tail lift:** arms `TailArms` about y through `PIV_TailArms` (length 90, modelled at +47.17 deg = floor; ground -53.66); `TailPlatform` stands up, its hinge `PIV_TailPlatform` travels with the arms' tip and it swings back 90 to lie level. Opening edge `IK_Handle_TailPlatform`, buttons `IK_Button_TailUp/Down`
  (right rear, `IK_Tail_Control_Stand`), `IK_Tail_Floor`, `IK_Tail_Stand`. It goes up with the body: fold it up before the lift.
- **Trolleys and boxes** are NOT in the truck: `stock` = 18 `Trolley` + 12 `GalleyBox` places (floor points with Blender yaw; they ride with the body). The trolley is open at both ends: two `TrolleyDoor`s, red + green pedal at each end, four `TrolleyCastor`s, places / hands / angles in `Art/CateringKit/sizes.json`
  (door 175 deg, pedal 14 deg, castor trail 18 mm). Lengthwise in the body, two abreast each side, aisle about 1.05 m. Rack of three shelves right rear. Straps `IK_Strap_*`. They may glow; the truck never.
- **Name:** `NamePanel_L/R` (360 x 62, `IK_Name_L/R`, ride with the body), `NamePanel_Front` (124 x 8.5), `TailPlatform_NamePanel_Rear` (110 x 26, `IK_Name_Rear`). Livery paints `CT_Livery_Body_Paint`, `CT_Livery_Main_Paint`.
- **At the A320** (measured on Art/A320/A320.glb, door drawings on its skin): R1 middle 0.30 m AHEAD of the nose wheel, 1.02 wide, sill 3.87, skin 2.0 m out; R2 middle 24.77 m behind, sill 3.49, skin 1.57 m out and turned about 11 deg (the aircraft's tail narrows).
  The bumper is 531.3 cm ahead of the origin with the platform out: origin 7.34 m right at R1, 6.88 m at R2 (turn it by the 11 deg), nose to the aircraft; the cab stays 0.7 m off. R2: tailplane root starts 27.8 m behind (1.2 m clear). The model's nose stands high (real sills 3.4): measure again in the game.
  `Turnaround.cpp` has `cat1` (R1, 4 trolleys) then `cat2` (R2, 7): one truck drives from one to the other.
- **Parking place (chosen, not drawn yet):** middle (-253.5, 146.0), facing north, box x -255.45..-251.55, y 140.5..151.5, beside the refueller (3.2 m between, with mirrors), 6.4 m off stand 9's line, 3 m off the van's way (x -247.2). Same `HomeRoad` 25: past it on the service road, backed
  round (the arc lies 6.7 m further from the tower road than the refueller's), 18 m straight back. Worked out on paper, not driven; nobody else has that place by 11-24.
- **Not there:** no truck, hydraulic, roller door or trolley sound checked for; nothing run in the game. The cab's aerial is left off (platform).

## Catering high-loader, part B as built (to-do 212, 2026-11-24, compiled, NOT run)
`Private/GSECater.cpp`, `Public/GSECater.h` (its parts, trolleys and work), one table line in `GSEService.cpp` (task `cat1`, `bCater`), hooks in `GSE*`, `ServiceDrive`, `StaffPeople`, `TugStaff`, `Turnaround`.
- **Parking box** (-253.5, 146) facing north, drawn by Airfield; the backed arc by `HomeRoad` swept on paper: inside the apron edge (0.35 m at its front), 2.8 m off the refueller's box, 2.0 m off stand 9's line.
- **At the stand:** down the lane 20.43 m right (the middle between two wingtips: 2.2 / 1.7 m off them, never under a wing), one bend, square on to R1; then back up the lane and on to R2 (turned 11 deg). Dock and sill are taken from the
  aircraft as it stands (R1 7.34 m, R2 6.81 m right; bumper 3 cm off the skin; sill clamped 3.30 - 4.05 m over the truck's ground). Swept: refueller 1.15 m, engine 2.3, tailplane 1.15 - 2.0, parked belt loader 2.85.
- **One on the right-hand lane at a time** (`SvcSideBusy`): the refueller, the catering truck; trains and belt loader leaving wait for it (`CaterMoving`). **A train stands where the truck stands at R2:** the truck waits at R1 until the train is gone,
  trains and the belt loader wait while it is at R2 (`CaterRear`, `CaterRearFree`); the flight says so.
- **Work, in stages** (one of the two works, the other waits): legs down, side door, three steps, up inside, (last passenger off), lift lever held, roll-up door, platform lever held (only from 180 cm), flap, right then left rail, a knock, 4 (R1) / 7 (R2)
  trolleys one trip at a time - pedal, both hands on the handle, out of their row, over the flap to the sill; an empty one back into the row - and all back; legs up; R2; home. The used truck is restocked after 30 s shut in its box.
- **Aircraft doors:** one mesh, no moving door, no cabin (to-do 209): at the sill person and trolley are hidden 14 s (logged), as the cleaners.
- **Plan tasks:** `cat1` asks 8 min ahead and is done when R1 is, `cat2` when R2 is; boarding waits for `cat2` as before. No cost changed (the catering already booked stays the one cost).
- **People:** hired `catering` (driver + loader, `b3D` true); fewer than two hired, other airlines, stands over 36 m: data with its time. One vanished: the other goes on alone and may drive; the last one: a relief.
- **Yours:** E at both cab doors and the side door; the driver's seat not while its crew are at work. No glow.
- **Not there:** truck, hydraulic, roller door, trolley sounds; castors do not swivel; the gate stays folded in the right rail (the fuselage closes the end); tail lift, rear roll-up door and galley boxes are not worked; the player cannot work the lift.
- **Not checked:** everything in the game - hand reach to the outer rows and the rails, the signs of the levers, flap and rams, the drive between the wingtips.

## Potable water truck, part A as built (to-do 212, 2026-11-24): model, blueprint, parking place - no code yet
Model `Art/WaterTruck` (make_watertruck.py) on `Tools/small_truck.py` = the small chassis and cab it shares with the toilet truck (own cab skin; materials, seat, mudguard from `truck_cab.py`), blueprint
`docs/Blueprints/Apron/WaterTruck.*` (`Tools/make_smalltrucks_blueprint.py`), riders `Data/watertruck-riders.json`. 5.785 x 2.00 (2.51 over the mirrors) x 2.52 m with the beacon; x -287.6 .. 290.9, axles +172 / -128,
wheelbase 300, tyre radius 38.3. Every point: `Art/WaterTruck/watertruck-parts.json` (`points_cm`: root, Unreal cm, y right; `hose`, `a320`).
**What part B (driving, crew, IK) must know**
- **Place:** `ABabylonModel::Place("WaterTruck")`. Wheels FL FR RL RR, twin `Tyre_RL` + `Tyre_RL_In` (`Rim_RL` likewise); steering wheel, pedals, wipers, doors (hinge in front, 68), seats (2), names as the refueller.
  **One** step: `IK_Step_DoorDriver` = `_Low` (44 cm), then the floor (82). HalfL 291, HalfW 101 (126 with mirrors), TurnR about 480 (38 deg is clear). Beacon 252 cm: it fits under the tail (belly 2.85 m and more there), never under the engines.
- **Work platform** (fixed, floor 100): steps left `IK_Step_Platform_0/1` (37 / 70), hands `IK_Grab_Platform_F/R`, gate `GatePlatform` (about z through its PIV, 80 deg inwards = Blender -80), `IK_Handle_GatePlatform`, `IK_Platform_In`, `IK_PanelStand`, `IK_ReelStand`, `IK_Platform_Work`; rails `IK_Rail_Back/Right`.
- **Coupling** = one part `Coupling` (PIV at its middle, held mouth up in the blue cup of `CouplingHolder`; grip `IK_Coupling_Hand`, twist ring `IK_Coupling_Ring`, lever `IK_Coupling_Lever`, hose at `IK_Coupling_Hose`, mouth `IK_Coupling_Mouth`). Hose 28 mm, 13.8 m on `HoseReel` (about y through `PIV_HoseReel`;
  turns `HoseTurn_HoseReel_00..13`, the outer layer is 07..13: hide from 13 down); it leaves at `IK_HoseOut`; `HoseTail` is the stowed piece (hide it, draw the hose out). Reel-in button `IK_Button_Rewind`.
- **Panel:** `IK_Button_PumpOn / PumpOff / Light / EStop`, `Lever_Valve` (about y), `IK_Panel_Screen`, gloves `IK_GloveBox`. **Fill point** (left side, hinge in front): `PanelDoorL` 110 deg, `IK_FillCoupling`, `Lever_Fill` (about x).
- **Own chocks** `Chock_0/1` in the right tray (`IK_Chock_*`, `IK_Stand_Chocks`, down at `IK_ChockPlace_0/1`).
- **Name:** `NamePanel_L/R` (156 x 30, `IK_Name_L/R`), `NamePanel_Front` (110 x 8.5), `NamePanel_Rear` (106 x 20, `IK_NamePanel_Rear`). Livery paints `WT_Livery_Body_Paint`, `WT_Livery_Main_Paint` (blue).
- **At the A320:** water door 171AL, 31.3 m behind the nose, 0.3 m LEFT of the middle, 2.6 m up (Airbus AC 5-4-9); on our model 26.2 m behind the nose wheel, belly 2.93 m (it stands 0.33 high). Suggested stand: behind it, nose to the tail, the truck's middle 1.2 m left, rear bar 26.4 m behind the nose wheel
  (truck 26.4 .. 32.2 behind; the tail is 3.1 m and more above it there), come and go from behind-left. The worker stands at the platform's left side and reaches 1.9 m above its floor, 0.6 m out. `Turnaround.cpp` has the task `water`.
- **Hygiene (owner):** water works on the LEFT and parks on the WEST side, toilet works on the RIGHT and parks on the EAST side; hoses, couplings and crews never mix.
- **Parking place (chosen, not drawn yet):** middle (-256.8, 117.0), facing north, box x -258.2..-255.4, y 113.5..120.5. The shared `SvcDriveHome` loop (TurnR 480) comes down x -247.2 (the van's own way in), round at y 132..127, straight back; 1.6 m to the catering box's end (y 140.5),
  18 m north of the van's box. B checks the van's way out (straight north on x -259.6 would pass 0.2 m beside this box) and moves the box 1 m east if it needs it. Worked out on paper, not driven.
- **Not there:** no electric small truck, pump or reel sound checked for; no free photo of a small water truck found (the body follows lavatory trucks' photos and makers' figures); nothing run in the game.

## Toilet truck, part A as built (to-do 212, 2026-11-24): model, blueprint, parking place - no code yet
Model `Art/ToiletTruck` (make_toilettruck.py) on the same `Tools/small_truck.py`; blueprint `docs/Blueprints/Apron/ToiletTruck.*`, riders `Data/toilettruck-riders.json`. Size, wheels, cab, step, doors, chocks, TurnR as the water truck (see there); orange band. Points: `Art/ToiletTruck/toilettruck-parts.json` (`waste_hose`, `rinse_hose`, `lift`, `a320`).
**What part B must know**
- **Place:** `ABabylonModel::Place("ToiletTruck")`.
- **Lift:** everything whose name starts with a prefix of `lift.moves` (`PlatformLift`, `LiftRod`, `GatePlatform`, `Lever_Platform`, `NamePanel_Rear`) goes straight up 0 .. 95 cm (floor 62 .. 157); the points IK_Platform_*, IK_Rail_*, IK_Grab_Platform_*, IK_Lever_Platform, IK_Button_PlatEStop, IK_Handle_GatePlatform, IK_Name_Rear ride too.
  `MastParts` stay. Checked by mesh overlap at 0 / 30 / 60 / 95. At our A320 raise it 73 cm (floor 135), at the real height 40. Down before the truck drives.
- **On to it:** step `IK_Step_Platform_0` (35), gate on the left `GatePlatform` (about z through its PIV, 80 deg inwards = Blender -80), `IK_Platform_In`, `IK_Platform_Hose` (beside the waste coupling), `IK_Platform_RinseStand`, `IK_Platform_Work`. Lever `Lever_Platform` (about y), `IK_Button_PlatEStop`; second lever at the mast `Lever_Lower` (about x, `IK_Stand_Lower`).
- **Waste hose** 4 in (11 cm outside, 4.6 m; 2.7 m straight from its inlet to the panel): `WasteHose` lies in its rack, path and length in `waste_hose`; fixed end `IK_WasteHose_In`. Coupling `WasteCoupling` (PIV, hands `IK_WasteCoupling_Hand_L/R`, cam lever joined to it, `IK_WasteCoupling_Mouth` / `_Hose`), cap `WasteCap` (off first, `IK_WasteCap`). Hide `WasteHose` and draw the hose when it is out.
  `SplashTray` (about y through its PIV, Blender -85 = hangs down) is under the mouth: down before the coupling leaves, up again after. Valve lever `Lever_Vent` on the tank panel (left, `IK_Stand_TankPanel`), emptying valve behind `Lever_Dump` (depot only).
- **Rinse hose** 32 mm, 11.3 m: `RinseReel` (about y, `PIV_RinseReel`, turns `HoseTurn_RinseReel_00..14`), out at `IK_RinseOut`, `RinseTail` stowed, coupling `RinseCoupling` (hand, ring, lever, mouth, hose as on the water truck). Buttons `IK_Button_RinsePump / RinseStop / ReelIn`.
- **Name:** `NamePanel_L/R` (160 x 28), `NamePanel_Front`, `NamePanel_Rear` (96 x 20, rides with the lift). Livery `TT_Livery_Body_Paint`, `TT_Livery_Main_Paint`.
- **At the A320:** waste door 172AR, 31.3 m behind the nose, 0.8 m RIGHT, 2.8 m up, drain 4 in + rinse 1 in (Airbus AC 5-4-10); on our model 26.2 m behind the nose wheel, belly 3.13 m. Suggested: behind it, nose to the tail, the truck's middle 1.5 m right, rear bar 26.4 m behind the nose wheel, from behind-right.
  The panel is then 0.7 m to the RIGHT of the truck's own middle; with the water truck at 1.2 left 0.2 m stays between the mirrors, so better one after the other. The catering truck at R2 stands 23.4 .. 26.1 m behind (mirrors to about 26.3): wait for `cat2` to leave. `Turnaround.cpp` task `toilet`, vehicle `toilettruck`. Gloves, visor, apron for the worker are not modelled.
- **Parking place (chosen, not drawn yet):** the EAST end, middle (554.6, 110.0), facing north, box x 553.2..556.0, y 106.5..113.5: about 5 m off stand 16's line (about x 548.3), loop in at x 564.2 (apron edge 570). Nothing is drawn or parked there (looked through airfield.json and the GSE sources). Worked out on paper.
- **Not there:** no lift, pump or vacuum sound checked for; nothing run in the game.

## Water truck and toilet truck, part B as built (to-do 212, 2026-11-24, compiled, NOT run)
`Private/GSEWater.cpp`, `Public/GSEWater.h` (both trucks), two table lines in `GSEService.cpp` (tasks `water`, `toilet`, field `Wet` 1 / 2), hooks in `GSE*`, `ServiceDrive`, `StaffPeople`, `TugStaff`, `Turnaround`.
- **Boxes** (drawn by Airfield, on paper only): water (-256.4, 117), 0.4 m east of part A's place: 0.7 m mirror to mirror off the van's straight way out, 0.4 m inside stand 9's line on its loop, 1.6 m to the catering box. Toilet (554.6, 110) as chosen.
- **At the stand:** they BACK in: behind the tail, a U-turn of two 4.8 m circles beside the line (y 37 - 46 behind the nose wheel), straight back; out straight forward. Water 0.60 m left, middle 28.60 m behind; toilet 0.85 m right, 28.32 m
  (changed from part A's 1.2 / 1.5: the panel is now over the platform where he can reach it). The panel points come from the aircraft as it stands (at most 22 cm off the place, else logged). The A320 has no modelled panels.
- **Work, in stages** (a relief goes on from the last finished): own chocks; water: steps and gate, coupling off its cup, on the panel with both hands, valve, pump, 240 s (200 l), back, reel button; toilet: tray down, cap off, rinse coupling hung on the rail, waste coupling out, lift by the held lever
  (73 cm), both on, down, drain lever 240 s, rinse 120 s, up again, all back. Hoses: `UCableComponent` without collision; reels and their turns follow.
- **Who waits for whom** (claimed when leaving the box, so no two wait at the stand): water or toilet, one at a time (`WetAt`); the toilet truck waits once the catering truck has its first trolley in at R1 until it has left R2, before that the catering truck waits at R1 for it. Given up: in its box after 40 min
  or when the aircraft is gone (logged, home). **Catering against trains at R2:** a train or belt loader already on its way (from any distance, new) goes first, later ones wait in `TrainApron` / `LoaderApron` while the truck is at R2; none of them waits for a truck at R1, so there is no ring; new: after 20 min at R1 the truck gives up R2 and
  goes home (`cat2` ends, logged); a train too late for its flight now turns back even while the truck is at R2. The flight itself waits 45 min at most, as before.
- **Plan tasks** `water` and `toilet` ask once the aircraft is chocked; they end when the hoses are stowed; the flight waits with the reason. People: role `water` (`b3D` true), one driver each (`EStaffDuty::WaterDriver`); fewer than two hired, other airlines, stands over 36 m: data with its time. Costs unchanged.
- **Yours:** E at both cab doors; the driver's seat not while the driver is at work; the fill point's door by E on the water truck. No glow.
- **Not there:** gloves, visor, apron (no garment); pump, lift, reel, vacuum sounds; the fill and dump work at a depot (30 s shut in its box instead); the mast's lower lever is not used; the cables may touch the truck.
- **Not checked:** everything in the game - reaches (coupling 20 cm over the floor, rinse coupling from the platform), lever turn signs, the walk under the tail, the U-turn beside a leaving cleaning van, the wipers' upper place (worked out), the single step of the cab (the rear handle is out of reach: left out).

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
