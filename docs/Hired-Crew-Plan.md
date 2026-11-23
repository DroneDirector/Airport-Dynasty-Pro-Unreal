# Hired crew - your staff are the people in the 3D airport (to-do 206)

Part 1 (built 2026-11-23, not run): the people and the pool - `StaffPeople.h/.cpp`, switch `adp.HiredCrew` (default 0 = as
before), `adp.CrewList` dumps everyone. Part 2a (built, not run): tug, pushback and ground power. Part 2b: bags, loader. Part 3: the hiring UI.

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
Switch `adp.HiredCrew` still 0; `TugStaff.cpp` (new), the queue in `StaffPeople.cpp`, one-line hooks in `Tugs.cpp`, `TugCrew.cpp`,
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

## Part 2b replaces
The GSE rows below (Borrow / GiveBack, trains, loader, GPU towed: same `AskTeam` / `Enlist` / `Discharge`), Turnaround's `bulk`
and the `Crew >= 2` before `Gs->Departure`; `GSEBags.cpp:338`'s count. Then the switch default 1. (Until then, with the switch
on, your own bags are still worked by the stand-ins.) E-talk with people who already work for you.

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

## Part 3: hiring in the Overview - what is there, what is missing
- **Screens** (Airline-Manager-Pro-Babylon/game/src/screens): `sim-crew.ts` - tabs (line 30-32; the `ground` tab = ground,
  fuel, catering, cleaning), hire tab with an advert per role (line 78), hire / dismiss / pay / interview (`sim-hire` line 84);
  built into www/game by `npm run build`. `apt-staff.ts` is the old web rota (its own staff, not the sim's).
- **Bridge:** UI/ADPBridgeSim.cpp: a person's JSON (line 41: status, home, shift), the actions hire / dismiss / pay / base /
  interview / advert / contract / dayoff (line 443-475); all work for any role.
- **Sim:** ADPStaff.cpp `BaseWage` (wages per role), `FillPool` (who can be hired), ADPGround `TeamNeeds`.
- **The newest example:** `checkin` "Passenger service": its own tab and advert, wage, pool entry, `ADPSchedule::OnShift` used
  by PaxFlow (lines 407, 508, 1609), "Hire them under Crew" in the notices.
- **Missing, per area:** ground - shift shown and changed (it is sent, no action sets it), which duties and how many a
  turn-round needs, "in 3D now: standing by / at work on ..." from `UStaffPeople`, a face picture, a warning for hours with
  too few; fuel / catering / cleaning - hireable, no 3D job; check-in and security - hireable, their 3D people are 210 / 211;
  `gate`, `bus`, `ops`, `tower` - no pool entry, no advert; People's "do you want to work for me" (People.cpp:310) makes no
  real `FADPPerson` yet (it should, with that face).
