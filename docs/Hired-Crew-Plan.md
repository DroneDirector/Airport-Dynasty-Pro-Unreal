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

## Passenger stairs, part A as built (to-do 212, 2026-11-24): model, blueprint, parking place - no code yet
Model `Art/PassengerStairs` (make_passengerstairs.py, own textures make_textures.py), blueprint `docs/Blueprints/Apron/PassengerStairs.*` (`Tools/make_passengerstairs_blueprint.py` - its Blender half drives every moving part: the reference
for part B), riders `Data/passengerstairs-riders.json`, every point `Art/PassengerStairs/passengerstairs-parts.json` (`points_cm`, `stairs`). 5.97 x 2.00 (2.47 over the mirrors) x 3.43 m; x -298.4 .. 299.0, axles +90 / -145, wheelbase 235,
tyre radius 32, HalfW 101 (124 mirrors), HalfL 299, TurnR about 450 (35 deg is clear), the front corner sweeps 7.1 m. Aircraft's side = the front (+x), the foot of the stairs = the back.
**What part B (driving, crew, IK) must know**
- **Place:** `ABabylonModel::Place("PassengerStairs")`. Wheels FL FR RL RR (single), steering wheel, pedals, wipers, doors (hinge in front, 68 deg), seats (2), names as the water truck. **One** step `IK_Step_DoorDriver` = `_Low` (27 cm), floor 48. Beacon 2.15 m; the platform rails are the top (3.43 m down).
- **Stair height:** n = treads up, 0 .. 10 (any fraction). Everything of the upper flight moves by n x (28, 0, 19) cm along one slope of 34.16 deg: `StairUp` (treads, plates, rails, platform, bumper), `StepLights_Up`, `GateSlide_L/R`, `NamePanel_Plat_L/R` and their points
  (`IK_TreadUp_*`, `IK_Rail_Up_*`, `IK_Rail_Plat_*`, `IK_Handle_Gate_*`, `IK_Stand_Gate_*`, `IK_Plat*`, `IK_LiftTop_*`, `IK_Name_Plat_*`). Platform = 232 + 19 n: **3.46 m n = 6 (L2), 3.84 m n = 8 (L1)** = whole treads, 3 cm under our A320's sills; real 3.4 m: n = 5.5.
- **Folding treads:** `TreadLow_02` .. `_10` (about y through `PIV_TreadLow_*`, modelled folded): open = +34.16 deg about Blender y (nosing UP), folded while `k > 1 + floor(n)`; 0 and 1 are fixed. Let them open over the last / first 3 cm of travel.
- **Lift:** `Lift_L/R_0..3` = 4-stage cylinders from `PIV_Lift_*` (box) to `IK_LiftTop_*`: turn all four to the new line, stage i out by i/3 of the lengthening (1.85 -> 5.2 m).
- **Jacks** `Stab_FL/FR/RL/RR` down 14.1 cm: first thing at the stand, up last; nobody on the stairs before. **Gates** `GateSlide_L/R` forward: 10.8 cm reaches the bumper's plane, up to 40 where the skin runs away (L2 is turned 11 deg); opened by hand at `IK_Handle_Gate_*` from `IK_Stand_Gate_*`.
- **Cab levers** `Lever_StairLever` (yellow, up / down), `Lever_LegLever` (blue), `Lever_GearSelect` (PIVs, tip about 20 deg), `IK_CabEStop`, `Needle_Speed` / `Needle_Battery` (about x through their PIV; the dials run over 270 deg, clockwise for the driver, 0-30 km/h and 0-100 %; modelled at 0 km/h and 72 %), `IK_Screen_Stair` (run-time text over the drawn screen). Platform buttons `IK_Plat_Btn0/1`, stops `IK_PlatEStop`, `IK_GroundEStop`.
- **Walking:** treads are real solid plates (rise 19, going 28, clear 127.7 wide); lower nosing line z = 19.8 + (x + 275) x 0.67857 cm, upper = the same + 3.2 (moved by n). Feet `IK_TreadLow_00..10` (visible while k <= floor(n) + 1), `IK_TreadUp_00..09`, `IK_Plat_Nose/Mid/Front/Agent`, on the ground `IK_Foot` (55 cm behind).
  The step onto the upper flight is 3 .. 22 cm, never more than a lower tread + 3. **Hands:** rails at 98.5 cm over the nosings, lower at |y| 72.2, upper at 65.5; both run past each other for at least 45 cm, so the hand changes tube without letting go; `IK_Rail_Low/Up_L/R_0..5`.
