# One departure, start to finish: the plan

*Airport Dynasty Pro, 22 November 2026. The example is an A320 (150–180 seats), short-haul, at Chablais (Schengen). **T** = off-blocks, the moment the push starts. Times are typical real-world values; each one comes from a data table, so it can be tuned per airline, aircraft and airport.*

## How it is set up: one clock per flight

- **The flight timeline.** Every flight at your airport gets ONE timeline. This is today's turnaround plan (Turnaround.cpp, Airbus AC 5-2), extended forward to the landside and the crew. Every step has a planned time and the steps it waits for.
- **Who listens to it:** the passengers (PaxFlow), the check-in agents, the gate agents, the cabin and flight crew, the ground crew (ramp), the vehicles (tug, GPU, belt loader, carts, catering, fuel, de-icing), the jetway, the aircraft and its doors, the screens and the announcements. Each one only reacts to the step that concerns it.
- **Delays spread by themselves.** When a step runs late (a late passenger, a slow load, de-icing), the steps that wait for it move with it, and so does the flight's departure time on the boards. That is the same chain as today's bag and boarding holds, applied to everything.
- **Real numbers in tables:** opening and closing times, boarding rates, door speeds, crew briefing times, holdover times. They come from Airbus, airline and handler standards and are kept in one data file.

## The staff, and who does what

| Role | Name to hire | Works | Today |
|---|---|---|---|
| Check-in, bag drop, gate and boarding | **Passenger Service Agent** (new hiring role) | desks, then the gate | none, to-do 210 |
| Ramp: tug, GPU, belt loader, bags, carts, wing walking | Ramp Agent (ground staff) | apron, bag hall | stand-in team |
| Jetway operator | Ramp Agent (jetway-trained) | jetway cab | to-do 209 |
| Cabin crew, flight crew | Cabin crew / Captain / First officer | aircraft | data only, to-do 202 |
| Cleaners, caterers, fuellers, de-icers, security screeners | their own roles (already in the staff pool) | as below | simulated |

## Step by step

### Landside: before T−180
1. **Passenger service agents arrive** for the flight's desks about 20 minutes before check-in opens (staff entrance, then the desks): they log in and test the bag-tag and boarding-pass printers. Desk screens show the airline and flight.
2. **Cabin and flight crew report** at the crew room about 75–60 minutes before T (to-do 116: their commute; to-do 202). They brief, the flight crew get the weather and the flight plan, and they walk airside through the staff security check.

### Check-in: T−150 to T−45
3. **Check-in opens** at T−150 (long-haul T−180). The number of desks follows the load: about 1 per 40–50 passengers, more when queues grow.
4. **Passengers arrive.** Most come between T−120 and T−60, a few later. Today they come from the car park; later by car, bus and train. *Missing today:* about half have **checked in online** and go straight to **bag drop**, or with hand luggage only straight to security. Some families and PRM passengers (reduced mobility, wheelchair) get help.
5. **At the desk** (about 1.5–3 minutes per passenger): ID and booking checked, the bag weighed and tagged, the boarding pass printed and handed over (a hand-over animation). The bag goes down the belt. That is already built.
6. **Check-in closes at T−45** (short haul, 40–60 min). The desk sign changes to CLOSED; those already in the queue are served. **Latecomers go to the airline's service desk** (rebooking, or refused): no bag flies without its passenger. The agents then walk to the gate.

### Security and airside
7. **Security:** the **boarding pass is scanned at the entrance** (a gate with glass flaps). *Missing today:* the scan, the tray (liquids out, laptops out, coat and belt off), and the random explosive trace check. About 6–10 minutes in total.
8. *Missing (a real rule):* **flights outside Schengen** (UK, Turkey and others) pass **border exit control** (passport check) on the way to non-Schengen gates. Chablais flights to Schengen don't.
9. **Floor 2: airside.** Passengers can't go back landside. Their own gate shows on the screens from about T−60; until then they shop, eat, use the toilets, sit or lean.
10. **The gate opens** at about T−40 ("Go to gate" on the screens). Passengers drift towards their gate and wait there. They may still leave briefly for the toilet or a shop and come back. The gate agents (the same passenger service agents) set up at the gate desk.

### The aircraft: arrival and turnaround (already mostly built)
11. **On blocks**, about 45–60 minutes before T. The jetway is docked by the operator, and **the cabin crew open L1 from inside** (to-do 209).
12. **Deplaning** (about 20 passengers a minute through L1) to the claim belt. The GPU is plugged in, then chocks and cones.
13. **Ramp work, in parallel:** arriving bags off, departure bags on (built); **catering** at R1/R2 (truck, trolleys, to-do); **cleaning crew** in the cabin (to-do); **potable water and toilet** service (simulated); **fuel** (truck, real litres and minutes, to-do).
    - *A real rule:* boarding during fuelling is only allowed with precautions (fire services informed, a door with stairs or the jetway kept clear, the seatbelt signs off). Otherwise boarding waits for fuelling to finish.
