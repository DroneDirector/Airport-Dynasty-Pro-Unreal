# One arrival, start to finish: the plan

*Airport Dynasty Pro, 22 November 2026. The example is an A320 (150–180 seats), short-haul, landing at Chablais (Schengen). **A** = on-blocks, the moment the aircraft stops at the stand with the parking brake set. Times are typical real-world values; each one comes from a data table, so it can be tuned per airline, aircraft and airport.*

## How it is set up: the same clock per flight

- **One timeline per flight**, the same system as for departures (Departure-Flow-Plan). An arrival's steps are the first half of the aircraft's turnaround: they flow straight into its next departure.
- **Who listens to it:** ATC and the radio, the aircraft (lights, gear, reversers, flaps, doors), the marshaller or docking guidance, the ramp crew (chocks, cones, GPU, belt loader, carts), the jetway operator, the cabin crew, the passengers (PaxFlow), the baggage system (claim belts), the screens and the announcements.
- **Delays spread by themselves.** A late landing, a busy stand, a slow unload or a missing crew member moves the steps after it, the next departure, and the board times ("Landed 14:32", "Baggage at belt 3").
- **Real numbers in tables:** approach and taxi speeds, deplaning rates, door speeds, first-bag and last-bag times, border-control rates.

## The staff, and who does what

| Role | Name to hire | Works | Today |
|---|---|---|---|
| Stand allocation, gate and belt planning | **Operations Controller** (your Operations room) | the screens | simulated |
| Jetway operator | Ramp Agent (jetway-trained) | jetway cab | to-do 209 |
| Chocks, cones, GPU, belt loader, unloading, carts | Ramp Agent (ground staff) | apron, bag hall | stand-in team |
| Meeting the flight, PRM help, lost and found, delayed bags | **Passenger Service Agent** | gate, claim hall | to-do 210 |
| Cabin crew, flight crew | Cabin crew / Captain / First officer | aircraft | data only, to-do 202 |
| Border control (non-Schengen) | border police (government, not hired) | arrivals hall | missing |

## Step by step

### In the air: about A−30 to A−5
1. **Approach.** The aircraft follows the real approach: 3° glide path, flaps and gear at the real points, landing lights on below 10,000 ft (built: lights, gear, flaps). The landing runway and approach come from the real wind.
2. **Stand allocation:** Operations give the flight its stand and gate about 30 minutes before landing, and the gate screens show "Arriving from …". *Missing today:* the player sees and can change it in Operations, and a busy stand means holding or another stand.
3. **The ground crew are ready before the aircraft arrives** (a real rule, and yours). About 5 minutes before on-blocks:
   - the **marshaller** or the **docking guidance display** is ready;
   - the **stand is checked clear** of vehicles and debris, a real FOD check;
   - the **jetway operator is in the cab** (to-do 209);
   - the **tug driver sits in the tug**, 3 minutes before (built), and the ramp agents wait behind the stand line.

### Landing and taxi in: about A−8 to A
4. **Touchdown:** the main gear first, with the strut animation; **reverse thrust**; the spoilers deploy; the speed falls to taxi speed; the runway is vacated at the planned exit (built in part).
5. **Taxi in** on the centre lines at ≤25 kt, ~10 kt in turns, ≤5 kt on the stand (built). The ATC ground frequency gives the taxi clearance by radio (built in the radio rules).
6. **Docking:** the docking guidance or marshaller guides the nose wheel to the stop line. The **parking brake** is set, then the anti-collision beacon goes off. That is the moment **A**. Every person and vehicle waits behind the stand line until the beacon is off (a real rule).

### On blocks: A to A+5
7. **Chocks and cones:** a ramp agent puts chocks at the nose wheel (and main wheels), then cones round the wingtips, tail and engines. *Missing today:* the chocks and cones, which are listed in the turnaround plan but not drawn.
8. **The GPU is plugged in** within about 1–2 minutes, and the APU shuts down (built: the GPU; the APU logic is in the plan).
9. **The jetway docks** to door L1 (built). The operator **waits until the cabin crew open the door from inside**, stays a moment, then moves on (to-do 209).
10. **The belt loader and the arrivals cart train** come to the bulk hold. A ramp agent walks up and **opens the hold door by hand**, at the real speed. Only then does the loader start (to-do 209 for the door; the train is built).