- **Names:** `NamePanel_L/R` (96 x 20), `NamePanel_Plat_L/R` (76 x 28), `NamePanel_Front` (110 x 11). Livery paints `PS_Livery_Body_Paint`, `PS_Livery_Main_Paint` (teal). Lamps: `LowBeam/HighBeam/IndicatorF_L/R_Lens` are the emitters in real reflectors behind a clear cover, rear `TailBrake/IndicatorR/Reverse_L/R_Lens`, `RearFog_Lens`, `BeaconLens`, `SideMarker_*_Lens`; `StepLights_Low/Up` glow by material (no real light).
- **At the A320** (sills measured for the catering truck): L1 middle 0.30 m ahead of the nose wheel, skin 2.0 m out: origin **6.19 m left**; L2 24.77 m behind, skin 1.57 m out, turned 11 deg: origin **5.20 m left**. Nose to the aircraft, bumper 1.95 + 0.28 n m ahead of the origin (the cab's front stays 1.20 / 0.64 m off). In low (n = 0), raise, then the last 30 cm.
  Stands without a jetway: 10, 12, 5 - 8; both doors there, with a jetway only L2. One vehicle is in the plan: two are needed for both doors (a second place: 6 m further north is too near the road; B decides).
- **Parking place (chosen, not drawn yet):** the EAST end, middle **(557.9, 140.0)**, facing north, box x 556.6..559.2, y 136.8..143.2; 0.7 m mirror to mirror off the toilet truck's way out (x 554.6), clear of its way in (564.2); loop in at x 566.9, round at y 155..148, swept 0.5 m inside the apron edge (570). Nothing is drawn or parked there. On paper.
- **Not there:** canopy; sounds (electric drive, hydraulic, jacks) not checked for; nothing run in the game. The paint's orange peel and the lens pillows are our own normal maps; the lamps' clear covers need the game's glass material.

## Passenger stairs, part B as built (to-do 212, 2026-11-24, compiled, NOT run)
- **Code:** `GSEStairs.cpp/.h`, table lines in `GSEService.cpp` (`bStairs`; `bRoof` = the model `PassengerStairsRoof`, its line and box appear only when `/Game/Babylon/PassengerStairsRoof/passengerstairsroof/StaticMeshes/StairUp` exists; same points file, same part names). Boxes: open (557.9, 140), roofed (551.0, 140), the toilet truck's way out (554.6) between them - on paper. Rain or snow: the roofed one first; dry: the open one.
- **When:** only stands with `jetway` false and not cargo (`SvcFor`); a gate with a jetway never. There is no "jetway out of order" state in the game, so no fallback for one. Remote stands R0-R7 work as soon as they are in `airfield.json` (one `x` a stand: `StandGeo`).
- **People:** role "ground" (Ground crew member), duties `StairsDriver` + `StairsHelper`, task `Stairs`; relief = the ordinary ground relief. Nobody free: the flight waits, reason shown.
- **Work:** asked at the "bridge" task (and before boarding); down the left lane (20.43 m), stop 1.5 m short, helper out, chocks in + nothing moving, creep in, jacks by the blue lever, up by the yellow one, the helper up the treads opens the gates (then stands at `IK_Plat_Agent`): ready. "doors" sends them away: gates, helper down, stairs, jacks, back out. "bridge" waits at most 30 min for them, "doors" / off-blocks until they have left; patience 45 min (standing open does not count). Interlocks: no raising without jacks, none with gates out, none with anybody on it, no driving unless stowed.
- **The sill:** `Data/aircraft-doors.json` (A320 L1 / L2 + the model's bounds when measured); the game measures the bounds of the model standing there and moves the doors by the ratio per axis - a rescaled A320 needs no edit.
- **API for walkers:** `AGSE::StairsReady(StandX, Door, Way)` (foot, treads, platform, door, direction), `StairsPose(Ti, up, K, PH, P)` over `StairsSeconds(Ti)` (feet on the real treads, right hand on the rail, keep right so two directions pass), `StairsEnter(+1 / -1)`. Only the helper walks them today: PaxFlow has no way over the apron yet (walkers of a stand without a jetway still end at the gate), no cabin.
- **Two vehicles:** then the rear left door gets one too (never before a waiting front door; the flight waits for the front one only to arrive, for both to leave). With one: front only.
- **You:** doors, seats, drive as the others; E at the ground panel (right side, `IK_Stand_GroundEStop`): legs + up to the aircraft in front / down again. On foot: solid ramps (34.16 deg) that follow the flights, rails, platform; its front shut until the gates are out.
- **Missing:** passengers and cabin crew walking them, levers worked by you from the seat, the run-time screen text, battery needle, hydraulic / jack sounds, tread lights on a switch.

## Passenger stairs, roofed version as built (to-do 212, 2026-11-24): model, blueprint, imported - not run
- **Model** `Art/PassengerStairsRoof` (`make_passengerstairsroof.py` runs the open one's script with `ROOF`; the roof alone is `roof_parts.py`), blueprint `docs/Blueprints/Apron/PassengerStairsRoof.*` (`Tools/make_passengerstairs_blueprint.py roof`). Same part names and the same points (the script stops if they differ: the game reads `passengerstairs-riders.json` for both), no code touched.
- **Roof:** 8 hoops (30 mm, on 12 x 40 straps bolted to the side plates outside the hand rails) a flight, smoked polycarbonate 4 mm (`PS_SmokedRoof_Glass`: the game's vehicle glass), screwed aluminium strips, gutters, rubber lips. The upper roof runs inside the lower one. Hood over the platform on its side walls + visor, open to the aircraft; LED strips are in `StepLights_Up / _Low`.
- **Parts:** frame in `StairUp` / `StairLow`, panes `StairUp_Glass` / `StairLow_Glass` (own parts, so the flights keep Nanite; the name moves them with the flight).
- **Numbers:** clear 2.05 m or more over every nosing and the platform across 1.2 m (2.24 in the middle); roofs 22 mm apart at the nearest over the whole range, overlap 0.48 m at 4.22. Hood's side walls end 48 cm behind the bumper, the visor 12 cm (underside 2.12 over the platform, over the door's top). 5.97 x 2.00 (2.47 mirrors) x **4.60 m when low**: not checked against what it drives under (jetways, pier). Highest 6.5 m.
- **After every import:** `Tools/share_model_materials.py PassengerStairsRoof PassengerStairs` (its meshes use the open one's materials, its 370 MB of copies go; the open one first).
- **Both:** the front bumper's and the gates' stripes are thin plates with plain sides (no yellow shreds on the edge).

## De-icing truck, part A as built (to-do 212, 2026-11-24): model, blueprint, parking place - no code yet
Model `Art/Deicer` (make_deicer.py + deicer_checks.py = every sweep and the pose matrices, own textures make_textures.py), blueprint `docs/Blueprints/Apron/Deicer.*` (`Tools/make_deicer_blueprint.py`, also the reach search),
riders `Data/deicer-riders.json`, every point `Art/Deicer/deicer-parts.json` (`points_cm`, `boom`, `chain`, `cabin`, `a320_reach`). 9.95 x 2.40 (2.92 over the mirrors) x 3.40 m; x -494.7 .. 500.3, axles +350 / -155, wheelbase 505, tyre radius 48, twin rear (`Tyre_RL_In`).
**What part B must know**
- **Place:** `ABabylonModel::Place("Deicer")`. Wheels, steering wheel, pedals, wipers, doors (hinge in front, 68 deg), seats (2) and names as the refueller; HalfL 503, HalfW 121 (146 mirrors), steer 38 deg clear, TurnR about 680. Cab floor 110, two steps a side (`IK_Step_Door*_Low` 47, `IK_Step_Door*` 80).
  `Needle_Speed` (0-100 km/h, at 0) / `Needle_Fuel` (modelled at 72 %), dials 270 deg; `IK_Screen_Dash`, `Lever_GearSelect`, `Lever_PtoLever`.
- **Superstructure (stowed = pointing BACK over the tanks, `pose()` in deicer_checks.py is the reference):** slew about z through `PIV_Slew` (215, 0): `Tur`, `LiftBarrel`, `LiftRod` and all below. Lift 0 .. 60 deg about Blender +y through `PIV_BoomLift` (245, 0, 286): `BoomBase`; `BoomSec2` + half, `BoomSec3` + whole extension (0 .. 6 m, along the boom), `LiftBarrel` / `LiftRod`
  aimed `PIV_Lift` - `PIV_LiftEye` (1.07 -> 1.72 m, the eye turns with the boom). The cabin (`Cabin`, `Cabin_*`, `CabinDoor*`, `CabinWiper*`, `CabinLamp*`, `Joy_*`, `SprayPedal`, `Noz*`, `HoseNozzle`) moves by the tip's shift `PIV_LevelTip` and slews, never tilts. **Never below 15 deg of lift away from the rest; lift straight up 15 deg first, last move straight down.**
- **Hose chain:** `ChainLink_00..69` (own origins; base group, not the whole boom): pitch 9.5, half loop 10.5; with extension e (cm) the loop's x = 55 - e / 2, lower run from x -245 (z 315) to the loop, round, back on z 336 to x = -275 - e; place link i at path length 9.5 (i + 0.5) (pitch turns by -s / r in the loop). `HoseLift`, `HoseTip`, `HoseNozzle` = stowed slack pieces: hide, cable `IK_*_A` - `IK_*_B`.
- **Cabin:** up the rear left `IK_Step_Deck_0..2` (hands `IK_Grab_DeckSteps_*`), `IK_Deck_Door`, `CabinDoor` slides 56 cm towards the boom (`IK_Handle_CabinDoor_*`), sill `IK_CabinStep` 110, floor 107, `IK_Seat_CabinSeat` (looks outwards = back when stowed), `IK_Joy_L/R` (stick +-18 deg), `IK_Foot_Cabin_*`, `SprayPedal`, `IK_Cabin_Screen`, buttons, stop, `CabinWiper` (about x, -40 .. 60), four work lamps, beacon, horn. **Both people get in only while it is stowed.**
- **Nozzles:** `NozPan` + `NozTilt` about z (+-60), `NozTilt` about y (-25 .. 45 checked); spray from `IK_Nozzle_I` (right) / `IK_Nozzle_IV`, along Blender -x when stowed; nozzle 2.05 m ahead of and 1.69 m under the tip.
- **Pump panel** (`PanelDoorL`, hinge at the back, 110): `Valve_*`, `GaugeNeedle_*`, `IK_Button_*`, `CoupCap_W/I/IV` + `IK_FillCoupling_*`. **Hand hose** (`PanelDoorR`): `HoseReel`, `HoseTurn_HoseReel_*` 18.8 m, `HoseTail`, `HandNozzle` (`IK_HandNozzle_Hand/Hose/Mouth`), out at `IK_HoseOut`. `HeaterDoor` 110. Caps `FillCap_W/I/IV` (about y, 100). Sight fluid is fixed. `IK_ReverseHorn`.
- **At the A320 (searched on published sizes, not on our model):** every sample of wing, tailplane, fin and fuselage top is reached, nozzle 2 - 6 m off, cabin >= 1.9 m and boom >= 1.0 m clear. Right side stops (x aft of the nose, y right, heading): 3.2/6.8/0, 9.0/11.0/45, 13.2/17.2/62, 19.5/21.6/20, 25.0/17.0/-62, 24.3/10.5/-60, 29.0/9.6/-30, 33.5/10.0/0, 38.5/8.2/-40; the poses per stop are in
  `a320_reach`; left side mirrored. It needs 23 m beside the aircraft: B checks the neighbour stand (or sends the truck round ahead of / behind the tip) and drives the curve between the stops.
- **Parking place (chosen on paper, not drawn):** EAST end, middle **(561.5, 101.5)**, facing north, box x 559.8..563.2, y 96..107: east of the toilet truck's box, south of its loop; way out north on x 561.5 (0.9 m past the stairs' mirrors). No loop fits (apron edge 570): it backs in from the north. B checks it in `Apron()`.
- **Not there:** outriggers (the type has none), moving tank levels, sounds not checked for, spray / heater smoke; nothing run in the game.

## De-icing truck, part B as built (to-do 212, 2026-11-24, compiled, NOT run)
- **Code:** `GSEDeicer.cpp/.h`, table line in `GSEService.cpp` (`bDeicer`); box (561.5, 101.5), home: from the west on y 122.5 between the toilet truck's box and the stairs', round to north, straight back in. Role "ground", duties `DeicerDriver` + `DeicerOperator`, task `Deice`.
- **When:** `DeiceTick` from `Turnaround.cpp` for your own departures at the A320 stands with people hired, from 10 min before the doors: `DiWeather` (the report + APuddles): falling and +3 C or less, snow on the paving, freezing fog, frost (0 C or less and the dew point within 2.5). It goes in when "doors" is done (shut, jetway back); the flight waits (`SvcWaits`, at most 135 min in all). Holdover 20-60 min by what falls (x 0.6 under -14 C); over while it still stands: again, three rounds at most.
- **Way:** NOT the stops of `a320_reach` (no truck of 6.8 m turning circle drives between them): straight lanes, in forwards, out backwards: 20.43 m beside each wing (turret 25.5 and 13.5 m behind the nose), 12.6 m beside the tail (34, 28.5, 25.8). Left wing, tail left, tail right, right wing. Boom stowed before every move. Places and poses are solved at the stop (published A320 scaled by the bounds of the aircraft standing there; cabin 1.0 m, boom 0.75 m clear, spheres against jetway and buildings). Checked on paper with the same formulas: every place of wing, tailplane and fin reached, routes clear.
- **A lane not free:** another vehicle 15 min, the neighbour's wingtip 5 min, something fixed 30 s - then that lane is left out with the reason in the log.
- **Two-step:** type I at every place of a stop, then type IV while something falls. 110 / 45 l a minute, water mixed by the cold; tanks 2600 / 2400 / 2400 l, filled up in its box (`bUsed`) when low; no money (no service has a price yet).
- **You:** the driver's seat as the others; E at the rear steps: the spray cabin (parked, nobody at work): move keys lift / slew, Shift / Ctrl (triggers) out / in, left / right mouse (RB / LB) type I / IV where you look, Home (pad Y) or E: stowed; out only when stowed.
- **Missing:** the fuselage's top ahead of the wing (snow: not reached from these lanes), wing wetness, spray and pump sounds (only the belt loader's hydraulics), pump panel, valves, gauge needles, hand hose, heater door, dash needles, levers; the truck seen refilling.

## Ambulift, part A as built (to-do 212, 2026-11-24): model, wheelchair, blueprint, parking place
Models `Art/Ambulift` (make_ambulift.py + ambulift_body.py + ambulift_checks.py; `pose()` there moves every part and is the reference for B) and `Art/Wheelchair` (make_wheelchair.py), blueprint `docs/Blueprints/Apron/Ambulift.*`,
riders `Data/ambulift-riders.json`, `Data/wheelchair-riders.json`, all points and numbers `ambulift-parts.json` (`lift`, `platform`, `tail`, `moves_with_body`, `points_cm`). 9.38 x 2.40 (2.92 mirrors) x 3.58 m; x -434 .. 504, axles +230 / -220, wheelbase 450, tyre 48, single wheels; HalfL 504, HalfW 121 (146), steer 38 deg clear, TurnR about 620.
**What part B must know** (Blender metres / degrees; y left)
- **Place:** `ABabylonModel::Place("Ambulift")`. Cab, doors (68 deg), seats, wheel, pedals, wipers, needles and names as the de-icer, 60 cm lower: one step `IK_Step_Door*` (32 cm), floor 50, cab roof 2.14 m. Electric (battery boxes); no exhaust.
- **Body:** every part of `moves_with_body` (prefixes, minus `not`) + points straight up by q 0 .. 2.92 (floor 1.28 .. 4.20). `t = asin((0.12 + q) / 4.60)`; centre x 2.35 - 2.30 cos t, z .97 + (.12 + q) / 2; `ScissorA_` turns +(t - t0), `ScissorB_` the other way about Blender y; `RamBarrel_L/R` (base `PIV_Ram_*`) aim at the eye on A (`lift.ram`), `RamRod_*` end on it.
- **Legs** `StabLeg_FL/FR/RL/RR` down .30: before the lift, up after. Lever `Lever_Stab` (stand `IK_Stand_Stab`, left outside).
- **Front platform** about y through `PIV_Plat` (2.82, 1.25 + q): folded 0 .. 90 level, **only at q >= 1.02**; parts `Platform`, `Bridge`, `RailPlat_*`, `GateSlide_*`. `PlatRamL/R_Barrel` / `_Rod` aim `PIV_PlatRam_*` - `PIV_PlatRamEye_*` (the eye turns with it). Then rails: LEFT first 90 about x through `PIV_RailPlat_L` (with `GateSlide_L`), then right; stowing the other way round. Bridge +x 0 .. .50, gates +x 0 .. .40 (each past the bumper, `IK_Plat_Nose` 4.93). Deck = floor height, so q = sill - 1.28: L1 3.87 q 2.59, L2 3.49 q 2.21. Origin 4.96 m + gap from the skin; cab front 5.04 is under it: the cab's nose reaches 11 cm past the bumper, low under the fuselage's side (on paper the round skin is about 1.1 m further in at the cab's 2.14 m roof; B measures it in the game).
- **Doors** `DoorFrontL/R`, `DoorRearL/R` slide .56 in y outwards (their `_Glass` too). Buttons `Button_DoorFront`, `_DoorFrontOut`, `_DoorRearIn`, `_DoorRearOut`.
- **Tail lift:** `TailArms` about y through `PIV_TailArms`, 0 .. -82 deg (floor .. ground; see `tail`); `TailPlatform` (with `TailRail_*`, `TailFlap`, rear name panel) follows the arms' tip and opens -90 about y through `PIV_TailPlatform`; rails LEFT first, then right, 90 about x (`PIV_TailRail_*`); `TailFlap` +90 stop, -12 ramp. Only with q = 0; stand it up before lifting.
- **Inside:** `IK_Seat_BoxSeat0..4` (look right; `IK_Foot_BoxSeat*_L/R`, `BoxSeatBelt*_Tongue`), bays `IK_Bay_L0/L1/R0/R1` (wheelchair origin, facing forward; straps `Strap_<bay>_<rail><end>` with `IK_` / `PIV_`, `BeltBay<bay>_Tongue`, attendant `IK_Attend_<bay>`; wheelchair hooks `IK_Strap_FL` ..), `Stretcher` (`IK_Stretcher_*`, `PIV_Stretcher`), `IK_Attend_Front`, `IK_Box_*`, handrails `IK_BoxRail_*`.
- **Controls, each its own part + PIV (to-do 238):** cab `Button_CabHazard/CabBlue/CabStop/CabBeacon/CabWork` (`IK_Button_*`), `Button_Horn` (`IK_Horn`), `Lever_GearSelect` (`PIV_GearSelect`), `Lever_PtoLever` (`PIV_PtoLever` = lift power), `Lever_ParkBrake` (`PIV_ParkBrake`), pedals, wheel; legs `Lever_Stab`, `Button_StabEStop`; cabin front left `Lever_Lift`, `Lever_Plat`, `Lever_Bridge` (about x through PIV, +-20), `Button_EStop`, `Button_Light`, `Button_Blue`, `Button_Heater`, `Button_Work`, `IK_BoxPanel_Screen`, stand `IK_BoxPanel_Stand`; cabin rear right `Button_InTailUp/InTailDown/InTailFold/InFlap/InTailStop` (`IK_RearPanel_Stand`); outside rear right `Button_TailUp/Down/Fold/Stop` (`IK_Tail_Control_Stand`); by hand: gates `IK_Handle_Gate_*`, rails `IK_Rail_Hand_*`, bridge `IK_Handle_Bridge`, platform `IK_Handle_TailPlatform`. Buttons press 4 mm. No `PIV_` for the seat belts, only the tongues' `IK_`.
- **Lights:** `BlueFront_F0..7 / R0..7_Lens`, `BlueRear_*`, `BlueGrille_L/R0..1_Lens` (emitters in reflectors behind `*_Cover_Glass`; units 0-3 / 4-7 for left-right flashing), `BeaconLens`, head / tail / indicator lenses as the others, `BoxLight*_Lens` inside, `BoxWork*_Lens`, `BoxCorner*_Lens`, `BoxSideMark*_Lens`. Glass materials: `AL_Glass`, `AL_BlueCover_Glass` (the game's glass material needed).
- **Names:** `NamePanel_L/R` (280 x 30, ride up), `NamePanel_Front`, `TailPlatform_NamePanel_Rear` (90 x 22). Livery `AL_Livery_Body_Paint`, `AL_Livery_Main_Paint` (blue).
- **Wheelchair:** 1.00 x .65 x .93; `Wheel_L/R`, `Castor_*`, `CastorFork_*` (z), `Footrest_*` (z), `FootPlate_*` (x, up 90), `Brake_*` (y), `Belt_*`, `Seat`, `Frame`; `IK_Seat`, `IK_Push_L/R`, `IK_Pusher`, `IK_Arm_*`, `IK_Foot_*`. Footrest lines to the plates: a footrest swings only with its plate.
- **Parking place (on paper, not drawn):** EAST end, middle **(550.6, 92.0)**, facing north, box x 548.9..552.3, y 86.5..97.5: 2.6 m off stand 16's edge (546.3), south-west of the toilet truck's box (553.2; 1.1 m mirror to box on the way out), 7.5 m off the de-icer's. Way in as the de-icer's: from the west on y 113, round to north (ends y 120, its nose 125, under the roofed stairs' box 136.8), straight back 28 m.
- **Not there:** weather hood (the types seen have none), aisle chair, side door (crew ride the tail lift), sounds not checked for; nothing run in the game.

## Ambulift, part B as built (to-do 212, 2026-11-24, compiled, NOT run): crew, wheelchairs, blue lights (you: see Vehicle controls)
- **Code:** `GSEAmbulift.cpp` (parts, interlocks, buttons, blue lamps, the two wheelchairs), `GSEAmbuliftWork.cpp` (calls, passengers, work), `GSEAmbulift.h`; table line in `GSEService.cpp` (`bAmbu`), box (550.6, 92) drawn from it, home as the de-icer (from the west on y 113, round to north, straight back).
- **Who:** role "ground" (`b3D` is true already), duties `AmbuDriver` + `AmbuHelper`, task `Ambulift`; two are always needed. Other airlines / nobody hired: nothing comes, as before.
- **When:** `AGSE::AmbuPax(flight, day, pax)`: 20 % of flights have one, 5 % two, always the same for a flight of a day. Arrival: asked when the chocks are in; "deplane" is not done until they are off. Departure: asked 15 min before boarding, after `cat2`; "board" (so "doors") is not done until they are aboard and the body is down. Nobody free: waits, reason in the messages and in `SvcWaits`; at most 45 min at work / 60 min over the task.
- **R1 only:** catering and ambulift keep each other out (`CaterAtFront`, `AmbuAtFront`): the caterers stay out of the lane while an arriving flight's ambulift is manned or there; if they were first, deplaning is done without the wheelchairs: they stay seated, the ambulift's 45 min do not run while it waits, and boarding (so the doors) and the push wait until they are off. **R2 is not used** (a train and the belt loader stand there).
- **Way:** the caterers' lane and bend to R1 (`CaterIn` / `CaterDock`, reach 499 cm = 3 cm off the skin), backed out the same way. The sill from the aircraft standing there (CaterDock's own L/R1 3.87 m through its transform; `aircraft-doors.json` is not read for R1).
- **Work (stages in `AmbuStage`):** legs by the lever, tail deck by the buttons outside, its rails by hand, the driver up (helper presses), rear doors, then the panel inside; the helper pushes the chair; platform / bridge / lift levers, front door button, rails and gates by hand; everything through `Want`, `AmbuMay` decides.
- **Passengers:** one of `PickBuilt("passenger")`, sat by the crew rig (pelvis, feet on the plates, hands on the arms). The vehicle's OWN wheelchair fetches them: nearest ramp room (`UStaffPeople::RampNear`, the staff door at x0 + 5, taken whole with `RampTake`), they get in / out behind its shut door and only while you are not in the room. Way over the apron: `AmbuRoute` (60 cm grid, vehicles' outlines, what blocks a pawn), stops for people and driving vehicles. No such door in the terminal: the ambulift is not called.
- **Aircraft:** no moving door, no cabin: hidden at the sill 14 s, as the caterers.
- **Lights:** `GBlue` (`AmbuBlue`): on the way, back, and while the body is up (and `bBlue`, for the switch); double flash, halves alternating; material only, no real light. `BlueCover` glass: the game's vehicle glass tinted blue (`VehicleDirt.cpp`).
- **Missing:** belts, floor straps, the chair's brakes, footrests swung away, the stretcher, the box seats, sounds, wipers (no table line yet), dash needles; the gates move together; the aisle turn and the ramp room's small circle are not checked against what stands there.

## Vehicle controls (238): the general layer, built 2026-11-24 (compiled, NOT run), used by the ambulift
- `VehicleControls.h/.cpp` (AVehicleControls): look, prompt or the reason it can't, E / X (held: the motion runs; W / S or the left stick choose a lever's way), hand IK (walker `SetArm`, seat `AServiceDrive::SetReach`), only the lever / button glows, crew's `Job` keeps it, solid pieces that carry you, left open and unattended 2 min: stows itself.
**How to add the next vehicle's controls**
1. In its own file write `TSharedPtr<FVehTable> AGSE::<Vehicle>Controls()` (declare in `GSE.h` beside `AmbuControls`), add its line in `AGSE::CtlTable`. Pattern: the end of `GSEAmbulift.cpp`.
2. `Controls`: one line each - part, hand point (`Data/*-riders.json`), kind Push / Hold / Lever / Hand, label, the piece it rides on, the way it faces, motion with A..B (or a Switch), `bSeat` for the cab.
3. `Frame`: where each piece is now (the same maths as its parts). `Now / Want / May`: the crew's own state, wanted value and interlock; a text for every no. `Press`: its buttons. `Switch`: lights (`bSwHaz`, `bSwWork` ... on FTractor work for all), horn, stop.
4. `Solids`: boxes per piece with their condition, floors flush; `Aboard`: which floor your feet are on (then `YouOn` blocks driving; use `YouOn` / `YouAt` in May). `Stowed`, `Stow`.
5. `Worked`: called when you bring a motion to its end - call what its crew's job calls, so it counts for the turn-round. Refuse a job while it is not stowed (SvcTick, as the ambulift).
- Ambulift, worked: legs, body, front platform, bridge, its rails and gates, tail lift (both panels), tail rails, front and rear doors (in / out), stops, cabin / work / blue lights, hazard, beacon, horn (the electric tractor's `BTHorn`). Flap: by itself.
- No function yet, not faked: `Button_Heater`, `Button_InFlap`, `Lever_PtoLever`, `Lever_GearSelect`, `Lever_ParkBrake`, wipers and headlights (no control part), seat belts, straps, bridge handle, stretcher, box seats, the wheelchairs (so nothing of yours counts for the turn-round yet).

## Vehicle controls (238): fuel truck and water truck, built 2026-11-24 (compiled, NOT run)
- `AGSE::FuelControls()` (end of GSEFuel.cpp), `AGSE::WaterControls()` (end of GSEWater.cpp): they set only the wants, modes and flags the crew's jobs set, so limits and speeds are the crew's.
- New hooks in `FVehTable`: `Say` (empty: hidden), `MaySwitch` (why not, before the press), `Held / Leash / Drop` (a thing in your hand, its hose's length), `Climb` (the crew's own steps animation).
- Carried: nozzle, bonding clamp, chocks, coupling: E at its place takes it, E at the aircraft (or its place) puts it. One at a time; order: chocks, clamp, nozzle; back the other way.
- Fuel flows only with clamp, nozzle, left hose and pump levers, no stop pressed, dead man held. Water: coupling, valve, pump.
- Your litres come off the aircraft's own job; all back in place = job done if it got all, else a crew does the rest.
- Nobody drives while not everything is back (`FuelStowed / WaterStowed`, seats refuse); crews do not take your truck, nor an aircraft you work on (`YouServe`). Left alone it stows as the ambulift does.
- Standing on a crew's vehicle that waits for you: "waits: you are standing on it" (every vehicle with a table).
- Fuel truck, the rest (compiled, NOT run): both hoses are an `FSvcHose` (`F` the left, `F.Rt` the right); one nozzle on the aircraft at a time, each needs its own lever.
- Hose and cable you took or pulled stay out (`FSvcPay`) until wound in by hand at the reel (hold E); then nozzle/clamp in place = stowed.
- Dead man: take it off its hook, 10 m lead; squeeze = hold E looking at nothing (`HandSay/HandDo`); times out at 120 s until let go; crew re-squeeze at 110 s.
- Meter: start, stop, preset (asked amount, 500 to 10000, off), counter; ticket prints at stop or at the printer, take it, pocket it. Crew press preset, start, stop too.
- Defuel lever + start: takes back only what you pumped into that aircraft (aircraft have no fuel quantity). Tank 20,000 l, refilled as before by the box.
- Extinguishers: take, carry, put back. `FuelStowed`, `Drop`, `Stow`, `Leash` cover all of it.
- Not in the model, so not built: fill coupling and lever, charge flap, ticket button; manhole lids are merged with the tank fittings and there is no way up; no fuel depot exists. Button FLOW works the work lights.
- Still open: gear selector, park brake, cab switches, glove box.
- Water truck, the rest (compiled, NOT run): hose pulled off by hand at its guide, wound in by the rewind button (hold E, `FSvcPay`); fill lever behind its door (turns about z, 70 deg outwards), tank 2500 l, filled in its box only, 30 s as its crew's.
- Pump buttons' prompts show litres given and tank (50 l/min, stops when empty); fill valve, coupling, pump and emergency stop lock each other; `WaterStowed` / `Stow` cover hose, valve, door.
- Body boxes measured on the GLB; the steps are solid treads and count as standing on it.
- Water truck, merged in the model, so not built: gauges, LCD, sinking buttons, fill cap, glove box, manholes, charge flap. FLOW on the fuel truck: wrong word on the work-light button (make_refueller.py 329), model job.
- Toilet truck (compiled, NOT run): `AGSE::ToiletControls()` + `YouToilet` (end of GSEWater.cpp). Order: chocks; rinse coupling on the rail; up, tray down, cap off (laid on the floor), waste coupling, gate shut, raise, waste on, rinse on; drain lever (240 s), rinse pump (120 s); back in reverse.
- Platform lever on the platform and at the mast (8 cm/s, the crew's height limit at the aircraft); not with the gate open, the stop pressed, you under it (crew too) or your head at the aircraft (`WetLiftHeld`).
- Tanks: rinse 550 l, waste 1500 l (168 + 40 l per aircraft). Emptying lever behind the mast, from the lowered platform, in its box only: 30 s, refills rinse too.
- A crew taking over drains and rinses only what you left (`J.Litres` 360 s); a lever worked with something in your hand: it goes to the left hand.
- Toilet truck, merged or not in the model, so not built: rinse fill cap, gauges, sinking buttons, coupling cam levers, charge flap, manhole; no work-light button, no lockers.
- Catering truck (compiled, NOT run): `AGSE::CaterControls()` + `YouCater` (end of GSECater.cpp). Order: stabilisers, body, front roll-up door, platform, flap, right then left rail, gate; back the other way round; side door, steps, rear door, tail lift, both stops.
- At an aircraft that waits for catering the body stops level with its sill (line traces stop body and platform at the fuselage). Nothing moves under a person (`CaterHeld`, `CaterUnder`).
- Trolleys: take by either end (brake off), push at up to 90 cm/s only where `CaterFree` has room; trigger at the sill hands the full one in (4 front / 7 rear, two in the cabin at once), the empty one comes out after 14 s, back to its place = one exchange (`J.CtYou`). The crew do only the rest (`CtTripsLeft`).
- `CaterStowed` keeps the seat shut; left alone open it puts itself back in the crew's order, loose trolleys straight to their places.
- Catering truck, merged or not in the model, so not built: chocks, cones, inside light, straps and nets, sinking tail-lift buttons; galley boxes and trolley doors have no function; trolleys not onto the tail lift; one arm on the handle.
- Cab controls, all eight service vehicles (compiled, NOT run): `CabControls.h/.cpp`; `CabAdd` adds the same seat lines to every table, shown where the model has the point. Codes above `ADPCab::First` go to `CabSay/CabMay/CabSwitch`.
- Keys: Space / A park brake, Page Up / Down or B + A gear, L / D-pad down beam, comma / full stop or B + D-pad indicators, F or B + D-pad down hazards, N / R3 beacon, H / L3 horn, J or B + R3 work lights, K or B + L3 wipers, U PTO.
- Park brake or neutral or PTO on: no drive. PTO (ambulift, de-icer) needs brake and neutral; legs, body, platform, tail, boom and spray need it from you. Getting out puts the brake on.
- Hired drivers: `CabTick` (brake, gear, beacon, lights back to normal); indicators as they steer (`Lamps`).
- Merged (hand goes there, nothing moves): gear and park brake of catering, fuel, water, toilet; van gear; stairs and de-icer park brake; stalks; horn hub. Levers that move: ambulift three, de-icer gear + PTO, stairs gear.
- No part (key only): van park brake; hazard, beacon, work-light, headlight switches (ambulift has three buttons). Merged, not built: sun visors, glove boxes, heater knobs, window switches. Only lens glow: service vehicles have no light beams.

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