14. **The cabin crew check the cabin** before any passenger boards. This is the safety equipment check (about 10 minutes) and the **cabin security search** (the first flight of the day, or after the aircraft was left alone). The cleaners must be finished first. Then the purser tells the gate "ready for boarding".
15. **The flight crew:** the walkaround outside (the captain or first officer walks round the aircraft), then cockpit preparation.

### Boarding: T−30 to T−10
16. **The boarding call** (already built): PRM passengers and families with young children first, then by group or zone.
17. **At the gate desk:** the **boarding pass is scanned** (and ID checked), a beep, then into the jetway.
    - *Missing today:* the scan animation, and a gate agent standing there.
    - *Built:* passengers walk the jetway and fade halfway. When the cabin exists (to-do 209), they walk to their own seat, put bags in the bins (some), and sit.
    - Real rate: about 12 passengers a minute (built).
18. **Final call** at about T−15, and passengers paged by name. They hurry or run (built).
19. **The gate closes** at about T−10. **No-shows:** their bags must come off the aircraft before departure (a real rule: passenger and bag reconciliation). The ramp crew find and unload the bag, which delays the flight. Today the flight waits up to 15 minutes, then goes, but the bag offload is still *missing*.

### Doors, push, taxi, take-off: T−10 to about T+15
20. **Load control:** the final load sheet (passengers, bags, cargo, fuel, trim) goes to the captain, who signs. *Missing today*, as a short real step.
21. **The doors close:** cabin crew close L1, the cargo doors are closed by the ramp crew (to-do 209), and the bulk hold too.
22. **The jetway pulls back** to its rest position (built), and **the GPU is unplugged** once the APU runs (built).
23. **De-icing if needed:** on the stand, or on a remote pad after the push. The fluid type and **holdover time** follow the real weather, and take-off must happen within the holdover time or it is done again. The trucks are *missing*; today it is simulated.
24. **Pushback:** ATC clearance requested and approved, the tug comes out, couples, the 11 steps (built).
25. **Engine start** during or after the push, engine 2 then engine 1 (built). The tug uncouples, the bypass pin is shown and the crew are clear (built).
26. **Taxi** on the centre lines (built), the holding point, the line-up clearance, then the **take-off clearance** and **take-off**.
27. **After take-off:** the aircraft keeps flying in the 3D world and in the simulation. Its MetaHumans are **not drawn** (to save frames). Enter the cabin later (a future feature) and they are drawn again, in their seats.

### The other way: arrivals (in brief)
On blocks, the jetway docks, the cabin crew open the door, passengers deplane, then **border entry control** for non-Schengen flights (*missing*), then the claim belt and out. Bags go from the hold to the claim (built).

## What is missing compared with the real world (for you to decide)

1. **Online check-in and bag drop / self-service kiosks:** most real passengers don't use a staffed desk.
2. **The boarding-pass scan at security**, the tray routine and the random explosive trace check.
3. **Border exit and entry control** for non-Schengen flights, and a non-Schengen part of the pier.
4. **Passenger and bag reconciliation:** a no-show's bag offloaded before departure.
5. **Catering, cleaning, fuel and de-icing trucks**, with their real times and the fuelling-during-boarding rule.
6. **Cabin crew preflight checks and the cabin security search** before boarding (needs the cabin, to-do 209).
7. **The load sheet** signed by the captain before the doors close.
8. **PRM passengers** (wheelchair assistance, boarding first) and unaccompanied minors.
9. **The flight crew walkaround** round the aircraft.
10. **Holdover time** after de-icing.

## How it gets built (suggested order)

1. **To-do 210 (Monday, first after the test):** Passenger Service Agents as a hiring role; check-in agents at the desks with real open and close times; after closing they go to the gate; boarding-pass scan at the gate; the service desk for latecomers.
2. **The flight timeline:** extend Turnaround into one timeline per flight with all the steps above, in a data table, with delays spreading.
3. **Security details:** boarding-pass scan, the trays, random checks.
4. **The trucks:** catering, fuel, de-icing (pushback tug quality standard), plus cleaning crew in the cabin.
5. **The cabin and crew** (to-dos 209 and 202), then the cabin checks and the load sheet.
6. **Border control** for non-Schengen flights.

## 294 as built: people appear and vanish only unseen

- One look for all (`Unseen.h/.cpp`, `ADPUnseen::Look`): on screen (with a margin) and any clear line to head, middle, feet or sides = seen; people, glass, translucent and masked things never hide; under 2 px nobody sees; within 25 m only something solid counts. All local cameras and running scene captures.
- Places: the car park and the pavement along the terminal, scanned once per world on the navmesh (a point a frame), with a way to the doors off the forecourt road; refreshed all the time. At most two looks a frame.
- Departures wait for a place (2 s), as data from 50 min before departure on; leavers walk on until unseen. The log counts it all every 30 s.
- Also the taxi bus, the landside staff and the kerb crowd. Not: the terminal's west end (off the navmesh), leaves as cover.
