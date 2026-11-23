# Passengers and crew: every step of the chain (kept current)

*Started 23 November 2026. A real new career at Chablais, the Schengen side, the player's A320s. "Before" is what the audit of 2026-11-23
found; "now" is proved by the functional test (`Tools/new_game_test.sh --career`, Private/ChainTest.cpp: its stage in brackets) unless it says otherwise.*

## The player makes the flights

| Step | Before | Now | Where |
|---|---|---|---|
| Buy / lease A320s | **broken**: Fleet and Market changed the menus' own game only, never the simulation | works (1): new, leased, used, sold; on a stand at once | `ADPBridgeSim` buy / buyused / sell, `sim-schedule.ts` |
| Hire | worked for crew and ground; no check-in / gate staff to hire | works (1): passenger service agents and security officers too | `ADPStaff`, `sim-crew.ts` |
| Make the schedule | **missing** in the simulation (the menus' own game had one) | works (1): lines (aircraft, place, time, days), never two at once, curfew, range, runway | `Sim/ADPSchedule`, `sim-schedule.ts` |
| Bookings | **missing**: the load was drawn as the flight left (0-8 min ahead), so nobody could come for it | works (1-2): planned legs three days ahead, booked by the demand until 2 h 10 before | `ADPSchedule::Step`, `ADPDemand` |
| Demo / stand-in | strollers without a flight; a bought aircraft "deplaned" 150 people | none: no flight, no passengers (1) | `People.cpp`, `Turnaround.cpp` |

## A departing passenger

| Step | Before | Now | Where |
|---|---|---|---|
| Car park, at their own time (2 h to 1 h 15 before) | only for aircraft standing here 2 h; guessed "80 %" | works (2): who booked | `PaxFlow::Poll / SpawnDep` |
| Forecourt, crossing, entrance / revolving door | worked | works (2) | `WayToHall` |
| Check-in: own airline's desks, bag on the belt; hand luggage only (about 1 in 4): checked in on the phone, straight to security | worked, never staffed, never closed | works (2); needs an agent on shift; closes 45 min before (late: home, counted) | `JoinDesk`, `ADPSchedule::OnShift` |
| Security lane | worked, never staffed | works (2); needs a security officer on shift | `JoinLane`, `Screened` |
| Escalators, Floor 2, waiting | worked | works (2) | `WayUpstairs`, `WayToWait` |
| Boarding call | 40-60 min guess, not tied to a departure | works (2): its turn-round; 30 min before; needs an agent and the crew aboard | `CallBoarding`, `Turnaround` |
| Jetway, aboard (fades half way: no cabin yet) | worked | works (2) | `WayToJetway` |
| Missed / cancelled | stood there for ever | home: arrivals escalators / main entrance -> car park | `GoHome` |

## An arriving passenger

| Step | Before | Now | Where |
|---|---|---|---|
| Off the aircraft as it is docked | also from aircraft that never arrived; count guessed | (3): the flight's own passengers | `Turnaround` bArrival, `ArrPax` |
| Arrivals escalators, claim belt | worked | (3) | `WayArrivals` |
| Their own bag | fewer bags than passengers now and then, 15 min patience | (3): one for each, 45 min | `GSEArrivals`, `TagsOf` |
| Out, car park | worked | (3) | `WayFromHall` |

## The crew (the stand-in ground team; hiring = numbers and shifts)

| Step | Before | Now | Where |
|---|---|---|---|
| Flight crew rostered | at the push | an hour before; no crew: waits and says so | `ADPFlightsModule`, `ADPRoster` |
| One timeline | the turn-round never knew its departure (made as the aircraft "flew"): always "60 min" | the planned leg drives it; a delay moves it | `Turnaround` |
| Jetway out / back | out on arrival only | at on-blocks, or 75 min before for one standing here; back when all are aboard | `ASimTraffic::DockAt / UndockAt` |
| Ground power | on arrival only | on and off (2) | `TugCrew` |
| Chocks, fuel, catering, water, cleaning | simulated (no models: to-do 212) | simulated, timed | `Turnaround` |
| Departing bags: make-up, train, belt loader, hold | made up; the train never knew its stand in a real career | works with its stand 50 min before; two trains at once | `GSE*` |
| Arriving bags: hold, belt loader, train, conveyor | as above | (3) | `GSEArrivals` |
| Pushback: request, ATC, tug, 11 steps | after the aircraft had vanished from its stand for up to 8 min | the parked model gives way to the moving one in one frame; off blocks = pushed | `SimTraffic`, `Tugs` |
| Taxi, take-off, landing, taxi in | worked | (2, 3); an arrival is on blocks when its 3D model is | `SimTraffic` |
| Nobody on shift | work done anyway | the step waits; a notice says which and why | `ADPSchedule::MissingStaff / Tell` |

## Not there yet (other to-dos)

- All passengers visible: up to 36 departing / 30 arriving of a flight walk, the rest are numbers (to-do 205: layers of detail).
- Agents and officers as people at the desks, lanes and gates with document hand-overs (to-do 206, 210); staff count by shifts only.
- The cabin (to-do 202 / 209): passengers fade in the jetway; crew are data.
- Passport control, customs (terminal part 2a / to-do 212, 214): flights outside Schengen are marked in the list and pass without it.
- Trucks (catering, fuel, de-icing, water, toilets), chocks and cones (to-do 212).
- Gates 5-8, 10, 12 have no jetway (walk-out) and gates 13-16 no plan door: an A320 gets 9, 11, 1-4 first, then the walk-outs, 13-16 last (never with fewer than 13 aircraft here).
- The ground team is 16 people: about two turn-rounds at once; more flights wait for free crew.