### Deplaning: A+2 to about A+12
11. **The cabin crew** disarm the doors (slides to manual) and give the all clear.
12. **Passengers get off:** PRM passengers (wheelchair) usually last, with an assistant waiting at the door, and unaccompanied minors handed over to an agent. Today they appear at the cab end and walk; with the cabin (to-do 209) they stand up, take their bags from the overhead bins and walk out. The real rate is about 20 passengers a minute through L1 (built).
13. **Transfer passengers** (connecting to another flight) follow the "Transfers" signs to their next gate without passing security again in a Schengen to Schengen transfer. *Missing.*
14. **Through the jetway, the gate and down to the arrivals level:** Floor 2, then the arrivals escalators (built).

### Border control and customs (non-Schengen flights only)
15. *Missing (a real rule):* passengers from outside Schengen (UK, Turkey and others) pass **border entry control** (passport booths and e-gates) before the claim hall. Schengen flights don't.
16. **Customs** after the claim: the green "Nothing to declare" and red "Goods to declare" channels, with random checks. *Missing.*

### Bags: A+5 to about A+30
17. **Unloading:** the bags come off the bulk hold onto the belt loader and into the carts (built). On a containerised aircraft the ULD loader takes the containers off (missing).
18. **To the claim belt:** the train drives to the arrivals conveyor and the bags go onto it (built). The claim screen shows the flight and belt number.
19. **Real times:** the **first bag on the belt within about 15–20 minutes** of on-blocks and the **last bag within about 30–40 minutes**, a usual airport service standard. Priority-tagged bags come first. *Missing:* priority bags and the measured times on the screens.
20. **Passengers take their own bag** off the belt (built: hand IK, their own tagged bag).
21. **A missing bag:** a passenger whose bag doesn't come waits, then goes to the **lost and found / baggage services desk**. A Passenger Service Agent files the report. *Missing.*
22. **Oversized items** (strollers, wheelchairs, bikes, skis) come out at a separate oversized belt or door. *Missing.*

### Out of the terminal: A+15 to about A+45
23. **The arrivals hall:** meeters and greeters wait at the exit (people holding name signs, families). *Missing.*
24. **Out to the car park** (built), the zebra crossings (built), and later taxis, buses, trains and rental cars.
    - Dropped kerbs (2026-11-28): wherever a zebra meets a pavement or an island, the kerb and the pavement edge come down
      flush with the road (0.5 cm above it) over the crossing's width plus 0.3 m each side. The kerb ramps back up over
      0.9 m along the road, and the pavement over 0.5 to 2.3 m behind the kerb. Islands are cut through. The kerb's
      collision follows, because it is cooked from the same mesh. This is done in RoadNetwork.cpp (CutKerbs, on every road
      part as it is built, and in the bake). The road hash moved, so saved roads rebuild. There is no tactile paving yet.

### The aircraft carries on
25. **The turnaround continues** into the next departure: cleaning, catering, water and toilet service, fuel, the cabin check, then boarding (the Departure-Flow-Plan, from its step 11).
26. **Night stop:** if the aircraft stays overnight, the jetway pulls back, chocks and cones stay, covers go on the engine intakes and pitot tubes in bad weather, the GPU goes off, the aircraft is locked, and the crew go to the hotel. *Missing.*

## What is missing compared with the real world (for you to decide)

1. **Stand allocation** in Operations (the player sees and changes stands; holding when a stand is busy).
2. **Docking guidance displays** or a marshaller with wands at each stand.
3. **Chocks and cones** drawn and placed by the ramp crew, and the beacon-off rule before anyone approaches.
4. **Transfer passengers** and their way to the next gate.
5. **Border entry control** and **customs** for non-Schengen flights.
6. **Priority bags** and the real first-bag and last-bag times on the screens.
7. **Lost and found** for missing bags, and an **oversized-items** belt.
8. **PRM assistance** (wheelchairs at the door) and unaccompanied minors handed to an agent.
9. **Meeters and greeters** in the arrivals hall.
10. **The night stop routine** (covers, locking, the crew to the hotel).

## How it gets built (suggested order)

1. **The flight timeline** (to-do 211) covers arrivals too: one timeline from approach to the last bag, flowing into the next departure.
2. **Chocks, cones, the beacon rule and docking guidance:** small, very visible, real.
3. **Real bag times:** first and last bag measured and shown on the screens; priority bags.
4. **Transfers, lost and found, oversized items, meeters and greeters.**
5. **Non-Schengen:** border entry control and customs, together with the departure side's exit control.
6. **The night stop routine.**
